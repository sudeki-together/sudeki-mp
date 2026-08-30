#include "hooks/talos_encounter_intercept.h"

#include <stddef.h>
#include <string.h>

static int provenance_valid(
    const SudekiMpTalosTransitionProvenance *provenance
) {
    return provenance != NULL && provenance->world_generation != 0u &&
        provenance->source_generation != 0u &&
        provenance->host_actor_generation != 0u &&
        provenance->host_lease_generation != 0u &&
        provenance->world_identity != 0u &&
        provenance->host_actor_identity != 0u;
}

static int provenance_matches(
    const SudekiMpTalosTransitionProvenance *left,
    const SudekiMpTalosTransitionProvenance *right
) {
    return provenance_valid(left) && provenance_valid(right) &&
        left->world_generation == right->world_generation &&
        left->source_generation == right->source_generation &&
        left->host_actor_generation == right->host_actor_generation &&
        left->host_lease_generation == right->host_lease_generation &&
        left->world_identity == right->world_identity &&
        left->host_actor_identity == right->host_actor_identity;
}

static int destination_is_exact_void(
    const char destination[SUDEKIMP_TALOS_ENCOUNTER_DESTINATION_CAPACITY]
) {
    static const char exact_void[] = "Void";

    return destination != NULL &&
        memcmp(destination, exact_void, sizeof(exact_void)) == 0;
}

static int observation_matches_retained_call(
    const SudekiMpTalosEncounterIntercept *intercept,
    const SudekiMpTalosEncounterSetZoneObservation *observation
) {
    return intercept != NULL && observation != NULL &&
        observation->exact_build_confirmed &&
        observation->host_hero == SUDEKIMP_TALOS_ENCOUNTER_HOST_HERO_TAL &&
        observation->active_human_mask == intercept->active_human_mask &&
        observation->callsite_rva == intercept->callsite_rva &&
        provenance_matches(&observation->provenance, &intercept->provenance) &&
        memcmp(observation->destination, intercept->destination,
            sizeof(intercept->destination)) == 0;
}

static unsigned int count_bits(uint8_t mask) {
    unsigned int count = 0u;

    mask &= SUDEKIMP_TALOS_ENCOUNTER_HUMAN_MASK;
    while (mask != 0u) {
        count += mask & 1u;
        mask >>= 1u;
    }
    return count;
}

static int lineage_identity_valid(
    const SudekiMpTalosSolTaskLineage *lineage,
    int source_action
) {
    if (lineage == NULL || lineage->sol_thread_identity == 0u ||
        lineage->task_identity == 0u ||
        lineage->root_task_identity == 0u ||
        lineage->sol_thread_generation == 0u ||
        lineage->task_generation == 0u ||
        lineage->root_task_generation == 0u ||
        lineage->script_runtime_generation == 0u ||
        lineage->native_thread_id == 0u) {
        return 0;
    }
    return !source_action ||
        (lineage->root_task_identity == lineage->task_identity &&
         lineage->root_task_generation == lineage->task_generation);
}

static int lineage_identity_equal(
    const SudekiMpTalosSolTaskLineage *left,
    const SudekiMpTalosSolTaskLineage *right
) {
    return left != NULL && right != NULL &&
        left->sol_thread_identity == right->sol_thread_identity &&
        left->task_identity == right->task_identity &&
        left->root_task_identity == right->root_task_identity &&
        left->sol_thread_generation == right->sol_thread_generation &&
        left->task_generation == right->task_generation &&
        left->root_task_generation == right->root_task_generation &&
        left->script_runtime_generation == right->script_runtime_generation &&
        left->native_thread_id == right->native_thread_id;
}

static int load_void_observation_equal(
    const SudekiMpTalosLoadVoidObservation *left,
    const SudekiMpTalosLoadVoidObservation *right
) {
    return left != NULL && right != NULL &&
        provenance_matches(&left->provenance, &right->provenance) &&
        lineage_identity_equal(&left->lineage, &right->lineage) &&
        left->observed_at_ms == right->observed_at_ms &&
        left->source_action_hash == right->source_action_hash &&
        left->source_action_start == right->source_action_start &&
        left->opcode_offset == right->opcode_offset &&
        left->scene_task_hash == right->scene_task_hash &&
        left->exact_build_confirmed == right->exact_build_confirmed &&
        left->interaction_authority_proven ==
            right->interaction_authority_proven;
}

