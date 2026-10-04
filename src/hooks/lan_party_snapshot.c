#include "hooks/lan_party_snapshot.h"
#include "hooks/lan_party_cast.h"
#include "hooks/lan_party_runtime.h"
#include "engine/log.h"
#include "hooks/lan_arena_hit_feedback.h"
#include "network/lan_party_motion.h"
#include "network/lan_arena_tal_combo_graph.h"
#include "cleanroom/engine.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include <math.h>
#include <string.h>

static struct { void *actor; SudekiMpLanArenaLocomotion phase; } captured[4];
typedef struct PartyActionCapture {
    void *actor;
    uint16_t sequence;
    uint8_t active,variant,count;
    SudekiMpLanArenaActionEvent history[SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY];
} PartyActionCapture;
static PartyActionCapture actions[4];
static struct {
    void *actor, *skill, *task;
    uint16_t sequence;
    uint8_t slot, active;
    uint32_t cost;
} buki_skill_capture;
typedef struct AilishReloadCapture {
    SudekiMpLanPartyLease lease;
    void *actor;
    uint16_t sequence;
    uint8_t active;
} AilishReloadCapture;
static AilishReloadCapture ailish_reload_capture;
static struct {
    void *actor;
    SudekiMpLanPartyRangedPresentation value;
} ranged_capture[2];
typedef struct PartyShotCapture {
    void *actor;
    SudekiMpLanPartyLease lease;
    SudekiMpLanWeaponState journal;
    uint16_t sequence;
} PartyShotCapture;
static PartyShotCapture ranged_shots[2];
static SudekiMpLanPartySession *capture_session;
static SudekiMpLanPartyRosterObservation capture_roster;
void SudekiMpLanPartyCaptureReset(void) {
    memset(captured,0,sizeof(captured)); memset(actions,0,sizeof(actions));
    memset(&ailish_reload_capture,0,sizeof(ailish_reload_capture));
    memset(&buki_skill_capture,0,sizeof(buki_skill_capture));
    memset(ranged_capture,0,sizeof(ranged_capture));
    memset(ranged_shots,0,sizeof(ranged_shots));
    capture_session=NULL; memset(&capture_roster,0,sizeof(capture_roster));
}

void SudekiMpLanPartyCaptureRangedShot(const SudekiMpLanPartyLease *lease,void *actor) {
    SudekiMpElcoWeaponObservation weapon;
    SudekiMpCharacterSkillState skill;
    SudekiMpLanWeaponShot *shot;
    /* The exact host emission callback supplies a world/actor key, independent
     * of which connection currently controls this actor. The observer never
     * turns a client input or ownership transition into a projectile event. */
    if(!lease || (lease->seat!=1u && lease->seat!=3u) || !capture_session ||
        lease->token!=(uint64_t)(uintptr_t)capture_session || lease->generation!=1u ||
        SudekiMpLanPartyLocalSeat(capture_session)!=0u ||
        actor!=capture_roster.actors[lease->seat] ||
        !SudekiMpLanPartyControlNativeActorExact(&capture_roster,lease->seat) ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !SudekiMpObserveRangedWeapon(actor,SudekiMpLanPartyActorType(lease->seat),&weapon) ||
        weapon.charge<weapon.required_charge) return;
    PartyShotCapture *capture=&ranged_shots[lease->seat==1u?0u:1u];
    if(capture->actor!=actor || capture->lease.token!=lease->token ||
        capture->lease.generation!=lease->generation) {
        /* Actor replacement or a new world retires the old journal. A mere
         * player reassignment keeps this world-owned shot history intact. */
        uint16_t sequence=capture->actor==actor?capture->sequence:0u;
        memset(capture,0,sizeof(*capture));
        capture->actor=actor; capture->lease=*lease;
        capture->sequence=sequence;
    }
    if(capture->journal.shot_count==SUDEKIMP_LAN_WEAPON_SHOT_HISTORY) {
        memmove(capture->journal.shots,capture->journal.shots+1,
            (SUDEKIMP_LAN_WEAPON_SHOT_HISTORY-1u)*sizeof(*shot));
        --capture->journal.shot_count;
    }
    shot=&capture->journal.shots[capture->journal.shot_count++];
    if(++capture->sequence==0u) ++capture->sequence;
    shot->sequence=capture->sequence; shot->item=weapon.item;
    shot->pre_charge_q8=(uint16_t)lroundf(weapon.charge*256.0f);
    shot->host_tick=GetTickCount();
    SudekiMpLogFormat("lan_party event=ranged_host_emission actor_seat=%u sequence=%u item=%u charge_q8=%u tick=%lu\r\n",
        lease->seat,shot->sequence,shot->item,shot->pre_charge_q8,(unsigned long)shot->host_tick);
}

