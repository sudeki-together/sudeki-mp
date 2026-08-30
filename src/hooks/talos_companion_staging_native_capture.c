#include "hooks/talos_companion_staging_native_capture.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Talos companion staging native capture requires 32-bit Windows"
#endif

typedef struct CaptureBackend {
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
} CaptureBackend;

typedef struct CaptureState {
    LONG attempt_claimed;
    volatile LONG status_sequence;
    SudekiMpTalosStagingNativeCaptureConfiguration configuration;
    CaptureBackend backend;
    SudekiMpTalosStagingNativeCaptureSink sink;
    void *sink_context;
    SudekiMpTalosStagingNativeReadableRange first_ranges[
        SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES];
    SudekiMpTalosStagingNativeReadableRange second_ranges[
        SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES];
    uint32_t range_protection[
        SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES];
    uint8_t first_arena[SUDEKIMP_TALOS_NATIVE_CAPTURE_ARENA_BYTES];
    uint8_t second_arena[SUDEKIMP_TALOS_NATIVE_CAPTURE_ARENA_BYTES];
    size_t range_count;
    uint32_t capture_bytes;
    uint32_t bound_registry_generation;
    uint32_t retry_callbacks_remaining;
    uint8_t configured;
    uint8_t completed_valid;
    SudekiMpTalosStagingNativeCaptureStatus public_status;
    SudekiMpTalosStagingNativeSamplerResult public_result;
} CaptureState;

static CaptureState capture_state;

#if defined(SUDEKIMP_TALOS_STAGING_NATIVE_CAPTURE_TESTING)
static CaptureBackend injected_backend;
static BOOL injected_backend_set;
#endif

static int bool_exact(uint8_t value) {
    return value == 0u || value == 1u;
}

static uint64_t advance_nonzero(uint64_t value) {
    ++value;
    if (value == 0u) ++value;
    return value;
}

static int add_u32(uint32_t base, uint32_t size, uint32_t *end) {
    if (end == NULL || size == 0u || base > UINT32_MAX - size) return 0;
    *end = base + size;
    return 1;
}

static int backend_complete(const CaptureBackend *backend) {
    return backend != NULL && backend->query != NULL &&
        backend->copy != NULL && backend->foreground != NULL &&
        backend->witness_still_exact != NULL && backend->sample != NULL &&
        backend->barrier != NULL;
}

static int readable_protection(DWORD protection) {
    DWORD base = protection & 0xffu;

    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0u) return 0;
    return base == PAGE_READONLY || base == PAGE_READWRITE ||
        base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READ ||
        base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

static int writable_protection(DWORD protection) {
    DWORD base = protection & 0xffu;

    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0u) return 0;
    return base == PAGE_READWRITE || base == PAGE_WRITECOPY ||
        base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

static BOOL production_query(
    void *context,
    uint32_t address,
    SudekiMpTalosStagingNativeCaptureMemoryRegion *region
) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t base;
    uintptr_t end;

    (void)context;
    if (address == 0u || region == NULL ||
        VirtualQuery((const void *)(uintptr_t)address, &information,
            sizeof(information)) == 0u) return FALSE;
    base = (uintptr_t)information.BaseAddress;
    end = base + information.RegionSize;
    if (base > UINT32_MAX || end <= (uintptr_t)address ||
        (uintptr_t)address < base || end - base > UINT32_MAX) return FALSE;
    memset(region, 0, sizeof(*region));
    region->address = (uint32_t)base;
    region->size = (uint32_t)(end - base);
    region->protection = information.Protect;
    region->committed = information.State == MEM_COMMIT ? 1u : 0u;
    region->readable = readable_protection(information.Protect) ? 1u : 0u;
    region->writable = writable_protection(information.Protect) ? 1u : 0u;
    return TRUE;
}

static BOOL production_copy(
    void *context,
    uint8_t *destination,
    uint32_t source_address,
    uint32_t size,
    SudekiMpTalosStagingNativeCaptureCopyPhase phase
) {
    (void)context;
    (void)phase;
    if (destination == NULL || source_address == 0u || size == 0u) {
        return FALSE;
    }
    memcpy(destination, (const void *)(uintptr_t)source_address, size);
    return TRUE;
}

