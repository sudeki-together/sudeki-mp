/* Synthetic story wrapper only. No native input, animation or damage runs. */
#include "../src/hooks/lan_story_control.c"
#include <assert.h>
#include <stdio.h>
static BOOL roster_ok=TRUE,lease_ok=TRUE,combat=TRUE,combat_known=TRUE;
static BOOL prime,pending_weapon,weapon_known=TRUE,cast_idle=TRUE;
static BOOL spirit_known=TRUE,lower_submits=TRUE,lower_returns=TRUE;
static int spirit_state;
static unsigned calls,last_kind;
static uint8_t speed[0x30],actor[0xdc],arbiter[0x64],skill[0x78];
static uint8_t trigger[0x1da],interaction[0x64],interaction_mode[0x84];
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->dispatch_serial==7;
}
BOOL SudekiMpObserveCharacterSkill(void *a,SudekiMpCharacterSkillState *out) {
    assert(a==actor); *out=(SudekiMpCharacterSkillState){.skill=skill}; return TRUE;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) { return roster_ok && w && r && w->dispatch_serial==7; }
BOOL SudekiMpLanPartyControlStoryExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k) {
    return lease_ok && w && r && k && k->token==5 && k->generation==2 && k->seat==2;
}
BOOL SudekiMpCleanroomEngineCombatMode(BOOL *out) { *out=combat; return combat_known; }
BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) { return prime; }
BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *out) { *out=spirit_state; return spirit_known; }
BOOL SudekiMpWeaponActivationPending(void *a,BOOL *out) { assert(a==actor); *out=pending_weapon; return weapon_known; }
BOOL SudekiMpLanStoryCastDrained(void *a) { assert(a==actor); return cast_idle; }
BOOL SudekiMpLanPartyControlStoryMelee(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,unsigned kind,BOOL *submitted) {
    assert(SudekiMpLanPartyControlStoryExact(w,r,k));
    ++calls; last_kind=kind; *submitted=lower_submits; return lower_returns;
}
int main(void) {
    assert((STORY_BODY_BUSY_FLAGS&0x1000u)!=0 && !(STORY_BODY_BUSY_FLAGS&3u));
    base=VirtualAlloc(NULL,0x409000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(base);
    *(void **)(base+0x408da0)=speed; installed=TRUE; native_thread=GetCurrentThreadId();
    *(void **)(base+0x408d24)=trigger;
    *(void **)trigger=base+0x2c5458; *(void **)(trigger+8)=base+0x2c5460;
    *(void **)(trigger+0x10)=base+0x2c5470;
    *(void **)(actor+0xa8)=interaction; *(void **)interaction=base+0x2d4abc;
    *(void **)(interaction+0x10)=actor; *(void **)(interaction+0x60)=interaction_mode;
    *(void **)interaction_mode=base+0x2cbfcc;
    const unsigned offsets[2]={PREVIOUS_CALL,NEXT_CALL};
    for(unsigned i=0;i<2;++i) {
        switches[i].installed=TRUE; switches[i].instruction=base+offsets[i];
        switches[i].instruction[0]=0xe8;
        memcpy(switches[i].instruction+1,&switches[i].replacement_displacement,4);
    }
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=7,.service_post_original_exact=TRUE};
    SudekiMpLanStoryNativeRoster r={.available_mask=12,.leader_character=3}; r.actors[2]=actor;
    SudekiMpLanPartyLease k={.seat=2,.token=5,.generation=2};
    *(void **)actor=base+0x2d5010;
    *(void **)(actor+0x90)=arbiter; *(void **)(actor+0xd8)=skill;
    *(void **)arbiter=base+0x2cc9ac; *(void **)(arbiter+0x10)=actor;
    *(void **)(skill+0x10)=actor;
    *(uint32_t *)(arbiter+0x50)=3; assert(body_idle(&k,actor,&w));
    *(uint32_t *)(arbiter+0x50)=0x1002; assert(!body_idle(&k,actor,&w));
    /* An in-progress swing blocks movement/AI drain but not a new combo
     * press. The real native arbiter, not this wrapper, decides its timing. */
    for(unsigned kind=1;kind<=3;++kind) {
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_SUBMITTED);
        assert(last_kind==kind && calls==kind);
    }
    *(uint32_t *)(arbiter+0x50)=3; assert(body_idle(&k,actor,&w));
    skill[0x6c]=1; assert(!body_idle(&k,actor,&w)); skill[0x6c]=0;
    #define BUSY() assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_BUSY && calls==3)
    trigger[0x1d9]=2; BUSY(); assert(trigger[0x1d9]==2 && !trigger[0x1d8]); trigger[0x1d9]=0;
    trigger[0x1d8]=1; BUSY(); assert(trigger[0x1d8]==1); trigger[0x1d8]=0;
    interaction_mode[0x4c]=1;
    for(unsigned kind=1;kind<=3;++kind)
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_BUSY && calls==3);
    interaction_mode[0x4c]=0;
    *(void **)(interaction+0x10)=NULL; BUSY(); *(void **)(interaction+0x10)=actor;
    *(void **)(interaction+0x60)=NULL; BUSY(); *(void **)(interaction+0x60)=interaction_mode;
    *(void **)(base+0x408d24)=NULL; BUSY(); *(void **)(base+0x408d24)=trigger;
    speed[0x28]=1; BUSY(); speed[0x28]=0;
    *(int *)(speed+0x20)=2; BUSY(); *(int *)(speed+0x20)=0;
    *(int *)(speed+0x24)=2; BUSY(); *(int *)(speed+0x24)=0;
    speed[0x2a]=1; BUSY(); speed[0x2a]=0;
    prime=TRUE; BUSY(); prime=FALSE;
    pending_weapon=TRUE; BUSY(); pending_weapon=FALSE;
    weapon_known=FALSE; BUSY(); weapon_known=TRUE;
    spirit_state=1; BUSY(); spirit_state=0;
    spirit_known=FALSE; BUSY(); spirit_known=TRUE;
    cast_idle=FALSE; BUSY(); cast_idle=TRUE;
    combat_known=FALSE; BUSY(); combat_known=TRUE;
    #define REFUSE() assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && calls==3)
    roster_ok=FALSE; REFUSE(); roster_ok=TRUE;
    lease_ok=FALSE; REFUSE(); lease_ok=TRUE;
    k.seat=3; REFUSE(); k.seat=2;
    assert(SudekiMpLanStoryControlMelee(&w,&r,&k,0)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    assert(SudekiMpLanStoryControlMelee(&w,&r,&k,4)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    switches[0].instruction[0]=0x90; REFUSE(); switches[0].instruction[0]=0xe8;
    lower_returns=FALSE; lower_submits=FALSE;
    assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    lower_submits=TRUE;
    assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_RETAINED);
    lower_returns=TRUE; trigger[0x1d9]=2; trigger[0x1d8]=1;
    for(unsigned kind=2;kind<=3;++kind)
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_SUBMITTED);
    assert(trigger[0x1d9]==2 && trigger[0x1d8]==1); /* no global input ownership */
    VirtualFree(base,0,MEM_RELEASE);
    puts("story Tal melee synthetic admission/ownership/busy/uncertain-return tests passed");
    return 0;
}
