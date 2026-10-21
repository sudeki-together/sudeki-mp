#include "hooks/lan_story_cast.h"
#include "hooks/lan_arena_spirit_audio.h"
#include "hooks/lan_story_realtime.h"
#include "hooks/lan_story_task_trace.h"
#include "hooks/lan_party_control.h"
#include "hooks/lan_arena_cast_context.h"
#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/lan_party_projectile_lifetime.h"
#include "hooks/lan_story_shots.h"
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
    BOOL spirit_started,spirit_active_seen,spirit_root_started;
    DWORD spirit_trace_at;
    unsigned spirit_traces;
    uint16_t audio_sequence;
    uint32_t audio_tick;
} StoryCaster;
static HMODULE image;
static SudekiMpLanStoryNativeRoster owners;
static StoryCaster caster[4];
static uint64_t lifetime,lifetime_serial;
static DWORD native_thread,last_service;
static DWORD bind_deferred_at;
static BOOL bind_deferred;
static BOOL retire_superseded;
static BOOL bound,initializing,initialized,context_owned,menu_owned,skill_routed,spirit_routed,task_routed;
static BOOL effects_owned,missiles_owned,stopping,fault;
static BOOL visuals_owned,audio_owned;
static int submitting=-1;
static BOOL submitting_spirit;
/* One synchronous namespace transition has no native calls except its
 * retained witnesses. Prove the whole roster once, then share that proof
 * among light/SSP/camera validators. Clear it BEFORE any script executes;
 * Leave must acquire a new proof after native execution. */
static BOOL transition_roster_proved;
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
static BOOL same_cast_owners(const SudekiMpLanStoryNativeRoster *r) {
    return r && r->available_mask==owners.available_mask && r->leader_character==owners.leader_character &&
        r->world==owners.world && r->group==owners.group && r->controller==owners.controller &&
        !memcmp(r->actors,owners.actors,sizeof(r->actors)) && !memcmp(r->ai,owners.ai,sizeof(r->ai));
}
static BOOL superseded_roster(SudekiMpLanStoryNativeRoster *out) {
    SudekiMpLanStoryScene scene;
    if(!bound || !image || native_thread!=GetCurrentThreadId() ||
        !SudekiMpLanStoryObserverPublished(&scene) || scene.epoch==owners.epoch) return FALSE;
    *out=owners; out->epoch=scene.epoch; out->revision=scene.revision;
    return SudekiMpLanStoryObserverNativeRosterExact(out);
}
BOOL SudekiMpLanStoryCastSuperseded(void) {
    SudekiMpLanStoryNativeRoster current;
    return superseded_roster(&current);
}
/* Supplied service rosters have already been proved by the runtime. Recheck
 * here at this public boundary; the new epoch grants no new object identity. */
