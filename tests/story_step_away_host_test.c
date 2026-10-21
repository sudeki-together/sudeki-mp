/* Actual host coordinator with synthetic roster/transport/native spies. */
#include "../src/hooks/lan_story_runtime.c"
#include "../src/network/lan_party_story.c"
#define get32 handoff_get32
#define put32 handoff_put32
#include "../src/network/lan_story_handoff.c"
#undef get32
#undef put32
#include "../src/engine/party_ownership.c"
#include "../src/network/lan_story_step_away.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static SudekiMpLanStoryNativeRoster test_roster;
static SudekiMpLanStoryScene test_scene;
static SudekiMpLanPartyPeerStatus test_peer;
static BOOL roster_known=TRUE,observer_hold,split_hold,drain_ok=TRUE,dead_hold;
static BOOL have_presence;
static BOOL arrival_run,arrival_ack;
static unsigned arrival_acquires,arrival_publishes,arrival_moves;
static SudekiMpLanStoryControlState arrival_state;
BOOL SudekiMpLanStoryHostControlRemoteLead(const SudekiMpLanStoryNativeRoster *r) {
    return client_lead_spectate && arrival_run && r->available_mask==4 && r->leader_character==2;
}
BOOL SudekiMpLanStoryControlNextLease(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *connection,unsigned c,SudekiMpLanPartyLease *out) {
    (void)w;assert(arrival_run && c==2);*out=*connection;out->seat=2;out->generation=arrival_acquires+1;return TRUE;
}
BOOL SudekiMpLanStoryControlAcquire(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key) {
    (void)w;assert(arrival_run && r->actors[2]==test_roster.actors[2]);assert(key->seat==2);++arrival_acquires;return TRUE;
}
BOOL SudekiMpLanStoryControlRetainsKey(const SudekiMpLanPartyLease *k) {(void)k;return FALSE;}
static unsigned begins,drains,away_offers,parked_offers,request_logs,away_drains,away_returns,away_cancels;
static unsigned eligibility_calls,eligibility_mask,eligibility_away,activity_calls,activity_mask;
static BOOL fixture_control_exact,split_parked,activity_retained,activity_ok=TRUE,identity_unknown,cast_superseded;
static int return_result=1;
static void *recorded_remote[3];
static char last_drain[128];
void SudekiMpLogWrite(const char *s) { (void)s; }
void SudekiMpLogFormat(const char *format,...) {
    char text[1024]; va_list args; va_start(args,format); vsnprintf(text,sizeof(text),format,args); va_end(args);
    if(strstr(text,"offer_stepped_away")) ++away_offers;
    if(strstr(text,"offer_parked")) ++parked_offers;
    if(strstr(text,"story_step_away event=request")) ++request_logs;
    if(strstr(text,"host_control_drain")) snprintf(last_drain,sizeof(last_drain),"%s",strstr(text,"reason="));
}
BOOL SudekiMpLanStoryObserverRoster(void *controller,const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *s,SudekiMpLanStoryNativeRoster *r) {
    (void)controller;(void)w;(void)s;*r=test_roster;return roster_known;
}
void SudekiMpLanStoryControlObserveIdleContact(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {(void)w;(void)r;}
BOOL SudekiMpLanStoryObserverHolding(void) {return observer_hold;}
BOOL SudekiMpLanStorySplitActive(void) {return split_hold;}
void SudekiMpLanStoryDeathHoldSetActive(BOOL active) {dead_hold=active;}
void SudekiMpLanStoryDeathHoldSetRemoteActors(void *const actors[3]) {memcpy(recorded_remote,actors,sizeof(recorded_remote));}
void SudekiMpLanStoryDeathHoldReport(void) {}
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned p,SudekiMpLanPartyPeerStatus *out) {
    (void)s; if(p!=1) {memset(out,0,sizeof(*out));return TRUE;} *out=test_peer;return TRUE;
}
unsigned SudekiMpLanPartyPlayerCharacter(SudekiMpLanPartySession *s,unsigned p) {(void)s;return p==1?2:4;}
BOOL SudekiMpLanPartyStoryCatchupComplete(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l) {(void)s;(void)l;return TRUE;}
BOOL SudekiMpLanStoryControlBegin(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {(void)w;(void)r;++begins;return arrival_run;}
BOOL SudekiMpLanStoryControlDrain(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *l) {(void)w;(void)r;(void)l;++drains;return drain_ok;}
BOOL SudekiMpLanPartyRevokeStoryControl(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l) {(void)s;(void)l;return TRUE;}
void SudekiMpLanStoryRemoteUseService(void) {}
/* Unused action channels and native services remain closed in this fixture. */
BOOL SudekiMpLanPartyPublishStoryActionResult(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryActionResult *result) { (void)session;(void)lease;(void)result; return FALSE; }
BOOL SudekiMpLanPartyPublishStoryControl(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlState *state) { (void)session;(void)lease;assert(arrival_run);arrival_state=*state;++arrival_publishes;return TRUE; }
BOOL SudekiMpLanPartyStoryControlAcknowledged(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence) { (void)session;(void)lease;(void)fence; return arrival_ack; }
BOOL SudekiMpLanPartyTakeStoryAction(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryActionRequest *request) { (void)session;(void)lease;(void)now;(void)request; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryBlock(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryBlock *input,uint32_t *received_at) { (void)session;(void)lease;(void)now;(void)input;(void)received_at; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryMelee(SudekiMpLanPartySession *, const SudekiMpLanPartyLease *,uint32_t now,BOOL admitted,uint8_t *buttons,uint32_t *serial) { (void)now;(void)admitted;(void)buttons;(void)serial; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryMovement(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryMovement *input, uint32_t *received_at) { (void)session;(void)lease;(void)now;(void)input;(void)received_at; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryRanged(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryRanged *input,uint32_t *received_at) { (void)session;(void)lease;(void)now;(void)input;(void)received_at; return FALSE; }
BOOL SudekiMpLanStoryCastInstall(HMODULE image) { (void)image; return FALSE; }
unsigned SudekiMpLanStoryCastSubmit(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned slot) { (void)w;(void)roster;(void)key;(void)slot; return 0; }
unsigned SudekiMpLanStoryCastSubmitSpirit(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned variant) { (void)w;(void)roster;(void)key;(void)variant; return 0; }
BOOL SudekiMpLanStoryCastTryBind(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster) { (void)w;(void)roster; return FALSE; }
BOOL SudekiMpLanStoryControlBlock(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,BOOL held) { (void)held; return arrival_run; }
unsigned SudekiMpLanStoryControlEquipArmour(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) { (void)w;(void)roster;(void)key;(void)item; return 0; }
unsigned SudekiMpLanStoryControlEquipWeapon(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) { (void)w;(void)roster;(void)key;(void)item; return 0; }
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key) { (void)native_key; return fixture_control_exact; }
BOOL SudekiMpLanStoryControlLoadDrain(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanPartyLease *) {  return FALSE; }
unsigned SudekiMpLanStoryControlMeleeSnapshot(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,unsigned buttons) { (void)buttons; return 0; }
BOOL SudekiMpLanStoryControlMove(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key, float world_x,float world_z,BOOL *temporarily_held) { (void)native_key;(void)world_x;(void)world_z;*temporarily_held=FALSE;++arrival_moves;return arrival_run; }
unsigned SudekiMpLanStoryControlRanged(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned state) { (void)w;(void)roster;(void)key;(void)state; return 0; }
unsigned SudekiMpLanStoryControlUseItem(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) { (void)w;(void)roster;(void)key;(void)item; return 0; }
BOOL SudekiMpLanStoryDoorCameraInstall(HMODULE image) { (void)image; return FALSE; }
BOOL SudekiMpLanStoryHostControlBound(void *controller, const SudekiMpControlUpdateDispatchWitness *witness, const SudekiMpLanStoryScene *scene) { (void)controller;(void)witness;(void)scene; return arrival_run; }
BOOL SudekiMpLanStoryHostControlExteriorBound(void *controller, const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *,unsigned character) { (void)controller;(void)character; return FALSE; }
BOOL SudekiMpLanStoryHostControlExteriorNativeBound(const SudekiMpLanStoryNativeRoster *,unsigned character) { (void)character; return FALSE; }
BOOL SudekiMpLanStoryHostControlExteriorNativeReady(const SudekiMpLanStoryNativeRoster *,unsigned character) { (void)character; return FALSE; }
BOOL SudekiMpLanStoryHostControlExteriorNativeSteeringReady(const SudekiMpLanStoryNativeRoster *,unsigned character) { (void)character; return FALSE; }
BOOL SudekiMpLanStoryMenuCapturesInput(void) {  return FALSE; }
BOOL SudekiMpLanStoryRemoteInteriorMemberParked(unsigned character) { (void)character; return FALSE; }
BOOL SudekiMpLanStoryRemoteUseActive(const void *actor) { (void)actor; return FALSE; }
BOOL SudekiMpLanStoryRemoteUseTry(void *actor) { (void)actor; return FALSE; }
void SudekiMpLanStoryResearch54Input(unsigned player,BOOL took,uint32_t receipt,DWORD now, BOOL fresh,BOOL packet_moving,BOOL applied_moving,BOOL held) { (void)player;(void)took;(void)receipt;(void)now;(void)fresh;(void)packet_moving;(void)applied_moving;(void)held;  }
BOOL SudekiMpLanStoryShotsAim(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,unsigned character, const SudekiMpLanStoryRanged *input,uint32_t receipt) { (void)w;(void)roster;(void)character;(void)input;(void)receipt; return FALSE; }
void SudekiMpLanStorySplitClearReturned(void) {   }
void SudekiMpLanStorySplitSetEligibility(BOOL host_ready,uint8_t outside_mask,uint8_t away_mask) { (void)host_ready;++eligibility_calls;eligibility_mask=outside_mask;eligibility_away=away_mask; }
BOOL SudekiMpLogResearchEnabled(void) {  return FALSE; }
BOOL SudekiMpLanPartyGetPresence(SudekiMpLanPartySession *s,SudekiMpLanPartyPresence *out) {(void)s;*out=story_presence;return have_presence;}
static void run(void) {service_story_controls((void *)1,(void *)2,&test_scene);}
static void held(void) {
    host_control[1].connection=test_peer.lease;host_control[1].native_key=test_peer.lease;
    host_control[1].native_key.seat=2;host_control[1].ready=host_control[1].acquired=TRUE;
    host_control[1].fence=(SudekiMpLanStoryControlFence){1,1,1,1,1,2};
    story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_DRAINING;
    story_presence.ownership.controlling=3;
}
static void test_arrival_runtime(void) {
    memset(host_control,0,sizeof(host_control));
    arrival_run=fixture_control_exact=client_lead_spectate=TRUE;
    observer_hold=split_hold=split_attempted=FALSE;
    test_scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=4,.leader_seat=2,.inside_mask=4,.world="newbrightwater",.temporary="lica_tals_room"};
    test_roster.available_mask=4;test_roster.leader_character=2;
    story_presence.ownership.away=2;story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_AI;
    story_presence.ownership.controlling=1;story_presence.ownership.assignment.available=15;
    assert(arrival_resume_owner(&story_presence.ownership,&test_scene));
    assert(!SudekiMpLanStoryRuntimeCharacterAway(2));
    menu_lobby_known=menu_scene_known=TRUE;menu_scene=test_scene;
    menu_lobby.members[0].present=menu_lobby.members[0].reserved=TRUE;
    menu_lobby.members[0].character=3;strcpy(menu_lobby.members[0].name,"Host");
    menu_lobby.members[1].present=menu_lobby.members[1].reserved=TRUE;
    menu_lobby.members[1].character=2;strcpy(menu_lobby.members[1].name,"Guest");
    char label[32];assert(SudekiMpLanStoryRuntimePlayerName(2,label) && !strcmp(label,"Guest"));
    assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_UNAVAILABLE);
    capture.last_tick=GetTickCount();run();
    assert(arrival_acquires==1 && host_control[1].acquired && !host_control[1].ready &&
        arrival_state.phase==SUDEKIMP_STORY_CONTROL_PREPARE);
    arrival_ack=TRUE;run();assert(host_control[1].ready && arrival_state.phase==SUDEKIMP_STORY_CONTROL_READY);
    /* Disconnect cannot bypass a positively unfinished native drain. */
    test_peer.phase=SUDEKIMP_LAN_PARTY_FREE;
    assert(SudekiMpPartyOwnershipDisconnect(&story_presence.ownership,1));
    drain_ok=FALSE;run();
    assert(host_control[1].native_key.token && host_control[1].draining && !host_control[1].ready);
    drain_ok=TRUE;run();assert(!host_control[1].native_key.token);
    assert(SudekiMpLanStoryRuntimePlayerName(2,label) && !strcmp(label,"Host"));
    assert(SudekiMpPartyOwnershipConnect(&story_presence.ownership,1));
    test_peer.phase=SUDEKIMP_LAN_PARTY_OBSERVING;++test_peer.lease.generation;test_peer.lease.token=8;
    arrival_ack=FALSE;capture.last_tick=GetTickCount();run();
    assert(arrival_acquires==2 && !host_control[1].ready && host_control[1].native_key.token==8);
    arrival_ack=TRUE;run();assert(host_control[1].ready);
    /* Reverse scene invalidates the old fence, then offers ordinary Tal. */
    test_scene.available_mask=test_roster.available_mask=12;
    test_scene.leader_seat=test_roster.leader_character=3;
    test_scene.epoch=test_roster.epoch=2;test_scene.revision=test_roster.revision=2;
    memset(test_scene.temporary,0,sizeof(test_scene.temporary));test_scene.inside_mask=0;run();assert(!host_control[1].native_key.token);
    capture.epoch=capture.revision=2;capture.last_tick=GetTickCount();arrival_ack=FALSE;run();
    assert(arrival_acquires==3 && host_control[1].acquired && !host_control[1].ready);
    arrival_ack=TRUE;run();assert(host_control[1].ready);
    puts("PASS named ArrivalRuntimeHandshake forward/reverse/disconnect/rejoin and active label");
}
int main(void) {
    control_attempted=story_policy_initialized=saved_profile=host_binding_ready=TRUE;session=(void *)1;
    runtime_thread=GetCurrentThreadId(); local_seat=0;
    test_scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,.available_mask=15,.leader_seat=3};
    strcpy(test_scene.world,"fixture");
    test_roster=(SudekiMpLanStoryNativeRoster){.epoch=1,.revision=1,.leader_character=3,.available_mask=15};
    test_roster.actors[2]=(void *)3;capture.epoch=capture.revision=1;capture.actors[2]=(void *)3;capture.generation[2]=1;capture.last_tick=GetTickCount();
    test_peer=(SudekiMpLanPartyPeerStatus){.phase=SUDEKIMP_LAN_PARTY_OBSERVING,.transport_confirmed=1,.lease={.token=7,.generation=1,.seat=1}};
    SudekiMpPartyAssignment a={.world=1,.revision=1,.generation={1,1,1,1},.humans=3,.available=15,.character={3,2,4,4}};
    assert(SudekiMpPartyOwnershipInitializePresence(&story_presence.ownership,&a,3,1));
    story_presence.ownership.away=2;
    assert(!have_presence && SudekiMpLanStoryRuntimeCharacterAway(2));
    local_seat=1;assert(!SudekiMpLanStoryRuntimeCharacterAway(2) && SudekiMpLanStoryRuntimeCharacterAwayState(2)==-1);
    have_presence=TRUE;assert(SudekiMpLanStoryRuntimeCharacterAway(2));local_seat=0;have_presence=FALSE;
    story_policy_initialized=FALSE;assert(!SudekiMpLanStoryRuntimeCharacterAway(2));story_policy_initialized=TRUE;
    run(); run(); assert(!begins && away_offers==1 && dead_hold && recorded_remote[0]==test_roster.actors[2]);
    held();observer_hold=TRUE;run();assert(!drains && host_control[1].native_key.token && dead_hold);
    observer_hold=FALSE;drain_ok=FALSE;run();assert(drains==1 && host_control[1].draining && strstr(last_drain,"stepped_away"));
    drain_ok=TRUE;run();assert(drains==2 && !host_control[1].native_key.token);
    run();assert(dead_hold && recorded_remote[0]==test_roster.actors[2] && story_presence.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_AI);
    held();roster_known=FALSE;split_attempted=split_hold=TRUE;run();assert(drains==2 && host_control[1].native_key.token);
    roster_known=TRUE;split_attempted=split_hold=FALSE;run();assert(drains==3 && !host_control[1].native_key.token);
    story_presence.ownership.away=0;capture.last_tick=GetTickCount();run();assert(begins==1 && dead_hold);
    story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_HUMAN;
    held();story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_HUMAN;
    observer_hold=TRUE;assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_BUSY);
    assert(away_host_admit(1,FALSE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_OK);
    observer_hold=FALSE;assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_OK);
    host_control[1].ready=FALSE;assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_BUSY);
    host_control[1].ready=TRUE;story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_WAIT_ACK;
    assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_BUSY);
    story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_HUMAN;
    SudekiMpPartyOwnership before_swap=story_presence.ownership;
    uint32_t request=before_swap.last_request[1]+1;
    assert(SudekiMpPartySwapRequest(&story_presence.ownership,1,0,request,before_swap.assignment.world,before_swap.assignment.revision,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    assert(story_presence.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_HUMAN && host_control[1].ready);
    assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_BUSY);
    assert(SudekiMpPartySwapBegin(&story_presence.ownership,1,request));
    assert(story_presence.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_HUMAN && host_control[1].ready);
    assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_SWAP_BUSY);
    story_presence.ownership=before_swap;
    test_scene.inside_mask=8;strcpy(test_scene.temporary,"house");assert(away_host_admit(1,TRUE,&test_scene,&test_roster)==SUDEKIMP_PARTY_AWAY_OTHER_AREA);
    test_arrival_runtime();
    puts("host away offer, once-only trace, held drain, AI completion and return path passed");return 0;
}

