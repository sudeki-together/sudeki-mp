#include "network/lan_party_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static SudekiMpLanPartySession *nodes[4];
static SudekiMpLanPartyPeerStatus peers[4];
static SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=15,.leader_seat=3,.world="block_fixture"};
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
int main(void) {
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
        SudekiMpLanStoryBlock input={.fence=state.fence,.sequence=1,.acknowledged_frame=1,
            .held=1},got,invalid;
        uint8_t bytes[SUDEKIMP_STORY_BLOCK_WIRE_SIZE];
        assert(SudekiMpLanStoryBlockEncode(&input,bytes,sizeof(bytes)));
        assert(SudekiMpLanStoryBlockDecode(bytes,sizeof(bytes),&got));
        uint8_t roundtrip[SUDEKIMP_STORY_BLOCK_WIRE_SIZE];
        assert(SudekiMpLanStoryBlockEncode(&got,roundtrip,sizeof(roundtrip)) &&
            !memcmp(bytes,roundtrip,sizeof(bytes)));
        bytes[29]=1;memset(&got,0xab,sizeof(got));invalid=got;
        assert(!SudekiMpLanStoryBlockDecode(bytes,sizeof(bytes),&got) && !memcmp(&got,&invalid,sizeof(got)));
        invalid=input;invalid.held=2;assert(!SudekiMpLanStoryBlockValid(&invalid));
        invalid=input;invalid.fence.character=3;assert(!SudekiMpLanStoryBlockValid(&invalid));
        invalid=input;invalid.fence.player=3;assert(!SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&invalid));
        invalid=input;invalid.acknowledged_frame=2;assert(!SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&invalid));
        assert(!SudekiMpLanPartySendStoryBlock(nodes[0],&peers[p].lease,&input));
        assert(SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&input));
        BOOL received=FALSE;uint32_t receipt=0;
        for(unsigned i=0;i<120 && !received;++i) {
            poll();received=SudekiMpLanPartyTakeStoryBlock(nodes[0],&peers[p].lease,GetTickCount(),&got,&receipt);Sleep(1);
        }
        assert(received && receipt && got.held && got.fence.character==input.fence.character);
        assert(!SudekiMpLanPartyTakeStoryBlock(nodes[0],&peers[p].lease,GetTickCount(),&got,NULL));
        assert(!SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&input));
        ++input.sequence;input.held=0;
        assert(SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(SudekiMpLanPartyTakeStoryBlock(nodes[0],&peers[p].lease,GetTickCount(),&got,NULL) && !got.held);
        ++input.sequence;assert(SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(!SudekiMpLanPartyTakeStoryBlock(nodes[0],&peers[p].lease,GetTickCount()+251,&got,NULL));
        control(p,2);++input.sequence;
        assert(!SudekiMpLanPartySendStoryBlock(nodes[p],&peers[p].lease,&input));
    }
    SudekiMpStoryVfx fx={.epoch=1,.revision=1,.stream=9,.tick=GetTickCount(),.observed=1,.count=2,.current="road"};
    for(unsigned i=0;i<2;++i)fx.entries[i]=(SudekiMpLanArenaSpiritVfxSnapshot){.instance_sequence=i+1,
        .owner_actor_type=i?SUDEKIMP_LAN_ARENA_BUKI_TYPE:SUDEKIMP_LAN_ARENA_TAL_TYPE,
        .kind=i?SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP:SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP,
        .emitted_host_tick=fx.tick,.position={5,2,7},.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    assert(SudekiMpLanPartyPublishStoryVfx(nodes[0],&fx));mask=0;
    for(unsigned i=0;i<120 && mask!=14;++i) {
        poll();for(unsigned p=1;p<4;++p) {SudekiMpStoryVfx got;
            if(SudekiMpLanPartyGetStoryVfx(nodes[p],&peers[p].lease,GetTickCount(),&got)) {
                assert(got.count==2 && !memcmp(got.entries,fx.entries,sizeof(fx.entries)));mask|=1u<<p;
            }}Sleep(1);
    }
    assert(mask==14); /* Both blockers and the third observing client see both shields. */
    for(unsigned p=1;p<4;++p)SudekiMpLanPartyDestroy(nodes[p],FALSE);
    SudekiMpLanPartyDestroy(nodes[0],FALSE);
    puts("PASS Tal/Buki block intent, release, duplicate/expiry/fence rejection, shields to all 3 clients");return 0;
}
