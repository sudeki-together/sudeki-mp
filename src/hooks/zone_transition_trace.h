#ifndef SUDEKIMP_ZONE_TRANSITION_TRACE_H
#define SUDEKIMP_ZONE_TRANSITION_TRACE_H

#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpZoneTransitionVoteSnapshot {
    BOOL active;
    unsigned int state;
    uint32_t serial;
    uint32_t remaining_ms;
    uint8_t requester_index;
    uint8_t participant_mask;
    uint8_t accepted_mask;
    uint8_t cancelled_mask;
    char destination[64];
} SudekiMpZoneTransitionVoteSnapshot;

/* Configure before installation. The live vote remains opt-in and installation
 * rejects it unless party-atomic transitions are also enabled. */
BOOL SudekiMpZoneTransitionConfigureVote(BOOL enabled);
/* Pointer-free, read-only state for the eventual menu overlay. */
BOOL SudekiMpZoneTransitionGetVoteSnapshot(
    SudekiMpZoneTransitionVoteSnapshot *snapshot
);
/* The renderer must report the exact prompt serial. The first successful draw
 * starts a fresh five-second countdown; a failed draw cancels the request. */
BOOL SudekiMpZoneTransitionReportVoteOverlay(
    uint32_t serial,
    BOOL visible
);

/* Observation-only door/zone transition tracing for the supported GOG build. */
BOOL SudekiMpInstallZoneTransitionTrace(
    HMODULE game_module,
    BOOL enable_party_atomic_transitions,
    BOOL enable_camera_trace
);
void SudekiMpUninstallZoneTransitionTrace(void);
/* No-op unless an opt-in vote or party transition awaits service. */
void SudekiMpZoneTransitionService(void);
/* Pure policy seam used by the runtime exit hook and exact-image regression:
 * only an active co-op lease, outside SetZoneNow cleanup, may treat a
 * state-4 descriptor as a real temporary-room exit. */
BOOL SudekiMpZoneTransitionShouldArmTemporaryExit(
    BOOL roles_locked,
    unsigned int set_zone_now_nesting,
    unsigned int active_descriptor_state
);

enum {
    SUDEKIMP_EXIT_PRESENTATION_WAIT = 0,
    SUDEKIMP_EXIT_PRESENTATION_READY = 1,
    SUDEKIMP_EXIT_PRESENTATION_SHOW_OWNED = 2
};

enum {
    SUDEKIMP_DOORWAY_STAGING_WAIT = 0,
    SUDEKIMP_DOORWAY_STAGING_REPOP_NOW = 1,
    SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST = 2
};

/* Pure policy seams for the asynchronous EXIT path.  The lead call occurs
 * before the exterior descriptor is safe, so a verified lead write is always
 * deferred.  Once the exterior is stable, presentation may either already be
 * balanced, consume SudekiMP's exact +1 Hide lease once, or remain waiting. */
BOOL SudekiMpZoneTransitionShouldDeferExitLeadPlacement(
    BOOL exit_transition_active,
    BOOL lead_setter_seen
);
unsigned int SudekiMpZoneTransitionExitPresentationAction(
    BOOL override_active,
    BOOL exit_balance_pending,
    BOOL baseline_visible,
    BOOL owned_hide_present,
    BOOL show_already_attempted
);
/* Pure policy for the bounded second native formation pop used after an
 * interior entry. Any loss of quarantine, identity, destination, neutral
 * input, or the original follower staging point accepts the first native
 * placement instead of moving a human-controlled actor. */
unsigned int SudekiMpZoneTransitionDoorwayStagingAction(
    BOOL identity_matches,
    BOOL destination_matches,
    BOOL player_two_unowned,
    BOOL player_two_input_neutral,
    BOOL follower_still_staged,
    BOOL timed_out,
    float inward_advance,
    float required_inward_advance
);

/* World-aware traversal actions. These are available only after the exact
 * transition trace has been installed and are intended for the developer
 * traversal menu, never for ordinary gameplay. */
BOOL SudekiMpZoneTraversalSwitchWorld(const char *zone_name);
BOOL SudekiMpZoneTraversalEnterTemporary(const char *zone_name);
BOOL SudekiMpZoneTraversalExitTemporary(void);
const char *SudekiMpZoneTraversalCurrentWorld(void);
const char *SudekiMpZoneTraversalCurrentTemporary(void);
BOOL SudekiMpZoneTraversalWorldMatches(const char *zone_name);
/* True when the destination is present in the developer traversal registry
 * and may be auto-discovered through Sudeki's native transition pipeline. */
BOOL SudekiMpZoneTraversalKnownDestination(
    const char *world_name,
    const char *temporary_name
);
/* A destination is safe to revisit only after a native save/door transition
 * has supplied an authored arrival anchor for it.  Known destinations may be
 * launched once without a cache; the transition trace captures their native
 * placement automatically and promotes the result to the cache. */
BOOL SudekiMpZoneTraversalArrivalContextReady(
    const char *world_name,
    const char *temporary_name
);
/* Apply the cached native arrival anchor to currently-present cleanroom PCs
 * after the destination load has settled. */
BOOL SudekiMpZoneTraversalApplyArrivalContext(void);
void SudekiMpZoneTraversalService(void);

#endif
