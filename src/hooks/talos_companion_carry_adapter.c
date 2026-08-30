#include "hooks/talos_companion_carry_adapter.h"

#include <stddef.h>
#include <string.h>

static const uint32_t delete_logical[3] = {
    SUDEKIMP_TALOS_CARRY_BUKI_LOGICAL,
    SUDEKIMP_TALOS_CARRY_AILISH_LOGICAL,
    SUDEKIMP_TALOS_CARRY_ELCO_LOGICAL
};
static const uint32_t delete_raw[3] = {
    SUDEKIMP_TALOS_CARRY_BUKI_RAW,
    SUDEKIMP_TALOS_CARRY_AILISH_RAW,
    SUDEKIMP_TALOS_CARRY_ELCO_RAW
};
static const uint8_t delete_resource[3] = {
    SUDEKIMP_TALOS_CARRY_RESOURCE_BUKI,
    SUDEKIMP_TALOS_CARRY_RESOURCE_AILISH,
    SUDEKIMP_TALOS_CARRY_RESOURCE_ELCO
};
static const SudekiMpTalosCarryAdapterEvent delete_event[3] = {
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_BUKI,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_AILISH,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_ELCO
};

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) ++value;
    return value;
}

static int exact_void(const char value[16]) {
    static const char expected[] = "Void";

    return value != NULL && memcmp(value, expected, sizeof(expected)) == 0;
}

static int provenance_valid(const SudekiMpTalosEncounterProvenance *value) {
    unsigned int i;
    unsigned int j;
    uint8_t seen_heroes = 0u;
    uint8_t seen_controllers = 0u;

    if (value == NULL || value->serial == 0u ||
        value->transition_serial == 0u || value->world_generation == 0u ||
        value->source_generation == 0u || value->host_actor == 0u ||
        value->host_actor_generation == 0u ||
        value->host_lease_generation == 0u || value->combatant_count != 4u ||
        value->talos_health_target != 180000u ||
        (value->active_human_mask & (uint8_t)~0x0fu) != 0u ||
        (value->active_human_mask & 0x01u) == 0u ||
        value->hero_by_seat[0] != SUDEKIMP_TALOS_HERO_TAL) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (value->hero_actor_identity[i] == 0u ||
            value->hero_actor_generation[i] == 0u) return 0;
        for (j = i + 1u; j < SUDEKIMP_TALOS_HERO_COUNT; ++j) {
            if (value->hero_actor_identity[i] ==
                    value->hero_actor_identity[j]) return 0;
        }
    }
    if (value->hero_actor_identity[SUDEKIMP_TALOS_HERO_TAL] !=
            value->host_actor ||
        value->hero_actor_generation[SUDEKIMP_TALOS_HERO_TAL] !=
            value->host_actor_generation) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_SEAT_COUNT; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        uint8_t hero = value->hero_by_seat[i];
        uint8_t controller = value->controller_slot_by_seat[i];

        if ((value->active_human_mask & bit) == 0u) {
            if (hero != SUDEKIMP_TALOS_HERO_NONE ||
                controller != SUDEKIMP_TALOS_CONTROLLER_NONE ||
                value->input_identity_by_seat[i] != 0u ||
                value->input_generation_by_seat[i] != 0u) return 0;
            continue;
        }
        if (hero >= SUDEKIMP_TALOS_HERO_COUNT ||
            (seen_heroes & (uint8_t)(1u << hero)) != 0u ||
            (i != 0u && hero == SUDEKIMP_TALOS_HERO_TAL) ||
            value->input_identity_by_seat[i] == 0u ||
            value->input_generation_by_seat[i] == 0u) return 0;
        for (j = 0u; j < i; ++j) {
            if ((value->active_human_mask & (uint8_t)(1u << j)) != 0u &&
                value->input_identity_by_seat[j] ==
                    value->input_identity_by_seat[i]) return 0;
        }
        if ((i == 0u && controller != SUDEKIMP_TALOS_CONTROLLER_NONE) ||
            (i != 0u && (controller >= 4u ||
             (seen_controllers & (uint8_t)(1u << controller)) != 0u)))
            return 0;
        if (i != 0u) seen_controllers |= (uint8_t)(1u << controller);
        seen_heroes |= (uint8_t)(1u << hero);
    }
    return 1;
}

