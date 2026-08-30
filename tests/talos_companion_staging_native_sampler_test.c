#include "hooks/control_separation.h"
#include "hooks/talos_companion_membership_abi.h"
#include "hooks/talos_companion_staging_native_sampler.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IMAGE_BASE = 0x00400000u,
    IMAGE_SIZE = 0x0045f000u,
    HEAP_BASE = 0x01000000u,
    HEAP_SIZE = 0x00060000u,
    WORLD = 0x01000100u,
    SOURCE = 0x01000400u,
    GROUP = 0x01000800u,
    LISTENER_STORAGE = 0x01000c00u,
    CONTROLLER = 0x01001000u,
    UPDATE_DATA = 0x01001400u,
    TRANSACTION_COOKIE = 0x01001500u,
    GAME_SPEED = 0x01001600u,
    TSA = 0x01001700u,
    AI_MANAGER = 0x01002000u,
    UI_CONTROLLER = 0x01002400u,
    HUD = 0x01002800u,
    UI_SCENE = 0x01002c00u,
    CAMERA_MANAGER = 0x01003000u,
    CURRENT_CAMERA = 0x01003400u,
    RENDER_STATE = 0x01003800u,
    SCENE_MANAGER = 0x01003c00u,
    SCENE_RENDERER = 0x01004000u,
    STAT_CAMERA_MANAGER = 0x01004400u,
    STAT_ACTIVE_CAMERA = 0x01004800u,
    STAT_CAMERA_PAYLOAD = 0x01004c00u,
    RESOURCE_TABLE = 0x01005000u,
    RESOURCE_TABLE_DATA = 0x01005800u,
    ACTOR_BASE = 0x01008000u,
    CONTROL_BASE = 0x0100a000u,
    CONTROL_STATE_BASE = 0x0100c000u,
    RESOURCE_ENTRIES_BASE = 0x0100e000u,
    RESOURCE_ENTRY_BASE = 0x0100f000u,
    GIZMO_BASE = 0x01010000u,
    COMBAT_BASE = 0x01012000u,
    STAT_DISPLAY_BASE = 0x01013000u,
    STAT_ARRAY_BASE = 0x01015000u,
    STAT_RENDERER_BASE = 0x01016000u,
    STAT_NODE_BASE = 0x01018000u,
    ELCO_ARBITER = 0x0101a000u,
    STATUS_OWNER_BASE = 0x0101b000u,
    STATUS_TARGET_BASE = 0x0101c000u,
    RESOURCE_STRING_BASE = 0x01020000u,
    FORMATION_COMPONENT_BASE = 0x01024000u,
    EXTERNAL_TPTR_HEAD = 0x01025000u,
    HERO_COUNT = 4u,
    MAX_TEST_RANGES = 6u,
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
    RVA_HUD_GLOBAL = 0x003c2f9cu,
    RVA_STAT_CAMERA_MANAGER_GLOBAL = 0x003c2f30u,
    RVA_RESOURCE_TABLE_GLOBAL = 0x003c305cu,
    RVA_RESOURCE_INITIALIZED = 0x00409e09u,
    RVA_STAT_CAMERA_INITIALIZED = 0x00409e08u,
    RVA_CONTROLLER_VTABLE = 0x002c9f5cu,
    RVA_UI_VTABLE = 0x002caf9cu,
    RVA_UI_DISPATCH = 0x0009d9b0u,
    RVA_HUD_VTABLE = 0x002cb3e4u,
    RVA_HUD_DISPATCH = 0x000a5930u,
    RVA_LISTENER_VTABLE = 0x002ca244u,
    RVA_LISTENER_ADD = 0x000f2b00u,
    RVA_LISTENER_REMOVE = 0x000f2b30u,
    RVA_GIZMO_PRIMARY = 0x002cb590u,
    RVA_GIZMO_SECONDARY = 0x002cb59cu,
    RVA_STAT_PRIMARY = 0x002d21e4u,
    RVA_STAT_SECONDARY = 0x002d2224u
};

typedef struct Fixture {
    uint8_t *image[2];
    uint8_t *heap[2];
    SudekiMpTalosStagingNativeReadableRange ranges[2][MAX_TEST_RANGES];
    SudekiMpTalosStagingNativeSamplerInput input;
    SudekiMpControlUpdateDispatchWitness witness;
} Fixture;

static int failures;
static unsigned int still_exact_calls;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)witness;
    ++still_exact_calls;
    return 0;
}

static uint32_t actor_address(unsigned int hero) {
    return ACTOR_BASE + hero * 0x400u;
}

static uint32_t control_address(unsigned int hero) {
    return CONTROL_BASE + hero * 0x400u;
}

static uint32_t control_state_address(unsigned int hero) {
    return CONTROL_STATE_BASE + hero * 0x400u;
}

static uint32_t gizmo_address(unsigned int hero) {
    return GIZMO_BASE + hero * 0x400u;
}

static uint32_t combat_address(unsigned int hero) {
    return COMBAT_BASE + hero * 0x100u;
}

static uint32_t stat_display_address(unsigned int hero) {
    return STAT_DISPLAY_BASE + hero * 0x400u;
}

static uint8_t *fixture_bytes(
    Fixture *fixture,
    unsigned int capture,
    uint32_t address,
    size_t size
) {
    uint64_t end = (uint64_t)address + (uint64_t)size;

    if (capture > 1u || end > UINT64_C(0x100000000)) return NULL;
    if (address >= IMAGE_BASE &&
        end <= (uint64_t)IMAGE_BASE + IMAGE_SIZE) {
        return fixture->image[capture] + (address - IMAGE_BASE);
    }
    if (address >= HEAP_BASE && end <= (uint64_t)HEAP_BASE + HEAP_SIZE) {
        return fixture->heap[capture] + (address - HEAP_BASE);
    }
    return NULL;
}

static void put_bytes_first(
    Fixture *fixture,
    uint32_t address,
    const void *value,
    size_t size
) {
    uint8_t *destination = fixture_bytes(fixture, 0u, address, size);

    CHECK(destination != NULL);
    if (destination != NULL) memcpy(destination, value, size);
}

static void put_u8_first(Fixture *fixture, uint32_t address, uint8_t value) {
    put_bytes_first(fixture, address, &value, sizeof(value));
}

static void put_u16_first(
    Fixture *fixture,
    uint32_t address,
    uint16_t value
) {
    put_bytes_first(fixture, address, &value, sizeof(value));
}

