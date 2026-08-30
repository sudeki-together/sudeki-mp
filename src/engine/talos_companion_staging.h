#ifndef SUDEKIMP_TALOS_COMPANION_STAGING_H
#define SUDEKIMP_TALOS_COMPANION_STAGING_H

#include "engine/talos_encounter_session.h"

#include <stdint.h>

enum {
    SUDEKIMP_TALOS_STAGING_REMOVE_PLAYER_RVA = 0x00023390u,
    SUDEKIMP_TALOS_STAGING_ADD_PLAYER_RVA = 0x00023230u,
    SUDEKIMP_TALOS_STAGING_MEMBER_NONE = 0xffu,
    SUDEKIMP_TALOS_STAGING_CONTROL_NATIVE_AI = 1u,
    SUDEKIMP_TALOS_STAGING_CONTROL_HUMAN = 2u
};

typedef enum SudekiMpTalosCompanionStagingState {
    SUDEKIMP_TALOS_STAGING_DISABLED = 0,
    SUDEKIMP_TALOS_STAGING_IDLE,
    SUDEKIMP_TALOS_STAGING_PREFLIGHT_PROVEN,
    SUDEKIMP_TALOS_STAGING_REMOVE_TICKET_ISSUED,
    SUDEKIMP_TALOS_STAGING_DETACHED_PROVEN,
    SUDEKIMP_TALOS_STAGING_RESTORE_TICKET_ISSUED,
    SUDEKIMP_TALOS_STAGING_RESTORED_PROVEN,
    SUDEKIMP_TALOS_STAGING_STABILITY_PROVEN,
    SUDEKIMP_TALOS_STAGING_RELEASED,
    SUDEKIMP_TALOS_STAGING_QUARANTINED
} SudekiMpTalosCompanionStagingState;

typedef enum SudekiMpTalosCompanionStagingResult {
    SUDEKIMP_TALOS_STAGING_DISABLED_RESULT = 0,
    SUDEKIMP_TALOS_STAGING_NO_CHANGE,
    SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED,
    SUDEKIMP_TALOS_STAGING_REMOVE_AUTHORIZED,
    SUDEKIMP_TALOS_STAGING_DETACH_PROVEN,
    SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED,
    SUDEKIMP_TALOS_STAGING_RESTORE_PROVEN,
    SUDEKIMP_TALOS_STAGING_STABILITY_ACCEPTED,
    SUDEKIMP_TALOS_STAGING_RELEASE_ACCEPTED,
    SUDEKIMP_TALOS_STAGING_SAFE_ABORT,
    SUDEKIMP_TALOS_STAGING_REJECTED_INVALID,
    SUDEKIMP_TALOS_STAGING_REJECTED_STATE,
    SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY,
    SUDEKIMP_TALOS_STAGING_QUARANTINE
} SudekiMpTalosCompanionStagingResult;

/* Every uintptr_t is an opaque identity token. The coordinator only copies
 * and compares these values. It never dereferences them or invokes a native
 * function. Records are indexed by SudekiMpTalosHero. Only Elco's wrapper
 * tuple is nonzero: the future adapter must deliberately hold that one native
 * wrapper across the synchronous remove/restore transaction. Independent
 * GetPC results are fresh wrappers and are not treated as stable identities;
 * the other heroes are proven by their stable actor/control tuples instead. */
typedef struct SudekiMpTalosStagingHeroRecord {
    uintptr_t wrapper_identity;
    uint32_t wrapper_generation;
    uintptr_t actor_identity;
    uint32_t actor_generation;
    /* Exact actor+0x94 AI-component +0x40 formation backpointer. Native
     * formation removal clears it; native formation add restores it. This is
     * deliberately distinct from the AI owner pointer at component +0x10. */
    uintptr_t formation_backpointer_identity;
    uint32_t formation_backpointer_generation;
    /* Character+0x94 AI/control component identity. A native adapter must
     * separately prove component+0x10 still owns this record's actor. */
    uintptr_t control_owner_identity;
    uint32_t control_owner_generation;
    uint8_t native_ai_enabled;
    uint8_t human_control_owned;
    /* Zero for Tal/front and native AI; one for a non-front human companion
     * whose existing AiOverrideControl lease must survive staging unchanged. */
    uint8_t override_active;
    uint8_t control_mode;
} SudekiMpTalosStagingHeroRecord;

/* group_order and formation_order contain stable hero IDs followed by
 * MEMBER_NONE up to four entries. The two orders are independent. */
