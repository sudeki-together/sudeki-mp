#include "hooks/story_ai_movement.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "AI movement gating requires the supported x86 ABI"
#endif
static uint8_t *base;
static SudekiMpInlineHook hooks[2];
static SudekiMpStoryAiMovementLease exact_lease;
static DWORD native_thread;
static BOOL installed;
void *SudekiMpStoryAiMovementNext[4] __attribute__((used));
/* Whole CMP instructions, no interior branch targets. Preserve the main
 * node's fade/state bookkeeping and the snap helper's intrusive cleanup. */
static const uint8_t move_compare[]={0x80,0x7c,0x24,0x0f,0};
static const uint8_t snap_compare[]={0x80,0xbd,0xa8,0,0,0,0};
static const unsigned sites[2]={STORY_AI_MOVE_GATE,STORY_AI_SNAP_GATE};
static const unsigned sizes[2]={sizeof(move_compare),sizeof(snap_compare)};
static const uint8_t *const originals[2]={move_compare,snap_compare};
static const struct { unsigned rva,size; uint32_t hash; } contracts[]={
    {0xf4bb0u,0x2eu,0x390488a5u}, /* AI_MOVE_FRAME */
    {0xf4cb3u,0x1au,0x47cf125bu}, /* AI_MOVE_CHOICE */
    {0xf4ccdu,0x3eu,0xccf4c58bu}, /* AI_MOVE_PLAYER_CACHE */
    {0xf50c0u,0x14u,0x509b92b7u}, /* AI_SNAP_FRAME */
    {0xf50ffu,0x39u,0xed651c65u}, /* AI_SNAP_CHOICE */
    {0xf51cau,0xa7u,0x0ab3db3fu}  /* AI_SNAP_CLEANUP */
};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL image_exact(uint8_t *b,BOOL owned) {
    for(unsigned k=0;k<sizeof(contracts)/sizeof(contracts[0]);++k) {
        unsigned rva=contracts[k].rva,n=contracts[k].size; uint32_t hash=2166136261u;
        if(!b || !readable(b+rva,n)) return FALSE;
        for(unsigned i=0;i<n;++i) {
            uint8_t byte=b[rva+i];
            for(unsigned h=0;owned && h<2u;++h)
                if(rva+i>=sites[h] && rva+i<sites[h]+sizes[h]) byte=originals[h][rva+i-sites[h]];
            hash=(hash^byte)*16777619u;
        }
        if(hash!=contracts[k].hash) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpStoryAiMovementImageExact(uint8_t *b) { return image_exact(b,FALSE); }
BOOL SudekiMpStoryAiMovementExact(void) {
    if(!base || !installed || !exact_lease) return FALSE;
    for(unsigned i=0;i<2u;++i)
        if(!hooks[i].installed || hooks[i].target!=base+sites[i] ||
            !readable(hooks[i].target,sizes[i]) ||
            memcmp(hooks[i].target,hooks[i].replacement,sizes[i])) return FALSE;
    return image_exact(base,TRUE);
}
/* pushad frame: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX, EFLAGS.
 * CAiDynamics = actor+98, owner +10; AI = actor+94. Keep actual game-raised
 * holds and modes native. Snap +A9 selects navigation-adjusted AI requests;
 * explicit PopToNamedLocation/TeleportGroup requests use zero and stay native. */
static BOOL __attribute__((used,noinline,noclone)) ai_decision(const uint32_t *f,unsigned id) {
    DWORD error=GetLastError(); BOOL allow=FALSE;
    if(native_thread && native_thread==GetCurrentThreadId() && SudekiMpStoryAiMovementExact()) {
        uint8_t *node=(uint8_t *)(uintptr_t)f[id?2:1];
        if(readable(node,0xaau) && *(void **)node==base+0x2d4994u) {
            uint8_t *actor=*(uint8_t **)(node+0x10u);
            if(readable(actor,0x9cu) && *(void **)(actor+0x98u)==node) {
                uint8_t *ai=*(uint8_t **)(actor+0x94u);
                if(readable(ai,0x16cu) && *(void **)ai==base+0x2d4924u &&
                    *(void **)(ai+0x10u)==actor && *(int16_t *)(ai+0x16au)==1) {
                    uint8_t *mode=*(uint8_t **)(ai+0x3cu);
                    BOOL context=id?node[0xa9u]==1u:
                        (f[4]==(uintptr_t)ai && f[0]==(uintptr_t)*(void **)(actor+0x44u));
                    allow=context && readable(mode,0xcu) && !mode[0xbu] && exact_lease(actor,id);
                }
            }
        }
    }
    SetLastError(error); return allow;
}
/* Neither skip target consumes incoming arithmetic flags. Restore all state
 * before the original CMP or the native player-cache / cleanup continuation. */
#define AI_STUB(name,id,normal,skip,compare) \
static void __attribute__((naked,noinline)) name(void) { \
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;" \
        "mov %ebp,(%esp); movl $" #id ",4(%esp); call _ai_decision; mov %eax,12(%ebp);" \
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; cmpl $0,12(%esp); jnz 1f;" \
        "popal; popfl; " compare "; jmp *(_SudekiMpStoryAiMovementNext+" #normal ");" \
        "1: popal; popfl; jmp *(_SudekiMpStoryAiMovementNext+" #skip ")"); }
AI_STUB(move_gate,0,0,8,"cmpb $0,0xf(%esp)")
AI_STUB(snap_gate,1,4,12,"cmpb $0,0xa8(%ebp)")

BOOL SudekiMpStoryAiMovementBindThread(void) {
    if(!SudekiMpStoryAiMovementExact() || (native_thread && native_thread!=GetCurrentThreadId())) return FALSE;
    native_thread=GetCurrentThreadId(); return TRUE;
}
BOOL SudekiMpStoryAiMovementInstall(HMODULE image,SudekiMpStoryAiMovementLease lease) {
    if(base || !lease || !image || !SudekiMpCheckLoadedExecutable(image) ||
        !SudekiMpStoryAiMovementImageExact((uint8_t *)image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=(uint8_t *)image; exact_lease=lease;
    SudekiMpStoryAiMovementNext[0]=base+sites[0]+sizes[0];
    SudekiMpStoryAiMovementNext[1]=base+sites[1]+sizes[1];
    SudekiMpStoryAiMovementNext[2]=base+0xf4ccdu;
    SudekiMpStoryAiMovementNext[3]=base+0xf51cau;
    if(!SudekiMpInstallInlineHook(&hooks[0],base+sites[0],move_compare,sizeof(move_compare),(void *)(uintptr_t)move_gate) ||
        !SudekiMpInstallInlineHook(&hooks[1],base+sites[1],snap_compare,sizeof(snap_compare),(void *)(uintptr_t)snap_gate)) {
        DWORD error=GetLastError(); (void)SudekiMpStoryAiMovementUninstall(); SetLastError(error); return FALSE;
    }
    installed=TRUE; return TRUE;
}
BOOL SudekiMpStoryAiMovementUninstall(void) {
    if(native_thread && native_thread!=GetCurrentThreadId()) { SetLastError(ERROR_BUSY); return FALSE; }
    installed=FALSE;
    BOOL restored=TRUE;
    for(unsigned i=2u;i-->0u;) if(!SudekiMpRestoreInlineHook(&hooks[i])) restored=FALSE;
    if(!restored) return FALSE;
    base=NULL; exact_lease=NULL; native_thread=0;
    ZeroMemory(SudekiMpStoryAiMovementNext,sizeof(SudekiMpStoryAiMovementNext)); return TRUE;
}