static int provenance_matches(
    const SudekiMpTalosEncounterProvenance *left,
    const SudekiMpTalosEncounterProvenance *right
) {
    unsigned int i;

    if (!provenance_valid(left) || !provenance_valid(right) ||
        left->serial != right->serial ||
        left->transition_serial != right->transition_serial ||
        left->world_generation != right->world_generation ||
        left->source_generation != right->source_generation ||
        left->host_actor != right->host_actor ||
        left->host_actor_generation != right->host_actor_generation ||
        left->host_lease_generation != right->host_lease_generation ||
        left->talos_health_target != right->talos_health_target ||
        left->active_human_mask != right->active_human_mask ||
        left->combatant_count != right->combatant_count) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (left->hero_actor_identity[i] != right->hero_actor_identity[i] ||
            left->hero_actor_generation[i] != right->hero_actor_generation[i] ||
            left->hero_by_seat[i] != right->hero_by_seat[i] ||
            left->controller_slot_by_seat[i] !=
                right->controller_slot_by_seat[i] ||
            left->input_identity_by_seat[i] !=
                right->input_identity_by_seat[i] ||
            left->input_generation_by_seat[i] !=
                right->input_generation_by_seat[i]) return 0;
    }
    return 1;
}

static int lineage_exact(const SudekiMpTalosCarryLineage *value) {
    return value != NULL && value->source_task != 0u &&
        value->load_void_task != 0u && value->source_task_generation != 0u &&
        value->load_void_task_generation != 0u &&
        value->runtime_generation != 0u && value->descendant_proven &&
        value->source_hash == SUDEKIMP_TALOS_CARRY_SOURCE_HASH &&
        value->source_start == SUDEKIMP_TALOS_CARRY_SOURCE_START &&
        value->source_opcode == SUDEKIMP_TALOS_CARRY_SOURCE_OPCODE &&
        value->load_void_hash == SUDEKIMP_TALOS_CARRY_LOAD_VOID_HASH &&
        value->load_void_start == SUDEKIMP_TALOS_CARRY_LOAD_VOID_START;
}

static int lineage_matches(
    const SudekiMpTalosCarryLineage *left,
    const SudekiMpTalosCarryLineage *right
) {
    return lineage_exact(left) && lineage_exact(right) &&
        left->source_task == right->source_task &&
        left->load_void_task == right->load_void_task &&
        left->source_task_generation == right->source_task_generation &&
        left->load_void_task_generation ==
            right->load_void_task_generation &&
        left->runtime_generation == right->runtime_generation;
}

static int formation_preflight_exact(
    const SudekiMpTalosCarryFormation *formation,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    unsigned int i;

    if (formation == NULL || !formation->observed ||
        formation->group_identity == 0u || formation->group_generation == 0u ||
        formation->formation_owner_identity == 0u ||
        formation->formation_owner_generation == 0u ||
        formation->formation_identity == 0u ||
        formation->formation_generation == 0u ||
        formation->group_member_mask != 0x0fu ||
        formation->formation_member_mask != 0x0fu) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (formation->actor_identity[i] !=
                provenance->hero_actor_identity[i] ||
            formation->actor_generation[i] !=
                provenance->hero_actor_generation[i]) return 0;
    }
    return 1;
}

static int transition_provenance_matches(
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosTransitionProvenance *transition
) {
    return provenance != NULL && transition != NULL &&
        transition->world_identity != 0u &&
        transition->host_actor_identity != 0u &&
        transition->world_generation == provenance->world_generation &&
        transition->source_generation == provenance->source_generation &&
        transition->host_actor_generation ==
            provenance->host_actor_generation &&
        transition->host_lease_generation == provenance->host_lease_generation &&
        transition->host_actor_identity == (uint64_t)provenance->host_actor &&
        (uintptr_t)transition->host_actor_identity == provenance->host_actor;
}

