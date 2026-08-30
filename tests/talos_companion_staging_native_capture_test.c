#include "hooks/talos_companion_staging_native_capture.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

enum {
    IMAGE_BASE = 0x00400000u,
    IMAGE_SPLIT = 0x00600000u,
    DYNAMIC_BASE = 0x10000000u,
    MAX_MISSING = 140u,
    MAX_EVENTS = 32768u
};

typedef struct MissingSpan {
    uint32_t address;
    uint32_t size;
    uint8_t write_required;
} MissingSpan;

typedef struct TestContext {
    MissingSpan missing[MAX_MISSING];
    unsigned int missing_count;
    unsigned int preliminary_samples;
    unsigned int final_samples;
    unsigned int query_calls;
    unsigned int copy_calls;
    unsigned int foreground_calls;
    unsigned int witness_calls;
    unsigned int barrier_calls;
    unsigned int sink_calls;
    unsigned int reset_busy_seen;
    unsigned int planning_valid_seen;
    unsigned int planning_result_leaked;
    unsigned int query_after_final;
    unsigned int order_violation;
    unsigned int final_a_calls;
    unsigned int final_b_calls;
    unsigned int final_a_barrier;
    unsigned int final_b_barrier;
    unsigned int planned_range_count;
    unsigned int final_range_count;
    unsigned int sink_inside_service;
    uint32_t final_capture_bytes;
    uint32_t final_addresses[SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES];
    uint32_t final_sizes[SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES];
    uint8_t final_writable[SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES];
    char events[MAX_EVENTS];
    unsigned int event_count;
    int inside_service;
    int split_image;
    int range_cap_image;
    int dynamic_read_only;
    int permission_drift_after_plan;
    int foreground_exact;
    int witness_exact;
    int final_double_read;
    int fail_final_copy;
    int sink_consumes;
    const SudekiMpControlUpdateDispatchWitness *expected_witness;
    const void *expected_controller;
    const void *expected_update_data;
    const void *expected_cookie;
    SudekiMpTalosStagingNativeSamplerResult sink_result;
    SudekiMpTalosStagingNativeCaptureStatus sink_status;
} TestContext;

static int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

static void event(TestContext *context, char value) {
    if (context->event_count < MAX_EVENTS) {
        context->events[context->event_count++] = value;
    } else {
        context->order_violation = 1u;
    }
}

static uint32_t view_bytes(const SudekiMpTalosStagingNativeView *view) {
    uint64_t total = 0u;
    size_t index;

    for (index = 0u; index < view->range_count; ++index) {
        total += view->ranges[index].size;
    }
    return total > UINT32_MAX ? UINT32_MAX : (uint32_t)total;
}

static BOOL test_query(
    void *opaque,
    uint32_t address,
    SudekiMpTalosStagingNativeCaptureMemoryRegion *region
) {
    TestContext *context = (TestContext *)opaque;
    uint32_t image_end = IMAGE_BASE +
        SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE;

    ++context->query_calls;
    event(context, 'Q');
    if (context->final_a_calls != 0u || context->final_b_calls != 0u) {
        ++context->query_after_final;
    }
    memset(region, 0, sizeof(*region));
    if (address >= IMAGE_BASE && address < image_end) {
        if (context->range_cap_image) {
            uint32_t page = (address - IMAGE_BASE) & ~UINT32_C(0xfff);

            region->address = IMAGE_BASE + page;
            region->size = 0x1000u;
            region->protection = 0x1000u + page / 0x1000u;
            region->committed = 1u;
            region->readable = 1u;
            region->writable = 0u;
            return TRUE;
        }
        if (context->split_image && address < IMAGE_SPLIT) {
            region->address = IMAGE_BASE;
            region->size = IMAGE_SPLIT - IMAGE_BASE;
            region->protection = PAGE_EXECUTE_READ;
            region->writable = 0u;
        } else if (context->split_image) {
            region->address = IMAGE_SPLIT;
            region->size = image_end - IMAGE_SPLIT;
            region->protection = PAGE_READWRITE;
            region->writable = 1u;
        } else {
            region->address = IMAGE_BASE;
            region->size = image_end - IMAGE_BASE;
            region->protection = PAGE_EXECUTE_READ;
            region->writable = 0u;
        }
        if (context->permission_drift_after_plan &&
            context->planning_valid_seen) {
            ++region->protection;
        }
        region->committed = 1u;
        region->readable = 1u;
        return TRUE;
    }
    if (address >= DYNAMIC_BASE && address < DYNAMIC_BASE + 0x00100000u) {
        region->address = DYNAMIC_BASE;
        region->size = 0x00100000u;
        region->protection = context->dynamic_read_only ?
            PAGE_READONLY : PAGE_READWRITE;
        region->committed = 1u;
        region->readable = 1u;
        region->writable = context->dynamic_read_only ? 0u : 1u;
        return TRUE;
    }
    return FALSE;
}

