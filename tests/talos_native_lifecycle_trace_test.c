#include "hooks/talos_native_lifecycle_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

enum {
    TEST_IMAGE_SIZE = 0x0040b000u,
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_SCENE_OPCODE = 0x001c4d30u,
    RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL = 0x001c4db8u,
    RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR = 0x001c3170u,
    RVA_KAZEL_GROUP_ADD_CALL = 0x000b15dbu,
    RVA_RAW_GROUP_ADD = 0x00023280u,
    RVA_AI_LISTENER_VTABLE = 0x002ca244u,
    RVA_AI_LISTENER_ADD = 0x000f2b00u,
    RVA_AI_LISTENER_FORMATION_ADD_CALL = 0x000f2b14u,
    RVA_RAW_FORMATION_ADD = 0x000b2cb0u,
    RVA_DELETE_PC = 0x000b2520u,
    RVA_REMOVE_ALL_PLAYERS = 0x000252d0u,
    RVA_FORMATION_POP_MEMBERS = 0x000f6260u,
    RVA_TSA_IS_PLAYING = 0x0001a230u,
    RVA_TSA_SET_PLAYING = 0x0001a240u,
    RVA_TSA_DISPATCH = 0x0003f3b0u,
    RVA_TSA_PLAYING_GLOBAL = 0x00408d4cu,
    RVA_TSA_SHADOW_GLOBAL = 0x003c2f3cu,
    RVA_TSA_SCRIPT_MANAGER_GLOBAL = 0x00409d8cu,
    RVA_TSA_EVENT_NAME = 0x003c3a64u,
    RVA_AI_MANAGER_GLOBAL = 0x00409de4u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_SCENE_OPCODE_SLOT = 0x00323fa8u
};

static const uint8_t script_call_opcode_signature[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x81u, 0xecu,
    0x90u, 0x00u, 0x00u, 0x00u, 0x53u, 0x55u, 0x56u, 0x57u,
    0x8bu, 0xe9u
};
static const uint8_t script_scene_opcode_signature[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x18u, 0x53u, 0x55u, 0x56u, 0x57u, 0x8bu, 0xf9u,
    0x8bu, 0x47u, 0x0cu
};
static const uint8_t script_scene_task_constructor_window[] = {
    0x8bu, 0xc3u, 0xe8u, 0xb3u, 0xe3u, 0xffu, 0xffu,
    0x8bu, 0x74u, 0x24u, 0x10u, 0x85u, 0xf6u, 0x74u, 0x3cu
};
static const uint8_t kazel_group_add_call_prefix[] = {
    0x8bu, 0x4cu, 0x24u, 0x18u, 0xa1u
};
static const uint8_t kazel_group_add_call_suffix[] = {
    0x51u, 0xe8u, 0xa0u, 0x1cu, 0xf7u, 0xffu, 0xebu, 0x1bu
};
static const uint8_t raw_group_add_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x8bu, 0x55u,
    0x08u, 0x83u, 0xecu, 0x14u, 0x53u, 0x56u, 0x57u, 0x8bu,
    0xf0u
};
static const uint8_t ai_listener_add_prefix[] = {
    0x8bu, 0x54u, 0x24u, 0x04u, 0x56u, 0x85u, 0xd2u, 0x74u,
    0x10u, 0x51u, 0x8bu, 0xc4u, 0x8du, 0xb1u, 0xb0u, 0x00u,
    0x00u, 0x00u, 0x89u, 0x10u
};
static const uint8_t raw_formation_add_entry[] = {
    0x51u, 0x8bu, 0x4eu, 0x30u, 0x8bu, 0x54u, 0x24u, 0x08u,
    0x33u, 0xc0u, 0x57u, 0x85u, 0xc9u, 0x7eu, 0x0eu
};
static const uint8_t delete_pc_signature[] = {
    0x83u, 0xecu, 0x0cu, 0x56u, 0x8bu, 0x74u, 0x24u, 0x14u
};
static const uint8_t remove_all_players_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x06u, 0x50u, 0xe8u,
    0x71u, 0xffu, 0xffu, 0xffu, 0xc3u
};
static const uint8_t formation_pop_members_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x10u, 0x05u, 0xf4u, 0x00u, 0x00u,
    0x00u, 0x74u, 0x09u, 0x6au, 0x00u, 0x6au, 0x00u
};
static const uint8_t tsa_is_playing_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x04u, 0x8au, 0x40u, 0x74u, 0xc3u,
    0x32u, 0xc0u, 0xc3u
};
static const uint8_t tsa_set_playing_tail[] = {
    0x8au, 0x4cu, 0x24u, 0x04u, 0x83u, 0xecu, 0x0cu, 0x85u,
    0xc0u, 0x74u, 0x03u, 0x88u, 0x48u, 0x74u, 0x33u, 0xc0u,
    0x84u, 0xc9u, 0x0fu, 0x95u, 0xc0u, 0x56u, 0x8bu, 0xf0u
};
static const uint8_t tsa_set_playing_after_shadow[] = {
    0x3bu, 0xf0u, 0x74u, 0x34u, 0x83u, 0xf8u, 0x01u,
    0x75u, 0x29u, 0x85u, 0xf6u, 0x75u, 0x25u
};
static const uint8_t tsa_set_playing_dispatch_body[] = {
    0x89u, 0x44u, 0x24u, 0x0cu, 0x51u, 0x8du, 0x44u,
    0x24u, 0x0cu, 0xc7u, 0x44u, 0x24u, 0x0cu, 0x09u,
    0x00u, 0x00u, 0x00u, 0x89u, 0x74u, 0x24u, 0x14u
};
static const uint8_t tsa_set_playing_suffix[] = {
    0x5eu, 0x83u, 0xc4u, 0x0cu, 0xc3u, 0xccu
};

typedef struct HeroIdentityFixture {
    uint32_t main_vtable_rva;
    uint32_t secondary_vtable_rva;
    uint32_t resource_vtable_rva;
    uint32_t main_col_rva;
    uint32_t secondary_col_rva;
    uint32_t resource_col_rva;
    uint32_t type_descriptor_rva;
    uint32_t type_method_rva;
    uint32_t type_value;
    const char *type_name;
} HeroIdentityFixture;

static const HeroIdentityFixture hero_identity_fixtures[] = {
    {0x002d5010u, 0x002d5034u, 0x002d5054u,
     0x002fd4e4u, 0x002fd4d0u, 0x002fd4bcu,
     0x0035a8fcu, 0x00139ad0u, 0x23u, ".?AVTalEntity@@"},
    {0x002d555cu, 0x002d5580u, 0x002d55a0u,
     0x002fe500u, 0x002fe4ecu, 0x002fe4d8u,
     0x0035ad34u, 0x001e8240u, 0x01u, ".?AVAilishEntity@@"},
    {0x002d5a88u, 0x002d5aacu, 0x002d5accu,
     0x002fec4cu, 0x002fec38u, 0x002fec24u,
     0x0035af80u, 0x0022c0e0u, 0x05u, ".?AVBukiEntity@@"},
    {0x002d66fcu, 0x002d6720u, 0x002d6740u,
     0x002ff718u, 0x002ff704u, 0x002ff6f0u,
     0x0035b1d4u, 0x0014d730u, 0x0eu, ".?AVElcoEntity@@"},
    {0x002d6884u, 0x002d68a8u, 0x002d68c8u,
     0x002ff864u, 0x002ff850u, 0x002ff83cu,
     0x0035b238u, 0x00151230u, 0x0bu, ".?AVDarkTalEntity@@"}
};

static void populate_hero_identity_fixtures(uint8_t *image) {
    static const uint32_t subobject_offsets[] = {0u, 8u, 0x2cu};
    size_t hero;

    for (hero = 0u;
            hero < sizeof(hero_identity_fixtures) /
                sizeof(hero_identity_fixtures[0]);
            ++hero) {
        const HeroIdentityFixture *fixture = &hero_identity_fixtures[hero];
        const uint32_t vtables[] = {
            fixture->main_vtable_rva,
            fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva
        };
        const uint32_t locators[] = {
            fixture->main_col_rva,
            fixture->secondary_col_rva,
            fixture->resource_col_rva
        };
        size_t subobject;

        for (subobject = 0u; subobject < 3u; ++subobject) {
            *(uint32_t *)(image + vtables[subobject] - sizeof(uint32_t)) =
                (uint32_t)(uintptr_t)(image + locators[subobject]);
            *(uint32_t *)(image + locators[subobject] + sizeof(uint32_t)) =
                subobject_offsets[subobject];
            *(uint32_t *)(image + locators[subobject] +
                3u * sizeof(uint32_t)) = (uint32_t)(uintptr_t)(
                    image + fixture->type_descriptor_rva);
        }
        memcpy(image + fixture->type_descriptor_rva + 8u,
            fixture->type_name, strlen(fixture->type_name) + 1u);
        *(uint32_t *)(image + fixture->resource_vtable_rva + 0x10u) =
            (uint32_t)(uintptr_t)(image + fixture->type_method_rva);
        image[fixture->type_method_rva] = 0xb8u;
        *(uint32_t *)(image + fixture->type_method_rva + 1u) =
            fixture->type_value;
        image[fixture->type_method_rva + 5u] = 0xc3u;
    }
}

