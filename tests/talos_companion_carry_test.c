#include "engine/talos_companion_carry.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpTalosEncounterProvenance provenance(void) {
    SudekiMpTalosEncounterProvenance value;
    unsigned int i;

    memset(&value, 0, sizeof(value));
    value.serial = 41u;
    value.transition_serial = 51u;
    value.world_generation = 61u;
    value.source_generation = 71u;
    value.host_actor = (uintptr_t)0x1000u;
    value.host_actor_generation = 81u;
    value.host_lease_generation = 91u;
    value.talos_health_target = 180000u;
    value.combatant_count = 4u;
    value.active_human_mask = 3u;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        value.hero_actor_identity[i] = (uintptr_t)(0x1000u + i * 0x100u);
        value.hero_actor_generation[i] = 81u + i;
        value.hero_by_seat[i] = SUDEKIMP_TALOS_HERO_NONE;
        value.controller_slot_by_seat[i] = SUDEKIMP_TALOS_CONTROLLER_NONE;
    }
    value.hero_by_seat[0] = SUDEKIMP_TALOS_HERO_TAL;
    value.hero_by_seat[1] = SUDEKIMP_TALOS_HERO_AILISH;
    value.controller_slot_by_seat[1] = 0u;
    value.input_identity_by_seat[0] = (uintptr_t)0x8000u;
    value.input_identity_by_seat[1] = (uintptr_t)0x8100u;
    value.input_generation_by_seat[0] = 101u;
    value.input_generation_by_seat[1] = 102u;
    return value;
}

static SudekiMpTalosCarryLineage lineage(void) {
    SudekiMpTalosCarryLineage value;

    memset(&value, 0, sizeof(value));
    value.source_task = (uintptr_t)0x5000u;
    value.load_void_task = (uintptr_t)0x6000u;
    value.source_task_generation = 11u;
    value.load_void_task_generation = 12u;
    value.runtime_generation = 13u;
    value.source_hash = SUDEKIMP_TALOS_CARRY_SOURCE_HASH;
    value.source_start = SUDEKIMP_TALOS_CARRY_SOURCE_START;
    value.source_opcode = SUDEKIMP_TALOS_CARRY_SOURCE_OPCODE;
    value.load_void_hash = SUDEKIMP_TALOS_CARRY_LOAD_VOID_HASH;
    value.load_void_start = SUDEKIMP_TALOS_CARRY_LOAD_VOID_START;
    value.descendant_proven = 1u;
    return value;
}

static SudekiMpTalosCarryFormation formation(
    const SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarryFormation value;
    unsigned int i;

    memset(&value, 0, sizeof(value));
    value.group_identity = (uintptr_t)0x7000u;
    value.formation_owner_identity = (uintptr_t)0x7100u;
    value.formation_identity = (uintptr_t)0x7200u;
    value.group_generation = 21u;
    value.formation_owner_generation = 22u;
    value.formation_generation = 23u;
    value.group_member_mask = 0x0fu;
    value.formation_member_mask = 0x0fu;
    value.observed = 1u;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        value.actor_identity[i] = provenance_value->hero_actor_identity[i];
        value.actor_generation[i] = provenance_value->hero_actor_generation[i];
    }
    return value;
}

static SudekiMpTalosCarryPreflight preflight(
    const SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarryPreflight value;

    memset(&value, 0, sizeof(value));
    value.lineage = lineage();
    value.formation = formation(provenance_value);
    value.exact_executable = 1u;
    value.exact_asset = 1u;
    value.host_authority = 1u;
    return value;
}

