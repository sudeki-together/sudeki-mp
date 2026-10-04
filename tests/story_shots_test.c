#include "network/lan_story_frame.h"
#include "network/lan_party_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static SudekiMpLanStoryShot shot(unsigned sequence,unsigned tick) {
    SudekiMpLanStoryShot e={.sequence=sequence,.host_tick=tick,.item=12,.pre_charge_q8=256};
    e.origin[0]=3; e.origin[1]=-5; e.origin[2]=400; e.direction[2]=1; return e;
}
static void transport(SudekiMpLanStoryFrame frame) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000};
    memset(config.game_hash,0x13,sizeof(config.game_hash));
    SudekiMpLanPartySession *host=SudekiMpLanPartyCreate(&config); assert(host);
    config.local_seat=1; config.host_ipv4="127.0.0.1"; config.port=SudekiMpLanPartyPort(host);
    SudekiMpLanPartySession *client=SudekiMpLanPartyCreate(&config); assert(client);
    SudekiMpLanStoryScene scene={.epoch=frame.epoch,.revision=frame.revision,
        .phase=SUDEKIMP_LAN_STORY_READY,.available_mask=8,.leader_seat=3};
    strcpy(scene.world,"brightwater");
    SudekiMpLanPartyPeerStatus peer={0},server_peer={0}; SudekiMpLanStoryScene observed;
    BOOL ready=FALSE;
    for(unsigned i=0;i<300 && !ready;++i) {
        uint32_t tick=GetTickCount();
        if(!i || tick!=scene.observed_tick) {
            scene.observed_tick=tick;
            assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
        }
        SudekiMpLanPartyPoll(host,scene.observed_tick); SudekiMpLanPartyPoll(client,scene.observed_tick);
        assert(SudekiMpLanPartyPeerStatusGet(client,1,&peer));
        assert(SudekiMpLanPartyPeerStatusGet(host,1,&server_peer));
        ready=peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && server_peer.transport_confirmed &&
            SudekiMpLanPartyGetStoryScene(client,&peer.lease,GetTickCount(),&observed);
        if(!ready) Sleep(5);
    }
    assert(ready); frame.host_tick=GetTickCount();
    for(unsigned i=0;i<frame.actors[3].shots.count;++i)
        frame.actors[3].shots.events[i].host_tick=frame.host_tick-(3-i)*30;
    assert(SudekiMpLanPartySendStoryFrame(host,&frame));
    assert(!SudekiMpLanPartySendStoryFrame(client,&frame)); /* host-only publication */
    SudekiMpLanStoryFrame received; BOOL got=FALSE;
    for(unsigned i=0;i<100 && !got;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
        got=SudekiMpLanPartyPopStoryFrame(client,&peer.lease,GetTickCount(),&received);
        if(!got) Sleep(5);
    }
    assert(got && received.actors[3].shots.latest==6 && received.actors[3].shots.count==4);
    assert(received.actors[3].shots.events[0].origin[2]==400);
    assert(!SudekiMpLanPartySendStoryFrame(host,&frame)); /* stale frame */
    assert(!SudekiMpLanPartyPopStoryFrame(client,&peer.lease,GetTickCount(),&received));
    SudekiMpLanPartyLease foreign=peer.lease; ++foreign.generation;
    assert(!SudekiMpLanPartyPopStoryFrame(client,&foreign,GetTickCount(),&received));
    SudekiMpLanPartyDestroy(client,FALSE); SudekiMpLanPartyDestroy(host,FALSE);
}
int main(void) {
    SudekiMpLanStoryShots s={0};
    assert(SudekiMpLanStoryShotsValid(&s,3));
    for(unsigned i=1;i<=6;++i) {
        SudekiMpLanStoryShot e=shot(i,1000+i*30);
        assert(SudekiMpLanStoryShotsAppend(&s,3,&e));
    }
    assert(s.count==4 && s.latest==6 && s.events[0].sequence==3);
    assert(!SudekiMpLanStoryShotsValid(&s,1)); /* wrong actor's weapon */
    assert(!SudekiMpLanStoryShotsValid(&s,2)); /* melee actor cannot shoot */
    SudekiMpLanStoryShots saved=s;
    SudekiMpLanStoryShot e=shot(6,1200);
    assert(!SudekiMpLanStoryShotsAppend(&s,3,&e) && !memcmp(&s,&saved,sizeof(s)));
    e=shot(8,1200); assert(!SudekiMpLanStoryShotsAppend(&s,3,&e));
    e=shot(7,1200); e.direction[2]=0; assert(!SudekiMpLanStoryShotsAppend(&s,3,&e));
    e=shot(7,1200); e.direction[2]=NAN; assert(!SudekiMpLanStoryShotsAppend(&s,3,&e));
    e=shot(7,1100); assert(!SudekiMpLanStoryShotsAppend(&s,3,&e));
    assert(!SudekiMpLanStoryShotNext(&s,3,1180,6)); /* duplicate */
    assert(!SudekiMpLanStoryShotNext(&s,3,1080,0)); /* not yet emitted */
    assert(!SudekiMpLanStoryShotNext(&s,3,1500,0)); /* no old-shot burst */
    assert(SudekiMpLanStoryShotNext(&s,3,1150,3)->sequence==4);

    SudekiMpLanStoryFrame f={.epoch=1,.revision=2,.sequence=3,.host_tick=1200,
        .available_mask=8,.leader_character=3,.combat_mode=1};
    f.actors[3]=(SudekiMpLanStoryActor){.generation=7,.character=3,.native_pose=1,
        .hp=100,.sp=50,.facing_z=1,.shots=s};
    uint8_t bytes[SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE]; size_t n=0;
    assert(SudekiMpLanStoryFrameEncode(&f,bytes,sizeof(bytes),&n));
    assert(n==SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE+SUDEKIMP_LAN_STORY_ACTOR_WIRE_SIZE);
    SudekiMpLanStoryFrame decoded={0};
    assert(SudekiMpLanStoryFrameDecode(bytes,n,&decoded));
    assert(decoded.actors[3].shots.latest==6 && decoded.actors[3].shots.events[3].origin[2]==400);
    bytes[3]=6; assert(!SudekiMpLanStoryFrameDecode(bytes,n,&decoded)); bytes[3]=7;
    bytes[108+97]=1; assert(!SudekiMpLanStoryFrameDecode(bytes,n,&decoded)); bytes[108+97]=0;
    bytes[108+104+9]=1; assert(!SudekiMpLanStoryFrameDecode(bytes,n,&decoded)); bytes[108+104+9]=0;
    assert(!SudekiMpLanStoryFrameDecode(bytes,n-1,&decoded));
    f.host_tick=1179; assert(!SudekiMpLanStoryFrameValid(&f)); f.host_tick=1200;
    SudekiMpLanStoryFrame next=f; next.host_tick=1250; ++next.sequence;
    e=shot(7,1230); assert(SudekiMpLanStoryShotsAppend(&next.actors[3].shots,3,&e));
    assert(SudekiMpLanStoryFrameInterpolate(&f,&next,1225,&decoded));
    assert(decoded.actors[3].shots.latest==6); /* never invent future events */
    assert(SudekiMpLanStoryFrameInterpolate(&f,&next,1250,&decoded));
    assert(decoded.actors[3].shots.latest==7);
    transport(f);
    memset(&s,0,sizeof(s)); s.events[3]=shot(1,1000);
    assert(!SudekiMpLanStoryShotsValid(&s,3)); /* hidden trailing payload */
    puts("story shot journal/codec tests passed"); return 0;
}
