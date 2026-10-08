#include "hooks/window_title.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

/* Supported image: the main window is created at RVA 0x28da62 by
 * CALL [0x29a220] (user32!CreateWindowExA) with class "Render Window"
 * (RVA 0x2c2514) and window name "Sudeki" (RVA 0x2c21a4); the call returns to
 * RVA 0x28da68. The IAT slot is shared by every CreateWindowExA caller, so the
 * replacement only changes the name for that exact return address and name. */
enum { RVA_IAT=0x29a220u, RVA_CALL=0x28da62u, RVA_RETURN=0x28da68u, RVA_NAME=0x2c21a4u, RVA_CLASS=0x2c2514u };
typedef HWND (WINAPI *CreateWindowFunction)(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
static SudekiMpPointerHook hook;
static CreateWindowFunction original;
static uint8_t *base;
static char title[96];
static volatile LONG callbacks;

__attribute__((noinline))
static HWND WINAPI titled_window(DWORD ex,LPCSTR cls,LPCSTR name,DWORD style,int x,int y,int w,int h,
    HWND parent,HMENU menu,HINSTANCE instance,LPVOID param) {
    InterlockedIncrement(&callbacks);
    BOOL ours=__builtin_return_address(0)==base+RVA_RETURN && name==(LPCSTR)(base+RVA_NAME) && title[0];
    HWND result=original(ex,cls,ours?title:name,style,x,y,w,h,parent,menu,instance,param);
    if(ours) {
        DWORD error=GetLastError();
        SudekiMpLogFormat("window_title event=applied title=\"%s\" created=%u\r\n",title,result!=NULL);
        SetLastError(error);
    }
    InterlockedDecrement(&callbacks);
    return result;
}
BOOL SudekiMpWindowTitleInstall(HMODULE game,const wchar_t *config_path) {
    wchar_t wide[96]={0};
    if(!config_path) return TRUE;
    GetPrivateProfileStringW(L"SudekiMP",L"WindowTitle",L"",wide,95,config_path);
    if(!wide[0]) return TRUE; /* not requested */
    /* Printable ASCII only: the native call is the ANSI CreateWindowExA. */
    unsigned n=0;
    for(;n<95u && wide[n];++n) { if(wide[n]<0x20 || wide[n]>0x7e) { SetLastError(ERROR_INVALID_DATA); return FALSE; } title[n]=(char)wide[n]; }
    title[n]=0;
    uint8_t *b=(uint8_t *)game;
    static const uint8_t call[]={0xff,0x15};
    if(!b || original || hook.installed || !SudekiMpCheckLoadedExecutable(game) ||
        memcmp(b+RVA_CALL,call,sizeof(call)) || *(uint32_t *)(b+RVA_CALL+2u)!=(uint32_t)(uintptr_t)(b+RVA_IAT) ||
        memcmp(b+RVA_NAME,"Sudeki",7) || memcmp(b+RVA_CLASS,"Render Window",14)) {
        title[0]=0; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    FARPROC expected=GetProcAddress(GetModuleHandleW(L"user32.dll"),"CreateWindowExA");
    if(!expected) { title[0]=0; return FALSE; }
    base=b; original=(CreateWindowFunction)(uintptr_t)expected;
    if(!SudekiMpInstallPointerHook(&hook,(void **)(b+RVA_IAT),(const void *)(uintptr_t)expected,
        (const void *)(uintptr_t)titled_window)) { original=NULL; base=NULL; title[0]=0; return FALSE; }
    SudekiMpLogFormat("window_title event=installed title=\"%s\" seam=create_window_iat:0x%06x\r\n",title,RVA_IAT);
    return TRUE;
}
BOOL SudekiMpWindowTitleUninstall(void) {
    if(!hook.installed) return TRUE;
    if(!SudekiMpRestorePointerHook(&hook) || InterlockedCompareExchange(&callbacks,0,0)) {
        HMODULE self;
        (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&hook,&self);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    /* original/base stay valid for a caller that is still returning. */
    return TRUE;
}
