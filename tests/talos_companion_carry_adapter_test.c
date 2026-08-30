#include "hooks/talos_companion_carry_adapter.h"

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

static SudekiMpTalosTransitionProvenance transition_provenance(
    const SudekiMpTalosEncounterProvenance *source
) {
    SudekiMpTalosTransitionProvenance value;

    memset(&value, 0, sizeof(value));
    value.world_generation = source->world_generation;
    value.source_generation = source->source_generation;
    value.host_actor_generation = source->host_actor_generation;
    value.host_lease_generation = source->host_lease_generation;
    value.world_identity = UINT64_C(0x9000);
    value.host_actor_identity = (uint64_t)source->host_actor;
    return value;
}

static SudekiMpTalosCarryLineage carry_lineage(void) {
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
    const SudekiMpTalosEncounterProvenance *source,
    uint8_t member_mask
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
    value.group_member_mask = member_mask;
    value.formation_member_mask = member_mask;
    value.observed = 1u;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        value.actor_identity[i] = source->hero_actor_identity[i];
        value.actor_generation[i] = source->hero_actor_generation[i];
    }
    return value;
}

static SudekiMpTalosCarryPreflight preflight(
    const SudekiMpTalosEncounterProvenance *source
) {
    SudekiMpTalosCarryPreflight value;

    memset(&value, 0, sizeof(value));
    value.lineage = carry_lineage();
    value.formation = formation(source, 0x0fu);
    value.exact_executable = 1u;
    value.exact_asset = 1u;
    value.host_authority = 1u;
    return value;
}

static SudekiMpTalosLineageSnapshot lineage_snapshot(
    const SudekiMpTalosEncounterProvenance *source,
    int carrier_matched
) {
    SudekiMpTalosLineageSnapshot value;
    SudekiMpTalosSolTaskLineage *load_lineage;
    SudekiMpTalosSolTaskLineage *carrier_lineage;

    memset(&value, 0, sizeof(value));
    value.serial = 31u;
    value.enabled = 1u;
    value.state = carrier_matched ?
        SUDEKIMP_TALOS_LINEAGE_CARRIER_MATCHED :
        SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED;
    value.exact_carrier_matched = carrier_matched ? 1u : 0u;
    value.load_void.provenance = transition_provenance(source);
    value.load_void.observed_at_ms = 1000u;
    value.load_void.source_action_hash =
        SUDEKIMP_TALOS_SOL_SOURCE_ACTION_HASH;
    value.load_void.source_action_start =
        SUDEKIMP_TALOS_SOL_SOURCE_ACTION_START;
    value.load_void.opcode_offset =
        SUDEKIMP_TALOS_SOL_LOAD_VOID_OPCODE_OFFSET;
    value.load_void.scene_task_hash = SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH;
    value.load_void.exact_build_confirmed = 1u;
    value.load_void.interaction_authority_proven = 1u;
    load_lineage = &value.load_void.lineage;
    load_lineage->sol_thread_identity = UINT64_C(0x4000);
    load_lineage->task_identity = UINT64_C(0x5000);
    load_lineage->root_task_identity = UINT64_C(0x5000);
    load_lineage->sol_thread_generation = 10u;
    load_lineage->task_generation = 11u;
    load_lineage->root_task_generation = 11u;
    load_lineage->script_runtime_generation = 13u;
    load_lineage->native_thread_id = 14u;
    if (!carrier_matched) return value;
    value.carrier.provenance = transition_provenance(source);
    value.carrier.observed_at_ms = 2000u;
    value.carrier.caller_function_hash = SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH;
    value.carrier.caller_opcode_offset =
        SUDEKIMP_TALOS_SOL_LOAD_VOID_SET_ZONE_OPCODE_OFFSET;
    value.carrier.function_hash = SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_HASH;
    value.carrier.function_start = SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_START;
    value.carrier.opcode_offset =
        SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_OPCODE_OFFSET;
    value.carrier.binding_hash = SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_HASH;
    value.carrier.exact_build_confirmed = 1u;
    memcpy(value.carrier.destination, "Void", sizeof("Void"));
    carrier_lineage = &value.carrier.lineage;
    carrier_lineage->sol_thread_identity = UINT64_C(0x4100);
    carrier_lineage->task_identity = UINT64_C(0x6000);
    carrier_lineage->root_task_identity = UINT64_C(0x5000);
    carrier_lineage->sol_thread_generation = 15u;
    carrier_lineage->task_generation = 12u;
    carrier_lineage->root_task_generation = 11u;
    carrier_lineage->script_runtime_generation = 13u;
    carrier_lineage->native_thread_id = 14u;
    return value;
}

