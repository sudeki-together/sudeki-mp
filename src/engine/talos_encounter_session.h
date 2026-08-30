#ifndef SUDEKIMP_TALOS_ENCOUNTER_SESSION_H
#define SUDEKIMP_TALOS_ENCOUNTER_SESSION_H

#include <stdint.h>

enum {
    SUDEKIMP_TALOS_HERO_COUNT = 4u,
    SUDEKIMP_TALOS_SEAT_COUNT = 4u,
    SUDEKIMP_TALOS_HUMAN_MASK = 0x0fu,
    SUDEKIMP_TALOS_BASE_HEALTH = 45000u,
    SUDEKIMP_TALOS_EXPANDED_COMBATANTS = 4u,
    SUDEKIMP_TALOS_HERO_NONE = 0xffu,
    SUDEKIMP_TALOS_SEAT_AI = 0xffu,
    SUDEKIMP_TALOS_CONTROLLER_NONE = 0xffu
};

typedef enum SudekiMpTalosHero {
    SUDEKIMP_TALOS_HERO_TAL = 0,
    SUDEKIMP_TALOS_HERO_AILISH = 1,
    SUDEKIMP_TALOS_HERO_BUKI = 2,
    SUDEKIMP_TALOS_HERO_ELCO = 3
} SudekiMpTalosHero;

typedef enum SudekiMpTalosEncounterState {
    SUDEKIMP_TALOS_ENCOUNTER_IDLE = 0,
    SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPEN,
    SUDEKIMP_TALOS_ENCOUNTER_CONFIRMED,
    SUDEKIMP_TALOS_ENCOUNTER_CLAIMED,
    SUDEKIMP_TALOS_ENCOUNTER_CANCELLED,
    SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED
} SudekiMpTalosEncounterState;

typedef enum SudekiMpTalosEncounterResult {
    SUDEKIMP_TALOS_ENCOUNTER_NO_CHANGE = 0,
    SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED,
    SUDEKIMP_TALOS_ENCOUNTER_CONFIRM_ACCEPTED,
    SUDEKIMP_TALOS_ENCOUNTER_CANCEL_ACCEPTED,
    SUDEKIMP_TALOS_ENCOUNTER_CLAIM_ACCEPTED,
    SUDEKIMP_TALOS_ENCOUNTER_FINISHED,
    SUDEKIMP_TALOS_ENCOUNTER_RECOVERED,
    SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID,
    SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY,
    SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE,
    SUDEKIMP_TALOS_ENCOUNTER_REJECTED_MISMATCH,
    SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY,
    SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED
} SudekiMpTalosEncounterResult;

typedef enum SudekiMpTalosEncounterQuarantineReason {
    SUDEKIMP_TALOS_QUARANTINE_NONE = 0,
    SUDEKIMP_TALOS_QUARANTINE_PROVENANCE_MISMATCH,
    SUDEKIMP_TALOS_QUARANTINE_NATIVE_LIFECYCLE_UNCERTAIN,
    SUDEKIMP_TALOS_QUARANTINE_TRANSITION_FAILED
} SudekiMpTalosEncounterQuarantineReason;

/* hero_actor_* is indexed by stable hero ID and captures the four existing
 * engine-owned actors before the transition. Every tuple must be nonzero and
 * distinct; Tal must equal the host tuple. hero_by_seat maps those heroes to
 * human seats: seat zero must be Tal, every occupied P2-P4 seat must contain
 * one unique companion, and inactive seats must contain HERO_NONE. Unassigned
 * companions remain native AI. */
typedef struct SudekiMpTalosEncounterRequest {
    uint32_t transition_serial;
    uint32_t world_generation;
    uint32_t source_generation;
    uintptr_t host_actor;
    uint32_t host_actor_generation;
    uint32_t host_lease_generation;
    uintptr_t hero_actor_identity[SUDEKIMP_TALOS_HERO_COUNT];
    uint32_t hero_actor_generation[SUDEKIMP_TALOS_HERO_COUNT];
    uint8_t active_human_mask;
    uint8_t hero_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uint8_t controller_slot_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uintptr_t input_identity_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uint32_t input_generation_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uint8_t combatant_count;
} SudekiMpTalosEncounterRequest;