static BOOL test_copy(
    void *opaque,
    uint8_t *destination,
    uint32_t source_address,
    uint32_t size,
    SudekiMpTalosStagingNativeCaptureCopyPhase phase
) {
    TestContext *context = (TestContext *)opaque;
    uint8_t fill = (uint8_t)(source_address ^ (source_address >> 8u));

    ++context->copy_calls;
    if (phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_PRELIMINARY_A) {
        event(context, 'a');
    } else if (phase ==
            SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_PRELIMINARY_B) {
        event(context, 'b');
    } else if (phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_A) {
        event(context, 'A');
        if (context->final_b_calls != 0u) context->order_violation = 1u;
        ++context->final_a_calls;
        if (context->fail_final_copy) return FALSE;
    } else if (phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY_FINAL_B) {
        event(context, 'B');
        if (context->final_a_barrier != 1u) context->order_violation = 1u;
        ++context->final_b_calls;
    } else {
        context->order_violation = 1u;
        return FALSE;
    }
    memset(destination, fill, size);
    return TRUE;
}

static BOOL test_foreground(void *opaque) {
    TestContext *context = (TestContext *)opaque;

    ++context->foreground_calls;
    event(context, 'F');
    return context->foreground_exact ? TRUE : FALSE;
}

static BOOL test_witness(
    void *opaque,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    TestContext *context = (TestContext *)opaque;

    ++context->witness_calls;
    event(context, 'W');
    if (witness != context->expected_witness ||
        context->final_a_barrier != 1u || context->final_b_barrier != 1u ||
        context->query_after_final != 0u) {
        context->order_violation = 1u;
    }
    return context->witness_exact ? TRUE : FALSE;
}

static void fill_result_metadata(
    const SudekiMpTalosStagingNativeSamplerInput *input,
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    result->observation_only = 1u;
    result->external_sha256_required = 1u;
    result->membership_abi_required = 1u;
    result->first_range_count = (uint32_t)input->first.range_count;
    result->second_range_count = (uint32_t)input->second.range_count;
    result->first_capture_bytes = view_bytes(&input->first);
    result->second_capture_bytes = view_bytes(&input->second);
    result->witness_dispatch_serial = witness->dispatch_serial;
    result->witness_native_thread_id = witness->native_thread_id;
    result->witness_observer_registry_generation =
        witness->observer_registry_generation;
    result->witness_dispatch_overlap_generation =
        witness->dispatch_overlap_generation;
    result->witness_source = witness->source;
    result->witness_entry_exact = 1u;
    result->witness_revalidated_exact =
        input->witness_still_exact_after_capture;
    result->transaction_lease_exact = input->transaction_lease_exclusive;
    result->capture_no_yield_exact = input->capture_no_yield_exact;
}