static BOOL production_foreground(void *context) {
    HWND foreground;
    DWORD process_id = 0u;

    (void)context;
    foreground = GetForegroundWindow();
    if (foreground == NULL) return FALSE;
    (void)GetWindowThreadProcessId(foreground, &process_id);
    return process_id != 0u && process_id == GetCurrentProcessId();
}

static BOOL production_witness_still_exact(
    void *context,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)context;
    return SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness);
}

static int production_sample(
    void *context,
    const SudekiMpTalosStagingNativeSamplerInput *input,
    const void *callback_controller,
    const void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const void *transaction_cookie,
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    (void)context;
    return SudekiMpTalosCompanionStagingNativeSample(input,
        callback_controller, callback_update_data, witness,
        transaction_cookie, result);
}

static void production_barrier(
    void *context,
    SudekiMpTalosStagingNativeCaptureBarrierPhase phase
) {
    (void)context;
    (void)phase;
    MemoryBarrier();
}

static CaptureBackend production_backend(void) {
    CaptureBackend backend;

    memset(&backend, 0, sizeof(backend));
    backend.query = production_query;
    backend.copy = production_copy;
    backend.foreground = production_foreground;
    backend.witness_still_exact = production_witness_still_exact;
    backend.sample = production_sample;
    backend.barrier = production_barrier;
    return backend;
}

static void initialize_status(
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    memset(status, 0, sizeof(*status));
    status->observation_only = 1u;
    status->native_engine_calls_permitted = 0u;
    status->actor_lifetime_authority = 0u;
    status->mutation_authority = 0u;
    status->no_post_boundary_query = 1u;
}

static void publish_status(
    SudekiMpTalosStagingNativeCaptureStatus *status,
    const SudekiMpTalosStagingNativeSamplerResult *result,
    int publish_result
) {
    status->status_serial = advance_nonzero(
        capture_state.public_status.status_serial);
    status->configured = capture_state.configured;
    status->completed_valid = capture_state.completed_valid;
    status->inert_after_success = capture_state.completed_valid;
    status->bound_observer_registry_generation =
        capture_state.bound_registry_generation;
    status->retry_callbacks_remaining =
        capture_state.retry_callbacks_remaining;
    (void)InterlockedIncrement(&capture_state.status_sequence);
    MemoryBarrier();
    capture_state.public_status = *status;
    if (publish_result && result != NULL) capture_state.public_result = *result;
    MemoryBarrier();
    (void)InterlockedIncrement(&capture_state.status_sequence);
}

static int configuration_valid(
    const SudekiMpTalosStagingNativeCaptureConfiguration *configuration
) {
    uint32_t image_end;

    return configuration != NULL && configuration->process_token != 0u &&
        configuration->identity_salt != 0u &&
        configuration->loaded_image_base != 0u &&
        configuration->mapped_image_size ==
            SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE &&
        add_u32(configuration->loaded_image_base,
            configuration->mapped_image_size, &image_end) &&
        bool_exact(configuration->exact_executable_hash) &&
        bool_exact(configuration->exact_sol_hash) &&
        bool_exact(configuration->membership_abi_valid) &&
        bool_exact(configuration->controller_abi_valid) &&
        bool_exact(configuration->modal_active) &&
        bool_exact(configuration->reload_required) &&
        bool_exact(configuration->require_default_camera_name) &&
        configuration->exact_executable_hash == 1u &&
        configuration->exact_sol_hash == 1u &&
        configuration->membership_abi_valid == 1u &&
        configuration->controller_abi_valid == 1u &&
        configuration->reserved[0] == 0u;
}

static int witness_entry_exact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    return witness != NULL && witness->dispatch_serial != 0u &&
        witness->native_thread_id != 0u &&
        witness->native_thread_id == GetCurrentThreadId() &&
        witness->outer_update_depth == 1u &&
        witness->active_dispatch_count == 1u &&
        witness->original_call_count == 1u &&
        witness->observer_snapshot_count == 1u &&
        witness->observer_registry_generation != 0u &&
        witness->hook_owned_exact == 1u && witness->slot_owned_exact == 1u &&
        witness->service_only == 1u && witness->post_original == 1u &&
        witness->source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL &&
        witness->source_exact == 1u &&
        witness->service_post_original_exact == 1u &&
        witness->sole_observer == 1u &&
        witness->registry_generation_stable == 1u &&
        witness->reserved[0] == 0u && witness->reserved[1] == 0u &&
        witness->reserved[2] == 0u;
}

