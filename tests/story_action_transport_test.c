/* Loopback datagrams only. Taking an action represents admission; no actor,
 * game executable or native skill is loaded by this test. */
#include "network/lan_party_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SudekiMpLanPartySession *host,*client;
static SudekiMpLanPartyPeerStatus peer;
static SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=15,.leader_seat=3,.world="skill_request_fixture"};
static SudekiMpLanStoryControlState state;
static void poll(void) {
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick) {
        scene.observed_tick=now; assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
    }
    SudekiMpLanPartyPoll(host,now); SudekiMpLanPartyPoll(client,now);
}
static void control(unsigned transaction) {
    state=(SudekiMpLanStoryControlState){.fence={1,1,transaction,1,1,2},
        .phase=SUDEKIMP_STORY_CONTROL_PREPARE,.observed_tick=GetTickCount()};
    assert(SudekiMpLanPartyPublishStoryControl(host,&peer.lease,&state));
    BOOL ack=FALSE; SudekiMpLanStoryControlState remote;
    for(unsigned i=0;i<80 && !ack;++i) {
        poll();
        if(SudekiMpLanPartyGetStoryControl(client,&peer.lease,GetTickCount(),&remote) &&
            remote.fence.transaction==transaction)
            assert(SudekiMpLanPartyAcknowledgeStoryControl(client,&peer.lease,&state.fence));
        ack=SudekiMpLanPartyStoryControlAcknowledged(host,&peer.lease,&state.fence);
        if(!ack) Sleep(1);
    }
    assert(ack); state.phase=SUDEKIMP_STORY_CONTROL_READY; state.observed_tick=GetTickCount();
    assert(SudekiMpLanPartyPublishStoryControl(host,&peer.lease,&state));
    BOOL ready=FALSE;
    for(unsigned i=0;i<80 && !ready;++i) {
        poll(); ready=SudekiMpLanPartyGetStoryControl(client,&peer.lease,GetTickCount(),&remote) &&
            remote.fence.transaction==transaction && remote.phase==SUDEKIMP_STORY_CONTROL_READY;
        if(!ready) Sleep(1);
    }
    assert(ready);
}
static void receive_request(const SudekiMpLanStoryActionRequest *r,BOOL consume) {
    assert(SudekiMpLanPartySendStoryAction(client,&peer.lease,r));
    for(unsigned i=0;i<4;++i) { poll(); Sleep(1); }
    if(consume) {
        SudekiMpLanStoryActionRequest got;
        assert(SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&got));
        assert(SudekiMpLanStoryActionRequestSame(r,&got));
        assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&got));
    }
}
static void receive_result(const SudekiMpLanStoryActionRequest *r,unsigned outcome) {
    SudekiMpLanStoryActionResult got; BOOL received=FALSE;
    for(unsigned i=0;i<80 && !received;++i) {
        poll(); received=SudekiMpLanPartyGetStoryActionResult(client,&peer.lease,&got);
        if(!received) Sleep(1);
    }
    assert(received && SudekiMpLanStoryActionResultMatches(&got,r) && got.outcome==outcome);
}
int main(void) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000,
        .assignment_enabled=1,.character={3,2,0,1}};
    memset(config.game_hash,0x25,sizeof(config.game_hash));
    host=SudekiMpLanPartyCreate(&config); assert(host);
    config.local_seat=1; config.host_ipv4="127.0.0.1"; config.port=SudekiMpLanPartyPort(host);
    client=SudekiMpLanPartyCreate(&config); assert(client);
    BOOL connected=FALSE;
    for(unsigned i=0;i<300 && !connected;++i) {
        SudekiMpLanPartyPeerStatus server; SudekiMpLanStoryScene remote;
        poll(); assert(SudekiMpLanPartyPeerStatusGet(client,1,&peer));
        assert(SudekiMpLanPartyPeerStatusGet(host,1,&server));
        connected=peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && server.transport_confirmed &&
            SudekiMpLanPartyGetStoryScene(client,&peer.lease,GetTickCount(),&remote);
        if(!connected) Sleep(5);
    }
    assert(connected);
    SudekiMpLanStoryFrame frame={.epoch=1,.revision=1,.sequence=1,.host_tick=GetTickCount(),
        .available_mask=15,.leader_character=3};
    for(unsigned c=0;c<4;++c) frame.actors[c]=(SudekiMpLanStoryActor){.generation=1,
        .character=(uint8_t)c,.native_pose=1,.facing_z=1,.hp=100,.sp=100};
    assert(SudekiMpLanPartySendStoryFrame(host,&frame));
    SudekiMpLanStoryFrame received; BOOL got=FALSE;
    for(unsigned i=0;i<80 && !got;++i) {
        poll(); got=SudekiMpLanPartyPopStoryFrame(client,&peer.lease,GetTickCount(),&received);
        if(!got) Sleep(1);
    }
    assert(got); control(1);
    SudekiMpLanStoryActionRequest r={state.fence,1,1,SUDEKIMP_STORY_ACTION_SKILL,3},taken;
    SudekiMpLanStoryActionRequest invalid=r; invalid.fence.character=3;
    assert(!SudekiMpLanPartySendStoryAction(client,&peer.lease,&invalid));
    invalid=r; invalid.acknowledged_frame=2;
    assert(!SudekiMpLanPartySendStoryAction(client,&peer.lease,&invalid));
    assert(!SudekiMpLanPartySendStoryAction(host,&peer.lease,&r));
    receive_request(&r,TRUE);
    invalid=r; invalid.slot=4; assert(!SudekiMpLanPartySendStoryAction(client,&peer.lease,&invalid));
    invalid=r; ++invalid.request; assert(!SudekiMpLanPartySendStoryAction(client,&peer.lease,&invalid));
    receive_request(&r,FALSE);
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    SudekiMpLanStoryActionResult result={r.fence,r.request,GetTickCount(),r.kind,r.slot,SUDEKIMP_STORY_ACTION_STARTED};
    assert(SudekiMpLanPartyPublishStoryActionResult(host,&peer.lease,&result));
    receive_result(&r,SUDEKIMP_STORY_ACTION_STARTED);
    result.outcome=SUDEKIMP_STORY_ACTION_BUSY;
    assert(!SudekiMpLanPartyPublishStoryActionResult(host,&peer.lease,&result));
    receive_request(&r,FALSE); receive_result(&r,SUDEKIMP_STORY_ACTION_STARTED);
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    for(unsigned attack=1;attack<=3;++attack) {
        ++r.request; r.kind=SUDEKIMP_STORY_ACTION_MELEE; r.slot=(uint8_t)attack;
        uint8_t bytes[SUDEKIMP_STORY_ACTION_REQUEST_WIRE_SIZE];
        assert(SudekiMpLanStoryActionRequestEncode(&r,bytes,sizeof(bytes)));
        invalid=r; invalid.fence.character=3; assert(!SudekiMpLanStoryActionRequestValid(&invalid));
        invalid=r; invalid.slot=0; assert(!SudekiMpLanStoryActionRequestValid(&invalid));
        invalid.slot=4; assert(!SudekiMpLanStoryActionRequestValid(&invalid));
        bytes[30]=1; assert(!SudekiMpLanStoryActionRequestDecode(bytes,sizeof(bytes),&invalid));
        receive_request(&r,TRUE);
        result=(SudekiMpLanStoryActionResult){r.fence,r.request,GetTickCount(),r.kind,r.slot,
            SUDEKIMP_STORY_ACTION_STARTED};
        assert(!SudekiMpLanStoryActionResultValid(&result)); /* submission is not acceptance */
        result.outcome=SUDEKIMP_STORY_ACTION_NO_SP; assert(!SudekiMpLanStoryActionResultValid(&result));
        result.outcome=SUDEKIMP_STORY_ACTION_SUBMITTED;
        assert(SudekiMpLanPartyPublishStoryActionResult(host,&peer.lease,&result));
        receive_result(&r,SUDEKIMP_STORY_ACTION_SUBMITTED);
        receive_request(&r,FALSE); receive_result(&r,SUDEKIMP_STORY_ACTION_SUBMITTED);
        assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    }
    r.kind=SUDEKIMP_STORY_ACTION_SKILL; r.slot=3;
    ++r.request; receive_request(&r,FALSE);
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,
        GetTickCount()+SUDEKIMP_STORY_CONTROL_MAX_AGE_MS+1,&taken));
    receive_result(&r,SUDEKIMP_STORY_ACTION_EXPIRED);
    receive_request(&r,FALSE);
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    ++r.request;
    assert(SudekiMpLanPartySendStoryAction(client,&peer.lease,&r));
    Sleep(SUDEKIMP_STORY_CONTROL_MAX_AGE_MS+20); /* first datagram arrives too late */
    poll(); receive_result(&r,SUDEKIMP_STORY_ACTION_EXPIRED);
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    control(2); assert(!SudekiMpLanPartySendStoryAction(client,&peer.lease,&r));
    frame.sequence=11; frame.host_tick=GetTickCount();
    assert(SudekiMpLanPartySendStoryFrame(host,&frame));
    got=FALSE;
    for(unsigned i=0;i<80 && !got;++i) {
        poll(); got=SudekiMpLanPartyPopStoryFrame(client,&peer.lease,GetTickCount(),&received);
        if(!got) Sleep(1);
    }
    assert(got && received.sequence==11);
    r.fence=state.fence; ++r.request;
    assert(SudekiMpLanPartySendStoryAction(client,&peer.lease,&r)); /* local, never-sent expiry */
    receive_result(&r,SUDEKIMP_STORY_ACTION_EXPIRED);
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    r.acknowledged_frame=11; ++r.request; receive_request(&r,FALSE);
    assert(SudekiMpLanPartyRevokeStoryControl(host,&peer.lease));
    assert(!SudekiMpLanPartyTakeStoryAction(host,&peer.lease,GetTickCount(),&taken));
    SudekiMpLanPartyLease previous=peer.lease;
    assert(SudekiMpLanPartyDisconnect(host,&peer.lease));
    assert(!SudekiMpLanPartyTakeStoryAction(host,&previous,GetTickCount(),&taken));
    SudekiMpLanPartyDestroy(client,FALSE); SudekiMpLanPartyDestroy(host,FALSE);
    puts("story skill/melee UDP request/duplicate/result/expiry/fence tests passed (no native actions)");
    return 0;
}
