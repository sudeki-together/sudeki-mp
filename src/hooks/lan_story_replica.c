#include "hooks/lan_story_replica.h"
#include "hooks/lan_arena_owner_view.h"
#include "hooks/lan_story_residency.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "engine/log.h"
#include "network/lan_party_motion.h"
#include <math.h>
#include <string.h>

/* Reuse the established world-motion setter ABI. This module installs no
 * hook and never enters the legacy two-character global identity adapter. */
typedef void (__attribute__((fastcall)) *PositionSet)(void *,const float *);
typedef const float *(__attribute__((thiscall)) *PositionMatrix)(void *);
typedef unsigned (__attribute__((thiscall)) *AnimationCount)(void *);
typedef int (__attribute__((thiscall)) *AnimationLookup)(void *,int);
typedef int (__attribute__((thiscall)) *SelectorGet)(void *,int,unsigned);
typedef void (__attribute__((thiscall)) *SelectorSet)(void *,int,unsigned,int);
typedef float (__attribute__((thiscall)) *ValueGet)(void *,int,unsigned);
typedef void (__attribute__((thiscall)) *ValueSet)(void *,int,unsigned,float);
typedef void (__attribute__((thiscall)) *TimeSet)(void *,int,unsigned,float,int);
typedef unsigned char (__attribute__((thiscall)) *StateGet)(void *,int,unsigned);
typedef void (__attribute__((thiscall)) *StateSet)(void *,int,unsigned,int);
typedef float (__attribute__((thiscall)) *BlendGet)(void *,int);
typedef void (__attribute__((thiscall)) *BlendSet)(void *,int,float);
typedef struct Methods {
    AnimationCount count; AnimationLookup lookup;
    SelectorGet get_selector; SelectorSet set_selector;
    ValueGet get_rate,get_time; ValueSet set_rate; TimeSet set_time;
    StateGet get_state; StateSet set_state;
    BlendGet get_blend; BlendSet set_blend;
} Methods;
typedef struct Target {
    uint8_t *actor,*position,*model,*wrapper,*object,*renderer;
    uint8_t *bank,*description,*entries,*table;
    unsigned submodels,animations,character;
    Methods methods;
} Target;
static uint8_t *base;
static PositionSet set_position;
static PositionMatrix position_matrix;
static void *story_set_forward __attribute__((used));
static DWORD native_thread;
static BOOL applying;
static struct EquipmentAttempt {
    void *actor,*weapon,*item;
    uint32_t epoch,generation;
} equipment_attempts[4];
static SudekiMpLanArenaOwnerViewLease view_owner;
static SudekiMpLanStoryView saved_view,last_view;
static float saved_anchor[3];
static BOOL saved_anchor_valid;
static const uint8_t types[4]={SUDEKIMP_LAN_ARENA_BUKI_TYPE,
    SUDEKIMP_LAN_ARENA_ELCO_TYPE,SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_AILISH_TYPE};
static const SudekiMpCleanroomActor kinds[4]={SUDEKIMP_CLEANROOM_BUKI,
    SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};