static void put_u32_first(
    Fixture *fixture,
    uint32_t address,
    uint32_t value
) {
    put_bytes_first(fixture, address, &value, sizeof(value));
}

static uint32_t float_bits(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t expected_fill_bits(uint32_t current_bits, uint32_t max_bits) {
    float current;
    float maximum;
    volatile float ratio;
    volatile float half;
    volatile float scaled;
    volatile float expected;
    int bucket;

    memcpy(&current, &current_bits, sizeof(current));
    memcpy(&maximum, &max_bits, sizeof(maximum));
    ratio = current / maximum;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    half = (float)(ratio * 0.5f);
    scaled = (float)(half * 100.0f);
    bucket = (int)scaled;
    expected = (float)((double)bucket * 0.009999999776482582);
    return float_bits(expected);
}

static void put_utf16_inline_first(
    Fixture *fixture,
    uint32_t object,
    const char *text
) {
    size_t length = strlen(text);
    size_t index;

    CHECK(length > 0u && length <= 27u);
    put_u32_first(fixture, object,
        UINT32_C(0x80000000) | (uint32_t)length);
    for (index = 0u; index < length; ++index) {
        put_u16_first(fixture, object + 4u + (uint32_t)index * 2u,
            (uint16_t)(uint8_t)text[index]);
    }
    put_u16_first(fixture, object + 4u + (uint32_t)length * 2u, 0u);
}

static void sync_second(Fixture *fixture) {
    memcpy(fixture->image[1], fixture->image[0], IMAGE_SIZE);
    memcpy(fixture->heap[1], fixture->heap[0], HEAP_SIZE);
}

static void initialize_range(
    SudekiMpTalosStagingNativeReadableRange *range,
    uint32_t address,
    uint8_t *bytes,
    uint32_t size,
    uint8_t writable
) {
    memset(range, 0, sizeof(*range));
    range->address = address;
    range->bytes = bytes;
    range->size = size;
    range->native_readable = 1u;
    range->native_writable = writable;
}

static int fixture_initialize(Fixture *fixture) {
    static const uint32_t actor_vtables[HERO_COUNT][3] = {
        {0x002d5010u, 0x002d5034u, 0x002d5054u},
        {0x002d555cu, 0x002d5580u, 0x002d55a0u},
        {0x002d5a88u, 0x002d5aacu, 0x002d5accu},
        {0x002d66fcu, 0x002d6720u, 0x002d6740u}
    };
    static const unsigned int formation_order[HERO_COUNT] = {0u, 3u, 1u, 2u};
    static const unsigned int formation_position[HERO_COUNT] = {0u, 2u, 3u, 1u};
    static const char *const labels[HERO_COUNT] = {
        "Tal", "Ailish", "Buki", "Elco"
    };
    uint32_t current_bits = float_bits(100.0f);
    uint32_t maximum_bits = float_bits(100.0f);
    uint32_t fill_bits = expected_fill_bits(current_bits, maximum_bits);
    unsigned int hero;
    unsigned int index;

    memset(fixture, 0, sizeof(*fixture));
    fixture->image[0] = (uint8_t *)calloc(1u, IMAGE_SIZE);
    fixture->image[1] = (uint8_t *)calloc(1u, IMAGE_SIZE);
    fixture->heap[0] = (uint8_t *)calloc(1u, HEAP_SIZE);
    fixture->heap[1] = (uint8_t *)calloc(1u, HEAP_SIZE);
    if (fixture->image[0] == NULL || fixture->image[1] == NULL ||
        fixture->heap[0] == NULL || fixture->heap[1] == NULL) return 0;

    put_u32_first(fixture, IMAGE_BASE + RVA_WORLD_GLOBAL, WORLD);
    put_u32_first(fixture, IMAGE_BASE + RVA_UI_SCENE_GLOBAL, UI_SCENE);
    put_u32_first(fixture, IMAGE_BASE + RVA_TSA_GLOBAL, TSA);
    put_u32_first(fixture, IMAGE_BASE + RVA_SCENE_MANAGER_GLOBAL,
        SCENE_MANAGER);
    put_u32_first(fixture, IMAGE_BASE + RVA_GROUP_GLOBAL, GROUP);
    put_u32_first(fixture, IMAGE_BASE + RVA_GAME_SPEED_GLOBAL, GAME_SPEED);
    put_u32_first(fixture, IMAGE_BASE + RVA_CONTROLLER_GLOBAL, CONTROLLER);
    put_u32_first(fixture, IMAGE_BASE + RVA_CAMERA_MANAGER_GLOBAL,
        CAMERA_MANAGER);
    put_u32_first(fixture, IMAGE_BASE + RVA_AI_MANAGER_GLOBAL, AI_MANAGER);
    put_u32_first(fixture, IMAGE_BASE + RVA_UI_CONTROLLER_GLOBAL,
        UI_CONTROLLER);
    put_u32_first(fixture, IMAGE_BASE + RVA_HUD_GLOBAL, HUD);
    put_u32_first(fixture, IMAGE_BASE + RVA_STAT_CAMERA_MANAGER_GLOBAL,
        STAT_CAMERA_MANAGER);
    put_u32_first(fixture, IMAGE_BASE + RVA_RESOURCE_TABLE_GLOBAL,
        RESOURCE_TABLE);
    put_u32_first(fixture, IMAGE_BASE + RVA_ASYNC_STREAM_GLOBAL, 0u);
    put_u32_first(fixture, IMAGE_BASE + RVA_ASYNC_PENDING_GLOBAL, 0u);
    put_u8_first(fixture, IMAGE_BASE + RVA_RESOURCE_INITIALIZED, 0u);
    put_u8_first(fixture, IMAGE_BASE + RVA_STAT_CAMERA_INITIALIZED, 1u);

    put_u32_first(fixture, IMAGE_BASE + RVA_LISTENER_VTABLE + 0x18u,
        IMAGE_BASE + RVA_LISTENER_ADD);
    put_u32_first(fixture, IMAGE_BASE + RVA_LISTENER_VTABLE + 0x1cu,
        IMAGE_BASE + RVA_LISTENER_REMOVE);
    put_u32_first(fixture, IMAGE_BASE + RVA_UI_VTABLE + 0x20u,
        IMAGE_BASE + RVA_UI_DISPATCH);
    put_u32_first(fixture, IMAGE_BASE + RVA_HUD_VTABLE + 0x2cu,
        IMAGE_BASE + RVA_HUD_DISPATCH);

    put_u32_first(fixture, WORLD + 0x0cu, SOURCE);
    put_u32_first(fixture, WORLD + 0x10u, SOURCE);
    put_u8_first(fixture, SOURCE, 0x5au);
    put_u32_first(fixture, GROUP + 0x38u, 1u);
    put_u32_first(fixture, GROUP + 0x40u, LISTENER_STORAGE);
    put_u32_first(fixture, GROUP + 0x58u, 0u);
    put_u32_first(fixture, GROUP + 0x5cu, 0u);
    put_u32_first(fixture, GROUP + 0xccu, HERO_COUNT);
    put_u8_first(fixture, GROUP + 0xd4u, 0u);
    put_u32_first(fixture, LISTENER_STORAGE, AI_MANAGER + 0x44u);

    put_u32_first(fixture, AI_MANAGER + 0x44u,
        IMAGE_BASE + RVA_LISTENER_VTABLE);
    put_u32_first(fixture, AI_MANAGER + 0x124u, HERO_COUNT);
    put_u32_first(fixture, AI_MANAGER + 0xf4u + 0x34u,
        float_bits(0.0f));
    put_u32_first(fixture, AI_MANAGER + 0xf4u + 0x38u,
        float_bits(1.0f));
    put_u32_first(fixture, AI_MANAGER + 0xf4u + 0x3cu,
        float_bits(2.0f));
    put_u32_first(fixture, AI_MANAGER + 0xf4u + 0x40u,
        float_bits(3.0f));
    put_u8_first(fixture, AI_MANAGER + 0xf4u + 0x50u, 0u);

    put_u32_first(fixture, CONTROLLER, IMAGE_BASE + RVA_CONTROLLER_VTABLE);
    put_u32_first(fixture, CONTROLLER + 0x80u, 1u);
    put_u32_first(fixture, CONTROLLER + 0x84u, 1u);
    put_u32_first(fixture, CONTROLLER + 0x248u, actor_address(0u));
    put_u8_first(fixture, UPDATE_DATA, 0x3cu);
    put_u8_first(fixture, GAME_SPEED + 0x28u, 0u);
    put_u8_first(fixture, TSA + 0x74u, 0u);

    put_u32_first(fixture, UI_CONTROLLER, IMAGE_BASE + RVA_UI_VTABLE);
    put_u32_first(fixture, UI_CONTROLLER + 0x6cu, HUD);
    put_u32_first(fixture, HUD, IMAGE_BASE + RVA_HUD_VTABLE);
    put_u32_first(fixture, UI_SCENE + 0x170u, CONTROLLER);
    put_u32_first(fixture, UI_SCENE + 0x148u, float_bits(0.25f));

    put_u32_first(fixture, CAMERA_MANAGER + 0x20u, CURRENT_CAMERA);
    put_u32_first(fixture, CAMERA_MANAGER + 0x24u, CURRENT_CAMERA);
    put_u32_first(fixture, CURRENT_CAMERA + 0x34u, RENDER_STATE);
    put_bytes_first(fixture, CURRENT_CAMERA + 0x4cu, "default", 8u);
    put_u32_first(fixture, SCENE_MANAGER + 0x40u, SCENE_RENDERER);
    put_u32_first(fixture, SCENE_RENDERER + 0x7cu, RENDER_STATE);

    put_u32_first(fixture, STAT_CAMERA_MANAGER + 0x20u,
        STAT_ACTIVE_CAMERA);
    put_u32_first(fixture, STAT_ACTIVE_CAMERA + 0x34u,
        STAT_CAMERA_PAYLOAD);
    put_u32_first(fixture, STAT_CAMERA_PAYLOAD + 0xc0u, float_bits(1.0f));
    put_u32_first(fixture, STAT_CAMERA_PAYLOAD + 0xc4u, float_bits(2.0f));
    put_u32_first(fixture, STAT_CAMERA_PAYLOAD + 0xc8u, float_bits(3.0f));

    put_u32_first(fixture, RESOURCE_TABLE + 0x08u, 0x200u);
    put_u32_first(fixture, RESOURCE_TABLE + 0x10u, RESOURCE_TABLE_DATA);

    for (hero = 0u; hero < HERO_COUNT; ++hero) {
        uint32_t actor = actor_address(hero);
        uint32_t control = control_address(hero);
        uint32_t state = control_state_address(hero);
        uint32_t group_slot = GROUP + 0x90u + hero * 0x0cu;
        uint32_t formation_slot = AI_MANAGER + 0xf4u +
            formation_position[hero] * 0x0cu;
        uint32_t entries = RESOURCE_ENTRIES_BASE + hero * 0x40u;
        uint32_t entry = RESOURCE_ENTRY_BASE + hero * 0x40u;
        uint32_t source_object = RESOURCE_STRING_BASE + hero * 0x100u;
        uint32_t gizmo = gizmo_address(hero);
        uint32_t combat = combat_address(hero);
        uint32_t display = stat_display_address(hero);
        uint32_t renderer_array = STAT_ARRAY_BASE + hero * 0x100u;
        uint32_t handle_array = renderer_array + 0x20u;
        uint32_t fill_array = renderer_array + 0x40u;
        uint32_t display_node = STAT_NODE_BASE + hero * 0x400u;
        uint32_t owner_node = display_node + 0x100u;
        uint32_t owner = display_node + 0x200u;
        uint32_t status_owner = STATUS_OWNER_BASE + hero * 0x100u;
        uint32_t formation_component =
            FORMATION_COMPONENT_BASE + hero * 0x100u;

        put_u32_first(fixture, actor, IMAGE_BASE + actor_vtables[hero][0]);
        put_u32_first(fixture, actor + 4u, group_slot);
        put_u32_first(fixture, actor + 8u,
            IMAGE_BASE + actor_vtables[hero][1]);
        put_u32_first(fixture, actor + 0x2cu,
            IMAGE_BASE + actor_vtables[hero][2]);
        put_u32_first(fixture, actor + 0x44u, formation_component);
        put_u32_first(fixture, actor + 0x4cu, combat);
        put_u32_first(fixture, actor + 0x94u, control);
        put_u32_first(fixture, actor + 0xa8u, status_owner);
        put_u32_first(fixture, actor + 0xb0u, display);
        put_u32_first(fixture, actor + 0x80u, 0u);

        put_u32_first(fixture, group_slot, actor);
        put_u32_first(fixture, group_slot + 4u, 0u);
        put_u32_first(fixture, group_slot + 8u, formation_slot);
        put_u32_first(fixture, formation_slot, actor);
        put_u32_first(fixture, formation_slot + 4u, group_slot);
        put_u32_first(fixture, formation_slot + 8u, 0u);

        put_u32_first(fixture, control + 0x10u, actor);
        put_u32_first(fixture, control + 0x3cu, state);
        put_u32_first(fixture, control + 0x40u, AI_MANAGER + 0xf4u);
        put_u32_first(fixture, control + 0x150u, float_bits(1.0f));
        put_u16_first(fixture, control + 0x16au, 0u);
        put_u32_first(fixture, formation_component + 0x18u,
            float_bits(2.0f));
        put_u32_first(fixture, formation_component + 0x1cu,
            float_bits(2.5f));
        put_u32_first(fixture, formation_component + 0x20u,
            float_bits(3.0f));
        put_u32_first(fixture, state + 0x04u, entries);
        put_u8_first(fixture, state + 0x09u, 1u);
        put_u8_first(fixture, state + 0x0bu, hero == 0u ? 0u : 1u);
        put_u32_first(fixture, entries, entry);
        put_u8_first(fixture, entry + 0x19u, (uint8_t)(hero + 1u));
        put_u32_first(fixture, RESOURCE_TABLE_DATA + 0x4d8u + hero * 4u,
            source_object);
        put_utf16_inline_first(fixture, source_object, labels[hero]);

        put_u32_first(fixture, HUD + 0x138u + hero * 4u, gizmo);
        put_u32_first(fixture, gizmo, IMAGE_BASE + RVA_GIZMO_PRIMARY);
        put_u32_first(fixture, gizmo + 4u,
            IMAGE_BASE + RVA_GIZMO_SECONDARY);
        put_u32_first(fixture, gizmo + 0x28u, 2u);
        put_u32_first(fixture, gizmo + 0x2a4u, 0u);
        put_u32_first(fixture, gizmo + 0x2acu, 0u);
        put_utf16_inline_first(fixture, gizmo + 0x2e0u, labels[hero]);
        put_u32_first(fixture, gizmo + 0x324u, 0u);
        put_u32_first(fixture, gizmo + 0x32cu, hero);

        put_u32_first(fixture, combat + 0x2cu, current_bits);
        put_u32_first(fixture, combat + 0x30u, maximum_bits);
        put_u32_first(fixture, combat + 0x34u, float_bits(0.0f));
        put_u32_first(fixture, combat + 0x40u, float_bits(0.0f));

        put_u32_first(fixture, display, IMAGE_BASE + RVA_STAT_PRIMARY);
        put_u32_first(fixture, display + 4u,
            IMAGE_BASE + RVA_STAT_SECONDARY);
        put_u32_first(fixture, display + 0x58u, display_node);
        put_u32_first(fixture, display + 0xccu, owner);
        put_u32_first(fixture, owner + 8u, owner_node);
        put_u32_first(fixture, display + 0x16cu, current_bits);
        put_u32_first(fixture, display + 0xd0u + 0x34u,
            renderer_array);
        put_u32_first(fixture, display + 0xd0u + 0x4cu, handle_array);
        put_u32_first(fixture, display + 0xd0u + 0x50u, 2u);
        put_u32_first(fixture, display + 0xd0u + 0x58u, fill_array);
        put_u32_first(fixture, handle_array, 10u + hero * 2u);
        put_u32_first(fixture, handle_array + 4u, 11u + hero * 2u);
        put_u32_first(fixture, fill_array, fill_bits);
        put_u32_first(fixture, fill_array + 4u, fill_bits);
        for (index = 0u; index < 2u; ++index) {
            uint32_t renderer = STAT_RENDERER_BASE + hero * 0x400u +
                index * 0x100u;
            uint32_t vtable = IMAGE_BASE + 0x002e0000u +
                hero * 0x100u + index * 0x40u;

            put_u32_first(fixture, renderer_array + index * 4u, renderer);
            put_u32_first(fixture, renderer, vtable);
            put_u32_first(fixture, vtable + 0x18u,
                IMAGE_BASE + 0x00012000u + index * 0x20u);
            put_u32_first(fixture, vtable + 0x38u,
                IMAGE_BASE + 0x00012100u + index * 0x20u);
        }

        for (index = 0u; index < 5u; ++index) {
            static const uint32_t status_offsets[5] = {
                0x40u, 0x44u, 0x50u, 0x54u, 0x58u
            };
            uint32_t target = STATUS_TARGET_BASE + hero * 0x800u +
                index * 0x100u;

            put_u32_first(fixture, status_owner + status_offsets[index],
                target);
            put_u8_first(fixture, target + 0x4cu,
                (uint8_t)(hero * 10u + index));
        }
    }

    for (index = 0u; index < HERO_COUNT; ++index) {
        uint32_t formation_slot = AI_MANAGER + 0xf4u + index * 0x0cu;
        uint32_t observed;

        memcpy(&observed, fixture_bytes(fixture, 0u, formation_slot, 4u),
            sizeof(observed));
        CHECK(observed == actor_address(formation_order[index]));
    }

    put_u32_first(fixture, actor_address(3u) + 0x90u, ELCO_ARBITER);
    put_u32_first(fixture, ELCO_ARBITER + 0x10u, actor_address(3u));
    put_u32_first(fixture, ELCO_ARBITER + 0x50u, 0u);
    put_u32_first(fixture, ELCO_ARBITER + 0x58u, 0u);
    put_u32_first(fixture, ELCO_ARBITER + 0x60u, 0u);

    sync_second(fixture);
    initialize_range(&fixture->ranges[0][0], IMAGE_BASE,
        fixture->image[0], IMAGE_SIZE, 1u);
    initialize_range(&fixture->ranges[0][1], HEAP_BASE,
        fixture->heap[0], HEAP_SIZE, 1u);
    initialize_range(&fixture->ranges[1][0], IMAGE_BASE,
        fixture->image[1], IMAGE_SIZE, 1u);
    initialize_range(&fixture->ranges[1][1], HEAP_BASE,
        fixture->heap[1], HEAP_SIZE, 1u);

    memset(&fixture->input, 0, sizeof(fixture->input));
    fixture->input.first.ranges = fixture->ranges[0];
    fixture->input.first.range_count = 2u;
    fixture->input.second.ranges = fixture->ranges[1];
    fixture->input.second.range_count = 2u;
    fixture->input.observation_serial = UINT64_C(101);
    fixture->input.process_token = UINT64_C(202);
    fixture->input.identity_salt = UINT64_C(0x123456789abcdef0);
    fixture->input.loaded_image_base = IMAGE_BASE;
    fixture->input.mapped_image_size = IMAGE_SIZE;
    fixture->input.expected_observer_registry_generation = 7u;
    fixture->input.exact_executable_hash = 1u;
    fixture->input.exact_sol_hash = 1u;
    fixture->input.membership_abi_valid = 1u;
    fixture->input.controller_abi_valid = 1u;
    fixture->input.foreground = 1u;
    fixture->input.witness_still_exact_after_capture = 1u;
    fixture->input.transaction_lease_exclusive = 1u;
    fixture->input.capture_no_yield_exact = 1u;
    fixture->input.require_default_camera_name = 1u;

    memset(&fixture->witness, 0, sizeof(fixture->witness));
    fixture->witness.dispatch_serial = UINT64_C(303);
    fixture->witness.native_thread_id = 77u;
    fixture->witness.outer_update_depth = 1u;
    fixture->witness.active_dispatch_count = 1u;
    fixture->witness.original_call_count = 1u;
    fixture->witness.observer_snapshot_count = 1u;
    fixture->witness.observer_registry_generation = 7u;
    fixture->witness.dispatch_overlap_generation = 0u;
    fixture->witness.hook_owned_exact = 1u;
    fixture->witness.slot_owned_exact = 1u;
    fixture->witness.service_only = 1u;
    fixture->witness.post_original = 1u;
    fixture->witness.source =
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL;
    fixture->witness.source_exact = 1u;
    fixture->witness.service_post_original_exact = 1u;
    fixture->witness.sole_observer = 1u;
    fixture->witness.registry_generation_stable = 1u;
    return 1;
}

static void fixture_destroy(Fixture *fixture) {
    free(fixture->image[0]);
    free(fixture->image[1]);
    free(fixture->heap[0]);
    free(fixture->heap[1]);
    memset(fixture, 0, sizeof(*fixture));
}

static int run_sample(
    Fixture *fixture,
    SudekiMpTalosStagingNativeSamplerResult *result
) {
    return SudekiMpTalosCompanionStagingNativeSample(
        &fixture->input,
        (const void *)(uintptr_t)CONTROLLER,
        (const void *)(uintptr_t)UPDATE_DATA,
        &fixture->witness,
        (const void *)(uintptr_t)TRANSACTION_COOKIE,
        result);
}

static void patch_u32(
    Fixture *fixture,
    unsigned int capture,
    uint32_t address,
    uint32_t value
) {
    uint8_t *bytes = fixture_bytes(fixture, capture, address, sizeof(value));

    CHECK(bytes != NULL);
    if (bytes != NULL) memcpy(bytes, &value, sizeof(value));
}

static void patch_u32_both(
    Fixture *fixture,
    uint32_t address,
    uint32_t value
) {
    patch_u32(fixture, 0u, address, value);
    patch_u32(fixture, 1u, address, value);
}

static void patch_u8_both(
    Fixture *fixture,
    uint32_t address,
    uint8_t value
) {
    uint8_t *first = fixture_bytes(fixture, 0u, address, 1u);
    uint8_t *second = fixture_bytes(fixture, 1u, address, 1u);

    CHECK(first != NULL && second != NULL);
    if (first != NULL && second != NULL) {
        *first = value;
        *second = value;
    }
}

static void use_external_tptr_head(
    Fixture *fixture,
    unsigned int hero
) {
    uint32_t actor = actor_address(hero);
    uint32_t group_slot = GROUP + 0x90u + hero * 0x0cu;

    CHECK(hero == 0u);
    patch_u32_both(fixture, actor + 4u, EXTERNAL_TPTR_HEAD);
    patch_u32_both(fixture, EXTERNAL_TPTR_HEAD, actor);
    patch_u32_both(fixture, EXTERNAL_TPTR_HEAD + 4u, 0u);
    patch_u32_both(fixture, EXTERNAL_TPTR_HEAD + 8u, group_slot);
    patch_u32_both(fixture, group_slot + 4u, EXTERNAL_TPTR_HEAD);
}

static void split_heap_range(
    Fixture *fixture,
    uint32_t address,
    uint32_t size,
    uint8_t include,
    uint8_t writable
) {
    unsigned int capture;

    CHECK(address >= HEAP_BASE &&
        (uint64_t)address + size <= (uint64_t)HEAP_BASE + HEAP_SIZE);
    for (capture = 0u; capture < 2u; ++capture) {
        size_t count = 1u;
        uint32_t before = address - HEAP_BASE;
        uint32_t after_address = address + size;
        uint32_t after = HEAP_BASE + HEAP_SIZE - after_address;

        initialize_range(&fixture->ranges[capture][0], IMAGE_BASE,
            fixture->image[capture], IMAGE_SIZE, 1u);
        if (before != 0u) {
            initialize_range(&fixture->ranges[capture][count], HEAP_BASE,
                fixture->heap[capture], before, 1u);
            ++count;
        }
        if (include != 0u) {
            initialize_range(&fixture->ranges[capture][count], address,
                fixture->heap[capture] + before, size, writable);
            ++count;
        }
        if (after != 0u) {
            initialize_range(&fixture->ranges[capture][count], after_address,
                fixture->heap[capture] + before + size, after, 1u);
            ++count;
        }
        if (capture == 0u) {
            fixture->input.first.range_count = count;
        } else {
            fixture->input.second.range_count = count;
        }
    }
}

static void expect_failure(
    Fixture *fixture,
    uint32_t failure
) {
    SudekiMpTalosStagingNativeSamplerResult result;

    CHECK(run_sample(fixture, &result) == 0);
    CHECK(result.valid == 0u);
    if (result.failure != failure) {
        fprintf(stderr,
            "failure mismatch expected=%u actual=%u address=0x%08x\n",
            (unsigned int)failure, (unsigned int)result.failure,
            (unsigned int)result.failed_address);
    }
    CHECK(result.failure == failure);
    CHECK(result.snapshot.production_authority == 0u);
    CHECK(result.native_engine_calls_permitted == 0u);
    CHECK(result.hooks_permitted == 0u);
    CHECK(result.actor_lifetime_authority == 0u);
    CHECK(result.mutation_authority == 0u);
}

static void test_descriptor_cross_check(void) {
    SudekiMpTalosMembershipAbiDescriptor descriptor =
        SudekiMpTalosCompanionMembershipAbiDescribe();

    CHECK(descriptor.abi_version == 3u);
    CHECK(descriptor.preferred_image_base == IMAGE_BASE);
    CHECK(descriptor.mapped_image_size == IMAGE_SIZE);
    CHECK(descriptor.native_capacity == HERO_COUNT);
    CHECK(descriptor.embedded_tptr_size == 0x0cu);
    CHECK(descriptor.group_members_offset == 0x90u);
    CHECK(descriptor.group_count_offset == 0xccu);
    CHECK(descriptor.formation_members_offset == 0xf4u);
    CHECK(descriptor.formation_count_offset == 0x124u);
    CHECK(descriptor.ai_manager_group_listener_offset == 0x44u);
    CHECK(descriptor.group_listener_add_slot_offset == 0x18u);
    CHECK(descriptor.group_listener_remove_slot_offset == 0x1cu);
    CHECK(descriptor.group_listener_to_formation_offset == 0xb0u);
    CHECK(descriptor.ai_manager_formation_offset == 0xf4u);
    CHECK(descriptor.stat_display_camera_init_rva == 0x00409e08u);
    CHECK(descriptor.stat_display_camera_ui_scene_global_rva ==
        RVA_UI_SCENE_GLOBAL);
    CHECK(descriptor.stat_display_camera_saved_bounds_rva == 0x003c303cu);
    CHECK(descriptor.stat_display_camera_active_bounds_rva == 0x0040cdc0u);
    CHECK(descriptor.stat_display_camera_manager_global_rva ==
        RVA_STAT_CAMERA_MANAGER_GLOBAL);
    CHECK(descriptor.stat_display_camera_ui_scene_last_float_offset ==
        0x158u);
    CHECK(descriptor.stat_display_scene_node_offset == 0x58u);
    CHECK(descriptor.stat_display_owner_offset == 0xccu);
    CHECK(descriptor.stat_display_owner_node_offset == 0x08u);
    CHECK(descriptor.scene_node_dirty_word_offset == 0x2cu);
    CHECK(descriptor.scene_node_matrix_offset == 0x90u);
    CHECK(descriptor.scene_node_matrix_float_count == 16u);
    CHECK(descriptor.camera_manager_active_offset == 0x20u);
    CHECK(descriptor.camera_active_payload_offset == 0x34u);
    CHECK(descriptor.camera_payload_position_offset == 0xc0u);
    CHECK(descriptor.hud_resource_initialized_rva ==
        RVA_RESOURCE_INITIALIZED);
    CHECK(descriptor.hud_resource_table_global_rva ==
        RVA_RESOURCE_TABLE_GLOBAL);
    CHECK(descriptor.hud_resource_actor_component_offset == 0x94u);
    CHECK(descriptor.hud_resource_set_offset == 0x3cu);
    CHECK(descriptor.hud_resource_count_offset == 0x09u);
    CHECK(descriptor.hud_resource_entries_offset == 0x04u);
    CHECK(descriptor.hud_resource_id_offset == 0x19u);
    CHECK(descriptor.hud_resource_table_count_offset == 0x08u);
    CHECK(descriptor.hud_resource_table_data_offset == 0x10u);
    CHECK(descriptor.hud_resource_table_first_slot_offset == 0x4d8u);
    CHECK(descriptor.hud_resource_table_slot_stride == 4u);
    CHECK(descriptor.hud_resource_table_slot_count == 11u);
    CHECK(descriptor.hud_resource_first_table_id == 0x136u);
    CHECK(descriptor.hud_resource_selected_id_min == 1u);
    CHECK(descriptor.hud_resource_selected_id_max == 11u);
    CHECK(descriptor.hud_portrait_gizmo_label_offset == 0x2e0u);
    CHECK(descriptor.hud_string_control_offset == 0u);
    CHECK(descriptor.hud_string_data_offset == 4u);
    CHECK(descriptor.hud_string_inline_mask == UINT32_C(0x80000000));
    CHECK(descriptor.hud_string_inline_capacity_utf16 == 28u);
    CHECK(descriptor.hud_string_proof_max_utf16_units == 27u);
    CHECK(descriptor.pure_validation_only == 1u);
    CHECK(descriptor.native_calls_permitted == 0u);
    CHECK(descriptor.hooks_permitted == 0u);
}

static void test_baseline(void) {
    Fixture fixture;
    SudekiMpTalosStagingNativeSamplerResult result;
    unsigned int hero;

    CHECK(fixture_initialize(&fixture));
    still_exact_calls = 0u;
    CHECK(run_sample(&fixture, &result) == 1);
    CHECK(result.valid == 1u);
    CHECK(result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK);
    CHECK(result.failed_address == 0u);
    CHECK(result.failed_size == 0u);
    CHECK(result.failed_write_required == 0u);
    CHECK(result.first_range_count == 2u);
    CHECK(result.second_range_count == 2u);
    CHECK(result.first_capture_bytes == IMAGE_SIZE + HEAP_SIZE);
    CHECK(result.second_capture_bytes == IMAGE_SIZE + HEAP_SIZE);
    CHECK(result.witness_dispatch_serial == fixture.witness.dispatch_serial);
    CHECK(result.witness_native_thread_id ==
        fixture.witness.native_thread_id);
    CHECK(result.witness_observer_registry_generation ==
        fixture.witness.observer_registry_generation);
    CHECK(result.witness_dispatch_overlap_generation ==
        fixture.witness.dispatch_overlap_generation);
    CHECK(result.witness_source == fixture.witness.source);
    CHECK(result.witness_entry_exact == 1u);
    CHECK(result.witness_revalidated_exact == 1u);
    CHECK(result.transaction_lease_exact == 1u);
    CHECK(result.capture_no_yield_exact == 1u);
    CHECK(result.snapshot.group_count == HERO_COUNT);
    CHECK(result.snapshot.formation_count == HERO_COUNT);
    CHECK(result.snapshot.group_order[0] == 0u);
    CHECK(result.snapshot.group_order[3] == 3u);
    CHECK(result.snapshot.formation_order[0] == 0u);
    CHECK(result.snapshot.formation_order[1] == 3u);
    CHECK(result.snapshot.formation_order[2] == 1u);
    CHECK(result.snapshot.formation_order[3] == 2u);
    CHECK(result.snapshot.transition_active == 0u);
    CHECK(result.snapshot.ui_hud_closure_exact == 1u);
    CHECK(result.snapshot.hero_hud_state_converged == 1u);
    CHECK(result.snapshot.controller_callback_exact == 1u);
    CHECK(result.snapshot.game_thread_exact == 1u);
    CHECK(result.snapshot.transaction_exclusive == 1u);
    CHECK(result.snapshot.no_yield_window_exact == 1u);
    CHECK(result.snapshot.production_authority == 0u);
    CHECK(result.snapshot.carry_authority == 0u);
    CHECK(result.snapshot.actor_lifetime_authority == 0u);
    CHECK(result.native_engine_calls_permitted == 0u);
    CHECK(result.hooks_permitted == 0u);
    CHECK(result.mutation_authority == 0u);
    for (hero = 0u; hero < HERO_COUNT; ++hero) {
        CHECK(result.snapshot.hero[hero].wrapper_token == 0u);
        CHECK(result.snapshot.hero[hero].actor_token != 0u);
        CHECK(result.snapshot.hero[hero].gizmo_label_length != 0u);
        CHECK(result.snapshot.hero[hero].fill_cache_primary_bits ==
            result.snapshot.hero[hero].fill_cache_secondary_bits);
    }
    CHECK(still_exact_calls == 0u);
    fixture_destroy(&fixture);
}

static void test_input_and_witness_near_misses(void) {
    Fixture fixture;
    SudekiMpTalosStagingNativeSamplerInput saved_input;
    SudekiMpControlUpdateDispatchWitness saved_witness;

    CHECK(fixture_initialize(&fixture));
    saved_input = fixture.input;
    saved_witness = fixture.witness;

    fixture.input.exact_executable_hash = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_FACT);
    fixture.input = saved_input;
    fixture.input.exact_sol_hash = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_IMAGE_FACT);
    fixture.input = saved_input;
    fixture.input.membership_abi_valid = 0u;
    expect_failure(&fixture,
        SUDEKIMP_TALOS_NATIVE_SAMPLE_MEMBERSHIP_ABI_FACT);
    fixture.input = saved_input;
    fixture.input.controller_abi_valid = 0u;
    expect_failure(&fixture,
        SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROLLER_ABI_FACT);
    fixture.input = saved_input;
    fixture.input.witness_still_exact_after_capture = 0u;
    expect_failure(&fixture,
        SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS_REVALIDATION);
    fixture.input = saved_input;
    fixture.input.transaction_lease_exclusive = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_TRANSACTION_WINDOW);
    fixture.input = saved_input;
    fixture.input.capture_no_yield_exact = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_TRANSACTION_WINDOW);
    fixture.input = saved_input;
    fixture.input.reload_required = 1u;
    expect_failure(&fixture,
        SUDEKIMP_TALOS_NATIVE_SAMPLE_ORDINARY_DISCIPLINE);
    fixture.input = saved_input;

    fixture.witness.dispatch_serial = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.native_thread_id = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.outer_update_depth = 2u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.active_dispatch_count = 2u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.original_call_count = 2u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.observer_snapshot_count = 2u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.observer_registry_generation = 8u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.hook_owned_exact = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.slot_owned_exact = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.service_only = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.post_original = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.source = SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_UNKNOWN;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.source_exact = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.service_post_original_exact = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.sole_observer = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);
    fixture.witness = saved_witness;
    fixture.witness.registry_generation_stable = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WITNESS);

    fixture_destroy(&fixture);
}

