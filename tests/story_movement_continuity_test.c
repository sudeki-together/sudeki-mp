/* Real loopback transport, no game objects. This proves bounded movement
 * acknowledgement continuity, not native ownership or visual continuity. */
#include "network/lan_party_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SudekiMpLanPartySession *host,*client;
static SudekiMpLanPartyPeerStatus peer;
static SudekiMpLanStoryScene scene;
static SudekiMpLanStoryControlState state;
static uint32_t frame_sequence,input_sequence;
static BOOL refresh_control;

static void poll(void) {
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick) {
        scene.observed_tick=now;assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
    }
    if(refresh_control && now!=state.observed_tick &&
        SudekiMpLanStoryControlMatchesScene(&state.fence,&scene)) {
        state.observed_tick=now;
        assert(SudekiMpLanPartyPublishStoryControl(host,&peer.lease,&state));
    }
    SudekiMpLanPartyPoll(host,now);SudekiMpLanPartyPoll(client,now);
}
static void connect_pair(void) {
    BOOL connected=FALSE;
    for(unsigned i=0;i<400 && !connected;++i) {
        SudekiMpLanPartyPeerStatus server;SudekiMpLanStoryScene got;
        poll();assert(SudekiMpLanPartyPeerStatusGet(client,1,&peer));
        assert(SudekiMpLanPartyPeerStatusGet(host,1,&server));
        connected=peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed &&
            server.transport_confirmed &&
            SudekiMpLanPartyGetStoryScene(client,&peer.lease,GetTickCount(),&got);
        if(!connected)Sleep(2);
    }
    assert(connected);
}
static void control(unsigned transaction,BOOL ready) {
    refresh_control=FALSE;
    state=(SudekiMpLanStoryControlState){.fence={scene.epoch,scene.revision,transaction,1,1,2},
        .phase=SUDEKIMP_STORY_CONTROL_PREPARE,.observed_tick=GetTickCount()};
    assert(SudekiMpLanPartyPublishStoryControl(host,&peer.lease,&state));
    BOOL acknowledged=FALSE;SudekiMpLanStoryControlState got;
    for(unsigned i=0;i<120 && !acknowledged;++i) {
        poll();
        if(SudekiMpLanPartyGetStoryControl(client,&peer.lease,GetTickCount(),&got) &&
            got.fence.transaction==transaction)
            assert(SudekiMpLanPartyAcknowledgeStoryControl(client,&peer.lease,&state.fence));
        acknowledged=SudekiMpLanPartyStoryControlAcknowledged(host,&peer.lease,&state.fence);
        if(!acknowledged)Sleep(1);
    }
    assert(acknowledged);
    if(ready) {
        state.phase=SUDEKIMP_STORY_CONTROL_READY;state.observed_tick=GetTickCount();
        assert(SudekiMpLanPartyPublishStoryControl(host,&peer.lease,&state));
        BOOL received=FALSE;
        for(unsigned i=0;i<120 && !received;++i) {
            poll();received=SudekiMpLanPartyGetStoryControl(client,&peer.lease,GetTickCount(),&got) &&
                got.fence.transaction==transaction && got.phase==SUDEKIMP_STORY_CONTROL_READY;
            if(!received)Sleep(1);
        }
        assert(received);refresh_control=TRUE;
    }
}
static void start(BOOL ready) {
    scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=15,.leader_seat=3,.world="movement_fixture"};
    memset(&state,0,sizeof(state));refresh_control=FALSE;frame_sequence=input_sequence=0;
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000,
        .assignment_enabled=1,.character={3,2,0,1}};
    memset(config.game_hash,0x36,sizeof(config.game_hash));
    host=SudekiMpLanPartyCreate(&config);assert(host);
    config.local_seat=1;config.host_ipv4="127.0.0.1";config.port=SudekiMpLanPartyPort(host);
    client=SudekiMpLanPartyCreate(&config);assert(client);connect_pair();control(1,ready);
}
static void stop(void) {
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
    client=host=NULL;
}
static uint32_t frames(unsigned generation,BOOL consume) {
    Sleep(1);poll();
    SudekiMpLanStoryFrame frame={.epoch=scene.epoch,.revision=scene.revision,
        .sequence=++frame_sequence,.host_tick=GetTickCount(),.available_mask=scene.available_mask,
        .leader_character=scene.leader_seat};
    for(unsigned c=0;c<4u;++c)if(scene.available_mask&(1u<<c))
        frame.actors[c]=(SudekiMpLanStoryActor){.generation=c==2u?generation:1u,
            .character=(uint8_t)c,.native_pose=1,.facing_z=1,.hp=100,.sp=100};
    SudekiMpLanStoryWorldFrame world={.epoch=frame.epoch,.revision=frame.revision,
        .host_tick=frame.host_tick,.sequence=frame.sequence};
    assert(SudekiMpLanPartySendStoryFrame(host,&frame));
    assert(SudekiMpLanPartySendStoryWorld(host,&world));
    if(!consume) {for(unsigned i=0;i<4u;++i){poll();Sleep(1);}return 0;}
    BOOL got_frame=FALSE,got_world=FALSE;uint32_t receipt=0;
    for(unsigned i=0;i<120 && (!got_frame || !got_world);++i) {
        SudekiMpLanStoryFrame f;SudekiMpLanStoryWorldFrame w;
        poll();
        if(!got_frame && SudekiMpLanPartyPopStoryFrameReceived(client,&peer.lease,GetTickCount(),&f,&receipt)) {
            assert(f.sequence==frame.sequence && f.revision==scene.revision);got_frame=TRUE;
        }
        if(!got_world && SudekiMpLanPartyPopStoryWorld(client,&peer.lease,GetTickCount(),&w,NULL)) {
            assert(w.sequence==world.sequence && w.revision==scene.revision);got_world=TRUE;
        }
        if(!got_frame || !got_world)Sleep(1);
    }
    assert(got_frame && got_world && receipt);return receipt;
}
static void change_scene(unsigned inside) {
    ++scene.revision;scene.inside_mask=(uint8_t)inside;
    memset(scene.temporary,0,sizeof(scene.temporary));
    if(inside)strcpy(scene.temporary,"shop");
    scene.observed_tick=GetTickCount();
    assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
    BOOL received=FALSE;
    for(unsigned i=0;i<120 && !received;++i) {
        SudekiMpLanStoryScene got;poll();
        received=SudekiMpLanPartyGetStoryScene(client,&peer.lease,GetTickCount(),&got) &&
            got.revision==scene.revision;
        if(!received)Sleep(1);
    }
    assert(received);
}
static SudekiMpLanStoryMovement movement(void) {
    return (SudekiMpLanStoryMovement){state.fence,++input_sequence,frame_sequence,0.5f,1.5f};
}
static void moving(void) {
    SudekiMpLanStoryMovement input=movement(),got;
    assert(SudekiMpLanPartySendStoryMovement(client,&peer.lease,&input));
    assert(!SudekiMpLanPartySendStoryMovement(client,&peer.lease,&input));
    BOOL received=FALSE;uint32_t receipt=0;
    for(unsigned i=0;i<120 && !received;++i) {
        poll();received=SudekiMpLanPartyTakeStoryMovement(host,&peer.lease,GetTickCount(),&got,&receipt);
        if(!received)Sleep(1);
    }
    assert(received && receipt && got.sequence==input.sequence &&
        got.world_x==0.5f && got.world_z==1.5f && got.acknowledged_frame==frame_sequence);
}
static void refused(void) {
    SudekiMpLanStoryMovement input=movement();
    assert(!SudekiMpLanPartySendStoryMovement(client,&peer.lease,&input));
}
static void media_and_combat_closed(void) {
    SudekiMpLanStoryFrame f;SudekiMpLanStoryWorldFrame w;
    assert(!SudekiMpLanPartyPopStoryFrame(client,&peer.lease,GetTickCount(),&f));
    assert(!SudekiMpLanPartyPopStoryWorld(client,&peer.lease,GetTickCount(),&w,NULL));
    SudekiMpLanStoryBlock block={state.fence,1,frame_sequence,1};
    SudekiMpLanStoryMelee melee={.fence=state.fence,.sequence=1,.acknowledged_frame=frame_sequence,
        .count=1,.samples={{.serial=1,.acknowledged_frame=frame_sequence,.buttons=1}}};
    assert(SudekiMpLanStoryBlockValid(&block) && SudekiMpLanStoryMeleeValid(&melee));
    assert(!SudekiMpLanPartySendStoryBlock(client,&peer.lease,&block));
    assert(!SudekiMpLanPartySendStoryMelee(client,&peer.lease,&melee));
    for(unsigned kind=SUDEKIMP_STORY_ACTION_SKILL;kind<=SUDEKIMP_STORY_ACTION_MELEE;++kind) {
        SudekiMpLanStoryActionRequest request={state.fence,1,frame_sequence,(uint8_t)kind,1,0};
        assert(SudekiMpLanStoryActionRequestValid(&request));
        assert(!SudekiMpLanPartySendStoryAction(client,&peer.lease,&request));
    }
}
static void wait_from(uint32_t receipt,unsigned milliseconds) {
    while((uint32_t)(GetTickCount()-receipt)<milliseconds){poll();Sleep(1);}
    poll();
}
static void same_area(void) {
    start(TRUE);frames(1,TRUE);frames(1,FALSE); /* Leave old media queued. */
    change_scene(8);media_and_combat_closed();moving();
    SudekiMpLanStoryMovement invalid=movement();++invalid.acknowledged_frame;
    assert(!SudekiMpLanPartySendStoryMovement(client,&peer.lease,&invalid));
    invalid=movement();SudekiMpLanPartyLease stale=peer.lease;++stale.generation;
    assert(!SudekiMpLanPartySendStoryMovement(client,&stale,&invalid));
    change_scene(0);media_and_combat_closed();moving();
    frames(1,TRUE);moving(); /* Fresh current media replaces the fallback. */
    SudekiMpLanStoryBlock block={state.fence,1,frame_sequence,1};
    assert(SudekiMpLanPartySendStoryBlock(client,&peer.lease,&block));
    stop();
}
static void original_receipt_expires(void) {
    start(TRUE);uint32_t receipt=frames(1,TRUE);change_scene(8);moving();
    wait_from(receipt,160);change_scene(0);moving();
    wait_from(receipt,270);refused(); /* Revision and READY heartbeats cannot renew it. */
    change_scene(8);refused(); /* Expired state cannot resurrect on another revision. */
    frames(1,TRUE);change_scene(0);moving(); /* Only a fresh frame creates a new horizon. */
    stop();
}
static void owner_and_generation(void) {
    start(TRUE);frames(1,TRUE);change_scene(12);refused();
    change_scene(0);refused();stop(); /* Own-area round trip cannot revive old frames. */
    start(FALSE);frames(1,TRUE);change_scene(8);refused();
    control(2,TRUE);refused();stop(); /* PREPARE and a new fence need fresh media. */
    start(TRUE);frames(2,TRUE);change_scene(8);refused();stop();
    start(TRUE);frames(1,TRUE);change_scene(8);moving();
    control(2,TRUE);refused();stop();
    for(unsigned kind=0;kind<4u;++kind) {
        start(TRUE);frames(1,TRUE);refresh_control=FALSE;
        if(kind==0)++scene.epoch;
        if(kind==1){++scene.epoch;strcpy(scene.world,"different_world");}
        if(kind==2)scene.available_mask=12;
        if(kind==3)scene.leader_seat=0;
        change_scene(8);refused();stop();
    }
}
static void new_connection(void) {
    start(TRUE);frames(1,TRUE);change_scene(8);moving();
    SudekiMpLanPartyLease old=peer.lease;
    refresh_control=FALSE;assert(SudekiMpLanPartyDisconnect(client,&old));refused();
    BOOL drained=FALSE;
    for(unsigned i=0;i<120 && !drained;++i) {
        SudekiMpLanPartyPeerStatus server;poll();
        assert(SudekiMpLanPartyPeerStatusGet(host,1,&server));
        drained=server.phase==SUDEKIMP_LAN_PARTY_DRAINING;if(!drained)Sleep(1);
    }
    assert(drained && SudekiMpLanPartyReleaseDrained(host,&old));
    assert(SudekiMpLanPartyClientRejoin(client));connect_pair();
    assert(peer.lease.token!=old.token || peer.lease.generation!=old.generation);
    control(1,TRUE);refused(); /* Fresh metadata alone cannot restore the old reference. */
    SudekiMpLanStoryMovement input=movement();
    assert(!SudekiMpLanPartySendStoryMovement(client,&old,&input));stop();
}
int main(void) {
    same_area();original_receipt_expires();owner_and_generation();new_connection();
    puts("PASS movement-only same-area revision continuity; original receipt expiry; media/combat, ownership, actor, fence and reconnect isolation");
    return 0;
}
