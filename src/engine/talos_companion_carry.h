#ifndef SUDEKIMP_TALOS_COMPANION_CARRY_H
#define SUDEKIMP_TALOS_COMPANION_CARRY_H

#include "engine/talos_encounter_session.h"

#include <stdint.h>

enum {
    SUDEKIMP_TALOS_CARRY_RESOURCE_TAL = 2u,
    SUDEKIMP_TALOS_CARRY_RESOURCE_AILISH = 3u,
    SUDEKIMP_TALOS_CARRY_RESOURCE_BUKI = 4u,
    SUDEKIMP_TALOS_CARRY_RESOURCE_ELCO = 5u,
    SUDEKIMP_TALOS_CARRY_SOURCE_START = 0x21b76u,
    SUDEKIMP_TALOS_CARRY_SOURCE_OPCODE = 0x21c0cu,
    SUDEKIMP_TALOS_CARRY_LOAD_VOID_START = 0x218f3u,
    SUDEKIMP_TALOS_CARRY_BUKI_LOGICAL = 0x2194du,
    SUDEKIMP_TALOS_CARRY_BUKI_RAW = 0x497b9u,
    SUDEKIMP_TALOS_CARRY_AILISH_LOGICAL = 0x21958u,
    SUDEKIMP_TALOS_CARRY_AILISH_RAW = 0x497c4u,
    SUDEKIMP_TALOS_CARRY_ELCO_LOGICAL = 0x21963u,
    SUDEKIMP_TALOS_CARRY_ELCO_RAW = 0x497cfu,
    SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL = 0x2196eu,
    SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW = 0x497dau,
    SUDEKIMP_TALOS_CARRY_END_TSA_LOGICAL = 0x21adcu,
    SUDEKIMP_TALOS_CARRY_END_TSA_RAW = 0x49948u,
    SUDEKIMP_TALOS_CARRY_FORMATION_POP_RVA = 0x0f6260u
};

#define SUDEKIMP_TALOS_CARRY_SOURCE_HASH UINT32_C(0xfac73f18)
#define SUDEKIMP_TALOS_CARRY_LOAD_VOID_HASH UINT32_C(0x70f470c2)
#define SUDEKIMP_TALOS_CARRY_DELETE_PC_HASH UINT32_C(0xfa7ec379)
#define SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH UINT32_C(0x76fc7114)
#define SUDEKIMP_TALOS_CARRY_END_TSA_HASH UINT32_C(0x343b1e0c)
#define SUDEKIMP_TALOS_CARRY_REMOVE_ALL_HASH UINT32_C(0xc1366076)

typedef enum SudekiMpTalosCarryState {
    SUDEKIMP_TALOS_CARRY_DISABLED = 0,
    SUDEKIMP_TALOS_CARRY_IDLE,
    SUDEKIMP_TALOS_CARRY_ARMED,
    SUDEKIMP_TALOS_CARRY_PARTIAL_SKIP,
    SUDEKIMP_TALOS_CARRY_PRESERVED,
    SUDEKIMP_TALOS_CARRY_SET_ZONE_PASSED,
    SUDEKIMP_TALOS_CARRY_FORMATION_CLAIMED,
    SUDEKIMP_TALOS_CARRY_ACTIVE,
    SUDEKIMP_TALOS_CARRY_RELEASED,
    SUDEKIMP_TALOS_CARRY_ABORTED,
    SUDEKIMP_TALOS_CARRY_QUARANTINED
} SudekiMpTalosCarryState;

typedef enum SudekiMpTalosCarryResult {
    SUDEKIMP_TALOS_CARRY_DISABLED_RESULT = 0,
    SUDEKIMP_TALOS_CARRY_NO_CHANGE,
    SUDEKIMP_TALOS_CARRY_STARTED,
    SUDEKIMP_TALOS_CARRY_FORMATION_AUTHORIZED,
    SUDEKIMP_TALOS_CARRY_FORMATION_COMMITTED,
    SUDEKIMP_TALOS_CARRY_TEARDOWN_OBSERVED,
    SUDEKIMP_TALOS_CARRY_ABORT_TO_VANILLA,
    SUDEKIMP_TALOS_CARRY_REJECTED_INVALID,
    SUDEKIMP_TALOS_CARRY_REJECTED_STATE,
    SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY,
    SUDEKIMP_TALOS_CARRY_QUARANTINE
} SudekiMpTalosCarryResult;