static void test_alias_and_range_layout(void) {
    Fixture fixture;

    CHECK(fixture_initialize(&fixture));
    fixture.ranges[0][1].bytes = fixture.ranges[0][0].bytes;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    fixture.ranges[0][0].bytes = (const uint8_t *)(uintptr_t)
        (UINTPTR_MAX - (uintptr_t)15u);
    fixture.ranges[0][0].size = 0x20u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    fixture.ranges[1][0].bytes = (const uint8_t *)(uintptr_t)
        (UINTPTR_MAX - (uintptr_t)15u);
    fixture.ranges[1][0].size = 0x20u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    fixture.ranges[1][1].bytes = fixture.ranges[0][1].bytes;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    fixture.ranges[0][1].address = IMAGE_BASE + 0x100u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    fixture.ranges[0][1].size =
        SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_CAPTURE_BYTES - IMAGE_SIZE + 1u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_RANGE_LAYOUT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    {
        SudekiMpTalosStagingNativeSamplerResult result;
        CHECK(SudekiMpTalosCompanionStagingNativeSample(
            &fixture.input,
            (const void *)(uintptr_t)CONTROLLER,
            (const void *)(uintptr_t)CONTROLLER,
            &fixture.witness,
            (const void *)(uintptr_t)TRANSACTION_COOKIE,
            &result) == 0);
        CHECK(result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_ALIAS);
    }
    fixture_destroy(&fixture);
}

