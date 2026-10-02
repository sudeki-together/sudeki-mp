#include "hooks/lan_party_cast.h"
#include "hooks/lan_arena_cast_context.h"
#include "hooks/lan_arena_client_input.h"
#include "hooks/lan_arena_client_skill_handoff.h"
#include "hooks/quick_skill_input.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/skill_activation_abi.h"
#include "engine/spirit_activation_abi.h"
#include "engine/spirit_instance_abi.h"
#include "engine/cast_light_abi.h"
#include "engine/cast_motion_blur_abi.h"
#include "engine/log.h"
#include "network/lan_party_motion.h"
#include <math.h>
#include <string.h>

typedef struct PartyCast {
    void *actor,*task,*skill;
    SudekiMpSpiritInstance instance;
    SudekiMpLanPartyLease network_owner;
    uint16_t sequence,prepared,seen,prime_sequence;
    uint8_t kind,slot;
    uint32_t cost;
    BOOL active,started,active_seen;
    SudekiMpLanArenaClientSkillRetryGate retry;
} PartyCast;
static PartyCast casts[4];
static HMODULE image;
static SudekiMpLanPartySession *cast_session;
static SudekiMpLanPartyRosterObservation bound_roster;
static uint64_t native_session,native_lifetime_serial;
static unsigned local_seat;
static DWORD game_thread,last_service,last_diagnostic,bind_retry_at;
static unsigned int reported_instance_fault;
static BOOL bound,initialized,context_installed,task_routed,skill_routed,spirit_routed;
static BOOL menu_owned,callbacks_pinned,initializing,install_failed;
static volatile LONG stopping;
static struct { void *actor; uint16_t sequence; uint8_t slot; } replay_entry;
static SudekiMpInlineHook host_camera_hook;
static SudekiMpInlineHook normal_speed_hook,master_speed_hook,variable_speed_hook;
static SudekiMpBytePatch alternate_speed_patches[4];
static SudekiMpRelativeCallHook host_skill_use_hooks[2];
static SudekiMpBytePatch quick_menu_speed_patch;
static SudekiMpSkillUseFunction host_skill_use_original;
typedef BOOL (__attribute__((thiscall)) *SetCamera)(void *,const char *);

