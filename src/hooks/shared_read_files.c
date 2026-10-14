#include "hooks/shared_read_files.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>

/* Supported image: IAT slot RVA 0x29a0ac = kernel32!CreateFileA (all 24
 * CreateFileA call sites go through it; the sound engine's wave-bank opens at
 * RVA 0x28870b/0x28878c/0x2890de pass GENERIC_READ, share 0, OPEN_EXISTING). */
enum { RVA_IAT=0x29a0acu };
typedef HANDLE (WINAPI *CreateFileFunction)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static SudekiMpPointerHook hook;
static CreateFileFunction original;
static volatile LONG callbacks,shared_logs;

__attribute__((noinline))
static HANDLE WINAPI shared_create_file(LPCSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,
    DWORD disposition,DWORD flags,HANDLE template_file) {
    InterlockedIncrement(&callbacks);
    BOOL widen=access==GENERIC_READ && share==0u && disposition==OPEN_EXISTING;
    HANDLE result=original(name,access,widen?FILE_SHARE_READ:share,security,disposition,flags,template_file);
    if(widen && InterlockedIncrement(&shared_logs)<=8) {
        DWORD error=GetLastError();
        SudekiMpLogFormat("shared_read_files event=shared name=%s opened=%u\r\n",name?name:"-",result!=INVALID_HANDLE_VALUE);
        SetLastError(error);
    }
    InterlockedDecrement(&callbacks);
    return result;
}
BOOL SudekiMpSharedReadFilesInstall(HMODULE game) {
    uint8_t *b=(uint8_t *)game;
    if(!b || original || hook.installed || !SudekiMpCheckLoadedExecutable(game)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    FARPROC expected=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateFileA");
    if(!expected) return FALSE;
    original=(CreateFileFunction)(uintptr_t)expected;
    if(!SudekiMpInstallPointerHook(&hook,(void **)(b+RVA_IAT),(const void *)(uintptr_t)expected,
        (const void *)(uintptr_t)shared_create_file)) { original=NULL; return FALSE; }
    SudekiMpLogWrite("shared_read_files event=installed seam=create_file_iat:0x29a0ac policy=read_only_open_existing_share_read\r\n");
    return TRUE;
}
BOOL SudekiMpSharedReadFilesUninstall(void) {
    if(!hook.installed) return TRUE;
    if(!SudekiMpRestorePointerHook(&hook) || InterlockedCompareExchange(&callbacks,0,0)) {
        HMODULE self;
        (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&hook,&self);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    return TRUE;
}