static void clear_plan(void) {
    capture_state.range_count = 0u;
    capture_state.capture_bytes = 0u;
    memset(capture_state.first_ranges, 0,
        sizeof(capture_state.first_ranges));
    memset(capture_state.second_ranges, 0,
        sizeof(capture_state.second_ranges));
    memset(capture_state.range_protection, 0,
        sizeof(capture_state.range_protection));
}

static int recompute_capture_bytes(uint32_t *bytes) {
    size_t index;
    uint64_t total = 0u;

    if (bytes == NULL) return 0;
    for (index = 0u; index < capture_state.range_count; ++index) {
        total += capture_state.first_ranges[index].size;
        if (total > SUDEKIMP_TALOS_NATIVE_CAPTURE_ARENA_BYTES) return 0;
    }
    *bytes = (uint32_t)total;
    return 1;
}

static void mirror_plan(void) {
    size_t index;

    for (index = 0u; index < capture_state.range_count; ++index) {
        capture_state.second_ranges[index].address =
            capture_state.first_ranges[index].address;
        capture_state.second_ranges[index].size =
            capture_state.first_ranges[index].size;
        capture_state.second_ranges[index].native_readable = 1u;
        capture_state.second_ranges[index].native_writable =
            capture_state.first_ranges[index].native_writable;
        capture_state.second_ranges[index].reserved[0] = 0u;
        capture_state.second_ranges[index].reserved[1] = 0u;
    }
}

/* Insert one exact VirtualQuery-homogeneous span.  Native overlap with a
 * different protection is permission drift; adjacency is merged only when
 * the full protection token and writable fact match. */
static int add_plan_span(
    uint32_t address,
    uint32_t size,
    uint32_t protection,
    uint8_t writable,
    int *changed,
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    uint32_t end;
    uint32_t merged_start;
    uint32_t merged_end;
    size_t old_count;
    uint32_t old_bytes;
    size_t index;
    size_t insertion;

    if (changed == NULL || status == NULL || address == 0u ||
        !bool_exact(writable) || !add_u32(address, size, &end)) {
        status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_OVERFLOW;
        status->failed_address = address;
        status->failed_size = size;
        return 0;
    }
    old_count = capture_state.range_count;
    old_bytes = capture_state.capture_bytes;
    merged_start = address;
    merged_end = end;
    index = 0u;
    while (index < capture_state.range_count) {
        SudekiMpTalosStagingNativeReadableRange *range =
            &capture_state.first_ranges[index];
        uint32_t range_end;
        int overlaps;
        int adjacent;
        int same;
        size_t move_index;

        if (!add_u32(range->address, range->size, &range_end)) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_OVERFLOW;
            return 0;
        }
        overlaps = range->address < merged_end && merged_start < range_end;
        adjacent = range_end == merged_start || merged_end == range->address;
        same = capture_state.range_protection[index] == protection &&
            range->native_writable == writable;
        if (overlaps && !same) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_PERMISSION_DRIFT;
            status->failed_address = address;
            status->failed_size = size;
            return 0;
        }
        if (!overlaps && !(adjacent && same)) {
            ++index;
            continue;
        }
        if (range->address < merged_start) merged_start = range->address;
        if (range_end > merged_end) merged_end = range_end;
        for (move_index = index + 1u;
                move_index < capture_state.range_count; ++move_index) {
            capture_state.first_ranges[move_index - 1u] =
                capture_state.first_ranges[move_index];
            capture_state.range_protection[move_index - 1u] =
                capture_state.range_protection[move_index];
        }
        --capture_state.range_count;
    }
    if (capture_state.range_count >=
            SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES) {
        status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_RANGE_CAPACITY;
        status->failed_address = address;
        status->failed_size = size;
        return 0;
    }
    insertion = 0u;
    while (insertion < capture_state.range_count &&
        capture_state.first_ranges[insertion].address < merged_start) {
        ++insertion;
    }
    for (index = capture_state.range_count; index > insertion; --index) {
        capture_state.first_ranges[index] =
            capture_state.first_ranges[index - 1u];
        capture_state.range_protection[index] =
            capture_state.range_protection[index - 1u];
    }
    memset(&capture_state.first_ranges[insertion], 0,
        sizeof(capture_state.first_ranges[insertion]));
    capture_state.first_ranges[insertion].address = merged_start;
    capture_state.first_ranges[insertion].size = merged_end - merged_start;
    capture_state.first_ranges[insertion].native_readable = 1u;
    capture_state.first_ranges[insertion].native_writable = writable;
    capture_state.range_protection[insertion] = protection;
    ++capture_state.range_count;
    if (!recompute_capture_bytes(&capture_state.capture_bytes)) {
        status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_BYTE_CAPACITY;
        status->failed_address = address;
        status->failed_size = size;
        return 0;
    }
    mirror_plan();
    *changed = capture_state.range_count != old_count ||
        capture_state.capture_bytes != old_bytes;
    return 1;
}

