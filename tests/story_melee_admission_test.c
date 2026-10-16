/* Synthetic story wrapper only. No native input, animation or damage runs. */
#include "../src/hooks/lan_story_control.c"
#include <assert.h>
#include <stdio.h>
static BOOL roster_ok=TRUE,lease_ok=TRUE,combat=TRUE,combat_known=TRUE;
static BOOL prime,pending_weapon,weapon_known=TRUE,cast_idle=TRUE;
static BOOL spirit_known=TRUE,lower_submits=TRUE,lower_returns=TRUE;
static int spirit_state;
static unsigned calls,last_kind;
static unsigned movement_calls,target_calls;
static float movement_x,movement_z;
static BOOL idle_loading,idle_remote=TRUE,idle_binding=TRUE;
static BOOL turn_ready=TRUE,ai_acquired=TRUE;
static unsigned ai_gate_calls,ai_last_gate;
BOOL SudekiMpLanStoryObserverWorldLoading(void) { return idle_loading; }
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return roster_ok && r && r->dispatch_serial==7;
}
BOOL SudekiMpLanPartyControlStoryTurnOwner(void *a,SudekiMpLanPartyLease *key) {
    (void)a; *key=(SudekiMpLanPartyLease){.seat=3,.token=5,.generation=2}; return idle_remote;
}
BOOL SudekiMpLanStoryRuntimeTurnLeaseExact(const SudekiMpLanPartyLease *key) {
    return turn_ready && idle_remote && lease_ok && key->token==5;
}
BOOL SudekiMpLanStoryRuntimeAiLeaseExact(const SudekiMpLanPartyLease *key) {
    return ai_acquired && idle_remote && lease_ok && key->seat==3 && key->token==5 && key->generation==2;
}
void SudekiMpLanStoryResearch54AiGate(const void *a,unsigned gate) {
    assert(a); ++ai_gate_calls; ai_last_gate=gate;
}
BOOL SudekiMpStoryIdleContactBindThread(void) { return idle_binding; }
uint32_t SudekiMpStoryIdleContactCount(void) { return 0; }
void SudekiMpLogWrite(const char *message) { (void)message; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLanPartyControlStoryRefreshTarget(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,float x,float z) {
    (void)x; (void)z; ++target_calls; return SudekiMpLanPartyControlStoryExact(w,r,k);
}
BOOL SudekiMpLanPartyControlStoryMove(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,float x,float z) {
    ++movement_calls; movement_x=x; movement_z=z;
    return SudekiMpLanPartyControlStoryExact(w,r,k);
}
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
    return lease_ok && w && r && k && k->token==5 && k->generation==2 && k->seat<4u;
}
BOOL SudekiMpCleanroomEngineCombatMode(BOOL *out) { *out=combat; return combat_known; }
BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) { return prime; }
BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *out) { *out=spirit_state; return spirit_known; }
BOOL SudekiMpWeaponActivationPending(void *a,BOOL *out) { assert(a==actor); *out=pending_weapon; return weapon_known; }
BOOL SudekiMpLanStoryCastBodyDrained(void *a) { assert(a==actor); return cast_idle; }
static BOOL lighting_tail,aiming;
BOOL SudekiMpLanStoryCastAiming(void *a) { assert(a==actor); return aiming; }
BOOL SudekiMpLanStoryCastDrained(void *a) { assert(a==actor); return cast_idle && !lighting_tail; }
BOOL SudekiMpLanPartyControlStoryMelee(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,unsigned kind,BOOL *submitted) {
    assert(SudekiMpLanPartyControlStoryExact(w,r,k));
    ++calls; last_kind=kind; *submitted=lower_submits; return lower_returns;
}
BOOL SudekiMpLanPartyControlStoryMeleeSnapshot(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,unsigned buttons,BOOL *submitted) {
    assert(SudekiMpLanPartyControlStoryExact(w,r,k));
    ++calls;last_kind=buttons;*submitted=lower_submits;return lower_returns;
}
BOOL SudekiMpLanPartyControlStoryRanged(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,unsigned state,BOOL *submitted) {
    assert(SudekiMpLanPartyControlStoryExact(w,r,k));
    ++calls; last_kind=state; *submitted=lower_submits; return lower_returns;
}
static unsigned block_calls;static BOOL last_block;
static BOOL dodge_held,dodge_ok=TRUE;static unsigned dodge_calls;
static float dodge_x,dodge_z;
BOOL SudekiMpLanPartyControlStoryDodge(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,float x,float z,BOOL *handled) {
    if(!SudekiMpLanPartyControlStoryExact(w,r,k)) return FALSE;
    ++dodge_calls;dodge_x=x;dodge_z=z;*handled=dodge_held;return dodge_ok;
}
BOOL SudekiMpLanPartyControlStoryBlock(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k,BOOL held) {
    assert(SudekiMpLanPartyControlStoryExact(w,r,k));++block_calls;last_block=held;return TRUE;
}
static void ai_prepare_admission(void) {
    /* PREPARE owns the acquired AI lease but does not grant player turning. */
    turn_ready=FALSE;
    assert(!player_turn_lease(actor));
    assert(ai_movement_lease(actor,2u) && ai_gate_calls==1 && ai_last_gate==2u);
    unsigned accepted=ai_gate_calls;
    #define AI_REFUSE() assert(!ai_movement_lease(actor,3u) && ai_gate_calls==accepted && !player_turn_lease(actor))
    ai_acquired=FALSE; AI_REFUSE(); ai_acquired=TRUE;
    lease_ok=FALSE; AI_REFUSE(); lease_ok=TRUE;
    idle_remote=FALSE; AI_REFUSE(); idle_remote=TRUE;
    installed=FALSE; AI_REFUSE(); installed=TRUE;
    DWORD thread=native_thread;
    native_thread=0; AI_REFUSE(); native_thread=thread^1u; AI_REFUSE(); native_thread=thread;
    for(unsigned i=0;i<2;++i) {
        switches[i].instruction[0]=0x90; AI_REFUSE(); switches[i].instruction[0]=0xe8;
    }
    speed[0x28]=1; AI_REFUSE(); speed[0x28]=0;
    *(uint16_t *)(speed+0x2a)=1; AI_REFUSE(); *(uint16_t *)(speed+0x2a)=0;
    *(int *)(speed+0x20)=1; AI_REFUSE(); *(int *)(speed+0x20)=0;
    *(int *)(speed+0x24)=1; AI_REFUSE(); *(int *)(speed+0x24)=0;
    combat_known=FALSE; AI_REFUSE(); combat_known=TRUE;
    *(void **)(base+0x408da0)=NULL; AI_REFUSE(); *(void **)(base+0x408da0)=speed;
    uint8_t *image=base; base=NULL; AI_REFUSE(); base=image;
    #undef AI_REFUSE
    assert(ai_movement_lease(actor,4u) && ai_gate_calls==accepted+1 && ai_last_gate==4u);
    turn_ready=TRUE;
    assert(player_turn_lease(actor) && ai_movement_lease(actor,5u));
    puts("story PREPARE AI suppression: acquired ownership without READY turning; lease, hooks, world and thread guards passed");
}
static void native_pause_movement(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryNativeRoster *r,SudekiMpLanPartyLease *k) {
    unsigned moves=movement_calls,targets=target_calls,dodges=dodge_calls;
    uint32_t flags=*(uint32_t *)(arbiter+0x50);
    *(uint32_t *)(arbiter+0x50)=2u;
    for(unsigned refs=1;refs<=2;++refs) {
        actor[0x2b]=(uint8_t)refs;
        uint8_t before[sizeof(actor)];memcpy(before,actor,sizeof(before));
        BOOL held=FALSE;
        assert(SudekiMpLanStoryControlMove(w,r,k,0.3f,1.8f,&held) && held);
        held=FALSE;
        assert(SudekiMpLanStoryControlMove(w,r,k,0,0,&held) && held);
        assert(movement_calls==moves && target_calls==targets && dodge_calls==dodges);
        assert(!memcmp(before,actor,sizeof(before)));
    }
    /* A paused body cannot hide a lost lease, stale generation or roster. */
    BOOL held=TRUE;
    lease_ok=FALSE;
    assert(!SudekiMpLanStoryControlMove(w,r,k,0,0,&held) && !held);
    lease_ok=TRUE;++k->generation;
    assert(!SudekiMpLanStoryControlMove(w,r,k,0.3f,1.8f,&held) && !held);
    --k->generation;roster_ok=FALSE;
    assert(!SudekiMpLanStoryControlMove(w,r,k,0,0,&held) && !held);
    roster_ok=TRUE;
    assert(movement_calls==moves && target_calls==targets && dodge_calls==dodges);
    assert(actor[0x2b]==2u);
    actor[0x2b]=0;
    /* Binary-exact values keep this a forwarding check, independent of the
     * x86 compiler's extended precision for decimal float constants. */
    assert(SudekiMpLanStoryControlMove(w,r,k,0.5f,1.5f,&held) && !held);
    assert(movement_calls==moves+1 && target_calls==targets+1 && dodge_calls==dodges+1);
    assert(movement_x==0.5f && movement_z==1.5f);
    *(uint32_t *)(arbiter+0x50)=flags;
    puts("story native actor pause: no target/dodge/movement calls while paused; stale ownership refuses and zero references resume");
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
    ai_prepare_admission();
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=7,.service_post_original_exact=TRUE};
    SudekiMpLanStoryNativeRoster r={.available_mask=12,.leader_character=3}; r.actors[2]=actor;
    SudekiMpLanPartyLease k={.seat=2,.token=5,.generation=2};
    *(void **)actor=base+0x2d5010;
    *(void **)(actor+0x90)=arbiter; *(void **)(actor+0xd8)=skill;
    *(void **)arbiter=base+0x2cc9ac; *(void **)(arbiter+0x10)=actor;
    *(void **)(skill+0x10)=actor;
    *(uint32_t *)(arbiter+0x50)=3; assert(body_idle(&k,actor,&w));
    lighting_tail=TRUE;
    assert(!body_idle(&k,actor,&w) && body_idle_common(&k,actor,&w,FALSE,FALSE));
    lighting_tail=FALSE;
    /* Native skill aim flags (0x813) allow walking, but keep the skill live
     * for transfer/drain and block melee/recast. Execution's 0x80000 remains
     * busy even if a stale phase observation says aiming. */
    aiming=TRUE; cast_idle=FALSE; skill[0x6c]=1;
    *(uint32_t *)(arbiter+0x50)=0x813;
    assert(body_idle_common(&k,actor,&w,FALSE,TRUE) && !body_idle(&k,actor,&w));
    *(uint32_t *)(arbiter+0x50)=0x80812;
    assert(!body_idle_common(&k,actor,&w,FALSE,TRUE));
    *(uint32_t *)(arbiter+0x50)=0x813; aiming=FALSE;
    assert(!body_idle_common(&k,actor,&w,FALSE,TRUE));
    cast_idle=TRUE; skill[0x6c]=0;
    *(uint32_t *)(arbiter+0x50)=0x1002; assert(!body_idle(&k,actor,&w));
    /* Preserve each frame's movement during a combo; native CanMove decides
     * translation. Retiring the AI lease must still wait for the swing. */
    BOOL held=TRUE;
    assert(body_idle_common(&k,actor,&w,FALSE,TRUE));
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0.3f,1.8f,&held) && !held);
    assert(movement_calls==1 && fabsf(movement_x-0.3f)<0.00001f && fabsf(movement_z-1.8f)<0.00001f);
    for(unsigned frame=0;frame<8;++frame) {
        assert(SudekiMpLanStoryControlMove(&w,&r,&k,-0.4f,1.5f,&held) && !held);
        assert(fabsf(movement_x+0.4f)<0.00001f && movement_z==1.5f);
    }
    assert(!body_idle(&k,actor,&w));
    *(uint32_t *)(arbiter+0x50)=0x80812; aiming=TRUE; cast_idle=FALSE; skill[0x6c]=1;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && held && !movement_x && !movement_z);
    *(uint32_t *)(arbiter+0x50)=0x813;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && !held && fabsf(movement_z-1.8f)<0.00001f);
    aiming=FALSE; cast_idle=TRUE; skill[0x6c]=0; *(uint32_t *)(arbiter+0x50)=0x1002;
    speed[0x28]=1;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && held && !movement_x && !movement_z);
    speed[0x28]=0; interaction_mode[0x4c]=1;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && held && !movement_x && !movement_z);
    interaction_mode[0x4c]=0;
    native_pause_movement(&w,&r,&k);
    /* Held block must bypass the generic body-busy gate and ordinary Move,
     * for Tal and Buki, including native cooldown and neutral input. */
    unsigned before_move=movement_calls;
    dodge_held=TRUE;*(uint32_t *)(arbiter+0x50)=0x10au;
    for(unsigned c=0;c<=2;c+=2) {
        k.seat=c;r.actors[c]=actor;
        assert(SudekiMpLanStoryControlMove(&w,&r,&k,-0.4f,1.5f,&held) && !held);
        assert(fabsf(dodge_x+0.4f)<0.00001f && dodge_z==1.5f && movement_calls==before_move);
        assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,0,&held) && !held && movement_calls==before_move);
    }
    unsigned before_dodge=dodge_calls;
    speed[0x28]=1;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && held && !movement_z);
    assert(dodge_calls==before_dodge);speed[0x28]=0;
    interaction_mode[0x4c]=1;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && held && !movement_z);
    assert(dodge_calls==before_dodge);interaction_mode[0x4c]=0;
    cast_idle=FALSE;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && held && !movement_z);
    assert(dodge_calls==before_dodge);cast_idle=TRUE;
    dodge_ok=FALSE;before_move=movement_calls;
    assert(!SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && movement_calls==before_move);
    dodge_ok=TRUE;lease_ok=FALSE;
    assert(!SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held));lease_ok=TRUE;
    dodge_held=FALSE;*(uint32_t *)(arbiter+0x50)=2;
    assert(SudekiMpLanStoryControlMove(&w,&r,&k,0,1.8f,&held) && !held && fabsf(movement_z-1.8f)<0.00001f);
    for(unsigned kind=1;kind<=3;++kind) {
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_SUBMITTED);
        assert(last_kind==kind && calls==kind);
    }
    *(uint32_t *)(arbiter+0x50)=3; assert(body_idle(&k,actor,&w));
    skill[0x6c]=1; assert(!body_idle(&k,actor,&w)); skill[0x6c]=0;
    /* An available interaction is safe: the native press wrapper owns and
     * restores its temporary latch. A preexisting latch is still busy. */
    trigger[0x1d9]=2;
    assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_SUBMITTED);
    assert(calls==4 && trigger[0x1d9]==2 && !trigger[0x1d8]); trigger[0x1d9]=0;
    assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && last_block);
    speed[0x28]=1;assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && !last_block);
    assert(SudekiMpLanStoryControlBlock(&w,&r,&k,FALSE) && !last_block);speed[0x28]=0;
    interaction_mode[0x4c]=1;
    assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && !last_block);
    assert(SudekiMpLanStoryControlBlock(&w,&r,&k,FALSE) && !last_block);interaction_mode[0x4c]=0;
    cast_idle=FALSE;assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && !last_block);cast_idle=TRUE;
    combat=FALSE;assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && !last_block);combat=TRUE;
    trigger[0x1d8]=1;assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && last_block);
    assert(trigger[0x1d8]==1);trigger[0x1d8]=0; /* block does not borrow the host latch */
    k.seat=0;r.actors[0]=actor;assert(SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE) && last_block);
    k.seat=1;assert(!SudekiMpLanStoryControlBlock(&w,&r,&k,TRUE));k.seat=2;
    lease_ok=FALSE;unsigned before_block=block_calls;
    assert(!SudekiMpLanStoryControlBlock(&w,&r,&k,FALSE) && block_calls==before_block);lease_ok=TRUE;
    const unsigned accepted_calls=calls;
    #define BUSY() assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_BUSY && calls==accepted_calls)
    trigger[0x1d8]=1; BUSY(); assert(trigger[0x1d8]==1); trigger[0x1d8]=0;
    interaction_mode[0x4c]=1;
    for(unsigned kind=1;kind<=3;++kind)
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_BUSY && calls==accepted_calls);
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
    #define REFUSE() assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && calls==accepted_calls)
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
    /* Buki uses the same native melee entry and admission guards. */
    r.available_mask=13;r.actors[0]=actor;k.seat=0;
    *(void **)actor=base+0x2d5a88;trigger[0x1d8]=trigger[0x1d9]=0;
    assert(body_idle(&k,actor,&w));
    for(unsigned kind=1;kind<=3;++kind) {
        unsigned before=calls;
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_SUBMITTED);
        assert(calls==before+1 && last_kind==kind);
        interaction_mode[0x4c]=1;
        assert(SudekiMpLanStoryControlMelee(&w,&r,&k,kind)==SUDEKIMP_STORY_ACTION_BUSY);
        assert(calls==before+1);interaction_mode[0x4c]=0;
    }
    for(unsigned seat=0;seat<=2;seat+=2) {
        k.seat=seat;r.actors[seat]=actor;
        trigger[0x1d8]=1;trigger[0x1d9]=2;
        for(unsigned buttons=0;buttons<64;++buttons) {
            unsigned before=calls;
            assert(SudekiMpLanStoryControlMeleeSnapshot(&w,&r,&k,buttons)==SUDEKIMP_STORY_ACTION_SUBMITTED);
            assert(calls==before+1 && last_kind==buttons && trigger[0x1d8]==1 && trigger[0x1d9]==2);
        }
        unsigned before=calls;
        #define SNAP_BUSY() assert(SudekiMpLanStoryControlMeleeSnapshot(&w,&r,&k,5)==SUDEKIMP_STORY_ACTION_BUSY && calls==before)
        interaction_mode[0x4c]=1;SNAP_BUSY();interaction_mode[0x4c]=0;
        speed[0x28]=1;SNAP_BUSY();speed[0x28]=0;
        pending_weapon=TRUE;SNAP_BUSY();pending_weapon=FALSE;
        weapon_known=FALSE;SNAP_BUSY();weapon_known=TRUE;
        prime=TRUE;SNAP_BUSY();prime=FALSE;
        spirit_state=1;SNAP_BUSY();spirit_state=0;
        spirit_known=FALSE;SNAP_BUSY();spirit_known=TRUE;
        cast_idle=FALSE;SNAP_BUSY();cast_idle=TRUE;
        combat_known=FALSE;SNAP_BUSY();combat_known=TRUE;
        roster_ok=FALSE;
        assert(SudekiMpLanStoryControlMeleeSnapshot(&w,&r,&k,5)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && calls==before);
        roster_ok=TRUE;lease_ok=FALSE;
        assert(SudekiMpLanStoryControlMeleeSnapshot(&w,&r,&k,5)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && calls==before);
        lease_ok=TRUE;
        assert(SudekiMpLanStoryControlMeleeSnapshot(&w,&r,&k,64)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && calls==before);
    }
    trigger[0x1d8]=trigger[0x1d9]=0;
    k.seat=1;
    assert(SudekiMpLanStoryControlMelee(&w,&r,&k,1)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    for(unsigned character=1;character<=3;character+=2) {
        r.actors[character]=actor;r.available_mask=15;k.seat=character;
        for(unsigned state=1;state<=2;++state) {
            unsigned before=calls;
            assert(SudekiMpLanStoryControlRanged(&w,&r,&k,state)==SUDEKIMP_STORY_ACTION_SUBMITTED);
            assert(calls==before+1 && last_kind==state);
        }
        interaction_mode[0x4c]=1;
        assert(SudekiMpLanStoryControlRanged(&w,&r,&k,2)==SUDEKIMP_STORY_ACTION_BUSY);
        interaction_mode[0x4c]=0;
        assert(SudekiMpLanStoryControlRanged(&w,&r,&k,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    }
    /* Host idle contact must not borrow a stale roster, remote input lease,
     * menu/scene state or an uncertain native cast/weapon observation. */
    memset(r.actors,0,sizeof(r.actors));r.actors[3]=actor;r.available_mask=8;
    r.leader_character=3;r.dispatch_serial=7;combat_known=TRUE;cast_idle=TRUE;
    SudekiMpLanStoryControlObserveIdleContact(&w,&r);assert(idle_contact_lease(actor));
    idle_loading=TRUE;assert(!idle_contact_lease(actor));idle_loading=FALSE;
    roster_ok=FALSE;assert(!idle_contact_lease(actor));roster_ok=TRUE;
    speed[0x28]=1;assert(!idle_contact_lease(actor));speed[0x28]=0;
    pending_weapon=TRUE;assert(!idle_contact_lease(actor));pending_weapon=FALSE;
    weapon_known=FALSE;assert(!idle_contact_lease(actor));weapon_known=TRUE;
    spirit_known=FALSE;assert(!idle_contact_lease(actor));spirit_known=TRUE;
    spirit_state=1;assert(!idle_contact_lease(actor));spirit_state=0;
    cast_idle=FALSE;assert(!idle_contact_lease(actor));cast_idle=TRUE;
    prime=TRUE;assert(!idle_contact_lease(actor));prime=FALSE;
    r.leader_character=2;r.available_mask=12;
    SudekiMpLanStoryControlObserveIdleContact(&w,&r);assert(idle_contact_lease(actor));
    idle_remote=FALSE;assert(!idle_contact_lease(actor));idle_remote=TRUE;
    lease_ok=FALSE;assert(!idle_contact_lease(actor));lease_ok=TRUE;
    for(unsigned c=0;c<4;++c) {
        memset(r.actors,0,sizeof(r.actors));r.actors[c]=actor;
        r.available_mask=1u<<c;r.leader_character=c;
        SudekiMpLanStoryControlObserveIdleContact(&w,&r);assert(idle_contact_lease(actor));
        r.leader_character=(c+1)%4;r.available_mask|=1u<<r.leader_character;
        SudekiMpLanStoryControlObserveIdleContact(&w,&r);assert(idle_contact_lease(actor));
        idle_remote=FALSE;assert(!idle_contact_lease(actor));idle_remote=TRUE;
        lease_ok=FALSE;assert(!idle_contact_lease(actor));lease_ok=TRUE;
    }
    SudekiMpLanStoryControlObserveIdleContact(&w,NULL);assert(!idle_contact_lease(actor));
    idle_binding=FALSE;SudekiMpLanStoryControlObserveIdleContact(&w,&r);assert(!idle_contact_lease(actor));
    puts("story idle contact: host roster, remote authority, load, pause, cast and unknown-state guards passed");
    VirtualFree(base,0,MEM_RELEASE);
    puts("story Tal/Buki melee synthetic admission/ownership/busy/uncertain-return tests passed");
    return 0;
}
