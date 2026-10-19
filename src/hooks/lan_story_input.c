#include "hooks/lan_story_input.h"
#include "network/lan_story_melee.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story input containment requires the supported 32-bit native ABI"
#endif

enum {
    CONTROLLER_GLOBAL=0x408da4u, CONTROLLER_VT=0x2c9f5cu,
    INPUT_VT=0x2c9f84u, INPUT_HANDLER=0x277b0u,
    INPUT_SUBOBJECT=0x2cu, ACTOR_TARGET=0x248u
};
typedef void (__attribute__((thiscall)) *NativeInput)(void *,void *);
static uint8_t *base;
static NativeInput original_input;
static SudekiMpPointerHook input_hook;
static volatile LONG callbacks, native_thread, fault, installed, retained;
static unsigned player;
static HHOOK focus_hook;
static HWND input_window;
static BOOL focused;
static void *sample_controller,*sample_actor,*away_controller,*away_actor;
static uint32_t sample_transaction;
static BOOL quick_down,quick_pending;
static BOOL menu_down,menu_pending,menu_pending_raw; /* native action 0x18: main menu (#75) */
static BOOL trace_events; static unsigned trace_event_count;
void SudekiMpLanStoryInputTraceEvents(BOOL on) { if(on && !trace_events) trace_event_count=0; trace_events=on; }
static unsigned melee_down;
static uint8_t melee_buttons;
static uint32_t melee_generation;
static void clear_melee(void) { melee_buttons=0; ++melee_generation; }
static float sample_x,sample_z,sample_scale=1.0f;
static BOOL ranged_held,block_down,block_held;
static uint32_t ranged_press,ranged_transaction;
static void *ranged_actor;
static float look_x,look_y;
static uint32_t look_x_at,look_y_at,look_sample_at;
typedef struct NativeEvent {
    uint32_t action,source,value;
    float magnitude;
    uint32_t flags;
    int16_t owner;
    uint16_t reserved;
} NativeEvent;

static void clear_sample(void) {
    sample_controller=sample_actor=NULL; sample_transaction=0;
    quick_pending=menu_pending=FALSE; ranged_held=block_held=FALSE;
    clear_melee(); /* Preserve down bits until physical releases. */
    sample_x=sample_z=0; sample_scale=1.0f;
    if(!away_controller) { look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0; }
}
static void sample_quick(const NativeEvent *e,BOOL admitted) {
    /* Native4279E4 chooses listener+90 for action19. The digital enable bit
     * belongs to controller+1d0, NOT event+10. Value is press/release. Track even
     * while disarmed so a held press cannot migrate to another binding. */
    if(e->action!=0x19u) return;
    BOOL down=e->value!=0;
    if(down && !quick_down && admitted) quick_pending=TRUE;
    quick_down=down;
}
/* Native 4277B0 maps action 0x18 to listener+98 (main menu) beside 0x19
 * (listener+90, Q). Same edge rule as Q: one admitted fresh press. */
static void sample_menu(const NativeEvent *e,BOOL admitted) {
    if(e->action!=0x18u) return;
    BOOL down=e->value!=0;
    if(down && !menu_down) { static unsigned logs; if(logs++<32u)
        SudekiMpLogFormat("story_main_menu event=key admitted=%u focused=%u transaction=%lu\r\n",(unsigned)admitted,(unsigned)focused,(unsigned long)sample_transaction); }
    if(down && !menu_down && admitted) menu_pending=TRUE;
    if(down && !menu_down && focused) menu_pending_raw=TRUE;
    menu_down=down;
}
/* Retail action31 maps listener+78 = controller+A4 (block).
 * Keep physical down across focus/menu/rebind; only a fresh admitted press
 * can acquire held intent, and release is always observed. */
