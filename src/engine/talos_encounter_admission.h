#ifndef SUDEKIMP_TALOS_ENCOUNTER_ADMISSION_H
#define SUDEKIMP_TALOS_ENCOUNTER_ADMISSION_H

#include "engine/talos_encounter_session.h"

#include <stdint.h>

enum {
    SUDEKIMP_TALOS_NATIVE_AI_CONTROL_REFERENCE = 0u,
    SUDEKIMP_TALOS_HUMAN_CONTROL_REFERENCE = 1u,
    SUDEKIMP_TALOS_NATIVE_AI_ENABLED = 1u,
    SUDEKIMP_TALOS_HUMAN_AI_ENABLED = 0u,
    SUDEKIMP_TALOS_BOSS_AI_UNIT_TYPE = 3u
};

typedef enum SudekiMpTalosAdmissionState {
    SUDEKIMP_TALOS_ADMISSION_IDLE = 0,
    SUDEKIMP_TALOS_ADMISSION_WAITING,
    SUDEKIMP_TALOS_ADMISSION_ADMITTED,
    SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIMED,
    SUDEKIMP_TALOS_ADMISSION_ACTIVE,
    SUDEKIMP_TALOS_ADMISSION_QUARANTINED
} SudekiMpTalosAdmissionState;

typedef enum SudekiMpTalosAdmissionResult {
    SUDEKIMP_TALOS_ADMISSION_NO_CHANGE = 0,
    SUDEKIMP_TALOS_ADMISSION_STARTED,
    SUDEKIMP_TALOS_ADMISSION_NOT_READY,
    SUDEKIMP_TALOS_ADMISSION_ACCEPTED,
    SUDEKIMP_TALOS_ADMISSION_HEALTH_CLAIM_ACCEPTED,
    SUDEKIMP_TALOS_ADMISSION_HEALTH_VERIFIED,
    SUDEKIMP_TALOS_ADMISSION_RELEASED,
    SUDEKIMP_TALOS_ADMISSION_REJECTED_INVALID,
    SUDEKIMP_TALOS_ADMISSION_REJECTED_BUSY,
    SUDEKIMP_TALOS_ADMISSION_REJECTED_STALE,
    SUDEKIMP_TALOS_ADMISSION_REJECTED_MISMATCH,
    SUDEKIMP_TALOS_ADMISSION_REJECTED_REPLAY,
    SUDEKIMP_TALOS_ADMISSION_REJECTED_QUARANTINED
} SudekiMpTalosAdmissionResult;

typedef enum SudekiMpTalosAdmissionFailure {
    SUDEKIMP_TALOS_ADMISSION_FAILURE_NONE = 0,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_PROVENANCE,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_ARRIVAL_LINEAGE,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_GROUP_MEMBERSHIP,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_HERO_IDENTITY,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_CONTROL_OWNERSHIP,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_AI_TARGETING,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_IDENTITY,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_HEALTH,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_BOSS_PRESENTATION,
    SUDEKIMP_TALOS_ADMISSION_FAILURE_HEALTH_COMMIT
} SudekiMpTalosAdmissionFailure;

typedef enum SudekiMpTalosHealthAction {
    SUDEKIMP_TALOS_HEALTH_ACTION_NONE = 0,
    SUDEKIMP_TALOS_HEALTH_ACTION_SCALE_FROM_VANILLA,
    SUDEKIMP_TALOS_HEALTH_ACTION_VERIFY_EXISTING_TARGET
} SudekiMpTalosHealthAction;

/* All identities are observation tokens. The pure coordinator never follows
 * or owns them. resource_actor_identity is the exact stable-hero GetPC result;
 * group_occurrences comes from the native four-slot active-group snapshot.
 * Each actor/generation must equal the same hero-indexed tuple captured in the
 * immutable pre-transition encounter provenance. */
typedef struct SudekiMpTalosHeroObservation {
    uintptr_t actor_identity;
    uintptr_t resource_actor_identity;
    uint32_t actor_generation;
    uint32_t group_occurrences;
    uintptr_t ai_component_identity;
    uintptr_t ai_owner_identity;
    uintptr_t ai_mode_state_identity;
    int32_t control_reference;
    uint8_t ai_enabled;
    uint8_t input_owner_seat;
    uintptr_t lease_actor_identity;
    uint32_t lease_actor_generation;
    uintptr_t input_identity;
    uint32_t input_generation;
    uintptr_t targeter_identity;
    uintptr_t current_target_actor_identity;
    int ally_target_category_enabled;
    int current_target_observed;
    int current_target_is_talos_encounter_threat;
    int identity_observed;
    int control_observed;
    int targeting_observed;
} SudekiMpTalosHeroObservation;

