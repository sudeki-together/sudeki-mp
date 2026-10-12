#include "network/lan_party_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static SudekiMpLanPartySession *nodes[4];
static SudekiMpLanPartyPeerStatus peers[4];
static SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=15,.leader_seat=3,.world="melee_snapshot_fixture"};
static void poll(void) {
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick) {scene.observed_tick=now;assert(SudekiMpLanPartyPublishStoryScene(nodes[0],&scene));}
    for(unsigned p=0;p<4;++p) SudekiMpLanPartyPoll(nodes[p],now);
}
static SudekiMpLanStoryControlState control(unsigned p,unsigned transaction) {
    SudekiMpLanStoryControlState s={.fence={1,1,transaction,1,p,p==1?2:p==2?0:1},
        .phase=SUDEKIMP_STORY_CONTROL_PREPARE,.observed_tick=GetTickCount()},got;
    assert(SudekiMpLanPartyPublishStoryControl(nodes[0],&peers[p].lease,&s));
    BOOL ack=FALSE;
    for(unsigned i=0;i<120 && !ack;++i) {
        poll();if(SudekiMpLanPartyGetStoryControl(nodes[p],&peers[p].lease,GetTickCount(),&got) &&
            got.fence.transaction==transaction)
            assert(SudekiMpLanPartyAcknowledgeStoryControl(nodes[p],&peers[p].lease,&s.fence));
        ack=SudekiMpLanPartyStoryControlAcknowledged(nodes[0],&peers[p].lease,&s.fence);Sleep(1);
    }
    assert(ack);s.phase=SUDEKIMP_STORY_CONTROL_READY;s.observed_tick=GetTickCount();
    assert(SudekiMpLanPartyPublishStoryControl(nodes[0],&peers[p].lease,&s));
    BOOL ready=FALSE;
    for(unsigned i=0;i<120 && !ready;++i) {
        poll();ready=SudekiMpLanPartyGetStoryControl(nodes[p],&peers[p].lease,GetTickCount(),&got) &&
            got.fence.transaction==transaction && got.phase==SUDEKIMP_STORY_CONTROL_READY;Sleep(1);
    }
    assert(ready);return s;
}
static void loopback(void) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000,
        .assignment_enabled=1,.character={3,2,0,1}};
    memset(config.game_hash,0x25,sizeof(config.game_hash));nodes[0]=SudekiMpLanPartyCreate(&config);assert(nodes[0]);
    config.host_ipv4="127.0.0.1";config.port=SudekiMpLanPartyPort(nodes[0]);
    for(unsigned p=1;p<4;++p) {config.local_seat=p;nodes[p]=SudekiMpLanPartyCreate(&config);assert(nodes[p]);}
    unsigned mask=0;
    for(unsigned i=0;i<400 && mask!=14;++i) {
        poll();mask=0;
        for(unsigned p=1;p<4;++p) {
            SudekiMpLanPartyPeerStatus h;SudekiMpLanStoryScene remote;
            assert(SudekiMpLanPartyPeerStatusGet(nodes[p],p,&peers[p]));
            assert(SudekiMpLanPartyPeerStatusGet(nodes[0],p,&h));
            if(peers[p].phase==SUDEKIMP_LAN_PARTY_OBSERVING && h.transport_confirmed &&
                SudekiMpLanPartyGetStoryScene(nodes[p],&peers[p].lease,GetTickCount(),&remote)) mask|=1u<<p;
        }
        Sleep(2);
    }
    assert(mask==14);
    SudekiMpLanStoryFrame frame={.epoch=1,.revision=1,.sequence=1,.host_tick=GetTickCount(),
        .available_mask=15,.leader_character=3};
    for(unsigned c=0;c<4;++c)frame.actors[c]=(SudekiMpLanStoryActor){.generation=1,.character=c,
        .native_pose=1,.facing_z=1,.hp=100,.sp=100};
    assert(SudekiMpLanPartySendStoryFrame(nodes[0],&frame));mask=0;
    for(unsigned i=0;i<120 && mask!=14;++i) {
        poll();for(unsigned p=1;p<4;++p) {SudekiMpLanStoryFrame got;
            if(SudekiMpLanPartyPopStoryFrame(nodes[p],&peers[p].lease,GetTickCount(),&got))mask|=1u<<p;}
        Sleep(1);
    }
    assert(mask==14);
    for(unsigned p=1;p<=2;++p) {
        SudekiMpLanStoryControlState state=control(p,1);
        SudekiMpStoryMeleeSender sender={0};SudekiMpLanStoryMelee input,invalid;
        uint32_t now=GetTickCount(),serial=0;uint8_t buttons=0;
        assert(SudekiMpStoryMeleeBuild(&sender,&state.fence,1,now,0,TRUE,&input));
        invalid=input;invalid.fence.player=3;assert(!SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&invalid));
        invalid=input;invalid.acknowledged_frame=2;assert(!SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&invalid));
        assert(!SudekiMpLanPartySendStoryMelee(nodes[0],&peers[p].lease,&input));
        assert(SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount(),TRUE,&buttons,&serial));
        assert(buttons==0 && serial==1);
        /* Send multiple frames before taking any host result. Their complete
         * states must stay ordered and no skill/action result is involved. */
        const unsigned states[]={5,10,11,8,12,0,16,32,48,0};
        for(unsigned i=0;i<sizeof(states)/sizeof(*states);++i) {
            assert(SudekiMpStoryMeleeBuild(&sender,&state.fence,1,GetTickCount(),states[i],TRUE,&input));
            assert(SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));poll();
        }
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        for(unsigned i=0;i<sizeof(states)/sizeof(*states);++i) {
            assert(SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount(),TRUE,&buttons,&serial));
            assert(buttons==states[i] && serial==i+2);
        }
        assert(!SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));
        assert(SudekiMpStoryMeleeBuild(&sender,&state.fence,1,GetTickCount(),0,TRUE,&input));
        assert(SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount(),TRUE,&buttons,&serial) && !buttons && !serial);
        /* Pending input is retired on a host menu/focus fence, never replayed. */
        assert(SudekiMpStoryMeleeBuild(&sender,&state.fence,1,GetTickCount(),1,TRUE,&input));
        assert(SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(!SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount(),FALSE,&buttons,&serial));
        assert(SudekiMpStoryMeleeBuild(&sender,&state.fence,1,GetTickCount(),0,FALSE,&input));
        assert(SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount(),TRUE,&buttons,&serial) && !buttons && !serial);
        assert(!SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount()+251,TRUE,&buttons,&serial));
        control(p,2);assert(!SudekiMpLanPartySendStoryMelee(nodes[p],&peers[p].lease,&input));
        assert(SudekiMpLanPartyRevokeStoryControl(nodes[0],&peers[p].lease));
        assert(!SudekiMpLanPartyTakeStoryMelee(nodes[0],&peers[p].lease,GetTickCount(),TRUE,&buttons,&serial));
    }
    for(unsigned p=1;p<4;++p)SudekiMpLanPartyDestroy(nodes[p],FALSE);
    SudekiMpLanPartyDestroy(nodes[0],FALSE);
    puts("PASS Tal/Buki grouped melee UDP, ordered in-flight samples, repeats, expiry, cancellation and lease rejection");return;
}

