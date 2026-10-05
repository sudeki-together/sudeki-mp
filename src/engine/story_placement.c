#include "engine/story_placement.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int text_valid(const char *s,size_t n,int empty,int area) {
    if(!s || (!empty && !s[0])) return 0;
    size_t i=0;
    for(;i<n && s[i];++i) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a'&&c<='z') || (!area && c>='A'&&c<='Z') ||
            (c>='0'&&c<='9') || c=='_' || c=='-' ||
            (!area && (c=='.' || c==':' || c=='/' || c=='\\')))) return 0;
    }
    if(i==n) return 0;
    for(;i<n;++i) if(s[i]) return 0;
    return 1;
}
static int scope_valid(const SudekiMpStoryPlacementScope *s) {
    if(!s || !s->visit || !text_valid(s->world,64,0,1) || !text_valid(s->temporary,64,1,1)) return 0;
    unsigned any=0; for(unsigned i=0;i<32;++i) any|=s->save_identity[i];
    return any!=0;
}
static int same_scope(const SudekiMpStoryPlacementScope *a,const SudekiMpStoryPlacementScope *b) {
    return a->visit==b->visit && !memcmp(a->save_identity,b->save_identity,32) &&
        !memcmp(a->world,b->world,64) && !memcmp(a->temporary,b->temporary,64);
}
static int order(const void *left,const void *right) {
    const SudekiMpStoryPlacement *a=left,*b=right;
    if(a->zone!=b->zone) return a->zone<b->zone?-1:1;
    if(a->anchor!=b->anchor) return a->anchor<b->anchor?-1:1;
    int anchor=memcmp(a->anchor_name,b->anchor_name,sizeof(a->anchor_name));
    if(anchor) return anchor;
    if(a->resource!=b->resource) return a->resource<b->resource?-1:1;
    int name=memcmp(a->name,b->name,sizeof(a->name));
    if(name) return name;
    for(unsigned i=0;i<3;++i) if(a->position[i]!=b->position[i])
        return a->position[i]<b->position[i]?-1:1;
    for(unsigned i=0;i<3;++i) if(a->forward[i]!=b->forward[i])
        return a->forward[i]<b->forward[i]?-1:1;
    return 0;
}
int SudekiMpStoryPlacementValid(const SudekiMpStoryPlacement *p) {
    if(!p || p->zone>=206 || p->anchor==0x7ffffu || p->resource==0x7ffffu ||
        !text_valid(p->anchor_name,sizeof(p->anchor_name),0,0) ||
        !text_valid(p->name,sizeof(p->name),0,0)) return 0;
    float length=0;
    for(unsigned i=0;i<3;++i) {
        float position,forward;
        memcpy(&position,&p->position[i],4); memcpy(&forward,&p->forward[i],4);
        if(!isfinite(position) || fabsf(position)>=1000000.0f ||
            !isfinite(forward) || fabsf(forward)>2.0f ||
            p->position[i]==0x80000000u || p->forward[i]==0x80000000u) return 0;
        length+=forward*forward;
    }
    return length>.99f && length<1.01f;
}
int SudekiMpStoryPlacementMake(SudekiMpStoryPlacement *out,uint32_t zone,
    uint32_t anchor,const char *anchor_name,uint32_t resource,const char *name,
    const float position[3],const float forward[3]) {
    if(!out || !name || !anchor_name || !position || !forward) return 0;
    SudekiMpStoryPlacement p={.zone=zone,.anchor=anchor,.resource=resource};
    size_t i=0;
    for(;i<sizeof(p.name) && name[i];++i) p.name[i]=name[i];
    if(i==sizeof(p.name)) return 0;
    for(i=0;i<sizeof(p.anchor_name) && anchor_name[i];++i) p.anchor_name[i]=anchor_name[i];
    if(i==sizeof(p.anchor_name)) return 0;
    for(unsigned j=0;j<3;++j) {
        float x=position[j]==0?0.0f:position[j],f=forward[j]==0?0.0f:forward[j];
        memcpy(&p.position[j],&x,4); memcpy(&p.forward[j],&f,4);
    }
    if(!SudekiMpStoryPlacementValid(&p)) return 0;
    *out=p; return 1;
}
int SudekiMpStoryPlacementCatalogValid(const SudekiMpStoryPlacementCatalog *c) {
    if(!c || c->bound!=1 || !scope_valid(&c->scope) || c->count>SUDEKIMP_STORY_PLACEMENT_MAX) return 0;
    for(unsigned i=0;i<c->count;++i)
        if(!SudekiMpStoryPlacementValid(&c->placements[i]) ||
            (i && (order(&c->placements[i-1],&c->placements[i])>=0 ||
             (c->placements[i-1].zone==c->placements[i].zone &&
              c->placements[i-1].anchor==c->placements[i].anchor &&
              !strcmp(c->placements[i-1].anchor_name,c->placements[i].anchor_name))))) return 0;
    return 1;
}
int SudekiMpStoryPlacementCatalogBind(SudekiMpStoryPlacementCatalog *out,
    const SudekiMpStoryPlacementScope *scope,const SudekiMpStoryPlacement *p,size_t count) {
    if(!out || !scope_valid(scope) || count>SUDEKIMP_STORY_PLACEMENT_MAX || (count && !p)) return 0;
    /* Heap scratch keeps the complete catalog off Sudeki's small native stack.
     * Nothing is published on allocation failure or an ambiguous placement. */
    SudekiMpStoryPlacementCatalog *next=calloc(1,sizeof(*next));
    if(!next) return 0;
    next->bound=1; next->scope=*scope; next->count=(uint32_t)count;
    if(count) memcpy(next->placements,p,count*sizeof(*p));
    qsort(next->placements,count,sizeof(*p),order);
    int ok=SudekiMpStoryPlacementCatalogValid(next);
    if(ok && out->bound) ok=SudekiMpStoryPlacementCatalogValid(out) &&
        same_scope(&out->scope,scope) && out->count==count &&
        !memcmp(out->placements,next->placements,count*sizeof(*p));
    if(ok && !out->bound) *out=*next;
    free(next); return ok;
}
static int lookup(const SudekiMpStoryPlacementCatalog *c,const SudekiMpStoryPlacement *p,size_t *at) {
    size_t lo=0,hi=c->count;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        int cmp=order(p,&c->placements[mid]);
        if(!cmp) { *at=mid; return 1; }
        if(cmp<0) hi=mid; else lo=mid+1;
    }
    return 0;
}
int SudekiMpStoryPlacementSource(const SudekiMpStoryPlacementCatalog *c,
    const SudekiMpStoryPlacement *p,uint64_t *source) {
    size_t at;
    if(!source || !SudekiMpStoryPlacementCatalogValid(c) || !SudekiMpStoryPlacementValid(p) ||
        !lookup(c,p,&at)) return 0;
    *source=at+1; return 1;
}
int SudekiMpStoryPlacementMatch(const SudekiMpStoryPlacementCatalog *c,
    const SudekiMpStoryPlacementScope *scope,const SudekiMpStoryPlacement *p,size_t count,
    uint16_t *map,size_t capacity) {
    if(!SudekiMpStoryPlacementCatalogValid(c) || !scope_valid(scope) || !same_scope(&c->scope,scope) ||
        count!=c->count || capacity<count || (count && (!p || !map))) return 0;
    uint16_t next[SUDEKIMP_STORY_PLACEMENT_MAX]={0};
    uint8_t used[SUDEKIMP_STORY_PLACEMENT_MAX]={0};
    for(size_t i=0;i<count;++i) {
        size_t at;
        if(!SudekiMpStoryPlacementValid(&p[i]) || !lookup(c,&p[i],&at) || used[at]) return 0;
        used[at]=1; next[at]=(uint16_t)i;
    }
    if(count) memcpy(map,next,count*sizeof(*map));
    return 1;
}
