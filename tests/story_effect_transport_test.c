/* Real loopback UDP, including reordered/lost/corrupted authenticated packets.
 * This proves plain transport behavior, not native gameplay or visuals. */
#include <winsock2.h>
#include "network/lan_party_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static SudekiMpStoryVfx vfx_sample(uint32_t tick) {
    SudekiMpStoryVfx s={.epoch=1,.revision=1,.sequence=1,.stream=9,.tick=tick,
        .cast_sequence=2,.observed=1,.count=1,.current="castle_road"};
    s.entries[0]=(SudekiMpLanArenaSpiritVfxSnapshot){.instance_sequence=1,.skill_sequence=2,
        .kind=1,.emitted_host_tick=tick,.phase_valid=1,.phase=0.25f,
        .position={1,2,3},.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    return s;
}
static void vfx_codec(void) {
    SudekiMpStoryVfx s=vfx_sample(100),out,unchanged,bad;
    uint8_t bytes[SUDEKIMP_STORY_VFX_SIZE];
    assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
    assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
    unchanged=out;
    for(unsigned i=0;i<sizeof(bytes);++i) assert(!SudekiMpStoryVfxDecode(bytes,i,&out));
    assert(!memcmp(&unchanged,&out,sizeof(out)));
    bytes[0]=2; assert(!SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out));
    bad=s; bad.entries[0].position[0]=NAN; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.entries[0].owner_actor_type=0x23; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.entries[0].kind=14; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.entries[0].rotation_xyzw[3]=0; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.entries[0].emitted_host_tick=101; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.count=2; bad.entries[1]=bad.entries[0]; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.observed=0; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; bad.count=0; assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; memset(bad.current,'a',sizeof(bad.current)); assert(!SudekiMpStoryVfxValid(&bad));
    bad=s; memset(bad.entries,0,sizeof(bad.entries)); bad.count=0; bad.observed=0;
    assert(SudekiMpStoryVfxValid(&bad)); /* Unknown differs from an observed empty roster. */
    bad.observed=1; assert(SudekiMpStoryVfxValid(&bad));
}
static SudekiMpStoryVfx vfx_large(uint32_t tick,unsigned count) {
    SudekiMpStoryVfx s=vfx_sample(tick);s.count=(uint8_t)count;
    for(unsigned i=0;i<count;++i) {
        s.entries[i]=s.entries[0];s.entries[i].instance_sequence=i+1;
        s.entries[i].skill_sequence=0;s.entries[i].owner_actor_type=1;
        s.entries[i].kind=i<6?67:44;s.entries[i].position[0]=(float)i;
    }
    return s;
}
static void vfx_large_codec(void) {
    assert(SUDEKIMP_STORY_VFX_SIZE==528u && SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY==8u);
    for(unsigned n=0;n<=64;++n) {
        SudekiMpStoryVfx s=vfx_large(100,n),out={0};
        if(!n) memset(s.entries,0,sizeof(s.entries));
        uint8_t bytes[SUDEKIMP_STORY_VFX_CHUNK_MAX];size_t written;
        unsigned count=SudekiMpStoryVfxChunkCount(&s);
        SudekiMpStoryVfxChunk chunks[SUDEKIMP_STORY_VFX_CHUNKS];
        for(unsigned i=0;i<count;++i) {
            assert(SudekiMpStoryVfxChunkEncode(&s,i,bytes,sizeof(bytes),&written) && written+28u<=1468u);
            assert(SudekiMpStoryVfxChunkDecode(bytes,written,&chunks[count-i-1]));
            for(size_t j=0;j<written;++j) assert(!SudekiMpStoryVfxChunkDecode(bytes,j,&chunks[count-i-1]));
        }
        assert(SudekiMpStoryVfxAssemble(chunks,count,&out) && !memcmp(&s,&out,sizeof(s)));
        SudekiMpStoryVfx before=out;
        chunks[0].bytes[0]^=1;
        assert(!SudekiMpStoryVfxAssemble(chunks,count,&out) && !memcmp(&before,&out,sizeof(out)));
        chunks[0].bytes[0]^=1;
        if(count>1) {
            assert(!SudekiMpStoryVfxAssemble(chunks,count-1,&out));
            ++chunks[0].tick;assert(!SudekiMpStoryVfxAssemble(chunks,count,&out));--chunks[0].tick;
            chunks[0]=chunks[1];assert(!SudekiMpStoryVfxAssemble(chunks,count,&out));
        }
        if(n>8) assert(!SudekiMpStoryVfxEncode(&s,bytes,SUDEKIMP_STORY_VFX_SIZE));
    }
    SudekiMpStoryVfx bad=vfx_large(100,64);bad.entries[63].instance_sequence=1;
    assert(!SudekiMpStoryVfxValid(&bad));
    bad=vfx_large(100,11);bad.entries[63]=bad.entries[0];assert(!SudekiMpStoryVfxValid(&bad));
    bad=vfx_large(100,64);bad.count=65;assert(!SudekiMpStoryVfxValid(&bad));
}
static void put(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));}
static SOCKET endpoint(struct sockaddr_in *a) {
    SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); assert(s!=INVALID_SOCKET);
    memset(a,0,sizeof(*a)); a->sin_family=AF_INET; a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(!bind(s,(struct sockaddr *)a,sizeof(*a))); int n=sizeof(*a);
    assert(!getsockname(s,(struct sockaddr *)a,&n)); DWORD timeout=1000;
    assert(!setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(timeout))); return s;
}
static void send_packet(SOCKET s,const struct sockaddr_in *to,unsigned kind,unsigned version,
    uint32_t envelope,const void *body,size_t n) {
    uint8_t bytes[1468]={0}; assert(n+28u<=sizeof(bytes));
    memcpy(bytes,"SMP4",4); bytes[4]=(uint8_t)version; bytes[6]=(uint8_t)kind; bytes[7]=1;
    if(kind!=1u) {put(bytes+8,1);put(bytes+12,123);} put(bytes+20,envelope);
    bytes[24]=(uint8_t)n; bytes[25]=(uint8_t)(n>>8); if(n) memcpy(bytes+28,body,n);
    assert(sendto(s,(const char *)bytes,(int)(n+28u),0,(const struct sockaddr *)to,sizeof(*to))==(int)(n+28u));
}
static void vfx_fragment(SOCKET server,const struct sockaddr_in *to,const SudekiMpStoryVfx *s,
    unsigned chunk,unsigned envelope) {
    uint8_t bytes[SUDEKIMP_STORY_VFX_CHUNK_MAX];size_t size;
    assert(SudekiMpStoryVfxChunkEncode(s,chunk,bytes,sizeof(bytes),&size));
    send_packet(server,to,39,SUDEKIMP_LAN_PARTY_VERSION,envelope,bytes,size);
}
static void vfx_reordered_transport(SOCKET server,const struct sockaddr_in *to,
    SudekiMpLanPartySession *client,const SudekiMpLanPartyLease *lease,uint32_t now) {
    SudekiMpStoryVfx s=vfx_large(now,64),out;s.sequence=4;
    unsigned n=SudekiMpStoryVfxChunkCount(&s);
    for(unsigned i=n-1;i>0;--i) vfx_fragment(server,to,&s,i,300+i);
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,lease,now+3,&out) && out.sequence==3);
    vfx_fragment(server,to,&s,n-1,310); /* duplicate cannot finish or refresh */
    vfx_fragment(server,to,&s,0,299);
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,lease,now+3,&out) && out.count==64 && out.sequence==4);
    /* Four lost updates cannot occupy every assembly and stall the next. */
    for(unsigned seq=5;seq<=8;++seq) {s.sequence=seq;vfx_fragment(server,to,&s,0,320+seq);}
    SudekiMpLanPartyPoll(client,now+3);
    s.sequence=9;
    for(unsigned i=0;i<64;++i) s.entries[i].position[0]+=10;
    for(unsigned i=0;i<n;++i) vfx_fragment(server,to,&s,i,340+i);
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,lease,now+3,&out) && out.sequence==9 && out.entries[63].position[0]==73);
    s.sequence=8;for(unsigned i=0;i<n;++i) vfx_fragment(server,to,&s,i,350+i);
    s.sequence=10;s.stream=8;for(unsigned i=0;i<n;++i) vfx_fragment(server,to,&s,i,360+i);
    s.stream=9;s.epoch=2;for(unsigned i=0;i<n;++i) vfx_fragment(server,to,&s,i,370+i);
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,lease,now+3,&out) && out.sequence==9);
    s=vfx_large(now,11);s.sequence=11;vfx_fragment(server,to,&s,0,380);
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,lease,now+3,&out) && out.sequence==11 && out.count==11);
}
static SudekiMpStoryEffectState sample(void) {
    SudekiMpStoryEffectState s={.epoch=1,.revision=1,.sequence=1,.observed_tick=1,.current="castle_road",
        .group_count=1,.groups={{"ambush",1}},.light_valid=1,.light_target={0x3f800000,0x3f000000,0},
        .sky_dome_serial=1,.sky_dome="sky",.cluster_valid=1,.cluster_hash=91,.sector=12,.in_combat=1,.flag36=2,.suspended=1};
    s.sky[0].serial=7; s.sky[0].args[4]=0x3f800000; strcpy(s.vertical_fog,"fog");
    for(unsigned i=0;i<SUDEKIMP_STORY_DESPAWN_MAX;++i) s.despawned[i]=(SudekiMpStoryDespawn){i+1,100+i,i%5u};
    s.light[11]=(SudekiMpStoryLightCall){19,SUDEKIMP_STORY_LIGHT_COLOUR_ADJUST,{1,2,3,4}};
    for(unsigned i=0;i<SUDEKIMP_STORY_SIGIL_MAX;++i) s.sigils[i]=(SudekiMpStorySigilState){i+1,2,i+31,i%2u};
    s.music_flags=3; s.music_intensity=0x3f19999a; strcpy(s.music_score,"BS_IllumCountry");
    s.music_curve[0]=0x3ecccccd; s.music_curve[1]=s.music_curve[3]=0x3f19999a;
    s.music_curve[4]=0x42f00000; s.music_curve[5]=0x41a00000;
    s.drops[15]=(SudekiMpStoryDropRecord){1,31,919,2,{0x3f800000,0,0xbf800000},1};
    return s;
}
static void codec(void) {
    uint8_t bytes[SUDEKIMP_STORY_EFFECT_WIRE_SIZE],packet[SUDEKIMP_STORY_EFFECT_CHUNK_MAX]; size_t n;
    SudekiMpStoryEffectState s=sample(),out,unchanged;
    assert(SudekiMpStoryEffectStateEncode(&s,bytes,sizeof(bytes)));
    assert(SudekiMpStoryEffectStateDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
    unchanged=out;
    for(size_t i=0;i<sizeof(bytes);++i) assert(!SudekiMpStoryEffectStateDecode(bytes,i,&out));
    assert(!memcmp(&out,&unchanged,sizeof(out)));
    bytes[0]=1; assert(!SudekiMpStoryEffectStateDecode(bytes,sizeof(bytes),&out));
    SudekiMpStoryEffectChunk c[SUDEKIMP_STORY_EFFECT_CHUNKS];
    for(unsigned i=0;i<SUDEKIMP_STORY_EFFECT_CHUNKS;++i) {
        assert(SudekiMpStoryEffectChunkEncode(&s,i,packet,sizeof(packet),&n) && n+28u<=1468u);
        assert(SudekiMpStoryEffectChunkDecode(packet,n,&c[i]));
        for(size_t j=0;j<n;++j) assert(!SudekiMpStoryEffectChunkDecode(packet,j,&c[i]));
    }
    assert(SudekiMpStoryEffectAssemble(c,SUDEKIMP_STORY_EFFECT_CHUNKS,&out) && !memcmp(&s,&out,sizeof(s)));
    SudekiMpStoryEffectChunk swap=c[0]; c[0]=c[1]; c[1]=swap;
    assert(SudekiMpStoryEffectAssemble(c,SUDEKIMP_STORY_EFFECT_CHUNKS,&out));
    c[1].bytes[0]^=1; assert(!SudekiMpStoryEffectAssemble(c,SUDEKIMP_STORY_EFFECT_CHUNKS,&out)); c[1].bytes[0]^=1;
    c[1]=c[0]; assert(!SudekiMpStoryEffectAssemble(c,SUDEKIMP_STORY_EFFECT_CHUNKS,&out));
    out=s; out.sigils[1]=out.sigils[0]; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.sigils[1].ended=2; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.drops[0].position[2]=1; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.drops[15].position[2]=0x7fc00000; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.cluster_valid=0; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.groups[0].sequence=-1; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.light[0].kind=1; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.despawned[1].death_style=5; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_flags=2; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_flags=0; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_intensity=0x7fc00000; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_intensity=0xbf000000; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_intensity=0x3f800001; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; strcpy(out.music_score,"BS_../bad"); assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_score[39]='x'; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_curve[4]=0; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_curve[5]=0x7f800000; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.music_variation=2; assert(!SudekiMpStoryEffectStateValid(&out));
    out=s; out.sky[1].args[0]=1; assert(!SudekiMpStoryEffectStateValid(&out));
}
static void effect_packet(SOCKET server,const struct sockaddr_in *client,const SudekiMpStoryEffectState *s,unsigned part,uint32_t envelope) {
    uint8_t b[SUDEKIMP_STORY_EFFECT_CHUNK_MAX]; size_t n;
    assert(SudekiMpStoryEffectChunkEncode(s,part,b,sizeof(b),&n));
    send_packet(server,client,35,SUDEKIMP_LAN_PARTY_VERSION,envelope,b,n);
}
int main(void) {
    vfx_large_codec();
    codec(); vfx_codec(); WSADATA w; assert(!WSAStartup(MAKEWORD(2,2),&w));
    struct sockaddr_in address,source; SOCKET server=endpoint(&address);
    SudekiMpLanPartyConfig cfg={.story_observation=2,.local_seat=1,.timeout_ms=5000,.host_ipv4="127.0.0.1"};
    cfg.port=ntohs(address.sin_port); memset(cfg.game_hash,0x13,32);
    SudekiMpLanPartySession *client=SudekiMpLanPartyCreate(&cfg); assert(client);
    uint32_t now=GetTickCount(); SudekiMpLanPartyPoll(client,now);
    uint8_t hello[1468],ack[9]; int addrsize=sizeof(source);
    assert(recvfrom(server,(char *)hello,sizeof(hello),0,(struct sockaddr *)&source,&addrsize)==75);
    memcpy(ack,hello+28,8); ack[8]=SUDEKIMP_LAN_PARTY_OBSERVING;
    send_packet(server,&source,2,20,1,ack,sizeof(ack)); SudekiMpLanPartyPoll(client,now+1);
    SudekiMpLanPartyPeerStatus peer; assert(SudekiMpLanPartyPeerStatusGet(client,1,&peer));
    assert(peer.phase==SUDEKIMP_LAN_PARTY_JOINING); /* old host cannot admit new client */
    send_packet(server,&source,2,SUDEKIMP_LAN_PARTY_VERSION,1,ack,sizeof(ack)); SudekiMpLanPartyPoll(client,now+2);
    assert(SudekiMpLanPartyPeerStatusGet(client,1,&peer) && peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING);
    SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=15,.leader_seat=2,.world="fixture",.observed_tick=now};
    uint8_t scene_bytes[SUDEKIMP_LAN_STORY_WIRE_SIZE];
    assert(SudekiMpLanStorySceneEncode(&scene,scene_bytes,sizeof(scene_bytes)));
    send_packet(server,&source,19,SUDEKIMP_LAN_PARTY_VERSION,100,scene_bytes,sizeof(scene_bytes));
    SudekiMpStoryAreaState area={.sequence=1,.observed_tick=now,.current="castle_road"},actual;
    uint8_t area_bytes[SUDEKIMP_STORY_AREA_WIRE_SIZE];
    assert(SudekiMpStoryAreaStateEncode(&area,area_bytes,sizeof(area_bytes)));
    send_packet(server,&source,34,SUDEKIMP_LAN_PARTY_VERSION,2,area_bytes,sizeof(area_bytes));
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryArea(client,&peer.lease,now+3,&actual) && actual.sequence==1);
    SudekiMpStoryVfx vfx=vfx_sample(now),vfx_got;
    uint8_t vfx_bytes[SUDEKIMP_STORY_VFX_SIZE];
    assert(SudekiMpStoryVfxEncode(&vfx,vfx_bytes,sizeof(vfx_bytes)));
    send_packet(server,&source,36,SUDEKIMP_LAN_PARTY_VERSION,101,vfx_bytes,sizeof(vfx_bytes));
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,&peer.lease,now+3,&vfx_got) && vfx_got.count==1);
    SudekiMpLanPartyLease foreign=peer.lease; ++foreign.generation;
    assert(!SudekiMpLanPartyGetStoryVfx(client,&foreign,now+3,&vfx_got));
    /* Wrong authenticated sender cannot publish; high envelope numbers do
     * not override the VFX stream's own sequence/generation fence. */
    struct sockaddr_in forged_address; SOCKET forged=endpoint(&forged_address);
    vfx.sequence=2; vfx.stream=8;
    assert(SudekiMpStoryVfxEncode(&vfx,vfx_bytes,sizeof(vfx_bytes)));
    send_packet(server,&source,36,SUDEKIMP_LAN_PARTY_VERSION,999,vfx_bytes,sizeof(vfx_bytes));
    vfx.stream=9; assert(SudekiMpStoryVfxEncode(&vfx,vfx_bytes,sizeof(vfx_bytes)));
    send_packet(forged,&source,36,SUDEKIMP_LAN_PARTY_VERSION,1000,vfx_bytes,sizeof(vfx_bytes));
    send_packet(server,&source,36,SUDEKIMP_LAN_PARTY_VERSION,1001,vfx_bytes,sizeof(vfx_bytes)-1);
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,&peer.lease,now+3,&vfx_got) && vfx_got.sequence==1);
    closesocket(forged);
    memset(vfx.entries,0,sizeof(vfx.entries)); vfx.observed=vfx.count=0;
    assert(SudekiMpStoryVfxEncode(&vfx,vfx_bytes,sizeof(vfx_bytes)));
    send_packet(server,&source,36,SUDEKIMP_LAN_PARTY_VERSION,102,vfx_bytes,sizeof(vfx_bytes));
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,&peer.lease,now+3,&vfx_got) && !vfx_got.observed);
    vfx.sequence=3; vfx.observed=1;
    assert(SudekiMpStoryVfxEncode(&vfx,vfx_bytes,sizeof(vfx_bytes)));
    send_packet(server,&source,36,SUDEKIMP_LAN_PARTY_VERSION,103,vfx_bytes,sizeof(vfx_bytes));
    SudekiMpLanPartyPoll(client,now+3);
    assert(SudekiMpLanPartyGetStoryVfx(client,&peer.lease,now+3,&vfx_got) && vfx_got.observed && !vfx_got.count);
    vfx_reordered_transport(server,&source,client,&peer.lease,now);
    SudekiMpStoryEffectState s=sample(),got; s.observed_tick=now;
    effect_packet(server,&source,&s,1,4); SudekiMpLanPartyPoll(client,now+4);
    assert(!SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+4,&got)); /* missing chunk */
    effect_packet(server,&source,&s,1,5); effect_packet(server,&source,&s,0,3); SudekiMpLanPartyPoll(client,now+5);
    assert(SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+5,&got) && !memcmp(&got,&s,sizeof(s)));
    /* Incomplete newer record cannot publish; older complete record remains usable. */
    s.sequence=2; s.sigils[0].ended=1; effect_packet(server,&source,&s,0,6);
    SudekiMpLanPartyPoll(client,now+6); assert(SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+6,&got) && got.sequence==1);
    s.sequence=3; effect_packet(server,&source,&s,1,8); effect_packet(server,&source,&s,0,7);
    SudekiMpLanPartyPoll(client,now+7); assert(SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+7,&got) && got.sequence==3);
    s.sequence=2; effect_packet(server,&source,&s,1,200); effect_packet(server,&source,&s,0,201);
    area.sequence=2; strcpy(area.current,"brightwater"); assert(SudekiMpStoryAreaStateEncode(&area,area_bytes,sizeof(area_bytes)));
    send_packet(server,&source,34,SUDEKIMP_LAN_PARTY_VERSION,9,area_bytes,sizeof(area_bytes));
    SudekiMpLanPartyPoll(client,now+8);
    assert(SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+8,&got) && got.sequence==3);
    assert(SudekiMpLanPartyGetStoryArea(client,&peer.lease,now+8,&actual) && !strcmp(actual.current,"brightwater"));
    area.sequence=1; strcpy(area.current,"castle_road"); assert(SudekiMpStoryAreaStateEncode(&area,area_bytes,sizeof(area_bytes)));
    send_packet(server,&source,34,SUDEKIMP_LAN_PARTY_VERSION,300,area_bytes,sizeof(area_bytes));
    SudekiMpLanPartyPoll(client,now+9);
    assert(SudekiMpLanPartyGetStoryArea(client,&peer.lease,now+9,&actual) && actual.sequence==2);
    /* Scene fence clears assemblies and complete effects without suppressing areas. */
    ++scene.epoch; ++scene.revision; scene.observed_tick+=10u; strcpy(scene.world,"next_fixture"); assert(SudekiMpLanStorySceneEncode(&scene,scene_bytes,sizeof(scene_bytes)));
    send_packet(server,&source,19,SUDEKIMP_LAN_PARTY_VERSION,301,scene_bytes,sizeof(scene_bytes));
    s.sequence=4; effect_packet(server,&source,&s,0,302); effect_packet(server,&source,&s,1,303);
    SudekiMpLanPartyPoll(client,now+10);
    assert(!SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+10,&got));
    assert(!SudekiMpLanPartyGetStoryVfx(client,&peer.lease,now+10,&vfx_got));
    assert(SudekiMpLanPartyGetStoryArea(client,&peer.lease,now+10,&actual));
    s.epoch=scene.epoch; s.revision=scene.revision; s.observed_tick=scene.observed_tick;
    effect_packet(server,&source,&s,0,304); effect_packet(server,&source,&s,1,305);
    SudekiMpLanPartyPoll(client,now+11);
    assert(SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+11,&got) && got.sequence==4);
    effect_packet(server,&source,&s,0,306); effect_packet(server,&source,&s,1,307);
    SudekiMpLanPartyPoll(client,now+1012);
    assert(!SudekiMpLanPartyGetStoryEffects(client,&peer.lease,now+1012,&got)); /* duplicate cannot refresh */
    assert(!SudekiMpLanPartyPublishStoryEffects(client,&s));
    assert(!SudekiMpLanPartyPublishStoryVfx(client,&vfx));
    assert(!SudekiMpLanPartyPublishStoryArea(client,&area));
    /* New host rejects an old client hello at the receiving boundary. */
    cfg.local_seat=0; cfg.port=0; cfg.host_ipv4=NULL;
    SudekiMpLanPartySession *host=SudekiMpLanPartyCreate(&cfg); assert(host);
    struct sockaddr_in hostaddr=address; hostaddr.sin_port=htons(SudekiMpLanPartyPort(host));
    send_packet(server,&hostaddr,1,20,0,hello+28,47); SudekiMpLanPartyPoll(host,now);
    uint8_t response[1468];
    /* Drain the new client's authenticated keepalive before reading rejection. */
    BOOL rejected=FALSE;
    for(unsigned i=0;i<12 && !rejected;++i) {
        addrsize=sizeof(address); int n=recvfrom(server,(char *)response,sizeof(response),0,(struct sockaddr *)&address,&addrsize);
        assert(n>=28); rejected=response[6]==3 && n==37 && response[36]==SUDEKIMP_LAN_ARENA_REJECT_VERSION;
    }
    assert(rejected);
    /* Public publisher integration, in addition to the raw receiving tests. */
    cfg.local_seat=1; cfg.port=SudekiMpLanPartyPort(host); cfg.host_ipv4="127.0.0.1";
    SudekiMpLanPartySession *second=SudekiMpLanPartyCreate(&cfg); assert(second);
    BOOL ready=FALSE; uint32_t last_scene_tick=0; SudekiMpLanPartyPeerStatus remote,local;
    for(unsigned i=0;i<200 && !ready;++i) {
        uint32_t t=GetTickCount(); scene.observed_tick=t;
        /* GetTickCount may repeat across Sleep(5). Publication requires a
         * strictly advancing observation; polling does not. */
        if(!last_scene_tick || t!=last_scene_tick) {
            assert(SudekiMpLanPartyPublishStoryScene(host,&scene)); last_scene_tick=t;
        }
        SudekiMpLanPartyPoll(host,t); SudekiMpLanPartyPoll(second,t);
        assert(SudekiMpLanPartyPeerStatusGet(host,1,&remote));
        assert(SudekiMpLanPartyPeerStatusGet(second,1,&local));
        SudekiMpLanStoryScene seen;
        ready=remote.transport_confirmed && local.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
            SudekiMpLanPartyGetStoryScene(second,&local.lease,t,&seen);
        if(!ready) Sleep(5);
    }
    assert(ready); area.sequence=0; s.sequence=0; s.observed_tick=GetTickCount(); area.observed_tick=s.observed_tick;
    assert(SudekiMpLanPartyPublishStoryArea(host,&area)); assert(SudekiMpLanPartyPublishStoryEffects(host,&s));
    SudekiMpLanPartyPoll(second,GetTickCount());
    assert(SudekiMpLanPartyGetStoryArea(second,&local.lease,GetTickCount(),&actual) && actual.sequence==1);
    assert(SudekiMpLanPartyGetStoryEffects(second,&local.lease,GetTickCount(),&got) && got.sequence==1 &&
        got.sigils[0].ended==s.sigils[0].ended && !strcmp(got.current,s.current));
    vfx=vfx_sample(GetTickCount());
    assert(SudekiMpLanPartyPublishStoryVfx(host,&vfx)); SudekiMpLanPartyPoll(second,GetTickCount());
    assert(SudekiMpLanPartyGetStoryVfx(second,&local.lease,GetTickCount(),&vfx_got) &&
        vfx_got.sequence==1 && vfx_got.epoch==scene.epoch && vfx_got.count==1);
    for(unsigned count=11;count<=64;count+=53) {
        vfx=vfx_large(GetTickCount(),count);
        assert(SudekiMpLanPartyPublishStoryVfx(host,&vfx));SudekiMpLanPartyPoll(second,GetTickCount());
        assert(SudekiMpLanPartyGetStoryVfx(second,&local.lease,GetTickCount(),&vfx_got) && vfx_got.count==count);
    }
    vfx=vfx_sample(GetTickCount());vfx.count=0;memset(vfx.entries,0,sizeof(vfx.entries));
    assert(SudekiMpLanPartyPublishStoryVfx(host,&vfx));SudekiMpLanPartyPoll(second,GetTickCount());
    assert(SudekiMpLanPartyGetStoryVfx(second,&local.lease,GetTickCount(),&vfx_got) && !vfx_got.count);
    assert(!SudekiMpLanPartyGetStoryVfx(second,&local.lease,GetTickCount()+2000,&vfx_got));
    SudekiMpLanPartyDestroy(second,FALSE);
    SudekiMpLanPartyDestroy(host,FALSE); SudekiMpLanPartyDestroy(client,FALSE); closesocket(server); WSACleanup();
    puts("story effect codec and UDP transport: atomic/reordered/lost/stale/version/scene-fence checks passed");
    return 0;
}
