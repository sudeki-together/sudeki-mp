#include "hooks/lan_story_area_close.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>
#include <limits.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Area close admission requires the supported x86 ABI"
#endif
enum { CLOSE_SITE=0x28d791, CLOSE_TAIL=0x28d7a7, NATIVE_WNDPROC=0x28d670,
    RUN_FLAG=0x3c30b0, WORLD_SLOT=0x408d10, ROOT_SLOT=0x3c2f38 };
static uint8_t *close_base;
static SudekiMpInlineHook close_hook;
static void *close_original __attribute__((used)),*close_continuation __attribute__((used));
static SRWLOCK close_lock=SRWLOCK_INIT;
static DWORD close_startup_thread,close_native_thread;
static const void *close_consumer;
static SudekiMpStoryAreaCloseReceipt close_record;
static uint64_t close_serial;
static BOOL close_installed,close_stopping,close_unknown,close_posting;
static unsigned close_callbacks;
/* A private per-window marker distinguishes a reused HWND. Its value is an
 * opaque, never-repeated 32-bit ticket, not a dereferenceable allocation. */
static const wchar_t close_property[]=L"SudekiMP.StoryAreaClose.Owner.v1";
static BOOL close_readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL close_bytes(uint8_t *b,unsigned at,const void *p,size_t n) {
    return close_readable(b+at,n) && !memcmp(b+at,p,n);
}
static void close_relocate(uint8_t *p,uint8_t *b,unsigned at) {
    uint32_t value=(uint32_t)(uintptr_t)(b+at);memcpy(p,&value,4);
}
static BOOL close_startup_exact(uint8_t *b) {
    return close_readable(b+WORLD_SLOT,4) && !*(void **)(b+WORLD_SLOT) &&
        close_readable(b+ROOT_SLOT,4) && !*(void **)(b+ROOT_SLOT);
}
static BOOL close_image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!close_readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !close_readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    uint8_t prologue[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x8b,0x0d,0,0,0,0,
        0x83,0xec,0x44,0x53,0x8b,0x5d,0x0c,0x56,0x57,0x83,0xfb,0x10,
        0x0f,0x87,0x24,1,0,0,0x0f,0x84,0xfd,0,0,0};
    uint8_t close_body[]={0xc6,5,0,0,0,0,0,0xe8,0x23,0xfe,0xff,0xff,
        0x8b,0x45,8,0x50,0xff,0x15,0,0,0,0,
        0x33,0xc0,0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc2,0x10,0};
    close_relocate(prologue+8,b,0x409e30);
    close_relocate(close_body+2,b,RUN_FLAG);close_relocate(close_body+18,b,0x29a20c);
    return close_startup_exact(b) && close_bytes(b,NATIVE_WNDPROC,prologue,sizeof(prologue)) &&
        close_bytes(b,CLOSE_SITE,close_body,sizeof(close_body));
}
static BOOL close_healthy(HMODULE image) {
    if(!close_base || close_base!=(uint8_t *)image || !close_installed || close_unknown) return FALSE;
    if(!close_hook.installed || !close_readable(close_base+CLOSE_SITE,close_hook.length) ||
        memcmp(close_base+CLOSE_SITE,close_hook.replacement,close_hook.length)) {
        close_unknown=TRUE;return FALSE;
    }
    return TRUE;
}
static BOOL close_witness(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_post_original_exact && w->dispatch_serial &&
        w->native_thread_id==GetCurrentThreadId() &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static BOOL close_window_exact(HWND window) {
    DWORD process=0;
    return window && GetWindowThreadProcessId(window,&process)==GetCurrentThreadId() &&
        process==GetCurrentProcessId() &&
        /* Match the game's RegisterClassExA/DefWindowProcA procedure identity. */
        (uintptr_t)GetWindowLongPtrA(window,GWLP_WNDPROC)==(uintptr_t)(close_base+NATIVE_WNDPROC);
}
static BOOL close_marker_exact(void) {
    return close_record.ticket && close_record.ticket<=UINT32_MAX &&
        GetPropW(close_record.window,close_property)==(HANDLE)(uintptr_t)close_record.ticket;
}
static BOOL close_owner(HMODULE image,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    return ticket && ticket==close_record.ticket && consumer && consumer==close_consumer &&
        close_native_thread==GetCurrentThreadId() && !close_callbacks && !close_posting &&
        close_witness(w) && close_healthy(image) && close_window_exact(close_record.window) && close_marker_exact();
}
static BOOL __attribute__((used,noinline)) close_should_defer(HWND window) {
    DWORD saved=GetLastError();BOOL defer=FALSE;
    AcquireSRWLockExclusive(&close_lock);++close_callbacks;
    DWORD thread=GetCurrentThreadId();
    if(!close_native_thread) close_native_thread=thread;
    else if(close_native_thread!=thread) close_unknown=TRUE;
    if(close_record.ticket) {
        defer=TRUE;
        if(!close_healthy((HMODULE)close_base) || window!=close_record.window ||
            (close_record.phase!=SUDEKIMP_AREA_CLOSE_DISPATCHED &&
             (!close_marker_exact() || !close_window_exact(window)))) close_unknown=TRUE;
        if(!close_unknown) {
            if(close_record.requests==UINT_MAX) close_unknown=TRUE;
            else {
                ++close_record.requests;
                if(close_record.phase==SUDEKIMP_AREA_CLOSE_HELD)
                    close_record.phase=SUDEKIMP_AREA_CLOSE_REQUESTED;
                else if(close_record.phase==SUDEKIMP_AREA_CLOSE_POSTED && !close_posting) {
                    if(RemovePropW(window,close_property)!=(HANDLE)(uintptr_t)close_record.ticket) close_unknown=TRUE;
                    else {
                        /* Publish before original cleanup can pump another close. */
                        close_record.phase=SUDEKIMP_AREA_CLOSE_DISPATCHED;defer=FALSE;
                    }
                }
            }
        }
    }
    --close_callbacks;ReleaseSRWLockExclusive(&close_lock);SetLastError(saved);return defer;
}
static void __attribute__((naked,noinline)) close_entry(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 8(%ebp),%eax; mov 8(%eax),%eax; mov %eax,(%esp);"
        "call _close_should_defer; test %eax,%eax; jz 1f;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; jmp *_close_continuation;"
        "1: add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; jmp *_close_original");
}
BOOL SudekiMpLanStoryAreaCloseHold(HMODULE image,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *w,HWND window,uint64_t *out) {
    if(!consumer || !out || !close_witness(w)) return FALSE;
    AcquireSRWLockExclusive(&close_lock);BOOL ok=FALSE;
    if(close_healthy(image) && !close_stopping && !close_callbacks && !close_posting &&
        (!close_native_thread || close_native_thread==GetCurrentThreadId()) && close_window_exact(window)) {
        if(close_record.ticket) {
            if(close_consumer==consumer && close_record.window==window &&
                close_record.phase<=SUDEKIMP_AREA_CLOSE_REQUESTED && close_marker_exact()) {*out=close_record.ticket;ok=TRUE;}
        } else if(close_serial<UINT32_MAX && !GetPropW(window,close_property)) {
            close_native_thread=GetCurrentThreadId();close_consumer=consumer;
            close_record=(SudekiMpStoryAreaCloseReceipt){.ticket=++close_serial,.window=window,
                .phase=SUDEKIMP_AREA_CLOSE_HELD};
            if(SetPropW(window,close_property,(HANDLE)(uintptr_t)close_record.ticket) && close_marker_exact()) {
                *out=close_record.ticket;ok=TRUE;
            } else if(!GetPropW(window,close_property)) {
                memset(&close_record,0,sizeof(close_record));close_consumer=NULL;
            } else close_unknown=TRUE; /* Do not overwrite/remove a foreign marker. */
        }
    }
    ReleaseSRWLockExclusive(&close_lock);return ok;
}
BOOL SudekiMpLanStoryAreaCloseRead(HMODULE image,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket,SudekiMpStoryAreaCloseReceipt *out) {
    if(!out) return FALSE;
    AcquireSRWLockExclusive(&close_lock);BOOL ok=close_owner(image,consumer,w,ticket);
    if(ok) *out=close_record;
    ReleaseSRWLockExclusive(&close_lock);return ok;
}
BOOL SudekiMpLanStoryAreaCloseRelease(HMODULE image,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    AcquireSRWLockExclusive(&close_lock);
    BOOL ok=close_owner(image,consumer,w,ticket) && close_record.phase==SUDEKIMP_AREA_CLOSE_HELD;
    if(ok) {
        if(RemovePropW(close_record.window,close_property)!=(HANDLE)(uintptr_t)close_record.ticket) {
            close_unknown=TRUE;ok=FALSE;
        } else {memset(&close_record,0,sizeof(close_record));close_consumer=NULL;}
    }
    ReleaseSRWLockExclusive(&close_lock);return ok;
}
BOOL SudekiMpLanStoryAreaClosePostDrainedClose(HMODULE image,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    AcquireSRWLockExclusive(&close_lock);
    if(!close_owner(image,consumer,w,ticket) || close_record.phase!=SUDEKIMP_AREA_CLOSE_REQUESTED) {
        ReleaseSRWLockExclusive(&close_lock);SetLastError(ERROR_BUSY);return FALSE;
    }
    HWND window=close_record.window;close_posting=TRUE;
    ReleaseSRWLockExclusive(&close_lock);
    /* No synchronous SendMessage/native WndProc call while controller code
     * still owns native stack frames. This API only queues the user intent. */
    BOOL posted=PostMessageW(window,WM_CLOSE,0,0);DWORD error=GetLastError();
    AcquireSRWLockExclusive(&close_lock);close_posting=FALSE;
    BOOL ok=close_healthy(image) && close_record.ticket==ticket && close_consumer==consumer && close_marker_exact();
    if(ok && posted) close_record.phase=SUDEKIMP_AREA_CLOSE_POSTED;
    ReleaseSRWLockExclusive(&close_lock);
    SetLastError(ok?(posted?ERROR_SUCCESS:error):ERROR_INVALID_STATE);return ok && posted;
}
static BOOL close_pin(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryAreaCloseUninstall,&self);
    SetLastError(error?error:ERROR_BUSY);return FALSE;
}
BOOL SudekiMpLanStoryAreaCloseUninstall(const SudekiMpControlUpdateDispatchWitness *w) {
    AcquireSRWLockExclusive(&close_lock);
    if(!close_base) {ReleaseSRWLockExclusive(&close_lock);return TRUE;}
    close_stopping=TRUE;DWORD error=ERROR_SUCCESS;
    if(close_callbacks || close_posting || close_record.ticket || close_unknown ||
        (close_native_thread?(close_native_thread!=GetCurrentThreadId() || !close_witness(w)):
        (close_startup_thread!=GetCurrentThreadId() || !close_startup_exact(close_base)))) error=ERROR_BUSY;
    if(!error && !SudekiMpRestoreInlineHook(&close_hook)) error=GetLastError();
    if(!error) {
        close_base=NULL;close_original=close_continuation=NULL;close_consumer=NULL;
        close_startup_thread=close_native_thread=0;close_installed=FALSE;
    }
    ReleaseSRWLockExclusive(&close_lock);return error?close_pin(error):TRUE;
}
BOOL SudekiMpLanStoryAreaCloseInstall(HMODULE image) {
    AcquireSRWLockExclusive(&close_lock);
    if(close_base || close_installed || close_unknown || close_serial>=UINT32_MAX || !close_image_exact(image)) {
        ReleaseSRWLockExclusive(&close_lock);SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    close_base=(uint8_t *)image;close_startup_thread=GetCurrentThreadId();close_stopping=FALSE;
    close_continuation=close_base+CLOSE_TAIL;
    uint8_t expected[]={0xc6,5,0,0,0,0,0};close_relocate(expected+2,close_base,RUN_FLAG);
    BOOL ok=SudekiMpInstallInlineHook(&close_hook,close_base+CLOSE_SITE,expected,sizeof(expected),
        (void *)(uintptr_t)close_entry);
    close_original=close_hook.trampoline;DWORD error=GetLastError();
    if(ok) close_installed=TRUE;
    ReleaseSRWLockExclusive(&close_lock);
    if(!ok) {(void)SudekiMpLanStoryAreaCloseUninstall(NULL);SetLastError(error);}
    return ok;
}
