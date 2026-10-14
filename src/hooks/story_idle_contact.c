#include "hooks/story_idle_contact.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <math.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Idle contact correction requires the supported x86 ABI"
#endif
static const unsigned sites[2]={0x33288u,0x334a8u};
static uint8_t *base;
static SudekiMpInlineHook hooks[2];
static SudekiMpStoryIdleContactLease lease;
static DWORD native_thread;
static BOOL installed;
static uint32_t corrections;
static uint8_t original[2][6];
void *SudekiMpStoryIdleContactNext[2] __attribute__((used));
static const double *native_margin __attribute__((used));
/* The retail small-penetration tolerance, in normalized collision space.
 * Keep a positive clearance; do not skip the query, gravity or swept solve. */
static const double idle_margin __attribute__((used))=0.00001;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL image_exact(uint8_t *b,BOOL owned) {
    if(!b) return FALSE;
    for(unsigned k=0;k<sizeof(story_idle_contact_contracts)/sizeof(*story_idle_contact_contracts);++k) {
        const __typeof__(*story_idle_contact_contracts) *c=&story_idle_contact_contracts[k];
        uint8_t bytes[0xe1];
        if(c->size>sizeof(bytes) || !readable(b+c->rva,c->size)) return FALSE;
        memcpy(bytes,b+c->rva,c->size);
        for(unsigned s=0;owned && s<2;++s)
            if(c->rva<=sites[s] && sites[s]+6<=c->rva+c->size)
                memcpy(bytes+sites[s]-c->rva,original[s],6);
        for(unsigned r=0;r<c->relocations;++r) {
            uint32_t operand; memcpy(&operand,bytes+c->reloc[r],4);
            operand-=(uint32_t)(uintptr_t)b-0x400000u;
            memcpy(bytes+c->reloc[r],&operand,4);
        }
        uint32_t h=2166136261u;
        for(unsigned i=0;i<c->size;++i) h=(h^bytes[i])*16777619u;
        if(h!=c->hash) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpStoryIdleContactImageExact(uint8_t *b) { return image_exact(b,FALSE); }
static BOOL exact(void) {
    if(!installed || !base || !lease) return FALSE;
    for(unsigned s=0;s<2;++s)
        if(!hooks[s].installed || hooks[s].target!=base+sites[s] ||
            !readable(hooks[s].target,6) || memcmp(hooks[s].target,hooks[s].replacement,6)) return FALSE;
    return image_exact(base,TRUE);
}
/* pushad: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX, EFLAGS.
 * Native EBP+8/+C are the query manager and collider; EDI selects the current
 * borrowed query record; native ESP+14 holds its freshly computed normal.
 * The first seam handles depth < 1e-5. Rounding at large world coordinates
 * can cross that boundary: the adjacent front-side separation branch applies
 * the same clearance. Admit only its shallow <=1e-4 contacts, with a freshly
 * normalized, almost-upward separation as well as an almost-upward plane.
 * Larger penetration, backside/edge response and the subsequent gravity sweep
 * remain native. No previous-frame ground bit/source is used as authority. */
static BOOL up_normal(const float *n) {
    return isfinite(n[0]) && isfinite(n[1]) && isfinite(n[2]) &&
        n[1]>=0.998f && n[1]<=1.00001f &&
        fabsf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]-1.0f)<=0.0001f;
}
static BOOL eligible(const uint32_t *f,unsigned site) {
    uint8_t *frame=(void *)(uintptr_t)f[2],*manager=(void *)(uintptr_t)f[4];
    const float *stack=(const float *)(f+9);
    uint8_t *collider,*actor,*position,*movement,*object,*arbiter,*record;
    if(site>=2 || !native_thread || native_thread!=GetCurrentThreadId() || !exact() ||
        !readable(frame,16) || *(void **)(frame+4)!=base+0x3265fu ||
        *(void **)(frame+8)!=manager || !readable(manager,0xa0) ||
        !readable(collider=*(uint8_t **)(frame+12),0x84) ||
        *(void **)collider!=base+0x2c85fcu ||
        !readable(actor=*(uint8_t **)(collider+0x10),0xdcu) ||
        (*(void **)actor!=base+0x2d555cu && *(void **)actor!=base+0x2d66fcu &&
         *(void **)actor!=base+0x2d5010u && *(void **)actor!=base+0x2d5a88u) ||
        *(void **)(actor+0x60)!=collider || !lease(actor)) return FALSE;
    unsigned count=*(unsigned *)(manager+0x9c);
    uint8_t **records=*(uint8_t ***)(manager+0x90);
    if(!count || count>500 || f[0]>=count || !readable(records,count*sizeof(*records)) ||
        !readable(record=records[f[0]],0x58) || *(unsigned *)(record+0x20)!=1 ||
        !readable(stack,site?0x50:0x20) || !isfinite(stack[3]) || stack[3]<0 ||
        (site?(stack[3]<0.00001f || stack[3]>0.0001f):stack[3]>=0.00001f) ||
        !up_normal(stack+5) || (site && !up_normal(stack+17)))
        return FALSE;
    if(!readable(position=*(uint8_t **)(actor+0x44),0x104) ||
        *(void **)position!=base+0x2cdefcu || *(void **)(position+0x10)!=actor || position[0x103] ||
        (*(uintptr_t *)(position+0x94)!=0 && *(uintptr_t *)(position+0x94)!=4) ||
        !readable(object=*(uint8_t **)(position+0x8c),0x104) ||
        *(void **)(object+0x100)!=position+4 ||
        !readable(movement=*(uint8_t **)(actor+0x80),0xc0) ||
        *(void **)movement!=base+0x2c8644u || *(void **)(movement+0x10)!=actor ||
        *(float *)(movement+0x24)!=0 || *(float *)(movement+0x28)!=0 ||
        !readable(arbiter=*(uint8_t **)(actor+0x90),0x64) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10)!=actor ||
        (*(uint32_t *)(arbiter+0x50)&0x081812c8u) || (arbiter[0x60]&5u)) return FALSE;
    /* Ranged first-person movement publishes an integrated delta. Native
     * Tal/Buki movement instead publishes velocity, integrated by the swept
     * solve. Keep separate bounds and require the matching native mode.
     * On gentle terrain the old .01 clearance repeatedly pushed stationary
     * Tal sideways into a neighboring wall, which then pushed him back. */
    BOOL ranged=*(void **)actor==base+0x2d555cu || *(void **)actor==base+0x2d66fcu;
    const float *delta=(const float *)(object+0xe0);
    if(movement[0x79]!=(ranged?1:0) ||
        delta[0]!=0 || delta[2]!=0 || !isfinite(delta[1]) || delta[1]>=0 ||
        delta[1]<(ranged?-0.02f:-0.5f) ||
        *(float *)(movement+0x34)!=0 || *(float *)(movement+0x3c)!=0 ||
        *(float *)(movement+0x40)!=0 || *(float *)(movement+0x44)!=0 || *(float *)(movement+0x48)!=0)
        return FALSE;
    return TRUE;
}
static BOOL __attribute__((used,noinline,noclone)) contact_decision(const uint32_t *f,unsigned site) {
    DWORD error=GetLastError(); BOOL result=eligible(f,site);
    if(result && corrections!=UINT32_MAX) ++corrections;
    SetLastError(error); return result;
}
#define CONTACT_ENTRY(name,site,offset,penetration) \
static void __attribute__((naked,noinline)) name(void) { \
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;" \
        "mov %ebp,(%esp); movl $" #site ",4(%esp); call _contact_decision; mov %eax,12(%ebp); add $16,%esp;" \
        "fxrstor (%esp); mov %ebp,%esp; cmpl $0,12(%esp); jnz 1f;" \
        "popal; popfl; push %eax; mov _native_margin,%eax; faddl (%eax); pop %eax;" \
        "jmp *(_SudekiMpStoryIdleContactNext+" #offset ");" \
        "1: popal; popfl; " penetration "faddl _idle_margin; jmp *(_SudekiMpStoryIdleContactNext+" #offset ")"); }
CONTACT_ENTRY(normal_entry,0,0,"")
/* ST0 is half the positive penetration at this seam. Clear all of this
 * bounded shallow overlap before adding the small margin, so reducing the
 * margin cannot leave a stationary actor below the contact surface. */
CONTACT_ENTRY(separation_entry,1,4,"fadd %st(0),%st;")
#undef CONTACT_ENTRY
BOOL SudekiMpStoryIdleContactInstall(HMODULE image,SudekiMpStoryIdleContactLease owner) {
    if(base || !owner || !image || !SudekiMpCheckLoadedExecutable(image) ||
        !SudekiMpStoryIdleContactImageExact((uint8_t *)image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    base=(uint8_t *)image; lease=owner;
    native_margin=(const double *)(base+0x2e37c0u);
    for(unsigned s=0;s<2;++s) {
        memcpy(original[s],base+sites[s],6); SudekiMpStoryIdleContactNext[s]=base+sites[s]+6;
    }
    const void *entries[2]={(void *)(uintptr_t)normal_entry,(void *)(uintptr_t)separation_entry};
    for(unsigned s=0;s<2;++s) {
        if(!SudekiMpInstallInlineHook(&hooks[s],base+sites[s],original[s],6,entries[s])) {
            DWORD error=GetLastError(); (void)SudekiMpStoryIdleContactUninstall(); SetLastError(error); return FALSE;
        }
    }
    installed=TRUE; corrections=0; return TRUE;
}
BOOL SudekiMpStoryIdleContactBindThread(void) {
    if(!exact() || (native_thread && native_thread!=GetCurrentThreadId())) return FALSE;
    native_thread=GetCurrentThreadId(); return TRUE;
}
uint32_t SudekiMpStoryIdleContactCount(void) { return corrections; }
BOOL SudekiMpStoryIdleContactUninstall(void) {
    if(native_thread && native_thread!=GetCurrentThreadId()) { SetLastError(ERROR_BUSY); return FALSE; }
    installed=FALSE;
    BOOL restored=TRUE;
    for(unsigned s=2;s-->0;) if(!SudekiMpRestoreInlineHook(&hooks[s])) restored=FALSE;
    if(!restored) return FALSE;
    base=NULL; lease=NULL; native_thread=0; native_margin=NULL;
    memset(SudekiMpStoryIdleContactNext,0,sizeof(SudekiMpStoryIdleContactNext));
    return TRUE;
}