static void test_readability_and_double_read(void) {
    Fixture fixture;
    SudekiMpTalosStagingNativeSamplerResult result;

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, GROUP + 0xccu, 4u, 0u, 0u);
    CHECK(run_sample(&fixture, &result) == 0);
    CHECK(result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_READABILITY);
    CHECK(result.failed_address == GROUP + 0xccu);
    CHECK(result.failed_size == 4u);
    CHECK(result.failed_write_required == 0u);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32(&fixture, 1u, GROUP + 0xccu, 3u);
    CHECK(run_sample(&fixture, &result) == 0);
    CHECK(result.failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_DOUBLE_READ);
    CHECK(result.failed_address == GROUP + 0xccu);
    fixture_destroy(&fixture);
}

static void test_pointer_scalar_and_order_near_misses(void) {
    Fixture fixture;

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, IMAGE_BASE + RVA_WORLD_GLOBAL, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WORLD);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, GROUP + 0xccu, 5u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, GROUP + 0x90u, actor_address(1u));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_HERO_IDENTITY);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, GROUP + 0x90u + 8u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, AI_MANAGER + 0x124u, 3u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, AI_MANAGER + 0xf4u + 0x0cu,
        actor_address(1u));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, AI_MANAGER + 0xf4u + 0x38u,
        UINT32_C(0x7fc00000));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_FORMATION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, CONTROLLER + 0x248u, actor_address(1u));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROLLER);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, control_address(1u) + 0x10u,
        actor_address(0u));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u8_both(&fixture, control_state_address(1u) + 0x0bu, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, control_address(0u) + 0x150u,
        UINT32_C(0x7fc00000));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, actor_address(0u) + 0x44u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, FORMATION_COMPONENT_BASE + 0x18u,
        UINT32_C(0x7f800000));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, FORMATION_COMPONENT_BASE + 0x1cu,
        UINT32_C(0xff800000));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, FORMATION_COMPONENT_BASE + 0x20u,
        UINT32_C(0x7fc00000));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CONTROL_COMPONENT);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, GROUP + 0x38u, 2u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_LISTENER);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, UI_CONTROLLER, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, gizmo_address(0u) + 0x324u, 3u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_GIZMO);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, gizmo_address(0u) + 0x28u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_GIZMO);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture,
        stat_display_address(0u) + 0xd0u + 0x58u,
        stat_display_address(0u) + 0xd0u + 0x4cu);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_DISPLAY);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u8_both(&fixture, RESOURCE_ENTRY_BASE + 0x19u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, stat_display_address(0u) + 0x58u, 0u);
    expect_failure(&fixture,
        SUDEKIMP_TALOS_NATIVE_SAMPLE_STAT_CAMERA_SYNC);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, STATUS_OWNER_BASE + 0x40u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, UI_SCENE + 0x148u, UINT32_C(0x7fc00000));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_UI_HUD);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, ELCO_ARBITER + 0x10u, actor_address(0u));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_ARBITER);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, CAMERA_MANAGER + 0x28u, CURRENT_CAMERA);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_CAMERA);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u8_both(&fixture, GAME_SPEED + 0x28u, 1u);
    expect_failure(&fixture,
        SUDEKIMP_TALOS_NATIVE_SAMPLE_ORDINARY_DISCIPLINE);
    fixture_destroy(&fixture);
}

