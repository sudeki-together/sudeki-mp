#ifndef SUDEKIMP_CONTROL_SEPARATION_H
#define SUDEKIMP_CONTROL_SEPARATION_H

#include "engine/roaming_boundary.h"

#include <stdint.h>
#include <windows.h>

typedef enum SudekiMpControlUpdateDispatchSource {
    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_UNKNOWN = 0,
    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_PRE_ORIGINAL,
    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL
} SudekiMpControlUpdateDispatchSource;

/* Pointer-free evidence borrowed for one observer invocation. The source is
 * assigned only by the owned controller wrapper at its internal notify site.
 * source_exact and service_post_original_exact are false whenever the wrapper
 * is re-entered, overlaps another controller update, loses its hook/slot, or
 * cannot prove its TLS frame. Registry stability is checked immediately
 * before the individual callback; unregister remains non-quiescent. */
typedef struct SudekiMpControlUpdateDispatchWitness {
    uint64_t dispatch_serial;
    uint32_t native_thread_id;
    uint32_t outer_update_depth;
    uint32_t active_dispatch_count;
    uint32_t original_call_count;
    uint32_t observer_snapshot_count;
    uint32_t observer_registry_generation;
    uint32_t dispatch_overlap_generation;
    uint8_t hook_owned_exact;
    uint8_t slot_owned_exact;
    uint8_t service_only;
    uint8_t post_original;
    uint8_t source;
    uint8_t source_exact;
    uint8_t service_post_original_exact;
    uint8_t sole_observer;
    uint8_t registry_generation_stable;
    uint8_t reserved[3];
} SudekiMpControlUpdateDispatchWitness;

typedef void (*SudekiMpControlUpdateObserver)(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
);

/* Teardown gate for observer-owned backing state. Disable before unregister,
 * then drain; a callback already present in the registry snapshot can still
 * invoke TryEnter, but it cannot win the gate after Disable returns. */
typedef struct SudekiMpControlUpdateObserverGate {
    volatile LONG enabled;
    volatile LONG active_entries;
} SudekiMpControlUpdateObserverGate;

BOOL SudekiMpControlUpdateObserverGateEnable(
    SudekiMpControlUpdateObserverGate *gate
);
BOOL SudekiMpControlUpdateObserverGateTryEnter(
    SudekiMpControlUpdateObserverGate *gate
);
void SudekiMpControlUpdateObserverGateLeave(
    SudekiMpControlUpdateObserverGate *gate
);
void SudekiMpControlUpdateObserverGateDisable(
    SudekiMpControlUpdateObserverGate *gate
);
void SudekiMpControlUpdateObserverGateDrain(
    SudekiMpControlUpdateObserverGate *gate
);

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT toggle_virtual_key,
    BOOL enable_second_player_movement,
    BOOL enable_camera_relative_movement,
    BOOL enable_separation_guard,
    float maximum_separation,
    BOOL enable_second_player_weak_attack,
    UINT weak_attack_virtual_key,
    BOOL enable_second_player_skills,
    const UINT second_player_skill_virtual_keys[4],
    BOOL enable_target_trace,
    BOOL enable_shared_group_camera,
    BOOL enable_input_bridge,
    float input_bridge_deadzone
);
BOOL SudekiMpControlSeparationRequestPlayerTwo(BOOL enabled);
BOOL SudekiMpControlSeparationRequestPlayerTwoCharacter(void *character);
/* Stable zero-based local-seat APIs for the fixed P1/P2/P3 control slice.
 * Seats 1 and 2 are the only companion seats accepted.  P2 retains its legacy
 * first-non-front request wrapper; P3 must always be named with the Character
 * form so it can never bind an arbitrary remaining AI.  These APIs are game-
 * thread transitions and do not manufacture camera ownership. */
BOOL SudekiMpControlSeparationRequestSeat(
    unsigned int seat_index,
    BOOL enabled
);
BOOL SudekiMpControlSeparationRequestSeatCharacter(
    unsigned int seat_index,
    void *character
);
/* Pure verifier for acquiring exactly one previously-unowned native AI lease. */
BOOL SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
    int16_t before_ref,
    uint8_t before_mode,
    int16_t after_ref,
    uint8_t after_mode
);
/* Pure verifier for releasing this module's one refcounted AI-control lease.
 * A nested native lease may remain active after our single decrement. */
BOOL SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
    int16_t before_ref,
    uint8_t before_mode,
    int16_t after_ref,
    uint8_t after_mode,
    BOOL controller_target
);
/* Game-thread transition barrier: disable the request and synchronously
 * return a currently-owned Player 2 character to native AI when possible. */
BOOL SudekiMpControlSeparationReleasePlayerTwoNow(void);
BOOL SudekiMpControlSeparationReleaseSeatNow(unsigned int seat_index);
BOOL SudekiMpControlSeparationSetRoleLock(BOOL enabled);
BOOL SudekiMpControlSeparationSetInteractionRequestsEnabled(BOOL enabled);
BOOL SudekiMpControlSeparationSeatRequested(unsigned int seat_index);
BOOL SudekiMpControlSeparationSeatActive(unsigned int seat_index);
void *SudekiMpControlSeparationSeatCharacter(unsigned int seat_index);
BOOL SudekiMpControlSeparationSeatInputReady(unsigned int seat_index);
/* Pure fail-closed policy used by P3 movement/combat and exact-image tests.
 * P2 preserves its existing native-camera fallback. P3 cannot submit until a
 * distinct viewport camera and render state have been published for seat 2. */
