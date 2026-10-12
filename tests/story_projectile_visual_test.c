/* Synthetic owned native-layout objects and a fake world-matrix getter.
 * Tests lifetime/identity/publication policy, not native rendering. */
#include "hooks/lan_party_projectile_lifetime.h"
#include "../src/hooks/lan_arena_spirit_visual_host.c"
#include <assert.h>
#include <stdio.h>
static SudekiMpLanPartyProjectileObservation observation;
static SudekiMpLanPartyProjectileObservation extra[64];
static BOOL observation_ok=TRUE;
static unsigned observation_count;
void SudekiMpLogFormat(const char *fmt,...) {(void)fmt;}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {(void)actor;return NULL;}
BOOL SudekiMpLanPartyProjectileLifetimeObserve(SudekiMpLanPartyProjectileObservation *out,unsigned cap,unsigned *n) {
    if(!observation_ok || cap<observation_count) return FALSE;
    *n=observation_count;if(*n)out[0]=observation;
    for(unsigned i=1;i<*n;++i) out[i]=extra[i];
    return TRUE;
}
static const float *__attribute__((thiscall)) matrix_get(void *p) {return *(const float **)((uint8_t *)p+0x8c);}
int main(void) {
    image=(HMODULE)VirtualAlloc(NULL,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(image);
    uint8_t *base=(uint8_t *)image,*jump=base+RVA_WORLD_MATRIX;jump[0]=0xe9;
    int32_t displacement=(int32_t)((uintptr_t)matrix_get-(uintptr_t)(jump+5));memcpy(jump+1,&displacement,4);
    uint8_t entity[0x100]={0},missile[0x60]={0},record[0x40]={0},position[0x104]={0};
    uint8_t wrapper[0x18]={0},renderer[0x9c]={0},render[0xd0]={0},model[0x20]={0},parts[0x10]={0};
    uint8_t bank[4]={0},channels[24]={0},actor[4]={0};float matrix[68]={0};
    matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[12]=10;matrix[13]=2;matrix[14]=3;
    *(void **)entity=base+0x2d5a10;*(void **)(entity+0xfc)=missile;*(void **)(entity+0x44)=position;
    *(void **)missile=base+0x2d915c;*(void **)(missile+0x10)=entity;*(void **)(missile+0x3c)=record;
    unsigned kind=0;for(unsigned i=35;i<=SUDEKIMP_LAN_STORY_VFX_AILISH_SHOT_LAST;++i)
        if(!strcmp(resource_names[i],"SFXAP001_ROYAL_SCEPTRE"))kind=i;
    assert(kind);*(uint32_t *)(record+0x24)=0x29;*(uint32_t *)(record+0x28)=resource_ids[kind];
    *(void **)position=base+RVA_POSITION_VTABLE;*(void **)(position+0x10)=entity;
    *(void **)(position+0x8c)=matrix;*(void **)(position+0xb4)=wrapper;
    *(void **)(wrapper+8)=render;*(void **)(wrapper+0xc)=renderer;*(void **)(wrapper+0x10)=renderer;
    *(void **)(render+0x14)=renderer;*(void **)renderer=base+RVA_RENDERER_VTABLE;
    *(void **)(renderer+8)=model;*(void **)(renderer+0x98)=bank;*(void **)bank=channels;
    *(void **)(model+0x1c)=parts;*(uint32_t *)(parts+0xc)=1;*(float *)(channels+8)=4;
    game_thread=GetCurrentThreadId();registry.session=7;registry.next_instance=20;
    observation=(SudekiMpLanPartyProjectileObservation){.owner={7,8,actor,1,0},.incarnation=1,
        .entity=entity,.missile=missile};observation_count=1;
    SudekiMpStoryVfx s={.tick=100,.observed=1},before;
    assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s));
    assert(s.count==1 && s.entries[0].kind==kind && s.entries[0].instance_sequence==21 &&
        s.entries[0].position[0]==10 && s.entries[0].phase==4 && s.entries[0].owner_actor_type==1);
    assert(SudekiMpLanStoryCastVfxEntriesValid(s.tick,s.cast_sequence,s.observed,s.count,s.entries,SUDEKIMP_STORY_VFX_CAPACITY));
    s.count=0;memset(s.entries,0,sizeof(s.entries));s.tick=110;matrix[12]=12;
    assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s));
    assert(s.entries[0].instance_sequence==21 && s.entries[0].emitted_host_tick==100 && s.entries[0].position[0]==12);
    before=s;observation_ok=FALSE;
    assert(!SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && !memcmp(&s,&before,sizeof(s)));observation_ok=TRUE;
    s.count=0;memset(s.entries,0,sizeof(s.entries));observation.termination_entered=TRUE;
    assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && !s.count);
    observation.termination_entered=FALSE;observation.incarnation=2;
    assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && s.entries[0].instance_sequence==22);
    s.count=0;memset(s.entries,0,sizeof(s.entries));before=s;
    *(void **)(position+0x10)=NULL;
    assert(!SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && !memcmp(&s,&before,sizeof(s)));
    *(void **)(position+0x10)=entity;observation.owner.session=8;
    assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && !s.count);
    observation.owner.session=7;observation.owner.actor_type=0x0e;before=s;
    assert(!SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,actor,NULL,NULL},&s) && !memcmp(&s,&before,sizeof(s)));
    /* Six wall impacts plus five in-flight shots reproduced the live
     * overflow. Synthetic native owners have separate entities/components. */
    observation.owner.actor_type=1;observation.incarnation=10;
    uint8_t entities[64][0x100],missile_copies[64][0x60],positions[64][0x104];
    for(unsigned i=1;i<64;++i) {
        memcpy(entities[i],entity,sizeof(entity));memcpy(missile_copies[i],missile,sizeof(missile));
        memcpy(positions[i],position,sizeof(position));
        *(void **)(entities[i]+0xfc)=missile_copies[i];*(void **)(entities[i]+0x44)=positions[i];
        *(void **)(missile_copies[i]+0x10)=entities[i];*(void **)(positions[i]+0x10)=entities[i];
        extra[i]=observation;extra[i].incarnation=10+i;extra[i].entity=entities[i];extra[i].missile=missile_copies[i];
    }
    observation_count=5;
    SudekiMpStoryVfx effects={.tick=200,.observed=1,.count=6};
    for(unsigned i=0;i<6;++i) effects.entries[i]=(SudekiMpLanArenaSpiritVfxSnapshot){
        .instance_sequence=i+1,.kind=67,.owner_actor_type=1,.emitted_host_tick=100,
        .rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    for(unsigned step=0;step<30;++step) {
        s=effects;s.tick+=step*16;matrix[12]=20+(float)step;
        assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && s.count==11);
        assert(SudekiMpLanStoryCastVfxEntriesValid(s.tick,0,1,s.count,s.entries,64));
        for(unsigned i=6;i<11;++i) assert(s.entries[i].position[0]==matrix[12]);
    }
    observation_count=64;s=effects;before=s;
    assert(!SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && !memcmp(&s,&before,sizeof(s)));
    observation_count=0;s=effects;
    assert(SudekiMpLanStoryProjectileVisuals(7,(void *const[4]){NULL,NULL,NULL,actor},&s) && s.count==6);
    /* Ailish, Elco and Tal skill missiles retain their own cast sequences,
     * including a late sample after a different caster's newer cast. */
    const unsigned kinds[]={SUDEKIMP_LAN_STORY_VFX_SFXAS007_FIREBALL_PROJECTILE,
        SUDEKIMP_LAN_STORY_VFX_SFXES005_PLASMATICA_PROJECTILE,
        SUDEKIMP_LAN_STORY_VFX_SFXTS010_BLADE_SPARK_PROJECTILE};
    const unsigned chars[]={3,1,2},types[]={1,14,35};
    observation_count=1;
    for(unsigned i=0;i<3;++i) {
        void *roster[4]={0};roster[chars[i]]=actor;
        observation.owner.actor_type=types[i];observation.owner.cast_sequence=41+i;
        observation.incarnation=200+i;
        *(uint32_t *)(record+0x28)=resource_ids[kinds[i]];
        s=(SudekiMpStoryVfx){.tick=1000,.cast_sequence=100,.observed=1};
        assert(SudekiMpLanStoryProjectileVisuals(7,roster,&s) && s.count==1);
        assert(s.entries[0].kind==kinds[i] && s.entries[0].skill_sequence==41+i &&
            s.entries[0].owner_actor_type==(i==2?0:types[i]));
        assert(SudekiMpLanStoryCastVfxEntriesValid(s.tick,s.cast_sequence,1,1,s.entries,64));
    }
    VirtualFree(image,0,MEM_RELEASE);
    puts("PASS owned projectile transform/phase, monotonic shared IDs, retirement/address reuse, unknown and foreign-owner rejection");return 0;
}
