/* Actual host coordinator with synthetic roster/transport/native spies. */
#include "../src/hooks/lan_story_runtime.c"
#include "../src/network/lan_party_story.c"
#define get32 handoff_get32
#define put32 handoff_put32
#include "../src/network/lan_story_handoff.c"
#undef get32
#undef put32
#include "../src/engine/party_ownership.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static SudekiMpLanStoryNativeRoster test_roster;
static SudekiMpLanStoryScene test_scene;
static SudekiMpLanPartyPeerStatus test_peer;
static BOOL roster_known=TRUE,observer_hold,split_hold,drain_ok=TRUE,dead_hold;
static unsigned begins,drains,away_offers;
static char last_drain[128];
void SudekiMpLogWrite(const char *s) { (void)s; }
void SudekiMpLogFormat(const char *format,...) {
    char text[1024]; va_list args; va_start(args,format); vsnprintf(text,sizeof(text),format,args); va_end(args);
    if(strstr(text,"offer_stepped_away")) ++away_offers;
    if(strstr(text,"host_control_drain")) snprintf(last_drain,sizeof(last_drain),"%s",strstr(text,"reason="));
}
BOOL SudekiMpLanStoryObserverRoster(void *controller,const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *s,SudekiMpLanStoryNativeRoster *r) {
    (void)controller;(void)w;(void)s;*r=test_roster;return roster_known;
}
void SudekiMpLanStoryControlObserveIdleContact(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {(void)w;(void)r;}
BOOL SudekiMpLanStoryObserverHolding(void) {return observer_hold;}
BOOL SudekiMpLanStorySplitActive(void) {return split_hold;}
void SudekiMpLanStoryDeathHoldSetActive(BOOL active) {dead_hold=active;}
void SudekiMpLanStoryDeathHoldSetRemoteActors(void *const actors[3]) {(void)actors;}
void SudekiMpLanStoryDeathHoldReport(void) {}
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned p,SudekiMpLanPartyPeerStatus *out) {
    (void)s; if(p!=1) {memset(out,0,sizeof(*out));return TRUE;} *out=test_peer;return TRUE;
}
unsigned SudekiMpLanPartyPlayerCharacter(SudekiMpLanPartySession *s,unsigned p) {(void)s;return p==1?2:4;}
BOOL SudekiMpLanPartyStoryCatchupComplete(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l) {(void)s;(void)l;return TRUE;}
BOOL SudekiMpLanStoryControlBegin(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {(void)w;(void)r;++begins;return FALSE;}
BOOL SudekiMpLanStoryControlDrain(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *l) {(void)w;(void)r;(void)l;++drains;return drain_ok;}
BOOL SudekiMpLanPartyRevokeStoryControl(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l) {(void)s;(void)l;return TRUE;}
void SudekiMpLanStoryRemoteUseService(void) {}
/* Unused action channels and native services remain closed in this fixture. */
BOOL SudekiMpLanPartyPublishStoryActionResult(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryActionResult *result) { (void)session;(void)lease;(void)result; return FALSE; }
BOOL SudekiMpLanPartyPublishStoryControl(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlState *state) { (void)session;(void)lease;(void)state; return FALSE; }
BOOL SudekiMpLanPartyStoryControlAcknowledged(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence) { (void)session;(void)lease;(void)fence; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryAction(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryActionRequest *request) { (void)session;(void)lease;(void)now;(void)request; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryBlock(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryBlock *input,uint32_t *received_at) { (void)session;(void)lease;(void)now;(void)input;(void)received_at; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryMelee(SudekiMpLanPartySession *, const SudekiMpLanPartyLease *,uint32_t now,BOOL admitted,uint8_t *buttons,uint32_t *serial) { (void)now;(void)admitted;(void)buttons;(void)serial; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryMovement(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryMovement *input, uint32_t *received_at) { (void)session;(void)lease;(void)now;(void)input;(void)received_at; return FALSE; }
BOOL SudekiMpLanPartyTakeStoryRanged(SudekiMpLanPartySession *session, const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryRanged *input,uint32_t *received_at) { (void)session;(void)lease;(void)now;(void)input;(void)received_at; return FALSE; }
BOOL SudekiMpLanStoryCastInstall(HMODULE image) { (void)image; return FALSE; }
unsigned SudekiMpLanStoryCastSubmit(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned slot) { (void)w;(void)roster;(void)key;(void)slot; return 0; }
unsigned SudekiMpLanStoryCastSubmitSpirit(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned variant) { (void)w;(void)roster;(void)key;(void)variant; return 0; }
BOOL SudekiMpLanStoryCastTryBind(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster) { (void)w;(void)roster; return FALSE; }
BOOL SudekiMpLanStoryControlBlock(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,BOOL held) { (void)held; return FALSE; }
unsigned SudekiMpLanStoryControlEquipArmour(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) { (void)w;(void)roster;(void)key;(void)item; return 0; }
unsigned SudekiMpLanStoryControlEquipWeapon(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) { (void)w;(void)roster;(void)key;(void)item; return 0; }
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key) { (void)native_key; return FALSE; }
BOOL SudekiMpLanStoryControlLoadDrain(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanPartyLease *) {  return FALSE; }
unsigned SudekiMpLanStoryControlMeleeSnapshot(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,unsigned buttons) { (void)buttons; return 0; }
BOOL SudekiMpLanStoryControlMove(const SudekiMpControlUpdateDispatchWitness *, const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key, float world_x,float world_z,BOOL *temporarily_held) { (void)native_key;(void)world_x;(void)world_z;(void)temporarily_held; return FALSE; }
unsigned SudekiMpLanStoryControlRanged(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned state) { (void)w;(void)roster;(void)key;(void)state; return 0; }
unsigned SudekiMpLanStoryControlUseItem(const SudekiMpControlUpdateDispatchWitness *w, const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) { (void)w;(void)roster;(void)key;(void)item; return 0; }
BOOL SudekiMpLanStoryDoorCameraInstall(HMODULE image) { (void)image; return FALSE; }
BOOL SudekiMpLanStoryHostControlBound(void *controller, const SudekiMpControlUpdateDispatchWitness *witness, const SudekiMpLanStoryScene *scene) { (void)controller;(void)witness;(void)scene; return FALSE; }
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
void SudekiMpLanStorySplitSetEligibility(BOOL host_ready,uint8_t outside_mask) { (void)host_ready;(void)outside_mask;  }
BOOL SudekiMpLogResearchEnabled(void) {  return FALSE; }
static void run(void) {service_story_controls((void *)1,(void *)2,&test_scene);}
static void held(void) {
    host_control[1].connection=test_peer.lease;host_control[1].native_key=test_peer.lease;
    host_control[1].native_key.seat=2;host_control[1].ready=host_control[1].acquired=TRUE;
    host_control[1].fence=(SudekiMpLanStoryControlFence){1,1,1,1,1,2};
    story_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_DRAINING;
    story_presence.ownership.controlling=3;
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
    run(); run(); assert(!begins && away_offers==1 && !dead_hold);
    held();observer_hold=TRUE;run();assert(!drains && host_control[1].native_key.token && dead_hold);
    observer_hold=FALSE;drain_ok=FALSE;run();assert(drains==1 && host_control[1].draining && strstr(last_drain,"stepped_away"));
    drain_ok=TRUE;run();assert(drains==2 && !host_control[1].native_key.token);
    run();assert(!dead_hold && story_presence.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_AI);
    held();roster_known=FALSE;split_attempted=split_hold=TRUE;run();assert(drains==2 && host_control[1].native_key.token);
    roster_known=TRUE;split_attempted=split_hold=FALSE;run();assert(drains==3 && !host_control[1].native_key.token);
    story_presence.ownership.away=0;capture.last_tick=GetTickCount();run();assert(begins==1 && dead_hold);
    puts("host away offer, once-only trace, held drain, AI completion and return path passed");return 0;
}
