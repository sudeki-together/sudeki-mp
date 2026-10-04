#include "hooks/lan_party_menu_native.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "SMP4 menu requires the supported x86 ABI"
#endif

enum {
    QUIT_OPEN_CALL=0x1db5d, QUIT_SHOW=0x1d700,
    QUIT_RENDER_CALL=0x28d572, QUIT_RENDER=0x1d690,
    QUIT_GLOBAL=0x408d68, QUIT_VTABLE=0x2c683c,
    NATIVE_PAUSE=0xfd610, PAUSE_REF=0x27490,
    SPEED_GLOBAL=0x408da0, TASK_REGISTRY=0x409d8c,
    TASK_SCHEDULER=0x409e14, WORLD_TASK=0x408d9c,
    PAUSE_NONE=0, PAUSE_OWNED=1, PAUSE_ACQUIRE_VERIFY=2,
    PAUSE_RELEASE_VERIFY=3
};
typedef void (__cdecl *NativePause)(BOOL paused,BOOL include_world);
typedef struct PauseLease {
    uint8_t *speed;
    void *registry,*scheduler,*world;
    uint16_t expected_count;
    unsigned phase;
} PauseLease;
static uint8_t *game_base;
static SudekiMpRelativeCallHook open_hook,render_hook;
static void *original_show __attribute__((used));
static void *original_render __attribute__((used));
static SudekiMpLanPartyMenuAdmission menu_admission;
static SudekiMpLanPartyMenuCallback menu_toggle,menu_frame;
static volatile LONG callbacks,stopping;
static DWORD native_thread;
static BOOL frame_active,ready;
static PauseLease pause_lease;
static struct {
    PauseLease owner;
    uint16_t held_count;
    BOOL prepared,released;
} pause_exit;

static BOOL retain(DWORD error) {
    HMODULE module;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanPartyMenuNativeUninstall,&module);
    SetLastError(error?error:ERROR_BUSY);
    return FALSE;
}

