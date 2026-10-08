/* Synthetic native-memory/control fixture. Known entry signatures seed an
 * isolated executable allocation; only the bounded speed setter executes.
 * AI wrappers are recording callbacks. No game, native party switch, hooks
 * or live input is exercised. */
#define SUDEKIMP_LAN_PARTY_CONTROL_TESTING 1
#include "../src/hooks/control_separation.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpLanStoryAvatarPartyObservation observed_party;
static SudekiMpLanStoryAvatarSpawnObservation observed_spawn[4];
static SudekiMpLanStoryNativeRoster observed_roster;
static BOOL observer_ready=TRUE,party_ready=TRUE,input_ready=TRUE,body_ready=TRUE;
static unsigned acquire_calls,release_calls;
/* The real dispatch witness compares callback addresses. Those callbacks are
 * linked but never entered by this fixture; abort if one tries unrelated UI
 * or input work rather than silently supplying a fabricated native result. */
void SudekiMpLogFormat(const char *format,...) { (void)format; assert(!"unexpected runtime log"); }
void SudekiMpLogWrite(const char *text) { (void)text; assert(!"unexpected runtime log"); }
BOOL SudekiMpInputBridgeGameplaySuppressed(void) { assert(!"unexpected input callback"); return TRUE; }
BOOL SudekiMpSplitScreenFixedThreeSeatEnabled(void) { assert(!"unexpected split callback"); return FALSE; }
BOOL SudekiMpSplitScreenSharedInteractionModalActive(void) { assert(!"unexpected split callback"); return FALSE; }
BOOL SudekiMpSplitScreenFixedThreeCustomQuickMenuEnabled(void) { assert(!"unexpected split callback"); return FALSE; }
BOOL SudekiMpCombatContextGetSnapshot(unsigned player,SudekiMpPlayerCombatSnapshot *out) {
    (void)player; (void)out; assert(!"unexpected combat context"); return FALSE;
}
uint8_t SudekiMpLocalInputHubRequestedMask(void) { assert(!"unexpected local input"); return 0; }
SudekiMpPlayerStatehood *SudekiMpPlayerStatehoodRuntime(void) { assert(!"unexpected statehood"); return NULL; }
BOOL SudekiMpObserveCharacterSkill(void *actor,SudekiMpCharacterSkillState *state) {
    (void)actor; (void)state; assert(!"unexpected hero skill"); return FALSE;
}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor type) {
    (void)type; assert(!"unexpected canonical hero lookup"); return NULL;
}
BOOL SudekiMpLanPartyCastLocalNoncaster(void *actor) {
    (void)actor; assert(!"unexpected party cast"); return FALSE;
}
BOOL SudekiMpLanStoryCastLocalNoncaster(void *actor) {
    (void)actor; assert(!"unexpected story cast"); return FALSE;
}
BOOL SudekiMpLanStoryAvatarPartyObserve(SudekiMpLanStoryAvatarPartyObservation *out) {
    if(!party_ready) return FALSE;
    *out=observed_party; return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned player,uint32_t epoch,uint32_t generation,
    SudekiMpLanStoryAvatarSpawnObservation *out) {
    if(player>=4 || observed_spawn[player].epoch!=epoch || observed_spawn[player].generation!=generation) return FALSE;
    *out=observed_spawn[player]; return TRUE;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    return observer_ready && SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
        !memcmp(r,&observed_roster,sizeof(*r));
}
BOOL SudekiMpLanStoryInputHostFenceExact(void *controller,void *actor) {
    return input_ready && controller==observed_roster.controller && actor==observed_roster.native_leader;
}
void *SudekiMpCleanroomEngineGenericEntity(const char *name) { (void)name; return NULL; }
static void ai_call(void **slot,BOOL acquire) {
    uint8_t *ai=*(uint8_t **)((uint8_t *)*slot+0x94u),*mode=*(uint8_t **)(ai+0x3cu);
    if(acquire) { ++acquire_calls; ++*(int16_t *)(ai+0x16au); mode[0xb]=0; }
    else { ++release_calls; --*(int16_t *)(ai+0x16au); mode[0xb]=1; }
}
static void acquire_ai(void *slot) { ai_call(slot,TRUE); }
static void release_ai(void *slot) { ai_call(slot,FALSE); }
static BOOL idle(const SudekiMpLanPartyLease *key,void *actor,const SudekiMpControlUpdateDispatchWitness *w) {
    return key && actor && w && body_ready;
}
static void entry_signatures(void) {
    static const uint8_t acquire[]={0x8b,0x44,0x24,4,0x85,0xc0,0x74,0x18,0x8b,0,0x85,0xc0,
        0x74,0x12,0x8b,0x80,0x94,0,0,0,0x85,0xc0,0x74,8,0x6a,1,0x50,0xe8,0x60,0x62,0xff,0xff,0xc3};
    uint8_t release[sizeof(acquire)]; memcpy(release,acquire,sizeof(release)); release[25]=0; release[28]=0x30;
    static const uint8_t ref[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x1c,0x53,0x8b,0x5d,8};
    static const uint8_t movement[]={0x53,0x8b,0x5c,0x24,8,0x55,0x8b,0x6c,0x24,0x10,0x56,0x57,0x8b,0xd3};
    memcpy(game_base+RVA_AI_OVERRIDE_CONTROL,acquire,sizeof(acquire));
    memcpy(game_base+RVA_AI_DEFAULT_CONTROL,release,sizeof(release));
    memcpy(game_base+0xec350u,ref,sizeof(ref));
    memcpy(game_base+RVA_ARBITER_MOVEMENT,movement,sizeof(movement));
    memcpy(game_base+RVA_ARBITER_COMBAT_INPUT,expected_arbiter_combat_input_entry,sizeof(expected_arbiter_combat_input_entry));
    memcpy(game_base+RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE,
        expected_movement_controller_set_speed_immediate_entry,sizeof(expected_movement_controller_set_speed_immediate_entry));
    assert(party_native_entries_exact());
}
int main(void) {
    __asm__ volatile("" : : "g"(&position_set_forward),"g"(&lan_arena_first_person_held_fire));
    game_base=VirtualAlloc(NULL,0x410000u,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE); assert(game_base);
    entry_signatures(); service_only_mode=TRUE;
    void **update_slot=(void **)(game_base+RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
    *update_slot=(void *)service_control_update_observers;
    controller_update_vtable_hook.installed=TRUE; controller_update_vtable_hook.slot=update_slot;
    controller_update_vtable_hook.original_value=game_base+RVA_CONTROLLER_UPDATE;
    controller_update_vtable_hook.replacement_value=*update_slot;
    original_controller_update=(ControllerUpdateFunction)(game_base+RVA_CONTROLLER_UPDATE);
    active_control_update_dispatches=1; update_observer_registry_generation=1;
    SudekiMpControlUpdateDispatchWitness w;
    ControlUpdateDispatchFrame dispatch={.active_witness=&w,.dispatch_serial=19,.native_thread_id=GetCurrentThreadId(),
        .update_depth=1,.original_call_count=1,.tls_exact=1,.service_only=1};
    control_update_dispatch_tls=TlsAlloc(); assert(control_update_dispatch_tls!=TLS_OUT_OF_INDEXES);
    assert(TlsSetValue(control_update_dispatch_tls,&dispatch));
    build_control_update_dispatch_witness(&dispatch,SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,1,1,&w);
    assert(SudekiMpControlSeparationUpdateDispatchWitnessStillExact(&w));
    uint8_t actors[2][0x140]={{0}},ai[2][0x180]={{0}},modes[2][0x10]={{0}},movement[2][0xc0]={{0}},arbiter[2][0x70]={{0}};
    uint8_t group[0x100]={0},controller[0x260]={0}; int world,descriptor;
    *(void **)controller=game_base+0x2c9f5cu; *(void **)(controller+0x248)=actors[0];
    *(unsigned *)(group+0xcc)=1; *(void **)(group+0x90)=actors[0];
    for(unsigned p=0;p<2;++p) {
        *(void **)actors[p]=game_base+0x2d55d4u;
        *(void **)(actors[p]+0x94)=ai[p]; *(void **)ai[p]=game_base+0x2d4924u;
        *(void **)(ai[p]+0x10)=actors[p]; *(void **)(ai[p]+0x3c)=modes[p]; modes[p][0xb]=(uint8_t)p;
        *(void **)(actors[p]+0x80)=movement[p]; *(void **)movement[p]=game_base+0x2c8644u;
        *(void **)(movement[p]+0x10)=actors[p]; movement[p][0xbe]=8;
        *(void **)(actors[p]+0x90)=arbiter[p]; *(void **)arbiter[p]=game_base+0x2cc9acu;
        *(void **)(arbiter[p]+0x10)=actors[p];
        observed_spawn[p]=(SudekiMpLanStoryAvatarSpawnObservation){.seat=p,.epoch=3,.generation=77+p,
            .actor=actors[p],.world=&world,.ready=TRUE};
    }
    observed_roster=(SudekiMpLanStoryNativeRoster){.dispatch_serial=w.dispatch_serial,.epoch=3,.revision=8,
        .leader_character=4,.world=&world,.descriptor=&descriptor,.group=group,.controller=controller,
        .native_leader=actors[0],.native_avatar_generation=77,.native_avatar_player=0};
    observed_party=(SudekiMpLanStoryAvatarPartyObservation){.phase=SUDEKIMP_AVATAR_PARTY_READY,
        .epoch=3,.spawn_generation=77,.leader_character=4,.world=&world,.group=group,.controller=controller,
        .native_leader=actors[0],.members={actors[0]},.member_count=1};
    party_test_acquire=acquire_ai; party_test_release=release_ai;
    assert(SudekiMpLanPartyControlStoryBegin(&w,&observed_roster));
    SudekiMpLanPartyLease keys[2];
    for(unsigned p=0;p<2;++p) {
        SudekiMpLanPartyLease connection={.seat=(uint8_t)p,.token=100+p,.generation=1};
        assert(SudekiMpLanPartyControlStoryAvatarEntity(&w,&observed_roster,p,actors[p],77+p));
        assert(SudekiMpLanPartyControlStoryNextLease(&w,&connection,4+p,&keys[p]));
    }
    input_ready=FALSE; assert(!SudekiMpLanPartyControlStoryAcquire(&w,&observed_roster,&keys[0],idle));
    assert(!SudekiMpLanPartyControlStoryRetainsKey(&keys[0])); input_ready=TRUE;
    modes[0][0xb]=1; assert(!SudekiMpLanPartyControlStoryAcquire(&w,&observed_roster,&keys[0],idle)); modes[0][0xb]=0;
    *(float *)(movement[0]+0x24)=5; *(float *)(movement[0]+0x28)=5;
    assert(SudekiMpLanPartyControlStoryAcquire(&w,&observed_roster,&keys[0],idle));
    assert(!acquire_calls && !release_calls && !*(int16_t *)(ai[0]+0x16a) && !modes[0][0xb]);
    assert(!*(float *)(movement[0]+0x24) && !*(float *)(movement[0]+0x28));
    assert(SudekiMpLanPartyControlStoryExact(&w,&observed_roster,&keys[0]));
    assert(SudekiMpLanPartyControlStoryAcquire(&w,&observed_roster,&keys[1],idle));
    assert(acquire_calls==1 && *(int16_t *)(ai[1]+0x16a)==1 && !modes[1][0xb]);
    body_ready=FALSE; assert(!SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[0],idle));
    assert(SudekiMpLanPartyControlStoryRetainsKey(&keys[0]) && !release_calls); body_ready=TRUE;
    input_ready=FALSE; assert(!SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[0],idle)); input_ready=TRUE;
    ++observed_spawn[0].generation;
    assert(!SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[0],idle)); --observed_spawn[0].generation;
    *(int16_t *)(ai[0]+0x16a)=1;
    assert(!SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[0],idle)); *(int16_t *)(ai[0]+0x16a)=0;
    assert(SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[0],idle));
    assert(!SudekiMpLanPartyControlStoryRetainsKey(&keys[0]) && !release_calls && !modes[0][0xb]);
    assert(SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[0],idle));
    assert(SudekiMpLanPartyControlStoryDrain(&w,&observed_roster,&keys[1],idle));
    assert(release_calls==1 && !*(int16_t *)(ai[1]+0x16a) && modes[1][0xb]==1);
    assert(SudekiMpLanPartyControlStoryEnd());
    assert(TlsFree(control_update_dispatch_tls)); assert(VirtualFree(game_base,0,MEM_RELEASE));
    puts("story native avatar leader: PASS (synthetic no-AI-ref acquire/drain, fresh input/party lease, remote AI ref preserved)");
    return 0;
}
