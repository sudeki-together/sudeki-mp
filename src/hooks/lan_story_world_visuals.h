#include "hooks/lan_story_inventory.h"
/* Private implementation of host-authored world cosmetics. Included by the
 * existing visual owner: no additional native hooks, AI or client missiles. */
enum { WORLD_VISUAL_MAX=128, WORLD_REGISTRY_MAX=8192 };
typedef struct StoryWorldVisual {
    SudekiMpSpiritVisualWeakNode weak;
    uint32_t instance,tick;
    uint8_t source,kind;
} StoryWorldVisual;
static StoryWorldVisual world_visuals[WORLD_VISUAL_MAX];
static uint64_t world_visual_session;
static void *world_registry[WORLD_REGISTRY_MAX];

/* WorldVisualWeakDestruction: Drop -> CEntity -> TPtr destructor; missile
 * secondary base -> TPtr destructor. The shared observer already verifies
 * the native weak assignment/destructor bodies and exact executable. */
static BOOL world_visual_code(void) {
    uint8_t *b=(uint8_t *)image;
    return b && memory_access(b+0x2d919cu,4,FALSE) &&
        *(void **)(b+0x2d3e68u)==b+0x133150u &&
        *(void **)(b+0x2d919cu)==b+0x187960u &&
        !memcmp(b+0x133150u,"\x56\x8b\xf1\x56\xe8\x17\0\0\0",9) &&
        !memcmp(b+0x133222u,"\x8b\xcb\xe8\xd7\xa8\0\0",7) &&
        !memcmp(b+0x13db47u,"\x5f\x5e\x8b\xcb\x5b\xe9\xdf\x71\xec\xff",10) &&
        !memcmp(b+0x187960u,"\x83\xe9\x04\xe9\x88\xe9\xff\xff",8) &&
        !memcmp(b+0x186453u,"\x8b\xcf\x5f\x5d\xc7\x06",6) &&
        *(void **)(b+0x186459u)==b+0x2c55acu &&
        !memcmp(b+0x18645du,"\x5b\x83\xc4\x04\xe9\xca\xe8\xe7\xff",9);
}
static BOOL world_drop(uint8_t *e,uint8_t **definition,unsigned *item) {
    uint8_t *b=(uint8_t *)image,*c,*stats,*manager,*d;
    float number;
    if(!memory_access(e,0x424u,FALSE) || *(void **)e!=b+0x2d3e68u ||
        !memory_access(c=*(uint8_t **)(e+0x100u),0x50u,FALSE) ||
        c!=e+0x1c0u || *(void **)c!=b+0x2d411cu || *(void **)(c+0x10u)!=e ||
        !memory_access(stats=*(uint8_t **)(e+0x4cu),0x54u,FALSE) || stats!=e+0x320u ||
        *(void **)(stats+0x10u)!=e ||
        !memory_access(manager=*(uint8_t **)(b+0x408d80u),0xfacu,FALSE) ||
        *(void **)manager!=b+0x2c693cu) return FALSE;
    number=*(float *)(stats+0x50u);
    if(!isfinite(number) || number<0 || number>=999u || floorf(number)!=number) return FALSE;
    unsigned id=(unsigned)number;
    d=*(uint8_t **)(manager+0xcu+id*4u);
    if(!memory_access(d,0x50u,FALSE) || d!=*(void **)(c+0x40u) || *(unsigned *)(d+0x14u)!=id) return FALSE;
    *definition=d;*item=id;return TRUE;
}
static void *world_entity(const StoryWorldVisual *v) {
    uint8_t *b=(uint8_t *)image,*p=v->weak.entity,*e;
    if(v->source==1u) return exact_effect(p)?p:NULL;
    if(v->source==2u) {
        if((uintptr_t)p<4u || !memory_access(p-4u,0x60u,FALSE)) return NULL;
        uint8_t *m=p-4u;e=*(void **)(m+0x10u);
        return *(void **)m==b+0x2d915cu && *(void **)p==b+0x2d919cu &&
            memory_access(e,0x100u,FALSE) && *(void **)e==b+0x2d5a10u &&
            *(void **)(e+0xfcu)==m?e:NULL;
    }
    if(v->source==4u) return memory_access(p,0x104u,FALSE) &&
        *(void **)p==b+0x2d3e68u?p:NULL;
    return NULL;
}
static BOOL world_links(const StoryWorldVisual *v) {
    const SudekiMpSpiritVisualWeakNode *p=&v->weak;
    if(!p->entity) return !p->previous && !p->next;
    if(!world_entity(v) || !memory_access(p->entity,8u,TRUE)) return FALSE;
    if(p->previous) {
        if(p->previous==p || !memory_access(p->previous,sizeof(*p),TRUE) ||
            p->previous->entity!=p->entity || p->previous->next!=p) return FALSE;
    } else if(*(void **)((uint8_t *)p->entity+4u)!=p) return FALSE;
    return !p->next || (p->next!=p && memory_access(p->next,sizeof(*p),TRUE) &&
        p->next->entity==p->entity && p->next->previous==p);
}
static BOOL world_bind(StoryWorldVisual *v,void *target) {
    if(!world_links(v) || GetCurrentThreadId()!=game_thread || !world_visual_code()) return FALSE;
    if(target) {
        StoryWorldVisual proof=*v;proof.weak.entity=target;
        if(!world_entity(&proof) || !memory_access(target,8u,TRUE)) return FALSE;
        SudekiMpSpiritVisualWeakNode *head=*(void **)((uint8_t *)target+4u);
        if(head && (!memory_access(head,sizeof(*head),TRUE) || head->entity!=target || head->previous)) return FALSE;
    }
    uintptr_t a=(uintptr_t)&v->weak,d=(uintptr_t)target;
    void *entry=(uint8_t *)image+RVA_WEAK_BIND;
    __asm__ volatile("call *%2" : "+a"(a), "+d"(d) : "r"(entry) : "ecx","memory","cc");
    return v->weak.entity==target && world_links(v);
}
static BOOL world_visual_reset(void) {
    BOOL okay=TRUE;
    for(unsigned i=0;i<WORLD_VISUAL_MAX;++i) {
        StoryWorldVisual *v=&world_visuals[i];
        if((v->weak.entity || v->weak.previous || v->weak.next) && !world_bind(v,NULL)) {okay=FALSE;continue;}
        memset(v,0,sizeof(*v));
    }
    if(okay) world_visual_session=0;
    return okay;
}
static BOOL world_pose(uint8_t *e,SudekiMpLanArenaSpiritVfxSnapshot *v) {
    uint8_t *b=(uint8_t *)image,*p;void *wrapper,*object,*renderer,*model_interface,*render_interface;
    if(!memory_access(p=*(uint8_t **)(e+0x44u),0x104u,TRUE) ||
        *(void **)p!=b+RVA_POSITION_VTABLE || *(void **)(p+0x10u)!=e ||
        !pointer_at(p,0xb4u,&wrapper) || !pointer_at(wrapper,8u,&object) ||
        !pointer_at(wrapper,0xcu,&model_interface) || !pointer_at(wrapper,0x10u,&renderer) ||
        !memory_access(object,0xd0u,TRUE) || !pointer_at(object,0x14u,&render_interface) ||
        !memory_access(renderer,0x9cu,FALSE) || *(void **)renderer!=b+RVA_RENDERER_VTABLE ||
        renderer!=model_interface || renderer!=render_interface || *(void **)(p+0x94u) ||
        !memory_access(*(void **)(p+0x8cu),0x110u,TRUE)) return FALSE;
    typedef const float *(__attribute__((thiscall)) *Matrix)(void *);
    const float *matrix=((Matrix)(b+RVA_WORLD_MATRIX))(p);
    return memory_access(matrix,64u,FALSE) && SudekiMpSpiritVisualDecomposeMatrix(matrix,v) && native_phase(renderer,v);
}
BOOL SudekiMpLanStoryWorldVisuals(uint64_t session,SudekiMpStoryVfx *out) {
    uint8_t *b=(uint8_t *)image,*manager;void **entries;
    if(!out || !session || session!=registry.session || !out->observed ||
        out->count>SUDEKIMP_STORY_VFX_CAPACITY || GetCurrentThreadId()!=game_thread ||
        !world_visual_code() || !memory_access(b+0x409d8cu,4u,FALSE) ||
        !memory_access(manager=*(uint8_t **)(b+0x409d8cu),0x40u,FALSE)) return FALSE;
    if(world_visual_session && world_visual_session!=session && !world_visual_reset()) return FALSE;
    world_visual_session=session;
    unsigned count=*(unsigned *)(manager+0x34u);
    entries=*(void ***)(manager+0x3cu);
    if(count>WORLD_REGISTRY_MAX || (count && !memory_access(entries,count*4u,FALSE))) return FALSE;
    memcpy(world_registry,entries,count*4u);
    for(unsigned i=0;i<WORLD_VISUAL_MAX;++i) {
        if(!world_links(&world_visuals[i])) return FALSE;
        if(!world_visuals[i].weak.entity) memset(&world_visuals[i],0,sizeof(world_visuals[i]));
    }
    SudekiMpStoryVfx next=*out;
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=world_registry[i],*resource=NULL,*target=e;unsigned source=0,item=0;
        if(!memory_access(e,0x48u,FALSE)) return FALSE;
        if(*(void **)e==b+RVA_EFFECT_VTABLE) {
            if(!exact_effect(e)) return FALSE;
            if(e[0x3e0u]&8u) continue;
            source=1u;resource=e+0x29cu;
        } else if(*(void **)e==b+0x2d5a10u) {
            if(!memory_access(e,0x100u,FALSE)) return FALSE;
            uint8_t *m=*(void **)(e+0xfcu),*owner,*record;
            if(!memory_access(m,0x60u,FALSE) || *(void **)m!=b+0x2d915cu || *(void **)(m+0x10u)!=e) return FALSE;
            owner=*(void **)(m+0x58u);record=*(void **)(m+0x3cu);
            if(!memory_access(owner,0x38u,FALSE) || *(void **)owner!=b+0x2d5fa0u) continue;
            if(!memory_access(record,0x30u,FALSE)) continue;
            source=2u;resource=record+0x24u;target=m+4u;
        } else if(*(void **)e==b+0x2d3e68u) {
            uint8_t *definition;
            if(!world_drop(e,&definition,&item)) continue; /* Native setup still pending. */
            uint8_t *c=*(void **)(e+0x100u);
            if(c[0x4cu]) continue;
            source=4u;resource=definition+0x44u;
        } else continue;
        if(!memory_access(resource,12u,FALSE) || (*(uint32_t *)resource&0x7fu)!=0x29u) continue;
        uint8_t kind=native_resource_kind(resource);
        if(!(SudekiMpLanStoryWorldVfxSourceMask(kind)&source)) continue;
        StoryWorldVisual *found=NULL,*empty=NULL;
        for(unsigned j=0;j<WORLD_VISUAL_MAX;++j) {
            StoryWorldVisual *v=&world_visuals[j];
            if(v->weak.entity==target) found=v;
            if(!v->weak.entity && !empty) empty=v;
        }
        if(found && (found->source!=source || found->kind!=kind)) return FALSE;
        SudekiMpLanArenaSpiritVfxSnapshot value={.kind=kind,.owner_actor_type=255u};
        BOOL sampled=source==1u || world_pose(e,&value);
        /* SpecialEffect sampling requires a real intrusive witness. Bind only
         * after native setup has supplied a render wrapper, then use it. */
        if(source==1u) {
            uint8_t *p=*(void **)(e+0x44u);
            if(!memory_access(p,0xb8u,FALSE) || !*(void **)(p+0xb4u)) continue;
            sampled=TRUE;
        }
        if(!sampled) {if(found) return FALSE;continue;}
        if(!found) {
            if(!empty || registry.next_instance==UINT32_MAX) return FALSE;
            found=empty;found->source=(uint8_t)source;found->kind=kind;
            if(!world_bind(found,target)) return FALSE;
            found->instance=++registry.next_instance;found->tick=out->tick;
            SudekiMpLogFormat("story_world_visual event=observed source=%u kind=%u instance=%lu item=%u\r\n",source,kind,(unsigned long)found->instance,item);
        }
        if(source==1u && !native_sample(image,&found->weak,kind,&value)) return FALSE;
        if(next.count>=SUDEKIMP_STORY_VFX_CAPACITY) return FALSE;
        value.instance_sequence=found->instance;value.emitted_host_tick=found->tick;
        next.entries[next.count++]=value;
    }
    if(*(void **)(b+0x409d8cu)!=manager || *(unsigned *)(manager+0x34u)!=count ||
        *(void ***)(manager+0x3cu)!=entries || memcmp(entries,world_registry,count*4u)) return FALSE;
    for(unsigned i=1;i<next.count;++i) {
        unsigned j=i;SudekiMpLanArenaSpiritVfxSnapshot v=next.entries[i];
        while(j && next.entries[j-1].instance_sequence>v.instance_sequence) {next.entries[j]=next.entries[j-1];--j;}
        next.entries[j]=v;
    }
    *out=next;return TRUE;
}