/* This record is copied when the prompt opens and is thereafter immutable.
 * Callers must present an exact copy at confirmation and immediately before
 * claiming permission to invoke the one saved native transition. */
typedef struct SudekiMpTalosEncounterProvenance {
    uint32_t serial;
    uint32_t transition_serial;
    uint32_t world_generation;
    uint32_t source_generation;
    uintptr_t host_actor;
    uint32_t host_actor_generation;
    uint32_t host_lease_generation;
    uint32_t talos_health_target;
    uintptr_t hero_actor_identity[SUDEKIMP_TALOS_HERO_COUNT];
    uint32_t hero_actor_generation[SUDEKIMP_TALOS_HERO_COUNT];
    uint8_t active_human_mask;
    uint8_t hero_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uint8_t controller_slot_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uintptr_t input_identity_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uint32_t input_generation_by_seat[SUDEKIMP_TALOS_SEAT_COUNT];
    uint8_t combatant_count;
} SudekiMpTalosEncounterProvenance;

typedef struct SudekiMpTalosEncounterSession {
    SudekiMpTalosEncounterState state;
    SudekiMpTalosEncounterQuarantineReason quarantine_reason;
    SudekiMpTalosEncounterProvenance provenance;
    uint32_t next_serial;
    uint32_t claimed_serial;
    uint32_t highest_terminal_transition_serial;
    uint32_t terminal_world_generation;
    uint32_t terminal_source_generation;
} SudekiMpTalosEncounterSession;

void SudekiMpTalosEncounterInitialize(
    SudekiMpTalosEncounterSession *session
);

SudekiMpTalosEncounterResult SudekiMpTalosEncounterOpenPrompt(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterRequest *request
);

SudekiMpTalosEncounterResult SudekiMpTalosEncounterConfirm(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterProvenance *visible_prompt
);

SudekiMpTalosEncounterResult SudekiMpTalosEncounterCancel(
    SudekiMpTalosEncounterSession *session,
    uint32_t serial
);

/* Only one exact claim can succeed. A successful claim is the permission
 * boundary for the integration layer to invoke the saved native transition. */
SudekiMpTalosEncounterResult SudekiMpTalosEncounterClaimTransition(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterProvenance *current
);

/* Call after the claimed native transition returns. A failed/uncertain call
 * quarantines the coordinator until a proven generation boundary. */
SudekiMpTalosEncounterResult SudekiMpTalosEncounterFinishTransition(
    SudekiMpTalosEncounterSession *session,
    uint32_t serial,
    int native_transition_succeeded
);

/* A cancelled prompt may be dismissed back to idle without losing serial
 * history. Claimed or quarantined work cannot be reset through this API. */
int SudekiMpTalosEncounterDismissCancelled(
    SudekiMpTalosEncounterSession *session,
    uint32_t serial
);

void SudekiMpTalosEncounterQuarantine(
    SudekiMpTalosEncounterSession *session,
    SudekiMpTalosEncounterQuarantineReason reason
);

/* Recovery requires a new, nonzero world or source generation. This prevents
 * retrying an uncertain native lifecycle in the same world transaction. */
SudekiMpTalosEncounterResult SudekiMpTalosEncounterRecover(
    SudekiMpTalosEncounterSession *session,
    uint32_t world_generation,
    uint32_t source_generation
);

int SudekiMpTalosEncounterGetProvenance(
    const SudekiMpTalosEncounterSession *session,
    SudekiMpTalosEncounterProvenance *provenance
);

uint32_t SudekiMpTalosEncounterHealthTarget(
    unsigned int combatant_count
);

unsigned int SudekiMpTalosEncounterHumanCount(uint8_t active_human_mask);

unsigned int SudekiMpTalosEncounterSeatForHero(
    const SudekiMpTalosEncounterProvenance *provenance,
    SudekiMpTalosHero hero
);

#endif