static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a+n<a || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return !write || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL readable(const void *p,size_t n) { return memory(p,n,FALSE); }
static BOOL writable(const void *p,size_t n) { return memory(p,n,TRUE); }
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
    /* SelectorSet's native resource helper dispatches these exact slots. */
    {0x178,0x21bf00,{0x8b,0x44,0x24,0x04,0x8b,0x51,0x08,0x56,0x8d,0x34,0xc5,0x00}},
    {0x18c,0x223b70,{0x53,0x8b,0x5c,0x24,0x08,0x56,0x57,0x8b,0xf9,0x8b,0x47,0x08}}
};
static BOOL renderer_methods(void *renderer,Methods *m) {
    if(!base || !m || !readable(renderer,0xacu) ||
        *(void **)renderer!=base+0x2df8ecu) return FALSE;
    uint8_t *v=*(uint8_t **)renderer;
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)
        if(*(void **)(v+identities[i].slot)!=base+identities[i].rva) return FALSE;
    m->lookup=(AnimationLookup)(base+0x21bac0); m->count=(AnimationCount)(base+0x21bb10);
    m->set_selector=(SelectorSet)(base+0x223000); m->get_selector=(SelectorGet)(base+0x2230b0);
    m->set_rate=(ValueSet)(base+0x2230d0); m->get_rate=(ValueGet)(base+0x223160);
    m->set_time=(TimeSet)(base+0x223180); m->get_time=(ValueGet)(base+0x223220);
    m->set_state=(StateSet)(base+0x223240); m->get_state=(StateGet)(base+0x223290);
    m->set_blend=(BlendSet)(base+0x2234c0); m->get_blend=(BlendGet)(base+0x2234e0);
    return TRUE;
}
static BOOL target(void *actor,unsigned character,Target *out) {
    Target t={.actor=actor,.character=character};
    if(character>=4u || !readable(actor,0x138u)) return FALSE;
    t.position=*(uint8_t **)(t.actor+0x44u); t.model=*(uint8_t **)(t.actor+0x130u);
    if(!writable(t.position,0x104u) || *(void **)t.position!=base+0x2cdefcu ||
        !readable(t.model,0x168u) || *(void **)(t.position+0x10u)!=actor || *(void **)(t.model+0x10u)!=actor) return FALSE;
    uintptr_t parent=*(uintptr_t *)(t.position+0x94u);
    if(parent && parent!=4u) return FALSE;
    t.wrapper=*(uint8_t **)(t.position+0xb4u);
    if(!readable(t.wrapper,0x14u)) return FALSE;
    t.renderer=*(uint8_t **)(t.wrapper+0x10u); t.object=*(uint8_t **)(t.wrapper+8u);
    /* Noncombat requires the attached WORLD model. Never overwrite arms or a
     * detached model selected by an unproven first-person transition. */
    if(character==1u || character==3u) {
        void *world=*(void **)(t.model+0x164u),*arms=*(void **)(t.model+0x160u);
        if(t.wrapper==arms || (world && world!=t.wrapper)) return FALSE;
    }
    if(!writable(t.object,0xd0u) || !writable(t.renderer,0xacu) ||
        !renderer_methods(t.renderer,&t.methods)) return FALSE;
    t.bank=*(uint8_t **)(t.renderer+8u);
    if(!readable(t.bank,0x5cu)) return FALSE;
    t.description=*(uint8_t **)(t.bank+0x1cu); t.entries=*(uint8_t **)(t.bank+0x20u);
    if(!readable(t.description,0x10u)) return FALSE;
    t.animations=*(unsigned *)t.description; t.submodels=*(unsigned *)(t.description+0xcu);
    if(!t.animations || t.animations>4096u || !t.submodels || t.submodels>32u ||
        !readable(t.entries,t.animations*28u) || t.methods.count(t.renderer)!=t.submodels ||
        !SudekiMpCleanroomWorldMotionStorageValid(kinds[character],t.renderer,t.submodels,TRUE)) return FALSE;
    /* Native lookup dereferences every loaded resource's handle and TimeSet
     * reads its length. Check the whole bounded bank before either native call. */
    for(unsigned i=0;i<t.animations;++i)
        if(!readable(*(void **)(t.entries+i*28u),0x20u)) return FALSE;
    t.table=*(uint8_t **)(t.model+0xdcu);
    if(!readable(t.table,0x14u)) return FALSE;
    *out=t; return TRUE;
}
static BOOL actor_exact(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryReplicaExact exact,void *context,const Target *t) {
    if(!exact || !exact(roster,context) || roster->actors[t->character]!=t->actor ||
        !readable(t->actor,0x134u) || !writable(t->position,0x104u) ||
        !readable(t->model,0x168u) || !readable(t->wrapper,0x14u) ||
        !writable(t->object,0xd0u) || !writable(t->renderer,0xacu) ||
        !readable(t->bank,0x5cu) || !readable(t->description,0x10u)) return FALSE;
    uintptr_t parent=*(uintptr_t *)(t->position+0x94u);
    if((parent && parent!=4u) || *(void **)t->position!=base+0x2cdefcu ||
        *(void **)(t->actor+0x44u)!=t->position ||
        *(void **)(t->actor+0x130u)!=t->model || *(void **)(t->position+0x10u)!=t->actor ||
        *(void **)(t->model+0x10u)!=t->actor || *(void **)(t->position+0xb4u)!=t->wrapper ||
        *(void **)(t->wrapper+8u)!=t->object || *(void **)(t->wrapper+0x10u)!=t->renderer ||
        *(void **)t->renderer!=base+0x2df8ecu || *(void **)(t->renderer+8u)!=t->bank ||
        *(void **)(t->bank+0x1cu)!=t->description || *(void **)(t->bank+0x20u)!=t->entries ||
        *(void **)(t->model+0xdcu)!=t->table || *(unsigned *)t->description!=t->animations ||
        *(unsigned *)(t->description+0xcu)!=t->submodels) return FALSE;
    if(t->character==1u || t->character==3u) {
        void *world=*(void **)(t->model+0x164u),*arms=*(void **)(t->model+0x160u);
        if(t->wrapper==arms || (world && world!=t->wrapper)) return FALSE;
    }
    return SudekiMpCleanroomWorldMotionStorageValid(kinds[t->character],
        t->renderer,t->submodels,TRUE);
}
static BOOL selector_loaded(const Target *t,int selector) {
    if(selector<0 || (unsigned)selector>=t->animations) return FALSE;
    if(!selector) return TRUE;
    /* Resolve the same semantic table/loaded-renderer mapping as SMP4. The
     * selector number alone is never treated as proof of an animation asset. */
    for(unsigned id=0;id<0xc4u;++id) {
        if(!readable(t->table,0x14u+(id+1u)*4u)) return FALSE;
        uint8_t *details=*(uint8_t **)(t->table+0x14u+id*4u);
        if(!readable(details,0x28u)) continue;
        for(unsigned bank=0;bank<2u;++bank) {
            uint32_t handle=*(uint32_t *)(details+(bank?0x20u:0x14u));
            if(handle && handle!=0x0007ffffu &&
                t->methods.lookup(t->renderer,(int)handle)==selector) return TRUE;
        }
    }
    return FALSE;
}
static BOOL current_motion_known(const Target *t) {
    /* A paused task may still own an unsupported pose. Do not mistake paused
     * for idle or erase dialogue, attacks, flight, or a fifth ranged action. */
    for(unsigned sub=0;sub<t->submodels;++sub) {
        int selectors[4]; uint8_t states[4]; float rates[4],times[4],blends[3];
        SudekiMpLanArenaLocomotion motion;
        for(unsigned channel=0;channel<4u;++channel) {
            selectors[channel]=t->methods.get_selector(t->renderer,channel,sub);
            states[channel]=t->methods.get_state(t->renderer,channel,sub);
            rates[channel]=t->methods.get_rate(t->renderer,channel,sub);
            times[channel]=t->methods.get_time(t->renderer,channel,sub);
        }
        for(unsigned blend=0;blend<3u;++blend)
            blends[blend]=t->methods.get_blend(t->renderer,blend);
        if(!SudekiMpLanPartyMotionCapture(types[t->character],selectors,states,
            rates,times,blends,NULL,&motion)) return FALSE;
        for(unsigned channel=0;channel<4u;++channel) if(motion.clip[channel]>5u) return FALSE;
        if((t->character==1u || t->character==3u) &&
            (t->methods.get_selector(t->renderer,4,sub)!=0 ||
                t->methods.get_state(t->renderer,4,sub)!=192u)) return FALSE;
    }
    return TRUE;
}
__attribute__((naked,noinline,used)) static void forward(
    void *p __attribute__((unused)),const float *v __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp),%esi\n\tmovl 12(%esp),%ecx\n\t"
        "call *_story_set_forward\n\tpopl %esi\n\tret\n\t");
}
static BOOL visible(const Target *t,const SudekiMpLanStoryActor *a) {
    const float *m=(const float *)(t->object+0x90u);
    float length=sqrtf(m[8]*m[8]+m[10]*m[10]);
    float wanted=sqrtf(a->facing_x*a->facing_x+a->facing_z*a->facing_z);
    if(!isfinite(length) || length<0.5f || !isfinite(wanted) || wanted<0.5f) return FALSE;
    return isfinite(m[12]) && isfinite(m[13]) && isfinite(m[14]) &&
        fabsf(m[12]-a->x)<=.01f && fabsf(m[13]-a->y)<=.01f && fabsf(m[14]-a->z)<=.01f &&
        (m[8]*a->facing_x+m[10]*a->facing_z)/(length*wanted)>=.9995f;
}
static BOOL close_float(float actual,float expected,float tolerance) {
    return isfinite(actual) && isfinite(expected) && fabsf(actual-expected)<=tolerance;
}
static BOOL camera_exact(SudekiMpLanArenaOwnerViewLease *owner) {
    void *mode=*(void **)(base+0x408da8u),*scene=*(void **)(base+0x408d58u);
    if(!owner->valid || !SudekiMpLanArenaOwnerViewService(owner,mode,scene,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER)) return FALSE;
    return readable(owner->camera,0x38u) && *(void **)owner->camera==base+0x2cce5cu &&
        writable(owner->render_state,0xdcu) && *(void **)owner->render_state==base+0x2dd638u;
}
static BOOL apply_view(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryView *view,SudekiMpLanStoryReplicaExact exact,void *context) {
    if(!SudekiMpLanStoryViewValid(view)) return FALSE;
    if(!view->valid) return TRUE;
    if(!exact(roster,context)) return FALSE;
    if(!view_owner.valid) {
        SudekiMpLanArenaOwnerViewLease owner={0}; SudekiMpLanStoryView prior={.valid=1};
        if(!SudekiMpLanArenaOwnerViewCapture(&owner,*(void **)(base+0x408da8u),
            *(void **)(base+0x408d58u)) || !camera_exact(&owner)) return FALSE;
        memcpy(prior.matrix,(uint8_t *)owner.render_state+0x90u,sizeof(prior.matrix));
        memcpy(prior.projection,(uint8_t *)owner.render_state+0xd0u,sizeof(prior.projection));
        /* This is a local restoration copy, with no host camera serial or
         * network authority. Only its native geometry is being validated. */
        if(!SudekiMpLanStoryViewGeometryValid(&prior)) return FALSE;
        if(roster->leader_character>=4u) return FALSE;
        uint8_t *actor=roster->actors[roster->leader_character];
        if(!readable(actor,0x48u)) return FALSE;
        uint8_t *position=*(uint8_t **)(actor+0x44u);
        if(!readable(position,0x24u) || *(void **)position!=base+0x2cdefcu ||
            *(void **)(position+0x10u)!=actor) return FALSE;
        for(unsigned i=0;i<3u;++i) {
            saved_anchor[i]=*(float *)(position+0x18u+4u*i);
            if(!isfinite(saved_anchor[i]) || fabsf(saved_anchor[i])>=1000000.0f) return FALSE;
        }
        saved_anchor_valid=TRUE;
        view_owner=owner; saved_view=prior; last_view=prior;
    }
    if(!camera_exact(&view_owner) || !exact(roster,context)) return FALSE;
    uint8_t *render=view_owner.render_state;
    /* Host cinematic matrices are published into this exact paused local
     * view, without selecting a named camera or running a dialogue script.
     * Match native and the established Spirit publication contract: camera
     * consumers also track this 16-bit revision. Matrix copies alone leave
     * derived render state stale. These writes contain no native callbacks. */
    memcpy(render+0x90u,view->matrix,sizeof(view->matrix));
    memcpy(render+0xd0u,view->projection,sizeof(view->projection));
    ++*(uint16_t *)(render+0x2cu);
    last_view=*view;
    return camera_exact(&view_owner) && exact(roster,context);
}
BOOL SudekiMpInitializeLanStoryReplica(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t position[]={0xd9,0x41,0x18,0xd9,0x02,0xda,0xe9,0xdf,0xe0,0xf6,0xc4,0x44};
    static const uint8_t facing[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x60,0xd9,0xee,0xd9};
    static const uint8_t matrix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0x56,0x8b,0xf1,0x8b,0x86};
    static const uint8_t update[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0xe4,0,0,0,0x53,0x8b,0x5d,0x08};
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
    base=b; set_position=(PositionSet)(b+0x3050u);
    position_matrix=(PositionMatrix)(b+0x111cc0u); story_set_forward=b+0x1114d0u;
    memset(equipment_attempts,0,sizeof(equipment_attempts));
    native_thread=0; return TRUE;
}
static BOOL equipment_idle(void *actor,void **weapon_out) {
    SudekiMpCharacterSkillState skill;
    BOOL pending=TRUE;
    if(!readable(actor,0xdcu) || !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !readable(skill.skill,0x78u) || *(void **)((uint8_t *)actor+0xd8u)!=skill.skill ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor || ((uint8_t *)skill.skill)[0x6cu]) return FALSE;
    void *task=*(void **)((uint8_t *)skill.skill+0x74u);
    if(task && (!readable(task,8u) || *(void **)task || !*((uint32_t *)task+1))) return FALSE;
    uint8_t *weapon=*(uint8_t **)((uint8_t *)actor+0xc0u);
    if(!readable(weapon,0x3b9u) || *(void **)(weapon+0x10u)!=actor ||
        *(void **)(weapon+0x26cu) || *(uint32_t *)(weapon+0x330u)!=3u ||
        (weapon[0x3b8u]&4u)) return FALSE;
    *weapon_out=weapon; return TRUE;
}
typedef struct TalWeaponPose {
    uint8_t *actor,*weapon,*item,*wrapper,*object,*renderer,*bank,*description,*entries,*channels,*rows;
    unsigned submodels,animations;
    Methods methods;
} TalWeaponPose;
static BOOL tal_weapon_pose_target(void *actor,TalWeaponPose *out) {
    TalWeaponPose t={.actor=actor};
    if(!readable(actor,0xc4u) || *(void **)actor!=base+0x2d5010u) return FALSE;
    if(*(void **)(base+0xd9218u)!=base+0xd91f6u ||
        *(void **)(base+0xd9224u)!=base+0xd91deu ||
        base[0xd91deu]!=0xb8u || *(uint32_t *)(base+0xd91dfu)!=3u ||
        base[0xd91e3u]!=0xc3u) return FALSE;
    t.weapon=*(uint8_t **)(t.actor+0xc0u);
    if(!readable(t.weapon,0x3b9u) || *(void **)(t.weapon+0x10u)!=actor ||
        *(void **)(t.weapon+0x26cu) || *(unsigned *)(t.weapon+0x330u)!=3u ||
        (t.weapon[0x3b8u]&4u)) return FALSE;
    t.item=*(uint8_t **)(t.weapon+0x268u);
    if(!readable(t.item,0x18u) || *(unsigned *)(t.item+0x14u)>=12u) return FALSE;
    t.wrapper=*(uint8_t **)(t.weapon+0xf4u);
    if(!readable(t.wrapper,0x14u)) return FALSE;
    t.object=*(uint8_t **)(t.wrapper+8u); t.renderer=*(uint8_t **)(t.wrapper+0xcu);
    if(!readable(t.object,0x38u) || *(void **)t.object!=base+0x2dd700u ||
        *(void **)(t.wrapper+0x10u)!=t.renderer || *(void **)(t.object+0x14u)!=t.renderer ||
        !writable(t.renderer,0xb0u) || !renderer_methods(t.renderer,&t.methods) ||
        *(unsigned *)(t.renderer+0xa0u)!=1u || *(unsigned *)(t.renderer+0xa4u)) return FALSE;
    t.bank=*(uint8_t **)(t.renderer+8u);
    if(!readable(t.bank,0x5cu)) return FALSE;
    t.description=*(uint8_t **)(t.bank+0x1cu); t.entries=*(uint8_t **)(t.bank+0x20u);
    if(!readable(t.description,0x10u)) return FALSE;
    t.animations=*(unsigned *)t.description; t.submodels=*(unsigned *)(t.description+0xcu);
    if(t.animations<4u || t.animations>4096u || !t.submodels || t.submodels>32u ||
        !readable(t.entries,t.animations*28u)) return FALSE;
    t.channels=*(uint8_t **)(t.renderer+0x98u);
    if(!writable(t.channels,36u)) return FALSE;
    t.rows=*(uint8_t **)t.channels;
    if(!writable(t.rows,t.submodels*24u)) return FALSE;
    for(unsigned c=0;c<=3u;c+=3u) {
        uint8_t *resource=*(uint8_t **)(t.entries+c*28u);
        if(!readable(resource,0x20u) || !*(uint32_t *)resource ||
            *(uint32_t *)resource==0x7ffffu || !isfinite(*(float *)(resource+4u)) ||
            *(float *)(resource+4u)<=0) return FALSE;
    }
    for(unsigned s=0;s<t.submodels;++s) {
        uint8_t *row=t.rows+s*24u;
        unsigned selector=*(uint16_t *)row,state=*(uint16_t *)(row+2u);
        if((selector!=0u && selector!=3u) || (state!=0u && state!=128u) ||
            *(float *)(row+4u)!=24.0f || !isfinite(*(float *)(row+8u)) ||
            !isfinite(*(float *)(row+0xcu))) return FALSE;
    }
    *out=t; return TRUE;
}
static BOOL tal_weapon_pose_same(const TalWeaponPose *a,const TalWeaponPose *b) {
    return a->actor==b->actor && a->weapon==b->weapon && a->item==b->item &&
        a->wrapper==b->wrapper && a->object==b->object && a->renderer==b->renderer &&
        a->bank==b->bank && a->description==b->description && a->entries==b->entries &&
        a->channels==b->channels && a->rows==b->rows &&
        a->submodels==b->submodels && a->animations==b->animations;
}
static BOOL tal_weapon_pose(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryActor *state,SudekiMpLanStoryReplicaExact exact,void *context) {
    /* The attachment stream already distinguishes authored hand/sheath.
     * Tal's weapon has its OWN animation: retail D9170/D92D0 resolve settled
     * drawn=0, sheathed=3. Moving the wrapper alone leaves the sheath mesh on
     * the blade. Do not arm CArbiter, alter CWeapon's native state, start a
     * draw task, or replay transition events inside the paused replica. */
    if(!state || state->character!=2u || state->weapon_attachment[0]>2u ||
        state->weapon_attachment[1]) return FALSE;
    if(!state->weapon_visible || !state->weapon_attachment[0]) return TRUE;
    unsigned desired=state->weapon_attachment[0]==1u?0u:3u;
    TalWeaponPose before,after;
    if(!exact(roster,context) || !tal_weapon_pose_target(roster->actors[2],&before) ||
        *(unsigned *)(before.item+0x14u)+1u!=state->weapon_item_plus_one) return FALSE;
    for(unsigned s=0;s<before.submodels;++s) {
        if(*(uint16_t *)(before.rows+s*24u)==desired) continue;
        SudekiMpLanStoryResidencyReport report;
        if(SudekiMpLanStoryResidencyInspect(before.renderer,before.bank,desired,&report)!=
            SUDEKIMP_STORY_RESIDENCY_READY) return FALSE;
        if(!exact(roster,context) || !tal_weapon_pose_target(before.actor,&after) ||
            !tal_weapon_pose_same(&before,&after)) return FALSE;
        before.methods.set_selector(before.renderer,0,s,(int)desired);
        if(!exact(roster,context) || !tal_weapon_pose_target(before.actor,&after) ||
            !tal_weapon_pose_same(&before,&after)) return FALSE;
        before.methods.set_time(before.renderer,0,s,0.0f,0);
        if(!exact(roster,context) || !tal_weapon_pose_target(before.actor,&after) ||
            !tal_weapon_pose_same(&before,&after) ||
            *(uint16_t *)(before.rows+s*24u)!=desired) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryReplicaPrepareEquipment(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryFrame *frame,SudekiMpLanStoryReplicaExact exact,void *context) {
    BOOL combat=TRUE;
    if(!base || applying || !roster || !exact || !SudekiMpLanStoryFrameValid(frame) ||
        frame->available_mask!=roster->available_mask ||
        (native_thread && native_thread!=GetCurrentThreadId()) || !exact(roster,context) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || combat) return FALSE;
    native_thread=GetCurrentThreadId();
    for(unsigned c=0;c<4u;++c) if(frame->available_mask&(1u<<c)) {
        void *actor=roster->actors[c],*record=NULL;
        const SudekiMpLanStoryActor *state=&frame->actors[c];
        SudekiMpWeaponQuickList list;
        if(!exact(roster,context) || !readable(actor,0xc4u) ||
            !SudekiMpDescribeCharacterWeapons(actor,&list)) return FALSE;
        uint8_t *weapon=*(uint8_t **)((uint8_t *)actor+0xc0u);
        if(!readable(weapon,0x3b9u) || *(void **)(weapon+0x10u)!=actor) return FALSE;
        void *expected=NULL;
        unsigned slot=SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS;
        if(state->weapon_item_plus_one) {
            for(unsigned i=0;i<list.row_count;++i) {
                void *item=list.rows[i].native_item;
                if(!readable(item,0x18u)) return FALSE;
                if(*(uint32_t *)((uint8_t *)item+0x14u)+1u==state->weapon_item_plus_one) {
                    if(expected) return FALSE;
                    expected=item; slot=list.rows[i].slot;
                }
            }
            if(!expected) { SetLastError(ERROR_NOT_FOUND); return FALSE; }
        }
        if(*(void **)(weapon+0x268u)!=expected) {
            /* There is no validated unequip contract in this adapter. */
            if(!expected) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
            if(!equipment_idle(actor,&record) || record!=weapon) {
                SetLastError(ERROR_IO_PENDING); return FALSE;
            }
            struct EquipmentAttempt *attempt=&equipment_attempts[c];
            if(attempt->actor==actor && attempt->weapon==weapon && attempt->item==expected &&
                attempt->epoch==frame->epoch && attempt->generation==state->generation) {
                SetLastError(ERROR_IO_PENDING); return FALSE;
            }
            if(!exact(roster,context)) return FALSE;
            *attempt=(struct EquipmentAttempt){actor,weapon,expected,frame->epoch,state->generation};
            applying=TRUE;
            SudekiMpWeaponActivationResult result=SudekiMpActivateCharacterWeapon(actor,slot);
            applying=FALSE;
            SudekiMpLogFormat("lan_story_replica event=equipment_requested character=%u epoch=%lu generation=%lu item=%u status=%u\r\n",
                c,(unsigned long)frame->epoch,(unsigned long)state->generation,
                state->weapon_item_plus_one-1u,result.status);
            if(!exact(roster,context) ||
                (result.status!=SUDEKIMP_WEAPON_ACTIVATION_STARTED &&
                 result.status!=SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED)) return FALSE;
            void *after=NULL;
            if(!equipment_idle(actor,&after) || after!=weapon ||
                *(void **)(weapon+0x268u)!=expected) {
                SetLastError(ERROR_IO_PENDING); return FALSE;
            }
        }
        if(state->weapon_item_plus_one) {
            uint8_t attachment[2];
            if(!SudekiMpObserveCharacterWeaponAttachment(actor,attachment)) return FALSE;
            if(!!(weapon[0x3b8u]&2u)!=!!state->weapon_visible ||
                (state->weapon_attachment[0] && attachment[0]!=state->weapon_attachment[0]) ||
                (state->weapon_attachment[1] && attachment[1]!=state->weapon_attachment[1])) {
                if(!exact(roster,context)) return FALSE;
                applying=TRUE;
                BOOL shown=SudekiMpReconcileCharacterWeaponAttachment(actor,
                    state->weapon_attachment,state->weapon_visible!=0u);
                applying=FALSE;
                if(!shown || !exact(roster,context)) { SetLastError(ERROR_IO_PENDING); return FALSE; }
            }
            if(c==2u) {
                applying=TRUE;
                BOOL posed=tal_weapon_pose(roster,state,exact,context);
                applying=FALSE;
                if(!posed) { SetLastError(ERROR_IO_PENDING); return FALSE; }
            }
        }
        if(*(void **)((uint8_t *)actor+0xc0u)!=weapon ||
            *(void **)(weapon+0x268u)!=expected || !exact(roster,context)) return FALSE;
    }
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryReplicaApply(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryFrame *frame,BOOL apply_host_view,SudekiMpLanStoryReplicaExact exact,void *context) {
    Target targets[4]={{0}}; BOOL okay=FALSE,combat=TRUE;
    if(!base || applying || !roster || !frame || !exact ||
        !SudekiMpLanStoryFrameValid(frame) || frame->available_mask!=roster->available_mask ||
        (native_thread && native_thread!=GetCurrentThreadId()) || !exact(roster,context) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || combat) return FALSE;
    if(!native_thread) native_thread=GetCurrentThreadId();
    applying=TRUE;
    for(unsigned c=0;c<4u;++c) if(frame->available_mask&(1u<<c)) {
        if(frame->actors[c].native_pose) continue; /* Complete WorldPrepare owns this pose. */
        if(!target(roster->actors[c],c,&targets[c]) || !current_motion_known(&targets[c])) goto done;
        for(unsigned channel=0;channel<4u;++channel)
            if(!selector_loaded(&targets[c],SudekiMpLanPartyMotionSelector(types[c],
                frame->actors[c].locomotion.clip[channel]))) goto done;
    }
    /* Nothing changes until every present actor has passed the bounded bank
     * preflight. Each later native setter rechecks the exact paused owner. */
    for(unsigned c=0;c<4u;++c) if(frame->available_mask&(1u<<c)) {
        if(frame->actors[c].native_pose) continue;
        Target *t=&targets[c]; const SudekiMpLanStoryActor *a=&frame->actors[c];
        const SudekiMpLanArenaLocomotion *m=&a->locomotion;
        float xyz[3]={a->x,a->y,a->z},direction[3]={a->facing_x,0,a->facing_z};
        if(!actor_exact(roster,exact,context,t)) goto done;
        set_position(t->position,xyz);
        if(!actor_exact(roster,exact,context,t)) goto done;
        forward(t->position,direction);
        for(unsigned channel=0;channel<4u;++channel) for(unsigned sub=0;sub<t->submodels;++sub) {
            int selector=SudekiMpLanPartyMotionSelector(types[c],m->clip[channel]);
            if(!actor_exact(roster,exact,context,t)) goto done;
            if(t->methods.get_selector(t->renderer,channel,sub)!=selector) {
                if(!actor_exact(roster,exact,context,t)) goto done;
                t->methods.set_selector(t->renderer,channel,sub,selector);
                if(!actor_exact(roster,exact,context,t)) goto done;
            }
            if(t->methods.get_state(t->renderer,channel,sub)!=m->state[channel]) {
                if(!actor_exact(roster,exact,context,t)) goto done;
                t->methods.set_state(t->renderer,channel,sub,m->state[channel]);
                if(!actor_exact(roster,exact,context,t)) goto done;
            }
            if(!close_float(t->methods.get_time(t->renderer,channel,sub),m->time[channel],.0001f)) {
                if(!actor_exact(roster,exact,context,t)) goto done;
                t->methods.set_time(t->renderer,channel,sub,m->time[channel],0);
                if(!actor_exact(roster,exact,context,t)) goto done;
            }
            if(!close_float(t->methods.get_rate(t->renderer,channel,sub),m->rate[channel],.0001f)) {
                if(!actor_exact(roster,exact,context,t)) goto done;
                t->methods.set_rate(t->renderer,channel,sub,m->rate[channel]);
                if(!actor_exact(roster,exact,context,t)) goto done;
            }
            if(!actor_exact(roster,exact,context,t) ||
                t->methods.get_selector(t->renderer,channel,sub)!=selector ||
                t->methods.get_state(t->renderer,channel,sub)!=m->state[channel] ||
                !close_float(t->methods.get_time(t->renderer,channel,sub),m->time[channel],.01f) ||
                !close_float(t->methods.get_rate(t->renderer,channel,sub),m->rate[channel],.01f)) goto done;
        }
        for(unsigned blend=0;blend<3u;++blend) {
            if(!actor_exact(roster,exact,context,t)) goto done;
            if(!close_float(t->methods.get_blend(t->renderer,blend),m->blend[blend],.0001f))
                t->methods.set_blend(t->renderer,blend,m->blend[blend]);
            if(!actor_exact(roster,exact,context,t) ||
                !close_float(t->methods.get_blend(t->renderer,blend),m->blend[blend],.0001f)) goto done;
        }
        if(!actor_exact(roster,exact,context,t)) goto done;
        if(!readable(position_matrix(t->position),64u)) goto done;
        if(!actor_exact(roster,exact,context,t)) goto done;
        if(!visible(t,a)) {
            t->position[0xb8u]=1;
            if(!actor_exact(roster,exact,context,t) || !readable(position_matrix(t->position),64u)) goto done;
        }
        if(!actor_exact(roster,exact,context,t) || !visible(t,a)) goto done;
    }
    if(apply_host_view && !apply_view(roster,&frame->view,exact,context)) goto done;
    okay=exact(roster,context);
done:
    applying=FALSE; if(!okay) SetLastError(ERROR_RETRY); return okay;
}
BOOL SudekiMpLanStoryReplicaRestoreView(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryReplicaExact exact,void *context) {
    if(!view_owner.valid) return TRUE;
    if(!base || applying || native_thread!=GetCurrentThreadId() || !roster || !exact ||
        !exact(roster,context) || !camera_exact(&view_owner)) return FALSE;
    uint8_t *render=view_owner.render_state;
    if(!memcmp(render+0x90u,saved_view.matrix,sizeof(saved_view.matrix)) &&
        !memcmp(render+0xd0u,saved_view.projection,sizeof(saved_view.projection))) {
        /* Native camera publication may already have restored exactly the
         * retained original value. Positive equality needs no overwrite. */
        SudekiMpLanArenaOwnerViewClear(&view_owner);
        memset(&saved_view,0,sizeof(saved_view)); memset(&last_view,0,sizeof(last_view));
        saved_anchor_valid=FALSE;
        return TRUE;
    }
    /* Only overwrite this adapter's last publication. A different camera or
     * intervening writer is unknown; retain the original for a safe retry. */
    if(memcmp(render+0x90u,last_view.matrix,sizeof(last_view.matrix)) ||
        memcmp(render+0xd0u,last_view.projection,sizeof(last_view.projection))) return FALSE;
    if(!exact(roster,context) || !camera_exact(&view_owner)) return FALSE;
    memcpy(render+0x90u,saved_view.matrix,sizeof(saved_view.matrix));
    memcpy(render+0xd0u,saved_view.projection,sizeof(saved_view.projection));
    ++*(uint16_t *)(render+0x2cu);
    last_view=saved_view;
    if(!exact(roster,context) || !camera_exact(&view_owner)) return FALSE;
    SudekiMpLanArenaOwnerViewClear(&view_owner);
    memset(&saved_view,0,sizeof(saved_view)); memset(&last_view,0,sizeof(last_view));
    saved_anchor_valid=FALSE;
    return TRUE;
}
BOOL SudekiMpLanStoryReplicaRetainsView(void) { return view_owner.valid; }
BOOL SudekiMpLanStoryReplicaViewSeed(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryReplicaExact exact,void *context,SudekiMpLanStoryView *view,float anchor[3]) {
    if(!view || !anchor || !base || applying || native_thread!=GetCurrentThreadId() ||
        !saved_anchor_valid || !view_owner.valid || !roster || !exact ||
        !exact(roster,context) || !camera_exact(&view_owner) ||
        !SudekiMpLanStoryViewGeometryValid(&saved_view)) return FALSE;
    *view=saved_view; memcpy(anchor,saved_anchor,sizeof(saved_anchor)); return TRUE;
}
BOOL SudekiMpUninstallLanStoryReplica(void) {
    if(applying || view_owner.valid) { SetLastError(ERROR_BUSY); return FALSE; }
    base=NULL; set_position=NULL; position_matrix=NULL; story_set_forward=NULL;
    memset(equipment_attempts,0,sizeof(equipment_attempts));
    native_thread=0; return TRUE;
}
