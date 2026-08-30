#include "hooks/talos_companion_staging_native_sampler.h"

#include "hooks/control_separation.h"

#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Talos companion staging native sampler requires 32-bit Windows"
#endif

enum {
    HERO_COUNT = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT,
    RVA_WORLD_GLOBAL = 0x00408d10u,
    RVA_UI_SCENE_GLOBAL = 0x00408d1cu,
    RVA_TSA_GLOBAL = 0x00408d4cu,
    RVA_SCENE_MANAGER_GLOBAL = 0x00408d58u,
    RVA_GROUP_GLOBAL = 0x00408d94u,
    RVA_GAME_SPEED_GLOBAL = 0x00408da0u,
    RVA_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_CAMERA_MANAGER_GLOBAL = 0x00409d7cu,
    RVA_AI_MANAGER_GLOBAL = 0x00409de4u,
    RVA_ASYNC_STREAM_GLOBAL = 0x003c30d4u,
    RVA_ASYNC_PENDING_GLOBAL = 0x003c30d8u,
    RVA_UI_CONTROLLER_GLOBAL = 0x003c2f88u,
    RVA_GAMEPLAY_HUD_GLOBAL = 0x003c2f9cu,
    RVA_STAT_CAMERA_MANAGER_GLOBAL = 0x003c2f30u,
    RVA_HUD_RESOURCE_TABLE_GLOBAL = 0x003c305cu,
    RVA_HUD_RESOURCE_INITIALIZED = 0x00409e09u,
    RVA_STAT_CAMERA_INITIALIZED = 0x00409e08u,
    RVA_STAT_CAMERA_SAVED_BOUNDS = 0x003c303cu,
    RVA_STAT_CAMERA_ACTIVE_BOUNDS = 0x0040cdc0u,
    RVA_HUD_RESOURCE_INLINE_TEXT = 0x002d2258u,
    RVA_CONTROLLER_VTABLE = 0x002c9f5cu,
    RVA_UI_CONTROLLER_VTABLE = 0x002caf9cu,
    RVA_UI_CONTROLLER_DISPATCH = 0x0009d9b0u,
    RVA_HUD_VTABLE = 0x002cb3e4u,
    RVA_HUD_DISPATCH = 0x000a5930u,
    RVA_AI_LISTENER_VTABLE = 0x002ca244u,
    RVA_AI_LISTENER_ADD = 0x000f2b00u,
    RVA_AI_LISTENER_REMOVE = 0x000f2b30u,
    RVA_GIZMO_PRIMARY_VTABLE = 0x002cb590u,
    RVA_GIZMO_SECONDARY_VTABLE = 0x002cb59cu,
    RVA_STAT_PRIMARY_VTABLE = 0x002d21e4u,
    RVA_STAT_SECONDARY_VTABLE = 0x002d2224u,
    WORLD_SOURCE_OFFSET = 0x0cu,
    WORLD_TRANSITION_PENDING_OFFSET = 0x10u,
    GROUP_LISTENER_COUNT_OFFSET = 0x38u,
    GROUP_LISTENER_STORAGE_OFFSET = 0x40u,
    GROUP_PENDING_A_OFFSET = 0x58u,
    GROUP_PENDING_B_OFFSET = 0x5cu,
    GROUP_MEMBERS_OFFSET = 0x90u,
    GROUP_MEMBER_STRIDE = 0x0cu,
    GROUP_COUNT_OFFSET = 0xccu,
    GROUP_COMBAT_OFFSET = 0xd4u,
    AI_LISTENER_OFFSET = 0x44u,
    AI_FORMATION_OFFSET = 0xf4u,
    AI_FORMATION_COUNT_OFFSET = 0x124u,
    FORMATION_CANONICAL_DESTINATION_OFFSET = 0x34u,
    FORMATION_DISTANCE_OFFSET = 0x38u,
    FORMATION_BRANCH_OFFSET = 0x50u,
    LISTENER_TO_FORMATION_OFFSET = 0xb0u,
    CONTROLLER_CURRENT_MODE_OFFSET = 0x80u,
    CONTROLLER_REQUESTED_MODE_OFFSET = 0x84u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    ACTOR_COMBAT_DATA_OFFSET = 0x4cu,
    ACTOR_ARBITER_OFFSET = 0x90u,
    ACTOR_CONTROL_OFFSET = 0x94u,
    ACTOR_FORMATION_COMPONENT_OFFSET = 0x44u,
    ACTOR_STATUS_OWNER_OFFSET = 0xa8u,
    ACTOR_STAT_DISPLAY_OFFSET = 0xb0u,
    CONTROL_OWNER_OFFSET = 0x10u,
    CONTROL_STATE_OFFSET = 0x3cu,
    CONTROL_FORMATION_OFFSET = 0x40u,
    CONTROL_FORMATION_DISTANCE_OFFSET = 0x150u,
    CONTROL_OVERRIDE_OFFSET = 0x16au,
    CONTROL_STATE_MODE_OFFSET = 0x0bu,
    UI_HUD_OFFSET = 0x6cu,
    UI_SCENE_CONTROLLER_OFFSET = 0x170u,
    UI_SCENE_HEALTH_THRESHOLD_OFFSET = 0x148u,
    HUD_GIZMO_ARRAY_OFFSET = 0x138u,
    GIZMO_SETTER_MODE_OFFSET = 0x28u,
    GIZMO_FLAGS_A_OFFSET = 0x2a4u,
    GIZMO_FLAGS_B_OFFSET = 0x2acu,
    GIZMO_LABEL_OFFSET = 0x2e0u,
    GIZMO_LABEL_DATA_OFFSET = 0x04u,
    GIZMO_STATE_OFFSET = 0x324u,
    GIZMO_INDEX_OFFSET = 0x32cu,
    FORMATION_COMPONENT_SCALAR_A_OFFSET = 0x18u,
    FORMATION_COMPONENT_SCALAR_B_OFFSET = 0x1cu,
    FORMATION_COMPONENT_SCALAR_C_OFFSET = 0x20u,
    STAT_HEALTH_BAR_OFFSET = 0xd0u,
    STAT_LAST_HP_OFFSET = 0x16cu,
    STAT_SCENE_NODE_OFFSET = 0x58u,
    STAT_OWNER_OFFSET = 0xccu,
    STAT_OWNER_NODE_OFFSET = 0x08u,
    STAT_BAR_HANDLES_OFFSET = 0x4cu,
    STAT_BAR_RENDERERS_OFFSET = 0x34u,
    STAT_BAR_COUNT_OFFSET = 0x50u,
    STAT_BAR_FILLS_OFFSET = 0x58u,
    COMBAT_CURRENT_HP_OFFSET = 0x2cu,
    COMBAT_MAX_HP_OFFSET = 0x30u,
    COMBAT_ID7_VALUE_OFFSET = 0x34u,
    COMBAT_HUD_RESOURCE_INDEX_OFFSET = 0x40u,
    ARBITER_OWNER_OFFSET = 0x10u,
    ARBITER_FLAGS_50_OFFSET = 0x50u,
    ARBITER_STATE_OFFSET = 0x58u,
    ARBITER_FLAGS_OFFSET = 0x60u,
    CAMERA_CURRENT_OFFSET = 0x20u,
    CAMERA_TABLE_OFFSET = 0x24u,
    CAMERA_TABLE_COUNT = 10u,
    CAMERA_RENDER_STATE_OFFSET = 0x34u,
    CAMERA_NAME_OFFSET = 0x4cu,
    SCENE_RENDERER_OFFSET = 0x40u,
    SCENE_CAMERA_STATE_OFFSET = 0x7cu,
    SCENE_NODE_DIRTY_OFFSET = 0x2cu,
    SCENE_NODE_MATRIX_OFFSET = 0x90u,
    SCENE_NODE_MATRIX_FLOAT_COUNT = 16u,
    STAT_CAMERA_ACTIVE_OFFSET = 0x20u,
    STAT_CAMERA_PAYLOAD_OFFSET = 0x34u,
    STAT_CAMERA_POSITION_OFFSET = 0xc0u,
    UI_SCENE_LAST_BOUNDS_OFFSET = 0x158u,
    HUD_RESOURCE_SET_ENTRIES_OFFSET = 0x04u,
    HUD_RESOURCE_SET_COUNT_OFFSET = 0x09u,
    HUD_RESOURCE_ENTRY_ID_OFFSET = 0x19u,
    HUD_RESOURCE_TABLE_COUNT_OFFSET = 0x08u,
    HUD_RESOURCE_TABLE_DATA_OFFSET = 0x10u,
    HUD_RESOURCE_TABLE_FIRST_SLOT_OFFSET = 0x4d8u,
    HUD_RESOURCE_FIRST_TABLE_ID = 0x136u,
    HUD_RESOURCE_SLOT_COUNT = 11u,
    GAME_SPEED_PAUSED_OFFSET = 0x28u,
    TSA_PLAYING_OFFSET = 0x74u,
    MAX_STAT_BAR_ENTRIES = 32u,
    TPTR_SIZE = 0x0cu,
    HUD_STRING_INLINE_CAPACITY_UNITS = 28u,
    HUD_STRING_PROOF_MAX_UNITS = 27u,
    MAX_LABEL_UNITS = SUDEKIMP_TALOS_STAGING_RESEARCH_HUD_LABEL_MAX_LENGTH
};

typedef struct SampleContext {
    const SudekiMpTalosStagingNativeSamplerInput *input;
    SudekiMpTalosStagingNativeSamplerResult *result;
} SampleContext;

