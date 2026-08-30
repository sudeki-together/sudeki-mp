#ifndef SUDEKIMP_TALOS_COMPANION_STAGING_NATIVE_CAPTURE_H
#define SUDEKIMP_TALOS_COMPANION_STAGING_NATIVE_CAPTURE_H

#include "hooks/control_separation.h"
#include "hooks/talos_companion_staging_native_sampler.h"

#include <stdint.h>
#include <windows.h>

enum {
    SUDEKIMP_TALOS_NATIVE_CAPTURE_ARENA_BYTES =
        SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_CAPTURE_BYTES,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES =
        SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_RANGES,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_PLANNING_RETRIES =
        SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_RANGES,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_DEFAULT_RETRY_DISPATCHES = 120u
};

typedef enum SudekiMpTalosStagingNativeCaptureFailure {
    SUDEKIMP_TALOS_NATIVE_CAPTURE_OK = 0,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_ARGUMENT,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_NOT_CONFIGURED,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_BUSY,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_THROTTLED,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_ALIAS,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_IMAGE_RANGE,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_QUERY,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_READ_PERMISSION,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_WRITE_PERMISSION,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_PERMISSION_DRIFT,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_RANGE_CAPACITY,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_BYTE_CAPACITY,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_OVERFLOW,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_NO_PROGRESS,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_PLANNING_LIMIT,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_FOREGROUND,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_WITNESS,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_SAMPLER,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_SINK
} SudekiMpTalosStagingNativeCaptureFailure;

/* These facts are copied while the service is externally quiescent.  A zero
 * expected registry generation binds the first exact sole-observer witness
 * and rejects later generation drift.  Configure and Reset are deliberately
 * not observer-registration or quiescence primitives: the owner must disable
 * its ControlUpdateObserverGate, unregister, and drain before Reset. */
typedef struct SudekiMpTalosStagingNativeCaptureConfiguration {
    uint64_t process_token;
    uint64_t identity_salt;
    uint32_t loaded_image_base;
    uint32_t mapped_image_size;
    uint32_t expected_observer_registry_generation;
    uint32_t failed_retry_dispatches;
    uint8_t exact_executable_hash;
    uint8_t exact_sol_hash;
    uint8_t membership_abi_valid;
    uint8_t controller_abi_valid;
    uint8_t modal_active;
    uint8_t reload_required;
    uint8_t require_default_camera_name;
    uint8_t reserved[1];
} SudekiMpTalosStagingNativeCaptureConfiguration;

/* Pointer-free public state.  Capture never grants engine-call, actor-
 * lifetime, or mutation authority. */
typedef struct SudekiMpTalosStagingNativeCaptureStatus {
    uint64_t status_serial;
    uint64_t attempts;
    uint64_t failed_attempts;
    uint64_t throttled_callbacks;
    uint64_t sink_publications;
    uint64_t last_dispatch_serial;
    uint32_t failure;
    uint32_t sampler_failure;
    uint32_t failed_address;
    uint32_t failed_size;
    uint32_t planning_passes;
    uint32_t planning_retries;
    uint32_t query_calls;
    uint32_t preliminary_copy_calls;
    uint32_t final_copy_calls;
    uint32_t final_range_count;
    uint32_t final_capture_bytes;
    uint32_t bound_observer_registry_generation;
    uint32_t witness_native_thread_id;
    uint32_t witness_observer_registry_generation;
    uint32_t retry_callbacks_remaining;
    uint8_t configured;
    uint8_t active;
    uint8_t throttled;
    uint8_t final_boundary_entered;
    uint8_t no_post_boundary_query;
    uint8_t foreground_exact;
    uint8_t witness_revalidated_exact;
    uint8_t witness_source;
    uint8_t sink_published;
    uint8_t completed_valid;
    uint8_t inert_after_success;
    uint8_t observation_only;
    uint8_t native_engine_calls_permitted;
    uint8_t actor_lifetime_authority;
    uint8_t mutation_authority;
    uint8_t reserved[1];
} SudekiMpTalosStagingNativeCaptureStatus;

typedef BOOL (*SudekiMpTalosStagingNativeCaptureSink)(
    void *context,
    const SudekiMpTalosStagingNativeCaptureStatus *status,
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const SudekiMpControlUpdateDispatchWitness *witness
);
/* The sink is invoked synchronously only for a final, witness-revalidated,
 * valid sampler result.  TRUE means that exact result was consumed. */

BOOL SudekiMpTalosCompanionStagingNativeCaptureConfigure(
    const SudekiMpTalosStagingNativeCaptureConfiguration *configuration,
    SudekiMpTalosStagingNativeCaptureSink sink,
    void *sink_context
);

/* Exact ControlUpdate observer signature.  This function neither installs nor
 * registers any hook and is safe to place behind the caller-owned observer
 * entry gate.  It preserves the callback's incoming LastError value. */
void SudekiMpTalosCompanionStagingNativeCaptureService(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
);

BOOL SudekiMpTalosCompanionStagingNativeCaptureGetStatus(
    SudekiMpTalosStagingNativeCaptureStatus *status,
    SudekiMpTalosStagingNativeSamplerResult *last_result
);

/* Requires external observer disable/unregister/drain first. */
BOOL SudekiMpTalosCompanionStagingNativeCaptureReset(void);

typedef enum SudekiMpTalosStagingNativeCaptureCopyPhase {
    SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_PRELIMINARY_A = 1,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_PRELIMINARY_B,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_A,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_B
} SudekiMpTalosStagingNativeCaptureCopyPhase;

typedef enum SudekiMpTalosStagingNativeCaptureBarrierPhase {
    SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_PRELIMINARY_A = 1,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_PRELIMINARY_B,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_FINAL_A,
    SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_FINAL_B
} SudekiMpTalosStagingNativeCaptureBarrierPhase;

typedef struct SudekiMpTalosStagingNativeCaptureMemoryRegion {
    uint32_t address;
    uint32_t size;
    uint32_t protection;
    uint8_t committed;
    uint8_t readable;
    uint8_t writable;
    uint8_t reserved[1];
} SudekiMpTalosStagingNativeCaptureMemoryRegion;

#if defined(SUDEKIMP_TALOS_STAGING_NATIVE_CAPTURE_TESTING)
typedef struct SudekiMpTalosStagingNativeCaptureTestBackend {
    void *context;
    BOOL (*query)(
        void *context,
        uint32_t address,
        SudekiMpTalosStagingNativeCaptureMemoryRegion *region
    );
    BOOL (*copy)(
        void *context,
        uint8_t *destination,
        uint32_t source_address,
        uint32_t size,
        SudekiMpTalosStagingNativeCaptureCopyPhase phase
    );
    BOOL (*foreground)(void *context);
    BOOL (*witness_still_exact)(
        void *context,
        const SudekiMpControlUpdateDispatchWitness *witness
    );
    int (*sample)(
        void *context,
        const SudekiMpTalosStagingNativeSamplerInput *input,
        const void *callback_controller,
        const void *callback_update_data,
        const SudekiMpControlUpdateDispatchWitness *witness,
        const void *transaction_cookie,
        SudekiMpTalosStagingNativeSamplerResult *result
    );
    void (*barrier)(
        void *context,
        SudekiMpTalosStagingNativeCaptureBarrierPhase phase
    );
} SudekiMpTalosStagingNativeCaptureTestBackend;

BOOL SudekiMpTalosCompanionStagingNativeCaptureSetBackendForTesting(
    const SudekiMpTalosStagingNativeCaptureTestBackend *backend
);
const void *SudekiMpTalosCompanionStagingNativeCaptureAttemptCookieForTesting(
    void
);
#endif

#endif
