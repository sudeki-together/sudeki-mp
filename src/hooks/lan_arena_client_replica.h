#ifndef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H
#define SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H

#include <windows.h>
#include <stdint.h>

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

/* Pure client presentation policy. Native idle is a loop, so returning to it
 * must not rewind its clock. If Sudeki has already entered the requested
 * selector, adopting that clock also avoids a visible start-frame restart. */
BOOL SudekiMpLanArenaClientAnimationShouldResetTime(
    uint8_t previous_animation_state,
    uint8_t next_animation_state,
    BOOL renderer_already_matches_target
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
 * scheduler cannot briefly replace the host-authoritative selector before
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
