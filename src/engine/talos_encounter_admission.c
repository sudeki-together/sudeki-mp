#include "engine/talos_encounter_admission.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

enum {
    OBSERVATION_INVALID = -1,
    OBSERVATION_WAITING = 0,
    OBSERVATION_VALID = 1
};

static int float_near(float left, float right) {
    return isfinite(left) && isfinite(right) && fabsf(left - right) <= 0.5f;
}

static int serial_is_newer(uint32_t candidate, uint32_t baseline) {
    uint32_t distance = candidate - baseline;

    return distance != 0u && distance < UINT32_C(0x80000000);
}

static void clear_current(SudekiMpTalosAdmission *admission) {
    memset(&admission->provenance, 0, sizeof(admission->provenance));
    memset(&admission->admitted_observation, 0,
        sizeof(admission->admitted_observation));
    memset(&admission->health_ticket, 0,
        sizeof(admission->health_ticket));
    admission->failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_NONE;
    admission->state = SUDEKIMP_TALOS_ADMISSION_IDLE;
}

static void quarantine(
    SudekiMpTalosAdmission *admission,
    SudekiMpTalosAdmissionFailure failure
) {
    admission->failure = failure == SUDEKIMP_TALOS_ADMISSION_FAILURE_NONE ?
        SUDEKIMP_TALOS_ADMISSION_FAILURE_PROVENANCE : failure;
    admission->state = SUDEKIMP_TALOS_ADMISSION_QUARANTINED;
}

static int provenance_valid(
    const SudekiMpTalosEncounterProvenance *provenance
) {
    uint8_t seen = 0u;
    uint8_t seen_controllers = 0u;
    unsigned int seat;
    unsigned int hero;

    if (provenance == NULL || provenance->serial == 0u ||
        provenance->transition_serial == 0u ||
        provenance->world_generation == 0u ||
        provenance->source_generation == 0u ||
        provenance->host_actor == 0u ||
        provenance->host_actor_generation == 0u ||
        provenance->host_lease_generation == 0u ||
        provenance->combatant_count != SUDEKIMP_TALOS_EXPANDED_COMBATANTS ||
        provenance->talos_health_target !=
            SudekiMpTalosEncounterHealthTarget(
                SUDEKIMP_TALOS_EXPANDED_COMBATANTS) ||
        (provenance->active_human_mask &
            (uint8_t)~SUDEKIMP_TALOS_HUMAN_MASK) != 0u ||
        (provenance->active_human_mask & 0x01u) == 0u ||
        provenance->hero_by_seat[0] != SUDEKIMP_TALOS_HERO_TAL) {
        return 0;
    }
    for (hero = 0u; hero < SUDEKIMP_TALOS_HERO_COUNT; ++hero) {
        unsigned int prior;

        if (provenance->hero_actor_identity[hero] == 0u ||
            provenance->hero_actor_generation[hero] == 0u) {
            return 0;
        }
        for (prior = 0u; prior < hero; ++prior) {
            if (provenance->hero_actor_identity[prior] ==
                    provenance->hero_actor_identity[hero]) {
                return 0;
            }
        }
    }
    if (provenance->hero_actor_identity[SUDEKIMP_TALOS_HERO_TAL] !=
            provenance->host_actor ||
        provenance->hero_actor_generation[SUDEKIMP_TALOS_HERO_TAL] !=
            provenance->host_actor_generation) {
        return 0;
    }
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        uint8_t hero = provenance->hero_by_seat[seat];
        uint8_t controller = provenance->controller_slot_by_seat[seat];
        uint8_t bit = (uint8_t)(1u << seat);
        unsigned int prior;

        if ((provenance->active_human_mask & bit) == 0u) {
            if (hero != SUDEKIMP_TALOS_HERO_NONE ||
                controller != SUDEKIMP_TALOS_CONTROLLER_NONE ||
                provenance->input_identity_by_seat[seat] != 0u ||
                provenance->input_generation_by_seat[seat] != 0u) {
                return 0;
            }
            continue;
        }
        if (hero >= SUDEKIMP_TALOS_HERO_COUNT ||
            (seen & (uint8_t)(1u << hero)) != 0u ||
            (seat != 0u && hero == SUDEKIMP_TALOS_HERO_TAL)) {
            return 0;
        }
        if (provenance->input_identity_by_seat[seat] == 0u ||
            provenance->input_generation_by_seat[seat] == 0u ||
            (seat == 0u && controller != SUDEKIMP_TALOS_CONTROLLER_NONE) ||
            (seat != 0u && controller >= 4u)) {
            return 0;
        }
        for (prior = 0u; prior < seat; ++prior) {
            if ((provenance->active_human_mask &
                    (uint8_t)(1u << prior)) != 0u &&
                provenance->input_identity_by_seat[prior] ==
                    provenance->input_identity_by_seat[seat]) {
                return 0;
            }
        }
        if (seat != 0u) {
            uint8_t controller_bit = (uint8_t)(1u << controller);

            if ((seen_controllers & controller_bit) != 0u) {
                return 0;
            }
            seen_controllers |= controller_bit;
        }
        seen |= (uint8_t)(1u << hero);
    }
    return 1;
}

