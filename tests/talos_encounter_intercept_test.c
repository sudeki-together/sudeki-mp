#include "hooks/talos_encounter_intercept.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static SudekiMpTalosTransitionProvenance provenance(void) {
    SudekiMpTalosTransitionProvenance value;

    memset(&value, 0, sizeof(value));
    value.world_generation = 7u;
    value.source_generation = 11u;
    value.host_actor_generation = 13u;
    value.host_lease_generation = 17u;
    value.world_identity = UINT64_C(0x12345000);
    value.host_actor_identity = UINT64_C(0x23456000);
    return value;
}

static SudekiMpTalosEncounterSetZoneObservation observation(
    uint32_t callsite_rva
) {
    SudekiMpTalosEncounterSetZoneObservation value;

    memset(&value, 0, sizeof(value));
    value.provenance = provenance();
    value.callsite_rva = callsite_rva;
    value.exact_build_confirmed = 1u;
    value.host_hero = SUDEKIMP_TALOS_ENCOUNTER_HOST_HERO_TAL;
    value.active_human_mask = 0x03u;
    memcpy(value.destination, "Void", sizeof("Void"));
    return value;
}

static SudekiMpTalosSolTaskLineage source_lineage(void) {
    SudekiMpTalosSolTaskLineage value;

    memset(&value, 0, sizeof(value));
    value.sol_thread_identity = UINT64_C(0x11110000);
    value.task_identity = UINT64_C(0x22220000);
    value.root_task_identity = value.task_identity;
    value.sol_thread_generation = 3u;
    value.task_generation = 5u;
    value.root_task_generation = value.task_generation;
    value.script_runtime_generation = 7u;
    value.native_thread_id = 11u;
    return value;
}

static SudekiMpTalosLoadVoidObservation load_void_observation(void) {
    SudekiMpTalosLoadVoidObservation value;

    memset(&value, 0, sizeof(value));
    value.provenance = provenance();
    value.lineage = source_lineage();
    value.observed_at_ms = 1000u;
    value.source_action_hash = SUDEKIMP_TALOS_SOL_SOURCE_ACTION_HASH;
    value.source_action_start = SUDEKIMP_TALOS_SOL_SOURCE_ACTION_START;
    value.opcode_offset = SUDEKIMP_TALOS_SOL_LOAD_VOID_OPCODE_OFFSET;
    value.scene_task_hash = SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH;
    value.exact_build_confirmed = 1u;
    value.interaction_authority_proven = 1u;
    return value;
}

static SudekiMpTalosSetZoneCarrierObservation set_zone_carrier_observation(
    int descendant
) {
    SudekiMpTalosSetZoneCarrierObservation value;
    SudekiMpTalosSolTaskLineage source = source_lineage();

    memset(&value, 0, sizeof(value));
    value.provenance = provenance();
    value.lineage = source;
    if (descendant) {
        value.lineage.sol_thread_identity = UINT64_C(0x33330000);
        value.lineage.task_identity = UINT64_C(0x44440000);
        value.lineage.sol_thread_generation = 13u;
        value.lineage.task_generation = 17u;
        value.lineage.root_task_identity = source.task_identity;
        value.lineage.root_task_generation = source.task_generation;
    }
    value.observed_at_ms = 2000u;
    value.caller_function_hash = SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH;
    value.caller_opcode_offset =
        SUDEKIMP_TALOS_SOL_LOAD_VOID_SET_ZONE_OPCODE_OFFSET;
    value.function_hash = SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_HASH;
    value.function_start = SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_START;
    value.opcode_offset = SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_OPCODE_OFFSET;
    value.binding_hash = SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_HASH;
    value.exact_build_confirmed = 1u;
    memcpy(value.destination, "Void", sizeof("Void"));
    return value;
}