static SudekiMpTalosCarryDeleteObservation deletion(unsigned int index) {
    static const uint32_t logical[] = {
        SUDEKIMP_TALOS_CARRY_BUKI_LOGICAL,
        SUDEKIMP_TALOS_CARRY_AILISH_LOGICAL,
        SUDEKIMP_TALOS_CARRY_ELCO_LOGICAL
    };
    static const uint32_t raw[] = {
        SUDEKIMP_TALOS_CARRY_BUKI_RAW,
        SUDEKIMP_TALOS_CARRY_AILISH_RAW,
        SUDEKIMP_TALOS_CARRY_ELCO_RAW
    };
    static const uint8_t resource[] = {
        SUDEKIMP_TALOS_CARRY_RESOURCE_BUKI,
        SUDEKIMP_TALOS_CARRY_RESOURCE_AILISH,
        SUDEKIMP_TALOS_CARRY_RESOURCE_ELCO
    };
    SudekiMpTalosCarryDeleteObservation value;

    memset(&value, 0, sizeof(value));
    value.lineage = lineage();
    value.logical_opcode = logical[index];
    value.raw_opcode = raw[index];
    value.binding_hash = SUDEKIMP_TALOS_CARRY_DELETE_PC_HASH;
    value.resource_id = resource[index];
    value.exact_executable = 1u;
    value.exact_asset = 1u;
    return value;
}

static SudekiMpTalosCarryFormationObservation formation_observation(
    const SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarryFormationObservation value;

    memset(&value, 0, sizeof(value));
    value.formation = formation(provenance_value);
    value.encounter_serial = provenance_value->serial;
    value.transition_serial = provenance_value->transition_serial;
    value.arrival_world_generation = provenance_value->world_generation + 1u;
    value.arrival_source_generation = provenance_value->source_generation + 1u;
    value.set_zone_logical = SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL;
    value.set_zone_raw = SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW;
    value.set_zone_hash = SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH;
    value.end_tsa_logical = SUDEKIMP_TALOS_CARRY_END_TSA_LOGICAL;
    value.end_tsa_raw = SUDEKIMP_TALOS_CARRY_END_TSA_RAW;
    value.end_tsa_hash = SUDEKIMP_TALOS_CARRY_END_TSA_HASH;
    value.arrival_settled = 1u;
    value.exact_release_point = 1u;
    value.tsa_active = 1u;
    value.tal_final_pop_settled = 1u;
    value.item_use_settled = 1u;
    value.boss_ready = 1u;
    value.no_pending_removal = 1u;
    return value;
}

static SudekiMpTalosCarrySetZoneObservation set_zone_observation(
    const SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarrySetZoneObservation value;

    memset(&value, 0, sizeof(value));
    value.lineage = lineage();
    value.encounter_serial = provenance_value->serial;
    value.transition_serial = provenance_value->transition_serial;
    value.logical_opcode = SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL;
    value.raw_opcode = SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW;
    value.binding_hash = SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH;
    value.exact_executable = 1u;
    value.exact_asset = 1u;
    memcpy(value.destination, "Void", sizeof("Void"));
    return value;
}

static SudekiMpTalosCarryFormationCompletion formation_completion(
    const SudekiMpTalosEncounterProvenance *provenance_value,
    const SudekiMpTalosCarryFormationTicket *ticket
) {
    SudekiMpTalosCarryFormationCompletion value;

    memset(&value, 0, sizeof(value));
    value.formation = formation(provenance_value);
    value.encounter_serial = ticket->encounter_serial;
    value.authorization_serial = ticket->authorization_serial;
    value.arrival_world_generation = ticket->arrival_world_generation;
    value.arrival_source_generation = ticket->arrival_source_generation;
    value.native_call_completed = 1u;
    value.placement_verified = 1u;
    value.no_pending_removal = 1u;
    return value;
}

static void begin_valid(
    SudekiMpTalosCompanionCarry *carry,
    SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarryPreflight preflight_value;

    *provenance_value = provenance();
    preflight_value = preflight(provenance_value);
    SudekiMpTalosCompanionCarryInitialize(carry);
    SudekiMpTalosCompanionCarryConfigure(carry, 1);
    CHECK(SudekiMpTalosCompanionCarryBegin(
        carry, provenance_value, &preflight_value) ==
        SUDEKIMP_TALOS_CARRY_STARTED);
}