static void sample_block(const NativeEvent *e,BOOL admitted) {
    if(e->action!=0x31u) return;
    BOOL down=e->value!=0;
    if(!down) block_held=FALSE;
    else if(!block_down && admitted) block_held=TRUE;
    block_down=down;
}
static void sample_melee(const NativeEvent *e,BOOL admitted,uint32_t now) {
    (void)now;
    if(e->action<0x2cu || e->action>0x2eu) return;
    unsigned kind=e->action-0x2bu,bit=1u<<(kind-1u);
    BOOL down=e->value!=0;
    if(kind==1u) {
        if(!down) ranged_held=FALSE;
        else if(!(melee_down&bit) && admitted && ranged_press!=UINT32_MAX) {
            ranged_held=TRUE; ++ranged_press;
        }
    }
    unsigned shift=2u*(kind-1u);
    if(admitted && down && !(melee_down&bit))
        melee_buttons=(uint8_t)((melee_buttons&~(3u<<shift))|(1u<<shift));
    else if(!down && ((melee_buttons>>shift)&3u))
        melee_buttons=(uint8_t)((melee_buttons&~(3u<<shift))|(admitted?3u<<shift:0u));
    if(down) melee_down|=bit; else melee_down&=~bit;
}
/* H1b: a press claimed by this game's own talk prompt. The weak pair is
 * dropped from the pending states and its later physical release produces
 * no state (melee_down still records the button). */
void SudekiMpLanStoryInputConsumeWeak(void) { melee_buttons=(uint8_t)(melee_buttons&~3u); }
static uint8_t take_melee(void) {
    uint8_t buttons=melee_buttons;
    melee_buttons=SudekiMpStoryMeleeAdvance(buttons);
    return buttons;
}

static BOOL window_exact(void) {
    DWORD pid=0;
    return input_window && IsWindow(input_window) &&
        GetWindowThreadProcessId(input_window,&pid)==(DWORD)native_thread &&
        pid==GetCurrentProcessId();
}
static LRESULT CALLBACK focus_messages(int code,WPARAM sent,LPARAM raw) {
    InterlockedIncrement(&callbacks);
    if(code>=0 && raw && input_window &&
        (DWORD)InterlockedCompareExchange(&native_thread,0,0)==GetCurrentThreadId()) {
        const CWPSTRUCT *m=(const CWPSTRUCT *)raw;
        if(m->hwnd==input_window) {
            if(m->message==WM_KILLFOCUS || m->message==WM_CANCELMODE ||
                (m->message==WM_ACTIVATE && LOWORD(m->wParam)==WA_INACTIVE) ||
                (m->message==WM_ACTIVATEAPP && !m->wParam)) {
                focused=FALSE; sample_x=sample_z=0;
                quick_pending=menu_pending=menu_pending_raw=FALSE; ranged_held=block_held=FALSE;
                clear_melee();
                look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
            } else if(m->message==WM_SETFOCUS) {
                focused=TRUE; sample_x=sample_z=0;
                quick_pending=menu_pending=menu_pending_raw=FALSE; ranged_held=block_held=FALSE;
                clear_melee();
                look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
            }
        }
    }
    LRESULT result=CallNextHookEx(focus_hook,code,sent,raw);
    InterlockedDecrement(&callbacks);
    return result;
}
static BOOL ensure_focus_hook(void) {
    if(focus_hook) return window_exact();
    HWND window=GetActiveWindow(); DWORD pid=0;
    if(!window || GetWindowThreadProcessId(window,&pid)!=(DWORD)native_thread ||
        pid!=GetCurrentProcessId()) return FALSE;
    input_window=window; focused=GetFocus()==window;
    focus_hook=SetWindowsHookExW(WH_CALLWNDPROC,focus_messages,NULL,(DWORD)native_thread);
    if(!focus_hook) { input_window=NULL; focused=FALSE; return FALSE; }
    return TRUE;
}

static BOOL retain_failure(DWORD error) {
    HMODULE pinned;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCSTR)&input_hook,&pinned);
    SetLastError(error?error:ERROR_BUSY);
    return FALSE;
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL native_thread_exact(void) {
    return (DWORD)InterlockedCompareExchange(&native_thread,0,0)==GetCurrentThreadId();
}
static BOOL controller_exact(void *controller) {
    uint8_t *c=(uint8_t *)controller;
    return base && readable(base+CONTROLLER_GLOBAL,sizeof(void *)) &&
        controller==*(void **)(base+CONTROLLER_GLOBAL) && readable(c,ACTOR_TARGET+4u) &&
        *(void **)c==base+CONTROLLER_VT &&
        *(void **)(c+INPUT_SUBOBJECT)==base+INPUT_VT;
}

