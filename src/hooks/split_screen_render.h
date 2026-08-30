#ifndef SUDEKIMP_SPLIT_SCREEN_RENDER_H
#define SUDEKIMP_SPLIT_SCREEN_RENDER_H

#include <windows.h>

typedef void (*SudekiMpSplitScreenOverlayRenderer)(void);
typedef BOOL (*SudekiMpModOwnedBlacksmithActiveQuery)(void);
typedef BOOL (*SudekiMpSplitScreenRuntimeAuthorizationQuery)(void);

enum {
    SUDEKIMP_QUICK_MENU_ISOLATION_IDLE = 0u,
    SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE = 1u,
    SUDEKIMP_QUICK_MENU_ISOLATION_FAILED = 2u,
    SUDEKIMP_QUICK_MENU_ISOLATION_TAIL = 3u
};

enum {
    SUDEKIMP_TEMP_CAMERA_OUTSIDE = 0u,
    SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH = 1u,
    SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO = 2u
};

enum {
    SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE = 0u,
    SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED = 1u,
    SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED = 2u
};

enum {
    SUDEKIMP_NATIVE_CAMERA_DECISION_FALLBACK = 0u,
    SUDEKIMP_NATIVE_CAMERA_DECISION_SET_TARGET = 1u,
    SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT = 2u,
    SUDEKIMP_NATIVE_CAMERA_DECISION_SET_STATE = 3u,
    SUDEKIMP_NATIVE_CAMERA_DECISION_READY = 4u,
    SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE = 5u
};

enum {
    SUDEKIMP_NATIVE_CAMERA_READINESS_INACTIVE = 0u,
    SUDEKIMP_NATIVE_CAMERA_READINESS_PENDING_STATE = 1u,
    SUDEKIMP_NATIVE_CAMERA_READINESS_CHECK_STATE = 2u,
    SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE = 3u
};

enum {
    SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT = 0u,
    SUDEKIMP_NATIVE_CAMERA_RELEASE_REMOVE = 1u,
    SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON = 2u
};

enum {
    SUDEKIMP_SHARED_INTERACTION_MODAL_NONE = 0u,
    SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP = 1u,
    SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH = 2u,
    SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN = 3u,
    SUDEKIMP_SHARED_INTERACTION_MODAL_SAVE_BOOK = 4u
};

BOOL SudekiMpInstallSplitScreenRender(
    HMODULE game_module,
    BOOL enable_second_player_camera,
    BOOL enable_dual_camera_frame_cache,
    UINT toggle_second_player_camera_virtual_key,
    BOOL enable_skill_camera_routing,
    BOOL enable_second_player_controller_camera,
    BOOL enable_native_second_player_camera_collision,
    BOOL enable_split_screen_ranged_model_isolation,
    BOOL enable_spirit_strike_viewport_effect_isolation,
    float controller_camera_deadzone,
    float controller_camera_yaw_speed,
    float controller_camera_pitch_speed,
    float controller_camera_maximum_pitch
);
BOOL SudekiMpSplitScreenSetRuntimeEnabled(BOOL enabled);
BOOL SudekiMpSplitScreenRuntimeEnabled(void);
/* Optional startup-owned presentation authorization. A configured query is
 * evaluated once before RenderStart and that result is latched through the
 * matching FrameEnd, before Camera 2 acquisition, render-state swaps, cache
 * capture, or split composition. False leaves the native full-width frame
 * untouched. The query is cleared by uninstall and must not mutate game
 * state. */
void SudekiMpSplitScreenSetRuntimeAuthorizationQuery(
    SudekiMpSplitScreenRuntimeAuthorizationQuery query
);
BOOL SudekiMpSplitScreenRuntimeAuthorizationPolicy(
    BOOL runtime_enabled,
    BOOL query_present,
    BOOL query_authorized
);
BOOL SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
    BOOL external_authorization_present,
    BOOL player_two_requested,
    BOOL player_two_active,
    const void *combat_context_character,
    const void *control_lease_character
);
BOOL SudekiMpSplitScreenRuntimeAuthorized(void);
/* Pure fail-closed gate for the dormant 1-4 human compositor.  Every ready
 * mask must exactly match active_human_mask: inactive seats may not retain a
 * stale native lease, and no active seat may render until its actor, camera,
 * render state, HUD owner, input source, and frame cache are all proven.
 * This policy has no live caller while P3/P4 native ownership is unproven. */