static int query_region(
    uint32_t address,
    SudekiMpTalosStagingNativeCaptureMemoryRegion *region,
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    uint32_t region_end;

    if (status->final_boundary_entered != 0u) {
        status->no_post_boundary_query = 0u;
        status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_QUERY;
        status->failed_address = address;
        return 0;
    }
    ++status->query_calls;
    memset(region, 0, sizeof(*region));
    if (!capture_state.backend.query(capture_state.backend.context,
            address, region) || region->address == 0u ||
        region->size == 0u || !add_u32(region->address, region->size,
            &region_end) || address < region->address ||
        address >= region_end || !bool_exact(region->committed) ||
        !bool_exact(region->readable) || !bool_exact(region->writable) ||
        region->reserved[0] != 0u) {
        status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_QUERY;
        status->failed_address = address;
        return 0;
    }
    return 1;
}

static int add_queried_span(
    uint32_t address,
    uint32_t size,
    uint8_t write_required,
    int *made_progress,
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    uint32_t request_end;
    uint32_t cursor;
    int progress = 0;

    if (made_progress == NULL || !bool_exact(write_required) ||
        address == 0u || !add_u32(address, size, &request_end)) {
        status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_OVERFLOW;
        status->failed_address = address;
        status->failed_size = size;
        return 0;
    }
    cursor = address;
    while (cursor < request_end) {
        SudekiMpTalosStagingNativeCaptureMemoryRegion region;
        uint32_t region_end;
        uint32_t piece_end;
        int changed = 0;

        if (!query_region(cursor, &region, status) ||
            !add_u32(region.address, region.size, &region_end)) return 0;
        if (region.committed != 1u || region.readable != 1u) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_READ_PERMISSION;
            status->failed_address = cursor;
            status->failed_size = request_end - cursor;
            return 0;
        }
        if (write_required != 0u && region.writable != 1u) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_WRITE_PERMISSION;
            status->failed_address = cursor;
            status->failed_size = request_end - cursor;
            return 0;
        }
        piece_end = region_end < request_end ? region_end : request_end;
        if (piece_end <= cursor || !add_plan_span(cursor,
                piece_end - cursor, region.protection, region.writable,
                &changed, status)) return 0;
        if (changed) progress = 1;
        cursor = piece_end;
    }
    *made_progress = progress;
    return 1;
}

static int build_fixed_image_plan(
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    int progress;

    clear_plan();
    return add_queried_span(capture_state.configuration.loaded_image_base,
        capture_state.configuration.mapped_image_size, 0u, &progress,
        status) && progress;
}

static void assign_arena_bytes(void) {
    size_t index;
    uint32_t offset = 0u;

    for (index = 0u; index < capture_state.range_count; ++index) {
        capture_state.first_ranges[index].bytes =
            capture_state.first_arena + offset;
        capture_state.second_ranges[index].bytes =
            capture_state.second_arena + offset;
        offset += capture_state.first_ranges[index].size;
    }
}

static int copy_view(
    int first,
    SudekiMpTalosStagingNativeCaptureCopyPhase phase,
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    SudekiMpTalosStagingNativeReadableRange *ranges = first ?
        capture_state.first_ranges : capture_state.second_ranges;
    size_t index;

    for (index = 0u; index < capture_state.range_count; ++index) {
        if (!capture_state.backend.copy(capture_state.backend.context,
                (uint8_t *)ranges[index].bytes, ranges[index].address,
                ranges[index].size, phase)) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY;
            status->failed_address = ranges[index].address;
            status->failed_size = ranges[index].size;
            return 0;
        }
        if (phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_A ||
            phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_B) {
            ++status->final_copy_calls;
        } else {
            ++status->preliminary_copy_calls;
        }
    }
    return 1;
}