static void test_corrected_gex_bytecode_mapping(void) {
    check(SUDEKIMP_TALOS_SOL_BYTECODE_FILE_BASE +
            SUDEKIMP_TALOS_SOL_LOAD_VOID_OPCODE_OFFSET == 0x00049a78u &&
            SUDEKIMP_TALOS_SOL_BYTECODE_FILE_BASE +
            SUDEKIMP_TALOS_SOL_LOAD_VOID_SET_ZONE_OPCODE_OFFSET ==
                0x000497dau &&
            SUDEKIMP_TALOS_SOL_BYTECODE_FILE_BASE +
            SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_OPCODE_OFFSET ==
                0x0002afe8u,
        "logical SOL offsets map to their independently verified raw bytes");
    check(SUDEKIMP_TALOS_SOL_SOURCE_ACTION_HASH == UINT32_C(0xfac73f18) &&
            SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH == UINT32_C(0x70f470c2) &&
            SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_HASH ==
                UINT32_C(0x76fc7114) &&
            SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_HASH ==
                UINT32_C(0xbc8fdc32),
        "corrected source, scene, wrapper, and native hashes stay exact");
}

static void test_lineage_tracker_is_passive_and_default_off(void) {
    SudekiMpTalosLineageTracker tracker;
    SudekiMpTalosLoadVoidObservation load = load_void_observation();
    SudekiMpTalosSetZoneCarrierObservation carrier =
        set_zone_carrier_observation(0);
    SudekiMpTalosLineageSnapshot snapshot;

    SudekiMpTalosLineageInitialize(&tracker);
    check(SudekiMpTalosLineageObserveLoadVoid(&tracker, &load) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED &&
            SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED,
        "lineage observation is disabled and inert by default");
    check(SudekiMpTalosLineageGetSnapshot(&tracker, &snapshot) &&
            !snapshot.enabled && !snapshot.exact_carrier_matched &&
            !snapshot.production_continuation_supported,
        "disabled lineage snapshot cannot advertise continuation");
}

static void test_lineage_matches_same_and_descendant_tasks(void) {
    SudekiMpTalosLineageTracker tracker;
    SudekiMpTalosLoadVoidObservation load = load_void_observation();
    SudekiMpTalosSetZoneCarrierObservation carrier =
        set_zone_carrier_observation(0);
    SudekiMpTalosLineageSnapshot snapshot;
    uint32_t first_serial;

    SudekiMpTalosLineageInitialize(&tracker);
    SudekiMpTalosLineageConfigure(&tracker, 1);
    check(SudekiMpTalosLineageObserveLoadVoid(&tracker, &load) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_LOAD_VOID_RECORDED,
        "exact LoadTheVoid pre-action is recorded passively");
    first_serial = tracker.serial;
    check(SudekiMpTalosLineageObserveLoadVoid(&tracker, &load) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_NO_CHANGE,
        "duplicate same-generation pre-action is idempotent");
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_CARRIER_MATCHED &&
            SudekiMpTalosLineageGetSnapshot(&tracker, &snapshot) &&
            snapshot.exact_carrier_matched &&
            !snapshot.production_continuation_supported &&
            snapshot.serial == first_serial,
        "same SOL thread and task produces evidence but no continuation");

    SudekiMpTalosLineageReset(&tracker);
    carrier = set_zone_carrier_observation(1);
    check(SudekiMpTalosLineageObserveLoadVoid(&tracker, &load) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_LOAD_VOID_RECORDED &&
            SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_CARRIER_MATCHED,
        "explicit root-task ancestry admits a descendant SetZone task");
    check(tracker.serial != first_serial,
        "a reset lineage observation receives a fresh monotonic serial");
}