static int lineage_matches(
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosAdmissionObservation *observation
) {
    return observation->encounter_serial == provenance->serial &&
        observation->request_transition_serial ==
            provenance->transition_serial &&
        observation->request_world_generation ==
            provenance->world_generation &&
        observation->request_source_generation ==
            provenance->source_generation &&
        observation->request_host_actor == provenance->host_actor &&
        observation->request_host_actor_generation ==
            provenance->host_actor_generation &&
        observation->request_host_lease_generation ==
            provenance->host_lease_generation &&
        observation->active_human_mask == provenance->active_human_mask;
}

static int hero_identity_valid(
    const SudekiMpTalosHeroObservation *hero
) {
    return hero->actor_identity != 0u &&
        hero->resource_actor_identity == hero->actor_identity &&
        hero->actor_generation != 0u && hero->group_occurrences == 1u &&
        hero->ai_component_identity != 0u &&
        hero->ai_owner_identity == hero->actor_identity &&
        hero->ai_mode_state_identity != 0u;
}

static int hero_control_valid(
    const SudekiMpTalosHeroObservation *hero,
    const SudekiMpTalosEncounterProvenance *provenance,
    unsigned int expected_seat
) {
    if (expected_seat == SUDEKIMP_TALOS_SEAT_AI) {
        return hero->input_owner_seat == SUDEKIMP_TALOS_SEAT_AI &&
            hero->lease_actor_identity == 0u &&
            hero->lease_actor_generation == 0u &&
            hero->input_identity == 0u && hero->input_generation == 0u &&
            hero->control_reference ==
                SUDEKIMP_TALOS_NATIVE_AI_CONTROL_REFERENCE &&
            hero->ai_enabled == SUDEKIMP_TALOS_NATIVE_AI_ENABLED;
    }
    return hero->input_owner_seat == expected_seat &&
        hero->lease_actor_identity == hero->actor_identity &&
        hero->lease_actor_generation == hero->actor_generation &&
        hero->input_identity ==
            provenance->input_identity_by_seat[expected_seat] &&
        hero->input_generation ==
            provenance->input_generation_by_seat[expected_seat] &&
        hero->control_reference == (expected_seat == 0u ?
            SUDEKIMP_TALOS_NATIVE_AI_CONTROL_REFERENCE :
            SUDEKIMP_TALOS_HUMAN_CONTROL_REFERENCE) &&
        hero->ai_enabled == SUDEKIMP_TALOS_HUMAN_AI_ENABLED;
}

static int boss_identity_valid(const SudekiMpTalosBossObservation *boss) {
    return boss->actor_identity != 0u &&
        boss->resource_actor_identity == boss->actor_identity &&
        boss->actor_generation != 0u &&
        boss->ai_component_identity != 0u &&
        boss->ai_owner_identity == boss->actor_identity &&
        boss->ai_unit_type == SUDEKIMP_TALOS_BOSS_AI_UNIT_TYPE &&
        boss->combat_identity != 0u &&
        boss->combat_owner_identity == boss->actor_identity &&
        boss->combat_data_identity != 0u;
}

