#include "engine/talos_encounter_admission.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpTalosEncounterProvenance provenance_for_mask(uint8_t mask) {
    SudekiMpTalosEncounterProvenance provenance;
    unsigned int seat;

    memset(&provenance, 0, sizeof(provenance));
    provenance.serial = 41u;
    provenance.transition_serial = 7u;
    provenance.world_generation = 10u;
    provenance.source_generation = 20u;
    provenance.host_actor = (uintptr_t)0x2000u;
    provenance.host_actor_generation = 50u;
    provenance.host_lease_generation = 31u;
    provenance.talos_health_target = 180000u;
    provenance.active_human_mask = mask;
    for (seat = 0u; seat < SUDEKIMP_TALOS_HERO_COUNT; ++seat) {
        provenance.hero_actor_identity[seat] =
            (uintptr_t)(0x2000u + seat * 0x100u);
        provenance.hero_actor_generation[seat] = 50u + seat;
    }
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        provenance.controller_slot_by_seat[seat] =
            SUDEKIMP_TALOS_CONTROLLER_NONE;
        if ((mask & (uint8_t)(1u << seat)) != 0u) {
            provenance.input_identity_by_seat[seat] =
                (uintptr_t)(0x5000u + seat * 0x100u);
            provenance.input_generation_by_seat[seat] = 100u + seat;
            if (seat != 0u) {
                provenance.controller_slot_by_seat[seat] =
                    (uint8_t)(seat - 1u);
            }
        }
    }
    provenance.hero_by_seat[0] = SUDEKIMP_TALOS_HERO_TAL;
    provenance.hero_by_seat[1] = (mask & 0x02u) != 0u ?
        SUDEKIMP_TALOS_HERO_AILISH : SUDEKIMP_TALOS_HERO_NONE;
    provenance.hero_by_seat[2] = (mask & 0x04u) != 0u ?
        SUDEKIMP_TALOS_HERO_BUKI : SUDEKIMP_TALOS_HERO_NONE;
    provenance.hero_by_seat[3] = (mask & 0x08u) != 0u ?
        SUDEKIMP_TALOS_HERO_ELCO : SUDEKIMP_TALOS_HERO_NONE;
    provenance.combatant_count = 4u;
    return provenance;
}

