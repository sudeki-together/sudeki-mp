#include "hooks/lan_party_runtime.h"
#include "hooks/lan_party_jetpack.h"
#include "hooks/lan_party_host_control.h"
#include "hooks/lan_party_client_control.h"
#include "hooks/lan_party_snapshot.h"
#include "hooks/lan_party_cast.h"
#include "hooks/lan_party_dummy_overlay.h"
#include "hooks/lan_arena_hit_feedback.h"
#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/lan_arena_client_input.h"
#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "hooks/lan_arena_skill_fade.h"
#include "hooks/noncaster_skill_locomotion.h"
#include "cleanroom/engine.h"
#include "cleanroom/menu.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "network/lan_party_motion.h"
#include "network/lan_party_replica.h"
#include "engine/log.h"
#include "loader/lobby_launch.h"
#include "hooks/lobby_gameplay.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static SudekiMpLanPartySession *session;
static SudekiMpLanPartyHostControl *host;
static SudekiMpLanPartyClientControl *client;
static SudekiMpLanPartyReplica replica;
static SudekiMpControlUpdateObserverGate gate;
static char owner;
static HANDLE worker,stop_worker;
static unsigned local_seat;
static DWORD next_publish,last_trace;
static unsigned long published,applied,failed;
static BOOL observer_registered;
static BOOL host_hit_install_attempted, shield_install_attempted;
static volatile LONG shield_stopping;
static SudekiMpLanPartyRosterObservation shield_roster;
static BOOL party_shield_owner(void *context,uint8_t type,void **actor) {
    unsigned seat=type==SUDEKIMP_LAN_ARENA_BUKI_TYPE?0u:
        type==SUDEKIMP_LAN_ARENA_TAL_TYPE?2u:4u;
    if(context!=session || !session || local_seat || seat>=4u || !actor ||
        InterlockedCompareExchange(&shield_stopping,0,0) ||
        shield_roster.present_mask!=15u ||
        !SudekiMpLanPartyControlNativeActorExact(&shield_roster,seat)) return FALSE;
    *actor=shield_roster.actors[seat]; return *actor!=NULL;
}
static void party_capture_shields(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanPartyFrame *frame) {
    SudekiMpLanArenaSnapshot visuals={0};
    if(!shield_install_attempted || InterlockedCompareExchange(&shield_stopping,0,0) ||
        !SudekiMpLanPartyControlObserveRoster(w,&shield_roster) ||
        shield_roster.present_mask!=15u ||
        !SudekiMpLanPartyShieldHostCapture((uint64_t)(uintptr_t)session,
            frame->chunk[0].host_tick,&visuals)) return;
    for(unsigned i=0; i<visuals.spirit_vfx_count; ++i) {
        unsigned chunk=visuals.spirit_vfx[i].owner_actor_type==SUDEKIMP_LAN_ARENA_BUKI_TYPE?0u:1u;
        frame->chunk[chunk].spirit_vfx[frame->chunk[chunk].spirit_vfx_count++]=visuals.spirit_vfx[i];
    }
    frame->chunk[0].spirit_vfx_observed=frame->chunk[1].spirit_vfx_observed=1u;
}
static BOOL party_aim_install_attempted;
static BOOL rejoin_key_down,tools_installed,tools_resources_ready;
static BOOL dummy_overlay_key_down;
static DWORD last_skill_resources;
static const SudekiMpControlUpdateDispatchWitness *tools_witness;
static const SudekiMpLanPartyHostControlReport *tools_report;
static int traced_phase[4];
static BOOL dummy_spawn_attempted;
static HMODULE party_game_module;
static BOOL party_fade_install_attempted;
static SudekiMpNoncasterSkillLocomotionLease party_noncaster_pose[4];
static void *projectile_target_actor;
static DWORD projectile_target_thread;
static float projectile_target[3];
/* Bounded SMP4 diagnostics: counters are game-thread-only, never admission
 * inputs. One summary and four actor observations per second on every seat. */
