#include "hooks/lan_story_client_camera_height.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Client camera height continuation requires the supported x86 ABI"
#endif
enum { HEIGHT_SITE=0x158026u, HEIGHT_APPLY=0x15805cu, HEIGHT_CALL_RETURN=0x1572bfu };
static uint8_t *base;
static SudekiMpInlineHook height_hook __attribute__((used));
static void *height_apply __attribute__((used));
static SudekiMpLanStoryClientCameraHeightProof proof;
static SudekiMpLanStoryClientCameraHeightScope scope;
static DWORD native_thread;
static BOOL active;
static uint32_t corrections;
static unsigned correction_logs;
static BOOL scope_corrected;
static float last_delta;
_Static_assert(offsetof(SudekiMpInlineHook,trampoline)==40u,"height bridge trampoline ABI");

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
/* Relocation-normalized exact-image contracts. These cover the native caller,
 * complete spring/error bound, discard decision and subsequent collision path.
 * No proprietary byte bodies are stored. Each check has its own failure name. */
static const struct HeightContract {
    const char *name; unsigned rva,size; uint32_t hash; unsigned count,reloc[3];
} contracts[]={
    {"HeightCaller",0x1572b5u,0xau,0xb7dd5d75u,0,{0}},
    {"HeightEntryAndErrorClamp",0x157e60u,0x1b5u,0x6028a92du,3,{0x3du,0x57u,0x70u}},
    {"HeightCutoff",0x158015u,0x22u,0xa024332fu,2,{0x6u,0x13u}},
    {"HeightApplyCollision",0x15805cu,0xa6u,0x76aae593u,0,{0}},
    {"HeightSpring",0x158110u,0xbeu,0x6c54bf1bu,0,{0}}
};
BOOL SudekiMpLanStoryClientCameraHeightImageExact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    for(unsigned k=0;k<sizeof(contracts)/sizeof(*contracts);++k) {
        const struct HeightContract *c=&contracts[k]; uint8_t bytes[0x1b5];
        if(c->size>sizeof(bytes) || !readable(b+c->rva,c->size)) return FALSE;
        memcpy(bytes,b+c->rva,c->size);
        for(unsigned j=0;j<c->count;++j) {
            uint32_t operand;memcpy(&operand,bytes+c->reloc[j],4);
            operand-=(uint32_t)(uintptr_t)b-0x400000u;memcpy(bytes+c->reloc[j],&operand,4);
        }
        uint32_t hash=2166136261u;
        for(unsigned j=0;j<c->size;++j) hash=(hash^bytes[j])*16777619u;
        if(hash!=c->hash) {
            SudekiMpLogFormat("story_client_camera_height event=image_refused guard=%s\r\n",c->name);
            return FALSE;
        }
    }
    static const uint64_t cutoff[2]={UINT64_C(0x3f8446737a400000),UINT64_C(0x3f84af4f05c00000)};
    if(!readable(b+0x2e3cd8u,sizeof(cutoff)) || memcmp(b+0x2e3cd8u,cutoff,sizeof(cutoff))) {
        SudekiMpLogWrite("story_client_camera_height event=image_refused guard=HeightConstants\r\n");
        return FALSE;
    }
    return TRUE;
}
static BOOL patch_exact(void) {
    return base && height_hook.installed && height_hook.target==base+HEIGHT_SITE &&
        height_hook.length==6u && height_hook.trampoline && readable(height_hook.target,6u) &&
        !memcmp(height_hook.target,height_hook.replacement,6u);
}
static BOOL identities_exact(const SudekiMpLanStoryClientCameraHeightScope *s) {
    uint8_t *camera=s->camera;
    return readable(camera,0xbcu) && readable(s->state,4u) && readable(s->data,4u) &&
        *(void **)(camera+0x3cu)==s->state && *(void **)(camera+0x40u)==s->data &&
        *(void **)(camera+0xb4u)==s->outer && *(void **)(camera+0xb8u)==s->inner &&
        *(void **)s->state==base+0x2d9854u && *(void **)s->data==base+0x2ca694u &&
        proof && proof(s);
}
BOOL SudekiMpLanStoryClientCameraHeightBegin(const SudekiMpLanStoryClientCameraHeightScope *s) {
    DWORD thread=GetCurrentThreadId();
    if(!s || active || (native_thread && native_thread!=thread) || !patch_exact() ||
        !identities_exact(s)) return FALSE;
    native_thread=thread;scope=*s;scope_corrected=FALSE;active=TRUE;return TRUE;
}
void SudekiMpLanStoryClientCameraHeightEnd(void) {
    if(active && native_thread==GetCurrentThreadId()) {
        active=FALSE;memset(&scope,0,sizeof(scope));
        /* Emit only after NodeUpdate has returned, outside its live x87 stack.
         * Admission is diagnostic evidence, not visual acceptance. */
        if(scope_corrected && correction_logs<4u) {
            ++correction_logs;
            SudekiMpLogFormat("story_client_camera_height event=small_step_admitted count=%lu delta=%.7f\r\n",
                (unsigned long)corrections,(double)last_delta);
        }
        scope_corrected=FALSE;
    }
}
/* pushad frame: EDI, ESI, EBP, saved ESP, EBX, EDX, ECX, EAX, EFLAGS.
 * The exact native function owns EDI=context and EBX=working geometry;
 * EBP+4/+8 are its caller and geometry argument. Original ESP+14/+18/+1C
 * hold abs(chosen correction), chosen correction and desired relative height.
 * The chosen correction has already passed the native spring/error clamp. */
