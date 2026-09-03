#ifndef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H
#define SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H

#include <windows.h>
#include <stdint.h>

struct SudekiMpLanArenaActorSnapshot;

typedef struct SudekiMpLanArenaReplicaActorDiagnostics {
    float sampled_position[3];
    float sampled_facing[2];
    float pre_apply_position[3];
    float pre_apply_facing[3];
    float pre_apply_target_speed;
    float pre_apply_smoothed_speed;
    float pre_apply_current_speed;
    float post_apply_position[3];
    float post_apply_facing[3];
    float native_position[3];
    float native_facing[3];
    float render_position[3];
    float render_facing[3];
    float movement_target_speed;
    float movement_smoothed_speed;
    float movement_current_speed;
    float movement_run_blend;
    float accepted_direction[3];
    float local_yaw;
    uint16_t native_generation;
    uint32_t movement_mode;
    uint8_t native_dirty;
    uint8_t position_valid;
    uint8_t render_valid;
    uint8_t movement_valid;
} SudekiMpLanArenaReplicaActorDiagnostics;

typedef struct SudekiMpLanArenaReplicaDiagnostics {
    uint32_t sequence;
    uint32_t upper_snapshot_host_tick;
    uint32_t render_host_tick;
    uint32_t sampled_at_ms;
    float camera_position[3];
    float camera_facing[3];
    uint8_t camera_valid;
    SudekiMpLanArenaReplicaActorDiagnostics actor[2];
    uint8_t valid;
} SudekiMpLanArenaReplicaDiagnostics;

/* Pure exact-image presentation policy used by tests and the replica adapter.
 * The returned selector is local-only and never enters a LAN packet. */
BOOL SudekiMpLanArenaClientIdleVariantSelector(
    uint8_t actor_type,
    uint8_t animation_state,
    int *selector
);

/* Pure client presentation policy. Native idle usually retains its clock, but
 * Tal's replicated action retirement must begin a fresh combat-idle clip
 * instead of inheriting the terminal combo clock. If Sudeki has already
 * entered the requested selector, adopting that clock avoids a restart. */
BOOL SudekiMpLanArenaClientAnimationShouldResetTime(
    unsigned int actor_index,
    uint8_t previous_animation_state,
    uint8_t next_animation_state,
    BOOL renderer_already_matches_target
);

/* Tal's replicated combo retirement must enter the running idle state
 * directly. Passing through the completed-idle state makes the renderer
 * settle once, then visibly restart the same idle clip. */
int SudekiMpLanArenaClientAnimationTransitionState(
    unsigned int actor_index,
    uint8_t previous_animation_state,
    uint8_t next_animation_state,
    int requested_state
);

/* Tal's inactive secondary channel must not retain a native locomotion clock
 * across replicated action/idle edges. Movement used to clear this state by
 * accident, making the same combo look different before and after walking. */
BOOL SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
    unsigned int actor_index,
    BOOL logical_transition,
    BOOL moving,
    BOOL base_target_already_matches
);

/* Pure policy for semantic presentation after the native combat transition.
 * Runtime code still requires an exact actor-local renderer/bank mapping
 * before any selector write. */
BOOL SudekiMpLanArenaClientPresentationOverrideAllowed(BOOL combat_mode);

/* A native combat transition is actor-local. One actor's unavailable render
 * graph must not suppress an independently validated peer presentation. */
BOOL SudekiMpLanArenaClientActorPresentationAllowed(
    unsigned int actor_index,
    BOOL transition_pending,
    BOOL tal_ready,
    BOOL ailish_ready);

/* Pure Tal combat presentation mapping. LAN packets carry semantic actions,
 * never retail selector numbers; this exact-image adapter resolves them only
 * inside the client process. */
BOOL SudekiMpLanArenaClientTalActionPresentation(
    uint8_t action_variant,
    int *selector,
    int *state
);

/* Preserve the local Ailish camera's immediate facing only while it owns the
 * client first-person view. Host position/resources and all non-owner facing
 * remain authoritative. */
BOOL SudekiMpLanArenaClientShouldApplyHostFacing(
    unsigned int actor_index,
    BOOL local_first_person_active
);

/* Converts the bounded LA14 action phase to Sudeki's local animation-time
 * units. It rejects non-action or unphased snapshots before any renderer
 * method is called. */
BOOL SudekiMpLanArenaClientActionPhaseTime(
    const struct SudekiMpLanArenaActorSnapshot *snapshot,
    float *phase_time
);

/* Applies authenticated host transforms/facing/resources for local Ailish,
 * the AI-disabled Tal replica, and the fixed training dummy. */
BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module);
/* Discard interpolation history without removing the exact-image native
 * adapters. Used synchronously when transport authority is lost. */
void SudekiMpLanArenaClientReplicaDiscardSnapshots(void);
void SudekiMpResetLanArenaClientReplica(void);
BOOL SudekiMpLanArenaClientReplicaApplyLatest(void);
/* Reasserts only the presentation semantics from the already-sampled frame.
 * This runs after Sudeki's native animation update so a client-local idle
 * scheduler cannot briefly replace the canonical shared-simulation selector before
 * world draw. It consumes no packet and does not touch transforms/resources. */
BOOL SudekiMpLanArenaClientReplicaReassertPresentation(void);
/* Publish the already-authoritative CPosition basis through Sudeki's native
 * world-matrix path. The LAN runtime uses this at its early animation-basis
 * boundaries and again as a late visible-transform verification; it consumes
 * no network sample and never runs host-side simulation. */
BOOL SudekiMpLanArenaClientReplicaPublishVisibleTransforms(void);
/* Refreshes the visible render-object witness after scene traversal without
 * consuming another network sample or mutating game state. */
void SudekiMpLanArenaClientReplicaRefreshDiagnostics(void);
/* Read-only witness captured after the final pre-draw replica commit. Actor 0
 * is Tal and actor 1 is Ailish. It compares the authenticated sample against
 * CPosition and the matrix attached to the visible render object. */
BOOL SudekiMpLanArenaClientReplicaGetDiagnostics(
    SudekiMpLanArenaReplicaDiagnostics *diagnostics
);

#endif