static const uint32_t actor_vtables[HERO_COUNT][3] = {
    {0x002d5010u, 0x002d5034u, 0x002d5054u},
    {0x002d555cu, 0x002d5580u, 0x002d55a0u},
    {0x002d5a88u, 0x002d5aacu, 0x002d5accu},
    {0x002d66fcu, 0x002d6720u, 0x002d6740u}
};

static int finite_float_bits(uint32_t bits);
static float bits_to_float(uint32_t bits);
static uint32_t bytes_u32(const uint8_t *bytes);

static void initialize_result(
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    memset(result, 0, sizeof(*result));
    result->observation_only = 1u;
    result->native_engine_calls_permitted = 0u;
    result->hooks_permitted = 0u;
    result->actor_lifetime_authority = 0u;
    result->mutation_authority = 0u;
    result->external_sha256_required = 1u;
    result->membership_abi_required = 1u;
}

static int fail_at(SampleContext *context, uint32_t failure, uint32_t address) {
    if (context->result->failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK) {
        context->result->failure = failure;
        context->result->failed_address = address;
    }
    memset(&context->result->snapshot, 0,
        sizeof(context->result->snapshot));
    context->result->valid = 0u;
    return 0;
}

static int fail_span(
    SampleContext *context,
    uint32_t failure,
    uint32_t address,
    size_t size,
    uint8_t write_required
) {
    if (context->result->failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK) {
        context->result->failed_size = size > UINT32_MAX ? UINT32_MAX :
            (uint32_t)size;
        context->result->failed_write_required = write_required;
    }
    return fail_at(context, failure, address);
}

static int bool_exact(uint8_t value) {
    return value == 0u || value == 1u;
}

static int add_address(uint32_t base, uint32_t offset, uint32_t *address) {
    if (address == NULL || base > UINT32_MAX - offset) return 0;
    *address = base + offset;
    return 1;
}

static int host_span_valid(const uint8_t *bytes, uint32_t size) {
    uintptr_t start;

    if (bytes == NULL || size == 0u) return 0;
    start = (uintptr_t)bytes;
    return start <= UINTPTR_MAX - ((uintptr_t)size - 1u);
}

static int range_layout_valid(const SudekiMpTalosStagingNativeView *view) {
    size_t i;
    size_t j;
    uint64_t total_size = 0u;

    if (view == NULL || view->ranges == NULL || view->range_count == 0u ||
        view->range_count > SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_RANGES) {
        return 0;
    }
    for (i = 0u; i < view->range_count; ++i) {
        uint64_t first_end;
        uint64_t first_host;
        uint64_t first_host_end;

        if (view->ranges[i].address == 0u ||
            !host_span_valid(view->ranges[i].bytes,
                view->ranges[i].size) ||
            view->ranges[i].native_readable != 1u ||
            !bool_exact(view->ranges[i].native_writable) ||
            view->ranges[i].reserved[0] != 0u ||
            view->ranges[i].reserved[1] != 0u) {
            return 0;
        }
        first_end = (uint64_t)view->ranges[i].address +
            (uint64_t)view->ranges[i].size;
        if (first_end > UINT64_C(0x100000000)) return 0;
        first_host = (uint64_t)(uintptr_t)view->ranges[i].bytes;
        first_host_end = first_host + (uint64_t)view->ranges[i].size;
        if (first_host_end < first_host) return 0;
        total_size += (uint64_t)view->ranges[i].size;
        if (total_size >
                SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_CAPTURE_BYTES) return 0;
        for (j = i + 1u; j < view->range_count; ++j) {
            uint64_t second_end;
            uint64_t second_host;
            uint64_t second_host_end;

            if (view->ranges[j].address == 0u ||
                !host_span_valid(view->ranges[j].bytes,
                    view->ranges[j].size) ||
                view->ranges[j].native_readable != 1u ||
                !bool_exact(view->ranges[j].native_writable) ||
                view->ranges[j].reserved[0] != 0u ||
                view->ranges[j].reserved[1] != 0u) return 0;
            second_end = (uint64_t)view->ranges[j].address +
                (uint64_t)view->ranges[j].size;
            if (second_end > UINT64_C(0x100000000)) return 0;
            second_host = (uint64_t)(uintptr_t)view->ranges[j].bytes;
            second_host_end = second_host +
                (uint64_t)view->ranges[j].size;
            if (second_host_end < second_host) return 0;
            if ((uint64_t)view->ranges[i].address < second_end &&
                (uint64_t)view->ranges[j].address < first_end) return 0;
            if (first_host < second_host_end &&
                second_host < first_host_end) return 0;
        }
    }
    return 1;
}

static int capture_storage_disjoint(
    const SudekiMpTalosStagingNativeView *first,
    const SudekiMpTalosStagingNativeView *second
) {
    size_t i;
    size_t j;

    for (i = 0u; i < first->range_count; ++i) {
        uint64_t first_start = (uint64_t)(uintptr_t)first->ranges[i].bytes;
        uint64_t first_end = first_start + first->ranges[i].size;

        if (first_end < first_start) return 0;
        for (j = 0u; j < second->range_count; ++j) {
            uint64_t second_start =
                (uint64_t)(uintptr_t)second->ranges[j].bytes;
            uint64_t second_end = second_start + second->ranges[j].size;

            if (second_end < second_start ||
                (first_start < second_end && second_start < first_end)) {
                return 0;
            }
        }
    }
    return 1;
}

static uint32_t view_capture_bytes(
    const SudekiMpTalosStagingNativeView *view
) {
    uint32_t total = 0u;
    size_t index;

    for (index = 0u; index < view->range_count; ++index) {
        total += view->ranges[index].size;
    }
    return total;
}

static const SudekiMpTalosStagingNativeReadableRange *view_range(
    const SudekiMpTalosStagingNativeView *view,
    uint32_t address,
    size_t size
) {
    size_t i;
    uint64_t requested_end;

    if (view == NULL || size == 0u) return NULL;
    requested_end = (uint64_t)address + (uint64_t)size;
    if (requested_end > UINT64_C(0x100000000)) return NULL;
    for (i = 0u; i < view->range_count; ++i) {
        uint64_t range_start = view->ranges[i].address;
        uint64_t range_end = range_start + view->ranges[i].size;

        if ((uint64_t)address >= range_start && requested_end <= range_end) {
            return &view->ranges[i];
        }
    }
    return NULL;
}

static const uint8_t *view_bytes(
    const SudekiMpTalosStagingNativeView *view,
    uint32_t address,
    size_t size
) {
    const SudekiMpTalosStagingNativeReadableRange *range;

    range = view_range(view, address, size);
    return range == NULL ? NULL : range->bytes +
        (size_t)((uint64_t)address - (uint64_t)range->address);
}

static int require_writable_pair(
    SampleContext *context,
    uint32_t address,
    size_t size
) {
    const SudekiMpTalosStagingNativeReadableRange *first;
    const SudekiMpTalosStagingNativeReadableRange *second;

    first = view_range(&context->input->first, address, size);
    second = view_range(&context->input->second, address, size);
    if (first == NULL || second == NULL) {
        return fail_span(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            address, size, 1u);
    }
    ++context->result->checks_completed;
    if (first->native_writable != 1u || second->native_writable != 1u) {
        return fail_span(context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION, address, size,
            1u);
    }
    return 1;
}

static int require_writable_at(
    SampleContext *context,
    uint32_t base,
    uint32_t offset,
    size_t size
) {
    uint32_t address;

    if (!add_address(base, offset, &address)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            base);
    }
    return require_writable_pair(context, address, size);
}

static int read_pair(
    SampleContext *context,
    uint32_t address,
    void *destination,
    size_t size
) {
    const uint8_t *first;
    const uint8_t *second;

    first = view_bytes(&context->input->first, address, size);
    second = view_bytes(&context->input->second, address, size);
    if (first == NULL || second == NULL) {
        return fail_span(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            address, size, 0u);
    }
    if (memcmp(first, second, size) != 0) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_DOUBLE_READ,
            address);
    }
    memcpy(destination, first, size);
    ++context->result->checks_completed;
    return 1;
}

static int read_u8(SampleContext *context, uint32_t address, uint8_t *value) {
    return read_pair(context, address, value, sizeof(*value));
}

static int read_u16(
    SampleContext *context,
    uint32_t address,
    uint16_t *value
) {
    return read_pair(context, address, value, sizeof(*value));
}

static int read_u32(
    SampleContext *context,
    uint32_t address,
    uint32_t *value
) {
    return read_pair(context, address, value, sizeof(*value));
}

static int read_at_u8(
    SampleContext *context,
    uint32_t base,
    uint32_t offset,
    uint8_t *value
) {
    uint32_t address;

    if (!add_address(base, offset, &address)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            base);
    }
    return read_u8(context, address, value);
}

static int read_pair_at(
    SampleContext *context,
    uint32_t base,
    uint32_t offset,
    void *destination,
    size_t size
) {
    uint32_t address;

    if (!add_address(base, offset, &address)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            base);
    }
    return read_pair(context, address, destination, size);
}

static int read_at_u16(
    SampleContext *context,
    uint32_t base,
    uint32_t offset,
    uint16_t *value
) {
    uint32_t address;

    if (!add_address(base, offset, &address)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            base);
    }
    return read_u16(context, address, value);
}