static BOOL hero_identity_fixture_bytes_match(const uint8_t *image) {
    static const uint32_t subobject_offsets[] = {0u, 8u, 0x2cu};
    size_t hero;

    for (hero = 0u;
            hero < sizeof(hero_identity_fixtures) /
                sizeof(hero_identity_fixtures[0]);
            ++hero) {
        const HeroIdentityFixture *fixture = &hero_identity_fixtures[hero];
        const uint32_t vtables[] = {
            fixture->main_vtable_rva,
            fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva
        };
        const uint32_t locators[] = {
            fixture->main_col_rva,
            fixture->secondary_col_rva,
            fixture->resource_col_rva
        };
        size_t subobject;

        for (subobject = 0u; subobject < 3u; ++subobject) {
            if (*(const uint32_t *)(image + vtables[subobject] - 4u) !=
                    (uint32_t)(uintptr_t)(image + locators[subobject]) ||
                *(const uint32_t *)(image + locators[subobject] + 4u) !=
                    subobject_offsets[subobject] ||
                *(const uint32_t *)(image + locators[subobject] + 12u) !=
                    (uint32_t)(uintptr_t)(
                        image + fixture->type_descriptor_rva)) return FALSE;
        }
        if (strcmp((const char *)(image + fixture->type_descriptor_rva + 8u),
                fixture->type_name) != 0 ||
            *(const uint32_t *)(image + fixture->resource_vtable_rva +
                0x10u) != (uint32_t)(uintptr_t)(
                    image + fixture->type_method_rva) ||
            image[fixture->type_method_rva] != 0xb8u ||
            *(const uint32_t *)(image + fixture->type_method_rva + 1u) !=
                fixture->type_value ||
            image[fixture->type_method_rva + 5u] != 0xc3u) return FALSE;
    }
    return TRUE;
}

static void point_relative_call(uint8_t *instruction, const uint8_t *target) {
    int32_t displacement = (int32_t)(target - (instruction + 5u));

    instruction[0] = 0xe8u;
    memcpy(instruction + 1u, &displacement, sizeof(displacement));
}

static uint8_t *relative_call_target(uint8_t *instruction) {
    int32_t displacement;

    memcpy(&displacement, instruction + 1u, sizeof(displacement));
    return instruction + 5u + displacement;
}

static uint8_t *make_exact_image(void) {
    uint8_t *image = (uint8_t *)VirtualAlloc(
        NULL, TEST_IMAGE_SIZE, MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);

    CHECK(image != NULL);
    image[RVA_FORMATION_POP_MEMBERS] = 0xa1u;
    *(uint32_t *)(image + RVA_FORMATION_POP_MEMBERS + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_AI_MANAGER_GLOBAL);
    memcpy(image + RVA_FORMATION_POP_MEMBERS + 5u,
        formation_pop_members_tail, sizeof(formation_pop_members_tail));
    image[RVA_TSA_IS_PLAYING] = 0xa1u;
    *(uint32_t *)(image + RVA_TSA_IS_PLAYING + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_TSA_PLAYING_GLOBAL);
    memcpy(image + RVA_TSA_IS_PLAYING + 5u,
        tsa_is_playing_tail, sizeof(tsa_is_playing_tail));
    image[RVA_TSA_SET_PLAYING] = 0xa1u;
    *(uint32_t *)(image + RVA_TSA_SET_PLAYING + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_TSA_PLAYING_GLOBAL);
    memcpy(image + RVA_TSA_SET_PLAYING + 5u,
        tsa_set_playing_tail, sizeof(tsa_set_playing_tail));
    image[RVA_TSA_SET_PLAYING + 0x1du] = 0xa1u;
    *(uint32_t *)(image + RVA_TSA_SET_PLAYING + 0x1eu) =
        (uint32_t)(uintptr_t)(image + RVA_TSA_SHADOW_GLOBAL);
    memcpy(image + RVA_TSA_SET_PLAYING + 0x22u,
        tsa_set_playing_after_shadow,
        sizeof(tsa_set_playing_after_shadow));
    image[RVA_TSA_SET_PLAYING + 0x2fu] = 0x8bu;
    image[RVA_TSA_SET_PLAYING + 0x30u] = 0x0du;
    *(uint32_t *)(image + RVA_TSA_SET_PLAYING + 0x31u) =
        (uint32_t)(uintptr_t)(image + RVA_TSA_SCRIPT_MANAGER_GLOBAL);
    image[RVA_TSA_SET_PLAYING + 0x35u] = 0x68u;
    *(uint32_t *)(image + RVA_TSA_SET_PLAYING + 0x36u) =
        (uint32_t)(uintptr_t)(image + RVA_TSA_EVENT_NAME);
    memcpy(image + RVA_TSA_SET_PLAYING + 0x3au,
        tsa_set_playing_dispatch_body,
        sizeof(tsa_set_playing_dispatch_body));
    point_relative_call(image + RVA_TSA_SET_PLAYING + 0x4fu,
        image + RVA_TSA_DISPATCH);
    image[RVA_TSA_SET_PLAYING + 0x54u] = 0x89u;
    image[RVA_TSA_SET_PLAYING + 0x55u] = 0x35u;
    *(uint32_t *)(image + RVA_TSA_SET_PLAYING + 0x56u) =
        (uint32_t)(uintptr_t)(image + RVA_TSA_SHADOW_GLOBAL);
    memcpy(image + RVA_TSA_SET_PLAYING + 0x5au,
        tsa_set_playing_suffix, sizeof(tsa_set_playing_suffix));
    memcpy(image + RVA_SCRIPT_CALL_OPCODE, script_call_opcode_signature,
        sizeof(script_call_opcode_signature));
    memcpy(image + RVA_SCRIPT_SCENE_OPCODE, script_scene_opcode_signature,
        sizeof(script_scene_opcode_signature));
    memcpy(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL - 2u,
        script_scene_task_constructor_window,
        sizeof(script_scene_task_constructor_window));
    memcpy(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        kazel_group_add_call_prefix,
        sizeof(kazel_group_add_call_prefix));
    *(uint32_t *)(image + RVA_KAZEL_GROUP_ADD_CALL - 5u) =
        (uint32_t)(uintptr_t)(image + 0x00408d94u);
    memcpy(image + RVA_KAZEL_GROUP_ADD_CALL - 1u,
        kazel_group_add_call_suffix,
        sizeof(kazel_group_add_call_suffix));
    point_relative_call(image + RVA_KAZEL_GROUP_ADD_CALL,
        image + RVA_RAW_GROUP_ADD);
    memcpy(image + RVA_RAW_GROUP_ADD, raw_group_add_entry,
        sizeof(raw_group_add_entry));
    memcpy(image + RVA_RAW_GROUP_ADD + 0x108u, "\xc2\x04\x00", 3u);
    *(uint32_t *)(image + RVA_AI_LISTENER_VTABLE + 0x18u) =
        (uint32_t)(uintptr_t)(image + RVA_AI_LISTENER_ADD);
    memcpy(image + RVA_AI_LISTENER_ADD, ai_listener_add_prefix,
        sizeof(ai_listener_add_prefix));
    point_relative_call(image + RVA_AI_LISTENER_FORMATION_ADD_CALL,
        image + RVA_RAW_FORMATION_ADD);
    memcpy(image + RVA_AI_LISTENER_ADD + 0x23u, "\xc2\x0c\x00", 3u);
    memcpy(image + RVA_RAW_FORMATION_ADD, raw_formation_add_entry,
        sizeof(raw_formation_add_entry));
    memcpy(image + RVA_RAW_FORMATION_ADD + 0x87u,
        "\xc2\x04\x00", 3u);
    memcpy(image + RVA_RAW_FORMATION_ADD + 0x8eu,
        "\xc2\x04\x00", 3u);
    memcpy(image + RVA_DELETE_PC, delete_pc_signature,
        sizeof(delete_pc_signature));
    image[RVA_REMOVE_ALL_PLAYERS] = 0xa1u;
    *(uint32_t *)(image + RVA_REMOVE_ALL_PLAYERS + 1u) =
        (uint32_t)(uintptr_t)(image + 0x00408d94u);
    memcpy(image + RVA_REMOVE_ALL_PLAYERS + 5u, remove_all_players_tail,
        sizeof(remove_all_players_tail));
    *(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) =
        image + RVA_SCRIPT_CALL_OPCODE;
    *(void **)(image + RVA_SCRIPT_SCENE_OPCODE_SLOT) =
        image + RVA_SCRIPT_SCENE_OPCODE;
    populate_hero_identity_fixtures(image);
    return image;
}

