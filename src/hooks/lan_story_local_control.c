#include "hooks/lan_story_local_control.h"
#include "hooks/lan_story_input.h"
#include "hooks/lan_story_menu.h"
#include "hooks/lan_party_menu_native.h"
#include "hooks/lan_arena_owner_view.h"
#include "network/lan_story_frame.h"
#include "engine/orbit_camera.h"
#include "engine/build_identity.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "cleanroom/engine.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <wincrypt.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story local control requires the supported native x86 ABI"
#endif

enum { NEXT=0x24060u, SWITCH_SIZE=0xfcu };
static uint8_t *base,verified_next[SWITCH_SIZE];
static DWORD native_thread;
static BOOL active,entered,uncertain;
static const char *switch_reason="native_selection_busy";
static uint64_t last_attempt;
static unsigned selected=4u;
static SudekiMpLanStoryNativeRoster retained;
static struct {
    SudekiMpLanArenaOwnerViewLease owner;
    SudekiMpLanStoryView geometry;
    float anchor[3],distance;
    BOOL seeded,framed,published;
} view;


static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE ||
        access==PAGE_EXECUTE_WRITECOPY) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL next_supported(uint8_t *image) {
    /* Same normalized whole-function proof as the established SMP4 native
     * rotation adapter. Story requires the pristine entry, never its RET
     * campaign patch or a trampoline borrowed from another owner. */
    static const uint8_t expected[32]={
        0x6a,0x72,0x83,0xc4,0x8a,0x49,0xd5,0xde,0x30,0x23,0x7c,0xf9,0xad,0x34,0x58,0x8d,
        0xb0,0x1f,0x1f,0x4c,0x22,0x13,0x98,0x46,0xcf,0xd7,0x27,0xc9,0x15,0x4d,0x15,0x66};
    uint8_t normalized[SWITCH_SIZE],digest[32]; uint32_t operand=0x408d1cu;
    HCRYPTPROV provider=0; HCRYPTHASH hash=0; DWORD n=sizeof(digest); BOOL exact=FALSE;
    if(!readable(image+NEXT,sizeof(normalized)) || image[NEXT]!=0x55 ||
        *(void **)(image+NEXT+0x5e)!=image+operand) return FALSE;
    memcpy(normalized,image+NEXT,sizeof(normalized)); memcpy(normalized+0x5e,&operand,4);
    if(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT) &&
        CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
        CryptHashData(hash,normalized,sizeof(normalized),0) &&
        CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0) && n==sizeof(digest))
        exact=!memcmp(digest,expected,sizeof(digest));
    if(hash) CryptDestroyHash(hash);
    if(provider) CryptReleaseContext(provider,0);
    if(exact) memcpy(verified_next,image+NEXT,sizeof(verified_next));
    return exact;
}
static BOOL actor_idle(void *actor) {
    uint8_t *a=actor,*arbiter,*interaction,*mode; SudekiMpCharacterSkillState skill;
    BOOL weapon_pending=TRUE;
    if(!readable(a,0xdcu) || !readable(arbiter=*(uint8_t **)(a+0x90u),0x64u) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10u)!=actor ||
        (*(uint32_t *)(arbiter+0x50u)&0x081802c8u) || (arbiter[0x60u]&5u) ||
        !SudekiMpWeaponActivationPending(actor,&weapon_pending) || weapon_pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !readable(skill.skill,0x78u) || *(void **)(a+0xd8u)!=skill.skill ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor || ((uint8_t *)skill.skill)[0x6cu]) return FALSE;
    void *task=*(void **)((uint8_t *)skill.skill+0x74u);
    if(task && (!readable(task,8u) || *(void **)task || !*((uint32_t *)task+1))) return FALSE;
    interaction=*(uint8_t **)(a+0xa8u);
    if(interaction && (!readable(interaction,0x64u) ||
        !readable(mode=*(uint8_t **)(interaction+0x60u),0x4du) || mode[0x4cu])) return FALSE;
    return TRUE;
}

