/* Camera codec/history and real loopback transport. No native scripts/render. */
#include "network/lan_party_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static SudekiMpStoryCastView sample(uint32_t tick) {
    SudekiMpStoryCastView s={.fence={1,1,1,7,1,2},.stream=9,.sequence=1,.revision=1,
        .tick=tick,.cast_sequence=3,.kind=1,.current="road",
        .view={.valid=1,.camera_serial=13,.matrix={1,0,0,0,0,1,0,0,0,0,1,0,1,2,3,1},
            .projection={1,.1f,1000}}};
    return s;
}
static void codec_history(void) {
    SudekiMpStoryCastView a=sample(100),b,out,before;
    uint8_t bytes[SUDEKIMP_STORY_CAST_VIEW_SIZE];
    assert(sizeof(bytes)+28<=1468);
    assert(SudekiMpStoryCastViewEncode(&a,bytes,sizeof(bytes)));
    assert(SudekiMpStoryCastViewDecode(bytes,sizeof(bytes),&out));
    uint8_t roundtrip[SUDEKIMP_STORY_CAST_VIEW_SIZE];
    assert(SudekiMpStoryCastViewEncode(&out,roundtrip,sizeof(roundtrip)) && !memcmp(bytes,roundtrip,sizeof(bytes)));
    for(unsigned character=0;character<4;++character) {
        b=a; b.fence.character=(uint8_t)character;
        assert(SudekiMpStoryCastViewEncode(&b,roundtrip,sizeof(roundtrip)));
        assert(SudekiMpStoryCastViewDecode(roundtrip,sizeof(roundtrip),&out));
        assert(out.fence.character==character);
    }
    before=out;
    for(unsigned n=0;n<sizeof(bytes);++n) {
        assert(!SudekiMpStoryCastViewDecode(bytes,n,&out) && !memcmp(&out,&before,sizeof(out)));
    }
    unsigned reserved[]={0,23,42,43};
    for(unsigned i=0;i<4;++i) {bytes[reserved[i]]^=4;assert(!SudekiMpStoryCastViewDecode(bytes,sizeof(bytes),&out));bytes[reserved[i]]^=4;}
    for(unsigned i=0;i<19;++i) {
        b=a; (i<16?b.view.matrix:b.view.projection)[i<16?i:i-16]=NAN;
        assert(!SudekiMpStoryCastViewValid(&b));
    }
    b=a;b.kind=3;b.view.camera_serial=a.cast_sequence*4u+3u;assert(SudekiMpStoryCastViewValid(&b)); /* door camera */
    b=a;b.kind=4;b.view.camera_serial=a.cast_sequence*4u+4u;assert(!SudekiMpStoryCastViewValid(&b));
    b=a;b.fence.character=4;assert(!SudekiMpStoryCastViewValid(&b));
    b=a;b.cast_sequence=0;assert(!SudekiMpStoryCastViewValid(&b));
    b=a;b.view.camera_serial=14;assert(!SudekiMpStoryCastViewValid(&b));
    b=a;b.kind=0;assert(!SudekiMpStoryCastViewValid(&b));
    memset(&b.view,0,sizeof(b.view));assert(SudekiMpStoryCastViewValid(&b));
    b=a;b.view.projection[2]=.01f;assert(!SudekiMpStoryCastViewValid(&b));
    b=a;memset(b.current,'a',sizeof(b.current));assert(!SudekiMpStoryCastViewValid(&b));
    SudekiMpStoryCastViewHistory h={0};
    assert(SudekiMpStoryCastViewPush(&h,&a));
    assert(!SudekiMpStoryCastViewSample(&h,99,&out)); /* No early camera. */
    b=a;b.sequence=2;b.tick=120;b.view.matrix[12]=3;
    assert(SudekiMpStoryCastViewPush(&h,&b));
    assert(!SudekiMpStoryCastViewPush(&h,&a)); /* Reordered. */
    assert(SudekiMpStoryCastViewSample(&h,110,&out) && fabsf(out.view.matrix[12]-2)<.001f);
    a=b;a.sequence=3;a.tick=140;a.kind=2;a.view.camera_serial=14;a.view.matrix[12]=40;
    assert(SudekiMpStoryCastViewPush(&h,&a));
    assert(SudekiMpStoryCastViewSample(&h,130,&out) && out.kind==1 && out.view.matrix[12]==3);
    assert(SudekiMpStoryCastViewSample(&h,140,&out) && out.kind==2);
    b=a;b.sequence=4;b.tick=160;b.kind=0;memset(&b.view,0,sizeof(b.view));
    assert(SudekiMpStoryCastViewPush(&h,&b));
    assert(SudekiMpStoryCastViewSample(&h,159,&out) && out.kind==2);
    assert(SudekiMpStoryCastViewSample(&h,160,&out) && out.kind==0);
    before=out;assert(!SudekiMpStoryCastViewSample(&h,411,&out) && !memcmp(&before,&out,sizeof(out)));
    a=b;a.sequence=5;a.tick=180;a.stream=8;assert(!SudekiMpStoryCastViewPush(&h,&a));
    a.stream=10;a.cast_sequence=0;assert(SudekiMpStoryCastViewPush(&h,&a) && h.count==1);
    a.sequence=6;a.tick=200;a.fence.transaction=2;assert(SudekiMpStoryCastViewPush(&h,&a) && h.count==1);
    for(unsigned i=0;i<16;++i) {++a.sequence;a.tick+=20;assert(SudekiMpStoryCastViewPush(&h,&a));}
    assert(h.count==8 && !SudekiMpStoryCastViewSample(&h,200,&out));
}
static SudekiMpLanPartySession *host,*clients[4];
static SudekiMpLanPartyPeerStatus peers[4];
static SudekiMpLanStoryControlState controls[4];
static SudekiMpLanStoryScene scene;
static void poll(void) {
    static uint32_t published_revision;
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick || published_revision!=scene.revision) {
        scene.observed_tick=now;
        assert(SudekiMpLanPartyPublishStoryScene(host,&scene)); published_revision=scene.revision;
    }
    SudekiMpLanPartyPoll(host,scene.observed_tick);
    for(unsigned player=1;player<4;++player) SudekiMpLanPartyPoll(clients[player],scene.observed_tick);
}
static void control_ready(unsigned player,unsigned character,unsigned transaction) {
    controls[player]=(SudekiMpLanStoryControlState){
        .fence={1,scene.revision,transaction,7+character,(uint8_t)player,(uint8_t)character},
        .phase=SUDEKIMP_STORY_CONTROL_PREPARE,.observed_tick=GetTickCount()};
    assert(SudekiMpLanPartyPublishStoryControl(host,&peers[player].lease,&controls[player]));
    BOOL ack=FALSE;
    for(unsigned i=0;i<80&&!ack;++i) {
        SudekiMpLanStoryControlState remote;poll();
        if(SudekiMpLanPartyGetStoryControl(clients[player],&peers[player].lease,GetTickCount(),&remote))
            assert(SudekiMpLanPartyAcknowledgeStoryControl(clients[player],&peers[player].lease,&controls[player].fence));
        ack=SudekiMpLanPartyStoryControlAcknowledged(host,&peers[player].lease,&controls[player].fence);
        if(!ack)Sleep(1);
    }
    assert(ack); controls[player].phase=SUDEKIMP_STORY_CONTROL_READY;
    controls[player].observed_tick=GetTickCount();
    assert(SudekiMpLanPartyPublishStoryControl(host,&peers[player].lease,&controls[player]));poll();
}
static BOOL get(unsigned player,uint32_t tick,SudekiMpStoryCastView *out) {
    return SudekiMpLanPartyGetStoryCastView(clients[player],&peers[player].lease,
        &controls[player].fence,GetTickCount(),tick,out);
}
static void transport(unsigned leader) {
    scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=15,.leader_seat=(uint8_t)leader,.world="fixture"};
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000,.assignment_enabled=1};
    for(unsigned i=0;i<4;++i) config.character[i]=(uint8_t)((leader+i)%4);
    memset(config.game_hash,0x25,sizeof(config.game_hash));
    host=SudekiMpLanPartyCreate(&config);assert(host);
    config.host_ipv4="127.0.0.1";config.port=SudekiMpLanPartyPort(host);
    for(unsigned i=1;i<4;++i) {
        config.local_seat=(uint8_t)i;clients[i]=SudekiMpLanPartyCreate(&config);assert(clients[i]);
    }
    BOOL connected=FALSE;
    for(unsigned i=0;i<300&&!connected;++i) {
        poll();connected=TRUE;
        for(unsigned player=1;player<4;++player) {
            SudekiMpLanPartyPeerStatus server;SudekiMpLanStoryScene remote;
            assert(SudekiMpLanPartyPeerStatusGet(clients[player],player,&peers[player]));
            assert(SudekiMpLanPartyPeerStatusGet(host,player,&server));
            connected=connected && peers[player].phase==SUDEKIMP_LAN_PARTY_OBSERVING && server.transport_confirmed &&
                SudekiMpLanPartyGetStoryScene(clients[player],&peers[player].lease,GetTickCount(),&remote);
        }
        if(!connected)Sleep(5);
    }
    assert(connected);
    for(unsigned player=1;player<4;++player) control_ready(player,config.character[player],1);
    SudekiMpStoryCastView views[4],got,before;
    for(unsigned player=1;player<4;++player) {
        unsigned character=config.character[player];
        views[player]=sample(GetTickCount());views[player].fence.character=(uint8_t)character;
        views[player].cast_sequence=(uint16_t)(10+character);
        views[player].kind=(uint8_t)(1+player%2);
        views[player].view.camera_serial=views[player].cast_sequence*4+views[player].kind;
        views[player].view.matrix[12]=(float)(100+character);
        assert(!SudekiMpLanPartyPublishStoryCastView(clients[player],&views[player]));
        assert(SudekiMpLanPartyPublishStoryCastView(host,&views[player]));poll();
        /* Publishing this actor cannot create a camera for another owner. */
        for(unsigned other=player+1;other<4;++other) assert(!get(other,GetTickCount(),&got));
    }
    for(unsigned player=1;player<4;++player) {
        assert(get(player,views[player].tick,&got));
        assert(got.fence.character==config.character[player] && got.fence.player==player &&
            got.cast_sequence==views[player].cast_sequence && got.kind==views[player].kind &&
            got.view.matrix[12]==views[player].view.matrix[12]);
        assert(!get(player,views[player].tick-1,&got));
        SudekiMpLanPartyLease foreign=peers[player].lease;++foreign.generation;
        assert(!SudekiMpLanPartyGetStoryCastView(clients[player],&foreign,&controls[player].fence,
            GetTickCount(),views[player].tick,&got));
        SudekiMpLanStoryControlFence bad=controls[player].fence;++bad.actor_generation;
        assert(!SudekiMpLanPartyGetStoryCastView(clients[player],&peers[player].lease,&bad,
            GetTickCount(),views[player].tick,&got));
        assert(!SudekiMpLanPartyGetStoryCastView(clients[player],&peers[player].lease,&controls[player].fence,
            GetTickCount()+2000,views[player].tick,&got));
    }
    /* The host's camera is never relabeled as a remote character's view. */
    SudekiMpStoryCastView local=sample(GetTickCount()); local.fence.character=(uint8_t)leader;
    local.view.matrix[12]=999;assert(SudekiMpLanPartyPublishStoryCastView(host,&local));poll();
    for(unsigned player=1;player<4;++player) {
        assert(get(player,GetTickCount(),&got) && got.view.matrix[12]==views[player].view.matrix[12]);
    }
    local.fence.character=4;assert(!SudekiMpLanPartyPublishStoryCastView(host,&local));
    ++scene.revision;scene.inside_mask=(uint8_t)(1u<<leader);strcpy(scene.temporary,"room");poll();
    for(unsigned player=1;player<4;++player) {
        views[player].tick=GetTickCount();assert(SudekiMpLanPartyPublishStoryCastView(host,&views[player]));
    }
    poll();
    for(unsigned player=1;player<4;++player) assert(get(player,GetTickCount(),&got) && got.revision==2);
    /* Ending one cast does not end the other clients' simultaneous cameras. */
    views[1].tick=GetTickCount();views[1].kind=0;memset(&views[1].view,0,sizeof(views[1].view));
    assert(SudekiMpLanPartyPublishStoryCastView(host,&views[1]));poll();
    assert(get(1,GetTickCount(),&got) && !got.kind);
    for(unsigned player=2;player<4;++player) assert(get(player,GetTickCount(),&got) && got.kind);
    /* A new acknowledged ownership transaction cannot inherit old camera
     * history, even for the same character and a lower fresh cast sequence. */
    assert(SudekiMpLanPartyRevokeStoryControl(host,&peers[1].lease));poll();
    assert(!get(1,GetTickCount(),&got));
    control_ready(1,config.character[1],2);
    assert(!get(1,GetTickCount(),&got));
    views[1]=sample(GetTickCount());views[1].fence.character=config.character[1];
    assert(SudekiMpLanPartyPublishStoryCastView(host,&views[1]));poll();
    assert(get(1,GetTickCount(),&got) && got.fence.transaction==2 && got.cast_sequence==3);
    before=got;
    SudekiMpLanStoryControlFence old=controls[1].fence;old.transaction=1;
    assert(!SudekiMpLanPartyGetStoryCastView(clients[1],&peers[1].lease,&old,GetTickCount(),views[1].tick,&got));
    assert(!memcmp(&got,&before,sizeof(got)));
    for(unsigned player=1;player<4;++player) SudekiMpLanPartyDestroy(clients[player],FALSE);
    SudekiMpLanPartyDestroy(host,FALSE);
}
int main(void) {
    codec_history();
    for(unsigned leader=0;leader<4;++leader) transport(leader);
    puts("Story cast view: all roster owners, three simultaneous clients, native camera kinds, timeline, isolation and reacquisition UDP PASS");
    return 0;
}
