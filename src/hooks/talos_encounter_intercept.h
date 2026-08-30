#ifndef SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_H
#define SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_H

#include <stdint.h>

enum {
    SUDEKIMP_TALOS_ENCOUNTER_HERO_COUNT = 4u,
    SUDEKIMP_TALOS_ENCOUNTER_HUMAN_MASK = 0x0fu,
    SUDEKIMP_TALOS_ENCOUNTER_HOST_BIT = 0x01u,
    SUDEKIMP_TALOS_ENCOUNTER_HOST_HERO_TAL = 0u,
    SUDEKIMP_TALOS_ENCOUNTER_DESTINATION_CAPACITY = 16u,
    SUDEKIMP_TALOS_ENCOUNTER_BASE_HP = 45000u,
    SUDEKIMP_TALOS_ENCOUNTER_EXPANDED_HP = 180000u
};

/* Exact SOLWORLDM.gex facts for the supported GOG data image
 * e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9.
 *
 * Serialized bytecode starts at raw file offset 0x27E6C. This mapping is
 * independently anchored by the live-runtime IsPlaying and
 * FireMissileScripted operands. All bytecode offsets below are logical SOL
 * offsets (raw file offset minus 0x27E6C), not raw GEX file offsets.
 *
 * CC_NPC_Caprine_TalkingT3|PP starts the authored LoadTheVoid scene with
 * opcode 0x29. In LoadTheVoid, raw bytes
 * `06 5f 72 b5 48 27 14 71 fc 76` put the Void resource through raw 0x497D9,
 * so the SetZone|S opcode 0x27 begins at raw 0x497DA/logical 0x2196E. That
 * wrapper calls native SetZoneNOW|S with opcode 0x27 and then waits on
 * IsZoneAndDoglegsLoaded|S. These constants are passive observation points
 * only. They do not make the generic native-dispatch return site safe. */
#define SUDEKIMP_TALOS_SOL_SOURCE_ACTION_HASH UINT32_C(0xfac73f18)
#define SUDEKIMP_TALOS_SOL_FMA07_LITERAL_HASH UINT32_C(0xbc28d699)
#define SUDEKIMP_TALOS_SOL_VOID_RESOURCE_HASH UINT32_C(0x48b5725f)
#define SUDEKIMP_TALOS_SOL_LOAD_VOID_HASH UINT32_C(0x70f470c2)
#define SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_HASH UINT32_C(0x76fc7114)
#define SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_HASH UINT32_C(0xbc8fdc32)

enum {
    SUDEKIMP_TALOS_SOL_BYTECODE_FILE_BASE = 0x00027e6cu,
    SUDEKIMP_TALOS_SOL_SOURCE_ACTION_START = 0x00021b76u,
    SUDEKIMP_TALOS_SOL_SOURCE_ACTION_END = 0x00021d85u,
    SUDEKIMP_TALOS_SOL_LOAD_VOID_OPCODE_OFFSET = 0x00021c0cu,
    SUDEKIMP_TALOS_SOL_LOAD_VOID_START = 0x000218f3u,
    SUDEKIMP_TALOS_SOL_LOAD_VOID_END = 0x00021b08u,
    SUDEKIMP_TALOS_SOL_LOAD_VOID_SET_ZONE_OPCODE_OFFSET = 0x0002196eu,
    SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_START = 0x00003139u,
    SUDEKIMP_TALOS_SOL_SET_ZONE_WRAPPER_END = 0x000031c2u,
    SUDEKIMP_TALOS_SOL_NATIVE_SET_ZONE_OPCODE_OFFSET = 0x0000317cu,
    SUDEKIMP_TALOS_SOL_CALL_OPCODE_RVA = 0x001c4970u,
    SUDEKIMP_TALOS_SOL_CALL_OPCODE_SLOT_RVA = 0x00323fa0u,
    SUDEKIMP_TALOS_NATIVE_BINDING_RETURN_RVA = 0x002352b1u,
    SUDEKIMP_TALOS_LINEAGE_MAX_AGE_MS = 180000u
};

/*
 * The supported executable currently exposes SetZoneNOW through its export
 * table, and the exact image contains neither a direct code caller nor an
 * ASCII "Void" literal. Exact asset analysis identifies the source action,
 * LoadTheVoid, SetZone|S, and native SetZoneNOW|S chain, but no live trace has
 * yet proven its task/call-stack ancestry. Until that lineage and a replay
 * boundary are proven, the production exact-callsite allowlist stays empty.
 */