BOOL SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
    BOOL feature_enabled,
    unsigned int active_human_mask,
    BOOL layout_ready,
    unsigned int actor_lease_mask,
    unsigned int camera_lease_mask,
    unsigned int render_state_lease_mask,
    unsigned int hud_lease_mask,
    unsigned int input_lease_mask,
    unsigned int frame_cache_ready_mask,
    BOOL global_presentation_clear
);
/* Camera-2-only ranged perspective.  The toggle only latches a requested
 * render-matrix mode; it never invokes Sudeki's global first-person transition
 * or changes a character's attached model wrapper. */
BOOL SudekiMpSplitScreenPlayerTwoPerspectivePolicy(
    BOOL runtime_ready,
    BOOL camera_ready,
    BOOL input_ready,
    BOOL presentation_clear,
    BOOL combat_active,
    BOOL actor_matches,
    BOOL lease_matches,
    BOOL ranged_capable
);
BOOL SudekiMpSplitScreenPlayerTwoPerspectiveAvailable(void *character);
BOOL SudekiMpSplitScreenTogglePlayerTwoPerspective(
    void *character,
    BOOL *first_person
);
/* Lock the two party identities for the current co-op gameplay session. */
BOOL SudekiMpSplitScreenLockRoles(void *player_one, void *player_two);
BOOL SudekiMpSplitScreenRolesLocked(void);
BOOL SudekiMpSplitScreenSetRosterTypes(
    unsigned int player_one_type,
    unsigned int player_two_type
);
BOOL SudekiMpSplitScreenClearRosterTypes(void);
/* Read the title-selected roster contract without changing runtime state. */
BOOL SudekiMpSplitScreenGetRosterTypes(
    unsigned int *player_one_type,
    unsigned int *player_two_type
);
/* Read-only proof that a stable seat tuple still names exactly one authored
 * actor in the live active-group party. No party ordinal is persisted. */
BOOL SudekiMpSplitScreenRosterActorIdentityMatches(
    unsigned int player_index,
    const void *actor,
    unsigned int character_type
);
/* Pure truth-table seam used by the exact-image regression. */
BOOL SudekiMpSplitScreenRosterActorIdentityPolicy(
    unsigned int player_index,
    const void *actor,
    unsigned int requested_type,
    unsigned int expected_type,
    const void *unique_party_actor,
    unsigned int actor_occurrences,
    const void *locked_actor,
    BOOL runtime_enabled,
    BOOL roles_locked,
    BOOL participation_requested,
    BOOL party_transition_active
);
/* Keep the title-selected roster while allowing Player 2 to leave and later
 * reclaim the same character.  This is deliberately participation state,
 * not a character-selection change. */
BOOL SudekiMpSplitScreenRosterParticipationAvailable(void);
BOOL SudekiMpSplitScreenRosterParticipationRequested(void);
BOOL SudekiMpSplitScreenRequestRosterParticipation(BOOL enabled);
/* Quiesce cameras/control around a one-world native scene transition, then
 * let the roster state machine reacquire the same identities afterward. */
BOOL SudekiMpSplitScreenBeginPartyTransition(void);
BOOL SudekiMpSplitScreenEndPartyTransition(BOOL party_placed);
/* Pure lifecycle predicates used by the runtime and exact-image tests. */
BOOL SudekiMpSplitScreenRosterLeadReady(
    const void *controller_target,
    const void *party_front,
    const void *expected_player_one,
    unsigned int controller_mode_80,
    unsigned int controller_mode_84,
    unsigned int group_state_d0,
    unsigned int group_switching_d6,
    unsigned int group_state_d7,
    unsigned int next_character_action,
    unsigned int previous_character_action
);
BOOL SudekiMpSplitScreenRosterLockHealthy(
    const void *controller_target,
    const void *party_front,
    const void *locked_player_one,
    const void *controlled_player_two,
    const void *locked_player_two
);
/* Pure native-camera readiness edge used by the exact-image regression test.
 * A fallback write refreshes the baseline and can never prove native work. */
