#include "hooks/lan_story_avatar_spawn.h"
#include "hooks/lan_story_task_trace.h"
#include <string.h>

enum { SEATS=4, MAX_ENTITIES=8192, COMPONENTS=8 };
typedef struct Resource { uint32_t kind,id; char name[96]; } Resource;
typedef struct Registry { uint8_t *owner; void **entries; uint32_t count; } Registry;
typedef struct Group { uint8_t *owner; uint32_t count; void *actors[4]; } Group;
typedef struct Request {
    SudekiMpLanStoryAvatarSpawnObservation observation;
    BOOL occupied,scope_ended,complete_entered,destroy_entered,group_member;
    void *job;
    uint32_t placement[7];
    Resource resource;
    Registry before;
    void *existing[MAX_ENTITIES];
    Group group;
    void *components[COMPONENTS];
} Request;
static uint8_t *avatar_image;
static Request requests[SEATS];
static Request *active_request;
static struct {
    const void *owner;
    void *actor;
    uint32_t epoch;
    SudekiMpLanStoryAvatarSpawnGroupOperation operation;
    Group before;
} group_change;
static uint32_t last_generation[SEATS];
static struct { uint32_t epoch,load; void *world; BOOL exact; } last_exit;
static const unsigned offsets[COMPONENTS]={0x44,0x80,0x8c,0x90,0x94,0xa8,0xac,0xb8};
static const unsigned vtables[COMPONENTS]={0x2cdefc,0x2c8644,0x2d48d4,0x2cc9ac,0x2d4924,0x2d4abc,0x2d4b24,0x2d4bd4};
static const unsigned sizes[COMPONENTS]={0x104,0xc0,0x14,0x64,0x16c,0x64,0x54,0xb4};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a+n>=a && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL resource_copy(const void *p,Resource *r) {
    uint32_t words[3],backing[2]; memset(r,0,sizeof(*r));
    if(!readable(p,sizeof(words))) return FALSE;
    memcpy(words,p,sizeof(words));
    const void *ref=(const void *)(uintptr_t)words[2];
    if(words[1]==0x7ffffu || !readable(ref,sizeof(backing))) return FALSE;
    memcpy(backing,ref,sizeof(backing));
    if(!backing[0] || backing[0]==UINT32_MAX) return FALSE;
    const char *s=(const char *)(uintptr_t)backing[1];
    for(unsigned i=0;i<sizeof(r->name);++i) {
        if(!readable(s+i,1)) return FALSE;
        unsigned char c=(unsigned char)s[i];
        if(!c) {
            if(!i || !readable(s,i+1u) || memcmp(s,r->name,i) ||
                memcmp(ref,backing,sizeof(backing)) || memcmp(p,words,sizeof(words))) return FALSE;
            r->kind=words[0]&0x1fffu; r->id=words[1];
            /* B20D0 adds .SOL; B1310 resolves kind and removes extension. */
            return (r->kind&0x7fu)==2u && !_stricmp(r->name,"ALLY_TALOS");
        }
        if(c<0x20u || c>0x7eu) return FALSE;
        r->name[i]=(char)c;
    }
    return FALSE;
}
static BOOL resource_same(const Resource *a,const Resource *b) {
    return a->id==b->id && (a->kind&0x7fu)==(b->kind&0x7fu) &&
        !_stricmp(a->name,b->name);
}
static BOOL registry(Registry *r) {
    r->owner=*(uint8_t **)(avatar_image+0x409d8c);
    if(!readable(r->owner,0x40)) return FALSE;
    r->count=*(uint32_t *)(r->owner+0x34); r->entries=*(void ***)(r->owner+0x3c);
    return r->count && r->count<=MAX_ENTITIES && readable(r->entries,r->count*sizeof(void *));
}
static BOOL registry_still(const Registry *r) {
    return *(void **)(avatar_image+0x409d8c)==r->owner && readable(r->owner,0x40) &&
        *(uint32_t *)(r->owner+0x34)==r->count && *(void **)(r->owner+0x3c)==r->entries;
}
static BOOL group_copy(Group *g) {
    g->owner=*(uint8_t **)(avatar_image+0x408d94);
    if(!readable(g->owner,0xd0)) return FALSE;
    g->count=*(uint32_t *)(g->owner+0xcc);
    if(!g->count || g->count>4u) return FALSE;
    for(unsigned i=0;i<4;++i) g->actors[i]=*(void **)(g->owner+0x90+i*12u);
    return TRUE;
}
static BOOL group_same(const Group *a,const Group *b,void *actor,BOOL member) {
    if(a->owner!=b->owner || a->count!=b->count || memcmp(a->actors,b->actors,sizeof(a->actors))) return FALSE;
    unsigned found=0;
    for(unsigned i=0;i<a->count;++i) if(a->actors[i]==actor && actor) ++found;
    return found==(member?1u:0u);
}
static BOOL actor_identity(Request *r,void *actor,BOOL retain_components) {
    Registry current; Group group; Resource name; uint8_t *e=actor;
    /* Check catalogue membership before dereferencing the borrowed actor. */
    if(*(void **)(avatar_image+0x408d10)!=r->observation.world ||
        !registry(&current) || current.owner!=r->before.owner) return FALSE;
    unsigned count=0;
    for(unsigned i=0;i<current.count;++i) if(current.entries[i]==actor) ++count;
    if(count!=1 || !registry_still(&current) || !group_copy(&group) ||
        !group_same(&r->group,&group,actor,r->group_member)) return FALSE;
    for(unsigned i=0;i<r->before.count;++i) if(r->existing[i]==actor) return FALSE;
    for(unsigned i=0;i<SEATS;++i) if(&requests[i]!=r && requests[i].occupied &&
        requests[i].observation.actor==actor) return FALSE;
    if(!readable(e,0x138) || *(void **)e!=avatar_image+0x2d55d4 ||
        *(void **)(e+8)!=avatar_image+0x2d55f8 || *(void **)(e+0x2c)!=avatar_image+0x2d5618 ||
        !resource_copy(e+0x30,&name) || name.kind!=0xf82u || !resource_same(&name,&r->resource)) return FALSE;
    for(unsigned i=0;i<COMPONENTS;++i) {
        uint8_t *p=*(uint8_t **)(e+offsets[i]);
        if(!readable(p,sizes[i]) || *(void **)p!=avatar_image+vtables[i] ||
            *(void **)(p+0x10)!=e || (!retain_components && r->components[i]!=p)) return FALSE;
        if(retain_components) r->components[i]=p;
    }
    return registry_still(&current);
}
static BOOL job_identity(Request *r,unsigned stage) {
    uint8_t *j=r->job; Resource name;
    return readable(j,0x58) && *(void **)j==avatar_image+0x2cb9a8 &&
        j[0x54]==stage && j[0x55]==1 && *(int32_t *)(j+0x50)==-1 &&
        !memcmp(j+0x1c,r->placement,2) && !memcmp(j+0x20,(uint8_t *)r->placement+4,24) &&
        resource_copy(j+0x10,&name) && resource_same(&name,&r->resource);
}
static void unknown_all(void) {
    for(unsigned i=0;i<SEATS;++i) if(requests[i].occupied) requests[i].observation.unknown=TRUE;
}
static void observe_event(const void *consumer,const SudekiMpLanStoryEntitySetupEvent *e) {
    if(consumer!=requests || !avatar_image || !e) return;
    if(e->phase==SUDEKIMP_ENTITY_SETUP_WORLD_EXIT) {
        /* Already witnessed by the shared owner AFTER native Quit. */
        memset(&last_exit,0,sizeof(last_exit));
        for(unsigned i=0;i<SEATS;++i) if(requests[i].occupied) {
            const SudekiMpLanStoryAvatarSpawnObservation *o=&requests[i].observation;
            if(!last_exit.world) {
                last_exit.epoch=o->epoch; last_exit.load=o->load_generation;
                last_exit.world=o->world; last_exit.exact=TRUE;
            }
            if(!o->world || o->epoch!=last_exit.epoch || o->world!=last_exit.world ||
                o->load_generation!=last_exit.load || o->load_generation!=e->load_generation)
                last_exit.exact=FALSE;
        }
        memset(requests,0,sizeof(requests)); active_request=NULL;
        memset(&group_change,0,sizeof(group_change)); return;
    }
    if(e->phase==SUDEKIMP_ENTITY_SETUP_UNKNOWN) { unknown_all(); return; }
    Request *r=NULL;
    for(unsigned i=0;i<SEATS;++i) if(requests[i].occupied && requests[i].job==e->job) {
        r=&requests[i]; break;
    }
    if(e->phase==SUDEKIMP_ENTITY_SETUP_CTOR_BEGIN) {
        if(r && !r->observation.job_storage_released) r->observation.unknown=TRUE;
        if(!active_request) return;
        r=active_request;
        if(r->job || !e->job || !readable(e->subject,28)) { r->observation.unknown=TRUE; return; }
        r->job=e->job; memcpy(r->placement,e->subject,28); return;
    }
    if(!r || r->observation.unknown) return;
    if(e->load_generation!=r->observation.load_generation) { r->observation.unknown=TRUE; return; }
    switch(e->phase) {
    case SUDEKIMP_ENTITY_SETUP_CTOR_END:
        r->observation.construction_exact=e->result==r->job && active_request==r &&
            readable(r->job,0x58) && resource_copy((uint8_t *)r->job+0x10,&r->resource) && job_identity(r,0);
        if(!r->observation.construction_exact) r->observation.unknown=TRUE;
        break;
    case SUDEKIMP_ENTITY_SETUP_COMPLETE_BEGIN:
        if(!r->scope_ended || !r->observation.construction_exact || r->complete_entered ||
            r->observation.completion_returned || !job_identity(r,4) ||
            !actor_identity(r,(void *)e->subject,TRUE)) r->observation.unknown=TRUE;
        else { r->complete_entered=TRUE; r->observation.actor=(void *)e->subject; }
        break;
    case SUDEKIMP_ENTITY_SETUP_COMPLETE_END:
        if(!r->complete_entered || e->subject!=r->observation.actor ||
            !job_identity(r,4) || !actor_identity(r,r->observation.actor,FALSE)) r->observation.unknown=TRUE;
        else { r->complete_entered=FALSE; r->observation.completion_returned=TRUE; }
        break;
    case SUDEKIMP_ENTITY_SETUP_DESTROY_BEGIN:
        if(r->destroy_entered || !r->observation.completion_returned || !job_identity(r,4) || !(e->flags&1u))
            r->observation.unknown=TRUE;
        else r->destroy_entered=TRUE;
        break;
    case SUDEKIMP_ENTITY_SETUP_DESTROY_END:
        /* No job access after its deleting destructor, even for validation. */
        if(!r->destroy_entered || e->result!=r->job || !(e->flags&1u)) r->observation.unknown=TRUE;
        else { r->destroy_entered=FALSE; r->observation.job_storage_released=TRUE; r->job=NULL; }
        break;
    default: break;
    }
}
static BOOL thread_status(SudekiMpLanStoryTaskTraceStatus *s) {
    return SudekiMpLanStoryTaskTraceGetStatus(s) && !s->unknown && s->load_generation &&
        SudekiMpLanStoryTaskTraceEntitySetupExact((HMODULE)avatar_image);
}
BOOL SudekiMpLanStoryAvatarSpawnBegin(HMODULE image,unsigned seat,uint32_t epoch,uint32_t generation) {
    SudekiMpLanStoryTaskTraceStatus s;
    if(!image || seat>=SEATS || !epoch || !generation || active_request || group_change.owner || requests[seat].occupied ||
        generation<=last_generation[seat] || (avatar_image && avatar_image!=(uint8_t *)image)) return FALSE;
    BOOL attached=avatar_image!=NULL;
    if(!attached) {
        if(!SudekiMpLanStoryTaskTraceEntitySetupAttach(image,requests,observe_event)) return FALSE;
        avatar_image=(uint8_t *)image;
    }
    if(!thread_status(&s)) return FALSE;
    Request *r=&requests[seat]; memset(r,0,sizeof(*r));
    void *world=*(void **)(avatar_image+0x408d10);
    if(!readable(world,0x39b) || !registry(&r->before) || !group_copy(&r->group)) return FALSE;
    memcpy(r->existing,r->before.entries,r->before.count*sizeof(void *));
    if(!registry_still(&r->before) || memcmp(r->existing,r->before.entries,r->before.count*sizeof(void *))) return FALSE;
    r->observation=(SudekiMpLanStoryAvatarSpawnObservation){.epoch=epoch,.generation=generation,
        .load_generation=s.load_generation,.seat=seat,.world=world};
    memset(&last_exit,0,sizeof(last_exit));
    r->occupied=TRUE; last_generation[seat]=generation; active_request=r; return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnEnd(unsigned seat,uint32_t epoch,uint32_t generation) {
    SudekiMpLanStoryTaskTraceStatus s;
    Request *r=active_request;
    if(!r || seat>=SEATS || r!=&requests[seat] || r->observation.epoch!=epoch ||
        r->observation.generation!=generation || !thread_status(&s)) return FALSE;
    active_request=NULL; r->scope_ended=TRUE;
    if(s.load_generation!=r->observation.load_generation ||
        *(void **)(avatar_image+0x408d10)!=r->observation.world || !r->observation.construction_exact)
        r->observation.unknown=TRUE;
    return !r->observation.unknown;
}
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned seat,uint32_t epoch,uint32_t generation,
    SudekiMpLanStoryAvatarSpawnObservation *out) {
    SudekiMpLanStoryTaskTraceStatus s;
    if(!out || seat>=SEATS || !avatar_image || active_request || group_change.owner || !thread_status(&s)) return FALSE;
    Request *r=&requests[seat];
    if(!r->occupied || r->observation.epoch!=epoch || r->observation.generation!=generation) return FALSE;
    r->observation.ready=FALSE;
    if(s.load_generation!=r->observation.load_generation ||
        *(void **)(avatar_image+0x408d10)!=r->observation.world) r->observation.unknown=TRUE;
    if(!r->observation.unknown && r->observation.completion_returned && r->observation.job_storage_released) {
        if(!actor_identity(r,r->observation.actor,FALSE)) r->observation.unknown=TRUE;
        else r->observation.ready=!(*(uint32_t *)((uint8_t *)r->components[3]+0x50)&0x400u);
    }
    *out=r->observation; return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnGroupBegin(const void *owner,uint32_t epoch,
    SudekiMpLanStoryAvatarSpawnGroupOperation operation,void *actor) {
    SudekiMpLanStoryTaskTraceStatus s; Group before;
    if(!owner || !actor || !epoch || !avatar_image || active_request || group_change.owner ||
        operation<SUDEKIMP_AVATAR_SPAWN_GROUP_ADD || operation>SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE ||
        !thread_status(&s) || !group_copy(&before)) return FALSE;
    unsigned occupied=0,avatar=0,index=4;
    for(unsigned i=0;i<before.count;++i) if(before.actors[i]==actor) index=i;
    for(unsigned i=0;i<SEATS;++i) if(requests[i].occupied) {
        SudekiMpLanStoryAvatarSpawnObservation o;
        Request *r=&requests[i]; ++occupied;
        if(r->observation.epoch!=epoch || !SudekiMpLanStoryAvatarSpawnObserve(i,epoch,r->observation.generation,&o) ||
            !o.ready || o.unknown || !group_same(&r->group,&before,o.actor,r->group_member)) return FALSE;
        if(o.actor==actor) ++avatar;
    }
    if(!occupied || (operation==SUDEKIMP_AVATAR_SPAWN_GROUP_ADD && (avatar!=1 || index!=4 || before.count>=4)) ||
        (operation==SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE && (avatar || index==0 || index>=before.count || before.count<=1)) ||
        (operation==SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE && (avatar!=1 || index>=before.count))) return FALSE;
    group_change.owner=owner; group_change.actor=actor; group_change.epoch=epoch;
    group_change.operation=operation; group_change.before=before; return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnGroupEnd(const void *owner,BOOL *changed) {
    SudekiMpLanStoryTaskTraceStatus s; Group after,expected=group_change.before;
    if(changed) *changed=FALSE;
    if(!owner || owner!=group_change.owner || !changed || !thread_status(&s)) return FALSE;
    BOOL exact=group_copy(&after) && after.owner==expected.owner;
    BOOL unchanged=exact && after.count==expected.count && !memcmp(after.actors,expected.actors,sizeof(after.actors));
    if(exact && !unchanged) {
        switch(group_change.operation) {
        case SUDEKIMP_AVATAR_SPAWN_GROUP_ADD:
            expected.actors[expected.count++]=group_change.actor;
            exact=after.count==expected.count && !memcmp(after.actors,expected.actors,sizeof(after.actors)); break;
        case SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE: {
            unsigned i=0; while(i<expected.count && expected.actors[i]!=group_change.actor) ++i;
            if(i>=expected.count) { exact=FALSE; break; }
            for(;i+1<expected.count;++i) expected.actors[i]=expected.actors[i+1];
            expected.actors[--expected.count]=NULL;
            exact=after.count==expected.count && !memcmp(after.actors,expected.actors,sizeof(after.actors)); break;
        }
        case SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE:
            exact=after.count==expected.count && after.actors[0]==group_change.actor;
            for(unsigned i=0;exact && i<expected.count;++i) {
                unsigned found=0;
                for(unsigned j=0;j<after.count;++j) if(expected.actors[i]==after.actors[j]) ++found;
                if(found!=1) exact=FALSE;
            }
            for(unsigned i=after.count;exact && i<4;++i) if(after.actors[i]) exact=FALSE;
            break;
        default: exact=FALSE;
        }
    }
    if(exact) {
        for(unsigned i=0;i<SEATS;++i) if(requests[i].occupied) {
            Request *r=&requests[i];
            if(r->observation.epoch!=group_change.epoch || r->observation.load_generation!=s.load_generation) { exact=FALSE; break; }
            r->group=after; r->group_member=FALSE;
            for(unsigned j=0;j<after.count;++j) if(after.actors[j]==r->observation.actor) r->group_member=TRUE;
        }
        for(unsigned i=0;exact && i<SEATS;++i) if(requests[i].occupied &&
            !actor_identity(&requests[i],requests[i].observation.actor,FALSE)) exact=FALSE;
    }
    if(!exact) unknown_all();
    else *changed=!unchanged;
    memset(&group_change,0,sizeof(group_change)); return exact;
}
BOOL SudekiMpLanStoryAvatarSpawnShutdown(void) {
    if(!avatar_image) return TRUE;
    if(active_request || group_change.owner) return FALSE;
    for(unsigned i=0;i<SEATS;++i) if(requests[i].occupied) return FALSE;
    if(!SudekiMpLanStoryTaskTraceEntitySetupDetach((HMODULE)avatar_image,requests,observe_event)) return FALSE;
    avatar_image=NULL; memset(&last_exit,0,sizeof(last_exit)); return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnWorldExited(uint32_t epoch,uint32_t load,void *world) {
    return avatar_image && epoch && load && world && last_exit.exact &&
        last_exit.epoch==epoch && last_exit.load==load && last_exit.world==world &&
        SudekiMpLanStoryTaskHostExact((HMODULE)avatar_image);
}