static int test_sample(
    void *opaque,
    const SudekiMpTalosStagingNativeSamplerInput *input,
    const void *controller,
    const void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const void *transaction_cookie,
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    TestContext *context = (TestContext *)opaque;
    size_t index;

    memset(result, 0, sizeof(*result));
    fill_result_metadata(input, witness, result);
    if (context->final_a_calls == 0u) {
        unsigned int sample_index = context->preliminary_samples++;

        event(context, 'P');
        if (sample_index < context->missing_count) {
            result->failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY;
            result->failed_address = context->missing[sample_index].address;
            result->failed_size = context->missing[sample_index].size;
            result->failed_write_required =
                context->missing[sample_index].write_required;
            return 0;
        }
        context->planning_valid_seen = 1u;
        context->planned_range_count = (unsigned int)input->first.range_count;
        if (context->sink_calls != 0u) context->planning_result_leaked = 1u;
        result->snapshot.observation_serial = UINT64_C(0x11111111);
        result->valid = 1u;
        result->failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_OK;
        return 1;
    }
    ++context->final_samples;
    event(context, 'S');
    if (context->witness_calls != 1u ||
        controller != context->expected_controller ||
        update_data != context->expected_update_data ||
        witness != context->expected_witness ||
        transaction_cookie != context->expected_cookie ||
        transaction_cookie == controller || transaction_cookie == update_data ||
        input->first.ranges == input->second.ranges ||
        input->first.range_count != input->second.range_count ||
        input->observation_serial != witness->dispatch_serial ||
        input->expected_observer_registry_generation !=
            witness->observer_registry_generation ||
        input->foreground != 1u ||
        input->transaction_lease_exclusive != 1u ||
        input->capture_no_yield_exact != 1u) {
        context->order_violation = 1u;
    }
    context->final_range_count = (unsigned int)input->first.range_count;
    context->final_capture_bytes = view_bytes(&input->first);
    for (index = 0u; index < input->first.range_count &&
            index < SUDEKIMP_TALOS_NATIVE_CAPTURE_MAX_RANGES; ++index) {
        const SudekiMpTalosStagingNativeReadableRange *first =
            &input->first.ranges[index];
        const SudekiMpTalosStagingNativeReadableRange *second =
            &input->second.ranges[index];
        uintptr_t first_start = (uintptr_t)first->bytes;
        uintptr_t first_end = first_start + first->size;
        uintptr_t second_start = (uintptr_t)second->bytes;
        uintptr_t second_end = second_start + second->size;

        context->final_addresses[index] = first->address;
        context->final_sizes[index] = first->size;
        context->final_writable[index] = first->native_writable;
        if (first->address != second->address || first->size != second->size ||
            first->native_readable != 1u || second->native_readable != 1u ||
            first->native_writable != second->native_writable ||
            (first_start < second_end && second_start < first_end)) {
            context->order_violation = 1u;
        }
    }
    result->snapshot.observation_serial = UINT64_C(0x22222222);
    if (input->witness_still_exact_after_capture != 1u) {
        result->failure =
            SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS_REVALIDATION;
        return 0;
    }
    if (context->final_double_read) {
        result->failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_DOUBLE_READ;
        result->failed_address = DYNAMIC_BASE;
        return 0;
    }
    result->valid = 1u;
    result->failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_OK;
    return 1;
}

static void test_barrier(
    void *opaque,
    SudekiMpTalosStagingNativeCaptureBarrierPhase phase
) {
    TestContext *context = (TestContext *)opaque;

    ++context->barrier_calls;
    if (phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_PRELIMINARY_A) {
        event(context, 'c');
    } else if (phase ==
            SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_PRELIMINARY_B) {
        event(context, 'd');
    } else if (phase == SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_FINAL_A) {
        event(context, 'X');
        if (context->final_a_calls != context->planned_range_count ||
            context->final_b_calls != 0u) {
            context->order_violation = 1u;
        }
        ++context->final_a_barrier;
    } else if (phase ==
            SUDEKIMP_TALOS_NATIVE_CAPTURE_BARRIER_FINAL_B) {
        event(context, 'Y');
        if (context->final_a_barrier != 1u ||
            context->final_b_calls != context->planned_range_count) {
            context->order_violation = 1u;
        }
        ++context->final_b_barrier;
    } else {
        context->order_violation = 1u;
    }
}

static BOOL test_sink(
    void *opaque,
    const SudekiMpTalosStagingNativeCaptureStatus *status,
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    TestContext *context = (TestContext *)opaque;

    ++context->sink_calls;
    event(context, 'K');
    context->sink_status = *status;
    context->sink_result = *result;
    if (context->inside_service) ++context->sink_inside_service;
    if (!context->planning_valid_seen ||
        result->snapshot.observation_serial != UINT64_C(0x22222222) ||
        witness != context->expected_witness || context->final_samples != 1u) {
        context->planning_result_leaked = 1u;
    }
    SetLastError(0xdeadu);
    if (!SudekiMpTalosCompanionStagingNativeCaptureReset() &&
        GetLastError() == ERROR_BUSY) {
        ++context->reset_busy_seen;
    }
    return context->sink_consumes ? TRUE : FALSE;
}