static BOOL world_pickup_code(void) {
    uint8_t *b=(uint8_t *)image;
    /* WorldDropCollect: stdcall CDroppedItem*, RET4 on both success and cap
     * refusal. The existing loot trace owns the vanilla collector callsite. */
    return world_visual_code() &&
        !memcmp(b+0x1338a0u,"\x53\x8b\x5c\x24\x08\x8b\x43\x10",8) &&
        *(void **)(b+0x1338c2u)==b+0x408d84u &&
        !memcmp(b+0x1338c6u,"\x6a\x01\x56\x8b\xcf\xe8\0\xe1\xee\xff",10) &&
        !memcmp(b+0x1338d4u,"\x6a\x01\x6a\x01\x56\x8b\xcf",7) &&
        SudekiMpLanStoryInventoryPickupCallExact(image) &&
        !memcmp(b+0x1338e0u,"\x53\xe8\x3a\0\0\0",6) &&
        !memcmp(b+0x1338ebu,"\xc2\x04\0",3) && !memcmp(b+0x133913u,"\xc2\x04\0",3);
}
BOOL SudekiMpLanStoryWorldCollectNearby(void *actor,uint8_t type) {
    uint8_t *b=(uint8_t *)image,*a=actor,*position,*stats;
    unsigned vt=type==5u?0x2d5a88u:type==14u?0x2d66fcu:type==35u?0x2d5010u:type==1u?0x2d555cu:0u;
    if(!vt || !world_visual_session || world_visual_session!=registry.session ||
        GetCurrentThreadId()!=game_thread || !world_pickup_code() ||
        !memory_access(a,0x50u,FALSE) || *(void **)a!=b+vt || a[0x2bu] ||
        !memory_access(position=*(void **)(a+0x44u),0x24u,FALSE) ||
        *(void **)position!=b+RVA_POSITION_VTABLE || *(void **)(position+0x10u)!=a ||
        !memory_access(stats=*(void **)(a+0x4cu),0x30u,FALSE) ||
        *(void **)(stats+0x10u)!=a || !isfinite(*(float *)(stats+0x2cu)) ||
        *(float *)(stats+0x2cu)<=0.0f) return FALSE;
    for(unsigned i=0;i<WORLD_VISUAL_MAX;++i) {
        StoryWorldVisual *v=&world_visuals[i];uint8_t *e=v->weak.entity,*definition;unsigned item;
        if(v->source!=4u || !e) continue;
        if(!world_links(v)) return FALSE;
        if(!world_drop(e,&definition,&item)) continue;
        uint8_t *c=*(void **)(e+0x100u),*p=*(void **)(e+0x44u);
        if(c[0x4cu] || !isfinite(*(float *)(c+0x48u)) || *(float *)(c+0x48u)>0.0f ||
            !memory_access(p,0x24u,FALSE) || *(void **)p!=b+RVA_POSITION_VTABLE || *(void **)(p+0x10u)!=e) continue;
        float distance=0;
        for(unsigned j=0;j<3;++j) {
            float delta=*(float *)(position+0x18u+j*4u)-*(float *)(p+0x18u+j*4u);
            distance+=delta*delta;
        }
        if(!isfinite(distance) || distance>1.0f) continue;
        /* Prove registry membership immediately before a native mutation;
         * an observer may remain linked after queued native removal. */
        uint8_t *manager=*(void **)(b+0x409d8cu);void **entries;
        if(!memory_access(manager,0x40u,FALSE)) return FALSE;
        unsigned n=*(unsigned *)(manager+0x34u);entries=*(void ***)(manager+0x3cu);
        if(n>WORLD_REGISTRY_MAX || !memory_access(entries,n*4u,FALSE)) return FALSE;
        BOOL present=FALSE;for(unsigned j=0;j<n;++j) if(entries[j]==e) present=TRUE;
        if(!present) continue;
        if(!memory_access(*(void **)(b+0x408d84u),0x130u,TRUE)) return FALSE;
        unsigned result=((unsigned (__attribute__((stdcall)) *)(void *))(b+0x1338a0u))(c);
        BOOL consumed=!v->weak.entity || (world_links(v) && c[0x4cu]!=0u);
        SudekiMpLogFormat("story_world_loot event=pickup character=%u item=%u instance=%lu native_result=%u consumed=%u\r\n",
            type,item,(unsigned long)v->instance,result&255u,consumed);
        return consumed; /* Re-observe all ownership before another pickup. */
    }
    return FALSE;
}
