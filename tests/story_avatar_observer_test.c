/* Synthetic native observation only: no hooks, constructor, deletion or
 * native controller code executes. Expected membership changes preserve the
 * world epoch; only a complete typed party receipt can publish zero heroes. */
#include "../src/hooks/lan_story_observer.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpLanStoryAvatarPartyObservation party;
static BOOL party_owned,party_exact,witness_exact=TRUE;
static void *heroes[4];
BOOL SudekiMpLanStoryAvatarPartyRetains(void) { return party_owned; }
BOOL SudekiMpLanStoryAvatarPartyObserve(SudekiMpLanStoryAvatarPartyObservation *out) {
    if(!party_owned || !party_exact) return FALSE;
    *out=party; return TRUE;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_exact && w && w->dispatch_serial && w->service_post_original_exact;
}
BOOL SudekiMpCleanroomEngineWorldReady(void) { return TRUE; }
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor type) {
    if(type==SUDEKIMP_CLEANROOM_BUKI) return heroes[0];
    if(type==SUDEKIMP_CLEANROOM_ELCO) return heroes[1];
    if(type==SUDEKIMP_CLEANROOM_TAL) return heroes[2];
    if(type==SUDEKIMP_CLEANROOM_AILISH) return heroes[3];
    return NULL;
}
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }

int main(void) {
    base=VirtualAlloc(NULL,0x410000u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(base);
    uint8_t world[0x400]={0},group[0x100]={0},controller[0x300]={0},descriptor[0x40]={0};
    uint8_t tal[0x200]={0},ailish[0x200]={0},tal_ai[0x180]={0},ailish_ai[0x180]={0},talos[0x200]={0};
    *(void **)(base+0x408d10)=world; *(void **)(base+0x408d94)=group;
    *(void **)(base+0x408da4)=controller; *(void **)(world+0xc)=descriptor;
    world[0x399]=world[0x39a]=1; *(uint32_t *)(descriptor+0x34)=3;
    *(void **)(tal+0x94)=tal_ai; *(void **)(tal_ai+0x10)=tal;
    *(void **)(ailish+0x94)=ailish_ai; *(void **)(ailish_ai+0x10)=ailish;
    heroes[2]=tal; heroes[3]=ailish;
    *(unsigned *)(group+0xcc)=2; *(void **)(group+0x90)=tal; *(void **)(group+0x9c)=ailish;
    *(void **)(controller+0x248)=tal;
    installed=TRUE; observed.epoch=3; strcpy(observed.world,"newbrightwater");
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=1,.service_post_original_exact=TRUE};
    SudekiMpLanStoryScene scene; SudekiMpLanStoryNativeRoster original,r;
    assert(SudekiMpLanStoryObserverSample(controller,&w,&scene));
    assert(scene.phase==SUDEKIMP_LAN_STORY_READY && scene.available_mask==12 && scene.leader_seat==2);
    assert(SudekiMpLanStoryObserverRoster(controller,&w,&scene,&original));
    assert(original.native_leader==tal && !original.native_avatar_generation);
    uint32_t epoch=scene.epoch,revision=scene.revision;

    /* A positively owned native transition is LOADING, never an empty READY
     * group inferred from failed hero lookup. The spawn epoch stays intact. */
    party_owned=party_exact=TRUE;
    party=(SudekiMpLanStoryAvatarPartyObservation){.phase=SUDEKIMP_AVATAR_PARTY_FILTER_PENDING,
        .epoch=epoch,.world=world,.group=group,.controller=controller,
        .native_leader=tal,.local_player=1,.spawn_generation=8,.member_count=2,
        .members={tal,ailish},.hero_mask=12,.leader_character=2,.heroes={NULL,NULL,tal,ailish}};
    ++w.dispatch_serial;
    assert(SudekiMpLanStoryObserverSample(controller,&w,&scene));
    assert(scene.epoch==epoch && scene.revision>revision && scene.phase==SUDEKIMP_LAN_STORY_LOADING);
    assert(!SudekiMpLanStoryObserverRoster(controller,&w,&scene,&r));
    assert(!SudekiMpLanStoryObserverNativeRosterExact(&original));

    party.phase=SUDEKIMP_AVATAR_PARTY_LEAD_PENDING;
    party.member_count=3; party.members[2]=talos;
    *(unsigned *)(group+0xcc)=3; *(void **)(group+0xa8)=talos; ++w.dispatch_serial;
    assert(SudekiMpLanStoryObserverSample(controller,&w,&scene));
    assert(scene.epoch==epoch && scene.phase==SUDEKIMP_LAN_STORY_LOADING);
    assert(!SudekiMpLanStoryObserverRoster(controller,&w,&scene,&r));

    party.phase=SUDEKIMP_AVATAR_PARTY_READY; party.native_leader=talos;
    party.hero_mask=0; party.leader_character=4; party.member_count=1;
    memset(party.heroes,0,sizeof(party.heroes)); memset(party.members,0,sizeof(party.members)); party.members[0]=talos;
    memset(group+0x90,0,0x30); *(unsigned *)(group+0xcc)=1; *(void **)(group+0x90)=talos;
    *(void **)(controller+0x248)=talos; heroes[2]=heroes[3]=NULL; ++w.dispatch_serial;
    assert(SudekiMpLanStoryObserverSample(controller,&w,&scene));
    assert(scene.epoch==epoch && scene.phase==SUDEKIMP_LAN_STORY_READY && !scene.available_mask && scene.leader_seat==4);
    assert(SudekiMpLanStoryObserverRoster(controller,&w,&scene,&r));
    assert(r.native_leader==talos && r.native_avatar_player==1 && r.native_avatar_generation==8);
    assert(SudekiMpLanStoryObserverRosterStillExact(&w,&r));
    assert(SudekiMpLanStoryObserverNativeRosterExact(&r));
    for(unsigned c=0;c<4u;++c) assert(!r.actors[c] && !r.ai[c]);

    for(unsigned changed=0;changed<3u;++changed) {
        SudekiMpLanStoryNativeRoster stale=r;
        if(!changed) stale.native_leader=tal;
        else if(changed==1) ++stale.native_avatar_generation;
        else stale.native_avatar_player=0;
        assert(!SudekiMpLanStoryObserverRosterStillExact(&w,&stale));
        assert(!SudekiMpLanStoryObserverNativeRosterExact(&stale));
    }
    *(void **)(controller+0x248)=tal;
    assert(!SudekiMpLanStoryObserverNativeRosterExact(&r));
    *(void **)(controller+0x248)=talos;
    party_exact=FALSE; ++w.dispatch_serial;
    assert(SudekiMpLanStoryObserverSample(controller,&w,&scene));
    assert(scene.epoch==epoch+1u && scene.phase==SUDEKIMP_LAN_STORY_LOADING);
    assert(!SudekiMpLanStoryObserverNativeRosterExact(&r));
    /* A local mode flag without the positive owner cannot admit the actor. */
    party_owned=FALSE; ++w.dispatch_serial;
    assert(SudekiMpLanStoryObserverSample(controller,&w,&scene));
    assert(scene.phase!=SUDEKIMP_LAN_STORY_READY);
    VirtualFree(base,0,MEM_RELEASE); base=NULL;
    puts("PASS: exact native observer hero-to-avatar transition, epoch preservation, typed anchor and revoked-owner rejection");
    return 0;
}
