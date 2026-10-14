#include "hooks/profile_folder.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <shlobj.h>
#include <stdint.h>
#include <string.h>

/* Supported image: the three native callers (RVA 0x28c628, 0x28d240,
 * 0x28e9d3) all pass CSIDL_APPDATA with fCreate=1. */
enum { RVA_IAT=0x29a1e8u };
typedef BOOL (WINAPI *SpecialFolderFunction)(HWND,LPSTR,int,BOOL);
static SudekiMpPointerHook hook;
static SpecialFolderFunction original;
static char folder_ansi[MAX_PATH];
static volatile LONG callbacks,logs;

__attribute__((noinline))
static BOOL WINAPI profile_folder(HWND owner,LPSTR path,int id,BOOL create) {
    InterlockedIncrement(&callbacks);
    BOOL result;
    if(path && (id&0xff)==CSIDL_APPDATA && folder_ansi[0]) {
        if(create) CreateDirectoryA(folder_ansi,NULL);
        strcpy(path,folder_ansi); result=TRUE;
        if(InterlockedIncrement(&logs)<=4) {
            DWORD error=GetLastError();
            SudekiMpLogFormat("profile_folder event=redirected id=0x%x folder=%s\r\n",(unsigned)id,folder_ansi);
            SetLastError(error);
        }
    } else result=original(owner,path,id,create);
    InterlockedDecrement(&callbacks);
    return result;
}
BOOL SudekiMpProfileFolderInstall(HMODULE game,const wchar_t *folder) {
    uint8_t *b=(uint8_t *)game;
    if(!b || !folder || !folder[0] || original || hook.installed || !SudekiMpCheckLoadedExecutable(game)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    /* The native buffer is MAX_PATH ANSI; the folder must round-trip exactly. */
    BOOL lossy=FALSE;
    int n=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,folder,-1,folder_ansi,MAX_PATH-32,NULL,&lossy);
    if(n<=1 || lossy) { folder_ansi[0]=0; SetLastError(ERROR_INVALID_DATA); return FALSE; }
    CreateDirectoryA(folder_ansi,NULL);
    FARPROC expected=GetProcAddress(GetModuleHandleW(L"shell32.dll"),"SHGetSpecialFolderPathA");
    if(!expected) { HMODULE shell=LoadLibraryW(L"shell32.dll"); expected=shell?GetProcAddress(shell,"SHGetSpecialFolderPathA"):NULL; }
    if(!expected) { folder_ansi[0]=0; return FALSE; }
    original=(SpecialFolderFunction)(uintptr_t)expected;
    if(!SudekiMpInstallPointerHook(&hook,(void **)(b+RVA_IAT),(const void *)(uintptr_t)expected,
        (const void *)(uintptr_t)profile_folder)) { original=NULL; folder_ansi[0]=0; return FALSE; }
    SudekiMpLogFormat("profile_folder event=installed folder=%s seam=special_folder_iat:0x%06x\r\n",folder_ansi,RVA_IAT);
    return TRUE;
}
BOOL SudekiMpProfileFolderUninstall(void) {
    if(!hook.installed) return TRUE;
    if(!SudekiMpRestorePointerHook(&hook) || InterlockedCompareExchange(&callbacks,0,0)) {
        HMODULE self;
        (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&hook,&self);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    return TRUE;
}