static BOOL rebase_owners(const SudekiMpLanStoryNativeRoster *r) {
    if(!bound || !r || r->epoch==owners.epoch || !same_cast_owners(r) ||
        !SudekiMpLanStoryObserverNativeRosterExact(r)) return FALSE;
    owners=*r; retire_superseded=stopping=TRUE; return TRUE;
}
static BOOL retained_actor(void *actor,uint64_t generation) {
    int i=index_of(actor);
    return lifetime && generation==lifetime && i>=0 &&
        (transition_roster_proved ? native_thread==GetCurrentThreadId():exact()) && owners.actors[i]==actor;
}
static BOOL audio_witness(void *unused,int *state,uint64_t *session,uint16_t *sequence,uint8_t *character) {
    (void)unused;SudekiMpSpiritInstance scope;
    if(!bound || !exact() || !SudekiMpObserveSpiritInstanceScope(&scope) || !scope.generation) return FALSE;
    for(unsigned i=0;i<4;++i) {
        StoryCaster *c=&caster[i];SudekiMpSpiritInstanceState observed;
        if(!c->actor || !c->sequence || scope.generation!=c->instance.generation ||
            scope.manager!=c->instance.manager || scope.camera!=c->instance.camera)continue;
        if(!retained_actor(c->actor,lifetime) || !SudekiMpObserveSpiritInstance(&c->instance,&observed) ||
            !observed.state)return FALSE;
        *state=(int)observed.state;*session=lifetime;*sequence=c->sequence;*character=(uint8_t)i;return TRUE;
    }
    return FALSE;
}
void SudekiMpLanStoryCastCaptureAudioStart(unsigned character,uint16_t *sequence,uint32_t *tick) {
    if(!sequence || !tick)return;
    *sequence=0;*tick=0;
    if(character>=4 || !bound || !audio_owned || !exact())return;
    SudekiMpLanArenaSpiritAudioEvent events[SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
    size_t count=SudekiMpLanArenaSpiritAudioTraceSnapshot(events,SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY,NULL);
    StoryCaster *c=&caster[character];
    for(size_t i=0;i<count;++i) {
        const SudekiMpLanArenaSpiritAudioEvent *e=&events[i];
        if(e->session_token!=lifetime || e->owner_seat!=character || !e->skill_sequence ||
            e->skill_sequence!=c->sequence || strcmp(e->cue,"spiritstrike_start"))continue;
        if(c->audio_sequence!=e->skill_sequence) {
            c->audio_sequence=e->skill_sequence;c->audio_tick=e->observed_tick;
        }
    }
    *sequence=c->audio_sequence;*tick=c->audio_tick;
}
static BOOL native_idle(void *actor) {
    SudekiMpCharacterSkillState s;
    if(!SudekiMpObserveCharacterSkill(actor,&s) || s.active || !memory(s.skill,0x78u) ||
        ((uint8_t *)s.skill)[0x6cu]) return FALSE;
    void *task=*(void **)((uint8_t *)s.skill+0x74u);
    return !task || (memory(task,8u) && !*(void **)task && *((uint32_t *)task+1));
}
static BOOL namespaces_idle(void) {
    int spirit=-1;
    if(!exact() || !SudekiMpLanCastContextDrained() ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit) return FALSE;
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        SudekiMpSpiritInstanceState s;
        if(caster[i].started || caster[i].spirit_started || !native_idle(caster[i].actor) ||
            (caster[i].instance.generation &&
             (!SudekiMpObserveSpiritInstance(&caster[i].instance,&s) || !s.idle))) return FALSE;
    }
    return TRUE;
}
static BOOL all_idle(void) { return submitting<0 && namespaces_idle(); }
static BOOL cast_drained(void *actor,BOOL include_lighting) {
    int i=index_of(actor); SudekiMpSpiritInstanceState s;
    if(!initialized) return !SudekiMpLanStoryCastRetains();
    return i>=0 && retained_actor(actor,lifetime) && !caster[i].started &&
        !caster[i].spirit_started && native_idle(actor) &&
        SudekiMpLanCastContextActorDrained(actor,lifetime) &&
        SudekiMpObserveSpiritInstance(&caster[i].instance,&s) && (include_lighting?s.idle:s.body_idle);
}
BOOL SudekiMpLanStoryCastDrained(void *actor) { return cast_drained(actor,TRUE); }
BOOL SudekiMpLanStoryCastBodyDrained(void *actor) { return cast_drained(actor,FALSE); }
/* Movement permission is narrower than cast completion: retain the exact
 * admitted ordinary skill and its native targeting task/sequence. The shared
 * phase observer closes this window at the authored deadline or disable call;
 * it never extends a cast or permits movement through its execution phase. */
