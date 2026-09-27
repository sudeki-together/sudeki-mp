#include "hooks/lan_party_runtime.h"
#include "hooks/lan_party_host_control.h"
#include "hooks/lan_party_client_control.h"
#include "hooks/lan_party_snapshot.h"
#include "hooks/lan_arena_hit_feedback.h"
#include "hooks/lan_arena_client_input.h"
#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "cleanroom/engine.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "network/lan_party_motion.h"
#include "network/lan_party_replica.h"
#include "engine/log.h"
#include <math.h>
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
static DWORD last_publish,last_trace;
static unsigned long published,applied,failed;
static BOOL observer_registered;
static BOOL host_hit_install_attempted;
static BOOL party_aim_install_attempted;

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
        if(!SudekiMpObserveElcoWeapon(actor,&weapon) ||
            !SudekiMpElcoWeaponReady(&weapon) ||
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
    if(local_seat==0u)
        return party_host_ranged_aim_sample(actor,projectile,direction,target,
            &target_valid,&held);
    if(projectile) return FALSE;
    return SudekiMpLanPartyClientRangedAim(actor,direction,target,
        &target_valid,&held);
}

static BOOL party_ranged_target(void *actor,float target[3]) {
    float direction[3],candidate[3]; BOOL target_valid=FALSE,held=FALSE;
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
    if(!SudekiMpLanPartyMovementDrained(lease,actor,w)) return FALSE;
    return local_seat ?
        SudekiMpLanPartyClientActionDrain(w,lease,actor) :
        SudekiMpLanPartyCombatActionsDrained(lease,actor,w);
}

static DWORD WINAPI poll_network(void *unused) {
    (void)unused;
    while(WaitForSingleObject(stop_worker,2)==WAIT_TIMEOUT)
        SudekiMpLanPartyPoll(session,GetTickCount());
    return 0;
}
static void service(void *controller,void *data,
    const SudekiMpControlUpdateDispatchWitness *w) {
    DWORD now=GetTickCount();
    (void)controller; (void)data;
    if(!SudekiMpControlUpdateObserverGateTryEnter(&gate)) return;
    if(!SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) goto done;
    if(host) {
        SudekiMpLanPartyHostControlReport report;
        if(!SudekiMpLanPartyHostControlService(host,w,now,&report)) { ++failed; goto done; }
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY &&
            report.owned_mask)
            SudekiMpLanAimActors(
                SudekiMpLanPartyControlObserveActor(w,1u),
                SudekiMpLanPartyControlObserveActor(w,3u));
        if(report.roster_status==SUDEKIMP_LAN_PARTY_ROSTER_READY && report.owned_mask &&
            (!last_publish || (DWORD)(now-last_publish)>=50)) {
            SudekiMpLanPartyFrame frame;
            BOOL captured_frame;
            BOOL combat=FALSE;
            captured_frame=SudekiMpCleanroomEngineCombatMode(&combat) &&
                (combat ? SudekiMpLanPartyCaptureBasicCombat(w,session,host,now,&frame) :
                    SudekiMpLanPartyCaptureMovement(w,session,now,&frame));
            if(captured_frame &&
                SudekiMpLanPartySendFrame(session,&frame)) { ++published; last_publish=now; }
            else ++failed;
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
        if(!SudekiMpLanPartyClientControlService(client,w,&report)) { ++failed; goto done; }
        if(!report.ready && !report.draining_mask && !report.failed_mask &&
            (!SudekiMpLanPartyClientEndPresentation(w) ||
             !SudekiMpLanPartyClientRestoreCombatMode())) ++failed;
        if(!SudekiMpLanPartyReplicaConsume(&replica,session,local_seat))
            SudekiMpLanPartyReplicaReset(&replica);
        if(report.ready && SudekiMpLanPartyReplicaSample(&replica,&report.lease,now,&frame,&tick)) {
            BOOL ok=frame.chunk[0].combat_enabled ?
                SudekiMpLanPartyClientApplyBasicCombat(w,&report.lease,&frame) :
                SudekiMpLanPartyClientApplyMovement(w,&report.lease,&frame);
            if(ok) ++applied;
            else ++failed;
        }
        /* The installed native reader is window-local; no global key polling.
         * It consumes local execution even before admission/after disconnect. */
        SudekiMpLanArenaClientInputService();
        if(!last_trace || (DWORD)(now-last_trace)>=1000) {
            SudekiMpLogFormat("lan_party role=client seat=%u scope=basic_combat roster=%u ready=%u owned=%u draining=%u failed=%u applied=%lu rejected=%lu render_tick=%lu\r\n",
                local_seat,report.roster_status,report.ready,report.owned_mask,report.draining_mask,
                report.failed_mask,applied,failed,(unsigned long)replica.clock.host_tick);
            last_trace=now;
        }
    }
done:
    SudekiMpControlUpdateObserverGateLeave(&gate);
}
BOOL SudekiMpUninstallLanPartyRuntime(void) {
    if(!session) return TRUE;
    if(host) SudekiMpLanPartyHostControlRequestStop(host);
    if(client) SudekiMpLanPartyClientControlRequestStop(client);
    SudekiMpControlUpdateObserverGateDisable(&gate);
    SudekiMpControlUpdateObserverGateDrain(&gate);
    if(SudekiMpLanPartyControlHasLeases()) {
        /* Re-enable the sole game-thread cleanup service, not new admission. */
        (void)SudekiMpControlUpdateObserverGateEnable(&gate);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!SudekiMpLanPartyRemovePresentationObserver()) return FALSE;
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
    return TRUE;
}
BOOL SudekiMpInstallLanPartyRuntime(HMODULE module,const SudekiMpLanPartyConfig *config) {
    if(!module || !config || session || config->local_seat>=4) return FALSE;
    session=SudekiMpLanPartyCreate(config);
    if(!session) return FALSE;
    local_seat=config->local_seat; last_publish=last_trace=0; published=applied=failed=0;
    host_hit_install_attempted=FALSE; party_aim_install_attempted=FALSE;
    SudekiMpLanPartyCaptureReset();
    SudekiMpLanPartyReplicaReset(&replica);
    if(local_seat) client=SudekiMpLanPartyClientControlCreate(session,local_seat,party_release_ready);
    else host=SudekiMpLanPartyHostControlCreate(session,party_release_ready);
    if((local_seat && !client) || (!local_seat && !host) ||
        !SudekiMpInstallLanArenaCampaignGuard(module) ||
        (local_seat && (!SudekiMpInitializeLanPartyClientReplica(module,session) ||
            !SudekiMpInstallLanPartyClientInput(module,session) ||
            !SudekiMpLanPartyInstallPresentationObserver(SudekiMpLanPartyClientPresent)))) goto failed_install;
    party_aim_install_attempted=TRUE;
    if(!SudekiMpLanAimInstall(module,party_ranged_aim,party_ranged_target,
            party_ranged_fire)) goto failed_install;
    if(!local_seat) {
        /* Install before admitting remote actors. The witness binds feedback
         * to this host runtime, never to a peer-specific token that changes
         * during an independent reconnect. */
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