BOOL SudekiMpSplitScreenObserveNativeCameraGeneration(
    unsigned short *baseline,
    unsigned short generation,
    BOOL manual_fallback_write
);
/* Read the raw actor only as a generation-scoped provenance identity. Neither
 * this actor nor the intrusive party slot is a valid SetCameraTarget argument;
 * the setter requires a caller-owned native GELGroupPtr wrapper. */
void *SudekiMpSplitScreenNativeCameraActorFromPartySlot(
    const void *party_slot
);
BOOL SudekiMpSplitScreenNativeCameraWrapperPolicy(
    const void *wrapper,
    const void *party_slot,
    const void *raw_actor,
    const void *embedded_actor,
    const void *resolved_actor,
    const void *leased_actor,
    unsigned int current_actor_generation,
    unsigned int expected_actor_generation,
    BOOL exact_wrapper_vtable
);
BOOL SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
    BOOL getter_already_attempted,
    BOOL live_group_slot_actor_valid,
    BOOL camera_ownership_valid
);
unsigned int SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
    unsigned int stage,
    BOOL bound
);
/* Pure fail-closed gates for the staged native Exploration bootstrap. */
BOOL SudekiMpSplitScreenNativeCameraOwnershipPolicy(
    BOOL current_manager_matches,
    BOOL named_camera_matches,
    BOOL global_camera_matches,
    BOOL player_one_render_state_matches,
    BOOL scene_render_state_matches,
    BOOL render_swap_inactive
);
BOOL SudekiMpSplitScreenNativeCameraIdentityPolicy(
    const void *current_slot,
    const void *bound_slot,
    const void *current_actor,
    const void *bound_actor,
    const void *leased_actor,
    unsigned int current_actor_generation,
    unsigned int bound_actor_generation
);
BOOL SudekiMpSplitScreenNativeCameraMatrixPolicy(const float matrix[16]);
/* A named camera may be removed only while every live ownership identity is
 * known and still names the saved Player 1 world state. Stale generations or
 * name reuse are abandoned only after proving that no live owner references
 * the saved Player 2 camera/state. */
unsigned int SudekiMpSplitScreenNativeCameraReleasePolicy(
    BOOL manager_matches,
    BOOL named_identity_known,
    const void *named_camera,
    const void *saved_player_two_camera,
    BOOL global_identity_known,
    const void *global_camera,
    const void *global_render_state,
    const void *saved_player_one_camera,
    const void *saved_player_one_render_state,
    BOOL scene_identity_known,
    const void *scene_render_state,
    const void *saved_player_two_render_state,
    BOOL render_swap_inactive
);
unsigned int SudekiMpSplitScreenNativeCameraBootstrapPolicy(
    BOOL session_enabled,
    unsigned int stage,
    BOOL phase_eligible,
    BOOL ownership_valid,
    BOOL identity_valid,
    BOOL target_pair_stable,
    BOOL later_frame,
    BOOL active_state_valid,
    BOOL player_one_exploration,
    BOOL player_two_exploration,
    BOOL native_generation_advanced,
    BOOL matrix_valid,
    BOOL readiness_deadline_expired
);
/* A settled TEMP room receives a distinct second view only after the live
 * Player 1 camera and the independently updated Player 2 camera have both
 * proved native Exploration ownership. Fixed, transitional, unavailable, or
 * not-yet-ready camera modes use the exact shared room pose. */
unsigned int SudekiMpSplitScreenTemporaryCameraPolicy(
    BOOL settled_temporary_zone,
    int player_one_camera_mode,
    BOOL native_player_two_ready
);
/* Apply a title-selected roster from the game/controller update thread. */
void SudekiMpSplitScreenApplyRosterOnGameThread(void);
BOOL SudekiMpSplitScreenQuickMenuLiveViewAccepted(
    BOOL isolation_requested,
    BOOL resources_ready,
    BOOL player_two_requested_before_apply,
    BOOL player_two_rendered
);
unsigned int SudekiMpSplitScreenQuickMenuIsolationBeginState(
    unsigned int state,
    BOOL was_visible,
    BOOL visible,
    BOOL eligible_on_rising_edge
);
unsigned int SudekiMpSplitScreenQuickMenuIsolationEndState(
    unsigned int state,
    BOOL quick_menu_submit_seen
);
unsigned int SudekiMpSplitScreenQuickMenuIsolationCancelState(
    BOOL quick_menu_visible
);
BOOL SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
    BOOL isolation_in_progress,
    BOOL render_phase_confirmed,
    BOOL player_two_expected,
    BOOL player_two_rendered
);
/* Pure policy used by the alternating frame cache and its exact-image tests.
 * The native map update owns both the centered facing pointer and the map
 * transform.  Its successfully resolved owner is latched and reused by the
 * later map render so the highlighted party dot cannot name another actor. */
