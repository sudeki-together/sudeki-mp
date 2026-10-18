#include "hooks/lan_story_muzzle.h"
#include "hooks/story_aimed_flash.h"
#include <math.h>
#include <string.h>

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
BOOL SudekiMpStoryMuzzleImageMatches(HMODULE image) {
    const uint8_t *b=(const uint8_t *)image;
    /* CModelInstance::locator(+20), not the static bind locator (+24).
     * These leaves have no relocated operands. No method is invoked here. */
    static const uint8_t locator[]={0x8b,0x44,0x24,0x04,0x8b,0x51,0x08,
        0x8b,0x52,0x50,0x8d,0x04,0x80,0x03,0xc0,0x8b,0x04,0xc2,
        0xc1,0xe0,0x06,0x03,0x41,0x0c};
    static const uint8_t origin[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x8b,0x45,0x08,
        0x8b,0x48,0x10,0x8b,0x81,0xc0,0x00,0x00,0x00,0x83,0xec,0x48,0x53,
        0x56,0x85,0xc0,0x74,0x43,0x8b,0x90,0x60,0x02,0x00,0x00,0x83,0xfa,0xff};
    return b && readable(b+0x21bd20u,sizeof(locator)) &&
        readable(b+0xc79c0u,sizeof(origin)) &&
        !memcmp(b+0x21bd20u,locator,sizeof(locator)) && !memcmp(b+0xc79c0u,origin,sizeof(origin));
}
static BOOL matrix(const float *m) {
    if(!readable(m,64u)) return FALSE;
    for(unsigned i=0;i<16u;++i) if(!isfinite(m[i])) return FALSE;
    return fabsf(m[3])<.0001f && fabsf(m[7])<.0001f && fabsf(m[11])<.0001f &&
        fabsf(m[15]-1.0f)<.0001f;
}
static BOOL renderer(uint8_t *b,uint8_t *wrapper,uint8_t **object,uint8_t **instance) {
    if(!readable(wrapper,0x14u)) return FALSE;
    uint8_t *o=*(uint8_t **)(wrapper+8u),*r=*(uint8_t **)(wrapper+0x10u);
    if(!readable(o,0xd0u) || *(void **)o!=b+0x2dd700u ||
        !readable(r,0x10u) || *(void **)r!=b+0x2df8ecu ||
        *(void **)(wrapper+0xcu)!=r || *(void **)(o+0x14u)!=r) return FALSE;
    *object=o; *instance=r; return TRUE;
}
static const float *locator(uint8_t *r,unsigned index,BOOL sfx) {
    uint8_t *bank=*(uint8_t **)(r+8u),*header,*records,*names;
    if(!readable(bank,0x54u)) return NULL;
    header=*(uint8_t **)(bank+0x1cu); records=*(uint8_t **)(bank+0x50u);
    if(!readable(header,0x18u)) return NULL;
    unsigned count=*(unsigned *)(header+0x14u),nodes=*(unsigned *)(header+4u);
    if(!count || count>256u || index>=count || !nodes || nodes>4096u ||
        !readable(records,count*0x50u)) return NULL;
    unsigned bone=*(unsigned *)(records+index*0x50u);
    if(bone>=nodes) return NULL;
    if(sfx) {
        uint8_t *groups=*(uint8_t **)(bank+0x24u);
        if(!readable(groups,4u)) return NULL;
        names=*(uint8_t **)groups;
        if(!readable(names,(bone+1u)*8u) || *(uint32_t *)(names+bone*8u)!=124205u) return NULL;
    }
    const float *poses=*(const float **)(r+0xcu);
    if(!readable(poses,nodes*64u)) return NULL;
    const float *m=poses+bone*16u; return matrix(m)?m:NULL;
}
static void transform(float p[3],const float m[16]) {
    float result[3];
    for(unsigned i=0;i<3u;++i) result[i]=p[0]*m[i]+p[1]*m[4u+i]+p[2]*m[8u+i]+m[12u+i];
    memcpy(p,result,sizeof(result));
}
BOOL SudekiMpStoryMuzzleObserve(HMODULE image,void *actor,uint8_t type,SudekiMpStoryMuzzle *out) {
    uint8_t *b=(uint8_t *)image,*a=actor,*weapon,*position,*model,*body,*br,*gun,*gr;
    if(!out || !SudekiMpStoryMuzzleImageMatches(image) || (type!=1u && type!=0x0eu) ||
        !readable(a,0x138u) || *(void **)a!=b+(type==1u?0x2d555cu:0x2d66fcu)) return FALSE;
    weapon=*(uint8_t **)(a+0xc0u);position=*(uint8_t **)(a+0x44u);model=*(uint8_t **)(a+0x134u);
    if(!readable(weapon,0x270u) || *(void **)weapon!=b+0x2d4d3cu ||
        *(void **)(weapon+0x10u)!=a || *(void **)(weapon+0x26cu) ||
        !readable(position,0xb8u) || *(void **)position!=b+0x2cdefcu ||
        *(void **)(position+0x10u)!=a || !readable(model,0x168u) ||
        *(void **)model!=b+0x2d5464u || *(void **)(model+0x10u)!=a ||
        *(void **)(weapon+0xd4u)!=position+4u) return FALSE;
    uint8_t *item=*(uint8_t **)(weapon+0x268u);
    if(!readable(item,0x18u) || !readable(b+0x408d80u,4u)) return FALSE;
    unsigned id=*(unsigned *)(item+0x14u),first=type==1u?12u:24u;
    uint8_t *db=*(uint8_t **)(b+0x408d80u);
    if(id<first || id>=first+12u || !readable(db,12u+(id+1u)*4u) ||
        *(void **)(db+12u+id*4u)!=item) return FALSE;
    uint8_t *bw=*(uint8_t **)(position+0xb4u),*gw=*(uint8_t **)(weapon+0xf4u);
    if(bw==*(void **)(model+0x160u) ||
        (*(void **)(model+0x164u) && bw!=*(void **)(model+0x164u)) ||
        !renderer(b,bw,&body,&br) || !renderer(b,gw,&gun,&gr) ||
        *(void **)(body+0x18u) || *(void **)(gun+0x18u)!=body ||
        !matrix((float *)(body+0x90u)) || !matrix((float *)(gun+0x90u))) return FALSE;
    const float *tip=locator(gr,*(unsigned *)(weapon+0x260u),TRUE);
    const float *hand=locator(br,*(uint16_t *)(gun+0x20u),FALSE);
    if(!tip || !hand) return FALSE;
    /* Retail 5d60d0/5d5fd0: locator * local weapon * parent locator * body.
     * Compose the already rendered pose without refreshing any native model
     * or reading a stale cached weapon world matrix after actor movement. */
    SudekiMpStoryMuzzle value={.valid=TRUE,.item=(uint8_t)id};
    memcpy(value.position,tip+12u,sizeof(value.position));
    transform(value.position,(float *)(gun+0x90u));transform(value.position,hand);
    transform(value.position,(float *)(body+0x90u));
    for(unsigned i=0;i<3u;++i) if(!isfinite(value.position[i])) return FALSE;
    *out=value; return TRUE;
}

