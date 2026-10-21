/* Actual session receive/reassembly, synthetic authenticated peer; no native objects. */
#include "../src/network/lan_party_session.c"
#include <assert.h>
#include <stdio.h>
void test_large_story_world_reassembly(const SudekiMpLanStoryWorldFrame *frame) {
    SudekiMpLanPartySession *s=calloc(1,sizeof(*s));assert(s);
    s->config.local_seat=1;s->config.story_observation=2;s->now=100;
    s->story_scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
        .observed_tick=100,.available_mask=1,.leader_seat=0,.world="capacity_fixture"};
    s->story_scene_received_at=100;
    Peer *p=&s->peer[1];p->status.phase=SUDEKIMP_LAN_PARTY_OBSERVING;
    p->status.lease=(SudekiMpLanPartyLease){.token=7,.generation=2,.seat=1};
    p->address.sin_family=AF_INET;p->address.sin_port=htons(12345);p->address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    for(unsigned i=0;i<16;++i) {
        uint8_t packet[MAX_DATAGRAM]={0};size_t size=0;
        assert(SudekiMpLanStoryWorldChunkEncode(frame,i,packet+HEADER,sizeof(packet)-HEADER,&size));
        memcpy(packet,"SMP4",4);put16(packet+4,SUDEKIMP_LAN_PARTY_VERSION);packet[6]=MSG_STORY_WORLD;packet[7]=1;
        put32(packet+8,2);put64(packet+12,7);put32(packet+20,i+1);put16(packet+24,(uint16_t)size);
        receive_message(s,&p->address,packet,HEADER+size);
        assert(s->story_world_count==(i==15?1u:0u));
    }
    assert(s->story_world_sequence==frame->sequence && s->story_world_frames[0].count==128);
    for(unsigned i=0;i<128;++i) assert(s->story_world_frames[0].actors[i].identifier==frame->actors[i].identifier);
    printf("PASS world receive: 16 chunks / 128 actors; session bytes=%u actor bytes=%u\n",(unsigned)sizeof(*s),(unsigned)sizeof(frame->actors[0]));
    free(s);
}
