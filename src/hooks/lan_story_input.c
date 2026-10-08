#include "hooks/lan_story_input.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#define COBJMACROS
#include <d3d9.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story input containment requires the supported 32-bit native ABI"
#endif

enum {
    CONTROLLER_GLOBAL=0x408da4u, CONTROLLER_VT=0x2c9f5cu,
    INPUT_VT=0x2c9f84u, INPUT_HANDLER=0x277b0u,
    INPUT_SUBOBJECT=0x2cu, ACTOR_TARGET=0x248u, D3D_DEVICE_GLOBAL=0x3c31dcu
};
typedef void (__attribute__((thiscall)) *NativeInput)(void *,void *);
static uint8_t *base;
static NativeInput original_input;
static SudekiMpPointerHook input_hook;
static volatile LONG callbacks, native_thread, fault, installed, retained;
static unsigned player;
static BOOL host_route;
static void *host_controller;
static HHOOK focus_hook;
static HWND input_window;
static BOOL focused;
typedef HRESULT (WINAPI *DeviceCreationParameters)(IDirect3DDevice9 *,D3DDEVICE_CREATION_PARAMETERS *);
typedef struct InputWindowIdentity {
    IDirect3DDevice9 *device;
    const IDirect3DDevice9Vtbl *vtable;
    DeviceCreationParameters creation;
    HWND window;
} InputWindowIdentity;
static InputWindowIdentity input_window_owner;
static void *sample_controller,*sample_actor;
static uint32_t sample_transaction;
static uint32_t sample_avatar_generation;
static SudekiMpLanStoryInputAvatarExact sample_avatar_exact;
static void *sample_avatar_context;
static BOOL quick_down,quick_pending;
enum { MELEE_QUEUE=4, MELEE_MAX_AGE=250 };
static unsigned melee_down,melee_first,melee_count;
static struct { unsigned kind; uint32_t at; } melee_queue[MELEE_QUEUE];
static float sample_x,sample_z,sample_scale=1.0f;
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
    sample_avatar_generation=0; sample_avatar_exact=NULL; sample_avatar_context=NULL;
    quick_pending=FALSE;
    melee_first=melee_count=0; /* Preserve down bits until physical releases. */
    sample_x=sample_z=0; sample_scale=1.0f;
    look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
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
static void sample_melee(const NativeEvent *e,BOOL admitted,BOOL action_gate,uint32_t now) {
    /* Weak 0x2C, Strong 0x2D, Sweep 0x2E -> kinds 1..3; Block 0x31 -> kind 4
     * (the ally seat's second special; the runtime drops it for hero seats). */
    if((e->action<0x2cu || e->action>0x2eu) && e->action!=0x31u) return;
    unsigned kind=e->action==0x31u?4u:e->action-0x2bu,bit=1u<<(kind-1u);
    BOOL down=e->value!=0;
    if(down && !(melee_down&bit)) {
        BOOL queued=FALSE;
        if(admitted && action_gate && melee_count<MELEE_QUEUE) {
            unsigned next=(melee_first+melee_count)%MELEE_QUEUE;
            melee_queue[next].kind=kind; melee_queue[next].at=now; ++melee_count; queued=TRUE;
        }
        /* Research (gated, bounded): why a melee press did or did not enter the
         * client outbox. action_gate is controller+1d0 bit 1 (native action gate). */
        static unsigned press_logs;
        if(SudekiMpLogResearchEnabled() && press_logs<200u) { ++press_logs;
            SudekiMpLogFormat("story_input event=melee_press kind=%u queued=%u admitted=%u action_gate=%u owner=%d value=%ld pending=%u\r\n",
                kind,queued,admitted,action_gate,e->owner,(long)e->value,melee_count); }
    }
    if(down) melee_down|=bit; else melee_down&=~bit;
}
static unsigned take_melee(uint32_t now) {
    while(melee_count) {
        unsigned next=melee_first;
        melee_first=(melee_first+1u)%MELEE_QUEUE; --melee_count;
        if((uint32_t)(now-melee_queue[next].at)<=MELEE_MAX_AGE) return melee_queue[next].kind;
    }
    return 0;
}

