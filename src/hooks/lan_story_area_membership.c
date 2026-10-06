#include "hooks/lan_story_area_membership.h"
#include "engine/build_identity.h"
#include <math.h>
#include <string.h>

enum { MAX_REGIONS=206, MAX_DESCRIPTORS=4096 };
typedef struct MemberBorrow {
    uint8_t *actor,*membership,*movement,*descriptor,*data,*navigation,*collision;
    const char *name;
    SudekiMpLanStoryAreaMember value;
} MemberBorrow;
typedef struct AreaBorrow {
    uint8_t *world,*table,*current;
    uint32_t count;
    MemberBorrow members[4];
} AreaBorrow;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL image_exact(uint8_t *b) {
    /* Loader supplies the SHA-checked image. Fence PE header reads before the
     * common helper, then check the native region lookup and collision fields.
     * Neither of these functions is hooked or executed by this observer. */
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    const IMAGE_DOS_HEADER *dos=(const void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable((HMODULE)b)) return FALSE;
    uint8_t lookup[]={0x8b,0x15,0,0,0,0,0x33,0xc0,0x85,0xd2,0x74,0x2f,
        0x8b,0x44,0x24,0x04,0xc1,0xe8,0x08,0x0f,0xb6,0xc8,0x33,0xc0,
        0x81,0xf9,0xce,0,0,0,0x73,0x1b,0x8b,0x4c,0x8a,0x58,
        0x85,0xc9,0x74,0x13,0x8b,0x49,0x14,0x85,0xc9,0x74,0x08,
        0x8b,0x49,0x28,0x8b,0xc1,0xc2,0x04,0,0x33,0xc9,0x8b,0xc1,0xc2,0x04,0};
    uint32_t global=(uint32_t)(uintptr_t)(b+0x408d10u);
    memcpy(lookup+2,&global,4);
    static const uint8_t collision[]={0x8b,0x47,0x14,0x8b,0x48,0x2c,
        0xc6,0x81,0xf4,0,0,0,0x01,0x8b,0x57,0x14,0x8b,0x42,0x2c,
        0xc7,0x40,0x44,0,0,0x02,0};
    static const struct {unsigned slot,target;} slots[]={
        {0x2c4c48u,0x5620u},{0x2c82a4u,0x3a330u},
        {0x2cdcdcu,0x147430u},{0x2d47d8u,0x185490u},{0x2d47dcu,0x1854c0u},
        {0x2c8644u,0xc3de0u},{0x2c8648u,0xc3070u},{0x2c8680u,0xc45c0u},
        {0x2c8684u,0xc4450u},{0x2c8688u,0xc4580u}
    };
    /* Collision pre-pass resets +BC; native floor-hit resolution publishes
     * {ZoneInfo region, hit sector}. Stock instantaneous placement marks +BE.
     * Observe only: neither movement method nor callback is executed here. */
    static const uint8_t movement_clear[]={0x66,0x89,0x8f,0xbc,0,0,0};
    static const uint8_t movement_publish[]={0x66,0x89,0x8b,0xbc,0,0,0};
    static const uint8_t movement_mark[]={0x80,0x88,0xbe,0,0,0,0x04};
    if(!readable(b+0xc44deu,sizeof(movement_clear)) ||
        memcmp(b+0xc44deu,movement_clear,sizeof(movement_clear)) ||
        !readable(b+0xc4698u,sizeof(movement_publish)) ||
        memcmp(b+0xc4698u,movement_publish,sizeof(movement_publish)) ||
        !readable(b+0xf5111u,sizeof(movement_mark)) ||
        memcmp(b+0xf5111u,movement_mark,sizeof(movement_mark))) return FALSE;
    if(!readable(b+0xef640u,sizeof(lookup)) || memcmp(b+0xef640u,lookup,sizeof(lookup)) ||
        !readable(b+0x10b4d9u,sizeof(collision)) || memcmp(b+0x10b4d9u,collision,sizeof(collision))) return FALSE;
    for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);++i)
        if(!readable(b+slots[i].slot,4) || *(void **)(b+slots[i].slot)!=b+slots[i].target) return FALSE;
    return TRUE;
}
static BOOL name_copy(char out[SUDEKIMP_LAN_STORY_NAME_SIZE],const char *p) {
    for(unsigned i=0;i<SUDEKIMP_LAN_STORY_NAME_SIZE;++i) {
        if(!p || !readable(p+i,1)) return FALSE;
        unsigned char c=(unsigned char)p[i];
        if(!c) return i!=0;
        if(c>='A' && c<='Z') c=(unsigned char)(c-'A'+'a');
        if(!((c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_' || c=='-')) return FALSE;
        out[i]=(char)c;
    }
    return FALSE;
}
static BOOL descriptor_exact(uint8_t *b,const AreaBorrow *a,const uint8_t *d) {
    uintptr_t start=(uintptr_t)a->table,p=(uintptr_t)d;
    return p>=start && (p-start)%0x54u==0 && (p-start)/0x54u<a->count &&
        readable(d,0x54u) && *(void *const *)d==b+0x2c82a4u;
}
static BOOL member_copy(uint8_t *b,const AreaBorrow *a,uint8_t *actor,MemberBorrow *m) {
    if(!readable(actor,0x84u)) return FALSE;
    m->actor=actor; m->membership=*(uint8_t **)(actor+0x74u);
    uint8_t *c=m->membership;
    if(!readable(c,0x42u) || *(void **)c!=b+0x2d47bcu || *(void **)(c+0x10u)!=actor) return FALSE;
    SudekiMpLanStoryAreaMember *v=&m->value;
    v->sector=*(uint16_t *)(c+0x40u); v->pause_refs=actor[0x2bu];
    unsigned region=v->sector>>8;
    if((v->sector&0xffu)==0xffu || region>=MAX_REGIONS) return FALSE;
    m->movement=*(uint8_t **)(actor+0x80u);
    if(m->movement) {
        uint8_t *motion=m->movement;
        if(!readable(motion,0xc0u) || *(void **)motion!=b+0x2c8644u ||
            *(void **)(motion+0x10u)!=actor) return FALSE;
        v->movement_present=1; v->movement_sector=*(uint16_t *)(motion+0xbcu);
        v->movement_flags[0]=motion[0xbeu]; v->movement_flags[1]=motion[0xbfu];
        v->movement_sector_valid=(v->movement_sector>>8)<MAX_REGIONS &&
            (v->movement_sector&0xffu)!=0xffu;
        v->movement_region_matches=v->movement_sector_valid &&
            (v->movement_sector>>8)==region;
    }
    m->descriptor=*(uint8_t **)(a->world+0x58u+4u*region);
    uint8_t *d=m->descriptor;
    if(!descriptor_exact(b,a,d) || *(uint32_t *)(d+0x20u)!=region) return FALSE;
    m->name=*(const char **)(d+0x24u);
    if(!name_copy(v->name,m->name)) return FALSE;
    v->native_state=*(uint32_t *)(d+0x34u);
    if(v->native_state>4u) return FALSE;
    v->zone_flags=d[0x50u]; v->is_current=d==a->current;
    v->data_pending=v->native_state==1u;
    m->data=*(uint8_t **)(d+(v->data_pending?0x18u:0x14u));
    if(!m->data) return TRUE; /* Known absent data, not a playable-area verdict. */
    uint8_t *data=m->data;
    if(!readable(data,0x30u) || *(void **)data!=b+0x2cdcdcu ||
        *(uint32_t *)(data+0x24u)!=region) return FALSE;
    v->data_present=1;
    m->navigation=*(uint8_t **)(data+0x28u); v->navigation_present=m->navigation!=NULL;
    m->collision=*(uint8_t **)(data+0x2cu);
    if(!m->collision) return TRUE;
    /* This native collision record has no vtable. Identity comes from its
     * exact Zone owner and reciprocal descriptor link, not readability alone.
     * Registration byte is a report; list enrollment/query behavior still
     * needs independent proof before any activation adapter may use it. */
    uint8_t *scene=m->collision;
    if(!readable(scene,0x110u) || *(void **)(scene+0x50u)!=d ||
        scene[0xf2u]>2u || scene[0xf4u]>1u) return FALSE;
    v->collision_present=1; v->collision_mask=*(uint32_t *)(scene+0x44u);
    v->collision_registration=scene[0xf2u]; v->collision_enabled=scene[0xf4u];
    return TRUE;
}
static BOOL capture(uint8_t *b,const SudekiMpLanStoryNativeRoster *r,AreaBorrow *a) {
    memset(a,0,sizeof(*a));
    if(!readable(b+0x408d10u,4) || *(void **)(b+0x408d10u)!=r->world ||
        !readable(r->world,0x390u) || *(void **)r->world!=b+0x2c4c3cu) return FALSE;
    a->world=r->world; a->table=*(uint8_t **)(a->world+0x50u);
    a->count=*(uint32_t *)(a->world+0x54u); a->current=*(uint8_t **)(a->world+0xcu);
    if(!a->count || a->count>MAX_DESCRIPTORS ||
        !readable(a->table,(size_t)a->count*0x54u) || a->current!=r->descriptor ||
        !descriptor_exact(b,a,a->current)) return FALSE;
    for(unsigned i=0;i<4u;++i) {
        if(!(r->available_mask&(1u<<i))) {if(r->actors[i]) return FALSE; continue;}
        for(unsigned j=0;j<i;++j) if(r->actors[j]==r->actors[i]) return FALSE;
        if(!member_copy(b,a,r->actors[i],&a->members[i])) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryAreaMembershipObserve(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,SudekiMpLanStoryAreaMembership *out) {
    uint8_t *b=(uint8_t *)image;
    if(!out || !w || !r || !w->service_post_original_exact || !w->dispatch_serial ||
        r->dispatch_serial!=w->dispatch_serial || !r->epoch || !r->revision ||
        !r->available_mask || (r->available_mask&~15u) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) || !image_exact(b)) return FALSE;
    AreaBorrow first,second;
    if(!capture(b,r,&first) || !SudekiMpLanStoryObserverRosterStillExact(w,r) ||
        !capture(b,r,&second) || memcmp(&first,&second,sizeof(first)) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    SudekiMpLanStoryAreaMembership result={0};
    result.epoch=r->epoch; result.revision=r->revision; result.available_mask=r->available_mask;
    for(unsigned i=0;i<4u;++i) result.members[i]=first.members[i].value;
    *out=result; return TRUE;
}

/* Native5084A0 scans Zone+C0 count / C8 array, stride30h; 508750 type35
 * selects that catalog. Native4FBBD0 copies AiLocEditable+08/+14/+2C.
 * Reading the resident catalog avoids the generic resource lookup's potential
 * load/create behavior and its fallback to global world+C. */
enum { MAX_ARRIVALS=4096, MAX_NAV_SECTORS=4096 };
typedef struct DestinationBorrow {
    uint8_t *world,*table,*current,*descriptor,*data,*markers,*selected;
    uint8_t *navigation,*sectors,*selected_sector;
    const char *zone_name,*marker_name;
    uint32_t count,marker_count,marker_capacity,*name_reference,name_refs,sector_count;
    uint8_t selected_bytes[0x30];
    uint8_t sector_bytes[0x40];
    SudekiMpLanStoryAreaDestination value;
} DestinationBorrow;
static BOOL arrival_image_exact(uint8_t *b) {
    static const uint8_t get_name[]={0x8b,0x44,0x24,0x04,0x8b,0x51,0x20,0x56,
        0x8b,0x30,0x33,0x71,0x1c,0x8b,0x49,0x24};
    static const uint8_t get_type[]={0xb8,0x35,0,0,0,0xc3};
    static const uint8_t catalog[]={0x83,0xec,0x20,0x53,0x55,0x56,0x57,
        0x8b,0x7c,0x24,0x34,0x8b,0xaf,0xc0,0,0,0};
    /* 4F2270 resolves the packed region through world+58, Zone+28; this
     * unhooked scan proves {count, rows}, stride40h and packed ID at row+0A.
     * Neither this function nor the native navigation callbacks are called. */
    static const uint8_t navigation_scan[]={0x8b,0x11,0x33,0xc0,0x56,0x85,0xd2,0x74,0x13,
        0x8b,0x71,0x04,0x8d,0x4e,0x0a,0x66,0x39,0x39,0x74,0x0c,0x40,
        0x83,0xc1,0x40,0x3b,0xc2,0x72,0xf3,0x33,0xc0,0x5e,0xc3,
        0xc1,0xe0,0x06,0x03,0xc6,0x5e,0xc3};
    return image_exact(b) && readable(b+0x106c90u,sizeof(get_name)) &&
        !memcmp(b+0x106c90u,get_name,sizeof(get_name)) &&
        readable(b+0x106d70u,sizeof(get_type)) && !memcmp(b+0x106d70u,get_type,sizeof(get_type)) &&
        readable(b+0x1084a0u,sizeof(catalog)) && !memcmp(b+0x1084a0u,catalog,sizeof(catalog)) &&
        readable(b+0xf22a7u,sizeof(navigation_scan)) &&
        !memcmp(b+0xf22a7u,navigation_scan,sizeof(navigation_scan)) &&
        readable(b+0x2cddacu,0x1cu) && *(void **)(b+0x2cddacu)==b+0x106e40u &&
        *(void **)(b+0x2cddc0u)==b+0x106c90u && *(void **)(b+0x2cddc4u)==b+0x106d70u;
}
static BOOL marker_text(char out[SUDEKIMP_STORY_ARRIVAL_NAME],const char *p) {
    for(unsigned i=0;i<SUDEKIMP_STORY_ARRIVAL_NAME;++i) {
        if(!p || !readable(p+i,1)) return FALSE;
        unsigned char c=(unsigned char)p[i];
        if(!c) return i!=0;
        /* Preserve authored spelling, including qualified resource paths.
         * No hashing, prefix stripping or nearest-marker fallback. */
        if(!((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') ||
            c=='_' || c=='-' || c=='.' || c==':' || c=='/' || c=='\\')) return FALSE;
        out[i]=(char)c;
    }
    return FALSE;
}
static BOOL destination_copy(uint8_t *b,const SudekiMpLanStoryNativeRoster *r,
    const char *zone,uint32_t identifier,const char *marker,DestinationBorrow *s) {
    memset(s,0,sizeof(*s));
    if(!readable(b+0x408d10u,4) || *(void **)(b+0x408d10u)!=r->world ||
        !readable(r->world,0x390u) || *(void **)r->world!=b+0x2c4c3cu) return FALSE;
    s->world=r->world; s->table=*(uint8_t **)(s->world+0x50u);
    s->count=*(uint32_t *)(s->world+0x54u); s->current=*(uint8_t **)(s->world+0xcu);
    if(!s->count || s->count>MAX_DESCRIPTORS || s->current!=r->descriptor ||
        !readable(s->table,(size_t)s->count*0x54u)) return FALSE;
    AreaBorrow table={0}; table.table=s->table; table.count=s->count;
    if(!descriptor_exact(b,&table,s->current)) return FALSE;
    /* Name must uniquely select an authored descriptor, including when the
     * destination is not the host's current descriptor. */
    for(unsigned i=0;i<s->count;++i) {
        uint8_t *d=s->table+i*0x54u; char name[SUDEKIMP_LAN_STORY_NAME_SIZE]={0};
        if(!descriptor_exact(b,&table,d) || !name_copy(name,*(const char **)(d+0x24u))) return FALSE;
        if(strcmp(name,zone)) continue;
        if(s->descriptor) return FALSE;
        s->descriptor=d; s->zone_name=*(const char **)(d+0x24u);
    }
    uint8_t *d=s->descriptor;
    if(!d) return FALSE;
    unsigned region=*(uint32_t *)(d+0x20u);
    if(region>=MAX_REGIONS || *(void **)(s->world+0x58u+4u*region)!=d) return FALSE;
    s->value.native_state=*(uint32_t *)(d+0x34u); s->value.zone_flags=d[0x50u];
    if(s->value.native_state<2u || s->value.native_state>4u) return FALSE;
    s->data=*(uint8_t **)(d+0x14u);
    if(!readable(s->data,0x12au) || *(void **)s->data!=b+0x2cdcdcu ||
        *(uint32_t *)(s->data+0x24u)!=region || s->data[0x128u]!=5u) return FALSE;
    s->marker_count=*(uint32_t *)(s->data+0xc0u);
    s->marker_capacity=*(uint32_t *)(s->data+0xc4u);
    s->markers=*(uint8_t **)(s->data+0xc8u);
    if(!s->marker_count || s->marker_count>s->marker_capacity || s->marker_capacity>MAX_ARRIVALS ||
        !readable(s->markers,(size_t)s->marker_count*0x30u)) return FALSE;
    for(unsigned i=0;i<s->marker_count;++i) {
        uint8_t *m=s->markers+i*0x30u;
        if(*(void **)m!=b+0x2cddacu || *(void **)(m+4u)!=b+0x2cddb4u) return FALSE;
        if(*(uint32_t *)(m+0x24u)!=identifier) continue;
        /* The stock scan selects first matching native ID. Refuse ambiguity
         * rather than depend on catalog ordering or a hash collision. */
        if(s->selected) return FALSE;
        s->selected=m;
    }
    if(!s->selected) return FALSE;
    uint8_t *m=s->selected;
    memcpy(s->selected_bytes,m,sizeof(s->selected_bytes));
    s->name_reference=*(uint32_t **)(m+0x28u);
    if(!readable(s->name_reference,8u)) return FALSE;
    s->name_refs=s->name_reference[0];
    s->marker_name=(const char *)(uintptr_t)s->name_reference[1];
    if(!s->name_refs || s->name_refs==UINT32_MAX ||
        !marker_text(s->value.marker,s->marker_name) || strcmp(s->value.marker,marker)) return FALSE;
    s->value.sector=*(uint16_t *)(m+0x2cu);
    if((s->value.sector&0xffu)==0xffu || (s->value.sector>>8)!=region) return FALSE;
    /* An authored marker with a plausible region byte may still refer to a
     * missing/stale navigation sector. Require its exact unique native row.
     * This proves catalog membership only, NOT collision query correctness,
     * an active scene, traversable terrain or a retained navigation lifetime. */
    s->navigation=*(uint8_t **)(s->data+0x28u);
    if(!readable(s->navigation,8u)) return FALSE;
    s->sector_count=*(uint32_t *)s->navigation;
    s->sectors=*(uint8_t **)(s->navigation+4u);
    if(!s->sector_count || s->sector_count>MAX_NAV_SECTORS ||
        !readable(s->sectors,(size_t)s->sector_count*0x40u)) return FALSE;
    for(unsigned i=0;i<s->sector_count;++i) {
        uint8_t *sector=s->sectors+i*0x40u;
        if(*(uint16_t *)(sector+0xau)!=s->value.sector) continue;
        if(s->selected_sector) return FALSE;
        s->selected_sector=sector;
    }
    if(!s->selected_sector) return FALSE;
    memcpy(s->sector_bytes,s->selected_sector,sizeof(s->sector_bytes));
    memcpy(s->value.position,m+8u,sizeof(s->value.position));
    memcpy(s->value.facing,m+0x14u,sizeof(s->value.facing));
    for(unsigned i=0;i<3u;++i)
        if(!isfinite(s->value.position[i]) || !isfinite(s->value.facing[i])) return FALSE;
    s->value.dispatch_serial=r->dispatch_serial; s->value.epoch=r->epoch; s->value.revision=r->revision;
    s->value.marker_identifier=identifier;
    memcpy(s->value.zone,zone,strlen(zone)+1u);
    return TRUE;
}
BOOL SudekiMpLanStoryAreaDestinationResolve(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const char *zone,uint32_t identifier,const char *marker,
    SudekiMpLanStoryAreaDestination *out) {
    uint8_t *b=(uint8_t *)image;
    if(!out || !w || !r || !w->service_post_original_exact || !w->dispatch_serial ||
        r->dispatch_serial!=w->dispatch_serial || !r->epoch || !r->revision ||
        identifier==0x7ffffu || identifier==UINT32_MAX ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) || !arrival_image_exact(b)) return FALSE;
    char zone_copy[SUDEKIMP_LAN_STORY_NAME_SIZE]={0},marker_copy[SUDEKIMP_STORY_ARRIVAL_NAME]={0};
    if(!name_copy(zone_copy,zone) || !marker_text(marker_copy,marker)) return FALSE;
    DestinationBorrow first,second;
    if(!destination_copy(b,r,zone_copy,identifier,marker_copy,&first) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) ||
        !destination_copy(b,r,zone_copy,identifier,marker_copy,&second) ||
        memcmp(&first,&second,sizeof(first)) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    *out=first.value; return TRUE;
}