static void test_tptr_object_head_closure(void) {
    Fixture fixture;
    SudekiMpTalosStagingNativeSamplerResult result;

    CHECK(fixture_initialize(&fixture));
    use_external_tptr_head(&fixture, 0u);
    CHECK(run_sample(&fixture, &result) == 1);
    CHECK(result.valid == 1u);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    use_external_tptr_head(&fixture, 0u);
    patch_u32_both(&fixture, EXTERNAL_TPTR_HEAD, actor_address(1u));
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    use_external_tptr_head(&fixture, 0u);
    patch_u32_both(&fixture, EXTERNAL_TPTR_HEAD + 4u,
        actor_address(0u) + 4u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    use_external_tptr_head(&fixture, 0u);
    split_heap_range(&fixture, EXTERNAL_TPTR_HEAD, 0x0cu, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);
}

static void test_overwritten_formation_values_not_gated(void) {
    Fixture fixture;
    SudekiMpTalosStagingNativeSamplerResult result;

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, AI_MANAGER + 0xf4u + 0x34u,
        UINT32_C(0xffffffff));
    patch_u32_both(&fixture, AI_MANAGER + 0xf4u + 0x3cu,
        UINT32_C(0x7fc00000));
    patch_u32_both(&fixture, AI_MANAGER + 0xf4u + 0x40u,
        UINT32_C(0xff800000));
    patch_u8_both(&fixture, AI_MANAGER + 0xf4u + 0x50u, 1u);
    CHECK(run_sample(&fixture, &result) == 1);
    CHECK(result.valid == 1u);
    fixture_destroy(&fixture);
}

