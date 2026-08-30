#ifndef SUDEKIMP_TALOS_COMPANION_STAGING_NATIVE_SAMPLER_H
#define SUDEKIMP_TALOS_COMPANION_STAGING_NATIVE_SAMPLER_H

#include "engine/talos_companion_staging_research.h"

#include <stddef.h>
#include <stdint.h>

/* The sampler parses two immutable captures of a 32-bit address space.  The
 * host byte pointers are borrowed only for the call; neither they nor any
 * decoded native address is retained in the result. */
typedef struct SudekiMpTalosStagingNativeReadableRange {
    uint32_t address;
    const uint8_t *bytes;
    uint32_t size;
    uint8_t native_readable;
    uint8_t native_writable;
    uint8_t reserved[2];
} SudekiMpTalosStagingNativeReadableRange;

typedef struct SudekiMpTalosStagingNativeView {
    const SudekiMpTalosStagingNativeReadableRange *ranges;
    size_t range_count;
} SudekiMpTalosStagingNativeView;

typedef enum SudekiMpTalosStagingNativeSamplerFailure {
    SUDEKIMP_TALOS_NATIVE_SAMPLE_OK = 0,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_ARGUMENT,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_FACT,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_MEMBERSHIP_ABI_FACT,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROLLER_ABI_FACT,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_DOUBLE_READ,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS_REVALIDATION,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_TRANSACTION_WINDOW,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_ALIAS,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_WORLD,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_HERO_IDENTITY,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROLLER,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_LISTENER,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_GIZMO,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_LABEL,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_CAMERA_SYNC,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_ARBITER,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_CAMERA,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_ORDINARY_DISCIPLINE,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_TOKEN_COLLISION
} SudekiMpTalosStagingNativeSamplerFailure;

enum {
    SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE = 0x0045f000u,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_RANGES = 128u,
    SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_CAPTURE_BYTES = 0x00500000u
};

/* All booleans are exact 0/1 facts.  exact_executable_hash and
 * exact_sol_hash are authenticated full-file facts supplied outside the
 * mapped-image parser.  membership_abi_valid and controller_abi_valid are
 * results of the already-completed pure/static gates, not assertions created
 * by this sampler.
 *
 * witness_still_exact_after_capture must be the result of calling
 * SudekiMpControlSeparationUpdateDispatchWitnessStillExact() after both
 * immutable views were captured and before invoking this parser.  Entry
 * exactness in the witness alone is intentionally insufficient.
 * transaction_lease_exclusive and capture_no_yield_exact are independent
 * bridge facts: a distinct cookie supplies continuity only and cannot prove
 * either fact by itself.
 *
 * modal_active and world+0x10 are diagnostics.  They are recorded but do not
 * grant or deny synchronous authority.  require_default_camera_name selects
 * the optional exact "default" camera-name profile. */
typedef struct SudekiMpTalosStagingNativeSamplerInput {
    SudekiMpTalosStagingNativeView first;
    SudekiMpTalosStagingNativeView second;
    uint64_t observation_serial;
    uint64_t process_token;
    uint64_t identity_salt;
    uint32_t loaded_image_base;
    uint32_t mapped_image_size;
    uint32_t expected_observer_registry_generation;
    uint8_t exact_executable_hash;
    uint8_t exact_sol_hash;
    uint8_t membership_abi_valid;
    uint8_t controller_abi_valid;
    uint8_t foreground;
    uint8_t witness_still_exact_after_capture;
    uint8_t transaction_lease_exclusive;
    uint8_t capture_no_yield_exact;
    uint8_t modal_active;
    uint8_t reload_required;
    uint8_t require_default_camera_name;
    uint8_t reserved[1];
} SudekiMpTalosStagingNativeSamplerInput;

/* Pointer-free, observation-only output.  A valid sample still grants no
 * production, carry, mutation, or actor-lifetime authority.  Elco's wrapper
 * token remains zero because this module has no GetPC path. */
typedef struct SudekiMpTalosStagingNativeSamplerResult {
    SudekiMpTalosStagingResearchSnapshot snapshot;
    uint32_t failure;
    uint32_t failed_address;
    uint32_t failed_size;
    uint32_t checks_completed;
    uint32_t first_range_count;
    uint32_t second_range_count;
    uint32_t first_capture_bytes;
    uint32_t second_capture_bytes;
    uint64_t witness_dispatch_serial;
    uint32_t witness_native_thread_id;
    uint32_t witness_observer_registry_generation;
    uint32_t witness_dispatch_overlap_generation;
    uint8_t valid;
    uint8_t observation_only;
    uint8_t native_engine_calls_permitted;
    uint8_t hooks_permitted;
    uint8_t actor_lifetime_authority;
    uint8_t mutation_authority;
    uint8_t external_sha256_required;
    uint8_t membership_abi_required;
    uint8_t failed_write_required;
    uint8_t witness_source;
    uint8_t witness_entry_exact;
    uint8_t witness_revalidated_exact;
    uint8_t transaction_lease_exact;
    uint8_t capture_no_yield_exact;
    uint8_t reserved[2];
} SudekiMpTalosStagingNativeSamplerResult;

struct SudekiMpControlUpdateDispatchWitness;

int SudekiMpTalosCompanionStagingNativeSample(
    const SudekiMpTalosStagingNativeSamplerInput *input,
    const void *callback_controller,
    const void *callback_update_data,
    const struct SudekiMpControlUpdateDispatchWitness *witness,
    const void *transaction_cookie,
    SudekiMpTalosStagingNativeSamplerResult *result
);

#endif