static BOOL eligible(const uint32_t *f) {
    if(!active || !native_thread || native_thread!=GetCurrentThreadId() || !patch_exact() ||
        !identities_exact(&scope)) return FALSE;
    const uint8_t *ctx=(void *)(uintptr_t)f[0],*frame=(void *)(uintptr_t)f[2];
    const uint8_t *geometry=(void *)(uintptr_t)f[4];const float *stack=(const float *)(f+9);
    if(!readable(frame,12u) || *(void **)(frame+4u)!=base+HEIGHT_CALL_RETURN ||
        *(const void **)(frame+8u)!=geometry || !readable(ctx,0x20u) ||
        *(void **)ctx!=scope.camera || *(void **)(ctx+4u)!=scope.data ||
        *(void **)(ctx+0x18u)!=scope.outer || *(void **)(ctx+0x1cu)!=scope.inner ||
        !readable(geometry,0x48u) || !readable(stack,0x20u)) return FALSE;
    float magnitude=stack[5],delta=stack[6],desired=stack[7];
    float target=*(const float *)(geometry+0x34u),eye=*(const float *)(geometry+0x44u);
    /* Leave zero, unordered and every correction already above the native
     * lower threshold on the original path. Only its discarded tiny finite
     * steps take the unchanged native apply/collision continuation. */
    return isfinite(magnitude) && isfinite(delta) && isfinite(desired) &&
        isfinite(target) && isfinite(eye) && fabsf(desired)<=1000000.0f &&
        fabsf(target)<=1000000.0f && fabsf(eye)<=1000000.0f && delta!=0.0f &&
        magnitude==fabsf(delta) && (double)magnitude<0.009899999779008795;
}
static BOOL __attribute__((used,noinline,noclone)) height_decision(const uint32_t *f) {
    DWORD error=GetLastError();BOOL answer=eligible(f);
    if(answer) {
        if(corrections!=UINT32_MAX) ++corrections;
        scope_corrected=TRUE;last_delta=((const float *)(f+9))[6];
    }
    SetLastError(error);return answer;
}
/* At the seam x87 holds [abs(delta), delta, relative height, ...]. Preserve
 * the whole floating-point/register environment across the read-only proof.
 * The accepted jump removes abs(delta), then the RETAIL FADDP adds the step;
 * the ordinary trampoline executes the displaced FCOMP and resumes at15802C.
 * Do not patch158033: other native branches enter its neighboring158037. */
static void __attribute__((naked,noinline)) height_entry(void) {
    __asm__ volatile(
        "pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov %ebp,(%esp); call _height_decision; mov %eax,12(%ebp); add $16,%esp;"
        "fxrstor (%esp); mov %ebp,%esp; cmpl $0,12(%esp); jnz 1f;"
        "popal; popfl; jmp *(_height_hook+40);"
        "1: popal; popfl; jmp *_height_apply;");
}
BOOL SudekiMpLanStoryClientCameraHeightInstall(HMODULE image,SudekiMpLanStoryClientCameraHeightProof owner) {
    if(base || !owner || !SudekiMpLanStoryClientCameraHeightImageExact(image)) {
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    base=(uint8_t *)image;proof=owner;height_apply=base+HEIGHT_APPLY;
    corrections=correction_logs=0;scope_corrected=FALSE;last_delta=0;
    /* All guards/dependencies precede the only final patch. Its trampoline is
     * published in height_hook before the callsite changes. No fallible work
     * follows success; InstallInlineHook failure owns no installed patch. */
    uint8_t original[6];memcpy(original,base+HEIGHT_SITE,sizeof(original));
    if(!SudekiMpInstallInlineHook(&height_hook,base+HEIGHT_SITE,original,sizeof(original),
        (const void *)(uintptr_t)height_entry)) {
        base=NULL;proof=NULL;height_apply=NULL;return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryClientCameraHeightUninstall(void) {
    if(active || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY);return FALSE;
    }
    if(!SudekiMpRestoreInlineHook(&height_hook)) return FALSE;
    base=NULL;proof=NULL;height_apply=NULL;native_thread=0;memset(&scope,0,sizeof(scope));return TRUE;
}