/* Production-only references remain linkable even though every test installs
 * the complete injected backend before Configure. */
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    return witness != NULL ? TRUE : FALSE;
}

int SudekiMpTalosCompanionStagingNativeSample(
    const SudekiMpTalosStagingNativeSamplerInput *input,
    const void *callback_controller,
    const void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const void *transaction_cookie,
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    (void)input;
    (void)callback_controller;
    (void)callback_update_data;
    (void)witness;
    (void)transaction_cookie;
    (void)result;
    return 0;
}

static SudekiMpControlUpdateDispatchWitness exact_witness(
    uint64_t serial
) {
    SudekiMpControlUpdateDispatchWitness witness;

    memset(&witness, 0, sizeof(witness));
    witness.dispatch_serial = serial;
    witness.native_thread_id = GetCurrentThreadId();
    witness.outer_update_depth = 1u;
    witness.active_dispatch_count = 1u;
    witness.original_call_count = 1u;
    witness.observer_snapshot_count = 1u;
    witness.observer_registry_generation = 9u;
    witness.dispatch_overlap_generation = 3u;
    witness.hook_owned_exact = 1u;
    witness.slot_owned_exact = 1u;
    witness.service_only = 1u;
    witness.post_original = 1u;
    witness.source =
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL;
    witness.source_exact = 1u;
    witness.service_post_original_exact = 1u;
    witness.sole_observer = 1u;
    witness.registry_generation_stable = 1u;
    return witness;
}

static SudekiMpTalosStagingNativeCaptureConfiguration configuration(void) {
    SudekiMpTalosStagingNativeCaptureConfiguration value;

    memset(&value, 0, sizeof(value));
    value.process_token = UINT64_C(0x12345678);
    value.identity_salt = UINT64_C(0xabcdef01);
    value.loaded_image_base = IMAGE_BASE;
    value.mapped_image_size = SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE;
    value.failed_retry_dispatches = 1u;
    value.exact_executable_hash = 1u;
    value.exact_sol_hash = 1u;
    value.membership_abi_valid = 1u;
    value.controller_abi_valid = 1u;
    value.reload_required = 1u;
    return value;
}

static SudekiMpTalosStagingNativeCaptureTestBackend backend(
    TestContext *context
) {
    SudekiMpTalosStagingNativeCaptureTestBackend value;

    memset(&value, 0, sizeof(value));
    value.context = context;
    value.query = test_query;
    value.copy = test_copy;
    value.foreground = test_foreground;
    value.witness_still_exact = test_witness;
    value.sample = test_sample;
    value.barrier = test_barrier;
    return value;
}

static void initialize_context(TestContext *context) {
    memset(context, 0, sizeof(*context));
    context->split_image = 1;
    context->foreground_exact = 1;
    context->witness_exact = 1;
    context->sink_consumes = 1;
}

static void install_context(
    TestContext *context,
    const SudekiMpTalosStagingNativeCaptureConfiguration *config
) {
    SudekiMpTalosStagingNativeCaptureTestBackend selected = backend(context);

    CHECK(SudekiMpTalosCompanionStagingNativeCaptureReset());
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureSetBackendForTesting(
        &selected));
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureConfigure(config,
        test_sink, context));
}

static void run_service(
    TestContext *context,
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    context->expected_controller = controller;
    context->expected_update_data = update_data;
    context->expected_witness = witness;
    context->expected_cookie =
        SudekiMpTalosCompanionStagingNativeCaptureAttemptCookieForTesting();
    context->inside_service = 1;
    SudekiMpTalosCompanionStagingNativeCaptureService(controller,
        update_data, witness);
    context->inside_service = 0;
}

