#include "network/lan_story_vfx.h"
#include <string.h>
static void put(uint8_t *p,uint32_t x) {for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
BOOL SudekiMpStoryVfxValid(const SudekiMpStoryVfx *s) {
    if(!s || !s->stream || s->observed>1u || s->count>SUDEKIMP_STORY_VFX_CAPACITY) return FALSE;
    unsigned end=0;
    for(;end<sizeof(s->current) && s->current[end];++end)
        if(s->current[end]<0x20 || s->current[end]>0x7e) return FALSE;
    if(end==sizeof(s->current)) return FALSE;
    for(;end<sizeof(s->current);++end) if(s->current[end]) return FALSE;
    return SudekiMpLanStoryCastVfxEntriesValid(s->tick,s->cast_sequence,s->observed,
        s->count,s->entries,SUDEKIMP_STORY_VFX_CAPACITY);
}
static BOOL encode(const SudekiMpStoryVfx *s,uint8_t *p,size_t size,unsigned slots,unsigned version) {
    if(!p || slots>SUDEKIMP_STORY_VFX_CAPACITY || size!=32u+SUDEKIMP_STORY_AREA_NAME+57u*slots ||
        !SudekiMpStoryVfxValid(s) || s->count>slots ||
        !s->epoch || !s->revision || !s->sequence || !s->current[0]) return FALSE;
    memset(p,0,size); put(p,version); put(p+4,s->epoch); put(p+8,s->revision);
    put(p+12,s->tick); put(p+16,s->sequence); put(p+20,(uint32_t)s->stream);
    put(p+24,(uint32_t)(s->stream>>32)); p[28]=(uint8_t)s->cast_sequence;
    p[29]=(uint8_t)(s->cast_sequence>>8); p[30]=s->observed; p[31]=s->count;
    memcpy(p+32,s->current,sizeof(s->current));
    for(unsigned i=0;i<slots;++i) {
        const SudekiMpLanArenaSpiritVfxSnapshot *v=&s->entries[i]; uint8_t *e=p+32+SUDEKIMP_STORY_AREA_NAME+57*i;
        put(e,v->instance_sequence); e[4]=(uint8_t)v->skill_sequence; e[5]=(uint8_t)(v->skill_sequence>>8);
        e[6]=v->kind; e[7]=v->phase_valid; put(e+8,v->emitted_host_tick);
        const float *groups[]={&v->phase,v->position,v->rotation_xyzw,v->scale};
        const unsigned counts[]={1,3,4,3}; unsigned offset=12;
        for(unsigned g=0;g<4;++g) for(unsigned j=0;j<counts[g];++j) {
            uint32_t bits; memcpy(&bits,&groups[g][j],4); put(e+offset,bits); offset+=4;
        }
        e[56]=v->owner_actor_type;
    }
    return TRUE;
}
static BOOL decode(const uint8_t *p,size_t size,SudekiMpStoryVfx *out,unsigned slots,unsigned version) {
    if(!p || !out || slots>SUDEKIMP_STORY_VFX_CAPACITY ||
        size!=32u+SUDEKIMP_STORY_AREA_NAME+57u*slots || get(p)!=version || p[31]>slots) return FALSE;
    SudekiMpStoryVfx s={0}; s.epoch=get(p+4); s.revision=get(p+8); s.tick=get(p+12); s.sequence=get(p+16);
    s.stream=(uint64_t)get(p+20)|((uint64_t)get(p+24)<<32); s.cast_sequence=(uint16_t)(p[28]|p[29]<<8);
    s.observed=p[30]; s.count=p[31];
    memcpy(s.current,p+32,sizeof(s.current));
    for(unsigned i=0;i<slots;++i) {
        SudekiMpLanArenaSpiritVfxSnapshot *v=&s.entries[i]; const uint8_t *e=p+32+SUDEKIMP_STORY_AREA_NAME+57*i;
        v->instance_sequence=get(e); v->skill_sequence=(uint16_t)(e[4]|e[5]<<8);
        v->kind=e[6]; v->phase_valid=e[7]; v->emitted_host_tick=get(e+8);
        float *groups[]={&v->phase,v->position,v->rotation_xyzw,v->scale};
        const unsigned counts[]={1,3,4,3}; unsigned offset=12;
        for(unsigned g=0;g<4;++g) for(unsigned j=0;j<counts[g];++j) {
            uint32_t bits=get(e+offset); memcpy(&groups[g][j],&bits,4); offset+=4;
        }
        v->owner_actor_type=e[56];
    }
    if(!s.epoch || !s.revision || !s.sequence || !s.current[0] || !SudekiMpStoryVfxValid(&s)) return FALSE;
    *out=s; return TRUE;
}

BOOL SudekiMpStoryVfxEncode(const SudekiMpStoryVfx *s,uint8_t *p,size_t size) {
    return encode(s,p,size,SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY,1);
}
BOOL SudekiMpStoryVfxDecode(const uint8_t *p,size_t size,SudekiMpStoryVfx *out) {
    return decode(p,size,out,SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY,1);
}
static uint32_t digest(const uint8_t *p,size_t size) {
    uint32_t hash=2166136261u;
    for(size_t i=0;i<size;++i) hash=(hash^p[i])*16777619u;
    return hash;
}
static unsigned wire_size(unsigned count) {return 32u+SUDEKIMP_STORY_AREA_NAME+57u*count;}
unsigned SudekiMpStoryVfxChunkCount(const SudekiMpStoryVfx *s) {
    if(!s || s->count>SUDEKIMP_STORY_VFX_CAPACITY) return 0;
    return (wire_size(s->count)+SUDEKIMP_STORY_VFX_CHUNK_BYTES-1u)/SUDEKIMP_STORY_VFX_CHUNK_BYTES;
}
BOOL SudekiMpStoryVfxChunkEncode(const SudekiMpStoryVfx *s,unsigned index,
    uint8_t *p,size_t capacity,size_t *written) {
    uint8_t wire[SUDEKIMP_STORY_VFX_WIRE_MAX];
    if(!p || !written || index>=SudekiMpStoryVfxChunkCount(s)) return FALSE;
    unsigned total=wire_size(s->count),offset=index*SUDEKIMP_STORY_VFX_CHUNK_BYTES,n=total-offset;
    if(n>SUDEKIMP_STORY_VFX_CHUNK_BYTES) n=SUDEKIMP_STORY_VFX_CHUNK_BYTES;
    if(capacity<SUDEKIMP_STORY_VFX_CHUNK_HEADER+n || !encode(s,wire,total,s->count,2)) return FALSE;
    put(p,1);put(p+4,s->epoch);put(p+8,s->revision);put(p+12,s->tick);
    put(p+16,s->sequence);put(p+20,digest(wire,total));put(p+24,total);put(p+28,index);
    memcpy(p+SUDEKIMP_STORY_VFX_CHUNK_HEADER,wire+offset,n);
    *written=SUDEKIMP_STORY_VFX_CHUNK_HEADER+n;return TRUE;
}
BOOL SudekiMpStoryVfxChunkDecode(const uint8_t *p,size_t size,SudekiMpStoryVfxChunk *out) {
    if(!p || !out || size<SUDEKIMP_STORY_VFX_CHUNK_HEADER || get(p)!=1) return FALSE;
    SudekiMpStoryVfxChunk c={.epoch=get(p+4),.revision=get(p+8),.tick=get(p+12),
        .sequence=get(p+16),.digest=get(p+20),.total=get(p+24),.index=get(p+28)};
    if(!c.epoch || !c.revision || !c.sequence || c.total<wire_size(0) ||
        c.total>SUDEKIMP_STORY_VFX_WIRE_MAX || (c.total-wire_size(0))%57u ||
        c.index>=(c.total+SUDEKIMP_STORY_VFX_CHUNK_BYTES-1u)/SUDEKIMP_STORY_VFX_CHUNK_BYTES) return FALSE;
    unsigned n=c.total-c.index*SUDEKIMP_STORY_VFX_CHUNK_BYTES;
    if(n>SUDEKIMP_STORY_VFX_CHUNK_BYTES) n=SUDEKIMP_STORY_VFX_CHUNK_BYTES;
    if(size!=SUDEKIMP_STORY_VFX_CHUNK_HEADER+n) return FALSE;
    memcpy(c.bytes,p+SUDEKIMP_STORY_VFX_CHUNK_HEADER,n);*out=c;return TRUE;
}
BOOL SudekiMpStoryVfxAssemble(const SudekiMpStoryVfxChunk *chunks,unsigned count,SudekiMpStoryVfx *out) {
    if(!chunks || !out || !count || count>SUDEKIMP_STORY_VFX_CHUNKS) return FALSE;
    const SudekiMpStoryVfxChunk *first=&chunks[0];
    if(first->total<wire_size(0) || first->total>SUDEKIMP_STORY_VFX_WIRE_MAX ||
        (first->total-wire_size(0))%57u ||
        count!=(first->total+SUDEKIMP_STORY_VFX_CHUNK_BYTES-1u)/SUDEKIMP_STORY_VFX_CHUNK_BYTES) return FALSE;
    uint8_t wire[SUDEKIMP_STORY_VFX_WIRE_MAX];unsigned mask=0;
    for(unsigned i=0;i<count;++i) {
        const SudekiMpStoryVfxChunk *c=&chunks[i];
        if(c->index>=count || (mask&(1u<<c->index)) || c->epoch!=first->epoch ||
            c->revision!=first->revision || c->tick!=first->tick || c->sequence!=first->sequence ||
            c->digest!=first->digest || c->total!=first->total) return FALSE;
        unsigned offset=c->index*SUDEKIMP_STORY_VFX_CHUNK_BYTES,n=c->total-offset;
        if(n>SUDEKIMP_STORY_VFX_CHUNK_BYTES) n=SUDEKIMP_STORY_VFX_CHUNK_BYTES;
        memcpy(wire+offset,c->bytes,n);mask|=1u<<c->index;
    }
    SudekiMpStoryVfx state;
    if(digest(wire,first->total)!=first->digest ||
        !decode(wire,first->total,&state,(first->total-wire_size(0))/57u,2) ||
        state.epoch!=first->epoch || state.revision!=first->revision ||
        state.tick!=first->tick || state.sequence!=first->sequence ||
        wire_size(state.count)!=first->total) return FALSE;
    *out=state;return TRUE;
}