BOOL SudekiMpLanStoryCastAiming(void *actor) {
    int i=index_of(actor); SudekiMpCharacterSkillState skill;
    uint8_t phase; uint16_t remaining;
    return initialized && bound && !stopping && !fault && i>=0 &&
        retained_actor(actor,lifetime) && caster[i].started && !caster[i].spirit_started &&
        SudekiMpObserveCharacterSkill(actor,&skill) && skill.active && skill.skill==caster[i].skill &&
        SudekiMpObserveSpiritInstanceSkillTiming(&caster[i].instance,caster[i].sequence,&phase,&remaining) &&
        phase==SUDEKIMP_SKILL_TARGET_AIMING && remaining>0u;
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
        if(caster[j].spirit_started ||
            !SudekiMpObserveSpiritInstanceActivity(&caster[j].instance,&active) || active) return FALSE;
    }
    return TRUE;
}
static BOOL root_owner(void *actor,uint8_t kind,uint64_t *session,uint8_t *type) {
    int i=index_of(actor);
    if(i<0 || !session || !type || (kind!=1u && kind!=2u)) return FALSE;
    if(kind==2u && (unsigned)i!=owners.leader_character) {
        /* Retail F900 launches the power-up script through 10C20/10DE5 on
         * a later manager tick, not in Activate(FBA0). The admitted cast must
         * retain this ONE root across request expiry/disconnect. This grants
         * no fresh player action and never routes a foreign manager's root. */
        SudekiMpSpiritInstance current; BOOL active;
        if(!caster[i].spirit_started || caster[i].spirit_root_started ||
            !retained_actor(actor,lifetime) ||
            !SudekiMpObserveSpiritInstanceScope(&current) ||
            current.generation!=caster[i].instance.generation ||
            current.manager!=caster[i].instance.manager || current.camera!=caster[i].instance.camera ||
            !SudekiMpObserveSpiritInstanceActivity(&caster[i].instance,&active) || !active) return FALSE;
        caster[i].spirit_root_started=TRUE;
    } else if(!authority((unsigned)i)) return FALSE;
    *session=lifetime; *type=SudekiMpLanPartyActorType((unsigned)i); return TRUE;
}
static uint32_t enter_task(const SudekiMpLanCastOwner *owner);
static BOOL leave_task(uint32_t cookie);
static BOOL start_task_routing(void) {
    if(task_routed) return TRUE;
    /* Enable BEFORE entering any private namespace, including synchronous
     * availability callbacks. The lineage owner proves its registry drained;
     * a failed enable must never fall through to native activation. */
    if(!SudekiMpLanCastContextSetTaskRouting(enter_task,leave_task)) return FALSE;
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
    if(i<0 || !manager || !authority((unsigned)i) ||
        ((unsigned)i!=owners.leader_character && !submitting_spirit) ||
        !SudekiMpResolveSpiritStrikeId(SudekiMpLanPartyActorType((unsigned)i),1,&first) ||
        (strike!=first && strike!=first+1) ||
        (activating && ((submitting>=0 && submitting!=i) || !namespaces_idle()))) return 0;
    if(!start_task_routing()) return 0;
    uint32_t cookie=SudekiMpEnterSpiritInstance(&caster[i].instance);
    if(cookie) *manager=caster[i].instance.manager;
    return cookie;
}
static uint32_t enter_task(const SudekiMpLanCastOwner *owner) {
    int i=owner?index_of(owner->actor):-1;
    if(transition_roster_proved || !lifetime || (owner && (i<0 || !owner->cast_id ||
        (owner->kind!=1u && owner->kind!=2u) || owner->session!=lifetime)) || !exact()) return 0;
    transition_roster_proved=TRUE;
    uint32_t cookie=SudekiMpEnterSpiritInstance(owner?&caster[i].instance:NULL);
    transition_roster_proved=FALSE;
    return cookie;
}
static BOOL leave_task(uint32_t cookie) {
    if(transition_roster_proved || !exact()) return FALSE;
    transition_roster_proved=TRUE;
    BOOL result=SudekiMpLeaveSpiritInstance(cookie);
    transition_roster_proved=FALSE;
    return result;
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
        if(!SudekiMpLanCastContextCurrentRetained(&cast) || cast.session!=lifetime || !cast.cast_id ||
            (cast.kind!=1u && cast.kind!=2u)) return FALSE;
        actor=cast.actor;
    }
    int i=index_of(actor);
    if(i<0 || !retained_actor(actor,lifetime) || !caster[i].instance.generation) return FALSE;
    if(component && i!=1 && i!=3 && !caster[i].started && !caster[i].spirit_started &&
        (!SudekiMpLanCastContextCurrentRetained(&cast) || cast.actor!=actor || cast.session!=lifetime || !cast.cast_id))
        return FALSE;
    BOOL scoped=SudekiMpLanCastContextCurrentRetained(&cast) &&
        cast.actor==actor && cast.session==lifetime && cast.cast_id && (cast.kind==1u || cast.kind==2u);
    if(!scoped && !caster[i].started && !caster[i].spirit_started && i!=1 && i!=3) return FALSE;
    *out=(SudekiMpLanPartyEffectOwner){lifetime,caster[i].instance.generation,actor,
        SudekiMpLanPartyActorType((unsigned)i),
        (scoped || caster[i].started || caster[i].spirit_started) ? caster[i].sequence : 0u};
    return TRUE;
}
BOOL SudekiMpLanStoryCastShotOwner(void *actor,SudekiMpLanPartyEffectOwner *out) {
    int i=index_of(actor); SudekiMpCharacterSkillState skill;
    if(!out || !bound || stopping || fault || (i!=1 && i!=3) || !exact() ||
        !retained_actor(actor,lifetime) || !caster[i].instance.generation ||
        caster[i].started || caster[i].spirit_started ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active) return FALSE;
    *out=(SudekiMpLanPartyEffectOwner){lifetime,caster[i].instance.generation,actor,SudekiMpLanPartyActorType((unsigned)i),0u};
    return TRUE;
}
static BOOL visual_owner(void *unused,void *component,uint64_t *session,uint16_t *sequence,
    uint32_t *tick,uint8_t *type) {
    SudekiMpLanPartyEffectOwner owner;
    (void)unused;
    if(!session || !sequence || !tick || !type || !bound || !exact()) return FALSE;
    if(component) {
        /* FX components use their registered emission identity in the visual
         * observer, never the caster's latest sequence. Only exact roster
         * actor components may use the current lifetime forwarding scope. */
        if(!memory(component,0x14u)) return FALSE;
        void *actor=*(void **)((uint8_t *)component+0x10u);
        if(index_of(actor)<0 || !memory(actor,0x5cu) ||
            *(void **)((uint8_t *)actor+0x58u)!=component) return FALSE;
    }
    if(!SudekiMpLanPartyEffectLifetimeCurrent(&owner)) return FALSE;
    int i=index_of(owner.actor);
    if(i<0 || !retained_actor(owner.actor,owner.session) ||
        owner.generation!=caster[i].instance.generation ||
        owner.actor_type!=SudekiMpLanPartyActorType((unsigned)i) ||
        (!owner.cast_sequence && i!=1 && i!=3)) return FALSE;
    *session=owner.session; *sequence=owner.cast_sequence; *tick=GetTickCount();
    *type=(i==2 && owner.cast_sequence) ? 0u : owner.actor_type;
    return TRUE;
}