static BOOL readable(const void *p,size_t bytes) {
    MEMORY_BASIC_INFORMATION m; uintptr_t start=(uintptr_t)p;
    return p && bytes && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && start+bytes>=start &&
        start+bytes<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL party_native_body_ready(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w,BOOL release) {
    SudekiMpCharacterSkillState skill; int spirit; BOOL weapon_pending=FALSE;
    void *task;
    if(!key || key->seat>=4 || !actor ||
        !(release ? SudekiMpLanPartyCastDrained(actor):SudekiMpLanPartyCastBodyIdle(actor)) ||
        !(w?(w->service_post_original_exact &&
            SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)):
            SudekiMpLanPartyPresentationBoundary()) ||
        SudekiMpLanPartyControlObserveActor(w,key->seat)!=actor ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&weapon_pending) || weapon_pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !readable(skill.skill,0x78)) return FALSE;
    task=*(void **)((uint8_t *)skill.skill+0x74u);
    /* CSkill's native update (RVA b47a0, this adjusted by +18) accepts a
     * retained reference cell whose thread pointer is null as completed.
     * The cell itself commonly survives the cast; requiring it to disappear
     * stalls every subsequent snapshot and prevents native ownership drain. */
    if(task && (!readable(task,8u) || *(void **)task ||
        !*((uint32_t *)task+1))) return FALSE;
    if(!readable(actor,0xdcu) ||
        *(void **)((uint8_t *)actor+0xd8u)!=skill.skill ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor ||
        ((uint8_t *)skill.skill)[0x6cu] ||
        *(void **)((uint8_t *)skill.skill+0x74u)!=task) return FALSE;
    return SudekiMpLanPartyControlObserveActor(w,key->seat)==actor &&
        (w?SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w):
            SudekiMpLanPartyPresentationBoundary());
}
BOOL SudekiMpLanPartyMovementDrained(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    return party_native_body_ready(key,actor,w,TRUE);
}
BOOL SudekiMpLanPartyCombatPresentationReady(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    /* The fade owns only its private light object after native body cleanup.
     * Keep every actor/task/weapon/witness check and its teardown lease, while
     * allowing fresh positions to be captured and rendered during that tail. */
    return party_native_body_ready(key,actor,w,FALSE);
}
BOOL SudekiMpLanPartyCaptureMovement(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanPartySession *session,uint32_t now,SudekiMpLanPartyFrame *out) {
    SudekiMpLanPartyRosterObservation before,after; SudekiMpLanPartyFrame frame;
    if(!out || SudekiMpLanPartyLocalSeat(session)!=0 ||
        !SudekiMpLanPartyControlObserveRoster(w,&before) ||
        before.present_mask!=15 || before.combat) return FALSE;
    capture_session=session; capture_roster=before;
    memset(&frame,0,sizeof(frame));
    for(unsigned seat=0;seat<4;++seat) {
        SudekiMpCleanroomActor actor; SudekiMpCleanroomActorPresentation native;
        SudekiMpLanPartyLease observation={1,1,(uint8_t)seat};
        SudekiMpLanArenaSnapshot *chunk=&frame.chunk[seat/2];
        SudekiMpLanArenaActorSnapshot *s=&chunk->seat[seat%2];
        SudekiMpWeaponQuickList weapons; float position[3],facing[2],hp,sp;
        s->actor_type=SudekiMpLanPartyActorType(seat); s->native_entity_id=s->actor_type;
        if(!SudekiMpCleanroomActorFromType(s->actor_type,&actor) ||
            !SudekiMpLanPartyMovementDrained(&observation,before.actors[seat],w) ||
            !SudekiMpCleanroomEngineActorPosition(actor,position) ||
            !SudekiMpCleanroomEngineActorFacing(actor,facing) ||
            !SudekiMpCleanroomEngineActorResources(actor,&hp,&sp) ||
            !SudekiMpCleanroomEngineWorldMotion(actor,&native) ||
            !SudekiMpLanPartyMotionObserve(s->actor_type,native.selector[0],&s->animation_state) ||
            !isfinite(hp) || !isfinite(sp) || hp<0 || sp<0 || hp>65535 || sp>65535 ||
            !SudekiMpDescribeCharacterWeapons(before.actors[seat],&weapons)) return FALSE;
        if(!SudekiMpLanPartyMotionCapture(s->actor_type,native.selector,native.state,
                native.rate,native.time,native.blend,
                captured[seat].actor==before.actors[seat]?&captured[seat].phase:NULL,
                &s->locomotion)) return FALSE;
        s->x=position[0]; s->y=position[1]; s->z=position[2];
        s->facing_x=facing[0]; s->facing_z=facing[1]; s->hp=(uint16_t)hp; s->sp=(uint16_t)sp;
        for(unsigned i=0;i<weapons.row_count && i<12;++i)
            if(weapons.rows[i].equipped) s->weapon_slot_plus_one=(uint8_t)(i+1);
        chunk->host_tick=now; chunk->match_state=SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    }
    /* A host collision fixture must be visible before combat entry too.
     * Capture its real position/resources; noncombat carries no hit journal. */
    float dummy_position[3],dummy_hp;
    if(!SudekiMpCleanroomEngineDummySnapshot(dummy_position,&dummy_hp) ||
        !isfinite(dummy_hp) || dummy_hp<0 ||
        dummy_hp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE) return FALSE;
    SudekiMpLanArenaEnemySnapshot *dummy=&frame.chunk[0].enemies[0];
    frame.chunk[0].enemy_count=1u;
    dummy->native_entity_id=SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    dummy->x=dummy_position[0]; dummy->y=dummy_position[1]; dummy->z=dummy_position[2];
    dummy->hp=(uint32_t)(dummy_hp+0.5f);
    dummy->combat_state=dummy_hp<=0.0f?
        SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED:SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    if(!SudekiMpLanPartyMovementFrameValid(&frame) ||
        !SudekiMpLanPartyControlObserveRoster(w,&after) || after.combat ||
        before.group!=after.group || before.controller!=after.controller ||
        before.present_mask!=after.present_mask) return FALSE;
    for(unsigned i=0;i<4;++i) if(before.actors[i]!=after.actors[i]) return FALSE;
    for(unsigned i=0;i<4;++i) {
        captured[i].actor=before.actors[i]; captured[i].phase=frame.chunk[i/2].seat[i%2].locomotion;
    }
    *out=frame; return TRUE;
}

