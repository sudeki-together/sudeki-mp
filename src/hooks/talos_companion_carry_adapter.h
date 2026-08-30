#ifndef SUDEKIMP_TALOS_COMPANION_CARRY_ADAPTER_H
#define SUDEKIMP_TALOS_COMPANION_CARRY_ADAPTER_H

#include "engine/talos_companion_carry.h"
#include "hooks/talos_encounter_intercept.h"

#include <stdint.h>

enum {
    SUDEKIMP_TALOS_CARRY_ADAPTER_CHANGE_CAPACITY = 16u
};

typedef enum SudekiMpTalosCarryAdapterState {
    SUDEKIMP_TALOS_CARRY_ADAPTER_DISABLED = 0,
    SUDEKIMP_TALOS_CARRY_ADAPTER_IDLE,
    SUDEKIMP_TALOS_CARRY_ADAPTER_LOAD_VOID_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_DELETE_SEQUENCE_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_SET_ZONE_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_RELEASE_POINT_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_END_TSA_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_TEARDOWN_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_INVALIDATED
} SudekiMpTalosCarryAdapterState;

typedef enum SudekiMpTalosCarryAdapterResult {
    SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_DISABLED = 0,
    SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_NO_CHANGE,
    SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_RECORDED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_INVALID,
    SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_NOT_EXACT,
    SUDEKIMP_TALOS_CARRY_ADAPTER_RESULT_REJECTED_SEQUENCE
} SudekiMpTalosCarryAdapterResult;

typedef enum SudekiMpTalosCarryAdapterEvent {
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_NONE = 0,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_ENABLED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DISABLED,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_RESET,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_LOAD_VOID,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_BUKI,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_AILISH,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_DELETE_ELCO,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_SET_ZONE,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_RELEASE_POINT,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_END_TSA,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_TEARDOWN,
    SUDEKIMP_TALOS_CARRY_ADAPTER_EVENT_INVALIDATED
} SudekiMpTalosCarryAdapterEvent;

/* These wrappers describe a native call after the observer has returned from
 * the original function. They contain no callback or continuation. A native
 * integration must call the original operation exactly once regardless of
 * the result returned by this adapter. */
typedef struct SudekiMpTalosCarryAdapterDeleteCall {
    SudekiMpTalosCarryDeleteObservation evidence;
    uint8_t original_call_entered;
    uint8_t original_call_completed;
    uint8_t reserved[2];
} SudekiMpTalosCarryAdapterDeleteCall;

typedef struct SudekiMpTalosCarryAdapterSetZoneCall {
    SudekiMpTalosCarrySetZoneObservation evidence;
    uint8_t original_call_entered;
    uint8_t original_call_completed;
    uint8_t reserved[2];
} SudekiMpTalosCarryAdapterSetZoneCall;

typedef struct SudekiMpTalosCarryAdapterEndTsaCall {
    SudekiMpTalosCarryLineage lineage;
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t logical_opcode;
    uint32_t raw_opcode;
    uint32_t binding_hash;
    uint8_t exact_executable;
    uint8_t exact_asset;
    uint8_t original_call_entered;
    uint8_t original_call_completed;
    uint8_t tsa_inactive_after;
    uint8_t reserved[3];
} SudekiMpTalosCarryAdapterEndTsaCall;

/* Pointer-free public view. Native actor, group, task, world, and input
 * identities are intentionally absent. Generations and masks are diagnostic
 * evidence only and never authorize a later operation. */
typedef struct SudekiMpTalosCarryAdapterSnapshot {
    uint32_t state;
    uint32_t last_result;
    uint32_t last_event;
    uint32_t change_serial;
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t world_generation;
    uint32_t source_generation;
    uint32_t lineage_serial;
    uint32_t runtime_generation;
    uint32_t arrival_world_generation;
    uint32_t arrival_source_generation;
    uint32_t hero_actor_generation[SUDEKIMP_TALOS_HERO_COUNT];
    uint32_t dropped_change_records;
    unsigned int teardown_member_count;
    uint8_t active_human_mask;
    uint8_t delete_native_completed_mask;
    uint8_t release_group_member_mask;
    uint8_t release_formation_member_mask;
    uint8_t release_actor_match_mask;
    uint8_t enabled;
    uint8_t exact_load_void;
    uint8_t exact_set_zone;
    uint8_t exact_release_point;
    uint8_t release_carry_ready;
    uint8_t end_tsa_native_completed;
    uint8_t tsa_inactive_after;
    uint8_t teardown_native_completed;
    uint8_t teardown_verified_empty;
    uint8_t observation_only;
    uint8_t native_passthrough_required;
    uint8_t mutation_supported;
    uint8_t reserved[3];
} SudekiMpTalosCarryAdapterSnapshot;