BOOL SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
    BOOL enabled,
    BOOL update_owner_valid,
    BOOL update_player_two
);
BOOL SudekiMpSplitScreenMinimapCaptureAllowed(
    BOOL enabled,
    BOOL update_owner_valid,
    BOOL update_player_two,
    BOOL rendered_player_two
);
/* Shop and Blacksmith are process-global native UI layers.  While either is
 * opening/active (or its ownership cannot be inspected safely), rendering and
 * control use one native Player 1 view.  The runtime query remains true until
 * the post-close two-camera cache has been rebuilt. */
unsigned int SudekiMpSplitScreenClassifySharedInteractionModal(
    BOOL inspection_valid,
    BOOL shop_reported_active,
    BOOL blacksmith_reported_active,
    unsigned int current_ui_mode,
    unsigned int next_ui_mode
);
BOOL SudekiMpSplitScreenSharedInteractionModalActive(void);
/* The save-book hook must enter this narrow lifecycle before publishing its
 * vote or invoking any native continuation.  Opening synchronously switches
 * presentation to the single native Player 1 frame and invalidates both
 * alternating caches.  Closed keeps control quiesced until both camera caches
 * have been captured afresh. */
BOOL SudekiMpSplitScreenNativeSaveModalOpening(void);
void SudekiMpSplitScreenNativeSaveModalClosed(void);
BOOL SudekiMpSplitScreenNativeSaveModalActive(void);
/* A mod-owned blacksmith start keeps UIBlackSmithActive true only to satisfy
 * the script polling contract. The inspector may exclude it from the native
 * full-width policy solely when this query is true and the actual native
 * layer/mode state independently proves inactive. */
BOOL SudekiMpSplitScreenNativeBlacksmithReportedActive(
    BOOL reported_active,
    BOOL mod_owned_active,
    BOOL native_layer_active,
    unsigned int current_ui_mode,
    unsigned int next_ui_mode
);
void SudekiMpSplitScreenSetModOwnedBlacksmithActiveQuery(
    SudekiMpModOwnedBlacksmithActiveQuery query
);
/* Pure fail-open lifecycle policies used by the runtime and exact-image test.
 * An unavailable inspector is not an uncertain native UI observation. */
BOOL SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
    BOOL inspector_installed,
    unsigned int observation,
    BOOL recovery_pending
);
BOOL SudekiMpSplitScreenSharedInteractionRecoveryEligible(
    BOOL runtime_enabled,
    BOOL participation_requested,
    BOOL roles_locked,
    BOOL camera_feature_enabled,
    BOOL cache_feature_enabled,
    BOOL camera_pair_ready,
    BOOL cache_pair_ready
);
BOOL SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
    BOOL recovery_pending,
    BOOL modal_closed_edge,
    BOOL modal_had_live_split,
    BOOL current_eligible,
    BOOL fresh_cache_pair_ready
);
/* Skip redundant same-owner assignments. A stored-enum change for the
 * currently rendered viewport may still invoke Sudeki's synchronous selector
 * on each alternating P1/P2 view. Native/uncertain modal construction
 * suspends it even when the currently stored enum differs. */
BOOL SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
    BOOL modal_construction_active,
    unsigned int current_portrait_enum,
    unsigned int desired_portrait_enum
);
void SudekiMpSplitScreenSetOverlayRenderer(
    SudekiMpSplitScreenOverlayRenderer renderer
);
BOOL SudekiMpTransformPlayerTwoMovement(
    const float local_direction[3],
    float world_direction[3]
);
BOOL SudekiMpAlignPlayerTwoFacingToCamera(void *character);
BOOL SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(void *character);
void SudekiMpSplitScreenBeginSkillCameraCall(void *caster);
void SudekiMpSplitScreenEndSkillCameraCall(void);
void SudekiMpSplitScreenClearSkillCamera(void *caster, const char *reason);
void SudekiMpUninstallSplitScreenRender(void);

#endif
