#include "hooks/lan_party_snapshot.h"
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
typedef struct AilishReloadCapture {
    SudekiMpLanPartyLease lease;
    void *actor;
    uint16_t sequence;
    uint8_t active;
} AilishReloadCapture;
static AilishReloadCapture ailish_reload_capture;
void SudekiMpLanPartyCaptureReset(void) {
    memset(captured,0,sizeof(captured)); memset(actions,0,sizeof(actions));
    memset(&ailish_reload_capture,0,sizeof(ailish_reload_capture));
}

static BOOL readable(const void *p,size_t bytes) {
    MEMORY_BASIC_INFORMATION m; uintptr_t start=(uintptr_t)p;
    return p && bytes && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && start+bytes>=start &&
        start+bytes<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
BOOL SudekiMpLanPartyMovementDrained(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    SudekiMpCharacterSkillState skill; int spirit; BOOL weapon_pending=FALSE;
    if(!key || key->seat>=4 || !actor ||
        !(w?(w->service_post_original_exact &&
            SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)):
            SudekiMpLanPartyPresentationBoundary()) ||
        SudekiMpLanPartyControlObserveActor(w,key->seat)!=actor ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&weapon_pending) || weapon_pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !readable(skill.skill,0x78) || *(void **)((uint8_t *)skill.skill+0x74)) return FALSE;
    return SudekiMpLanPartyControlObserveActor(w,key->seat)==actor &&
        (w?SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w):
            SudekiMpLanPartyPresentationBoundary());
}
BOOL SudekiMpLanPartyCaptureMovement(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanPartySession *session,uint32_t now,SudekiMpLanPartyFrame *out) {
    SudekiMpLanPartyRosterObservation before,after; SudekiMpLanPartyFrame frame;
    if(!out || SudekiMpLanPartyLocalSeat(session)!=0 ||
        !SudekiMpLanPartyControlObserveRoster(w,&before) ||
        before.present_mask!=15 || before.combat) return FALSE;
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
        if(selector!=20) {
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
        if(selector!=17 && selector!=3 &&
            !SudekiMpLanArenaTalActionFromNativePresentation(selector,state,&v)) {
            if(state==128u && actions[seat].active &&
                SudekiMpLanArenaTalActionFromNativePresentation(selector,1u,&v) &&
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
    return action==SUDEKIMP_LAN_ARENA_ACTION_NONE;
}
static BOOL party_capture_combat_actor(unsigned seat,void *native_actor,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanArenaInput *input,
    uint32_t now,SudekiMpLanArenaActorSnapshot *s) {
    SudekiMpCleanroomActor actor;
    SudekiMpCleanroomActorPresentation native;
    SudekiMpWeaponQuickList weapons;
    SudekiMpElcoWeaponObservation weapon;
    float position[3],facing[2],hp,sp,action_phase;
    int selectors[4],ranged_selector=0; uint8_t ranged_state=0,action;
    float ranged_time=0.0f; uint8_t type=SudekiMpLanPartyActorType(seat);
    int idle=SudekiMpLanPartyCombatMotionSelector(type,1);
    if(idle<0 || !SudekiMpCleanroomActorFromType(type,&actor) ||
        !SudekiMpLanPartyMovementDrained(&(SudekiMpLanPartyLease){1,1,(uint8_t)seat},
            native_actor,w) ||
        !SudekiMpCleanroomEngineActorPosition(actor,position) ||
        !SudekiMpCleanroomEngineActorFacing(actor,facing) ||
        !SudekiMpCleanroomEngineActorResources(actor,&hp,&sp) ||
        !SudekiMpCleanroomEngineWorldMotion(actor,&native) ||
        !SudekiMpDescribeCharacterWeapons(native_actor,&weapons) ||
        !isfinite(hp) || !isfinite(sp) || hp<0 || sp<0 || hp>65535 || sp>65535)
        return FALSE;
    if((type==SUDEKIMP_LAN_ARENA_ELCO_TYPE || type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) &&
        !SudekiMpCleanroomEngineRangedActionPresentation(actor,native_actor,
            &ranged_selector,&ranged_state,&ranged_time)) return FALSE;
    if(!party_actor_action(seat,native_actor,&native,ranged_selector,ranged_state,
            ranged_time,&action,&action_phase)) return FALSE;
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
    /* Melee primary selector becomes the semantic action channel while an
     * action is active. The actor-specific action variant carries that exact
     * clip; keep the locomotion layer on a verified combat idle underneath. */
    if(action && (type==SUDEKIMP_LAN_ARENA_BUKI_TYPE ||
                  type==SUDEKIMP_LAN_ARENA_TAL_TYPE)) {
        selectors[0]=idle;
        native.state[0]=128u; native.rate[0]=12.0f; native.time[0]=0.0f;
    } else if((type==SUDEKIMP_LAN_ARENA_TAL_TYPE && native.selector[0]==3) ||
        (type==SUDEKIMP_LAN_ARENA_AILISH_TYPE && native.selector[0]==12)) {
        /* The native combat entry clip is a transition, not locomotion. Let
         * the replica run its own validated weapon-arm event, then publish a
         * stable idle layer while the transition completes. */
        selectors[0]=idle;
        native.state[0]=128u; native.rate[0]=12.0f; native.time[0]=0.0f;
    }
    if(!SudekiMpLanPartyCombatMotionCapture(type,selectors,native.state,
            native.rate,native.time,native.blend,
            captured[seat].actor==native_actor?&captured[seat].phase:NULL,
            &s->locomotion)) return FALSE;
    for(unsigned i=0;i<weapons.row_count && i<12u;++i)
        if(weapons.rows[i].equipped) { s->weapon_slot_plus_one=(uint8_t)(i+1u); break; }
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE) {
        if(!SudekiMpObserveElcoWeapon(native_actor,&weapon)) return FALSE;
        if(!isfinite(weapon.charge) || weapon.charge<0 || weapon.charge>100 ||
            !isfinite(weapon.reload_seconds) || weapon.reload_seconds<0 ||
            weapon.reload_seconds>60) return FALSE;
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
    SudekiMpLanPartyPeerStatus peer;
    BOOL active;
    if(state) ZeroMemory(state,sizeof(*state));
    if(!state || !actor || !SudekiMpLanPartyPeerStatusGet(session,3u,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_ACTIVE || !peer.lease.token ||
        !peer.lease.generation || peer.lease.seat!=3u ||
        SudekiMpLanPartyControlObserveActor(w,3u)!=actor ||
        !SudekiMpLanPartyControlObserveAilishWeapon(w,&peer.lease,actor,state))
        return FALSE;
    if(ailish_reload_capture.actor!=actor ||
        ailish_reload_capture.lease.token!=peer.lease.token ||
        ailish_reload_capture.lease.generation!=peer.lease.generation ||
        ailish_reload_capture.sequence==0u) {
        ailish_reload_capture.lease=peer.lease;
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
    memset(&frame,0,sizeof(frame));
    for(unsigned seat=0;seat<4;++seat) {
        SudekiMpLanArenaSnapshot *chunk=&frame.chunk[seat/2];
        SudekiMpLanArenaActorSnapshot *s=&chunk->seat[seat%2];
        SudekiMpLanArenaInput input; const SudekiMpLanArenaInput *accepted=NULL;
        if(seat && host && SudekiMpLanPartyHostControlLatestInput(host,w,seat,now,&input))
            accepted=&input;
        if(!party_capture_combat_actor(seat,before.actors[seat],w,accepted,now,s)) return FALSE;
        chunk->host_tick=now; chunk->match_state=SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
        chunk->combat_enabled=1u;
    }
    if(!SudekiMpCleanroomEngineDummySnapshot(position,&hp) || !isfinite(hp) ||
        hp<0 || hp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE) return FALSE;
    frame.chunk[0].enemy_count=1u;
    frame.chunk[0].enemies[0].native_entity_id=SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    frame.chunk[0].enemies[0].x=position[0]; frame.chunk[0].enemies[0].y=position[1];
    frame.chunk[0].enemies[0].z=position[2];
    frame.chunk[0].enemies[0].hp=(uint32_t)(hp+0.5f);
    frame.chunk[0].enemies[0].combat_state=hp<=0.0f?
        SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED:SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    SudekiMpLanHitHostSnapshot(&frame.chunk[0].enemies[0]);
    if(!SudekiMpLanPartyBasicCombatFrameValid(&frame) ||
        !SudekiMpLanPartyControlObserveRoster(w,&after) || !after.combat ||
        before.group!=after.group || before.controller!=after.controller ||
        before.present_mask!=after.present_mask) return FALSE;
    for(unsigned seat=0;seat<4;++seat)
        if(before.actors[seat]!=after.actors[seat]) return FALSE;
    (void)party_capture_ailish_weapon_state(w,session,before.actors[3],
        &frame.ailish_weapon);
    if(!SudekiMpLanPartyFrameValid(&frame)) return FALSE;
    *out=frame; return TRUE;
}