BOOL SudekiMpLanStorySplitMemberParked(unsigned c) {return c==2 && split_parked;}
BOOL SudekiMpLanStorySplitParkMember(unsigned c) {(void)c;return TRUE;}
BOOL SudekiMpLanStorySplitUnparkMember(unsigned c) {(void)c;return TRUE;}
void SudekiMpLanStoryRemoteInteriorAwayDrained(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryScene *s,unsigned c) {(void)r;(void)s;assert(c==2);++away_drains;}
int SudekiMpLanStoryRemoteInteriorAwayReturn(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryScene *s,unsigned c,uint32_t now) {(void)r;(void)s;assert(c==2);(void)now;++away_returns;return return_result;}

BOOL SudekiMpLanStoryCastSuperseded(void) {return cast_superseded;}
BOOL SudekiMpLanStoryObserverIdentityUnknown(void) {return identity_unknown;}

void SudekiMpLanStoryCastRequestStop(void) {}
BOOL SudekiMpLanStoryCastRetains(void) {return FALSE;}
BOOL SudekiMpLanStoryCastService(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {(void)w;(void)r;return TRUE;}
BOOL SudekiMpLanStoryCastUninstall(void) {return TRUE;}
void SudekiMpLanStoryDoorCameraUninstall(void) {}

void SudekiMpLanStoryRemoteInteriorCancelAwayReturn(unsigned c) {assert(c==2);++away_cancels;}