static SudekiMpTalosAdmissionObservation observation_for(
    const SudekiMpTalosEncounterProvenance *provenance
) {
    SudekiMpTalosAdmissionObservation observation;
    unsigned int hero_index;

    memset(&observation, 0, sizeof(observation));
    observation.encounter_serial = provenance->serial;
    observation.request_transition_serial = provenance->transition_serial;
    observation.request_world_generation = provenance->world_generation;
    observation.request_source_generation = provenance->source_generation;
    observation.request_host_actor = provenance->host_actor;
    observation.request_host_actor_generation =
        provenance->host_actor_generation;
    observation.request_host_lease_generation =
        provenance->host_lease_generation;
    observation.arrival_world_generation = 11u;
    observation.arrival_source_generation = 21u;
    observation.active_human_mask = provenance->active_human_mask;
    observation.group_count = 4u;
    observation.arrival_ready = 1;
    observation.group_observed = 1;

    for (hero_index = 0u; hero_index < SUDEKIMP_TALOS_HERO_COUNT;
            ++hero_index) {
        SudekiMpTalosHeroObservation *hero =
            &observation.heroes[hero_index];
        unsigned int seat = SudekiMpTalosEncounterSeatForHero(
            provenance, (SudekiMpTalosHero)hero_index);

        hero->actor_identity = (uintptr_t)(0x2000u + hero_index * 0x100u);
        hero->resource_actor_identity = hero->actor_identity;
        hero->actor_generation = 50u + hero_index;
        hero->group_occurrences = 1u;
        hero->ai_component_identity = hero->actor_identity + 0x40u;
        hero->ai_owner_identity = hero->actor_identity;
        hero->ai_mode_state_identity = hero->actor_identity + 0x80u;
        hero->targeter_identity = hero->actor_identity + 0xc0u;
        hero->identity_observed = 1;
        hero->control_observed = 1;
        if (seat == SUDEKIMP_TALOS_SEAT_AI) {
            hero->input_owner_seat = SUDEKIMP_TALOS_SEAT_AI;
            hero->control_reference = 0;
            hero->ai_enabled = 1u;
            hero->ally_target_category_enabled = 1;
            hero->targeting_observed = 1;
            hero->current_target_observed = 1;
            hero->current_target_is_talos_encounter_threat = 1;
        } else {
            hero->input_owner_seat = (uint8_t)seat;
            hero->lease_actor_identity = hero->actor_identity;
            hero->lease_actor_generation = hero->actor_generation;
            hero->input_identity =
                provenance->input_identity_by_seat[seat];
            hero->input_generation =
                provenance->input_generation_by_seat[seat];
            hero->control_reference = seat == 0u ? 0 : 1;
            hero->ai_enabled = 0u;
        }
        if (seat == SUDEKIMP_TALOS_SEAT_AI) {
            hero->current_target_actor_identity = (uintptr_t)0x9000u;
        }
    }

    observation.boss.actor_identity = (uintptr_t)0x9000u;
    observation.boss.resource_actor_identity = (uintptr_t)0x9000u;
    observation.boss.actor_generation = 77u;
    observation.boss.ai_component_identity = (uintptr_t)0x9100u;
    observation.boss.ai_owner_identity = (uintptr_t)0x9000u;
    observation.boss.ai_unit_type = 3u;
    observation.boss.combat_identity = (uintptr_t)0x9f64u;
    observation.boss.combat_owner_identity = (uintptr_t)0x9000u;
    observation.boss.combat_data_identity = (uintptr_t)0xa000u;
    observation.boss.boss_bar_identity = (uintptr_t)0xb000u;
    observation.boss.boss_bar_entity_identity = (uintptr_t)0x9000u;
    observation.boss.stat_display_identity = (uintptr_t)0xc000u;
    observation.boss.current_hp = 45000.0f;
    observation.boss.maximum_hp = 45000.0f;
    observation.boss.health_storage_writable = 1;
    observation.boss.native_health_callback_exact = 1;
    observation.boss.boss_candidate_filter_exact = 1;
    observation.boss.identity_observed = 1;
    observation.boss.health_observed = 1;
    observation.boss.presentation_observed = 1;
    observation.boss.candidate_filter_observed = 1;
    return observation;
}

static void test_one_human_three_native_ai_admits(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x01u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_ADMITTED);
}

static void test_four_humans_do_not_require_ai_filter(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x0fu);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    observation.boss.candidate_filter_observed = 0;
    observation.boss.boss_candidate_filter_exact = 0;
    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
}

static void test_incomplete_native_state_waits_without_mutation(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.group_count = 1u;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_NOT_READY);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_WAITING);
    observation.group_count = 4u;
    observation.boss.presentation_observed = 0;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_NOT_READY);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_WAITING);
}

static void test_stale_input_does_not_poison(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.encounter_serial++;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_WAITING);
}

static void test_same_serial_lineage_mismatch_quarantines(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.request_host_actor = (uintptr_t)0xdeadu;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_PROVENANCE);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.arrival_world_generation = provenance.world_generation;
    observation.arrival_source_generation = provenance.source_generation;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_ARRIVAL_LINEAGE);
}

static void test_wrong_control_or_identity_quarantines(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].control_reference = 0;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_CONTROL_OWNERSHIP);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.heroes[SUDEKIMP_TALOS_HERO_ELCO].actor_identity =
        observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].actor_identity;
    observation.heroes[SUDEKIMP_TALOS_HERO_ELCO].resource_actor_identity =
        observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].actor_identity;
    observation.heroes[SUDEKIMP_TALOS_HERO_ELCO].ai_owner_identity =
        observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].actor_identity;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].actor_identity =
        (uintptr_t)0x88880000u;
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].resource_actor_identity =
        (uintptr_t)0x88880000u;
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].ai_owner_identity =
        (uintptr_t)0x88880000u;
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].lease_actor_identity =
        (uintptr_t)0x88880000u;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.heroes[SUDEKIMP_TALOS_HERO_TAL].actor_identity =
        (uintptr_t)0x77770000u;
    observation.heroes[SUDEKIMP_TALOS_HERO_TAL].resource_actor_identity =
        (uintptr_t)0x77770000u;
    observation.heroes[SUDEKIMP_TALOS_HERO_TAL].ai_owner_identity =
        (uintptr_t)0x77770000u;
    observation.heroes[SUDEKIMP_TALOS_HERO_TAL].lease_actor_identity =
        (uintptr_t)0x77770000u;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY);
}