static BOOL memory(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static int actor_index(void *actor) {
    for(unsigned i=0;i<4;++i) if(actor && casts[i].actor==actor) return (int)i;
    return -1;
}
static BOOL roster_exact(void) {
    if(!image || !game_thread || GetCurrentThreadId()!=game_thread ||
        !SudekiMpLanPartyControlNativeRosterExact(&bound_roster)) return FALSE;
    for(unsigned i=0;i<4;++i) if(bound_roster.actors[i]!=casts[i].actor) return FALSE;
    return TRUE;
}
static BOOL retained(void *actor,uint64_t session) {
    int i=actor_index(actor);
    return native_session && session==native_session && i>=0 &&
        bound_roster.actors[i]==actor &&
        SudekiMpLanPartyControlNativeActorExact(&bound_roster,(unsigned)i);
}
static BOOL authority(unsigned seat) {
    SudekiMpLanPartyPeerStatus p;
    unsigned owner=local_seat ? local_seat:seat;
    if(!bound || InterlockedCompareExchange(&stopping,0,0) || !roster_exact() ||
        !SudekiMpLanPartyPeerStatusGet(cast_session,owner,&p) ||
        p.phase!=SUDEKIMP_LAN_PARTY_ACTIVE) return FALSE;
    /* Seat zero is the local authoritative controller, not a network peer.
     * The exact local roster grants admission; peer tokens only authorize
     * the three remote controllers. */
    if(!local_seat && !seat) return TRUE;
    if(!SudekiMpLanPartyLeaseActive(cast_session,&p.lease)) return FALSE;
    if(seat==local_seat) return TRUE;
    p.lease.seat=(uint8_t)seat;
    return SudekiMpLanPartyControlRetainedNativeThreadExact(&p.lease,casts[seat].actor);
}
static BOOL owner_witness(void *actor,uint8_t kind,uint64_t *session,uint8_t *type) {
    int i=actor_index(actor);
    if(i<0 || !session || !type || (kind!=1 && kind!=2) ||
        (local_seat && kind!=1) || !authority((unsigned)i)) return FALSE;
    *session=native_session; *type=SudekiMpLanPartyActorType((unsigned)i); return TRUE;
}
static BOOL native_skill_idle(void *actor) {
    SudekiMpCharacterSkillState s; void *task;
    if(!SudekiMpObserveCharacterSkill(actor,&s) || s.active || !memory(s.skill,0x78)) return FALSE;
    task=*(void **)((uint8_t *)s.skill+0x74);
    return !task || (memory(task,8) && !*(void **)task && *((uint32_t *)task+1));
}
static BOOL idle(void) {
    int spirit;
    if(!roster_exact() || !SudekiMpLanCastContextDrained() ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit) return FALSE;
    for(unsigned i=0;i<4;++i) {
        SudekiMpSpiritInstanceState s;
        if(!native_skill_idle(casts[i].actor) || casts[i].started ||
            (casts[i].instance.generation &&
             (!SudekiMpObserveSpiritInstance(&casts[i].instance,&s) || !s.idle))) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanPartyCastReady(void) { return bound && !InterlockedCompareExchange(&stopping,0,0); }
BOOL SudekiMpLanPartyCastActive(void *actor) {
    int i=actor_index(actor); BOOL spirit_active; SudekiMpCharacterSkillState skill;
    return bound && i>=0 && retained(actor,native_session) &&
        SudekiMpObserveSpiritInstanceActivity(&casts[i].instance,&spirit_active) &&
        SudekiMpObserveCharacterSkill(actor,&skill) && (spirit_active || skill.active || casts[i].started ||
            !SudekiMpLanCastContextActorDrained(actor,native_session));
}
BOOL SudekiMpLanPartyCastTargeting(void *actor) {
    int i=actor_index(actor);
    SudekiMpCharacterSkillState skill;
    SudekiMpLanPartyPeerStatus peer;
    uint8_t phase=0; uint16_t ms=0,sequence;
    if(!bound || i<0 || !casts[i].started || !authority((unsigned)i) ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || !skill.active ||
        skill.skill!=casts[i].skill) return FALSE;
    if((local_seat || i) &&
        (!SudekiMpLanPartyPeerStatusGet(cast_session,local_seat?local_seat:(unsigned)i,&peer) ||
         peer.lease.token!=casts[i].network_owner.token ||
         peer.lease.generation!=casts[i].network_owner.generation)) return FALSE;
    sequence=local_seat?casts[i].seen:casts[i].sequence;
    return sequence && SudekiMpObserveSpiritInstanceSkillTiming(
        &casts[i].instance,sequence,&phase,&ms) &&
        phase==SUDEKIMP_SKILL_TARGET_AIMING && ms>0;
}
BOOL SudekiMpLanPartyCastLocalTargeting(void) {
    return bound && SudekiMpLanPartyCastTargeting(casts[local_seat].actor);
}
BOOL SudekiMpLanPartyCastDrained(void *actor) {
    int i=actor_index(actor); SudekiMpSpiritInstanceState s;
    if(!image || !bound) return !initialized;
    return i>=0 && retained(actor,native_session) && !casts[i].started &&
        native_skill_idle(actor) && SudekiMpLanCastContextActorDrained(actor,native_session) &&
        SudekiMpObserveSpiritInstance(&casts[i].instance,&s) && s.idle;
}
BOOL SudekiMpLanPartyCastBodyIdle(void *actor) {
    int i=actor_index(actor); SudekiMpSpiritInstanceState s;
    if(!image || !bound) return !initialized;
    return i>=0 && retained(actor,native_session) && !casts[i].started &&
        native_skill_idle(actor) && SudekiMpLanCastContextActorDrained(actor,native_session) &&
        SudekiMpObserveSpiritInstance(&casts[i].instance,&s) && s.body_idle;
}
BOOL SudekiMpLanPartyCastNoncaster(void *actor) {
    int i=actor_index(actor);
    if(i<0 || !bound || !SudekiMpLanPartyCastDrained(actor)) return FALSE;
    for(unsigned j=0;j<4;++j) if(j!=(unsigned)i && SudekiMpLanPartyCastActive(casts[j].actor)) return TRUE;
    return FALSE;
}
BOOL SudekiMpLanPartyCastLocalNoncaster(void *actor) {
    return bound && actor==casts[local_seat].actor && SudekiMpLanPartyCastNoncaster(actor);
}
static BOOL ordinary_ready(unsigned i) {
    if(SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !authority(i) || !SudekiMpLanPartyCastDrained(casts[i].actor)) return FALSE;
    /* Preserve the established Spirit admission boundary. Ordinary CSkill
     * tasks may overlap each other only in their private native namespaces. */
    for(unsigned j=0;j<4;++j) {
        BOOL active;
        if(!SudekiMpObserveSpiritInstanceActivity(&casts[j].instance,&active) || active) return FALSE;
    }
    return TRUE;
}
static BOOL admit_generation(unsigned i) {
    SudekiMpLanPartyPeerStatus p; PartyCast *c=&casts[i];
    if(!local_seat && !i) return authority(i);
    if(!authority(i) || !SudekiMpLanPartyPeerStatusGet(cast_session,
        local_seat?local_seat:i,&p)) return FALSE;
    if(c->network_owner.token==p.lease.token &&
        c->network_owner.generation==p.lease.generation) return TRUE;
    if(!SudekiMpLanPartyCastDrained(c->actor) ||
        !SudekiMpRearmSpiritInstanceSkillTiming(&c->instance)) return FALSE;
    c->network_owner=p.lease; c->seen=c->prime_sequence=c->prepared=0;
    SudekiMpLanArenaClientSkillRetryReset(&c->retry);
    return TRUE;
}
static uint16_t next_sequence(unsigned i) {
    uint16_t next=(uint16_t)(casts[i].sequence+1u); return next ? next:1u;
}
static uint32_t skill_enter(void *skill,int slot,BOOL using_skill) {
    void *actor; int i; SudekiMpCharacterSkillState s;
    if(!memory(skill,0x78) || slot<0 || slot>=6) return 0;
    actor=*(void **)((uint8_t *)skill+0x10); i=actor_index(actor);
    if(i<0 || !authority((unsigned)i) || !SudekiMpObserveCharacterSkill(actor,&s) || s.skill!=skill) return 0;
    if(using_skill) {
        if(!ordinary_ready((unsigned)i) || !admit_generation((unsigned)i) || (local_seat &&
            (replay_entry.actor!=actor || replay_entry.slot!=slot || !replay_entry.sequence))) return 0;
        casts[i].prepared=local_seat ? replay_entry.sequence:next_sequence((unsigned)i);
        if(!SudekiMpBeginSpiritInstanceSkillTiming(&casts[i].instance,casts[i].prepared)) return 0;
    }
    return SudekiMpEnterSpiritInstance(&casts[i].instance);
}
/* The retail menu and number-key paths call CSkill::Use directly. Routing
 * just their validator leaves Use's input/UI, timing and camera work outside
 * the actor's namespace. Reuse the established two-seat Use boundary and ABI
 * without installing its controller/network adapter in SMP4. */
static uint8_t __attribute__((fastcall)) host_skill_use(void *skill,void *edx,int slot) {
    SudekiMpCharacterSkillState before,after;
    if(local_seat || !bound || !host_skill_use_original ||
        !authority(0u) || slot<0 || slot>=6 ||
        !SudekiMpObserveCharacterSkill(casts[0].actor,&before) ||
        before.skill!=skill || before.active) return 0;
    uint8_t result=SudekiMpInvokeSkillUse(skill,edx,slot,host_skill_use_original);
    DWORD error=GetLastError();
    if(result) {
        casts[0].started=TRUE; casts[0].active_seen=FALSE; casts[0].skill=skill;
        if(SudekiMpObserveCharacterSkill(casts[0].actor,&after) &&
            after.skill==skill && after.active) casts[0].active_seen=TRUE;
    }
    SudekiMpLogFormat("lan_party_cast event=host_native_use actor=0 slot=%d result=%u prepared=%u active_seen=%u tick=%lu\r\n",
        slot,result,casts[0].prepared,casts[0].active_seen,(unsigned long)GetTickCount());
    SetLastError(error);
    return result; /* Preserve a successful native start even on leave failure. */
}
static uint32_t spirit_enter(void *actor,int strike,BOOL activating,void **manager) {
    int i,first; uint32_t cookie;
    if(!actor) actor=casts[0].actor;
    i=actor_index(actor);
    if(local_seat || i<0 || !manager || !authority((unsigned)i) ||
        !SudekiMpResolveSpiritStrikeId(SudekiMpLanPartyActorType((unsigned)i),1,&first) ||
        (strike!=first && strike!=first+1) ||
        (activating && (!idle() || !admit_generation((unsigned)i)))) return 0;
    cookie=SudekiMpEnterSpiritInstance(&casts[i].instance);
    if(cookie) *manager=casts[i].instance.manager;
    return cookie;
}
static uint32_t task_enter(const SudekiMpLanCastOwner *owner) {
    int i;
    if(!owner) return SudekiMpEnterSpiritInstance(NULL);
    i=actor_index(owner->actor);
    if(i<0 || !owner->cast_id || (owner->kind!=1 && owner->kind!=2) ||
        (local_seat && owner->kind!=1) || !retained(owner->actor,owner->session)) return 0;
    return SudekiMpEnterSpiritInstance(&casts[i].instance);
}
int SudekiMpLanPartyCastRouteCamera(void *manager,const char *name) {
    SudekiMpSpiritInstance scope; unsigned kind;
    if(!bound) return 0;
    if(!SudekiMpObserveSpiritInstanceScope(&scope)) return -1;
    if(!scope.generation || scope.generation==casts[local_seat].instance.generation) return 2;
    for(unsigned i=0;i<4;++i) if(scope.generation==casts[i].instance.generation)
        return SudekiMpRouteSpiritInstanceRenderCamera(manager,name,&kind)==1 ? 1:-1;
    return -1;
}
static BOOL __attribute__((thiscall)) host_camera(void *manager,const char *name) {
    int result=SudekiMpLanPartyCastRouteCamera(manager,name);
    if(result==1) return TRUE;
    if(result<0) return FALSE;
    return ((SetCamera)host_camera_hook.trampoline)(manager,name);
}
static int motion_blur_scope(uint32_t *generation) {
    SudekiMpSpiritInstance scope;
    if(!generation || !initialized || GetCurrentThreadId()!=game_thread ||
        !SudekiMpObserveSpiritInstanceScope(&scope)) return SUDEKIMP_CAST_BLUR_UNKNOWN;
    *generation=scope.generation;
    if(!scope.generation) return SUDEKIMP_CAST_BLUR_NEUTRAL;
    for(unsigned i=0;i<4;++i) if(scope.generation==casts[i].instance.generation &&
        scope.manager==casts[i].instance.manager && scope.camera==casts[i].instance.camera &&
        retained(casts[i].actor,native_session))
        return i==local_seat ? SUDEKIMP_CAST_BLUR_LOCAL:SUDEKIMP_CAST_BLUR_REMOTE;
    return SUDEKIMP_CAST_BLUR_UNKNOWN;
}
/* SMP4 owns the native speed sources for its entire installed lifetime, not
 * just while a SetSpeedMode call is on the stack. CGameSpeed queries are also
 * inlined into task/component setup and divide the selected scale by normal
 * speed. Changing only the frame delta would leave those tasks at 0.07 speed.
 * Keep mode transitions (actor timing groups and skill presentation) native;
 * every source they select is unity. The separate full-pause path is intact. */
static void __cdecl normal_speed(float requested __attribute__((unused))) {
    *(float *)((uint8_t *)image+0x345f70)=1.f;
}
static void __cdecl master_speed(float requested __attribute__((unused))) {
    *(float *)((uint8_t *)image+0x325810)=1.f;
}
static void __attribute__((naked,noinline)) variable_speed(void) {
    /* Enter only AFTER the native (0,1] validation. ECX=this, ST0=request;
     * replace its accepted value, preserve x87 depth and the thiscall ret4. */
    __asm__ volatile("fstp %st(0)\n\tfld1\n\tfstps 0x2c(%ecx)\n\tret $4\n\t");
}
BOOL SudekiMpLanPartyCastRealtimeExact(HMODULE module) {
    SudekiMpInlineHook *hooks[]={&normal_speed_hook,&master_speed_hook,&variable_speed_hook};
    static const uint8_t unity[]={0,0,0x80,0x3f};
    if(!image || image!=module) return FALSE;
    for(unsigned i=0;i<3;++i)
        if(!hooks[i]->installed || memcmp(hooks[i]->target,hooks[i]->replacement,hooks[i]->length)) return FALSE;
    for(unsigned i=0;i<4;++i)
        if(!alternate_speed_patches[i].installed ||
            alternate_speed_patches[i].target!=(uint8_t *)image+0x2c4018+i ||
            *alternate_speed_patches[i].target!=unity[i]) return FALSE;
    return !memcmp((uint8_t *)image+0x345f70,unity,4) &&
        !memcmp((uint8_t *)image+0x325810,unity,4);
}
static BOOL install_realtime(void) {
    uint8_t *base=(uint8_t *)image;
    uint8_t normal[]={0xd9,0x44,0x24,0x04,0xd9,0x1d,0,0,0,0};
    uint8_t master[sizeof(normal)];
    static const uint8_t variable[]={
        0xd9,0xee,0xd9,0x44,0x24,0x04,0xd8,0xd1,0xdf,0xe0,0xdd,0xd9,
        0xf6,0xc4,0x41,0x75,0x11,0xd9,0xe8,0xd8,0xd9,0xdf,0xe0,
        0xf6,0xc4,0x01,0x75,0x06,0xd9,0x59,0x2c,0xc2,0x04,0x00,
        0xdd,0xd8,0xc2,0x04,0x00};
    static const uint8_t alternate[]={0x29,0x5c,0x8f,0x3d},unity[]={0,0,0x80,0x3f};
    uintptr_t address=(uintptr_t)(base+0x345f70);
    memcpy(normal+6,&address,4); memcpy(master,normal,sizeof(master));
    address=(uintptr_t)(base+0x325810); memcpy(master+6,&address,4);
    /* Exact image and ASLR operands, including all source values, before
     * the first write. Other native normal-speed stores initialize 1.0;
     * the variable constructor/reset reads this same alternate constant. */
    if(!memory(base+0x27040,sizeof(normal)) || memcmp(base+0x27040,normal,sizeof(normal)) ||
        !memory(base+0x28be90,sizeof(master)) || memcmp(base+0x28be90,master,sizeof(master)) ||
        !memory(base+0x27510,sizeof(variable)) || memcmp(base+0x27510,variable,sizeof(variable)) ||
        !memory(base+0x2c4018,4) || memcmp(base+0x2c4018,alternate,4) ||
        !memory(base+0x345f70,4) || memcmp(base+0x345f70,unity,4) ||
        !memory(base+0x325810,4) || memcmp(base+0x325810,unity,4)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if(!SudekiMpInstallInlineHook(&normal_speed_hook,base+0x27040,normal,sizeof(normal),normal_speed) ||
        !SudekiMpInstallInlineHook(&master_speed_hook,base+0x28be90,master,sizeof(master),master_speed) ||
        !SudekiMpInstallInlineHook(&variable_speed_hook,base+0x2752c,variable+28,6,variable_speed)) return FALSE;
    for(unsigned i=0;i<4;++i)
        if(!SudekiMpInstallBytePatch(&alternate_speed_patches[i],base+0x2c4018+i,alternate[i],unity[i])) return FALSE;
    return SudekiMpLanPartyCastRealtimeExact(image);
}
static BOOL restore_realtime(void) {
    BOOL restored=TRUE; DWORD error=0;
    for(unsigned i=4;i>0;--i)
        if(!SudekiMpRestoreBytePatch(&alternate_speed_patches[i-1])) {
            restored=FALSE; if(!error) error=GetLastError();
        }
    SudekiMpInlineHook *hooks[]={&variable_speed_hook,&master_speed_hook,&normal_speed_hook};
    for(unsigned i=0;i<3;++i)
        if(!SudekiMpRestoreInlineHook(hooks[i])) {
            restored=FALSE; if(!error) error=GetLastError();
        }
    if(!restored) SetLastError(error?error:ERROR_BUSY);
    return restored;
}
BOOL SudekiMpLanPartyCastLocalCameraActive(void) {
    return bound && SudekiMpLanPartyCastActive(casts[local_seat].actor);
}
BOOL SudekiMpLanPartyCastLight(float rgb[3]) {
    float baseline[3];
    return bound && rgb && retained(casts[local_seat].actor,native_session) &&
        SudekiMpReadCastLight(casts[local_seat].instance.generation,rgb,baseline);
}
BOOL SudekiMpLanPartyCastInstall(HMODULE module,SudekiMpLanPartySession *session) {
    uint8_t *base=(uint8_t *)module;
    static const uint8_t request[]={0xc7,0x40,0x24,1,0,0,0};
    if(!module || !session || image) return FALSE;
    image=module; cast_session=session; local_seat=SudekiMpLanPartyLocalSeat(session);
    install_failed=FALSE;
    bind_retry_at=0;
    InterlockedExchange(&stopping,0);
    /* Quick Menu writes mode 1 directly, bypassing SetSpeedMode. Only this
     * proven menu request becomes mode 0; skill mode/input transitions remain
     * native. Retain a reversible owner for the exact immediate byte. */
    if(local_seat>=4 || !memory(base+0x98ee5,18) || base[0x98ee5]!=0xa1 ||
        *(void **)(base+0x98ee6)!=base+0x408da0 ||
        base[0x98eea]!=0x8b || base[0x98eeb]!=0x35 ||
        *(void **)(base+0x98eec)!=base+0x408d1c ||
        memcmp(base+0x98ef0,request,sizeof(request)) ||
        !SudekiMpInstallBytePatch(&quick_menu_speed_patch,base+0x98ef3,1,0) ||
        !install_realtime()) {
        DWORD error=GetLastError();
        if(!error) error=ERROR_INVALID_DATA;
        install_failed=TRUE;
        (void)SudekiMpLanPartyCastUninstall(); SetLastError(error); return FALSE;
    }
    SudekiMpLogFormat("lan_party_cast event=realtime_installed seat=%u normal=1 alternate=1 variable=1 master=1 menu_request=0 policy=all_native_speed_sources_modes_preserved\r\n",local_seat);
    return TRUE;
}
static BOOL bind_casts(const SudekiMpControlUpdateDispatchWitness *w) {
    SudekiMpLanPartyPeerStatus p;
    const char *stage="input_isolation"; DWORD error; unsigned binding_seat=0;
    if(!SudekiMpLanPartyControlObserveRoster(w,&bound_roster) || bound_roster.present_mask!=15 ||
        !SudekiMpLanPartyPeerStatusGet(cast_session,local_seat,&p) ||
        p.phase!=SUDEKIMP_LAN_PARTY_ACTIVE) return TRUE;
    game_thread=GetCurrentThreadId();
    for(unsigned i=0;i<4;++i) casts[i].actor=bound_roster.actors[i];
    if(!idle()) return TRUE;
    /* This process-local native lifetime survives transport reconnect and
     * grants no network authority. The listen host has no peer token. */
    if(native_lifetime_serial==UINT64_MAX) return FALSE;
    native_session=++native_lifetime_serial;
    initializing=TRUE;
    if(!SudekiMpLanPartyControlEnableCastInputIsolation(w)) goto failed;
    stage="task_context";
    if(!context_installed) {
        if(!SudekiMpInstallLanCastContext(image,owner_witness)) goto failed;
        context_installed=TRUE;
    }
    stage="instance_abi";
    if(!(local_seat ? SudekiMpInitializeSpiritInstanceAbiWithInputOwner(image,idle,
        SudekiMpLanArenaClientCharacterInputOwnerExact):SudekiMpInitializeSpiritInstanceAbi(image,idle))) goto failed;
    initialized=TRUE;
    stage="instance_updates";
    if(!SudekiMpInstallSpiritInstanceUpdates() || !SudekiMpEnableSpiritInstanceNamedCameraBanking()) goto failed;
    stage="construct_owners";
    for(unsigned i=0;i<4;++i) {
        binding_seat=i; stage="construct_owner";
        if(!SudekiMpCreateSpiritInstance(&casts[i].instance)) goto failed;
        stage="bind_owner";
        if(!SudekiMpBindSpiritInstanceCaster(&casts[i].instance,casts[i].actor,
                SudekiMpLanPartyActorType(i),native_session,retained)) goto failed;
    }
    stage="owner_cameras_ui_input";
    for(unsigned i=0;i<4;++i) {
        binding_seat=i;
        if(i!=local_seat && (!SudekiMpEnableSpiritInstanceRemoteUi(&casts[i].instance) ||
            !SudekiMpEnableSpiritInstanceRemoteSkillUi(&casts[i].instance) ||
            !SudekiMpEnableSpiritInstanceRemoteSkillInput(&casts[i].instance,casts[local_seat].actor,retained))) goto failed;
        if(!SudekiMpEnableSpiritInstanceNamedCameras(&casts[i].instance)) goto failed;
    }
    stage="camera_callbacks";
    if(!SudekiMpInstallSpiritInstanceNamedCameraUpdates()) goto failed;
    for(unsigned i=0;i<4;++i) if(i!=local_seat &&
        !SudekiMpEnableSpiritInstanceRemoteCameraSelection(&casts[i].instance)) goto failed;
    stage="cast_gates_light_targeting";
    if(!SudekiMpEnableSpiritInstanceCastGates() || !SudekiMpEnableSpiritInstanceLighting() ||
        !SudekiMpEnableSpiritInstanceSkillTargeting() ||
        (!local_seat && !SudekiMpEnableSpiritInstanceSharedSsp())) goto failed;
    stage="timing_schedule";
    for(unsigned i=0;i<4;++i) {
        if(!SudekiMpConfigureSpiritInstanceSkillTiming(&casts[i].instance,local_seat!=0)) goto failed;
        if(!local_seat && !SudekiMpScheduleSpiritInstanceManager(&casts[i].instance)) goto failed;
    }
    stage="caster_motion_blur";
    if(!SudekiMpInstallCastMotionBlur(image,motion_blur_scope)) goto failed;
    stage="host_adapters";
    if(!local_seat) {
        static const uint8_t camera_bytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
        uint8_t *base=(uint8_t *)image;
        if(!SudekiMpLanPartyCastRealtimeExact(image) ||
            !SudekiMpInstallInlineHook(&host_camera_hook,base+0x36fb0,camera_bytes,sizeof(camera_bytes),host_camera)) goto failed;
        if(!SudekiMpQuickSkillSpiritRoutingReady()) {
            if(!SudekiMpInstallQuickSkillInputTrace(image,FALSE,FALSE)) goto failed;
            menu_owned=TRUE;
        }
        if(!SudekiMpSetSpiritActivationRouting(idle,spirit_enter,SudekiMpLeaveSpiritInstance)) goto failed;
        spirit_routed=TRUE;
    }
    stage="skill_routing";
    if(!SudekiMpSetSkillActivationRouting(idle,skill_enter,SudekiMpLeaveSpiritInstance)) goto failed;
    skill_routed=TRUE;
    if(!local_seat) {
        static const uint32_t use_sites[]={0x27cb1,0x998a1};
        stage="host_native_skill_use";
        host_skill_use_original=(SudekiMpSkillUseFunction)((uint8_t *)image+0xb4810);
        for(unsigned i=0;i<2u;++i)
            if(!SudekiMpInstallRelativeCallHook(&host_skill_use_hooks[i],
                (uint8_t *)image+use_sites[i],host_skill_use_original,host_skill_use)) goto failed;
    }
    stage="task_routing";
    if(!SudekiMpLanCastContextSetTaskRouting(task_enter,SudekiMpLeaveSpiritInstance)) goto failed;
    task_routed=TRUE; bound=TRUE; initializing=FALSE;
    for(unsigned i=0;i<4;++i) {
        SudekiMpLanPartyPeerStatus peer;
        if(SudekiMpLanPartyPeerStatusGet(cast_session,local_seat?local_seat:i,&peer) &&
            peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE) casts[i].network_owner=peer.lease;
    }
    SudekiMpLogFormat("lan_party_cast event=bound seat=%u owners=4 named_slots=2 policy=host_authority_private_native_lifetimes\r\n",local_seat);
    return TRUE;
failed:
    error=GetLastError();
    if(!error) error=ERROR_INVALID_DATA;
    HMODULE retry_image=image;
    SudekiMpLanPartySession *retry_session=cast_session;
    install_failed=TRUE;
    SudekiMpLanPartyCastRequestStop();
    BOOL restored=SudekiMpLanPartyCastUninstall();
    SudekiMpLogFormat("lan_party_cast event=bind_failed stage=%s owner=%u error=%lu retained=%u policy=admission_closed\r\n",
        stage,binding_seat,(unsigned long)error,!restored);
    /* A native busy boundary is not a permanent lack of skill support. Only
     * a fully restored transaction may retry; retained hooks/objects and
     * unknown image/ownership failures stay closed. Reinstall the normal-time
     * adapters before returning to gameplay, then retry on a later ready-roster
     * boundary. No task is cancelled or declared drained by this deadline. */
    if(restored && SudekiMpLanPartyCastInstall(retry_image,retry_session)) {
        if(error==ERROR_BUSY) {
            bind_retry_at=GetTickCount()+1000u;
            SudekiMpLogFormat("lan_party_cast event=bind_retry local=%u error=%lu policy=full_restore_then_ready_idle_boundary\r\n",
                local_seat,(unsigned long)error);
            SetLastError(error);
            return TRUE;
        }
        /* Keep the profile's normal-time policy even when skill admission
         * failed permanently. Do not retry an unknown native failure. */
        install_failed=TRUE;
    }
    SetLastError(error);
    return FALSE;
}

BOOL SudekiMpLanPartyCastTryBind(const SudekiMpControlUpdateDispatchWitness *w) {
    if(!image || install_failed || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        initializing || InterlockedCompareExchange(&stopping,0,0)) return FALSE;
    if(bound || (bind_retry_at && (int32_t)(GetTickCount()-bind_retry_at)<0)) return TRUE;
    return bind_casts(w);
}

BOOL SudekiMpLanPartyCastService(const SudekiMpControlUpdateDispatchWitness *w) {
    DWORD now=GetTickCount();
    if(!image) return !install_failed;
    if(!w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    if(!bound) {
        if(install_failed || initializing || InterlockedCompareExchange(&stopping,0,0)) return FALSE;
        /* Actor presence precedes inventory/component setup. The runtime
         * calls TryBind only after its roster coordinator finishes startup. */
        return TRUE;
    }
    unsigned int fault_site=SudekiMpSpiritInstanceFaultSite();
    if(fault_site && fault_site!=reported_instance_fault) {
        reported_instance_fault=fault_site;
        SudekiMpLogFormat("lan_party_cast event=instance_fault local=%u source=spirit_instance_abi.c line=%u tick=%lu policy=retain_native_owners\r\n",
            local_seat,fault_site,(unsigned long)now);
    }
    if(!roster_exact() || !SudekiMpLanCastContextPoll()) return FALSE;
    float seconds=last_service && now-last_service<=100u ? (float)(now-last_service)/1000.0f:0.0f;
    last_service=now;
    BOOL trace=!last_diagnostic || now-last_diagnostic>=1000u;
    if(trace) last_diagnostic=now;
    for(unsigned i=0;i<4;++i) {
        PartyCast *c=&casts[i]; SudekiMpCharacterSkillState s;
        if(!SudekiMpObserveCharacterSkill(c->actor,&s)) return FALSE;
        /* admit_generation performs the complete authority check on every
         * branch before changing a generation. Do not repeat that same
         * roster lookup immediately before entering it. */
        BOOL admitted=admit_generation(i);
        /* A newer transport lease cannot truncate an old native cast. Keep
         * its timer/task drain progressing while fresh admission stays shut. */
        if(!admitted && (c->started || s.active) &&
            !SudekiMpDrainSpiritInstanceSkillTiming(&c->instance)) return FALSE;
        if(!SudekiMpAdvanceSpiritInstanceSkillTiming(&c->instance,seconds)) return FALSE;
        if(c->started) {
            if(s.skill!=c->skill) return FALSE;
            if(s.active) c->active_seen=TRUE;
            if(c->active_seen && !s.active && native_skill_idle(c->actor) &&
                SudekiMpLanCastContextActorDrained(c->actor,native_session)) {
                SudekiMpLogFormat("lan_party_cast event=drained local=%u actor=%u sequence=%u tick=%lu\r\n",
                    local_seat,i,local_seat?c->seen:c->sequence,(unsigned long)now);
                c->started=FALSE; c->active_seen=FALSE; c->skill=NULL;
            }
        }
        if(trace) {
            uint8_t phase=0; uint16_t remaining=0;
            SudekiMpSpiritInstanceState native={0};
            BOOL native_known=SudekiMpObserveSpiritInstance(&c->instance,&native);
            uint16_t sequence=local_seat?c->seen:c->sequence;
            BOOL timing=sequence && SudekiMpObserveSpiritInstanceSkillTiming(&c->instance,sequence,&phase,&remaining);
            BOOL task_known=memory(s.skill,0x78);
            void *task=task_known?*(void **)((uint8_t *)s.skill+0x74):NULL;
            if(task && !memory(task,8)) task_known=FALSE;
            SudekiMpLogFormat("lan_party_cast_diag local=%u actor=%u tick=%lu native_active=%u started=%u active_seen=%u slot=%d sequence=%u admitted=%u task_known=%u task_live=%u timing_known=%u target_phase=%u target_ms=%u advance_ms=%lu instance_known=%u body_idle=%u full_idle=%u\r\n",
                local_seat,i,(unsigned long)now,s.active,c->started,c->active_seen,s.slot,sequence,admitted,
                task_known,task_known && task && *(void **)task!=NULL,timing,phase,remaining,
                (unsigned long)(seconds*1000.0f),native_known,
                native_known && native.body_idle,native_known && native.idle);
        }
    }
    if(InterlockedCompareExchange(&stopping,0,0) && !callbacks_pinned && idle())
        return SudekiMpLanPartyCastUninstall();
    return TRUE;
}
BOOL SudekiMpLanPartyCastSubmit(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,void *actor,BOOL spirit,unsigned slot) {
    if(local_seat || !lease || lease->seat==0 || lease->seat>=4 ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyLeaseActive(cast_session,lease) ||
        !SudekiMpLanPartyControlExact(w,lease,actor) || actor!=casts[lease->seat].actor ||
        !ordinary_ready(lease->seat) || !admit_generation(lease->seat)) return FALSE;
    if(spirit) {
        /* Remote Spirit needs the established two-phase native UI prime,
         * followed by a fresh lease/readiness proof, plus complete replicated
         * view/VFX ownership. Keep this admission closed until those paths
         * are connected; direct activation after validation can strand the
         * native Spirit state. Ordinary CSkill does not need that intent. */
        return FALSE;
    }
    if(slot>=6) return FALSE;
    SudekiMpSkillActivationResult result=SudekiMpActivateCharacterSkillSlot(actor,(int)slot);
    if(result.status==SUDEKIMP_SKILL_ACTIVATION_STARTED) {
        SudekiMpCharacterSkillState state;
        casts[lease->seat].started=TRUE; casts[lease->seat].active_seen=FALSE;
        casts[lease->seat].skill=result.skill;
        if(SudekiMpObserveCharacterSkill(actor,&state) && state.skill==result.skill && state.active)
            casts[lease->seat].active_seen=TRUE;
    }
    return result.status==SUDEKIMP_SKILL_ACTIVATION_STARTED;
}
BOOL SudekiMpLanPartyCastCapture(unsigned seat,SudekiMpLanArenaActorSnapshot *out) {
    PartyCast *c; SudekiMpCharacterSkillState skill; SudekiMpSpiritInstanceState spirit;
    SudekiMpCleanroomActor kind; SudekiMpCleanroomActorPresentation pose;
    void *task; BOOL active; uint8_t cast_kind,slot; uint32_t cost; int first;
    if(!out || seat>=4 || !bound || local_seat || !retained(casts[seat].actor,native_session)) return FALSE;
    c=&casts[seat];
    if(!SudekiMpObserveCharacterSkill(c->actor,&skill) ||
        !SudekiMpObserveSpiritInstance(&c->instance,&spirit) || !memory(skill.skill,0x78) ||
        (skill.active && spirit.state)) return FALSE;
    if(c->started && skill.skill==c->skill && skill.active) c->active_seen=TRUE;
    active=skill.active || spirit.state; cast_kind=c->kind; slot=c->slot; cost=c->cost;
    task=*(void **)((uint8_t *)skill.skill+0x74);
    if(active) {
        cast_kind=skill.active ? SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER:SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
        if(skill.active) { slot=(uint8_t)skill.slot; cost=skill.cost; }
        else {
            if(!SudekiMpResolveSpiritStrikeId(SudekiMpLanPartyActorType(seat),1,&first) ||
                spirit.strike_id<(uint32_t)first || spirit.strike_id>(uint32_t)(first+1)) return FALSE;
            slot=(uint8_t)(spirit.strike_id-first); cost=0;
        }
        if(!c->active || c->kind!=cast_kind || c->slot!=slot ||
            (skill.active && (c->task!=task || (c->prepared && c->prepared!=c->sequence)))) {
            c->sequence=skill.active && c->prepared ? c->prepared:next_sequence(seat);
            c->prepared=0;
        }
        c->task=task; c->kind=cast_kind; c->slot=slot; c->cost=cost;
        if(!SudekiMpCleanroomActorFromType(SudekiMpLanPartyActorType(seat),&kind) ||
            !SudekiMpCleanroomEngineWorldMotion(kind,&pose)) return FALSE;
        if(seat==1u || seat==3u) {
            SudekiMpCleanroomActorPresentation firing;
            if(!SudekiMpCleanroomEngineRangedWorldPresentation(kind,c->actor,&firing)) return FALSE;
            pose.selector[4]=firing.selector[4]; pose.state[4]=firing.state[4];
            pose.rate[4]=firing.rate[4]; pose.time[4]=firing.time[4]; pose.blend[3]=firing.blend[3];
        }
        out->skill_presentation_valid=1;
        out->skill_presentation_channel_count=(seat==1 || seat==3) ? 5:seat==0 ? 4:2;
        for(unsigned j=0;j<out->skill_presentation_channel_count;++j) {
            out->skill_presentation_selector[j]=pose.selector[j];
            out->skill_presentation_state[j]=pose.state[j];
            out->skill_presentation_rate[j]=pose.rate[j];
            out->skill_presentation_time[j]=pose.time[j];
        }
        unsigned blends=seat==0 || seat==2 ? 3:4;
        for(unsigned j=0;j<blends;++j) out->skill_presentation_blend[j]=pose.blend[j];
    }
    c->active=active;
    if(c->sequence) {
        out->skill_sequence=c->sequence; out->skill_kind=c->kind;
        out->skill_slot=c->kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT ? 0:c->slot;
        out->skill_cost=c->cost; out->skill_active=active;
        if(active && skill.active && !SudekiMpObserveSpiritInstanceSkillTiming(&c->instance,c->sequence,
            &out->skill_target_phase,&out->skill_target_remaining_ms)) return FALSE;
        /* Preserve the terminal targeting edge after the active frame. A
         * replica may miss the last active packet while the host completes. */
        if(!active && c->kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
            native_skill_idle(c->actor) &&
            SudekiMpLanCastContextActorDrained(c->actor,native_session))
            out->skill_target_phase=SUDEKIMP_SKILL_TARGET_RELEASED;
    }
    return SudekiMpLanArenaSkillPresentationValid(out,SudekiMpLanPartyActorType(seat));
}
BOOL SudekiMpLanPartyCastNativeBody(void *actor) {
    int i=actor_index(actor); return bound && local_seat && i>=0 && casts[i].started;
}
BOOL SudekiMpLanPartyCastCallbacksRetained(void) { return callbacks_pinned || replay_entry.actor!=NULL; }
BOOL SudekiMpLanPartyCastReplayTiming(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame) {
    BOOL combat;
    if(!bound || !local_seat || !lease || lease->seat!=local_seat || !frame ||
        !SudekiMpLanPartyLeaseActive(cast_session,lease) ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) || !roster_exact()) return FALSE;
    combat=frame->chunk[0].combat_enabled!=0;
    if(!(combat ? SudekiMpLanPartyBasicCombatFrameValid(frame):
        SudekiMpLanPartyMovementFrameValid(frame))) return FALSE;
    for(unsigned i=0;i<4;++i) {
        PartyCast *c=&casts[i];
        const SudekiMpLanArenaActorSnapshot *s=&frame->chunk[i/2].seat[i%2];
        if(!c->started) continue;
        if(!authority(i) || c->network_owner.token!=lease->token ||
            c->network_owner.generation!=lease->generation) return FALSE;
        if(!combat) {
            /* This profile publishes movement only after every host cast
             * positively drains. Release our targeting predicate, not the
             * retained task or renderer; its remaining script still runs. */
            if(!SudekiMpApplySpiritInstanceSkillTiming(&c->instance,c->seen,
                SUDEKIMP_SKILL_TARGET_RELEASED,0)) return FALSE;
        } else if(s->skill_kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
            s->skill_sequence==c->seen && s->skill_target_phase &&
            !SudekiMpApplySpiritInstanceSkillTiming(&c->instance,s->skill_sequence,
                s->skill_target_phase,s->skill_target_remaining_ms)) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanPartyCastReplay(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame) {
    if(!bound || !local_seat || !lease || lease->seat!=local_seat ||
        !SudekiMpLanPartyLeaseActive(cast_session,lease) ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyBasicCombatFrameValid(frame)) return FALSE;
    for(unsigned i=0;i<4;++i) {
        const SudekiMpLanArenaActorSnapshot *s=&frame->chunk[i/2].seat[i%2]; PartyCast *c=&casts[i];
        SudekiMpCleanroomActor kind;
        if(!admit_generation(i)) return FALSE;
        if(c->started) continue;
        if(s->skill_kind!=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER ||
            !s->skill_active || s->skill_sequence==c->seen ||
            !s->skill_target_phase) continue;
        /* Pose-only fallback frames contain no targeting lifecycle. Starting
         * a native replica from one strands its task at the targeting gate. */
        if(!ordinary_ready(i) || SudekiMpLanArenaClientSkillRetryDecide(&c->retry,
            s->skill_sequence,s->skill_slot,GetTickCount(),FALSE)!=SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT) continue;
        HMODULE pinned;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)SudekiMpLanPartyCastReplay,&pinned) ||
            !SudekiMpCleanroomActorFromType(s->actor_type,&kind)) return FALSE;
        /* CSkill children can emit native projectiles beyond root retirement.
         * No terminal witness exists for every resource: retain the installed
         * client containment/module after successful submission. */
        uint32_t sp=s->sp+s->skill_cost;
        if(sp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE) sp=SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE;
        if(!SudekiMpCleanroomEngineSetActorResources(kind,s->hp,(float)sp)) return FALSE;
        replay_entry.actor=c->actor; replay_entry.sequence=s->skill_sequence; replay_entry.slot=s->skill_slot;
        SudekiMpSkillActivationResult result=SudekiMpReplayHostApprovedCharacterSkillSlot(c->actor,s->skill_slot);
        memset(&replay_entry,0,sizeof(replay_entry));
        if(result.status==SUDEKIMP_SKILL_ACTIVATION_STARTED) {
            SudekiMpCharacterSkillState state;
            callbacks_pinned=TRUE; c->started=TRUE; c->active_seen=FALSE;
            c->skill=result.skill; c->seen=s->skill_sequence;
            if(SudekiMpObserveCharacterSkill(c->actor,&state) && state.skill==result.skill && state.active)
                c->active_seen=TRUE;
        }
        if(!SudekiMpCleanroomEngineSetActorResources(kind,s->hp,s->sp)) return FALSE;
        if(result.status!=SUDEKIMP_SKILL_ACTIVATION_STARTED && i==local_seat &&
            (i==1u || i==3u) && c->prime_sequence!=s->skill_sequence &&
            (result.validation_result==2 || result.validation_result==3) &&
            (result.validation_result!=3 || SudekiMpCleanroomEngineRefreshCombatMode()) &&
            SudekiMpCleanroomEnginePrimeRangedCombat()) c->prime_sequence=s->skill_sequence;
        if(c->started && s->skill_target_phase && !SudekiMpApplySpiritInstanceSkillTiming(&c->instance,
            s->skill_sequence,s->skill_target_phase,s->skill_target_remaining_ms)) return FALSE;
        SudekiMpLogFormat("lan_party_cast event=replay local=%u seat=%u sequence=%u slot=%u status=%u validation=%d tick=%lu host_tick=%lu target_phase=%u target_ms=%u\r\n",
            local_seat,i,s->skill_sequence,s->skill_slot,result.status,result.validation_result,
            (unsigned long)GetTickCount(),(unsigned long)frame->chunk[i/2].host_tick,
            s->skill_target_phase,s->skill_target_remaining_ms);
    }
    return TRUE;
}
void SudekiMpLanPartyCastRequestStop(void) { InterlockedExchange(&stopping,1); }
BOOL SudekiMpLanPartyCastUninstall(void) {
    BOOL restored=TRUE; DWORD error=0;
    if(!image) return TRUE;
    SudekiMpLanPartyCastRequestStop();
    if(callbacks_pinned || (initialized && !idle())) { SetLastError(ERROR_BUSY); return FALSE; }
    /* Close the two native entry callers before dropping their routing or
     * actor dependencies. Failed restoration keeps everything reachable. */
    for(unsigned i=2u;i>0;--i)
        if(!SudekiMpRestoreRelativeCallHook(&host_skill_use_hooks[i-1u])) {
            restored=FALSE; if(!error) error=GetLastError();
        }
    if(!restored) { SetLastError(error?error:ERROR_BUSY); return FALSE; }
    host_skill_use_original=NULL;
    /* This adapter can query the native namespace from its setter callback.
     * Restore it before releasing routing, instances, or retained identities. */
    if(!SudekiMpUninstallCastMotionBlur()) return FALSE;
    if(task_routed) {
        if(SudekiMpLanCastContextSetTaskRouting(NULL,NULL)) task_routed=FALSE;
        else { restored=FALSE; error=GetLastError(); }
    }
    if(skill_routed) {
        if(SudekiMpSetSkillActivationRouting(NULL,NULL,NULL)) skill_routed=FALSE;
        else { restored=FALSE; if(!error) error=GetLastError(); }
    }
    if(spirit_routed) {
        if(SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL)) spirit_routed=FALSE;
        else { restored=FALSE; if(!error) error=GetLastError(); }
    }
    if(menu_owned) {
        if(SudekiMpUninstallQuickSkillInputTrace()) menu_owned=FALSE;
        else { restored=FALSE; if(!error) error=GetLastError(); }
    }
    if(!SudekiMpRestoreInlineHook(&host_camera_hook)) { restored=FALSE; if(!error) error=GetLastError(); }
    if(!restored) { SetLastError(error?error:ERROR_BUSY); return FALSE; }
    /* Keep all native namespace/callback dependencies until every routed
     * entry has been restored. Each object destructor retains failed work. */
    for(unsigned i=4;i>0;--i) if(casts[i-1].instance.generation &&
        !SudekiMpDestroySpiritInstance(&casts[i-1].instance)) {
        restored=FALSE; if(!error) error=GetLastError();
    }
    if(!restored) { SetLastError(error?error:ERROR_BUSY); return FALSE; }
    if(initialized && !SudekiMpResetSpiritInstanceAbi()) return FALSE;
    initialized=FALSE;
    if(context_installed && !SudekiMpUninstallLanCastContext()) return FALSE;
    if(!restore_realtime()) { restored=FALSE; error=GetLastError(); }
    if(!SudekiMpRestoreBytePatch(&quick_menu_speed_patch)) { restored=FALSE; if(!error) error=GetLastError(); }
    if(!restored) { SetLastError(error?error:ERROR_BUSY); return FALSE; }
    context_installed=FALSE; bound=FALSE; initializing=FALSE;
    memset(casts,0,sizeof(casts)); memset(&bound_roster,0,sizeof(bound_roster));
    image=NULL; cast_session=NULL; native_session=0; game_thread=last_service=bind_retry_at=0;
    return TRUE;
}
