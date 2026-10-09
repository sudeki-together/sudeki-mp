#include "network/lan_story_area_state.h"

#include <string.h>

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
BOOL SudekiMpStoryAreaStateEncode(const SudekiMpStoryAreaState *s,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_AREA_WIRE_SIZE || !SudekiMpStoryAreaStateValid(s)) return FALSE;
    p[0]=(uint8_t)s->observed_tick; p[1]=(uint8_t)(s->observed_tick>>8);
    p[2]=(uint8_t)(s->observed_tick>>16); p[3]=(uint8_t)(s->observed_tick>>24);
    memcpy(p+4,s->current,SUDEKIMP_STORY_AREA_NAME);
    memcpy(p+4+SUDEKIMP_STORY_AREA_NAME,s->target,SUDEKIMP_STORY_AREA_NAME);
    p[4+2*SUDEKIMP_STORY_AREA_NAME]=s->count;
    uint8_t *e=p+5+2*SUDEKIMP_STORY_AREA_NAME;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_MAX;++i,e+=SUDEKIMP_STORY_AREA_NAME+1u) {
        memcpy(e,s->entries[i].name,SUDEKIMP_STORY_AREA_NAME); e[SUDEKIMP_STORY_AREA_NAME]=s->entries[i].state;
    }
    return TRUE;
}
BOOL SudekiMpStoryAreaStateDecode(const uint8_t *p,size_t n,SudekiMpStoryAreaState *s) {
    SudekiMpStoryAreaState v;
    if(!p || !s || n!=SUDEKIMP_STORY_AREA_WIRE_SIZE) return FALSE;
    memset(&v,0,sizeof(v));
    v.observed_tick=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
    memcpy(v.current,p+4,SUDEKIMP_STORY_AREA_NAME);
    memcpy(v.target,p+4+SUDEKIMP_STORY_AREA_NAME,SUDEKIMP_STORY_AREA_NAME);
    v.count=p[4+2*SUDEKIMP_STORY_AREA_NAME];
    const uint8_t *e=p+5+2*SUDEKIMP_STORY_AREA_NAME;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_MAX;++i,e+=SUDEKIMP_STORY_AREA_NAME+1u) {
        memcpy(v.entries[i].name,e,SUDEKIMP_STORY_AREA_NAME); v.entries[i].state=e[SUDEKIMP_STORY_AREA_NAME];
    }
    if(!SudekiMpStoryAreaStateValid(&v)) return FALSE;
    *s=v; return TRUE;
}