static void test_controller_assignment_is_immutable(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x07u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    provenance.controller_slot_by_seat[2] =
        provenance.controller_slot_by_seat[1];
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID);

    provenance = provenance_for_mask(0x07u);
    observation = observation_for(&provenance);
    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].input_generation++;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_CONTROL_OWNERSHIP);
}

static void test_ai_targeting_must_be_proven(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].targeting_observed = 0;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_NOT_READY);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_WAITING);

    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].targeting_observed = 1;
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].
        ally_target_category_enabled = 0;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_AI_TARGETING);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].
        ally_target_category_enabled = 1;
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].
        current_target_actor_identity = (uintptr_t)0xd00du;
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].
        current_target_is_talos_encounter_threat = 0;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_NOT_READY);
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].
        current_target_actor_identity = (uintptr_t)0x9200u;
    observation.heroes[SUDEKIMP_TALOS_HERO_BUKI].
        current_target_is_talos_encounter_threat = 1;
    observation.boss.boss_candidate_filter_exact = 0;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_AI_TARGETING);
}

static void test_native_ai_may_idle_or_switch_between_talos_entities(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x01u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;
    SudekiMpTalosHealthTicket ticket;

    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].
        current_target_actor_identity = 0u;
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].
        current_target_is_talos_encounter_threat = 0;
    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);

    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].
        current_target_actor_identity = (uintptr_t)0x9200u;
    observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].
        current_target_is_talos_encounter_threat = 1;
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED);
}

static void test_uncommitted_arrival_can_abandon_safely(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;
    SudekiMpTalosHealthTicket ticket;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionAbandonUncommitted(
        &admission, provenance.serial,
        SUDEKIMP_TALOS_ADMISSION_FAILURE_GROUP_MEMBERSHIP) ==
        SUDEKIMP_TALOS_ADMISSION_RELEASED);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_IDLE);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY);

    ++provenance.serial;
    observation = observation_for(&provenance);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionAbandonUncommitted(
        &admission, provenance.serial,
        SUDEKIMP_TALOS_ADMISSION_FAILURE_AI_TARGETING) ==
        SUDEKIMP_TALOS_ADMISSION_RELEASED);

    ++provenance.serial;
    observation = observation_for(&provenance);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionAbandonUncommitted(
        &admission, provenance.serial,
        SUDEKIMP_TALOS_ADMISSION_FAILURE_HEALTH_COMMIT) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY);
}

static void finalize_with_ticket(
    SudekiMpTalosAdmission *admission,
    SudekiMpTalosAdmissionObservation *observation,
    SudekiMpTalosHealthTicket *ticket
) {
    observation->boss.current_hp = ticket->target_current_hp;
    observation->boss.maximum_hp = ticket->target_maximum_hp;
    CHECK(SudekiMpTalosAdmissionFinalizeHealth(
        admission, ticket, &observation->boss) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_VERIFIED);
}

static void test_one_shot_health_scale_preserves_ratio(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;
    SudekiMpTalosHealthTicket ticket;
    SudekiMpTalosHealthTicket snapshot;

    observation.boss.current_hp = 22500.0f;
    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED);
    CHECK(ticket.action ==
        SUDEKIMP_TALOS_HEALTH_ACTION_SCALE_FROM_VANILLA);
    CHECK(ticket.before_current_hp == 22500.0f);
    CHECK(ticket.before_maximum_hp == 45000.0f);
    CHECK(ticket.target_current_hp == 90000.0f);
    CHECK(ticket.target_maximum_hp == 180000.0f);
    CHECK(SudekiMpTalosAdmissionGetHealthTicket(&admission, &snapshot));
    CHECK(snapshot.encounter_serial == ticket.encounter_serial);
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &snapshot) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY);
    finalize_with_ticket(&admission, &observation, &ticket);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_ACTIVE);
    CHECK(SudekiMpTalosAdmissionFinalizeHealth(
        &admission, &ticket, &observation.boss) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY);
    CHECK(SudekiMpTalosAdmissionRelease(&admission, provenance.serial) ==
        SUDEKIMP_TALOS_ADMISSION_RELEASED);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY);
}