static int load_void_facts_exact(
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosCarryPreflight *preflight,
    const SudekiMpTalosLoadVoidObservation *load
) {
    if (load == NULL || preflight == NULL || !preflight->exact_executable ||
        !preflight->exact_asset || !preflight->host_authority ||
        !lineage_exact(&preflight->lineage)) return 0;
    return load->exact_build_confirmed &&
        load->interaction_authority_proven &&
        load->source_action_hash == SUDEKIMP_TALOS_SOL_SOURCE_ACTION_HASH &&
        load->source_action_start == SUDEKIMP_TALOS_SOL_SOURCE_ACTION_START &&
        load->opcode_offset == SUDEKIMP_TALOS_SOL_LOAD_VOID_OPCODE_OFFSET &&
        load->scene_task_hash == SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH &&
        load->lineage.sol_thread_identity != 0u &&
        load->lineage.sol_thread_generation != 0u &&
        load->lineage.task_identity != 0u &&
        load->lineage.task_generation != 0u &&
        load->lineage.root_task_identity != 0u &&
        load->lineage.root_task_generation != 0u &&
        load->lineage.script_runtime_generation != 0u &&
        load->lineage.native_thread_id != 0u &&
        preflight->lineage.source_task ==
            (uintptr_t)load->lineage.task_identity &&
        (uint64_t)preflight->lineage.source_task ==
            load->lineage.task_identity &&
        preflight->lineage.source_task_generation ==
            load->lineage.task_generation &&
        preflight->lineage.runtime_generation ==
            load->lineage.script_runtime_generation &&
        transition_provenance_matches(provenance, &load->provenance);
}

static int load_void_lineage_exact(
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosCarryPreflight *preflight,
    const SudekiMpTalosLineageSnapshot *lineage
) {
    return lineage != NULL && lineage->enabled && lineage->serial != 0u &&
        lineage->state == SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED &&
        !lineage->exact_carrier_matched &&
        !lineage->production_continuation_supported &&
        load_void_facts_exact(provenance, preflight, &lineage->load_void);
}

static int carrier_lineage_exact(
    const SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosLineageSnapshot *lineage
) {
    const SudekiMpTalosLoadVoidObservation *load;
    const SudekiMpTalosSetZoneCarrierObservation *carrier;
    int same_task;
    int named_descendant;

    if (lineage == NULL || !lineage->enabled ||
        !lineage->exact_carrier_matched ||
        lineage->state != SUDEKIMP_TALOS_LINEAGE_CARRIER_MATCHED ||
        lineage->production_continuation_supported ||
        lineage->serial != adapter->lineage_serial) return 0;
    load = &lineage->load_void;
    carrier = &lineage->carrier;
    same_task = load->lineage.sol_thread_identity ==
            carrier->lineage.sol_thread_identity &&
        load->lineage.sol_thread_generation ==
            carrier->lineage.sol_thread_generation &&
        load->lineage.task_identity == carrier->lineage.task_identity &&
        load->lineage.task_generation == carrier->lineage.task_generation;
    named_descendant = carrier->lineage.root_task_identity ==
            load->lineage.task_identity &&
        carrier->lineage.root_task_generation ==
            load->lineage.task_generation;
    return load_void_facts_exact(
            &adapter->provenance, &adapter->preflight, load) &&
        carrier->lineage.sol_thread_identity != 0u &&
        carrier->lineage.sol_thread_generation != 0u &&
        carrier->lineage.task_identity != 0u &&
        carrier->lineage.task_generation != 0u &&
        carrier->lineage.root_task_identity != 0u &&
        carrier->lineage.root_task_generation != 0u &&
        carrier->lineage.native_thread_id == load->lineage.native_thread_id &&
        carrier->lineage.script_runtime_generation ==
            load->lineage.script_runtime_generation &&
        (same_task || named_descendant) &&
        (uint32_t)(carrier->observed_at_ms - load->observed_at_ms) <=
            SUDEKIMP_TALOS_LINEAGE_MAX_AGE_MS &&
        carrier->provenance.world_identity ==
            load->provenance.world_identity &&
        transition_provenance_matches(&adapter->provenance,
            &carrier->provenance) &&
        carrier->exact_build_confirmed &&
        carrier->caller_function_hash == SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH &&
        carrier->caller_opcode_offset ==
            SUDEKIMP_TALOS_SOL_LOAD_VOID_SET_ZONE_OPCODE_OFFSET &&
        carrier->function_hash == SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_HASH &&
        carrier->function_start == SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_START &&
        carrier->opcode_offset ==
            SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_OPCODE_OFFSET &&
        carrier->binding_hash == SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_HASH &&
        exact_void(carrier->destination) &&
        adapter->preflight.lineage.load_void_task ==
            (uintptr_t)carrier->lineage.task_identity &&
        (uint64_t)adapter->preflight.lineage.load_void_task ==
            carrier->lineage.task_identity &&
        adapter->preflight.lineage.load_void_task_generation ==
            carrier->lineage.task_generation &&
        adapter->preflight.lineage.runtime_generation ==
            carrier->lineage.script_runtime_generation;
}

