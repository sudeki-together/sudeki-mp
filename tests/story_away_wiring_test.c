/* Actual runtime ownership and control services with recording transport/native seams. */
#define main prior_host_cases
#include "story_step_away_host_test.c"
#undef main
static BOOL audit_enabled;
static DWORD audit_window;
static unsigned audit_reads;
BOOL SudekiMpCleanroomEngineTPtrAuditEnabled(void) {return audit_enabled;}
DWORD SudekiMpLanStoryObserverBackgroundSwitchTick(void) {return audit_window;}
BOOL SudekiMpLanStoryObserverSwitchMember(unsigned c,SudekiMpLanStorySwitchMember *out) {(void)c;memset(out,0,sizeof(*out));++audit_reads;return TRUE;}
static SudekiMpLanPartyCommand queued;
static BOOL command_pending;
BOOL SudekiMpLanPartyTakeCommand(SudekiMpLanPartySession *s,SudekiMpLanPartyCommand *out) {
    (void)s;if(!command_pending)return FALSE;*out=queued;command_pending=FALSE;return TRUE;
}
BOOL SudekiMpLanPartyPublishPresence(SudekiMpLanPartySession *s,const SudekiMpLanPartyPresence *p) {(void)s;(void)p;return TRUE;}
BOOL SudekiMpLanStoryHostControlCanSwap(void *c,const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *s) {(void)c;(void)w;(void)s;return FALSE;}
BOOL SudekiMpLanStoryHostControlSelect(void *c,const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *s,unsigned target) {(void)c;(void)w;(void)s;(void)target;return FALSE;}
BOOL SudekiMpLanStoryControlRetains(void) {return FALSE;}
BOOL SudekiMpLanStoryActivityRetains(void) {return activity_retained;}
BOOL SudekiMpLanStoryActivityService(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,unsigned mask) {(void)w;(void)r;++activity_calls;activity_mask=mask;return activity_ok;}
BOOL SudekiMpLanStoryActivityReleaseForLoad(const SudekiMpControlUpdateDispatchWitness *w) {(void)w;return activity_ok;}
uint8_t SudekiMpLanPartyActorType(unsigned c) {return (uint8_t)c;}
BOOL SudekiMpLanStoryWorldCollectNearby(void *actor,uint8_t type) {(void)actor;(void)type;assert(0);return 0;}
static void setup(void) {
    memset(host_control,0,sizeof(host_control));memset(&story_presence,0,sizeof(story_presence));memset(&story_published,0,sizeof(story_published));
    control_attempted=story_policy_initialized=saved_profile=host_binding_ready=story_step_away=TRUE;
    activity_attempted=split_attempted=observer_hold=split_hold=fixture_control_exact=split_parked=activity_retained=FALSE;
    roster_known=drain_ok=activity_ok=TRUE;return_result=1;session=(void *)1;local_seat=0;runtime_thread=GetCurrentThreadId();
    begins=drains=away_drains=away_returns=away_cancels=away_offers=parked_offers=request_logs=away_request_traces=0;
    eligibility_calls=eligibility_mask=eligibility_away=activity_calls=activity_mask=0;command_pending=FALSE;
    test_scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,.available_mask=15,.leader_seat=3};strcpy(test_scene.world,"fixture");
    test_roster=(SudekiMpLanStoryNativeRoster){.epoch=1,.revision=1,.leader_character=3,.available_mask=15};test_roster.actors[2]=(void *)3;
    capture.epoch=capture.revision=1;capture.actors[2]=(void *)3;capture.generation[2]=1;capture.last_tick=GetTickCount();
    test_peer=(SudekiMpLanPartyPeerStatus){.phase=SUDEKIMP_LAN_PARTY_OBSERVING,.transport_confirmed=1,.lease={.token=7,.generation=1,.seat=1}};
    SudekiMpPartyAssignment a={.world=1,.revision=1,.generation={1,1,1,1},.humans=3,.available=15,.character={3,2,4,4}};
    assert(SudekiMpPartyOwnershipInitializePresence(&story_presence.ownership,&a,3,1));
}
static void ownership(void) {service_story_ownership((void *)1,(void *)2,&test_scene);}
static void request(BOOL away,uint32_t id) {
    queued=(SudekiMpLanPartyCommand){.lease=test_peer.lease,.request=id,.world=story_presence.ownership.assignment.world,
        .revision=story_presence.ownership.assignment.revision,.generation=1,.player=1,.kind=SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,
        .value=away?SUDEKIMP_LAN_PARTY_PRESENCE_AWAY:0};command_pending=TRUE;ownership();assert(!command_pending);
}
int main(void) {
    setup();split_parked=TRUE;run();assert(!begins && parked_offers==1);
    /* Disconnect clears reservation intent, but never the split owner's park record. */
    story_presence.ownership.away=2;test_peer.phase=SUDEKIMP_LAN_PARTY_FREE;ownership();
    assert(!(story_presence.ownership.away&2) && split_parked);
    test_peer.phase=SUDEKIMP_LAN_PARTY_OBSERVING;ownership();capture.last_tick=GetTickCount();run();assert(!begins && split_parked);
    split_parked=FALSE;run();assert(begins==1);
    setup();story_presence.ownership.away=2;ownership();
    for(int result=-1;result<=1;++result) {
        return_result=result;SudekiMpPartyOwnership before=story_presence.ownership;request(FALSE,(uint32_t)(result+2));
        assert(away_returns==(unsigned)(result+2));
        assert(story_presence.ack_result[1]==(result<0?SUDEKIMP_PARTY_SWAP_UNAVAILABLE:result==0?SUDEKIMP_PARTY_SWAP_BUSY:SUDEKIMP_PARTY_SWAP_OK));
        if(result<=0)assert(!memcmp(&before,&story_presence.ownership,sizeof(before)));
        else assert(!(story_presence.ownership.away&2) && story_presence.ownership.last_request[1]==3);
    }
    setup();story_presence.ownership.away=2;return_result=0;request(FALSE,1);request(TRUE,2);
    assert(away_returns==1 && away_cancels==1 && (story_presence.ownership.away&2));
    for(unsigned i=3;i<80;++i)request(TRUE,i);
    assert(request_logs==64 && away_request_traces==64);
    setup();story_presence.ownership.away=2;split_hold=TRUE;held();drain_ok=FALSE;run();assert(!away_drains && drains==1);
    drain_ok=TRUE;run();assert(away_drains==1 && !host_control[1].native_key.token);run();assert(away_drains==2);
    setup();story_presence.ownership.away=2;split_hold=TRUE;held();activity_attempted=activity_retained=TRUE;activity_ok=FALSE;
    run();assert(!drains && host_control[1].native_key.token && activity_calls);
    activity_ok=TRUE;run();assert(drains==1 && away_drains==1);
    /* A non-draining lease during observer grace skips drain but reaches post-service eligibility. */
    setup();story_presence.ownership.away=2;held();host_control[1].input_held=TRUE;activity_attempted=split_attempted=observer_hold=fixture_control_exact=TRUE;
    run();assert(!drains && host_control[1].acquired && !host_control[1].draining);
    assert(activity_calls==1 && eligibility_calls==2 && eligibility_mask==0 && eligibility_away==4);
    identity_unknown=TRUE;cast_superseded=FALSE;assert(story_cast_identity_unknown());
    identity_unknown=FALSE;cast_superseded=TRUE;assert(story_cast_identity_unknown());
    cast_superseded=FALSE;assert(!story_cast_identity_unknown());
    audit_window=1000;audit_enabled=FALSE;trace_background_switch(1000);assert(!audit_reads);
    audit_enabled=TRUE;trace_background_switch(1000);assert(audit_reads==4);
    trace_background_switch(1999);assert(audit_reads==4);trace_background_switch(2000);assert(audit_reads==8);
    trace_background_switch(21000);assert(audit_reads==8);
    puts("StoryAwayWiringTest PASS: ownership transaction, parked reconnect, drains, request bound, activity and eligibility");return 0;
}
