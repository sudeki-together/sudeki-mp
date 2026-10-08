/* Synthetic native-memory ownership and terminal drain checks. Native pause
 * calls are modeled; no game hook, group mutation or constructor executes. */
#include "../src/hooks/lan_story_client.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpLanStoryAvatarPartyObservation party;
static BOOL party_exact=TRUE,observer_exact=TRUE,world_ready=TRUE;
static void *heroes[4];
static unsigned party_calls,observer_calls,party_refuse_call;
static BOOL menu_owned,menu_prepared,input_exact=TRUE,release_verify_once,release_pending;
static unsigned release_calls,release_mutations,reacquire_calls;
BOOL SudekiMpLanPartyMenuNativeOwnsPause(void) { return menu_owned; }
BOOL SudekiMpLanPartyMenuNativePauseExact(BOOL *paused) {
    *paused=world_pause.speed && world_pause.speed[0x28]; return menu_owned;
}
BOOL SudekiMpLanPartyMenuNativePreparePauseExit(void) {
    if(!menu_owned) return FALSE;
    menu_prepared=TRUE; return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeExitPauseExact(BOOL *paused) {
    *paused=world_pause.speed && world_pause.speed[0x28]; return menu_prepared;
}
static void model_pause(unsigned extra) {
    *(uint16_t *)(world_pause.speed+0x2a)=world_pause.speed_references+extra;
    world_pause.speed[0x28]=(uint8_t)(extra!=0);
    world_pause.gel[0x23]=(uint8_t)(world_pause.gel_references+extra);
    for(unsigned i=0;i<world_pause.count;++i)
        entity_pause[i].entity[0x2b]=(uint8_t)entity_references(&entity_pause[i],extra);
}
BOOL SudekiMpLanPartyMenuNativeFinishPauseExit(void) {
    ++release_calls;
    assert(menu_prepared && menu_owned);
    if(release_pending) { release_pending=menu_owned=FALSE; return TRUE; }
    assert(registry_exact(1)); ++release_mutations; model_pause(0);
    if(release_verify_once) {
        release_verify_once=FALSE; release_pending=TRUE; return FALSE;
    }
    menu_owned=FALSE; return TRUE;
}
BOOL SudekiMpLanPartyMenuNativeReacquirePauseExit(void) {
    ++reacquire_calls; assert(menu_prepared);
    assert(registry_exact(menu_owned?1:0));
    model_pause(1); menu_owned=TRUE; return TRUE;
}
static BOOL input_fenced(void *controller) {
    return input_exact && controller==retained_roster.controller;
}
BOOL SudekiMpRestoreRelativeCallHook(SudekiMpRelativeCallHook *hook) { (void)hook; return FALSE; }
BOOL SudekiMpLanStoryTaskTraceGetStatus(SudekiMpLanStoryTaskTraceStatus *out) { (void)out; return FALSE; }
BOOL SudekiMpLanStoryLoadGetResult(SudekiMpStoryLoadResult *out) { (void)out; return FALSE; }
void SudekiMpLogWrite(const char *text) { (void)text; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLanStoryAvatarPartyObserve(SudekiMpLanStoryAvatarPartyObservation *out) {
    ++party_calls;
    if(!party_exact || party_calls==party_refuse_call) return FALSE;
    *out=party; return TRUE;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    ++observer_calls; return r && observer_exact;
}
BOOL SudekiMpCleanroomEngineWorldReady(void) { return world_ready; }
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor type) {
    if(type==SUDEKIMP_CLEANROOM_BUKI) return heroes[0];
    if(type==SUDEKIMP_CLEANROOM_ELCO) return heroes[1];
    if(type==SUDEKIMP_CLEANROOM_TAL) return heroes[2];
    if(type==SUDEKIMP_CLEANROOM_AILISH) return heroes[3];
    return NULL;
}
static void model_call(unsigned rva,void *target) {
    int32_t relative=(int32_t)((uintptr_t)target-(uintptr_t)(base+rva+5u));
    base[rva]=0xe8; memcpy(base+rva+1u,&relative,4u);
}
static void terminal_drain_fixture(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene) {
    uint8_t registry[0x40]={0},scheduler[0x20]={0},speed[0x30]={0},gel[0x4c]={0};
    uint8_t trigger[0x200]={0},cluster[0x40]={0},manager[0x1008c]={0};
    uint8_t *entities[]={roster->native_leader};
    *(void **)(base+REGISTRY_GLOBAL)=registry; *(void **)(base+SCHEDULER_GLOBAL)=scheduler;
    *(void **)(base+SPEED_GLOBAL)=speed; *(void **)(base+GEL_GLOBAL)=gel;
    *(void **)(base+TRIGGER_GLOBAL)=trigger; *(void **)(base+CLUSTER_GLOBAL)=cluster;
    *(void **)trigger=base+0x2c5458; *(void **)(trigger+8)=base+0x2c5460;
    *(void **)(trigger+0x10)=base+0x2c5470;
    *(void **)cluster=base+0x2c7b30; *(void **)(cluster+8)=base+0x2c7b44;
    *(void **)(cluster+0x1c)=base+0x2c7b4c;
    *(void **)(gel+0x30)=manager; *(unsigned *)(registry+0x34)=1;
    *(void **)(registry+0x3c)=entities;
    model_call(TRIGGER_SWEEP_CALL,trigger_sweep); model_call(TRIGGER_DISPATCH_CALL,trigger_dispatch);
    model_call(CLUSTER_EXIT_QUERY,cluster_exit_query); model_call(CLUSTER_ENTRY_QUERY,cluster_entry_query);
    model_call(ACTIVITY_PAUSE_CALL,activity_pause); model_call(ACTIVITY_RESUME_CALL,activity_resume);
    for(unsigned i=0;i<2;++i)
        trigger_hooks[i].installed=cluster_hooks[i].installed=activity_hooks[i].installed=TRUE;
    assert(capture_registry());
    retained_roster=*roster; retained_scene=*scene; input_closed=input_fenced;
    attempted=menu_owned=TRUE; retained_reference=1; phase=SUDEKIMP_STORY_CLIENT_PAUSED;
    model_pause(1);
    assert(!exit_roster_exact());
    assert(SudekiMpLanStoryClientPrepareExit(NULL) && exit_prepared);
    observer_exact=FALSE; /* Runtime retires Observer before balancing pause. */
    unsigned observed_before=observer_calls;
    assert(!roster_matches(roster,scene));
    assert(observer_calls==observed_before+1u);
    observed_before=observer_calls;
    assert(exit_roster_exact() && observer_calls==observed_before);
    assert(!roster_matches_scope(roster,scene,TRUE)); /* Cannot borrow a caller tuple. */
    presentation_active=TRUE; assert(!exit_roster_exact()); presentation_active=FALSE;
    assert(!SudekiMpLanStoryClientCleanupRosterExact(roster)); /* No presentation backdoor. */

    /* Every refusal retains the pause and performs no native release. */
    party_exact=FALSE; assert(!SudekiMpLanStoryClientDrain(NULL)); party_exact=TRUE;
    ++party.spawn_generation; assert(!SudekiMpLanStoryClientDrain(NULL)); --party.spawn_generation;
    party.phase=SUDEKIMP_AVATAR_PARTY_UNKNOWN; assert(!SudekiMpLanStoryClientDrain(NULL));
    party.phase=SUDEKIMP_AVATAR_PARTY_READY;
    party_refuse_call=party_calls+2u; assert(!SudekiMpLanStoryClientDrain(NULL));
    assert(party_calls==party_refuse_call); party_refuse_call=0;
    *(void **)((uint8_t *)roster->group+0x90)=manager;
    assert(!SudekiMpLanStoryClientDrain(NULL));
    *(void **)((uint8_t *)roster->group+0x90)=roster->native_leader;
    *(unsigned *)((uint8_t *)roster->group+0xcc)=2;
    assert(!SudekiMpLanStoryClientDrain(NULL));
    *(unsigned *)((uint8_t *)roster->group+0xcc)=1;
    *(void **)((uint8_t *)roster->controller+0x248)=manager;
    assert(!SudekiMpLanStoryClientDrain(NULL));
    *(void **)((uint8_t *)roster->controller+0x248)=roster->native_leader;
    *(void **)(base+WORLD_GLOBAL)=manager; assert(!SudekiMpLanStoryClientDrain(NULL));
    *(void **)(base+WORLD_GLOBAL)=roster->world;
    input_exact=FALSE; assert(!SudekiMpLanStoryClientDrain(NULL)); input_exact=TRUE;
    entities[0]=manager; assert(!SudekiMpLanStoryClientDrain(NULL)); entities[0]=roster->native_leader;
    ++entities[0][0x2b]; assert(!SudekiMpLanStoryClientDrain(NULL)); --entities[0][0x2b];
    *(unsigned *)(trigger+0x14)=1; assert(!SudekiMpLanStoryClientDrain(NULL));
    *(unsigned *)(trigger+0x14)=0;
    assert(!release_calls && menu_owned && attempted && retained_reference==1 && registry_exact(1));

    assert(SudekiMpLanStoryClientDrain(NULL));
    assert(release_calls==1 && release_mutations==1 && !menu_owned && !attempted &&
        exit_released && !retained_reference && registry_exact(0));
    assert(!roster_matches(roster,scene)); /* Gameplay remains closed even after release. */
    party_exact=FALSE; assert(!SudekiMpLanStoryClientReacquireExit(NULL)); party_exact=TRUE;
    ++party.spawn_generation; assert(!SudekiMpLanStoryClientReacquireExit(NULL)); --party.spawn_generation;
    assert(!reacquire_calls && registry_exact(0));
    assert(SudekiMpLanStoryClientReacquireExit(NULL));
    assert(reacquire_calls==1 && menu_owned && attempted && !exit_released &&
        retained_reference==1 && registry_exact(1));
    assert(SudekiMpLanStoryClientReacquireExit(NULL));
    assert(reacquire_calls==2 && registry_exact(1)); /* Retry never adds a second reference. */

    /* Native release may return before its final ownership verification.
     * A later Drain verifies the return without decrementing twice. */
    release_verify_once=TRUE;
    assert(!SudekiMpLanStoryClientDrain(NULL));
    assert(release_calls==2 && release_mutations==2 && release_pending && registry_exact(0));
    assert(SudekiMpLanStoryClientDrain(NULL));
    assert(release_calls==3 && release_mutations==2 && !menu_owned && registry_exact(0));
    assert(SudekiMpLanStoryClientDrain(NULL));
    assert(release_calls==3 && release_mutations==2 && registry_exact(0));
    clear_observation(); observer_exact=TRUE; menu_prepared=FALSE;
}

int main(void) {
    base=VirtualAlloc(NULL,0x410000u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(base);
    uint8_t world[0x400]={0},group[0x100]={0},controller[0x300]={0},descriptor[0x40]={0};
    uint8_t actor[0x200]={0},ai[0x180]={0}; int unrelated;
    installed=TRUE; native_thread=GetCurrentThreadId();
    *(void **)(base+WORLD_GLOBAL)=world; *(void **)(base+GROUP_GLOBAL)=group;
    *(void **)(base+CONTROLLER_GLOBAL)=controller;
    *(void **)(world+0xc)=descriptor; world[0x399]=world[0x39a]=1;
    *(uint32_t *)(descriptor+0x34)=3;
    *(unsigned *)(group+0xcc)=1; *(void **)(group+0x90)=actor;
    *(void **)(controller+0x248)=actor;
    SudekiMpLanStoryScene scene={.epoch=7,.revision=9,.phase=SUDEKIMP_LAN_STORY_READY,.leader_seat=4};
    strcpy(scene.world,"newbrightwater");
    SudekiMpLanStoryNativeRoster r={.dispatch_serial=19,.epoch=7,.revision=9,.leader_character=4,
        .world=world,.descriptor=descriptor,.group=group,.controller=controller,
        .native_leader=actor,.native_avatar_generation=5,.native_avatar_player=1};
    party=(SudekiMpLanStoryAvatarPartyObservation){.phase=SUDEKIMP_AVATAR_PARTY_READY,
        .epoch=7,.spawn_generation=5,.local_player=1,.leader_character=4,
        .world=world,.group=group,.controller=controller,.native_leader=actor,
        .members={actor},.member_count=1};
    assert(SudekiMpLanStoryAvatarPartyRosterExact(&r));
    assert(roster_matches(&r,&scene) && observer_calls==1 && party_calls>=3);
    SudekiMpLanStoryNativeRoster saved=r;
    for(unsigned which=0;which<14;++which) {
        r=saved;
        switch(which) {
        case 0:r.native_leader=&unrelated; break;
        case 1:++r.native_avatar_generation; break;
        case 2:r.native_avatar_generation=0; break;
        case 3:++r.native_avatar_player; break;
        case 4:r.native_avatar_player=4; break;
        case 5:r.actors[2]=actor; break;
        case 6:r.ai[2]=ai; break;
        case 7:r.available_mask=4; break;
        case 8:r.leader_character=2; break;
        case 9:++r.epoch; break;
        case 10:++r.revision; break;
        case 11:r.controller=&unrelated; break;
        case 12:r.group=&unrelated; break;
        case 13:r.world=&unrelated; break;
        }
        assert(!same_roster(&r,&saved)); assert(!roster_matches(&r,&scene));
    }
    r=saved;
    SudekiMpLanStoryAvatarPartyObservation saved_party=party;
    for(unsigned which=0;which<13;++which) {
        party=saved_party;
        switch(which) {
        case 0:party.phase=SUDEKIMP_AVATAR_PARTY_LEAD_PENDING; break;
        case 1:party.phase=SUDEKIMP_AVATAR_PARTY_UNKNOWN; break;
        case 2:++party.spawn_generation; break;
        case 3:++party.local_player; break;
        case 4:party.hero_mask=4; break;
        case 5:party.heroes[2]=actor; break;
        case 6:party.member_count=2; break;
        case 7:party.members[0]=&unrelated; break;
        case 8:party.members[1]=&unrelated; break;
        case 9:party.native_leader=&unrelated; break;
        case 10:party.controller=&unrelated; break;
        case 11:party.world=&unrelated; break;
        case 12:++party.epoch; break;
        }
        assert(!roster_matches(&r,&scene));
    }
    party=saved_party;
    party_exact=FALSE; assert(!roster_matches(&r,&scene)); party_exact=TRUE;
    observer_exact=FALSE; assert(!roster_matches(&r,&scene)); observer_exact=TRUE;
    world_ready=FALSE; assert(!roster_matches(&r,&scene)); world_ready=TRUE;
    *(void **)(controller+0x248)=&unrelated; assert(!roster_matches(&r,&scene));
    *(void **)(controller+0x248)=actor;
    *(void **)(group+0x90)=&unrelated; assert(!roster_matches(&r,&scene));
    *(void **)(group+0x90)=actor;
    *(unsigned *)(group+0xcc)=0; assert(!roster_matches(&r,&scene)); *(unsigned *)(group+0xcc)=1;
    scene.inside_mask=1; assert(!roster_matches(&r,&scene)); scene.inside_mask=0;
    strcpy(scene.temporary,"lnbr_church"); assert(!roster_matches(&r,&scene));
    memset(scene.temporary,0,sizeof(scene.temporary));
    assert(roster_matches(&r,&scene));
    terminal_drain_fixture(&r,&scene);

    /* Regular retained hero route remains independent of the Dev owner. */
    memset(&r,0,sizeof(r)); r=saved; r.native_avatar_generation=0; r.native_avatar_player=4;
    r.available_mask=4; r.leader_character=2; r.actors[2]=actor; r.ai[2]=ai;
    scene.available_mask=4; scene.leader_seat=2; memset(scene.temporary,0,sizeof(scene.temporary));
    heroes[2]=actor; *(void **)(actor+0x94)=ai; *(void **)(ai+0x10)=actor;
    party_exact=observer_exact=FALSE;
    assert(roster_matches(&r,&scene));
    heroes[2]=&unrelated; assert(!roster_matches(&r,&scene)); heroes[2]=actor;
    *(void **)(ai+0x10)=&unrelated; assert(!roster_matches(&r,&scene));
    assert(VirtualFree(base,0,MEM_RELEASE)); base=NULL;
    puts("story avatar roster: PASS (typed owner, Observer-retired terminal drain/reacquire, reference retry and regular isolation; native pause modeled)");
    return 0;
}
