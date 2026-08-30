#ifndef SUDEKIMP_TALOS_POST_MOVIE_PARTY_RESTORE_H
#define SUDEKIMP_TALOS_POST_MOVIE_PARTY_RESTORE_H

#include "hooks/talos_native_lifecycle_trace.h"

#include <stdint.h>
#include <windows.h>

enum {
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_AILISH_MASK = 1u << 0u,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_BUKI_MASK = 1u << 1u,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_ELCO_MASK = 1u << 2u,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_AILISH_MASK |
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_BUKI_MASK |
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_ELCO_MASK,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_POSTSPAWN_TIMEOUT_MS = 10000u,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_PLAYER_TWO_TIMEOUT_MS = 5000u
};

enum {
    SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_SPLIT = 1u << 0u,
    SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_PLAYER_TWO = 1u << 1u,
    SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_DUAL_CACHE = 1u << 2u,
    SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_EXACT =
        SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_SPLIT |
        SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_PLAYER_TWO |
        SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_DUAL_CACHE
};

typedef enum SudekiMpTalosPostMoviePartyRestoreState {
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_DISABLED = 0,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_TICKET,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_PREFLIGHT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_SPAWN_REQUESTS,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_INITIALIZE_ACTORS,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REQUEST_PLAYER_TWO,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PLAYER_TWO,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REFRESH_COMBAT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED
} SudekiMpTalosPostMoviePartyRestoreState;

typedef enum SudekiMpTalosPostMoviePartyRestoreFailure {
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_NONE = 0,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_INSTALL_ARGUMENT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_DLL_PIN,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CLEANROOM_INITIALIZE,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_AI_FILTER_HOOK,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_OBSERVER_REGISTER,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_WITNESS_LOST,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_TICKET_SHAPE,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PREFLIGHT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_SPAWN_REJECTED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_POSTSPAWN_TIMEOUT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_ACTOR_INITIALIZE,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_TARGET_POLICY,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_BOSS_FILTER_IDENTITY,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_REQUEST,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_TIMEOUT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_COMBAT_REFRESH,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE
} SudekiMpTalosPostMoviePartyRestoreFailure;

typedef enum SudekiMpTalosPostMoviePartyRestoreEvent {
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED = 1,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_ACCEPTED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_REJECTED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SPAWN_RESULT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_COMBAT_REFRESH_RESULT,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SESSION_ENDED,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT
} SudekiMpTalosPostMoviePartyRestoreEvent;

/* Pointer-free pure reducer used by the live observer and unit tests. A
 * ticket claim and each native mutation phase are accepted only in their one
 * exact state. value is a companion mask for SPAWN/PARTY/INITIALIZE, a strict
 * Boolean for Player 2/combat events, and a failure enum for ABORT. */
typedef struct SudekiMpTalosPostMoviePartyRestoreMachine {
    uint32_t state;
    uint32_t failure;
    uint32_t ticket_claimed_at_ms;
    uint32_t postspawn_started_at_ms;
    uint32_t player_two_started_at_ms;
    uint8_t spawn_accepted_mask;
    uint8_t party_present_mask;
    uint8_t initialized_mask;
    uint8_t ticket_claimed;
    uint8_t player_two_requested;
    uint8_t player_two_active;
    uint8_t combat_refreshed;
    uint8_t reserved;
} SudekiMpTalosPostMoviePartyRestoreMachine;

void SudekiMpTalosPostMoviePartyRestoreMachineInitialize(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    BOOL enabled
);
BOOL SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    SudekiMpTalosPostMoviePartyRestoreEvent event,
    uint32_t value,
    uint32_t now_ms
);

/* Pointer-free public state. Native identities never escape this module; the
 * lifecycle ticket contains salted equality evidence only. */
typedef struct SudekiMpTalosPostMoviePartyRestoreStatus {
    uint64_t status_serial;
    uint64_t exact_update_count;
    uint64_t rejected_update_count;
    uint64_t ticket_claim_call_count;
    uint64_t spawn_call_count;
    uint64_t actor_initialize_call_count;
    uint64_t player_two_request_call_count;
    uint64_t combat_refresh_call_count;
    uint64_t ai_filter_bypass_count;
    uint32_t last_native_thread_id;
    uint32_t last_witness_registry_generation;
    uint32_t last_witness_overlap_generation;
    uint32_t last_party_count;
    uint64_t last_witness_dispatch_serial;
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    SudekiMpTalosNativePostMovieRestoreTicket ticket;
    uint8_t installed;
    uint8_t enabled;
    uint8_t observer_registered;
    uint8_t ai_filter_installed;
    uint8_t cleanroom_initialized;
    uint8_t target_policy_active;
    uint8_t boss_filter_identity_ready;
    uint8_t combat_ready;
    uint8_t party_topology_exact;
    /* These are current-frame action eligibility, not merely durable lease
     * ownership. Both temporarily clear while native SkillCam owns a nested
     * refcounted control lease; the ACTIVE session itself remains healthy. */
    uint8_t control_state_exact;
    uint8_t player_two_input_ready;
    uint8_t valid;
    uint8_t reload_required;
    uint8_t reserved[2];
} SudekiMpTalosPostMoviePartyRestoreStatus;

/* Default-off closed final-battle profile. When enabled, this module owns one
 * normal-post-original control-update observer and only the exact AI candidate
 * filter hook at RVA 0x001b6ec0. It never owns InternalSpawnPC, RemovePC, or a
 * character-input hook. */
BOOL SudekiMpInstallTalosPostMoviePartyRestore(
    HMODULE game_module,
    BOOL enabled
);
void SudekiMpUninstallTalosPostMoviePartyRestore(void);
BOOL SudekiMpTalosPostMoviePartyRestoreGetStatus(
    SudekiMpTalosPostMoviePartyRestoreStatus *status
);
/* Pure authorization consumed by the optional Talos dual-camera profile.
 * Camera presentation is deliberately stricter than durable session health:
 * native nested control/cinematic leases clear valid and therefore restore
 * one full-width native view until exact action ownership returns. */
BOOL SudekiMpTalosPostMoviePartyRestoreCameraAuthorized(
    const SudekiMpTalosPostMoviePartyRestoreStatus *status
);
/* Pure closed-profile configuration gate. The baseline admits no camera
 * owners; the distinct-angle experiment admits the complete three-feature
 * bundle. Every partial or unknown bit pattern is rejected. */
BOOL SudekiMpTalosPostMoviePartyRestoreCameraBundleAllowed(
    unsigned int bundle_mask
);
/* The Talos baseline has neither camera bundle nor camera-relative input. The
 * distinct-angle profile requires both the complete camera bundle and the
 * proven camera-relative movement transform; partial pairings are rejected. */
BOOL SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
    unsigned int bundle_mask,
    BOOL camera_relative_movement
);

#endif