BOOL SudekiMpControlSeparationSeatSubmissionReadyPolicy(
    unsigned int seat_index,
    BOOL requested,
    BOOL active,
    BOOL input_ready,
    BOOL seat_view_ready
);
/* Pure fixed-seat lifecycle policies used by the live reconciler and focused
 * tests. `actor_changed` covers disable and retarget operations. */
BOOL SudekiMpControlSeparationSeatRequestTransitionPolicy(
    unsigned int seat_index,
    BOOL enabling,
    BOOL actor_changed,
    BOOL player_two_exact_requested,
    BOOL player_three_present
);
BOOL SudekiMpControlSeparationSeatAcquireOrderPolicy(
    unsigned int seat_index,
    BOOL player_two_exact_active
);
BOOL SudekiMpControlSeparationSeatReleaseOrderPolicy(
    unsigned int seat_index,
    BOOL player_three_owned
);
/* A committed fixed-three roster owns camera-before-actor teardown.  The
 * control reconciler must defer release until the roster observer can run. */
BOOL SudekiMpControlSeparationDeferReleaseToRosterPolicy(
    BOOL fixed_three_contract,
    BOOL role_lock_active,
    BOOL release_required
);
BOOL SudekiMpControlSeparationSeatInputLeaseExactPolicy(
    const void *leased_identity,
    uint32_t leased_generation,
    const void *current_identity,
    uint32_t current_generation
);
/* Pre-acquire readiness is intentionally independent of a lease that does not
 * exist yet. Fixed 0x07 requires current input for both companion seats;
 * legacy 0x03 preserves its established P2 acquire/reconnect behavior. */
BOOL SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
    unsigned int seat_index,
    BOOL fixed_three_contract,
    BOOL current_input_ready
);
/* Fixed 0x07 must never begin a partial native-control claim.  This is
 * evaluated before either companion request is submitted. */
BOOL SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
    BOOL player_two_input_ready,
    BOOL player_three_input_ready
);
/* Active submission/publication must match the input identity and generation
 * captured by the AI lease for P3 and for P2 under fixed 0x07. */
BOOL SudekiMpControlSeparationSeatActiveInputLeasePolicy(
    unsigned int seat_index,
    BOOL fixed_three_contract,
    BOOL current_input_ready,
    const void *leased_identity,
    uint32_t leased_generation,
    const void *current_identity,
    uint32_t current_generation
);
BOOL SudekiMpControlSeparationSeatInputLeaseActive(unsigned int seat_index);
/* LAN host-only ingress. The runtime must enable this only after the UDP
 * session has authenticated a fixed Ailish client; local keyboard/bridge
 * input remains a separate path. Inputs are world-relative and are submitted
 * through the same native Ailish AI lease and arbiter ABI as local co-op. */
BOOL SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(BOOL enabled);
/* LAN profiles name their replica/remote actor explicitly and must not allow
 * the ordinary local drop-in hotkey to retarget that lease. */
BOOL SudekiMpControlSeparationSetManualToggleEnabled(BOOL enabled);
BOOL SudekiMpControlSeparationLanArenaRemoteSubmissionPolicy(
    BOOL remote_session_authenticated,
    BOOL player_two_requested,
    BOOL player_two_lease_exact,
    BOOL character_in_active_group,
    BOOL native_control_state_exact,
    BOOL direction_finite,
    BOOL weak_attack_edge
);
BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
    float world_direction_x,
    float world_direction_z,
    BOOL weak_attack_edge
);
BOOL SudekiMpControlSeparationPlayerTwoRequested(void);
BOOL SudekiMpControlSeparationPlayerTwoActive(void);
void *SudekiMpControlSeparationPlayerTwoCharacter(void);
BOOL SudekiMpControlSeparationInputReady(void);
BOOL SudekiMpControlSeparationGameplayInputFrozen(void);
BOOL SudekiMpControlSeparationSecondPlayerMovementActive(void);
float SudekiMpControlSeparationSecondPlayerMovementMagnitude(void);
BOOL SudekiMpControlSeparationGetRoamingBoundarySnapshot(
    SudekiMpRoamingBoundaryEvaluation *snapshot
);
void SudekiMpControlSeparationReportRoamingBoundaryOverlay(BOOL visible);
/* The owner identity must remain stable until it is unregistered. Registration
 * is idempotent for the same owner/observer pair and never replaces another
 * owner's callback. Observer callbacks run on the native controller-update
 * thread. The controller, update-data, and witness arguments are borrowed
 * from that native update and must not be retained past the callback. The
 * service-only profile invokes observers after its original update.
 * Unregister is not a quiescence barrier: a callback already snapshotted for
 * an update may still run. Before its owner unregisters it, a teardown-
 * sensitive observer must atomically disable its own callback entry gate and
 * retain backing state for any callback that already passed that gate. */
BOOL SudekiMpControlSeparationRegisterUpdateObserver(
    const void *owner,
    SudekiMpControlUpdateObserver observer
);
BOOL SudekiMpControlSeparationUnregisterUpdateObserver(
    const void *owner
);
/* Revalidate a borrowed witness synchronously from its observer callback.
 * This fails after callback return, re-entry/overlap, registry mutation, or
 * loss of the exact controller hook/slot. It preserves the caller's
 * LastError value. */
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness
);
void SudekiMpUninstallControlSeparation(void);

#endif
