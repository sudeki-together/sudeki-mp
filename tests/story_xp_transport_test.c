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


    SudekiMpStoryXp xp={.epoch=1,.revision=1,.tick=GetTickCount(),.stream=1,.current="FixtureExterior"};
    for(unsigned i=0;i<24;++i)xp.slots[i]=(SudekiMpStoryXpPopup){.serial=i+1,.amount=64+i,.position={1,2,3},.height=1.6f,.elapsed=.2f,.duration=1.5f};
    uint8_t wire[SUDEKIMP_STORY_XP_SIZE];size_t size=0;SudekiMpStoryXp got;
    assert(SudekiMpStoryXpEncode(&xp,wire,sizeof(wire),&size) && size==sizeof(wire));
    assert(SudekiMpStoryXpDecode(wire,size,&got) && !memcmp(&xp,&got,sizeof(xp)));
    SudekiMpStoryXp bad=xp;bad.slots[1].elapsed=NAN;assert(!SudekiMpStoryXpValid(&bad));
    bad=xp;bad.slots[0].serial=0;assert(!SudekiMpStoryXpValid(&bad));
    bad=xp;bad.slots[0].amount=10000;assert(!SudekiMpStoryXpValid(&bad));
    bad=xp;memset(bad.current,'x',40);assert(!SudekiMpStoryXpValid(&bad));
    bad=xp;bad.slots[0].position[0]=INFINITY;assert(!SudekiMpStoryXpValid(&bad));
    assert(!SudekiMpStoryXpDecode(wire,size-1,&got));
    assert(!SudekiMpLanPartyPublishStoryXp(nodes[1],&xp));
    poll();xp.tick=GetTickCount();assert(SudekiMpLanPartyPublishStoryXp(nodes[0],&xp));
    for(unsigned i=0;i<20;++i){poll();Sleep(1);}
    for(unsigned p=1;p<4;++p) {
        uint32_t age;assert(SudekiMpLanPartyGetStoryXp(nodes[p],&peers[p].lease,GetTickCount(),&got,&age));
        assert(got.slots[23].amount==87 && got.slots[0].serial==1 && age<1000);
        SudekiMpLanPartyLease stale=peers[p].lease;stale.generation++;
        assert(!SudekiMpLanPartyGetStoryXp(nodes[p],&stale,GetTickCount(),&got,&age));
        assert(!SudekiMpLanPartyGetStoryXp(nodes[p],&peers[p].lease,GetTickCount()+10000,&got,&age));
    }
    ++scene.epoch;++scene.revision;poll();for(unsigned i=0;i<10;++i){poll();Sleep(1);}
    uint32_t age;assert(!SudekiMpLanPartyGetStoryXp(nodes[1],&peers[1].lease,GetTickCount(),&got,&age));
    assert(!SudekiMpLanPartyPublishStoryXp(nodes[0],&xp));
    xp.epoch=scene.epoch;xp.revision=scene.revision;xp.tick=GetTickCount();memset(xp.slots,0,sizeof(xp.slots));
    assert(SudekiMpLanPartyPublishStoryXp(nodes[0],&xp));
    for(unsigned i=0;i<10;++i){poll();Sleep(1);}
    assert(SudekiMpLanPartyGetStoryXp(nodes[1],&peers[1].lease,GetTickCount(),&got,&age) && !got.slots[0].serial);
    for(unsigned p=1;p<4;++p)SudekiMpLanPartyDestroy(nodes[p],FALSE);
    SudekiMpLanPartyDestroy(nodes[0],FALSE);
    puts("PASS XP codec, all three observing clients, host authority, stale lease/scene/age and empty pool");return 0;
}