static int has_final_dynamic_range(
    const TestContext *context,
    uint32_t address,
    uint32_t size,
    uint8_t writable
) {
    unsigned int index;

    for (index = 0u; index < context->final_range_count; ++index) {
        if (context->final_addresses[index] == address &&
            context->final_sizes[index] == size &&
            context->final_writable[index] == writable) return 1;
    }
    return 0;
}

static void test_success_order_exact_spans_and_inert(void) {
    TestContext context;
    SudekiMpTalosStagingNativeCaptureConfiguration config = configuration();
    SudekiMpControlUpdateDispatchWitness witness = exact_witness(41u);
    SudekiMpTalosStagingNativeCaptureStatus status;
    SudekiMpTalosStagingNativeSamplerResult result;
    unsigned int calls_before;

    initialize_context(&context);
    context.missing[0].address = DYNAMIC_BASE + 0x1000u;
    context.missing[0].size = 4u;
    context.missing[0].write_required = 1u;
    context.missing[1].address = DYNAMIC_BASE + 0x1004u;
    context.missing[1].size = 4u;
    context.missing[1].write_required = 1u;
    context.missing_count = 2u;
    install_context(&context, &config);
    SetLastError(0xbeefu);
    run_service(&context, (void *)(uintptr_t)0x20000000u,
        (void *)(uintptr_t)0x20001000u, &witness);
    CHECK(GetLastError() == 0xbeefu);
    CHECK(context.preliminary_samples == 3u);
    CHECK(context.final_samples == 1u);
    CHECK(context.sink_calls == 1u);
    CHECK(context.sink_inside_service == 1u);
    CHECK(context.reset_busy_seen == 1u);
    CHECK(context.planning_valid_seen == 1u);
    CHECK(context.planning_result_leaked == 0u);
    CHECK(context.query_after_final == 0u);
    CHECK(context.order_violation == 0u);
    CHECK(context.final_range_count == 3u);
    CHECK(context.final_capture_bytes ==
        SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE + 8u);
    CHECK(has_final_dynamic_range(&context, DYNAMIC_BASE + 0x1000u,
        8u, 1u));
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status,
        &result));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_OK);
    CHECK(status.completed_valid == 1u && status.inert_after_success == 1u);
    CHECK(status.final_range_count == 3u);
    CHECK(status.final_capture_bytes ==
        SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE + 8u);
    CHECK(status.no_post_boundary_query == 1u);
    CHECK(status.witness_revalidated_exact == 1u);
    CHECK(status.native_engine_calls_permitted == 0u);
    CHECK(status.actor_lifetime_authority == 0u);
    CHECK(status.mutation_authority == 0u);
    CHECK(result.valid == 1u);
    CHECK(result.witness_dispatch_serial == witness.dispatch_serial);
    CHECK(result.witness_native_thread_id == witness.native_thread_id);
    CHECK(result.witness_observer_registry_generation ==
        witness.observer_registry_generation);
    CHECK(result.witness_source == witness.source);
    calls_before = context.query_calls + context.copy_calls +
        context.preliminary_samples + context.final_samples +
        context.sink_calls;
    witness.dispatch_serial = 42u;
    run_service(&context, (void *)(uintptr_t)0x20000000u,
        (void *)(uintptr_t)0x20001000u, &witness);
    CHECK(calls_before == context.query_calls + context.copy_calls +
        context.preliminary_samples + context.final_samples +
        context.sink_calls);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureReset());
    memset(&result, 0xa5, sizeof(result));
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status,
        &result));
    CHECK(status.configured == 0u && status.completed_valid == 0u);
    CHECK(result.valid == 0u && result.failure == 0u &&
        result.checks_completed == 0u);
}