static int set_zone_carrier_observation_equal(
    const SudekiMpTalosSetZoneCarrierObservation *left,
    const SudekiMpTalosSetZoneCarrierObservation *right
) {
    return left != NULL && right != NULL &&
        provenance_matches(&left->provenance, &right->provenance) &&
        lineage_identity_equal(&left->lineage, &right->lineage) &&
        left->observed_at_ms == right->observed_at_ms &&
        left->caller_function_hash == right->caller_function_hash &&
        left->caller_opcode_offset == right->caller_opcode_offset &&
        left->function_hash == right->function_hash &&
        left->function_start == right->function_start &&
        left->opcode_offset == right->opcode_offset &&
        left->binding_hash == right->binding_hash &&
        left->exact_build_confirmed == right->exact_build_confirmed &&
        memcmp(left->destination, right->destination,
            sizeof(left->destination)) == 0;
}

static int exact_load_void_observation(
    const SudekiMpTalosLoadVoidObservation *observation
) {
    return observation != NULL && observation->exact_build_confirmed &&
        observation->interaction_authority_proven &&
        provenance_valid(&observation->provenance) &&
        lineage_identity_valid(&observation->lineage, 1) &&
        observation->source_action_hash ==
            SUDEKIMP_TALOS_SOL_SOURCE_ACTION_HASH &&
        observation->source_action_start ==
            SUDEKIMP_TALOS_SOL_SOURCE_ACTION_START &&
        observation->opcode_offset ==
            SUDEKIMP_TALOS_SOL_LOAD_VOID_OPCODE_OFFSET &&
        observation->scene_task_hash ==
            SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH;
}

static int exact_set_zone_carrier_observation(
    const SudekiMpTalosSetZoneCarrierObservation *observation
) {
    return observation != NULL && observation->exact_build_confirmed &&
        provenance_valid(&observation->provenance) &&
        lineage_identity_valid(&observation->lineage, 0) &&
        observation->caller_function_hash ==
            SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH &&
        observation->caller_opcode_offset ==
            SUDEKIMP_TALOS_SOL_LOAD_VOID_SET_ZONE_OPCODE_OFFSET &&
        observation->function_hash ==
            SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_HASH &&
        observation->function_start ==
            SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_START &&
        observation->opcode_offset ==
            SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_OPCODE_OFFSET &&
        observation->binding_hash ==
            SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_HASH &&
        destination_is_exact_void(observation->destination);
}

static int set_zone_carrier_descends_from_load_void(
    const SudekiMpTalosLoadVoidObservation *load_void,
    const SudekiMpTalosSetZoneCarrierObservation *carrier
) {
    const SudekiMpTalosSolTaskLineage *source;
    const SudekiMpTalosSolTaskLineage *candidate;
    int same_task;
    int named_descendant;

    if (load_void == NULL || carrier == NULL) {
        return 0;
    }
    source = &load_void->lineage;
    candidate = &carrier->lineage;
    if (source->native_thread_id != candidate->native_thread_id ||
        source->script_runtime_generation !=
            candidate->script_runtime_generation) {
        return 0;
    }
    same_task = source->sol_thread_identity ==
            candidate->sol_thread_identity &&
        source->sol_thread_generation ==
            candidate->sol_thread_generation &&
        source->task_identity == candidate->task_identity &&
        source->task_generation == candidate->task_generation;
    named_descendant = candidate->root_task_identity ==
            source->task_identity &&
        candidate->root_task_generation == source->task_generation;
    return same_task || named_descendant;
}

static void advance_lineage_serial(SudekiMpTalosLineageTracker *tracker) {
    ++tracker->serial;
    if (tracker->serial == 0u) {
        ++tracker->serial;
    }
}

void SudekiMpTalosLineageInitialize(SudekiMpTalosLineageTracker *tracker) {
    if (tracker == NULL) {
        return;
    }
    memset(tracker, 0, sizeof(*tracker));
    tracker->state = SUDEKIMP_TALOS_LINEAGE_DISABLED;
    tracker->last_result = SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED;
}