static void __attribute__((thiscall,force_align_arg_pointer)) story_input(
    void *listener,void *event) {
    InterlockedIncrement(&callbacks);
    /* A dispatcher may have fetched our slot before restoration. Keep these
     * two immutable dependencies after retirement, and honor retail behavior
     * once the client fence itself has safely been retired. */
    if(!InterlockedCompareExchange(&retained,0,0)) {
        if(original_input) original_input(listener,event);
        goto done;
    }
    DWORD thread=(DWORD)InterlockedCompareExchange(&native_thread,0,0);
    if(thread && thread!=GetCurrentThreadId()) {
        InterlockedExchange(&fault,1);
        goto done;
    }
    if(!base || !readable(base+CONTROLLER_GLOBAL,sizeof(void *))) {
        InterlockedExchange(&fault,1);
        goto done;
    }
    uint8_t *controller=*(uint8_t **)(base+CONTROLLER_GLOBAL);
    /* The native constructor stores this exact input vtable at controller+2c.
     * Suppress even before the first loaded actor/observer callback: admitting
     * events during loading would seed native held actions for that actor.
     * An unexpected owner shape closes this adapter's readiness proof; it
     * never reopens native input for the same global controller listener. */
    if(controller && listener==controller+INPUT_SUBOBJECT) {
        if(!controller_exact(controller)) InterlockedExchange(&fault,1);
        else if(readable(event,sizeof(NativeEvent))) {
            NativeEvent e; memcpy(&e,event,sizeof(e));
            int owner=*(int *)(controller+INPUT_SUBOBJECT+0x5cu);
            if(trace_events && trace_event_count<300u) { ++trace_event_count;
                SudekiMpLogFormat("story_input event=native action=%#x value=%ld owner=%d device=%ld mode=%lu/%lu\r\n",
                    e.action,(long)e.value,owner,(long)e.owner,
                    (unsigned long)*(uint32_t *)(controller+0x80u),(unsigned long)*(uint32_t *)(controller+0x84u)); }
            BOOL admitted=sample_transaction && focused && window_exact() &&
            controller==sample_controller &&
                *(void **)(controller+ACTOR_TARGET)==sample_actor;
            /* The main-menu toggle is sampled for any device: the native menu
             * re-owns the listener's device while shown (#75). */
            sample_menu(&e,admitted && (controller[0x1d0u]&2u));
            if(owner==-1 || owner==e.owner) {
                sample_quick(&e,admitted && (controller[0x1d0u]&2u));
                sample_melee(&e,admitted && (controller[0x1d0u]&2u),GetTickCount());
                sample_block(&e,admitted && (controller[0x1d0u]&2u));
            }
            BOOL orbit=focused && window_exact() && controller==away_controller && away_actor &&
                *(void **)(controller+ACTOR_TARGET)==away_actor;
            if(!admitted && !orbit) goto done;
            /* Exact42781C checks this signed device owner. 4278F9..427980
             * maps29 to lateral and28 to forward magnitude. The ordinary
             * input backend dispatches releases through the same listener;
             * never call the native handler or seed its cached actions. */
            if(admitted && (owner==-1 || owner==e.owner) && (e.action==0x29u || e.action==0x28u || e.action==0x2bu)) {
                BOOL scaled=(controller[0x1d0u]&1u)!=0u;
                if(scaled && e.action==0x2bu) {
                    /* Native4278E4 uses exact constant2.0 minus this axis. */
                    float scale=2.0f-e.magnitude;
                    if(!isfinite(scale) || scale<0 || scale>4.0f) {
                        sample_x=sample_z=0; sample_scale=0;
                    } else sample_scale=scale;
                } else if(e.action==0x2bu) { /* Native unscaled branch ignores it. */
                } else if(!isfinite(e.magnitude) || fabsf(e.magnitude)>1.001f) {
                    sample_x=sample_z=0;
                } else if(e.action==0x29u) sample_x=e.magnitude*(scaled?sample_scale:1.0f);
                else sample_z=e.magnitude*(scaled?sample_scale:1.0f);
            }
            if((owner==-1 || owner==e.owner) && (e.action==0x69u || e.action==0x6au)) {
                /* Mouse axes are not normalized like a stick: observed
                 * native events routinely exceed one. Saturate this custom
                 * orbit's rate without losing the turn's direction. */
                float axis=isfinite(e.magnitude)?e.magnitude:0.0f;
                if(axis>1.0f) axis=1.0f; else if(axis< -1.0f) axis=-1.0f;
                if(e.action==0x69u) { look_x=axis; look_x_at=GetTickCount(); }
                else { look_y=axis; look_y_at=GetTickCount(); }
            }
        }
        goto done;
    }
    /* Other instances retain retail behavior. Native title/menu listeners use
     * their own vtable slots and never enter this adapter in the first place. */
    if(original_input) original_input(listener,event);
done:
    InterlockedDecrement(&callbacks);
}