static struct {
    DWORD last_call,last_log,gap_sum,gap_max,service_sum,service_max;
    DWORD cast_ms,control_ms,playback_ms,input_ms;
    unsigned calls,capture_fail,consume_fail,sample_fail,apply_fail,cast_fail;
    BOOL frame_valid;
    SudekiMpLanPartyFrame frame;
} diagnostic;
static void party_diagnostic(const SudekiMpControlUpdateDispatchWitness *w,DWORD start) {
    DWORD now=GetTickCount(),cost=now-start;
    diagnostic.service_sum+=cost;
    if(cost>diagnostic.service_max) diagnostic.service_max=cost;
    if(diagnostic.last_log && now-diagnostic.last_log<1000u) return;
    SudekiMpLogFormat("lan_party_diag seat=%u tick=%lu window_ms=%lu calls=%u gap_avg_ms=%lu gap_max_ms=%lu service_avg_ms=%lu service_max_ms=%lu cast_ms=%lu control_ms=%lu playback_ms=%lu input_ms=%lu capture_fail=%u consume_fail=%u sample_fail=%u apply_fail=%u cast_fail=%u recv_age_ms=%lu latest_tick=%lu render_tick=%lu cast_ready=%u\r\n",
        local_seat,(unsigned long)now,(unsigned long)(diagnostic.last_log?now-diagnostic.last_log:0),
        diagnostic.calls,(unsigned long)(diagnostic.calls?diagnostic.gap_sum/diagnostic.calls:0),
        (unsigned long)diagnostic.gap_max,(unsigned long)(diagnostic.calls?diagnostic.service_sum/diagnostic.calls:0),
        (unsigned long)diagnostic.service_max,(unsigned long)diagnostic.cast_ms,
        (unsigned long)diagnostic.control_ms,(unsigned long)diagnostic.playback_ms,(unsigned long)diagnostic.input_ms,
        diagnostic.capture_fail,diagnostic.consume_fail,diagnostic.sample_fail,diagnostic.apply_fail,diagnostic.cast_fail,
        (unsigned long)(local_seat && replica.received_at?now-replica.received_at:0),
        (unsigned long)(local_seat && replica.chunks[0].latest_valid?replica.chunks[0].latest.host_tick:0),
        (unsigned long)(diagnostic.frame_valid?diagnostic.frame.chunk[0].host_tick:0),
        SudekiMpLanPartyCastReady());
    SudekiMpLanPartyRosterObservation roster;
    if(SudekiMpLanPartyControlObserveRoster(w,&roster) && roster.present_mask==15u) {
        for(unsigned i=0;i<4u;++i) {
            SudekiMpCleanroomActor kind;
            SudekiMpCharacterSkillState skill={0};
            SudekiMpCleanroomActorPresentation pose={0};
            SudekiMpLanArenaInput input={0};
            float position[3]={0};
            BOOL actor_ok=SudekiMpCleanroomActorFromType(SudekiMpLanPartyActorType(i),&kind);
            BOOL pos_ok=actor_ok && SudekiMpCleanroomEngineActorPosition(kind,position);
            BOOL pose_ok=actor_ok && SudekiMpCleanroomEngineWorldMotion(kind,&pose);
            BOOL skill_ok=SudekiMpObserveCharacterSkill(roster.actors[i],&skill);
            BOOL input_ok=host && i && SudekiMpLanPartyHostControlLatestInput(host,w,i,now,&input);
            const SudekiMpLanArenaActorSnapshot *s=&diagnostic.frame.chunk[i/2].seat[i%2];
            SudekiMpLogFormat("lan_party_actor_diag seat=%u actor=%u tick=%lu observed=%u%u%u pos=%.3f,%.3f,%.3f frame_valid=%u frame_tick=%lu host_pos=%.3f,%.3f,%.3f clip=%ld,%ld,%ld,%ld phase=%.3f,%.3f,%.3f,%.3f native_skill=%u slot=%d wire_skill=%u sequence=%u target_phase=%u target_ms=%u action=%u action_sequence=%u input_valid=%u input_move=%d,%d\r\n",
                local_seat,i,(unsigned long)now,pos_ok,pose_ok,skill_ok,position[0],position[1],position[2],
                diagnostic.frame_valid,(unsigned long)(diagnostic.frame_valid?diagnostic.frame.chunk[0].host_tick:0),
                s->x,s->y,s->z,(long)pose.selector[0],(long)pose.selector[1],(long)pose.selector[2],(long)pose.selector[3],
                pose.time[0],pose.time[1],pose.time[2],pose.time[3],skill.active,skill.slot,
                s->skill_active,s->skill_sequence,s->skill_target_phase,s->skill_target_remaining_ms,
                s->action_variant,s->action_sequence,input_ok,input.world_direction_x,input.world_direction_z);
            if(i==1u) {
                SudekiMpElcoWeaponObservation weapon={0};
                int selector=0; uint8_t state=0; float phase=0.0f;
                BOOL weapon_ok=SudekiMpObserveElcoWeapon(roster.actors[i],&weapon);
                BOOL ranged_ok=actor_ok && SudekiMpCleanroomEngineRangedActionPresentation(
                    kind,roster.actors[i],&selector,&state,&phase);
                SudekiMpLogFormat("lan_party_elco_diag seat=%u tick=%lu observed=%u ranged_observed=%u item=%u stage=%u charge=%.3f cost=%.3f reload=%.3f wire_valid=%u wire_item=%u wire_charge=%.3f wire_reload_ms=%u shot_sequence=%u channel=%d,%u,%.3f\r\n",
                    local_seat,(unsigned long)now,weapon_ok,ranged_ok,weapon.item,weapon.stage,
                    weapon.charge,weapon.required_charge,weapon.reload_seconds,s->weapon.valid,
                    s->weapon.item,s->weapon.charge_q8/256.0f,s->weapon.reload_ms,
                    s->weapon.shot_count?s->weapon.shots[s->weapon.shot_count-1u].sequence:0u,
                    selector,state,phase);
            }
        }
    }
    diagnostic.last_log=now; diagnostic.calls=0; diagnostic.gap_sum=diagnostic.gap_max=0;
    diagnostic.service_sum=diagnostic.service_max=0;
    diagnostic.cast_ms=diagnostic.control_ms=diagnostic.playback_ms=diagnostic.input_ms=0;
    diagnostic.capture_fail=diagnostic.consume_fail=diagnostic.sample_fail=diagnostic.apply_fail=diagnostic.cast_fail=0;
}
static BOOL party_view_light(float rgb[3]) {
    return SudekiMpLanPartyCastReady() ? SudekiMpLanPartyCastLight(rgb):
        (local_seat && SudekiMpLanPartyClientSkillLight(rgb));
}

static void party_host_present(unsigned phase) {
    static const SudekiMpNoncasterSkillLocomotionActor kinds[4]={
        SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_BUKI,
        SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_ELCO,
        SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TAL,
        SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_AILISH};
    SudekiMpLanPartyRosterObservation roster;
    SudekiMpCharacterSkillState caster;
    int spirit;
    /* Reuse the existing main-thread post-render observer. Its owner refuses
     * removal while party_render_calls is nonzero; the presenter adds no hook. */
    if(!SudekiMpLanPartyPresentationBoundary() ||
        !SudekiMpLanPartyToolsRestoreRenderState() ||
        !SudekiMpLanPartyDummyOverlayRestore()) return;
    if(phase!=2u || !host || local_seat || !party_game_module) return;
    BOOL roster_ready=SudekiMpLanPartyControlObserveRoster(NULL,&roster) &&
        roster.present_mask==15u && SudekiMpLanPartyControlNativeRosterExact(&roster);
    if(roster_ready) SudekiMpLanPartyDummyOverlayRender(party_game_module,0u);
    if(!SudekiMpLanPartyDummyOverlayDrained()) return;
    if(tools_installed) SudekiMpCleanroomMenuRender();
    if(!roster_ready) return;
    BOOL casting=roster.combat &&
        SudekiMpObserveCharacterSkill(roster.actors[0],&caster) && caster.active &&
        SudekiMpCleanroomEngineSpiritPresentationState(&spirit) && spirit==0;
    for(unsigned i=0u;i<4u;++i) {
        SudekiMpLanPartyPeerStatus peer;
        SudekiMpLanArenaInput input;
        SudekiMpCharacterSkillState own;
        BOOL owned=(SudekiMpLanPartyCastReady() ? SudekiMpLanPartyCastNoncaster(roster.actors[i]):
            (i!=0u && casting)) && SudekiMpLanPartyPeerStatusGet(session,i,&peer) &&
            peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE &&
            (!i || SudekiMpLanPartyControlExact(NULL,&peer.lease,roster.actors[i])) &&
            SudekiMpObserveCharacterSkill(roster.actors[i],&own) && !own.active &&
            SudekiMpLanPartyCombatActionsDrained(&peer.lease,roster.actors[i],NULL);
        BOOL moving=owned && SudekiMpLanPartyHostControlLatestInput(host,NULL,i,
            GetTickCount(),&input) && (input.world_direction_x || input.world_direction_z);
        if(owned && !i) {
            const uint8_t *controller=roster.controller;
            /* Observe only the exact Buki controller already proved by the
             * roster; native input/controller execution still owns its axes. */
            moving=fabsf(*(const float *)(controller+0x1a0u))>0.0001f ||
                fabsf(*(const float *)(controller+0x1a4u))>0.0001f;
        }
        (void)SudekiMpNoncasterSkillLocomotionService(party_game_module,roster.actors[i],
            kinds[i],owned,moving,&party_noncaster_pose[i],NULL);
    }
}