static SudekiMpTalosCarryAdapterDeleteCall deletion(unsigned int index) {
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
    SudekiMpTalosCarryAdapterDeleteCall value;

    memset(&value, 0, sizeof(value));
    value.evidence.lineage = carry_lineage();
    value.evidence.logical_opcode = logical[index];
    value.evidence.raw_opcode = raw[index];
    value.evidence.binding_hash = SUDEKIMP_TALOS_CARRY_DELETE_PC_HASH;
    value.evidence.resource_id = resource[index];
    value.evidence.exact_executable = 1u;
    value.evidence.exact_asset = 1u;
    value.original_call_entered = 1u;
    value.original_call_completed = 1u;
    return value;
}

static SudekiMpTalosCarryAdapterSetZoneCall set_zone(
    const SudekiMpTalosEncounterProvenance *source
) {
    SudekiMpTalosCarryAdapterSetZoneCall value;

    memset(&value, 0, sizeof(value));
    value.evidence.lineage = carry_lineage();
    value.evidence.encounter_serial = source->serial;
    value.evidence.transition_serial = source->transition_serial;
    value.evidence.logical_opcode = SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL;
    value.evidence.raw_opcode = SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW;
    value.evidence.binding_hash = SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH;
    value.evidence.exact_executable = 1u;
    value.evidence.exact_asset = 1u;
    memcpy(value.evidence.destination, "Void", sizeof("Void"));
    value.original_call_entered = 1u;
    value.original_call_completed = 1u;
    return value;
}

