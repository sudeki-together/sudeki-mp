#include "network/lan_story_world_frame.h"
#include <math.h>
#include <string.h>

_Static_assert(sizeof(float)==4u,"World presentation requires binary32");
_Static_assert(SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE+28u<=1468u,
    "World presentation chunk exceeds the SMP4 datagram limit");

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint64_t get64(const uint8_t *p) { return get32(p)|((uint64_t)get32(p+4)<<32); }
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));
}
static void put64(uint8_t *p,uint64_t v) { put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32)); }
static float get_float(const uint8_t *p) { uint32_t b=get32(p); float f; memcpy(&f,&b,4); return f; }
static void put_float(uint8_t *p,float f) { uint32_t b; memcpy(&b,&f,4); put32(p,b); }
static BOOL bounded(float f,float low,float high) { return isfinite(f) && f>=low && f<=high; }
static BOOL channel_state(uint8_t state) {
    return state==0u || state==1u || state==64u || state==65u || state==128u || state==192u;
}

uint32_t SudekiMpLanStoryWorldCharacterIdentifier(unsigned character) {
    static const uint32_t ids[4]={0x019c1ebau,0x0180e1d4u,0x0213755cu,0x8557d453u};
    return character<4u?ids[character]:0u;
}
unsigned SudekiMpLanStoryWorldCharacter(uint32_t identifier) {
    for(unsigned c=0;c<4u;++c)
        if(identifier==SudekiMpLanStoryWorldCharacterIdentifier(c)) return c;
    return 4u;
}
uint32_t SudekiMpLanStoryWorldAvatarIdentifier(unsigned player) {
    return player<4u?0xffffff00u+player:0u;
}
unsigned SudekiMpLanStoryWorldAvatarPlayer(uint32_t identifier) {
    return identifier>=0xffffff00u && identifier<=0xffffff03u?identifier-0xffffff00u:4u;
}
BOOL SudekiMpLanStoryWorldActorValid(const SudekiMpLanStoryWorldActor *a) {
    if(!a || (a->kind!=SUDEKIMP_LAN_STORY_WORLD_NPC_KIND &&
        a->kind!=SUDEKIMP_LAN_STORY_WORLD_PC_KIND &&
        a->kind!=SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND &&
        a->kind!=SUDEKIMP_LAN_STORY_WORLD_ENEMY_KIND &&
        a->kind!=SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND) ||
        (a->visual_flags&~SUDEKIMP_LAN_STORY_WORLD_HIDDEN) ||
        (a->kind!=SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND && a->visual_flags) ||
        (a->kind==SUDEKIMP_LAN_STORY_WORLD_PC_KIND && SudekiMpLanStoryWorldCharacter(a->identifier)>=4u) ||
        !a->identifier ||
        !a->generation || !a->animation_sequence || !a->bank_fingerprint ||
        !a->submodels || a->submodels>32u) return FALSE;
    float norm=0;
    for(unsigned i=0;i<3u;++i) {
        if(!bounded(a->position[i],-1000000.0f,1000000.0f) ||
            !bounded(a->forward[i],-1.5f,1.5f) || !bounded(a->blend[i],0,1)) return FALSE;
        norm+=a->forward[i]*a->forward[i];
    }
    if(!bounded(norm,.25f,2.25f)) return FALSE;
    for(unsigned i=0;i<5u;++i)
        if(a->clip_occurrence[i]>=SUDEKIMP_LAN_STORY_WORLD_MAX_CLIP_OCCURRENCES ||
            (!a->clip[i] && a->clip_occurrence[i]) ||
            !channel_state(a->state[i]) || !bounded(a->rate[i],0,256) ||
            !bounded(a->time[i],0,4096)) return FALSE;
    if(!bounded(a->blend[3],0,1)) return FALSE;
    unsigned character=a->kind==SUDEKIMP_LAN_STORY_WORLD_PC_KIND?
        SudekiMpLanStoryWorldCharacter(a->identifier):4u;
    if(character!=1u && character!=3u &&
        (a->clip[4] || a->clip_occurrence[4] || a->state[4] ||
         a->rate[4] || a->time[4] || a->blend[3])) return FALSE;
    if(a->kind==SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND) {
        /* Its single native channel includes a real authored selector zero.
         * The remaining portable channels and blends are canonical padding. */
        if(!a->clip[0]) return FALSE;
        for(unsigned i=1;i<5u;++i)
            if(a->clip[i] || a->clip_occurrence[i] || a->state[i] || a->rate[i] || a->time[i]) return FALSE;
        for(unsigned i=0;i<4u;++i) if(a->blend[i]) return FALSE;
    }
    return TRUE;
}
static BOOL before(const SudekiMpLanStoryWorldActor *a,const SudekiMpLanStoryWorldActor *b) {
    return a->kind<b->kind || (a->kind==b->kind && a->identifier<b->identifier);
}
BOOL SudekiMpLanStoryWorldFrameValid(const SudekiMpLanStoryWorldFrame *f) {
    if(!f || !f->epoch || !f->revision || !f->sequence ||
        f->count>SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS) return FALSE;
    for(unsigned i=0;i<f->count;++i)
        if(!SudekiMpLanStoryWorldActorValid(&f->actors[i]) ||
            (i && !before(&f->actors[i-1],&f->actors[i]))) return FALSE;
    /* Unused in-memory records are deliberately not serialized or consulted. */
    return TRUE;
}
BOOL SudekiMpLanStoryWorldFrameMatchesForPolicy(const SudekiMpLanStoryWorldFrame *w,
    const SudekiMpLanStoryFrame *p,SudekiMpLanStoryPolicy policy) {
    if(!w || !p || w->epoch!=p->epoch || w->revision!=p->revision ||
        w->sequence!=p->sequence || w->host_tick!=p->host_tick ||
        !SudekiMpLanStoryWorldFrameValid(w) || !SudekiMpLanStoryFrameValidForPolicy(p,policy)) return FALSE;
    unsigned found=0,expected=0;
    for(unsigned c=0;c<4u;++c) if((p->available_mask&(1u<<c)) && p->actors[c].native_pose)
        expected|=1u<<c;
    for(unsigned i=0;i<w->count;++i) {
        const SudekiMpLanStoryWorldActor *a=&w->actors[i];
        if(a->kind!=SUDEKIMP_LAN_STORY_WORLD_PC_KIND) continue;
        unsigned c=SudekiMpLanStoryWorldCharacter(a->identifier);
        if(c>=4u || !(expected&(1u<<c)) || (found&(1u<<c))) return FALSE;
        const SudekiMpLanStoryActor *actor=&p->actors[c];
        if(a->generation!=actor->generation || a->position[0]!=actor->x ||
            a->position[1]!=actor->y || a->position[2]!=actor->z) return FALSE;
        found|=1u<<c;
    }
    return found==expected;
}
BOOL SudekiMpLanStoryWorldFrameMatches(const SudekiMpLanStoryWorldFrame *w,
    const SudekiMpLanStoryFrame *p) {
    return SudekiMpLanStoryWorldFrameMatchesForPolicy(w,p,SUDEKIMP_LAN_STORY_POLICY_REGULAR);
}
BOOL SudekiMpLanStoryWorldFrameMatchesSceneForPolicy(const SudekiMpLanStoryWorldFrame *f,
    const SudekiMpLanStoryScene *s,SudekiMpLanStoryPolicy policy) {
    return SudekiMpLanStoryWorldFrameValid(f) && SudekiMpLanStorySceneValidForPolicy(s,policy) &&
        s->phase==SUDEKIMP_LAN_STORY_READY && f->epoch==s->epoch && f->revision==s->revision;
}
BOOL SudekiMpLanStoryWorldFrameMatchesScene(const SudekiMpLanStoryWorldFrame *f,
    const SudekiMpLanStoryScene *s) {
    return SudekiMpLanStoryWorldFrameMatchesSceneForPolicy(f,s,SUDEKIMP_LAN_STORY_POLICY_REGULAR);
}
unsigned SudekiMpLanStoryWorldChunkCount(unsigned n) {
    if(n>SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS) return 0;
    return n ? (n+SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS-1u)/SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS : 1u;
}
static unsigned chunk_count(unsigned total,unsigned index) {
    unsigned first=index*SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS;
    if(first>=total) return 0;
    unsigned n=total-first;
    return n<SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS?n:SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS;
}
static void write_actor(uint8_t *p,const SudekiMpLanStoryWorldActor *a) {
    put16(p,a->kind); p[2]=a->submodels; p[3]=a->visual_flags;
    put32(p+4,a->identifier); put32(p+8,a->generation); put32(p+12,a->animation_sequence);
    put64(p+16,a->bank_fingerprint);
    for(unsigned i=0;i<3u;++i) {
        put_float(p+24+4*i,a->position[i]); put_float(p+36+4*i,a->forward[i]);
        put_float(p+100+4*i,a->blend[i]);
    }
    for(unsigned i=0;i<4u;++i) {
        put32(p+48+4*i,a->clip[i]); p[64+i]=a->state[i];
        put_float(p+68+4*i,a->rate[i]); put_float(p+84+4*i,a->time[i]);
        put16(p+112+2*i,a->clip_occurrence[i]);
    }
    put32(p+120,a->clip[4]); put16(p+124,a->clip_occurrence[4]);
    p[126]=a->state[4]; p[127]=0;
    put_float(p+128,a->rate[4]); put_float(p+132,a->time[4]);
    put_float(p+136,a->blend[3]);
}
static BOOL read_actor(const uint8_t *p,SudekiMpLanStoryWorldActor *a) {
    a->kind=get16(p); a->submodels=p[2]; a->visual_flags=p[3];
    a->identifier=get32(p+4); a->generation=get32(p+8); a->animation_sequence=get32(p+12);
    a->bank_fingerprint=get64(p+16);
    for(unsigned i=0;i<3u;++i) {
        a->position[i]=get_float(p+24+4*i); a->forward[i]=get_float(p+36+4*i);
        a->blend[i]=get_float(p+100+4*i);
    }
    for(unsigned i=0;i<4u;++i) {
        a->clip[i]=get32(p+48+4*i); a->state[i]=p[64+i];
        a->rate[i]=get_float(p+68+4*i); a->time[i]=get_float(p+84+4*i);
        a->clip_occurrence[i]=get16(p+112+2*i);
    }
    if(p[127]) return FALSE;
    a->clip[4]=get32(p+120); a->clip_occurrence[4]=get16(p+124);
    a->state[4]=p[126]; a->rate[4]=get_float(p+128); a->time[4]=get_float(p+132);
    a->blend[3]=get_float(p+136);
    return SudekiMpLanStoryWorldActorValid(a);
}
BOOL SudekiMpLanStoryWorldChunkEncode(const SudekiMpLanStoryWorldFrame *f,
    unsigned index,uint8_t *bytes,size_t capacity,size_t *written) {
    if(written) *written=0;
    if(!bytes || !written || !SudekiMpLanStoryWorldFrameValid(f) ||
        index>=SudekiMpLanStoryWorldChunkCount(f->count)) return FALSE;
    unsigned count=chunk_count(f->count,index);
    size_t size=SUDEKIMP_LAN_STORY_WORLD_HEADER_SIZE+count*SUDEKIMP_LAN_STORY_WORLD_ACTOR_SIZE;
    if(capacity<size) return FALSE;
    memset(bytes,0,size);
    bytes[0]='S'; bytes[1]='W'; bytes[2]='F'; bytes[3]=SUDEKIMP_LAN_STORY_WORLD_VERSION;
    put32(bytes+4,f->epoch); put32(bytes+8,f->revision); put32(bytes+12,f->host_tick);
    put32(bytes+16,f->sequence); bytes[20]=f->count; bytes[21]=(uint8_t)index;
    bytes[22]=(uint8_t)SudekiMpLanStoryWorldChunkCount(f->count); bytes[23]=(uint8_t)count;
    for(unsigned i=0;i<count;++i)
        write_actor(bytes+28+i*SUDEKIMP_LAN_STORY_WORLD_ACTOR_SIZE,
            &f->actors[index*SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS+i]);
    *written=size; return TRUE;
}
BOOL SudekiMpLanStoryWorldChunkDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryWorldChunk *out) {
    SudekiMpLanStoryWorldChunk c={0};
    if(!bytes || !out || size<28u || size>SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE ||
        bytes[0]!='S' || bytes[1]!='W' || bytes[2]!='F' || bytes[3]!=SUDEKIMP_LAN_STORY_WORLD_VERSION ||
        bytes[24] || bytes[25] || bytes[26] || bytes[27] ||
        bytes[20]>SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS ||
        bytes[22]!=SudekiMpLanStoryWorldChunkCount(bytes[20]) || bytes[21]>=bytes[22] ||
        bytes[23]!=chunk_count(bytes[20],bytes[21]) ||
        size!=28u+bytes[23]*SUDEKIMP_LAN_STORY_WORLD_ACTOR_SIZE) return FALSE;
    c.epoch=get32(bytes+4); c.revision=get32(bytes+8); c.host_tick=get32(bytes+12);
    c.sequence=get32(bytes+16); c.total=bytes[20]; c.index=bytes[21]; c.chunks=bytes[22]; c.count=bytes[23];
    if(!c.epoch || !c.revision || !c.sequence) return FALSE;
    for(unsigned i=0;i<c.count;++i)
        if(!read_actor(bytes+28+i*SUDEKIMP_LAN_STORY_WORLD_ACTOR_SIZE,&c.actors[i]) ||
            (i && !before(&c.actors[i-1],&c.actors[i]))) return FALSE;
    *out=c; return TRUE;
}
static float lerp(float a,float b,float t) { return a+(b-a)*t; }
BOOL SudekiMpLanStoryWorldFrameInterpolate(const SudekiMpLanStoryWorldFrame *a,
    const SudekiMpLanStoryWorldFrame *b,uint32_t tick,SudekiMpLanStoryWorldFrame *out) {
    if(!out || !SudekiMpLanStoryWorldFrameValid(a) || !SudekiMpLanStoryWorldFrameValid(b) ||
        a->epoch!=b->epoch || a->revision!=b->revision || a->count!=b->count ||
        (int32_t)(b->sequence-a->sequence)<=0 || (int32_t)(b->host_tick-a->host_tick)<=0 ||
        (int32_t)(tick-a->host_tick)<0 || (int32_t)(b->host_tick-tick)<0) return FALSE;
    for(unsigned i=0;i<a->count;++i) {
        const SudekiMpLanStoryWorldActor *x=&a->actors[i],*y=&b->actors[i];
        if(x->kind!=y->kind || x->identifier!=y->identifier || x->generation!=y->generation ||
            x->bank_fingerprint!=y->bank_fingerprint || x->submodels!=y->submodels) return FALSE;
    }
    if(tick==b->host_tick) { *out=*b; return TRUE; }
    *out=*a;
    if(tick==a->host_tick) return TRUE;
    float t=(float)(tick-a->host_tick)/(float)(b->host_tick-a->host_tick);
    out->host_tick=tick;
    for(unsigned i=0;i<a->count;++i) {
        const SudekiMpLanStoryWorldActor *x=&a->actors[i],*y=&b->actors[i];
        SudekiMpLanStoryWorldActor *z=&out->actors[i]; float norm=0;
        SudekiMpLanStoryInterpolatePosition(x->position,y->position,
            tick-a->host_tick,b->host_tick-a->host_tick,z->position);
        for(unsigned k=0;k<3u;++k) {
            z->forward[k]=lerp(x->forward[k],y->forward[k],t); norm+=z->forward[k]*z->forward[k];
        }
        if(norm>.000001f) for(unsigned k=0;k<3u;++k) z->forward[k]/=sqrtf(norm);
        else memcpy(z->forward,x->forward,sizeof(z->forward));
        BOOL same=x->animation_sequence==y->animation_sequence &&
            !memcmp(x->clip,y->clip,sizeof(x->clip)) &&
            !memcmp(x->clip_occurrence,y->clip_occurrence,sizeof(x->clip_occurrence)) &&
            !memcmp(x->state,y->state,sizeof(x->state));
        for(unsigned k=0;k<5u;++k) if(y->time[k]<x->time[k]) same=FALSE;
        if(!same) continue;
        for(unsigned k=0;k<5u;++k) {
            z->rate[k]=lerp(x->rate[k],y->rate[k],t); z->time[k]=lerp(x->time[k],y->time[k],t);
        }
        for(unsigned k=0;k<4u;++k) z->blend[k]=lerp(x->blend[k],y->blend[k],t);
    }
    return SudekiMpLanStoryWorldFrameValid(out);
}