static int boss_health_valid(const SudekiMpTalosBossObservation *boss) {
    return boss->health_storage_writable && isfinite(boss->current_hp) &&
        isfinite(boss->maximum_hp) && boss->maximum_hp > 0.0f &&
        boss->current_hp >= 0.0f && boss->current_hp <= boss->maximum_hp &&
        (float_near(boss->maximum_hp,
             (float)SUDEKIMP_TALOS_BASE_HEALTH) ||
         float_near(boss->maximum_hp,
             (float)(SUDEKIMP_TALOS_BASE_HEALTH *
                 SUDEKIMP_TALOS_EXPANDED_COMBATANTS)));
}

static int boss_presentation_valid(
    const SudekiMpTalosBossObservation *boss
) {
    return boss->boss_bar_identity != 0u &&
        boss->boss_bar_entity_identity == boss->actor_identity &&
        boss->stat_display_identity != 0u &&
        boss->native_health_callback_exact;
}

static int observation_status(
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosAdmissionObservation *observation,
    SudekiMpTalosAdmissionFailure *failure
) {
    uintptr_t actor_identities[SUDEKIMP_TALOS_HERO_COUNT];
    int ai_companion_present = 0;
    unsigned int hero_index;

    *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_NONE;
    if (!lineage_matches(provenance, observation)) {
        *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_PROVENANCE;
        return OBSERVATION_INVALID;
    }
    if (!observation->arrival_ready) {
        return OBSERVATION_WAITING;
    }
    if (observation->arrival_world_generation == 0u ||
        observation->arrival_source_generation == 0u ||
        (observation->arrival_world_generation ==
             provenance->world_generation &&
         observation->arrival_source_generation ==
             provenance->source_generation)) {
        *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_ARRIVAL_LINEAGE;
        return OBSERVATION_INVALID;
    }
    if (!observation->group_observed ||
        observation->group_count < SUDEKIMP_TALOS_HERO_COUNT) {
        return OBSERVATION_WAITING;
    }
    if (observation->group_count != SUDEKIMP_TALOS_HERO_COUNT) {
        *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_GROUP_MEMBERSHIP;
        return OBSERVATION_INVALID;
    }

    for (hero_index = 0u; hero_index < SUDEKIMP_TALOS_HERO_COUNT;
            ++hero_index) {
        const SudekiMpTalosHeroObservation *hero =
            &observation->heroes[hero_index];
        unsigned int expected_seat = SudekiMpTalosEncounterSeatForHero(
            provenance, (SudekiMpTalosHero)hero_index);
        unsigned int prior;

        if (!hero->identity_observed || !hero->control_observed) {
            return OBSERVATION_WAITING;
        }
        if (!hero_identity_valid(hero)) {
            *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY;
            return OBSERVATION_INVALID;
        }
        if (hero->actor_identity !=
                provenance->hero_actor_identity[hero_index] ||
            hero->actor_generation !=
                provenance->hero_actor_generation[hero_index]) {
            *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY;
            return OBSERVATION_INVALID;
        }
        for (prior = 0u; prior < hero_index; ++prior) {
            if (actor_identities[prior] == hero->actor_identity) {
                *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY;
                return OBSERVATION_INVALID;
            }
        }
        actor_identities[hero_index] = hero->actor_identity;
        if (!hero_control_valid(hero, provenance, expected_seat)) {
            *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_CONTROL_OWNERSHIP;
            return OBSERVATION_INVALID;
        }
        if (expected_seat == SUDEKIMP_TALOS_SEAT_AI) {
            ai_companion_present = 1;
            if (!hero->targeting_observed ||
                !hero->current_target_observed) {
                return OBSERVATION_WAITING;
            }
            if (hero->targeter_identity == 0u ||
                !hero->ally_target_category_enabled) {
                *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_AI_TARGETING;
                return OBSERVATION_INVALID;
            }
            if (hero->current_target_actor_identity != 0u &&
                !hero->current_target_is_talos_encounter_threat) {
                return OBSERVATION_WAITING;
            }
        }
    }

    if (!observation->boss.identity_observed) {
        return OBSERVATION_WAITING;
    }
    if (!boss_identity_valid(&observation->boss)) {
        *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_IDENTITY;
        return OBSERVATION_INVALID;
    }
    for (hero_index = 0u; hero_index < SUDEKIMP_TALOS_HERO_COUNT;
            ++hero_index) {
        if (observation->boss.actor_identity == actor_identities[hero_index]) {
            *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_IDENTITY;
            return OBSERVATION_INVALID;
        }
    }
    if (!observation->boss.health_observed) {
        return OBSERVATION_WAITING;
    }
    if (!boss_health_valid(&observation->boss)) {
        *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_HEALTH;
        return OBSERVATION_INVALID;
    }
    if (!observation->boss.presentation_observed) {
        return OBSERVATION_WAITING;
    }
    if (!boss_presentation_valid(&observation->boss)) {
        *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_PRESENTATION;
        return OBSERVATION_INVALID;
    }
    if (ai_companion_present) {
        if (!observation->boss.candidate_filter_observed) {
            return OBSERVATION_WAITING;
        }
        if (!observation->boss.boss_candidate_filter_exact) {
            *failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_AI_TARGETING;
            return OBSERVATION_INVALID;
        }
    }
    return OBSERVATION_VALID;
}