static uint8_t party_anim_id(void *actor) {
    uint8_t *model,*state;
    if(!readable(actor,0x138u)) return 0;
    model=*(uint8_t **)((uint8_t *)actor+0x130u);
    if(!readable(model,0x100u) || *(void **)(model+0x10u)!=actor) return 0;
    state=*(uint8_t **)(model+0xf8u);
    if(!readable(state,3u) || state[2u]>=0xc4u) return 0;
    return state[2u];
}
static BOOL party_actor_action(unsigned seat,void *actor,
    const SudekiMpCleanroomActorPresentation *native,int ranged_selector,
    uint8_t ranged_state,float ranged_time,uint8_t *variant,
    float *phase) {
    uint8_t type=SudekiMpLanPartyActorType(seat),v=SUDEKIMP_LAN_ARENA_ACTION_NONE;
    int selector; uint8_t state; unsigned channel; float action_time;
    if(!native || !variant || !phase) return FALSE;
    if(actions[seat].actor!=actor) {
        memset(&actions[seat],0,sizeof(actions[seat])); actions[seat].actor=actor;
    }
    selector=native->selector[0]; state=native->state[0];
    if(type==SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
        uint8_t anim=party_anim_id(actor);
        if(selector==70) {
            /* LA40 already carries this authored failed-combo body as clip28.
             * It has no accepted attack variant. Keep its real four channels
             * below instead of withholding the entire four-player frame. */
            v=SUDEKIMP_LAN_ARENA_ACTION_NONE;
        } else if(selector!=SudekiMpLanPartyCombatMotionSelector(type,1u) &&
            selector!=SudekiMpLanPartyCombatMotionSelector(type,2u) &&
            selector!=SudekiMpLanPartyCombatMotionSelector(type,3u)) {
            if(!anim && state!=128u) return FALSE;
            if(!SudekiMpLanArenaBukiActionFromNativeAnimation(anim,selector,state,&v)) {
                int expected_selector,expected_state;
                if(state!=128u || !actions[seat].active ||
                    !SudekiMpLanArenaBukiActionToNativePresentation(
                        actions[seat].variant,&expected_selector,&expected_state) ||
                    expected_selector!=selector) return FALSE;
                v=actions[seat].variant;
            }
        }
        channel=0;
    } else if(type==SUDEKIMP_LAN_ARENA_TAL_TYPE) {
        if(selector!=SudekiMpLanPartyCombatMotionSelector(type,1u) &&
            selector!=SudekiMpLanPartyCombatMotionSelector(type,2u) &&
            selector!=SudekiMpLanPartyCombatMotionSelector(type,3u) && selector!=3 &&
            !SudekiMpLanPartyTalActionObserve(selector,state,&v)) {
            if(state==128u && actions[seat].active &&
                SudekiMpLanPartyTalActionObserve(selector,1u,&v) &&
                v==actions[seat].variant) {
                /* Tal's terminal selector remains visible through state 128. */
            } else return FALSE;
        }
        channel=0;
    } else {
        channel=4;
        if(!SudekiMpLanPartyRangedActionObserve(type,ranged_selector,
                ranged_state,&v)) return FALSE;
    }
    action_time=channel==4u?ranged_time:native->time[channel];
    if(v!=SUDEKIMP_LAN_ARENA_ACTION_NONE &&
        (!isfinite(action_time) || action_time<0.0f)) return FALSE;
    if(v!=SUDEKIMP_LAN_ARENA_ACTION_NONE &&
        (!actions[seat].active || actions[seat].variant!=v)) {
        PartyActionCapture *a=&actions[seat];
        unsigned index;
        if(++a->sequence==0u) ++a->sequence;
        if(a->count==SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY) {
            memmove(a->history,a->history+1,
                (SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY-1u)*sizeof(a->history[0]));
            --a->count;
        }
        index=a->count++;
        a->history[index].sequence=a->sequence;
        a->history[index].variant=v;
        a->history[index].host_tick=GetTickCount();
    }
    actions[seat].active=v!=SUDEKIMP_LAN_ARENA_ACTION_NONE;
    actions[seat].variant=v;
    *variant=v;
    *phase=v==SUDEKIMP_LAN_ARENA_ACTION_NONE?0.0f:action_time;
    return TRUE;
}
BOOL SudekiMpLanPartyCombatActionsDrained(const SudekiMpLanPartyLease *key,
    void *actor,const SudekiMpControlUpdateDispatchWitness *w) {
    SudekiMpCleanroomActor native_actor;
    SudekiMpCleanroomActorPresentation native;
    uint8_t type,action; BOOL combat=FALSE;
    int ranged_selector=0; uint8_t ranged_state=0;
    float ranged_time=0.0f,phase;
    if(!key || key->seat>=4u || !actor ||
        SudekiMpLanPartyControlObserveActor(w,key->seat)!=actor) return FALSE;
    if(!SudekiMpCleanroomEngineCombatMode(&combat)) return FALSE;
    if(!combat && !actions[key->seat].active) return TRUE;
    type=SudekiMpLanPartyActorType(key->seat);
    if(!SudekiMpCleanroomActorFromType(type,&native_actor) ||
        SudekiMpCleanroomEngineActorEntity(native_actor)!=actor ||
        !SudekiMpCleanroomEngineWorldMotion(native_actor,&native)) return FALSE;
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) {
        if(!SudekiMpCleanroomEngineRangedActionPresentation(native_actor,actor,
                &ranged_selector,&ranged_state,&ranged_time)) return FALSE;
        return SudekiMpLanPartyRangedActionChannelDrained(
            type,ranged_selector,ranged_state);
    }
    if(!party_actor_action(key->seat,actor,&native,ranged_selector,
            ranged_state,ranged_time,&action,&phase)) return FALSE;
    /* Failure is a publishable body pose, not permission to restore native AI
     * in the middle of Buki's recovery. Wait for the actual idle transition. */
    return action==SUDEKIMP_LAN_ARENA_ACTION_NONE &&
        !(type==SUDEKIMP_LAN_ARENA_BUKI_TYPE && native.selector[0]==70);
}
static BOOL combat_capture_rejected(const char *reason,unsigned seat,uint32_t now) {
    static DWORD last_trace;
    if(!last_trace || (DWORD)(now-last_trace)>=2000u) {
        SudekiMpLogFormat("lan_party event=combat_capture_rejected stage=%s seat=%u tick=%lu\r\n",
            reason,seat,(unsigned long)now);
        last_trace=now;
    }
    return FALSE;
}
/* Capture admission is separate from ownership release. The bound cast owner
 * may publish its native body while running and hand it back after body/task
 * cleanup; an independent lighting tail retains its own lifetime. Unknown
 * native work still fails closed. The pre-cast fallback admits only Buki. */
