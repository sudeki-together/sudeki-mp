#ifndef SUDEKIMP_TALOS_NATIVE_LIFECYCLE_TRACE_H
#define SUDEKIMP_TALOS_NATIVE_LIFECYCLE_TRACE_H

#include <windows.h>
#include <stdint.h>

typedef enum SudekiMpTalosNativeLifecycleEvent {
    SUDEKIMP_TALOS_NATIVE_EVENT_NONE = 0,
    SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID,
    SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI,
    SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH,
    SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO,
    SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_CARRIER,
    SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_NOW,
    SUDEKIMP_TALOS_NATIVE_EVENT_END_TSA,
    SUDEKIMP_TALOS_NATIVE_EVENT_REMOVE_ALL_PLAYERS,
    SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID_TASK_CREATED,
    SUDEKIMP_TALOS_NATIVE_EVENT_FORMATION_POP_MEMBERS,
    SUDEKIMP_TALOS_NATIVE_EVENT_TSA_BECAME_INACTIVE,
    SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE,
    SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER,
    SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC,
    SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL,
    SUDEKIMP_TALOS_NATIVE_EVENT_KAZEL_GROUP_ADD
} SudekiMpTalosNativeLifecycleEvent;

typedef enum SudekiMpTalosNativeHeroIdentity {
    SUDEKIMP_TALOS_NATIVE_HERO_TAL = 0,
    SUDEKIMP_TALOS_NATIVE_HERO_AILISH = 1,
    SUDEKIMP_TALOS_NATIVE_HERO_BUKI = 2,
    SUDEKIMP_TALOS_NATIVE_HERO_ELCO = 3,
    SUDEKIMP_TALOS_NATIVE_HERO_COUNT = 4,
    SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN = 255
} SudekiMpTalosNativeHeroIdentity;

enum {
    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_IDLE = 0,
    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_BUKI_REMOVED = 1,
    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_AILISH_REMOVED = 2,
    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED = 3,
    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_RELEASED = 4,
    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_QUARANTINED = 5
};

enum {
    SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE = 0,
    SUDEKIMP_TALOS_NATIVE_KAZEL_SPAWN_ARMED = 1,
    SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_ACTIVE = 2,
    SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_CORROBORATED = 3,
    SUDEKIMP_TALOS_NATIVE_KAZEL_DELETE_CORROBORATED = 4,
    SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED = 5
};

typedef struct SudekiMpTalosNativeRosterIdentitySnapshot {
    uint32_t observation_serial;
    uint32_t script_runtime_generation;
    uint32_t load_void_task_generation;
    uint32_t roster_revision;
    uint32_t hero_roster_lease_generation[4];
    uint64_t hero_token[4];
    uint8_t hero_present_mask;
    uint8_t group_hero_mask;
    uint8_t formation_hero_mask;
    uint8_t sequence_state;
    uint8_t quarantine_reason;
    uint8_t immediate_identity_complete;
    uint8_t delete_delta_corroborated_mask;
    uint8_t actor_lifetime_authority_proven;
} SudekiMpTalosNativeRosterIdentitySnapshot;

/* Pointer-free one-shot evidence captured when the retail Void transition
 * returns to stable gameplay. Generations are observational correlation
 * counters, never ownership or actor-lifetime authority. */
typedef struct SudekiMpTalosNativeSettleEvidenceSnapshot {
    uint32_t session_generation;
    uint32_t script_runtime_generation;
    uint32_t load_void_task_generation;
    uint32_t camera_observation_generation;
    uint32_t default_camera_generation;
    uint32_t settle_validation_generation;
    uint8_t void_set_zone_completed;
    uint8_t default_camera_committed;
    uint8_t default_camera_revalidated;
    uint8_t tal_control_revalidated;
    uint8_t settle_evidence_complete;
    uint8_t reserved[3];
} SudekiMpTalosNativeSettleEvidenceSnapshot;

/* Pointer-free evidence for the authored transient PC_KAZEL lifecycle. The
 * request is armed from the exact serialized SOL opcode sequence and
 * correlated with the native raw-group-add call by opaque equality tokens
 * only. */
typedef struct SudekiMpTalosNativeKazelSnapshot {
    uint32_t session_generation;
    uint32_t observation_serial;
    uint32_t request_generation;
    uint32_t script_runtime_generation;
    uint32_t load_void_task_generation;
    uint32_t source_native_thread_id;
    uint32_t completion_native_thread_id;
    uint64_t original_tal_token;
    uint64_t kazel_token;
    uint8_t state;
    uint8_t spawn_binding_before_seen;
    uint8_t spawn_binding_after_seen;
    uint8_t group_add_before_seen;
    uint8_t group_add_after_seen;
    uint8_t exact_dark_tal_identity;
    uint8_t group_add_corroborated;
    uint8_t delete_corroborated;
    uint8_t completion_was_synchronous;
    uint8_t serialized_opcode_mask;
    uint8_t ambiguity_reason;
    uint8_t actor_lifetime_authority_proven;
    uint8_t reserved[2];
} SudekiMpTalosNativeKazelSnapshot;