static int materialize_preliminary(
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    assign_arena_bytes();
    if (!copy_view(1, SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_PRELIMINARY_A,
            status)) return 0;
    capture_state.backend.barrier(capture_state.backend.context,
        SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_PRELIMINARY_A);
    if (!copy_view(0, SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_PRELIMINARY_B,
            status)) return 0;
    capture_state.backend.barrier(capture_state.backend.context,
        SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_PRELIMINARY_B);
    return 1;
}

static void fill_sampler_input(
    SudekiMpTalosStagingNativeSamplerInput *input,
    uint64_t observation_serial,
    uint8_t foreground,
    uint8_t witness_still_exact
) {
    memset(input, 0, sizeof(*input));
    input->first.ranges = capture_state.first_ranges;
    input->first.range_count = capture_state.range_count;
    input->second.ranges = capture_state.second_ranges;
    input->second.range_count = capture_state.range_count;
    input->observation_serial = observation_serial;
    input->process_token = capture_state.configuration.process_token;
    input->identity_salt = capture_state.configuration.identity_salt;
    input->loaded_image_base = capture_state.configuration.loaded_image_base;
    input->mapped_image_size = capture_state.configuration.mapped_image_size;
    input->expected_observer_registry_generation =
        capture_state.bound_registry_generation;
    input->exact_executable_hash =
        capture_state.configuration.exact_executable_hash;
    input->exact_sol_hash = capture_state.configuration.exact_sol_hash;
    input->membership_abi_valid =
        capture_state.configuration.membership_abi_valid;
    input->controller_abi_valid =
        capture_state.configuration.controller_abi_valid;
    input->foreground = foreground;
    input->witness_still_exact_after_capture = witness_still_exact;
    input->transaction_lease_exclusive = 1u;
    input->capture_no_yield_exact = 1u;
    input->modal_active = capture_state.configuration.modal_active;
    input->reload_required = capture_state.configuration.reload_required;
    input->require_default_camera_name =
        capture_state.configuration.require_default_camera_name;
}

static int discover_plan(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    uint64_t observation_serial,
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    unsigned int pass;

    if (!build_fixed_image_plan(status)) return 0;
    for (pass = 0u;
            pass <= SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_PLANNING_RETRIES;
            ++pass) {
        SudekiMpTalosStagingNativeSamplerInput hypothetical_input;
        SudekiMpTalosStagingNativeSamplerResult discarded_result;
        int sampled;
        int progress;

        ++status->planning_passes;
        if (!materialize_preliminary(status)) return 0;
        /* This local input is deliberately hypothetical.  Discovery uses the
         * pure sampler only as an exact missing-span oracle; neither this bit
         * nor a valid discarded result may reach the public sink. */
        fill_sampler_input(&hypothetical_input, observation_serial, 1u, 1u);
        memset(&discarded_result, 0, sizeof(discarded_result));
        sampled = capture_state.backend.sample(
            capture_state.backend.context, &hypothetical_input, controller,
            update_data, witness, &capture_state.attempt_claimed,
            &discarded_result);
        if (sampled && discarded_result.valid == 1u &&
            discarded_result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK) {
            return 1;
        }
        status->sampler_failure = discarded_result.failure;
        status->failed_address = discarded_result.failed_address;
        status->failed_size = discarded_result.failed_size;
        if (discarded_result.failure !=
                SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY ||
            discarded_result.failed_address == 0u ||
            discarded_result.failed_size == 0u ||
            !bool_exact(discarded_result.failed_write_required)) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_SAMPLER;
            return 0;
        }
        if (pass == SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_PLANNING_RETRIES) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_PLANNING_LIMIT;
            return 0;
        }
        progress = 0;
        if (!add_queried_span(discarded_result.failed_address,
                discarded_result.failed_size,
                discarded_result.failed_write_required, &progress,
                status)) return 0;
        ++status->planning_retries;
        if (!progress) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_NO_PROGRESS;
            return 0;
        }
    }
    status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_PLANNING_LIMIT;
    return 0;
}