static void test_exact_classifier(void) {
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x29u, 0x21c0du,
        UINT32_C(0x70f470c2)) == SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x2194eu,
        UINT32_C(0xfa7ec379)) == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x21959u,
        UINT32_C(0xfa7ec379)) == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x21964u,
        UINT32_C(0xfa7ec379)) == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x2196fu,
        UINT32_C(0x76fc7114)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_CARRIER);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x317du,
        UINT32_C(0xbc8fdc32)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_NOW);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x21addu,
        UINT32_C(0x343b1e0c)) == SUDEKIMP_TALOS_NATIVE_EVENT_END_TSA);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x219f8u,
        UINT32_C(0x882300d3)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0xbc1b3u,
        UINT32_C(0xb3d3544b)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x3099u,
        UINT32_C(0xe9b77316)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0xbc433u,
        UINT32_C(0xfa7ec379)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x284bu,
        UINT32_C(0xc1366076)) ==
        SUDEKIMP_TALOS_NATIVE_EVENT_REMOVE_ALL_PLAYERS);
}

static void test_near_misses_are_inert(void) {
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x21c0du,
        UINT32_C(0x70f470c2)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x29u, 0x21c0cu,
        UINT32_C(0x70f470c2)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x29u, 0x21c0du,
        UINT32_C(0x70f470c3)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x21965u,
        UINT32_C(0xfa7ec379)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x29u, 0x219f8u,
        UINT32_C(0x882300d3)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x219f9u,
        UINT32_C(0x882300d3)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x219f8u,
        UINT32_C(0x882300d2)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0xbc1b3u,
        UINT32_C(0xb3d3544a)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x3098u,
        UINT32_C(0xe9b77316)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0xbc432u,
        UINT32_C(0xfa7ec379)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
    CHECK(SudekiMpTalosNativeLifecycleClassify(0x27u, 0x284bu,
        UINT32_C(0xc1366077)) == SUDEKIMP_TALOS_NATIVE_EVENT_NONE);
}

static void test_delete_resource_policy_uses_native_identifiers(void) {
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier("PC_Buki") ==
        UINT32_C(0x019c1eba));
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier("pc_ailish") ==
        UINT32_C(0x8557d453));
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier("PC_Elco") ==
        UINT32_C(0x0180e1d4));
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier("PC_Tal") ==
        UINT32_C(0x0213755c));
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier("PC_KAZEL") ==
        UINT32_C(0xa6d349cc));
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier("") == 0u);
    CHECK(SudekiMpTalosNativeLifecycleResourceIdentifier(
        "0123456789abcdef") == 0u);

    CHECK(SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI,
        UINT32_C(0x019c1eba), "PC_Buki", TRUE, TRUE));
    CHECK(SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH,
        UINT32_C(0x8557d453), "pc_ailish", TRUE, TRUE));
    CHECK(SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO,
        UINT32_C(0x0180e1d4), "PC_Elco", TRUE, TRUE));
    CHECK(SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL,
        UINT32_C(0xa6d349cc), "PC_KAZEL", TRUE, TRUE));

    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI,
        4u, "PC_Buki", TRUE, TRUE));
    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI,
        UINT32_C(0x8557d453), "PC_Ailish", TRUE, TRUE));
    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH,
        UINT32_C(0x8557d453), "PC_Buki", TRUE, TRUE));
    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO,
        UINT32_C(0x0180e1d4), "PC_Elco", FALSE, TRUE));
    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO,
        UINT32_C(0x0180e1d4), "PC_Elco", TRUE, FALSE));
    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_NONE,
        UINT32_C(0x019c1eba), "PC_Buki", TRUE, TRUE));
    CHECK(!SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
        SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL,
        UINT32_C(0xa6d349cc), "PC_Tal", TRUE, TRUE));
}

static void test_tsa_inactive_evidence_policy_is_exact(void) {
#define TSA_POLICY(armed, opcode, operand, hash, runtime, thread, lineage, requested, before, after) \
    SudekiMpTalosNativeLifecycleTsaInactiveEvidencePolicy( \
        (armed), (opcode), (operand), (hash), (runtime), (thread), \
        (lineage), (requested), (before), (after))
    CHECK(TSA_POLICY(TRUE, 0x27u, 0x00039bcbu, UINT32_C(0x0f3b3bff),
        TRUE, TRUE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(FALSE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x29u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcau,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bfe), TRUE, TRUE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), FALSE, TRUE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, FALSE, TRUE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, FALSE, 0u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, TRUE, 1u, TRUE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, TRUE, 0u, FALSE, FALSE));
    CHECK(!TSA_POLICY(TRUE, 0x27u, 0x00039bcbu,
        UINT32_C(0x0f3b3bff), TRUE, TRUE, TRUE, 0u, TRUE, TRUE));
#undef TSA_POLICY
}

static void test_camera_and_control_settle_policies_are_exact(void) {
    unsigned int missing;

    CHECK(SudekiMpTalosNativeLifecycleDefaultCameraEvidencePolicy(
        TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE));
    for (missing = 0u; missing < 7u; ++missing) {
        BOOL evidence[7] = {
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE
        };

        evidence[missing] = FALSE;
        CHECK(!SudekiMpTalosNativeLifecycleDefaultCameraEvidencePolicy(
            evidence[0], evidence[1], evidence[2], evidence[3],
            evidence[4], evidence[5], evidence[6]));
    }
    CHECK(SudekiMpTalosNativeLifecycleSettleEvidencePolicy(
        TRUE, TRUE, TRUE, TRUE));
    for (missing = 0u; missing < 4u; ++missing) {
        BOOL evidence[4] = {TRUE, TRUE, TRUE, TRUE};

        evidence[missing] = FALSE;
        CHECK(!SudekiMpTalosNativeLifecycleSettleEvidencePolicy(
            evidence[0], evidence[1], evidence[2], evidence[3]));
    }
}

static void make_exact_post_movie_ticket_evidence(
    SudekiMpTalosNativeLifecycleSnapshot *lifecycle,
    SudekiMpTalosNativeRosterIdentitySnapshot *roster,
    SudekiMpTalosNativeKazelSnapshot *kazel,
    SudekiMpTalosNativeSettleEvidenceSnapshot *settle
) {
    memset(lifecycle, 0, sizeof(*lifecycle));
    memset(roster, 0, sizeof(*roster));
    memset(kazel, 0, sizeof(*kazel));
    memset(settle, 0, sizeof(*settle));

    lifecycle->run_id_high = 0x1234u;
    lifecycle->run_id_low = 0x5678u;
    lifecycle->event_serial = 41u;
    lifecycle->script_runtime_generation = 7u;
    lifecycle->load_void_task_generation = 9u;
    lifecycle->installed = 1u;
    lifecycle->load_void_task_bound = 1u;
    lifecycle->load_void_descendant_observed = 1u;
    lifecycle->tsa_inactive_observed = 1u;

    roster->observation_serial = 17u;
    roster->script_runtime_generation = 7u;
    roster->load_void_task_generation = 9u;
    roster->roster_revision = 4u;
    roster->hero_token[SUDEKIMP_TALOS_NATIVE_HERO_TAL] =
        UINT64_C(0x1111111111111111);
    roster->hero_present_mask = 0x01u;
    roster->group_hero_mask = 0x01u;
    roster->formation_hero_mask = 0x01u;
    roster->sequence_state =
        SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED;
    roster->delete_delta_corroborated_mask = 0x0eu;

    kazel->session_generation = 3u;
    kazel->observation_serial = 23u;
    kazel->request_generation = 2u;
    kazel->script_runtime_generation = 7u;
    kazel->load_void_task_generation = 9u;
    kazel->source_native_thread_id = 37u;
    kazel->completion_native_thread_id = 37u;
    kazel->original_tal_token =
        UINT64_C(0x1111111111111111);
    kazel->kazel_token = UINT64_C(0x2222222222222222);
    kazel->state = SUDEKIMP_TALOS_NATIVE_KAZEL_DELETE_CORROBORATED;
    kazel->spawn_binding_before_seen = 1u;
    kazel->spawn_binding_after_seen = 1u;
    kazel->group_add_before_seen = 1u;
    kazel->group_add_after_seen = 1u;
    kazel->exact_dark_tal_identity = 1u;
    kazel->group_add_corroborated = 1u;
    kazel->delete_corroborated = 1u;
    kazel->serialized_opcode_mask = 0x3fu;

    settle->session_generation = 3u;
    settle->script_runtime_generation = 7u;
    settle->load_void_task_generation = 9u;
    settle->default_camera_generation = 5u;
    settle->settle_validation_generation = 6u;
    settle->void_set_zone_completed = 1u;
    settle->default_camera_committed = 1u;
    settle->default_camera_revalidated = 1u;
    settle->tal_control_revalidated = 1u;
    settle->settle_evidence_complete = 1u;
}

static void test_post_movie_restore_ticket_policy_is_exact(void) {
    SudekiMpTalosNativeLifecycleSnapshot lifecycle;
    SudekiMpTalosNativeRosterIdentitySnapshot roster;
    SudekiMpTalosNativeKazelSnapshot kazel;
    SudekiMpTalosNativeSettleEvidenceSnapshot settle;

#define TICKET_POLICY(allow, asset, lifecycle_value, roster_value, \
        kazel_value, settle_value, current_thread, settle_thread) \
    SudekiMpTalosNativeLifecyclePostMovieRestoreTicketPolicy( \
        (allow), (asset), (lifecycle_value), (roster_value), \
        (kazel_value), (settle_value), (current_thread), (settle_thread))
