#ifndef SUDEKIMP_TALOS_COMPANION_STAGING_RESEARCH_ADAPTER_H
#define SUDEKIMP_TALOS_COMPANION_STAGING_RESEARCH_ADAPTER_H

#include "engine/talos_companion_staging_research.h"
#include "hooks/talos_companion_staging_native_sampler.h"

#include <windows.h>

/* This adapter is a disposable, ordinary-world research instrument. It is
 * deliberately unrelated to the production Talos carry coordinator and it
 * never grants production, carry, or actor-lifetime authority. */
typedef enum SudekiMpTalosStagingResearchAdapterFailure {
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE = 0,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_INSTALL_ARGUMENT,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_IMAGE_ABI,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DLL_PIN,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_SAMPLE,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_GET_PC,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_WRAPPER,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESOLVE,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_MEMBERSHIP,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_COORDINATOR,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RANGE_PREFLIGHT,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESTORE,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_STABILITY,
    SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_WRAPPER_DESTROY
} SudekiMpTalosStagingResearchAdapterFailure;

typedef enum SudekiMpTalosStagingResearchAdapterStage {
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_NONE = 0,
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT,
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_REMOVE_AUTHORIZATION,
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE,
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_INDEPENDENT,
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_RESTORE_IMMEDIATE,
    SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_STABILITY
} SudekiMpTalosStagingResearchAdapterStage;

/* Pointer-free public evidence. Tokens are salted equality witnesses, never
 * addresses or native-call continuations. A reported remove always makes the
 * process reload-required, including the apparent-success case. */
typedef struct SudekiMpTalosStagingResearchAdapterStatus {
    uint64_t status_serial;
    uint64_t attempt_serial;
    uint32_t coordinator_state;
    uint32_t coordinator_result;
    uint32_t failure;
    uint32_t last_stage;
    uint32_t abi_failure;
    uint32_t abi_failed_symbol;
    uint32_t abi_checks_completed;
    uint32_t observation_calls;
    uint32_t observation_failure;
    uint32_t observation_failed_address;
    uint32_t observation_failed_size;
    uint32_t observation_checks_completed;
    uint32_t observation_first_range_count;
    uint32_t observation_second_range_count;
    uint32_t observation_first_capture_bytes;
    uint32_t observation_second_capture_bytes;
    uint32_t observation_witness_native_thread_id;
    uint32_t observation_witness_registry_generation;
    uint32_t observation_witness_overlap_generation;
    uint64_t observation_witness_dispatch_serial;
    uint32_t service_calls;
    uint32_t sample_calls;
    uint32_t get_pc_calls;
    uint32_t resolve_calls;
    uint32_t tptr_cleanup_calls;
    uint32_t is_player_calls;
    uint32_t get_index_calls;
    uint32_t remove_calls;
    uint32_t add_calls;
    uint32_t wrapper_destroy_calls;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchSnapshot stable;
    uint8_t installed;
    uint8_t enabled;
    uint8_t active;
    uint8_t attempt_consumed;
    uint8_t exact_executable_hash;
    uint8_t exact_sol_hash;
    uint8_t abi_seams_valid;
    uint8_t external_sha256_required;
    uint8_t modal_active;
    uint8_t transition_active;
    uint8_t remove_reported;
    uint8_t reload_required;
    uint8_t quarantined;
    uint8_t success;
    uint8_t production_mutation_compiled;
    uint8_t observation_only;
    uint8_t observation_failed_write_required;
    uint8_t observation_witness_present;
    uint8_t observation_witness_entry_exact;
    uint8_t observation_witness_revalidated_exact;
    uint8_t observation_witness_source;
    uint8_t observation_last_contract_valid;
    uint8_t observation_valid;
} SudekiMpTalosStagingResearchAdapterStatus;

/* enabled=FALSE is a true no-op and accepts no hotkey or hash assertion. In
 * the normal DLL build, native membership mutation is structurally absent.
 * A zero proof_virtual_key selects the production observation-only profile;
 * nonzero hotkey-driven transactions exist only in TESTING builds. The
 * normal backend has no raw sampler and cannot fabricate callback or
 * transaction cookies.
 * exact_executable_hash and exact_sol_hash are authenticated external facts;
 * the mapped-image ABI validator does not replace either full-file hash. */
BOOL SudekiMpInstallTalosCompanionStagingResearchAdapter(
    HMODULE game_module,
    BOOL enabled,
    UINT proof_virtual_key,
    BOOL exact_executable_hash,
    BOOL exact_sol_hash
);

struct SudekiMpControlUpdateDispatchWitness;

