#include "hooks/lan_story_cast.h"
#include "hooks/lan_story_realtime.h"
#include "hooks/lan_story_task_trace.h"
#include "hooks/lan_party_control.h"
#include "hooks/lan_arena_cast_context.h"
#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/lan_party_projectile_lifetime.h"
#include "hooks/quick_skill_input.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/skill_activation_abi.h"
#include "engine/spirit_activation_abi.h"
#include "engine/spirit_instance_abi.h"
#include "engine/cast_motion_blur_abi.h"
#include "engine/log.h"
#include "cleanroom/engine.h"
#include <stdint.h>
#include <string.h>

typedef struct StoryCaster {
    SudekiMpSpiritInstance instance;
    void *actor,*skill;
    uint16_t sequence;
    BOOL started,active_seen;
} StoryCaster;
static HMODULE image;
static SudekiMpLanStoryNativeRoster owners;
static StoryCaster caster[4];
static uint64_t lifetime,lifetime_serial;
static DWORD native_thread,last_service;
static BOOL bound,initializing,initialized,context_owned,menu_owned,skill_routed,spirit_routed,task_routed;
static BOOL effects_owned,missiles_owned,stopping,fault;
static int submitting=-1;
static SudekiMpRelativeCallHook use_hooks[2];
static SudekiMpInlineHook camera_hook;
static SudekiMpSkillUseFunction original_use;
typedef BOOL (__attribute__((thiscall)) *SetCamera)(void *,const char *);
static BOOL memory(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static int index_of(void *actor) {
    for(unsigned i=0;i<4;++i) if(actor && caster[i].actor==actor) return (int)i;
    return -1;
}
static BOOL exact(void) {
    return image && native_thread==GetCurrentThreadId() &&
        SudekiMpLanStoryObserverNativeRosterExact(&owners);
}
static BOOL retained_actor(void *actor,uint64_t generation) {
    int i=index_of(actor);
    return lifetime && generation==lifetime && i>=0 && exact() && owners.actors[i]==actor;
}
static BOOL native_idle(void *actor) {
    SudekiMpCharacterSkillState s;
    if(!SudekiMpObserveCharacterSkill(actor,&s) || s.active || !memory(s.skill,0x78u) ||
        ((uint8_t *)s.skill)[0x6cu]) return FALSE;
    void *task=*(void **)((uint8_t *)s.skill+0x74u);
    return !task || (memory(task,8u) && !*(void **)task && *((uint32_t *)task+1));
}
static BOOL all_idle(void) {
    int spirit=-1;
    if(!exact() || submitting>=0 || !SudekiMpLanCastContextDrained() ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit) return FALSE;
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        SudekiMpSpiritInstanceState s;
        if(caster[i].started || !native_idle(caster[i].actor) ||
            (caster[i].instance.generation &&
             (!SudekiMpObserveSpiritInstance(&caster[i].instance,&s) || !s.idle))) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryCastDrained(void *actor) {
    int i=index_of(actor); SudekiMpSpiritInstanceState s;
    if(!initialized) return !SudekiMpLanStoryCastRetains();
    return i>=0 && retained_actor(actor,lifetime) && !caster[i].started && native_idle(actor) &&
        SudekiMpLanCastContextActorDrained(actor,lifetime) &&
        SudekiMpObserveSpiritInstance(&caster[i].instance,&s) && s.idle;
}
static BOOL authority(unsigned i) {
    /* Remote admission exists only around a freshly fenced Submit invocation.
     * Descendant tasks use retained_actor, not this short-lived authority. */
    return bound && !stopping && !fault && i<4u && exact() && caster[i].actor &&
        (i==owners.leader_character || (int)i==submitting);
}
static BOOL ready(unsigned i) {
    if(!authority(i) || !SudekiMpLanStoryCastDrained(caster[i].actor) ||
        SudekiMpCleanroomEngineRangedCombatPrimePending()) return FALSE;
    for(unsigned j=0;j<4;++j) if(caster[j].actor) {
        BOOL active;
        if(!SudekiMpObserveSpiritInstanceActivity(&caster[j].instance,&active) || active) return FALSE;
    }
    return TRUE;
}
static BOOL root_owner(void *actor,uint8_t kind,uint64_t *session,uint8_t *type) {
    int i=index_of(actor);
    if(i<0 || !session || !type || (kind!=1u && kind!=2u) || !authority((unsigned)i) ||
        (kind==2u && (unsigned)i!=owners.leader_character)) return FALSE;
    *session=lifetime; *type=SudekiMpLanPartyActorType((unsigned)i); return TRUE;
}
static uint32_t enter_task(const SudekiMpLanCastOwner *owner);
static BOOL start_task_routing(void) {
    if(task_routed) return TRUE;
    /* Enable BEFORE entering any private namespace, including synchronous
     * availability callbacks. The lineage owner proves its registry drained;
     * a failed enable must never fall through to native activation. */
    if(!SudekiMpLanCastContextSetTaskRouting(enter_task,SudekiMpLeaveSpiritInstance)) return FALSE;
    task_routed=TRUE;
    SudekiMpLogWrite("story_cast event=task_routing active=1 reason=private_entry\r\n");
    return TRUE;
}
static uint32_t enter_skill(void *skill,int slot,BOOL using) {
    if(slot<0 || slot>=6 || !memory(skill,0x78u)) return 0;
    void *actor=*(void **)((uint8_t *)skill+0x10u); int i=index_of(actor);
    SudekiMpCharacterSkillState observed;
    if(i<0 || !authority((unsigned)i) || !SudekiMpObserveCharacterSkill(actor,&observed) || observed.skill!=skill)
        return 0;
    if(using) {
        if(!ready((unsigned)i) || caster[i].sequence==UINT16_MAX ||
            !start_task_routing() ||
            !SudekiMpBeginSpiritInstanceSkillTiming(&caster[i].instance,(uint16_t)(caster[i].sequence+1u))) return 0;
        ++caster[i].sequence;
    }
    if(!start_task_routing()) return 0;
    return SudekiMpEnterSpiritInstance(&caster[i].instance);
}
static uint32_t enter_spirit(void *actor,int strike,BOOL activating,void **manager) {
    if(!actor && owners.leader_character<4u) actor=caster[owners.leader_character].actor;
    int i=index_of(actor),first;
    if(i<0 || !manager || (unsigned)i!=owners.leader_character || !authority((unsigned)i) ||
        !SudekiMpResolveSpiritStrikeId(SudekiMpLanPartyActorType((unsigned)i),1,&first) ||
        (strike!=first && strike!=first+1) || (activating && !all_idle())) return 0;
    if(!start_task_routing()) return 0;
    uint32_t cookie=SudekiMpEnterSpiritInstance(&caster[i].instance);
    if(cookie) *manager=caster[i].instance.manager;
    return cookie;
}
static uint32_t enter_task(const SudekiMpLanCastOwner *owner) {
    if(!owner) return SudekiMpEnterSpiritInstance(NULL);
    int i=index_of(owner->actor);
    if(i<0 || !owner->cast_id || (owner->kind!=1u && owner->kind!=2u) ||
        !retained_actor(owner->actor,owner->session)) return 0;
    return SudekiMpEnterSpiritInstance(&caster[i].instance);
}
static void started(unsigned i,void *skill) {
    SudekiMpCharacterSkillState after;
    caster[i].started=TRUE; caster[i].skill=skill; caster[i].active_seen=FALSE;
    if(SudekiMpObserveCharacterSkill(caster[i].actor,&after) && after.skill==skill && after.active)
        caster[i].active_seen=TRUE;
}
static uint8_t __attribute__((fastcall)) native_use(void *skill,void *edx,int slot) {
    unsigned i=owners.leader_character;
    SudekiMpCharacterSkillState before;
    if(!original_use || i>=4u || !authority(i) || slot<0 || slot>=6 ||
        !SudekiMpObserveCharacterSkill(caster[i].actor,&before) || before.skill!=skill || before.active) return 0;
    uint8_t result=SudekiMpInvokeSkillUse(skill,edx,slot,original_use);
    DWORD error=GetLastError();
    if(result) started(i,skill);
    if(!SudekiMpSkillActivationRoutingHealthy()) fault=TRUE;
    SetLastError(error); return result;
}
static BOOL effect_owner(void *component,SudekiMpLanPartyEffectOwner *out) {
    SudekiMpLanCastOwner cast; void *actor=NULL;
    if(!bound || !out || !exact()) return FALSE;
    if(!component && SudekiMpLanPartyProjectileLifetimeCurrent(out)) {
        int i=index_of(out->actor);
        return i>=0 && retained_actor(out->actor,out->session) &&
            out->generation==caster[i].instance.generation && out->actor_type==SudekiMpLanPartyActorType((unsigned)i);
    }
    if(component) {
        if(!memory(component,0x14u)) return FALSE;
        actor=*(void **)((uint8_t *)component+0x10u);
        if(!memory(actor,0x5cu) || *(void **)((uint8_t *)actor+0x58u)!=component) return FALSE;
    } else {
        if(!SudekiMpLanCastContextCurrentRetained(&cast) || cast.session!=lifetime || !cast.cast_id) return FALSE;
        actor=cast.actor;
    }
    int i=index_of(actor);
    if(i<0 || !retained_actor(actor,lifetime) || !caster[i].instance.generation) return FALSE;
    if(component && !caster[i].started &&
        (!SudekiMpLanCastContextCurrentRetained(&cast) || cast.actor!=actor || cast.session!=lifetime || !cast.cast_id))
        return FALSE;
    *out=(SudekiMpLanPartyEffectOwner){lifetime,caster[i].instance.generation,actor,SudekiMpLanPartyActorType((unsigned)i)};
    return TRUE;
}
static BOOL __attribute__((thiscall)) camera_select(void *manager,const char *name) {
    SudekiMpSpiritInstance scope_owner; unsigned kind;
    if(!bound) return ((SetCamera)camera_hook.trampoline)(manager,name);
    if(!SudekiMpObserveSpiritInstanceScope(&scope_owner)) return FALSE;
    if(!scope_owner.generation || scope_owner.generation==caster[owners.leader_character].instance.generation)
        return ((SetCamera)camera_hook.trampoline)(manager,name);
    for(unsigned i=0;i<4;++i) if(caster[i].actor && scope_owner.generation==caster[i].instance.generation &&
        retained_actor(caster[i].actor,lifetime))
        return SudekiMpRouteSpiritInstanceRenderCamera(manager,name,&kind)==1;
    return FALSE;
}
static int blur_scope(uint32_t *generation) {
    SudekiMpSpiritInstance scoped;
    if(!generation || !initialized || !exact() || !SudekiMpObserveSpiritInstanceScope(&scoped))
        return SUDEKIMP_CAST_BLUR_UNKNOWN;
    *generation=scoped.generation;
    if(!scoped.generation) return SUDEKIMP_CAST_BLUR_NEUTRAL;
    for(unsigned i=0;i<4;++i) if(caster[i].actor && scoped.generation==caster[i].instance.generation &&
        scoped.manager==caster[i].instance.manager && scoped.camera==caster[i].instance.camera)
        return i==owners.leader_character?SUDEKIMP_CAST_BLUR_LOCAL:SUDEKIMP_CAST_BLUR_REMOTE;
    return SUDEKIMP_CAST_BLUR_UNKNOWN;
}
static BOOL shutdown_drained(void) {
    /* Descendant retirement requires body/task drain, not full effect idle:
     * asking an effect to disappear before its native retire entry deadlocks. */
    if(!bound || !stopping || !exact() || submitting>=0 ||
        !SudekiMpLanCastContextDrained() ||
        SudekiMpCleanroomEngineRangedCombatPrimePending()) return FALSE;
    for(unsigned i=0;i<4u;++i) if(caster[i].actor) {
        SudekiMpSpiritInstanceState state;
        if(caster[i].started || !native_idle(caster[i].actor) ||
            !SudekiMpObserveSpiritInstance(&caster[i].instance,&state) || !state.body_idle) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryCastReady(void) {
    return bound && !stopping && !fault && exact();
}
BOOL SudekiMpLanStoryCastLocalNoncaster(void *actor) {
    if(!bound || !initialized || fault || owners.leader_character>=4u ||
        actor!=owners.actors[owners.leader_character] || !SudekiMpLanStoryCastDrained(actor)) return FALSE;
    uint8_t *front=*(uint8_t **)((uint8_t *)image+0x408d1cu);
    uint8_t *quick=*(uint8_t **)((uint8_t *)image+0x3c2f84u);
    if(!memory(front,0x178u) || !front[0x8cu] || front[0x8du] ||
        *(void **)(front+0x170u)!=*(void **)(front+0x174u) ||
        !memory(quick,0x2au) || quick[0x29u]) return FALSE;
    for(unsigned i=0;i<4u;++i) if(caster[i].actor && caster[i].actor!=actor) {
        SudekiMpCharacterSkillState s;
        if(caster[i].started && retained_actor(caster[i].actor,lifetime) &&
            SudekiMpObserveCharacterSkill(caster[i].actor,&s) && s.active && s.skill==caster[i].skill)
            return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryCastInstall(HMODULE module) {
    if(image || !module || !SudekiMpCheckLoadedExecutable(module) || !SudekiMpLanStoryRealtimeExact(module)) return FALSE;
    image=module; stopping=fault=FALSE; return TRUE;
}
static BOOL bind_casters(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {
    if(!r || !SudekiMpLanStoryObserverRosterStillExact(w,r) || !r->available_mask || r->leader_character>=4u)
        return FALSE;
    uint8_t *front=*(uint8_t **)((uint8_t *)image+0x408d1cu);
    uint8_t *speed=*(uint8_t **)((uint8_t *)image+0x408da0u);
    uint8_t *quick=*(uint8_t **)((uint8_t *)image+0x3c2f84u);
    /* Construction cannot borrow an already-open native menu's globals. A
     * later ready dispatch retries this preflight without retaining actors. */
    if(!memory(front,0x178u) || !front[0x8cu] || front[0x8du] ||
        *(void **)(front+0x170u)!=*(void **)(front+0x174u) ||
        !memory(quick,0x2au) || quick[0x29u] || !memory(speed,0x2cu) || speed[0x28u] ||
        *(uint16_t *)(speed+0x2au) || *(int *)(speed+0x20u) || *(int *)(speed+0x24u)) return TRUE;
    owners=*r; native_thread=GetCurrentThreadId();
    for(unsigned i=0;i<4;++i) caster[i].actor=r->actors[i];
    if(!all_idle()) return TRUE; /* no native construction until positive idle */
    if(lifetime_serial==UINT64_MAX) return FALSE;
    lifetime=++lifetime_serial; initializing=TRUE;
    const char *stage="input_isolation";
    if(!SudekiMpLanPartyControlEnableCastInputIsolation(w)) goto failed;
    stage="task_context";
    static const SudekiMpLanCastTaskHost task_owner={SudekiMpLanStoryTaskHostExact,
        SudekiMpLanStoryTaskHostAttach,SudekiMpLanStoryTaskHostDetach};
    context_owned=TRUE;
    if(!SudekiMpInstallLanCastContextWithTaskHost(image,root_owner,&task_owner)) goto failed;
    stage="native_namespaces";
    if(!SudekiMpInitializeSpiritInstanceAbi(image,all_idle)) goto failed;
    initialized=TRUE;
    if(!SudekiMpInstallSpiritInstanceUpdates() || !SudekiMpEnableSpiritInstanceNamedCameraBanking()) goto failed;
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        if(!SudekiMpCreateSpiritInstance(&caster[i].instance) ||
            !SudekiMpBindSpiritInstanceCaster(&caster[i].instance,caster[i].actor,
                SudekiMpLanPartyActorType(i),lifetime,retained_actor)) goto failed;
    }
    stage="private_ui_camera_input";
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        if(i!=owners.leader_character && (!SudekiMpEnableSpiritInstanceRemoteUi(&caster[i].instance) ||
            !SudekiMpEnableSpiritInstanceRemoteSkillUi(&caster[i].instance) ||
            !SudekiMpEnableSpiritInstanceRemoteSkillInput(&caster[i].instance,caster[owners.leader_character].actor,
                retained_actor))) goto failed;
        if(!SudekiMpEnableSpiritInstanceNamedCameras(&caster[i].instance)) goto failed;
    }
    if(!SudekiMpInstallSpiritInstanceNamedCameraUpdates()) goto failed;
    for(unsigned i=0;i<4;++i) if(caster[i].actor && i!=owners.leader_character &&
        !SudekiMpEnableSpiritInstanceRemoteCameraSelection(&caster[i].instance)) goto failed;
    stage="gates_timing";
    if(!SudekiMpEnableSpiritInstanceCastGates() || !SudekiMpEnableSpiritInstanceLighting() ||
        !SudekiMpEnableSpiritInstanceSkillTargeting() || !SudekiMpEnableSpiritInstanceSharedSsp()) goto failed;
    for(unsigned i=0;i<4;++i) if(caster[i].actor &&
        (!SudekiMpConfigureSpiritInstanceSkillTiming(&caster[i].instance,FALSE) ||
         !SudekiMpScheduleSpiritInstanceManager(&caster[i].instance))) goto failed;
    if(!SudekiMpInstallCastMotionBlur(image,blur_scope)) goto failed;
    static const uint8_t camera_bytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    if(!SudekiMpInstallInlineHook(&camera_hook,(uint8_t *)image+0x36fb0u,camera_bytes,sizeof(camera_bytes),camera_select))
        goto failed;
    stage="native_entry_routing";
    if(SudekiMpQuickSkillSpiritRoutingReady()) { SetLastError(ERROR_ALREADY_EXISTS); goto failed; }
    menu_owned=TRUE;
    if(!SudekiMpInstallQuickSkillInputTrace(image,FALSE,FALSE)) goto failed;
    if(!SudekiMpSetSpiritActivationRouting(all_idle,enter_spirit,SudekiMpLeaveSpiritInstance)) goto failed;
    spirit_routed=TRUE;
    if(!SudekiMpSetSkillActivationRouting(all_idle,enter_skill,SudekiMpLeaveSpiritInstance)) goto failed;
    skill_routed=TRUE;
    original_use=(SudekiMpSkillUseFunction)((uint8_t *)image+0xb4810u);
    static const unsigned sites[]={0x27cb1u,0x998a1u};
    for(unsigned i=0;i<2;++i)
        if(!SudekiMpInstallRelativeCallHook(&use_hooks[i],(uint8_t *)image+sites[i],original_use,native_use)) goto failed;
    stage="descendant_lifetimes";
    missiles_owned=TRUE;
    if(!SudekiMpLanPartyProjectileLifetimeInstall(image)) goto failed;
    effects_owned=TRUE;
    if(!SudekiMpLanPartyEffectLifetimeInitialize(image,effect_owner,shutdown_drained)) goto failed;
    if(!SudekiMpLanPartyEffectLifetimePoll()) goto failed;
    /* Ordinary story scripts remain on the original step path while idle.
     * Native entry routing above activates task isolation before the first
     * private callback; no camera/name/light banking per unrelated idle task. */
    bound=TRUE; initializing=FALSE;
    SudekiMpLogFormat("story_cast event=bound mask=%u local=%u authority=host sparse_party=1\r\n",
        owners.available_mask,owners.leader_character);
    return TRUE;
failed: {
        DWORD error=GetLastError(); HMODULE retry=image;
        initializing=FALSE; stopping=TRUE;
        BOOL restored=SudekiMpLanStoryCastUninstall();
        SudekiMpLogFormat("story_cast event=bind_failed stage=%s error=%lu retained=%u\r\n",
            stage,(unsigned long)error,!restored);
        if(restored) { image=retry; fault=TRUE; stopping=TRUE; }
        SetLastError(error); return FALSE;
    }
}
BOOL SudekiMpLanStoryCastTryBind(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!image || stopping || fault || initializing || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    return bound ? exact() : bind_casters(w,r);
}
BOOL SudekiMpLanStoryCastService(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!image) return TRUE;
    if(!w || !w->service_post_original_exact || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    (void)r;
    if(!bound) return !fault && !initializing;
    if(!exact() || !SudekiMpLanCastContextPoll() || !SudekiMpLanPartyEffectLifetimePoll() ||
        !SudekiMpLanPartyProjectileLifetimePoll()) { fault=TRUE; return FALSE; }
    DWORD now=GetTickCount();
    float seconds=last_service && now-last_service<=100u?(float)(now-last_service)/1000.0f:0;
    last_service=now;
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        StoryCaster *c=&caster[i]; SudekiMpCharacterSkillState s;
        if(!SudekiMpObserveCharacterSkill(c->actor,&s) ||
            (stopping && (c->started || s.active) &&
             !SudekiMpDrainSpiritInstanceSkillTiming(&c->instance)) ||
            !SudekiMpAdvanceSpiritInstanceSkillTiming(&c->instance,seconds)) { fault=TRUE; return FALSE; }
        if(c->started) {
            if(s.skill!=c->skill) { fault=TRUE; return FALSE; }
            if(s.active) c->active_seen=TRUE;
            if(c->active_seen && !s.active && native_idle(c->actor) &&
                SudekiMpLanCastContextActorDrained(c->actor,lifetime)) {
                c->started=c->active_seen=FALSE; c->skill=NULL;
            }
        }
    }
    if(shutdown_drained() && (!SudekiMpLanPartyProjectileLifetimeRequestRetire() ||
        !SudekiMpLanPartyEffectLifetimeRequestRetire())) return FALSE;
    if(task_routed && !fault && all_idle() &&
        !SudekiMpLanPartyEffectLifetimeRetains() && !SudekiMpLanPartyProjectileLifetimeRetains()) {
        SudekiMpSpiritInstance current;
        /* Body completion alone is insufficient: descendants, effects and
         * native namespace restoration must all be positively observed. */
        if(!SudekiMpObserveSpiritInstanceScope(&current) || current.generation ||
            !SudekiMpLanCastContextSetTaskRouting(NULL,NULL)) { fault=TRUE; return FALSE; }
        task_routed=FALSE;
        SudekiMpLogWrite("story_cast event=task_routing active=0 reason=fully_drained\r\n");
    }
    return !fault;
}
unsigned SudekiMpLanStoryCastSubmit(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key,unsigned slot) {
    if(!key || key->seat>=4u || slot>=6u || !bound || stopping || fault || submitting>=0 ||
        !exact() || !SudekiMpLanStoryControlExact(w,r,key) || r->actors[key->seat]!=caster[key->seat].actor)
        return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    unsigned i=key->seat; submitting=(int)i;
    /* Reject actual pause/loading/cinematic modes. Normal-time skill mode
     * keeps its native task semantics; the realtime adapter changes speed,
     * never fabricates an unpaused world. */
    uint8_t *speed=*(uint8_t **)((uint8_t *)image+0x408da0u);
    if(!memory(speed,0x2cu) || speed[0x28u] || *(uint16_t *)(speed+0x2au) ||
        (*(int *)(speed+0x20u)!=0 && *(int *)(speed+0x20u)!=2) ||
        (*(int *)(speed+0x24u)!=0 && *(int *)(speed+0x24u)!=2)) {
        submitting=-1; return SUDEKIMP_STORY_ACTION_BUSY;
    }
    if(*(int *)(speed+0x20u)==2 || *(int *)(speed+0x24u)==2) {
        BOOL owned_skill=FALSE;
        for(unsigned j=0;j<4u;++j) if(caster[j].actor && caster[j].started) {
            SudekiMpCharacterSkillState s;
            if(SudekiMpObserveCharacterSkill(caster[j].actor,&s) && s.active && s.skill==caster[j].skill)
                owned_skill=TRUE;
        }
        if(!owned_skill) { submitting=-1; return SUDEKIMP_STORY_ACTION_BUSY; }
    }
    if(!ready(i)) { submitting=-1; return SUDEKIMP_STORY_ACTION_BUSY; }
    SudekiMpSkillActivationResult result=SudekiMpActivateCharacterSkillSlot(caster[i].actor,slot);
    /* Keep the native rejection, rather than losing SP/readiness failures
     * behind the wire's deliberately coarse UNAVAILABLE result. This is one
     * diagnostic per deduplicated request, never a retry or an SP override. */
    SudekiMpLogFormat("story_cast event=activation_result character=%u slot=%u status=%u validation=%d use=%u\r\n",
        i,slot,(unsigned)result.status,result.validation_result,(unsigned)result.use_result);
    if(result.status==SUDEKIMP_SKILL_ACTIVATION_STARTED) started(i,result.skill);
    submitting=-1;
    if(!SudekiMpSkillActivationRoutingHealthy()) {
        fault=TRUE; return SUDEKIMP_STORY_ACTION_RETAINED;
    }
    if(result.status==SUDEKIMP_SKILL_ACTIVATION_STARTED) return SUDEKIMP_STORY_ACTION_STARTED;
    /* Positive idle is required even after native Use reports rejection. */
    if(!SudekiMpLanStoryCastDrained(caster[i].actor)) return SUDEKIMP_STORY_ACTION_RETAINED;
    if(result.status==SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED && result.validation_result==1)
        return SUDEKIMP_STORY_ACTION_NO_SP;
    return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
}
void SudekiMpLanStoryCastRequestStop(void) { stopping=TRUE; }
BOOL SudekiMpLanStoryCastRetains(void) {
    return initialized || context_owned || bound || initializing || menu_owned || effects_owned || missiles_owned;
}
BOOL SudekiMpLanStoryCastUninstall(void) {
    stopping=TRUE;
    if(!image) return TRUE;
    if(initializing || submitting>=0 || (native_thread && native_thread!=GetCurrentThreadId()) ||
        (initialized && !all_idle()) || SudekiMpLanPartyEffectLifetimeRetains() ||
        SudekiMpLanPartyProjectileLifetimeRetains()) { SetLastError(ERROR_BUSY); return FALSE; }
    for(unsigned i=2;i>0;--i) if(!SudekiMpRestoreRelativeCallHook(&use_hooks[i-1u])) return FALSE;
    if(task_routed && !SudekiMpLanCastContextSetTaskRouting(NULL,NULL)) return FALSE;
    task_routed=FALSE;
    if(skill_routed && !SudekiMpSetSkillActivationRouting(NULL,NULL,NULL)) return FALSE;
    skill_routed=FALSE;
    if(spirit_routed && !SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL)) return FALSE;
    spirit_routed=FALSE;
    if(menu_owned && !SudekiMpUninstallQuickSkillInputTrace()) return FALSE;
    menu_owned=FALSE;
    if(!SudekiMpRestoreInlineHook(&camera_hook) || !SudekiMpUninstallCastMotionBlur()) return FALSE;
    if(effects_owned && !SudekiMpLanPartyEffectLifetimeReset()) return FALSE;
    effects_owned=FALSE;
    if(missiles_owned && !SudekiMpLanPartyProjectileLifetimeUninstall()) return FALSE;
    missiles_owned=FALSE;
    for(unsigned i=4;i>0;--i) if(caster[i-1].instance.generation &&
        !SudekiMpDestroySpiritInstance(&caster[i-1].instance)) return FALSE;
    if(initialized && !SudekiMpResetSpiritInstanceAbi()) return FALSE;
    initialized=FALSE;
    if(context_owned && !SudekiMpUninstallLanCastContext()) return FALSE;
    context_owned=bound=FALSE; native_thread=last_service=0; lifetime=0;
    memset(caster,0,sizeof(caster)); memset(&owners,0,sizeof(owners)); image=NULL;
    return TRUE;
}
