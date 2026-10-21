/* Synthetic real ObserverSample regression; no game writes or hook install. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../src/hooks/lan_story_observer.c"
#include "../src/network/lan_party_story.c"
static uint8_t world[0x3a0],zones[2*0x54],group[0xd8],controller[0x250];
static uint8_t body[4][0x100],ais[4][0x180],modes[4][0x10];
static BOOL witness_ok=TRUE,world_ok=TRUE;
static unsigned abandoned;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpCleanroomEngineWorldReady(void) {return world_ok;}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && witness_ok;
}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor c) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,SUDEKIMP_CLEANROOM_ELCO,
        SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    for(unsigned i=0;i<4;++i) if(c==types[i]) return body[i];
    assert(0);return NULL;
}
static void ended(BOOL vanilla) {assert(vanilla);++abandoned;}
static const SudekiMpLanStoryObserverSplit owner={.ended=ended};
static SudekiMpControlUpdateDispatchWitness dispatch={.service_post_original_exact=1,.dispatch_serial=1};
static void fixture(void) {
    memset(world,0,sizeof(world));memset(zones,0,sizeof(zones));memset(group,0,sizeof(group));
    memset(controller,0,sizeof(controller));memset(body,0,sizeof(body));memset(ais,0,sizeof(ais));memset(modes,0,sizeof(modes));
    memset(&observed,0,sizeof(observed));memset(&published,0,sizeof(published));memset(last_actors,0,sizeof(last_actors));
    installed=last_exact=TRUE;exhausted=foreign_thread=FALSE;call_depth=0;native_thread=GetCurrentThreadId();
    split_active=split_transition=TRUE;split_exiting=FALSE;remote_mask=0;split_owner=&owner;
    local_transition=background_switch_tick=hold_started=0;holding=FALSE;last_logged_revision=0;
    abandoned=0;witness_ok=world_ok=TRUE;
    *(void **)(base+0x408d10u)=world;*(void **)(base+0x408d94u)=group;*(void **)(base+0x408da4u)=controller;
    *(void **)(world+0xcu)=zones;*(void **)(world+0x50u)=zones;*(unsigned *)(world+0x54u)=2;
    world[0x399u]=world[0x39au]=1;
    *(const char **)(zones+0x24u)="lica_tals_room";*(unsigned *)(zones+0x34u)=4;
    *(const char **)(zones+0x54u+0x24u)="illuminacastle";*(unsigned *)(zones+0x54u+0x34u)=3;
    for(unsigned i=0;i<4;++i) {*(void **)(body[i]+0x94u)=ais[i];*(void **)(ais[i]+0x10u)=body[i];*(void **)(ais[i]+0x3cu)=modes[i];}
    *(unsigned *)(group+0xccu)=1;*(void **)(group+0x90u)=body[2];*(void **)(controller+0x248u)=body[2];
    observed=(SudekiMpLanStoryScene){.epoch=6,.revision=6,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=12,.leader_seat=3,.inside_mask=8};
    strcpy(observed.world,"newbrightwater");strcpy(observed.temporary,"lica_tals_room");published=observed;
    last_world=world;last_descriptor=zones+0x54u;last_group=group;last_controller=controller;
    last_actors[2]=body[2];last_actors[3]=body[3];
}
static void replacement_and_reverse(void) {
    fixture();SudekiMpLanStoryScene result;
    uint8_t before[sizeof(world)];memcpy(before,world,sizeof(world));
    assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
    assert(result.phase==SUDEKIMP_LAN_STORY_READY && result.epoch==7 && result.available_mask==4 && result.leader_seat==2);
    assert(!strcmp(result.temporary,"lica_tals_room") && result.inside_mask==4);
    assert(abandoned==1 && !split_active && !split_transition && !memcmp(before,world,sizeof(world)));
    assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
    assert(result.phase==SUDEKIMP_LAN_STORY_READY && result.epoch==7 && result.available_mask==4 && result.inside_mask==4);
    /* Normal native exit is independently observed; adding Ailish back does
     * not preserve the room or its old inside mask. No control claim. */
    memset(observed.temporary,0,sizeof(observed.temporary));observed.inside_mask=0;*(void **)(world+0xcu)=zones+0x54u;
    *(unsigned *)(group+0xccu)=2;*(void **)(group+0x90u)=body[3];*(void **)(group+0x9cu)=body[2];
    *(void **)(controller+0x248u)=body[3];
    assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
    assert(result.phase==SUDEKIMP_LAN_STORY_READY && result.epoch==8 && result.available_mask==12 && result.leader_seat==3);
    assert(!result.temporary[0] && !result.inside_mask && abandoned==1);
}
static void unproved_temp(void) {
    for(unsigned mutation=0;mutation<6;++mutation) {
        fixture();SudekiMpLanStoryScene result;
        if(mutation==0) *(const char **)(zones+0x24u)="another_room";
        if(mutation==1) *(void **)(world+0x50u)=zones+0x54u;
        if(mutation==2) last_world=world+4;
        if(mutation==3) last_group=group+4;
        if(mutation==4) last_controller=controller+4;
        if(mutation==5) *(unsigned *)(world+0x54u)=4097;
        assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
        assert(result.phase!=SUDEKIMP_LAN_STORY_READY && !result.available_mask && !result.temporary[0]);
    }
    fixture();SudekiMpLanStoryScene result;remote_mask=4;observed.inside_mask=12;published=observed;
    assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
    assert(result.phase==SUDEKIMP_LAN_STORY_READY && !strcmp(result.temporary,"lica_tals_room") && result.inside_mask==4 && !remote_mask);
    fixture();witness_ok=FALSE;
    assert(!SudekiMpLanStoryObserverSample(controller,&dispatch,&result) && !abandoned);
    fixture();*(void **)(world+0x14u)=zones+0x54u;
    assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
    assert(split_transition && !abandoned && result.epoch==6); /* retained scene is not a new exact roster */
    fixture();*(unsigned *)(zones+0x34u)=3;
    assert(SudekiMpLanStoryObserverSample(controller,&dispatch,&result));
    assert(split_transition && !abandoned && result.epoch==6);
}
int main(void) {
    base=VirtualAlloc(NULL,0x410000u,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(base);
    replacement_and_reverse();unproved_temp();
    assert(VirtualFree(base,0,MEM_RELEASE));puts("StoryArrivalObserverTest: PASS (synthetic observer only)");return 0;
}