static void test_existing_target_is_verified_not_scaled(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x0fu);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;
    SudekiMpTalosHealthTicket ticket;

    observation.boss.current_hp = 135000.0f;
    observation.boss.maximum_hp = 180000.0f;
    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED);
    CHECK(ticket.action ==
        SUDEKIMP_TALOS_HEALTH_ACTION_VERIFY_EXISTING_TARGET);
    CHECK(ticket.target_current_hp == 135000.0f);
    finalize_with_ticket(&admission, &observation, &ticket);
}

static void test_partial_health_commit_quarantines(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x01u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;
    SudekiMpTalosHealthTicket ticket;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED);
    observation.boss.maximum_hp = 180000.0f;
    observation.boss.current_hp = 45000.0f;
    CHECK(SudekiMpTalosAdmissionFinalizeHealth(
        &admission, &ticket, &observation.boss) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_HEALTH_COMMIT);
}

static void test_claim_revalidates_identity_and_ticket_exactly(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x03u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;
    SudekiMpTalosHealthTicket ticket;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    observation.boss.combat_data_identity++;
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.state == SUDEKIMP_TALOS_ADMISSION_QUARANTINED);

    observation = observation_for(&provenance);
    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_ACCEPTED);
    CHECK(SudekiMpTalosAdmissionClaimHealth(
        &admission, &observation, &ticket) ==
        SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED);
    observation.boss.current_hp = ticket.target_current_hp;
    observation.boss.maximum_hp = ticket.target_maximum_hp;
    ticket.target_maximum_hp -= 0.25f;
    CHECK(SudekiMpTalosAdmissionFinalizeHealth(
        &admission, &ticket, &observation.boss) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_HEALTH_COMMIT);
}

static void test_unexpected_boss_identity_or_health_quarantines(void) {
    SudekiMpTalosEncounterProvenance provenance = provenance_for_mask(0x01u);
    SudekiMpTalosAdmissionObservation observation =
        observation_for(&provenance);
    SudekiMpTalosAdmission admission;

    SudekiMpTalosAdmissionInitialize(&admission);
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    observation.boss.combat_owner_identity = (uintptr_t)0xdeadu;
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_IDENTITY);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.boss.actor_identity =
        observation.heroes[SUDEKIMP_TALOS_HERO_AILISH].actor_identity;
    observation.boss.resource_actor_identity = observation.boss.actor_identity;
    observation.boss.ai_owner_identity = observation.boss.actor_identity;
    observation.boss.combat_owner_identity = observation.boss.actor_identity;
    observation.boss.boss_bar_entity_identity = observation.boss.actor_identity;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_IDENTITY);

    SudekiMpTalosAdmissionInitialize(&admission);
    observation = observation_for(&provenance);
    observation.boss.maximum_hp = 90000.0f;
    CHECK(SudekiMpTalosAdmissionBegin(&admission, &provenance) ==
        SUDEKIMP_TALOS_ADMISSION_STARTED);
    CHECK(SudekiMpTalosAdmissionObserve(&admission, &observation) ==
        SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH);
    CHECK(admission.failure ==
        SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_HEALTH);
}

int main(void) {
    test_one_human_three_native_ai_admits();
    test_four_humans_do_not_require_ai_filter();
    test_incomplete_native_state_waits_without_mutation();
    test_stale_input_does_not_poison();
    test_same_serial_lineage_mismatch_quarantines();
    test_wrong_control_or_identity_quarantines();
    test_controller_assignment_is_immutable();
    test_ai_targeting_must_be_proven();
    test_native_ai_may_idle_or_switch_between_talos_entities();
    test_uncommitted_arrival_can_abandon_safely();
    test_one_shot_health_scale_preserves_ratio();
    test_existing_target_is_verified_not_scaled();
    test_partial_health_commit_quarantines();
    test_claim_revalidates_identity_and_ticket_exactly();
    test_unexpected_boss_identity_or_health_quarantines();

    if (failures != 0) {
        fprintf(stderr, "talos admission checks failed: %d\n", failures);
        return 1;
    }
    puts("talos admission checks passed");
    return 0;
}