static void test_lineage_rejects_mismatch_reuse_and_stale_generations(void) {
    SudekiMpTalosLineageTracker tracker;
    SudekiMpTalosLoadVoidObservation load = load_void_observation();
    SudekiMpTalosSetZoneCarrierObservation carrier =
        set_zone_carrier_observation(1);

    SudekiMpTalosLineageInitialize(&tracker);
    SudekiMpTalosLineageConfigure(&tracker, 1);
    (void)SudekiMpTalosLineageObserveLoadVoid(&tracker, &load);
    ++carrier.lineage.root_task_identity;
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_LINEAGE &&
            tracker.state == SUDEKIMP_TALOS_LINEAGE_QUARANTINED,
        "unrelated root task quarantines an exact-looking carrier");

    SudekiMpTalosLineageReset(&tracker);
    (void)SudekiMpTalosLineageObserveLoadVoid(&tracker, &load);
    carrier = set_zone_carrier_observation(0);
    ++carrier.lineage.sol_thread_generation;
    ++carrier.lineage.task_generation;
    carrier.lineage.root_task_generation = carrier.lineage.task_generation;
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_LINEAGE,
        "reused thread and task addresses with new generations are rejected");

    SudekiMpTalosLineageReset(&tracker);
    (void)SudekiMpTalosLineageObserveLoadVoid(&tracker, &load);
    carrier = set_zone_carrier_observation(0);
    ++carrier.provenance.source_generation;
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STALE,
        "changed world/source/host provenance is stale");

    SudekiMpTalosLineageReset(&tracker);
    (void)SudekiMpTalosLineageObserveLoadVoid(&tracker, &load);
    carrier = set_zone_carrier_observation(0);
    carrier.observed_at_ms = load.observed_at_ms +
        SUDEKIMP_TALOS_LINEAGE_MAX_AGE_MS + 1u;
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STALE,
        "expired pre-action cannot authorize a later generic carrier");
}

static void test_lineage_exact_static_identity_and_busy_reuse(void) {
    SudekiMpTalosLineageTracker tracker;
    SudekiMpTalosLoadVoidObservation load = load_void_observation();
    SudekiMpTalosLoadVoidObservation reused;
    SudekiMpTalosSetZoneCarrierObservation carrier =
        set_zone_carrier_observation(0);

    SudekiMpTalosLineageInitialize(&tracker);
    SudekiMpTalosLineageConfigure(&tracker, 1);
    --load.opcode_offset;
    check(SudekiMpTalosLineageObserveLoadVoid(&tracker, &load) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_NOT_EXACT &&
            tracker.state == SUDEKIMP_TALOS_LINEAGE_IDLE,
        "nearby opcode cannot arm the LoadTheVoid lineage");
    load = load_void_observation();
    (void)SudekiMpTalosLineageObserveLoadVoid(&tracker, &load);
    --carrier.opcode_offset;
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_NOT_EXACT &&
            tracker.state == SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED,
        "nearby SetZone call does not consume the exact pre-action");

    carrier = set_zone_carrier_observation(0);
    --carrier.caller_opcode_offset;
    check(SudekiMpTalosLineageObserveSetZoneCarrier(&tracker, &carrier) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_NOT_EXACT &&
            tracker.state == SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED,
        "native SetZone carrier without the exact LoadTheVoid caller is rejected");

    reused = load;
    ++reused.lineage.sol_thread_generation;
    ++reused.lineage.task_generation;
    reused.lineage.root_task_generation = reused.lineage.task_generation;
    check(SudekiMpTalosLineageObserveLoadVoid(&tracker, &reused) ==
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STATE &&
            tracker.state == SUDEKIMP_TALOS_LINEAGE_QUARANTINED,
        "a second generation cannot replace a pending exact pre-action");
}