static BOOL service_dummy(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame) {
    SudekiMpLanPartyRosterObservation roster;
    float position[3];
    if(!SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) || roster.present_mask!=15u)
        return FALSE;
    if(SudekiMpCleanroomEngineDummyPresent()) return TRUE;
    if(dummy_spawn_attempted) return FALSE; /* positive appearance retires startup */
    if(local_seat) {
        if(!lease || !frame || lease->seat!=local_seat ||
            !SudekiMpLanPartyLeaseActive(session,lease) ||
            !(frame->chunk[0].combat_enabled ?
                SudekiMpLanPartyBasicCombatFrameValid(frame):
                SudekiMpLanPartyMovementFrameValid(frame)) ||
            frame->chunk[0].enemy_count!=1u ||
            frame->chunk[0].enemies[0].native_entity_id!=
                SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID) return FALSE;
        for(unsigned seat=0;seat<4;++seat) {
            SudekiMpLanPartyLease key=*lease; key.seat=(uint8_t)seat;
            if(seat!=local_seat &&
                !SudekiMpLanPartyControlExact(w,&key,roster.actors[seat])) return FALSE;
        }
        position[0]=frame->chunk[0].enemies[0].x;
        position[1]=frame->chunk[0].enemies[0].y;
        position[2]=frame->chunk[0].enemies[0].z;
    } else {
        if(!SudekiMpCleanroomEngineActorPosition(SUDEKIMP_CLEANROOM_BUKI,position))
            return FALSE;
        position[2]+=6.0f;
    }
    for(unsigned i=0;i<3;++i)
        if(!isfinite(position[i]) || fabsf(position[i])>=1000000.0f) return FALSE;
    dummy_spawn_attempted=TRUE;
    if(!SudekiMpCleanroomEngineSpawnDummy(position)) dummy_spawn_attempted=FALSE;
    SudekiMpLogFormat("lan_party event=dummy_startup seat=%u requested=%u source=%s\r\n",
        local_seat,dummy_spawn_attempted,local_seat?"validated_host_frame":"host_testroom");
    return SudekiMpCleanroomEngineDummyPresent();
}

static BOOL operator_key_pressed(int key,BOOL *was_down) {
    DWORD pid=0;
    /* Follow this window's message queue, as the two-seat input adapter does.
     * Wine's asynchronous desktop state can leak across isolated prefixes. */
    BOOL down=(GetKeyState(key)&0x8000)!=0;
    BOOL pressed=down && !*was_down;
    *was_down=down;
    HWND window=GetForegroundWindow();
    if(window) GetWindowThreadProcessId(window,&pid);
    return pressed && pid==GetCurrentProcessId();
}

static void trace_peer_transitions(DWORD now) {
    for(unsigned seat=1;seat<4;++seat) {
        SudekiMpLanPartyPeerStatus p;
        if(local_seat && seat!=local_seat) continue;
        if(!SudekiMpLanPartyPeerStatusGet(session,seat,&p) ||
            traced_phase[seat]==(int)p.phase) continue;
        SudekiMpLogFormat("lan_party event=peer_phase seat=%u phase=%u reason=%u generation=%lu tick=%lu\r\n",
            seat,p.phase,p.failure,(unsigned long)p.lease.generation,(unsigned long)now);
        traced_phase[seat]=(int)p.phase;
    }
}

/* The client gets only this local-display dispatcher. No command text or
 * host-tool action is transmitted; host mutation always rechecks its role. */
static BOOL local_tool_command(unsigned action) {
    if(!session || action!=SUDEKIMP_PARTY_TOOL_HITBOXES) return FALSE;
    SudekiMpLanPartyDummyOverlayToggle();
    return TRUE;
}
static BOOL tool_query(unsigned action,BOOL *enabled) {
    if(!session || !enabled) return FALSE;
    if(action==SUDEKIMP_PARTY_TOOL_HITBOXES) {
        *enabled=SudekiMpLanPartyDummyOverlayEnabled(); return TRUE;
    }
    if(local_seat || !host) return FALSE;
    switch(action) {
    case SUDEKIMP_PARTY_TOOL_COMBAT: return SudekiMpCleanroomEngineCombatMode(enabled);
    case SUDEKIMP_PARTY_TOOL_INFINITE_SP: return SudekiMpCleanroomEngineInfiniteSp(enabled);
    case SUDEKIMP_PARTY_TOOL_TRAINING_SKILLS: return SudekiMpCleanroomEngineTrainingSkills(enabled);
    case SUDEKIMP_PARTY_TOOL_INFINITE_SPIRIT: return SudekiMpCleanroomEngineInfiniteSpirit(enabled);
    case SUDEKIMP_PARTY_TOOL_INFINITE_JETPACK: return SudekiMpCleanroomEngineInfiniteJetpackFuel(enabled);
    case SUDEKIMP_PARTY_TOOL_FUEL_CRYSTAL: return SudekiMpLanPartyJetpackFixture(enabled);
    case SUDEKIMP_PARTY_TOOL_FLIGHT_LEDGE: return SudekiMpLanPartyJetpackLedgeReady(enabled);
    case SUDEKIMP_PARTY_TOOL_CAMERA: return SudekiMpCleanroomEngineFirstPersonMode(enabled);
    default: return FALSE;
    }
}
static void tool_connection_status(char *text,unsigned capacity) {
    static const char *const names[4]={"BUKI","ELCO","TAL","AILISH"};
    SudekiMpLanPartyPeerStatus peer;
    if(!text || !capacity) return;
    if(!session || local_seat>=4u || !SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer)) {
        snprintf(text,capacity,"CONNECTION UNAVAILABLE"); return;
    }
    const char *state=peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE?"CONNECTED":
        peer.phase==SUDEKIMP_LAN_PARTY_DRAINING?"DISCONNECTING":"WAITING FOR HOST";
    snprintf(text,capacity,"%s - %s",names[local_seat],state);
}
static void party_client_present(unsigned phase) {
    if(!SudekiMpLanPartyPresentationBoundary() ||
        !SudekiMpLanPartyToolsRestoreRenderState()) return;
    SudekiMpLanPartyClientPresent(phase);
    /* Local UI remains available while disconnected; no actor lease is
     * required to draw connection information or edit a local setting. */
    if(phase==2u && tools_installed && SudekiMpLanPartyDummyOverlayDrained())
        SudekiMpCleanroomMenuRender();
}

