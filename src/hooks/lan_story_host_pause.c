#include "hooks/lan_story_host_pause.h"
#include "hooks/call_hook.h"
#include "hooks/lan_story_menu.h"
#include "engine/build_identity.h"
#include <stdint.h>
#include <string.h>
/* Sole owner of the M pause-enable call inside controller update (RVA
 * 27CF0). Other calls to 29410 disable pause and are intentionally native.
 * Call ABI: ESI=controller, DL=1, no stack arguments; result is unused.
 * The full branch tail and rel32 target are verified before installation. */
enum { PAUSE_CONTEXT=0x2836c, PAUSE_CALL=0x28382, PAUSE_TARGET=0x29410 };
static SudekiMpRelativeCallHook pause_hook;
static volatile LONG pause_thread;
static volatile LONG pause_callbacks;
static BOOL pause_enabled;
static const uint8_t pause_context[]={0xf6,0x83,0xd0,0x01,0,0,0x08,0x74,0x12,
    0xe8,0xc6,0x02,0,0,0x84,0xc0,0x74,0x09,0xb2,0x01,0x8b,0xf3};
static void __attribute__((used,noinline,cdecl,force_align_arg_pointer)) pause_notice(void) {
    InterlockedIncrement(&pause_callbacks);
    /* Installation can run on the loader thread; this verified controller
     * call establishes the native callback thread, like the runtime seam. */
    LONG thread=(LONG)GetCurrentThreadId();
    (void)InterlockedCompareExchange(&pause_thread,thread,0);
    if(pause_enabled && InterlockedCompareExchange(&pause_thread,0,0)==thread)
        SudekiMpLanStoryMenuNotice("Can't step away as host yet");
    InterlockedDecrement(&pause_callbacks);
}
static void __attribute__((naked,noinline)) pause_entry(void) {
    __asm__ volatile("pushfl; pushal; call _pause_notice; popal; popfl; ret");
}
BOOL SudekiMpLanStoryHostPauseInstall(HMODULE image) {
    if(pause_hook.installed || !image || !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    uint8_t *b=(uint8_t *)image;
    if(memcmp(b+PAUSE_CONTEXT,pause_context,sizeof(pause_context))) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    InterlockedExchange(&pause_thread,0); pause_enabled=TRUE;
    if(!SudekiMpInstallRelativeCallHook(&pause_hook,b+PAUSE_CALL,b+PAUSE_TARGET,(const void *)(uintptr_t)pause_entry)) {
        pause_enabled=FALSE; return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryHostPauseUninstall(void) {
    if(!pause_hook.installed) return TRUE;
    LONG thread=InterlockedCompareExchange(&pause_thread,0,0);
    if((thread && thread!=(LONG)GetCurrentThreadId()) || InterlockedCompareExchange(&pause_callbacks,0,0) ||
        !SudekiMpRestoreRelativeCallHook(&pause_hook)) return FALSE;
    pause_enabled=FALSE; return TRUE;
}