typedef enum SudekiMpTalosCarryDeleteAction {
    SUDEKIMP_TALOS_CARRY_DELETE_PASS_NATIVE = 0,
    SUDEKIMP_TALOS_CARRY_DELETE_SKIP_NATIVE,
    SUDEKIMP_TALOS_CARRY_DELETE_ABORT_TO_VANILLA,
    SUDEKIMP_TALOS_CARRY_DELETE_QUARANTINE
} SudekiMpTalosCarryDeleteAction;

typedef enum SudekiMpTalosCarrySetZoneAction {
    SUDEKIMP_TALOS_CARRY_SET_ZONE_PASS_NATIVE = 0,
    SUDEKIMP_TALOS_CARRY_SET_ZONE_ALLOW_NATIVE_ONCE,
    SUDEKIMP_TALOS_CARRY_SET_ZONE_BLOCK_REPLAY,
    SUDEKIMP_TALOS_CARRY_SET_ZONE_QUARANTINE
} SudekiMpTalosCarrySetZoneAction;

/* uintptr_t fields are opaque identity tokens. This module only copies and
 * compares them; it never follows a pointer or invokes a native function. */
typedef struct SudekiMpTalosCarryLineage {
    uintptr_t source_task;
    uintptr_t load_void_task;
    uint32_t source_task_generation;
    uint32_t load_void_task_generation;
    uint32_t runtime_generation;
    uint32_t source_hash;
    uint32_t source_start;
    uint32_t source_opcode;
    uint32_t load_void_hash;
    uint32_t load_void_start;
    uint8_t descendant_proven;
    uint8_t reserved[3];
} SudekiMpTalosCarryLineage;

typedef struct SudekiMpTalosCarryFormation {
    uintptr_t group_identity;
    uintptr_t formation_owner_identity;
    uintptr_t formation_identity;
    uint32_t group_generation;
    uint32_t formation_owner_generation;
    uint32_t formation_generation;
    uintptr_t actor_identity[SUDEKIMP_TALOS_HERO_COUNT];
    uint32_t actor_generation[SUDEKIMP_TALOS_HERO_COUNT];
    uint8_t group_member_mask;
    uint8_t formation_member_mask;
    uint8_t observed;
    uint8_t reserved;
} SudekiMpTalosCarryFormation;

typedef struct SudekiMpTalosCarryPreflight {
    SudekiMpTalosCarryLineage lineage;
    SudekiMpTalosCarryFormation formation;
    uint8_t exact_executable;
    uint8_t exact_asset;
    uint8_t host_authority;
    uint8_t reserved;
} SudekiMpTalosCarryPreflight;

typedef struct SudekiMpTalosCarryDeleteObservation {
    SudekiMpTalosCarryLineage lineage;
    uint32_t logical_opcode;
    uint32_t raw_opcode;
    uint32_t binding_hash;
    uint8_t resource_id;
    uint8_t exact_executable;
    uint8_t exact_asset;
    uint8_t reserved;
} SudekiMpTalosCarryDeleteObservation;

typedef struct SudekiMpTalosCarrySetZoneObservation {
    SudekiMpTalosCarryLineage lineage;
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t logical_opcode;
    uint32_t raw_opcode;
    uint32_t binding_hash;
    uint8_t exact_executable;
    uint8_t exact_asset;
    uint8_t reserved[2];
    char destination[16];
} SudekiMpTalosCarrySetZoneObservation;

typedef struct SudekiMpTalosCarrySetZoneTicket {
    SudekiMpTalosCarryLineage lineage;
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t authorization_serial;
} SudekiMpTalosCarrySetZoneTicket;