#define CHECK_TICKET_REJECTS(mutation) do { \
    SudekiMpTalosNativeLifecycleSnapshot changed_lifecycle = lifecycle; \
    SudekiMpTalosNativeRosterIdentitySnapshot changed_roster = roster; \
    SudekiMpTalosNativeKazelSnapshot changed_kazel = kazel; \
    SudekiMpTalosNativeSettleEvidenceSnapshot changed_settle = settle; \
    mutation; \
    CHECK(!TICKET_POLICY(TRUE, TRUE, &changed_lifecycle, \
        &changed_roster, &changed_kazel, &changed_settle, 37u, 37u)); \
} while (0)

    make_exact_post_movie_ticket_evidence(
        &lifecycle, &roster, &kazel, &settle);
    CHECK(TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));
    CHECK(!TICKET_POLICY(FALSE, TRUE, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));
    CHECK(!TICKET_POLICY(TRUE, FALSE, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, NULL, &roster, &kazel, &settle,
        37u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, NULL, &kazel, &settle,
        37u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, NULL, &settle,
        37u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, NULL,
        37u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        0u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        37u, 0u));

    CHECK_TICKET_REJECTS(changed_lifecycle.installed = 0u);
    CHECK_TICKET_REJECTS(changed_lifecycle.run_id_high = 0u;
        changed_lifecycle.run_id_low = 0u);
    CHECK_TICKET_REJECTS(changed_lifecycle.event_serial = 0u);
    CHECK_TICKET_REJECTS(changed_lifecycle.load_void_task_bound = 0u);
    CHECK_TICKET_REJECTS(
        changed_lifecycle.load_void_descendant_observed = 0u);
    CHECK_TICKET_REJECTS(changed_lifecycle.tsa_inactive_observed = 0u);
    CHECK_TICKET_REJECTS(changed_lifecycle.opcode_27_depth = 1u);
    CHECK_TICKET_REJECTS(changed_lifecycle.opcode_29_depth = 1u);
    CHECK_TICKET_REJECTS(
        changed_lifecycle.script_runtime_generation = 0u);
    CHECK_TICKET_REJECTS(
        changed_lifecycle.load_void_task_generation = 0u);

    CHECK_TICKET_REJECTS(changed_roster.observation_serial = 0u);
    CHECK_TICKET_REJECTS(changed_roster.roster_revision = 0u);
    CHECK_TICKET_REJECTS(changed_roster.script_runtime_generation = 8u);
    CHECK_TICKET_REJECTS(changed_roster.load_void_task_generation = 10u);
    CHECK_TICKET_REJECTS(changed_roster.sequence_state =
        SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_RELEASED);
    CHECK_TICKET_REJECTS(changed_roster.quarantine_reason = 1u);
    CHECK_TICKET_REJECTS(changed_roster.hero_present_mask = 0x03u);
    CHECK_TICKET_REJECTS(changed_roster.group_hero_mask = 0x03u);
    CHECK_TICKET_REJECTS(changed_roster.formation_hero_mask = 0x03u);
    CHECK_TICKET_REJECTS(
        changed_roster.delete_delta_corroborated_mask = 0x06u);
    CHECK_TICKET_REJECTS(changed_roster.hero_token[
        SUDEKIMP_TALOS_NATIVE_HERO_TAL] = 0u);

    CHECK_TICKET_REJECTS(changed_kazel.session_generation = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.observation_serial = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.request_generation = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.script_runtime_generation = 8u);
    CHECK_TICKET_REJECTS(changed_kazel.load_void_task_generation = 10u);
    CHECK_TICKET_REJECTS(changed_kazel.source_native_thread_id = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.completion_native_thread_id = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.completion_native_thread_id = 38u);
    CHECK_TICKET_REJECTS(changed_kazel.state =
        SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_CORROBORATED);
    CHECK_TICKET_REJECTS(changed_kazel.spawn_binding_before_seen = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.spawn_binding_after_seen = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.group_add_before_seen = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.group_add_after_seen = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.exact_dark_tal_identity = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.group_add_corroborated = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.delete_corroborated = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.serialized_opcode_mask = 0x1fu);
    CHECK_TICKET_REJECTS(changed_kazel.ambiguity_reason = 1u);
    CHECK_TICKET_REJECTS(changed_kazel.original_tal_token =
        UINT64_C(0x3333333333333333));
    CHECK_TICKET_REJECTS(changed_kazel.kazel_token = 0u);
    CHECK_TICKET_REJECTS(changed_kazel.kazel_token =
        changed_kazel.original_tal_token);

    CHECK_TICKET_REJECTS(changed_settle.session_generation = 0u);
    CHECK_TICKET_REJECTS(changed_settle.session_generation = 4u);
    CHECK_TICKET_REJECTS(changed_settle.script_runtime_generation = 8u);
    CHECK_TICKET_REJECTS(changed_settle.load_void_task_generation = 10u);
    CHECK_TICKET_REJECTS(changed_settle.default_camera_generation = 0u);
    CHECK_TICKET_REJECTS(
        changed_settle.settle_validation_generation = 0u);
    CHECK_TICKET_REJECTS(changed_settle.void_set_zone_completed = 0u);
    CHECK_TICKET_REJECTS(changed_settle.default_camera_committed = 0u);
    CHECK_TICKET_REJECTS(changed_settle.default_camera_revalidated = 0u);
    CHECK_TICKET_REJECTS(changed_settle.tal_control_revalidated = 0u);
    CHECK_TICKET_REJECTS(changed_settle.settle_evidence_complete = 0u);

    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        38u, 37u));
    CHECK(!TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        37u, 38u));
    kazel.completion_was_synchronous = 1u;
    CHECK(TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));
    kazel.actor_lifetime_authority_proven = 1u;
    roster.actor_lifetime_authority_proven = 1u;
    CHECK(TICKET_POLICY(TRUE, TRUE, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));

#undef CHECK_TICKET_REJECTS
#undef TICKET_POLICY
}

static SudekiMpTalosNativePostMovieRestoreTicket
make_exact_post_movie_restore_ticket(
    const SudekiMpTalosNativeLifecycleSnapshot *lifecycle,
    const SudekiMpTalosNativeRosterIdentitySnapshot *roster,
    const SudekiMpTalosNativeKazelSnapshot *kazel,
    const SudekiMpTalosNativeSettleEvidenceSnapshot *settle
) {
    SudekiMpTalosNativePostMovieRestoreTicket ticket;

    memset(&ticket, 0, sizeof(ticket));
    ticket.run_id_high = lifecycle->run_id_high;
    ticket.run_id_low = lifecycle->run_id_low;
    ticket.authorization_generation = 1u;
    ticket.lifecycle_event_serial = lifecycle->event_serial;
    ticket.roster_observation_serial = roster->observation_serial;
    ticket.roster_revision = roster->roster_revision;
    ticket.kazel_observation_serial = kazel->observation_serial;
    ticket.kazel_request_generation = kazel->request_generation;
    ticket.session_generation = kazel->session_generation;
    ticket.script_runtime_generation =
        lifecycle->script_runtime_generation;
    ticket.load_void_task_generation =
        lifecycle->load_void_task_generation;
    ticket.settle_validation_generation =
        settle->settle_validation_generation;
    ticket.default_camera_generation = settle->default_camera_generation;
    ticket.native_thread_id = kazel->source_native_thread_id;
    ticket.tal_token = roster->hero_token[
        SUDEKIMP_TALOS_NATIVE_HERO_TAL];
    ticket.kazel_token = kazel->kazel_token;
    return ticket;
}

static void test_ready_ticket_survives_normal_camera_invalidation(void) {
    SudekiMpTalosNativeLifecycleSnapshot lifecycle;
    SudekiMpTalosNativeRosterIdentitySnapshot roster;
    SudekiMpTalosNativeKazelSnapshot kazel;
    SudekiMpTalosNativeSettleEvidenceSnapshot settle;
    SudekiMpTalosNativePostMovieRestoreTicket ticket;

#define READY_POLICY(ticket_value, lifecycle_value, roster_value, \
        kazel_value, settle_value, current_thread, settle_thread) \
    SudekiMpTalosNativeLifecyclePostMovieRestoreReadyClaimPolicy( \
        (ticket_value), (lifecycle_value), (roster_value), (kazel_value), \
        (settle_value), (current_thread), (settle_thread))