static BOOL retain(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)&retained,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
static BOOL same_party(const SudekiMpLanStoryNativeRoster *a,
    const SudekiMpLanStoryNativeRoster *b) {
    /* The current zone descriptor is not party identity: walking into the
     * next zone of the same world (including a followed host zone, #42)
     * keeps world, group, controller, epoch, actors and AI. */
    return a->world==b->world && a->group==b->group &&
        a->controller==b->controller && a->epoch==b->epoch &&
        a->available_mask && a->available_mask==b->available_mask &&
        !memcmp(a->actors,b->actors,sizeof(a->actors)) && !memcmp(a->ai,b->ai,sizeof(a->ai));
}
static BOOL boundary(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryScene *scene) {
    BOOL paused=FALSE;
    return base && w && r && scene && (!native_thread || native_thread==GetCurrentThreadId()) &&
        w->service_post_original_exact && w->dispatch_serial && r->controller==controller &&
        r->available_mask && !(r->available_mask&~15u) && r->leader_character<4u &&
        (r->available_mask&(1u<<r->leader_character)) &&
        scene->epoch==r->epoch && scene->revision==r->revision &&
        scene->leader_seat==r->leader_character && scene->available_mask==r->available_mask &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
        SudekiMpLanStoryObserverRosterStillExact(w,r) &&
        SudekiMpLanStoryInputExact(controller,r->actors[r->leader_character]) &&
        SudekiMpLanPartyMenuNativeObserveOwnedPause(&paused) && paused &&
        SudekiMpLanPartyMenuNativeOwnsPause() &&
        readable(base+NEXT,sizeof(verified_next)) &&
        !memcmp(base+NEXT,verified_next,sizeof(verified_next));
}
static BOOL binding(const SudekiMpLanStoryNativeRoster *r) {
    for(unsigned c=0u;c<4u;++c) if(r->available_mask&(1u<<c)) {
        uint8_t *a=r->actors[c],*ai=r->ai[c],*mode;
        if(!readable(a,0x98u) || *(void **)(a+0x94u)!=ai ||
            !readable(ai,0x174u) || *(void **)ai!=base+0x2d4924u ||
            *(void **)(ai+0x10u)!=a || *(int16_t *)(ai+0x16au)!=0 ||
            !readable(mode=*(uint8_t **)(ai+0x3cu),0xcu) ||
            mode[0xbu]!=(c==r->leader_character?0u:1u)) return FALSE;
    }
    return TRUE;
}
static BOOL listeners_closed(const SudekiMpLanStoryNativeRoster *r) {
    uint8_t *g=r->group;
    unsigned count=*(unsigned *)(g+0x38u);
    uint8_t **items=*(uint8_t ***)(g+0x40u);
    if(!count || count>8u || !readable(items,count*4u)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *p=items[i],*vt;
        if(!readable(p,4u) || !readable(vt=*(uint8_t **)p,0x18u)) return FALSE;
        if(vt==base+0x2ca244u || vt==base+0x2c5248u) {
            if(*(void **)(vt+0x14u)!=base+0x8760u) return FALSE;
        } else if(vt==base+0x2d4d14u) {
            if(*(void **)(vt+0x14u)!=base+0xc8d20u) return FALSE;
        } else if(vt==base+0x2d4fa4u) {
            if(*(void **)(vt+0x14u)!=base+0xbcc30u || (uintptr_t)p<0x3cu) return FALSE;
            uint8_t *owner=p-0x3cu;
            /* Status listener can retire a native effect. Admit only its
             * empty cosmetic state, never force cancellation during handoff. */
            unsigned matches=0;
            if(!readable(owner,0x94u)) return FALSE;
            for(unsigned c=0;c<4u;++c) if((r->available_mask&(1u<<c)) &&
                *(void **)(owner+0x10u)==r->actors[c]) ++matches;
            if(matches!=1u || *(void **)owner!=base+0x2d4f3cu ||
                *(void **)(owner+0x80u) || *(void **)(owner+0x54u) ||
                *(uint32_t *)(owner+0x88u)) return FALSE;
        } else return FALSE;
    }
    return count==*(unsigned *)(g+0x38u) && items==*(uint8_t ***)(g+0x40u);
}
static BOOL companion_state_can_exit(uint8_t *mode) {
    /* Newly recruited companions have no current state. A loaded companion
     * can already have AwareActiveBlock selected, even outside combat. The
     * contained client cannot tick that AI state to completion. Retail Next
     * disables it through 4EC2D0 and its synchronous virtual Exit callback;
     * do not require the callback's post-state before entering that path. */
    if(mode[8u]==0xffu) return TRUE;
    uint8_t **states,*state;
    if(*(void **)mode!=base+0x2da340u || !mode[9u] || mode[9u]>32u ||
        mode[8u]>=mode[9u] ||
        !readable(states=*(uint8_t ***)(mode+4u),mode[9u]*sizeof(*states)) ||
        !readable(state=states[mode[8u]],0x28u) ||
        *(void **)state!=base+0x2dc5d4u ||
        !readable(base+0x2dc5d4u,0x10u) ||
        *(void **)(base+0x2dc5e0u)!=base+0x1acc20u) return FALSE;
    /* Whole 5ACC20 body: only the exact state object's flags, weight and
     * cooldown are changed. No task, script, actor or scheduler callback.
     * Its sole absolute operand is the retail cooldown constant. */
    static const uint8_t expected[]={
        0xd9,0x41,0x0c,0x66,0x83,0x49,0x1a,0x01,0xd9,0x59,0x10,
        0xc6,0x41,0x18,0x00,0xd9,0x05,0xcc,0x35,0x6e,0x00,
        0xd9,0x59,0x14,0xc2,0x0c,0x00};
    uint8_t normalized[sizeof(expected)]; uint32_t operand=0x006e35ccu;
    if(!readable(base+0x1acc20u,sizeof(expected)) || !readable(base+0x2e35ccu,4u) ||
        *(void **)(base+0x1acc31u)!=base+0x2e35ccu) return FALSE;
    memcpy(normalized,base+0x1acc20u,sizeof(normalized));
    memcpy(normalized+17u,&operand,sizeof(operand));
    MEMORY_BASIC_INFORMATION memory;
    if(memcmp(normalized,expected,sizeof(expected)) ||
        VirtualQuery(state,&memory,sizeof(memory))!=sizeof(memory)) return FALSE;
    DWORD access=memory.Protect&0xffu;
    return access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL switch_ready(const SudekiMpLanStoryNativeRoster *r) {
    uint8_t *g=r->group,*c=r->controller,*speech,*quit,*quick,*front,*query;
    BOOL combat=TRUE; int spirit=-1;
    switch_reason="native_selection_group_or_filter";
    if(!readable(g,0xd8u) || !readable(c,0x24cu) ||
        *(unsigned *)(g+0xd0u) || !g[0xd6u] || g[0xd7u] || *(void **)(g+0xc0u) ||
        *(unsigned *)(c+0xf4u) || *(unsigned *)(c+0xfcu) ||
        *(int *)(c+0x80u)!=1 || *(int *)(c+0x84u)!=1) return FALSE;
    switch_reason="native_selection_action_or_menu";
    if(SudekiMpLanStoryMenuCapturesInput() || *(void **)(base+0x408db8u) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || combat ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0) return FALSE;
    switch_reason="native_selection_ai_binding";
    if(!binding(r)) return FALSE;
    switch_reason="native_selection_listeners";
    if(!listeners_closed(r)) return FALSE;
    switch_reason="native_selection_native_ui";
    speech=*(uint8_t **)(base+0x408d3cu); quit=*(uint8_t **)(base+0x408d68u);
    quick=*(uint8_t **)(base+0x3c2f84u); front=*(uint8_t **)(base+0x408d1cu);
    if((speech && (!readable(speech,0x47eu) || speech[0x47du])) ||
        !readable(quit,0x1c3u) || quit[0x1c2u] || !readable(quick,0x2au) || quick[0x29u] ||
        !readable(front,0x178u) || !readable(query=*(uint8_t **)(front+0x174u),0xb0u) ||
        *(void **)query!=base+0x2caf9cu || *(void **)(base+0x2cafc8u)!=base+0x9c930u)
        return FALSE;
    for(unsigned i=0u;i<4u;++i) if(r->available_mask&(1u<<i)) {
        uint8_t *ai=r->ai[i],*mode=*(uint8_t **)(ai+0x3cu);
        switch_reason="native_selection_actor_action";
        if(!actor_idle(r->actors[i]) || *(void **)(ai+0xc0u)) return FALSE;
        switch_reason="native_selection_companion_ai_state";
        if(i!=r->leader_character && !companion_state_can_exit(mode)) return FALSE;
    }
    return TRUE;
}
/* Pristine retail Next: ESI=group, no stack arguments, ordinary RET. The
 * entire native transaction rotates the real party, controller, AI modes,
 * camera/HUD and listeners. No global actor identity is manufactured here. */
__attribute__((naked,noinline,used)) static void call_next(void *group __attribute__((unused)),
    void *function __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp),%esi\n\tcall *12(%esp)\n\tpopl %esi\n\tret\n\t");
}