static void policy(void) {
    SudekiMpLanStoryControlFence fence={1,1,1,1,1,2};
    SudekiMpStoryMeleeSender sender={0};SudekiMpStoryMeleeReceiver receiver={0};
    SudekiMpLanStoryMelee input,got,invalid,old;
    uint8_t wire[SUDEKIMP_STORY_MELEE_WIRE_SIZE],roundtrip[sizeof(wire)],buttons;
    uint32_t serial;
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,100,0,TRUE,&input));
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,110,5,TRUE,&input));old=input;
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,120,10,TRUE,&input));
    /* The first two packets were lost: the third contains both button edges
     * as one snapshot and recovers its predecessor without a result ACK. */
    assert(SudekiMpLanStoryMeleeEncode(&input,wire,sizeof(wire)));
    assert(SudekiMpLanStoryMeleeDecode(wire,sizeof(wire),&got));
    assert(SudekiMpLanStoryMeleeEncode(&got,roundtrip,sizeof(roundtrip)) && !memcmp(wire,roundtrip,sizeof(wire)));
    memset(&got,0xab,sizeof(got));invalid=got;
    for(unsigned n=0;n<sizeof(wire);++n)
        assert(!SudekiMpLanStoryMeleeDecode(wire,n,&got) && !memcmp(&got,&invalid,sizeof(got)));
    const unsigned reserved[]={18,19,34,35,47,59,71,72,227};
    for(unsigned i=0;i<sizeof(reserved)/sizeof(*reserved);++i) {
        wire[reserved[i]]=1;
        assert(!SudekiMpLanStoryMeleeDecode(wire,sizeof(wire),&got) && !memcmp(&got,&invalid,sizeof(got)));
        wire[reserved[i]]=0;
    }
    invalid=input;invalid.steady=1;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    invalid=input;invalid.count=17;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    invalid=input;invalid.fence.character=1;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    invalid=input;invalid.samples[1].serial=1;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    invalid=input;invalid.samples[0].age=251;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    invalid=input;invalid.samples[0].age=0;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    invalid=input;invalid.samples[0].buttons=64;assert(!SudekiMpLanStoryMeleeValid(&invalid));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,130,1));
    assert(!SudekiMpStoryMeleeReceive(&receiver,&old,131,1));
    const unsigned expected[]={0,5,10};
    for(unsigned i=0;i<3u;++i) {
        assert(SudekiMpStoryMeleeTake(&receiver,135+i,1,&buttons,&serial));
        assert(buttons==expected[i] && serial==i+1u);
    }
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,140,10,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,141,1));
    assert(SudekiMpStoryMeleeTake(&receiver,142,1,&buttons,&serial) && buttons==10 && !serial);
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,150,15,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,151,1));
    assert(SudekiMpStoryMeleeTake(&receiver,152,1,&buttons,&serial) && buttons==15);
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,160,0,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,161,1));
    assert(SudekiMpStoryMeleeTake(&receiver,162,1,&buttons,&serial) && !buttons);
    /* Menu/focus cancellation retires a pending press and any retransmission. */
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,170,1,TRUE,&input));old=input;
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,171,1));
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,172,0,FALSE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,173,1));
    assert(!SudekiMpStoryMeleeReceive(&receiver,&old,174,1));
    assert(SudekiMpStoryMeleeTake(&receiver,175,1,&buttons,&serial) && !buttons && !serial);
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,176,1,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,177,1));
    SudekiMpStoryMeleeMute(&receiver);
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,178,2,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,179,1));
    assert(!SudekiMpStoryMeleeTake(&receiver,180,1,&buttons,&serial));
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,181,0,FALSE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,182,1));
    assert(SudekiMpStoryMeleeTake(&receiver,183,1,&buttons,&serial) && !buttons && !serial);
    assert(!SudekiMpStoryMeleeTake(&receiver,434,1,&buttons,&serial));
    /* Repeats cannot refresh the age of queued input. */
    sender=(SudekiMpStoryMeleeSender){0};receiver=(SudekiMpStoryMeleeReceiver){0};
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,100,1,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,100,1));
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,340,2,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,340,1));
    assert(SudekiMpStoryMeleeTake(&receiver,351,1,&buttons,&serial) && buttons==2 && serial==2);
    assert(receiver.dropped==1);
    /* Older than the acknowledged-world window: no replay at a new frame. */
    sender=(SudekiMpStoryMeleeSender){0};receiver=(SudekiMpStoryMeleeReceiver){0};
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,10,1,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,11,10));
    assert(SudekiMpStoryMeleeTake(&receiver,12,10,&buttons,&serial) && buttons==2 && !serial && receiver.dropped==1);
    /* Bounded history/overflow plus saturation; no wrapped ID can replay. */
    sender=(SudekiMpStoryMeleeSender){0};receiver=(SudekiMpStoryMeleeReceiver){0};
    for(unsigned i=0;i<40u;++i)
        assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,100+i,i%2?3:1,TRUE,&input));
    assert(input.count==16 && input.samples[0].serial==25);
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,140,1) && receiver.dropped==24 && receiver.count==16);
    for(unsigned i=0;i<16u;++i) {
        assert(SudekiMpStoryMeleeTake(&receiver,141,1,&buttons,&serial));assert(serial==i+25u);
    }
    sender.sequence=UINT32_MAX;assert(!SudekiMpStoryMeleeBuild(&sender,&fence,1,142,1,TRUE,&input));
    sender.sequence=0;sender.serial=UINT32_MAX;assert(!SudekiMpStoryMeleeBuild(&sender,&fence,1,142,1,TRUE,&input));
    sender=(SudekiMpStoryMeleeSender){0};receiver=(SudekiMpStoryMeleeReceiver){0};
    assert(SudekiMpStoryMeleeBuild(&sender,&fence,1,UINT32_MAX-5u,1,TRUE,&input));
    assert(SudekiMpStoryMeleeReceive(&receiver,&input,UINT32_MAX-4u,1));
    assert(SudekiMpStoryMeleeTake(&receiver,4u,1,&buttons,&serial) && buttons==1);
    puts("PASS melee codec, loss/reordering/dedup, native states, cancellation, stale/overflow and clock wrap policies");
}
int main(void) {policy();loopback();return 0;}
