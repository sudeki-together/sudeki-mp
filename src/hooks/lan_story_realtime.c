#include "hooks/lan_story_realtime.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story realtime policy requires the supported retail x86 ABI"
#endif
static uint8_t *base;
static BOOL installed;
static DWORD native_thread;
static SudekiMpInlineHook normal_hook,master_hook,variable_hook;
static SudekiMpBytePatch menu_patch,alternate_patches[4],camera_patches[4];
/* The compiler-pooled 0.07 constant has one non-speed reader (47B668).
 * Preserve that reader's original threshold. This immutable dependency must
 * outlive any partially restored operand; failed cleanup pins the module. */
static const float camera_threshold=0.07f;
static const uint8_t unity[4]={0,0,0x80,0x3f},alternate[4]={0x29,0x5c,0x8f,0x3d};
static const uint8_t variable_bytes[]={
    0xd9,0xee,0xd9,0x44,0x24,0x04,0xd8,0xd1,0xdf,0xe0,0xdd,0xd9,
    0xf6,0xc4,0x41,0x75,0x11,0xd9,0xe8,0xd8,0xd9,0xdf,0xe0,
    0xf6,0xc4,0x01,0x75,0x06,0xd9,0x59,0x2c,0xc2,0x04,0x00,
    0xdd,0xd8,0xc2,0x04,0x00};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL bytes(unsigned rva,const void *expected,size_t n) {
    return base && readable(base+rva,n) && !memcmp(base+rva,expected,n);
}
static BOOL retains(void) {
    if(normal_hook.installed || master_hook.installed || variable_hook.installed || menu_patch.installed)
        return TRUE;
    for(unsigned i=0;i<4u;++i) if(alternate_patches[i].installed || camera_patches[i].installed) return TRUE;
    return FALSE;
}
static BOOL pin(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&SudekiMpLanStoryRealtimeUninstall,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
/* Exact cdecl setters, no native callbacks or object lifetimes. Keep base
 * immutable after retirement for a fetched callback at the UI boundary. */
static void __cdecl normal_speed(float requested __attribute__((unused))) {
    *(uint32_t *)(base+0x345f70u)=0x3f800000u;
}
static void __cdecl master_speed(float requested __attribute__((unused))) {
    *(uint32_t *)(base+0x325810u)=0x3f800000u;
}
static void __attribute__((naked,noinline)) variable_speed(void) {
    /* Native validation still rejects <=0, >1 and NaN. Enter with the one
     * accepted request in ST0, ECX=this and the original stack argument. */
    __asm__ volatile("fstp %st(0)\n\tfld1\n\tfstps 0x2c(%ecx)\n\tret $4\n\t");
}
BOOL SudekiMpLanStoryRealtimeExact(HMODULE image) {
    if(!installed || base!=(uint8_t *)image) return FALSE;
    SudekiMpInlineHook *hooks[]={&normal_hook,&master_hook,&variable_hook};
    for(unsigned i=0;i<3u;++i)
        if(!hooks[i]->installed || !readable(hooks[i]->target,hooks[i]->length) ||
            memcmp(hooks[i]->target,hooks[i]->replacement,hooks[i]->length)) return FALSE;
    if(!menu_patch.installed || *menu_patch.target!=0u) return FALSE;
    uintptr_t camera=(uintptr_t)&camera_threshold;
    for(unsigned i=0;i<4u;++i)
        if(!alternate_patches[i].installed || *alternate_patches[i].target!=unity[i] ||
            !camera_patches[i].installed || *camera_patches[i].target!=((uint8_t *)&camera)[i]) return FALSE;
    return bytes(0x345f70u,unity,4u) && bytes(0x325810u,unity,4u);
}
BOOL SudekiMpLanStoryRealtimeUninstall(void) {
    if(!retains()) { installed=FALSE; return TRUE; }
    if(native_thread!=GetCurrentThreadId()) return pin(ERROR_INVALID_THREAD_ID);
    /* Reverse order; stop on failure so dependencies remain installed until
     * this exact owner can be retried. Never overwrite another adapter. */
    for(unsigned i=4u;i>0u;--i)
        if(!SudekiMpRestoreBytePatch(&alternate_patches[i-1u])) return pin(GetLastError());
    for(unsigned i=4u;i>0u;--i)
        if(!SudekiMpRestoreBytePatch(&camera_patches[i-1u])) return pin(GetLastError());
    if(!SudekiMpRestoreInlineHook(&variable_hook) || !SudekiMpRestoreInlineHook(&master_hook) ||
        !SudekiMpRestoreInlineHook(&normal_hook) || !SudekiMpRestoreBytePatch(&menu_patch)) return pin(GetLastError());
    installed=FALSE; SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryRealtimeInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!image || installed || retains() || (base && base!=b) || !SudekiMpCheckLoadedExecutable(image)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=b;
    uint8_t normal[]={0xd9,0x44,0x24,4,0xd9,0x1d,0,0,0,0,0xc3},master[sizeof(normal)];
    uint8_t menu[]={0xa1,0,0,0,0,0x8b,0x35,0,0,0,0,0xc7,0x40,0x24,1,0,0,0};
    uint8_t camera[]={0xd9,0x05,0,0,0,0,0x89,0x46,0x1c,0xd8,0x5e,0x0c};
    uintptr_t address=(uintptr_t)(base+0x345f70u); memcpy(normal+6,&address,4);
    memcpy(master,normal,sizeof(master)); address=(uintptr_t)(base+0x325810u); memcpy(master+6,&address,4);
    address=(uintptr_t)(base+0x408da0u); memcpy(menu+1,&address,4);
    address=(uintptr_t)(base+0x408d1cu); memcpy(menu+7,&address,4);
    address=(uintptr_t)(base+0x2c4018u); memcpy(camera+2,&address,4);
    /* Validate the native world init/reset source too. Both run after this
     * pre-load installation and initialize CGameSpeed+2c from the same source.
     * No write to an existing CGameSpeed object or its pause state is made. */
    uint8_t init[]={0xd9,0x05,0,0,0,0,0xd9,0x9f,0x8c,0x0c,0,0};
    uint8_t reset[]={0xd9,0x05,0,0,0,0,0x33,0xd2,0xd9,0x9d,0x8c,0x0c,0,0};
    memcpy(init+2,&address,4); memcpy(reset+2,&address,4);
    if(!bytes(0x27040u,normal,sizeof(normal)) || !bytes(0x28be90u,master,sizeof(master)) ||
        !bytes(0x27510u,variable_bytes,sizeof(variable_bytes)) || !bytes(0x98ee5u,menu,sizeof(menu)) ||
        !bytes(0x7b668u,camera,sizeof(camera)) || !bytes(0x777acu,init,sizeof(init)) ||
        !bytes(0x77973u,reset,sizeof(reset)) || !bytes(0x2c4018u,alternate,4u) ||
        !bytes(0x345f70u,unity,4u) || !bytes(0x325810u,unity,4u) ||
        memcmp(&camera_threshold,alternate,4u)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    native_thread=GetCurrentThreadId();
    if(!SudekiMpInstallBytePatch(&menu_patch,base+0x98ef3u,1u,0u) ||
        !SudekiMpInstallInlineHook(&normal_hook,base+0x27040u,normal,10u,(const void *)(uintptr_t)normal_speed) ||
        !SudekiMpInstallInlineHook(&master_hook,base+0x28be90u,master,10u,(const void *)(uintptr_t)master_speed) ||
        !SudekiMpInstallInlineHook(&variable_hook,base+0x2752cu,variable_bytes+28u,6u,(const void *)(uintptr_t)variable_speed)) goto failed;
    uintptr_t camera_source=(uintptr_t)&camera_threshold;
    for(unsigned i=0;i<4u;++i)
        if(!SudekiMpInstallBytePatch(&camera_patches[i],base+0x7b66au+i,
            ((uint8_t *)&address)[i],((uint8_t *)&camera_source)[i])) goto failed;
    for(unsigned i=0;i<4u;++i)
        if(!SudekiMpInstallBytePatch(&alternate_patches[i],base+0x2c4018u+i,alternate[i],unity[i])) goto failed;
    installed=TRUE;
    if(!SudekiMpLanStoryRealtimeExact(image)) { SetLastError(ERROR_INVALID_STATE); goto failed; }
    SudekiMpLogWrite("story_realtime event=installed normal=1 master=1 alternate=1 variable=1 menu_request=0 full_pause=unchanged camera_threshold=unchanged\r\n");
    SetLastError(ERROR_SUCCESS); return TRUE;
failed:
    {
        DWORD error=GetLastError();
        if(!SudekiMpLanStoryRealtimeUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
}