static BOOL host_tool_command(unsigned action) {
    const SudekiMpControlUpdateDispatchWitness *w=tools_witness;
    const SudekiMpLanPartyHostControlReport *report=tools_report;
    SudekiMpLanPartyRosterObservation roster;
    SudekiMpLanPartyPeerStatus peers[4];
    unsigned active_mask=0u,checked_seat=4u;
    const char *reason="host_context";
    BOOL combat=FALSE,ok=FALSE;
    if(action==SUDEKIMP_PARTY_TOOL_HITBOXES && !local_seat && host)
        return local_tool_command(action);
    if(local_seat || !host || !w || !report ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        action>SUDEKIMP_PARTY_TOOL_FLIGHT_LEDGE ||
        report->roster_status!=SUDEKIMP_LAN_PARTY_ROSTER_READY ||
        report->roster_present_mask!=15u || report->roster_initialized_mask!=15u ||
        report->roster_pending_mask || report->waiting_mask ||
        report->draining_mask || report->failed_mask ||
        !SudekiMpCleanroomEngineDummyPresent() ||
        !SudekiMpLanPartyControlGameplayReady(w) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) ||
        !SudekiMpLanPartyControlNativeRosterExact(&roster)) goto rejected;
    for(unsigned seat=0;seat<4;++seat) {
        SudekiMpLanPartyPeerStatus *p=&peers[seat];
        void *actor=roster.actors[seat];
        /* The drain readers use only seat identity; an AI companion gets no
         * fabricated network token and is never acquired for this command. */
        SudekiMpLanPartyLease observation={0,0,(uint8_t)seat};
        checked_seat=seat; reason="peer_state";
        if(!actor || !SudekiMpLanPartyPeerStatusGet(session,seat,p)) goto rejected;
        if(!seat) {
            if(p->phase!=SUDEKIMP_LAN_PARTY_ACTIVE) goto rejected;
        } else if(p->phase==SUDEKIMP_LAN_PARTY_ACTIVE) {
            active_mask|=1u<<seat; reason="remote_ownership";
            if(!(report->owned_mask&(1u<<seat)) ||
                !SudekiMpLanPartyControlExact(w,&p->lease,actor)) goto rejected;
            observation=p->lease;
        } else if(p->phase==SUDEKIMP_LAN_PARTY_FREE ||
                  p->phase==SUDEKIMP_LAN_PARTY_REJECTED) {
            reason="native_ai_ownership";
            if((report->owned_mask&(1u<<seat)) ||
                !SudekiMpLanPartyControlHostAiExact(w,seat,actor)) goto rejected;
        } else goto rejected; /* Joining/pending/draining is not a free AI. */
        reason="actor_busy";
        if(!SudekiMpLanPartyMovementDrained(&observation,actor,w) ||
            !SudekiMpLanPartyCombatActionsDrained(&observation,actor,w)) goto rejected;
    }
    checked_seat=4u; reason="roster_changed";
    if(active_mask!=report->owned_mask ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyControlNativeRosterExact(&roster)) goto rejected;
    /* Network polling can change admission while the game thread observes
     * actors. Reject a changed phase/generation before the native command. */
    for(unsigned seat=0;seat<4;++seat) {
        SudekiMpLanPartyPeerStatus p;
        checked_seat=seat; reason="peer_changed";
        if(!SudekiMpLanPartyPeerStatusGet(session,seat,&p) ||
            p.phase!=peers[seat].phase || p.lease.seat!=peers[seat].lease.seat ||
            p.lease.token!=peers[seat].lease.token ||
            p.lease.generation!=peers[seat].lease.generation) goto rejected;
    }
    checked_seat=4u; reason="tool_state";
    BOOL enabled=FALSE;
    if(!tool_query(action,&enabled)) goto rejected;
    reason="native_command";
    switch(action) {
    case SUDEKIMP_PARTY_TOOL_COMBAT:
        ok=SudekiMpLanPartyJetpackIdle() && SudekiMpCleanroomEngineSetCombatMode(!combat); break;
    case SUDEKIMP_PARTY_TOOL_FLIGHT_LEDGE: ok=SudekiMpLanPartyJetpackToLedge(w); break;
    case SUDEKIMP_PARTY_TOOL_INFINITE_SP: ok=SudekiMpCleanroomEngineSetInfiniteSp(!enabled); break;
    case SUDEKIMP_PARTY_TOOL_TRAINING_SKILLS: ok=SudekiMpCleanroomEngineSetTrainingSkills(!enabled); break;
    case SUDEKIMP_PARTY_TOOL_INFINITE_SPIRIT: ok=SudekiMpCleanroomEngineSetInfiniteSpirit(!enabled); break;
    case SUDEKIMP_PARTY_TOOL_INFINITE_JETPACK: ok=SudekiMpCleanroomEngineSetInfiniteJetpackFuel(!enabled); break;
    case SUDEKIMP_PARTY_TOOL_FUEL_CRYSTAL: ok=SudekiMpLanPartyJetpackSetFixture(!enabled); break;
    case SUDEKIMP_PARTY_TOOL_CAMERA: ok=SudekiMpCleanroomEngineSetFirstPersonMode(!enabled); break;
    default: break;
    }
rejected:
    SudekiMpLogFormat("lan_party_tools event=command action=%u result=%s tick=%lu reason=%s seat=%u owned=%u active=%u policy=host_only_exact_roster_idle_gameplay\r\n",
        action,ok?"confirmed":"rejected",(unsigned long)GetTickCount(),
        ok?"accepted":reason,checked_seat,report?report->owned_mask:0u,active_mask);
    return ok;
}

static BOOL party_host_elco_aim_sample(void *actor,BOOL projectile,
    float direction[3],float target[3],BOOL *target_valid,BOOL *held) {
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanArenaInput input;
    SudekiMpCleanroomActor elco;
    SudekiMpElcoWeaponObservation weapon;
    SudekiMpCharacterSkillState skill;
    float position[3],candidate[3],unit[3];
    BOOL combat=FALSE;
    int spirit=0,selector; uint8_t state; float phase;
    if(target_valid) *target_valid=FALSE;
    if(held) *held=FALSE;
    if(!session || !host || local_seat!=0u || !actor || !direction || !target ||
        !target_valid || !held ||
        actor!=SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_ELCO) ||
        !SudekiMpLanPartyPeerStatusGet(session,1u,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_ACTIVE || !peer.lease.token ||
        !peer.lease.generation ||
        !SudekiMpLanPartyHostControlLatestInput(host,NULL,1u,GetTickCount(),&input) ||
        input.actor_type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        !input.ranged_first_person_active ||
        !(input.aim_direction_x || input.aim_direction_y || input.aim_direction_z) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || !combat ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpCleanroomActorFromType(SUDEKIMP_LAN_ARENA_ELCO_TYPE,&elco) ||
        !SudekiMpCleanroomEngineActorPosition(elco,position)) return FALSE;
    candidate[0]=input.aim_direction_x==INT16_MIN?-1.0f:input.aim_direction_x/32767.0f;
    candidate[1]=input.aim_direction_y==INT16_MIN?-1.0f:input.aim_direction_y/32767.0f;
    candidate[2]=input.aim_direction_z==INT16_MIN?-1.0f:input.aim_direction_z/32767.0f;
    if(!SudekiMpLanAimNormalize(candidate,unit)) return FALSE;
    if(input.aim_target_valid) {
        memcpy(target,input.aim_target,sizeof(candidate));
        if(!SudekiMpLanAimTargetNearActor(position,unit,target)) return FALSE;
        *target_valid=TRUE;
    } else {
        for(unsigned i=0;i<3;++i) target[i]=position[i]+unit[i]*100.0f;
    }
    if(projectile) {
        /* The direction callback runs inside an admitted native shot.
         * Ready-to-start (stage 0/6) rejects its active stage 1/2 and
         * silently leaves the retail horizontal direction in place. */
        if(!SudekiMpObserveElcoWeaponEmission(actor,&weapon) ||
            !SudekiMpCleanroomEngineRangedActionPresentation(elco,actor,
                &selector,&state,&phase) ||
            SudekiMpLanPartyRangedActionChannelDrained(
                SUDEKIMP_LAN_ARENA_ELCO_TYPE,selector,state)) return FALSE;
        if(!*target_valid) return FALSE;
    }
    memcpy(direction,unit,sizeof(unit));
    *held=input.weak_attack_held!=0u;
    return TRUE;
}