typedef struct SudekiMpTalosNativeLifecycleSnapshot {
    uint32_t run_id_high;
    uint32_t run_id_low;
    uint32_t event_serial;
    uint32_t last_native_thread_id;
    uint32_t last_operand_offset;
    uint32_t last_binding_hash;
    uint32_t last_handler_result;
    uint32_t observed_event_mask;
    uint32_t rejected_observation_count;
    uint32_t opcode_29_depth;
    uint32_t opcode_27_depth;
    uint32_t task_constructor_return_count;
    uint32_t source_thread_generation;
    uint32_t load_void_task_generation;
    uint32_t load_void_thread_generation;
    uint32_t script_runtime_generation;
    uint32_t delete_pc_native_before_count;
    uint32_t delete_pc_native_after_count;
    uint32_t remove_all_players_before_count;
    uint32_t remove_all_players_after_count;
    uint32_t last_delete_resource_kind;
    uint32_t last_delete_resource_identifier;
    uint32_t last_delete_recomputed_identifier;
    uint32_t last_delete_caller_rva;
    uint32_t group_count_before_remove_all;
    uint32_t group_count_after_remove_all;
    uint32_t formation_count_before_remove_all;
    uint32_t formation_count_after_remove_all;
    uint32_t formation_pop_before_count;
    uint32_t formation_pop_after_count;
    uint32_t tsa_set_playing_observation_count;
    uint32_t tsa_inactive_caller_rva;
    uint8_t installed;
    uint8_t set_zone_before_seen;
    uint8_t native_passthrough_required;
    uint8_t mutation_supported;
    uint8_t load_void_task_bound;
    uint8_t load_void_descendant_observed;
    uint8_t interaction_authority_proven;
    uint8_t remove_all_verified_empty;
    uint8_t last_tsa_playing;
    uint8_t last_delete_resource_backing_valid;
    uint8_t last_delete_resource_name_matches;
    uint8_t tsa_completion_armed;
    uint8_t tsa_inactive_observed;
    uint8_t last_tsa_requested;
    uint8_t reserved[1];
} SudekiMpTalosNativeLifecycleSnapshot;

enum {
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_DISABLED = 0,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING = 1,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE = 2,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY = 3,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED = 4,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED = 5
};

typedef enum SudekiMpTalosNativePostMovieTicketClaimResult {
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_NOT_READY = 0,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED = 1,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_DISABLED = 2,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_REPLAY = 3,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_INVALID = 4,
    SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED = 5
} SudekiMpTalosNativePostMovieTicketClaimResult;

/* Pointer-free authorization for one post-movie companion restoration
 * attempt. Tokens are opaque equality evidence and must never be decoded or
 * treated as actor addresses. The ticket is consumed when claimed and has no
 * reset/replay path in the process. */
typedef struct SudekiMpTalosNativePostMovieRestoreTicket {
    uint32_t run_id_high;
    uint32_t run_id_low;
    uint32_t authorization_generation;
    uint32_t lifecycle_event_serial;
    uint32_t roster_observation_serial;
    uint32_t roster_revision;
    uint32_t kazel_observation_serial;
    uint32_t kazel_request_generation;
    uint32_t session_generation;
    uint32_t script_runtime_generation;
    uint32_t load_void_task_generation;
    uint32_t settle_validation_generation;
    uint32_t default_camera_generation;
    uint32_t native_thread_id;
    uint64_t tal_token;
    uint64_t kazel_token;
} SudekiMpTalosNativePostMovieRestoreTicket;

/* Pure exact-asset classifier. The operand offset is the value at SOL thread
 * +0x0C before the original handler advances it; the opcode byte is at
 * operand_offset-1. Returning NONE never changes the native call path. */
SudekiMpTalosNativeLifecycleEvent
SudekiMpTalosNativeLifecycleClassify(
    uint8_t opcode,
    uint32_t operand_offset,
    uint32_t binding_hash
);

/* Pure correlation for the exact ResourceName identity passed to native
 * DeletePC. Script resource table indices (Buki=4, Ailish=3, Elco=5) are not
 * the native identifier: DeletePC receives the case-folded PC_* hash plus a
 * reference-backed name. This is passive evidence and never authorizes a
 * skipped call. encoded_kind is logged separately until its live lazy-state
 * values are proven.
 */