static void preserve_all(
    SudekiMpTalosCompanionCarry *carry,
    SudekiMpTalosEncounterProvenance *provenance_value
) {
    unsigned int i;

    begin_valid(carry, provenance_value);
    CHECK(deletion(0u).resource_id == SUDEKIMP_TALOS_CARRY_RESOURCE_BUKI &&
        deletion(1u).resource_id == SUDEKIMP_TALOS_CARRY_RESOURCE_AILISH &&
        deletion(2u).resource_id == SUDEKIMP_TALOS_CARRY_RESOURCE_ELCO);
    for (i = 0u; i < 3u; ++i) {
        SudekiMpTalosCarryDeleteObservation value = deletion(i);
        CHECK(SudekiMpTalosCompanionCarryObserveDelete(
            carry, provenance_value, &value) ==
            SUDEKIMP_TALOS_CARRY_DELETE_SKIP_NATIVE);
    }
    CHECK(carry->state == SUDEKIMP_TALOS_CARRY_PRESERVED);
    CHECK(carry->skipped_mask == 0x07u);
}

static SudekiMpTalosCarrySetZoneTicket pass_set_zone(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarrySetZoneObservation value =
        set_zone_observation(provenance_value);
    SudekiMpTalosCarrySetZoneTicket ticket;

    CHECK(SudekiMpTalosCompanionCarryObserveSetZone(
        carry, provenance_value, &value, &ticket) ==
        SUDEKIMP_TALOS_CARRY_SET_ZONE_ALLOW_NATIVE_ONCE);
    return ticket;
}

static void activate(
    SudekiMpTalosCompanionCarry *carry,
    SudekiMpTalosEncounterProvenance *provenance_value
) {
    SudekiMpTalosCarryFormationObservation observation;
    SudekiMpTalosCarryFormationTicket ticket;
    SudekiMpTalosCarryFormationCompletion completion;
    SudekiMpTalosCarrySetZoneTicket set_zone_ticket;

    preserve_all(carry, provenance_value);
    set_zone_ticket = pass_set_zone(carry, provenance_value);
    observation = formation_observation(provenance_value);
    observation.set_zone_authorization_serial =
        set_zone_ticket.authorization_serial;
    CHECK(SudekiMpTalosCompanionCarryClaimFormation(
        carry, provenance_value, &observation, &ticket) ==
        SUDEKIMP_TALOS_CARRY_FORMATION_AUTHORIZED);
    completion = formation_completion(provenance_value, &ticket);
    CHECK(SudekiMpTalosCompanionCarryFinishFormation(
        carry, &ticket, &completion) ==
        SUDEKIMP_TALOS_CARRY_FORMATION_COMMITTED);
}

static void test_default_inert_and_unrelated_delete_passes(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value = provenance();
    SudekiMpTalosCarryDeleteObservation value = deletion(0u);

    SudekiMpTalosCompanionCarryInitialize(&carry);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &provenance_value, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_PASS_NATIVE);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    value.logical_opcode = 0x1234u;
    value.raw_opcode = 0x5678u;
    value.resource_id = 99u;
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, NULL, &value) == SUDEKIMP_TALOS_CARRY_DELETE_PASS_NATIVE);
}

static void test_preflight_and_first_delete_can_abort_vanilla(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value = provenance();
    SudekiMpTalosCarryPreflight preflight_value = preflight(&provenance_value);
    SudekiMpTalosCarryDeleteObservation value;

    preflight_value.formation.actor_generation[2]++;
    SudekiMpTalosCompanionCarryInitialize(&carry);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    CHECK(SudekiMpTalosCompanionCarryBegin(
        &carry, &provenance_value, &preflight_value) ==
        SUDEKIMP_TALOS_CARRY_ABORT_TO_VANILLA);

    begin_valid(&carry, &provenance_value);
    value = deletion(1u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &provenance_value, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_ABORT_TO_VANILLA);
    CHECK(carry.state == SUDEKIMP_TALOS_CARRY_ABORTED);
}

static void test_partial_skip_uncertainty_quarantines(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value;
    SudekiMpTalosEncounterProvenance stale;
    SudekiMpTalosCarryDeleteObservation value;

    begin_valid(&carry, &provenance_value);
    value = deletion(0u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &provenance_value, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_SKIP_NATIVE);
    stale = provenance_value;
    stale.hero_actor_generation[1]++;
    value = deletion(1u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &stale, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_QUARANTINE);
    CHECK(carry.state == SUDEKIMP_TALOS_CARRY_QUARANTINED);
}