static BOOL party_host_ailish_aim_sample(void *actor,BOOL projectile,
    float direction[3],float target[3],BOOL *target_valid,BOOL *held) {
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanArenaInput input;
    SudekiMpCleanroomActor ailish;
    SudekiMpCharacterSkillState skill;
    float position[3],candidate[3],unit[3];
    BOOL combat=FALSE;
    int spirit=0,selector; uint8_t state; float phase;
    if(target_valid) *target_valid=FALSE;
    if(held) *held=FALSE;
    if(!session || !host || local_seat!=0u || !actor || !direction || !target ||
        !target_valid || !held ||
        actor!=SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH) ||
        !SudekiMpLanPartyPeerStatusGet(session,3u,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_ACTIVE || !peer.lease.token ||
        !peer.lease.generation ||
        !SudekiMpLanPartyHostControlLatestInput(host,NULL,3u,GetTickCount(),&input) ||
        input.actor_type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
        !input.ranged_first_person_active ||
        !(input.aim_direction_x || input.aim_direction_y || input.aim_direction_z) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || !combat ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpCleanroomActorFromType(SUDEKIMP_LAN_ARENA_AILISH_TYPE,&ailish) ||
        !SudekiMpCleanroomEngineActorPosition(ailish,position)) return FALSE;
    candidate[0]=input.aim_direction_x==INT16_MIN?-1.0f:input.aim_direction_x/32767.0f;
    candidate[1]=input.aim_direction_y==INT16_MIN?-1.0f:input.aim_direction_y/32767.0f;
    candidate[2]=input.aim_direction_z==INT16_MIN?-1.0f:input.aim_direction_z/32767.0f;
    if(!SudekiMpLanAimNormalize(candidate,unit)) return FALSE;
    /* The v3 input validator reserves aim_target for Elco. Keep that wire
     * contract unchanged and derive Ailish's ray point from host actor state. */
    if(input.aim_target_valid) memcpy(target,input.aim_target,sizeof(candidate));
    else for(unsigned i=0;i<3;++i) target[i]=position[i]+unit[i]*100.0f;
    if(!SudekiMpLanAimTargetNearActor(position,unit,target)) return FALSE;
    *target_valid=TRUE;
    if(projectile &&
        (!SudekiMpCleanroomEngineRangedActionPresentation(
            ailish,actor,&selector,&state,&phase) ||
         SudekiMpLanPartyRangedActionChannelDrained(
            SUDEKIMP_LAN_ARENA_AILISH_TYPE,selector,state))) return FALSE;
    memcpy(direction,unit,sizeof(unit));
    *held=input.weak_attack_held!=0u;
    return TRUE;
}

static BOOL party_host_ranged_aim_sample(void *actor,BOOL projectile,
    float direction[3],float target[3],BOOL *target_valid,BOOL *held) {
    if(actor==SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_ELCO))
        return party_host_elco_aim_sample(actor,projectile,direction,target,
            target_valid,held);
    if(actor==SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH))
        return party_host_ailish_aim_sample(actor,projectile,direction,target,
            target_valid,held);
    return FALSE;
}

static BOOL party_ranged_aim(void *actor,BOOL projectile,float direction[3]) {
    float target[3]; BOOL target_valid=FALSE,held=FALSE;
    if(!actor || !direction) return FALSE;
    projectile_target_actor=NULL;
    if(local_seat==0u)
        return party_host_ranged_aim_sample(actor,projectile,direction,target,
            &target_valid,&held);
    if(projectile) {
        if(!SudekiMpLanPartyClientProjectileAim(actor,direction,projectile_target))
            return FALSE;
        projectile_target_actor=actor; projectile_target_thread=GetCurrentThreadId();
        return TRUE;
    }
    return SudekiMpLanPartyClientRangedAim(actor,direction,target,
        &target_valid,&held);
}

static BOOL party_ranged_target(void *actor,float target[3]) {
    float direction[3],candidate[3]; BOOL target_valid=FALSE,held=FALSE;
    if(target && actor==projectile_target_actor &&
        GetCurrentThreadId()==projectile_target_thread) {
        projectile_target_actor=NULL;
        memcpy(target,projectile_target,sizeof(projectile_target));
        return TRUE;
    }
    if(!target || (local_seat==0u ?
        !party_host_ranged_aim_sample(actor,FALSE,direction,candidate,
            &target_valid,&held) :
        !SudekiMpLanPartyClientRangedAim(actor,direction,candidate,
            &target_valid,&held)) || !target_valid) return FALSE;
    memcpy(target,candidate,sizeof(candidate));
    return TRUE;
}

static BOOL party_ranged_fire(void *actor,BOOL *held) {
    float direction[3],target[3]; BOOL target_valid=FALSE,firing=FALSE;
    if(!held) return FALSE;
    if(local_seat==0u)
        return party_host_ranged_aim_sample(actor,FALSE,direction,target,
            &target_valid,held);
    if(!SudekiMpLanPartyClientRangedAim(actor,direction,target,
            &target_valid,&firing)) return FALSE;
    *held=firing;
    return TRUE;
}
static void party_weapon_shot(void *actor) {
    SudekiMpLanPartyPeerStatus peer;
    float direction[3],target[3]; BOOL target_valid,held;
    if(local_seat) {
        SudekiMpLanArenaClientReplicaNativeWeaponFired(actor);
        return;
    }
    unsigned seat=actor==SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_ELCO)?1u:
        actor==SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH)?3u:0u;
    if(!seat || !party_host_ranged_aim_sample(actor,FALSE,direction,target,&target_valid,&held) ||
        !SudekiMpLanPartyPeerStatusGet(session,seat,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_ACTIVE) return;
    SudekiMpLanPartyCaptureRangedShot(&peer.lease,actor);
}

static BOOL party_host_hit_target_witness(SudekiMpLanHitTarget *target) {
    BOOL active_peer=FALSE;
    uintptr_t identity=(uintptr_t)session;
    if(!target || !session || !host || !identity ||
        SudekiMpLanPartyLocalSeat(session)!=0u ||
        !SudekiMpLanPartyControlHasLeases()) return FALSE;
    for(unsigned seat=1;seat<4;++seat) {
        SudekiMpLanPartyPeerStatus peer;
        if(SudekiMpLanPartyPeerStatusGet(session,seat,&peer) &&
            peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE && peer.lease.token &&
            peer.lease.generation) {
            active_peer=TRUE;
            break;
        }
    }
    return active_peer && SudekiMpLanHitResolveTarget(
        SudekiMpCleanroomEngineGenericEntity("MON_TrainingDummy"),
        (uint64_t)identity,target);
}