enum {
    SUDEKIMP_TALOS_ENCOUNTER_PRODUCTION_CONTINUATION_SUPPORTED = 0u
};

typedef enum SudekiMpTalosEncounterInterceptState {
    SUDEKIMP_TALOS_INTERCEPT_DISABLED = 0,
    SUDEKIMP_TALOS_INTERCEPT_IDLE,
    SUDEKIMP_TALOS_INTERCEPT_OBSERVED_ONLY,
    SUDEKIMP_TALOS_INTERCEPT_AWAITING_PROMPT,
    SUDEKIMP_TALOS_INTERCEPT_AWAITING_HOST,
    SUDEKIMP_TALOS_INTERCEPT_READY,
    SUDEKIMP_TALOS_INTERCEPT_CONTINUATION_CLAIMED,
    SUDEKIMP_TALOS_INTERCEPT_COMPLETED,
    SUDEKIMP_TALOS_INTERCEPT_CANCELLED,
    SUDEKIMP_TALOS_INTERCEPT_QUARANTINED
} SudekiMpTalosEncounterInterceptState;

/* The observation result tells the SetZoneNOW adapter what to do with the
 * current native call. Only DEFER_NATIVE permits a later continuation. */
typedef enum SudekiMpTalosEncounterObserveResult {
    SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE = 0,
    SUDEKIMP_TALOS_OBSERVE_PASS_NATIVE_OBSERVED,
    SUDEKIMP_TALOS_OBSERVE_DEFER_NATIVE,
    SUDEKIMP_TALOS_OBSERVE_DROP_NATIVE_BUSY
} SudekiMpTalosEncounterObserveResult;

typedef enum SudekiMpTalosEncounterCommandResult {
    SUDEKIMP_TALOS_COMMAND_NO_CHANGE = 0,
    SUDEKIMP_TALOS_COMMAND_ACCEPTED,
    SUDEKIMP_TALOS_COMMAND_REJECTED_INVALID,
    SUDEKIMP_TALOS_COMMAND_REJECTED_STALE,
    SUDEKIMP_TALOS_COMMAND_REJECTED_STATE,
    SUDEKIMP_TALOS_COMMAND_QUARANTINED
} SudekiMpTalosEncounterCommandResult;

typedef enum SudekiMpTalosEncounterServiceAction {
    SUDEKIMP_TALOS_SERVICE_NONE = 0,
    SUDEKIMP_TALOS_SERVICE_CONTINUE_NATIVE_ONCE,
    SUDEKIMP_TALOS_SERVICE_DISCARD_DEFERRED
} SudekiMpTalosEncounterServiceAction;

/* Pointer-free lifecycle identity. The integration layer obtains the source
 * generation/world identity from SudekiMpZoneTransitionGetSourceSnapshot and
 * supplies independently proven world and Tal lease generations. */
typedef struct SudekiMpTalosTransitionProvenance {
    uint32_t world_generation;
    uint32_t source_generation;
    uint32_t host_actor_generation;
    uint32_t host_lease_generation;
    uint64_t world_identity;
    uint64_t host_actor_identity;
} SudekiMpTalosTransitionProvenance;

/* Short-lived identities for passive SOL lineage observation. These integer
 * tokens must never be dereferenced or serialized. The adapter must assign a
 * new nonzero generation whenever a native thread/task address is reused.
 * A descendant scene task names the initiating action task as root_task. */
typedef struct SudekiMpTalosSolTaskLineage {
    uint64_t sol_thread_identity;
    uint64_t task_identity;
    uint64_t root_task_identity;
    uint32_t sol_thread_generation;
    uint32_t task_generation;
    uint32_t root_task_generation;
    uint32_t script_runtime_generation;
    uint32_t native_thread_id;
} SudekiMpTalosSolTaskLineage;

typedef struct SudekiMpTalosLoadVoidObservation {
    SudekiMpTalosTransitionProvenance provenance;
    SudekiMpTalosSolTaskLineage lineage;
    uint32_t observed_at_ms;
    uint32_t source_action_hash;
    uint32_t source_action_start;
    uint32_t opcode_offset;
    uint32_t scene_task_hash;
    uint8_t exact_build_confirmed;
    uint8_t interaction_authority_proven;
    uint8_t reserved[2];
} SudekiMpTalosLoadVoidObservation;