#define CHECK_READY_REJECTS(mutation) do { \
    SudekiMpTalosNativeLifecycleSnapshot changed_lifecycle = lifecycle; \
    SudekiMpTalosNativeRosterIdentitySnapshot changed_roster = roster; \
    SudekiMpTalosNativeKazelSnapshot changed_kazel = kazel; \
    SudekiMpTalosNativeSettleEvidenceSnapshot changed_settle = settle; \
    SudekiMpTalosNativePostMovieRestoreTicket changed_ticket = ticket; \
    mutation; \
    CHECK(!READY_POLICY(&changed_ticket, &changed_lifecycle, \
        &changed_roster, &changed_kazel, &changed_settle, 37u, 37u)); \
} while (0)

    make_exact_post_movie_ticket_evidence(
        &lifecycle, &roster, &kazel, &settle);
    ticket = make_exact_post_movie_restore_ticket(
        &lifecycle, &roster, &kazel, &settle);
    CHECK(READY_POLICY(&ticket, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));

    /* This is the normal live post-TSA sequence: a non-default InitCam or
     * SkillCam observation supersedes the current default camera. It may
     * clear all volatile settle flags, but cannot erase the already captured
     * exact historical boundary. */
    ++settle.camera_observation_generation;
    settle.default_camera_generation += 3u;
    settle.void_set_zone_completed = 0u;
    settle.default_camera_committed = 0u;
    settle.default_camera_revalidated = 0u;
    settle.tal_control_revalidated = 0u;
    settle.settle_evidence_complete = 0u;
    CHECK(READY_POLICY(&ticket, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));
    {
        uint8_t state = SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY;
        BOOL ready_after_camera = READY_POLICY(
            &ticket, &lifecycle, &roster, &kazel, &settle, 37u, 37u);

        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state, ready_after_camera, FALSE, &state) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED);
        CHECK(state == SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED);
        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state, TRUE, FALSE, &state) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_REPLAY);
    }

    CHECK_READY_REJECTS(changed_lifecycle.installed = 0u);
    CHECK_READY_REJECTS(changed_lifecycle.run_id_low ^= 1u);
    CHECK_READY_REJECTS(changed_lifecycle.event_serial += 1u);
    CHECK_READY_REJECTS(
        changed_lifecycle.script_runtime_generation += 1u);
    CHECK_READY_REJECTS(
        changed_lifecycle.load_void_task_generation += 1u);
    CHECK_READY_REJECTS(changed_lifecycle.opcode_27_depth = 1u);
    CHECK_READY_REJECTS(changed_roster.observation_serial += 1u);
    CHECK_READY_REJECTS(changed_roster.roster_revision += 1u);
    CHECK_READY_REJECTS(changed_roster.quarantine_reason = 1u);
    CHECK_READY_REJECTS(changed_roster.hero_present_mask = 0x03u);
    CHECK_READY_REJECTS(changed_roster.hero_token[
        SUDEKIMP_TALOS_NATIVE_HERO_TAL] ^= UINT64_C(1));
    CHECK_READY_REJECTS(changed_kazel.observation_serial += 1u);
    CHECK_READY_REJECTS(changed_kazel.session_generation += 1u);
    CHECK_READY_REJECTS(changed_kazel.state =
        SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED);
    CHECK_READY_REJECTS(changed_kazel.ambiguity_reason = 1u);
    CHECK_READY_REJECTS(changed_kazel.kazel_token ^= UINT64_C(1));
    CHECK_READY_REJECTS(changed_settle.session_generation += 1u);
    CHECK_READY_REJECTS(changed_settle.script_runtime_generation += 1u);
    CHECK_READY_REJECTS(changed_settle.load_void_task_generation += 1u);
    CHECK_READY_REJECTS(
        changed_settle.settle_validation_generation += 1u);
    CHECK_READY_REJECTS(changed_ticket.authorization_generation = 0u);
    CHECK(!READY_POLICY(&ticket, &lifecycle, &roster, &kazel, &settle,
        38u, 37u));
    CHECK(!READY_POLICY(&ticket, &lifecycle, &roster, &kazel, &settle,
        37u, 38u));
    CHECK(!READY_POLICY(NULL, &lifecycle, &roster, &kazel, &settle,
        37u, 37u));

#undef CHECK_READY_REJECTS
#undef READY_POLICY
}

static void test_post_movie_ticket_claim_state_is_one_shot(void) {
    unsigned int state;
    unsigned int exact;
    unsigned int mismatch;

    for (state = 0u; state <= 0xffu; ++state) {
        for (exact = 0u; exact < 2u; ++exact) {
            for (mismatch = 0u; mismatch < 2u; ++mismatch) {
                SudekiMpTalosNativePostMovieTicketClaimResult result;
                SudekiMpTalosNativePostMovieTicketClaimResult expected;
                uint8_t next_state = 0xa5u;
                uint8_t expected_state = (uint8_t)state;

                if (state ==
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_DISABLED) {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_DISABLED;
                } else if (state ==
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED) {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_REPLAY;
                } else if (state ==
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED) {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED;
                } else if (state !=
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING &&
                    state !=
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE &&
                    state !=
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY) {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED;
                    expected_state =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
                } else if (mismatch != 0u ||
                    (state ==
                         SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY &&
                     exact == 0u) ||
                    (state ==
                         SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING &&
                     exact != 0u)) {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED;
                    expected_state =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
                } else if (exact == 0u || state ==
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE) {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_NOT_READY;
                } else {
                    expected =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED;
                    expected_state =
                        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED;
                }
                result =
                    SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
                        (uint8_t)state, exact != 0u, mismatch != 0u,
                        &next_state);
                CHECK(result == expected);
                CHECK(next_state == expected_state);
            }
        }
    }

    CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE,
        TRUE, FALSE, NULL) ==
        SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_INVALID);
    {
        uint8_t state_value =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE;

        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state_value, FALSE, FALSE, &state_value) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_NOT_READY);
        CHECK(state_value ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE);
        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state_value, TRUE, FALSE, &state_value) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_NOT_READY);
        CHECK(state_value ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE);
        state_value = SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY;
        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state_value, TRUE, FALSE, &state_value) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED);
        CHECK(state_value ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED);
        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state_value, TRUE, FALSE, &state_value) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_REPLAY);
    }
    {
        uint8_t state_value =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE;

        /* A runtime/bytecode replacement after the first exact LoadVoid is
         * an irreversible provenance break, not another pollable NOT_READY. */
        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state_value, FALSE, TRUE, &state_value) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED);
        CHECK(state_value ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED);
        CHECK(SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            state_value, TRUE, FALSE, &state_value) ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED);
    }
}

static void test_kazel_group_add_policy_is_exact(void) {
    unsigned int missing;

#define KAZEL_POLICY(pending, runtime, task, thread, group, dark, gb, ga, fb, fa, tal, added, formation, raw) \
    SudekiMpTalosNativeLifecycleKazelGroupAddEvidencePolicy( \
        (pending), (runtime), (task), (thread), (group), (dark), \
        (gb), (ga), (fb), (fa), (tal), (added), (formation), (raw))
    CHECK(KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 2u, UINT64_C(0x11), UINT64_C(0x22),
        UINT64_C(0x22), UINT64_C(0x22)));
    for (missing = 0u; missing < 6u; ++missing) {
        BOOL evidence[6] = {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE};

        evidence[missing] = FALSE;
        CHECK(!KAZEL_POLICY(
            evidence[0], evidence[1], evidence[2], evidence[3],
            evidence[4], evidence[5], 1u, 2u, 1u, 2u,
            UINT64_C(0x11), UINT64_C(0x22), UINT64_C(0x22),
            UINT64_C(0x22)));
    }
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        0u, 2u, 1u, 2u, 0x11u, 0x22u, 0x22u, 0x22u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 3u, 1u, 2u, 0x11u, 0x22u, 0x22u, 0x22u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 0u, 2u, 0x11u, 0x22u, 0x22u, 0x22u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 3u, 0x11u, 0x22u, 0x22u, 0x22u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 2u, 0u, 0x22u, 0x22u, 0x22u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 2u, 0x11u, 0u, 0u, 0u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 2u, 0x11u, 0x11u, 0x11u, 0x11u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 2u, 0x11u, 0x22u, 0x23u, 0x22u));
    CHECK(!KAZEL_POLICY(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        1u, 2u, 1u, 2u, 0x11u, 0x22u, 0x22u, 0x23u));
#undef KAZEL_POLICY
}

