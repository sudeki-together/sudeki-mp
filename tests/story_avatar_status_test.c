/* Four real loopback transports and plain status records. No native game
 * objects, executable, HUD or stat writes participate in this fixture. */
#include "network/lan_party_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static SudekiMpLanPartySession *nodes[4];
static SudekiMpLanStoryScene scene;
static SudekiMpLanStoryScene published_scene;
static SudekiMpLanStoryAvatarStatus records[4];
static SudekiMpLanPartyPeerStatus peers[4];
static void pump(void) {
    uint32_t now=GetTickCount();
    scene.observed_tick=now;
    if(!published_scene.revision || now!=published_scene.observed_tick ||
        scene.revision!=published_scene.revision) {
        assert(SudekiMpLanPartyPublishStoryScene(nodes[0],&scene)); published_scene=scene;
    }
    for(unsigned pass=0;pass<3u;++pass)
        for(unsigned i=0;i<4u;++i) if(nodes[i]) SudekiMpLanPartyPoll(nodes[i],now);
}
static SudekiMpLanStoryAvatarStatus sample(unsigned player) {
    SudekiMpLanStoryAvatarStatus v={.epoch=3,.revision=7,.spawn_generation=100+player,
        .sequence=1,.observed_tick=UINT32_C(0x70000000),.received_tick=123456,
        .player=(uint8_t)player,.present=1,.hp=70.5f+player,.max_hp=100.5f+player,
        .sp=player?12.25f:0,.max_sp=player?20.5f:0};
    snprintf(v.name,sizeof(v.name),"Talos player %u",player+1u); return v;
}
static void codec(void) {
    SudekiMpLanStoryAvatarStatus v=sample(3),got;
    uint8_t bytes[SUDEKIMP_STORY_AVATAR_STATUS_WIRE_SIZE];
    assert(SudekiMpLanStoryAvatarStatusEncode(&v,bytes,sizeof(bytes)));
    assert(SudekiMpLanStoryAvatarStatusDecode(bytes,sizeof(bytes),&got));
    assert(got.received_tick==0 && got.observed_tick==v.observed_tick);
    assert(got.hp==v.hp && got.max_hp==v.max_hp && got.sp==v.sp && got.max_sp==v.max_sp);
    assert(got.player==3 && got.spawn_generation==103 && !strcmp(got.name,v.name));
    for(unsigned i=0;i<13u;++i) {
        SudekiMpLanStoryAvatarStatus bad=v;
        switch(i) {
        case 0: bad.hp=NAN; break;
        case 1: bad.max_hp=INFINITY; break;
        case 2: bad.sp=-1; break;
        case 3: bad.hp=bad.max_hp+1; break;
        case 4: bad.sp=bad.max_sp+1; break;
        case 5: bad.hp=bad.max_hp=0; break;
        case 6: bad.player=4; break;
        case 7: bad.spawn_generation=0; break;
        case 8: bad.sequence=0; break;
        case 9: bad.name[2]='\n'; break;
        case 10: memset(bad.name,'x',sizeof(bad.name)); break;
        case 11: bad.name[31]='x'; break; /* noncanonical tail after NUL */
        case 12: bad.present=0; break; /* tombstone must carry no stats/name */
        }
        assert(!SudekiMpLanStoryAvatarStatusValid(&bad));
        assert(!SudekiMpLanStoryAvatarStatusEncode(&bad,bytes,sizeof(bytes)));
    }
    assert(SudekiMpLanStoryAvatarStatusEncode(&v,bytes,sizeof(bytes)));
    bytes[38]=1; assert(!SudekiMpLanStoryAvatarStatusDecode(bytes,sizeof(bytes),&got));
    bytes[38]=0; assert(!SudekiMpLanStoryAvatarStatusDecode(bytes,sizeof(bytes)-1u,&got));
    v.present=0; v.hp=v.max_hp=v.sp=v.max_sp=0; memset(v.name,0,sizeof(v.name));
    assert(SudekiMpLanStoryAvatarStatusEncode(&v,bytes,sizeof(bytes)));
    assert(SudekiMpLanStoryAvatarStatusDecode(bytes,sizeof(bytes),&got) && !got.present);
}
static void setup(void) {
    SudekiMpLanPartyConfig c={.dev_play=1,.story_observation=2,.assignment_enabled=1,
        .lobby_members=15,.character={4,4,4,4},.avatar={5,5,5,5},
        .lobby_nonce={201,202,203,204},.timeout_ms=5000};
    scene=(SudekiMpLanStoryScene){.epoch=3,.revision=7,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=15,.leader_seat=2,.world="avatar_status_fixture"};
    nodes[0]=SudekiMpLanPartyCreate(&c); assert(nodes[0]);
    pump(); records[0]=sample(0);
    assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&records[0]));
    SudekiMpLanStoryAvatarStatus got;
    assert(SudekiMpLanPartyGetAvatarStatus(nodes[0],0,GetTickCount(),&got));
    assert(got.hp==70.5f && got.max_sp==0); /* local host, zero connected clients */
    c.port=SudekiMpLanPartyPort(nodes[0]); c.host_ipv4="127.0.0.1"; c.lobby_nonce[0]=0;
    for(unsigned i=1;i<4u;++i) {
        memset(c.lobby_nonce,0,sizeof(c.lobby_nonce)); c.lobby_nonce[i]=201u+i;
        c.local_seat=(uint8_t)i; nodes[i]=SudekiMpLanPartyCreate(&c); assert(nodes[i]);
    }
    BOOL complete=FALSE;
    for(unsigned retry=0;retry<1000u && !complete;++retry) {
        pump(); complete=TRUE;
        for(unsigned i=1;i<4u;++i) {
            assert(SudekiMpLanPartyPeerStatusGet(nodes[0],i,&peers[i]));
            complete=complete && peers[i].transport_confirmed && peers[i].phase==SUDEKIMP_LAN_PARTY_OBSERVING;
        }
        if(!complete) Sleep(1);
    }
    assert(complete);
    for(unsigned i=1;i<4u;++i) records[i]=sample(i);
}
static void all_peers(void) {
    BOOL complete=FALSE;
    for(unsigned retry=0;retry<500u && !complete;++retry) {
        pump();
        for(unsigned p=0;p<4u;++p) { ++records[p].sequence; assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&records[p])); }
        pump(); complete=TRUE;
        for(unsigned n=0;n<4u;++n) for(unsigned p=0;p<4u;++p) {
            SudekiMpLanStoryAvatarStatus got;
            if(!SudekiMpLanPartyGetAvatarStatus(nodes[n],p,GetTickCount(),&got)) complete=FALSE;
            else {
                assert(got.player==p && got.spawn_generation==records[p].spawn_generation);
                assert(got.hp==records[p].hp && !strcmp(got.name,records[p].name));
                assert(got.observed_tick==UINT32_C(0x70000000));
                assert(got.received_tick!=records[p].received_tick);
            }
        }
        if(!complete) Sleep(1);
    }
    assert(complete);
    assert(!SudekiMpLanPartyPublishAvatarStatus(nodes[1],&records[1])); /* host authority only */
}
static void zero_hero_transport(void) {
    ++scene.revision; scene.available_mask=0; scene.leader_seat=4; pump();
    SudekiMpLanPartyConfig regular_config={.story_observation=2};
    SudekiMpLanPartySession *regular=SudekiMpLanPartyCreate(&regular_config); assert(regular);
    assert(SudekiMpLanPartyStoryPolicy(regular)==SUDEKIMP_LAN_STORY_POLICY_REGULAR);
    assert(!SudekiMpLanPartyPublishStoryScene(regular,&scene));
    SudekiMpLanPartyDestroy(regular,FALSE);
    for(unsigned p=0;p<4u;++p) records[p].revision=scene.revision;
    all_peers(); /* The native HUD status stream is independent of hero slots. */
    BOOL received[4]={TRUE,FALSE,FALSE,FALSE},complete=FALSE;
    for(unsigned attempt=0;attempt<500u && !complete;++attempt) {
        Sleep(1); pump();
        SudekiMpLanStoryFrame frame={.epoch=scene.epoch,.revision=scene.revision,
            .host_tick=scene.observed_tick,.sequence=attempt+1u,.leader_character=4};
        SudekiMpLanStoryWorldFrame world={.epoch=scene.epoch,.revision=scene.revision,
            .host_tick=frame.host_tick,.sequence=frame.sequence,.count=4};
        for(unsigned p=0;p<4u;++p) world.actors[p]=(SudekiMpLanStoryWorldActor){
            .kind=SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND,.submodels=1,
            .identifier=SudekiMpLanStoryWorldAvatarIdentifier(p),.generation=p+1u,
            .animation_sequence=1,.bank_fingerprint=17,.forward={0,0,1}};
        assert(SudekiMpLanPartySendStoryFrame(nodes[0],&frame));
        assert(SudekiMpLanPartySendStoryWorld(nodes[0],&world)); pump();
        complete=TRUE;
        for(unsigned n=1;n<4u;++n) {
            SudekiMpLanPartyPeerStatus peer; SudekiMpLanStoryFrame got;
            SudekiMpLanStoryWorldFrame got_world;
            assert(SudekiMpLanPartyPeerStatusGet(nodes[n],n,&peer));
            if(!received[n] && SudekiMpLanPartyPopStoryFrame(nodes[n],&peer.lease,GetTickCount(),&got) &&
                SudekiMpLanPartyPopStoryWorld(nodes[n],&peer.lease,GetTickCount(),&got_world,NULL)) {
                assert(!got.available_mask && got.leader_character==4 && got_world.count==4);
                assert(SudekiMpLanStoryWorldFrameMatchesForPolicy(&got_world,&got,SUDEKIMP_LAN_STORY_POLICY_DEV_AVATARS));
                received[n]=TRUE;
            }
            complete=complete && received[n];
        }
    }
    assert(complete);
    ++scene.revision; scene.available_mask=15; scene.leader_seat=2; pump();
    for(unsigned p=0;p<4u;++p) records[p].revision=scene.revision;
    all_peers();
}
static void freshness_and_replacement(void) {
    SudekiMpLanStoryAvatarStatus got,old=records[2];
    assert(SudekiMpLanPartyGetAvatarStatus(nodes[1],2,GetTickCount(),&got));
    uint32_t received=got.received_tick;
    assert(!SudekiMpLanPartyGetAvatarStatus(nodes[1],2,received-1u,&got));
    assert(!SudekiMpLanPartyGetAvatarStatus(nodes[1],2,received+251u,&got));
    Sleep(2); assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&old)); pump();
    assert(SudekiMpLanPartyGetAvatarStatus(nodes[1],2,GetTickCount(),&got));
    assert(got.received_tick==received); /* identical retries never renew freshness */
    SudekiMpLanStoryAvatarStatus conflict=old; conflict.hp-=1;
    assert(!SudekiMpLanPartyPublishAvatarStatus(nodes[0],&conflict));
    ++records[2].spawn_generation; records[2].sequence=1; records[2].hp=5.5f;
    assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&records[2])); pump();
    assert(SudekiMpLanPartyGetAvatarStatus(nodes[1],2,GetTickCount(),&got) && got.hp==5.5f);
    old.sequence=1000; assert(!SudekiMpLanPartyPublishAvatarStatus(nodes[0],&old));
    records[2].present=0; ++records[2].sequence;
    records[2].hp=records[2].max_hp=records[2].sp=records[2].max_sp=0;
    memset(records[2].name,0,sizeof(records[2].name));
    assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&records[2])); pump();
    for(unsigned n=0;n<4u;++n) assert(!SudekiMpLanPartyGetAvatarStatus(nodes[n],2,GetTickCount(),&got));
    assert(!SudekiMpLanPartyPublishAvatarStatus(nodes[0],&old));
    ++scene.revision; strcpy(scene.temporary,"avatar_status_interior"); scene.inside_mask=1; pump();
    assert(!SudekiMpLanPartyGetAvatarStatus(nodes[0],0,GetTickCount(),&got));
    assert(!SudekiMpLanPartyPublishAvatarStatus(nodes[0],&records[0]));
    old.revision=scene.revision; assert(!SudekiMpLanPartyPublishAvatarStatus(nodes[0],&old));
    ++scene.epoch; ++scene.revision; pump();
    old.revision=scene.revision;
    old.epoch=scene.epoch; old.spawn_generation=1; old.sequence=1;
    assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&old)); /* new world has its own generation domain */
}
static void isolation_and_disconnect(void) {
    SudekiMpLanPartyConfig c={.story_observation=2,.assignment_enabled=1,.character={0,1,2,3}};
    SudekiMpLanPartySession *regular=SudekiMpLanPartyCreate(&c); assert(regular);
    assert(SudekiMpLanPartyPublishStoryScene(regular,&scene));
    SudekiMpLanStoryAvatarStatus v=sample(0),got;
    v.epoch=scene.epoch; v.revision=scene.revision;
    assert(!SudekiMpLanPartyPublishAvatarStatus(regular,&v));
    assert(!SudekiMpLanPartyGetAvatarStatus(regular,0,GetTickCount(),&got));
    SudekiMpLanPartyDestroy(regular,FALSE);
    c=(SudekiMpLanPartyConfig){.dev_play=1,.story_observation=2,.assignment_enabled=1,
        .reserved_mask=1,.character={0,4,4,4},.avatar={0,4,4,5},.lobby_members=1,.lobby_nonce={301,0,0,0}};
    SudekiMpLanPartySession *restricted=SudekiMpLanPartyCreate(&c); assert(restricted);
    assert(SudekiMpLanPartyPublishStoryScene(restricted,&scene));
    assert(SudekiMpLanPartyPublishAvatarStatus(restricted,&v)); /* first selected native hero */
    assert(SudekiMpLanPartyGetAvatarStatus(restricted,0,GetTickCount(),&got) && got.hp==v.hp);
    v.player=3; assert(!SudekiMpLanPartyPublishAvatarStatus(restricted,&v)); /* absent lobby member */
    SudekiMpLanPartyDestroy(restricted,FALSE);
    /* The second selection of hero 0 is an explicit spectator. */
    c.avatar[1]=0; c.lobby_members=3; c.lobby_nonce[1]=302;
    restricted=SudekiMpLanPartyCreate(&c); assert(restricted);
    assert(SudekiMpLanPartyPublishStoryScene(restricted,&scene));
    v.player=1; assert(!SudekiMpLanPartyPublishAvatarStatus(restricted,&v));
    SudekiMpLanPartyDestroy(restricted,FALSE);
    v.player=0; ++v.sequence;
    assert(SudekiMpLanPartyPublishAvatarStatus(nodes[0],&v)); pump();
    assert(SudekiMpLanPartyGetAvatarStatus(nodes[1],0,GetTickCount(),&got));
    assert(SudekiMpLanPartyDisconnect(nodes[1],&peers[1].lease)); pump();
    assert(!SudekiMpLanPartyGetAvatarStatus(nodes[1],0,GetTickCount(),&got));
    assert(!SudekiMpLanPartyGetAvatarStatus(nodes[0],1,GetTickCount(),&got));
    assert(SudekiMpLanPartyClientRejoin(nodes[1]));
    assert(!SudekiMpLanPartyGetAvatarStatus(nodes[1],0,GetTickCount(),&got));
    for(unsigned i=0;i<4u;++i) { SudekiMpLanPartyDestroy(nodes[i],FALSE); nodes[i]=NULL; }
}
int main(void) {
    codec(); setup(); all_peers(); zero_hero_transport(); freshness_and_replacement(); isolation_and_disconnect();
    puts("PASS: Dev Play avatar status codec, four-peer broadcast, host-local cache, generation/scene/freshness/tombstone rejection and regular-mode isolation");
    return 0;
}