static BOOL party_release_ready(const SudekiMpLanPartyLease *lease,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    if(!SudekiMpLanPartyJetpackDrained(actor) || !SudekiMpLanPartyCastDrained(actor)) return FALSE;
    if(!SudekiMpLanPartyMovementDrained(lease,actor,w)) return FALSE;
    return local_seat ?
        SudekiMpLanPartyClientActionDrain(w,lease,actor) :
        SudekiMpLanPartyCombatActionsDrained(lease,actor,w);
}

static DWORD WINAPI poll_network(void *unused) {
    (void)unused;
    while(WaitForSingleObject(stop_worker,2)==WAIT_TIMEOUT)
        if (SudekiMpLobbyLaunchChildRunning() && SudekiMpLobbyGameplayRunning()) SudekiMpLanPartyPoll(session,GetTickCount());
    return 0;
}
static void service(void *controller,void *data,
    const SudekiMpControlUpdateDispatchWitness *w) {
    DWORD now=GetTickCount(),part;
    (void)controller;
    if(!SudekiMpControlUpdateObserverGateTryEnter(&gate)) return;
    if(!SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) || !SudekiMpLobbyGameplayRunning()) goto done;
    SudekiMpLanPartyJetpackServiceStop(w);
    if(diagnostic.last_call) {
        DWORD gap=now-diagnostic.last_call;
        diagnostic.gap_sum+=gap;
        if(gap>diagnostic.gap_max) diagnostic.gap_max=gap;
    }
    diagnostic.last_call=now; ++diagnostic.calls;
    if(operator_key_pressed(VK_F9,&dummy_overlay_key_down))
        (void)local_tool_command(SUDEKIMP_PARTY_TOOL_HITBOXES);
    /* SMP4 bypasses the legacy cleanroom resource-maintenance loop, but must
     * still retire its native UI prime on the original owner game thread. */
    if(!SudekiMpCleanroomEngineServiceRangedPrime()) ++failed;
    part=GetTickCount();
    if(!SudekiMpLanPartyCastService(w)) { ++failed; ++diagnostic.cast_fail; }
    diagnostic.cast_ms+=GetTickCount()-part;
    trace_peer_transitions(now);
    if(host) {
        SudekiMpLanPartyHostControlReport report;
        float frame_delta=0.0f;
        /* This is the live original callback's CUpdateData, borrowed only
         * under its exact game-thread dispatch witness, as in the two-seat
         * service. Do not substitute elapsed network/render time. */
        if(data) {
            float candidate=*(float *)((uint8_t *)data+0x0cu);
            if(isfinite(candidate) && candidate>0.0f && candidate<=0.25f)
                frame_delta=candidate;
        }
        part=GetTickCount();
        BOOL serviced=SudekiMpLanPartyHostControlServiceFrame(host,w,now,frame_delta,&report);
        diagnostic.control_ms+=GetTickCount()-part;
        if(!serviced) { ++failed; goto done; }
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY &&
            !SudekiMpLanPartyCastReady()) {
            part=GetTickCount();
            if(!SudekiMpLanPartyCastTryBind(w)) { ++failed; ++diagnostic.cast_fail; }
            diagnostic.cast_ms+=GetTickCount()-part;
        }
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY)
            (void)service_dummy(w,NULL,NULL);
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY && SudekiMpLanPartyCastReady())
            { SudekiMpLobbyLaunchChildLoaded(); SudekiMpLobbyGameplayLoaded(); }
        if(tools_installed) {
            if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY) {
                if(!tools_resources_ready)
                    tools_resources_ready=SudekiMpCleanroomEngineSetInfiniteSp(TRUE);
                if(tools_resources_ready && (!last_skill_resources || now-last_skill_resources>=200u)) {
                    SudekiMpCleanroomEngineMaintainTrainingResources(); last_skill_resources=now;
                }
            }
            /* Borrow stack observations only across this synchronous UI call
             * under the existing observer gate. No queued callback gets them. */
            tools_witness=w; tools_report=&report;
            SudekiMpCleanroomMenuUpdate();
            tools_witness=NULL; tools_report=NULL;
        }
        SudekiMpLanPartyJetpackService(w,NULL);
        /* A mode transition is authoritative as soon as the host's native
         * group confirms it. Publish independently of animation capture so
         * clients can run their own draw/sheathe concurrently. The transport
         * worker never observes native pointers or renews this observation. */
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY) {
            SudekiMpLanPartyRosterObservation roster;
            static int traced_mode=-1;
            if(SudekiMpLanPartyControlObserveRoster(w,&roster) &&
                roster.present_mask==15u &&
                SudekiMpLanPartyControlNativeRosterExact(&roster)) {
                DWORD mode_tick=GetTickCount();
                if(SudekiMpLanPartyPublishCombatMode(session,roster.combat!=0,mode_tick) &&
                    traced_mode!=(int)(roster.combat!=0)) {
                    traced_mode=roster.combat!=0;
                    SudekiMpLogFormat("lan_party event=host_combat_notice enabled=%u tick=%lu\r\n",
                        (unsigned)traced_mode,(unsigned long)mode_tick);
                }
            }
        }
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY &&
            report.owned_mask)
            SudekiMpLanAimActors(
                SudekiMpLanPartyControlObserveActor(w,1u),
                SudekiMpLanPartyControlObserveActor(w,3u));
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY && report.owned_mask &&
            (!next_publish || (int32_t)(now-next_publish)>=0)) {
            SudekiMpLanPartyFrame frame;
            BOOL captured_frame;
            BOOL combat=FALSE;
            part=GetTickCount();
            captured_frame=SudekiMpCleanroomEngineCombatMode(&combat) &&
                (combat ? SudekiMpLanPartyCaptureBasicCombat(w,session,host,now,&frame) :
                    SudekiMpLanPartyCaptureMovement(w,session,now,&frame));
            if(captured_frame) {
                party_capture_shields(w,&frame);
                SudekiMpLanPartyJetpackCapture(&frame.jetpack);
                diagnostic.frame=frame; diagnostic.frame_valid=TRUE;
            }
            if(captured_frame &&
                SudekiMpLanPartySendFrame(session,&frame)) {
                ++published;
                /* Keep the 20Hz deadline. Resetting it to now+50 on a ~40ms
                 * game loop sends only every other callback (~12.5Hz).
                 * Skip missed slots; never burst multiple frames to catch up. */
                DWORD late=next_publish?now-next_publish:0;
                next_publish=now+50u-late%50u;
            }
            else { ++failed; ++diagnostic.capture_fail; }
            diagnostic.playback_ms+=GetTickCount()-part;
        }
        if(!last_trace || (DWORD)(now-last_trace)>=1000) {
            SudekiMpLogFormat("lan_party role=host scope=basic_combat roster=%u present=%u initialized=%u pending=%u owned=%u waiting=%u draining=%u failed=%u unsupported=%u frames=%lu rejected=%lu\r\n",
                report.roster_status,report.roster_present_mask,report.roster_initialized_mask,
                report.roster_pending_mask,report.owned_mask,report.waiting_mask,report.draining_mask,
                report.failed_mask,report.unsupported_input_mask,published,failed);
            last_trace=now;
        }
    } else if(client) {
        SudekiMpLanPartyClientControlReport report; SudekiMpLanPartyFrame frame; uint32_t tick;
        BOOL rejoin=operator_key_pressed(VK_F7,&rejoin_key_down);
        if(tools_installed) SudekiMpCleanroomMenuUpdate();
        if(rejoin) SudekiMpLogFormat("lan_party event=client_rejoin_request key=F7 seat=%u\r\n",local_seat);
        part=GetTickCount();
        BOOL serviced=SudekiMpLanPartyClientControlService(client,w,&report);
        diagnostic.control_ms+=GetTickCount()-part;
        if(!serviced) { ++failed; goto done; }
        if(report.ready && !SudekiMpLanPartyCastReady()) {
            part=GetTickCount();
            if(!SudekiMpLanPartyCastTryBind(w)) { ++failed; ++diagnostic.cast_fail; }
            diagnostic.cast_ms+=GetTickCount()-part;
        }
        if(!report.ready && !report.draining_mask && !report.failed_mask) {
            BOOL restored=SudekiMpLanPartyClientEndPresentation(w) &&
                SudekiMpLanPartyClientRestoreCombatMode(w);
            if(!restored) ++failed;
            if(rejoin) {
                BOOL started=restored && !SudekiMpCleanroomEngineRangedCombatPrimePending() &&
                    SudekiMpLanPartyControlGameplayReady(w) &&
                    SudekiMpLanPartyClientControlRejoin(client,w);
                if(started) SudekiMpLanPartyReplicaReset(&replica);
                SudekiMpLogFormat("lan_party event=client_rejoin key=F7 seat=%u result=%s policy=after_native_and_presentation_drain\r\n",
                    local_seat,started?"joining":"rejected");
            }
        }
        if(!SudekiMpLanPartyReplicaConsume(&replica,session,local_seat)) {
            ++diagnostic.consume_fail;
            SudekiMpLanPartyReplicaReset(&replica);
        }
        /* Control/lease service can take part of a frame. Sample against the
         * time at playback, rather than the older callback-entry timestamp. */
        part=GetTickCount();
        SudekiMpLanPartyFrame confirmed;
        BOOL confirmed_ready=report.ready && SudekiMpLanPartyReplicaLatestFrame(
            &replica,&report.lease,GetTickCount(),&confirmed);
        /* Native targeting gates need the newest host phase even while body
         * sampling or an actor's renderer is temporarily waiting to drain. */
        if(confirmed_ready && SudekiMpLanPartyCastReady() &&
            !SudekiMpLanPartyCastReplayTiming(w,&report.lease,&confirmed)) {
            confirmed_ready=FALSE; ++diagnostic.cast_fail;
        }
        SudekiMpLanPartyJetpackService(w,confirmed_ready?&confirmed.jetpack:NULL);
        BOOL mode_ready=report.ready && SudekiMpLanPartyClientServiceCombatMode(
            w,&report.lease,confirmed_ready?&confirmed:NULL);
        if(mode_ready && confirmed_ready && SudekiMpLanPartyReplicaSample(&replica,&report.lease,
                GetTickCount(),&frame,&tick)) {
            diagnostic.frame=frame; diagnostic.frame_valid=TRUE;
            BOOL dummy_ready=!frame.chunk[0].enemy_count ||
                service_dummy(w,&report.lease,&frame);
            BOOL ok=dummy_ready && (frame.chunk[0].combat_enabled ?
                SudekiMpLanPartyClientApplyBasicCombat(w,&report.lease,&frame,&confirmed) :
                SudekiMpLanPartyClientApplyMovement(w,&report.lease,&frame,&confirmed));
            if(ok) { ++applied; SudekiMpLobbyLaunchChildLoaded(); SudekiMpLobbyGameplayLoaded(); }
            else { ++failed; ++diagnostic.apply_fail; }
        } else if(report.ready) ++diagnostic.sample_fail;
        diagnostic.playback_ms+=GetTickCount()-part;
        /* The installed native reader is window-local; no global key polling.
         * It consumes local execution even before admission/after disconnect. */
        part=GetTickCount();
        SudekiMpLanArenaClientInputService();
        diagnostic.input_ms+=GetTickCount()-part;
        if(!last_trace || (DWORD)(now-last_trace)>=1000) {
            SudekiMpLogFormat("lan_party role=client seat=%u scope=basic_combat roster=%u ready=%u owned=%u draining=%u failed=%u applied=%lu rejected=%lu render_tick=%lu\r\n",
                local_seat,report.roster_status,report.ready,report.owned_mask,report.draining_mask,
                report.failed_mask,applied,failed,(unsigned long)replica.clock.host_tick);
            last_trace=now;
        }
    }
