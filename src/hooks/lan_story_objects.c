#include "hooks/lan_story_objects.h"
#include "engine/build_identity.h"
#include <stdlib.h>
#include <string.h>

enum { MAX_REGISTRY=8192,MAX_ZONES=206,MAX_SPAWNS=4096 };
typedef struct ZoneBorrow {
    uint8_t *owner,*data,*descriptor,*spawns;
    uint32_t state,count;
} ZoneBorrow;
typedef struct SpawnBorrow {
    uint8_t *spawn;
    uint32_t zone;
    uint8_t bytes[0x90];
} SpawnBorrow;
typedef struct Scratch {
    uint8_t *owner,*world;
    void **entries;
    uint32_t count,objects;
    void *entities[MAX_REGISTRY];
    uint8_t matched[MAX_REGISTRY];
    ZoneBorrow zones[MAX_ZONES];
    SpawnBorrow spawns[SUDEKIMP_STORY_PLACEMENT_MAX];
    SudekiMpLanStoryObjectSnapshot result;
} Scratch;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL image_exact(uint8_t *b) {
    static const uint8_t setup[]={0x8d,0x81,0xbc,0x05,0,0};
    static const uint8_t hit[]={0x53,0x8b,0xd9,0x83,0x7b,0x64,0x4d};
    static const uint8_t position[]={0x8d,0x41,0x40,0xc3};
    /* File hash is checked by the loader. These loaded-image checks fence the
     * exact entity and Zone::Spawn layouts; no native method is executed. */
    return b && SudekiMpCheckLoadedExecutable((HMODULE)b) &&
        readable(b+0x145b90u,sizeof(setup)) && !memcmp(b+0x145b90u,setup,sizeof(setup)) &&
        readable(b+0x150d70u,sizeof(hit)) && !memcmp(b+0x150d70u,hit,sizeof(hit)) &&
        readable(b+0x13d620u,sizeof(position)) && !memcmp(b+0x13d620u,position,sizeof(position)) &&
        readable(b+0x2c869cu,0x68u) && readable(b+0x2c8a8cu,0x48u) && readable(b+0x2cddf4u,12u) &&
        *(void **)(b+0x2c8aa8u)==b+0x145b90u &&
        *(void **)(b+0x2c8700u)==b+0x150d70u && *(void **)(b+0x2c86dcu)==b+0x151130u &&
        *(void **)(b+0x2cddf4u)==b+0x13cfc0u && *(void **)(b+0x2cddf8u)==b+0x13d620u;
}
static BOOL registry_still(uint8_t *b,const Scratch *s) {
    return readable(b+0x409d8cu,4) && *(void **)(b+0x409d8cu)==s->owner &&
        readable(s->owner,0x40u) && *(uint32_t *)(s->owner+0x34u)==s->count &&
        *(void ***)(s->owner+0x3cu)==s->entries &&
        readable(s->entries,s->count*sizeof(void *)) &&
        !memcmp(s->entities,s->entries,s->count*sizeof(void *));
}
static BOOL zone(uint8_t *b,uint8_t *world,unsigned index,ZoneBorrow *out) {
    ZoneBorrow z={0};
    if(index>=MAX_ZONES || !readable(world,0x390u)) return FALSE;
    z.owner=*(uint8_t **)(world+0x58u+index*4u);
    if(z.owner) {
        if(!readable(z.owner,0x54u) || *(void **)z.owner!=b+0x2c82a4u) return FALSE;
        z.state=*(uint32_t *)(z.owner+0x34u);
        z.data=*(uint8_t **)(z.owner+(z.state==1u?0x18u:0x14u));
        if(z.data) {
            if(!readable(z.data,0x12au) || *(void **)z.data!=b+0x2cdcdcu ||
                *(uint32_t *)(z.data+0x24u)!=index) return FALSE;
            z.descriptor=*(uint8_t **)(z.data+0x118u);
            if(!readable(z.descriptor,0x1cu)) return FALSE;
            z.count=*(uint32_t *)(z.descriptor+0x10u); z.spawns=*(uint8_t **)(z.descriptor+0x18u);
            if(z.count>MAX_SPAWNS || (z.count && !readable(z.spawns,z.count*0x90u))) return FALSE;
        }
    }
    *out=z; return TRUE;
}
static BOOL native_name(uint32_t *reference,char out[SUDEKIMP_STORY_PLACEMENT_NAME]) {
    if(!readable(reference,8u) || !reference[0] || reference[0]==UINT32_MAX || !reference[1]) return FALSE;
    const char *p=(const char *)(uintptr_t)reference[1];
    memset(out,0,SUDEKIMP_STORY_PLACEMENT_NAME);
    for(unsigned i=0;i<SUDEKIMP_STORY_PLACEMENT_NAME;++i) {
        if(!readable(p+i,1u)) return FALSE;
        out[i]=p[i]; if(!out[i]) return i!=0;
    }
    return FALSE;
}
static BOOL key(uint8_t *b,uint8_t *s,unsigned region,SudekiMpStoryPlacement *out) {
    if(!readable(s,0x90u) || *(void **)s!=b+0x2cddf4u || *(void **)(s+4u)!=b+0x2cde04u ||
        (*(uint32_t *)(s+0x6cu)&0x1fffu)!=0xf84u) return FALSE;
    char anchor[SUDEKIMP_STORY_PLACEMENT_NAME],name[SUDEKIMP_STORY_PLACEMENT_NAME];
    float position[3],forward[3];
    if(!native_name(*(uint32_t **)(s+0x58u),anchor) || !native_name(*(uint32_t **)(s+0x74u),name)) return FALSE;
    /* Exact Zone::Spawn serializer names these fields Position/Facing X/Y/Z.
     * They are authored placement, independent of the runtime resource+78. */
    memcpy(position,s+0x40u,sizeof(position)); memcpy(forward,s+0x30u,sizeof(forward));
    return SudekiMpStoryPlacementMake(out,region,*(uint32_t *)(s+0x54u),anchor,
        *(uint32_t *)(s+0x70u),name,position,forward);
}
static BOOL entity(uint8_t *b,uint8_t *e,const SudekiMpStoryPlacement *p,uint8_t *phase) {
    if(!readable(e,0x150u) || *(void **)e!=b+0x2c8a8cu || *(void **)(e+8u)!=b+0x2c8ab0u ||
        *(void **)(e+0x2cu)!=b+0x2c8ad0u || *(uint32_t *)(e+0x34u)!=p->resource) return FALSE;
    uint8_t *position=*(uint8_t **)(e+0x44u),*component=*(uint8_t **)(e+0x128u),*model=*(uint8_t **)(e+0x130u);
    if(position!=e+0x150u || component!=e+0x70cu || model!=e+0x3a4u ||
        *(void **)(e+0x48u)!=component || *(void **)(e+0x58u)!=model ||
        !readable(position,0xb8u) || !readable(component,0x80u) || !readable(model,0x14u) ||
        *(void **)position!=b+0x2cdefcu || *(void **)(position+0x10u)!=e ||
        *(void **)component!=b+0x2c869cu || *(void **)(component+0x10u)!=e ||
        *(void **)model!=b+0x2c8504u || *(void **)(model+0x10u)!=e ||
        *(uint32_t *)(component+0x7cu)>5 || *(uintptr_t *)(position+0x94u)) return FALSE;
    char name[SUDEKIMP_STORY_PLACEMENT_NAME];
    if(!native_name(*(uint32_t **)(e+0x38u),name) || memcmp(name,p->name,sizeof(name))) return FALSE;
    uint8_t *wrapper=*(uint8_t **)(position+0xb4u);
    if(!readable(wrapper,0x14u) || *(void **)wrapper!=b+0x2d1df0u) return FALSE;
    uint8_t *object=*(uint8_t **)(wrapper+8u),*renderer=*(uint8_t **)(wrapper+0x10u);
    if(!readable(object,0x38u) || !readable(renderer,4u) ||
        *(void **)object!=b+0x2dd700u || *(void **)renderer!=b+0x2df8ecu ||
        *(void **)(object+0x14u)!=renderer || *(void **)(object+0x18u)) return FALSE;
    *phase=(uint8_t)*(uint32_t *)(component+0x7cu); return TRUE;
}
static BOOL observe_row(uint8_t *b,Scratch *s,SpawnBorrow *row,SudekiMpStoryPlacement *p,
    uint8_t *present,uint8_t *phase,BOOL record) {
    if(!key(b,row->spawn,row->zone,p)) return FALSE;
    uintptr_t resource=*(uintptr_t *)(row->spawn+0x78u);
    *present=0; *phase=0;
    if(resource) {
        if(resource<=0x2cu) return FALSE;
        uint8_t *e=(uint8_t *)(resource-0x2cu); unsigned index=MAX_REGISTRY,matches=0;
        for(unsigned i=0;i<s->count;++i) if(s->entities[i]==e) {index=i;++matches;}
        if(matches!=1 || (record && s->matched[index]) || !entity(b,e,p,phase)) return FALSE;
        if(record) s->matched[index]=1;
        *present=1;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryObjectsObserve(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,SudekiMpLanStoryObjectSnapshot *out) {
    uint8_t *b=(uint8_t *)image;
    if(!out || !w || !r || !w->service_post_original_exact ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) || !image_exact(b)) return FALSE;
    Scratch *s=calloc(1,sizeof(*s));
    if(!s) {SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    BOOL ok=FALSE;
    s->world=r->world;
    if(!readable(b+0x409d8cu,4) || !readable(b+0x408d10u,4) || *(void **)(b+0x408d10u)!=s->world) goto done;
    s->owner=*(uint8_t **)(b+0x409d8cu);
    if(!readable(s->owner,0x40u)) goto done;
    s->count=*(uint32_t *)(s->owner+0x34u); s->entries=*(void ***)(s->owner+0x3cu);
    if(!s->count || s->count>MAX_REGISTRY || !readable(s->entries,s->count*sizeof(void *))) goto done;
    memcpy(s->entities,s->entries,s->count*sizeof(void *));
    if(!registry_still(b,s)) goto done;
    for(unsigned z=0;z<MAX_ZONES;++z) {
        if(!zone(b,s->world,z,&s->zones[z])) goto done;
        for(unsigned i=0;i<s->zones[z].count;++i) {
            uint8_t *sp=s->zones[z].spawns+i*0x90u;
            if(*(void **)sp!=b+0x2cddf4u || *(void **)(sp+4u)!=b+0x2cde04u) goto done;
            if((*(uint32_t *)(sp+0x6cu)&0x1fffu)!=0xf84u) continue;
            if(s->objects==SUDEKIMP_STORY_PLACEMENT_MAX) goto done;
            unsigned n=s->objects;
            SpawnBorrow *row=&s->spawns[n]; row->zone=z; row->spawn=sp; memcpy(row->bytes,sp,0x90u);
            if(!observe_row(b,s,row,&s->result.placements[n],&s->result.present[n],&s->result.native_phase[n],TRUE)) goto done;
            for(unsigned j=0;j<n;++j) {
                const SudekiMpStoryPlacement *a=&s->result.placements[j],*p=&s->result.placements[n];
                if(a->zone==p->zone && a->anchor==p->anchor && !strcmp(a->anchor_name,p->anchor_name)) goto done;
            }
            ++s->objects;
        }
    }
    for(unsigned i=0;i<s->count;++i) {
        if(!readable(s->entities[i],4u)) goto done;
        if(*(void **)s->entities[i]==b+0x2c8a8cu && !s->matched[i]) goto done; /* no guessed dynamic identity */
    }
    if(!registry_still(b,s) || !SudekiMpLanStoryObserverRosterStillExact(w,r)) goto done;
    for(unsigned z=0;z<MAX_ZONES;++z) {
        ZoneBorrow check;
        if(!zone(b,s->world,z,&check) || memcmp(&check,&s->zones[z],sizeof(check))) goto done;
    }
    for(unsigned i=0;i<s->objects;++i) {
        SudekiMpStoryPlacement p; uint8_t present,phase;
        if(!readable(s->spawns[i].spawn,0x90u) || memcmp(s->spawns[i].spawn,s->spawns[i].bytes,0x90u) ||
            !observe_row(b,s,&s->spawns[i],&p,&present,&phase,FALSE) ||
            memcmp(&p,&s->result.placements[i],sizeof(p)) || present!=s->result.present[i] ||
            phase!=s->result.native_phase[i]) goto done;
    }
    if(!registry_still(b,s) || !SudekiMpLanStoryObserverRosterStillExact(w,r)) goto done;
    s->result.count=s->objects; *out=s->result; ok=TRUE;
done:
    free(s); SetLastError(ok?ERROR_SUCCESS:ERROR_INVALID_DATA); return ok;
}
