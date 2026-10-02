#include "hooks/lobby_instance.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef HANDLE (WINAPI *CreateMutexFunction)(LPSECURITY_ATTRIBUTES,BOOL,LPCSTR);
static SudekiMpPointerHook mutex_hook;
static CreateMutexFunction original;
static uint8_t *base;
static volatile LONG callbacks;

__attribute__((noinline))
static HANDLE WINAPI child_mutex(LPSECURITY_ATTRIBUTES attributes,BOOL owner,LPCSTR name) {
    InterlockedIncrement(&callbacks);
    HANDLE result;
    if (__builtin_return_address(0)==base+0x102d && !attributes && !owner && name &&
        !strcmp(name,"Global\\Sudeki")) {
        char local[80];
        snprintf(local,sizeof(local),"Local\\SudekiMP-TestRoom-%lu",(unsigned long)GetCurrentProcessId());
        result=original(attributes,owner,local);
        DWORD error=GetLastError();
        SudekiMpLogFormat("lobby_launch event=instance_mutex scoped=child created=%u\r\n",result!=NULL);
        SetLastError(error);
    } else result=original(attributes,owner,name);
    InterlockedDecrement(&callbacks);
    return result;
}
BOOL SudekiMpInstallLobbyInstance(HMODULE game) {
    static const uint8_t entry[]={0x81,0xec,0x48,0x01,0x00,0x00,0x56,0x68};
    static const uint8_t call[]={0x51,0x6a,0x00,0x6a,0x00,0xff,0x15};
    uint8_t *candidate=(uint8_t *)game;
    if (!game || original || mutex_hook.installed || !SudekiMpCheckLoadedExecutable(game) ||
        memcmp(candidate+0x1000,entry,sizeof(entry)) || memcmp(candidate+0x1022,call,sizeof(call)) ||
        *(uint32_t *)(candidate+0x1029)!=(uint32_t)(uintptr_t)(candidate+0x29a058) ||
        memcmp(candidate+0x2bfff4,"Global\\%s",10) || memcmp(candidate+0x2c21a4,"Sudeki",7)) return FALSE;
    FARPROC expected=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateMutexA");
    if (!expected) return FALSE;
    base=candidate; original=(CreateMutexFunction)(uintptr_t)expected;
    return SudekiMpInstallPointerHook(&mutex_hook,(void **)(candidate+0x29a058),
        (const void *)(uintptr_t)expected,(const void *)(uintptr_t)child_mutex);
}
BOOL SudekiMpUninstallLobbyInstance(void) {
    if (!SudekiMpRestorePointerHook(&mutex_hook) || InterlockedCompareExchange(&callbacks,0,0)) {
        HMODULE self;
        (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)&mutex_hook,&self);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    /* Original and caller identity remain immutable for a retained caller. */
    return TRUE;
}
