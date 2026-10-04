#include "hooks/lan_story_world.h"
#include "hooks/lan_story_curve.h"
#include "hooks/lan_story_material.h"
#include "hooks/lan_story_residency.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/ranged_world_pose.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_REGISTRY=8192, MAX_ANIMATIONS=4096, MAX_SUBMODELS=32,
    MAX_CHILDREN=256, MAX_CHILD_ROWS=4096 };
typedef void (__attribute__((fastcall)) *PositionSet)(void *,const float *);
typedef const float *(__attribute__((thiscall)) *PositionMatrix)(void *);
typedef void (__attribute__((thiscall)) *SelectorSet)(void *,int,unsigned,int);
typedef void (__attribute__((thiscall)) *ValueSet)(void *,int,unsigned,float);
typedef void (__attribute__((thiscall)) *TimeSet)(void *,int,unsigned,float,int);
typedef void (__attribute__((thiscall)) *StateSet)(void *,int,unsigned,int);
typedef void (__attribute__((thiscall)) *BlendSet)(void *,int,float);
typedef struct Target {
    uint8_t *entity,*position,*model,*wrapper,*object,*renderer;
    uint8_t *attached_wrapper;
    uint8_t *bank,*description,*entries,*channels,*blends;
    uint8_t *rows[5];
    uint32_t identifier,animations,submodels,channel_count;
    uint16_t kind;
    unsigned character;
    uint64_t fingerprint;
} Target;
typedef struct Registry {
    uint8_t *owner;
    void **entries;
    uint32_t count;
    void *entities[MAX_REGISTRY];
} Registry;
typedef struct Bound {
    Target target;
    SudekiMpLanStoryWorldActor previous;
    BOOL ranged_base;
    uint32_t tick;
} Bound;
typedef struct Prepared {
    BOOL valid;
    BOOL independent_view;
    Registry registry;
    Target targets[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
    unsigned count,selectors[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS][5];
    const SudekiMpLanStoryNativeRoster *roster_address;
    const SudekiMpLanStoryWorldFrame *frame_address;
    SudekiMpLanStoryNativeRoster roster;
    SudekiMpLanStoryWorldFrame frame;
    SudekiMpLanStoryReplicaExact exact;
    void *context;
} Prepared;
static uint8_t *base;
static DWORD native_thread;
static BOOL active,host_seen,client_seen,resource_fault;
static uint32_t next_generation,host_epoch,host_sequence,client_epoch;
static unsigned host_count,client_count;
static Bound host_bound[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
static Bound client_bound[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
static struct { BOOL retained; void *world,*descriptor,*removed_npc; uint32_t before_epoch; } recruitment;
static Prepared prepared;
static PositionSet set_position;
static PositionMatrix position_matrix;
static SelectorSet set_selector;
static ValueSet set_rate;
static TimeSet set_time;
static StateSet set_state;
static BlendSet set_blend;
static void *world_set_forward __attribute__((used));
static const char *operation="none",*stage="none",*last_stage;
static uint32_t stage_identifier,stage_detail,last_identifier,last_detail;
static DWORD last_error;
static unsigned diagnostic_count;
typedef struct CurveProof {
    const void *bank,*chunk,*curve,*header;
    SudekiMpLanStoryCurveKind kind;
    unsigned floats;
} CurveProof;
enum { MAX_CURVE_PROOFS=4096 };
static CurveProof curve_proofs[MAX_CURVE_PROOFS];
static unsigned curve_proof_count;
static void observe_stage(const char *value,uint32_t identifier,uint32_t detail) {
    stage=value; stage_identifier=identifier; stage_detail=detail;
}

static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a+n<a || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return !write || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL readable(const void *p,size_t n) { return memory(p,n,FALSE); }
static BOOL writable(const void *p,size_t n) { return memory(p,n,TRUE); }
static BOOL begin(void) {
    if(!base || active || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    native_thread=GetCurrentThreadId(); active=TRUE; curve_proof_count=0; return TRUE;
}
static BOOL finish(BOOL result,DWORD error) {
    if(!result && diagnostic_count<64u &&
        (stage!=last_stage || stage_identifier!=last_identifier ||
            stage_detail!=last_detail || error!=last_error)) {
        SudekiMpLogFormat("lan_story_world event=refused operation=%s stage=%s identifier=%08lx detail=%lu win32_error=%lu\r\n",
            operation,stage,(unsigned long)stage_identifier,(unsigned long)stage_detail,(unsigned long)error);
        ++diagnostic_count; last_stage=stage; last_identifier=stage_identifier;
        last_detail=stage_detail; last_error=error;
    }
    memset(&prepared,0,sizeof(prepared));
    memset(curve_proofs,0,curve_proof_count*sizeof(curve_proofs[0])); curve_proof_count=0;
    active=FALSE; SetLastError(result?ERROR_SUCCESS:error); return result;
}
static BOOL registry_still(const Registry *r) {
    return *(void **)(base+0x409d8cu)==r->owner && readable(r->owner,0x40u) &&
        *(uint32_t *)(r->owner+0x34u)==r->count &&
        *(void **)(r->owner+0x3cu)==r->entries &&
        readable(r->entries,r->count*sizeof(void *)) &&
        !memcmp(r->entries,r->entities,r->count*sizeof(void *));
}
static BOOL registry_capture(Registry *r) {
    memset(r,0,sizeof(*r)); r->owner=*(uint8_t **)(base+0x409d8cu);
    if(!readable(r->owner,0x40u)) return FALSE;
    r->count=*(uint32_t *)(r->owner+0x34u);
    r->entries=*(void ***)(r->owner+0x3cu);
    if(!r->count || r->count>MAX_REGISTRY || !readable(r->entries,r->count*sizeof(void *))) return FALSE;
    memcpy(r->entities,r->entries,r->count*sizeof(void *)); return registry_still(r);
}

/* Method slots and entry bytes are shared with the established player world
 * renderer adapter. No native getter is called while capturing host state. */
typedef struct MethodIdentity { unsigned slot,rva; uint8_t prefix[12]; } MethodIdentity;
static const MethodIdentity identities[]={
    {0x40,0x21bac0,{0x8b,0x51,0x08,0x8b,0x4a,0x1c,0x8b,0x09,0x56,0x33,0xc0,0x57}},
    {0xf8,0x21bb10,{0x8b,0x41,0x08,0x8b,0x48,0x1c,0x8b,0x41,0x0c,0xc3,0xcc,0xcc}},
    {0xfc,0x223000,{0x53,0x55,0x56,0x8b,0xf1,0x8b,0x4c,0x24,0x10,0x8b,0x86,0x98}},
    {0x100,0x2230b0,{0x8b,0x44,0x24,0x04,0x8b,0x89,0x98,0x00,0x00,0x00,0x8d,0x14}},
    {0x104,0x2230d0,{0x8b,0x44,0x24,0x04,0xd9,0x44,0x24,0x0c,0x53,0x56,0x57,0x8b}},
    {0x108,0x223160,{0x8b,0x44,0x24,0x04,0x8b,0x89,0x98,0x00,0x00,0x00,0x8d,0x14}},
    {0x10c,0x223180,{0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0x53,0x56,0x8b,0x75,0x08}},
    {0x110,0x223220,{0x8b,0x44,0x24,0x04,0x8b,0x89,0x98,0x00,0x00,0x00,0x8d,0x14}},
    {0x114,0x223240,{0x8b,0x44,0x24,0x04,0x8b,0x54,0x24,0x08,0x53,0x66,0x8b,0x5c}},
    {0x118,0x223290,{0x8b,0x44,0x24,0x04,0x8b,0x89,0x98,0x00,0x00,0x00,0x8d,0x14}},
    {0x144,0x2234c0,{0x8b,0x44,0x24,0x04,0xd9,0x44,0x24,0x08,0x8b,0x89,0x9c,0x00}},
    {0x148,0x2234e0,{0x8b,0x44,0x24,0x04,0x8b,0x89,0x9c,0x00,0x00,0x00,0x8d,0x04}},
    {0x178,0x21bf00,{0x8b,0x44,0x24,0x04,0x8b,0x51,0x08,0x56,0x8d,0x34,0xc5,0x00}},
    {0x18c,0x223b70,{0x53,0x8b,0x5c,0x24,0x08,0x56,0x57,0x8b,0xf9,0x8b,0x47,0x08}}
};
static BOOL methods_exact(void) {
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)
        if(*(void **)(base+0x2df8ecu+identities[i].slot)!=base+identities[i].rva) return FALSE;
    return *(void **)(base+0x2df8ecu+0x184u)==base+0x21bf50u &&
        *(void **)(base+0x2d65e4u+0xcu)==base+0x3ae30u;
}
static BOOL scenery(const Target *t) {
    return t->kind==SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND;
}
static unsigned pose_channels(const Target *t) { return t->channel_count; }
static unsigned pose_blends(const Target *t) { return t->channel_count-1u; }
static unsigned first_clip(const Target *t) { return scenery(t)?0u:1u; }
static BOOL blend_storage(const Target *t,BOOL write) {
    if(scenery(t)) return t->channel_count==1u && !t->blends;
    return memory(t->blends,(t->channel_count-1u)*20u,write) &&
        *(uint16_t *)t->blends==0u && *(uint16_t *)(t->blends+2u)==1u &&
        *(uint16_t *)(t->blends+20u)==2u && *(uint16_t *)(t->blends+22u)==3u &&
        *(uint16_t *)(t->blends+40u)==0x8000u && *(uint16_t *)(t->blends+42u)==0x8001u &&
        (t->channel_count==4u || (t->channel_count==5u &&
            *(uint16_t *)(t->blends+60u)==0x8002u && *(uint16_t *)(t->blends+62u)==4u));
}
static uint64_t hash_u32(uint64_t hash,uint32_t value) {
    for(unsigned b=0;b<4u;++b) { hash^=(value>>(b*8u))&255u; hash*=UINT64_C(1099511628211); }
    return hash;
}
typedef struct BankHeaderSpan { uintptr_t lower,upper; } BankHeaderSpan;
static BOOL bank_header_readable(const void *pointer,BankHeaderSpan *span) {
    uintptr_t p=(uintptr_t)pointer;
    if(!pointer || p>UINTPTR_MAX-0x20u) return FALSE;
    if(p>=span->lower && p+0x20u<=span->upper) return TRUE;
    MEMORY_BASIC_INFORMATION memory;
    if(VirtualQuery(pointer,&memory,sizeof(memory))!=sizeof(memory) ||
        memory.State!=MEM_COMMIT || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=memory.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    uintptr_t lower=(uintptr_t)memory.BaseAddress;
    if(memory.RegionSize>UINTPTR_MAX-lower || p<lower ||
        p+0x20u>lower+memory.RegionSize) return FALSE;
    span->lower=lower; span->upper=lower+memory.RegionSize; return TRUE;
}
static BOOL bank_fingerprint(const Target *t,uint64_t *out) {
    uint64_t hash=UINT64_C(14695981039346656037);
    /* This range proof lives only on this read-only traversal's stack. All
     * header contents are still read/validated/hashed in order. No native
     * setter, loader, callback or allocation runs in the loop; no range or
     * fingerprint authorization survives into another traversal or frame. */
    BankHeaderSpan headers={0};
    hash=hash_u32(hash,t->animations); hash=hash_u32(hash,t->submodels);
    hash=hash_u32(hash,t->kind); hash=hash_u32(hash,t->channel_count);
    /* Native lookup61BAC0 compares resource[0] in entries of stride28.
     * Legitimate distinct selectors can share a resource handle. The wire
     * includes its nonempty occurrence, and this fingerprint retains the
     * full bank ORDER rather than treating a handle as a unique asset key. */
    for(unsigned i=0;i<t->animations;++i) {
        observe_stage("bank_resource",t->identifier,i);
        uint8_t *resource=*(uint8_t **)(t->entries+i*28u);
        if(!bank_header_readable(resource,&headers)) return FALSE;
        uint32_t handle=*(uint32_t *)resource,length;
        float duration=*(float *)(resource+4u);
        if(!isfinite(duration) || duration<0.0f || duration>1000000.0f) return FALSE;
        /* Characters reserve selector zero for the empty clip. Generic
         * scenery starts with a real authored clip at selector zero. */
        if(i>=first_clip(t) && (!handle || handle==0x0007ffffu)) return FALSE;
        memcpy(&length,&duration,sizeof(length));
        hash=hash_u32(hash,handle); hash=hash_u32(hash,length);
        /* Constructor61CBE0 publishes an authored header separately from
         * runtime entry pointers. Its +C loop count, 61F410's +18 event count
         * and 61BF50/61F330's +1C dependency index are numeric, never pointers.
         * Include them so same-handle/duration aliases cannot erase them. */
        hash=hash_u32(hash,*(uint32_t *)(resource+0xcu));
        hash=hash_u32(hash,*(uint32_t *)(resource+0x18u));
        hash=hash_u32(hash,*(uint32_t *)(resource+0x1cu));
    }
    for(unsigned i=0;i<pose_blends(t);++i) {
        hash=hash_u32(hash,*(uint16_t *)(t->blends+i*20u));
        hash=hash_u32(hash,*(uint16_t *)(t->blends+i*20u+2u));
    }
    if(!hash) return FALSE;
    *out=hash; return TRUE;
}
/* Native character class and canonical ResourceName are independent of the
 * transport's PC category. Packed kind's low seven bits are lazy-resolution
 * state; exact roster, class, canonical name/hash and component ownership are
 * the native authority. No generic resource lookup is performed here. */
static BOOL entity_identity(void *entity,const SudekiMpLanStoryNativeRoster *roster,
    unsigned *character,uint16_t *kind,uint8_t **model) {
    static const uint32_t vtables[4][3]={{0x2d5a88u,0x2d5aacu,0x2d5accu},
        {0x2d66fcu,0x2d6720u,0x2d6740u},{0x2d5010u,0x2d5034u,0x2d5054u},
        {0x2d555cu,0x2d5580u,0x2d55a0u}};
    static const char *const names[4]={"PC_BUKI","PC_ELCO","PC_TAL","PC_AILISH"};
    uint8_t *e=entity;
    if(!roster || !readable(e,0x254u)) return FALSE;
    *model=NULL; *character=4u;
    if(*(void **)e==base+0x2d5b00u) {
        /* GenericEntity542E10: exact update/resource subobjects and embedded
         * CPosition. Excludes triggers, pickups and similarly named classes. */
        if(!readable(e,0x314u) || *(void **)(e+8u)!=base+0x2d5b24u ||
            *(void **)(e+0x2cu)!=base+0x2d5b44u ||
            *(void **)(base+0x2d5b44u+0xcu)!=base+0x3ae30u ||
            (*(uint32_t *)(e+0x30u)&0x1fffu)!=SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND ||
            *(void **)(e+0x44u)!=e+0x210u) return FALSE;
        if(*(uint32_t *)(e+0x34u)==0x36f00a6cu) {
            static const char lens[]="DB_TelescopeLens";
            uint32_t *name=*(uint32_t **)(e+0x38u);
            if(!readable(name,8u) || !name[0] || name[0]==UINT32_MAX ||
                !readable((void *)(uintptr_t)name[1],sizeof(lens)) ||
                memcmp((void *)(uintptr_t)name[1],lens,sizeof(lens))) return FALSE;
        }
        *kind=SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND; return TRUE;
    }
    if(*(void **)e==base+0x2d65a0u) {
        if(*(void **)(e+8u)!=base+0x2d65c4u || *(void **)(e+0x2cu)!=base+0x2d65e4u ||
            (*(uint32_t *)(e+0x30u)&0x1fffu)!=SUDEKIMP_LAN_STORY_WORLD_NPC_KIND) return FALSE;
        *kind=SUDEKIMP_LAN_STORY_WORLD_NPC_KIND; return TRUE;
    }
    unsigned c=0;
    for(;c<4u && roster->actors[c]!=entity;++c) {}
    if(c>=4u || !(roster->available_mask&(1u<<c)) ||
        *(void **)e!=base+vtables[c][0] || *(void **)(e+8u)!=base+vtables[c][1] ||
        *(void **)(e+0x2cu)!=base+vtables[c][2] ||
        (*(uint32_t *)(e+0x30u)&0x1f80u)!=0x0f80u ||
        *(uint32_t *)(e+0x34u)!=SudekiMpLanStoryWorldCharacterIdentifier(c)) return FALSE;
    uint32_t *name=*(uint32_t **)(e+0x38u);
    size_t length=strlen(names[c])+1u;
    if(!readable(name,8u) || !name[0] || name[0]==UINT32_MAX ||
        !readable((void *)(uintptr_t)name[1],length) ||
        memcmp((void *)(uintptr_t)name[1],names[c],length)) return FALSE;
    uint8_t *m=*(uint8_t **)(e+0x130u);
    if(!readable(m,0x168u) || *(void **)(m+0x10u)!=e ||
        *(void **)m!=base+((c==1u || c==3u)?0x2d5464u:0x2c8504u)) return FALSE;
    *model=m; *character=c; *kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND; return TRUE;
}
static BOOL renderer_target(Target t,Target *out,BOOL write);
static BOOL target(void *entity,const SudekiMpLanStoryNativeRoster *roster,Target *out,BOOL write) {
    Target t={.entity=entity};
    observe_stage("entity_identity",0,0);
    if(!entity_identity(entity,roster,&t.character,&t.kind,&t.model)) return FALSE;
    t.identifier=*(uint32_t *)(t.entity+0x34u);
    observe_stage("entity_resource",t.identifier,0);
    uint32_t *name=*(uint32_t **)(t.entity+0x38u);
    if(!name || !readable(name,8u) || !name[0] || !name[1]) return FALSE;
    t.position=*(uint8_t **)(t.entity+0x44u);
    observe_stage("position",t.identifier,0);
    /* Exact54C8A0 embeds this NPCEntity's CPosition at+150; this is not the
     * unrelated playable-character model-component layout. */
    if((t.kind==SUDEKIMP_LAN_STORY_WORLD_NPC_KIND && t.position!=t.entity+0x150u) ||
        (scenery(&t) && t.position!=t.entity+0x210u) ||
        !memory(t.position,0x104u,write) ||
        *(void **)t.position!=base+0x2cdefcu || *(void **)(t.position+0x10u)!=entity) return FALSE;
    uintptr_t parent=*(uintptr_t *)(t.position+0x94u);
    if(parent && parent!=4u) return FALSE;
    t.wrapper=t.attached_wrapper=*(uint8_t **)(t.position+0xb4u);
    observe_stage("wrapper",t.identifier,0);
    if(!readable(t.wrapper,0x14u)) return FALSE;
    if(t.character==1u || t.character==3u) {
        void *world=*(void **)(t.model+0x164u),*arms=*(void **)(t.model+0x160u);
        /* Retain the world-bank identity, but do not treat its dormant pose
         * as current while arms are attached. read_pose projects the live
         * authored arms state into this bank without mutating the host.
         * Replica writes still require the world model attached locally. */
        if(t.wrapper==arms) {
            if(write || !world || world==arms || !readable(world,0x14u)) return FALSE;
            t.wrapper=world;
        } else if(world && world!=t.wrapper) return FALSE;
    }
    return renderer_target(t,out,write);
}
static BOOL renderer_target(Target t,Target *out,BOOL write) {
    if(!readable(t.wrapper,0x14u)) return FALSE;
    t.object=*(uint8_t **)(t.wrapper+8u); t.renderer=*(uint8_t **)(t.wrapper+0x10u);
    observe_stage("renderer",t.identifier,0);
    if(!memory(t.object,0xd0u,write) || !memory(t.renderer,0xb0u,write) ||
        *(void **)(t.object+0x14u)!=t.renderer || *(void **)(t.object+0x18u) ||
        *(void **)t.renderer!=base+0x2df8ecu || !methods_exact()) return FALSE;
    t.bank=*(uint8_t **)(t.renderer+8u);
    observe_stage("bank",t.identifier,0);
    if(!readable(t.bank,0x5cu)) return FALSE;
    t.description=*(uint8_t **)(t.bank+0x1cu); t.entries=*(uint8_t **)(t.bank+0x20u);
    if(!readable(t.description,0x24u)) return FALSE;
    t.animations=*(uint32_t *)t.description; t.submodels=*(uint32_t *)(t.description+0xcu);
    t.channel_count=scenery(&t)?1u:(t.character==1u || t.character==3u)?5u:4u;
    if(!t.animations || t.animations>MAX_ANIMATIONS || !t.submodels || t.submodels>MAX_SUBMODELS ||
        !readable(t.entries,t.animations*28u) || *(uint32_t *)(t.renderer+0xa0u)!=t.channel_count ||
        *(uint32_t *)(t.renderer+0xa4u)!=t.channel_count-1u) return FALSE;
    t.channels=*(uint8_t **)(t.renderer+0x98u); t.blends=*(uint8_t **)(t.renderer+0x9cu);
    observe_stage("channel_topology",t.identifier,t.submodels);
    if(!memory(t.channels,t.channel_count*36u,write) || !blend_storage(&t,write)) return FALSE;
    for(unsigned c=0;c<t.channel_count;++c) {
        observe_stage("channel_storage",t.identifier,c);
        t.rows[c]=*(uint8_t **)(t.channels+c*36u);
        if(!memory(t.rows[c],t.submodels*24u,write)) return FALSE;
    }
    if(!bank_fingerprint(&t,&t.fingerprint)) return FALSE;
    *out=t; return TRUE;
}
static BOOL same_target(const Target *a,const Target *b) {
    return a->entity==b->entity && a->position==b->position && a->model==b->model && a->wrapper==b->wrapper &&
        a->attached_wrapper==b->attached_wrapper &&
        a->object==b->object && a->renderer==b->renderer && a->bank==b->bank &&
        a->description==b->description && a->entries==b->entries && a->channels==b->channels &&
        a->blends==b->blends && !memcmp(a->rows,b->rows,sizeof(a->rows)) &&
        a->kind==b->kind && a->character==b->character && a->channel_count==b->channel_count &&
        a->identifier==b->identifier && a->animations==b->animations &&
        a->submodels==b->submodels && a->fingerprint==b->fingerprint;
}
static int target_order(const void *left,const void *right) {
    const Target *a=left,*b=right;
    if(a->kind!=b->kind) return a->kind<b->kind?-1:1;
    return a->identifier<b->identifier?-1:a->identifier>b->identifier;
}
static BOOL catalog(const Registry *registry,const SudekiMpLanStoryNativeRoster *roster,
    Target *targets,unsigned *count,BOOL write) {
    unsigned n=0;
    for(unsigned i=0;i<registry->count;++i) {
        observe_stage("registry_entry",0,i);
        uint8_t *entity=registry->entities[i];
        if(!readable(entity,0x3cu)) return FALSE;
        BOOL include=*(void **)entity==base+0x2d65a0u;
        for(unsigned c=0;c<4u;++c) if(roster->actors[c]==entity) include=TRUE;
        if(*(void **)entity==base+0x2d5b00u) {
            unsigned character; uint16_t kind; uint8_t *model;
            if(!entity_identity(entity,roster,&character,&kind,&model)) return FALSE;
            /* Native generic marker/audio entities have no render wrapper.
             * A malformed nonnull wrapper is rejected by target(). */
            uint8_t *position=*(uint8_t **)(entity+0x44u);
            if(!readable(position,0xb8u) || *(void **)position!=base+0x2cdefcu ||
                *(void **)(position+0x10u)!=entity) return FALSE;
            include=*(void **)(position+0xb4u)!=NULL;
        }
        if(!include) continue;
        if(n==SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS || !target(entity,roster,&targets[n],write)) return FALSE;
        for(unsigned j=0;j<n;++j)
            if(targets[j].entity==entity || (targets[j].kind==targets[n].kind &&
                targets[j].identifier==targets[n].identifier)) {
                observe_stage("duplicate_entity",targets[n].identifier,n); return FALSE;
            }
        ++n;
    }
    for(unsigned c=0;c<4u;++c) if(roster->available_mask&(1u<<c)) {
        unsigned matches=0;
        for(unsigned i=0;i<n;++i) if(targets[i].entity==roster->actors[c] && targets[i].character==c) ++matches;
        if(matches!=1u) return FALSE;
    }
    qsort(targets,n,sizeof(*targets),target_order); *count=n;
    return registry_still(registry);
}
static BOOL read_native_pose(const Target *t,SudekiMpLanStoryWorldActor *a) {
    observe_stage("transform",t->identifier,0);
    memset(a,0,sizeof(*a)); a->kind=t->kind;
    a->identifier=t->identifier; a->submodels=(uint8_t)t->submodels; a->bank_fingerprint=t->fingerprint;
    if(scenery(t) && (*(uint32_t *)(t->object+0x34u)&4u))
        a->visual_flags=SUDEKIMP_LAN_STORY_WORLD_HIDDEN;
    memcpy(a->position,t->position+0x18u,sizeof(a->position));
    memcpy(a->forward,t->position+0x50u,sizeof(a->forward));
    float length=0;
    for(unsigned i=0;i<3u;++i) {
        if(!isfinite(a->position[i]) || fabsf(a->position[i])>=1000000.0f ||
            !isfinite(a->forward[i])) return FALSE;
        length+=a->forward[i]*a->forward[i];
    }
    length=sqrtf(length); if(!isfinite(length) || length<.0001f || length>1000.0f) return FALSE;
    for(unsigned i=0;i<3u;++i) a->forward[i]/=length;
    /* This function only reads already owned structures. Reuse range proofs
     * within this traversal; every header value is still read, with no proof
     * carried across a native call or into another observation. */
    BankHeaderSpan headers={0};
    for(unsigned c=0;c<pose_channels(t);++c) {
        observe_stage("channel_pose",t->identifier,c);
        uint8_t *row=t->rows[c]; unsigned selector=*(uint16_t *)row,state=*(uint16_t *)(row+2u);
        if(selector>=t->animations || state>255u) return FALSE;
        float rate=*(float *)(row+4u),time=*(float *)(row+8u),phase=*(float *)(row+0xcu);
        if(!isfinite(rate) || !isfinite(time) || !isfinite(phase)) return FALSE;
        for(unsigned sub=1;sub<t->submodels;++sub) {
            uint8_t *other=row+sub*24u;
            if(*(uint16_t *)other!=selector || *(uint16_t *)(other+2u)!=state ||
                memcmp(row+4u,other+4u,12u)) {
                observe_stage("submodel_pose",t->identifier,(c<<16)|sub); return FALSE;
            }
        }
        BOOL authored=selector>=first_clip(t);
        a->clip[c]=authored?**(uint32_t **)(t->entries+selector*28u):0u;
        if(authored) {
            unsigned occurrence=0;
            for(unsigned previous=first_clip(t);previous<selector;++previous) {
                uint32_t *resource=*(uint32_t **)(t->entries+previous*28u);
                if(!bank_header_readable(resource,&headers)) return FALSE;
                if(*resource==a->clip[c]) ++occurrence;
            }
            a->clip_occurrence[c]=(uint16_t)occurrence;
        }
        a->state[c]=(uint8_t)state; a->rate[c]=rate;
        a->time[c]=!authored?0.0f:(state&1u)?time:phase;
    }
    for(unsigned i=0;i<pose_blends(t);++i) a->blend[i]=*(float *)(t->blends+i*20u+0xcu);
    return TRUE;
}
static int authored_selector(const Target *t,uint32_t handle) {
    if(!handle || handle==0x7ffffu) return -1;
    for(unsigned s=1;s<t->animations;++s) {
        uint32_t *header=*(uint32_t **)(t->entries+s*28u);
        if(!readable(header,0x20u)) return -1;
        if(*header==handle) return (int)s; /* native lookup's first match */
    }
    return -1;
}
static BOOL read_pose_detail(const Target *t,SudekiMpLanStoryWorldActor *a,BOOL *ranged_base) {
    if(ranged_base) *ranged_base=FALSE;
    if(t->wrapper==t->attached_wrapper) return read_native_pose(t,a);
    observe_stage("ranged_world_projection",t->identifier,0);
    uint8_t *arbiter,*table,*states; Target arms=*t;
    if((t->character!=1u && t->character!=3u) ||
        *(void **)(t->entity+0x134u)!=t->model ||
        *(void **)(t->model+0x160u)!=t->attached_wrapper ||
        *(void **)(t->model+0x164u)!=t->wrapper ||
        !readable(arbiter=*(uint8_t **)(t->entity+0x90u),0x64u) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10u)!=t->entity ||
        !(*(uint32_t *)(arbiter+0x50u)&0x400000u) ||
        !readable(table=*(uint8_t **)(t->model+0xdcu),0x414u) ||
        !readable(states=*(uint8_t **)(t->model+0xf8u),20u)) return FALSE;
    uint8_t semantics[20]; memcpy(semantics,states,sizeof(semantics));
    arms.wrapper=t->attached_wrapper;
    SudekiMpLanStoryWorldActor source;
    if(!renderer_target(arms,&arms,FALSE) || !read_native_pose(&arms,&source) ||
        !read_native_pose(t,a)) return FALSE;
    /* All output clip identities/fingerprints belong to the body bank.
     * Neither dormant body rows nor first-person selector numbers authorize
     * a body pose. Source rows, semantic definition, and both authored banks
     * must agree before translating any active channel. */
    for(unsigned c=0;c<5u;++c) {
        a->clip[c]=0; a->clip_occurrence[c]=0;
        a->state[c]=192u; a->time[c]=a->rate[c]=0;
        if(!source.clip[c]) {
            if(source.state[c]!=192u || source.time[c]!=0) return FALSE;
            continue;
        }
        unsigned semantic=semantics[c*4u+2u];
        observe_stage("ranged_world_semantic",t->identifier,(c<<16)|semantic);
        uint8_t *definition=*(uint8_t **)(table+0x14u+semantic*4u);
        if(!readable(definition,0x28u) || source.clip_occurrence[c] ||
            (source.clip[c]!=*(uint32_t *)(definition+0x14u) &&
             source.clip[c]!=*(uint32_t *)(definition+0x20u))) return FALSE;
        unsigned mapped=SudekiMpRangedWorldSemantic(semantic);
        if(semantic>=0x8cu && semantic<=0x8eu) {
            /* The equipped missile record, not FP clip length, chooses the
             * world fire family. The existing Test Room adapter uses these
             * exact fields too: Ailish item23 is light FP / heavy world. */
            uint8_t *manager=*(uint8_t **)(t->entity+0xbcu),*record;
            if(!readable(manager,0x64u) || *(void **)manager!=base+0x2d4c8cu ||
                *(void **)(manager+0x10u)!=t->entity ||
                !readable(record=*(uint8_t **)(manager+0x60u),0xc4u) ||
                *(uint32_t *)(record+0x9cu)!=semantic) return FALSE;
            mapped=*(uint32_t *)(record+0x98u);
            if(mapped<0x85u || mapped>0x87u) return FALSE;
        }
        uint8_t *world_definition=*(uint8_t **)(table+0x14u+mapped*4u);
        if(!readable(world_definition,0x28u)) return FALSE;
        /* In first-person combat the ordinary bank's first handle is the
         * combat body; the second can be an exploration idle/move. */
        int selector=authored_selector(t,*(uint32_t *)(world_definition+0x14u));
        if(selector<1) selector=authored_selector(t,*(uint32_t *)(world_definition+0x20u));
        int source_selector=authored_selector(&arms,source.clip[c]);
        if(selector<1 || source_selector<1) return FALSE;
        uint8_t *resource=*(uint8_t **)(t->entries+(unsigned)selector*28u);
        uint8_t *source_resource=*(uint8_t **)(arms.entries+(unsigned)source_selector*28u);
        if(!SudekiMpRangedWorldClock(*(float *)(source_resource+4u),*(float *)(resource+4u),
            source.time[c],source.rate[c],&a->time[c],&a->rate[c])) return FALSE;
        a->clip[c]=*(uint32_t *)resource; a->state[c]=source.state[c];
    }
    memcpy(a->blend,source.blend,sizeof(a->blend));
    /* FP blends move a missile clip from channel2 to channel0. Repeating
     * that topology on a world body repeatedly replaces its base motion.
     * The established world ranged adapter instead owns channel4/blend3.
     * Admit only the positively observed idle/fire topology here; unrelated
     * actions keep the semantic bridge above, never an inferred fire event. */
    BOOL firing_topology=TRUE; int fire=-1;
    for(unsigned c=0;c<5u;++c) if(source.clip[c]) {
        unsigned semantic=semantics[c*4u+2u];
        if((c!=0u && c!=2u) ||
            (semantic!=5u && (semantic<0x8cu || semantic>0x8eu))) firing_topology=FALSE;
        if(semantic>=0x8cu && semantic<=0x8eu) {
            if(fire>=0) firing_topology=FALSE;
            fire=(int)c;
        }
    }
    if(firing_topology && source.clip[0] &&
        (source.clip[2] || source.blend[2]==0) &&
        source.blend[0]==0 && source.blend[1]==0 && source.blend[3]==0) {
        uint8_t *idle_definition=*(uint8_t **)(table+0x14u+2u*4u);
        if(!readable(idle_definition,0x28u)) return FALSE;
        int idle=authored_selector(t,*(uint32_t *)(idle_definition+0x14u));
        if(idle<1) return FALSE; /* an exploration fallback is not combat idle */
        uint8_t *idle_resource=*(uint8_t **)(t->entries+(unsigned)idle*28u);
        float idle_length=*(float *)(idle_resource+4u);
        if(!isfinite(idle_length) || idle_length<=0) return FALSE;
        SudekiMpLanStoryWorldActor projected=*a;
        for(unsigned c=0;c<5u;++c) {
            a->clip[c]=0; a->clip_occurrence[c]=0;
            a->state[c]=192u; a->time[c]=a->rate[c]=0;
        }
        memset(a->blend,0,sizeof(a->blend));
        a->clip[0]=*(uint32_t *)idle_resource;
        a->state[0]=0; a->rate[0]=12.0f; /* established native world idle */
        if(fire>=0) {
            a->clip[4]=projected.clip[fire];
            a->state[4]=projected.state[fire];
            a->time[4]=projected.time[fire]; a->rate[4]=projected.rate[fire];
            a->blend[3]=fire==0?1.0f-source.blend[2]:source.blend[2];
        }
        if(ranged_base) *ranged_base=TRUE;
    }
    /* A fully empty result is never an acceptable substitute for a body. */
    if(!a->clip[0] && !a->clip[1] && !a->clip[2] && !a->clip[3] && !a->clip[4]) return FALSE;
    return *(void **)(t->position+0xb4u)==t->attached_wrapper &&
        *(void **)(t->model+0xdcu)==table && *(void **)(t->model+0xf8u)==states &&
        !memcmp(semantics,states,sizeof(semantics));
}
static BOOL read_pose(const Target *t,SudekiMpLanStoryWorldActor *a) {
    return read_pose_detail(t,a,NULL);
}
static BOOL retain_ranged_base(const Target *t,const Bound *old,uint32_t now,
    SudekiMpLanStoryWorldActor *a) {
    if(!old || !old->ranged_base || !same_target(&old->target,t) ||
        old->previous.clip[0]!=a->clip[0] || old->previous.state[0]!=a->state[0] ||
        now-old->tick>250u) return TRUE; /* new owner/gap starts a fresh base */
    int selector=authored_selector(t,a->clip[0]);
    if(selector<1) return FALSE;
    uint8_t *resource=*(uint8_t **)(t->entries+(unsigned)selector*28u);
    float length=*(float *)(resource+4u);
    if(!isfinite(length) || length<=0 || !isfinite(old->previous.time[0])) return FALSE;
    a->time[0]=fmodf(old->previous.time[0]+a->rate[0]*(now-old->tick)/1000.0f,length);
    return isfinite(a->time[0]) && a->time[0]>=0;
}
static BOOL animation_edge(const SudekiMpLanStoryWorldActor *old,const SudekiMpLanStoryWorldActor *next) {
    if(memcmp(old->clip,next->clip,sizeof(old->clip)) ||
        memcmp(old->clip_occurrence,next->clip_occurrence,sizeof(old->clip_occurrence)) ||
        memcmp(old->state,next->state,sizeof(old->state))) return TRUE;
    for(unsigned i=0;i<5u;++i)
        if(next->clip[i] && next->rate[i]>=0.0f && next->time[i]+.0001f<old->time[i]) return TRUE;
    return FALSE;
}
BOOL SudekiMpLanStoryWorldCapture(SudekiMpLanPartySession *session,void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,
    const SudekiMpLanStoryFrame *party,SudekiMpLanStoryWorldFrame *out) {
    SudekiMpLanStoryNativeRoster roster; Registry registry;
    Target targets[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS]; unsigned count=0;
    Bound next[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS]={0};
    SudekiMpLanStoryWorldFrame frame={0};
    if(!session || !out || !party || !scene || SudekiMpLanPartyLocalSeat(session)!=0u ||
        !SudekiMpLanStoryFrameMatchesScene(party,scene) || !begin()) return FALSE;
    DWORD error=ERROR_RETRY;
    operation="capture"; observe_stage("host_witness_registry",0,0);
    if(client_seen || (host_seen && ((int32_t)(party->sequence-host_sequence)<=0 || party->epoch<host_epoch)) ||
        !SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) || !registry_capture(&registry) ||
        !catalog(&registry,&roster,targets,&count,FALSE)) goto fail;
    frame.epoch=party->epoch; frame.revision=party->revision; frame.sequence=party->sequence;
    frame.host_tick=party->host_tick; frame.count=(uint8_t)count;
    uint32_t generation=next_generation;
    for(unsigned i=0;i<count;++i) {
        next[i].target=targets[i];
        if(!read_pose_detail(&targets[i],&frame.actors[i],&next[i].ranged_base)) {
            error=ERROR_NOT_SUPPORTED; goto fail;
        }
        next[i].tick=party->host_tick;
        const Bound *old=NULL;
        if(host_seen && host_epoch==party->epoch) for(unsigned j=0;j<host_count;++j)
            if(host_bound[j].previous.kind==targets[i].kind &&
                host_bound[j].previous.identifier==targets[i].identifier) { old=&host_bound[j]; break; }
        if(next[i].ranged_base && !retain_ranged_base(&targets[i],old,party->host_tick,&frame.actors[i])) {
            error=ERROR_NOT_SUPPORTED; goto fail;
        }
        if(old && same_target(&old->target,&targets[i])) {
            frame.actors[i].generation=old->previous.generation;
            frame.actors[i].animation_sequence=old->previous.animation_sequence;
            if(animation_edge(&old->previous,&frame.actors[i])) {
                if(frame.actors[i].animation_sequence==UINT32_MAX) { error=ERROR_ARITHMETIC_OVERFLOW; goto fail; }
                ++frame.actors[i].animation_sequence;
            }
        } else {
            if(generation==UINT32_MAX) { error=ERROR_ARITHMETIC_OVERFLOW; goto fail; }
            frame.actors[i].generation=++generation; frame.actors[i].animation_sequence=1u;
        }
        if(targets[i].character<4u) {
            const SudekiMpLanStoryActor *actor=&party->actors[targets[i].character];
            if(!actor->native_pose) { error=ERROR_NOT_SUPPORTED; goto fail; }
            frame.actors[i].generation=actor->generation;
        }
        if(!SudekiMpLanStoryWorldActorValid(&frame.actors[i])) {
            observe_stage("wire_pose_bounds",targets[i].identifier,i); error=ERROR_NOT_SUPPORTED; goto fail;
        }
        next[i].previous=frame.actors[i];
    }
    if(!registry_still(&registry) || !SudekiMpLanStoryObserverRosterStillExact(w,&roster)) goto fail;
    for(unsigned i=0;i<count;++i) {
        Target check;
        if(!target(targets[i].entity,&roster,&check,FALSE) || !same_target(&targets[i],&check)) goto fail;
    }
    if(!registry_still(&registry) || !SudekiMpLanStoryWorldFrameMatches(&frame,party) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,&roster)) goto fail;
    memcpy(host_bound,next,sizeof(next)); host_count=count; host_epoch=party->epoch;
    host_sequence=party->sequence; next_generation=generation; host_seen=TRUE; *out=frame;
    return finish(TRUE,0);
fail:
    return finish(FALSE,error);
}

static BOOL target_still(const Target *t,const Registry *registry,
    const SudekiMpLanStoryNativeRoster *roster,SudekiMpLanStoryReplicaExact exact,void *context) {
    observe_stage("write_owner",t->identifier,0);
    unsigned character; uint16_t kind; uint8_t *model;
    if(!exact(roster,context) || !entity_identity(t->entity,roster,&character,&kind,&model) ||
        character!=t->character || kind!=t->kind || model!=t->model) return FALSE;
    if(*(void **)(base+0x409d8cu)!=registry->owner ||
        !readable(registry->owner,0x40u) || *(uint32_t *)(registry->owner+0x34u)!=registry->count ||
        *(void **)(registry->owner+0x3cu)!=registry->entries ||
        *(uint32_t *)(t->entity+0x34u)!=t->identifier ||
        *(void **)(t->entity+0x44u)!=t->position ||
        (t->kind==SUDEKIMP_LAN_STORY_WORLD_NPC_KIND && t->position!=t->entity+0x150u) ||
        (scenery(t) && t->position!=t->entity+0x210u) ||
        !writable(t->position,0x104u) || *(void **)t->position!=base+0x2cdefcu ||
        *(void **)(t->position+0x10u)!=t->entity || *(void **)(t->position+0xb4u)!=t->wrapper ||
        !readable(t->wrapper,0x14u) || *(void **)(t->wrapper+8u)!=t->object ||
        *(void **)(t->wrapper+0x10u)!=t->renderer || !writable(t->object,0xd0u) ||
        *(void **)(t->object+0x14u)!=t->renderer || *(void **)(t->object+0x18u) ||
        !writable(t->renderer,0xb0u) || *(void **)t->renderer!=base+0x2df8ecu ||
        *(void **)(t->renderer+8u)!=t->bank || !readable(t->bank,0x5cu) ||
        *(void **)(t->bank+0x1cu)!=t->description || *(void **)(t->bank+0x20u)!=t->entries ||
        !readable(t->description,0x24u) || *(uint32_t *)t->description!=t->animations ||
        *(uint32_t *)(t->description+0xcu)!=t->submodels ||
        *(void **)(t->renderer+0x98u)!=t->channels || *(void **)(t->renderer+0x9cu)!=t->blends ||
        *(uint32_t *)(t->renderer+0xa0u)!=t->channel_count ||
        *(uint32_t *)(t->renderer+0xa4u)!=t->channel_count-1u ||
        !writable(t->channels,t->channel_count*36u) ||
        !blend_storage(t,TRUE) || !methods_exact()) return FALSE;
    uintptr_t parent=*(uintptr_t *)(t->position+0x94u);
    if(parent && parent!=4u) return FALSE;
    for(unsigned c=0;c<t->channel_count;++c)
        if(*(void **)(t->channels+c*36u)!=t->rows[c] || !writable(t->rows[c],t->submodels*24u)) return FALSE;
    if(t->character==1u || t->character==3u) {
        void *world=*(void **)(t->model+0x164u),*arms=*(void **)(t->model+0x160u);
        if(t->wrapper==arms || (world && world!=t->wrapper)) return FALSE;
    }
    return TRUE;
}
static BOOL resolve_clip(const Target *t,uint32_t handle,unsigned occurrence,unsigned *selector) {
    if(!handle) { *selector=0; return !scenery(t) && occurrence==0u; }
    if(occurrence>=MAX_ANIMATIONS-1u) return FALSE;
    unsigned matches=0;
    BankHeaderSpan headers={0};
    for(unsigned i=first_clip(t);i<t->animations;++i) {
        uint8_t *resource=*(uint8_t **)(t->entries+i*28u);
        if(!bank_header_readable(resource,&headers)) return FALSE;
        if(*(uint32_t *)resource==handle) {
            if(matches++==occurrence) { *selector=i; return TRUE; }
        }
    }
    return FALSE;
}
static BOOL selector_available(const Target *t,unsigned selector) {
    observe_stage("clip_residency",t->identifier,selector);
    if(selector>=t->animations || !readable(t->entries,t->animations*28u)) return FALSE;
    uint8_t *resource=*(uint8_t **)(t->entries+selector*28u);
    if(!readable(resource,0x20u)) return FALSE;
    uint32_t dependency=*(uint32_t *)(resource+0x1cu);
    if(!dependency) return TRUE;
    uint32_t index=dependency&0x7fffffffu;
    uint8_t *package=*(uint8_t **)(t->bank+0x10u);
    if(!readable(package,0x18u)) return FALSE;
    uint8_t *header=*(uint8_t **)(package+4u),*rows=*(uint8_t **)(package+0x14u);
    if(!readable(header,6u)) return FALSE;
    unsigned row_count=*(uint16_t *)(header+4u);
    /* Native626170 allocates12*uint16(header+4) for package+14. The
     * renderer's reference bytes have a DIFFERENT bound:61FDEA allocates
     * descriptor+20 minus one byte. Both bounds precede the native predicate. */
    if(!row_count || index>=row_count || !readable(rows,row_count*12u) || !(rows[index*12u+4u]&1u)) return FALSE;
    if(dependency&0x80000000u) return TRUE;
    unsigned count=*(uint32_t *)(t->description+0x20u);
    uint8_t *references=*(uint8_t **)(t->renderer+0xacu);
    return index && count>1u && count<=65535u && index<count &&
        readable(references,count-1u) && references[index-1u]!=0u;
}
typedef struct VisualOutput {
    SudekiMpLanStoryCurveKind kind;
    unsigned floats;
} VisualOutput;
static BOOL selector_binding_closed(const Target *t,unsigned index,const uint32_t *binding,
    VisualOutput *output);
static BOOL curve_preflight(const Target *t,const void *chunk,const void *curve,
    const VisualOutput *output) {
    if(!readable(curve,8u)) return FALSE;
    unsigned table=output->kind==SUDEKIMP_STORY_CURVE_WEIGHTS?0x2de748u:
        output->kind==SUDEKIMP_STORY_CURVE_UV_MATRIX?0x2de6ecu:
        output->kind==SUDEKIMP_STORY_CURVE_COLOR?0x2de70cu:0x2df2a8u;
    if(*(void *const *)curve!=base+table) return FALSE;
    const void *header=*(void *const *)((const uint8_t *)curve+4u);
    for(unsigned i=0;i<curve_proof_count;++i) {
        const CurveProof *p=&curve_proofs[i];
        if(p->bank==t->bank && p->chunk==chunk && p->curve==curve && p->header==header &&
            p->kind==output->kind && p->floats==output->floats) return TRUE;
    }
    if(!SudekiMpLanStoryCurveExact((HMODULE)base,curve,output->kind,output->floats)) return FALSE;
    if(curve_proof_count<MAX_CURVE_PROOFS)
        curve_proofs[curve_proof_count++]=(CurveProof){t->bank,chunk,curve,header,output->kind,output->floats};
    return TRUE;
}
static BOOL selector_events_closed(const Target *t,unsigned channel,unsigned selector,BOOL starting) {
    observe_stage("clip_child_controller",t->identifier,(channel<<16)|selector);
    if(selector>=t->animations || !readable(t->entries,t->animations*28u)) return FALSE;
    uint8_t *entry=t->entries+selector*28u,*resource=*(uint8_t **)entry;
    if(!readable(resource,0x20u)) return FALSE;
    unsigned events=*(uint32_t *)(resource+0x18u);
    if(!events) return TRUE;
    unsigned count=*(uint32_t *)(t->description+0x1cu);
    uint8_t *event_rows=*(uint8_t **)(entry+0x14u),*bindings=*(uint8_t **)(t->renderer+0x94u);
    if(events>MAX_CHILD_ROWS || !count || count>MAX_CHILD_ROWS ||
        !readable(event_rows,events*8u) || !writable(bindings,count*12u)) return FALSE;
    uint8_t **curves=NULL;
    uint8_t *chunk=NULL;
    unsigned curve_count=0;
    if(starting) {
        /*61F330 indexes bank+78 with the unmasked dependency. A negative
         * residency identifier is therefore NOT a valid curve-table index. */
        unsigned dependency=*(uint32_t *)(resource+0x1cu);
        unsigned packages=*(uint32_t *)(t->description+0x20u);
        if(!readable(t->bank,0x7cu) || !packages || packages>65535u || dependency>=packages) return FALSE;
        uint8_t **chunks=*(uint8_t ***)(t->bank+0x78u);
        if(!readable(chunks,packages*sizeof(*chunks)) || !readable(chunks[dependency],0x10u)) return FALSE;
        chunk=chunks[dependency];
        uint8_t *header=*(uint8_t **)chunk;
        if(!readable(header,8u)) return FALSE;
        curve_count=*(uint32_t *)(header+4u);
        curves=*(uint8_t ***)(chunk+0xcu);
        if(!curve_count || curve_count>MAX_CHILD_ROWS || !readable(curves,curve_count*sizeof(*curves))) return FALSE;
    }
    for(unsigned i=0;i<events;++i) {
        unsigned index=*(uint32_t *)(event_rows+i*8u+4u);
        if(index>=count) return FALSE;
        uint32_t *binding=(uint32_t *)(bindings+index*12u);
        if(starting || (binding[2]==channel && binding[0] && (binding[1]&0x100u))) {
            VisualOutput output={SUDEKIMP_STORY_CURVE_WEIGHTS,0};
            if(!selector_binding_closed(t,index,starting?NULL:binding,&output)) return FALSE;
            if(starting && output.floats) {
                unsigned curve=*(uint32_t *)(event_rows+i*8u);
                observe_stage("clip_curve",t->identifier,index);
                if(curve>=curve_count || !curve_preflight(t,chunk,curves[curve],&output)) return FALSE;
            }
        }
    }
    return TRUE;
}
static BOOL selector_cleanup_closed(const Target *t,unsigned channel,unsigned selector) {
    if(!selector_available(t,selector)) return FALSE;
    unsigned old=*(uint16_t *)t->rows[channel];
    if(old==selector || !(*(uint32_t *)(t->renderer+0x8cu)&(1u<<channel))) return TRUE;
    /*623000 ->622820 ->61F410 flag0 only enters a child callback for a
     * matching active binding carrying bit100. Empty bindings still receive
     * their normal native cleanup; a flagged channel alone is not unsafe. */
    return selector_events_closed(t,channel,old,FALSE);
}
static BOOL selector_change_closed(const Target *t,unsigned channel,unsigned selector) {
    /* Normal native rendering may start the desired clip's attached targets
     * even at delta0. Admit its complete reachable target graph in the
     * read-only preflight, before player or NPC presentation mutates anything. */
    return selector_available(t,selector) &&
        selector_events_closed(t,channel,selector,TRUE) &&
        selector_cleanup_closed(t,channel,selector);
}
static BOOL code_hash(unsigned rva,unsigned size,uint32_t expected) {
    uint32_t value=2166136261u;
    for(unsigned i=0;i<size;++i) value=(value^base[rva+i])*16777619u;
    return value==expected;
}
/*61FAB0 creates initial children from bank+68, but native621220 also attaches
 * external children by their virtual+C0 slot. NPC bodies use the latter path;
 * the bank resource list need not contain their retained resources. Mirror
 * the current parent-slot ownership and exact child getter without calling it. */
static BOOL child_resource_exact(const Target *t,unsigned slot,uint8_t *child,uint8_t *resource) {
    if(!readable(t->bank,0x5cu) || !readable(t->renderer,0x68u) ||
        !readable(child,0x18u)) return FALSE;
    unsigned count=*(uint32_t *)(t->bank+0x58u);
    uint8_t **children=*(uint8_t ***)(t->renderer+0x64u);
    if(!count || count>MAX_CHILDREN || slot>=count ||
        !readable(children,count*sizeof(*children)) || children[slot]!=child) return FALSE;
    for(unsigned i=0;i<count;++i) if(i!=slot && children[i]==child) return FALSE;
    unsigned table,offset,getter,resource_table,resource_size;
    uint32_t hash;
    if(*(void **)child==base+0x2dec74u) {
        table=0x2dec74u; offset=0x10u; getter=0x203740u; hash=0xd01087f1u;
        resource_table=0x2debf8u; resource_size=0x7cu;
    } else if(*(void **)child==base+0x2dfb14u) {
        table=0x2dfb14u; offset=0x14u; getter=0x2240a0u; hash=0xd08f7395u;
        resource_table=0x2dfaf0u; resource_size=0x24u;
    } else if(*(void **)child==base+0x2de564u) {
        table=0x2de564u; offset=0x10u; getter=0x203740u; hash=0xd01087f1u;
        resource_table=0x2de520u; resource_size=0x44u;
    } else return FALSE;
    if(*(void **)(base+table+0xc0u)!=base+getter || !code_hash(getter,10u,hash) ||
        *(void **)(child+offset)!=resource || !readable(resource,resource_size) ||
        *(void **)resource!=base+resource_table ||
        !(*(uint32_t *)(resource+8u)&0x3fffffffu)) return FALSE;
    /* Constructors605540/6244C0 retain resource+8; both destructors test
     * its low30 reference bits. The exact slot getters
     * read the resource descriptor at+14, then its authored slot at+4. */
    uint8_t *definition=*(uint8_t **)(resource+0x14u);
    if(!readable(definition,8u) || *(uint32_t *)(definition+4u)!=slot) return FALSE;
    if(table==0x2dec74u) {
        /*621220 also requires the attached skin's skeleton count to
         * match the parent bank.6038B0 is its complete no-call getter. */
        if(!readable(t->description,8u) ||
            *(void **)(base+table+0x12cu)!=base+0x2038b0u ||
            !code_hash(0x2038b0u,9u,0x1b8541d1u)) return FALSE;
        uint8_t *header=*(uint8_t **)(resource+0x78u);
        if(!readable(header,4u) || *(uint32_t *)header!=*(uint32_t *)(t->description+4u)) return FALSE;
    }
    return TRUE;
}
static BOOL mcon_output(const Target *t,uint8_t *owner,uint8_t *resource,unsigned token,
    unsigned argument,unsigned member,VisualOutput *output) {
    observe_stage("material_graph",t->identifier,token);
    /* The complete original/override graph is preflighted for every child by
     * material_children_closed, including children with an empty token list.
     * Immediate selector cleanup never prepares or replaces those materials. */
    if(*(void **)(owner+0xcu)!=base+0x2dedd8u ||
        *(void **)(base+0x2dedd8u+8u)!=base+0x2038c0u) return FALSE;
    uint8_t *manager=*(uint8_t **)(resource+0x2cu);
    if(!readable(manager,8u)) return FALSE;
    unsigned count=*(uint32_t *)manager;
    if(!count || count>MAX_CHILDREN) return FALSE;
    uint8_t **materials=*(uint8_t ***)(owner+0x20u);
    if(!materials) materials=*(uint8_t ***)(manager+4u);
    if(token>=count || !readable(materials,count*sizeof(*materials))) return FALSE;
    uint8_t *material=materials[token];
    if(!writable(material,0x40u) || *(void **)material!=base+0x2deb7cu ||
        *(void **)(base+0x2deb7cu+4u)!=base+0x202e40u ||
        *(void **)(base+0x2deb7cu+8u)!=base+0x3b870u) return FALSE;
    if(argument==3u && member==0u) {
        /*5E6ED0 returns this scalar. Its flag update and consequent BONA
         * material-flag aggregation are covered by MaterialOwnerExact. */
        output->kind=SUDEKIMP_STORY_CURVE_SCALAR; output->floats=1u;
        return writable(material+0x24u,4u);
    }
    if(argument==0u && member>=2u && member<6u) {
        /* D3D602E40 returns a float4 shader constant. Require its authored
         * slot in BOTH the selected material and retained original; native
         * preparation may replace the override with a clone. Missing slots
         * enter6032C0 (definition replacement), which is not admitted. */
        uint8_t **originals=*(uint8_t ***)(manager+4u);
        if(!readable(originals,count*sizeof(*originals))) return FALSE;
        uint8_t *values[2]={material,originals[token]};
        for(unsigned i=0;i<2u;++i) {
            if(!readable(values[i],0x40u)) return FALSE;
            uint8_t *d=*(uint8_t **)(values[i]+4u);
            if(!readable(d,0x30u)) return FALSE;
            unsigned n=*(unsigned *)(d+0x10u);
            uint32_t *mapping=*(uint32_t **)(d+0x2cu);
            if(!n || n>4096u || !readable(mapping,16u) || mapping[member-2u]>=n ||
                !memory(*(void **)(values[i]+0x38u),n*16u,i==0u)) return FALSE;
        }
        output->kind=SUDEKIMP_STORY_CURVE_COLOR; output->floats=4u; return TRUE;
    }
    if(argument!=2u || member>=6u) return FALSE;
    uint8_t *description=*(uint8_t **)(material+4u);
    if(!readable(description,0x2cu)) return FALSE;
    unsigned textures=*(uint32_t *)(description+0xcu);
    uint32_t *mapping=*(uint32_t **)(description+0x28u);
    if(!readable(mapping,6u*sizeof(*mapping))) return FALSE;
    unsigned texture=mapping[member];
    if(texture==UINT32_MAX) return TRUE; /* authored absent output */
    if(texture>=textures || texture>=20u) return FALSE;
    uint32_t mask=*(uint32_t *)(material+0xcu);
    if(mask&(1u<<(texture+12u))) {
        unsigned order=0,lower=(mask>>12u)&((1u<<texture)-1u);
        while(lower) { order+=lower&1u; lower>>=1; }
        unsigned matrices=*(uint16_t *)(material+0x16u);
        uint8_t *rows=*(uint8_t **)(material+0x3cu);
        if(order>=matrices || !readable(rows,matrices*8u) ||
            *(uint32_t *)(rows+order*8u)!=texture) return FALSE;
        uint8_t *matrix=*(uint8_t **)(rows+order*8u+4u);
        if(!readable(matrix,4u)) return FALSE;
        if(*(void **)matrix==base+0x2df67cu) {
            /* This class currently returns NULL. Still validate a UV curve:
             * native material preparation can replace the override with a
             * clone of its original before the attached target is started. */
            if(*(void **)(base+0x2df67cu+0x18u)!=base+0x3b860u) return FALSE;
        } else if(*(void **)matrix!=base+0x2dee3cu ||
            *(void **)(base+0x2dee3cu+0x18u)!=base+0x206d10u ||
            !writable(matrix+0x10u,64u)) return FALSE;
    }
    /* Missing UV rows are created by the verified graphics-owned 5E74E0
     * path. The material validator admits its counted clone/free graph. */
    output->kind=SUDEKIMP_STORY_CURVE_UV_MATRIX; output->floats=16u;
    return TRUE;
}
static BOOL selector_binding_closed(const Target *t,unsigned index,const uint32_t *binding,
    VisualOutput *output) {
    observe_stage("clip_target",t->identifier,index);
    if(!readable(t->bank,0x74u)) return FALSE;
    unsigned event_count=*(uint32_t *)(t->description+0x1cu);
    unsigned control_count=*(uint32_t *)(t->description+0x18u);
    uint8_t *events=*(uint8_t **)(t->bank+0x70u),*definitions=*(uint8_t **)(t->bank+0x6cu);
    uint8_t *owners=*(uint8_t **)(t->renderer+0xa8u);
    if(!event_count || event_count>MAX_CHILD_ROWS || index>=event_count ||
        !control_count || control_count>MAX_CHILD_ROWS || !readable(events,event_count*12u) ||
        !readable(definitions,control_count*8u) || !readable(owners,control_count*8u)) return FALSE;
    uint32_t *event=(uint32_t *)(events+index*12u);
    unsigned control=event[0];
    if(control>=control_count) return FALSE;
    uint8_t *owner=*(uint8_t **)(owners+control*8u);
    unsigned token=*(uint32_t *)(owners+control*8u+4u);
    if(token==UINT32_MAX) return TRUE; /* native61F410 skips this binding */
    unsigned kind=*(uint32_t *)(definitions+control*8u)&0xffffu;
    if(kind>1u || !readable(owner,0x28u)) return FALSE;
    unsigned children=*(uint32_t *)(t->bank+0x58u),slot=MAX_CHILDREN;
    uint8_t **child_array=*(uint8_t ***)(t->renderer+0x64u);
    if(!children || children>MAX_CHILDREN || !readable(child_array,children*sizeof(*child_array))) return FALSE;
    for(unsigned i=0;i<children;++i) if(child_array[i]==owner) {
        if(slot!=MAX_CHILDREN) return FALSE;
        slot=i;
    }
    if(slot==MAX_CHILDREN) return FALSE;
    if(kind==0u && *(void **)owner==base+0x2dfb14u) {
        uint8_t *morph=*(uint8_t **)(owner+0x14u);
        if(event[1]!=0u || !child_resource_exact(t,slot,owner,morph) ||
            !readable(morph,0x1cu) || *(void **)(owner+0xcu)!=base+0x2dfc30u ||
            *(void **)(base+0x2dfb14u+0xccu)!=base+0x224900u ||
            *(void **)(base+0x2dfc30u+4u)!=base+0x224980u ||
            *(void **)(base+0x2dfc30u+8u)!=base+0x2249b0u ||
            !code_hash(0x224980u,48u,0x9d29beacu) ||
            !code_hash(0x2249b0u,38u,0x5fe9ba5cu)) return FALSE;
        uint32_t *description=*(uint32_t **)(morph+0x18u);
        uint8_t *rows=*(uint8_t **)(owner+0x18u);
        if(!readable(description,4u) || !*description || *description>MAX_CHILD_ROWS ||
            token>=*description || !writable(rows,*description*0xb0u) ||
            !readable(rows-4u,4u) || *(uint32_t *)(rows-4u)!=*description) return FALSE;
        output->kind=SUDEKIMP_STORY_CURVE_SCALAR; output->floats=1u;
        return !binding || binding[0]==(uint32_t)(uintptr_t)(rows+token*0xb0u);
    }
    if(*(void **)owner!=base+0x2dec74u) return FALSE;
    uint8_t *resource=*(uint8_t **)(owner+0x10u);
    if(!child_resource_exact(t,slot,owner,resource)) return FALSE;
    if(kind==1u) return mcon_output(t,owner,resource,token,event[1],event[2],output);
    if(event[1]!=0u || *(void **)(owner+4u)!=base+0x2dedacu ||
        *(void **)(base+0x2dec74u+0xccu)!=base+0x2036b0u ||
        *(void **)(base+0x2dec74u+0xf8u)!=base+0x203750u ||
        *(void **)(base+0x2dedacu+4u)!=base+0x2058a0u ||
        *(void **)(base+0x2dedacu+8u)!=base+0x2058e0u ||
        !code_hash(0x2036b0u,83u,0x5e6df1b0u) || !code_hash(0x203750u,13u,0xa3ba83abu) ||
        !code_hash(0x2058a0u,61u,0xd811648eu) || !code_hash(0x2058e0u,43u,0x8e6ace1eu)) return FALSE;
    /* This ATGT interface starts by returning a counted float buffer and
     * stops by zeroing it. It does not create objects or dispatch tasks. */
    if(!readable(resource,0x88u)) return FALSE;
    uint8_t *header=*(uint8_t **)(resource+0x78u),*descriptors=*(uint8_t **)(resource+0x84u);
    uint8_t **nodes=*(uint8_t ***)(owner+0x1cu);
    if(!readable(header,8u)) return FALSE;
    unsigned count=*(uint32_t *)(header+4u);
    if(!count || count>MAX_CHILD_ROWS || token>=count || !readable(descriptors,count*8u) ||
        !readable(nodes,count*sizeof(*nodes))) return FALSE;
    uint8_t *definition=*(uint8_t **)(descriptors+token*8u),*node=nodes[token];
    if(!readable(definition,0xcu) || !readable(node,0x24u)) return FALSE;
    unsigned n=*(uint32_t *)(node+0x14u),extra=*(uint32_t *)(node+0x18u);
    uint8_t *allocation=*(uint8_t **)(node+8u),*values=*(uint8_t **)(node+0xcu);
    if(!n || n!=*(uint32_t *)(definition+4u) || n>MAX_CHILD_ROWS || extra>MAX_CHILD_ROWS) return FALSE;
    if(!writable(allocation,(2u*n+extra)*4u) || values!=allocation+(n+extra)*4u ||
        !writable(values,n*4u)) return FALSE;
    output->kind=SUDEKIMP_STORY_CURVE_WEIGHTS; output->floats=n;
    return !binding || binding[0]==(uint32_t)(uintptr_t)values;
}
static BOOL rate_child_methods_exact(void) {
    /* Complete supported-image identities; these bodies have no relocations. */
    return *(void **)(base+0x2dfb14u+0xccu)==base+0x224900u &&
        *(void **)(base+0x2dfc14u+0xcu)==base+0x224020u &&
        code_hash(0x224900u,69u,0x987fc63du) && code_hash(0x224020u,49u,0x5fc0b621u);
}
static BOOL rate_change_closed(const Target *t,unsigned channel,unsigned submodel,float wanted) {
    if(channel || submodel || *(float *)(t->rows[0]+4u)==wanted || !(t->renderer[0x6cu]&8u)) return TRUE;
    observe_stage("rate_child_controller",t->identifier,channel);
    unsigned count=*(uint32_t *)(t->bank+0x58u);
    if(!count) return TRUE;
    uint8_t **children=*(uint8_t ***)(t->renderer+0x64u);
    uint16_t *flags=*(uint16_t **)(t->renderer+0x68u);
    if(count>MAX_CHILDREN || !readable(children,count*sizeof(*children)) ||
        !readable(flags,count*sizeof(*flags))) return FALSE;
    for(unsigned i=0;i<count;++i) if(flags[i]&8u) {
        observe_stage("rate_target",t->identifier,i);
        uint8_t *child=children[i];
        /*624900 returns child+8 for BONA. Its exact624020 rate callback has
         * no calls: it writes only rate in this renderer's counted rows. */
        if(!readable(child,0x1cu) || *(void **)child!=base+0x2dfb14u ||
            *(void **)(child+8u)!=base+0x2dfc14u ||
            !rate_child_methods_exact()) return FALSE;
        uint8_t *resource=*(uint8_t **)(child+0x14u),*rows=*(uint8_t **)(child+0x18u);
        if(!child_resource_exact(t,i,child,resource) || !readable(resource,0x1cu)) return FALSE;
        uint32_t *description=*(uint32_t **)(resource+0x18u);
        if(!readable(description,4u) || *description>MAX_CHILD_ROWS) return FALSE;
        unsigned n=*description;
        /*62457B..6245EE allocates4+n*B0, stores n in the array cookie,
         * and publishes allocation+4 as child+18. */
        if(n && (!writable(rows,n*0xb0u) || !readable(rows-4u,4u) ||
            *(uint32_t *)(rows-4u)!=n)) return FALSE;
    }
    return TRUE;
}
static BOOL material_children_closed(const Target *t) {
    unsigned count=*(uint32_t *)(t->bank+0x58u);
    if(!count) return TRUE;
    uint8_t **children=*(uint8_t ***)(t->renderer+0x64u);
    if(count>MAX_CHILDREN || !readable(children,count*sizeof(*children))) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *child=children[i];
        if(!child) continue;
        observe_stage("material_child",t->identifier,i);
        if(!readable(child,0x28u)) return FALSE;
        unsigned resource_offset;
        if(*(void **)child==base+0x2dec74u) {
            observe_stage("material_query",t->identifier,i);
            if(*(void **)(base+0x2dec74u+0xccu)!=base+0x2036b0u ||
                !code_hash(0x2036b0u,83u,0x5e6df1b0u)) return FALSE;
            resource_offset=0x10u;
        } else if(*(void **)child==base+0x2dfb14u) {
            observe_stage("material_query",t->identifier,i);
            if(!rate_child_methods_exact()) return FALSE;
            resource_offset=0x14u;
        } else if(*(void **)child==base+0x2de564u) {
            observe_stage("material_query",t->identifier,i);
            if(*(void **)(base+0x2de564u+0xccu)!=base+0x1fa070u ||
                !code_hash(0x1fa070u,60u,0x47323b67u)) return FALSE;
            resource_offset=0x10u;
        }
        else return FALSE;
        uint8_t *resource=*(uint8_t **)(child+resource_offset);
        observe_stage("material_resource",t->identifier,i);
        if(!child_resource_exact(t,i,child,resource)) return FALSE;
        observe_stage("material_owner",t->identifier,i);
        if(!SudekiMpLanStoryMaterialOwnerExact((HMODULE)base,child,resource)) {
            if(GetLastError()==ERROR_BUSY) observe_stage("material_resource_jobs",t->identifier,i);
            return FALSE;
        }
    }
    return TRUE;
}
static BOOL pose_supported(const Target *t,const SudekiMpLanStoryWorldActor *a,unsigned selectors[5]) {
    observe_stage("bank_compatibility",t->identifier,a->submodels);
    if(a->kind!=t->kind || a->identifier!=t->identifier ||
        a->submodels!=t->submodels || a->bank_fingerprint!=t->fingerprint ||
        !material_children_closed(t)) return FALSE;
    for(unsigned c=0;c<pose_channels(t);++c) {
        observe_stage("clip_mapping",t->identifier,c);
        if(!resolve_clip(t,a->clip[c],a->clip_occurrence[c],&selectors[c])) return FALSE;
        if(!selector_change_closed(t,c,selectors[c])) return FALSE;
        if(!rate_change_closed(t,c,0,a->rate[c])) return FALSE;
        if(!selectors[c] && !scenery(t)) {
            if(a->state[c]!=192u || a->time[c]!=0.0f) {
                observe_stage("empty_clip_state",t->identifier,(c<<16)|a->state[c]);
                return FALSE;
            }
        } else {
            uint8_t *resource=*(uint8_t **)(t->entries+selectors[c]*28u);
            float length=*(float *)(resource+4u);
            if(length<=0.0f || a->time[c]>length+.01f) {
                observe_stage("clip_time_bounds",t->identifier,(c<<16)|selectors[c]);
                return FALSE;
            }
        }
    }
    return TRUE;
}
__attribute__((naked,noinline,used)) static void set_forward(
    void *position __attribute__((unused)),const float *direction __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp),%esi\n\tmovl 12(%esp),%ecx\n\t"
        "call *_world_set_forward\n\tpopl %esi\n\tret\n\t");
}
static BOOL close_float(float actual,float wanted,float tolerance) {
    return isfinite(actual) && isfinite(wanted) && fabsf(actual-wanted)<=tolerance;
}
static BOOL visible(const Target *t,const SudekiMpLanStoryWorldActor *a) {
    const float *matrix=(const float *)(t->object+0x90u);
    float norm=0,dot=0;
    for(unsigned i=0;i<3u;++i) {
        if(!close_float(matrix[12u+i],a->position[i],.01f) || !isfinite(matrix[8u+i])) return FALSE;
        norm+=matrix[8u+i]*matrix[8u+i]; dot+=matrix[8u+i]*a->forward[i];
    }
    return isfinite(norm) && norm>.25f && dot/sqrtf(norm)>.9995f;
}
typedef struct ResidencyWitness {
    const Target *target;
    const Registry *registry;
    const SudekiMpLanStoryNativeRoster *roster;
    SudekiMpLanStoryReplicaExact exact;
    void *context;
} ResidencyWitness;
static BOOL residency_owner_exact(void *context) {
    const ResidencyWitness *w=context;
    return active && GetCurrentThreadId()==native_thread &&
        registry_still(w->registry) &&
        target_still(w->target,w->registry,w->roster,w->exact,w->context);
}
static DWORD prepare_missing_clip(const Target *target,const Registry *registry,
    const SudekiMpLanStoryNativeRoster *roster,SudekiMpLanStoryReplicaExact exact,void *context) {
    if(strcmp(stage,"clip_residency") || stage_identifier!=target->identifier ||
        stage_detail>=target->animations) return ERROR_NOT_SUPPORTED;
    unsigned selector=stage_detail;
    ResidencyWitness witness={target,registry,roster,exact,context};
    if(!residency_owner_exact(&witness)) return ERROR_INVALID_STATE;
    SudekiMpLanStoryResidencyReport report={0};
    SudekiMpLanStoryResidencyState state=SudekiMpLanStoryResidencyInspect(
        target->renderer,target->bank,selector,&report);
    if(state==SUDEKIMP_STORY_RESIDENCY_WAITING) {
        observe_stage("clip_resource_waiting",target->identifier,selector);
        return ERROR_IO_PENDING;
    }
    if(state!=SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD) {
        observe_stage(state==SUDEKIMP_STORY_RESIDENCY_READY?"clip_resource_readiness_mismatch":
            report.reason?report.reason:"clip_resource_unsupported",target->identifier,selector);
        return ERROR_NOT_SUPPORTED;
    }
    state=SudekiMpLanStoryResidencyAcquire(target->renderer,target->bank,selector,
        residency_owner_exact,&witness,&report);
    if(state==SUDEKIMP_STORY_RESIDENCY_LOADED && residency_owner_exact(&witness)) {
        SudekiMpLogFormat("lan_story_world event=clip_resource_prepared identifier=%08lx selector=%u dependency=%lu renderer_refs=%u bank_refs=%u policy=fresh_preflight_required\r\n",
            (unsigned long)target->identifier,selector,(unsigned long)report.dependency,
            report.renderer_references,report.bank_references);
        observe_stage("clip_resource_prepared",target->identifier,selector);
        /* Normal native acquisition transfers the reference to the renderer.
         * Its destructor owns release. Never release it after SetSelector:
         * that setter sees the resident clip and does not acquire a second ref.
         * No pose is applied under the native observations made before the
         * file load. The next presentation attempt must repeat full preflight. */
        return ERROR_IO_PENDING;
    }
    if(state==SUDEKIMP_STORY_RESIDENCY_WAITING) {
        observe_stage("clip_resource_waiting",target->identifier,selector);
        return ERROR_IO_PENDING;
    }
    /* A failed native call is not proof that nothing changed. Do not retry
     * it or authorize later presentation in this world-adapter lifetime. */
    resource_fault=TRUE;
    SudekiMpLogFormat("lan_story_world event=clip_resource_refused identifier=%08lx selector=%u dependency=%lu reason=%s policy=retain_world_until_native_exit\r\n",
        (unsigned long)target->identifier,selector,(unsigned long)report.dependency,
        report.reason?report.reason:"unknown");
    observe_stage("clip_resource_unknown",target->identifier,selector);
    return ERROR_INVALID_STATE;
}
BOOL SudekiMpLanStoryWorldPrepare(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame,SudekiMpLanStoryReplicaExact exact,void *context,
    BOOL independent_view) {
    Registry registry; Target targets[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
    unsigned count=0,selectors[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS][5]={{0}};
    if(!roster || !frame || !exact || !SudekiMpLanStoryWorldFrameValid(frame) || !begin()) return FALSE;
    DWORD error=ERROR_RETRY;
    operation="apply"; observe_stage("client_witness_registry",0,0);
    if(resource_fault) { observe_stage("clip_resource_unknown",0,0); error=ERROR_INVALID_STATE; goto fail; }
    if(recruitment.retained) { error=ERROR_IO_PENDING; goto fail; }
    if(host_seen || !exact(roster,context) || !registry_capture(&registry) ||
        !catalog(&registry,roster,targets,&count,TRUE)) goto fail;
    if(count!=frame->count || (client_seen && (frame->epoch!=client_epoch || count!=client_count))) {
        observe_stage("catalog_identity",0,(count<<16)|frame->count); goto fail;
    }
    for(unsigned i=0;i<count;++i) {
        if(client_seen && (!same_target(&client_bound[i].target,&targets[i]) ||
            client_bound[i].previous.kind!=frame->actors[i].kind ||
            client_bound[i].previous.identifier!=frame->actors[i].identifier ||
            client_bound[i].previous.generation!=frame->actors[i].generation)) {
            observe_stage("retained_owner_changed",targets[i].identifier,i);
            error=ERROR_NOT_SUPPORTED; goto fail;
        }
        if(!pose_supported(&targets[i],&frame->actors[i],selectors[i])) {
            error=prepare_missing_clip(&targets[i],&registry,roster,exact,context);
            goto fail;
        }
        /* Never let the host's compact record erase a different client-side
         * per-submodel pose graph. All native channels, including the fifth
         * ranged action channel, must have uniform validated submodel rows. */
        SudekiMpLanStoryWorldActor current;
        if(!read_pose(&targets[i],&current)) { error=ERROR_NOT_SUPPORTED; goto fail; }
    }
    if(!registry_still(&registry) || !exact(roster,context)) goto fail;
    prepared.registry=registry; memcpy(prepared.targets,targets,count*sizeof(targets[0]));
    memcpy(prepared.selectors,selectors,count*sizeof(selectors[0]));
    prepared.count=count; prepared.roster_address=roster; prepared.frame_address=frame;
    memcpy(&prepared.roster,roster,sizeof(*roster)); memcpy(&prepared.frame,frame,sizeof(*frame));
    prepared.exact=exact; prepared.context=context;
    prepared.independent_view=independent_view;
    /* No curve authorization survives into the mutation phase. The admitted
     * native setters do not replace curve resources; selector's immediate
     * cleanup is checked separately against its current attached owner. */
    memset(curve_proofs,0,curve_proof_count*sizeof(curve_proofs[0])); curve_proof_count=0;
    prepared.valid=TRUE; SetLastError(ERROR_SUCCESS); return TRUE;
fail:
    return finish(FALSE,error);
}
BOOL SudekiMpLanStoryWorldCancelPrepared(void) {
    if(!active && !prepared.valid) return TRUE;
    if(!active || !prepared.valid || GetCurrentThreadId()!=native_thread) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    return finish(TRUE,0);
}
BOOL SudekiMpLanStoryWorldRecruiting(void) { return recruitment.retained; }
BOOL SudekiMpLanStoryWorldBeginRecruitment(const SudekiMpLanStoryNativeRoster *roster,
    void *npc,uint32_t remote_before_epoch,SudekiMpLanStoryReplicaExact exact,void *context) {
    if(!roster || !npc || !exact || !remote_before_epoch || recruitment.retained ||
        !client_seen || host_seen || client_epoch!=remote_before_epoch || resource_fault ||
        roster->available_mask!=4u || roster->leader_character!=2u || !begin()) return FALSE;
    Registry registry; Target targets[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
    unsigned count=0,removed=SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS;
    operation="recruit_retire";
    if(!exact(roster,context) || !registry_capture(&registry) ||
        !catalog(&registry,roster,targets,&count,FALSE) || count!=client_count) return finish(FALSE,ERROR_RETRY);
    for(unsigned i=0;i<count;++i) {
        if(!same_target(&targets[i],&client_bound[i].target)) return finish(FALSE,ERROR_INVALID_STATE);
        if(targets[i].entity==npc) {
            if(targets[i].kind!=SUDEKIMP_LAN_STORY_WORLD_NPC_KIND ||
                targets[i].identifier!=0x882ab028u || removed!=SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS)
                return finish(FALSE,ERROR_NOT_SUPPORTED);
            removed=i;
        }
    }
    if(removed>=count || !registry_still(&registry) || !exact(roster,context))
        return finish(FALSE,ERROR_INVALID_STATE);
    recruitment.retained=TRUE; recruitment.world=roster->world;
    recruitment.descriptor=roster->descriptor; recruitment.removed_npc=npc;
    recruitment.before_epoch=remote_before_epoch;
    /* These are borrowed observations, not renderer reference counts. Retire
     * the dying NPC before the native removal call, without dereferencing it
     * again or releasing engine-owned resources. */
    memmove(client_bound+removed,client_bound+removed+1u,
        (count-removed-1u)*sizeof(client_bound[0]));
    memset(client_bound+count-1u,0,sizeof(client_bound[0])); --client_count;
    return finish(TRUE,ERROR_SUCCESS);
}
BOOL SudekiMpLanStoryWorldFinishRecruitment(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame,uint32_t remote_after_epoch,
    SudekiMpLanStoryReplicaExact exact,void *context) {
    if(!roster || !frame || !exact || !recruitment.retained || !client_seen || host_seen ||
        resource_fault || roster->world!=recruitment.world || roster->descriptor!=recruitment.descriptor ||
        roster->available_mask!=12u || roster->leader_character!=2u ||
        remote_after_epoch<=recruitment.before_epoch || frame->epoch!=remote_after_epoch ||
        !SudekiMpLanStoryWorldFrameValid(frame) || !begin()) return FALSE;
    Registry registry; Target targets[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS]; unsigned count=0;
    Bound next[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS]={0};
    operation="recruit_enroll";
    if(!exact(roster,context) || !registry_capture(&registry) ||
        !catalog(&registry,roster,targets,&count,FALSE) || count!=client_count+1u || count!=frame->count)
        return finish(FALSE,ERROR_RETRY);
    uint64_t survivors=0; unsigned added=0;
    for(unsigned i=0;i<count;++i) {
        const Target *t=&targets[i]; const SudekiMpLanStoryWorldActor *a=&frame->actors[i];
        if(t->entity==recruitment.removed_npc || a->kind!=t->kind || a->identifier!=t->identifier ||
            a->bank_fingerprint!=t->fingerprint || a->submodels!=t->submodels)
            return finish(FALSE,ERROR_INVALID_STATE);
        unsigned previous=0;
        for(;previous<client_count && (client_bound[previous].previous.kind!=t->kind ||
            client_bound[previous].previous.identifier!=t->identifier);++previous) {}
        if(previous<client_count) {
            if((survivors&(UINT64_C(1)<<previous)) ||
                !same_target(t,&client_bound[previous].target)) return finish(FALSE,ERROR_INVALID_STATE);
            survivors|=UINT64_C(1)<<previous;
        } else {
            if(added || t->kind!=SUDEKIMP_LAN_STORY_WORLD_PC_KIND || t->character!=3u ||
                t->entity!=roster->actors[3] || t->identifier!=SudekiMpLanStoryWorldCharacterIdentifier(3u))
                return finish(FALSE,ERROR_INVALID_STATE);
            ++added;
        }
        next[i].target=*t; next[i].previous=*a;
    }
    if(added!=1u || client_count>=64u || survivors!=((UINT64_C(1)<<client_count)-1u) ||
        !registry_still(&registry) || !exact(roster,context)) return finish(FALSE,ERROR_RETRY);
    memcpy(client_bound,next,sizeof(next)); client_count=count;
    client_epoch=remote_after_epoch; memset(&recruitment,0,sizeof(recruitment));
    return finish(TRUE,ERROR_SUCCESS);
}
BOOL SudekiMpLanStoryWorldApplyPrepared(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame,SudekiMpLanStoryReplicaExact exact,void *context) {
    if(!active || !prepared.valid || GetCurrentThreadId()!=native_thread) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    DWORD error=ERROR_RETRY;
    observe_stage("prepared_identity",0,0);
    if(roster!=prepared.roster_address || frame!=prepared.frame_address || exact!=prepared.exact ||
        context!=prepared.context || memcmp(roster,&prepared.roster,sizeof(*roster)) ||
        memcmp(frame,&prepared.frame,sizeof(*frame)) || !exact(roster,context) ||
        !registry_still(&prepared.registry)) goto fail;
    Registry *registry=&prepared.registry; Target *targets=prepared.targets;
    unsigned count=prepared.count; unsigned (*selectors)[5]=prepared.selectors;
    /* Recheck all exact targets after the composed player publication and
     * before the first native pose write. The expensive bank preflight is not repeated. */
    for(unsigned i=0;i<count;++i)
        if(!target_still(&targets[i],registry,roster,exact,context)) goto fail;
    if(!client_seen) {
        for(unsigned i=0;i<count;++i) {
            client_bound[i].target=targets[i]; client_bound[i].previous=frame->actors[i];
        }
        client_epoch=frame->epoch; client_count=count; client_seen=TRUE;
    }
    /* Every target passed before the first native call. The containment owner
     * performs a complete registry check on entry and exit; the per-setter
     * witness below cannot be reused after this synchronous Present callback. */
    for(unsigned i=0;i<count;++i) {
        Target *t=&targets[i]; const SudekiMpLanStoryWorldActor *a=&frame->actors[i];
        if(!target_still(t,registry,roster,exact,context)) goto fail;
        if(scenery(t)) {
            /* Native5D5300/612050 use bit4 as hidden. Preserve every other
             * local renderer flag. The telescope's authored lens is a view
             * overlay: an independent companion camera must not render the
             * host's lens as scenery. Spectators sharing his camera use the
             * host's visibility exactly. Its canonical name was checked. */
            BOOL hidden=(a->visual_flags&SUDEKIMP_LAN_STORY_WORLD_HIDDEN)!=0u ||
                (prepared.independent_view && t->identifier==0x36f00a6cu);
            uint32_t *flags=(uint32_t *)(t->object+0x34u);
            *flags=(*flags&~4u)|(hidden?4u:0u);
            if(((*flags&4u)!=0u)!=hidden) goto fail;
        }
        /* 403050 changes only CPosition coordinates/dirty serial. 5114D0
         * changes its basis through scalar/D3DX math, with no owner, resource,
         * scheduler or task callbacks. Keep one full ownership bracket around
         * these synchronous writes; the visibility bit above is local too. */
        set_position(t->position,a->position);
        set_forward(t->position,a->forward);
        if(!target_still(t,registry,roster,exact,context)) goto fail;
        /* State/time/blend setters are closed native memory/math operations:
         * no allocation, object callbacks or message pumping. One synchronous
         * proof bracket covers them. Selector and rate retain their separate
         * pre/post owner proof because they may touch verified child targets. */
        for(unsigned c=0;c<pose_channels(t);++c) for(unsigned sub=0;sub<t->submodels;++sub) {
            uint8_t *row=t->rows[c]+sub*24u;
            if(*(uint16_t *)row!=selectors[i][c]) {
                if(!target_still(t,registry,roster,exact,context) ||
                    !selector_cleanup_closed(t,c,selectors[i][c])) goto fail;
                set_selector(t->renderer,(int)c,sub,(int)selectors[i][c]);
                if(!target_still(t,registry,roster,exact,context)) goto fail;
            }
            if(*(uint16_t *)(row+2u)!=a->state[c]) {
                set_state(t->renderer,(int)c,sub,a->state[c]);
            }
            if(!close_float(*(float *)(row+8u),a->time[c],.0001f)) {
                set_time(t->renderer,(int)c,sub,a->time[c],0);
            }
            if(!close_float(*(float *)(row+4u),a->rate[c],.0001f)) {
                if(!target_still(t,registry,roster,exact,context) ||
                    !rate_change_closed(t,c,sub,a->rate[c])) goto fail;
                set_rate(t->renderer,(int)c,sub,a->rate[c]);
                if(!target_still(t,registry,roster,exact,context)) goto fail;
            }
            if(*(uint16_t *)row!=selectors[i][c] || *(uint16_t *)(row+2u)!=a->state[c] ||
                !close_float(*(float *)(row+8u),a->time[c],.01f) ||
                !close_float(*(float *)(row+4u),a->rate[c],.01f)) {
                observe_stage("channel_readback",t->identifier,(c<<16)|sub); goto fail;
            }
        }
        for(unsigned blend=0;blend<pose_blends(t);++blend) {
            if(!close_float(*(float *)(t->blends+blend*20u+0xcu),a->blend[blend],.0001f)) {
                set_blend(t->renderer,(int)blend,a->blend[blend]);
            }
            if(!close_float(*(float *)(t->blends+blend*20u+0xcu),a->blend[blend],.0001f)) goto fail;
        }
        if(!target_still(t,registry,roster,exact,context) || !readable(position_matrix(t->position),64u) ||
            !target_still(t,registry,roster,exact,context)) goto fail;
        if(!visible(t,a)) {
            /* Same exact dirty-publication field used by the existing party
             * replica after native SetPosition/SetForward. No spatial/AI tick. */
            t->position[0xb8u]=1;
            if(!target_still(t,registry,roster,exact,context) || !readable(position_matrix(t->position),64u) ||
                !target_still(t,registry,roster,exact,context)) goto fail;
        }
        if(!visible(t,a)) { observe_stage("visible_matrix",t->identifier,0); goto fail; }
        Target after;
        if(!target(t->entity,roster,&after,TRUE) || !same_target(t,&after)) goto fail;
        SudekiMpLanStoryWorldActor readback;
        if(!read_pose(&after,&readback) || memcmp(readback.clip,a->clip,sizeof(a->clip)) ||
            memcmp(readback.clip_occurrence,a->clip_occurrence,sizeof(a->clip_occurrence))) {
            observe_stage("clip_identity_readback",t->identifier,i); goto fail;
        }
    }
    if(!registry_still(registry) || !exact(roster,context)) goto fail;
    for(unsigned i=0;i<count;++i) client_bound[i].previous=frame->actors[i];
    return finish(TRUE,0);
fail:
    return finish(FALSE,error);
}
BOOL SudekiMpLanStoryWorldApply(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame,SudekiMpLanStoryReplicaExact exact,void *context) {
    return SudekiMpLanStoryWorldPrepare(roster,frame,exact,context,FALSE) &&
        SudekiMpLanStoryWorldApplyPrepared(roster,frame,exact,context);
}
BOOL SudekiMpInitializeLanStoryWorld(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t position[]={0xd9,0x41,0x18,0xd9,0x02,0xda,0xe9,0xdf,0xe0,0xf6,0xc4,0x44};
    static const uint8_t facing[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x60,0xd9,0xee,0xd9};
    static const uint8_t matrix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0x56,0x8b,0xf1,0x8b,0x86};
    static const uint8_t update[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0xe4,0,0,0,0x53,0x8b,0x5d,0x08};
    static const uint8_t available[]={0x56,0x8b,0x74,0x24,0x08,0x85,0xf6,0x75,0x06,0xb0,0x01,0x5e};
    if(base || !image || !SudekiMpCheckLoadedExecutable(image) ||
        memcmp(b+0x3050u,position,sizeof(position)) || memcmp(b+0x1114d0u,facing,sizeof(facing)) ||
        memcmp(b+0x111cc0u,matrix,sizeof(matrix)) || memcmp(b+0x110d40u,update,sizeof(update)) ||
        b[0x111cdau]!=0xe8 || b+0x111cdfu+*(int32_t *)(b+0x111cdbu)!=b+0x110d40u) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)
        if(*(void **)(b+0x2df8ecu+identities[i].slot)!=b+identities[i].rva ||
            memcmp(b+identities[i].rva,identities[i].prefix,12u)) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    if(*(void **)(b+0x2df8ecu+0x184u)!=b+0x21bf50u ||
        *(void **)(b+0x2d65e4u+0xcu)!=b+0x3ae30u ||
        memcmp(b+0x21bf50u,available,sizeof(available))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    set_position=(PositionSet)(b+0x3050u); position_matrix=(PositionMatrix)(b+0x111cc0u);
    world_set_forward=b+0x1114d0u; set_selector=(SelectorSet)(b+0x223000u);
    set_rate=(ValueSet)(b+0x2230d0u); set_time=(TimeSet)(b+0x223180u);
    set_state=(StateSet)(b+0x223240u); set_blend=(BlendSet)(b+0x2234c0u);
    if(!SudekiMpLanStoryCurveInitialize(image) || !SudekiMpLanStoryMaterialInitialize(image) ||
        !SudekiMpLanStoryResidencyInitialize(image)) return FALSE;
    base=b; SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpUninstallLanStoryWorld(void) {
    if(active || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    memset(host_bound,0,sizeof(host_bound)); memset(client_bound,0,sizeof(client_bound));
    memset(&recruitment,0,sizeof(recruitment));
    memset(&prepared,0,sizeof(prepared));
    native_thread=0; host_seen=client_seen=resource_fault=FALSE; next_generation=host_epoch=host_sequence=client_epoch=0;
    host_count=client_count=0; base=NULL; set_position=NULL; position_matrix=NULL;
    set_selector=NULL; set_rate=NULL; set_time=NULL; set_state=NULL; set_blend=NULL;
    world_set_forward=NULL; operation=stage="none"; last_stage=NULL;
    stage_identifier=stage_detail=last_identifier=last_detail=0; last_error=0; diagnostic_count=0;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