/* The supported image's exact boss seam is BOSS_Talos -> embedded CCombat
 * owner -> CStats, plus the native boss-bar entity binding. The callback and
 * candidate-filter booleans mean their exact entry images were independently
 * verified by the integration layer; they are not inferred by this policy.
 * Companion target identity is deliberately not pinned to BOSS_Talos: native
 * AI may defend itself by selecting either the real entity or a verified clone. */
typedef struct SudekiMpTalosBossObservation {
    uintptr_t actor_identity;
    uintptr_t resource_actor_identity;
    uint32_t actor_generation;
    uintptr_t ai_component_identity;
    uintptr_t ai_owner_identity;
    uint32_t ai_unit_type;
    uintptr_t combat_identity;
    uintptr_t combat_owner_identity;
    uintptr_t combat_data_identity;
    uintptr_t boss_bar_identity;
    uintptr_t boss_bar_entity_identity;
    uintptr_t stat_display_identity;
    float current_hp;
    float maximum_hp;
    int health_storage_writable;
    int native_health_callback_exact;
    int boss_candidate_filter_exact;
    int identity_observed;
    int health_observed;
    int presentation_observed;
    int candidate_filter_observed;
} SudekiMpTalosBossObservation;

typedef struct SudekiMpTalosAdmissionObservation {
    uint32_t encounter_serial;
    uint32_t request_transition_serial;
    uint32_t request_world_generation;
    uint32_t request_source_generation;
    uintptr_t request_host_actor;
    uint32_t request_host_actor_generation;
    uint32_t request_host_lease_generation;
    uint32_t arrival_world_generation;
    uint32_t arrival_source_generation;
    uint8_t active_human_mask;
    uint8_t group_count;
    int arrival_ready;
    int group_observed;
    SudekiMpTalosHeroObservation heroes[SUDEKIMP_TALOS_HERO_COUNT];
    SudekiMpTalosBossObservation boss;
} SudekiMpTalosAdmissionObservation;

/* This is the only authorization to touch Talos HP. It binds the exact boss,
 * combat data, presentation objects, encounter serial, before values, and
 * ratio-preserving target. A ticket can be finalized only once. */
typedef struct SudekiMpTalosHealthTicket {
    uint32_t encounter_serial;
    SudekiMpTalosHealthAction action;
    uintptr_t boss_actor_identity;
    uint32_t boss_actor_generation;
    uintptr_t boss_ai_component_identity;
    uintptr_t combat_identity;
    uintptr_t combat_data_identity;
    uintptr_t boss_bar_identity;
    uintptr_t boss_bar_entity_identity;
    uintptr_t stat_display_identity;
    float before_current_hp;
    float before_maximum_hp;
    float target_current_hp;
    float target_maximum_hp;
} SudekiMpTalosHealthTicket;

typedef struct SudekiMpTalosAdmission {
    SudekiMpTalosAdmissionState state;
    SudekiMpTalosAdmissionFailure failure;
    SudekiMpTalosEncounterProvenance provenance;
    SudekiMpTalosAdmissionObservation admitted_observation;
    SudekiMpTalosHealthTicket health_ticket;
    uint32_t claimed_health_serial;
    uint32_t highest_terminal_serial;
} SudekiMpTalosAdmission;

void SudekiMpTalosAdmissionInitialize(SudekiMpTalosAdmission *admission);

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionBegin(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosEncounterProvenance *provenance
);

/* Missing arrival, group, AI, boss, or presentation observations return
 * NOT_READY and make no state change. A complete contradictory same-serial
 * image quarantines instead of guessing. */
SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionObserve(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosAdmissionObservation *observation
);

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionClaimHealth(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosAdmissionObservation *current,
    SudekiMpTalosHealthTicket *ticket
);

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionFinalizeHealth(
    SudekiMpTalosAdmission *admission,
    const SudekiMpTalosHealthTicket *ticket,
    const SudekiMpTalosBossObservation *after
);

SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionRelease(
    SudekiMpTalosAdmission *admission,
    uint32_t encounter_serial
);

/* Release a WAITING or ADMITTED transaction before any HP ticket has been
 * claimed. The integration layer calls this after its bounded arrival/lifecycle
 * deadline so vanilla can continue without retaining mod input/camera/actor
 * ownership. Once a health mutation is possible, abandonment is rejected. */
SudekiMpTalosAdmissionResult SudekiMpTalosAdmissionAbandonUncommitted(
    SudekiMpTalosAdmission *admission,
    uint32_t encounter_serial,
    SudekiMpTalosAdmissionFailure failure
);

void SudekiMpTalosAdmissionQuarantine(
    SudekiMpTalosAdmission *admission,
    SudekiMpTalosAdmissionFailure failure
);

int SudekiMpTalosAdmissionGetHealthTicket(
    const SudekiMpTalosAdmission *admission,
    SudekiMpTalosHealthTicket *ticket
);

#endif
