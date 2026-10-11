#include "hooks/lan_story_death_hold.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story death hold requires the supported x86 native ABI"
#endif

/* FUN_00424220 (member died, EAX=group, arg=actor): with no living member it
 * runs the native game-over path; otherwise, when the dead actor is the lead,
 * it calls the party "previous" rotation 423F60 (ESI=group, no arguments,
 * plain RET) at 0x42428C. That rotation is what hands the host's controller
 * to a client's character. Skipping only this call keeps the dead lead in
 * slot 0; game over (all dead) and every other rotation stay native. */
/* Scripted lead change (live, castle-road ambush: lead=2 right after the
 * cutscene took Tal's AI): SetAsLeadPlayer stores a pending lead at
 * group+0xC0 and the group update (FUN_00423080) rotates with the party
 * "next" call 424060 at 0x4231E4 (ESI=group, no arguments, plain RET) until
 * slot 0 holds it. While the pending lead is a character a connected client
 * drives, skip that rotation: every player keeps their own character and the
 * host controller never takes a client's character. Other scripted leads
 * (the host's own characters) rotate natively. */
/* FUN_00424160 (a member went down, from 4C0900/4C0AB0; live: the ambush
 * lead change after Ailish fell never reached 424220) makes the same lead
 * rotation at 0x4241C9 and then moves the native move-disable flag from the
 * downed actor to the new lead; skipped, both reads resolve to the same
 * downed lead, which stays disabled. */
enum { DOWN_ROTATION_CALL=0x241c9u, DEATH_ROTATION_CALL=0x2428cu, PREVIOUS=0x23f60u, PENDING_ROTATION_CALL=0x231e4u, NEXT=0x24060u };
static SudekiMpRelativeCallHook hook,down_hook,pending_hook;
static volatile LONG active,held,pending_held;
static LONG reported,pending_reported;
void *death_hold_original __attribute__((used));
LONG death_hold_skip __attribute__((used));
void *pending_hold_original __attribute__((used));
LONG pending_hold_skip __attribute__((used));
static void *volatile remote_actors[3];

static void __attribute__((used,noinline)) death_hold_decide(void) {
    death_hold_skip=InterlockedCompareExchange(&active,0,0)!=0;
    if(death_hold_skip) InterlockedIncrement(&held);
}
static void __attribute__((naked,noinline)) death_rotation(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tcall _death_hold_decide\n\tpopal\n\tpopfl\n\t"
        "cmpl $0,_death_hold_skip\n\tjne 1f\n\tjmp *_death_hold_original\n\t1: ret\n\t");
}

static void __attribute__((used,noinline)) pending_hold_decide(const uint8_t *group) {
    void *pending=*(void *const *)(group+0xc0u);
    pending_hold_skip=0;
    if(!pending || !InterlockedCompareExchange(&active,0,0)) return;
    for(unsigned i=0;i<3u;++i) if(remote_actors[i]==pending) pending_hold_skip=1;
    if(pending_hold_skip) InterlockedIncrement(&pending_held);
}
static void __attribute__((naked,noinline)) pending_rotation(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl %esi\n\tcall _pending_hold_decide\n\taddl $4,%esp\n\tpopal\n\tpopfl\n\t"
        "cmpl $0,_pending_hold_skip\n\tjne 1f\n\tjmp *_pending_hold_original\n\t1: ret\n\t");
}
void SudekiMpLanStoryDeathHoldSetRemoteActors(void *const actors[3]) {
    for(unsigned i=0;i<3u;++i) remote_actors[i]=actors?actors[i]:NULL;
}

BOOL SudekiMpLanStoryDeathHoldInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    death_hold_original=b+PREVIOUS;
    pending_hold_original=b+NEXT;
    if(!SudekiMpInstallRelativeCallHook(&hook,b+DEATH_ROTATION_CALL,b+PREVIOUS,(void *)death_rotation))
        return FALSE;
    if(!SudekiMpInstallRelativeCallHook(&down_hook,b+DOWN_ROTATION_CALL,b+PREVIOUS,(void *)death_rotation)) {
        DWORD error=GetLastError(); (void)SudekiMpRestoreRelativeCallHook(&hook); SetLastError(error); return FALSE;
    }
    if(!SudekiMpInstallRelativeCallHook(&pending_hook,b+PENDING_ROTATION_CALL,b+NEXT,(void *)pending_rotation)) {
        DWORD error=GetLastError(); (void)SudekiMpRestoreRelativeCallHook(&down_hook);
        (void)SudekiMpRestoreRelativeCallHook(&hook); SetLastError(error); return FALSE;
    }
    SudekiMpLogWrite("story_death_hold event=installed policy=lead_keeps_slot_while_remote\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryDeathHoldUninstall(void) {
    InterlockedExchange(&active,0);
    BOOL ok=!pending_hook.installed || SudekiMpRestoreRelativeCallHook(&pending_hook);
    if(down_hook.installed && !SudekiMpRestoreRelativeCallHook(&down_hook)) ok=FALSE;
    return (!hook.installed || SudekiMpRestoreRelativeCallHook(&hook)) && ok;
}
void SudekiMpLanStoryDeathHoldSetActive(BOOL value) {
    LONG next=value?1:0;
    if(InterlockedExchange(&active,next)!=next)
        SudekiMpLogFormat("story_death_hold event=%s\r\n",next?"armed":"disarmed");
}
void SudekiMpLanStoryDeathHoldReport(void) {
    LONG n=InterlockedCompareExchange(&held,0,0);
    if(n!=reported) { reported=n;
        SudekiMpLogFormat("story_death_hold event=lead_kept holds=%ld\r\n",(long)n); }
    LONG p=InterlockedCompareExchange(&pending_held,0,0);
    if(p && (!pending_reported || p-pending_reported>=600)) { pending_reported=p;
        SudekiMpLogFormat("story_death_hold event=scripted_lead_held holds=%ld\r\n",(long)p); }
}
