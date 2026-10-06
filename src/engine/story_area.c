#include "engine/story_area.h"
#include <string.h>

static int name_valid(const char *s,int empty) {
    if(!s || (!empty && !*s)) return 0;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_NAME;++i) {
        unsigned char c=(unsigned char)s[i];
        if(!c) return 1;
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-')) return 0;
    }
    return 0;
}
int SudekiMpStoryAreaRefSame(SudekiMpStoryAreaRef a,SudekiMpStoryAreaRef b) {
    return a.session && a.lifetime && a.session==b.session &&
        a.lifetime==b.lifetime && a.slot==b.slot;
}
static const SudekiMpStoryAreaRecord *area(const SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    if(!s || !s->session || r.session!=s->session || r.slot>=SUDEKIMP_STORY_AREAS) return NULL;
    const SudekiMpStoryAreaRecord *a=&s->areas[r.slot];
    return a->phase && SudekiMpStoryAreaRefSame(a->ref,r)?a:NULL;
}
static int usable(const SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    const SudekiMpStoryAreaRecord *a=area(s,r);
    return a && (a->phase==SUDEKIMP_STORY_AREA_LOADING || a->phase==SUDEKIMP_STORY_AREA_READY);
}
static int ready(const SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    const SudekiMpStoryAreaRecord *a=area(s,r);
    return a && a->phase==SUDEKIMP_STORY_AREA_READY;
}
static int owner_valid(SudekiMpStoryAreaOwner o) {
    return o.session && o.player<SUDEKIMP_STORY_AREA_PLAYERS && o.character<4u &&
        o.actor_generation && o.connection_generation;
}
static int owner_same(SudekiMpStoryAreaOwner a,SudekiMpStoryAreaOwner b) {
    return a.session==b.session && a.player==b.player && a.character==b.character &&
        a.actor_generation==b.actor_generation && a.connection_generation==b.connection_generation;
}
static const SudekiMpStoryAreaPlayer *player(const SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o) {
    if(!s || !s->session || !owner_valid(o) || o.session!=s->session) return NULL;
    const SudekiMpStoryAreaPlayer *p=&s->players[o.player];
    return p->bound && owner_same(p->owner,o)?p:NULL;
}
static int ticket_exact(const SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,uint64_t ticket,
    unsigned phase) {
    const SudekiMpStoryAreaPlayer *p=player(s,o);
    return p && ticket && p->travel.ticket==ticket && p->travel.phase==phase;
}
int SudekiMpStoryAreasInitialize(SudekiMpStoryAreas *s,uint64_t session) {
    if(!s || !session) return 0;
    const unsigned char *bytes=(const unsigned char *)s;
    for(unsigned i=0;i<sizeof(*s);++i) if(bytes[i]) return 0;
    s->session=session; return 1;
}
int SudekiMpStoryAreaLoad(SudekiMpStoryAreas *s,const char *world,const char *temporary,
    SudekiMpStoryAreaRef *out) {
    if(!s || !s->session || !out || !name_valid(world,0) || !name_valid(temporary,1)) return 0;
    unsigned slot=*temporary?1u:0u;
    SudekiMpStoryAreaRecord *a=&s->areas[slot];
    if(a->phase) {
        if(!usable(s,a->ref) || strcmp(a->world,world) || strcmp(a->temporary,temporary)) return 0;
        *out=a->ref; return 1;
    }
    if(slot && (!ready(s,s->areas[0].ref) || strcmp(s->areas[0].world,world))) return 0;
    if(s->next_lifetime==UINT64_MAX) return 0;
    SudekiMpStoryAreaRecord fresh={0};
    fresh.ref.session=s->session; fresh.ref.lifetime=++s->next_lifetime; fresh.ref.slot=slot;
    fresh.phase=SUDEKIMP_STORY_AREA_LOADING;
    if(slot) fresh.parent=s->areas[0].ref;
    memcpy(fresh.world,world,strlen(world)+1u);
    memcpy(fresh.temporary,temporary,strlen(temporary)+1u);
    *a=fresh; *out=fresh.ref; return 1;
}
int SudekiMpStoryAreaReady(SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    if(!usable(s,r)) return 0;
    s->areas[r.slot].phase=SUDEKIMP_STORY_AREA_READY; return 1;
}
int SudekiMpStoryAreaBind(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,SudekiMpStoryAreaRef r) {
    if(!s || !owner_valid(o) || o.session!=s->session || !ready(s,r)) return 0;
    SudekiMpStoryAreaPlayer *p=&s->players[o.player];
    if(p->bound) return p->connected && owner_same(p->owner,o) && SudekiMpStoryAreaRefSame(p->area,r);
    if(o.connection_generation<=s->connection_floor[o.player]) return 0;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PLAYERS;++i)
        if(s->players[i].bound && s->players[i].owner.character==o.character) return 0;
    SudekiMpStoryAreaPlayer fresh={0};
    fresh.owner=o; fresh.area=r; fresh.bound=fresh.connected=1;
    s->connection_floor[o.player]=o.connection_generation;
    *p=fresh; return 1;
}
int SudekiMpStoryAreaInputAllowed(const SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o) {
    const SudekiMpStoryAreaPlayer *p=player(s,o);
    return p && p->connected && ready(s,p->area) &&
        (p->travel.phase==SUDEKIMP_STORY_TRAVEL_NONE || p->travel.phase==SUDEKIMP_STORY_TRAVEL_REQUESTED);
}
int SudekiMpStoryAreaRequest(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,
    SudekiMpStoryAreaRef destination,uint64_t *out) {
    const SudekiMpStoryAreaPlayer *p=player(s,o);
    if(!p || !out || !p->connected || !ready(s,p->area) || !usable(s,destination) ||
        SudekiMpStoryAreaRefSame(p->area,destination)) return 0;
    if(p->travel.phase) {
        if(p->travel.phase!=SUDEKIMP_STORY_TRAVEL_REQUESTED ||
            !SudekiMpStoryAreaRefSame(p->travel.destination,destination)) return 0;
        *out=p->travel.ticket; return 1;
    }
    if(s->next_ticket==UINT64_MAX) return 0;
    SudekiMpStoryTravel t={0};
    t.ticket=++s->next_ticket; t.source=p->area; t.destination=destination;
    t.phase=SUDEKIMP_STORY_TRAVEL_REQUESTED;
    s->players[o.player].travel=t; *out=t.ticket; return 1;
}
int SudekiMpStoryAreaCancelRequest(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,uint64_t t) {
    if(!ticket_exact(s,o,t,SUDEKIMP_STORY_TRAVEL_REQUESTED)) return 0;
    memset(&s->players[o.player].travel,0,sizeof(s->players[o.player].travel)); return 1;
}
int SudekiMpStoryAreaStarted(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,uint64_t t,uint64_t task) {
    if(!task || !ticket_exact(s,o,t,SUDEKIMP_STORY_TRAVEL_REQUESTED) ||
        !SudekiMpStoryAreaInputAllowed(s,o)) return 0;
    SudekiMpStoryTravel *v=&s->players[o.player].travel;
    if(!ready(s,v->source) || !usable(s,v->destination)) return 0;
    v->native_task=task; v->phase=SUDEKIMP_STORY_TRAVEL_ACTIVE; return 1;
}
int SudekiMpStoryAreaNativeReturned(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,
    uint64_t t,uint64_t task,SudekiMpStoryAreaRef actual) {
    if(!task || !ticket_exact(s,o,t,SUDEKIMP_STORY_TRAVEL_ACTIVE) || !ready(s,actual)) return 0;
    SudekiMpStoryAreaPlayer *p=&s->players[o.player];
    SudekiMpStoryTravel *v=&p->travel;
    if(v->native_task!=task || (!SudekiMpStoryAreaRefSame(actual,v->source) &&
        !SudekiMpStoryAreaRefSame(actual,v->destination))) return 0;
    p->area=actual; v->native_task=0; v->phase=SUDEKIMP_STORY_TRAVEL_SETTLED; return 1;
}
int SudekiMpStoryAreaTravelReleased(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,uint64_t t) {
    if(!ticket_exact(s,o,t,SUDEKIMP_STORY_TRAVEL_SETTLED)) return 0;
    memset(&s->players[o.player].travel,0,sizeof(s->players[o.player].travel)); return 1;
}
int SudekiMpStoryAreaRevoke(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o) {
    if(!player(s,o)) return 0;
    SudekiMpStoryAreaPlayer *p=&s->players[o.player];
    p->connected=0;
    if(p->travel.phase==SUDEKIMP_STORY_TRAVEL_REQUESTED)
        memset(&p->travel,0,sizeof(p->travel));
    return 1;
}
int SudekiMpStoryAreaUnbind(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o) {
    const SudekiMpStoryAreaPlayer *p=player(s,o);
    if(!p || p->connected || p->travel.phase) return 0;
    memset(&s->players[o.player],0,sizeof(s->players[o.player])); return 1;
}
int SudekiMpStoryAreaRetain(SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r,uint64_t *out) {
    if(!out || !usable(s,r) || s->next_pin==UINT64_MAX) return 0;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i) if(!s->pins[i].serial) {
        s->pins[i].serial=++s->next_pin; s->pins[i].area=r;
        *out=s->pins[i].serial; return 1;
    }
    return 0;
}
int SudekiMpStoryAreaRelease(SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r,uint64_t pin) {
    if(!pin || !area(s,r)) return 0;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i)
        if(s->pins[i].serial==pin && SudekiMpStoryAreaRefSame(s->pins[i].area,r)) {
            memset(&s->pins[i],0,sizeof(s->pins[i])); return 1;
        }
    return 0;
}
static int needed(const SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PLAYERS;++i) {
        const SudekiMpStoryAreaPlayer *p=&s->players[i];
        if(!p->bound) continue;
        if(SudekiMpStoryAreaRefSame(p->area,r) || (p->travel.phase &&
            (SudekiMpStoryAreaRefSame(p->travel.source,r) ||
             SudekiMpStoryAreaRefSame(p->travel.destination,r)))) return 1;
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_AREAS;++i)
        if(s->areas[i].phase && SudekiMpStoryAreaRefSame(s->areas[i].parent,r)) return 1;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i)
        if(s->pins[i].serial && SudekiMpStoryAreaRefSame(s->pins[i].area,r)) return 1;
    return 0;
}
int SudekiMpStoryAreaRetireBegin(SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    if(!usable(s,r) || needed(s,r)) return 0;
    s->areas[r.slot].phase=SUDEKIMP_STORY_AREA_RETIRING; return 1;
}
int SudekiMpStoryAreaRetireReturned(SudekiMpStoryAreas *s,SudekiMpStoryAreaRef r) {
    const SudekiMpStoryAreaRecord *a=area(s,r);
    if(!a || a->phase!=SUDEKIMP_STORY_AREA_RETIRING || needed(s,r)) return 0;
    memset(&s->areas[r.slot],0,sizeof(s->areas[r.slot])); return 1;
}