static void test_no_progress_write_permission_and_overflow(void) {
    TestContext context;
    SudekiMpTalosStagingNativeCaptureConfiguration config = configuration();
    SudekiMpControlUpdateDispatchWitness witness = exact_witness(50u);
    SudekiMpTalosStagingNativeCaptureStatus status;

    initialize_context(&context);
    context.missing[0].address = IMAGE_BASE + 0x100u;
    context.missing[0].size = 4u;
    context.missing_count = 1u;
    install_context(&context, &config);
    run_service(&context, (void *)(uintptr_t)0x21000000u,
        (void *)(uintptr_t)0x21001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_NO_PROGRESS);
    CHECK(context.sink_calls == 0u && context.final_samples == 0u);

    initialize_context(&context);
    context.dynamic_read_only = 1;
    context.missing[0].address = DYNAMIC_BASE + 0x20u;
    context.missing[0].size = 4u;
    context.missing[0].write_required = 1u;
    context.missing_count = 1u;
    install_context(&context, &config);
    witness.dispatch_serial = 51u;
    run_service(&context, (void *)(uintptr_t)0x21000000u,
        (void *)(uintptr_t)0x21001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_WRITE_PERMISSION);
    CHECK(context.sink_calls == 0u);

    initialize_context(&context);
    context.missing[0].address = 0xfffffff0u;
    context.missing[0].size = 0x40u;
    context.missing_count = 1u;
    install_context(&context, &config);
    witness.dispatch_serial = 52u;
    run_service(&context, (void *)(uintptr_t)0x21000000u,
        (void *)(uintptr_t)0x21001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_OVERFLOW);
    CHECK(context.sink_calls == 0u);
}

static void test_capacity_and_permission_drift_fail_closed(void) {
    TestContext context;
    SudekiMpTalosStagingNativeCaptureConfiguration config = configuration();
    SudekiMpControlUpdateDispatchWitness witness = exact_witness(60u);
    SudekiMpTalosStagingNativeCaptureStatus status;

    initialize_context(&context);
    context.range_cap_image = 1;
    context.split_image = 0;
    install_context(&context, &config);
    run_service(&context, (void *)(uintptr_t)0x22000000u,
        (void *)(uintptr_t)0x22001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_RANGE_CAPACITY);
    CHECK(context.preliminary_samples == 0u && context.sink_calls == 0u);

    initialize_context(&context);
    context.permission_drift_after_plan = 1;
    install_context(&context, &config);
    witness.dispatch_serial = 61u;
    run_service(&context, (void *)(uintptr_t)0x22000000u,
        (void *)(uintptr_t)0x22001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_PERMISSION_DRIFT);
    CHECK(context.planning_valid_seen == 1u);
    CHECK(context.final_a_calls == 0u && context.query_after_final == 0u);
    CHECK(context.sink_calls == 0u);

    initialize_context(&context);
    context.missing[0].address = DYNAMIC_BASE;
    context.missing[0].size = 0x000a2000u;
    context.missing_count = 1u;
    install_context(&context, &config);
    witness.dispatch_serial = 62u;
    run_service(&context, (void *)(uintptr_t)0x22000000u,
        (void *)(uintptr_t)0x22001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_BYTE_CAPACITY);
    CHECK(context.sink_calls == 0u);
}

static void test_invalid_final_not_published_throttle_wrap_and_witness(void) {
    TestContext context;
    SudekiMpTalosStagingNativeCaptureConfiguration config = configuration();
    SudekiMpControlUpdateDispatchWitness witness =
        exact_witness(UINT32_MAX);
    SudekiMpTalosStagingNativeCaptureStatus status;
    SudekiMpTalosStagingNativeSamplerResult result;

    initialize_context(&context);
    context.final_double_read = 1;
    install_context(&context, &config);
    run_service(&context, (void *)(uintptr_t)0x23000000u,
        (void *)(uintptr_t)0x23001000u, &witness);
    CHECK(context.sink_calls == 0u);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status,
        &result));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_SAMPLER);
    CHECK(status.sink_publications == 0u);
    CHECK(status.completed_valid == 0u);
    CHECK(result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_DOUBLE_READ);
    witness.dispatch_serial = 1u;
    run_service(&context, (void *)(uintptr_t)0x23000000u,
        (void *)(uintptr_t)0x23001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_THROTTLED);
    CHECK(status.retry_callbacks_remaining == 0u);
    CHECK(context.sink_calls == 0u);

    initialize_context(&context);
    context.witness_exact = 0;
    install_context(&context, &config);
    witness = exact_witness(70u);
    run_service(&context, (void *)(uintptr_t)0x23000000u,
        (void *)(uintptr_t)0x23001000u, &witness);
    CHECK(context.final_samples == 1u && context.sink_calls == 0u);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_WITNESS);
    CHECK(status.witness_revalidated_exact == 0u);
}