static void test_default_and_production_path_are_inert(void) {
    SudekiMpTalosEncounterIntercept intercept;
    SudekiMpTalosEncounterSetZoneObservation candidate = observation(0u);
    SudekiMpTalosEncounterPromptSnapshot snapshot;

    SudekiMpTalosEncounterInterceptInitialize(&intercept);
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "default-disabled gate always passes native SetZoneNOW");

    SudekiMpTalosEncounterInterceptConfigure(&intercept, 1);
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE_OBSERVED,
        "unproven Void call is observed without interception");
    check(SudekiMpTalosEncounterInterceptGetPromptSnapshot(
            &intercept, &snapshot) &&
            snapshot.state == SUDEKIMP_TALOS_INTERCEPT_OBSERVED_ONLY &&
            snapshot.serial != 0u && !snapshot.continuation_supported &&
            !snapshot.prompt_active && !snapshot.native_deferred,
        "unproven call exposes diagnostics but no prompt or continuation");
    check(snapshot.party_size == 4u && snapshot.talos_hp == 180000u &&
            snapshot.active_human_mask == 0x03u &&
            snapshot.human_count == 2u &&
            strcmp(snapshot.destination, "Void") == 0,
        "pointer-free snapshot reports expanded encounter intent");
    check(SudekiMpTalosEncounterInterceptHostConfirm(
            &intercept, snapshot.serial, &candidate.provenance) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STATE &&
            SudekiMpTalosEncounterInterceptHostCancel(
                &intercept, snapshot.serial) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STATE &&
            SudekiMpTalosEncounterInterceptService(
                &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_NONE,
        "host commands cannot turn observation-only evidence into a load");
}

static void test_exact_candidate_requirements(void) {
    SudekiMpTalosEncounterIntercept intercept;
    SudekiMpTalosEncounterSetZoneObservation candidate = observation(0u);
    SudekiMpTalosEncounterPromptSnapshot snapshot;

    SudekiMpTalosEncounterInterceptInitialize(&intercept);
    SudekiMpTalosEncounterInterceptConfigure(&intercept, 1);

    memcpy(candidate.destination, "void", sizeof("void"));
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "destination matching is exact and case sensitive");
    candidate = observation(0u);
    candidate.destination[4] = 'X';
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "unterminated or extended Void destination is rejected");
    candidate = observation(0u);
    candidate.exact_build_confirmed = 0u;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "unknown executable cannot arm observation");
    candidate = observation(0u);
    candidate.host_hero = 1u;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "non-Tal host cannot arm observation");
    candidate = observation(0u);
    candidate.active_human_mask = 0x02u;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "candidate without host seat cannot arm observation");
    candidate = observation(0u);
    candidate.active_human_mask = 0x11u;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "unknown human-mask bits cannot be normalized into authority");
    candidate = observation(0u);
    candidate.provenance.source_generation = 0u;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "incomplete provenance cannot arm observation");
    check(SudekiMpTalosEncounterInterceptGetPromptSnapshot(
            &intercept, &snapshot) && snapshot.serial == 0u &&
            snapshot.state == SUDEKIMP_TALOS_INTERCEPT_IDLE,
        "rejected observations leave no retained candidate");
}

#ifdef SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_TESTING
static uint32_t open_test_prompt(
    SudekiMpTalosEncounterIntercept *intercept,
    SudekiMpTalosEncounterSetZoneObservation *candidate
) {
    SudekiMpTalosEncounterPromptSnapshot snapshot;

    SudekiMpTalosEncounterInterceptInitialize(intercept);
    SudekiMpTalosEncounterInterceptConfigure(intercept, 1);
    *candidate = observation(
        SUDEKIMP_TALOS_ENCOUNTER_TEST_PROVEN_CALLSITE_RVA);
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            intercept, candidate) == SUDEKIMP_TALOS_OBSERVE_DEFER_NATIVE,
        "test-only proven seam defers native call");
    check(SudekiMpTalosEncounterInterceptGetPromptSnapshot(
            intercept, &snapshot) && snapshot.prompt_active &&
            !snapshot.prompt_visible && snapshot.native_deferred &&
            snapshot.continuation_supported,
        "deferred request exposes hidden pointer-free prompt");
    return snapshot.serial;
}

