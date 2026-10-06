#include "hooks/lan_story_collision_owner.h"
#include "engine/build_identity.h"
#include <string.h>

enum { MAX_SOURCES=8192,MAX_REGIONS=206,MAX_DESCRIPTORS=4096,MAX_PARENTS=16 };
typedef struct ListBorrow {
    void **items;
    uint32_t count,index;
} ListBorrow;
typedef struct PositionBorrow {
    uint8_t *position,*entity,*parent,*source;
} PositionBorrow;
typedef struct OwnerBorrow {
    uint8_t *system,*grid,*world,*table,*current,*descriptor,*data,*registry;
    uint8_t *auxiliary,*position_reference;
    uint32_t descriptors,grid_capacity,mask,query_mask,state;
    ListBorrow lists[2],grid_list,entities;
    PositionBorrow positions[MAX_PARENTS];
    SudekiMpLanStoryCollisionOwner value;
} OwnerBorrow;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL image_exact(uint8_t *b) {
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    const IMAGE_DOS_HEADER *dos=(const void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable((HMODULE)b)) return FALSE;
    static const struct {unsigned rva,size;uint8_t bytes[24];} signatures[]={
        /* Entity->CPosition->collision source reciprocal creation. */
        {0x13de33,3,{0x8b,0x71,0x44}},
        {0x13de4e,18,{0x8a,0x8e,0,1,0,0,0x89,0x86,0x8c,0,0,0,0x88,0x88,0xf4,0,0,0}},
        /* Registration1 uses system+7C vector, registration2 system+6C. */
        {0x318eb,3,{0x8d,0x43,0x7c}}, {0x318f3,7,{0xc6,0x86,0xf2,0,0,0,1}},
        {0x31903,3,{0x8d,0x43,0x6c}}, {0x3190b,7,{0xc6,0x86,0xf2,0,0,0,2}},
        /* Grid enrollment backreference and bounded table append. */
        {0x3107f,19,{0x8b,0xb1,0xa0,0x15,0,0,0x89,0x04,0x96,
            0xff,0x81,0xa4,0x15,0,0,0x80,0x78,0x1c,0}},
        {0x31092,3,{0x89,0x48,8}},
        /* Native position parent TPtr is +94, not the collision auxiliary+50. */
        {0x11230d,3,{0x39,0x71,0x10}}, {0x112321,6,{0x8b,0x89,0x94,0,0,0}},
        {0x32339,6,{0x8b,0x81,0,1,0,0}}, {0x32345,3,{0x8d,0x70,0xfc}}
    };
    for(unsigned i=0;i<sizeof(signatures)/sizeof(signatures[0]);++i)
        if(!readable(b+signatures[i].rva,signatures[i].size) ||
            memcmp(b+signatures[i].rva,signatures[i].bytes,signatures[i].size)) return FALSE;
    uint8_t constructor[]={0xc7,0x00,0,0,0,0,0xc7,0x40,4,0,0,0,0};
    uint32_t main=(uint32_t)(uintptr_t)(b+0x2cdefc),sub=(uint32_t)(uintptr_t)(b+0x2cdf3c);
    memcpy(constructor+2,&main,4); memcpy(constructor+9,&sub,4);
    return readable(b+0x110628,sizeof(constructor)) &&
        !memcmp(b+0x110628,constructor,sizeof(constructor)) &&
        readable(b+0x2cdefc,4) && *(void **)(b+0x2cdefc)==b+0x110880;
}
static BOOL list_capture(const uint8_t *owner,unsigned count_offset,unsigned items_offset,
    const void *source,ListBorrow *out,unsigned *matches) {
    out->count=*(const uint32_t *)(owner+count_offset);
    out->items=*(void ***)(owner+items_offset); out->index=UINT32_MAX;
    if(out->count>MAX_SOURCES || (out->count && !readable(out->items,out->count*sizeof(void *)))) return FALSE;
    for(unsigned i=0;i<out->count;++i) if(out->items[i]==source) {
        out->index=i; ++*matches;
    }
    return TRUE;
}
static BOOL descriptor_exact(uint8_t *b,const OwnerBorrow *s,const uint8_t *d) {
    uintptr_t start=(uintptr_t)s->table,p=(uintptr_t)d;
    return p>=start && (p-start)%0x54u==0 && (p-start)/0x54u<s->descriptors &&
        readable(d,0x54u) && *(void *const *)d==b+0x2c82a4;
}
static BOOL capture(uint8_t *b,const SudekiMpLanStoryNativeRoster *r,const void *address,OwnerBorrow *s) {
    memset(s,0,sizeof(*s));
    if(!readable(b+0x408dd4,4) || !readable(b+0x408d10,4) ||
        *(void **)(b+0x408d10)!=r->world) return FALSE;
    s->system=*(uint8_t **)(b+0x408dd4); s->world=r->world;
    if(!readable(s->system,0x8c) || !readable(s->world,0x390) ||
        *(void **)s->world!=b+0x2c4c3c) return FALSE;
    s->grid=*(uint8_t **)(s->system+0x30);
    if(!readable(s->grid,0x15ac)) return FALSE;
    unsigned matches=0;
    if(!list_capture(s->system,0x70,0x78,address,&s->lists[0],&matches) ||
        !list_capture(s->system,0x80,0x88,address,&s->lists[1],&matches) || matches!=1) return FALSE;
    matches=0;
    if(!list_capture(s->grid,0x15a4,0x15a0,address,&s->grid_list,&matches) || matches!=1) return FALSE;
    s->grid_capacity=*(uint32_t *)(s->grid+0x15a8);
    if(s->grid_capacity>MAX_SOURCES || s->grid_list.count>s->grid_capacity) return FALSE;
    const uint8_t *source=address;
    if(!readable(source,0x110) || *(void *const *)(source+8)!=s->grid ||
        source[0xf4]>1 || source[0xf2]!=(s->lists[0].index!=UINT32_MAX?2:1)) return FALSE;
    s->mask=*(const uint32_t *)(source+0x44); s->query_mask=*(const uint32_t *)(source+0x48);
    s->auxiliary=*(uint8_t *const *)(source+0x50);
    s->position_reference=*(uint8_t *const *)(source+0x100);
    s->table=*(uint8_t **)(s->world+0x50); s->descriptors=*(uint32_t *)(s->world+0x54);
    s->current=*(uint8_t **)(s->world+0xc);
    if(!s->descriptors || s->descriptors>MAX_DESCRIPTORS ||
        !readable(s->table,s->descriptors*0x54u) || s->current!=r->descriptor ||
        !descriptor_exact(b,s,s->current)) return FALSE;
    SudekiMpLanStoryCollisionOwner *v=&s->value;
    v->source=(void *)source; v->dispatch_serial=r->dispatch_serial;
    v->epoch=r->epoch; v->revision=r->revision; v->region=UINT32_MAX;
    v->registration=source[0xf2]; v->enabled=source[0xf4];
    /* +50 is polymorphic native auxiliary data (e.g. a CTrigger pointer).
     * Only a reciprocal Zone data link classifies a source as terrain. */
    if(descriptor_exact(b,s,s->auxiliary)) {
        uint8_t *d=s->auxiliary; unsigned region=*(uint32_t *)(d+0x20);
        s->state=*(uint32_t *)(d+0x34); s->data=*(uint8_t **)(d+0x14);
        if(region<MAX_REGIONS && *(void **)(s->world+0x58+4*region)==d &&
            s->state>=2 && s->state<=4 && readable(s->data,0x30) &&
            *(void **)s->data==b+0x2cdcdc && *(uint32_t *)(s->data+0x24)==region &&
            *(void **)(s->data+0x2c)==source) {
            s->descriptor=d; v->kind=SUDEKIMP_STORY_COLLISION_TERRAIN;
            v->owner=d; v->region=region; return TRUE;
        }
        /* A descriptor-shaped auxiliary without reciprocal terrain ownership
         * is unknown, not a reason to reinterpret it as a dynamic entity. */
        return FALSE;
    }
    if(!readable(b+0x409d8c,4)) return FALSE;
    s->registry=*(uint8_t **)(b+0x409d8c);
    if(!readable(s->registry,0x40)) return FALSE;
    uintptr_t reference=(uintptr_t)s->position_reference;
    for(unsigned depth=0;depth<MAX_PARENTS;++depth) {
        if(reference<=4) return FALSE;
        PositionBorrow *p=&s->positions[depth]; p->position=(uint8_t *)(reference-4);
        for(unsigned j=0;j<depth;++j) if(s->positions[j].position==p->position) return FALSE;
        if(!readable(p->position,0x98) || *(void **)p->position!=b+0x2cdefc ||
            *(void **)(p->position+4)!=b+0x2cdf3c) return FALSE;
        p->entity=*(uint8_t **)(p->position+0x10); p->parent=*(uint8_t **)(p->position+0x94);
        p->source=*(uint8_t **)(p->position+0x8c);
        if(!depth && p->source!=source) return FALSE;
        if(p->entity) {
            matches=0;
            if(!list_capture(s->registry,0x34,0x3c,p->entity,&s->entities,&matches) || matches!=1 ||
                !readable(p->entity,0x48) || *(void **)(p->entity+0x44)!=p->position) return FALSE;
            v->kind=SUDEKIMP_STORY_COLLISION_ENTITY; v->owner=p->entity;
            v->resource_identifier=*(uint32_t *)(p->entity+0x34); v->parent_depth=(uint8_t)depth;
            return TRUE;
        }
        reference=(uintptr_t)p->parent;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryCollisionOwnerResolve(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const void *source,SudekiMpLanStoryCollisionOwner *out) {
    uint8_t *b=(uint8_t *)image;
    if(!out || !source || !w || !r || !w->service_post_original_exact || !w->dispatch_serial ||
        r->dispatch_serial!=w->dispatch_serial || !r->epoch || !r->revision ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) || !image_exact(b)) return FALSE;
    OwnerBorrow first,second;
    if(!capture(b,r,source,&first) || !SudekiMpLanStoryObserverRosterStillExact(w,r) ||
        !capture(b,r,source,&second) || memcmp(&first,&second,sizeof(first)) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    *out=first.value; return TRUE;
}

enum { MAX_SPAWNS=4096,MAX_TOTAL_SPAWNS=16384 };
typedef struct OriginZoneBorrow {
    uint8_t *descriptor,*data,*catalog,*spawns;
    const char *name;
    uint32_t state,count;
    uint8_t setup,flags;
} OriginZoneBorrow;
typedef struct OriginBorrow {
    OwnerBorrow owner;
    OriginZoneBorrow zones[MAX_REGIONS];
    uint8_t entity[0x48],spawn[0x90];
    SudekiMpLanStoryCollisionAuthoredOrigin value;
} OriginBorrow;
static BOOL origin_image_exact(uint8_t *b) {
    static const struct {unsigned slot,target;} slots[]={
        {0x2c8ad0,0x3a490},{0x2d5b44,0x3a490},{0x2d65e4,0x3a490},
        {0x2c8adc,0x3ae30},{0x2d5b50,0x3ae30},{0x2d65f0,0x3ae30},
        {0x2cddf4,0x13cfc0},{0x2cddf8,0x13d620},{0x2cddfc,0x13d630}
    };
    static const uint8_t publication[]={0x8b,0x4e,0x10,0x89,0x4f,0x78};
    static const uint8_t existing[]={0x8d,0x78,0x2c};
    static const uint8_t projection[]={0x8d,0x41,0xd4,0xc3};
    if(!readable(b+0x13cf9f,sizeof(publication)) ||
        memcmp(b+0x13cf9f,publication,sizeof(publication)) ||
        !readable(b+0x13cf16,sizeof(existing)) ||
        memcmp(b+0x13cf16,existing,sizeof(existing)) ||
        !readable(b+0x3a490,sizeof(projection)) ||
        memcmp(b+0x3a490,projection,sizeof(projection))) return FALSE;
    for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);++i)
        if(!readable(b+slots[i].slot,4) || *(void **)(b+slots[i].slot)!=b+slots[i].target) return FALSE;
    return TRUE;
}
static BOOL origin_text(char *out,size_t size,const char *text,BOOL zone) {
    for(size_t i=0;i<size;++i) {
        if(!readable(text,1)) return FALSE;
        unsigned char c=(unsigned char)*text++;
        if(!c) return i!=0;
        if(zone) {
            if(c>='A' && c<='Z') c=(unsigned char)(c-'A'+'a');
            if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-')) return FALSE;
        }
        out[i]=(char)c;
    }
    return FALSE;
}
static BOOL origin_name(char out[SUDEKIMP_STORY_ORIGIN_TEXT],const uint32_t *reference) {
    return readable(reference,8) && reference[0] && reference[0]!=UINT32_MAX &&
        origin_text(out,SUDEKIMP_STORY_ORIGIN_TEXT,(const char *)(uintptr_t)reference[1],FALSE);
}
static BOOL origin_entity(uint8_t *b,const SudekiMpLanStoryNativeRoster *r,OriginBorrow *s) {
    if(s->owner.value.kind!=SUDEKIMP_STORY_COLLISION_ENTITY) return FALSE;
    uint8_t *e=s->owner.value.owner;
    for(unsigned i=0;i<4;++i) if(r->actors[i]==e) return FALSE;
    static const struct {unsigned main,update,resource,position,kind;} classes[]={
        {0x2c8a8c,0x2c8ab0,0x2c8ad0,0x150,0xf84},
        {0x2d5b00,0x2d5b24,0x2d5b44,0x210,0xf8f},
        {0x2d65a0,0x2d65c4,0x2d65e4,0x150,0xf9b}
    };
    for(unsigned i=0;i<sizeof(classes)/sizeof(classes[0]);++i) {
        if(*(void **)e!=b+classes[i].main) continue;
        uint8_t *p=*(uint8_t **)(e+0x44);
        if(*(void **)(e+8)!=b+classes[i].update || *(void **)(e+0x2c)!=b+classes[i].resource ||
            (*(uint32_t *)(e+0x30)&0x1fff)!=classes[i].kind || p!=e+classes[i].position ||
            !readable(p,0x98) || (*(uintptr_t *)(p+0x94) && *(uintptr_t *)(p+0x94)!=4)) return FALSE;
        memcpy(s->entity,e,sizeof(s->entity)); s->value.resource_kind=classes[i].kind;
        return origin_name(s->value.resource,*(uint32_t **)(e+0x38));
    }
    return FALSE;
}
static BOOL origin_capture(uint8_t *b,const SudekiMpLanStoryNativeRoster *r,
    const void *source,OriginBorrow *s) {
    memset(s,0,sizeof(*s));
    if(!capture(b,r,source,&s->owner) || !origin_entity(b,r,s)) return FALSE;
    uint8_t *e=s->owner.value.owner; unsigned matches=0,total=0;
    for(unsigned region=0;region<MAX_REGIONS;++region) {
        OriginZoneBorrow *z=&s->zones[region];
        z->descriptor=*(uint8_t **)(s->owner.world+0x58+4*region);
        if(!z->descriptor) continue;
        uint8_t *d=z->descriptor;
        if(!descriptor_exact(b,&s->owner,d) || *(uint32_t *)(d+0x20)!=region) return FALSE;
        z->state=*(uint32_t *)(d+0x34); z->data=*(uint8_t **)(d+0x14);
        if(z->state==0 && !z->data && !*(void **)(d+0x18)) continue;
        if(z->state<2 || z->state>4 || !readable(z->data,0x12a) ||
            *(void **)z->data!=b+0x2cdcdc || *(uint32_t *)(z->data+0x24)!=region) return FALSE;
        z->setup=z->data[0x128]; z->flags=z->data[0x129];
        if(z->setup!=5) return FALSE;
        z->name=*(const char **)(d+0x24); z->catalog=*(uint8_t **)(z->data+0x118);
        if(!readable(z->catalog,0x1c)) return FALSE;
        z->count=*(uint32_t *)(z->catalog+0x10); z->spawns=*(uint8_t **)(z->catalog+0x18);
        if(z->count>MAX_SPAWNS || z->count>MAX_TOTAL_SPAWNS-total ||
            (z->count && !readable(z->spawns,z->count*0x90u))) return FALSE;
        total+=z->count;
        for(unsigned j=0;j<z->count;++j) {
            uint8_t *sp=z->spawns+j*0x90u;
            if(*(void **)sp!=b+0x2cddf4 || *(void **)(sp+4)!=b+0x2cde04) return FALSE;
            if(*(void **)(sp+0x78)!=e+0x2c) continue;
            if(++matches!=1 || (*(uint32_t *)(sp+0x6c)&0x1fff)!=s->value.resource_kind ||
                *(uint32_t *)(sp+0x70)!=s->owner.value.resource_identifier) return FALSE;
            char resource[SUDEKIMP_STORY_ORIGIN_TEXT]={0};
            if(!origin_name(resource,*(uint32_t **)(sp+0x74)) ||
                memcmp(resource,s->value.resource,sizeof(resource)) ||
                !origin_name(s->value.anchor,*(uint32_t **)(sp+0x58)) ||
                !origin_text(s->value.zone,sizeof(s->value.zone),z->name,TRUE)) return FALSE;
            memcpy(s->spawn,sp,sizeof(s->spawn));
            s->value.descriptor=d; s->value.data=z->data; s->value.spawn=sp;
            s->value.region=region; s->value.spawn_index=j;
            s->value.anchor_identifier=*(uint32_t *)(sp+0x54);
        }
    }
    if(matches!=1) return FALSE;
    s->value.collision=s->owner.value; return TRUE;
}
BOOL SudekiMpLanStoryCollisionAuthoredOriginResolve(HMODULE image,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,
    const void *source,SudekiMpLanStoryCollisionAuthoredOrigin *out) {
    uint8_t *b=(uint8_t *)image;
    if(!out || !source || !w || !r || !w->service_post_original_exact || !w->dispatch_serial ||
        r->dispatch_serial!=w->dispatch_serial || !r->epoch || !r->revision ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) || !image_exact(b) || !origin_image_exact(b)) return FALSE;
    OriginBorrow first,second;
    if(!origin_capture(b,r,source,&first) || !SudekiMpLanStoryObserverRosterStillExact(w,r) ||
        !origin_capture(b,r,source,&second) || memcmp(&first,&second,sizeof(first)) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    *out=first.value; return TRUE;
}