static int revalidate_plan(
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    size_t index;

    for (index = 0u; index < capture_state.range_count; ++index) {
        const SudekiMpTalosStagingNativeReadableRange *range =
            &capture_state.first_ranges[index];
        uint32_t end;
        uint32_t cursor;

        if (!add_u32(range->address, range->size, &end)) {
            status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_OVERFLOW;
            return 0;
        }
        cursor = range->address;
        while (cursor < end) {
            SudekiMpTalosStagingNativeCaptureMemoryRegion region;
            uint32_t region_end;

            if (!query_region(cursor, &region, status) ||
                !add_u32(region.address, region.size, &region_end)) return 0;
            if (region.committed != 1u || region.readable != 1u ||
                region.writable != range->native_writable ||
                region.protection != capture_state.range_protection[index]) {
                status->failure =
                    SUDEKIMP_TALOS_NATIVE_CAPTURE_PERMISSION_DRIFT;
                status->failed_address = cursor;
                status->failed_size = end - cursor;
                return 0;
            }
            if (region_end <= cursor) {
                status->failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_NO_PROGRESS;
                status->failed_address = cursor;
                return 0;
            }
            cursor = region_end < end ? region_end : end;
        }
    }
    return 1;
}

static int materialize_final(
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    assign_arena_bytes();
    status->final_boundary_entered = 1u;
    if (!copy_view(1, SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_A,
            status)) return 0;
    capture_state.backend.barrier(capture_state.backend.context,
        SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_FINAL_A);
    if (!copy_view(0, SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_B,
            status)) return 0;
    capture_state.backend.barrier(capture_state.backend.context,
        SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_FINAL_B);
    return 1;
}

static uint32_t retry_interval(void) {
    uint32_t configured =
        capture_state.configuration.failed_retry_dispatches;

    return configured == 0u ?
        SUDEKIMP_TALOS_NATIVE_CAPTURE_DEFAULT_RETRY_DISPATCHES : configured;
}

static void finish_failed_attempt(
    SudekiMpTalosStagingNativeCaptureStatus *status
) {
    ++status->failed_attempts;
    status->active = 0u;
    capture_state.retry_callbacks_remaining = retry_interval();
    status->retry_callbacks_remaining =
        capture_state.retry_callbacks_remaining;
    publish_status(status, NULL, 0);
}