static unsigned visual_character(const SudekiMpLanArenaSpiritVfxSnapshot *v,BOOL *flash) {
    if(v->skill_sequence) return 4u;
    *flash=FALSE;
    /* Flashes without locator follow stay where they were fired (native
     * 0x4C7D20): anchored once at this client's tip like a bolt, not tracked. */
    if(SudekiMpAimedFlashKind(v->owner_actor_type,v->kind,v->skill_sequence))
        return v->owner_actor_type==1u?3u:1u;
    if(v->owner_actor_type==1u) {
        if(v->kind>=35u && v->kind<=39u) {*flash=TRUE;return 3u;}
        if(v->kind>=43u && v->kind<=51u) return 3u;
    } else if(v->owner_actor_type==0x0eu) {
        if(v->kind>=52u && v->kind<=57u) {*flash=TRUE;return 1u;}
        if(v->kind>=59u && v->kind<=66u) return 1u;
    }
    return 4u; /* Impacts, skills, spirits and unknown kinds retain host geometry. */
}
uint64_t SudekiMpStoryMuzzleUnchangedMask(const SudekiMpStoryVfx *state) {
    if(!state || state->count>SUDEKIMP_STORY_VFX_CAPACITY) return 0;
    uint64_t prepared=0;
    for(unsigned i=0;i<state->count;++i) {
        BOOL flash=FALSE;
        if(visual_character(&state->entries[i],&flash)>=4u) prepared|=(UINT64_C(1)<<i);
    }
    return prepared;
}
static const SudekiMpLanStoryShot *shot_for(const SudekiMpLanStoryShots *shots,uint32_t tick,uint8_t item) {
    const SudekiMpLanStoryShot *best=NULL;uint32_t distance=101u;
    if(shots->count>SUDEKIMP_LAN_STORY_SHOT_HISTORY) return NULL;
    for(unsigned i=0;i<shots->count;++i) {
        const SudekiMpLanStoryShot *s=&shots->events[i];
        int32_t delta=(int32_t)(tick-s->host_tick);
        /* A flash can be born immediately before the direction observer in
         * the same native fire call. Bound both independent stream clocks. */
        if(!s->sequence || s->item!=item || delta < -30 || delta>100) continue;
        uint32_t d=(uint32_t)(delta<0?-delta:delta);
        if(d<distance) {best=s;distance=d;}
    }
    return best;
}
uint64_t SudekiMpStoryMuzzleProject(SudekiMpStoryMuzzleProjection *cache,SudekiMpStoryVfx *copy,
    const SudekiMpLanStoryFrame *frame,const SudekiMpStoryMuzzle tips[4]) {
    uint64_t prepared=SudekiMpStoryMuzzleUnchangedMask(copy);
    if(!cache || !copy || !frame || !tips || !copy->observed ||
        copy->count>SUDEKIMP_STORY_VFX_CAPACITY ||
        copy->epoch!=frame->epoch || copy->revision!=frame->revision) return prepared;
    if(cache->stream!=copy->stream || cache->epoch!=copy->epoch || cache->revision!=copy->revision) {
        memset(cache,0,sizeof(*cache));cache->stream=copy->stream;cache->epoch=copy->epoch;cache->revision=copy->revision;
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_VFX_CAPACITY;++i) {
        SudekiMpStoryMuzzleEntry *e=&cache->entries[i];BOOL present=FALSE;
        for(unsigned j=0;j<copy->count;++j) {
            const SudekiMpLanArenaSpiritVfxSnapshot *v=&copy->entries[j];
            if(e->instance && e->instance==v->instance_sequence && e->tick==v->emitted_host_tick &&
                e->kind==v->kind && e->character<4u &&
                e->generation==frame->actors[e->character].generation) present=TRUE;
        }
        if(!present) memset(e,0,sizeof(*e));
    }
    for(unsigned i=0;i<copy->count;++i) {
        SudekiMpLanArenaSpiritVfxSnapshot *v=&copy->entries[i];BOOL flash=FALSE;
        unsigned c=visual_character(v,&flash);
        if(c>=4u || !(frame->available_mask&(1u<<c))) continue;
        const SudekiMpLanStoryActor *a=&frame->actors[c];const SudekiMpStoryMuzzle *tip=&tips[c];
        SudekiMpStoryMuzzleEntry *e=NULL,*empty=NULL;
        for(unsigned j=0;j<SUDEKIMP_STORY_VFX_CAPACITY;++j) {
            SudekiMpStoryMuzzleEntry *candidate=&cache->entries[j];
            if(candidate->instance==v->instance_sequence && candidate->character==c) e=candidate;
            if(!candidate->instance && !empty) empty=candidate;
        }
        if(!e) {
            if(!empty || !a->generation || !tip->valid ||
                a->weapon_item_plus_one!=tip->item+1u || (uint32_t)(copy->tick-v->emitted_host_tick)>150u) continue;
            const SudekiMpLanStoryShot *shot=shot_for(&a->shots,v->emitted_host_tick,tip->item);
            if(!shot) continue;
            float norm=0;
            for(unsigned k=0;k<3u;++k) {float d=tip->position[k]-shot->origin[k];norm+=d*d;}
            if(!isfinite(norm) || norm>4.0f) continue; /* Do not bridge a teleport or stale pose. */
            e=empty;*e=(SudekiMpStoryMuzzleEntry){.instance=v->instance_sequence,.tick=v->emitted_host_tick,
                .generation=a->generation,.kind=v->kind,.character=(uint8_t)c,.item=tip->item};
            memcpy(e->origin,shot->origin,sizeof(e->origin));
            for(unsigned k=0;k<3u;++k) e->offset[k]=tip->position[k]-shot->origin[k];
        }
        if(flash) {
            if(tip->valid && tip->item==e->item && a->weapon_item_plus_one==e->item+1u)
                memcpy(v->position,tip->position,sizeof(v->position));
            else for(unsigned k=0;k<3u;++k) v->position[k]+=e->offset[k];
        } else {
            float distance=0;
            for(unsigned k=0;k<3u;++k) {float d=v->position[k]-e->origin[k];distance+=d*d;}
            distance=sqrtf(distance);if(!isfinite(distance)) continue;
            if(distance>e->distance) e->distance=distance;
            float t=fminf(e->distance/2.0f,1.0f),weight=1.0f-t*t*(3.0f-2.0f*t);
            for(unsigned k=0;k<3u;++k) v->position[k]+=e->offset[k]*weight;
        }
        prepared|=(UINT64_C(1)<<i);
    }
    return prepared;
}