/* Defined with the selected native camera contract below. A physically
 * selected actor is reported independently from camera readiness. */
static BOOL camera_ready(const SudekiMpLanStoryNativeRoster *r,float matrix[16]);

BOOL SudekiMpLanStoryLocalControlInstall(HMODULE image) {
    if(base || !image || !SudekiMpCheckLoadedExecutable(image) || !next_supported((uint8_t *)image)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=(uint8_t *)image; native_thread=0; active=entered=uncertain=FALSE;
    last_attempt=0; selected=4u; memset(&retained,0,sizeof(retained)); memset(&view,0,sizeof(view));
    return TRUE;
}
static BOOL publish(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *before,SudekiMpLanStoryLocalControlExact exact,
    void *context,SudekiMpLanStoryLocalControlReport *report) {
    SudekiMpLanStoryScene scene;
    SudekiMpLanStoryNativeRoster fresh;
    if(!SudekiMpLanStoryObserverSample(controller,w,&scene) ||
        !SudekiMpLanStoryObserverRoster(controller,w,&scene,&fresh) ||
        !same_party(before,&fresh) || !boundary(controller,w,&fresh,&scene) ||
        !binding(&fresh) || !exact(&fresh,&scene,context)) {
        uncertain=TRUE; report->unknown=TRUE;
        report->reason="native_selection_confirmation"; return retain(ERROR_RETRY);
    }
    report->roster=fresh; report->scene=scene;
    report->coherent=TRUE; retained=fresh;
    report->bound=fresh.leader_character==selected;
    if(!report->bound) {
        /* A proved intermediate native permutation is adopted by containment.
         * Only a fresh dispatch may rotate again toward the reserved target. */
        report->reason=fresh.leader_character==before->leader_character?
            "native_selection_veto":"native_selection_intermediate"; SetLastError(ERROR_BUSY); return FALSE;
    }
    report->camera_exact=camera_ready(&fresh,NULL);
    report->reason=report->camera_exact?"ready":"local_camera_unconfirmed";
    SetLastError(report->camera_exact?ERROR_SUCCESS:ERROR_RETRY);
    return report->camera_exact;
}
BOOL SudekiMpLanStoryLocalControlSwitch(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *before,
    const SudekiMpLanStoryScene *scene,unsigned character,
    SudekiMpLanStoryLocalControlExact exact,void *context,
    SudekiMpLanStoryLocalControlReport *out) {
    SudekiMpLanStoryLocalControlReport report={0};
    report.reason="invalid_selection_boundary";
    if(!out || !exact || active || character>=4u || !before ||
        !(before->available_mask&(1u<<character)) || !boundary(controller,w,before,scene) ||
        !exact(before,scene,context)) {
        if(out) *out=report;
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    native_thread=GetCurrentThreadId();
    if(uncertain || (entered && !same_party(&retained,before))) {
        report.entered=entered; report.unknown=TRUE; report.reason="selection_retained_unknown";
        *out=report; return retain(ERROR_INVALID_STATE);
    }
    if(!view.seeded) {
        report.reason="native_view_seed_unavailable"; *out=report; return FALSE;
    }
    if(selected!=character) { selected=character; view.published=FALSE; view.framed=FALSE; }
    if(before->leader_character==character) {
        retained=*before; entered=TRUE; report.entered=TRUE;
        BOOL result=publish(controller,w,before,exact,context,&report);
        *out=report; return result;
    }
    if(!switch_ready(before) ||
        (entered && w->dispatch_serial==last_attempt) || !exact(before,scene,context) ||
        !boundary(controller,w,before,scene)) {
        report.entered=entered; report.reason=switch_reason;
        *out=report; SetLastError(ERROR_BUSY); return FALSE;
    }
    /* Save each attempt before entering native code. A failed post proof
     * retains identity; successful intermediate steps need a new dispatch. */
    retained=*before; entered=TRUE; last_attempt=w->dispatch_serial;
    report.entered=TRUE; active=TRUE;
    call_next(before->group,base+NEXT);
    active=FALSE;
    BOOL result=publish(controller,w,before,exact,context,&report);
    *out=report; return result;
}
BOOL SudekiMpLanStoryLocalControlReady(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene) {
    return !active && entered && !uncertain && boundary(controller,w,r,scene) &&
        same_party(&retained,r) && r->leader_character==selected && binding(r) &&
        view.published && camera_ready(r,NULL);
}
BOOL SudekiMpLanStoryLocalControlDirection(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,float local_x,float local_z,float *world_x,float *world_z) {
    float matrix[16],local[3]={local_x,0.0f,local_z},world[3];
    if(world_x) *world_x=0.0f;
    if(world_z) *world_z=0.0f;
    if(!world_x || !world_z || !isfinite(local_x) || !isfinite(local_z) ||
        fabsf(local_x)>1.001f || fabsf(local_z)>1.001f ||
        !SudekiMpLanStoryLocalControlReady(controller,w,r,scene) || !camera_ready(r,matrix)) return FALSE;
    float magnitude=sqrtf(local_x*local_x+local_z*local_z);
    if(magnitude<0.0001f) return boundary(controller,w,r,scene);
    if(!SudekiMpCameraTransformHorizontalDirection(matrix,local,world) ||
        !boundary(controller,w,r,scene)) return FALSE;
    if(magnitude>1.0f) magnitude=1.0f;
    *world_x=world[0]*magnitude; *world_z=world[2]*magnitude; return TRUE;
}
BOOL SudekiMpLanStoryLocalControlRetains(void) { return active || entered; }
BOOL SudekiMpLanStoryLocalControlNativeExitReturned(void) {
    if(active || (native_thread && native_thread!=GetCurrentThreadId())) return retain(ERROR_BUSY);
    /* Native quit is the caller's destruction witness. Never dereference the
     * now-dead actor/controller/camera to prove destruction a second time. */
    entered=uncertain=FALSE; last_attempt=0; selected=4u; memset(&retained,0,sizeof(retained));
    memset(&view,0,sizeof(view));
    return TRUE;
}
BOOL SudekiMpLanStoryLocalControlUninstall(void) {
    if(active || entered || (native_thread && native_thread!=GetCurrentThreadId())) return retain(ERROR_BUSY);
    base=NULL; native_thread=0; uncertain=FALSE; memset(verified_next,0,sizeof(verified_next));
    return TRUE;
}

static BOOL actor_position(void *actor,float out[3]) {
    if(!readable(actor,0x48u)) return FALSE;
    uint8_t *p=*(uint8_t **)((uint8_t *)actor+0x44u);
    if(!readable(p,0x98u) || *(void **)p!=base+0x2cdefcu ||
        *(void **)(p+0x10u)!=actor ||
        (*(uintptr_t *)(p+0x94u) && *(uintptr_t *)(p+0x94u)!=4u)) return FALSE;
    memcpy(out,p+0x18u,12u);
    for(unsigned i=0;i<3u;++i) if(!isfinite(out[i]) || fabsf(out[i])>1000000.0f) return FALSE;
    return TRUE;
}
static BOOL camera_owner(SudekiMpLanArenaOwnerViewLease *owner,void *actor,BOOL target) {
    if(!base || !owner || !readable(base+0x408da8u,4u) || !readable(base+0x408d58u,4u)) return FALSE;
    void *mode=*(void **)(base+0x408da8u),*scene=*(void **)(base+0x408d58u);
    SudekiMpLanArenaOwnerViewLease candidate=*owner;
    if(!candidate.valid) {
        if(!SudekiMpLanArenaOwnerViewCapture(&candidate,mode,scene)) return FALSE;
    } else if(!SudekiMpLanArenaOwnerViewService(&candidate,mode,scene,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER)) return FALSE;
    uint8_t *camera=candidate.camera,*state=candidate.render_state;
    if(!readable(camera,0xbcu) || *(void **)camera!=base+0x2cce5cu ||
        !readable(state,0xdcu) || *(void **)state!=base+0x2dd638u) return FALSE;
    uint8_t *manager=*(uint8_t **)(base+0x409d7cu);
    if(!readable(manager,0x60u) || *(void **)manager!=base+0x2c7b80u ||
        *(void **)(manager+0x20u)!=camera) return FALSE;
    unsigned matches=0;
    for(unsigned i=0;i<10u;++i) if(*(void **)(manager+0x24u+4u*i)==camera) ++matches;
    if(matches!=1u) return FALSE;
    if(target) {
        uint8_t *a=*(uint8_t **)(camera+0xb4u),*b=*(uint8_t **)(camera+0xb8u);
        /* 535040 carries the native weak actor tuple at GameObjectTarget+20.
         * Reassignment installs it in both slots. Exploration may retain an
         * OffsetTarget in slot0; its exact +20 source must be this very slot1
         * target (59A150/59A1A0 delegate to that source). No arbitrary target
         * chain, matrix target, camera-state callback or native call here. */
        if(!readable(b,0x34u) || *(void **)b!=base+0x2d42ccu ||
            !*(uint32_t *)(b+4u) || *(void **)(b+0x20u)!=actor) return FALSE;
        if(a!=b && (!readable(a,0xd0u) || *(void **)a!=base+0x2d436cu ||
            !*(uint32_t *)(a+4u) || *(void **)(a+0x20u)!=b)) return FALSE;
    }
    *owner=candidate; return TRUE;
}
BOOL SudekiMpLanStoryLocalControlSeedView(const SudekiMpLanStoryView *seed,
    const float anchor[3]) {
    if(!base || active || entered || !seed || seed->valid!=1u || !anchor ||
        (native_thread && native_thread!=GetCurrentThreadId()) ||
        !SudekiMpLanStoryViewGeometryValid(seed)) return FALSE;
    float distance=0.0f;
    for(unsigned i=0;i<3u;++i) {
        if(!isfinite(anchor[i]) || fabsf(anchor[i])>1000000.0f) return FALSE;
        float d=seed->matrix[12u+i]-anchor[i]; distance+=d*d;
    }
    if(!isfinite(distance) || distance<0.01f || distance>1000000.0f) return FALSE;
    if(view.seeded) return !memcmp(view.geometry.matrix,seed->matrix,sizeof(seed->matrix)) &&
        !memcmp(view.geometry.projection,seed->projection,sizeof(seed->projection)) &&
        !memcmp(view.anchor,anchor,sizeof(view.anchor));
    native_thread=GetCurrentThreadId();
    view.geometry=*seed; memcpy(view.anchor,anchor,sizeof(view.anchor)); view.seeded=TRUE;
    return TRUE;
}
static BOOL camera_ready(const SudekiMpLanStoryNativeRoster *r,float matrix[16]) {
    if(!view.seeded || selected>=4u || r->leader_character!=selected ||
        !camera_owner(&view.owner,r->actors[selected],TRUE) ||
        !SudekiMpLanStoryViewGeometryValid(&view.geometry)) return FALSE;
    if(matrix) memcpy(matrix,view.geometry.matrix,sizeof(view.geometry.matrix));
    return TRUE;
}
/* Retail 4E8D50 -> 518580/518850: camera+38 is a resource handle, not
 * the config payload. Float fields are packed according to the 207-bit
 * presence mask; a missing field inherits through payload+4. Read only. */
static BOOL camera_setting(unsigned index,float *out) {
    if(index<2u || index>=200u) return FALSE;
    uint8_t *camera=view.owner.camera,*handle=*(uint8_t **)(camera+0x38u);
    void *seen[16]; unsigned count=0;
    while(handle && count<16u) {
        if(!readable(handle,0xcu)) return FALSE;
        uint8_t *config=*(uint8_t **)(handle+8u);
        if(!readable(config,0x48u)) return FALSE;
        for(unsigned i=0;i<count;++i) if(seen[i]==config) return FALSE;
        seen[count++]=config;
        unsigned size=*(unsigned *)config,offset=0,total=0;
        uint32_t *mask=(uint32_t *)(config+8u);
        if(mask[6]&0xffff8000u) return FALSE;
        for(unsigned i=0;i<207u;++i) if(mask[i/32u]&(1u<<(i%32u))) {
            unsigned bytes=i<200u?4u:12u;
            total+=bytes; if(i<index) offset+=bytes;
        }
        uint8_t *values=*(uint8_t **)(config+0x44u);
        if(size!=0x48u+total || values!=config+0x48u || !readable(config,size)) return FALSE;
        if(mask[index/32u]&(1u<<(index%32u))) {
            float value=*(float *)(values+offset);
            if(!isfinite(value)) return FALSE;
            *out=value; return TRUE;
        }
        handle=*(uint8_t **)(config+4u);
    }
    return FALSE;
}
static float clamp(float x,float lo,float hi) {return x<lo?lo:x>hi?hi:x;}
static BOOL frame_camera(SudekiMpLanStoryView *next,const float position[3],
    float yaw,float vertical,float *distance_out) {
    float minimum,maximum,initial,scale,at_target,near_distance,near_height,far_height,lookat;
    float rot_min,rot_max,rot_scale;
    if(!camera_setting(6u,&lookat) || !camera_setting(7u,&minimum) ||
        !camera_setting(8u,&maximum) || !camera_setting(11u,&initial) ||
        !camera_setting(12u,&rot_min) || !camera_setting(13u,&rot_max) ||
        !camera_setting(18u,&rot_scale) ||
        !camera_setting(26u,&scale) || !camera_setting(27u,&at_target) ||
        !camera_setting(28u,&near_distance) || !camera_setting(29u,&near_height) ||
        !camera_setting(30u,&far_height) || minimum<0.1f || maximum<=minimum ||
        maximum>30.0f || initial<minimum || initial>maximum || scale<0 || scale>100.0f ||
        near_distance<=0 || near_distance>=maximum || fabsf(lookat)>5.0f ||
        fabsf(at_target)>20 || fabsf(near_height)>20 || fabsf(far_height)>20 ||
        rot_min<0 || rot_max<0 || rot_min>360 || rot_max>360 || fabsf(rot_scale)>10) return FALSE;
    uint8_t *camera=view.owner.camera,*target=*(uint8_t **)(camera+0xb4u),
        *source=*(uint8_t **)(camera+0xb8u);
    float anchor[3]; memcpy(anchor,position,sizeof(anchor));
    if(target!=source) {
        /* The exact native OffsetTarget holds a cached transformed point;
         * GameObjectTarget+14 is its source observation. Use their relative
         * offset (native exploration is +1.8 high), never their stale world
         * coordinates while the client simulation is contained. */
        for(unsigned i=0;i<3u;++i) {
            float offset=*(float *)(target+0xb0u+4u*i)-*(float *)(source+0x14u+4u*i);
            if(!isfinite(offset) || fabsf(offset)>5.0f) return FALSE;
            anchor[i]+=offset;
        }
    }
    anchor[1]+=lookat;
    /* 47CBB0 changes flat distance, clamped to the authored limits.
     * Input supplies sensitivity-scaled axis seconds, not a pitch angle. */
    float distance=clamp((view.framed?view.distance:initial)-vertical*scale,minimum,maximum);
    /* 47C2C0/47CCD0 interpolate native degrees per second with distance. */
    yaw=clamp(yaw*rot_scale*(rot_min+(rot_max-rot_min)*
        clamp((distance-minimum)/(maximum-minimum),0,1))*0.01745329252f,-0.5f,0.5f);
    float height=distance<near_distance?
        at_target+(near_height-at_target)*clamp(distance/near_distance,0,1):
        near_height+(far_height-near_height)*clamp((distance-near_distance)/(maximum-near_distance),0,1);
    /* Preserve heading only. The old seed's arbitrary lateral offset and
     * pitch must not survive the spectator -> own-character handoff. */
    float fx=next->matrix[8],fz=next->matrix[10],flat=sqrtf(fx*fx+fz*fz);
    if(!isfinite(flat) || flat<0.001f) return FALSE;
    fx/=flat; fz/=flat;
    float sine=sinf(yaw),cosine=cosf(yaw),x=fx*cosine+fz*sine;
    fz=fz*cosine-fx*sine; fx=x;
    float eye[3]={position[0]-fx*distance,position[1]+height,position[2]-fz*distance};
    float forward[3]={anchor[0]-eye[0],anchor[1]-eye[1],anchor[2]-eye[2]};
    float length=sqrtf(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
    if(!isfinite(length) || length<0.1f) return FALSE;
    for(unsigned i=0;i<3u;++i) forward[i]/=length;
    flat=sqrtf(forward[0]*forward[0]+forward[2]*forward[2]);
    if(flat<0.001f) return FALSE;
    float right[3]={-forward[2]/flat,0,forward[0]/flat};
    float up[3]={-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]};
    memset(next->matrix,0,sizeof(next->matrix));
    memcpy(next->matrix,right,12u); memcpy(next->matrix+4u,up,12u);
    memcpy(next->matrix+8u,forward,12u); memcpy(next->matrix+12u,eye,12u); next->matrix[15]=1;
    *distance_out=distance;
    return SudekiMpLanStoryViewGeometryValid(next);
}
BOOL SudekiMpLanStoryLocalControlPresent(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryLocalViewExact exact,void *context,float yaw,float pitch) {
    if(!base || !entered || uncertain || active || native_thread!=GetCurrentThreadId() ||
        !r || !exact || !isfinite(yaw) || !isfinite(pitch) ||
        fabsf(yaw)>0.5f || fabsf(pitch)>0.5f || !same_party(&retained,r) ||
        r->leader_character!=selected || !exact(r,context) || !binding(r) ||
        !SudekiMpLanStoryInputExact(r->controller,r->actors[selected]) || !camera_ready(r,NULL)) return FALSE;
    float position[3]; SudekiMpLanStoryView next=view.geometry;
    if(!actor_position(r->actors[selected],position)) return FALSE;
    float distance;
    if(!frame_camera(&next,position,yaw,pitch,&distance) ||
        !SudekiMpLanStoryViewGeometryValid(&next) || !exact(r,context) || !camera_ready(r,NULL)) return FALSE;
    uint8_t *state=view.owner.render_state; MEMORY_BASIC_INFORMATION memory;
    if(VirtualQuery(state,&memory,sizeof(memory))!=sizeof(memory)) return FALSE;
    DWORD access=memory.Protect&0xffu;
    if(access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    /* Native camera interpolation has zero elapsed world time on a contained
     * client. Publish only this exact owned view: no camera-state tick, actor
     * movement, collision, trigger or script callback is executed here. */
    active=TRUE;
    memcpy(state+0x90u,next.matrix,sizeof(next.matrix));
    memcpy(state+0xd0u,next.projection,sizeof(next.projection));
    ++*(uint16_t *)(state+0x2cu);
    active=FALSE;
    view.geometry=next; view.distance=distance; view.framed=TRUE;
    memcpy(view.anchor,position,sizeof(position));
    if(!exact(r,context) || !camera_ready(r,NULL) ||
        memcmp(state+0x90u,next.matrix,sizeof(next.matrix)) ||
        memcmp(state+0xd0u,next.projection,sizeof(next.projection))) {
        uncertain=TRUE; return retain(ERROR_RETRY);
    }
    view.published=TRUE; SetLastError(ERROR_SUCCESS); return TRUE;
}