typedef struct SudekiMpTalosCarryAdapterChange {
    SudekiMpTalosCarryAdapterEvent event;
    SudekiMpTalosCarryAdapterSnapshot snapshot;
} SudekiMpTalosCarryAdapterChange;

typedef struct SudekiMpTalosCompanionCarryAdapter {
    SudekiMpTalosEncounterProvenance provenance;
    SudekiMpTalosCarryPreflight preflight;
    SudekiMpTalosCarryFormation release_formation;
    SudekiMpTalosCarryAdapterState state;
    SudekiMpTalosCarryAdapterResult last_result;
    SudekiMpTalosCarryAdapterEvent last_event;
    uint32_t change_serial;
    uint32_t lineage_serial;
    uint32_t arrival_world_generation;
    uint32_t arrival_source_generation;
    uint32_t dropped_change_records;
    unsigned int teardown_member_count;
    uint8_t enabled;
    uint8_t delete_cursor;
    uint8_t delete_native_completed_mask;
    uint8_t release_actor_match_mask;
    uint8_t exact_set_zone;
    uint8_t exact_release_point;
    uint8_t release_carry_ready;
    uint8_t end_tsa_native_completed;
    uint8_t tsa_inactive_after;
    uint8_t teardown_native_completed;
    uint8_t teardown_verified_empty;
    uint8_t change_head;
    uint8_t change_count;
    uint8_t reserved[2];
    SudekiMpTalosCarryAdapterChange
        changes[SUDEKIMP_TALOS_CARRY_ADAPTER_CHANGE_CAPACITY];
} SudekiMpTalosCompanionCarryAdapter;

void SudekiMpTalosCompanionCarryAdapterInitialize(
    SudekiMpTalosCompanionCarryAdapter *adapter
);

/* The adapter is disabled after initialization. Configure and Reset only
 * alter copied observer state; neither function installs/removes a hook or
 * invokes a native function. */
void SudekiMpTalosCompanionCarryAdapterConfigure(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    int enabled
);
SudekiMpTalosCarryAdapterResult SudekiMpTalosCompanionCarryAdapterReset(
    SudekiMpTalosCompanionCarryAdapter *adapter
);

SudekiMpTalosCarryAdapterResult SudekiMpTalosCompanionCarryAdapterBegin(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosCarryPreflight *preflight,
    const SudekiMpTalosLineageSnapshot *lineage
);
SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveDeletePc(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryAdapterDeleteCall *call
);
SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveSetZone(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryAdapterSetZoneCall *call,
    const SudekiMpTalosLineageSnapshot *lineage
);
SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveReleasePoint(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryFormationObservation *observation
);
SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveEndTsa(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryAdapterEndTsaCall *call
);
SudekiMpTalosCarryAdapterResult
SudekiMpTalosCompanionCarryAdapterObserveTeardown(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    const SudekiMpTalosCarryTeardownObservation *observation
);

int SudekiMpTalosCompanionCarryAdapterGetSnapshot(
    const SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterSnapshot *snapshot
);

/* Pops one pointer-free change record. Repeated service calls return zero
 * until another state/evidence change occurs, providing change-only logging
 * without giving the caller any native action or continuation token. */
int SudekiMpTalosCompanionCarryAdapterService(
    SudekiMpTalosCompanionCarryAdapter *adapter,
    SudekiMpTalosCarryAdapterChange *change
);

const char *SudekiMpTalosCarryAdapterEventName(
    SudekiMpTalosCarryAdapterEvent event
);
const char *SudekiMpTalosCarryAdapterStateName(
    SudekiMpTalosCarryAdapterState state
);

#endif