static int release_formation_observed(
    const SudekiMpTalosCarryFormation *formation
) {
    unsigned int i;

    if (formation == NULL || !formation->observed ||
        formation->group_identity == 0u || formation->group_generation == 0u ||
        formation->formation_owner_identity == 0u ||
        formation->formation_owner_generation == 0u ||
        formation->formation_identity == 0u ||
        formation->formation_generation == 0u ||
        (formation->group_member_mask & (uint8_t)~0x0fu) != 0u ||
        (formation->formation_member_mask & (uint8_t)~0x0fu) != 0u)
        return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if (((formation->group_member_mask |
                formation->formation_member_mask) & bit) != 0u &&
            (formation->actor_identity[i] == 0u ||
             formation->actor_generation[i] == 0u)) return 0;
    }
    return 1;
}

static int formation_identity_matches(
    const SudekiMpTalosCarryFormation *left,
    const SudekiMpTalosCarryFormation *right
) {
    return left->group_identity == right->group_identity &&
        left->formation_owner_identity == right->formation_owner_identity &&
        left->formation_identity == right->formation_identity &&
        left->group_generation == right->group_generation &&
        left->formation_owner_generation ==
            right->formation_owner_generation &&
        left->formation_generation == right->formation_generation;
}

static uint8_t actor_match_mask(
    const SudekiMpTalosCarryFormation *formation,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    unsigned int i;
    uint8_t mask = 0u;

    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (formation->actor_identity[i] ==
                provenance->hero_actor_identity[i] &&
            formation->actor_generation[i] ==
                provenance->hero_actor_generation[i])
            mask |= (uint8_t)(1u << i);
    }
    return mask;
}

static void clear_observation(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterState state
) {
    memset(&adapter->provenance, 0, sizeof(adapter->provenance));
    memset(&adapter->preflight, 0, sizeof(adapter->preflight));
    memset(&adapter->release_formation, 0,
        sizeof(adapter->release_formation));
    adapter->state = state;
    adapter->lineage_serial = 0u;
    adapter->arrival_world_generation = 0u;
    adapter->arrival_source_generation = 0u;
    adapter->teardown_member_count = 0u;
    adapter->delete_cursor = 0u;
    adapter->delete_native_completed_mask = 0u;
    adapter->release_actor_match_mask = 0u;
    adapter->exact_set_zone = 0u;
    adapter->exact_release_point = 0u;
    adapter->release_carry_ready = 0u;
    adapter->end_tsa_native_completed = 0u;
    adapter->tsa_inactive_after = 0u;
    adapter->teardown_native_completed = 0u;
    adapter->teardown_verified_empty = 0u;
}