static BOOL party_capture_skill(unsigned seat,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpCharacterSkillState *skill) {
    void *task;
    BOOL pending=FALSE;
    int spirit;
    if(!w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        SudekiMpLanPartyControlObserveActor(w,seat)!=actor ||
        !SudekiMpObserveCharacterSkill(actor,skill)) return FALSE;
    if(SudekiMpLanPartyCastReady() && SudekiMpLanPartyCastActive(actor)) return TRUE;
    if(!skill->active)
        return SudekiMpLanPartyCombatPresentationReady(
            &(SudekiMpLanPartyLease){1,1,(uint8_t)seat},actor,w);
    if((seat!=0u && !SudekiMpLanPartyCastReady()) || skill->slot<0 || skill->slot>=6 ||
        skill->cost>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !readable(skill->skill,0x78u)) return FALSE;
    task=*(void **)((uint8_t *)skill->skill+0x74u);
    return readable(task,8u) && *(void **)task && *((uint32_t *)task+1) &&
        *(void **)((uint8_t *)actor+0xd8u)==skill->skill &&
        *(void **)((uint8_t *)skill->skill+0x10u)==actor &&
        ((uint8_t *)skill->skill)[0x6cu] &&
        SudekiMpLanPartyControlObserveActor(w,seat)==actor &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static BOOL party_capture_buki_skill(void *actor,
    const SudekiMpCharacterSkillState *skill,
    const SudekiMpCleanroomActorPresentation *native,
    SudekiMpLanArenaActorSnapshot *s) {
    void *task=*(void **)((uint8_t *)skill->skill+0x74u);
    if(buki_skill_capture.actor!=actor || buki_skill_capture.skill!=skill->skill) {
        memset(&buki_skill_capture,0,sizeof(buki_skill_capture));
        buki_skill_capture.actor=actor; buki_skill_capture.skill=skill->skill;
    }
    if(skill->active) {
        if(!buki_skill_capture.active || buki_skill_capture.task!=task ||
            buki_skill_capture.slot!=(uint8_t)skill->slot) {
            if(++buki_skill_capture.sequence==0u) ++buki_skill_capture.sequence;
        }
        buki_skill_capture.task=task;
        buki_skill_capture.slot=(uint8_t)skill->slot;
        buki_skill_capture.cost=skill->cost;
        s->skill_presentation_valid=1;
        s->skill_presentation_channel_count=4;
        for(unsigned c=0;c<4u;++c) {
            s->skill_presentation_selector[c]=native->selector[c];
            s->skill_presentation_state[c]=native->state[c];
            s->skill_presentation_rate[c]=native->rate[c];
            s->skill_presentation_time[c]=native->time[c];
        }
        /* Buki owns three blends; the fourth wire field stays canonical zero. */
        memcpy(s->skill_presentation_blend,native->blend,3u*sizeof(float));
    }
    buki_skill_capture.active=skill->active;
    if(buki_skill_capture.sequence) {
        s->skill_sequence=buki_skill_capture.sequence;
        s->skill_kind=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
        s->skill_slot=buki_skill_capture.slot;
        s->skill_cost=buki_skill_capture.cost;
        s->skill_active=skill->active;
    }
    return SudekiMpLanArenaSkillPresentationValid(s,s->actor_type);
}
static BOOL party_capture_combat_actor(unsigned seat,void *native_actor,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanArenaInput *input,
    uint32_t now,SudekiMpLanArenaActorSnapshot *s,
    SudekiMpLanPartySession *session,SudekiMpLanPartyRangedPresentation *ranged) {
    SudekiMpCleanroomActor actor;
    SudekiMpCleanroomActorPresentation native;
    SudekiMpWeaponQuickList weapons;
    SudekiMpElcoWeaponObservation weapon;
    SudekiMpCharacterSkillState skill;
    float position[3],facing[2],hp,sp,action_phase;
    int selectors[4],ranged_selector=0; uint8_t ranged_state=0,action;
    float ranged_time=0.0f; uint8_t type=SudekiMpLanPartyActorType(seat);
    int idle=SudekiMpLanPartyCombatMotionSelector(type,1);
    if(idle<0 || !SudekiMpCleanroomActorFromType(type,&actor))
        return combat_capture_rejected("actor_type",seat,now);
    if(!party_capture_skill(seat,native_actor,w,&skill))
        return combat_capture_rejected("actor_skill_drain",seat,now);
    if(!SudekiMpCleanroomEngineActorPosition(actor,position))
        return combat_capture_rejected("actor_position",seat,now);
    if(!SudekiMpCleanroomEngineActorFacing(actor,facing))
        return combat_capture_rejected("actor_facing",seat,now);
    if(!SudekiMpCleanroomEngineActorResources(actor,&hp,&sp) ||
        !isfinite(hp) || !isfinite(sp) || hp<0 || sp<0 || hp>65535 || sp>65535)
        return combat_capture_rejected("actor_resources",seat,now);
    if(!SudekiMpCleanroomEngineWorldMotion(actor,&native))
        return combat_capture_rejected("actor_world_motion",seat,now);
    if(!SudekiMpDescribeCharacterWeapons(native_actor,&weapons))
        return combat_capture_rejected("actor_weapons",seat,now);
    if(SudekiMpLanPartyCastReady() && !SudekiMpLanPartyCastCapture(seat,s))
        return combat_capture_rejected("cast_capture",seat,now);
    BOOL casting=s->skill_active || skill.active;
    if(!casting && (type==SUDEKIMP_LAN_ARENA_ELCO_TYPE || type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) &&
        !SudekiMpCleanroomEngineRangedActionPresentation(actor,native_actor,
            &ranged_selector,&ranged_state,&ranged_time))
        return combat_capture_rejected("ranged_channel",seat,now);
    if(ranged && !casting) {
        SudekiMpCleanroomActorPresentation firing;
        unsigned index=seat==1u?0u:1u;
        SudekiMpLanPartyRangedPresentation *prior=&ranged_capture[index].value;
        int clip;
        if(!SudekiMpCleanroomEngineRangedWorldPresentation(actor,native_actor,&firing) ||
            (clip=SudekiMpLanPartyRangedClip(type,firing.selector[4]))<0)
            return combat_capture_rejected("ranged_pose",seat,now);
        ranged->valid=1u; ranged->held=input && input->ranged_first_person_active &&
            input->weak_attack_held;
        ranged->clip=(uint8_t)clip; ranged->state=firing.state[4];
        ranged->rate=firing.rate[4]; ranged->time=firing.time[4]; ranged->blend=firing.blend[3];
        ranged->sequence=ranged_capture[index].actor==native_actor?prior->sequence:0u;
        if(!ranged->sequence || ranged->clip!=prior->clip || ranged->state!=prior->state ||
            ranged->time<prior->time)
            if(++ranged->sequence==0u) ++ranged->sequence;
        if(!SudekiMpLanPartyRangedPresentationValid(ranged))
            return combat_capture_rejected("ranged_pose_values",seat,now);
        ranged_capture[index].actor=native_actor; *prior=*ranged;
    }
    action=SUDEKIMP_LAN_ARENA_ACTION_NONE; action_phase=0.0f;
    if(casting) actions[seat].active=FALSE;
    if(!casting && !party_actor_action(seat,native_actor,&native,ranged_selector,ranged_state,
            ranged_time,&action,&action_phase))
        return combat_capture_rejected("action_observation",seat,now);
    s->actor_type=type; s->native_entity_id=type;
    s->x=position[0]; s->y=position[1]; s->z=position[2];
    s->facing_x=facing[0]; s->facing_z=facing[1];
    s->hp=(uint16_t)hp; s->sp=(uint16_t)sp;
    s->action_variant=action;
    s->animation_state=action?SUDEKIMP_LAN_ARENA_ANIMATION_ACTION:
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    if(action) {
        if(!SudekiMpLanArenaActionCombatState(action,&s->combat_state) ||
            !isfinite(action_phase) || action_phase<0.0f ||
            action_phase>65535.0f/SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE) return FALSE;
        s->action_phase_valid=1;
        s->action_phase_q8=(uint16_t)(action_phase*
            SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE+0.5f);
    } else s->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    s->action_sequence=actions[seat].sequence;
    s->action_history_count=actions[seat].count;
    memcpy(s->action_history,actions[seat].history,sizeof(s->action_history));
    s->anim_id=type==SUDEKIMP_LAN_ARENA_BUKI_TYPE?party_anim_id(native_actor):0;
    for(unsigned i=0;i<4;++i) selectors[i]=native.selector[i];
    if(!SudekiMpLanPartyCastReady() && seat==0u && !party_capture_buki_skill(native_actor,&skill,&native,s))
        return combat_capture_rejected("buki_skill_presentation",seat,now);
    /* SMP4 v4 carries the complete native melee result: current/outgoing
     * clips, authored rates, phases and blends. Semantic combo metadata still
     * drives the HUD, but never replaces that result with an idle/24fps pose. */
    if(type==SUDEKIMP_LAN_ARENA_AILISH_TYPE && native.selector[0]==12) {
        /* The native combat entry clip is a transition, not locomotion. Let
         * the replica run its own validated weapon-arm event, then publish a
         * stable idle layer while the transition completes. */
        selectors[0]=idle;
        native.state[0]=128u; native.rate[0]=12.0f; native.time[0]=0.0f;
    }
    if(!casting && !SudekiMpLanPartyCombatMotionCapture(type,selectors,native.state,
            native.rate,native.time,native.blend,
            captured[seat].actor==native_actor?&captured[seat].phase:NULL,
            &s->locomotion)) return combat_capture_rejected("motion_capture",seat,now);
    for(unsigned i=0;i<weapons.row_count && i<12u;++i)
        if(weapons.rows[i].equipped) { s->weapon_slot_plus_one=(uint8_t)(i+1u); break; }
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE) {
        if(!SudekiMpObserveElcoWeapon(native_actor,&weapon))
            return combat_capture_rejected("elco_weapon",seat,now);
        if(!isfinite(weapon.charge) || weapon.charge<0 || weapon.charge>100 ||
            !isfinite(weapon.reload_seconds) || weapon.reload_seconds<0 ||
            weapon.reload_seconds>60) return FALSE;
        if(ranged_shots[0].actor==native_actor &&
            ranged_shots[0].lease.token==(uint64_t)(uintptr_t)session &&
            ranged_shots[0].lease.generation==1u && ranged_shots[0].lease.seat==1u)
            s->weapon=ranged_shots[0].journal;
        s->weapon.valid=1; s->weapon.item=weapon.item; s->weapon.stage=weapon.stage;
        s->weapon.charge_q8=(uint16_t)(weapon.charge*256.0f+0.5f);
        s->weapon.reload_ms=(uint16_t)ceilf(weapon.reload_seconds*1000.0f);
    }
    if((type==SUDEKIMP_LAN_ARENA_ELCO_TYPE || type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) &&
        input && input->ranged_first_person_active &&
        (input->aim_direction_x || input->aim_direction_y || input->aim_direction_z)) {
        float aim[3]={input->aim_direction_x==INT16_MIN?-1.0f:input->aim_direction_x/32767.0f,
            input->aim_direction_y==INT16_MIN?-1.0f:input->aim_direction_y/32767.0f,
            input->aim_direction_z==INT16_MIN?-1.0f:input->aim_direction_z/32767.0f};
        float norm=sqrtf(aim[0]*aim[0]+aim[1]*aim[1]+aim[2]*aim[2]);
        if(!isfinite(norm) || norm<0.5f || norm>1.5f) return FALSE;
        s->ranged_aim_valid=1;
        for(unsigned i=0;i<3;++i)
            s->ranged_aim[i]=(int16_t)lroundf(aim[i]*32767.0f/norm);
        if(input->aim_target_valid) {
            s->ranged_target_valid=1;
            memcpy(s->ranged_target,input->aim_target,sizeof(s->ranged_target));
        }
    }
    captured[seat].actor=native_actor; captured[seat].phase=s->locomotion;
    (void)now;
    return TRUE;
}
static BOOL party_capture_ailish_weapon_state(
    const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanPartySession *session,void *actor,
    SudekiMpLanPartyAilishWeaponState *state) {
    SudekiMpLanPartyLease world={(uint64_t)(uintptr_t)session,1u,3u};
    BOOL active;
    if(state) ZeroMemory(state,sizeof(*state));
    if(!state || !actor || !session || SudekiMpLanPartyLocalSeat(session)!=0u ||
        SudekiMpLanPartyControlObserveActor(w,3u)!=actor ||
        !SudekiMpLanPartyControlObserveAilishWorldWeapon(w,actor,state))
        return FALSE;
    if(ailish_reload_capture.actor!=actor ||
        ailish_reload_capture.lease.token!=world.token ||
        ailish_reload_capture.lease.generation!=world.generation ||
        ailish_reload_capture.sequence==0u) {
        ailish_reload_capture.lease=world;
        ailish_reload_capture.actor=actor;
        ailish_reload_capture.sequence=1u;
        ailish_reload_capture.active=FALSE;
    }
    active=state->reload_ms!=0u;
    if(active && !ailish_reload_capture.active &&
        ++ailish_reload_capture.sequence==0u)
        ++ailish_reload_capture.sequence;
    ailish_reload_capture.active=active;
    state->reload_sequence=ailish_reload_capture.sequence;
    const PartyShotCapture *shots=&ranged_shots[1];
    if(shots->actor==actor && shots->lease.token==world.token &&
        shots->lease.generation==world.generation && shots->lease.seat==3u) {
        state->shot_count=shots->journal.shot_count;
        memcpy(state->shots,shots->journal.shots,sizeof(state->shots));
    }
    return TRUE;
}
BOOL SudekiMpLanPartyCaptureBasicCombat(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanPartySession *session,SudekiMpLanPartyHostControl *host,uint32_t now,
    SudekiMpLanPartyFrame *out) {
    SudekiMpLanPartyRosterObservation before,after; SudekiMpLanPartyFrame frame;
    float position[3],hp;
    if(!out || SudekiMpLanPartyLocalSeat(session)!=0 ||
        !SudekiMpLanPartyControlObserveRoster(w,&before) ||
        before.present_mask!=15 || !before.combat) return FALSE;
    capture_session=session; capture_roster=before;
    memset(&frame,0,sizeof(frame));
    for(unsigned seat=0;seat<4;++seat) {
        SudekiMpLanArenaSnapshot *chunk=&frame.chunk[seat/2];
        SudekiMpLanArenaActorSnapshot *s=&chunk->seat[seat%2];
        SudekiMpLanArenaInput input; const SudekiMpLanArenaInput *accepted=NULL;
        if(SudekiMpLanPartyCharacterPlayer(session,seat)==0u &&
            SudekiMpLanPartyRuntimeHostLocalInput(w,seat,now,&input)) accepted=&input;
        else if(host && SudekiMpLanPartyHostControlLatestInput(host,w,seat,now,&input))
            accepted=&input;
        if(!party_capture_combat_actor(seat,before.actors[seat],w,accepted,now,s,session,
            seat==1u?&frame.ranged[0]:seat==3u?&frame.ranged[1]:NULL)) return FALSE;
        chunk->host_tick=now; chunk->match_state=SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
        chunk->combat_enabled=1u;
    }
    if(!SudekiMpCleanroomEngineDummySnapshot(position,&hp) || !isfinite(hp) ||
        hp<0 || hp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE)
        return combat_capture_rejected("dummy_observation",0u,now);
    frame.chunk[0].enemy_count=1u;
    frame.chunk[0].enemies[0].native_entity_id=SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    frame.chunk[0].enemies[0].x=position[0]; frame.chunk[0].enemies[0].y=position[1];
    frame.chunk[0].enemies[0].z=position[2];
    frame.chunk[0].enemies[0].hp=(uint32_t)(hp+0.5f);
    frame.chunk[0].enemies[0].combat_state=hp<=0.0f?
        SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED:SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    SudekiMpLanHitHostSnapshot(&frame.chunk[0].enemies[0]);
    if(!SudekiMpLanPartyBasicCombatFrameValid(&frame))
        return combat_capture_rejected("combat_frame_validation",4u,now);
    if(!SudekiMpLanPartyControlObserveRoster(w,&after) || !after.combat ||
        before.group!=after.group || before.controller!=after.controller ||
        before.present_mask!=after.present_mask) return FALSE;
    for(unsigned seat=0;seat<4;++seat)
        if(before.actors[seat]!=after.actors[seat]) return FALSE;
    (void)party_capture_ailish_weapon_state(w,session,before.actors[3],
        &frame.ailish_weapon);
    if(!SudekiMpLanPartyFrameValid(&frame))
        return combat_capture_rejected("weapon_sidecar_validation",4u,now);
    *out=frame; return TRUE;
}