void SudekiMpTalosLineageConfigure(
    SudekiMpTalosLineageTracker *tracker,
    int enabled
) {
    uint32_t serial;

    if (tracker == NULL) {
        return;
    }
    serial = tracker->serial;
    memset(tracker, 0, sizeof(*tracker));
    tracker->serial = serial;
    tracker->enabled = enabled ? 1u : 0u;
    tracker->state = enabled ? SUDEKIMP_TALOS_LINEAGE_IDLE :
        SUDEKIMP_TALOS_LINEAGE_DISABLED;
    tracker->last_result = enabled ?
        SUDEKIMP_TALOS_LINEAGE_RESULT_NO_CHANGE :
        SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED;
}

SudekiMpTalosLineageResult SudekiMpTalosLineageObserveLoadVoid(
    SudekiMpTalosLineageTracker *tracker,
    const SudekiMpTalosLoadVoidObservation *observation
) {
    if (tracker == NULL || observation == NULL) {
        return SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_INVALID;
    }
    if (!tracker->enabled) {
        return SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED;
    }
    if (!exact_load_void_observation(observation)) {
        tracker->last_result =
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_NOT_EXACT;
        return tracker->last_result;
    }
    if (tracker->state == SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED &&
        load_void_observation_equal(&tracker->load_void, observation)) {
        tracker->last_result = SUDEKIMP_TALOS_LINEAGE_RESULT_NO_CHANGE;
        return tracker->last_result;
    }
    if (tracker->state != SUDEKIMP_TALOS_LINEAGE_IDLE) {
        tracker->state = SUDEKIMP_TALOS_LINEAGE_QUARANTINED;
        tracker->last_result =
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STATE;
        return tracker->last_result;
    }
    advance_lineage_serial(tracker);
    tracker->load_void = *observation;
    memset(&tracker->carrier, 0, sizeof(tracker->carrier));
    tracker->state = SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED;
    tracker->last_result =
        SUDEKIMP_TALOS_LINEAGE_RESULT_LOAD_VOID_RECORDED;
    return tracker->last_result;
}

SudekiMpTalosLineageResult SudekiMpTalosLineageObserveSetZoneCarrier(
    SudekiMpTalosLineageTracker *tracker,
    const SudekiMpTalosSetZoneCarrierObservation *observation
) {
    if (tracker == NULL || observation == NULL) {
        return SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_INVALID;
    }
    if (!tracker->enabled) {
        return SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED;
    }
    if (!exact_set_zone_carrier_observation(observation)) {
        tracker->last_result =
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_NOT_EXACT;
        return tracker->last_result;
    }
    if (tracker->state == SUDEKIMP_TALOS_LINEAGE_CARRIER_MATCHED &&
        set_zone_carrier_observation_equal(&tracker->carrier, observation)) {
        tracker->last_result = SUDEKIMP_TALOS_LINEAGE_RESULT_NO_CHANGE;
        return tracker->last_result;
    }
    if (tracker->state != SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED) {
        tracker->last_result =
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STATE;
        return tracker->last_result;
    }
    if (!provenance_matches(&tracker->load_void.provenance,
            &observation->provenance) ||
        (uint32_t)(observation->observed_at_ms -
            tracker->load_void.observed_at_ms) >
            SUDEKIMP_TALOS_LINEAGE_MAX_AGE_MS) {
        tracker->state = SUDEKIMP_TALOS_LINEAGE_QUARANTINED;
        tracker->last_result =
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STALE;
        return tracker->last_result;
    }
    if (!set_zone_carrier_descends_from_load_void(&tracker->load_void,
            observation)) {
        tracker->state = SUDEKIMP_TALOS_LINEAGE_QUARANTINED;
        tracker->last_result =
            SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_LINEAGE;
        return tracker->last_result;
    }
    tracker->carrier = *observation;
    tracker->state = SUDEKIMP_TALOS_LINEAGE_CARRIER_MATCHED;
    tracker->last_result =
        SUDEKIMP_TALOS_LINEAGE_RESULT_CARRIER_MATCHED;
    return tracker->last_result;
}

int SudekiMpTalosLineageGetSnapshot(
    const SudekiMpTalosLineageTracker *tracker,
    SudekiMpTalosLineageSnapshot *snapshot
) {
    if (tracker == NULL || snapshot == NULL) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->load_void = tracker->load_void;
    snapshot->carrier = tracker->carrier;
    snapshot->state = (uint32_t)tracker->state;
    snapshot->last_result = (uint32_t)tracker->last_result;
    snapshot->serial = tracker->serial;
    snapshot->enabled = tracker->enabled;
    snapshot->exact_carrier_matched =
        tracker->state == SUDEKIMP_TALOS_LINEAGE_CARRIER_MATCHED;
    snapshot->production_continuation_supported = 0u;
    return 1;
}