static BOOL shield_owner(void *unused,uint8_t type,void **actor) {
    (void)unused;
    if(!actor || !bound || !exact()) return FALSE;
    for(unsigned c=0;c<4;c+=2) if(type==SudekiMpLanPartyActorType(c) && caster[c].actor &&
        retained_actor(caster[c].actor,lifetime)) {
        *actor=caster[c].actor;return TRUE;
    }
    return FALSE; /* A sparse story party need not contain Buki yet. */
}
BOOL SudekiMpLanStoryCastCaptureVisuals(SudekiMpStoryVfx *out) {
    if(!out || !bound || !visuals_owned || !exact()) return FALSE;
    SudekiMpStoryVfx result={0};
    BOOL idle=TRUE;
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        BOOL active;
        if(caster[i].started || caster[i].spirit_started ||
            !SudekiMpObserveSpiritInstanceActivity(&caster[i].instance,&active) || active) idle=FALSE;
    }
    if(!SudekiMpLanPartySpiritVisualHostCapture(lifetime,idle,&result)) return FALSE;
    result.stream=lifetime; result.tick=GetTickCount(); result.cast_sequence=caster[2].sequence;
    if(!SudekiMpLanStoryShotsAppendVisuals(&owners,&result) || !SudekiMpStoryVfxValid(&result)) return FALSE;
    *out=result; return TRUE;
}
BOOL SudekiMpLanStoryCastCaptureView(unsigned character,SudekiMpStoryCastView *out) {
    SudekiMpSpiritCameraView native;
    if(!out || character>=4u || !bound || stopping || fault || !exact() ||
        !(owners.available_mask&(1u<<character)) || owners.leader_character==character ||
        !retained_actor(caster[character].actor,lifetime) ||
        !SudekiMpObserveSpiritInstanceView(&caster[character].instance,&native)) return FALSE;
    const StoryCaster *c=&caster[character];
    SudekiMpStoryCastView next={0};
    next.stream=lifetime; next.tick=GetTickCount(); next.fence.character=(uint8_t)character;
    next.cast_sequence=c->sequence;
    /* Both cast families select these retained native cameras. Publish only
     * the observed choice: native obstruction checks may keep it at default.
     * A native default selection or positively drained cast ends the view;
     * an unknown observation above must never manufacture that transition. */
    next.kind=(c->started || c->spirit_started) ? (uint8_t)native.kind:0;
    if(next.kind) {
        next.view.valid=1; next.view.camera_serial=(uint32_t)next.cast_sequence*4u+next.kind;
        memcpy(next.view.matrix,native.matrix,sizeof(next.view.matrix));
        memcpy(next.view.projection,native.projection,sizeof(next.view.projection));
        if(!next.cast_sequence || !SudekiMpLanStoryViewValid(&next.view)) return FALSE;
    }
    if(!exact()) return FALSE;
    static unsigned last_kind[4]={3,3,3,3},reports;
    if(next.kind!=last_kind[character] && reports<64) {
        ++reports; last_kind[character]=next.kind;
        SudekiMpLogFormat("story_cast_view event=host character=%u kind=%u cast=%u tick=%lu\r\n",
            character,next.kind,next.cast_sequence,(unsigned long)next.tick);
    }
    *out=next; return TRUE;
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
        if(caster[i].started || caster[i].spirit_started || !native_idle(caster[i].actor) ||
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
    image=module; retire_superseded=FALSE; stopping=fault=bind_deferred=FALSE; return TRUE;
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
    BOOL retry_busy=FALSE;
    int stage_character=-1;
    const char *stage="input_isolation";
    if(!SudekiMpLanPartyControlEnableCastInputIsolation(w)) goto failed;
    stage="task_context";
    static const SudekiMpLanCastTaskHost task_owner={SudekiMpLanStoryTaskHostExact,
        SudekiMpLanStoryTaskHostAttach,SudekiMpLanStoryTaskHostDetach};
    context_owned=TRUE;
    if(!SudekiMpInstallLanCastContextWithTaskHost(image,root_owner,&task_owner)) goto failed;
    stage="native_namespace_abi";
    if(!SudekiMpInitializeSpiritInstanceAbi(image,all_idle)) goto failed;
    initialized=TRUE;
    stage="native_namespace_updates";
    if(!SudekiMpInstallSpiritInstanceUpdates()) goto failed;
    stage="native_camera_banking"; retry_busy=TRUE;
    SetLastError(ERROR_SUCCESS);
    if(!SudekiMpEnableSpiritInstanceNamedCameraBanking()) goto failed;
    for(unsigned i=0;i<4;++i) if(caster[i].actor) {
        stage="native_namespace_create"; stage_character=(int)i; retry_busy=TRUE;
        SetLastError(ERROR_SUCCESS);
        if(!SudekiMpCreateSpiritInstance(&caster[i].instance)) goto failed;
        stage="native_namespace_caster"; retry_busy=FALSE;
        if(!SudekiMpBindSpiritInstanceCaster(&caster[i].instance,caster[i].actor,
                SudekiMpLanPartyActorType(i),lifetime,retained_actor)) goto failed;
    }
    retry_busy=FALSE; stage_character=-1;
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
    stage="cast_gates";
    if(!SudekiMpEnableSpiritInstanceCastGates()) goto failed;
    stage="cast_lighting";
    if(!SudekiMpEnableSpiritInstanceLighting()) goto failed;
    stage="skill_targeting";
    if(!SudekiMpEnableSpiritInstanceSkillTargeting()) goto failed;
    stage="shared_ssp";
    if(!SudekiMpEnableSpiritInstanceSharedSsp()) goto failed;
    stage="skill_timing";
    for(unsigned i=0;i<4;++i) if(caster[i].actor &&
        (!SudekiMpConfigureSpiritInstanceSkillTiming(&caster[i].instance,FALSE) ||
         !SudekiMpScheduleSpiritInstanceManager(&caster[i].instance))) goto failed;
    stage="motion_blur";
    if(!SudekiMpInstallCastMotionBlur(image,blur_scope)) goto failed;
    stage="camera_selection";
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
    stage="spirit_audio";
    if(!SudekiMpInstallLanArenaSpiritAudioTrace(image,audio_witness,NULL))goto failed;
    audio_owned=TRUE;
    stage="descendant_lifetimes";
    missiles_owned=TRUE;
    if(!SudekiMpLanPartyProjectileLifetimeInstall(image)) goto failed;
    visuals_owned=TRUE;
    if(!SudekiMpLanArenaSpiritVisualHostInitialize(image,visual_owner,NULL) ||
        !SudekiMpLanPartySpiritVisualHostSetShieldWitness(image,shield_owner)) goto failed;
    effects_owned=TRUE;
    if(!SudekiMpLanPartyEffectLifetimeInitialize(image,effect_owner,shutdown_drained)) goto failed;
    if(!SudekiMpLanPartyEffectLifetimePoll()) goto failed;
    /* Ordinary story scripts remain on the original step path while idle.
     * Native entry routing above activates task isolation before the first
     * private callback; no camera/name/light banking per unrelated idle task. */
    bound=TRUE; initializing=bind_deferred=FALSE;
    {
        SudekiMpStoryVfx baseline;
        if(!SudekiMpLanStoryCastCaptureVisuals(&baseline)) goto failed;
    }
    SudekiMpLogFormat("story_cast event=bound mask=%u local=%u authority=host sparse_party=1\r\n",
        owners.available_mask,owners.leader_character);
    return TRUE;
failed: {
        DWORD error=GetLastError(); HMODULE retry=image;
        initializing=FALSE; stopping=TRUE;
        BOOL restored=SudekiMpLanStoryCastUninstall();
        /* A busy native construction boundary can become idle on a later
         * dispatch (the original camera may outlive the skill body). Retry
         * only after every retained object/hook has positively rolled back.
         * Byte/identity failures and incomplete rollback remain quarantined. */
        BOOL deferred=restored && retry_busy && error==ERROR_BUSY;
        SudekiMpLogFormat("story_cast event=%s stage=%s character=%d error=%lu retained=%u retry=%u\r\n",
            deferred?"bind_deferred":"bind_failed",stage,stage_character,
            (unsigned long)error,!restored,deferred);
        if(restored) {
            image=retry; fault=stopping=!deferred;
            bind_deferred=deferred; bind_deferred_at=GetTickCount();
        }
        SetLastError(error); return deferred;
    }
}
BOOL SudekiMpLanStoryCastTryBind(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!image || stopping || fault || initializing || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    if(bind_deferred && GetTickCount()-bind_deferred_at<1000u) return TRUE;
    return bound ? (exact() || rebase_owners(r)) : bind_casters(w,r);
}
BOOL SudekiMpLanStoryCastService(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!image) return TRUE;
    if(!w || !w->service_post_original_exact || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    if(!bound) return !fault && !initializing;
    if(!exact() && !rebase_owners(r)) {
        if(SudekiMpLanStoryObserverIdentityUnknown()) return TRUE;
        fault=TRUE; return FALSE;
    }
    if(retire_superseded) stopping=TRUE;
    if(!SudekiMpLanCastContextPoll() || !SudekiMpLanPartyEffectLifetimePoll() ||
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
        if(c->spirit_started) {
            BOOL active; SudekiMpSpiritInstanceState state;
            if(!SudekiMpObserveSpiritInstanceActivity(&c->instance,&active) ||
                !SudekiMpObserveSpiritInstance(&c->instance,&state)) { fault=TRUE; return FALSE; }
            if(active) c->spirit_active_seen=TRUE;
            if(SudekiMpLogResearchEnabled() && c->spirit_traces<40u &&
                (!c->spirit_trace_at || now-c->spirit_trace_at>=1000u)) {
                uint32_t ticks=0,cam=0,soul=0;
                BOOL observed=SudekiMpObserveSpiritInstanceManagerTicks(&c->instance,&ticks) &&
                    SudekiMpObserveSpiritInstanceUpdates(&c->instance,&cam,&soul);
                /* The successful instance observation above re-proves this
                 * caster, state component, vtable and reciprocal owner. */
                uint8_t *participant=*(uint8_t **)((uint8_t *)c->actor+0x130u);
                ++c->spirit_traces; c->spirit_trace_at=now;
                SudekiMpLogFormat("story_cast event=spirit_progress character=%u state=%lu strike=%lu camera=%u body_idle=%u tasks_idle=%u participant=%u ticks_known=%u ticks=%lu,%lu,%lu ms=%lu\r\n",
                    i,(unsigned long)state.state,(unsigned long)state.strike_id,state.camera_active,
                    state.body_idle,SudekiMpLanCastContextActorDrained(c->actor,lifetime),
                    participant[0x131u],observed,(unsigned long)ticks,(unsigned long)cam,
                    (unsigned long)soul,(unsigned long)now);
            }
            if(c->spirit_active_seen && !active && state.body_idle &&
                SudekiMpLanCastContextActorDrained(c->actor,lifetime)) {
                c->spirit_started=c->spirit_active_seen=FALSE;
                SudekiMpLogFormat("story_cast event=spirit_body_drained character=%u\r\n",i);
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
    if(retire_superseded && !fault && all_idle() &&
        !SudekiMpLanPartyEffectLifetimeRetains() && !SudekiMpLanPartyProjectileLifetimeRetains()) {
        HMODULE module=image;
        if(!SudekiMpLanStoryCastUninstall()) return FALSE;
        /* Return to the installed/unbound state. The runtime's existing
         * READY path alone may construct the replacement namespaces. */
        return SudekiMpLanStoryCastInstall(module);
    }
    return !fault;
}
static void trace_readiness(unsigned i,unsigned kind,unsigned slot) {
    DWORD error=GetLastError();
    uint8_t *a=caster[i].actor,*arb=memory(a,0xa8u)?*(uint8_t **)(a+0x90u):NULL;
    uint8_t *combat=arb?*(uint8_t **)(a+0xa4u):NULL;
    if(memory(arb,0x64u) && *(void **)arb==(uint8_t *)image+0x2cc9acu &&
        *(void **)(arb+0x10u)==a && memory(combat,0x74u) &&
        *(void **)combat==(uint8_t *)image+0x2c8754u && *(void **)(combat+0x10u)==a)
        SudekiMpLogFormat("story_cast event=readiness character=%u kind=%u slot=%u arb50=%08lx arb58=%08lx arb60=%02x combat5c=%08lx combat72=%02x ms=%lu\r\n",
            i,kind,slot,(unsigned long)*(uint32_t *)(arb+0x50u),
            (unsigned long)*(uint32_t *)(arb+0x58u),arb[0x60u],
            (unsigned long)*(uint32_t *)(combat+0x5cu),combat[0x72u],(unsigned long)GetTickCount());
    SetLastError(error);
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
    trace_readiness(i,SUDEKIMP_STORY_ACTION_SKILL,slot);
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
    if(result.status==SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED && result.validation_result==2)
        return SUDEKIMP_STORY_ACTION_BUSY;
    return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
}
unsigned SudekiMpLanStoryCastSubmitSpirit(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key,unsigned variant) {
    if(!key || key->seat>=4u || variant<1u || variant>2u || !bound || stopping || fault ||
        submitting>=0 || !exact() || !SudekiMpLanStoryControlExact(w,r,key) ||
        r->actors[key->seat]!=caster[key->seat].actor) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    /* Spirit requires a neutral world and drained namespaces. It never
     * overlaps a skill, another Spirit, a cinematic or a native modal action. */
    uint8_t *speed=*(uint8_t **)((uint8_t *)image+0x408da0u);
    if(!memory(speed,0x2cu) || speed[0x28u] || *(uint16_t *)(speed+0x2au) ||
        *(int *)(speed+0x20u) || *(int *)(speed+0x24u) || !all_idle())
        return SUDEKIMP_STORY_ACTION_BUSY;
    unsigned i=key->seat;
    if(caster[i].sequence==UINT16_MAX) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    submitting=(int)i; submitting_spirit=TRUE;
    trace_readiness(i,SUDEKIMP_STORY_ACTION_SPIRIT,variant);
    SudekiMpSpiritActivationResult result=SudekiMpActivateCharacterSpirit(caster[i].actor,variant);
    if(result.status==SUDEKIMP_SPIRIT_ACTIVATION_STARTED) {
        /* Successful activation is an asynchronous obligation even when the
         * first observation fails. Never retry it or declare unknown idle. */
        BOOL active=FALSE;
        ++caster[i].sequence;
        caster[i].spirit_started=TRUE;
        caster[i].spirit_root_started=FALSE;
        caster[i].spirit_trace_at=0; caster[i].spirit_traces=0;
        caster[i].spirit_active_seen=SudekiMpObserveSpiritInstanceActivity(&caster[i].instance,&active) && active;
    }
    submitting_spirit=FALSE; submitting=-1;
    SudekiMpLogFormat("story_cast event=spirit_result character=%u variant=%u strike=%d status=%u validation=%d activate=%d\r\n",
        i,variant,result.strike_id,(unsigned)result.status,result.validation_result,result.activation_result);
    if(!SudekiMpSpiritActivationRoutingHealthy()) { fault=TRUE; return SUDEKIMP_STORY_ACTION_RETAINED; }
    if(result.status==SUDEKIMP_SPIRIT_ACTIVATION_STARTED) return SUDEKIMP_STORY_ACTION_STARTED;
    if(!SudekiMpLanStoryCastDrained(caster[i].actor)) return SUDEKIMP_STORY_ACTION_RETAINED;
    if(result.status==SUDEKIMP_SPIRIT_ACTIVATION_BUSY ||
        (result.status==SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED && result.validation_result==2))
        return SUDEKIMP_STORY_ACTION_BUSY;
    if(result.status==SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED && result.validation_result==1)
        return SUDEKIMP_STORY_ACTION_NO_SP; /* Shared native SSP; kind distinguishes the notice. */
    return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
}
void SudekiMpLanStoryCastRequestStop(void) { stopping=TRUE; }
BOOL SudekiMpLanStoryCastRetains(void) {
    return initialized || context_owned || bound || initializing || menu_owned || effects_owned || missiles_owned || visuals_owned || audio_owned;
}
BOOL SudekiMpLanStoryCastUninstall(void) {
    stopping=TRUE;
    if(!image) return TRUE;
    /* Destruction invokes retained_actor from the light owner. Rebase once
     * under a fresh name/roster proof before asking whether all tails drained. */
    SudekiMpLanStoryNativeRoster current;
    if(superseded_roster(&current)) owners=current;
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
    if(audio_owned && !SudekiMpUninstallLanArenaSpiritAudioTrace())return FALSE;
    audio_owned=FALSE;
    if(visuals_owned && !SudekiMpLanArenaSpiritVisualHostReset()) return FALSE;
    visuals_owned=FALSE;
    effects_owned=FALSE;
    if(missiles_owned && !SudekiMpLanPartyProjectileLifetimeUninstall()) return FALSE;
    missiles_owned=FALSE;
    for(unsigned i=4;i>0;--i) if(caster[i-1].instance.generation &&
        !SudekiMpDestroySpiritInstance(&caster[i-1].instance)) return FALSE;
    if(initialized && !SudekiMpResetSpiritInstanceAbi()) return FALSE;
    initialized=FALSE;
    if(context_owned && !SudekiMpUninstallLanCastContext()) return FALSE;
    context_owned=bound=bind_deferred=retire_superseded=FALSE; native_thread=last_service=0; lifetime=0;
    memset(caster,0,sizeof(caster)); memset(&owners,0,sizeof(owners)); image=NULL;
    return TRUE;
}

BOOL SudekiMpLanStoryCastSpiritInFlight(unsigned c,BOOL *active) {
    if(!active || c>=4 || !bound || !exact() || !caster[c].actor) return FALSE;
    BOOL native=FALSE;
    if(!SudekiMpObserveSpiritInstanceActivity(&caster[c].instance,&native)) return FALSE;
    *active=native || caster[c].spirit_started || (submitting_spirit && submitting==(int)c);
    return TRUE;
}
