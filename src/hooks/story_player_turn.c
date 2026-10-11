#include "hooks/story_player_turn.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Player turning requires the supported x86 ABI"
#endif
static uint8_t *base;
static SudekiMpInlineHook hooks[2];
static SudekiMpStoryPlayerTurnLease exact_lease;
static void *move_actor;
static DWORD move_thread;
static DWORD native_thread;
static BOOL installed;
void *SudekiMpStoryPlayerTurnNext[2] __attribute__((used));
/* Whole CMP instruction; neither range contains a branch target. The next
 * native JG and mode check are retained. EAX is AI at both decisions. */
static const uint8_t compare[]={0x66,0x83,0xb8,0x6a,1,0,0,0};
static const unsigned sites[2]={STORY_TURN_BRANCH,STORY_CAN_TURN_BRANCH};
static const struct { unsigned rva,size; uint32_t hash; } contracts[]={
    {0xb7420u,0x25u,0x0cfb635fu}, /* STORY_TURN_CHOICE */
    {0xdbb20u,0x4au,0xddbaa165u}, /* STORY_CAN_TURN_CHOICE */
    {0xb73d0u,0x33u,0x08cc5e81u}, /* STORY_TURN_GATE */
    {0xdaeb9u,0x21u,0x64b25a9au}, /* STORY_MOVE_GATE */
    {0xb759au,0x12u,0x34f18707u}, /* STORY_PLAYER_ARGS */
    {0xb7b90u,0x11u,0xe497bb05u}  /* STORY_PLAYER_STEP */
};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL image_exact(uint8_t *b,BOOL owned) {
    for(unsigned k=0;k<sizeof(contracts)/sizeof(contracts[0]);++k) {
        unsigned rva=contracts[k].rva,n=contracts[k].size;
        uint32_t hash=2166136261u;
        if(!b || !readable(b+rva,n)) return FALSE;
        for(unsigned i=0;i<n;++i) {
            uint8_t byte=b[rva+i];
            for(unsigned h=0;owned && h<2u;++h)
                if(rva+i>=sites[h] && rva+i<sites[h]+sizeof(compare))
                    byte=compare[rva+i-sites[h]];
            hash=(hash^byte)*16777619u;
        }
        if(hash!=contracts[k].hash) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpStoryPlayerTurnImageExact(uint8_t *b) { return image_exact(b,FALSE); }
BOOL SudekiMpStoryPlayerTurnExact(void) {
    if(!base || !installed || !exact_lease) return FALSE;
    for(unsigned i=0;i<2u;++i)
        if(!hooks[i].installed || hooks[i].target!=base+sites[i] ||
            !readable(hooks[i].target,sizeof(compare)) ||
            memcmp(hooks[i].target,hooks[i].replacement,sizeof(compare))) return FALSE;
    return image_exact(base,TRUE);
}
/* f = pushad registers, EFLAGS, native return address. CanTurn's third
 * caller (turn mode 0x4B77F0) and AI movement calls are deliberately excluded.
 * A false/unknown lease replays the original CMP with original registers. */
static BOOL __attribute__((used,noinline,noclone)) player_decision(const uint32_t *f,unsigned id) {
    DWORD error=GetLastError(); BOOL allow=FALSE;
    uint8_t *component=(uint8_t *)(uintptr_t)f[id?1:0];
    if(native_thread && native_thread==GetCurrentThreadId() &&
        SudekiMpStoryPlayerTurnExact() && readable(component,id?0x64u:0x4cu) &&
        *(void **)component==base+(id?0x2cc9acu:0x2d48d4u)) {
        uint8_t *actor=*(uint8_t **)(component+0x10u);
        uint8_t *ai=(uint8_t *)(uintptr_t)f[7];
        if(readable(actor,0x98u) && *(void **)(actor+(id?0x90u:0x8cu))==component &&
            *(void **)(actor+0x94u)==ai && readable(ai,0x16cu) &&
            *(void **)ai==base+0x2d4924u && *(void **)(ai+0x10u)==actor &&
            *(int16_t *)(ai+0x16au)==1) {
            uint8_t *mode=*(uint8_t **)(ai+0x3cu);
            BOOL caller=!id || f[9]==(uintptr_t)(base+0xb73e8u) ||
                (f[9]==(uintptr_t)(base+0xdaec0u) && move_actor==actor &&
                 move_thread==GetCurrentThreadId());
            allow=caller && readable(mode,0xcu) && !mode[0xbu] && exact_lease(actor);
        }
    }
    SetLastError(error); return allow;
}
/* The native code resumes at JG. CMP EAX,EAX supplies the flags for a zero
 * count, but changes no register/memory. Preserve FP/SSE, stack and all other
 * flags; the original CMP also overwrites arithmetic flags at this seam. */
#define TURN_STUB(name,id,offset) \
static void __attribute__((naked,noinline)) name(void) { \
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;" \
        "mov %ebp,(%esp); movl $" #id ",4(%esp); call _player_decision;" \
        "mov %eax,12(%ebp); add $16,%esp; fxrstor (%esp); mov %ebp,%esp;" \
        "cmpl $0,12(%esp); jnz 1f; popal; popfl; cmpw $0,0x16a(%eax);" \
        "jmp *(_SudekiMpStoryPlayerTurnNext+" #offset ");" \
        "1: popal; popfl; cmp %eax,%eax; jmp *(_SudekiMpStoryPlayerTurnNext+" #offset ")"); }
TURN_STUB(turn_decision,0,0)
TURN_STUB(can_turn_decision,1,4)

BOOL SudekiMpStoryPlayerTurnBindThread(void) {
    if(!SudekiMpStoryPlayerTurnExact() ||
        (native_thread && native_thread!=GetCurrentThreadId())) return FALSE;
    native_thread=GetCurrentThreadId(); return TRUE;
}
BOOL SudekiMpStoryPlayerTurnMoveBegin(void *actor) {
    if(!native_thread || native_thread!=GetCurrentThreadId() || move_actor || !actor ||
        !SudekiMpStoryPlayerTurnExact() || !exact_lease(actor)) return FALSE;
    move_thread=GetCurrentThreadId(); move_actor=actor; return TRUE;
}
void SudekiMpStoryPlayerTurnMoveEnd(void) {
    if(move_thread==GetCurrentThreadId()) { move_actor=NULL; move_thread=0; }
}
BOOL SudekiMpStoryPlayerTurnInstall(HMODULE image,SudekiMpStoryPlayerTurnLease lease) {
    if(base || !lease || !image || !SudekiMpCheckLoadedExecutable(image) ||
        !SudekiMpStoryPlayerTurnImageExact((uint8_t *)image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=(uint8_t *)image; exact_lease=lease;
    for(unsigned i=0;i<2u;++i) SudekiMpStoryPlayerTurnNext[i]=base+sites[i]+sizeof(compare);
    if(!SudekiMpInstallInlineHook(&hooks[0],base+sites[0],compare,sizeof(compare),(void *)(uintptr_t)turn_decision) ||
        !SudekiMpInstallInlineHook(&hooks[1],base+sites[1],compare,sizeof(compare),(void *)(uintptr_t)can_turn_decision)) {
        DWORD error=GetLastError(); (void)SudekiMpStoryPlayerTurnUninstall(); SetLastError(error); return FALSE;
    }
    installed=TRUE; return TRUE;
}
BOOL SudekiMpStoryPlayerTurnUninstall(void) {
    if(move_actor || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    BOOL restored=TRUE;
    for(unsigned i=2u;i-->0u;) if(!SudekiMpRestoreInlineHook(&hooks[i])) restored=FALSE;
    if(!restored) return FALSE; /* Keep continuations/callbacks until all seams return. */
    installed=FALSE; base=NULL; exact_lease=NULL; native_thread=0;
    ZeroMemory(SudekiMpStoryPlayerTurnNext,sizeof(SudekiMpStoryPlayerTurnNext));
    return TRUE;
}
