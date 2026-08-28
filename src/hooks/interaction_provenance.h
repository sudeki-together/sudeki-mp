#ifndef SUDEKIMP_INTERACTION_PROVENANCE_H
#define SUDEKIMP_INTERACTION_PROVENANCE_H

#include <windows.h>
#include <stdint.h>

enum {
    SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT = 2u,
    SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT = 15u,
    SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT = 16u,
    SUDEKIMP_INTERACTION_PROVENANCE_SOL_LIFETIME_MS = 10000u
};

typedef enum SudekiMpInteractionCandidateStatus {
    SUDEKIMP_INTERACTION_CANDIDATE_UNOBSERVED = 0,
    SUDEKIMP_INTERACTION_CANDIDATE_SEEN,
    SUDEKIMP_INTERACTION_CANDIDATE_ACCEPTED_UNVALIDATED,
    SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED
} SudekiMpInteractionCandidateStatus;

/* Pointer values are identities only. They are never dereferenced by a
 * consumer and become unusable authority as soon as either generation
 * changes. target_owner is the intrusive owner passed to Sudeki's native
 * interaction message constructor; target is the candidate's script object. */
typedef struct SudekiMpInteractionCandidateObservation {
    uintptr_t target_owner;
    uintptr_t target;
    uint32_t event_type;
    uint8_t initial_auxiliary_flag;
    uint8_t initial_rejected_flag;
    uint8_t reserved[2];
    SudekiMpInteractionCandidateStatus status;
} SudekiMpInteractionCandidateObservation;

typedef struct SudekiMpInteractionSeatObservation {
    uint32_t serial;
    uint32_t player_index;
    uintptr_t source_actor;
    uint32_t actor_generation;
    uint32_t source_generation;
    uint32_t native_candidate_count;
    uint32_t candidate_count;
    int source_is_native_front;
    int overflowed;
    int identity_ambiguous;
    int completed;
    SudekiMpInteractionCandidateObservation
        candidates[SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT];
} SudekiMpInteractionSeatObservation;

typedef struct SudekiMpSolInteractionProvenance {
    uint32_t serial;
    uint32_t player_index;
    uintptr_t source_actor;
    uint32_t actor_generation;
    uint32_t source_generation;
    uintptr_t target_owner;
    uintptr_t target;
    uint32_t event_resource_flags;
    uintptr_t event_resource_storage;
    uint32_t event_type;
    uintptr_t task_handle;
    uintptr_t sol_thread;
    uint32_t native_thread_id;
    uint32_t observed_at_ms;
    int source_is_native_front;
} SudekiMpSolInteractionProvenance;

/* The loader's exact SHA-256/PE gate remains a prerequisite. This installer
 * adds three local callsite signatures and is disabled unless explicitly
 * requested. Every hook observes and then calls the original native path;
 * it never builds, selects, validates, or submits a candidate itself. */
BOOL SudekiMpInstallInteractionProvenance(
    HMODULE game_module,
    BOOL enabled
);
void SudekiMpUninstallInteractionProvenance(void);

/* A zero generation deliberately makes every observation non-authoritative.
 * Changing it invalidates all retained actor, candidate, and SOL identities.
 * A future always-on world lifecycle observer must publish this value before
 * any P2 request seam can be enabled. */
void SudekiMpInteractionProvenanceSetSourceGeneration(uint32_t generation);
void SudekiMpInteractionProvenanceInvalidate(void);
void SudekiMpInteractionProvenanceInvalidateSolThread(uintptr_t sol_thread);

/* Runs Sudeki's existing read-only nearby-collision query around one live
 * player lease and logs bounded entity identities.  It deliberately does not
 * select, validate, enqueue, or dispatch an interaction.  This is the first
 * actor-local discovery seam because the native CUsable contact path only
 * scans the front character. */
BOOL SudekiMpInteractionProvenanceProbeActorLocalNearby(
    uint32_t player_index
);

BOOL SudekiMpInteractionProvenanceGetSeat(
    uint32_t player_index,
    SudekiMpInteractionSeatObservation *observation
);
BOOL SudekiMpInteractionProvenanceFindSolThread(
    uintptr_t sol_thread,
    uint32_t now_ms,
    SudekiMpSolInteractionProvenance *provenance
);

/* These predicates intentionally fail closed on P2's current path: native
 * 0x40D7A0 skips 0x40D9A0 whenever source_is_native_front is false. Merely
 * reaching message construction is therefore recorded as UNVALIDATED and
 * can never authorize activation. */
BOOL SudekiMpInteractionCandidateAuthorityProven(
    const SudekiMpInteractionSeatObservation *observation,
    uint32_t candidate_index
);
BOOL SudekiMpSolInteractionAuthorityProven(
    const SudekiMpSolInteractionProvenance *provenance,
    uint32_t now_ms
);

/* Pointer-free observation inputs are also the deterministic unit-test seam.
 * Runtime wrappers populate them from the bounded exact native layout. */
BOOL SudekiMpInteractionProvenanceObserveDispatchBegin(
    uintptr_t source_actor,
    int source_is_native_front,
    const SudekiMpInteractionCandidateObservation *candidates,
    uint32_t native_candidate_count
);
void SudekiMpInteractionProvenanceObserveAcceptedCandidate(
    uintptr_t source_actor,
    uintptr_t target_owner,
    uintptr_t target,
    uint32_t event_type
);
void SudekiMpInteractionProvenanceObserveDispatchEnd(
    uintptr_t source_actor
);
BOOL SudekiMpInteractionProvenanceObserveSolSubmission(
    const SudekiMpSolInteractionProvenance *provenance
);

#endif