typedef struct SudekiMpTalosSetZoneCarrierObservation {
    SudekiMpTalosTransitionProvenance provenance;
    SudekiMpTalosSolTaskLineage lineage;
    uint32_t observed_at_ms;
    uint32_t caller_function_hash;
    uint32_t caller_opcode_offset;
    uint32_t function_hash;
    uint32_t function_start;
    uint32_t opcode_offset;
    uint32_t binding_hash;
    uint8_t exact_build_confirmed;
    uint8_t reserved[3];
    char destination[SUDEKIMP_TALOS_ENCOUNTER_DESTINATION_CAPACITY];
} SudekiMpTalosSetZoneCarrierObservation;

typedef enum SudekiMpTalosLineageState {
    SUDEKIMP_TALOS_LINEAGE_DISABLED = 0,
    SUDEKIMP_TALOS_LINEAGE_IDLE,
    SUDEKIMP_TALOS_LINEAGE_LOAD_VOID_OBSERVED,
    SUDEKIMP_TALOS_LINEAGE_CARRIER_MATCHED,
    SUDEKIMP_TALOS_LINEAGE_QUARANTINED
} SudekiMpTalosLineageState;

typedef enum SudekiMpTalosLineageResult {
    SUDEKIMP_TALOS_LINEAGE_RESULT_DISABLED = 0,
    SUDEKIMP_TALOS_LINEAGE_RESULT_NO_CHANGE,
    SUDEKIMP_TALOS_LINEAGE_RESULT_LOAD_VOID_RECORDED,
    SUDEKIMP_TALOS_LINEAGE_RESULT_CARRIER_MATCHED,
    SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_INVALID,
    SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_NOT_EXACT,
    SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STATE,
    SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_STALE,
    SUDEKIMP_TALOS_LINEAGE_RESULT_REJECTED_LINEAGE
} SudekiMpTalosLineageResult;

typedef struct SudekiMpTalosLineageTracker {
    SudekiMpTalosLoadVoidObservation load_void;
    SudekiMpTalosSetZoneCarrierObservation carrier;
    SudekiMpTalosLineageState state;
    SudekiMpTalosLineageResult last_result;
    uint32_t serial;
    uint8_t enabled;
    uint8_t reserved[3];
} SudekiMpTalosLineageTracker;

typedef struct SudekiMpTalosLineageSnapshot {
    SudekiMpTalosLoadVoidObservation load_void;
    SudekiMpTalosSetZoneCarrierObservation carrier;
    uint32_t state;
    uint32_t last_result;
    uint32_t serial;
    uint8_t enabled;
    uint8_t exact_carrier_matched;
    uint8_t production_continuation_supported;
    uint8_t reserved;
} SudekiMpTalosLineageSnapshot;

/* Passive research tracker. Even a CARRIER_MATCHED result is evidence only:
 * it cannot arm SudekiMpTalosEncounterInterceptObserveSetZoneNow and never
 * suppresses, defers, invokes, or replays a native function. */
void SudekiMpTalosLineageInitialize(SudekiMpTalosLineageTracker *tracker);
void SudekiMpTalosLineageConfigure(
    SudekiMpTalosLineageTracker *tracker,
    int enabled
);
SudekiMpTalosLineageResult SudekiMpTalosLineageObserveLoadVoid(
    SudekiMpTalosLineageTracker *tracker,
    const SudekiMpTalosLoadVoidObservation *observation
);
SudekiMpTalosLineageResult SudekiMpTalosLineageObserveSetZoneCarrier(
    SudekiMpTalosLineageTracker *tracker,
    const SudekiMpTalosSetZoneCarrierObservation *observation
);
int SudekiMpTalosLineageGetSnapshot(
    const SudekiMpTalosLineageTracker *tracker,
    SudekiMpTalosLineageSnapshot *snapshot
);
void SudekiMpTalosLineageReset(SudekiMpTalosLineageTracker *tracker);

/* The adapter must defensively copy the native zone string before calling
 * this API. No native pointer or callback is retained by this module. */