static int read_at_u32(
    SampleContext *context,
    uint32_t base,
    uint32_t offset,
    uint32_t *value
) {
    uint32_t address;

    if (!add_address(base, offset, &address)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY,
            base);
    }
    return read_u32(context, address, value);
}

static int image_address(
    SampleContext *context,
    uint32_t rva,
    uint32_t *address
) {
    if (rva >= context->input->mapped_image_size ||
        !add_address(context->input->loaded_image_base, rva, address)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_FACT,
            rva);
    }
    return 1;
}

static int address_in_loaded_image(
    const SampleContext *context,
    uint32_t address
) {
    uint64_t image_end;

    if (context == NULL || context->input == NULL) return 0;
    image_end = (uint64_t)context->input->loaded_image_base +
        (uint64_t)context->input->mapped_image_size;
    return (uint64_t)address >= context->input->loaded_image_base &&
        (uint64_t)address < image_end;
}

static int read_global_pointer(
    SampleContext *context,
    uint32_t rva,
    uint32_t *pointer
) {
    uint32_t address;

    return image_address(context, rva, &address) &&
        read_u32(context, address, pointer) && *pointer != 0u;
}

static uint64_t mix_token(uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value == 0u ? UINT64_C(1) : value;
}

static uint64_t address_token(const SampleContext *context, uint32_t address) {
    if (address == 0u) return 0u;
    return mix_token((uint64_t)address ^ context->input->identity_salt);
}

static uint64_t integer_token(
    const SampleContext *context,
    uint64_t value,
    uint64_t domain
) {
    if (value == 0u) return 0u;
    return mix_token(value ^ context->input->identity_salt ^ domain);
}

static int borrowed_address(const void *pointer, uint32_t *address) {
    uint64_t value = (uint64_t)(uintptr_t)pointer;

    if (pointer == NULL || value > UINT32_MAX || address == NULL) return 0;
    *address = (uint32_t)value;
    return *address != 0u;
}

static int actor_hero(
    SampleContext *context,
    uint32_t actor,
    uint8_t *hero
) {
    uint32_t observed[3];
    unsigned int candidate;
    unsigned int offset;

    if (actor == 0u || hero == NULL ||
        !read_at_u32(context, actor, 0u, &observed[0]) ||
        !read_at_u32(context, actor, 0x08u, &observed[1]) ||
        !read_at_u32(context, actor, 0x2cu, &observed[2])) return 0;
    for (candidate = 0u; candidate < HERO_COUNT; ++candidate) {
        int matches = 1;

        for (offset = 0u; offset < 3u; ++offset) {
            uint32_t expected;

            if (!image_address(context, actor_vtables[candidate][offset],
                    &expected)) return 0;
            if (observed[offset] != expected) matches = 0;
        }
        if (matches) {
            *hero = (uint8_t)candidate;
            return 1;
        }
    }
    return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HERO_IDENTITY,
        actor);
}

/* TPtr<T> is a weak intrusive observer: +0 is the observed object and +4/+8
 * are the previous/next observer nodes.  The native assignment/cleanup path
 * writes the node and the reciprocal link it observes, so local reciprocity
 * and write permission are part of the finite preflight.  This proves no
 * ownership and deliberately does not traverse an unbounded observer list. */
static int sample_tptr_node(
    SampleContext *context,
    uint32_t node,
    uint32_t expected_object,
    uint32_t failure
) {
    uint8_t record[TPTR_SIZE];
    uint8_t head_record[TPTR_SIZE];
    uint8_t neighbor[TPTR_SIZE];
    uint32_t object;
    uint32_t previous;
    uint32_t next;
    uint32_t object_head;
    uint32_t object_head_address;
    uint32_t head_previous_address;
    uint32_t reciprocal_address;

    if (!read_pair(context, node, record, sizeof(record))) return 0;
    object = bytes_u32(record);
    previous = bytes_u32(record + 4u);
    next = bytes_u32(record + 8u);
    if (object != expected_object || previous == node || next == node ||
        (previous != 0u && previous == next) ||
        !add_address(expected_object, 4u, &object_head_address) ||
        !read_u32(context, object_head_address, &object_head) ||
        object_head == 0u ||
        !read_pair(context, object_head, head_record,
            sizeof(head_record)) ||
        bytes_u32(head_record) != expected_object ||
        bytes_u32(head_record + 4u) != 0u ||
        !add_address(object_head, 4u, &head_previous_address) ||
        !require_writable_pair(context, node, sizeof(record)) ||
        !require_writable_pair(context, object_head_address, 4u) ||
        !require_writable_pair(context, head_previous_address, 4u)) {
        return fail_at(context, failure, node);
    }
    if (previous == 0u) {
        if (object_head != node) return fail_at(context, failure, node);
    } else {
        if (!add_address(previous, 8u, &reciprocal_address) ||
            !read_pair(context, previous, neighbor, sizeof(neighbor)) ||
            bytes_u32(neighbor) != expected_object ||
            bytes_u32(neighbor + 8u) != node ||
            !require_writable_pair(context, reciprocal_address, 4u)) {
            return fail_at(context, failure, previous);
        }
    }
    if (next != 0u) {
        if (!add_address(next, 4u, &reciprocal_address) ||
            !read_pair(context, next, neighbor, sizeof(neighbor)) ||
         bytes_u32(neighbor) != expected_object ||
         bytes_u32(neighbor + 4u) != node ||
            !require_writable_pair(context, reciprocal_address, 4u)) {
            return fail_at(context, failure, next);
        }
    }
    return 1;
}

static int sample_formation_mutation_closure(
    SampleContext *context,
    uint32_t formation
) {
    uint32_t distance_bits;

    if (!read_at_u32(context, formation, FORMATION_DISTANCE_OFFSET,
            &distance_bits) || !finite_float_bits(distance_bits)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION,
            formation);
    }
    return require_writable_at(context, formation,
            FORMATION_CANONICAL_DESTINATION_OFFSET, 4u) &&
        require_writable_at(context, formation,
            FORMATION_DISTANCE_OFFSET, 4u) &&
        require_writable_at(context, formation,
            FORMATION_BRANCH_OFFSET, 1u);
}

static int sample_actor_formation_closure(
    SampleContext *context,
    uint32_t actor,
    uint32_t control
) {
    uint32_t component;
    uint32_t control_distance_bits;
    uint32_t scalar_a_bits;
    uint32_t scalar_b_bits;
    uint32_t scalar_c_bits;

    if (!read_at_u32(context, actor, ACTOR_FORMATION_COMPONENT_OFFSET,
            &component) || component == 0u ||
        !read_at_u32(context, control,
            CONTROL_FORMATION_DISTANCE_OFFSET, &control_distance_bits) ||
        !finite_float_bits(control_distance_bits) ||
        !read_at_u32(context, component,
            FORMATION_COMPONENT_SCALAR_A_OFFSET, &scalar_a_bits) ||
        !finite_float_bits(scalar_a_bits) ||
        !read_at_u32(context, component,
            FORMATION_COMPONENT_SCALAR_B_OFFSET, &scalar_b_bits) ||
        !finite_float_bits(scalar_b_bits) ||
        !read_at_u32(context, component,
            FORMATION_COMPONENT_SCALAR_C_OFFSET, &scalar_c_bits) ||
        !finite_float_bits(scalar_c_bits)) {
        return fail_at(context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT, actor);
    }
    return 1;
}