void SudekiMpTalosLineageReset(SudekiMpTalosLineageTracker *tracker) {
    uint32_t serial;
    uint8_t enabled;

    if (tracker == NULL) {
        return;
    }
    serial = tracker->serial;
    enabled = tracker->enabled;
    memset(tracker, 0, sizeof(*tracker));
    tracker->serial = serial;
    tracker->enabled = enabled;
    tracker->state = enabled ? SUDEKIMP_TALOS_LINEAGE_IDLE :
        SUDEKIMP_TALOS_LINEAGE_DISABLED;
    tracker->last_result = enabled ?
        SUDEKIMP_TALOS_LINEAGE_RESULT_NO_CHANGE :
        SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED;
}

static void advance_serial(SudekiMpTalosEncounterIntercept *intercept) {
    ++intercept->serial;
    if (intercept->serial == 0u) {
        ++intercept->serial;
    }
}

static int exact_pre_call_identity_proven(uint32_t callsite_rva) {
#ifdef SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_TESTING
    return callsite_rva ==
        SUDEKIMP_TALOS_ENCOUNTER_TEST_PROVEN_CALLSITE_RVA;
#else
    (void)callsite_rva;
    return 0;
#endif
}

static int intercept_has_retained_call(
    const SudekiMpTalosEncounterIntercept *intercept
) {
    return intercept != NULL &&
        (intercept->native_deferred ||
         intercept->state ==
            SUDEKIMP_TALOS_INTERCEPT_CONTINUATION_CLAIMED);
}

static void set_terminal_state(
    SudekiMpTalosEncounterIntercept *intercept,
    SudekiMpTalosEncounterInterceptState state
) {
    intercept->state = state;
    intercept->prompt_visible = 0u;
    intercept->host_confirmed = 0u;
}

void SudekiMpTalosEncounterInterceptInitialize(
    SudekiMpTalosEncounterIntercept *intercept
) {
    if (intercept == NULL) {
        return;
    }
    memset(intercept, 0, sizeof(*intercept));
    intercept->state = SUDEKIMP_TALOS_INTERCEPT_DISABLED;
}

void SudekiMpTalosEncounterInterceptConfigure(
    SudekiMpTalosEncounterIntercept *intercept,
    int enabled
) {
    uint32_t serial;

    if (intercept == NULL) {
        return;
    }
    serial = intercept->serial;
    memset(intercept, 0, sizeof(*intercept));
    intercept->serial = serial;
    intercept->enabled = enabled ? 1u : 0u;
    intercept->state = enabled ? SUDEKIMP_TALOS_INTERCEPT_IDLE :
        SUDEKIMP_TALOS_INTERCEPT_DISABLED;
}

SudekiMpTalosEncounterObserveResult
SudekiMpTalosEncounterInterceptObserveSetZoneNow(
    SudekiMpTalosEncounterIntercept *intercept,
    const SudekiMpTalosEncounterSetZoneObservation *observation
) {
    uint8_t human_mask;

    if (intercept == NULL || observation == NULL || !intercept->enabled) {
        return SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE;
    }
    if (intercept_has_retained_call(intercept)) {
        return observation_matches_retained_call(intercept, observation) ?
            SUDEKIMP_TALOS_OBSERVE_DROP_NATIVE_BUSY :
            SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE;
    }
    if (!destination_is_exact_void(observation->destination)) {
        return SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE;
    }
    human_mask = observation->active_human_mask &
        SUDEKIMP_TALOS_ENCOUNTER_HUMAN_MASK;
    if (!observation->exact_build_confirmed ||
        (observation->active_human_mask &
            (uint8_t)~SUDEKIMP_TALOS_ENCOUNTER_HUMAN_MASK) != 0u ||
        observation->host_hero != SUDEKIMP_TALOS_ENCOUNTER_HOST_HERO_TAL ||
        (human_mask & SUDEKIMP_TALOS_ENCOUNTER_HOST_BIT) == 0u ||
        !provenance_valid(&observation->provenance)) {
        return SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE;
    }

    advance_serial(intercept);
    intercept->provenance = observation->provenance;
    intercept->callsite_rva = observation->callsite_rva;
    intercept->active_human_mask = human_mask;
    intercept->prompt_visible = 0u;
    intercept->native_deferred = 0u;
    intercept->host_confirmed = 0u;
    intercept->discard_reported = 0u;
    memcpy(intercept->destination, observation->destination,
        sizeof(intercept->destination));

    if (!exact_pre_call_identity_proven(observation->callsite_rva)) {
        intercept->state = SUDEKIMP_TALOS_INTERCEPT_OBSERVED_ONLY;
        return SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE_OBSERVED;
    }

    intercept->state = SUDEKIMP_TALOS_INTERCEPT_AWAITING_PROMPT;
    intercept->native_deferred = 1u;
    return SUDEKIMP_TALOS_OBSERVE_DEFER_NATIVE;
}