static int admitted_identities_match(
    const SudekiMpTalosAdmissionObservation *admitted,
    const SudekiMpTalosAdmissionObservation *current
) {
    unsigned int hero_index;

    if (admitted->arrival_world_generation !=
            current->arrival_world_generation ||
        admitted->arrival_source_generation !=
            current->arrival_source_generation ||
        admitted->active_human_mask != current->active_human_mask ||
        admitted->group_count != current->group_count ||
        admitted->boss.actor_identity != current->boss.actor_identity ||
        admitted->boss.resource_actor_identity !=
            current->boss.resource_actor_identity ||
        admitted->boss.actor_generation != current->boss.actor_generation ||
        admitted->boss.ai_component_identity !=
            current->boss.ai_component_identity ||
        admitted->boss.ai_owner_identity !=
            current->boss.ai_owner_identity ||
        admitted->boss.ai_unit_type != current->boss.ai_unit_type ||
        admitted->boss.combat_identity != current->boss.combat_identity ||
        admitted->boss.combat_owner_identity !=
            current->boss.combat_owner_identity ||
        admitted->boss.combat_data_identity !=
            current->boss.combat_data_identity ||
        admitted->boss.boss_bar_identity !=
            current->boss.boss_bar_identity ||
        admitted->boss.boss_bar_entity_identity !=
            current->boss.boss_bar_entity_identity ||
        admitted->boss.stat_display_identity !=
            current->boss.stat_display_identity) {
        return 0;
    }
    for (hero_index = 0u; hero_index < SUDEKIMP_TALOS_HERO_COUNT;
            ++hero_index) {
        const SudekiMpTalosHeroObservation *left =
            &admitted->heroes[hero_index];
        const SudekiMpTalosHeroObservation *right =
            &current->heroes[hero_index];

        if (left->actor_identity != right->actor_identity ||
            left->resource_actor_identity !=
                right->resource_actor_identity ||
            left->actor_generation != right->actor_generation ||
            left->group_occurrences != right->group_occurrences ||
            left->ai_component_identity != right->ai_component_identity ||
            left->ai_owner_identity != right->ai_owner_identity ||
            left->ai_mode_state_identity != right->ai_mode_state_identity ||
            left->control_reference != right->control_reference ||
            left->ai_enabled != right->ai_enabled ||
            left->input_owner_seat != right->input_owner_seat ||
            left->lease_actor_identity != right->lease_actor_identity ||
            left->lease_actor_generation != right->lease_actor_generation ||
            left->input_identity != right->input_identity ||
            left->input_generation != right->input_generation ||
            left->targeter_identity != right->targeter_identity ||
            left->ally_target_category_enabled !=
                right->ally_target_category_enabled) {
            return 0;
        }
    }
    return 1;
}

static int health_ticket_equal(
    const SudekiMpTalosHealthTicket *left,
    const SudekiMpTalosHealthTicket *right
) {
    return left != NULL && right != NULL &&
        left->encounter_serial == right->encounter_serial &&
        left->action == right->action &&
        left->boss_actor_identity == right->boss_actor_identity &&
        left->boss_actor_generation == right->boss_actor_generation &&
        left->boss_ai_component_identity ==
            right->boss_ai_component_identity &&
        left->combat_identity == right->combat_identity &&
        left->combat_data_identity == right->combat_data_identity &&
        left->boss_bar_identity == right->boss_bar_identity &&
        left->boss_bar_entity_identity ==
            right->boss_bar_entity_identity &&
        left->stat_display_identity == right->stat_display_identity &&
        left->before_current_hp == right->before_current_hp &&
        left->before_maximum_hp == right->before_maximum_hp &&
        left->target_current_hp == right->target_current_hp &&
        left->target_maximum_hp == right->target_maximum_hp;
}