done:
    if(shield_install_attempted && InterlockedCompareExchange(&shield_stopping,0,0))
        (void)SudekiMpLanArenaSpiritVisualHostReset();
    if(SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) party_diagnostic(w,now);
    SudekiMpControlUpdateObserverGateLeave(&gate);
}
BOOL SudekiMpUninstallLanPartyRuntime(void) {
    if(!session) return TRUE;
    SudekiMpLanPartyJetpackRequestStop();
    SudekiMpLanPartyDummyOverlayRequestStop();
    InterlockedExchange(&shield_stopping,1);
    SudekiMpLanPartyCastRequestStop();
    if(host) SudekiMpLanPartyHostControlRequestStop(host);
    if(client) SudekiMpLanPartyClientControlRequestStop(client);
    SudekiMpControlUpdateObserverGateDisable(&gate);
    SudekiMpControlUpdateObserverGateDrain(&gate);
    if(SudekiMpLanPartyControlHasLeases() ||
        (local_seat && SudekiMpLanPartyClientCallbacksRetained())) {
        /* Re-enable the sole game-thread cleanup service, not new admission. */
        (void)SudekiMpControlUpdateObserverGateEnable(&gate);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!SudekiMpLanPartyJetpackUninstall()) {
        (void)SudekiMpControlUpdateObserverGateEnable(&gate); return FALSE;
    }
    if(shield_install_attempted) {
        if(!SudekiMpLanArenaSpiritVisualHostReset()) {
            (void)SudekiMpControlUpdateObserverGateEnable(&gate);
            SetLastError(ERROR_BUSY); return FALSE;
        }
        shield_install_attempted=FALSE;
        ZeroMemory(&shield_roster,sizeof(shield_roster));
    }
    if(!SudekiMpLanPartyCastUninstall()) {
        (void)SudekiMpControlUpdateObserverGateEnable(&gate);
        return FALSE;
    }
    if(!SudekiMpLanPartyDummyOverlayDrained() || !SudekiMpLanPartyToolsRenderDrained()) {
        (void)SudekiMpControlUpdateObserverGateEnable(&gate);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!SudekiMpLanPartyRemovePresentationObserver()) return FALSE;
    /* Update gate is drained above and render callbacks are now restored.
     * The presenter releases only its texture; engine ownership stays here. */
    if(tools_installed) {
        SudekiMpUninstallCleanroomMenu(); tools_installed=FALSE;
    }
    if(party_fade_install_attempted) {
        if(!SudekiMpUninstallLanArenaSkillFade()) return FALSE;
        party_fade_install_attempted=FALSE;
    }
    if(observer_registered && !SudekiMpControlSeparationUnregisterUpdateObserver(&owner)) return FALSE;
    observer_registered=FALSE;
    if(party_aim_install_attempted) {
        if(!SudekiMpLanAimUninstall()) return FALSE;
        party_aim_install_attempted=FALSE;
    }
    if(host_hit_install_attempted) {
        if(!SudekiMpLanHitHostUninstall()) return FALSE;
        host_hit_install_attempted=FALSE;
    }
    if(local_seat && (!SudekiMpUninstallLanArenaClientInput() ||
        !SudekiMpResetLanArenaClientReplica())) return FALSE;
    if(!SudekiMpUninstallLanArenaCampaignGuard()) return FALSE;
    if(host && !SudekiMpLanPartyHostControlDestroy(host)) return FALSE;
    host=NULL;
    if(client && !SudekiMpLanPartyClientControlDestroy(client)) return FALSE;
    client=NULL;
    if(worker) {
        if(!SetEvent(stop_worker) || WaitForSingleObject(worker,1000)!=WAIT_OBJECT_0) return FALSE;
        CloseHandle(worker); worker=NULL;
    }
    if(stop_worker) { CloseHandle(stop_worker); stop_worker=NULL; }
    SudekiMpLanPartyDestroy(session,TRUE); session=NULL;
    SudekiMpLanPartyReplicaReset(&replica);
    ZeroMemory(party_noncaster_pose,sizeof(party_noncaster_pose));
    party_game_module=NULL;
    return TRUE;
}
BOOL SudekiMpInstallLanPartyRuntime(HMODULE module,const SudekiMpLanPartyConfig *config) {
    if(!module || !config || session || config->local_seat>=4) return FALSE;
    if(!SudekiMpLanPartyDummyOverlayReset()) return FALSE;
    session=SudekiMpLanPartyCreate(config);
    if(!session) return FALSE;
    SudekiMpLobbyLaunchChildPort(SudekiMpLanPartyPort(session));
    local_seat=config->local_seat; next_publish=last_trace=0; published=applied=failed=0;
    party_game_module=module; party_fade_install_attempted=FALSE;
    ZeroMemory(party_noncaster_pose,sizeof(party_noncaster_pose));
    tools_installed=tools_resources_ready=FALSE; last_skill_resources=0;
    rejoin_key_down=(GetKeyState(VK_F7)&0x8000)!=0;
    dummy_overlay_key_down=(GetKeyState(VK_F9)&0x8000)!=0;
    for(unsigned seat=0;seat<4;++seat) traced_phase[seat]=-1;
    dummy_spawn_attempted=FALSE;
    host_hit_install_attempted=FALSE; party_aim_install_attempted=FALSE;
    shield_install_attempted=FALSE; InterlockedExchange(&shield_stopping,0);
    ZeroMemory(&shield_roster,sizeof(shield_roster));
    SudekiMpLanPartyCaptureReset();
    SudekiMpLanPartyReplicaReset(&replica);
    if(local_seat) client=SudekiMpLanPartyClientControlCreate(session,local_seat,party_release_ready);
    else host=SudekiMpLanPartyHostControlCreate(session,party_release_ready);
    if((local_seat && !client) || (!local_seat && !host) ||
        !SudekiMpLanPartyJetpackInstall(module,session) ||
        !SudekiMpLanPartyCastInstall(module,session) ||
        !SudekiMpInstallLanArenaCampaignGuard(module) ||
        (local_seat && (!SudekiMpInitializeLanPartyClientReplica(module,session) ||
            !SudekiMpInstallLanPartyClientInput(module,session) ||
            !SudekiMpLanPartyInstallPresentationObserver(party_client_present)))) goto failed_install;
    if(!local_seat && !SudekiMpLanPartyInstallPresentationObserver(party_host_present))
        goto failed_install;
    if(!SudekiMpInstallLanPartyToolsMenu(module,VK_F8,local_seat,
        local_seat?local_tool_command:host_tool_command,tool_query,tool_connection_status)) goto failed_install;
    tools_installed=TRUE;
    if(local_seat) SudekiMpLanPartyClientInputSetConsoleGate(SudekiMpLanPartyToolsCaptureInput);
    party_fade_install_attempted=TRUE;
    if(!SudekiMpInstallLanArenaSkillFade(module,party_view_light)) goto failed_install;
    party_aim_install_attempted=TRUE;
    if(!SudekiMpLanAimInstall(module,party_ranged_aim,party_ranged_target,
            party_ranged_fire)) goto failed_install;
    SudekiMpLanAimSetShotObserver(party_weapon_shot);
    if(!local_seat) {
        /* Install before admitting remote actors. The witness binds feedback
         * to this host runtime, never to a peer-specific token that changes
         * during an independent reconnect. */
        shield_install_attempted=TRUE;
        if(!SudekiMpLanPartyShieldHostInitialize(module,party_shield_owner,session)) goto failed_install;
        host_hit_install_attempted=TRUE;
        if(!SudekiMpLanHitHostInstall(module,party_host_hit_target_witness)) goto failed_install;
    }
    stop_worker=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!stop_worker) goto failed_install;
    worker=CreateThread(NULL,0,poll_network,NULL,0,NULL);
    if(!worker || !SudekiMpControlUpdateObserverGateEnable(&gate)) goto failed_install;
    if(!SudekiMpControlSeparationRegisterUpdateObserver(&owner,service)) goto failed_install;
    observer_registered=TRUE;
    SudekiMpLogFormat("lan_party runtime=installed seat=%u scope=private_basic_combat gameplay_acceptance=pending\r\n",local_seat);
    return TRUE;
failed_install:
    {
        DWORD error=GetLastError();
        if(!SudekiMpUninstallLanPartyRuntime()) return FALSE;
        SetLastError(error); return FALSE;
    }
}

unsigned SudekiMpLanPartyRuntimePort(void) { return session?SudekiMpLanPartyPort(session):0; }