static uint64_t fnv1a_utf16_bytes(const uint8_t *bytes, uint32_t units) {
    uint64_t hash = UINT64_C(14695981039346656037);
    uint32_t index;

    for (index = 0u; index < units * 2u; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int address_spans_overlap(
    uint32_t first,
    uint32_t first_size,
    uint32_t second,
    uint32_t second_size
) {
    uint64_t first_end = (uint64_t)first + (uint64_t)first_size;
    uint64_t second_end = (uint64_t)second + (uint64_t)second_size;

    if (first_size == 0u || second_size == 0u ||
        first_end > UINT64_C(0x100000000) ||
        second_end > UINT64_C(0x100000000)) return 1;
    return (uint64_t)first < second_end && (uint64_t)second < first_end;
}

static int read_bounded_utf16_text(
    SampleContext *context,
    uint32_t source,
    uint32_t *length,
    uint64_t *hash
) {
    uint8_t bytes[(MAX_LABEL_UNITS + 1u) * 2u];
    uint32_t unit;
    uint32_t unit_address;

    if (source == 0u || length == NULL || hash == NULL) return 0;
    for (unit = 0u; unit <= MAX_LABEL_UNITS; ++unit) {
        if (!add_address(source, unit * 2u, &unit_address) ||
            !read_pair(context, unit_address,
                bytes + unit * 2u, 2u)) return 0;
        if (bytes[unit * 2u] == 0u && bytes[unit * 2u + 1u] == 0u) {
            if (unit == 0u) {
                return fail_at(context,
                    SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE, source);
            }
            *length = unit;
            *hash = fnv1a_utf16_bytes(bytes, unit);
            return *hash != 0u;
        }
    }
    return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
        source);
}

static int sample_hud_resource_closure(
    SampleContext *context,
    uint32_t actor,
    uint32_t resource_set,
    uint32_t combat_data,
    uint32_t gizmo,
    const SudekiMpTalosStagingResearchHeroEvidence *evidence
) {
    uint32_t index_bits;
    float index_float;
    int selected_index;
    uint8_t resource_count;
    uint32_t entries;
    uint32_t entry;
    uint8_t resource_id;
    uint32_t initialized_address = 0u;
    uint8_t initialized;
    uint32_t source;
    uint32_t source_object;
    uint32_t source_control;
    uint32_t table_global_address;
    uint32_t table = 0u;
    uint32_t table_count;
    uint32_t table_data;
    uint32_t table_id;
    uint32_t destination;
    uint32_t destination_control;
    uint32_t source_length;
    uint32_t source_declared_length = 0u;
    uint32_t entry_address;
    uint32_t source_size;
    uint32_t destination_size =
        4u + HUD_STRING_INLINE_CAPACITY_UNITS * 2u;
    uint64_t source_hash;

    if (evidence == NULL ||
        !read_at_u32(context, combat_data,
            COMBAT_HUD_RESOURCE_INDEX_OFFSET, &index_bits) ||
        !finite_float_bits(index_bits)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
            combat_data);
    }
    index_float = bits_to_float(index_bits);
    if (index_float < 0.0f || index_float > 63.0f) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
            combat_data + COMBAT_HUD_RESOURCE_INDEX_OFFSET);
    }
    selected_index = (int)index_float;
    if ((float)selected_index != index_float ||
        !read_at_u8(context, resource_set, HUD_RESOURCE_SET_COUNT_OFFSET,
            &resource_count) || resource_count == 0u ||
        selected_index >= (int)resource_count ||
        !read_at_u32(context, resource_set, HUD_RESOURCE_SET_ENTRIES_OFFSET,
            &entries) || entries == 0u ||
        !add_address(entries, (uint32_t)selected_index * 4u,
            &entry_address) ||
        !read_u32(context, entry_address, &entry) || entry == 0u ||
        !read_at_u8(context, entry, HUD_RESOURCE_ENTRY_ID_OFFSET,
            &resource_id) || resource_id == 0u ||
        resource_id > HUD_RESOURCE_SLOT_COUNT) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
            resource_set);
    }
    if (!image_address(context, RVA_HUD_RESOURCE_INITIALIZED,
            &initialized_address) ||
        !read_u8(context, initialized_address, &initialized) ||
        initialized > 1u) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
            initialized_address);
    }
    if (initialized != 0u) {
        if (!image_address(context, RVA_HUD_RESOURCE_INLINE_TEXT, &source)) {
            return 0;
        }
    } else {
        if (!image_address(context, RVA_HUD_RESOURCE_TABLE_GLOBAL,
                &table_global_address) ||
            !read_u32(context, table_global_address, &table) || table == 0u ||
            !read_at_u32(context, table, HUD_RESOURCE_TABLE_COUNT_OFFSET,
                &table_count) ||
            !read_at_u32(context, table, HUD_RESOURCE_TABLE_DATA_OFFSET,
                &table_data) || table_data == 0u) {
            return fail_at(context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE, table);
        }
        table_id = HUD_RESOURCE_FIRST_TABLE_ID + (uint32_t)resource_id - 1u;
        if (table_count <= table_id ||
            !read_at_u32(context, table_data,
                HUD_RESOURCE_TABLE_FIRST_SLOT_OFFSET +
                    ((uint32_t)resource_id - 1u) * 4u,
                &source_object) || source_object == 0u ||
            !read_at_u32(context, source_object, 0u, &source_control)) {
            return fail_at(context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE, table);
        }
        if ((source_control & UINT32_C(0x80000000)) != 0u) {
            source_declared_length =
                source_control & UINT32_C(0x7fffffff);
            if (source_declared_length > HUD_STRING_PROOF_MAX_UNITS ||
                !add_address(source_object, 4u, &source)) {
                return fail_at(context,
                    SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
                    source_object);
            }
        } else if (!read_at_u32(context, source_object, 4u, &source) ||
                source == 0u) {
            return fail_at(context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE, source_object);
        } else {
            source_declared_length =
                source_control & UINT32_C(0x7fffffff);
        }
    }
    if (!read_bounded_utf16_text(context, source, &source_length,
            &source_hash) ||
        source_length > HUD_STRING_PROOF_MAX_UNITS ||
        (initialized == 0u && source_declared_length != source_length) ||
        source_length != evidence->gizmo_label_length ||
        source_hash != evidence->gizmo_label_hash ||
        !add_address(gizmo, GIZMO_LABEL_OFFSET, &destination) ||
        !read_u32(context, destination, &destination_control) ||
        (destination_control & UINT32_C(0x80000000)) == 0u ||
        (destination_control & UINT32_C(0x7fffffff)) >
            HUD_STRING_PROOF_MAX_UNITS) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
            actor);
    }
    source_size = (source_length + 1u) * 2u;
    if (address_spans_overlap(source, source_size, destination,
            destination_size) ||
        !require_writable_pair(context, destination, destination_size)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE,
            actor);
    }
    return 1;
}

static int finite_float_array(const uint8_t *bytes, unsigned int count) {
    unsigned int index;

    for (index = 0u; index < count; ++index) {
        if (!finite_float_bits(bytes_u32(bytes + index * 4u))) return 0;
    }
    return 1;
}

static int sample_stat_camera_sync_closure(
    SampleContext *context,
    uint32_t display
) {
    uint32_t initialized_address = 0u;
    uint8_t initialized;
    uint32_t ui_scene;
    uint8_t bounds[16];
    uint32_t saved_bounds;
    uint32_t active_bounds;
    uint32_t display_node;
    uint32_t owner;
    uint32_t owner_node;
    uint16_t dirty;
    uint8_t matrix[SCENE_NODE_MATRIX_FLOAT_COUNT * 4u];
    uint32_t manager_global;
    uint32_t manager;
    uint32_t active_camera;
    uint32_t payload;
    uint32_t position;
    uint8_t position_bytes[12];

    if (!image_address(context, RVA_STAT_CAMERA_INITIALIZED,
            &initialized_address) ||
        !read_u8(context, initialized_address, &initialized) ||
        initialized > 1u) {
        return fail_at(context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_CAMERA_SYNC,
            initialized_address);
    }
    if (initialized == 0u) {
        if (!read_global_pointer(context, RVA_UI_SCENE_GLOBAL, &ui_scene) ||
            !read_pair_at(context, ui_scene,
                UI_SCENE_LAST_BOUNDS_OFFSET - 12u,
                bounds, sizeof(bounds)) ||
            !finite_float_array(bounds, 4u) ||
            !image_address(context, RVA_STAT_CAMERA_SAVED_BOUNDS,
                &saved_bounds) ||
            !image_address(context, RVA_STAT_CAMERA_ACTIVE_BOUNDS,
                &active_bounds) ||
            !require_writable_pair(context, initialized_address, 1u) ||
            !require_writable_pair(context, saved_bounds, sizeof(bounds)) ||
            !require_writable_pair(context, active_bounds, sizeof(bounds))) {
            return fail_at(context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_CAMERA_SYNC, ui_scene);
        }
    }
    if (!read_at_u32(context, display, STAT_SCENE_NODE_OFFSET,
            &display_node) || display_node == 0u ||
        !read_at_u32(context, display, STAT_OWNER_OFFSET, &owner) ||
        owner == 0u ||
        !read_at_u32(context, owner, STAT_OWNER_NODE_OFFSET, &owner_node) ||
        owner_node == 0u || owner_node == display_node ||
        !read_at_u16(context, owner_node, SCENE_NODE_DIRTY_OFFSET, &dirty) ||
        !read_pair_at(context, owner_node, SCENE_NODE_MATRIX_OFFSET,
            matrix, sizeof(matrix)) || !finite_float_array(matrix,
                SCENE_NODE_MATRIX_FLOAT_COUNT) ||
        !require_writable_at(context, owner_node,
            SCENE_NODE_DIRTY_OFFSET, sizeof(dirty)) ||
        !require_writable_at(context, owner_node,
            SCENE_NODE_MATRIX_OFFSET, sizeof(matrix)) ||
        !read_at_u16(context, display_node, SCENE_NODE_DIRTY_OFFSET, &dirty) ||
        !read_pair_at(context, display_node, SCENE_NODE_MATRIX_OFFSET,
            matrix, sizeof(matrix)) || !finite_float_array(matrix,
                SCENE_NODE_MATRIX_FLOAT_COUNT) ||
        !require_writable_at(context, display_node,
            SCENE_NODE_DIRTY_OFFSET, sizeof(dirty)) ||
        !require_writable_at(context, display_node,
            SCENE_NODE_MATRIX_OFFSET, sizeof(matrix)) ||
        !image_address(context, RVA_STAT_CAMERA_MANAGER_GLOBAL,
            &manager_global) ||
        !read_u32(context, manager_global, &manager) || manager == 0u ||
        !read_at_u32(context, manager, STAT_CAMERA_ACTIVE_OFFSET,
            &active_camera) || active_camera == 0u ||
        !read_at_u32(context, active_camera, STAT_CAMERA_PAYLOAD_OFFSET,
            &payload) || payload == 0u ||
        !add_address(payload, STAT_CAMERA_POSITION_OFFSET, &position) ||
        !read_pair(context, position, position_bytes, sizeof(position_bytes)) ||
        !finite_float_array(position_bytes, 3u)) {
        return fail_at(context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_CAMERA_SYNC, display);
    }
    return 1;
}