int SudekiMpTalosEncounterInterceptGetPromptSnapshot(
    const SudekiMpTalosEncounterIntercept *intercept,
    SudekiMpTalosEncounterPromptSnapshot *snapshot
) {
    if (intercept == NULL || snapshot == NULL) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->provenance = intercept->provenance;
    snapshot->state = (uint32_t)intercept->state;
    snapshot->serial = intercept->serial;
    snapshot->callsite_rva = intercept->callsite_rva;
    snapshot->party_size = SUDEKIMP_TALOS_ENCOUNTER_HERO_COUNT;
    snapshot->talos_hp = SUDEKIMP_TALOS_ENCOUNTER_EXPANDED_HP;
    snapshot->active_human_mask = intercept->active_human_mask;
    snapshot->human_count = (uint8_t)count_bits(
        intercept->active_human_mask);
    snapshot->continuation_supported =
        exact_pre_call_identity_proven(intercept->callsite_rva) ? 1u : 0u;
    snapshot->prompt_active =
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_AWAITING_PROMPT ||
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_AWAITING_HOST;
    snapshot->prompt_visible = intercept->prompt_visible;
    snapshot->native_deferred = intercept->native_deferred;
    snapshot->host_confirmed = intercept->host_confirmed;
    snapshot->terminal =
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_COMPLETED ||
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_CANCELLED ||
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_QUARANTINED;
    memcpy(snapshot->destination, intercept->destination,
        sizeof(snapshot->destination));
    return 1;
}

static SudekiMpTalosEncounterCommandResult validate_serial(
    const SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial
) {
    if (intercept == NULL || serial == 0u) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_INVALID;
    }
    if (serial != intercept->serial) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_STALE;
    }
    return SUDEKIMP_TALOS_COMMAND_NO_CHANGE;
}

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptReportPrompt(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial,
    int visible
) {
    SudekiMpTalosEncounterCommandResult result = validate_serial(
        intercept, serial);

    if (result != SUDEKIMP_TALOS_COMMAND_NO_CHANGE) {
        return result;
    }
    if (intercept->state != SUDEKIMP_TALOS_INTERCEPT_AWAITING_PROMPT &&
        intercept->state != SUDEKIMP_TALOS_INTERCEPT_AWAITING_HOST) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_STATE;
    }
    if (!visible) {
        set_terminal_state(intercept, SUDEKIMP_TALOS_INTERCEPT_CANCELLED);
        return SUDEKIMP_TALOS_COMMAND_ACCEPTED;
    }
    intercept->prompt_visible = 1u;
    intercept->state = SUDEKIMP_TALOS_INTERCEPT_AWAITING_HOST;
    return SUDEKIMP_TALOS_COMMAND_ACCEPTED;
}

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptHostConfirm(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial,
    const SudekiMpTalosTransitionProvenance *current
) {
    SudekiMpTalosEncounterCommandResult result = validate_serial(
        intercept, serial);

    if (result != SUDEKIMP_TALOS_COMMAND_NO_CHANGE) {
        return result;
    }
    if (intercept->state != SUDEKIMP_TALOS_INTERCEPT_AWAITING_HOST ||
        !intercept->prompt_visible || !intercept->native_deferred) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_STATE;
    }
    if (!provenance_matches(&intercept->provenance, current)) {
        set_terminal_state(intercept,
            SUDEKIMP_TALOS_INTERCEPT_QUARANTINED);
        return SUDEKIMP_TALOS_COMMAND_QUARANTINED;
    }
    intercept->host_confirmed = 1u;
    intercept->state = SUDEKIMP_TALOS_INTERCEPT_READY;
    return SUDEKIMP_TALOS_COMMAND_ACCEPTED;
}

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptHostCancel(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial
) {
    SudekiMpTalosEncounterCommandResult result = validate_serial(
        intercept, serial);

    if (result != SUDEKIMP_TALOS_COMMAND_NO_CHANGE) {
        return result;
    }
    if (intercept->state != SUDEKIMP_TALOS_INTERCEPT_AWAITING_PROMPT &&
        intercept->state != SUDEKIMP_TALOS_INTERCEPT_AWAITING_HOST) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_STATE;
    }
    set_terminal_state(intercept, SUDEKIMP_TALOS_INTERCEPT_CANCELLED);
    return SUDEKIMP_TALOS_COMMAND_ACCEPTED;
}