typedef struct SudekiMpTalosCarryFormationObservation {
    SudekiMpTalosCarryFormation formation;
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t arrival_world_generation;
    uint32_t arrival_source_generation;
    uint32_t set_zone_logical;
    uint32_t set_zone_raw;
    uint32_t set_zone_hash;
    uint32_t end_tsa_logical;
    uint32_t end_tsa_raw;
    uint32_t end_tsa_hash;
    uint32_t set_zone_authorization_serial;
    uint8_t arrival_settled;
    uint8_t exact_release_point;
    uint8_t tsa_active;
    uint8_t tal_final_pop_settled;
    uint8_t item_use_settled;
    uint8_t boss_ready;
    uint8_t no_pending_removal;
} SudekiMpTalosCarryFormationObservation;

typedef struct SudekiMpTalosCarryFormationTicket {
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t arrival_world_generation;
    uint32_t arrival_source_generation;
    uint32_t authorization_serial;
    uint32_t native_function_rva;
    uintptr_t group_identity;
    uintptr_t formation_owner_identity;
    uintptr_t formation_identity;
    uint32_t group_generation;
    uint32_t formation_owner_generation;
    uint32_t formation_generation;
    uintptr_t actor_identity[SUDEKIMP_TALOS_HERO_COUNT];
    uint32_t actor_generation[SUDEKIMP_TALOS_HERO_COUNT];
} SudekiMpTalosCarryFormationTicket;

typedef struct SudekiMpTalosCarryFormationCompletion {
    SudekiMpTalosCarryFormation formation;
    uint32_t encounter_serial;
    uint32_t authorization_serial;
    uint32_t arrival_world_generation;
    uint32_t arrival_source_generation;
    uint8_t native_call_completed;
    uint8_t placement_verified;
    uint8_t no_pending_removal;
    uint8_t reserved;
} SudekiMpTalosCarryFormationCompletion;

typedef struct SudekiMpTalosCarryTeardownObservation {
    uint32_t encounter_serial;
    uint32_t binding_hash;
    unsigned int member_count;
    uint8_t exact_callsite;
    uint8_t post_native_completion;
    uint8_t verified_empty;
    uint8_t reserved;
} SudekiMpTalosCarryTeardownObservation;

typedef struct SudekiMpTalosCompanionCarry {
    SudekiMpTalosEncounterProvenance provenance;
    SudekiMpTalosCarryPreflight preflight;
    SudekiMpTalosCarrySetZoneTicket set_zone_ticket;
    SudekiMpTalosCarryFormationTicket ticket;
    SudekiMpTalosCarryState state;
    uint32_t next_authorization_serial;
    uint32_t highest_terminal_encounter_serial;
    uint32_t terminal_transition_serial;
    uint32_t terminal_world_generation;
    uint32_t terminal_source_generation;
    uint8_t enabled;
    uint8_t delete_cursor;
    uint8_t skipped_mask;
    uint8_t reserved;
} SudekiMpTalosCompanionCarry;

void SudekiMpTalosCompanionCarryInitialize(SudekiMpTalosCompanionCarry *carry);
void SudekiMpTalosCompanionCarryConfigure(
    SudekiMpTalosCompanionCarry *carry,
    int enabled
);
SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryBegin(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosCarryPreflight *preflight
);
SudekiMpTalosCarryDeleteAction SudekiMpTalosCompanionCarryObserveDelete(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryDeleteObservation *observation
);
SudekiMpTalosCarrySetZoneAction SudekiMpTalosCompanionCarryObserveSetZone(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarrySetZoneObservation *observation,
    SudekiMpTalosCarrySetZoneTicket *ticket
);
SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryClaimFormation(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryFormationObservation *observation,
    SudekiMpTalosCarryFormationTicket *ticket
);
SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryFinishFormation(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosCarryFormationTicket *ticket,
    const SudekiMpTalosCarryFormationCompletion *completion
);
SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryObserveTeardown(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosCarryTeardownObservation *observation
);

#endif