static BOOL owner_exact(void) {
    return base && InterlockedCompareExchange(&installed,0,0) &&
        input_hook.installed && input_hook.slot==(void **)(base+INPUT_VT) &&
        input_hook.original_value==(void *)(base+INPUT_HANDLER) &&
        input_hook.replacement_value==(void *)story_input &&
        readable(input_hook.slot,sizeof(void *)) &&
        *input_hook.slot==input_hook.replacement_value;
}

BOOL SudekiMpLanStoryInputObserve(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w) {
    if(!owner_exact() || InterlockedCompareExchange(&fault,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0) || !w ||
        !w->service_post_original_exact || !w->dispatch_serial ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !controller_exact(controller)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    DWORD thread=GetCurrentThreadId();
    LONG previous=InterlockedCompareExchange(&native_thread,(LONG)thread,0);
    if(previous && (DWORD)previous!=thread) {
        InterlockedExchange(&fault,1);
        SetLastError(ERROR_INVALID_THREAD_ID); return FALSE;
    }
    (void)ensure_focus_hook();
    return TRUE;
}

BOOL SudekiMpLanStoryInputExact(void *controller,void *actor) {
    return actor && owner_exact() && native_thread_exact() &&
        !InterlockedCompareExchange(&fault,0,0) &&
        !InterlockedCompareExchange(&callbacks,0,0) && controller_exact(controller) &&
        *(void **)((uint8_t *)controller+ACTOR_TARGET)==actor;
}

BOOL SudekiMpLanStoryInputArm(void *controller,void *actor,uint32_t transaction) {
    if(!transaction || !SudekiMpLanStoryInputExact(controller,actor) ||
        !ensure_focus_hook()) return FALSE;
    if(ranged_transaction!=transaction || ranged_actor!=actor) {
        ranged_transaction=transaction;ranged_actor=actor;ranged_press=0;ranged_held=FALSE;
    }
    if(sample_controller!=controller || sample_actor!=actor || sample_transaction!=transaction) {
        clear_sample(); sample_controller=controller; sample_actor=actor;
        sample_transaction=transaction;
        if(((uint8_t *)controller)[0x1d0u]&1u) {
            float scale=*(float *)((uint8_t *)controller+0x1b8u);
            if(!isfinite(scale) || scale<0 || scale>4.0f) { clear_sample(); return FALSE; }
            sample_scale=scale;
        }
    }
    return TRUE;
}
BOOL SudekiMpLanStoryInputAwayLook(void *controller,void *actor,BOOL enabled) {
    if(!native_thread_exact() || InterlockedCompareExchange(&callbacks,0,0)) return FALSE;
    if(!enabled) {
        if(away_controller) { look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0; }
        away_controller=away_actor=NULL; return TRUE;
    }
    if(!SudekiMpLanStoryInputExact(controller,actor) || !ensure_focus_hook()) {
        away_controller=away_actor=NULL; look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
        return FALSE;
    }
    if(away_controller!=controller || away_actor!=actor) {
        look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
    }
    away_controller=controller; away_actor=actor; return TRUE;
}
BOOL SudekiMpLanStoryInputSample(void *controller,void *actor,uint32_t transaction,
    float *x,float *z) {
    if(x) *x=0;
    if(z) *z=0;
    if(!x || !z || !transaction || !SudekiMpLanStoryInputExact(controller,actor) ||
        !focus_hook || !window_exact() || sample_controller!=controller ||
        sample_actor!=actor || sample_transaction!=transaction) return FALSE;
    if(!focused) { sample_x=sample_z=0; return TRUE; }
    if(!isfinite(sample_x) || !isfinite(sample_z) ||
        fabsf(sample_x)>0x1.00418ap+2f || fabsf(sample_z)>0x1.00418ap+2f) {
        /* 4.004 rounded to the stored float, including the boundary on x87. */
        sample_x=sample_z=0; return FALSE;
    }
    /* Preserve native action2B scaling until the controller's local shaping
     * and camera transform; normalization here erased walk/run magnitude. */
    *x=sample_x; *z=sample_z;
    return TRUE;
}
void SudekiMpLanStoryInputClear(void) {
    if(native_thread_exact() && !InterlockedCompareExchange(&callbacks,0,0)) clear_sample();
}
BOOL SudekiMpLanStoryInputTakeQuickMenu(void *controller,void *actor,uint32_t transaction) {
    float x,z;
    if(!SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z) || !focused) return FALSE;
    BOOL pressed=quick_pending; quick_pending=FALSE; return pressed;
}
BOOL SudekiMpLanStoryInputTakeMainMenu(void *controller,void *actor,uint32_t transaction) {
    float x,z;
    if(!SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z) || !focused) return FALSE;
    BOOL pressed=menu_pending; menu_pending=FALSE; return pressed;
}
BOOL SudekiMpLanStoryInputTakeMainMenuRaw(void) {
    BOOL pressed=menu_pending_raw && focused; menu_pending_raw=FALSE; return pressed;
}
BOOL SudekiMpLanStoryInputMelee(void *controller,void *actor,uint32_t transaction,
    uint8_t *buttons,uint32_t *generation) {
    float x,z;
    if(!buttons || !generation) return FALSE;
    *buttons=0;*generation=melee_generation;
    if(!SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z) || !focused) return FALSE;
    *buttons=take_melee();return TRUE;
}