typedef struct SudekiMpTalosStagingSnapshot {
    uint64_t observation_serial;
    uint64_t continuity_fingerprint;
    uint32_t world_generation;
    uint32_t source_generation;
    uint32_t runtime_generation;
    uintptr_t native_thread_identity;
    uintptr_t group_identity;
    uint32_t group_generation;
    uintptr_t formation_owner_identity;
    uint32_t formation_owner_generation;
    uintptr_t formation_identity;
    uint32_t formation_generation;
    SudekiMpTalosStagingHeroRecord hero[SUDEKIMP_TALOS_HERO_COUNT];
    uintptr_t tal_controller_identity;
    uint32_t tal_controller_generation;
    uintptr_t front_actor_identity;
    uint32_t front_actor_generation;
    uintptr_t camera_identity;
    uint32_t camera_generation;
    uintptr_t camera_target_actor_identity;
    uint32_t camera_target_actor_generation;
    uintptr_t ownership_identity;
    uint32_t ownership_generation;
    uint8_t group_order[SUDEKIMP_TALOS_HERO_COUNT];
    uint8_t formation_order[SUDEKIMP_TALOS_HERO_COUNT];
    uint8_t group_count;
    uint8_t formation_count;
    uint8_t exact_executable;
    uint8_t exact_asset;
    uint8_t ownership_frozen;
    uint8_t in_combat;
    uint8_t tsa_active;
    uint8_t transition_active;
    uint8_t modal_active;
    uint8_t reserved;
} SudekiMpTalosStagingSnapshot;

typedef struct SudekiMpTalosStagingTicket {
    uint64_t operation_serial;
    uint64_t authorization_serial;
    uint64_t authorized_observation_serial;
    uint32_t encounter_serial;
    uint32_t transition_serial;
    uint32_t world_generation;
    uint32_t source_generation;
    uint32_t runtime_generation;
    uint32_t native_function_rva;
    uintptr_t native_thread_identity;
    uintptr_t wrapper_identity;
    uint32_t wrapper_generation;
    uintptr_t actor_identity;
    uint32_t actor_generation;
    uint8_t hero;
    uint8_t reserved[3];
} SudekiMpTalosStagingTicket;

typedef struct SudekiMpTalosCompanionStaging {
    SudekiMpTalosCompanionStagingState state;
    SudekiMpTalosEncounterProvenance provenance;
    SudekiMpTalosStagingSnapshot original;
    SudekiMpTalosStagingSnapshot detached;
    SudekiMpTalosStagingSnapshot restored;
    SudekiMpTalosStagingTicket remove_ticket;
    SudekiMpTalosStagingTicket restore_ticket;
    uint64_t operation_serial;
    uint64_t next_authorization_serial;
    uint64_t consumed_remove_authorization_serial;
    uint64_t consumed_restore_authorization_serial;
    uint64_t highest_terminal_operation_serial;
    uint32_t terminal_encounter_serial;
    uint32_t terminal_transition_serial;
    uint32_t terminal_world_generation;
    uint32_t terminal_source_generation;
    uint8_t enabled;
    uint8_t reserved[7];
} SudekiMpTalosCompanionStaging;

/* Operation and observation serials use the unsigned half-range rule, reserve
 * zero, and therefore accept MAX -> 1 as a forward wrap. Authorization
 * serials never wrap: exhaustion fails safe before removal and quarantines
 * after detachment. A quarantined coordinator cannot be cleared by toggling
 * this default-off feature. */

void SudekiMpTalosCompanionStagingInitialize(
    SudekiMpTalosCompanionStaging *staging
);

void SudekiMpTalosCompanionStagingConfigure(
    SudekiMpTalosCompanionStaging *staging,
    int enabled
);

/* Invalid preflight evidence is a safe no-op: no ticket has been issued and
 * state remains IDLE. */
SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingBegin(
    SudekiMpTalosCompanionStaging *staging,
    uint64_t operation_serial,
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosStagingSnapshot *preflight
);

/* A successful claim authorizes exactly one call to public RemovePlayer at
 * RVA 0x23390 for the captured Elco wrapper/actor tuple. */
SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingClaimRemove(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *immediate,
    SudekiMpTalosStagingTicket *ticket
);

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingFinishRemove(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosStagingTicket *ticket,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *completion,
    unsigned int original_call_count
);

/* The detached group must preserve [Tal,Ailish,Buki]. Native formation
 * canonicalization may reorder those same three unique survivors, so their
 * order is not constrained until the exact four-member restore proof. */

/* A successful claim authorizes exactly one call to public AddPlayer at RVA
 * 0x23230 for the same captured Elco wrapper/actor tuple. */
SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingClaimRestore(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *immediate,
    SudekiMpTalosStagingTicket *ticket
);

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingFinishRestore(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosStagingTicket *ticket,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *completion,
    unsigned int original_call_count
);

/* This must be a later, independently sampled observation. Release is not
 * available until this exact post-restore stability proof succeeds. */
SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingObserveStability(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *observation
);

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingRelease(
    SudekiMpTalosCompanionStaging *staging
);

#endif