static void test_hud_string_bounds(void) {
    Fixture fixture;
    uint32_t source_object = RESOURCE_STRING_BASE;
    unsigned int index;

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, source_object,
        UINT32_C(0x80000000) | 28u);
    for (index = 0u; index < 28u; ++index) {
        uint16_t unit = (uint16_t)'X';
        uint8_t *first = fixture_bytes(&fixture, 0u,
            source_object + 4u + index * 2u, 2u);
        uint8_t *second = fixture_bytes(&fixture, 1u,
            source_object + 4u + index * 2u, 2u);
        memcpy(first, &unit, 2u);
        memcpy(second, &unit, 2u);
    }
    {
        uint16_t zero = 0u;
        memcpy(fixture_bytes(&fixture, 0u,
            source_object + 4u + 56u, 2u), &zero, 2u);
        memcpy(fixture_bytes(&fixture, 1u,
            source_object + 4u + 56u, 2u), &zero, 2u);
    }
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u32_both(&fixture, source_object,
        UINT32_C(0x80000000) | 4u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_HUD_RESOURCE);
    fixture_destroy(&fixture);
}

static void test_write_permissions(void) {
    Fixture fixture;

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, GROUP + 0x90u, HERO_COUNT * 0x0cu, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, AI_MANAGER + 0xf4u,
        HERO_COUNT * 0x0cu, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, AI_MANAGER + 0xf4u + 0x34u,
        4u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, AI_MANAGER + 0xf4u + 0x38u,
        4u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, AI_MANAGER + 0xf4u + 0x50u,
        1u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, LISTENER_STORAGE, 4u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, control_address(0u) + 0x40u, 4u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, actor_address(0u) + 4u, 4u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, gizmo_address(0u) + 0x2e0u, 60u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, STAT_ARRAY_BASE + 0x40u, 8u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, STAT_NODE_BASE + 0x90u, 64u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    split_heap_range(&fixture, ELCO_ARBITER + 0x60u, 4u, 1u, 0u);
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);
}

