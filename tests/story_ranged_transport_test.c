#include "network/lan_party_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static SudekiMpLanPartySession *nodes[4];
static SudekiMpLanPartyPeerStatus peers[4];
static SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=15,.leader_seat=2,.world="ranged_fixture"};
static void poll(void) {
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick) {scene.observed_tick=now;assert(SudekiMpLanPartyPublishStoryScene(nodes[0],&scene));}
    for(unsigned p=0;p<4;++p) SudekiMpLanPartyPoll(nodes[p],now);
}
static SudekiMpLanStoryControlState control(unsigned p,unsigned transaction) {
    SudekiMpLanStoryControlState s={.fence={1,1,transaction,1,p,p==1?3:p==2?1:0},
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
        .assignment_enabled=1,.character={2,3,1,0}};
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
        .available_mask=15,.leader_character=2};
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
        SudekiMpLanStoryRanged input={.fence=state.fence,.sequence=1,.acknowledged_frame=1,
            .press=1,.held=1,.direction={0,0,1},.target={10,2,100}},got,invalid;
        uint8_t bytes[SUDEKIMP_STORY_RANGED_WIRE_SIZE];
        assert(SudekiMpLanStoryRangedEncode(&input,bytes,sizeof(bytes)));
        assert(SudekiMpLanStoryRangedDecode(bytes,sizeof(bytes),&got));
        uint8_t roundtrip[SUDEKIMP_STORY_RANGED_WIRE_SIZE];
        assert(SudekiMpLanStoryRangedEncode(&got,roundtrip,sizeof(roundtrip)) &&
            !memcmp(bytes,roundtrip,sizeof(bytes)));
        bytes[33]=1;memset(&got,0xab,sizeof(got));invalid=got;
        assert(!SudekiMpLanStoryRangedDecode(bytes,sizeof(bytes),&got) && !memcmp(&got,&invalid,sizeof(got)));
        invalid=input;invalid.direction[0]=NAN;assert(!SudekiMpLanStoryRangedValid(&invalid));
        invalid=input;invalid.fence.character=2;assert(!SudekiMpLanStoryRangedValid(&invalid));
        invalid=input;invalid.fence.player=3;assert(!SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&invalid));
        invalid=input;invalid.acknowledged_frame=2;assert(!SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&invalid));
        assert(!SudekiMpLanPartySendStoryRanged(nodes[0],&peers[p].lease,&input));
        assert(SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&input));
        BOOL received=FALSE;uint32_t receipt=0;
        for(unsigned i=0;i<120 && !received;++i) {
            poll();received=SudekiMpLanPartyTakeStoryRanged(nodes[0],&peers[p].lease,GetTickCount(),&got,&receipt);Sleep(1);
        }
        assert(received && receipt && got.held && got.press==1 && got.fence.character==input.fence.character);
        assert(!SudekiMpLanPartyTakeStoryRanged(nodes[0],&peers[p].lease,GetTickCount(),&got,NULL));
        assert(!SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&input));
        ++input.sequence;input.held=0;
        assert(SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(SudekiMpLanPartyTakeStoryRanged(nodes[0],&peers[p].lease,GetTickCount(),&got,NULL) && !got.held && got.press==1);
        ++input.sequence;assert(SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&input));
        for(unsigned i=0;i<6;++i) {poll();Sleep(1);}
        assert(!SudekiMpLanPartyTakeStoryRanged(nodes[0],&peers[p].lease,GetTickCount()+251,&got,NULL));
        control(p,2);++input.sequence;
        assert(!SudekiMpLanPartySendStoryRanged(nodes[p],&peers[p].lease,&input));
    }
    SudekiMpStoryVfx fx={.epoch=1,.revision=1,.stream=9,.tick=GetTickCount(),.observed=1,.count=2,.current="road"};
    for(unsigned i=0;i<2;++i)fx.entries[i]=(SudekiMpLanArenaSpiritVfxSnapshot){.instance_sequence=i+1,
        .owner_actor_type=i?0x0e:1,.kind=i?SUDEKIMP_LAN_STORY_VFX_ELCO_SHOT_FIRST:SUDEKIMP_LAN_STORY_VFX_AILISH_SHOT_FIRST,
        .emitted_host_tick=fx.tick,.position={5,2,7},.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    assert(SudekiMpLanPartyPublishStoryVfx(nodes[0],&fx));mask=0;
    for(unsigned i=0;i<120 && mask!=14;++i) {
        poll();for(unsigned p=1;p<4;++p) {SudekiMpStoryVfx got;
            if(SudekiMpLanPartyGetStoryVfx(nodes[p],&peers[p].lease,GetTickCount(),&got)) {
                assert(got.count==2 && !memcmp(got.entries,fx.entries,sizeof(fx.entries)));mask|=1u<<p;
            }}Sleep(1);
    }
    assert(mask==14); /* Includes both shooters and a third observing client. */
    /* Host-owned skill effects for every caster reach every client, including
     * the caster's own view. Their counters are independent of Tal's header. */
    const unsigned kinds[]={SUDEKIMP_LAN_STORY_VFX_SFXBS001_SPIRIT_WIND,
        SUDEKIMP_LAN_STORY_VFX_SFXES008_NANO_REPAIRS,
        SUDEKIMP_LAN_STORY_VFX_SFXTS008_INVINCIBLE_CITADEL,
        SUDEKIMP_LAN_STORY_VFX_SFXAS011_WITCHES_KISS};
    const unsigned types[]={5,14,0,1};
    fx.count=4;fx.cast_sequence=2;fx.tick=GetTickCount();
    for(unsigned c=0;c<4;++c) fx.entries[c]=(SudekiMpLanArenaSpiritVfxSnapshot){
        .instance_sequence=10+c,.skill_sequence=c==2?2:50+c,.owner_actor_type=types[c],.kind=kinds[c],
        .emitted_host_tick=fx.tick,.position={5,2,7},.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    assert(!SudekiMpLanPartyPublishStoryVfx(nodes[1],&fx));
    assert(SudekiMpLanPartyPublishStoryVfx(nodes[0],&fx));mask=0;
    for(unsigned i=0;i<120 && mask!=14;++i) {
        poll();for(unsigned p=1;p<4;++p) {SudekiMpStoryVfx got;
            if(SudekiMpLanPartyGetStoryVfx(nodes[p],&peers[p].lease,GetTickCount(),&got) && got.count==4) {
                assert(got.cast_sequence==2 && !memcmp(got.entries,fx.entries,sizeof(fx.entries)));mask|=1u<<p;
            }}Sleep(1);
    }
    assert(mask==14);
    for(unsigned p=1;p<4;++p)SudekiMpLanPartyDestroy(nodes[p],FALSE);
    SudekiMpLanPartyDestroy(nodes[0],FALSE);
    puts("PASS Ailish/Elco intent, release, duplicate/expiry/fence rejection, host shots and independent four-roster skill visuals to all 3 clients");return 0;
}
