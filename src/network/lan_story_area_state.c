#include "network/lan_story_area_state.h"
#include <string.h>
_Static_assert(SUDEKIMP_STORY_AREA_WIRE_SIZE+28u<=1468u,"story area datagram bound");
static void put(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));}
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static BOOL name_valid(const char *n,BOOL allow_empty) {
    unsigned i=0;
    for(;i<SUDEKIMP_STORY_AREA_NAME && n[i];++i) if(n[i]<0x20 || n[i]>0x7e) return FALSE;
    if(i==SUDEKIMP_STORY_AREA_NAME) return FALSE;
    for(unsigned j=i;j<SUDEKIMP_STORY_AREA_NAME;++j) if(n[j]) return FALSE;
    return allow_empty || i>0u;
}
BOOL SudekiMpStoryAreaStateValid(const SudekiMpStoryAreaState *s) {
    static const SudekiMpStoryAreaEntry zero;
    if(!s || s->count>SUDEKIMP_STORY_AREA_MAX || !name_valid(s->current,FALSE) || !name_valid(s->target,TRUE)) return FALSE;
    for(unsigned i=0;i<s->count;++i)
        if(!name_valid(s->entries[i].name,FALSE) || !s->entries[i].state || s->entries[i].state>4u) return FALSE;
    for(unsigned i=s->count;i<SUDEKIMP_STORY_AREA_MAX;++i)
        if(memcmp(&s->entries[i],&zero,sizeof(zero))) return FALSE;
    return TRUE;
}
BOOL SudekiMpStoryAreaStateSame(const SudekiMpStoryAreaState *a,const SudekiMpStoryAreaState *b) {
    return SudekiMpStoryAreaStateValid(a) && SudekiMpStoryAreaStateValid(b) &&
        !memcmp(a->current,b->current,sizeof(a->current)) && !memcmp(a->target,b->target,sizeof(a->target)) &&
        a->count==b->count && !memcmp(a->entries,b->entries,sizeof(a->entries));
}
BOOL SudekiMpStoryAreaStateExterior(const SudekiMpStoryAreaState *s) {
    if(!SudekiMpStoryAreaStateValid(s)) return FALSE;
    BOOL current=FALSE;
    for(unsigned i=0;i<s->count;++i) {
        /* Reject mixed transition reports, including an exterior current
         * accompanied by an active temporary descriptor. Names use the
         * native case-insensitive lookup convention. */
        if(s->entries[i].state==4u) return FALSE;
        for(unsigned j=0;j<i;++j)
            if(!_stricmp(s->entries[i].name,s->entries[j].name)) return FALSE;
        if(!_stricmp(s->entries[i].name,s->current)) {
            if(s->entries[i].state!=3u) return FALSE;
            current=TRUE;
        }
    }
    return current;
}
BOOL SudekiMpStoryAreaStateEncode(const SudekiMpStoryAreaState *s,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_AREA_WIRE_SIZE || !SudekiMpStoryAreaStateValid(s) || !s->sequence) return FALSE;
    put(p,SUDEKIMP_STORY_AREA_CODEC_VERSION); put(p+4,s->sequence); put(p+8,s->observed_tick);
    memcpy(p+12,s->current,40u); memcpy(p+52,s->target,40u); p[92]=s->count;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_MAX;++i) {
        memcpy(p+93+i*41u,s->entries[i].name,40u); p[133+i*41u]=s->entries[i].state;
    }
    return TRUE;
}
BOOL SudekiMpStoryAreaStateDecode(const uint8_t *p,size_t n,SudekiMpStoryAreaState *out) {
    if(!p || !out || n!=SUDEKIMP_STORY_AREA_WIRE_SIZE || get(p)!=SUDEKIMP_STORY_AREA_CODEC_VERSION) return FALSE;
    SudekiMpStoryAreaState s={0}; s.sequence=get(p+4); s.observed_tick=get(p+8);
    memcpy(s.current,p+12,40u); memcpy(s.target,p+52,40u); s.count=p[92];
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_MAX;++i) {
        memcpy(s.entries[i].name,p+93+i*41u,40u); s.entries[i].state=p[133+i*41u];
    }
    if(!s.sequence || !SudekiMpStoryAreaStateValid(&s)) return FALSE;
    *out=s; return TRUE;
}
