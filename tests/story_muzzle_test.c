#include "hooks/lan_story_muzzle.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static SudekiMpStoryVfx source;
static SudekiMpLanStoryFrame frame;
static SudekiMpStoryMuzzle tips[4];
static SudekiMpStoryMuzzleProjection cache;
static uint64_t prepared;
static void seed(unsigned character) {
    memset(&cache,0,sizeof(cache));memset(&frame,0,sizeof(frame));
    memset(tips,0,sizeof(tips));memset(&source,0,sizeof(source));
    source=(SudekiMpStoryVfx){.epoch=4,.revision=2,.stream=9,.tick=1000,.sequence=1,.observed=1,.count=3};
    frame.epoch=4;frame.revision=2;frame.available_mask=(uint8_t)(1u<<character);
    uint8_t item=character==3u?12u:24u,type=character==3u?1u:0x0eu;
    frame.actors[character]=(SudekiMpLanStoryActor){.generation=17,.character=(uint8_t)character,
        .weapon_item_plus_one=(uint8_t)(item+1u)};
    frame.actors[character].shots=(SudekiMpLanStoryShots){.latest=1,.count=1,
        .events={{.sequence=1,.host_tick=1000,.item=item,.origin={10,2,30},.direction={0,0,1}}}};
    tips[character]=(SudekiMpStoryMuzzle){.valid=TRUE,.item=item,.position={10,1,30}};
    for(unsigned i=0;i<3;++i) source.entries[i]=(SudekiMpLanArenaSpiritVfxSnapshot){
        .instance_sequence=i+1,.emitted_host_tick=1000,.owner_actor_type=type,
        .phase_valid=1,.phase=.25f,.position={10,2,30},.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    source.entries[0].kind=character==3u?35u:53u;
    source.entries[1].kind=character==3u?44u:59u;
    source.entries[2].kind=character==3u?40u:58u;
}
static SudekiMpStoryVfx project(void) {
    SudekiMpStoryVfx copy=source;
    prepared=SudekiMpStoryMuzzleProject(&cache,&copy,&frame,tips);
    return copy;
}
static void launch(unsigned c) {
    seed(c);SudekiMpStoryVfx before=source,out=project();
    assert(prepared==7u);
    assert(out.entries[0].position[1]==1 && out.entries[1].position[1]==1);
    assert(!memcmp(&out.entries[2],&before.entries[2],sizeof(out.entries[2])));
    for(unsigned i=0;i<2;++i) {
        memcpy(out.entries[i].position,before.entries[i].position,sizeof(out.entries[i].position));
        assert(!memcmp(&out.entries[i],&before.entries[i],sizeof(out.entries[i])));
    }
    assert(!memcmp(&before,&source,sizeof(source))); /* Original wire observation is immutable. */
    source.tick+=16;source.entries[1].position[2]+=1; tips[c].position[0]+=1;
    out=project();assert(out.entries[0].position[0]==11 && out.entries[1].position[0]==10);
    assert(fabsf(out.entries[1].position[1]-1.5f)<.0001f);
    source.entries[1].position[2]+=1;out=project();
    assert(!memcmp(&out.entries[1],&source.entries[1],sizeof(out.entries[1])));
    /* A later reversed path/repeated sample cannot turn the displacement back on. */
    source.entries[1].position[2]-=1;out=project();assert(out.entries[1].position[1]==2);
    /* Missing geometry cannot manufacture a zero vector; existing projectile
     * trajectory correction is plain data and needs no retained native owner. */
    tips[c].valid=FALSE;out=project();assert(out.entries[0].position[1]==1 && prepared==7u);
}
static void fences(void) {
    SudekiMpStoryVfx out;
    seed(3);frame.actors[3].shots.count=0;out=project();assert(!memcmp(&out,&source,sizeof(out)) && prepared==4u);
    seed(3);tips[3].valid=FALSE;out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);tips[3].item=13;out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);tips[3].position[0]+=10;out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);source.tick+=151;out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);frame.revision++;out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);source.entries[0].skill_sequence=3;out=project();assert(out.entries[0].position[1]==2);
    seed(3);source.entries[0].owner_actor_type=0x0e;out=project();assert(out.entries[0].position[1]==2);
    seed(3);(void)project();frame.actors[3].generation++;frame.actors[3].shots.count=0;
    out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);(void)project();source.stream++;frame.actors[3].shots.count=0;
    out=project();assert(!memcmp(&out,&source,sizeof(out)));
    seed(3);(void)project();source.count=0;(void)project();
    for(unsigned i=0;i<SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY;++i) assert(!cache.entries[i].instance);
    /* Host tick wrap and a flash born just before the fire-direction observer. */
    seed(3);source.tick=5;for(unsigned i=0;i<3;++i)source.entries[i].emitted_host_tick=UINT32_MAX-5;
    frame.actors[3].shots.events[0].host_tick=0;out=project();assert(out.entries[0].position[1]==1);
}
static void delayed_journal(void) {
    for(unsigned c=1;c<4;c+=2) {
        seed(c);SudekiMpLanStoryShots journal=frame.actors[c].shots;
        frame.actors[c].shots.count=0;
        SudekiMpStoryVfx copy=project();
        assert(prepared==4 && !memcmp(&copy,&source,sizeof(copy)));
        for(unsigned i=0;i<8;++i) assert(!cache.entries[i].instance);
        /* Same VFX sequence retried after the party packet: no new effect
         * identity, phase reset, or uncorrected initial spawn is needed. */
        frame.actors[c].shots=journal;copy=project();
        assert(prepared==7 && copy.entries[0].position[1]==1 && copy.entries[1].position[1]==1);
        tips[c].valid=FALSE;frame.actors[c].shots.count=0;copy=project();
        assert(prepared==7 && copy.entries[0].position[1]==1 && copy.entries[1].position[1]==1);
        frame.revision++;copy=project();assert(prepared==4);
    }
    seed(3);source.entries[0].skill_sequence=4;source.entries[1].owner_actor_type=0x23;
    assert(SudekiMpStoryMuzzleUnchangedMask(&source)==7);
    /* Wall impact is already at its authoritative contact point. It must
     * never wait for a muzzle or move toward the current weapon tip. */
    seed(3);source.entries[0].kind=SUDEKIMP_LAN_STORY_VFX_MAGIC_WALL_HIT;
    tips[3].valid=FALSE;frame.actors[3].shots.count=0;
    SudekiMpStoryVfx copy=project();
    assert((prepared&1u) && !memcmp(&copy.entries[0],&source.entries[0],sizeof(copy.entries[0])));
}
static void large_roster(void) {
    seed(3);source.count=64;
    for(unsigned i=0;i<64;++i) {source.entries[i]=source.entries[1];source.entries[i].instance_sequence=i+1;}
    SudekiMpStoryVfx copy=project();assert(prepared==UINT64_MAX);
    assert(copy.entries[63].position[1]==1 && cache.entries[63].instance==64);
    source.tick+=1000;frame.actors[3].shots.count=0;
    for(unsigned i=0;i<64;++i) source.entries[i].position[2]+=3;
    copy=project();assert(prepared==UINT64_MAX && copy.entries[63].position[1]==2);
    source.count=0;memset(source.entries,0,sizeof(source.entries));copy=project();
    for(unsigned i=0;i<64;++i) assert(!cache.entries[i].instance);
}
int main(void) {large_roster();launch(3);launch(1);fences();delayed_journal();puts("story muzzle projection tests passed");return 0;}