static void test_full_provenance_and_host_tuple_are_bound(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value;
    SudekiMpTalosEncounterProvenance changed;
    SudekiMpTalosCarryPreflight preflight_value;
    SudekiMpTalosCarryDeleteObservation value;

    provenance_value = provenance();
    changed = provenance_value;
    changed.host_actor++;
    preflight_value = preflight(&changed);
    SudekiMpTalosCompanionCarryInitialize(&carry);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    CHECK(SudekiMpTalosCompanionCarryBegin(&carry, &changed,
        &preflight_value) == SUDEKIMP_TALOS_CARRY_ABORT_TO_VANILLA);

    changed = provenance_value;
    changed.host_actor_generation++;
    preflight_value = preflight(&changed);
    SudekiMpTalosCompanionCarryInitialize(&carry);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    CHECK(SudekiMpTalosCompanionCarryBegin(&carry, &changed,
        &preflight_value) == SUDEKIMP_TALOS_CARRY_ABORT_TO_VANILLA);

    begin_valid(&carry, &provenance_value);
    changed = provenance_value;
    changed.controller_slot_by_seat[1] = 1u;
    value = deletion(0u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &changed, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_ABORT_TO_VANILLA);

    begin_valid(&carry, &provenance_value);
    value = deletion(0u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &provenance_value, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_SKIP_NATIVE);
    changed = provenance_value;
    changed.input_generation_by_seat[1]++;
    value = deletion(1u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &changed, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_QUARANTINE);
}

static void test_set_zone_requires_all_deletes_and_is_one_shot(void) {
    unsigned int skipped;

    for (skipped = 1u; skipped <= 2u; ++skipped) {
        SudekiMpTalosCompanionCarry carry;
        SudekiMpTalosEncounterProvenance provenance_value;
        SudekiMpTalosCarrySetZoneObservation zone;
        SudekiMpTalosCarrySetZoneTicket ticket;
        unsigned int i;

        begin_valid(&carry, &provenance_value);
        for (i = 0u; i < skipped; ++i) {
            SudekiMpTalosCarryDeleteObservation value = deletion(i);
            (void)SudekiMpTalosCompanionCarryObserveDelete(
                &carry, &provenance_value, &value);
        }
        zone = set_zone_observation(&provenance_value);
        CHECK(SudekiMpTalosCompanionCarryObserveSetZone(
            &carry, &provenance_value, &zone, &ticket) ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_QUARANTINE);
    }

    {
        SudekiMpTalosCompanionCarry carry;
        SudekiMpTalosEncounterProvenance provenance_value;
        SudekiMpTalosCarrySetZoneObservation zone;
        SudekiMpTalosCarrySetZoneTicket ticket;

        begin_valid(&carry, &provenance_value);
        zone = set_zone_observation(&provenance_value);
        CHECK(SudekiMpTalosCompanionCarryObserveSetZone(
            &carry, &provenance_value, &zone, &ticket) ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_PASS_NATIVE);
        CHECK(carry.state == SUDEKIMP_TALOS_CARRY_ABORTED);

        preserve_all(&carry, &provenance_value);
        zone = set_zone_observation(&provenance_value);
        zone.lineage.load_void_task_generation++;
        CHECK(SudekiMpTalosCompanionCarryObserveSetZone(
            &carry, &provenance_value, &zone, &ticket) ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_QUARANTINE);

        preserve_all(&carry, &provenance_value);
        zone = set_zone_observation(&provenance_value);
        CHECK(SudekiMpTalosCompanionCarryObserveSetZone(
            &carry, &provenance_value, &zone, &ticket) ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_ALLOW_NATIVE_ONCE);
        CHECK(ticket.authorization_serial != 0u);
        CHECK(SudekiMpTalosCompanionCarryObserveSetZone(
            &carry, &provenance_value, &zone, &ticket) ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_BLOCK_REPLAY);
    }
}