void SudekiMpTalosAdmissionInitialize(SudekiMpTalosAdmission *admission) {
    if (admission != NULL) {
        memset(admission, 0, sizeof(*admission));
    }
}

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionBegin(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    if (admission == NULL || !provenance_valid(provenance)) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID;
    }
    if (admission->state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED;
    }
    if (provenance->serial == admission->claimed_health_serial ||
        (admission->highest_terminal_serial != 0u &&
         !serial_is_newer(
             provenance->serial, admission->highest_terminal_serial))) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY;
    }
    if (admission->state != SUDEKIMP_TALOS_ADMISSION_IDLE) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY;
    }
    admission->provenance = *provenance;
    admission->failure = SUDEKIMP_TALOS_ADMISSION_FAILURE_NONE;
    admission->state = SUDEKIMP_TALOS_ADMISSION_WAITING;
    return SUDEKIMP_TALOS_ADMISSION_STARTED;
}

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionObserve(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosAdmissionObservation *observation
) {
    SudekiMpTalosAdmissionFailure failure;
    int status;

    if (admission == NULL || observation == NULL) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID;
    }
    if (admission->state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED;
    }
    if (admission->state != SUDEKIMP_TALOS_ADMISSION_WAITING) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY;
    }
    if (observation->encounter_serial != admission->provenance.serial) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE;
    }
    status = observation_status(
        &admission->provenance, observation, &failure);
    if (status == OBSERVATION_WAITING) {
        return SUDEKIMP_TALOS_ADMISSION_NOT_READY;
    }
    if (status == OBSERVATION_INVALID) {
        quarantine(admission, failure);
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH;
    }
    admission->admitted_observation = *observation;
    admission->state = SUDEKIMP_TALOS_ADMISSION_ADMITTED;
    return SUDEKIMP_TALOS_ADMISSION_ACCEPTED;
}

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionClaimHealth(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosAdmissionObservation *current,
    SudekiMpTalosHealthTicket *ticket
) {
    SudekiMpTalosAdmissionFailure failure;
    SudekiMpTalosHealthTicket next;
    int status;

    if (admission == NULL || current == NULL || ticket == NULL) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID;
    }
    if (admission->state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED;
    }
    if (current->encounter_serial == admission->claimed_health_serial) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY;
    }
    if (current->encounter_serial != admission->provenance.serial) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE;
    }
    if (admission->state != SUDEKIMP_TALOS_ADMISSION_ADMITTED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY;
    }
    status = observation_status(
        &admission->provenance, current, &failure);
    if (status != OBSERVATION_VALID ||
        !admitted_identities_match(&admission->admitted_observation, current)) {
        quarantine(admission, status == OBSERVATION_INVALID ? failure :
            SUDEKIMP_TALOS_ADMISSION_FAILURE_PROVENANCE);
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH;
    }

    memset(&next, 0, sizeof(next));
    next.encounter_serial = current->encounter_serial;
    next.boss_actor_identity = current->boss.actor_identity;
    next.boss_actor_generation = current->boss.actor_generation;
    next.boss_ai_component_identity =
        current->boss.ai_component_identity;
    next.combat_identity = current->boss.combat_identity;
    next.combat_data_identity = current->boss.combat_data_identity;
    next.boss_bar_identity = current->boss.boss_bar_identity;
    next.boss_bar_entity_identity =
        current->boss.boss_bar_entity_identity;
    next.stat_display_identity = current->boss.stat_display_identity;
    next.before_current_hp = current->boss.current_hp;
    next.before_maximum_hp = current->boss.maximum_hp;
    next.target_maximum_hp = (float)admission->provenance.talos_health_target;
    if (float_near(current->boss.maximum_hp,
            (float)SUDEKIMP_TALOS_BASE_HEALTH)) {
        next.action = SUDEKIMP_TALOS_HEALTH_ACTION_SCALE_FROM_VANILLA;
        next.target_current_hp = current->boss.current_hp *
            next.target_maximum_hp / current->boss.maximum_hp;
    } else {
        next.action = SUDEKIMP_TALOS_HEALTH_ACTION_VERIFY_EXISTING_TARGET;
        next.target_current_hp = current->boss.current_hp;
    }
    admission->health_ticket = next;
    admission->claimed_health_serial = next.encounter_serial;
    admission->state = SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIMED;
    *ticket = next;
    return SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED;
}

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionFinalizeHealth(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosHealthTicket *ticket,
    const SudekiMpTalosBossObservation *after
) {
    if (admission == NULL || ticket == NULL || after == NULL) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID;
    }
    if (admission->state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED;
    }
    if (ticket->encounter_serial != admission->provenance.serial) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE;
    }
    if (admission->state != SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIMED) {
        return ticket->encounter_serial == admission->claimed_health_serial ?
            SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY :
            SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY;
    }
    if (!health_ticket_equal(&admission->health_ticket, ticket) ||
        !after->identity_observed || !after->health_observed ||
        !after->presentation_observed ||
        !boss_identity_valid(after) || !boss_presentation_valid(after) ||
        !after->health_storage_writable ||
        after->actor_identity != ticket->boss_actor_identity ||
        after->actor_generation != ticket->boss_actor_generation ||
        after->ai_component_identity !=
            ticket->boss_ai_component_identity ||
        after->combat_identity != ticket->combat_identity ||
        after->combat_data_identity != ticket->combat_data_identity ||
        after->boss_bar_identity != ticket->boss_bar_identity ||
        after->boss_bar_entity_identity !=
            ticket->boss_bar_entity_identity ||
        after->stat_display_identity != ticket->stat_display_identity ||
        !float_near(after->maximum_hp, ticket->target_maximum_hp) ||
        !float_near(after->current_hp, ticket->target_current_hp)) {
        quarantine(admission,
            SUDEKIMP_TALOS_ADMISSION_FAILURE_HEALTH_COMMIT);
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH;
    }
    admission->state = SUDEKIMP_TALOS_ADMISSION_ACTIVE;
    return SUDEKIMP_TALOS_ADMISSION_HEALTH_VERIFIED;
}

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionRelease(
    SudekiMpTalosAdmission *admission,
    uint32_t encounter_serial
) {
    if (admission == NULL || encounter_serial == 0u) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID;
    }
    if (admission->state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED;
    }
    if (encounter_serial != admission->provenance.serial) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE;
    }
    if (admission->state != SUDEKIMP_TALOS_ADMISSION_ACTIVE) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY;
    }
    admission->highest_terminal_serial = encounter_serial;
    clear_current(admission);
    return SUDEKIMP_TALOS_ADMISSION_RELEASED;
}

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionAbandonUncommitted(
    SudekiMpTalosAdmission *admission,
    uint32_t encounter_serial,
    SudekiMpTalosAdmissionFailure failure
) {
    if (admission == NULL || encounter_serial == 0u ||
        failure == SUDEKIMP_TALOS_ADMISSION_FAILURE_NONE) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID;
    }
    if (admission->state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED;
    }
    if (encounter_serial != admission->provenance.serial) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE;
    }
    if (admission->state != SUDEKIMP_TALOS_ADMISSION_WAITING &&
        admission->state != SUDEKIMP_TALOS_ADMISSION_ADMITTED) {
        return SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY;
    }
    admission->highest_terminal_serial = encounter_serial;
    clear_current(admission);
    return SUDEKIMP_TALOS_ADMISSION_RELEASED;
}

void SudekiMpTalosAdmissionQuarantine(
    SudekiMpTalosAdmission *admission,
    SudekiMpTalosAdmissionFailure failure
) {
    if (admission != NULL) {
        quarantine(admission, failure);
    }
}

int SudekiMpTalosAdmissionGetHealthTicket(
    const SudekiMpTalosAdmission *admission,
    SudekiMpTalosHealthTicket *ticket
) {
    if (admission == NULL || ticket == NULL ||
        (admission->state != SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIMED &&
         admission->state != SUDEKIMP_TALOS_ADMISSION_ACTIVE)) {
        return 0;
    }
    *ticket = admission->health_ticket;
    return 1;
}
