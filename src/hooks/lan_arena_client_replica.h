#ifndef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H
#define SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H

#include <windows.h>
#include "network/lan_arena_protocol.h"
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
    uint32_t render_clock_local_elapsed_ms;
    uint32_t render_clock_advance_ms;
    uint32_t tal_action_sequence;
    uint16_t tal_action_phase_q8;
    uint16_t tal_action_terminal_phase_q8;
    uint16_t tal_idle_entry_phase_q8;
    uint8_t tal_animation_state;
    uint8_t tal_action_variant;
    uint8_t tal_action_phase_valid;
    uint8_t tal_action_retirement_valid;
    uint8_t action_clock_protected;
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

/* Once an exact actor/renderer has reached its native transition handoff,
 * later authenticated presentation may legitimately replace the idle/run
 * selector with an action or Spirit selector. Preserve that actor-local
 * readiness while the same identity remains exact; a changed or unreadable
 * identity revokes it immediately. */
BOOL SudekiMpLanArenaClientActorTransitionReadinessRetained(
    BOOL previously_ready,
    BOOL identity_exact,
    BOOL currently_observed_ready
);

/* Runtime-owned lifecycle identity for the client-only Tal replica. Native
 * object addresses may be reused after a same-session remove/spawn cycle, so
 * an address tuple alone cannot retain presentation readiness. */
BOOL SudekiMpLanArenaClientTalLifecycleLeaseExact(
    void *current_actor,
    uint32_t current_generation,
    void *leased_actor,
    uint32_t leased_generation
);

/* Published only by the LAN runtime at the exact PlayerTwo claim/release
 * boundary. A NULL/zero pair invalidates Tal presentation immediately. */
void SudekiMpLanArenaClientReplicaSetRemoteTalLease(
    void *actor,
    uint32_t actor_generation
);
/* A Tal actor may not be released/replaced while its client-started native
 * combat-input transition is still owned or while the synchronous Spirit VFX
 * entry is on the game-thread stack. This positively drains the exact native
 * lease or returns ERROR_BUSY without weakening containment. */
BOOL SudekiMpLanArenaClientReplicaRemoteTalReleaseReady(void);

/* A native Tal combat transition may settle in armed idle or armed run,
 * depending on the host motion snapshot visible when the transition begins.
 * Both are proven combat-bank states and therefore safe handoff points for
 * actor-local authoritative presentation. */
BOOL SudekiMpLanArenaClientTalTransitionSelectorReady(
    BOOL combat_target,
    int selector
);

/* Sudeki's ranged arm helper closes its native UI lease after 75 ms. The
 * client retries the party-wide arm transition once after that boundary so
 * replica-owned actors cannot remain in an exploration/run graph forever. */
BOOL SudekiMpLanArenaClientCombatTransitionRefreshDue(
    BOOL combat_target,
    BOOL refresh_attempted,
    uint32_t elapsed_ms
);

enum {
    SUDEKIMP_LAN_ARENA_CLIENT_AILISH_REFRESH_BACKOFF_MS = 100u,
    SUDEKIMP_LAN_ARENA_CLIENT_AILISH_REFRESH_MAX_ATTEMPTS = 20u
};

/* The party combat transition and its ranged/UI work are asynchronous. Delay
 * Ailish's actor-local model/weapon refresh until that native window closes,
 * then retry at a bounded cadence instead of making one permanently decisive
 * call during a transient wrapper topology. */
BOOL SudekiMpLanArenaClientAilishRangedRefreshDue(
    unsigned int attempt_count,
    uint32_t elapsed_since_attempt_ms
);

/* Renderer time setters are stateful, not passive assignments. Reasserting a
 * matching terminal action clock every frame prevents Sudeki's native
 * action-to-idle blend from advancing. Correct only measurable drift. */
BOOL SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
    float actual_phase,
    float authoritative_phase
);

/* Pure Tal combat presentation mapping. LAN packets carry semantic actions,
 * never retail selector numbers; this exact-image adapter resolves them only
 * inside the client process. */
