#include "hooks/lan_party_host_control.h"
#include "hooks/lan_party_control.h"
#include "hooks/lan_party_cast.h"
#include "hooks/lan_party_jetpack.h"
#include "cleanroom/engine.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "engine/log.h"
#include "network/lan_party_motion.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct HostActor {
    SudekiMpLanPartyLease lease;
    void *actor;
    SudekiMpLanPartyInput input;
    BOOL have_input, draining, stopped, action_serviced, block_held, movement_busy;
    uint32_t next_held_fire_at;
    BOOL weapon_swap;
    uint32_t weapon_swap_started;
} HostActor;
struct SudekiMpLanPartyHostControl {
    SudekiMpLanPartySession *session;
    SudekiMpLanPartyControlDrainProbe drained;
    HostActor actors[3];
    volatile LONG stopping;
    BOOL initialized;
    SudekiMpLanPartyRoster roster;
};
static PVOID volatile coordinator_owner;
#ifdef SUDEKIMP_LAN_PARTY_CONTROL_TESTING
static SudekiMpLanPartyHostTestCombatMode party_host_test_combat_mode;
void SudekiMpLanPartyHostControlTestCombatMode(
    SudekiMpLanPartyHostTestCombatMode mode) {
    party_host_test_combat_mode = mode;
}
#endif
static BOOL host_combat_mode(BOOL *enabled) {
#ifdef SUDEKIMP_LAN_PARTY_CONTROL_TESTING
    if (party_host_test_combat_mode)
        return party_host_test_combat_mode(enabled);
#endif
    return SudekiMpCleanroomEngineCombatMode(enabled);
}
static BOOL same_lease(const SudekiMpLanPartyLease *a,
    const SudekiMpLanPartyLease *b) {
    return a->seat == b->seat && a->generation == b->generation && a->token == b->token;
}
static BOOL ordinary_input(const SudekiMpLanArenaInput *i,
    const SudekiMpLanPartyCombatInputExtension *combat) {
    BOOL ranged=i->actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        i->actor_type==SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    return combat && combat->strong_pressed<=1u &&
        combat->sweep_pressed<=1u && combat->block_held<=1u &&
        combat->flight_held<=1u && (!combat->flight_held ||
            i->actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE) &&
        (!i->weak_attack_held ||
            (ranged && i->ranged_first_person_active)) &&
        !i->cleanroom_combat_test_pressed &&
        (i->kit_action==SUDEKIMP_LAN_ARENA_KIT_NONE ||
         i->kit_action==SUDEKIMP_LAN_ARENA_KIT_WEAPON ||
         i->kit_action==SUDEKIMP_LAN_ARENA_KIT_SPIRIT);
}
static BOOL ranged_action_active(void *actor,uint8_t type) {
    SudekiMpCleanroomActor cleanroom_actor;
    int selector; uint8_t state; float time;
    if(!actor || !SudekiMpCleanroomActorFromType(type,&cleanroom_actor) ||
        !SudekiMpCleanroomEngineRangedActionPresentation(cleanroom_actor,actor,
            &selector,&state,&time)) return TRUE;
    return !SudekiMpLanPartyRangedActionChannelDrained(type,selector,state);
}
static BOOL ranged_weapon_ready(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,void *actor,uint8_t type) {
    SudekiMpElcoWeaponObservation weapon;
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE)
        return SudekiMpObserveElcoWeapon(actor,&weapon) &&
            SudekiMpElcoWeaponReady(&weapon);
    if(type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) {
        BOOL ready=FALSE;
        return SudekiMpLanPartyControlAilishRangedReady(
            w,lease,actor,&ready) && ready;
    }
    return TRUE;
}
static BOOL submit_weapon_selection(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,void *actor,unsigned slot,BOOL *changed) {
    SudekiMpCharacterSkillState skill; BOOL combat=FALSE; int spirit=0;
    SudekiMpWeaponActivationResult result;
    SudekiMpWeaponQuickList weapons;
    if(changed) *changed=FALSE;
    if(!w || !lease || !actor || !changed || slot>=12u ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyControlExact(w,lease,actor) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || !combat ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpDescribeCharacterWeapons(actor,&weapons) || slot>=weapons.row_count)
        return FALSE;
    result=SudekiMpActivateCharacterWeapon(actor,slot);
    BOOL accepted=result.status==SUDEKIMP_WEAPON_ACTIVATION_STARTED &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
        SudekiMpLanPartyControlExact(w,lease,actor);
    *changed=accepted && !weapons.rows[slot].equipped;
    return accepted;
}
static float axis(int16_t value) {
    return value == INT16_MIN ? -1.0f : value / 32767.0f;
}
static BOOL input_fresh(uint32_t now, uint32_t received) {
    int32_t age = (int32_t)(now - received);
    /* The socket worker can timestamp a new packet just AFTER the game
     * callback sampled now. Both are host clocks; do not turn that bounded
     * race into a uint32 wrap and a spurious movement stop. */
    return age >= -(int32_t)SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS &&
        age <= (int32_t)SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS;
}
static BOOL release_block(const SudekiMpControlUpdateDispatchWitness *w,
    HostActor *a) {
    BOOL combat=FALSE;
    if(!a || !a->block_held) return TRUE;
    if(!host_combat_mode(&combat) ||
        (combat && !SudekiMpLanPartyControlSubmitBlockState(
            w,&a->lease,a->actor,3u))) return FALSE;
    a->block_held=FALSE;
    return TRUE;
}
SudekiMpLanPartyHostControl *SudekiMpLanPartyHostControlCreate(
    SudekiMpLanPartySession *session, SudekiMpLanPartyControlDrainProbe drained) {
    SudekiMpLanPartyPeerStatus self;
    SudekiMpLanPartyHostControl *h;
    if (!session || !drained || SudekiMpLanPartyControlHasLeases() ||
        !SudekiMpLanPartyPeerStatusGet(session,0,&self) ||
        self.phase != SUDEKIMP_LAN_PARTY_ACTIVE) return NULL;
    h = calloc(1,sizeof(*h));
    if (h) {
        h->session = session; h->drained = drained;
        if (InterlockedCompareExchangePointer(&coordinator_owner,h,NULL) != NULL) {
            free(h); return NULL;
        }
    }
    return h;
}
void SudekiMpLanPartyHostControlRequestStop(SudekiMpLanPartyHostControl *h) {
    unsigned int seat;
    if (!h) return;
    InterlockedExchange(&h->stopping,1);
    for (seat=1; seat<4; ++seat) {
        SudekiMpLanPartyPeerStatus p;
        if (SudekiMpLanPartyPeerStatusGet(h->session,seat,&p) &&
            (p.phase == SUDEKIMP_LAN_PARTY_ACTIVE || p.phase == SUDEKIMP_LAN_PARTY_PENDING))
            (void)SudekiMpLanPartyDisconnect(h->session,&p.lease);
    }
}
BOOL SudekiMpLanPartyHostControlDestroy(SudekiMpLanPartyHostControl *h) {
    unsigned int i;
    if (!h) return TRUE;
    if (!InterlockedCompareExchange(&h->stopping,0,0)) return FALSE;
    for (i=0; i<3; ++i) if (h->actors[i].actor) return FALSE;
    if (InterlockedCompareExchangePointer(&coordinator_owner,NULL,h) != h)
        return FALSE;
    free(h); return TRUE;
}
static BOOL retire(SudekiMpLanPartyHostControl *h, HostActor *a,
    const SudekiMpControlUpdateDispatchWitness *w) {
    if(!release_block(w,a) || !SudekiMpLanPartyJetpackDrained(a->actor)) return FALSE;
    a->have_input = FALSE; a->draining = TRUE;
    /* Quiesce may fail because Default consumed the reference on an earlier
     * pass. Release retains its own retry/verification state in that case. */
    (void)SudekiMpLanPartyControlQuiesce(w,&a->lease,a->actor);
    if (!SudekiMpLanPartyControlRelease(w,&a->lease,a->actor,h->drained))
        return FALSE;
    /* Clear native publication only after positive native release. Socket
     * release is separate and retried from DRAINING if it races shutdown. */
    (void)SudekiMpLanPartyReleaseDrained(h->session,&a->lease);
    memset(a,0,sizeof(*a)); return TRUE;
}
BOOL SudekiMpLanPartyHostControlServiceFrame(SudekiMpLanPartyHostControl *h,
    const SudekiMpControlUpdateDispatchWitness *w, uint32_t now,
    float frame_delta, SudekiMpLanPartyHostControlReport *report) {
    unsigned int seat;
    if (!h || !report || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    memset(report,0,sizeof(*report));
    if (!h->initialized) {
        if (!SudekiMpLanPartyControlBeginSession(w)) return FALSE;
        h->initialized = TRUE;
    }
    if (!InterlockedCompareExchange(&h->stopping,0,0)) {
        report->roster_status = SudekiMpLanPartyRosterService(&h->roster,w);
        report->roster_present_mask = h->roster.bound.present_mask;
        report->roster_initialized_mask = h->roster.initialized_mask;
        report->roster_pending_mask = h->roster.spawn_pending_mask;
        if (report->roster_status == SUDEKIMP_LAN_PARTY_ROSTER_REPLACED)
            SudekiMpLanPartyHostControlRequestStop(h);
    }
    for (seat=1; seat<4; ++seat) {
        HostActor *a = &h->actors[seat-1];
        SudekiMpLanPartyPeerStatus p;
        SudekiMpLanPartyInput next;
        uint8_t bit = (uint8_t)(1u << seat);
        uint32_t repeat_ms=seat==1u?0u:250u;
        BOOL fresh, admitted = FALSE, new_input=FALSE;
        if (!SudekiMpLanPartyPeerStatusGet(h->session,seat,&p)) {
            report->failed_mask |= bit; continue;
        }
        if (InterlockedCompareExchange(&h->stopping,0,0) &&
            (p.phase == SUDEKIMP_LAN_PARTY_ACTIVE || p.phase == SUDEKIMP_LAN_PARTY_PENDING)) {
            (void)SudekiMpLanPartyDisconnect(h->session,&p.lease);
            if (!SudekiMpLanPartyPeerStatusGet(h->session,seat,&p)) {
                report->failed_mask |= bit; continue;
            }
        }
        if (a->actor && (a->draining || p.phase != SUDEKIMP_LAN_PARTY_ACTIVE ||
                !same_lease(&a->lease,&p.lease))) {
            report->draining_mask |= bit;
            if (!retire(h,a,w)) report->failed_mask |= bit;
            continue;
        }
        if (!a->actor && p.phase == SUDEKIMP_LAN_PARTY_DRAINING) {
            /* This coordinator never acquired this pending peer (or already
             * positively released it). There is no native cleanup to invent. */
            if (!SudekiMpLanPartyReleaseDrained(h->session,&p.lease))
                report->failed_mask |= bit;
            continue;
        }
        if (!a->actor && p.phase == SUDEKIMP_LAN_PARTY_PENDING && p.transport_confirmed &&
            !InterlockedCompareExchange(&h->stopping,0,0)) {
            if (report->roster_status != SUDEKIMP_LAN_PARTY_ROSTER_READY) {
                report->waiting_mask |= bit; continue;
            }
            void *actor = SudekiMpLanPartyControlObserveActor(w,seat);
            BOOL acquired = actor && SudekiMpLanPartyControlAcquire(w,&p.lease,actor,h->drained);
            if (acquired || (actor && SudekiMpLanPartyControlRetains(w,&p.lease,actor))) {
                a->lease = p.lease; a->actor = actor;
                if (!acquired || !SudekiMpLanPartyApprove(h->session,&p.lease)) {
                    a->draining = TRUE;
                    (void)SudekiMpLanPartyDisconnect(h->session,&p.lease);
                    report->failed_mask |= bit; continue;
                }
                p.phase = SUDEKIMP_LAN_PARTY_ACTIVE;
            } else { report->waiting_mask |= bit; continue; }
        }
        if (!a->actor) {
            if (p.phase == SUDEKIMP_LAN_PARTY_ACTIVE) {
                /* A foreign approval is not this coordinator's native lease. */
                (void)SudekiMpLanPartyDisconnect(h->session,&p.lease);
                report->failed_mask |= bit;
            }
            continue;
        }
        if (!SudekiMpLanPartyControlExact(w,&a->lease,a->actor)) {
            SudekiMpLogFormat("lan_party event=host_disconnect seat=%u reason=native_lease_not_exact tick=%lu\r\n",
                seat,(unsigned long)now);
            a->draining = TRUE;
            (void)SudekiMpLanPartyDisconnect(h->session,&a->lease);
            report->failed_mask |= bit; continue;
        }
        report->owned_mask |= bit;
        if (report->roster_status != SUDEKIMP_LAN_PARTY_ROSTER_READY) {
            /* An unreadable/inconsistent sibling is not a complete canonical
             * world. Retain ownership, but neither consume nor ACK its input
             * while the roster proof is unknown. Stop only through the exact
             * still-owned movement path, never by writing a stale pointer. */
            report->waiting_mask |= bit;
            a->have_input = FALSE;
            if(!release_block(w,a)) {
                a->draining=TRUE;
                (void)SudekiMpLanPartyDisconnect(h->session,&a->lease);
                report->failed_mask|=bit;
                continue;
            }
            if (!a->stopped) {
                if (SudekiMpLanPartyControlMove(w,&a->lease,a->actor,0,0,0,0,FALSE))
                    a->stopped = TRUE;
                else report->failed_mask |= bit;
            }
            continue;
        }
        if (SudekiMpLanPartyTakeInput(h->session,seat,&next)) {
            if (!same_lease(&a->lease,&next.lease) ||
                next.input.actor_type != SudekiMpLanPartyActorType(seat)) {
                a->have_input = FALSE;
                report->failed_mask |= bit;
            } else if (!ordinary_input(&next.input,&next.combat)) {
                a->have_input = FALSE;
                report->unsupported_input_mask |= bit;
            } else {
                a->input = next; a->have_input = TRUE; new_input=TRUE;
                a->action_serviced = FALSE; admitted = TRUE;
            }
        }
        fresh = a->have_input && input_fresh(now,a->input.received_at_ms);
        if(a->weapon_swap && now-a->weapon_swap_started>=SUDEKIMP_RANGED_WEAPON_SWAP_MS)
            a->weapon_swap=FALSE;
        if (!SudekiMpLanPartyLeaseActive(h->session,&a->lease)) {
            a->draining = TRUE;
            (void)retire(h,a,w);
            continue;
        }
        /* Retail recharges only the local controller's gun. Elco is remote
         * on this host: reuse the exact two-seat rapid-weapon adapter once
         * per native update, including frames with the trigger released.
         * It verifies the actual item, authored rate/cycle and Buki controller
         * before entering native recharge. Other guns retain native readiness
         * and reload gates; their full reload duration is not a shot interval. */
        if(seat==1u && fresh && a->input.input.ranged_first_person_active &&
            !SudekiMpLanPartyCastActive(a->actor) &&
            isfinite(frame_delta) && frame_delta>0.0f && frame_delta<=0.25f) {
            BOOL combat=FALSE;
            if(host_combat_mode(&combat) && combat)
                (void)SudekiMpServiceRemoteRapidWeapon(a->actor,
                    h->roster.bound.actors[0],frame_delta,&repeat_ms);
        }
        if(seat==1u) {
            BOOL held=fresh && a->input.combat.flight_held;
            if(!SudekiMpLanPartyJetpackInput(w,&a->lease,a->actor,held) && held) {
                admitted=FALSE;
                report->unsupported_input_mask|=bit;
            }
        }
        /* Retail handles block transitions before movement. In particular,
         * a native movement lock must never swallow the release transition. */
        if(new_input && fresh && !SudekiMpLanPartyCastActive(a->actor)) {
            BOOL combat=FALSE;
            BOOL requested=a->input.combat.block_held!=0u;
            unsigned int block_state=requested ? (a->block_held?2u:1u) :
                (a->block_held?3u:0u);
            if(block_state) {
                if(!host_combat_mode(&combat) ||
                    (combat && !SudekiMpLanPartyControlSubmitBlockState(
                        w,&a->lease,a->actor,block_state)) ||
                    (!combat && requested)) {
                    admitted=FALSE;
                    report->unsupported_input_mask|=bit;
                } else a->block_held=requested;
            }
        }
        if(!fresh && a->block_held) {
            if(!release_block(w,a)) {
                report->failed_mask|=bit;
                a->draining=TRUE;
                (void)SudekiMpLanPartyDisconnect(h->session,&a->lease);
                continue;
            }
        }
        if(fresh && SudekiMpLanPartyCastTargeting(a->actor)) {
            const SudekiMpLanArenaInput *i=&a->input.input;
            if((i->aim_direction_x || i->aim_direction_z) &&
                !SudekiMpLanPartyControlSkillFacing(w,&a->lease,a->actor,
                    axis(i->aim_direction_x),axis(i->aim_direction_z))) {
                admitted=FALSE;
                report->unsupported_input_mask|=bit;
            }
        }
        if ((fresh || !a->stopped) && !SudekiMpLanPartyCastActive(a->actor)) {
            const SudekiMpLanArenaInput *i = &a->input.input;
            BOOL ok = fresh && seat==2u && a->block_held ?
                SudekiMpLanPartyControlSubmitDodge(w,&a->lease,a->actor,
                    axis(i->world_direction_x),axis(i->world_direction_z)) :
                SudekiMpLanPartyControlMove(w,&a->lease,a->actor,
                fresh ? axis(i->world_direction_x) : 0,
                fresh ? axis(i->world_direction_z) : 0,
                fresh ? axis(i->aim_direction_x) : 0,
                fresh ? axis(i->aim_direction_z) : 0,
                fresh && i->ranged_first_person_active && seat != 2u &&
                    (i->aim_direction_x != 0 || i->aim_direction_z != 0));
            if (!ok) {
                DWORD error=GetLastError();
                if(error==ERROR_BUSY &&
                    SudekiMpLanPartyControlExact(w,&a->lease,a->actor)) {
                    if(!a->movement_busy)
                        SudekiMpLogFormat("lan_party event=movement_deferred seat=%u reason=native_movement_disabled tick=%lu\r\n",
                            seat,(unsigned long)now);
                    a->movement_busy=TRUE;
                    /* Consume no gameplay edges or ACK while blocked. Require
                     * a new fresh packet when the native controller resumes. */
                    a->have_input=FALSE;
                    a->stopped=FALSE;
                    report->waiting_mask|=bit;
                    continue;
                }
                SudekiMpLogFormat("lan_party event=host_disconnect seat=%u reason=movement_validation_failed error=%lu tick=%lu\r\n",
                    seat,(unsigned long)error,(unsigned long)now);
                a->draining = TRUE;
                (void)SudekiMpLanPartyDisconnect(h->session,&a->lease);
                report->failed_mask |= bit; continue;
            }
            if(a->movement_busy)
                SudekiMpLogFormat("lan_party event=movement_resumed seat=%u tick=%lu\r\n",
                    seat,(unsigned long)now);
            a->movement_busy=FALSE;
            a->stopped = !fresh;
        }
        if(new_input && fresh && (a->input.input.skill_pressed ||
            a->input.input.kit_action==SUDEKIMP_LAN_ARENA_KIT_SPIRIT)) {
            BOOL spirit=a->input.input.kit_action==SUDEKIMP_LAN_ARENA_KIT_SPIRIT;
            BOOL combat=FALSE;
            BOOL started=host_combat_mode(&combat) && combat && release_block(w,a) &&
                SudekiMpLanPartyCastSubmit(w,&a->lease,a->actor,spirit,
                    spirit?a->input.input.kit_slot:a->input.input.skill_slot);
            a->action_serviced=TRUE;
            if(!started) { admitted=FALSE; report->unsupported_input_mask|=bit; }
        }
        if(fresh && new_input && !a->action_serviced &&
            (a->input.combat.strong_pressed || a->input.combat.sweep_pressed)) {
            BOOL combat=FALSE;
            BOOL action_admitted=host_combat_mode(&combat) && combat &&
                SudekiMpLanPartyControlSubmitMelee(w,&a->lease,a->actor,
                    a->input.input.weak_attack_pressed,
                    a->input.combat.strong_pressed,
                    a->input.combat.sweep_pressed);
            a->action_serviced=TRUE;
            if(!action_admitted) {
                admitted=FALSE;
                report->unsupported_input_mask|=bit;
            }
        }
        if (fresh && a->input.input.weak_attack_pressed && !a->action_serviced) {
            BOOL combat = FALSE;
            BOOL ranged_ready=!a->weapon_swap;
            if(a->input.input.ranged_first_person_active &&
                (a->input.input.actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
                 a->input.input.actor_type==SUDEKIMP_LAN_ARENA_AILISH_TYPE))
                ranged_ready=ranged_ready && !ranged_action_active(a->actor,a->input.input.actor_type) &&
                    ranged_weapon_ready(w,&a->lease,a->actor,
                        a->input.input.actor_type);
            BOOL action_admitted = ranged_ready &&
                host_combat_mode(&combat) && combat &&
                SudekiMpLanPartyControlSubmitWeakAttack(w, &a->lease, a->actor);
            a->action_serviced = TRUE;
            if(action_admitted && a->input.input.ranged_first_person_active)
                a->next_held_fire_at=now+repeat_ms;
            if (!action_admitted) {
                /* The actor-local native arbiter is the action validator.
                 * Keep locomotion alive,
                 * but do not advance this action-bearing input ACK. */
                admitted = FALSE;
                report->unsupported_input_mask |= bit;
            }
        }
        if(fresh && a->input.input.ranged_first_person_active &&
            a->input.input.weak_attack_held && !a->action_serviced) {
            if(!a->next_held_fire_at) a->next_held_fire_at=now;
            if((int32_t)(now-a->next_held_fire_at)>=0) {
                BOOL combat=FALSE;
                if(!a->weapon_swap && !ranged_action_active(a->actor,a->input.input.actor_type) &&
                    ranged_weapon_ready(w,&a->lease,a->actor,
                        a->input.input.actor_type) &&
                    host_combat_mode(&combat) && combat &&
                    SudekiMpLanPartyControlSubmitWeakAttack(w,&a->lease,a->actor))
                    a->next_held_fire_at=now+repeat_ms;
                else a->next_held_fire_at=now+50u;
                a->action_serviced=TRUE;
            }
        }
        if(fresh && (!a->input.input.ranged_first_person_active ||
            !a->input.input.weak_attack_held)) a->next_held_fire_at=0;
        if(fresh && a->input.input.kit_action==SUDEKIMP_LAN_ARENA_KIT_WEAPON &&
            !a->action_serviced) {
            BOOL changed=FALSE;
            BOOL selected=submit_weapon_selection(w,&a->lease,a->actor,
                a->input.input.kit_slot,&changed);
            if(changed && seat==1u) {
                /* Remote equipment activation lacks Elco's local C1 input
                 * interval. Reserve its authored duration on the host too;
                 * the client independently plays the validated native clip. */
                a->weapon_swap=TRUE;
                a->weapon_swap_started=now;
                a->next_held_fire_at=now+SUDEKIMP_RANGED_WEAPON_SWAP_MS;
                SudekiMpLogFormat("lan_party event=weapon_swap seat=%u slot=%u duration_ms=%u tick=%lu\r\n",
                    seat,a->input.input.kit_slot,SUDEKIMP_RANGED_WEAPON_SWAP_MS,
                    (unsigned long)now);
            }
            a->action_serviced=TRUE;
            if(!selected) {
                admitted=FALSE;
                report->unsupported_input_mask|=bit;
            }
        }
        if (!fresh) a->have_input = FALSE;
        /* Never acknowledge an action-bearing packet, an expired movement,
         * failed native submission, or input from a replaced peer. */
        if (admitted && fresh &&
            (!a->input.input.weak_attack_pressed ||
             a->action_serviced) &&
            !SudekiMpLanPartyAdmitInput(h->session,&a->input))
            report->failed_mask |= bit;
    }
    return TRUE;
}
BOOL SudekiMpLanPartyHostControlService(SudekiMpLanPartyHostControl *h,
    const SudekiMpControlUpdateDispatchWitness *w,uint32_t now,
    SudekiMpLanPartyHostControlReport *report) {
    return SudekiMpLanPartyHostControlServiceFrame(h,w,now,0.0f,report);
}
BOOL SudekiMpLanPartyHostControlLatestInput(SudekiMpLanPartyHostControl *h,
    const SudekiMpControlUpdateDispatchWitness *w,unsigned seat,uint32_t now,
    SudekiMpLanArenaInput *input) {
    HostActor *a;
    if(!h || !input || seat==0u || seat>=4u ||
        (w && !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w))) return FALSE;
    a=&h->actors[seat-1u];
    if(!a->actor || a->draining || !a->have_input ||
        !input_fresh(now,a->input.received_at_ms) ||
        !SudekiMpLanPartyLeaseActive(h->session,&a->lease) ||
        a->input.input.actor_type!=SudekiMpLanPartyActorType(seat) ||
        !same_lease(&a->lease,&a->input.lease) ||
        !(w ? SudekiMpLanPartyControlExact(w,&a->lease,a->actor) :
            SudekiMpLanPartyControlRetainedNativeThreadExact(&a->lease,a->actor)))
        return FALSE;
    *input=a->input.input;
    return TRUE;
}