BOOL SudekiMpTalosCompanionStagingNativeCaptureConfigure(
    const SudekiMpTalosStagingNativeCaptureConfiguration *configuration,
    SudekiMpTalosStagingNativeCaptureSink sink,
    void *sink_context
) {
    CaptureBackend backend;
    SudekiMpTalosStagingNativeCaptureStatus status;
    SudekiMpTalosStagingNativeSamplerResult empty_result;

    if (!configuration_valid(configuration) || sink == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (InterlockedCompareExchange(&capture_state.attempt_claimed, 1, 0) !=
            0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
#if defined(SUDEKIMP_TALOS_STAGING_NATIVE_CAPTURE_TESTING)
    backend = injected_backend_set ? injected_backend : production_backend();
#else
    backend = production_backend();
#endif
    if (!backend_complete(&backend)) {
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (capture_state.configured != 0u) {
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    capture_state.configuration = *configuration;
    capture_state.backend = backend;
    capture_state.sink = sink;
    capture_state.sink_context = sink_context;
    capture_state.bound_registry_generation =
        configuration->expected_observer_registry_generation;
    capture_state.retry_callbacks_remaining = 0u;
    capture_state.configured = 1u;
    capture_state.completed_valid = 0u;
    clear_plan();
    memset(&empty_result, 0, sizeof(empty_result));
    initialize_status(&status);
    status.configured = 1u;
    publish_status(&status, &empty_result, 1);
    (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
    return TRUE;
}

void SudekiMpTalosCompanionStagingNativeCaptureService(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD entry_error = GetLastError();
    SudekiMpTalosStagingNativeCaptureStatus status;
    SudekiMpTalosStagingNativeSamplerInput final_input;
    SudekiMpTalosStagingNativeSamplerResult final_result;
    BOOL foreground;
    BOOL witness_exact;
    BOOL sink_published;
    int sampled;

    if (InterlockedCompareExchange(&capture_state.attempt_claimed, 1, 0) !=
            0) {
        SetLastError(entry_error);
        return;
    }
    status = capture_state.public_status;
    status.configured = capture_state.configured;
    status.active = 0u;
    status.throttled = 0u;
    status.final_boundary_entered = 0u;
    status.no_post_boundary_query = 1u;
    status.foreground_exact = 0u;
    status.witness_revalidated_exact = 0u;
    status.sink_published = 0u;
    status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_OK;
    status.sampler_failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_OK;
    status.failed_address = 0u;
    status.failed_size = 0u;
    status.planning_passes = 0u;
    status.planning_retries = 0u;
    status.query_calls = 0u;
    status.preliminary_copy_calls = 0u;
    status.final_copy_calls = 0u;
    status.final_range_count = 0u;
    status.final_capture_bytes = 0u;
    status.witness_native_thread_id = 0u;
    status.witness_observer_registry_generation = 0u;
    status.witness_source = 0u;
    if (capture_state.configured == 0u) {
        status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_NOT_CONFIGURED;
        publish_status(&status, NULL, 0);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    if (capture_state.completed_valid != 0u) {
        status.completed_valid = 1u;
        status.inert_after_success = 1u;
        publish_status(&status, NULL, 0);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    if (!witness_entry_exact(witness) ||
        (capture_state.configuration.expected_observer_registry_generation !=
            0u && witness != NULL &&
            witness->observer_registry_generation !=
                capture_state.configuration.
                    expected_observer_registry_generation)) {
        status.failure = witness == NULL ?
            SUDEKIMP_TALOS_NATIVE_CAPTURE_ARGUMENT :
            SUDEKIMP_TALOS_NATIVE_CAPTURE_WITNESS;
        finish_failed_attempt(&status);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    status.last_dispatch_serial = witness->dispatch_serial;
    status.witness_native_thread_id = witness->native_thread_id;
    status.witness_observer_registry_generation =
        witness->observer_registry_generation;
    status.witness_source = witness->source;
    if (capture_state.retry_callbacks_remaining != 0u) {
        --capture_state.retry_callbacks_remaining;
        ++status.throttled_callbacks;
        status.throttled = 1u;
        status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_THROTTLED;
        status.retry_callbacks_remaining =
            capture_state.retry_callbacks_remaining;
        publish_status(&status, NULL, 0);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    ++status.attempts;
    status.active = 1u;
    publish_status(&status, NULL, 0);
    if (controller == NULL || update_data == NULL) {
        status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_ARGUMENT;
        finish_failed_attempt(&status);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    if (controller == update_data ||
        controller == (const void *)&capture_state.attempt_claimed ||
        update_data == (const void *)&capture_state.attempt_claimed ||
        witness == (const void *)&capture_state.attempt_claimed) {
        status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_ALIAS;
        finish_failed_attempt(&status);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    if (capture_state.bound_registry_generation == 0u) {
        capture_state.bound_registry_generation =
            witness->observer_registry_generation;
    }
    status.bound_observer_registry_generation =
        capture_state.bound_registry_generation;
    if (capture_state.bound_registry_generation == 0u ||
        !discover_plan(controller, update_data, witness,
            witness->dispatch_serial, &status) ||
        !revalidate_plan(&status)) {
        finish_failed_attempt(&status);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    foreground = capture_state.backend.foreground(
        capture_state.backend.context);
    if (foreground != TRUE) {
        status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_FOREGROUND;
        finish_failed_attempt(&status);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    status.foreground_exact = 1u;
    status.final_range_count = (uint32_t)capture_state.range_count;
    status.final_capture_bytes = capture_state.capture_bytes;
    if (!materialize_final(&status)) {
        finish_failed_attempt(&status);
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(entry_error);
        return;
    }
    witness_exact = capture_state.backend.witness_still_exact(
        capture_state.backend.context, witness);
    status.witness_revalidated_exact = witness_exact == TRUE ? 1u : 0u;
    fill_sampler_input(&final_input, witness->dispatch_serial, 1u,
        status.witness_revalidated_exact);
    memset(&final_result, 0, sizeof(final_result));
    sampled = capture_state.backend.sample(capture_state.backend.context,
        &final_input, controller, update_data, witness,
        &capture_state.attempt_claimed, &final_result);
    status.sampler_failure = final_result.failure;
    status.failed_address = final_result.failed_address;
    status.failed_size = final_result.failed_size;
    if (!sampled || final_result.valid != 1u ||
        final_result.failure != SUDEKIMP_TALOS_NATIVE_SAMPLE_OK) {
        status.failure = witness_exact == TRUE ?
            SUDEKIMP_TALOS_NATIVE_CAPTURE_SAMPLER :
            SUDEKIMP_TALOS_NATIVE_CAPTURE_WITNESS;
    }
    sink_published = FALSE;
    if (sampled && final_result.valid == 1u &&
        final_result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK &&
        witness_exact == TRUE) {
        /* The immutable final window ends with the sampler return.
         * Publication is synchronous and receives the same borrowed witness,
         * but it cannot feed a planning or invalid final result to the
         * adapter. */
        sink_published = capture_state.sink(capture_state.sink_context,
            &status, &final_result, witness);
        if (sink_published == TRUE) {
            ++status.sink_publications;
            status.sink_published = 1u;
        } else {
            status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_SINK;
        }
    }
    if (sampled && final_result.valid == 1u &&
        final_result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK &&
        witness_exact == TRUE && sink_published == TRUE) {
        capture_state.completed_valid = 1u;
        capture_state.retry_callbacks_remaining = 0u;
        status.failure = SUDEKIMP_TALOS_NATIVE_CAPTURE_OK;
        status.completed_valid = 1u;
        status.inert_after_success = 1u;
    } else {
        ++status.failed_attempts;
        capture_state.retry_callbacks_remaining = retry_interval();
    }
    status.active = 0u;
    status.retry_callbacks_remaining =
        capture_state.retry_callbacks_remaining;
    publish_status(&status, &final_result, 1);
    (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
    SetLastError(entry_error);
}

BOOL SudekiMpTalosCompanionStagingNativeCaptureGetStatus(
    SudekiMpTalosStagingNativeCaptureStatus *status,
    SudekiMpTalosStagingNativeSamplerResult *last_result
) {
    unsigned int attempt;

    if (status == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (attempt = 0u; attempt < 32u; ++attempt) {
        LONG before = InterlockedCompareExchange(
            &capture_state.status_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) continue;
        MemoryBarrier();
        *status = capture_state.public_status;
        if (last_result != NULL) *last_result = capture_state.public_result;
        MemoryBarrier();
        after = InterlockedCompareExchange(
            &capture_state.status_sequence, 0, 0);
        if (before == after && (after & 1) == 0) return TRUE;
    }
    memset(status, 0, sizeof(*status));
    if (last_result != NULL) memset(last_result, 0, sizeof(*last_result));
    SetLastError(ERROR_BUSY);
    return FALSE;
}

BOOL SudekiMpTalosCompanionStagingNativeCaptureReset(void) {
    SudekiMpTalosStagingNativeCaptureStatus status;
    SudekiMpTalosStagingNativeSamplerResult empty_result;

    if (InterlockedCompareExchange(&capture_state.attempt_claimed, 1, 0) !=
            0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    memset(&capture_state.configuration, 0,
        sizeof(capture_state.configuration));
    memset(&capture_state.backend, 0, sizeof(capture_state.backend));
    capture_state.sink = NULL;
    capture_state.sink_context = NULL;
    capture_state.bound_registry_generation = 0u;
    capture_state.retry_callbacks_remaining = 0u;
    capture_state.configured = 0u;
    capture_state.completed_valid = 0u;
    clear_plan();
    memset(&empty_result, 0, sizeof(empty_result));
    initialize_status(&status);
    publish_status(&status, &empty_result, 1);
    (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
    return TRUE;
}

#if defined(SUDEKIMP_TALOS_STAGING_NATIVE_CAPTURE_TESTING)
BOOL SudekiMpTalosCompanionStagingNativeCaptureSetBackendForTesting(
    const SudekiMpTalosStagingNativeCaptureTestBackend *backend
) {
    CaptureBackend converted;

    if (InterlockedCompareExchange(&capture_state.attempt_claimed, 1, 0) !=
            0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (capture_state.configured != 0u) {
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (backend == NULL) {
        memset(&injected_backend, 0, sizeof(injected_backend));
        injected_backend_set = FALSE;
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        return TRUE;
    }
    memset(&converted, 0, sizeof(converted));
    converted.context = backend->context;
    converted.query = backend->query;
    converted.copy = backend->copy;
    converted.foreground = backend->foreground;
    converted.witness_still_exact = backend->witness_still_exact;
    converted.sample = backend->sample;
    converted.barrier = backend->barrier;
    if (!backend_complete(&converted)) {
        (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    injected_backend = converted;
    injected_backend_set = TRUE;
    (void)InterlockedExchange(&capture_state.attempt_claimed, 0);
    return TRUE;
}

const void *SudekiMpTalosCompanionStagingNativeCaptureAttemptCookieForTesting(
    void
) {
    return (const void *)&capture_state.attempt_claimed;
}
#endif