static SudekiMpTalosCarryFormationObservation release_point(
    const SudekiMpTalosEncounterProvenance *source,
    uint8_t member_mask
) {
    SudekiMpTalosCarryFormationObservation value;

    memset(&value, 0, sizeof(value));
    value.formation = formation(source, member_mask);
    value.encounter_serial = source->serial;
    value.transition_serial = source->transition_serial;
    value.arrival_world_generation = source->world_generation + 1u;
    value.arrival_source_generation = source->source_generation + 1u;
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

static SudekiMpTalosCarryAdapterEndTsaCall end_tsa(
    const SudekiMpTalosEncounterProvenance *source
) {
    SudekiMpTalosCarryAdapterEndTsaCall value;

    memset(&value, 0, sizeof(value));
    value.lineage = carry_lineage();
    value.encounter_serial = source->serial;
    value.transition_serial = source->transition_serial;
    value.logical_opcode = SUDEKIMP_TALOS_CARRY_END_TSA_LOGICAL;
    value.raw_opcode = SUDEKIMP_TALOS_CARRY_END_TSA_RAW;
    value.binding_hash = SUDEKIMP_TALOS_CARRY_END_TSA_HASH;
    value.exact_executable = 1u;
    value.exact_asset = 1u;
    value.original_call_entered = 1u;
    value.original_call_completed = 1u;
    value.tsa_inactive_after = 1u;
    return value;
}

static void begin_and_delete(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosEncounterProvenance *source
) {
    SudekiMpTalosCarryPreflight before;
    SudekiMpTalosLineageSnapshot load;
    unsigned int i;

    *source = provenance();
    before = preflight(source);
    load = lineage_snapshot(source, 0);
    SudekiMpTalosCompanionCarryAdapterInitialize(adapter);
    SudekiMpTalosCompanionCarryAdapterConfigure(adapter, 1);
    CHECK(SudekiMpTalosCompanionCarryAdapterBegin(
        adapter, source, &before, &load) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
    for (i = 0u; i < 3u; ++i) {
        SudekiMpTalosCarryAdapterDeleteCall call = deletion(i);
        CHECK(SudekiMpTalosCompanionCarryAdapterObserveDeletePc(
            adapter, source, &call) ==
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
    }
}

static void through_end_tsa(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosEncounterProvenance *source,
    uint8_t release_mask
) {
    SudekiMpTalosCarryAdapterSetZoneCall zone;
    SudekiMpTalosLineageSnapshot carrier;
    SudekiMpTalosCarryFormationObservation release;
    SudekiMpTalosCarryAdapterEndTsaCall end;

    begin_and_delete(adapter, source);
    zone = set_zone(source);
    carrier = lineage_snapshot(source, 1);
    CHECK(SudekiMpTalosCompanionCarryAdapterObserveSetZone(
        adapter, source, &zone, &carrier) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
    release = release_point(source, release_mask);
    CHECK(SudekiMpTalosCompanionCarryAdapterObserveReleasePoint(
        adapter, source, &release) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
    end = end_tsa(source);
    CHECK(SudekiMpTalosCompanionCarryAdapterObserveEndTsa(
        adapter, source, &end) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
}

static void test_default_off_and_pointer_free_snapshot(void) {
    SudekiMpTalosCompanionCarryAdapter adapter;
    SudekiMpTalosCarryAdapterSnapshot snapshot;
    SudekiMpTalosCarryAdapterChange change;
    SudekiMpTalosEncounterProvenance source = provenance();
    SudekiMpTalosCarryPreflight before = preflight(&source);
    SudekiMpTalosLineageSnapshot load = lineage_snapshot(&source, 0);

    SudekiMpTalosCompanionCarryAdapterInitialize(&adapter);
    CHECK(SudekiMpTalosCompanionCarryAdapterBegin(
        &adapter, &source, &before, &load) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED);
    CHECK(SudekiMpTalosCompanionCarryAdapterGetSnapshot(
        &adapter, &snapshot));
    CHECK(!snapshot.enabled && snapshot.observation_only &&
        snapshot.native_passthrough_required && !snapshot.mutation_supported);
    CHECK(snapshot.encounter_serial == 0u && snapshot.lineage_serial == 0u);
    CHECK(!SudekiMpTalosCompanionCarryAdapterService(&adapter, &change));
}

static void test_exact_vanilla_observation_and_change_only_service(void) {
    SudekiMpTalosCompanionCarryAdapter adapter;
    SudekiMpTalosEncounterProvenance source;
    SudekiMpTalosCarryAdapterSnapshot snapshot;
    SudekiMpTalosCarryAdapterChange change;
    SudekiMpTalosCarryDeleteObservation unrelated;
    unsigned int changes = 0u;

    through_end_tsa(&adapter, &source, 0x01u);
    CHECK(SudekiMpTalosCompanionCarryAdapterGetSnapshot(&adapter, &snapshot));
    CHECK(snapshot.state == SUDEKIMP_TALOS_CARRY_ADAPTER_END_TSA_OBSERVED);
    CHECK(snapshot.delete_native_completed_mask == 0x07u &&
        snapshot.exact_set_zone && snapshot.exact_release_point &&
        snapshot.end_tsa_native_completed && snapshot.tsa_inactive_after);
    CHECK(snapshot.release_group_member_mask == 0x01u &&
        !snapshot.release_carry_ready);
    while (SudekiMpTalosCompanionCarryAdapterService(&adapter, &change)) {
        CHECK(change.event != SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_NONE);
        CHECK(change.snapshot.observation_only &&
            change.snapshot.native_passthrough_required &&
            !change.snapshot.mutation_supported);
        ++changes;
    }
    CHECK(changes == 8u); /* enabled + load + 3 deletes + zone + release + TSA */
    CHECK(!SudekiMpTalosCompanionCarryAdapterService(&adapter, &change));

    memset(&unrelated, 0, sizeof(unrelated));
    unrelated.logical_opcode = 0x1234u;
    unrelated.raw_opcode = 0x5678u;
    {
        SudekiMpTalosCarryAdapterDeleteCall call;
        memset(&call, 0, sizeof(call));
        call.evidence = unrelated;
        CHECK(SudekiMpTalosCompanionCarryAdapterObserveDeletePc(
            &adapter, &source, &call) ==
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_NO_CHANGE);
    }
    CHECK(!SudekiMpTalosCompanionCarryAdapterService(&adapter, &change));
}

static void test_full_release_and_teardown_are_evidence_not_authority(void) {
    SudekiMpTalosCompanionCarryAdapter adapter;
    SudekiMpTalosEncounterProvenance source;
    SudekiMpTalosCarryTeardownObservation teardown;
    SudekiMpTalosCarryAdapterSnapshot snapshot;

    through_end_tsa(&adapter, &source, 0x0fu);
    memset(&teardown, 0, sizeof(teardown));
    teardown.encounter_serial = source.serial;
    teardown.binding_hash = SUDEKIMP_TALOS_CARRY_REMOVE_ALL_HASH;
    teardown.exact_callsite = 1u;
    teardown.post_native_completion = 1u;
    teardown.verified_empty = 1u;
    CHECK(SudekiMpTalosCompanionCarryAdapterObserveTeardown(
        &adapter, &teardown) == SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
    CHECK(SudekiMpTalosCompanionCarryAdapterGetSnapshot(&adapter, &snapshot));
    CHECK(snapshot.release_carry_ready &&
        snapshot.release_actor_match_mask == 0x0fu &&
        snapshot.teardown_native_completed &&
        snapshot.teardown_verified_empty && !snapshot.mutation_supported);
    CHECK(SudekiMpTalosCompanionCarryAdapterReset(&adapter) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED);
}

static void test_incomplete_native_call_invalidates_once(void) {
    SudekiMpTalosCompanionCarryAdapter adapter;
    SudekiMpTalosEncounterProvenance source = provenance();
    SudekiMpTalosCarryPreflight before = preflight(&source);
    SudekiMpTalosLineageSnapshot load = lineage_snapshot(&source, 0);
    SudekiMpTalosCarryAdapterDeleteCall call = deletion(0u);
    SudekiMpTalosCarryAdapterChange change;
    unsigned int invalidated = 0u;

    SudekiMpTalosCompanionCarryAdapterInitialize(&adapter);
    SudekiMpTalosCompanionCarryAdapterConfigure(&adapter, 1);
    (void)SudekiMpTalosCompanionCarryAdapterBegin(
        &adapter, &source, &before, &load);
    call.original_call_completed = 0u;
    CHECK(SudekiMpTalosCompanionCarryAdapterObserveDeletePc(
        &adapter, &source, &call) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    CHECK(SudekiMpTalosCompanionCarryAdapterObserveDeletePc(
        &adapter, &source, &call) ==
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    while (SudekiMpTalosCompanionCarryAdapterService(&adapter, &change)) {
        if (change.event == SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_INVALIDATED)
            ++invalidated;
    }
    CHECK(invalidated == 1u);
}

int main(void) {
    test_default_off_and_pointer_free_snapshot();
    test_exact_vanilla_observation_and_change_only_service();
    test_full_release_and_teardown_are_evidence_not_authority();
    test_incomplete_native_call_invalidates_once();
    CHECK(strcmp(SudekiMpTalosCarryAdapterEventName(
        SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_SET_ZONE), "set_zone") == 0);
    CHECK(strcmp(SudekiMpTalosCarryAdapterStateName(
        SUDEKIMP_TALOS_CARRY_ADAPTER_END_TSA_OBSERVED),
        "end_tsa_observed") == 0);
    if (failures != 0) {
        fprintf(stderr, "talos companion carry adapter checks failed: %d\n",
            failures);
        return 1;
    }
    puts("talos companion carry adapter checks passed");
    return 0;
}