BOOL SudekiMpLanStoryInputRanged(void *controller,void *actor,uint32_t transaction,
    BOOL *held,uint32_t *press) {
    float x,z;
    if(!held || !press || !SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z)) return FALSE;
    *held=focused && ranged_held;*press=ranged_press;
    return TRUE;
}
BOOL SudekiMpLanStoryInputBlock(void *controller,void *actor,uint32_t transaction) {
    float x,z;
    return SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z) && focused && block_held;
}
void SudekiMpLanStoryInputMuteForCamera(void) {
    if(!native_thread_exact() || InterlockedCompareExchange(&callbacks,0,0)) return;
    sample_x=sample_z=0;
    clear_melee(); ranged_held=block_held=FALSE;
}
void SudekiMpLanStoryInputMuteMovement(void) {
    if(!native_thread_exact() || InterlockedCompareExchange(&callbacks,0,0)) return;
    SudekiMpLanStoryInputMuteForCamera();
    look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
}

BOOL SudekiMpLanStoryInputOrbit(void *controller,void *actor,uint32_t transaction,
    float *yaw,float *pitch) {
    if(yaw) *yaw=0;
    if(pitch) *pitch=0;
    float x,z;
    if(!yaw || !pitch) return FALSE;
    BOOL orbit=controller==away_controller && actor==away_actor && actor && focus_hook && window_exact() &&
        SudekiMpLanStoryInputExact(controller,actor);
    if(!orbit && !SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z)) return FALSE;
    uint32_t now=GetTickCount(),elapsed=look_sample_at?now-look_sample_at:0;
    look_sample_at=now;
    if(!focused || !elapsed || elapsed>100u) return TRUE;
    if(!readable(base+0x339598u,sizeof(float)) || !readable(base+0x3c2fe2u,3u)) return FALSE;
    float sensitivity=*(float *)(base+0x339598u);
    if(!isfinite(sensitivity) || sensitivity<0 || sensitivity>10.0f) return FALSE;
    float gain=1.2f*sensitivity+0.3f,dt=(float)elapsed/1000.0f;
    /* Supply sensitivity-scaled axis seconds for the local presentation.
     * Its exact native config supplies distance-dependent rotation speed,
     * ExplorationUserDistanceScale and the authored distance/height curve.
     * Preserve the native sensitivity and Exploration inversion preferences. */
    float horizontal=(uint32_t)(now-look_x_at)<=100u?look_x:0.0f;
    float vertical=(uint32_t)(now-look_y_at)<=100u?look_y:0.0f;
    *yaw=horizontal*gain*dt*(base[0x3c2fe4u]?-1.0f:1.0f);
    *pitch=vertical*gain*dt*(base[0x3c2fe2u]?-1.0f:1.0f);
    if(*yaw>0.5f) *yaw=0.5f; else if(*yaw< -0.5f) *yaw=-0.5f;
    if(*pitch>0.5f) *pitch=0.5f; else if(*pitch< -0.5f) *pitch=-0.5f;
    return TRUE;
}