static void test_kazel_serialized_sequence_policy_is_exact(void) {
    static const struct KazelSerializedEdge {
        uint8_t current_mask;
        SudekiMpTalosNativeLifecycleEvent event;
        BOOL before;
        uint8_t next_mask;
    } sequence[] = {
        {0u, SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE, TRUE, 1u},
        {1u, SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE, FALSE, 3u},
        {3u, SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER, TRUE, 7u},
        {7u, SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER, FALSE, 15u},
        {15u, SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC, TRUE, 31u},
        {31u, SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC, FALSE, 63u}
    };
    uint8_t current_mask = 0u;
    uint8_t next_mask;
    size_t edge;
    unsigned int mask;
    unsigned int event_value;
    unsigned int before_value;

    for (edge = 0u; edge < sizeof(sequence) / sizeof(sequence[0]); ++edge) {
        CHECK(current_mask == sequence[edge].current_mask);
        CHECK(SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
            current_mask, sequence[edge].event, sequence[edge].before,
            TRUE, &next_mask));
        CHECK(next_mask == sequence[edge].next_mask);
        current_mask = next_mask;
    }
    CHECK(current_mask == 63u);

    /* Across every byte mask, defined event, and edge direction, exactly the
     * six authored transitions above may advance. This also exhaustively
     * rejects skipped, reordered, duplicate, unrelated, and nonexact masks. */
    for (mask = 0u; mask <= 0xffu; ++mask) {
        for (event_value = SUDEKIMP_TALOS_NATIVE_EVENT_NONE;
                event_value <= SUDEKIMP_TALOS_NATIVE_EVENT_KAZEL_GROUP_ADD;
                ++event_value) {
            for (before_value = 0u; before_value < 2u; ++before_value) {
                BOOL expected_accept = FALSE;
                uint8_t expected_next = (uint8_t)mask;
                BOOL accepted;

                for (edge = 0u;
                        edge < sizeof(sequence) / sizeof(sequence[0]);
                        ++edge) {
                    if (mask == sequence[edge].current_mask &&
                            event_value ==
                                (unsigned int)sequence[edge].event &&
                            (before_value != 0u) ==
                                (sequence[edge].before != FALSE)) {
                        expected_accept = TRUE;
                        expected_next = sequence[edge].next_mask;
                        break;
                    }
                }

                next_mask = 0xa5u;
                accepted =
                    SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
                        (uint8_t)mask,
                        (SudekiMpTalosNativeLifecycleEvent)event_value,
                        before_value != 0u, TRUE, &next_mask);
                CHECK(accepted == expected_accept);
                CHECK(next_mask == expected_next);

                next_mask = 0xa5u;
                CHECK(!SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
                    (uint8_t)mask,
                    (SudekiMpTalosNativeLifecycleEvent)event_value,
                    before_value != 0u, FALSE, &next_mask));
                CHECK(next_mask == (uint8_t)mask);
            }
        }
    }

    next_mask = 0xa5u;
    CHECK(!SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
        0u, (SudekiMpTalosNativeLifecycleEvent)(
            SUDEKIMP_TALOS_NATIVE_EVENT_KAZEL_GROUP_ADD + 1),
        TRUE, TRUE, &next_mask));
    CHECK(next_mask == 0u);

    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
        0u, SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE,
        TRUE, TRUE, NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
}

static void test_kazel_session_start_policy_is_process_one_shot(void) {
    CHECK(SudekiMpTalosNativeLifecycleKazelSessionStartPolicy(0u));
    CHECK(!SudekiMpTalosNativeLifecycleKazelSessionStartPolicy(1u));
    CHECK(!SudekiMpTalosNativeLifecycleKazelSessionStartPolicy(
        UINT32_MAX));
}

static void test_hero_vtable_classifier_is_exact(void) {
    static const SudekiMpTalosNativeHeroIdentity expected[] = {
        SUDEKIMP_TALOS_NATIVE_HERO_TAL,
        SUDEKIMP_TALOS_NATIVE_HERO_AILISH,
        SUDEKIMP_TALOS_NATIVE_HERO_BUKI,
        SUDEKIMP_TALOS_NATIVE_HERO_ELCO
    };
    size_t hero;

    for (hero = 0u; hero < sizeof(expected) / sizeof(expected[0]); ++hero) {
        const HeroIdentityFixture *fixture = &hero_identity_fixtures[hero];

        CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
            fixture->main_vtable_rva, fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva) == expected[hero]);
        CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
            fixture->main_vtable_rva ^ 1u, fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva) ==
            SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN);
        CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
            fixture->main_vtable_rva,
            fixture->secondary_vtable_rva ^ 1u,
            fixture->resource_vtable_rva) ==
            SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN);
        CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
            fixture->main_vtable_rva, fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva ^ 1u) ==
            SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN);
    }
    CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
        hero_identity_fixtures[0].main_vtable_rva,
        hero_identity_fixtures[1].secondary_vtable_rva,
        hero_identity_fixtures[0].resource_vtable_rva) ==
        SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN);
    CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(0u, 0u, 0u) ==
        SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN);
    CHECK(SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
        UINT32_MAX, UINT32_MAX, UINT32_MAX) ==
        SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN);
    CHECK(SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
        hero_identity_fixtures[4].main_vtable_rva,
        hero_identity_fixtures[4].secondary_vtable_rva,
        hero_identity_fixtures[4].resource_vtable_rva));
    CHECK(!SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
        hero_identity_fixtures[4].main_vtable_rva ^ 1u,
        hero_identity_fixtures[4].secondary_vtable_rva,
        hero_identity_fixtures[4].resource_vtable_rva));
    CHECK(!SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
        hero_identity_fixtures[4].main_vtable_rva,
        hero_identity_fixtures[4].secondary_vtable_rva ^ 1u,
        hero_identity_fixtures[4].resource_vtable_rva));
    CHECK(!SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
        hero_identity_fixtures[4].main_vtable_rva,
        hero_identity_fixtures[4].secondary_vtable_rva,
        hero_identity_fixtures[4].resource_vtable_rva ^ 1u));
    CHECK(!SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
        hero_identity_fixtures[0].main_vtable_rva,
        hero_identity_fixtures[0].secondary_vtable_rva,
        hero_identity_fixtures[0].resource_vtable_rva));
}

static void test_roster_identity_snapshot_is_inert_when_uninstalled(void) {
    SudekiMpTalosNativeRosterIdentitySnapshot snapshot;
    static const uint8_t zero_snapshot[
        sizeof(SudekiMpTalosNativeRosterIdentitySnapshot)] = {0};

    SudekiMpUninstallTalosNativeLifecycleTrace();
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleGetRosterIdentitySnapshot(NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    memset(&snapshot, 0xff, sizeof(snapshot));
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleGetRosterIdentitySnapshot(&snapshot));
    CHECK(GetLastError() == ERROR_NOT_READY);
    CHECK(memcmp(&snapshot, zero_snapshot, sizeof(snapshot)) == 0);
}

static void test_settle_snapshot_is_inert_when_uninstalled(void) {
    SudekiMpTalosNativeSettleEvidenceSnapshot snapshot;
    static const uint8_t zero_snapshot[
        sizeof(SudekiMpTalosNativeSettleEvidenceSnapshot)] = {0};

    SudekiMpUninstallTalosNativeLifecycleTrace();
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleGetSettleEvidenceSnapshot(NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    memset(&snapshot, 0xff, sizeof(snapshot));
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleGetSettleEvidenceSnapshot(&snapshot));
    CHECK(GetLastError() == ERROR_NOT_READY);
    CHECK(memcmp(&snapshot, zero_snapshot, sizeof(snapshot)) == 0);
}

static void test_kazel_snapshot_is_inert_when_uninstalled(void) {
    SudekiMpTalosNativeKazelSnapshot snapshot;
    static const uint8_t zero_snapshot[
        sizeof(SudekiMpTalosNativeKazelSnapshot)] = {0};

    SudekiMpUninstallTalosNativeLifecycleTrace();
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleGetKazelSnapshot(NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    memset(&snapshot, 0xff, sizeof(snapshot));
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleGetKazelSnapshot(&snapshot));
    CHECK(GetLastError() == ERROR_NOT_READY);
    CHECK(memcmp(&snapshot, zero_snapshot, sizeof(snapshot)) == 0);
}

static void test_disabled_install_is_inert(void) {
    SudekiMpTalosNativeLifecycleSnapshot snapshot;

    CHECK(SudekiMpInstallTalosNativeLifecycleTrace(NULL, FALSE));
    CHECK(!SudekiMpTalosNativeLifecycleGetSnapshot(&snapshot));
    CHECK(snapshot.installed == 0u);
    CHECK(snapshot.mutation_supported == 0u);
    SudekiMpUninstallTalosNativeLifecycleTrace();
}