static void test_alias_entry_binding_and_duplicate_configuration(void) {
    TestContext context;
    SudekiMpTalosStagingNativeCaptureConfiguration config = configuration();
    SudekiMpControlUpdateDispatchWitness witness = exact_witness(80u);
    SudekiMpTalosStagingNativeCaptureStatus status;
    const void *cookie;

    initialize_context(&context);
    install_context(&context, &config);
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosCompanionStagingNativeCaptureConfigure(&config,
        test_sink, &context));
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);
    SetLastError(0x1234u);
    run_service(&context, (void *)(uintptr_t)0x24000000u,
        (void *)(uintptr_t)0x24000000u, &witness);
    CHECK(GetLastError() == 0x1234u);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_ALIAS);
    CHECK(context.query_calls == 0u && context.sink_calls == 0u);

    initialize_context(&context);
    install_context(&context, &config);
    cookie =
        SudekiMpTalosCompanionStagingNativeCaptureAttemptCookieForTesting();
    run_service(&context, (void *)cookie,
        (void *)(uintptr_t)0x24001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_ALIAS);
    CHECK(context.query_calls == 0u);

    initialize_context(&context);
    config.expected_observer_registry_generation = 10u;
    install_context(&context, &config);
    witness.observer_registry_generation = 9u;
    run_service(&context, (void *)(uintptr_t)0x24000000u,
        (void *)(uintptr_t)0x24001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_WITNESS);
    CHECK(status.bound_observer_registry_generation == 10u);
    CHECK(context.query_calls == 0u);

    initialize_context(&context);
    config.expected_observer_registry_generation = 0u;
    install_context(&context, &config);
    witness = exact_witness(81u);
    witness.source_exact = 0u;
    run_service(&context, (void *)(uintptr_t)0x24000000u,
        (void *)(uintptr_t)0x24001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.bound_observer_registry_generation == 0u);
    CHECK(context.query_calls == 0u);

    initialize_context(&context);
    install_context(&context, &config);
    witness = exact_witness(82u);
    ++witness.native_thread_id;
    if (witness.native_thread_id == 0u) witness.native_thread_id = 1u;
    run_service(&context, (void *)(uintptr_t)0x24000000u,
        (void *)(uintptr_t)0x24001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_WITNESS);
    CHECK(context.query_calls == 0u && context.preliminary_samples == 0u &&
        context.final_samples == 0u && context.sink_calls == 0u);
}

static void test_copy_failure_never_queries_or_publishes_after_boundary(void) {
    TestContext context;
    SudekiMpTalosStagingNativeCaptureConfiguration config = configuration();
    SudekiMpControlUpdateDispatchWitness witness = exact_witness(90u);
    SudekiMpTalosStagingNativeCaptureStatus status;

    initialize_context(&context);
    context.fail_final_copy = 1;
    install_context(&context, &config);
    run_service(&context, (void *)(uintptr_t)0x25000000u,
        (void *)(uintptr_t)0x25001000u, &witness);
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL));
    CHECK(status.failure == SUDEKIMP_TALOS_NATIVE_CAPTURE_COPY);
    CHECK(status.final_boundary_entered == 1u);
    CHECK(status.no_post_boundary_query == 1u);
    CHECK(context.query_after_final == 0u);
    CHECK(context.witness_calls == 0u && context.final_samples == 0u);
    CHECK(context.sink_calls == 0u);
}

int main(void) {
    test_success_order_exact_spans_and_inert();
    test_no_progress_write_permission_and_overflow();
    test_capacity_and_permission_drift_fail_closed();
    test_invalid_final_not_published_throttle_wrap_and_witness();
    test_alias_entry_binding_and_duplicate_configuration();
    test_copy_failure_never_queries_or_publishes_after_boundary();
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureReset());
    CHECK(SudekiMpTalosCompanionStagingNativeCaptureSetBackendForTesting(
        NULL));
    if (failures != 0) {
        fprintf(stderr, "talos companion native capture: %d failure(s)\n",
            failures);
        return 1;
    }
    puts("talos companion native capture: PASS");
    return 0;
}