static void test_confirm_revalidates_and_continues_once(void) {
    SudekiMpTalosEncounterIntercept intercept;
    SudekiMpTalosEncounterSetZoneObservation candidate;
    SudekiMpTalosEncounterSetZoneObservation unrelated;
    uint32_t serial = open_test_prompt(&intercept, &candidate);

    check(SudekiMpTalosEncounterInterceptHostConfirm(
            &intercept, serial, &candidate.provenance) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STATE,
        "host cannot confirm before prompt is visibly reported");
    check(SudekiMpTalosEncounterInterceptReportPrompt(
            &intercept, serial + 1u, 1) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STALE,
        "old prompt serial cannot become visible");
    check(SudekiMpTalosEncounterInterceptReportPrompt(
            &intercept, serial, 1) == SUDEKIMP_TALOS_COMMAND_ACCEPTED &&
            SudekiMpTalosEncounterInterceptHostConfirm(
                &intercept, serial, &candidate.provenance) ==
            SUDEKIMP_TALOS_COMMAND_ACCEPTED,
        "visible prompt and matching provenance accept host confirmation");
    check(SudekiMpTalosEncounterInterceptService(
            &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_CONTINUE_NATIVE_ONCE,
        "ready request claims exactly one native continuation");
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_DROP_NATIVE_BUSY,
        "retained call remains blocked while its continuation is in flight");
    unrelated = candidate;
    memcpy(unrelated.destination, "Brightwater", sizeof("Brightwater"));
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "unrelated calls pass while the Talos continuation is in flight");
    unrelated = candidate;
    ++unrelated.provenance.source_generation;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "a different transition lineage passes during continuation replay");
    check(SudekiMpTalosEncounterInterceptService(
            &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_NONE,
        "claimed continuation cannot replay");
    check(SudekiMpTalosEncounterInterceptFinishContinuation(
            &intercept, serial, 1) == SUDEKIMP_TALOS_COMMAND_ACCEPTED &&
            SudekiMpTalosEncounterInterceptService(
                &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_NONE,
        "completed continuation remains terminal and one-shot");
}

static void test_cancel_and_failed_prompt_discard_once(void) {
    SudekiMpTalosEncounterIntercept intercept;
    SudekiMpTalosEncounterSetZoneObservation candidate;
    SudekiMpTalosEncounterSetZoneObservation unrelated;
    uint32_t serial = open_test_prompt(&intercept, &candidate);

    check(SudekiMpTalosEncounterInterceptHostCancel(&intercept, serial) ==
            SUDEKIMP_TALOS_COMMAND_ACCEPTED,
        "Escape cancels before native load");
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_DROP_NATIVE_BUSY,
        "cancelled retained call stays blocked until discard is serviced");
    unrelated = candidate;
    memcpy(unrelated.destination, "Brightwater", sizeof("Brightwater"));
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "unrelated transition passes during the cancel/discard window");
    check(SudekiMpTalosEncounterInterceptReset(&intercept) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STATE,
        "reset cannot erase a cancelled call before service discards it");
    check(SudekiMpTalosEncounterInterceptService(
            &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED &&
            SudekiMpTalosEncounterInterceptService(
            &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_NONE,
        "cancel reports one discard and never continues");
    check(SudekiMpTalosEncounterInterceptReset(&intercept) ==
            SUDEKIMP_TALOS_COMMAND_ACCEPTED,
        "reset is allowed after the retained cancellation is discarded");

    serial = open_test_prompt(&intercept, &candidate);
    check(SudekiMpTalosEncounterInterceptReportPrompt(
            &intercept, serial, 0) == SUDEKIMP_TALOS_COMMAND_ACCEPTED &&
            SudekiMpTalosEncounterInterceptService(
                &intercept, &candidate.provenance) ==
            SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED,
        "failed prompt draw discards deferred call fail-closed");
}

static void test_stale_provenance_quarantines_before_load(void) {
    SudekiMpTalosEncounterIntercept intercept;
    SudekiMpTalosEncounterSetZoneObservation candidate;
    SudekiMpTalosEncounterSetZoneObservation unrelated;
    SudekiMpTalosTransitionProvenance stale;
    uint32_t serial = open_test_prompt(&intercept, &candidate);

    (void)SudekiMpTalosEncounterInterceptReportPrompt(
        &intercept, serial, 1);
    stale = candidate.provenance;
    ++stale.source_generation;
    check(SudekiMpTalosEncounterInterceptHostConfirm(
            &intercept, serial, &stale) ==
            SUDEKIMP_TALOS_COMMAND_QUARANTINED,
        "source change at confirm quarantines before load");
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_DROP_NATIVE_BUSY,
        "quarantined retained call stays blocked pending explicit discard");
    unrelated = candidate;
    ++unrelated.provenance.source_generation;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "unrelated lineage passes during the quarantine/discard window");
    check(SudekiMpTalosEncounterInterceptReset(&intercept) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STATE,
        "reset cannot erase a quarantined retained call before discard");
    check(SudekiMpTalosEncounterInterceptService(&intercept, &stale) ==
            SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED &&
            SudekiMpTalosEncounterInterceptReset(&intercept) ==
            SUDEKIMP_TALOS_COMMAND_ACCEPTED,
        "service discards the quarantined call before reset is allowed");

    serial = open_test_prompt(&intercept, &candidate);
    (void)SudekiMpTalosEncounterInterceptReportPrompt(
        &intercept, serial, 1);
    check(SudekiMpTalosEncounterInterceptHostConfirm(
            &intercept, serial, &candidate.provenance) ==
            SUDEKIMP_TALOS_COMMAND_ACCEPTED,
        "matching provenance reaches ready state");
    stale = candidate.provenance;
    ++stale.host_lease_generation;
    check(SudekiMpTalosEncounterInterceptService(&intercept, &stale) ==
            SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED,
        "host lease change after confirmation wins over continuation");
}