static void test_install_restore_and_callbacks_are_inert(void) {
    uint8_t *image = make_exact_image();
    void **call_slot = (void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT);
    void **scene_slot = (void **)(image + RVA_SCRIPT_SCENE_OPCODE_SLOT);
    void *native_call = image + RVA_SCRIPT_CALL_OPCODE;
    void *native_scene = image + RVA_SCRIPT_SCENE_OPCODE;
    uint8_t saved_constructor_call[5];
    uint8_t saved_kazel_group_add_window[17];
    uint8_t saved_delete_pc[sizeof(delete_pc_signature)];
    uint8_t saved_remove_all_players[5];
    uint8_t saved_formation_pop[5];
    uint8_t saved_tsa_set_playing[0x60];
    SudekiMpTalosNativeLifecycleSnapshot snapshot;
    SudekiMpTalosNativeSettleEvidenceSnapshot settle_snapshot;
    SudekiMpTalosNativeKazelSnapshot kazel_snapshot;
    SudekiMpTalosNativePostMovieRestoreTicket restore_ticket;
    static const uint8_t zero_ticket[
        sizeof(SudekiMpTalosNativePostMovieRestoreTicket)] = {0};

    memcpy(saved_constructor_call,
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
        sizeof(saved_constructor_call));
    memcpy(saved_kazel_group_add_window,
        image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        sizeof(saved_kazel_group_add_window));
    memcpy(saved_delete_pc, image + RVA_DELETE_PC, sizeof(saved_delete_pc));
    memcpy(saved_remove_all_players, image + RVA_REMOVE_ALL_PLAYERS,
        sizeof(saved_remove_all_players));
    memcpy(saved_formation_pop, image + RVA_FORMATION_POP_MEMBERS,
        sizeof(saved_formation_pop));
    memcpy(saved_tsa_set_playing, image + RVA_TSA_SET_PLAYING,
        sizeof(saved_tsa_set_playing));
    CHECK(relative_call_target(
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL) ==
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR);
    CHECK(relative_call_target(image + RVA_KAZEL_GROUP_ADD_CALL) ==
        image + RVA_RAW_GROUP_ADD);
    CHECK(relative_call_target(image + RVA_TSA_SET_PLAYING + 0x4fu) ==
        image + RVA_TSA_DISPATCH);
    CHECK(SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(*call_slot != native_call);
    CHECK(*scene_slot != native_scene);
    CHECK(relative_call_target(
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL) !=
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR);
    CHECK(relative_call_target(image + RVA_KAZEL_GROUP_ADD_CALL) !=
        image + RVA_RAW_GROUP_ADD);
    CHECK(memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        saved_kazel_group_add_window, 11u) == 0);
    CHECK(memcmp(image + RVA_KAZEL_GROUP_ADD_CALL + 5u,
        saved_kazel_group_add_window + 15u, 2u) == 0);
    CHECK(image[RVA_DELETE_PC] == 0xe9u);
    CHECK(image[RVA_REMOVE_ALL_PLAYERS] == 0xe9u);
    CHECK(image[RVA_FORMATION_POP_MEMBERS] == 0xe9u);
    CHECK(image[RVA_TSA_SET_PLAYING] == 0xe9u);
    CHECK(memcmp(image + RVA_TSA_SET_PLAYING + 5u,
        saved_tsa_set_playing + 5u,
        sizeof(saved_tsa_set_playing) - 5u) == 0);
    CHECK(SudekiMpTalosNativeLifecycleGetSnapshot(&snapshot));
    CHECK(snapshot.installed == 1u);
    CHECK(snapshot.native_passthrough_required == 1u);
    CHECK(snapshot.mutation_supported == 0u);
    SetLastError(1234u);
    SudekiMpTalosNativeLifecycleObserveSetZoneNowBefore("Void");
    CHECK(GetLastError() == 1234u);
    SudekiMpTalosNativeLifecycleObserveSetZoneNowAfter("Void");
    CHECK(GetLastError() == 1234u);
    SudekiMpTalosNativeLifecycleObserveRenderCameraAfter(
        image + 0x100u, "default", TRUE);
    CHECK(GetLastError() == 1234u);
    CHECK(SudekiMpTalosNativeLifecycleGetSnapshot(&snapshot));
    CHECK(snapshot.event_serial == 0u);
    CHECK(snapshot.set_zone_before_seen == 0u);
    CHECK(SudekiMpTalosNativeLifecycleGetSettleEvidenceSnapshot(
        &settle_snapshot));
    CHECK(settle_snapshot.session_generation == 0u);
    CHECK(!settle_snapshot.void_set_zone_completed);
    CHECK(!settle_snapshot.default_camera_committed);
    CHECK(!settle_snapshot.settle_evidence_complete);
    CHECK(SudekiMpTalosNativeLifecycleGetKazelSnapshot(&kazel_snapshot));
    CHECK(kazel_snapshot.session_generation == 0u);
    CHECK(kazel_snapshot.observation_serial == 0u);
    CHECK(kazel_snapshot.state == SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE);
    CHECK(!kazel_snapshot.actor_lifetime_authority_proven);
    memset(&restore_ticket, 0xff, sizeof(restore_ticket));
    SetLastError(4321u);
    CHECK(!SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(
        &restore_ticket));
    CHECK(GetLastError() == 4321u);
    CHECK(memcmp(&restore_ticket, zero_ticket,
        sizeof(restore_ticket)) == 0);

    SudekiMpUninstallTalosNativeLifecycleTrace();
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    CHECK(memcmp(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
        saved_constructor_call, sizeof(saved_constructor_call)) == 0);
    CHECK(memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        saved_kazel_group_add_window,
        sizeof(saved_kazel_group_add_window)) == 0);
    CHECK(memcmp(image + RVA_DELETE_PC, saved_delete_pc,
        sizeof(saved_delete_pc)) == 0);
    CHECK(memcmp(image + RVA_REMOVE_ALL_PLAYERS, saved_remove_all_players,
        sizeof(saved_remove_all_players)) == 0);
    CHECK(memcmp(image + RVA_FORMATION_POP_MEMBERS, saved_formation_pop,
        sizeof(saved_formation_pop)) == 0);
    CHECK(memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing,
        sizeof(saved_tsa_set_playing)) == 0);
    CHECK(!SudekiMpTalosNativeLifecycleGetSnapshot(&snapshot));
    CHECK(snapshot.installed == 0u);

    CHECK(SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    SudekiMpUninstallTalosNativeLifecycleTrace();
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    CHECK(memcmp(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
        saved_constructor_call, sizeof(saved_constructor_call)) == 0);
    CHECK(memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        saved_kazel_group_add_window,
        sizeof(saved_kazel_group_add_window)) == 0);
    CHECK(memcmp(image + RVA_DELETE_PC, saved_delete_pc,
        sizeof(saved_delete_pc)) == 0);
    CHECK(memcmp(image + RVA_REMOVE_ALL_PLAYERS, saved_remove_all_players,
        sizeof(saved_remove_all_players)) == 0);
    CHECK(memcmp(image + RVA_FORMATION_POP_MEMBERS, saved_formation_pop,
        sizeof(saved_formation_pop)) == 0);
    CHECK(memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing,
        sizeof(saved_tsa_set_playing)) == 0);
    CHECK(VirtualFree(image, 0u, MEM_RELEASE));
}

static void test_post_movie_ticket_startup_gates_and_teardown_reset(void) {
    uint8_t *image = make_exact_image();
    SudekiMpTalosNativePostMovieRestoreTicket ticket;
    static const uint8_t zero_ticket[
        sizeof(SudekiMpTalosNativePostMovieRestoreTicket)] = {0};
    unsigned int attempt;

    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
        (HMODULE)image, TRUE, TRUE, FALSE));
    for (attempt = 0u; attempt < 2u; ++attempt) {
        memset(&ticket, 0xff, sizeof(ticket));
        SetLastError(5000u + attempt);
        CHECK(!SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(
            &ticket));
        CHECK(GetLastError() == 5000u + attempt);
        CHECK(memcmp(&ticket, zero_ticket, sizeof(ticket)) == 0);
    }
    SudekiMpUninstallTalosNativeLifecycleTrace();

    /* Exact authentication enables the coordinator, but incomplete evidence
     * remains non-consuming across repeated controller-update polls. */
    CHECK(SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
        (HMODULE)image, TRUE, TRUE, TRUE));
    for (attempt = 0u; attempt < 2u; ++attempt) {
        memset(&ticket, 0xff, sizeof(ticket));
        SetLastError(6000u + attempt);
        CHECK(!SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(
            &ticket));
        CHECK(GetLastError() == 6000u + attempt);
        CHECK(memcmp(&ticket, zero_ticket, sizeof(ticket)) == 0);
    }
    SudekiMpUninstallTalosNativeLifecycleTrace();

    /* Teardown owns reset: a new startup may configure the exact gate without
     * inheriting WAITING/READY/terminal state from the prior installation. */
    CHECK(SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
        (HMODULE)image, TRUE, TRUE, TRUE));
    memset(&ticket, 0xff, sizeof(ticket));
    CHECK(!SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(&ticket));
    CHECK(memcmp(&ticket, zero_ticket, sizeof(ticket)) == 0);
    SudekiMpUninstallTalosNativeLifecycleTrace();
    CHECK(VirtualFree(image, 0u, MEM_RELEASE));
}

