#include "network/lan_party_story.h"
#include <string.h>

static int name_valid(const char name[SUDEKIMP_LAN_STORY_NAME_SIZE], int required) {
    size_t end=0;
    while(end<SUDEKIMP_LAN_STORY_NAME_SIZE && name[end]) {
        unsigned char c=(unsigned char)name[end++];
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-'))
            return 0;
    }
    if(end==SUDEKIMP_LAN_STORY_NAME_SIZE || (required && !end)) return 0;
    for(;end<SUDEKIMP_LAN_STORY_NAME_SIZE;++end) if(name[end]) return 0;
    return 1;
}
int SudekiMpLanStorySceneValid(const SudekiMpLanStoryScene *s) {
    if(!s || !s->epoch || !s->revision || s->phase>SUDEKIMP_LAN_STORY_READY ||
        (s->available_mask&~15u) || s->leader_seat>SUDEKIMP_LAN_STORY_NO_SEAT ||
        !name_valid(s->world,s->phase==SUDEKIMP_LAN_STORY_READY) ||
        !name_valid(s->temporary,0) || (s->temporary[0] && !s->world[0]) ||
        (s->inside_mask&~15u)) return 0;
    if(s->phase!=SUDEKIMP_LAN_STORY_READY)
        return !s->available_mask && !s->inside_mask && s->leader_seat==SUDEKIMP_LAN_STORY_NO_SEAT;
    if((s->inside_mask&~s->available_mask) || (!s->temporary[0])!=(!s->inside_mask)) return 0;
    return s->leader_seat<4u && (s->available_mask&(1u<<s->leader_seat))!=0;
}
int SudekiMpLanStorySceneSame(const SudekiMpLanStoryScene *a,
    const SudekiMpLanStoryScene *b) {
    return a && b && a->epoch==b->epoch && a->phase==b->phase &&
        a->available_mask==b->available_mask && a->leader_seat==b->leader_seat &&
        !memcmp(a->world,b->world,sizeof(a->world)) &&
        !memcmp(a->temporary,b->temporary,sizeof(a->temporary)) &&
        a->inside_mask==b->inside_mask;
}
static int exterior_occupied(const SudekiMpLanStoryScene *s) {
    return s->phase==SUDEKIMP_LAN_STORY_READY && (s->available_mask&~s->inside_mask)!=0;
}
int SudekiMpLanStorySceneAdvances(const SudekiMpLanStoryScene *p,
    const SudekiMpLanStoryScene *n) {
    if(!SudekiMpLanStorySceneValid(n)) return 0;
    if(!p || !p->revision) return 1;
    /* Epoch/revision exhaustion closes the session; these counters never wrap.
     * Tick arithmetic alone wraps, like GetTickCount's bounded freshness. */
    if(!SudekiMpLanStorySceneValid(p) || n->epoch<p->epoch ||
        n->revision<p->revision || (int32_t)(n->observed_tick-p->observed_tick)<0)
        return 0;
    if(n->revision==p->revision)
        return SudekiMpLanStorySceneSame(p,n) &&
            (int32_t)(n->observed_tick-p->observed_tick)>0;
    if(SudekiMpLanStorySceneSame(p,n)) return 0;
    if(n->epoch==p->epoch) {
        if(memcmp(p->world,n->world,sizeof(p->world))) return 0;
        /* Split occupancy: the exterior stays live, so its epoch continues. */
        if(memcmp(p->temporary,n->temporary,sizeof(p->temporary)) &&
            (!exterior_occupied(p) || !exterior_occupied(n))) return 0;
    }
    return 1;
}
static void put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
int SudekiMpLanStorySceneEncode(const SudekiMpLanStoryScene *s,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_LAN_STORY_WIRE_SIZE || !SudekiMpLanStorySceneValid(s)) return 0;
    put32(p,s->epoch); put32(p+4,s->revision); put32(p+8,s->observed_tick);
    p[12]=s->phase; p[13]=s->available_mask; p[14]=s->leader_seat;
    memcpy(p+15,s->world,sizeof(s->world));
    memcpy(p+79,s->temporary,sizeof(s->temporary));
    p[143]=s->inside_mask;
    return 1;
}
int SudekiMpLanStorySceneDecode(const uint8_t *p,size_t n,SudekiMpLanStoryScene *s) {
    SudekiMpLanStoryScene v={0};
    if(!p || !s || n!=SUDEKIMP_LAN_STORY_WIRE_SIZE) return 0;
    v.epoch=get32(p); v.revision=get32(p+4); v.observed_tick=get32(p+8);
    v.phase=p[12]; v.available_mask=p[13]; v.leader_seat=p[14];
    memcpy(v.world,p+15,sizeof(v.world));
    memcpy(v.temporary,p+79,sizeof(v.temporary));
    v.inside_mask=p[143];
    if(!SudekiMpLanStorySceneValid(&v)) return 0;
    *s=v; return 1;
}
int SudekiMpLanStorySceneCharacterArea(const SudekiMpLanStoryScene *s,unsigned int c,
    char world[SUDEKIMP_LAN_STORY_NAME_SIZE],char temporary[SUDEKIMP_LAN_STORY_NAME_SIZE]) {
    if(c>=4u || !world || !temporary || !SudekiMpLanStorySceneValid(s) ||
        s->phase!=SUDEKIMP_LAN_STORY_READY || !(s->available_mask&(1u<<c))) return 0;
    memcpy(world,s->world,SUDEKIMP_LAN_STORY_NAME_SIZE);
    if(s->inside_mask&(1u<<c)) memcpy(temporary,s->temporary,SUDEKIMP_LAN_STORY_NAME_SIZE);
    else memset(temporary,0,SUDEKIMP_LAN_STORY_NAME_SIZE);
    return 1;
}
int SudekiMpLanStorySceneCharacterAreaMatches(const SudekiMpLanStoryScene *s,unsigned int c,
    const char *world,const char *temporary) {
    char w[SUDEKIMP_LAN_STORY_NAME_SIZE],t[SUDEKIMP_LAN_STORY_NAME_SIZE];
    return world && temporary && SudekiMpLanStorySceneCharacterArea(s,c,w,t) &&
        !strcmp(w,world) && !strcmp(t,temporary);
}
int SudekiMpLanStorySceneSameArea(const SudekiMpLanStoryScene *s,unsigned int a,unsigned int b) {
    char wa[SUDEKIMP_LAN_STORY_NAME_SIZE],ta[SUDEKIMP_LAN_STORY_NAME_SIZE];
    return SudekiMpLanStorySceneCharacterArea(s,a,wa,ta) &&
        SudekiMpLanStorySceneCharacterAreaMatches(s,b,wa,ta);
}
unsigned int SudekiMpLanStoryViewSeat(const SudekiMpLanStoryScene *s,
    unsigned int player,unsigned int preferred) {
    if(player>=4u || !SudekiMpLanStorySceneValid(s) ||
        s->phase!=SUDEKIMP_LAN_STORY_READY) return SUDEKIMP_LAN_STORY_NO_SEAT;
    if(s->available_mask&(1u<<player)) return player;
    if(preferred<4u && (s->available_mask&(1u<<preferred))) return preferred;
    return s->leader_seat;
}