static int sample_remove_epilogue_closure(
    SampleContext *context,
    uint32_t actor,
    uint32_t combat_data,
    uint32_t gizmo,
    uint32_t ui_scene
) {
    static const uint32_t status_offsets[5] = {
        0x40u, 0x44u, 0x50u, 0x54u, 0x58u
    };
    uint32_t threshold_bits;
    uint32_t id7_bits;
    uint32_t status_owner;
    uint32_t target;
    uint8_t status_value;
    unsigned int index;

    if (!read_at_u32(context, ui_scene,
            UI_SCENE_HEALTH_THRESHOLD_OFFSET, &threshold_bits) ||
        !finite_float_bits(threshold_bits) ||
        !read_at_u32(context, combat_data, COMBAT_ID7_VALUE_OFFSET,
            &id7_bits) || !finite_float_bits(id7_bits) ||
        !read_at_u32(context, actor, ACTOR_STATUS_OWNER_OFFSET,
            &status_owner) || status_owner == 0u ||
        !require_writable_at(context, combat_data,
            COMBAT_ID7_VALUE_OFFSET, 4u) ||
        !require_writable_at(context, gizmo, GIZMO_FLAGS_A_OFFSET, 4u) ||
        !require_writable_at(context, gizmo, GIZMO_FLAGS_B_OFFSET, 4u) ||
        !require_writable_at(context, gizmo, GIZMO_STATE_OFFSET, 4u)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD,
            actor);
    }
    for (index = 0u; index < 5u; ++index) {
        if (!read_at_u32(context, status_owner, status_offsets[index],
                &target) || target == 0u ||
            !read_at_u8(context, target, 0x4cu, &status_value)) {
            return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD,
                status_owner);
        }
    }
    return 1;
}

static int sample_gizmo(
    SampleContext *context,
    uint32_t gizmo,
    unsigned int hero,
    SudekiMpTalosStagingResearchHeroEvidence *evidence
) {
    uint32_t primary;
    uint32_t secondary;
    uint32_t expected_primary;
    uint32_t expected_secondary;
    uint32_t setter_mode;
    uint32_t index;
    uint32_t state;
    uint32_t flags_a;
    uint32_t flags_b;
    uint32_t flags;
    uint32_t label_object;
    uint32_t control;
    uint32_t length;
    uint32_t source;
    uint32_t byte_count;
    uint8_t label[(MAX_LABEL_UNITS + 1u) * 2u];
    uint32_t unit;
    static const uint32_t expected_flags[3] = {0u, 5u, 2u};

    if (gizmo == 0u || evidence == NULL ||
        !image_address(context, RVA_GIZMO_PRIMARY_VTABLE,
            &expected_primary) ||
        !image_address(context, RVA_GIZMO_SECONDARY_VTABLE,
            &expected_secondary) ||
        !read_at_u32(context, gizmo, 0u, &primary) ||
        !read_at_u32(context, gizmo, 4u, &secondary) ||
        primary != expected_primary || secondary != expected_secondary ||
        !read_at_u32(context, gizmo, GIZMO_SETTER_MODE_OFFSET,
            &setter_mode) || setter_mode != 2u ||
        !read_at_u32(context, gizmo, GIZMO_INDEX_OFFSET, &index) ||
        index != hero ||
        !read_at_u32(context, gizmo, GIZMO_STATE_OFFSET, &state) ||
        state > 2u ||
        !read_at_u32(context, gizmo, GIZMO_FLAGS_A_OFFSET, &flags_a) ||
        !read_at_u32(context, gizmo, GIZMO_FLAGS_B_OFFSET, &flags_b)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_GIZMO, gizmo);
    }
    flags = (flags_a & 1u) | ((flags_b & 3u) << 1u);
    if (flags != expected_flags[state] ||
        !add_address(gizmo, GIZMO_LABEL_OFFSET, &label_object) ||
        !read_u32(context, label_object, &control)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_GIZMO, gizmo);
    }
    length = control & UINT32_C(0x7fffffff);
    if (length == 0u || length > MAX_LABEL_UNITS) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_LABEL,
            label_object);
    }
    if ((control & UINT32_C(0x80000000)) != 0u) {
        if (length > 27u ||
            !add_address(label_object, GIZMO_LABEL_DATA_OFFSET, &source)) {
            return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_LABEL,
                label_object);
        }
    } else {
        if (!read_at_u32(context, label_object, GIZMO_LABEL_DATA_OFFSET,
                &source) || source == 0u) {
            return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_LABEL,
                label_object);
        }
    }
    byte_count = (length + 1u) * 2u;
    if (!read_pair(context, source, label, byte_count)) return 0;
    for (unit = 0u; unit < length; ++unit) {
        if (label[unit * 2u] == 0u && label[unit * 2u + 1u] == 0u) {
            return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_LABEL,
                source + unit * 2u);
        }
    }
    if (label[length * 2u] != 0u || label[length * 2u + 1u] != 0u) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_LABEL,
            source + length * 2u);
    }
    evidence->gizmo_token = address_token(context, gizmo);
    evidence->gizmo_label_hash = fnv1a_utf16_bytes(label, length);
    evidence->gizmo_state = state;
    evidence->gizmo_flags_masked = flags;
    evidence->gizmo_label_length = length;
    return evidence->gizmo_label_hash != 0u;
}

static int finite_float_bits(uint32_t bits) {
    return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}

static float bits_to_float(uint32_t bits) {
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_to_bits(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t expected_fill_bits(uint32_t current_bits, uint32_t max_bits) {
    volatile float current = bits_to_float(current_bits);
    volatile float maximum = bits_to_float(max_bits);
    volatile float ratio = current / maximum;
    volatile float half;
    volatile float scaled;
    volatile float expected;
    int bucket;

    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    half = (float)(ratio * 0.5f);
    scaled = (float)(half * 100.0f);
    bucket = (int)scaled;
    expected = (float)((double)bucket * 0.009999999776482582);
    return float_to_bits(expected);
}

static uint32_t bytes_u32(const uint8_t *bytes) {
    uint32_t value;

    memcpy(&value, bytes, sizeof(value));
    return value;
}

static int sample_stat_display(
    SampleContext *context,
    uint32_t actor,
    SudekiMpTalosStagingResearchHeroEvidence *evidence,
    uint32_t *combat_data_result,
    uint32_t *display_result
) {
    uint32_t combat_data;
    uint32_t display = 0u;
    uint32_t primary;
    uint32_t secondary;
    uint32_t expected_primary;
    uint32_t expected_secondary;
    uint32_t current_bits;
    uint32_t maximum_bits;
    uint32_t last_bits;
    uint32_t bar;
    uint32_t renderers;
    uint32_t handles;
    uint32_t count;
    uint32_t fills;
    uint32_t expected_bits;
    uint32_t renderer;
    uint32_t renderer_vtable;
    uint32_t renderer_test;
    uint32_t renderer_apply;
    uint32_t last_hp_address;
    uint8_t handle_bytes[MAX_STAT_BAR_ENTRIES * 4u];
    uint8_t renderer_bytes[MAX_STAT_BAR_ENTRIES * 4u];
    uint8_t fill_bytes[MAX_STAT_BAR_ENTRIES * 4u];
    float current;
    float maximum;
    unsigned int renderer_index;

    if (!read_at_u32(context, actor, ACTOR_COMBAT_DATA_OFFSET,
            &combat_data) || combat_data == 0u ||
        !read_at_u32(context, combat_data, COMBAT_CURRENT_HP_OFFSET,
            &current_bits) ||
        !read_at_u32(context, combat_data, COMBAT_MAX_HP_OFFSET,
            &maximum_bits) ||
        !finite_float_bits(current_bits) || !finite_float_bits(maximum_bits)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY,
            actor);
    }
    current = bits_to_float(current_bits);
    maximum = bits_to_float(maximum_bits);
    if (current < 0.0f || maximum <= 0.0f ||
        !read_at_u32(context, actor, ACTOR_STAT_DISPLAY_OFFSET, &display) ||
        display == 0u ||
        !image_address(context, RVA_STAT_PRIMARY_VTABLE,
            &expected_primary) ||
        !image_address(context, RVA_STAT_SECONDARY_VTABLE,
            &expected_secondary) ||
        !read_at_u32(context, display, 0u, &primary) ||
        !read_at_u32(context, display, 4u, &secondary) ||
        primary != expected_primary || secondary != expected_secondary ||
        !read_at_u32(context, display, STAT_LAST_HP_OFFSET, &last_bits) ||
        last_bits != current_bits ||
        !add_address(display, STAT_HEALTH_BAR_OFFSET, &bar) ||
        !read_at_u32(context, bar, STAT_BAR_RENDERERS_OFFSET, &renderers) ||
        renderers == 0u ||
        !read_at_u32(context, bar, STAT_BAR_HANDLES_OFFSET, &handles) ||
        handles == 0u ||
        !read_at_u32(context, bar, STAT_BAR_COUNT_OFFSET, &count) ||
        count < 2u || count > MAX_STAT_BAR_ENTRIES ||
        !read_at_u32(context, bar, STAT_BAR_FILLS_OFFSET, &fills) ||
        fills == 0u || fills == handles || fills == renderers ||
        handles == renderers ||
        !read_pair(context, renderers, renderer_bytes, count * 4u) ||
        !read_pair(context, handles, handle_bytes, count * 4u) ||
        !read_pair(context, fills, fill_bytes, count * 4u)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY,
            display);
    }
    if (bytes_u32(handle_bytes) == UINT32_MAX ||
        bytes_u32(handle_bytes + 4u) == UINT32_MAX) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY,
            handles);
    }
    for (renderer_index = 0u; renderer_index < 2u; ++renderer_index) {
        renderer = bytes_u32(renderer_bytes + renderer_index * 4u);
        if (renderer == 0u ||
            !read_at_u32(context, renderer, 0u, &renderer_vtable) ||
            renderer_vtable == 0u ||
            !read_at_u32(context, renderer_vtable, 0x18u, &renderer_test) ||
            !read_at_u32(context, renderer_vtable, 0x38u, &renderer_apply) ||
            !address_in_loaded_image(context, renderer_test) ||
            !address_in_loaded_image(context, renderer_apply)) {
            return fail_at(context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY, renderer);
        }
    }
    expected_bits = expected_fill_bits(current_bits, maximum_bits);
    if (bytes_u32(fill_bytes) != expected_bits ||
        bytes_u32(fill_bytes + 4u) != expected_bits) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY,
            fills);
    }
    if (!add_address(display, STAT_LAST_HP_OFFSET, &last_hp_address) ||
        !require_writable_pair(context, last_hp_address, 4u) ||
        !require_writable_pair(context, handles, count * 4u) ||
        !require_writable_pair(context, fills, count * 4u)) {
        return fail_at(context, SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY,
            display);
    }
    evidence->stat_display_token = address_token(context, display);
    evidence->current_hp_bits = current_bits;
    evidence->fill_cache_primary_bits = bytes_u32(fill_bytes);
    evidence->fill_cache_secondary_bits = bytes_u32(fill_bytes + 4u);
    *combat_data_result = combat_data;
    *display_result = display;
    return 1;
}