SudekiMpTalosEncounterServiceAction
SudekiMpTalosEncounterInterceptService(
    SudekiMpTalosEncounterIntercept *intercept,
    const SudekiMpTalosTransitionProvenance *current
) {
    if (intercept == NULL || !intercept->enabled ||
        !intercept->native_deferred) {
        return SUDEKIMP_TALOS_SERVICE_NONE;
    }
    if (intercept->state == SUDEKIMP_TALOS_INTERCEPT_CANCELLED ||
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_QUARANTINED) {
        if (intercept->discard_reported) {
            return SUDEKIMP_TALOS_SERVICE_NONE;
        }
        intercept->discard_reported = 1u;
        intercept->native_deferred = 0u;
        return SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED;
    }
    if (intercept->state ==
            SUDEKIMP_TALOS_INTERCEPT_CONTINUATION_CLAIMED ||
        intercept->state == SUDEKIMP_TALOS_INTERCEPT_COMPLETED) {
        return SUDEKIMP_TALOS_SERVICE_NONE;
    }
    if (!provenance_matches(&intercept->provenance, current)) {
        set_terminal_state(intercept,
            SUDEKIMP_TALOS_INTERCEPT_QUARANTINED);
        intercept->discard_reported = 1u;
        intercept->native_deferred = 0u;
        return SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED;
    }
    if (intercept->state != SUDEKIMP_TALOS_INTERCEPT_READY ||
        !intercept->host_confirmed) {
        return SUDEKIMP_TALOS_SERVICE_NONE;
    }
    intercept->state = SUDEKIMP_TALOS_INTERCEPT_CONTINUATION_CLAIMED;
    intercept->native_deferred = 0u;
    return SUDEKIMP_TALOS_SERVICE_CONTINUE_NATIVE_ONCE;
}

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptFinishContinuation(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial,
    int succeeded
) {
    SudekiMpTalosEncounterCommandResult result = validate_serial(
        intercept, serial);

    if (result != SUDEKIMP_TALOS_COMMAND_NO_CHANGE) {
        return result;
    }
    if (intercept->state !=
            SUDEKIMP_TALOS_INTERCEPT_CONTINUATION_CLAIMED) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_STATE;
    }
    set_terminal_state(intercept, succeeded ?
        SUDEKIMP_TALOS_INTERCEPT_COMPLETED :
        SUDEKIMP_TALOS_INTERCEPT_QUARANTINED);
    return succeeded ? SUDEKIMP_TALOS_COMMAND_ACCEPTED :
        SUDEKIMP_TALOS_COMMAND_QUARANTINED;
}

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptReset(
    SudekiMpTalosEncounterIntercept *intercept
) {
    uint32_t serial;
    uint8_t enabled;

    if (intercept == NULL) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_INVALID;
    }
    if (intercept_has_retained_call(intercept)) {
        return SUDEKIMP_TALOS_COMMAND_REJECTED_STATE;
    }
    serial = intercept->serial;
    enabled = intercept->enabled;
    memset(intercept, 0, sizeof(*intercept));
    intercept->serial = serial;
    intercept->enabled = enabled;
    intercept->state = enabled ? SUDEKIMP_TALOS_INTERCEPT_IDLE :
        SUDEKIMP_TALOS_INTERCEPT_DISABLED;
    return SUDEKIMP_TALOS_COMMAND_ACCEPTED;
}