uint32_t SudekiMpTalosNativeLifecycleResourceIdentifier(
    const char *resource_text
);
BOOL SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
    SudekiMpTalosNativeLifecycleEvent event,
    uint32_t identifier,
    const char *resource_text,
    BOOL backing_valid,
    BOOL name_readable
);

/* Pure evidence rule for the asynchronous EndTSA completion edge. The
 * observer accepts only the exact nested TSASetPlaying(false) SOL binding
 * after an exact lineage-bound EndTSA source edge, on the same script runtime
 * generation and native thread, plus a native true->false state change. The
 * setter task itself remains observational until child-task lineage is proven.
 * This rule never calls or suppresses native code. */
BOOL SudekiMpTalosNativeLifecycleTsaInactiveEvidencePolicy(
    BOOL armed,
    uint8_t opcode,
    uint32_t operand_offset,
    uint32_t binding_hash,
    BOOL runtime_generation_matched,
    BOOL native_thread_matched,
    BOOL script_task_lineage_matched,
    uint8_t requested,
    BOOL before_playing,
    BOOL after_playing
);

/* Pure fail-closed seams for the passive camera/control observer. */
BOOL SudekiMpTalosNativeLifecycleDefaultCameraEvidencePolicy(
    BOOL void_set_zone_completed,
    BOOL runtime_generation_matched,
    BOOL load_void_task_generation_matched,
    BOOL native_thread_matched,
    BOOL native_call_succeeded,
    BOOL exact_default_name,
    BOOL committed_camera_state_valid
);
BOOL SudekiMpTalosNativeLifecycleSettleEvidencePolicy(
    BOOL exact_original_tal_survivor,
    BOOL default_camera_committed,
    BOOL default_camera_revalidated,
    BOOL tal_control_revalidated
);

/* Pure, pointer-free policy for the exact post-movie authorization boundary.
 * It intentionally does not require Kazel's native completion to have been
 * synchronous. settle_native_thread_id is kept private by the live tracker;
 * exposing it as an input here makes the complete thread rule testable
 * without publishing it in an observational snapshot. */
BOOL SudekiMpTalosNativeLifecyclePostMovieRestoreTicketPolicy(
    BOOL allow_post_movie_restore_ticket,
    BOOL exact_asset_authenticated,
    const SudekiMpTalosNativeLifecycleSnapshot *lifecycle,
    const SudekiMpTalosNativeRosterIdentitySnapshot *roster,
    const SudekiMpTalosNativeKazelSnapshot *kazel,
    const SudekiMpTalosNativeSettleEvidenceSnapshot *settle,
    uint32_t current_native_thread_id,
    uint32_t settle_native_thread_id
);

/* Pure revalidation for a ticket whose complete settle boundary was already
 * captured. Later normal non-default camera commits may clear volatile settle
 * flags, so READY claims revalidate immutable run/generation/session/thread,
 * Tal-only roster, and Kazel-delete provenance instead of demanding the
 * historical default camera remain current. */
BOOL SudekiMpTalosNativeLifecyclePostMovieRestoreReadyClaimPolicy(
    const SudekiMpTalosNativePostMovieRestoreTicket *ticket,
    const SudekiMpTalosNativeLifecycleSnapshot *lifecycle,
    const SudekiMpTalosNativeRosterIdentitySnapshot *roster,
    const SudekiMpTalosNativeKazelSnapshot *kazel,
    const SudekiMpTalosNativeSettleEvidenceSnapshot *settle,
    uint32_t current_native_thread_id,
    uint32_t settle_native_thread_id
);

/* Pure terminal-state rule used by the live claim seam. NOT_READY does not
 * consume the attempt. Only READY plus exact immutable evidence consumes
 * exactly once; ACTIVE remains pollable until try_arm publishes a complete
 * ticket. A lost ready condition, explicit contradiction, invalid state, or
 * prior quarantine fails closed. */
SudekiMpTalosNativePostMovieTicketClaimResult
SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
    uint8_t current_state,
    BOOL exact_evidence_ready,
    BOOL irreversible_mismatch,
    uint8_t *next_state
);

/* Pure policy for the passive raw-group-add edge. Tokens are opaque equality
 * values, not retained pointers or actor-lifetime authority. */