BOOL SudekiMpLanArenaClientTalActionPresentation(
    uint8_t action_variant,
    int *selector,
    int *state
);

/* Converts a host-observed semantic Tal action into one native client
 * presentation input edge. Exactly one output is asserted. The host remains
 * the world/damage authority; this edge exists only to let the client's own
 * CGameModel/arbiter animation state perform the transition. */
BOOL SudekiMpLanArenaClientTalNativeCombatInput(
    uint8_t action_variant,
    int *weak,
    int *strong,
    int *sweep,
    int *block
);

/* Preserve the local Ailish camera's immediate facing only while it owns the
 * client first-person view. Host position/resources and all non-owner facing
 * remain authoritative. */
BOOL SudekiMpLanArenaClientShouldApplyHostFacing(
    unsigned int actor_index,
    BOOL local_first_person_active
);

/* Converts the bounded LAN action phase to Sudeki's local animation-time
 * units. It rejects non-action or unphased snapshots before any renderer
 * method is called. */
BOOL SudekiMpLanArenaClientActionPhaseTime(
    const struct SudekiMpLanArenaActorSnapshot *snapshot,
    float *phase_time
);

/* Converts the host-observed first idle timestamp on an action-retirement
 * snapshot. This is deliberately distinct from the live action clock: it is
 * consumed once at the semantic ACTION -> IDLE edge. */
BOOL SudekiMpLanArenaClientRetirementIdlePhaseTime(
    const struct SudekiMpLanArenaActorSnapshot *snapshot,
    float *phase_time
);

/* Converts the host's first post-update idle clock into the clock that must
 * be installed immediately before the client's native animation update. */
BOOL SudekiMpLanArenaClientRetirementPreUpdatePhase(
    float host_idle_entry_phase,
    uint32_t local_frame_elapsed_ms,
    BOOL final_presentation_boundary,
    float *phase_time
);

/* Applies authenticated host transforms/facing/resources for local Ailish,
 * the AI-disabled Tal replica, and the fixed training dummy. */
BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module);
/* Discard interpolation history without removing the exact-image native
 * adapters. Used synchronously when transport authority is lost. */
void SudekiMpLanArenaClientReplicaDiscardSnapshots(void);
/* Restores the exact-image hooks only after every locally-started native
 * presentation CSkill is positively observed inactive and no synchronous
 * Spirit VFX entry remains on the game-thread stack. ERROR_BUSY means the task
 * and damage-containment hook remain owned and the caller must retain the
 * runtime/DLL and retry; other failures likewise retain all still-live hook
 * dependencies. */
BOOL SudekiMpResetLanArenaClientReplica(void);
BOOL SudekiMpLanArenaClientReplicaApplyLatest(void);
/* The first native RenderStart precedes the primary component/CSkill update.
 * Capture the locally-owned Ailish basis only at that safe boundary; never
 * refresh from the second RenderStart after a remote task may have mutated
 * the active render state. */
BOOL SudekiMpLanArenaClientReplicaRefreshOwnerViewAfterRender(void);
/* Restores the most recently first-RenderStart-published Ailish basis after a
 * bounded remote Tal CSkill mutation. Exact identity mismatch retains the
 * lease and fails closed so teardown can retry. */
BOOL SudekiMpLanArenaClientReplicaReassertOwnerViewAfterRemoteMutation(void);
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
/* Services the authenticated, generation-bound host visual roster on the
 * replica render clock, including bounded resource prewarm and native clone
 * retirement. Call only at the first pre-RenderStart game-thread boundary
 * after successful visible-transform publication. Clones are parent-free;
 * complete roster removals, not actor combat readiness, end their lifetime. */
BOOL SudekiMpLanArenaClientReplicaServiceSpiritVfx(void);
/* Pure same-session actor-generation fence. Keeps only effects belonging to
 * a positively newer cast, preserving UNKNOWN and positive-empty semantics. */