static void test_signature_and_slot_rejection_are_atomic(void) {
    static const size_t tsa_corrupt_offsets[] = {
        0x01u, 0x1eu, 0x56u, 0x31u, 0x36u, 0x5fu, 0x50u
    };
    static const uint32_t kazel_corrupt_rvas[] = {
        RVA_KAZEL_GROUP_ADD_CALL - 10u,
        RVA_KAZEL_GROUP_ADD_CALL - 5u,
        RVA_KAZEL_GROUP_ADD_CALL + 1u,
        RVA_KAZEL_GROUP_ADD_CALL + 6u,
        RVA_RAW_GROUP_ADD + 16u,
        RVA_RAW_GROUP_ADD + 0x108u,
        RVA_AI_LISTENER_VTABLE + 0x18u,
        RVA_AI_LISTENER_ADD + 19u,
        RVA_AI_LISTENER_FORMATION_ADD_CALL + 1u,
        RVA_AI_LISTENER_ADD + 0x23u,
        RVA_RAW_FORMATION_ADD + 14u,
        RVA_RAW_FORMATION_ADD + 0x87u,
        RVA_RAW_FORMATION_ADD + 0x8eu
    };
    static const uint32_t hero_corrupt_rvas[] = {
        0x002d5010u - 4u,
        0x002fe4ecu + 4u,
        0x002fec24u + 12u,
        0x0035b1d4u + 8u,
        0x002d5054u + 0x10u,
        0x0014d730u + 5u,
        0x002d6884u - 4u,
        0x002ff850u + 4u,
        0x002ff83cu + 12u,
        0x0035b238u + 8u,
        0x002d68c8u + 0x10u,
        0x00151230u + 5u
    };
    uint8_t *image = make_exact_image();
    void **call_slot = (void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT);
    void **scene_slot = (void **)(image + RVA_SCRIPT_SCENE_OPCODE_SLOT);
    void *native_call = image + RVA_SCRIPT_CALL_OPCODE;
    void *native_scene = image + RVA_SCRIPT_SCENE_OPCODE;
    uint8_t saved_window_byte;
    uint8_t saved_native_byte;
    uint8_t saved_constructor_call[5];
    uint8_t saved_kazel_group_add_window[17];
    uint8_t saved_delete_pc[sizeof(delete_pc_signature)];
    uint8_t saved_remove_all_players[5];
    uint8_t saved_formation_pop[5];
    uint8_t saved_tsa_set_playing[0x60];
    size_t index;

    memcpy(saved_constructor_call,
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
        sizeof(saved_constructor_call));
    memcpy(saved_kazel_group_add_window,
        image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        sizeof(saved_kazel_group_add_window));
    memcpy(saved_delete_pc, image + RVA_DELETE_PC, sizeof(saved_delete_pc));
    memcpy(saved_remove_all_players, image + RVA_REMOVE_ALL_PLAYERS,
        sizeof(saved_remove_all_players));
    memcpy(saved_formation_pop, image + RVA_FORMATION_POP_MEMBERS,
        sizeof(saved_formation_pop));
    memcpy(saved_tsa_set_playing, image + RVA_TSA_SET_PLAYING,
        sizeof(saved_tsa_set_playing));

#define CHECK_ALL_NATIVE_SEAMS_UNCHANGED() do { \
    CHECK(*call_slot == native_call); \
    CHECK(*scene_slot == native_scene); \
    CHECK(memcmp(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL, \
        saved_constructor_call, sizeof(saved_constructor_call)) == 0); \
    CHECK(memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u, \
        saved_kazel_group_add_window, \
        sizeof(saved_kazel_group_add_window)) == 0); \
    CHECK(memcmp(image + RVA_RAW_GROUP_ADD, raw_group_add_entry, \
        sizeof(raw_group_add_entry)) == 0); \
    CHECK(memcmp(image + RVA_RAW_GROUP_ADD + 0x108u, \
        "\xc2\x04\x00", 3u) == 0); \
    CHECK(*(uint32_t *)(image + RVA_AI_LISTENER_VTABLE + 0x18u) == \
        (uint32_t)(uintptr_t)(image + RVA_AI_LISTENER_ADD)); \
    CHECK(memcmp(image + RVA_AI_LISTENER_ADD, ai_listener_add_prefix, \
        sizeof(ai_listener_add_prefix)) == 0); \
    CHECK(relative_call_target( \
        image + RVA_AI_LISTENER_FORMATION_ADD_CALL) == \
        image + RVA_RAW_FORMATION_ADD); \
    CHECK(memcmp(image + RVA_AI_LISTENER_ADD + 0x23u, \
        "\xc2\x0c\x00", 3u) == 0); \
    CHECK(memcmp(image + RVA_RAW_FORMATION_ADD, raw_formation_add_entry, \
        sizeof(raw_formation_add_entry)) == 0); \
    CHECK(memcmp(image + RVA_RAW_FORMATION_ADD + 0x87u, \
        "\xc2\x04\x00", 3u) == 0); \
    CHECK(memcmp(image + RVA_RAW_FORMATION_ADD + 0x8eu, \
        "\xc2\x04\x00", 3u) == 0); \
    CHECK(memcmp(image + RVA_DELETE_PC, saved_delete_pc, \
        sizeof(saved_delete_pc)) == 0); \
    CHECK(memcmp(image + RVA_REMOVE_ALL_PLAYERS, saved_remove_all_players, \
        sizeof(saved_remove_all_players)) == 0); \
    CHECK(memcmp(image + RVA_FORMATION_POP_MEMBERS, saved_formation_pop, \
        sizeof(saved_formation_pop)) == 0); \
    CHECK(memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing, \
        sizeof(saved_tsa_set_playing)) == 0); \
    CHECK(hero_identity_fixture_bytes_match(image)); \
} while (0)

    image[RVA_SCRIPT_SCENE_OPCODE +
        sizeof(script_scene_opcode_signature) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    image[RVA_SCRIPT_SCENE_OPCODE +
        sizeof(script_scene_opcode_signature) - 1u] ^= 0x01u;

    saved_window_byte = image[RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL + 5u];
    image[RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL + 5u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    image[RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL + 5u] = saved_window_byte;

    saved_native_byte = image[RVA_DELETE_PC + 7u];
    image[RVA_DELETE_PC + 7u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    image[RVA_DELETE_PC + 7u] = saved_native_byte;

    saved_native_byte = image[RVA_REMOVE_ALL_PLAYERS + 15u];
    image[RVA_REMOVE_ALL_PLAYERS + 15u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    image[RVA_REMOVE_ALL_PLAYERS + 15u] = saved_native_byte;

    saved_native_byte = image[RVA_FORMATION_POP_MEMBERS + 19u];
    image[RVA_FORMATION_POP_MEMBERS + 19u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    image[RVA_FORMATION_POP_MEMBERS + 19u] = saved_native_byte;

    saved_native_byte = image[RVA_TSA_IS_PLAYING + 15u];
    image[RVA_TSA_IS_PLAYING + 15u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == native_scene);
    image[RVA_TSA_IS_PLAYING + 15u] = saved_native_byte;

    for (index = 0u;
            index < sizeof(tsa_corrupt_offsets) /
                sizeof(tsa_corrupt_offsets[0]);
            ++index) {
        size_t offset = tsa_corrupt_offsets[index];
        uint8_t saved_byte = image[RVA_TSA_SET_PLAYING + offset];

        image[RVA_TSA_SET_PLAYING + offset] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        CHECK(!SudekiMpInstallTalosNativeLifecycleTrace(
            (HMODULE)image, TRUE));
        CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
        image[RVA_TSA_SET_PLAYING + offset] = saved_byte;
        CHECK_ALL_NATIVE_SEAMS_UNCHANGED();
    }

    for (index = 0u;
            index < sizeof(kazel_corrupt_rvas) /
                sizeof(kazel_corrupt_rvas[0]);
            ++index) {
        uint32_t rva = kazel_corrupt_rvas[index];
        uint8_t saved_byte = image[rva];

        image[rva] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        CHECK(!SudekiMpInstallTalosNativeLifecycleTrace(
            (HMODULE)image, TRUE));
        CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
        image[rva] = saved_byte;
        CHECK_ALL_NATIVE_SEAMS_UNCHANGED();
    }

    for (index = 0u;
            index < sizeof(hero_corrupt_rvas) /
                sizeof(hero_corrupt_rvas[0]);
            ++index) {
        uint32_t rva = hero_corrupt_rvas[index];
        uint8_t saved_byte = image[rva];

        image[rva] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        CHECK(!SudekiMpInstallTalosNativeLifecycleTrace(
            (HMODULE)image, TRUE));
        CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
        image[rva] = saved_byte;
        CHECK_ALL_NATIVE_SEAMS_UNCHANGED();
    }

    *call_slot = image + RVA_SCRIPT_SCENE_OPCODE;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BUSY);
    CHECK(*call_slot == image + RVA_SCRIPT_SCENE_OPCODE);
    CHECK(*scene_slot == native_scene);
    *call_slot = native_call;

    *scene_slot = image + RVA_SCRIPT_CALL_OPCODE;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE));
    CHECK(GetLastError() == ERROR_BUSY);
    CHECK(*call_slot == native_call);
    CHECK(*scene_slot == image + RVA_SCRIPT_CALL_OPCODE);
    *scene_slot = native_scene;
    CHECK_ALL_NATIVE_SEAMS_UNCHANGED();
    SudekiMpUninstallTalosNativeLifecycleTrace();
#undef CHECK_ALL_NATIVE_SEAMS_UNCHANGED
    CHECK(VirtualFree(image, 0u, MEM_RELEASE));
}

int main(void) {
    test_exact_classifier();
    test_near_misses_are_inert();
    test_delete_resource_policy_uses_native_identifiers();
    test_tsa_inactive_evidence_policy_is_exact();
    test_camera_and_control_settle_policies_are_exact();
    test_post_movie_restore_ticket_policy_is_exact();
    test_ready_ticket_survives_normal_camera_invalidation();
    test_post_movie_ticket_claim_state_is_one_shot();
    test_kazel_group_add_policy_is_exact();
    test_kazel_serialized_sequence_policy_is_exact();
    test_kazel_session_start_policy_is_process_one_shot();
    test_hero_vtable_classifier_is_exact();
    test_roster_identity_snapshot_is_inert_when_uninstalled();
    test_settle_snapshot_is_inert_when_uninstalled();
    test_kazel_snapshot_is_inert_when_uninstalled();
    test_disabled_install_is_inert();
    test_install_restore_and_callbacks_are_inert();
    test_post_movie_ticket_startup_gates_and_teardown_reset();
    test_signature_and_slot_rejection_are_atomic();
    puts("talos_native_lifecycle_trace_test: PASS");
    return 0;
}