static int tokens_distinct(
    const SudekiMpTalosStagingResearchSnapshot *snapshot
) {
    if (snapshot->controller_token == snapshot->controller_callback_token ||
        snapshot->controller_token == snapshot->transaction_token ||
        snapshot->controller_callback_token == snapshot->transaction_token) {
        return 0;
    }
    return 1;
}

int SudekiMpTalosCompanionStagingNativeSample(
    const SudekiMpTalosStagingNativeSamplerInput *input,
    const void *callback_controller,
    const void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const void *transaction_cookie,
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    SampleContext context;
    SudekiMpTalosStagingResearchSnapshot snapshot;
    SudekiMpControlUpdateDispatchWitness witness_copy;
    uint32_t callback_controller_address;
    uint32_t callback_update_data_address;
    uint32_t transaction_address;
    uint32_t world;
    uint32_t source;
    uint32_t transition_pending;
    uint32_t group;
    uint32_t group_count;
    uint32_t pending_a;
    uint32_t pending_b;
    uint8_t group_combat;
    uint32_t actor[HERO_COUNT];
    uint32_t group_slot[HERO_COUNT];
    uint32_t formation_slot[HERO_COUNT];
    uint32_t control[HERO_COUNT];
    uint32_t control_state[HERO_COUNT];
    uint32_t combat_data[HERO_COUNT];
    uint32_t gizmo[HERO_COUNT];
    uint32_t stat_display[HERO_COUNT];
    uint32_t ai_manager;
    uint32_t listener;
    uint32_t formation;
    uint32_t formation_count;
    uint32_t listener_vtable;
    uint32_t listener_add;
    uint32_t listener_remove;
    uint32_t expected;
    uint32_t listener_count;
    uint32_t listener_storage;
    uint32_t listener_entry;
    uint32_t controller;
    uint32_t controller_vtable;
    uint32_t controller_current_mode;
    uint32_t controller_requested_mode;
    uint32_t controller_target;
    uint32_t ui_controller;
    uint32_t hud_global;
    uint32_t hud;
    uint32_t ui_vtable;
    uint32_t ui_dispatch;
    uint32_t hud_vtable;
    uint32_t hud_dispatch;
    uint32_t ui_scene;
    uint32_t ui_scene_controller;
    uint32_t game_speed;
    uint32_t tsa;
    uint32_t async_pending_address;
    uint32_t async_stream_address;
    uint32_t async_pending;
    uint32_t async_stream;
    uint8_t tsa_playing;
    uint8_t paused;
    uint32_t arbiter;
    uint32_t arbiter_owner;
    uint32_t arbiter_attachment;
    uint32_t arbiter_state;
    uint32_t arbiter_flags;
    uint32_t camera_manager;
    uint32_t current_camera;
    uint32_t table_camera;
    uint32_t render_state;
    uint32_t scene_manager;
    uint32_t scene_renderer;
    uint32_t scene_camera_state;
    unsigned int camera_matches = 0u;
    unsigned int index;
    unsigned int other;
    static const uint8_t expected_group[HERO_COUNT] = {0u, 1u, 2u, 3u};
    static const uint8_t expected_formation[HERO_COUNT] = {0u, 3u, 1u, 2u};
    static const uint8_t default_camera[] = {
        'd','e','f','a','u','l','t','\0'
    };
    uint8_t camera_name[sizeof(default_camera)];
    uint8_t readable_probe;

    if (result == NULL) return 0;
    initialize_result(result);
    context.input = input;
    context.result = result;
    if (input == NULL || witness == NULL) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARGUMENT, 0u);
    }
    result->first_range_count = (uint32_t)input->first.range_count;
    result->second_range_count = (uint32_t)input->second.range_count;
    if (!range_layout_valid(&input->first) ||
        !range_layout_valid(&input->second) ||
        !capture_storage_disjoint(&input->first, &input->second)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT,
            0u);
    }
    result->first_capture_bytes = view_capture_bytes(&input->first);
    result->second_capture_bytes = view_capture_bytes(&input->second);
    if (input->loaded_image_base == 0u ||
        input->mapped_image_size != SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_SIZE ||
        input->loaded_image_base >
            UINT32_MAX - input->mapped_image_size ||
        input->observation_serial == 0u || input->process_token == 0u ||
        input->identity_salt == 0u ||
        !bool_exact(input->exact_executable_hash) ||
        !bool_exact(input->exact_sol_hash) ||
        !bool_exact(input->membership_abi_valid) ||
        !bool_exact(input->controller_abi_valid) ||
        !bool_exact(input->foreground) ||
        !bool_exact(input->witness_still_exact_after_capture) ||
        !bool_exact(input->transaction_lease_exclusive) ||
        !bool_exact(input->capture_no_yield_exact) ||
        !bool_exact(input->modal_active) ||
        !bool_exact(input->reload_required) ||
        !bool_exact(input->require_default_camera_name) ||
        input->reserved[0] != 0u) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARGUMENT, 0u);
    }
    if (input->exact_executable_hash != 1u || input->exact_sol_hash != 1u) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_FACT,
            input->loaded_image_base);
    }
    if (input->membership_abi_valid != 1u) {
        return fail_at(&context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_MEMBERSHIP_ABI_FACT,
            input->loaded_image_base);
    }
    if (input->controller_abi_valid != 1u) {
        return fail_at(&context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROLLER_ABI_FACT,
            input->loaded_image_base);
    }
    witness_copy = *witness;
    if (witness_copy.dispatch_serial == 0u ||
        witness_copy.native_thread_id == 0u ||
        witness_copy.outer_update_depth != 1u ||
        witness_copy.active_dispatch_count != 1u ||
        witness_copy.original_call_count != 1u ||
        witness_copy.observer_snapshot_count != 1u ||
        witness_copy.observer_registry_generation == 0u ||
        input->expected_observer_registry_generation == 0u ||
        witness_copy.observer_registry_generation !=
            input->expected_observer_registry_generation ||
        witness_copy.hook_owned_exact != 1u ||
        witness_copy.slot_owned_exact != 1u ||
        witness_copy.service_only != 1u ||
        witness_copy.post_original != 1u ||
        witness_copy.source !=
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL ||
        witness_copy.source_exact != 1u ||
        witness_copy.service_post_original_exact != 1u ||
        witness_copy.sole_observer != 1u ||
        witness_copy.registry_generation_stable != 1u ||
        witness_copy.reserved[0] != 0u || witness_copy.reserved[1] != 0u ||
        witness_copy.reserved[2] != 0u) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS, 0u);
    }
    result->witness_dispatch_serial = witness_copy.dispatch_serial;
    result->witness_native_thread_id = witness_copy.native_thread_id;
    result->witness_observer_registry_generation =
        witness_copy.observer_registry_generation;
    result->witness_dispatch_overlap_generation =
        witness_copy.dispatch_overlap_generation;
    result->witness_source = witness_copy.source;
    result->witness_entry_exact = 1u;
    if (input->witness_still_exact_after_capture != 1u) {
        return fail_at(&context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS_REVALIDATION, 0u);
    }
    result->witness_revalidated_exact = 1u;
    if (!borrowed_address(callback_controller,
            &callback_controller_address) ||
        !borrowed_address(callback_update_data, &callback_update_data_address) ||
        !borrowed_address(transaction_cookie, &transaction_address)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARGUMENT, 0u);
    }
    if (callback_controller_address == callback_update_data_address ||
        callback_controller_address == transaction_address ||
        callback_update_data_address == transaction_address) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ALIAS, 0u);
    }
    if (input->transaction_lease_exclusive != 1u ||
        input->capture_no_yield_exact != 1u) {
        return fail_at(&context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_TRANSACTION_WINDOW,
            transaction_address);
    }
    result->transaction_lease_exact = 1u;
    result->capture_no_yield_exact = 1u;
    if (!read_pair(&context, callback_update_data_address,
            &readable_probe, sizeof(readable_probe))) return 0;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.observation_serial = input->observation_serial;
    snapshot.process_token = input->process_token;
    snapshot.native_thread_token = integer_token(&context,
        witness_copy.native_thread_id, UINT64_C(0x7468726561640001));
    snapshot.controller_callback_token = integer_token(&context,
        witness_copy.dispatch_serial ^
            ((uint64_t)witness_copy.observer_registry_generation << 32u),
        UINT64_C(0x63616c6c6261636b));
    snapshot.transaction_token = address_token(&context, transaction_address);
    snapshot.exact_executable_hash = 1u;
    snapshot.exact_sol_hash = 1u;
    snapshot.foreground = input->foreground;
    snapshot.modal_active = input->modal_active;
    snapshot.reload_required = input->reload_required;
    snapshot.controller_callback_exact = 1u;
    snapshot.game_thread_exact = 1u;
    snapshot.transaction_exclusive = 1u;
    snapshot.no_yield_window_exact = 1u;
    snapshot.production_authority = 0u;
    snapshot.carry_authority = 0u;
    snapshot.actor_lifetime_authority = 0u;

    if (!read_global_pointer(&context, RVA_WORLD_GLOBAL, &world) ||
        !read_at_u32(&context, world, WORLD_SOURCE_OFFSET, &source) ||
        source == 0u ||
        !read_pair(&context, source, &readable_probe,
            sizeof(readable_probe)) ||
        !read_at_u32(&context, world, WORLD_TRANSITION_PENDING_OFFSET,
            &transition_pending)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_WORLD, world);
    }
    snapshot.world_token = address_token(&context, world);
    snapshot.source_token = address_token(&context, source);
    snapshot.transition_active = transition_pending != 0u &&
        transition_pending != source ? 1u : 0u;

    if (!read_global_pointer(&context, RVA_GROUP_GLOBAL, &group) ||
        !read_at_u32(&context, group, GROUP_COUNT_OFFSET, &group_count) ||
        group_count != HERO_COUNT ||
        !read_at_u32(&context, group, GROUP_PENDING_A_OFFSET, &pending_a) ||
        !read_at_u32(&context, group, GROUP_PENDING_B_OFFSET, &pending_b) ||
        !read_at_u8(&context, group, GROUP_COMBAT_OFFSET, &group_combat) ||
        group_combat > 1u) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP, group);
    }
    snapshot.group_token = address_token(&context, group);
    snapshot.group_count = (uint8_t)group_count;
    snapshot.all_pending_loaded = pending_a == 0u && pending_b == 0u ? 1u : 0u;
    snapshot.in_combat = group_combat;
    snapshot.group_armed = group_combat;
    for (index = 0u; index < HERO_COUNT; ++index) {
        uint8_t slot_record[TPTR_SIZE];
        uint8_t hero;

        if (!add_address(group,
                GROUP_MEMBERS_OFFSET + index * GROUP_MEMBER_STRIDE,
                &group_slot[index]) ||
            !read_pair(&context, group_slot[index], slot_record,
                sizeof(slot_record)) ||
            (actor[index] = bytes_u32(slot_record)) == 0u ||
            !actor_hero(&context, actor[index], &hero) || hero != index) {
            return fail_at(&context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_HERO_IDENTITY,
                group_slot[index]);
        }
        snapshot.group_order[index] = hero;
        snapshot.hero[index].actor_token =
            address_token(&context, actor[index]);
        for (other = 0u; other < index; ++other) {
            if (actor[other] == actor[index]) {
                return fail_at(&context,
                    SUDEKIMP_TALOS_NATIVE_SAMPLE_HERO_IDENTITY,
                    group_slot[index]);
            }
        }
    }
    if (memcmp(snapshot.group_order, expected_group,
            sizeof(expected_group)) != 0) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP, group);
    }
    snapshot.front_actor_token = snapshot.hero[0].actor_token;

    if (!read_global_pointer(&context, RVA_AI_MANAGER_GLOBAL, &ai_manager) ||
        !add_address(ai_manager, AI_LISTENER_OFFSET, &listener) ||
        !add_address(ai_manager, AI_FORMATION_OFFSET, &formation) ||
        !add_address(listener, LISTENER_TO_FORMATION_OFFSET, &expected) ||
        expected != formation ||
        !read_at_u32(&context, ai_manager, AI_FORMATION_COUNT_OFFSET,
            &formation_count) || formation_count != HERO_COUNT) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION,
            ai_manager);
    }
    snapshot.formation_owner_token = address_token(&context, ai_manager);
    snapshot.formation_token = address_token(&context, formation);
    snapshot.formation_count = (uint8_t)formation_count;
    for (index = 0u; index < HERO_COUNT; ++index) {
        uint8_t slot_record[TPTR_SIZE];

        if (!add_address(formation, index * GROUP_MEMBER_STRIDE,
                &formation_slot[index]) ||
            !read_pair(&context, formation_slot[index], slot_record,
                sizeof(slot_record)) ||
            (expected = bytes_u32(slot_record)) == 0u ||
            expected != actor[expected_formation[index]]) {
            return fail_at(&context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION,
                formation_slot[index]);
        }
        snapshot.formation_order[index] = expected_formation[index];
    }
    if (!sample_formation_mutation_closure(&context, formation)) return 0;
    if (!require_writable_at(&context, group, GROUP_MEMBERS_OFFSET,
            HERO_COUNT * TPTR_SIZE) ||
        !require_writable_at(&context, group, GROUP_COUNT_OFFSET, 4u) ||
        !require_writable_pair(&context, formation,
            HERO_COUNT * TPTR_SIZE) ||
        !require_writable_at(&context, ai_manager,
            AI_FORMATION_COUNT_OFFSET, 4u)) return 0;
    for (index = 0u; index < HERO_COUNT; ++index) {
        if (!sample_tptr_node(&context, group_slot[index], actor[index],
                SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP) ||
            !sample_tptr_node(&context, formation_slot[index],
                actor[expected_formation[index]],
                SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION)) return 0;
    }
    if (!image_address(&context, RVA_AI_LISTENER_VTABLE, &expected) ||
        !read_at_u32(&context, listener, 0u, &listener_vtable) ||
        listener_vtable != expected ||
        !read_at_u32(&context, listener_vtable, 0x18u, &listener_add) ||
        !image_address(&context, RVA_AI_LISTENER_ADD, &expected) ||
        listener_add != expected ||
        !read_at_u32(&context, listener_vtable, 0x1cu, &listener_remove) ||
        !image_address(&context, RVA_AI_LISTENER_REMOVE, &expected) ||
        listener_remove != expected ||
        !read_at_u32(&context, group, GROUP_LISTENER_COUNT_OFFSET,
            &listener_count) || listener_count != 1u ||
        !read_at_u32(&context, group, GROUP_LISTENER_STORAGE_OFFSET,
            &listener_storage) || listener_storage == 0u ||
        !read_u32(&context, listener_storage, &listener_entry) ||
        listener_entry != listener) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_LISTENER,
            listener);
    }
    if (!require_writable_at(&context, group,
            GROUP_LISTENER_COUNT_OFFSET, 4u) ||
        !require_writable_at(&context, group,
            GROUP_LISTENER_STORAGE_OFFSET, 4u) ||
        !require_writable_pair(&context, listener_storage, 4u)) return 0;
    snapshot.listener_storage_token =
        address_token(&context, listener_storage);
    snapshot.listener_token = address_token(&context, listener);
    snapshot.listener_count = listener_count;
    snapshot.listener_callback_closure_exact = 1u;

    if (!read_global_pointer(&context, RVA_CONTROLLER_GLOBAL, &controller) ||
        controller != callback_controller_address ||
        !read_at_u32(&context, controller, 0u, &controller_vtable) ||
        !image_address(&context, RVA_CONTROLLER_VTABLE, &expected) ||
        controller_vtable != expected ||
        !read_at_u32(&context, controller, CONTROLLER_CURRENT_MODE_OFFSET,
            &controller_current_mode) ||
        !read_at_u32(&context, controller, CONTROLLER_REQUESTED_MODE_OFFSET,
            &controller_requested_mode) ||
        !read_at_u32(&context, controller, CONTROLLER_TARGET_OFFSET,
            &controller_target) || controller_target != actor[0]) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROLLER,
            controller);
    }
    snapshot.controller_token = address_token(&context, controller);
    snapshot.controller_current_mode = controller_current_mode;
    snapshot.controller_requested_mode = controller_requested_mode;

    for (index = 0u; index < HERO_COUNT; ++index) {
        uint32_t owner;
        uint32_t backpointer;
        uint16_t override_count;
        uint8_t mode;
        uint8_t expected_mode = index == 0u ? 0u : 1u;

        if (!read_at_u32(&context, actor[index], ACTOR_CONTROL_OFFSET,
                &control[index]) || control[index] == 0u ||
            !read_at_u32(&context, control[index], CONTROL_OWNER_OFFSET,
                &owner) || owner != actor[index] ||
            !read_at_u32(&context, control[index], CONTROL_STATE_OFFSET,
                &control_state[index]) || control_state[index] == 0u ||
            !read_at_u8(&context, control_state[index],
                CONTROL_STATE_MODE_OFFSET, &mode) || mode != expected_mode ||
            !read_at_u32(&context, control[index], CONTROL_FORMATION_OFFSET,
                &backpointer) || backpointer != formation ||
            !read_at_u16(&context, control[index], CONTROL_OVERRIDE_OFFSET,
                &override_count) || override_count != 0u ||
            !require_writable_at(&context, control[index],
                CONTROL_FORMATION_OFFSET, 4u)) {
            return fail_at(&context,
                SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT,
                control[index]);
        }
        if (!sample_actor_formation_closure(&context, actor[index],
                control[index])) return 0;
        for (other = 0u; other < index; ++other) {
            if (control[other] == control[index] ||
                control_state[other] == control_state[index]) {
                return fail_at(&context,
                    SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT,
                    control[index]);
            }
        }
        snapshot.hero[index].control_component_token =
            address_token(&context, control[index]);
        snapshot.hero[index].control_owner_actor_token =
            address_token(&context, owner);
        snapshot.hero[index].formation_backpointer_token =
            address_token(&context, backpointer);
        snapshot.hero[index].native_ai_enabled = expected_mode;
        snapshot.hero[index].human_control_owned = index == 0u ? 1u : 0u;
        snapshot.hero[index].override_active = 0u;
        snapshot.hero[index].control_mode = index == 0u ?
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI;
    }

    if (!read_global_pointer(&context, RVA_UI_CONTROLLER_GLOBAL,
            &ui_controller) ||
        !read_global_pointer(&context, RVA_GAMEPLAY_HUD_GLOBAL, &hud_global) ||
        !read_at_u32(&context, ui_controller, 0u, &ui_vtable) ||
        !image_address(&context, RVA_UI_CONTROLLER_VTABLE, &expected) ||
        ui_vtable != expected ||
        !read_at_u32(&context, ui_vtable, 0x20u, &ui_dispatch) ||
        !image_address(&context, RVA_UI_CONTROLLER_DISPATCH, &expected) ||
        ui_dispatch != expected ||
        !read_at_u32(&context, ui_controller, UI_HUD_OFFSET, &hud) ||
        hud == 0u || hud != hud_global ||
        !read_at_u32(&context, hud, 0u, &hud_vtable) ||
        !image_address(&context, RVA_HUD_VTABLE, &expected) ||
        hud_vtable != expected ||
        !read_at_u32(&context, hud_vtable, 0x2cu, &hud_dispatch) ||
        !image_address(&context, RVA_HUD_DISPATCH, &expected) ||
        hud_dispatch != expected ||
        !read_global_pointer(&context, RVA_UI_SCENE_GLOBAL, &ui_scene) ||
        !read_at_u32(&context, ui_scene, UI_SCENE_CONTROLLER_OFFSET,
            &ui_scene_controller) || ui_scene_controller != controller) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD,
            ui_controller);
    }
    snapshot.ui_controller_token = address_token(&context, ui_controller);
    snapshot.hud_owner_token = address_token(&context, hud);
    snapshot.ui_scene_token = address_token(&context, ui_scene);

    for (index = 0u; index < HERO_COUNT; ++index) {
        if (!read_at_u32(&context, hud,
                HUD_GIZMO_ARRAY_OFFSET + index * 4u, &gizmo[index]) ||
            !sample_gizmo(&context, gizmo[index], index,
                &snapshot.hero[index]) ||
            !sample_stat_display(&context, actor[index],
                &snapshot.hero[index], &combat_data[index],
                &stat_display[index]) ||
            !sample_hud_resource_closure(&context, actor[index],
                control_state[index], combat_data[index], gizmo[index],
                &snapshot.hero[index]) ||
            !sample_stat_camera_sync_closure(&context,
                stat_display[index]) ||
            !sample_remove_epilogue_closure(&context, actor[index],
                combat_data[index], gizmo[index], ui_scene)) return 0;
        for (other = 0u; other < index; ++other) {
            if (gizmo[other] == gizmo[index] ||
                stat_display[other] == stat_display[index] ||
                combat_data[other] == combat_data[index]) {
                return fail_at(&context,
                    SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD, gizmo[index]);
            }
        }
    }
    snapshot.hero_hud_state_converged = 1u;
    snapshot.ui_hud_closure_exact = 1u;

    if (!read_at_u32(&context,
            actor[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO],
            ACTOR_ARBITER_OFFSET, &arbiter) || arbiter == 0u ||
        !read_at_u32(&context, arbiter, ARBITER_OWNER_OFFSET,
            &arbiter_owner) ||
        arbiter_owner !=
            actor[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO] ||
        !read_at_u32(&context, arbiter_owner, 0x80u,
            &arbiter_attachment) ||
        !read_at_u32(&context, arbiter, ARBITER_STATE_OFFSET,
            &arbiter_state) ||
        !read_at_u32(&context, arbiter, ARBITER_FLAGS_OFFSET,
            &arbiter_flags) ||
        !require_writable_at(&context, arbiter,
            ARBITER_FLAGS_50_OFFSET, 4u) ||
        !require_writable_at(&context, arbiter,
            ARBITER_STATE_OFFSET, 4u) ||
        !require_writable_at(&context, arbiter,
            ARBITER_FLAGS_OFFSET, 4u)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARBITER,
            arbiter);
    }
    if (arbiter_attachment != 0u &&
        !read_u32(&context, arbiter_attachment, &expected)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARBITER,
            arbiter_attachment);
    }
    snapshot.elco_arbiter_token = address_token(&context, arbiter);
    snapshot.elco_arbiter_state_58 = arbiter_state;
    snapshot.elco_arbiter_flags_60_masked = arbiter_flags &
        SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
    if (((snapshot.elco_arbiter_flags_60_masked != 0u) !=
            (group_combat != 0u)) ||
        (((arbiter_state & 0x0fu) == 2u) && group_combat == 0u) ||
        (((arbiter_state & 0x0fu) == 4u) && group_combat != 0u)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARBITER,
            arbiter);
    }
    snapshot.elco_arbiter_safe = 1u;

    if (!read_global_pointer(&context, RVA_CAMERA_MANAGER_GLOBAL,
            &camera_manager) ||
        !read_at_u32(&context, camera_manager, CAMERA_CURRENT_OFFSET,
            &current_camera) || current_camera == 0u) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_CAMERA,
            camera_manager);
    }
    for (index = 0u; index < CAMERA_TABLE_COUNT; ++index) {
        if (!read_at_u32(&context, camera_manager,
                CAMERA_TABLE_OFFSET + index * 4u, &table_camera)) return 0;
        if (table_camera == current_camera) ++camera_matches;
    }
    if (camera_matches != 1u ||
        !read_at_u32(&context, current_camera, CAMERA_RENDER_STATE_OFFSET,
            &render_state) || render_state == 0u ||
        !read_global_pointer(&context, RVA_SCENE_MANAGER_GLOBAL,
            &scene_manager) ||
        !read_at_u32(&context, scene_manager, SCENE_RENDERER_OFFSET,
            &scene_renderer) || scene_renderer == 0u ||
        !read_at_u32(&context, scene_renderer, SCENE_CAMERA_STATE_OFFSET,
            &scene_camera_state) || scene_camera_state != render_state) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_CAMERA,
            current_camera);
    }
    if (input->require_default_camera_name != 0u) {
        uint32_t name_address;

        if (!add_address(current_camera, CAMERA_NAME_OFFSET, &name_address) ||
            !read_pair(&context, name_address, camera_name,
                sizeof(camera_name)) ||
            memcmp(camera_name, default_camera, sizeof(default_camera)) != 0) {
            return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_CAMERA,
                current_camera);
        }
    }
    snapshot.camera_token = address_token(&context, camera_manager);
    snapshot.current_render_camera_token =
        address_token(&context, current_camera);
    snapshot.render_state_token = address_token(&context, render_state);
    snapshot.scene_manager_token = address_token(&context, scene_manager);
    snapshot.scene_renderer_token = address_token(&context, scene_renderer);
    snapshot.camera_scene_consistent = 1u;

    if (!read_global_pointer(&context, RVA_GAME_SPEED_GLOBAL, &game_speed) ||
        !read_at_u8(&context, game_speed, GAME_SPEED_PAUSED_OFFSET, &paused) ||
        paused > 1u ||
        !read_global_pointer(&context, RVA_TSA_GLOBAL, &tsa) ||
        !read_at_u8(&context, tsa, TSA_PLAYING_OFFSET, &tsa_playing) ||
        tsa_playing > 1u ||
        !image_address(&context, RVA_ASYNC_PENDING_GLOBAL,
            &async_pending_address) ||
        !image_address(&context, RVA_ASYNC_STREAM_GLOBAL,
            &async_stream_address) ||
        !read_u32(&context, async_pending_address, &async_pending) ||
        !read_u32(&context, async_stream_address, &async_stream)) {
        return fail_at(&context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_ORDINARY_DISCIPLINE, 0u);
    }
    snapshot.paused = paused;
    snapshot.tsa_active = tsa_playing;
    snapshot.async_active = async_pending != 0u || async_stream != 0u ? 1u : 0u;
    if (snapshot.foreground != 1u || snapshot.all_pending_loaded != 1u ||
        snapshot.in_combat != 0u || snapshot.async_active != 0u ||
        snapshot.tsa_active != 0u || snapshot.paused != 0u ||
        snapshot.reload_required != 0u ||
        snapshot.controller_current_mode != 1u ||
        snapshot.controller_requested_mode != 1u) {
        return fail_at(&context,
            SUDEKIMP_TALOS_NATIVE_SAMPLE_ORDINARY_DISCIPLINE, group);
    }

    if (!tokens_distinct(&snapshot)) {
        return fail_at(&context, SUDEKIMP_TALOS_NATIVE_SAMPLE_TOKEN_COLLISION,
            controller);
    }
    result->snapshot = snapshot;
    result->failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_OK;
    result->failed_address = 0u;
    result->valid = 1u;
    return 1;
}