BOOL SudekiMpLanArenaClientSpiritVisualFilterGeneration(
    SudekiMpLanArenaSnapshot *snapshot, uint16_t skill_floor);
/* True only while locally-owned Ailish's host-approved native skill task owns
 * the client process camera. A remote Tal task may run for native effects,
 * but its camera writes are contained by Ailish's dynamic owner-view lease. */
BOOL SudekiMpLanArenaClientReplicaLocalSkillCameraActive(void);
/* True while either host-approved native presentation replay is active.
 * Camera input is held at neutral only when LocalSkillCameraActive is true. */
BOOL SudekiMpLanArenaClientReplicaAnySkillReplayActive(void);
/* Returns the last authenticated host combat state only while a live replica
 * frame is installed. Client UI may use this as a read-only availability
 * witness; it never grants the client authority to change combat mode. */
BOOL SudekiMpLanArenaClientReplicaHostCombatState(BOOL *enabled);
/* Refreshes the visible render-object witness after scene traversal without
 * consuming another network sample or mutating game state. */
void SudekiMpLanArenaClientReplicaRefreshDiagnostics(void);
/* Read-only witness captured after the final pre-draw replica commit. Actor 0
 * is Tal and actor 1 is Ailish. It compares the authenticated sample against
 * CPosition and the matrix attached to the visible render object. */
BOOL SudekiMpLanArenaClientReplicaGetDiagnostics(
    SudekiMpLanArenaReplicaDiagnostics *diagnostics
);

#ifdef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_TESTING
enum {
    SUDEKIMP_LAN_ARENA_CLIENT_AILISH_MODEL_UNKNOWN = 0,
    SUDEKIMP_LAN_ARENA_CLIENT_AILISH_MODEL_DESIRED = 1,
    SUDEKIMP_LAN_ARENA_CLIENT_AILISH_MODEL_OPPOSITE = 2
};
/* Direct topology seam for the exact-image regression harness. It exposes no
 * runtime mutation and still exercises the production renderer resolver. */
BOOL SudekiMpLanArenaClientReplicaTestActorPresentationRenderer(
    void *character,
    unsigned int actor_index,
    void **renderer_result,
    void **ailish_component_result
);
/* Exact test seam for the attached-world/first-person model witness used by
 * the bounded Ailish combat transition. */
BOOL SudekiMpLanArenaClientReplicaTestAilishDesiredModelAttached(
    void *character,
    void **attached_wrapper_result,
    void **attached_renderer_result,
    BOOL *first_person_result,
    BOOL *fallback_world_result
);
/* Read-only admission seam: only OPPOSITE may enter the destructive native
 * model-switch wrapper; UNKNOWN and DESIRED both reject that mutation. */
int SudekiMpLanArenaClientReplicaTestAilishModelAttachmentState(
    void *character
);
BOOL SudekiMpLanArenaClientReplicaTestAilishModelRefreshAllowed(
    void *character
);
/* CPosition's native parent slot is an intrusive link biased four bytes from
 * the owning position address. */
BOOL SudekiMpLanArenaClientReplicaTestWeaponParentMatchesPosition(
    void *weapon,
    void *position
);
/* Read-only seams for the exact native mutation-admission graphs. They do not
 * invoke WeaponFollow or SetWeaponVisible and intentionally omit only the
 * runtime cleanroom-actor lookup so isolated fixtures can exercise every
 * pointer, writability, and callback precondition. */
BOOL SudekiMpLanArenaClientReplicaTestAilishWeaponReattachMutationAllowed(
    void *character
);
BOOL SudekiMpLanArenaClientReplicaTestAilishWeaponVisibilityMutationAllowed(
    void *character
);
/* Exercises the otherwise lease-empty reentrant window while native CSkill
 * activation still owns Tal on the call stack. */
BOOL SudekiMpLanArenaClientReplicaTestRemoteTalReleaseActivationEntryBlocked(
    void
);
#endif

#endif