typedef struct SudekiMpTalosEncounterSetZoneObservation {
    SudekiMpTalosTransitionProvenance provenance;
    uint32_t callsite_rva;
    uint8_t exact_build_confirmed;
    uint8_t host_hero;
    uint8_t active_human_mask;
    uint8_t reserved;
    char destination[SUDEKIMP_TALOS_ENCOUNTER_DESTINATION_CAPACITY];
} SudekiMpTalosEncounterSetZoneObservation;

/* Pointer-free presentation state. A renderer may copy this structure and
 * must answer with the exact serial; prompt_active is never set by the current
 * production build because no safe pre-action continuation seam is proven. */
typedef struct SudekiMpTalosEncounterPromptSnapshot {
    SudekiMpTalosTransitionProvenance provenance;
    uint32_t state;
    uint32_t serial;
    uint32_t callsite_rva;
    uint32_t party_size;
    uint32_t talos_hp;
    uint8_t active_human_mask;
    uint8_t human_count;
    uint8_t continuation_supported;
    uint8_t prompt_active;
    uint8_t prompt_visible;
    uint8_t native_deferred;
    uint8_t host_confirmed;
    uint8_t terminal;
    char destination[SUDEKIMP_TALOS_ENCOUNTER_DESTINATION_CAPACITY];
} SudekiMpTalosEncounterPromptSnapshot;

typedef struct SudekiMpTalosEncounterIntercept {
    SudekiMpTalosTransitionProvenance provenance;
    SudekiMpTalosEncounterInterceptState state;
    uint32_t serial;
    uint32_t callsite_rva;
    uint8_t enabled;
    uint8_t active_human_mask;
    uint8_t prompt_visible;
    uint8_t native_deferred;
    uint8_t host_confirmed;
    uint8_t discard_reported;
    uint8_t reserved[2];
    char destination[SUDEKIMP_TALOS_ENCOUNTER_DESTINATION_CAPACITY];
} SudekiMpTalosEncounterIntercept;

void SudekiMpTalosEncounterInterceptInitialize(
    SudekiMpTalosEncounterIntercept *intercept
);

/* Configuration is inert by default. Disabling while a call is deferred
 * permanently discards that call; it can never cause a late native replay. */
void SudekiMpTalosEncounterInterceptConfigure(
    SudekiMpTalosEncounterIntercept *intercept,
    int enabled
);

SudekiMpTalosEncounterObserveResult
SudekiMpTalosEncounterInterceptObserveSetZoneNow(
    SudekiMpTalosEncounterIntercept *intercept,
    const SudekiMpTalosEncounterSetZoneObservation *observation
);

int SudekiMpTalosEncounterInterceptGetPromptSnapshot(
    const SudekiMpTalosEncounterIntercept *intercept,
    SudekiMpTalosEncounterPromptSnapshot *snapshot
);

/* A failed/missing presentation must be reported as visible=0, which cancels
 * a hypothetical deferred request without running SetZoneNOW. */
SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptReportPrompt(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial,
    int visible
);

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptHostConfirm(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial,
    const SudekiMpTalosTransitionProvenance *current
);

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptHostCancel(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial
);

/* Revalidates world/source/Tal lease identity immediately before granting the
 * one continuation action. Invalid or missing provenance discards the load. */
SudekiMpTalosEncounterServiceAction
SudekiMpTalosEncounterInterceptService(
    SudekiMpTalosEncounterIntercept *intercept,
    const SudekiMpTalosTransitionProvenance *current
);

SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptFinishContinuation(
    SudekiMpTalosEncounterIntercept *intercept,
    uint32_t serial,
    int succeeded
);

/* Reset is allowed only when no native call remains retained. Cancellation
 * and quarantine therefore require Service() to report DISCARD_DEFERRED
 * first; an in-flight claimed continuation requires FinishContinuation().
 * Reset preserves the monotonic serial. */
SudekiMpTalosEncounterCommandResult
SudekiMpTalosEncounterInterceptReset(
    SudekiMpTalosEncounterIntercept *intercept
);

#ifdef SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_TESTING
enum {
    SUDEKIMP_TALOS_ENCOUNTER_TEST_PROVEN_CALLSITE_RVA = 0x00abcdefu
};
#endif

#endif