static BOOL readable(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t start=(uintptr_t)pointer;
    if(!pointer || !size || start>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE &&
        access!=PAGE_WRITECOPY && access!=PAGE_EXECUTE_READ &&
        access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return start+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL call_targets(const uint8_t *call,const void *target) {
    int32_t displacement;
    if(!readable(call,5) || *call!=0xe8) return FALSE;
    memcpy(&displacement,call+1,4);
    return call+5+displacement==target;
}
static BOOL hook_exact(const SudekiMpRelativeCallHook *hook,const void *entry) {
    return hook->installed && call_targets(hook->instruction,entry);
}
static BOOL quit_exact(void *screen) {
    return game_base && readable(screen,0x1d0) &&
        screen==*(void **)(game_base+QUIT_GLOBAL) &&
        *(void **)screen==game_base+QUIT_VTABLE;
}
static BOOL native_pause_exact(void) {
    static const uint8_t entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,
        0x83,0xec,0x0c,0x53,0x56,0x57,0x8b,0x3d};
    static const uint8_t reference[]={0x0f,0xb6,0xc9,0xf7,0xd9,0x1b,0xc9,
        0x81,0xe1,0x02,0x00,0xff,0xff,0x81,0xc1,0xff,0xff,0x00,0x00,
        0x66,0x01,0x48,0x2a,0x0f,0x95,0xc1,0x88,0x48,0x28};
    return game_base &&
        !memcmp(game_base+NATIVE_PAUSE,entry,sizeof(entry)) &&
        *(void **)(game_base+NATIVE_PAUSE+14)==game_base+TASK_REGISTRY &&
        game_base[0xfd75d]==0xa1 &&
        *(void **)(game_base+0xfd75e)==game_base+SPEED_GLOBAL &&
        call_targets(game_base+0xfd769,game_base+PAUSE_REF) &&
        !memcmp(game_base+PAUSE_REF,reference,sizeof(reference)) &&
        !memcmp(game_base+0xfd7a9,"\x5f\x5e\x5b\x8b\xe5\x5d\xc3",7);
}
static BOOL observe_pause(PauseLease *out,uint16_t *count) {
    PauseLease p={0};
    if(!game_base || !native_pause_exact()) return FALSE;
    p.speed=*(uint8_t **)(game_base+SPEED_GLOBAL);
    p.registry=*(void **)(game_base+TASK_REGISTRY);
    p.scheduler=*(void **)(game_base+TASK_SCHEDULER);
    p.world=*(void **)(game_base+WORLD_TASK);
    /* These singleton slots are the exact native transaction's owners. Their
     * identities are retained across the matched pause/unpause, not inferred
     * solely from readable memory. */
    if(!readable(p.speed,0x30) || !readable(p.registry,0x40) ||
        !readable(p.scheduler,0x20) || !readable(p.world,0x24)) return FALSE;
    uint16_t value=*(uint16_t *)(p.speed+0x2a);
    if(p.speed[0x28]!=(value!=0)) return FALSE;
    if(out) *out=p;
    if(count) *count=value;
    return TRUE;
}
static BOOL lease_identity(const PauseLease *p) {
    return pause_lease.speed==p->speed && pause_lease.registry==p->registry &&
        pause_lease.scheduler==p->scheduler && pause_lease.world==p->world;
}

__attribute__((noinline,used,force_align_arg_pointer))
static BOOL handle_escape(void *screen,unsigned visible) {
    BOOL handled=FALSE;
    InterlockedIncrement(&callbacks);
    if(ready && !InterlockedCompareExchange(&stopping,0,0) && native_thread &&
        native_thread==GetCurrentThreadId() && visible==1 && quit_exact(screen) &&
        !((uint8_t *)screen)[0x1c2] && *(unsigned *)((uint8_t *)screen+0x10)==0 &&
        menu_admission && menu_admission()) {
        handled=TRUE;
        menu_toggle();
    }
    InterlockedDecrement(&callbacks);
    return handled;
}
__attribute__((naked,noinline,used))
static void open_entry(void) {
    __asm__ volatile(
        "pushfl\n\tpushal\n\tpushl 40(%esp)\n\tpushl %esi\n\t"
        "call _handle_escape\n\taddl $8,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tret $4\n\t"
        "1: popal\n\tpopfl\n\tjmp *_original_show\n\t");
}
__attribute__((noinline,used,force_align_arg_pointer))
static void handle_frame(void) {
    InterlockedIncrement(&callbacks);
    if(ready && !InterlockedCompareExchange(&stopping,0,0) &&
        !frame_active && quit_exact(*(void **)(game_base+QUIT_GLOBAL))) {
        if(!native_thread) native_thread=GetCurrentThreadId();
        if(native_thread==GetCurrentThreadId()) {
            frame_active=TRUE;
            menu_frame();
            frame_active=FALSE;
        }
    }
    InterlockedDecrement(&callbacks);
}
__attribute__((naked,noinline,used))
static void render_entry(void) {
    __asm__ volatile(
        "call *_original_render\n\tpushfl\n\tpushal\n\t"
        "call _handle_frame\n\tpopal\n\tpopfl\n\tret\n\t");
}

BOOL SudekiMpLanPartyMenuNativePauseExact(BOOL *paused) {
    PauseLease observed; uint16_t count;
    if(!paused || !frame_active || native_thread!=GetCurrentThreadId() ||
        !observe_pause(&observed,&count)) return FALSE;
    if(pause_lease.phase!=PAUSE_NONE && !lease_identity(&observed)) return FALSE;
    *paused=count!=0;
    return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeOwnsPause(void) {
    return pause_lease.phase!=PAUSE_NONE;
}
BOOL SudekiMpLanPartyMenuNativeObserveOwnedPause(BOOL *paused) {
    PauseLease observed; uint16_t count;
    if(!paused || !ready || InterlockedCompareExchange(&stopping,0,0) ||
        !native_thread || native_thread!=GetCurrentThreadId() || pause_exit.prepared ||
        pause_lease.phase!=PAUSE_OWNED ||
        !hook_exact(&open_hook,open_entry) || !hook_exact(&render_hook,render_entry) ||
        !observe_pause(&observed,&count) || !lease_identity(&observed) ||
        !count || count!=pause_lease.expected_count) return FALSE;
    *paused=TRUE; return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeSetPaused(BOOL paused) {
    PauseLease observed; uint16_t before,after;
    if(!ready || !frame_active || pause_exit.prepared || native_thread!=GetCurrentThreadId() ||
        !hook_exact(&open_hook,open_entry) || !hook_exact(&render_hook,render_entry) ||
        !observe_pause(&observed,&before)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(pause_lease.phase!=PAUSE_NONE && !lease_identity(&observed)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(pause_lease.phase==PAUSE_RELEASE_VERIFY) {
        if(before!=pause_lease.expected_count) { SetLastError(ERROR_BUSY); return FALSE; }
        memset(&pause_lease,0,sizeof(pause_lease));
    } else if(pause_lease.phase==PAUSE_ACQUIRE_VERIFY) {
        if(before!=pause_lease.expected_count) { SetLastError(ERROR_BUSY); return FALSE; }
        pause_lease.phase=PAUSE_OWNED;
    }
    if(paused) {
        if(pause_lease.phase==PAUSE_OWNED) return before!=0;
        if(before==UINT16_MAX) { SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE; }
        pause_lease=observed;
        pause_lease.phase=PAUSE_ACQUIRE_VERIFY;
        pause_lease.expected_count=(uint16_t)(before+1);
    } else {
        if(pause_lease.phase==PAUSE_NONE) return TRUE;
        if(before==0) { SetLastError(ERROR_BUSY); return FALSE; }
        pause_lease.phase=PAUSE_RELEASE_VERIFY;
        pause_lease.expected_count=(uint16_t)(before-1);
    }
    /* Retain ownership before entering native code. The native function also
     * suspends/resumes scheduled world tasks; a raw speed/byte write cannot
     * reproduce this operation. No cast timing hook is changed. */
    ((NativePause)(game_base+NATIVE_PAUSE))(paused!=FALSE,TRUE);
    if(!observe_pause(&observed,&after) || !lease_identity(&observed) ||
        after!=pause_lease.expected_count) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(paused) pause_lease.phase=PAUSE_OWNED;
    else memset(&pause_lease,0,sizeof(pause_lease));
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

static BOOL exit_identity(const PauseLease *p) {
    return pause_exit.owner.speed==p->speed && pause_exit.owner.registry==p->registry &&
        pause_exit.owner.scheduler==p->scheduler && pause_exit.owner.world==p->world;
}
static BOOL exit_observe(PauseLease *observed,uint16_t *count) {
    /* Prepare proved both callback owners before granting this token. Final
     * release/rollback invokes only the independently validated native pause
     * primitive. A failed hook teardown may already have restored one CALL
     * and set stopping; that must not prevent re-pausing this same world.
     * Successful uninstall clears the token and native thread instead. */
    return ready && pause_exit.prepared && pause_exit.held_count && !frame_active &&
        native_thread==GetCurrentThreadId() && !InterlockedCompareExchange(&callbacks,0,0) &&
        observe_pause(observed,count) && exit_identity(observed);
}
BOOL SudekiMpLanPartyMenuNativePreparePauseExit(void) {
    PauseLease observed; uint16_t count;
    if(!ready || !frame_active || native_thread!=GetCurrentThreadId() ||
        !hook_exact(&open_hook,open_entry) || !hook_exact(&render_hook,render_entry) ||
        !observe_pause(&observed,&count) || !count || pause_lease.phase!=PAUSE_OWNED ||
        !lease_identity(&observed) || pause_lease.expected_count!=count) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(pause_exit.prepared) {
        if(pause_exit.released || !exit_identity(&observed) || pause_exit.held_count!=count)
            return retain(ERROR_BUSY);
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    pause_exit.owner=observed; pause_exit.held_count=count;
    pause_exit.prepared=TRUE; pause_exit.released=FALSE;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeExitPauseExact(BOOL *paused) {
    PauseLease observed; uint16_t count;
    if(!paused || !exit_observe(&observed,&count)) return FALSE;
    BOOL held=count==pause_exit.held_count && lease_identity(&observed) &&
        pause_lease.expected_count==count &&
        (pause_lease.phase==PAUSE_OWNED || pause_lease.phase==PAUSE_ACQUIRE_VERIFY);
    BOOL released=count==(uint16_t)(pause_exit.held_count-1u) &&
        ((pause_exit.released && pause_lease.phase==PAUSE_NONE) ||
         (pause_lease.phase==PAUSE_RELEASE_VERIFY && lease_identity(&observed) &&
          pause_lease.expected_count==count));
    if(!held && !released) return FALSE;
    *paused=count!=0u; return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeFinishPauseExit(void) {
    PauseLease observed; uint16_t before,after;
    if(!exit_observe(&observed,&before)) return retain(ERROR_INVALID_STATE);
    uint16_t target=(uint16_t)(pause_exit.held_count-1u);
    if(pause_lease.phase==PAUSE_NONE) {
        if(!pause_exit.released || before!=target) return retain(ERROR_BUSY);
        SetLastError(ERROR_SUCCESS); return TRUE;
    }
    if(!lease_identity(&observed)) return retain(ERROR_BUSY);
    if(pause_lease.phase==PAUSE_RELEASE_VERIFY) {
        if(before!=target || pause_lease.expected_count!=target) return retain(ERROR_BUSY);
        memset(&pause_lease,0,sizeof(pause_lease)); pause_exit.released=TRUE;
        SetLastError(ERROR_SUCCESS); return TRUE;
    }
    if(before!=pause_exit.held_count || pause_lease.expected_count!=before ||
        (pause_lease.phase!=PAUSE_OWNED && pause_lease.phase!=PAUSE_ACQUIRE_VERIFY))
        return retain(ERROR_BUSY);
    /* Retain the prepared owner through release. No ordinary frame path can
     * claim or retire this reference until exit completes or reacquires it. */
    pause_lease.phase=PAUSE_RELEASE_VERIFY; pause_lease.expected_count=target;
    ((NativePause)(game_base+NATIVE_PAUSE))(FALSE,TRUE);
    if(!observe_pause(&observed,&after) || !exit_identity(&observed) ||
        !lease_identity(&observed) || after!=target) return retain(ERROR_BUSY);
    memset(&pause_lease,0,sizeof(pause_lease)); pause_exit.released=TRUE;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeReacquirePauseExit(void) {
    PauseLease observed; uint16_t before,after;
    if(!exit_observe(&observed,&before)) return retain(ERROR_INVALID_STATE);
    if(pause_lease.phase==PAUSE_OWNED || pause_lease.phase==PAUSE_ACQUIRE_VERIFY) {
        if(!lease_identity(&observed) || before!=pause_exit.held_count ||
            pause_lease.expected_count!=before) return retain(ERROR_BUSY);
        pause_lease.phase=PAUSE_OWNED; pause_exit.released=FALSE;
        SetLastError(ERROR_SUCCESS); return TRUE;
    }
    uint16_t baseline=(uint16_t)(pause_exit.held_count-1u);
    if(before!=baseline ||
        !((pause_exit.released && pause_lease.phase==PAUSE_NONE) ||
          (pause_lease.phase==PAUSE_RELEASE_VERIFY && lease_identity(&observed) &&
           pause_lease.expected_count==baseline))) return retain(ERROR_BUSY);
    pause_lease=observed; pause_lease.phase=PAUSE_ACQUIRE_VERIFY;
    pause_lease.expected_count=pause_exit.held_count;
    ((NativePause)(game_base+NATIVE_PAUSE))(TRUE,TRUE);
    if(!observe_pause(&observed,&after) || !exit_identity(&observed) ||
        !lease_identity(&observed) || after!=pause_exit.held_count) return retain(ERROR_BUSY);
    pause_lease.phase=PAUSE_OWNED; pause_exit.released=FALSE;
    SetLastError(ERROR_SUCCESS); return TRUE;
}

BOOL SudekiMpLanPartyMenuNativeInstall(HMODULE module,
    SudekiMpLanPartyMenuAdmission admission,
    SudekiMpLanPartyMenuCallback toggle,
    SudekiMpLanPartyMenuCallback frame) {
    static const uint8_t show[]={0x53,0x8b,0x5c,0x24,0x08,0x57,0x8b,0x3d};
    static const uint8_t render[]={0x57,0x8b,0xf8,0x80,0xbf,0xc2,1,0,0,0};
    if(!module || !admission || !toggle || !frame || ready ||
        open_hook.installed || render_hook.installed || pause_lease.phase ||
        InterlockedCompareExchange(&callbacks,0,0) ||
        (game_base && game_base!=(uint8_t *)module) ||
        !SudekiMpCheckLoadedExecutable(module)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    game_base=(uint8_t *)module;
    if(!native_pause_exact() || memcmp(game_base+QUIT_SHOW,show,sizeof(show)) ||
        *(void **)(game_base+QUIT_SHOW+8)!=game_base+0x408d1c ||
        memcmp(game_base+QUIT_RENDER,render,sizeof(render)) ||
        game_base[QUIT_OPEN_CALL-2]!=0x6a || game_base[QUIT_OPEN_CALL-1]!=1 ||
        !call_targets(game_base+QUIT_OPEN_CALL,game_base+QUIT_SHOW) ||
        !call_targets(game_base+QUIT_RENDER_CALL,game_base+QUIT_RENDER)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    original_show=game_base+QUIT_SHOW;
    original_render=game_base+QUIT_RENDER;
    menu_admission=admission; menu_toggle=toggle; menu_frame=frame;
    native_thread=0; ready=FALSE; InterlockedExchange(&stopping,0);
    if(!SudekiMpInstallRelativeCallHook(&render_hook,game_base+QUIT_RENDER_CALL,
            original_render,render_entry) ||
        !SudekiMpInstallRelativeCallHook(&open_hook,game_base+QUIT_OPEN_CALL,
            original_show,open_entry)) {
        DWORD error=GetLastError();
        if(!SudekiMpLanPartyMenuNativeUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
    ready=TRUE;
    SudekiMpLogWrite("lan_party_menu_native event=install escape=native_eligible "
        "menu_pause=disabled session_pause=native_owned_reference\r\n");
    return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeUninstall(void) {
    if(pause_lease.phase!=PAUSE_NONE || InterlockedCompareExchange(&callbacks,0,0)) {
        return retain(ERROR_BUSY);
    }
    InterlockedExchange(&stopping,1);
    BOOL restored=SudekiMpRestoreRelativeCallHook(&open_hook);
    DWORD error=restored?ERROR_SUCCESS:GetLastError();
    if(!SudekiMpRestoreRelativeCallHook(&render_hook)) {
        if(!error) error=GetLastError();
        restored=FALSE;
    }
    if(!restored || InterlockedCompareExchange(&callbacks,0,0)) {
        /* Retain immutable callbacks/base for any live or cached native entry. */
        return retain(error);
    }
    /* Keep native entry pointers/image immutable for a thread that fetched the
     * old CALL before restoration. No native trampoline is allocated/freed. */
    ready=FALSE;
    menu_admission=NULL; menu_toggle=menu_frame=NULL;
    native_thread=0; frame_active=FALSE;
    memset(&pause_exit,0,sizeof(pause_exit));
    SetLastError(ERROR_SUCCESS); return TRUE;
}