static void fill_snapshot(
    const SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterSnapshot *snapshot
) {
    unsigned int i;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state = (uint32_t)adapter->state;
    snapshot->last_result = (uint32_t)adapter->last_result;
    snapshot->last_event = (uint32_t)adapter->last_event;
    snapshot->change_serial = adapter->change_serial;
    snapshot->encounter_serial = adapter->provenance.serial;
    snapshot->transition_serial = adapter->provenance.transition_serial;
    snapshot->world_generation = adapter->provenance.world_generation;
    snapshot->source_generation = adapter->provenance.source_generation;
    snapshot->lineage_serial = adapter->lineage_serial;
    snapshot->runtime_generation = adapter->preflight.lineage.runtime_generation;
    snapshot->arrival_world_generation = adapter->arrival_world_generation;
    snapshot->arrival_source_generation = adapter->arrival_source_generation;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i)
        snapshot->hero_actor_generation[i] =
            adapter->provenance.hero_actor_generation[i];
    snapshot->dropped_change_records = adapter->dropped_change_records;
    snapshot->teardown_member_count = adapter->teardown_member_count;
    snapshot->active_human_mask = adapter->provenance.active_human_mask;
    snapshot->delete_native_completed_mask =
        adapter->delete_native_completed_mask;
    snapshot->release_group_member_mask =
        adapter->release_formation.group_member_mask;
    snapshot->release_formation_member_mask =
        adapter->release_formation.formation_member_mask;
    snapshot->release_actor_match_mask = adapter->release_actor_match_mask;
    snapshot->enabled = adapter->enabled;
    snapshot->exact_load_void = adapter->lineage_serial != 0u;
    snapshot->exact_set_zone = adapter->exact_set_zone;
    snapshot->exact_release_point = adapter->exact_release_point;
    snapshot->release_carry_ready = adapter->release_carry_ready;
    snapshot->end_tsa_native_completed = adapter->end_tsa_native_completed;
    snapshot->tsa_inactive_after = adapter->tsa_inactive_after;
    snapshot->teardown_native_completed = adapter->teardown_native_completed;
    snapshot->teardown_verified_empty = adapter->teardown_verified_empty;
    snapshot->observation_only = 1u;
    snapshot->native_passthrough_required = 1u;
    snapshot->mutation_supported = 0u;
}

static void record_change(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterEvent event
) {
    unsigned int tail;
    SudekiMpTalosCarryAdapterChange *change;

    adapter->change_serial = advance_nonzero(adapter->change_serial);
    adapter->last_event = event;
    if (adapter->change_count ==
            SUDEKIMP_TALOS_CARRY_ADAPTER_CHANGE_CAPACITY) {
        adapter->change_head = (uint8_t)((adapter->change_head + 1u) %
            SUDEKIMP_TALOS_CARRY_ADAPTER_CHANGE_CAPACITY);
        --adapter->change_count;
        ++adapter->dropped_change_records;
    }
    tail = (unsigned int)(adapter->change_head + adapter->change_count) %
        SUDEKIMP_TALOS_CARRY_ADAPTER_CHANGE_CAPACITY;
    change = &adapter->changes[tail];
    change->event = event;
    fill_snapshot(adapter, &change->snapshot);
    ++adapter->change_count;
}

static SudekiMpTalosCarryAdapterResult recorded(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterEvent event
) {
    adapter->last_result = SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED;
    record_change(adapter, event);
    return adapter->last_result;
}

static SudekiMpTalosCarryAdapterResult invalidate(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterResult result
) {
    adapter->last_result = result;
    if (adapter->state != SUDEKIMP_TALOS_CARRY_ADAPTER_INVALIDATED) {
        adapter->state = SUDEKIMP_TALOS_CARRY_ADAPTER_INVALIDATED;
        record_change(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_INVALIDATED);
    }
    return result;
}

void SudekiMpTalosCompanionCarryAdapterInitialize(
    SudekiMpTalosCompanionCarryAdapter *adapter
) {
    if (adapter == NULL) return;
    memset(adapter, 0, sizeof(*adapter));
    adapter->state = SUDEKIMP_TALOS_CARRY_ADAPTER_DISABLED;
    adapter->last_result = SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
}

void SudekiMpTalosCompanionCarryAdapterConfigure(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    int enabled
) {
    uint8_t requested;

    if (adapter == NULL) return;
    requested = enabled ? 1u : 0u;
    if (adapter->enabled == requested) return;
    adapter->enabled = requested;
    adapter->change_head = 0u;
    adapter->change_count = 0u;
    clear_observation(adapter, requested ?
        SUDEKIMP_TALOS_CARRY_ADAPTER_IDLE :
        SUDEKIMP_TALOS_CARRY_ADAPTER_DISABLED);
    adapter->last_result = requested ?
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_NO_CHANGE :
        SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    record_change(adapter, requested ?
        SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_ENABLED :
        SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DISABLED);
}

