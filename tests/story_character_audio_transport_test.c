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

    SudekiMpStoryAudio audio={.epoch=1,.revision=1,.stream=1,.bank=123,.character=2,.count=64,.tick=GetTickCount()};
    for(unsigned i=0;i<64;++i)audio.cues[i]=(SudekiMpStoryAudioCue){i+1,audio.tick,321,0x3f800000,(uint16_t)i,1,0};
    uint8_t bytes[SUDEKIMP_STORY_AUDIO_MAX];size_t size=0;SudekiMpStoryAudio got,bad;
    assert(SudekiMpStoryAudioEncode(&audio,bytes,sizeof(bytes),&size) && size+28<=1468);
    assert(SudekiMpStoryAudioDecode(bytes,size,&got) && got.count==64);
    bytes[28]=1;assert(!SudekiMpStoryAudioDecode(bytes,size,&got));bytes[28]=0;
    audio.start_sequence=7;audio.start_tick=audio.tick;
    assert(SudekiMpStoryAudioEncode(&audio,bytes,sizeof(bytes),&size));
    assert(SudekiMpStoryAudioDecode(bytes,size,&got) && got.start_sequence==7 && got.start_tick==audio.tick);
    bad=audio;bad.start_tick=bad.tick+1;assert(!SudekiMpStoryAudioValid(&bad));
    bad=audio;bad.start_sequence=0;assert(!bad.start_tick || !SudekiMpStoryAudioValid(&bad));
    assert(!SudekiMpStoryAudioDecode(bytes,size-1,&got));
    bad=audio;bad.cues[1].id=bad.cues[0].id;assert(!SudekiMpStoryAudioValid(&bad));
    bad=audio;bad.cues[0].gain=0x7fc00000;assert(!SudekiMpStoryAudioValid(&bad));
    bad=audio;bad.cues[0].started=audio.tick+1;assert(!SudekiMpStoryAudioValid(&bad));
    bad=audio;bad.cues[0].mode=2;assert(!SudekiMpStoryAudioValid(&bad));
    assert(!SudekiMpLanPartyPublishStoryAudio(nodes[1],&audio));
    assert(SudekiMpLanPartyPublishStoryAudio(nodes[0],&audio));mask=0;
    for(unsigned i=0;i<120 && mask!=14;++i) {
        poll();for(unsigned p=1;p<4;++p)if(SudekiMpLanPartyGetStoryAudio(nodes[p],&peers[p].lease,GetTickCount(),2,&got)) {
            assert(got.count==64 && got.stream==1 && got.bank==123 && got.start_sequence==7 && got.start_tick==audio.start_tick);mask|=1u<<p;
        }Sleep(1);
    }
    assert(mask==14);
    /* Every observer receives every character, independent of its assigned
     * local roster slot. Same cue IDs across actors must not alias banks. */
    for(unsigned c=0;c<4;++c) {
        audio.character=(uint8_t)c;audio.bank=123+c;audio.tick=GetTickCount();
        for(unsigned j=0;j<audio.count;++j)audio.cues[j].started=audio.tick;
        assert(SudekiMpLanPartyPublishStoryAudio(nodes[0],&audio));
    }
    unsigned delivered=0;
    for(unsigned i=0;i<120 && delivered!=0xfffu;++i) {
        poll();
        for(unsigned p=1;p<4;++p)for(unsigned c=0;c<4;++c)
            if(SudekiMpLanPartyGetStoryAudio(nodes[p],&peers[p].lease,GetTickCount(),c,&got) &&
                got.bank==123+c && got.count==64) delivered|=1u<<((p-1)*4+c);
        Sleep(1);
    }
    assert(delivered==0xfffu);audio.character=2;audio.bank=125;
    SudekiMpLanPartyLease wrong=peers[1].lease;++wrong.generation;
    assert(!SudekiMpLanPartyGetStoryAudio(nodes[1],&wrong,GetTickCount(),2,&got));
    assert(!SudekiMpLanPartyGetStoryAudio(nodes[1],&peers[1].lease,GetTickCount()+1000,2,&got));
    audio.tick=GetTickCount();audio.count=0;assert(SudekiMpLanPartyPublishStoryAudio(nodes[0],&audio));
    for(unsigned i=0;i<10;++i){poll();Sleep(1);}
    assert(SudekiMpLanPartyGetStoryAudio(nodes[1],&peers[1].lease,GetTickCount(),2,&got) && !got.count);
    ++scene.epoch;++scene.revision;poll();for(unsigned i=0;i<10;++i){poll();Sleep(1);}
    assert(!SudekiMpLanPartyGetStoryAudio(nodes[1],&peers[1].lease,GetTickCount(),2,&got));
    for(unsigned p=1;p<4;++p)SudekiMpLanPartyDestroy(nodes[p],FALSE);
    SudekiMpLanPartyDestroy(nodes[0],FALSE);
    puts("PASS complete character audio to all clients, malformed/duplicate cue/late/lease/client-authority rejection, empty retirement and scene fence");return 0;
}
