#include "engine/cast_motion_blur_abi.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include <math.h>
#include <string.h>

enum { BLUR_SET=0x1be60, SCENE_GLOBAL=0x408d58, SCENE_VTABLE=0x2c66b8 };
typedef void (__attribute__((thiscall)) *SetMotionBlur)(void *,float);
static uint8_t *blur_image;
static DWORD blur_thread,last_trace;
static volatile LONG call_depth;
static SudekiMpCastMotionBlurScope read_scope;
static SudekiMpInlineHook blur_hook;
static const uint8_t setter_entry[]={0x51,0xd9,0xee,0x53,0xd8,0x5c,0x24,0x0c};

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL exact_bytes(const uint8_t *b,uint32_t rva,const void *p,size_t n) {
    return readable(b+rva,n) && !memcmp(b+rva,p,n);
}
static BOOL exact_pointer(const uint8_t *b,uint32_t rva,uint32_t target) {
    return readable(b+rva,4) && *(const void *const *)(b+rva)==b+target;
}
static BOOL exact_call(const uint8_t *b,uint32_t rva,uint32_t target) {
    int32_t offset;
    if(!readable(b+rva,5) || b[rva]!=0xe8) return FALSE;
    memcpy(&offset,b+rva+1,4);
    return (int64_t)rva+5+offset==target;
}
BOOL SudekiMpCastMotionBlurImageMatches(HMODULE image) {
    const uint8_t *b=(const uint8_t *)image;
    static const uint8_t branch[]={0x56,0x57,0x8b,0xf9,0xdf,0xe0,0xf6,0xc4,5,0x7a,0x40};
    static const uint8_t allocation[]={0x83,0x7f,0x70,0,0x75,0x21,0x8b,0x5f,0x40,0x6a,0x18};
    static const uint8_t allocated[]={0x8b,0xf0,0x83,0xc4,4,0x85,0xf6,0x74,9,0x8b,0xc3};
    static const uint8_t assign[]={0xeb,2,0x33,0xc0,0x89,0x47,0x70};
    static const uint8_t tails[]={
        0x8b,0x4f,0x70,0xd9,0x44,0x24,0x14,0x8b,1,0x8b,0x50,4,0x51,0xd9,0x1c,0x24,
        0xff,0xd2,0x5f,0x5e,0x5b,0x59,0xc2,4,0,
        0x8b,0x4f,0x70,0x85,0xc9,0x74,8,0x8b,1,0x8b,0x10,0x6a,1,0xff,0xd2,
        0xc7,0x47,0x70,0,0,0,0,0x5f,0x5e,0x5b,0x59,0xc2,4,0};
    /* Exact thiscall setter, native allocation/destruction, and effect setter
     * ABI. Displaced instructions contain no relative branch or operand. */
    return b && exact_bytes(b,BLUR_SET,setter_entry,sizeof(setter_entry)) &&
        exact_bytes(b,BLUR_SET+8,branch,sizeof(branch)) &&
        exact_bytes(b,0x1be73,allocation,sizeof(allocation)) &&
        exact_call(b,0x1be7e,0x2484fa) &&
        exact_bytes(b,0x1be83,allocated,sizeof(allocated)) &&
        exact_call(b,0x1be8e,0x1ddfb0) &&
        exact_bytes(b,0x1be93,assign,sizeof(assign)) &&
        exact_bytes(b,0x1be9a,tails,sizeof(tails)) &&
        exact_pointer(b,0x1ddfc7,0x2dd91c) &&
        exact_pointer(b,0x2dd91c,0x1de050) && exact_pointer(b,0x2dd920,0x1ddfa0) &&
        exact_bytes(b,0x1ddfa0,"\xd9\x44\x24\x04\xd9\x59\x08\xc2\x04\x00",10);
}
static void __attribute__((thiscall)) route_motion_blur(void *scene,float amount) {
    DWORD saved_error=GetLastError();
    uint32_t generation=0;
    int scope=SUDEKIMP_CAST_BLUR_UNKNOWN;
    BOOL exact=blur_image && read_scope && GetCurrentThreadId()==blur_thread &&
        blur_hook.installed && readable(blur_hook.target,blur_hook.length) &&
        !memcmp(blur_hook.target,blur_hook.replacement,blur_hook.length) &&
        readable(blur_image+SCENE_GLOBAL,4) &&
        *(void **)(blur_image+SCENE_GLOBAL)==scene && readable(scene,0x74) &&
        *(void **)scene==blur_image+SCENE_VTABLE && isfinite(amount);
    InterlockedIncrement(&call_depth);
    if(exact) scope=read_scope(&generation);
    if((scope==SUDEKIMP_CAST_BLUR_NEUTRAL && !generation) ||
        (scope==SUDEKIMP_CAST_BLUR_LOCAL && generation)) {
        SetLastError(saved_error);
        ((SetMotionBlur)blur_hook.trampoline)(scene,amount);
        saved_error=GetLastError();
    } else {
        /* Suppress BOTH the remote enable and its later clear: a peer's
         * cleanup cannot end a simultaneous local caster's native effect.
         * Reject unknown ownership rather than borrowing an enclosing cast.
         * No native post-render job or effect object has been created here. */
        DWORD now=GetTickCount();
        if(!last_trace || now-last_trace>=1000u) {
            last_trace=now;
            SudekiMpLogFormat("cast_motion_blur event=request_suppressed scope=%d generation=%lu amount=%.3f tick=%lu policy=local_caster_only_native_render_callbacks_preserved\r\n",
                scope,(unsigned long)generation,amount,(unsigned long)now);
        }
    }
    InterlockedDecrement(&call_depth);
    SetLastError(saved_error);
}
BOOL SudekiMpInstallCastMotionBlur(HMODULE image,SudekiMpCastMotionBlurScope scope) {
    if(blur_image || !scope || !SudekiMpCastMotionBlurImageMatches(image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    blur_image=(uint8_t *)image; read_scope=scope; blur_thread=GetCurrentThreadId();
    if(!SudekiMpInstallInlineHook(&blur_hook,blur_image+BLUR_SET,
            setter_entry,sizeof(setter_entry),route_motion_blur)) {
        DWORD error=GetLastError();
        (void)SudekiMpUninstallCastMotionBlur();
        SetLastError(error); return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpUninstallCastMotionBlur(void) {
    if(!blur_image) return TRUE;
    if(InterlockedCompareExchange(&call_depth,0,0) || GetCurrentThreadId()!=blur_thread) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!SudekiMpRestoreInlineHook(&blur_hook)) return FALSE;
    blur_image=NULL; read_scope=NULL; blur_thread=last_trace=0;
    return TRUE;
}