static void test_pending_request_blocks_reentrant_load(void) {
    SudekiMpTalosEncounterIntercept intercept;
    SudekiMpTalosEncounterSetZoneObservation candidate;
    SudekiMpTalosEncounterSetZoneObservation unrelated;

    (void)open_test_prompt(&intercept, &candidate);
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &candidate) ==
            SUDEKIMP_TALOS_OBSERVE_DROP_NATIVE_BUSY,
        "exact duplicate SetZoneNOW cannot bypass a deferred request");

    unrelated = candidate;
    memcpy(unrelated.destination, "Brightwater", sizeof("Brightwater"));
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "an unrelated destination is never swallowed by a Talos defer");
    unrelated = candidate;
    ++unrelated.provenance.source_generation;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "a different transition provenance is not treated as a duplicate");
    unrelated = candidate;
    ++unrelated.callsite_rva;
    check(SudekiMpTalosEncounterInterceptObserveSetZoneNow(
            &intercept, &unrelated) == SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE,
        "a different callsite is not treated as the retained Talos call");
    check(SudekiMpTalosEncounterInterceptReset(&intercept) ==
            SUDEKIMP_TALOS_COMMAND_REJECTED_STATE,
        "pending native call cannot be erased by reset");
}
#endif

int main(void) {
    test_corrected_gex_bytecode_mapping();
    test_lineage_tracker_is_passive_and_default_off();
    test_lineage_matches_same_and_descendant_tasks();
    test_lineage_rejects_mismatch_reuse_and_stale_generations();
    test_lineage_exact_static_identity_and_busy_reuse();
    test_default_and_production_path_are_inert();
    test_exact_candidate_requirements();
#ifdef SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_TESTING
    test_confirm_revalidates_and_continues_once();
    test_cancel_and_failed_prompt_discard_once();
    test_stale_provenance_quarantines_before_load();
    test_pending_request_blocks_reentrant_load();
#endif

    if (failures != 0) {
        fprintf(stderr, "%d Talos encounter intercept test(s) failed\n",
            failures);
        return 1;
    }
    puts("Talos encounter intercept tests passed");
    return 0;
}