static void test_conditional_camera_initialization(void) {
    Fixture fixture;
    SudekiMpTalosStagingNativeSamplerResult result;

    CHECK(fixture_initialize(&fixture));
    patch_u8_both(&fixture, IMAGE_BASE + RVA_STAT_CAMERA_INITIALIZED, 0u);
    CHECK(run_sample(&fixture, &result) == 1);
    CHECK(result.snapshot.ui_hud_closure_exact == 1u);
    fixture_destroy(&fixture);

    CHECK(fixture_initialize(&fixture));
    patch_u8_both(&fixture, IMAGE_BASE + RVA_STAT_CAMERA_INITIALIZED, 0u);
    fixture.ranges[0][0].native_writable = 0u;
    fixture.ranges[1][0].native_writable = 0u;
    expect_failure(&fixture, SUDEKIMP_TALOS_NATIVE_SAMPLE_WRITE_PERMISSION);
    fixture_destroy(&fixture);
}

int main(void) {
    test_descriptor_cross_check();
    test_baseline();
    test_input_and_witness_near_misses();
    test_alias_and_range_layout();
    test_readability_and_double_read();
    test_pointer_scalar_and_order_near_misses();
    test_tptr_object_head_closure();
    test_overwritten_formation_values_not_gated();
    test_hud_string_bounds();
    test_write_permissions();
    test_conditional_camera_initialization();

    if (failures != 0) {
        fprintf(stderr,
            "talos companion staging native sampler tests failed: %d\n",
            failures);
        return 1;
    }
    puts("talos companion staging native sampler tests passed");
    return 0;
}