BOOL SudekiMpTalosNativeLifecycleKazelGroupAddEvidencePolicy(
    BOOL pending_exact_request,
    BOOL runtime_generation_matched,
    BOOL load_void_task_generation_matched,
    BOOL native_thread_matched,
    BOOL group_argument_matches_active,
    BOOL exact_dark_tal_identity,
    uint32_t group_count_before,
    uint32_t group_count_after,
    uint32_t formation_count_before,
    uint32_t formation_count_after,
    uint64_t original_tal_token,
    uint64_t group_added_token,
    uint64_t formation_added_token,
    uint64_t raw_actor_token
);

/* One final-battle lifecycle may contribute Kazel evidence per process. A
 * later LoadVoid must start in a fresh process so an old asynchronous native
 * completion can never be attributed to a newer request. */
BOOL SudekiMpTalosNativeLifecycleKazelSessionStartPolicy(
    uint32_t prior_session_generation
);

/* Pure ordered policy for the authored, serialized
 * TalKazelMerge -> SpawnPC -> InternalSpawnPC opcode edges. */
BOOL SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
    uint8_t current_mask,
    SudekiMpTalosNativeLifecycleEvent event,
    BOOL before,
    BOOL exact_context,
    uint8_t *next_mask
);

/* Pure relocated-vtable classifier. It compares the complete actor vtable
 * triplet and never invokes a virtual method or engine allocator. */
SudekiMpTalosNativeHeroIdentity
SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
    uint32_t main_vtable_rva,
    uint32_t secondary_vtable_rva,
    uint32_t resource_vtable_rva
);
BOOL SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
    uint32_t main_vtable_rva,
    uint32_t secondary_vtable_rva,
    uint32_t resource_vtable_rva
);

/* Default-off exact-build observer. When enabled it owns the raw opcode 0x29
 * and 0x27 dispatch slots plus exact native observation detours exclusively,
 * observes copied evidence, and invokes each original exactly once. The
 * owning DLL is pinned before the first patch; production installation and
 * teardown are startup/rollback operations, never dynamic gameplay actions.
 * The two-argument installer has no continuation or mutation API. The
 * separately named extended installer may expose one pointer-free
 * authorization ticket, but this module still performs no native mutation.
 */
BOOL SudekiMpInstallTalosNativeLifecycleTrace(
    HMODULE game_module,
    BOOL enabled
);
/* Explicit mutation-adjacent startup seam. The existing two-argument
 * installer remains ABI-compatible and calls this path with both ticket gates
 * false. A ticket can be armed only when both startup gates are true and the
 * exact image has also passed the native signature checks. */
BOOL SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
    HMODULE game_module,
    BOOL enabled,
    BOOL allow_post_movie_restore_ticket,
    BOOL exact_asset_authenticated
);
void SudekiMpUninstallTalosNativeLifecycleTrace(void);

/* Called only by the already-owned SetZoneNOW inline wrapper. The BEFORE call
 * must be the first operation in that wrapper, before the zone source
 * generation changes. Both functions are inert unless an exact nested native
 * SetZoneNOW SOL opcode is active on the same OS thread. */
void SudekiMpTalosNativeLifecycleObserveSetZoneNowBefore(
    const char *zone_name
);
void SudekiMpTalosNativeLifecycleObserveSetZoneNowAfter(
    const char *zone_name
);
/* Called by the already-owned SetRenderCamera wrapper immediately after its
 * native call. manager and camera_name are borrowed for this call only; the
 * observer copies bounded text and never retains either pointer. */
void SudekiMpTalosNativeLifecycleObserveRenderCameraAfter(
    const void *manager,
    const char *camera_name,
    BOOL native_result
);

BOOL SudekiMpTalosNativeLifecycleGetSnapshot(
    SudekiMpTalosNativeLifecycleSnapshot *snapshot
);
/* Pointer-free observational roster evidence. Lease generations advance on
 * accepted membership edges, not on proven allocation events; consequently
 * actor_lifetime_authority_proven is deliberately always false. */
BOOL SudekiMpTalosNativeLifecycleGetRosterIdentitySnapshot(
    SudekiMpTalosNativeRosterIdentitySnapshot *snapshot
);
BOOL SudekiMpTalosNativeLifecycleGetSettleEvidenceSnapshot(
    SudekiMpTalosNativeSettleEvidenceSnapshot *snapshot
);
BOOL SudekiMpTalosNativeLifecycleGetKazelSnapshot(
    SudekiMpTalosNativeKazelSnapshot *snapshot
);

/* Called only from an already-owned controller-update observer after the
 * original controller update returns. A false result is non-consuming while
 * evidence is merely incomplete; claimed, replayed, or quarantined attempts
 * are process-terminal. */
BOOL SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(
    SudekiMpTalosNativePostMovieRestoreTicket *ticket
);

#endif