SudekiMpTalosCarryAdapterResult SudekiMpTalosCompanionCarryAdapterReset(
    SudekiMpTalosCompanionCarryAdapter *adapter
) {
    if (adapter == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    if (adapter->state != SUDEKIMP_TALOS_CARRY_ADAPTER_TEARDOWN_OBSERVED &&
        adapter->state != SUDEKIMP_TALOS_CARRY_ADAPTER_INVALIDATED)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_SEQUENCE;
    clear_observation(adapter, SUDEKIMP_TALOS_CARRY_ADAPTER_IDLE);
    return recorded(adapter, SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_RESET);
}

SudekiMpTalosCarryAdapterResult SudekiMpTalosCompanionCarryAdapterBegin(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosCarryPreflight *preflight,
    const SudekiMpTalosLineageSnapshot *lineage
) {
    if (adapter == NULL || provenance == NULL || preflight == NULL ||
        lineage == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    if (adapter->state != SUDEKIMP_TALOS_CARRY_ADAPTER_IDLE)
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_SEQUENCE);
    if (!provenance_valid(provenance) ||
        !formation_preflight_exact(&preflight->formation, provenance) ||
        !load_void_lineage_exact(provenance, preflight, lineage))
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    adapter->provenance = *provenance;
    adapter->preflight = *preflight;
    adapter->lineage_serial = lineage->serial;
    adapter->state = SUDEKIMP_TALOS_CARRY_ADAPTER_LOAD_VOID_OBSERVED;
    return recorded(adapter,
        SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_LOAD_VOID);
}

static int delete_index(
    const SudekiMpTalosCarryDeleteObservation *observation,
    int *candidate
) {
    int logical = -1;
    int raw = -1;
    unsigned int i;

    *candidate = 0;
    for (i = 0u; i < 3u; ++i) {
        if (observation->logical_opcode == delete_logical[i]) {
            logical = (int)i;
            *candidate = 1;
        }
        if (observation->raw_opcode == delete_raw[i]) {
            raw = (int)i;
            *candidate = 1;
        }
    }
    if (!*candidate) return -1;
    return logical == raw ? logical : -2;
}

SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveDeletePc(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryAdapterDeleteCall *call
) {
    int candidate;
    int index;

    if (adapter == NULL || call == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    index = delete_index(&call->evidence, &candidate);
    if (!candidate) return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_NO_CHANGE;
    if ((adapter->state !=
            SUDEKIMP_TALOS_CARRY_ADAPTER_LOAD_VOID_OBSERVED &&
         adapter->state !=
            SUDEKIMP_TALOS_CARRY_ADAPTER_DELETE_SEQUENCE_OBSERVED) ||
        index < 0 || (unsigned int)index != adapter->delete_cursor ||
        !provenance_matches(&adapter->provenance, current) ||
        !call->original_call_entered || !call->original_call_completed ||
        !call->evidence.exact_executable || !call->evidence.exact_asset ||
        call->evidence.binding_hash !=
            SUDEKIMP_TALOS_CARRY_DELETE_PC_HASH ||
        call->evidence.resource_id != delete_resource[index] ||
        !lineage_matches(&adapter->preflight.lineage,
            &call->evidence.lineage))
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    adapter->delete_native_completed_mask |= (uint8_t)(1u << index);
    ++adapter->delete_cursor;
    if (adapter->delete_cursor == 3u)
        adapter->state =
            SUDEKIMP_TALOS_CARRY_ADAPTER_DELETE_SEQUENCE_OBSERVED;
    return recorded(adapter, delete_event[index]);
}

SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveSetZone(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryAdapterSetZoneCall *call,
    const SudekiMpTalosLineageSnapshot *lineage
) {
    int candidate;

    if (adapter == NULL || call == NULL || lineage == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    candidate = call->evidence.logical_opcode ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL ||
        call->evidence.raw_opcode == SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW;
    if (!candidate) return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_NO_CHANGE;
    if (adapter->state !=
            SUDEKIMP_TALOS_CARRY_ADAPTER_DELETE_SEQUENCE_OBSERVED ||
        adapter->delete_native_completed_mask != 0x07u ||
        !provenance_matches(&adapter->provenance, current) ||
        !call->original_call_entered || !call->original_call_completed ||
        call->evidence.encounter_serial != adapter->provenance.serial ||
        call->evidence.transition_serial !=
            adapter->provenance.transition_serial ||
        call->evidence.logical_opcode !=
            SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL ||
        call->evidence.raw_opcode != SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW ||
        call->evidence.binding_hash != SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH ||
        !call->evidence.exact_executable || !call->evidence.exact_asset ||
        !exact_void(call->evidence.destination) ||
        !lineage_matches(&adapter->preflight.lineage,
            &call->evidence.lineage) ||
        !carrier_lineage_exact(adapter, lineage))
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    adapter->exact_set_zone = 1u;
    adapter->state = SUDEKIMP_TALOS_CARRY_ADAPTER_SET_ZONE_OBSERVED;
    return recorded(adapter, SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_SET_ZONE);
}

SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveReleasePoint(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryFormationObservation *observation
) {
    uint8_t match_mask;

    if (adapter == NULL || observation == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    if (adapter->state != SUDEKIMP_TALOS_CARRY_ADAPTER_SET_ZONE_OBSERVED ||
        !provenance_matches(&adapter->provenance, current) ||
        observation->encounter_serial != adapter->provenance.serial ||
        observation->transition_serial !=
            adapter->provenance.transition_serial ||
        observation->arrival_world_generation == 0u ||
        observation->arrival_source_generation == 0u ||
        (observation->arrival_world_generation ==
            adapter->provenance.world_generation &&
         observation->arrival_source_generation ==
            adapter->provenance.source_generation) ||
        observation->set_zone_logical !=
            SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL ||
        observation->set_zone_raw != SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW ||
        observation->set_zone_hash != SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH ||
        observation->end_tsa_logical !=
            SUDEKIMP_TALOS_CARRY_END_TSA_LOGICAL ||
        observation->end_tsa_raw != SUDEKIMP_TALOS_CARRY_END_TSA_RAW ||
        observation->end_tsa_hash != SUDEKIMP_TALOS_CARRY_END_TSA_HASH ||
        observation->set_zone_authorization_serial != 0u ||
        !observation->exact_release_point || !observation->tsa_active ||
        !release_formation_observed(&observation->formation))
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    adapter->release_formation = observation->formation;
    adapter->arrival_world_generation =
        observation->arrival_world_generation;
    adapter->arrival_source_generation =
        observation->arrival_source_generation;
    match_mask = actor_match_mask(&observation->formation,
        &adapter->provenance);
    adapter->release_actor_match_mask = match_mask;
    adapter->exact_release_point = 1u;
    adapter->release_carry_ready = observation->arrival_settled &&
        observation->tal_final_pop_settled && observation->item_use_settled &&
        observation->boss_ready && observation->no_pending_removal &&
        observation->formation.group_member_mask == 0x0fu &&
        observation->formation.formation_member_mask == 0x0fu &&
        match_mask == 0x0fu && formation_identity_matches(
            &adapter->preflight.formation, &observation->formation);
    adapter->state =
        SUDEKIMP_TALOS_CARRY_ADAPTER_RELEASE_POINT_OBSERVED;
    return recorded(adapter,
        SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_RELEASE_POINT);
}

SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveEndTsa(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryAdapterEndTsaCall *call
) {
    if (adapter == NULL || call == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    if (adapter->state !=
            SUDEKIMP_TALOS_CARRY_ADAPTER_RELEASE_POINT_OBSERVED ||
        !provenance_matches(&adapter->provenance, current) ||
        call->encounter_serial != adapter->provenance.serial ||
        call->transition_serial != adapter->provenance.transition_serial ||
        call->logical_opcode != SUDEKIMP_TALOS_CARRY_END_TSA_LOGICAL ||
        call->raw_opcode != SUDEKIMP_TALOS_CARRY_END_TSA_RAW ||
        call->binding_hash != SUDEKIMP_TALOS_CARRY_END_TSA_HASH ||
        !call->exact_executable || !call->exact_asset ||
        !call->original_call_entered || !call->original_call_completed ||
        !lineage_matches(&adapter->preflight.lineage, &call->lineage))
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    adapter->end_tsa_native_completed = 1u;
    adapter->tsa_inactive_after = call->tsa_inactive_after ? 1u : 0u;
    adapter->state = SUDEKIMP_TALOS_CARRY_ADAPTER_END_TSA_OBSERVED;
    return recorded(adapter, SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_END_TSA);
}

SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveTeardown(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosCarryTeardownObservation *observation
) {
    if (adapter == NULL || observation == NULL)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID;
    if (!adapter->enabled)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED;
    if (observation->binding_hash != SUDEKIMP_TALOS_CARRY_REMOVE_ALL_HASH)
        return SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_NO_CHANGE;
    if (adapter->state != SUDEKIMP_TALOS_CARRY_ADAPTER_END_TSA_OBSERVED ||
        observation->encounter_serial != adapter->provenance.serial ||
        !observation->exact_callsite || !observation->post_native_completion)
        return invalidate(adapter,
            SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT);
    adapter->teardown_member_count = observation->member_count;
    adapter->teardown_native_completed = 1u;
    adapter->teardown_verified_empty =
        observation->verified_empty && observation->member_count == 0u;
    adapter->state = SUDEKIMP_TALOS_CARRY_ADAPTER_TEARDOWN_OBSERVED;
    return recorded(adapter, SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_TEARDOWN);
}

int SudekiMpTalosCompanionCarryAdapterGetSnapshot(
    const SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterSnapshot *snapshot
) {
    if (adapter == NULL || snapshot == NULL) return 0;
    fill_snapshot(adapter, snapshot);
    return 1;
}

int SudekiMpTalosCompanionCarryAdapterService(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterChange *change
) {
    if (adapter == NULL || change == NULL || adapter->change_count == 0u)
        return 0;
    *change = adapter->changes[adapter->change_head];
    memset(&adapter->changes[adapter->change_head], 0,
        sizeof(adapter->changes[adapter->change_head]));
    adapter->change_head = (uint8_t)((adapter->change_head + 1u) %
        SUDEKIMP_TALOS_CARRY_ADAPTER_CHANGE_CAPACITY);
    --adapter->change_count;
    return 1;
}

const char *SudekiMpTalosCarryAdapterEventName(
    SudekiMpTalosCarryAdapterEvent event
) {
    switch (event) {
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_ENABLED: return "enabled";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DISABLED: return "disabled";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_RESET: return "reset";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_LOAD_VOID: return "load_void";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_BUKI: return "delete_buki";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_AILISH:
        return "delete_ailish";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_ELCO: return "delete_elco";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_SET_ZONE: return "set_zone";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_RELEASE_POINT:
        return "release_point";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_END_TSA: return "end_tsa";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_TEARDOWN: return "teardown";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_INVALIDATED:
        return "invalidated";
    default: return "none";
    }
}

const char *SudekiMpTalosCarryAdapterStateName(
    SudekiMpTalosCarryAdapterState state
) {
    switch (state) {
    case SUDEKIMP_TALOS_CARRY_ADAPTER_DISABLED: return "disabled";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_IDLE: return "idle";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_LOAD_VOID_OBSERVED:
        return "load_void_observed";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_DELETE_SEQUENCE_OBSERVED:
        return "delete_sequence_observed";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_SET_ZONE_OBSERVED:
        return "set_zone_observed";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_RELEASE_POINT_OBSERVED:
        return "release_point_observed";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_END_TSA_OBSERVED:
        return "end_tsa_observed";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_TEARDOWN_OBSERVED:
        return "teardown_observed";
    case SUDEKIMP_TALOS_CARRY_ADAPTER_INVALIDATED: return "invalidated";
    default: return "unknown";
    }
}