/* Observer-compatible entry point. All three arguments are borrowed from the
 * owned controller wrapper and must not be retained. Production performs no
 * raw sampling and no coordinator or native membership call here. A legacy
 * hotkey transaction exists only for an injected TESTING backend. */
void SudekiMpTalosCompanionStagingResearchAdapterService(
    void *controller,
    void *update_data,
    const struct SudekiMpControlUpdateDispatchWitness *witness
);

/* Publish one immutable, pointer-free native sampler result. The borrowed
 * witness must be the exact witness used by the sampler. Invalid observations
 * still publish their diagnostics but never publish a partial snapshot in
 * status.original. The first valid snapshot is one-shot and sticky. TRUE
 * means this call published that first valid snapshot; the caller's
 * LastError value is always preserved. */
BOOL SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const struct SudekiMpControlUpdateDispatchWitness *witness
);

BOOL SudekiMpTalosCompanionStagingResearchAdapterGetStatus(
    SudekiMpTalosStagingResearchAdapterStatus *status
);

/* The integration layer owns callback quiescence: disable its observer gate,
 * unregister, drain the gate, then call this before releasing bridge state.
 * This publishes active=0 but is not itself a registry quiescence barrier.
 * Adapter backing remains process-lifetime because the DLL is pinned. */
void SudekiMpUninstallTalosCompanionStagingResearchAdapter(void);

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
typedef struct SudekiMpTalosStagingResearchAdapterRawSample {
    SudekiMpTalosStagingResearchSnapshot snapshot;
    void *source_descriptor;
    void *world;
    void *group;
    void *formation_owner;
    void *formation;
    void *controller;
    void *controller_callback;
    void *transaction;
    void *listener_storage;
    void *listener;
    void *ui_controller;
    void *hud_owner;
    void *ui_scene;
    void *elco_arbiter;
    void *front_actor;
    void *camera_manager;
    void *current_render_camera;
    void *render_state;
    void *scene_manager;
    void *scene_renderer;
    void *actor[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    void *control_component[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    void *control_owner_actor[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    void *formation_backpointer[
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    void *gizmo[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    void *stat_display[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    /* The one sampled group+0xd4 byte; mapping publishes it under both core
     * interpretations and never trusts independent snapshot copies. */
    uint8_t group_combat_d4;
    uint8_t topology_ranges_prevalidated;
    uint8_t mutation_ranges_prevalidated;
    uint8_t controller_callback_exact;
    uint8_t game_thread_exact;
    uint8_t transaction_exclusive;
    uint8_t no_yield_window_exact;
    uint8_t listener_callback_closure_exact;
    uint8_t ui_hud_closure_exact;
    uint8_t hero_hud_state_converged;
    uint8_t elco_arbiter_safe;
} SudekiMpTalosStagingResearchAdapterRawSample;

typedef struct SudekiMpTalosStagingResearchAdapterTestBackend {
    void *context;
    BOOL (*hotkey_down)(void *context, UINT virtual_key);
    BOOL (*sample)(
        void *context,
        SudekiMpTalosStagingResearchAdapterStage stage,
        void *callback_controller,
        void *callback_update_data,
        const struct SudekiMpControlUpdateDispatchWitness *witness,
        SudekiMpTalosStagingResearchAdapterRawSample *sample
    );
    BOOL (*prevalidate_mutation)(
        void *context,
        void *group,
        void *wrapper,
        void *actor
    );
    void *(*get_pc)(void *context, const char *resource_name);
    BOOL (*inspect_wrapper)(
        void *context,
        void *wrapper,
        void **embedded_actor
    );
    BOOL (*resolve_wrapper)(
        void *context,
        void *wrapper,
        void *tracked_tptr_12,
        void **actor
    );
    void (*cleanup_tptr)(void *context, void *tracked_tptr_12);
    BOOL (*is_player)(void *context, void *group, void *wrapper);
    int (*get_index)(void *context, void *group, void *wrapper);
    void (*remove_player)(void *context, void *group, void *wrapper);
    void (*add_player)(void *context, void *group, void *wrapper);
    BOOL (*destroy_wrapper)(void *context, void *wrapper, unsigned int flags);
    void (*publish)(
        void *context,
        const SudekiMpTalosStagingResearchAdapterStatus *status
    );
    uint8_t mutation_authority_available;
    uint8_t reserved[3];
} SudekiMpTalosStagingResearchAdapterTestBackend;

BOOL SudekiMpTalosCompanionStagingResearchAdapterSetBackendForTesting(
    const SudekiMpTalosStagingResearchAdapterTestBackend *backend
);
void SudekiMpTalosCompanionStagingResearchAdapterResetForTesting(void);
#endif

#endif