static BOOL readable(const void *p,size_t n);
static BOOL executable(const void *p) {
    MEMORY_BASIC_INFORMATION m;
    if(!p || !VirtualQuery(p,&m,sizeof(m)) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE || access==PAGE_EXECUTE_READ ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
/* The supported native device global is also the title renderer's owner.
 * GetCreationParameters supplies its HWND even when this process is in the
 * background. Enrollment proves identity only; it never activates a window. */
static BOOL observe_window(InputWindowIdentity *out) {
    if(!out || !base || !native_thread || (DWORD)native_thread!=GetCurrentThreadId() ||
        !readable(base+D3D_DEVICE_GLOBAL,sizeof(void *))) return FALSE;
    IDirect3DDevice9 *device=*(IDirect3DDevice9 **)(base+D3D_DEVICE_GLOBAL);
    if(!readable(device,sizeof(*device))) return FALSE;
    const IDirect3DDevice9Vtbl *vtable=device->lpVtbl;
    if(!readable(vtable,offsetof(IDirect3DDevice9Vtbl,GetCreationParameters)+sizeof(void *))) return FALSE;
    DeviceCreationParameters creation=vtable->GetCreationParameters;
    D3DDEVICE_CREATION_PARAMETERS parameters={0};
    if(!executable((const void *)creation) || FAILED(creation(device,&parameters)) ||
        *(IDirect3DDevice9 **)(base+D3D_DEVICE_GLOBAL)!=device || !readable(device,sizeof(*device)) ||
        device->lpVtbl!=vtable || !readable(vtable,sizeof(void *)*10u) ||
        vtable->GetCreationParameters!=creation || !parameters.hFocusWindow ||
        !IsWindow(parameters.hFocusWindow)) return FALSE;
    DWORD pid=0;
    if(GetWindowThreadProcessId(parameters.hFocusWindow,&pid)!=(DWORD)native_thread ||
        pid!=GetCurrentProcessId()) return FALSE;
    *out=(InputWindowIdentity){device,vtable,creation,parameters.hFocusWindow};
    return TRUE;
}
static BOOL window_exact(void) {
    InputWindowIdentity current;
    return input_window && observe_window(&current) && current.window==input_window &&
        current.device==input_window_owner.device && current.vtable==input_window_owner.vtable &&
        current.creation==input_window_owner.creation && current.window==input_window_owner.window;
}
static LRESULT CALLBACK focus_messages(int code,WPARAM sent,LPARAM raw) {
    InterlockedIncrement(&callbacks);
    if(code>=0 && raw && input_window &&
        (DWORD)InterlockedCompareExchange(&native_thread,0,0)==GetCurrentThreadId()) {
        const CWPSTRUCT *m=(const CWPSTRUCT *)raw;
        if(m->hwnd==input_window && window_exact()) {
            if(m->message==WM_KILLFOCUS || m->message==WM_CANCELMODE ||
                (m->message==WM_ACTIVATE && LOWORD(m->wParam)==WA_INACTIVE) ||
                (m->message==WM_ACTIVATEAPP && !m->wParam)) {
                focused=FALSE; sample_x=sample_z=0;
                quick_pending=FALSE;
                melee_first=melee_count=0;
                look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
            } else if(m->message==WM_SETFOCUS) {
                focused=TRUE; sample_x=sample_z=0;
                quick_pending=FALSE;
                melee_first=melee_count=0;
                look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
            }
        }
    }
    LRESULT result=CallNextHookEx(focus_hook,code,sent,raw);
    InterlockedDecrement(&callbacks);
    return result;
}
static BOOL ensure_focus_hook(void) {
    if(focus_hook) {
        if(window_exact()) return TRUE;
        focused=FALSE; clear_sample(); return FALSE;
    }
    InputWindowIdentity owner;
    if(!observe_window(&owner)) return FALSE;
    input_window_owner=owner; input_window=owner.window; focused=FALSE;
    focus_hook=SetWindowsHookExW(WH_CALLWNDPROC,focus_messages,NULL,(DWORD)native_thread);
    if(!focus_hook) { input_window=NULL; memset(&input_window_owner,0,sizeof(input_window_owner)); return FALSE; }
    if(!window_exact()) {
        focused=FALSE; clear_sample();
        /* A failed unhook keeps the complete callback owner for teardown or
         * a later exact retry; never forget a hook that may still call us. */
        if(UnhookWindowsHookEx(focus_hook)) {
            focus_hook=NULL; input_window=NULL; memset(&input_window_owner,0,sizeof(input_window_owner));
        }
        return FALSE;
    }
    focused=GetFocus()==input_window;
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
        (!host_route || controller==host_controller) &&
        controller==*(void **)(base+CONTROLLER_GLOBAL) && readable(c,ACTOR_TARGET+4u) &&
        *(void **)c==base+CONTROLLER_VT &&
        *(void **)(c+INPUT_SUBOBJECT)==base+INPUT_VT;
}
/* CONFIRMED_STATIC on the supported image: ctor76E40 zeros 17 action pairs
 * at8C..113; Init276C0 zeros held bytes114..19F, cached axis events50..7F,
 * movement/turn/look floats1A0..1B4, and seeds scale1B8=2. Init leaves
 * input-dirty1C9 unchanged; the separate transition290D0 clears that byte.
 * Handler277B0 writes exactly those input caches; frame27CF0 consumes them.
 * FilterNone/290D0 is not a full reset (it skips action6 and leaves look/held
 * caches). Observe only: no reset, filter call or cache write is permitted. */
static BOOL native_cache_neutral(const uint8_t *c) {
    if(!readable(c,0x24cu)) return FALSE;
    for(unsigned i=0x50u;i<0x80u;++i) if(c[i]) return FALSE;
    for(unsigned i=0;i<17u;++i)
        if(*(const uint32_t *)(c+0x8cu+i*8u) || *(const float *)(c+0x90u+i*8u)!=0.0f) return FALSE;
    for(unsigned i=0x114u;i<0x1a0u;++i) if(c[i]) return FALSE;
    for(unsigned i=0x1a0u;i<0x1b8u;i+=4u) if(*(const float *)(c+i)!=0.0f) return FALSE;
    int current=*(const int *)(c+0x80u),pending=*(const int *)(c+0x84u);
    return current>=0 && current<=3 && pending==current && !c[0x1c9u] &&
        *(const float *)(c+0x1b8u)==2.0f;
}
typedef struct HostTitleIdentity {
    void *controller,*world,*group,*scene,*title;
} HostTitleIdentity;
static BOOL host_title_identity(uint8_t *b,HostTitleIdentity *out) {
    if(!b || !out || !readable(b+0x408d10u,4u) || !readable(b+0x408d94u,4u) ||
        !readable(b+CONTROLLER_GLOBAL,4u) || !readable(b+0x408d1cu,4u)) return FALSE;
    uint8_t *c=*(uint8_t **)(b+CONTROLLER_GLOBAL),*world=*(uint8_t **)(b+0x408d10u),
        *group=*(uint8_t **)(b+0x408d94u),*scene=*(uint8_t **)(b+0x408d1cu),*title;
    if(!readable(c,0x24cu) || *(void **)c!=b+CONTROLLER_VT ||
        *(void **)(c+INPUT_SUBOBJECT)!=b+INPUT_VT || *(void **)(c+ACTOR_TARGET) ||
        !native_cache_neutral(c) || !readable(world,0x39bu) || *(void **)world!=b+0x2c4c3cu ||
        *(void **)(world+0xcu) || *(void **)(world+0x10u) || *(void **)(world+0x14u) ||
        *(void **)(world+0x394u) || world[0x399u] || world[0x39au] ||
        !readable(group,0xd0u) || *(void **)group!=b+0x2c6d30u || *(uint32_t *)(group+0xccu) ||
        !readable(scene,0x174u) || !readable(title=*(uint8_t **)(scene+0x170u),0x1844u) ||
        *(void **)title!=b+0x2cb1fcu || *(unsigned *)(title+0x44u)!=5u) return FALSE;
    for(unsigned i=0;i<4u;++i)
        if(*(void **)(group+0x60u+i*0xcu) || *(void **)(group+0x90u+i*0xcu)) return FALSE;
    if(*(void **)(group+0xc0u)) return FALSE;
    *out=(HostTitleIdentity){c,world,group,scene,title};
    return TRUE;
}
static BOOL host_title_unchanged(uint8_t *b,const HostTitleIdentity *before) {
    HostTitleIdentity after;
    return before && host_title_identity(b,&after) && !memcmp(before,&after,sizeof(after));
}
/* Controller shape has already been established by the caller. Avatar
 * identity belongs to its runtime lease, independently of the native hero
 * input target. A revoked generation cannot resume without explicit Arm. */
static BOOL sample_target_exact(void *controller,void *actor,uint32_t transaction) {
    if(!actor || !transaction || controller!=sample_controller ||
        actor!=sample_actor || transaction!=sample_transaction) return FALSE;
    if(sample_avatar_exact) {
        if(sample_avatar_generation && sample_avatar_exact(actor,sample_avatar_generation,
                transaction,sample_avatar_context)) return TRUE;
        clear_sample(); return FALSE;
    }
    return *(void **)((uint8_t *)controller+ACTOR_TARGET)==actor;
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
    if(host_route && (controller!=host_controller || listener!=(uint8_t *)host_controller+INPUT_SUBOBJECT)) {
        InterlockedExchange(&fault,1);
        goto done; /* A replacement controller cannot inherit this host fence. */
    }
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
            BOOL admitted=!InterlockedCompareExchange(&fault,0,0) &&
                sample_target_exact(controller,sample_actor,sample_transaction) &&
                focused && window_exact();
            if(owner==-1 || owner==e.owner) {
                sample_quick(&e,admitted && (controller[0x1d0u]&2u));
                sample_melee(&e,admitted,(controller[0x1d0u]&2u)!=0u,GetTickCount());
            }
            if(!admitted) goto done;
            /* Exact42781C checks this signed device owner. 4278F9..427980
             * maps29 to lateral and28 to forward magnitude. The ordinary
             * input backend dispatches releases through the same listener;
             * never call the native handler or seed its cached actions. */
            if((owner==-1 || owner==e.owner) && (e.action==0x29u || e.action==0x28u || e.action==0x2bu)) {
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
static BOOL host_install_published(const HostTitleIdentity *before) {
    if(owner_exact() && host_title_unchanged(base,before)) return TRUE;
    if(!SudekiMpLanStoryInputUninstall()) return FALSE;
    SetLastError(ERROR_INVALID_STATE); return FALSE;
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

BOOL SudekiMpLanStoryInputControllerExact(void *controller) {
    return owner_exact() && native_thread_exact() &&
        !InterlockedCompareExchange(&fault,0,0) &&
        !InterlockedCompareExchange(&callbacks,0,0) && controller_exact(controller);
}
BOOL SudekiMpLanStoryInputExact(void *controller,void *actor) {
    return actor && SudekiMpLanStoryInputControllerExact(controller) &&
        *(void **)((uint8_t *)controller+ACTOR_TARGET)==actor;
}
BOOL SudekiMpLanStoryInputHostFenceExact(void *controller,void *original_leader) {
    return host_route && host_controller==controller &&
        SudekiMpLanStoryInputExact(controller,original_leader) && native_cache_neutral(controller);
}
static BOOL arm_sample(void *controller,void *actor,uint32_t transaction,
    uint32_t generation,SudekiMpLanStoryInputAvatarExact exact,void *context) {
    if(sample_controller!=controller || sample_actor!=actor || sample_transaction!=transaction ||
        sample_avatar_generation!=generation || sample_avatar_exact!=exact || sample_avatar_context!=context) {
        clear_sample(); sample_controller=controller; sample_actor=actor;
        sample_transaction=transaction;
        sample_avatar_generation=generation; sample_avatar_exact=exact; sample_avatar_context=context;
        if(((uint8_t *)controller)[0x1d0u]&1u) {
            float scale=*(float *)((uint8_t *)controller+0x1b8u);
            if(!isfinite(scale) || scale<0 || scale>4.0f) { clear_sample(); return FALSE; }
            sample_scale=scale;
        }
    }
    return TRUE;
}
BOOL SudekiMpLanStoryInputArm(void *controller,void *actor,uint32_t transaction) {
    if(!transaction || !SudekiMpLanStoryInputExact(controller,actor) ||
        !ensure_focus_hook()) return FALSE;
    return arm_sample(controller,actor,transaction,0,NULL,NULL);
}
BOOL SudekiMpLanStoryInputArmAvatar(void *controller,void *entity,uint32_t generation,
    uint32_t transaction,SudekiMpLanStoryInputAvatarExact exact,void *context) {
    if(!SudekiMpLanStoryInputControllerExact(controller)) return FALSE;
    if(!entity || !generation || !transaction || !exact ||
        !exact(entity,generation,transaction,context) || !ensure_focus_hook()) {
        clear_sample(); return FALSE;
    }
    return arm_sample(controller,entity,transaction,generation,exact,context);
}
BOOL SudekiMpLanStoryInputSample(void *controller,void *actor,uint32_t transaction,
    float *x,float *z) {
    if(x) *x=0;
    if(z) *z=0;
    if(!x || !z || !transaction || !SudekiMpLanStoryInputControllerExact(controller) ||
        !sample_target_exact(controller,actor,transaction) || !focus_hook || !window_exact()) return FALSE;
    if(!focused) { sample_x=sample_z=0; return TRUE; }
    float n=sqrtf(sample_x*sample_x+sample_z*sample_z);
    if(!isfinite(n)) { sample_x=sample_z=0; return FALSE; }
    *x=n>1.0f?sample_x/n:sample_x;
    *z=n>1.0f?sample_z/n:sample_z;
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
unsigned SudekiMpLanStoryInputTakeMelee(void *controller,void *actor,uint32_t transaction) {
    float x,z;
    if(!SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z) || !focused) return 0;
    return take_melee(GetTickCount());
}
void SudekiMpLanStoryInputMuteMovement(void) {
    if(!native_thread_exact() || InterlockedCompareExchange(&callbacks,0,0)) return;
    sample_x=sample_z=0;
    melee_first=melee_count=0;
    look_x=look_y=0; look_x_at=look_y_at=look_sample_at=0;
}

BOOL SudekiMpLanStoryInputOrbit(void *controller,void *actor,uint32_t transaction,
    float *yaw,float *pitch) {
    if(yaw) *yaw=0;
    if(pitch) *pitch=0;
    float x,z;
    if(!yaw || !pitch || !SudekiMpLanStoryInputSample(controller,actor,transaction,&x,&z)) return FALSE;
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

static BOOL install_input(HMODULE image,unsigned client_player,BOOL host) {
    uint8_t *b=(uint8_t *)image;
    uint8_t handler[]={0x56,0x57,0x8b,0xf9,0x8b,0x0d,0,0,0,0,0x80,0x79,0x56,0,0x74,0x5c};
    uint8_t constructor[]={0x89,0x0d,0,0,0,0,0xc7,0x06,0,0,0,0,
        0xc7,0x46,0x24,0,0,0,0,0xc7,0x46,0x2c,0,0,0,0};
    HostTitleIdentity before={0};
    if(!image || (host?client_player!=0u:(client_player<1u || client_player>3u))) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if((base && base!=b) || InterlockedCompareExchange(&retained,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(host) {
        DWORD pid=0; HWND window=GetActiveWindow();
        if(!window || GetWindowThreadProcessId(window,&pid)!=GetCurrentThreadId() ||
            pid!=GetCurrentProcessId() || !host_title_identity(b,&before)) {
            SetLastError(ERROR_NOT_READY); return FALSE;
        }
    }
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
    host_route=host; host_controller=host?before.controller:NULL;
    if(host) InterlockedExchange(&native_thread,(LONG)GetCurrentThreadId());
    quick_down=quick_pending=FALSE;
    InterlockedExchange(&retained,1);
    if(!SudekiMpInstallPointerHook(&input_hook,(void **)(b+INPUT_VT),
            b+INPUT_HANDLER,story_input)) {
        DWORD error=GetLastError();
        if(!SudekiMpLanStoryInputUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
    InterlockedExchange(&installed,1);
    if(host && !host_install_published(&before)) return FALSE;
    SudekiMpLogFormat("story_input event=install player=%u native_character_input=closed listener_offset=44 other_listener_slots=unchanged\r\n",player);
    return TRUE;
}
BOOL SudekiMpLanStoryInputInstall(HMODULE image,unsigned client_player) {
    return install_input(image,client_player,FALSE);
}
BOOL SudekiMpLanStoryInputInstallHostBeforeLoad(HMODULE image) {
    return install_input(image,0,TRUE);
}

BOOL SudekiMpLanStoryInputUninstall(void) {
    if(!InterlockedCompareExchange(&retained,0,0)) return TRUE;
    if(InterlockedCompareExchange(&callbacks,0,0) ||
        (InterlockedCompareExchange(&native_thread,0,0) && !native_thread_exact())) {
        return retain_failure(ERROR_BUSY);
    }
    clear_sample();
    if(focus_hook && !UnhookWindowsHookEx(focus_hook)) return retain_failure(GetLastError());
    focus_hook=NULL; input_window=NULL; focused=FALSE;
    memset(&input_window_owner,0,sizeof(input_window_owner));
    if(!SudekiMpRestorePointerHook(&input_hook)) return retain_failure(GetLastError());
    if(InterlockedCompareExchange(&callbacks,0,0)) {
        return retain_failure(ERROR_BUSY);
    }
    InterlockedExchange(&installed,0);
    InterlockedExchange(&retained,0);
    player=0;
    host_route=FALSE; host_controller=NULL;
    InterlockedExchange(&native_thread,0); InterlockedExchange(&fault,0);
    return TRUE;
}