static void test_reconfigure_and_upstream_replay_fence(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value;
    SudekiMpTalosEncounterProvenance replay;
    SudekiMpTalosCarryPreflight preflight_value;
    SudekiMpTalosCarryDeleteObservation value;

    begin_valid(&carry, &provenance_value);
    value = deletion(0u);
    (void)SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &provenance_value, &value);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    CHECK(carry.state == SUDEKIMP_TALOS_CARRY_PARTIAL_SKIP);
    SudekiMpTalosCompanionCarryConfigure(&carry, 0);
    CHECK(carry.state == SUDEKIMP_TALOS_CARRY_QUARANTINED);
    CHECK(carry.skipped_mask == 0x01u && carry.enabled);

    activate(&carry, &provenance_value);
    {
        SudekiMpTalosCarryTeardownObservation teardown;
        memset(&teardown, 0, sizeof(teardown));
        teardown.encounter_serial = provenance_value.serial;
        teardown.binding_hash = SUDEKIMP_TALOS_CARRY_REMOVE_ALL_HASH;
        teardown.exact_callsite = 1u;
        teardown.post_native_completion = 1u;
        teardown.verified_empty = 1u;
        (void)SudekiMpTalosCompanionCarryObserveTeardown(&carry, &teardown);
    }
    SudekiMpTalosCompanionCarryConfigure(&carry, 0);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    replay = provenance_value;
    replay.serial++;
    preflight_value = preflight(&replay);
    CHECK(SudekiMpTalosCompanionCarryBegin(
        &carry, &replay, &preflight_value) ==
        SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY);

    replay.serial = 1u;
    replay.transition_serial = 1u;
    replay.world_generation++;
    replay.source_generation++;
    preflight_value = preflight(&replay);
    CHECK(SudekiMpTalosCompanionCarryBegin(
        &carry, &replay, &preflight_value) == SUDEKIMP_TALOS_CARRY_STARTED);
    value = deletion(1u);
    CHECK(SudekiMpTalosCompanionCarryObserveDelete(
        &carry, &replay, &value) ==
        SUDEKIMP_TALOS_CARRY_DELETE_ABORT_TO_VANILLA);
    SudekiMpTalosCompanionCarryConfigure(&carry, 0);
    SudekiMpTalosCompanionCarryConfigure(&carry, 1);
    replay.serial = 2u;
    replay.transition_serial = 2u;
    preflight_value = preflight(&replay);
    CHECK(SudekiMpTalosCompanionCarryBegin(
        &carry, &replay, &preflight_value) == SUDEKIMP_TALOS_CARRY_STARTED);
}

static void test_formation_authorization_is_exact_and_one_shot(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value;
    SudekiMpTalosCarryFormationObservation observation;
    SudekiMpTalosCarryFormationTicket ticket;
    SudekiMpTalosCarryFormationCompletion completion;
    SudekiMpTalosCarrySetZoneTicket set_zone_ticket;

    preserve_all(&carry, &provenance_value);
    set_zone_ticket = pass_set_zone(&carry, &provenance_value);
    observation = formation_observation(&provenance_value);
    observation.set_zone_authorization_serial =
        set_zone_ticket.authorization_serial;
    observation.set_zone_raw--;
    CHECK(SudekiMpTalosCompanionCarryClaimFormation(
        &carry, &provenance_value, &observation, &ticket) ==
        SUDEKIMP_TALOS_CARRY_QUARANTINE);

    preserve_all(&carry, &provenance_value);
    set_zone_ticket = pass_set_zone(&carry, &provenance_value);
    observation = formation_observation(&provenance_value);
    observation.set_zone_authorization_serial =
        set_zone_ticket.authorization_serial;
    CHECK(SudekiMpTalosCompanionCarryClaimFormation(
        &carry, &provenance_value, &observation, &ticket) ==
        SUDEKIMP_TALOS_CARRY_FORMATION_AUTHORIZED);
    CHECK(ticket.native_function_rva ==
        SUDEKIMP_TALOS_CARRY_FORMATION_POP_RVA);
    CHECK(ticket.actor_identity[3] ==
        provenance_value.hero_actor_identity[3]);
    CHECK(SudekiMpTalosCompanionCarryClaimFormation(
        &carry, &provenance_value, &observation, &ticket) ==
        SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY);
    completion = formation_completion(&provenance_value, &ticket);
    CHECK(SudekiMpTalosCompanionCarryFinishFormation(
        &carry, &ticket, &completion) ==
        SUDEKIMP_TALOS_CARRY_FORMATION_COMMITTED);
    CHECK(SudekiMpTalosCompanionCarryFinishFormation(
        &carry, &ticket, &completion) == SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY);
    CHECK(carry.state == SUDEKIMP_TALOS_CARRY_ACTIVE);
}