BOOL SudekiMpLanStoryInputLookAxes(float *x,float *y) {
    if(!x || !y) return FALSE;
    uint32_t now=GetTickCount();
    *x=focused && (uint32_t)(now-look_x_at)<=100u?look_x:0.0f;
    *y=focused && (uint32_t)(now-look_y_at)<=100u?look_y:0.0f;
    if(!isfinite(*x) || !isfinite(*y)) { *x=*y=0.0f; return FALSE; }
    return TRUE;
}
BOOL SudekiMpLanStoryInputInstall(HMODULE image,unsigned client_player) {
    uint8_t *b=(uint8_t *)image;
    uint8_t handler[]={0x56,0x57,0x8b,0xf9,0x8b,0x0d,0,0,0,0,0x80,0x79,0x56,0,0x74,0x5c};
    uint8_t constructor[]={0x89,0x0d,0,0,0,0,0xc7,0x06,0,0,0,0,
        0xc7,0x46,0x24,0,0,0,0,0xc7,0x46,0x2c,0,0,0,0};
    if(!image || client_player<1u || client_player>3u) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if((base && base!=b) || InterlockedCompareExchange(&retained,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    uint32_t address=(uint32_t)(uintptr_t)(b+0x408d8cu);
    memcpy(handler+6,&address,4);
    address=(uint32_t)(uintptr_t)(b+CONTROLLER_GLOBAL); memcpy(constructor+2,&address,4);
    address=(uint32_t)(uintptr_t)(b+CONTROLLER_VT); memcpy(constructor+8,&address,4);
    address=(uint32_t)(uintptr_t)(b+0x2c9f7cu); memcpy(constructor+15,&address,4);
    address=(uint32_t)(uintptr_t)(b+INPUT_VT); memcpy(constructor+22,&address,4);
    if(!readable(b+INPUT_HANDLER,sizeof(handler)) ||
        memcmp(b+INPUT_HANDLER,handler,sizeof(handler)) ||
        !readable(b+0x76ea8u,sizeof(constructor)) ||
        memcmp(b+0x76ea8u,constructor,sizeof(constructor)) ||
        !readable(b+INPUT_VT,sizeof(void *)) ||
        *(void **)(b+INPUT_VT)!=(void *)(b+INPUT_HANDLER) ||
        !readable(b+0x2e3628u,sizeof(double)) || *(double *)(b+0x2e3628u)!=2.0) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    /* Dependencies precede the single aligned pointer publication. This hook
     * installs no trampoline and never changes native controller/actor fields. */
    if(!base) { base=b; original_input=(NativeInput)(b+INPUT_HANDLER); }
    player=client_player;
    away_controller=away_actor=NULL;
    quick_down=quick_pending=menu_down=menu_pending=menu_pending_raw=FALSE;
    InterlockedExchange(&retained,1);
    if(!SudekiMpInstallPointerHook(&input_hook,(void **)(b+INPUT_VT),
            b+INPUT_HANDLER,story_input)) {
        DWORD error=GetLastError();
        if(!SudekiMpLanStoryInputUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
    InterlockedExchange(&installed,1);
    SudekiMpLogFormat("story_input event=install player=%u native_character_input=closed listener_offset=44 other_listener_slots=unchanged\r\n",player);
    return TRUE;
}

BOOL SudekiMpLanStoryInputUninstall(void) {
    if(!InterlockedCompareExchange(&retained,0,0)) return TRUE;
    if(InterlockedCompareExchange(&callbacks,0,0) ||
        (InterlockedCompareExchange(&native_thread,0,0) && !native_thread_exact())) {
        return retain_failure(ERROR_BUSY);
    }
    away_controller=away_actor=NULL; menu_pending_raw=FALSE;
    clear_sample();
    if(focus_hook && !UnhookWindowsHookEx(focus_hook)) return retain_failure(GetLastError());
    focus_hook=NULL; input_window=NULL; focused=FALSE;
    if(!SudekiMpRestorePointerHook(&input_hook)) return retain_failure(GetLastError());
    if(InterlockedCompareExchange(&callbacks,0,0)) {
        return retain_failure(ERROR_BUSY);
    }
    InterlockedExchange(&installed,0);
    InterlockedExchange(&retained,0);
    player=0;
    InterlockedExchange(&native_thread,0); InterlockedExchange(&fault,0);
    return TRUE;
}