static void test_ticket_or_native_failure_quarantines(void) {
    SudekiMpTalosCompanionCarry carry;
    SudekiMpTalosEncounterProvenance provenance_value;
    SudekiMpTalosCarryFormationObservation observation;
    SudekiMpTalosCarryFormationTicket ticket;
    SudekiMpTalosCarryFormationCompletion completion;
    SudekiMpTalosCarrySetZoneTicket set_zone_ticket;

    preserve_all(&carry, &provenance_value);
    set_zone_ticket = pass_set_zone(&carry, &provenance_value);
    observation = formation_observation(&provenance_value);
    observation.set_zone_authorization_serial =
        set_zone_ticket.authorization_serial;
    (void)SudekiMpTalosCompanionCarryClaimFormation(
        &carry, &provenance_value, &observation, &ticket);
    completion = formation_completion(&provenance_value, &ticket);
    ticket.actor_generation[0]++;
    CHECK(SudekiMpTalosCompanionCarryFinishFormation(
        &carry, &ticket, &completion) == SUDEKIMP_TALOS_CARRY_QUARANTINE);

    preserve_all(&carry, &provenance_value);
    set_zone_ticket = pass_set_zone(&carry, &provenance_value);
    observation = formation_observation(&provenance_value);
    observation.set_zone_authorization_serial =
        set_zone_ticket.authorization_serial;
    (void)SudekiMpTalosCompanionCarryClaimFormation(
        &carry, &provenance_value, &observation, &ticket);
    completion = formation_completion(&provenance_value, &ticket);
    completion.placement_verified = 0u;
    CHECK(SudekiMpTalosCompanionCarryFinishFormation(
        &carry, &ticket, &completion) == SUDEKIMP_TALOS_CARRY_QUARANTINE);
}

static void test_remove_all_players_is_count_generic(void) {
    unsigned int count;

    for (count = 0u; count <= SUDEKIMP_TALOS_HERO_COUNT; ++count) {
        SudekiMpTalosCompanionCarry carry;
        SudekiMpTalosEncounterProvenance provenance_value;
        SudekiMpTalosCarryTeardownObservation observation;

        activate(&carry, &provenance_value);
        memset(&observation, 0, sizeof(observation));
        observation.encounter_serial = provenance_value.serial;
        observation.binding_hash = SUDEKIMP_TALOS_CARRY_REMOVE_ALL_HASH;
        observation.member_count = count;
        observation.exact_callsite = 1u;
        CHECK(SudekiMpTalosCompanionCarryObserveTeardown(
            &carry, &observation) == SUDEKIMP_TALOS_CARRY_NO_CHANGE);
        CHECK(carry.state == SUDEKIMP_TALOS_CARRY_ACTIVE);
        observation.member_count = 0u;
        observation.post_native_completion = 1u;
        observation.verified_empty = 1u;
        CHECK(SudekiMpTalosCompanionCarryObserveTeardown(
            &carry, &observation) == SUDEKIMP_TALOS_CARRY_TEARDOWN_OBSERVED);
        CHECK(carry.state == SUDEKIMP_TALOS_CARRY_RELEASED);
        CHECK(SudekiMpTalosCompanionCarryObserveTeardown(
            &carry, &observation) == SUDEKIMP_TALOS_CARRY_NO_CHANGE);
        CHECK(carry.state == SUDEKIMP_TALOS_CARRY_RELEASED);
    }
}

int main(void) {
    test_default_inert_and_unrelated_delete_passes();
    test_preflight_and_first_delete_can_abort_vanilla();
    test_partial_skip_uncertainty_quarantines();
    test_full_provenance_and_host_tuple_are_bound();
    test_set_zone_requires_all_deletes_and_is_one_shot();
    test_reconfigure_and_upstream_replay_fence();
    test_formation_authorization_is_exact_and_one_shot();
    test_ticket_or_native_failure_quarantines();
    test_remove_all_players_is_count_generic();
    if (failures != 0) {
        fprintf(stderr, "talos companion carry checks failed: %d\n", failures);
        return 1;
    }
    puts("talos companion carry checks passed");
    return 0;
}
