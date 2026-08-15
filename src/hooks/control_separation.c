#include "hooks/control_separation.h"

#include "engine/arbiter_combat_input.h"
#include "engine/camera_target_abi.h"
#include "engine/log.h"
#include "engine/player_combat_context.h"
#include "engine/skill_activation_abi.h"
#include "hooks/call_hook.h"
#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#else
#error "Control separation requires 32-bit GCC thiscall support"
#endif

typedef void (SUDEKIMP_THISCALL *ControllerUpdateFunction)(
    void *controller,
    void *update_data
);
typedef int (SUDEKIMP_THISCALL *CharacterResourceTypeFunction)(
    void *component
);
typedef void (*AiControlFunction)(void *character_pointer);
typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
);
typedef void (SUDEKIMP_THISCALL *ArbiterSetSpeedFunction)(
    void *arbiter,
    float speed,
    float turn_rate
);
typedef void (__stdcall *MovementCameraTransformFunction)(
    void *controller,
    float *output_direction,
    const float *local_direction
);
typedef void (__stdcall *MatrixTargetCreateFunction)(
    void *target_list,
    void **output_target,
    const float *matrix
);
typedef const float *(SUDEKIMP_THISCALL *CameraTargetMatrixFunction)(
    void *target
);
enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_CAMERA_TARGET_LIST_OWNER_GLOBAL = 0x003c2f30u,
    RVA_AI_OVERRIDE_CONTROL = 0x000f60d0u,
    RVA_AI_DEFAULT_CONTROL = 0x000f6100u,
    RVA_MOVEMENT_CAMERA_TRANSFORM = 0x000291a0u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_ARBITER_SET_SPEED = 0x000db070u,
    RVA_ARBITER_COMBAT_INPUT = 0x000db0e0u,
    RVA_CAMERA_TARGET_INSTALL = 0x000e84c0u,
    RVA_MATRIX_TARGET_CREATE = 0x00134fb0u,
    RVA_CAMERA_TARGET_RELEASE = 0x00135340u,
    RVA_GAME_OBJECT_TARGET_VTABLE = 0x002d42ccu,
    RVA_OFFSET_TARGET_VTABLE = 0x002d436cu,
    RVA_MATRIX_TARGET_VTABLE = 0x002d43bcu,
    SUPPORTED_IMAGE_SIZE = 0x0045f000u,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_FIRST_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    BUKI_RESOURCE_TYPE = 0x05u
};

static SudekiMpPointerHook controller_update_vtable_hook;
static ControllerUpdateFunction original_controller_update;
static AiControlFunction ai_override_control;
static AiControlFunction ai_default_control;
static ArbiterMovementFunction arbiter_movement;
static ArbiterSetSpeedFunction arbiter_set_speed;
static MovementCameraTransformFunction movement_camera_transform;
static uint8_t *game_base;
static void *overridden_character;
static UINT selected_virtual_key;
static BOOL hotkey_was_down;
static BOOL second_player_movement_enabled;
static BOOL camera_relative_movement_enabled;
static BOOL separation_guard_enabled;
static float maximum_separation_distance;
static BOOL separation_blocked;
static BOOL separation_data_missing_logged;
static BOOL second_player_weak_attack_enabled;
static UINT weak_attack_virtual_key;
static BOOL weak_attack_was_down;
static BOOL second_player_skills_enabled;
static UINT second_player_skill_virtual_keys[4];
static BOOL second_player_skill_keys_were_down[4];
static BOOL target_trace_enabled;
static DWORD target_trace_last_sample_tick;
static void *target_trace_last_node;
static int target_trace_last_auto_enabled;
static BOOL buki_movement_active;
static int last_movement_x;
static int last_movement_z;
static BOOL input_bridge_enabled;
static float input_bridge_deadzone;
static SudekiMpInputBridgeState input_bridge_state;
static BOOL input_bridge_connected;
static DWORD input_bridge_last_right_stick_log_tick;
static int16_t input_bridge_last_right_x;
static int16_t input_bridge_last_right_y;
static BOOL shared_group_camera_enabled;
static MatrixTargetCreateFunction matrix_target_create;
static void *camera_target_install;
static void *camera_target_release;
static void *group_camera_camera;
static void *group_camera_target;
static void *group_camera_original_targets[2];
static void *group_camera_target_list;
static unsigned int group_camera_last_rejection;

static const uint8_t expected_arbiter_combat_input_entry[] = {
    0x55, 0x8b, 0x6c, 0x24, 0x08, 0x56, 0x57, 0x8b, 0xf8, 0x8b, 0xf1
};
static const uint8_t expected_movement_camera_transform_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x8b, 0x55, 0x08, 0xd9, 0xee
};
static const uint8_t expected_camera_target_install_entry[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x8b, 0x94, 0x9e,
    0xb4, 0x00, 0x00, 0x00
};
static const uint8_t expected_matrix_target_create_entry[] = {
    0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x68, 0x80,
    0x00, 0x00, 0x00
};
static const uint8_t expected_camera_target_release_entry[] = {
    0x53, 0x56, 0x8b, 0x77, 0x04, 0x33, 0xdb, 0x32,
    0xc0
};
static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL game_code_pointer(const void *pointer) {
    return game_base != NULL && pointer != NULL &&
        (const uint8_t *)pointer >= game_base &&
        (const uint8_t *)pointer < game_base + SUPPORTED_IMAGE_SIZE;
}

static void log_group_camera_rejection(
    unsigned int rejection,
    const char *reason,
    void *camera,
    void *controller_target,
    void *second_character,
    void *slot_zero,
    void *slot_one,
    void *target_vtable
) {
    void *slot_one_vtable;

    if (group_camera_last_rejection == rejection) {
        return;
    }
    slot_one_vtable = readable_memory(slot_one, sizeof(void *)) ?
        *(void **)slot_one : NULL;
    group_camera_last_rejection = rejection;
    SudekiMpLogFormat(
        "control_separation event=shared_group_camera phase=reject reason=%s camera=0x%08lx controller_target=0x%08lx second_character=0x%08lx slot_zero=0x%08lx slot_one=0x%08lx slot_zero_vtable=0x%08lx slot_one_vtable=0x%08lx\r\n",
        reason,
        (unsigned long)(uintptr_t)camera,
        (unsigned long)(uintptr_t)controller_target,
        (unsigned long)(uintptr_t)second_character,
        (unsigned long)(uintptr_t)slot_zero,
        (unsigned long)(uintptr_t)slot_one,
        (unsigned long)(uintptr_t)target_vtable,
        (unsigned long)(uintptr_t)slot_one_vtable
    );
}

static void retain_camera_target(void *target) {
    if (readable_memory(target, 8u)) {
        ++*(uint32_t *)((uint8_t *)target + 4u);
    }
}

static void release_camera_target(void *target) {
    uint32_t *references;

    if (target == NULL || group_camera_target_list == NULL ||
        camera_target_release == NULL || !readable_memory(target, 8u)) {
        return;
    }
    references = (uint32_t *)((uint8_t *)target + 4u);
    if (*references == 0u) {
        return;
    }
    --*references;
    if (*references == 0u) {
        SudekiMpCallCameraTargetRelease(
            group_camera_target_list,
            target,
            camera_target_release
        );
    }
}

static void reset_target_trace_state(void) {
    target_trace_last_sample_tick = 0;
    target_trace_last_node = NULL;
    target_trace_last_auto_enabled = -1;
}

static BOOL character_is_in_active_group(void *wanted_character) {
    uint8_t *group;
    unsigned int index;

    if (game_base == NULL || wanted_character == NULL) {
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group == NULL) {
        return FALSE;
    }
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)slot == wanted_character) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL overridden_character_is_in_active_group(void) {
    return character_is_in_active_group(overridden_character);
}

static uint8_t *current_gameplay_camera(void) {
    uint8_t *mode;
    uint8_t *camera_member;
    uint8_t *camera;

    if (game_base == NULL) {
        return NULL;
    }
    mode = *(uint8_t **)(game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    if (!readable_memory(mode, 0x10u)) {
        return NULL;
    }
    camera_member = *(uint8_t **)(mode + 0x0cu);
    if ((uintptr_t)camera_member < 0x2cu) {
        return NULL;
    }
    camera = camera_member - 0x2cu;
    if (!readable_memory(camera, 0xbcu)) {
        return NULL;
    }
    return camera;
}

static BOOL character_position(void *character, float output[3]) {
    uint8_t *transform;

    if (!readable_memory(character, 0x48u)) {
        return FALSE;
    }
    transform = *(uint8_t **)((uint8_t *)character + 0x44u);
    if (!readable_memory(transform, 0x24u)) {
        return FALSE;
    }
    output[0] = *(float *)(transform + 0x18u);
    output[1] = *(float *)(transform + 0x1cu);
    output[2] = *(float *)(transform + 0x20u);
    return isfinite(output[0]) && isfinite(output[1]) &&
        isfinite(output[2]);
}

static BOOL camera_target_matrix(void *target, float output[16]) {
    void **vtable;
    CameraTargetMatrixFunction get_matrix;
    const float *matrix;

    if (!readable_memory(target, 0x24u)) {
        return FALSE;
    }
    vtable = *(void ***)target;
    if (!readable_memory(vtable, 0x24u)) {
        return FALSE;
    }
    get_matrix = (CameraTargetMatrixFunction)vtable[8];
    if (!game_code_pointer((const void *)get_matrix)) {
        return FALSE;
    }
    matrix = get_matrix(target);
    if (!readable_memory(matrix, sizeof(float) * 16u)) {
        return FALSE;
    }
    memcpy(output, matrix, sizeof(float) * 16u);
    return TRUE;
}

static BOOL update_group_camera_target(
    void *controller_target,
    void *second_character
) {
    float first_position[3];
    float second_position[3];
    float matrix[16];
    uint8_t *target = (uint8_t *)group_camera_target;

    if (!readable_memory(target, 0x80u) ||
        *(void **)target != game_base + RVA_MATRIX_TARGET_VTABLE ||
        !camera_target_matrix(group_camera_original_targets[0], matrix) ||
        !character_position(controller_target, first_position) ||
        !character_position(second_character, second_position)) {
        return FALSE;
    }
    matrix[12] +=
        (first_position[0] + second_position[0]) * 0.5f -
        first_position[0];
    matrix[13] +=
        (first_position[1] + second_position[1]) * 0.5f -
        first_position[1];
    matrix[14] +=
        (first_position[2] + second_position[2]) * 0.5f -
        first_position[2];
    matrix[15] = 1.0f;
    memcpy(target + 0x20u, matrix, sizeof(matrix));
    memcpy(target + 0x14u, matrix + 12, sizeof(float) * 3u);
    return TRUE;
}

static void restore_group_camera(const char *reason) {
    uint8_t *camera = (uint8_t *)group_camera_camera;
    void *target = group_camera_target;
    unsigned int slot;

    if (target == NULL) {
        return;
    }
    if (readable_memory(camera, 0xbcu) && camera_target_install != NULL) {
        for (slot = 0; slot < 2u; ++slot) {
            void **camera_slot = (void **)(
                camera + 0xb4u + slot * sizeof(void *)
            );
            if (*camera_slot == target) {
                retain_camera_target(group_camera_original_targets[slot]);
                SudekiMpCallCameraTargetInstall(
                    camera,
                    group_camera_original_targets[slot],
                    slot,
                    camera_target_install
                );
            }
        }
    }
    release_camera_target(group_camera_original_targets[0]);
    release_camera_target(group_camera_original_targets[1]);
    release_camera_target(target);
    SudekiMpLogFormat(
        "control_separation event=shared_group_camera phase=restore reason=%s\r\n",
        reason == NULL ? "unspecified" : reason
    );
    group_camera_camera = NULL;
    group_camera_target = NULL;
    group_camera_original_targets[0] = NULL;
    group_camera_original_targets[1] = NULL;
    group_camera_target_list = NULL;
}

static BOOL acquire_group_camera(
    uint8_t *camera,
    void *controller_target,
    void *second_character
) {
    uint8_t *target_list_owner;
    void *created_target = NULL;
    float matrix[16];
    float first_position[3];
    float second_position[3];
    void *slot_zero = *(void **)(camera + 0xb4u);
    void *slot_one = *(void **)(camera + 0xb8u);
    void *target_vtable = readable_memory(slot_zero, sizeof(void *)) ?
        *(void **)slot_zero : NULL;
    void *slot_one_vtable = readable_memory(slot_one, sizeof(void *)) ?
        *(void **)slot_one : NULL;
    BOOL same_game_object_target =
        slot_zero == slot_one &&
        target_vtable == game_base + RVA_GAME_OBJECT_TARGET_VTABLE;
    BOOL offset_and_game_object_targets =
        slot_zero != slot_one &&
        target_vtable == game_base + RVA_OFFSET_TARGET_VTABLE &&
        slot_one_vtable == game_base + RVA_GAME_OBJECT_TARGET_VTABLE;

    if (!same_game_object_target && !offset_and_game_object_targets) {
        log_group_camera_rejection(
            5u,
            "unsupported_target_pair",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    if (!readable_memory(slot_zero, 0x34u) ||
        !readable_memory(slot_one, 0x34u)) {
        log_group_camera_rejection(
            6u,
            "target_unreadable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    if (!camera_target_matrix(slot_zero, matrix)) {
        log_group_camera_rejection(
            8u,
            "target_matrix_unavailable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    if (!character_position(controller_target, first_position) ||
        !character_position(second_character, second_position)) {
        log_group_camera_rejection(
            9u,
            "character_position_unavailable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    target_list_owner = *(uint8_t **)(
        game_base + RVA_CAMERA_TARGET_LIST_OWNER_GLOBAL
    );
    if (!readable_memory(target_list_owner, 0x58u)) {
        log_group_camera_rejection(
            10u,
            "target_list_owner_unavailable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    matrix[12] +=
        (first_position[0] + second_position[0]) * 0.5f -
        first_position[0];
    matrix[13] +=
        (first_position[1] + second_position[1]) * 0.5f -
        first_position[1];
    matrix[14] +=
        (first_position[2] + second_position[2]) * 0.5f -
        first_position[2];
    matrix[15] = 1.0f;
    group_camera_target_list = target_list_owner + 0x4cu;
    matrix_target_create(
        group_camera_target_list,
        &created_target,
        matrix
    );
    if (!readable_memory(created_target, 0x80u) ||
        *(void **)created_target != game_base + RVA_MATRIX_TARGET_VTABLE) {
        log_group_camera_rejection(
            11u,
            "matrix_target_creation_failed",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        release_camera_target(created_target);
        group_camera_target_list = NULL;
        return FALSE;
    }

    group_camera_camera = camera;
    group_camera_target = created_target;
    group_camera_original_targets[0] = slot_zero;
    group_camera_original_targets[1] = slot_one;
    retain_camera_target(slot_zero);
    retain_camera_target(slot_one);
    retain_camera_target(created_target);
    SudekiMpCallCameraTargetInstall(
        camera,
        created_target,
        0u,
        camera_target_install
    );
    retain_camera_target(created_target);
    SudekiMpCallCameraTargetInstall(
        camera,
        created_target,
        1u,
        camera_target_install
    );
    if (*(void **)(camera + 0xb4u) != created_target ||
        *(void **)(camera + 0xb8u) != created_target) {
        restore_group_camera("install_verification_failed");
        return FALSE;
    }
    group_camera_last_rejection = 0u;
    SudekiMpLogFormat(
        "control_separation event=shared_group_camera phase=acquire camera=0x%08lx original_targets=0x%08lx,0x%08lx matrix_target=0x%08lx midpoint_bits=%08lx,%08lx,%08lx policy=two_player_centroid_preserve_native_offset_no_zoom\r\n",
        (unsigned long)(uintptr_t)camera,
        (unsigned long)(uintptr_t)slot_zero,
        (unsigned long)(uintptr_t)slot_one,
        (unsigned long)(uintptr_t)created_target,
        (unsigned long)float_bits(matrix[12]),
        (unsigned long)float_bits(matrix[13]),
        (unsigned long)float_bits(matrix[14])
    );
    return TRUE;
}

static void poll_shared_group_camera(void *controller) {
    uint8_t *camera;
    void *controller_target;

    if (!shared_group_camera_enabled) {
        return;
    }
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248u);
    camera = current_gameplay_camera();
    if (overridden_character == NULL) {
        group_camera_last_rejection = 0u;
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (!overridden_character_is_in_active_group()) {
        log_group_camera_rejection(
            1u, "second_character_not_in_active_group", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (!character_is_in_active_group(controller_target)) {
        log_group_camera_rejection(
            2u, "controller_target_not_in_active_group", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (controller_target == overridden_character) {
        log_group_camera_rejection(
            3u, "controller_target_is_second_character", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (camera == NULL) {
        log_group_camera_rejection(
            4u, "gameplay_camera_unavailable", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (group_camera_target != NULL) {
        if (camera != group_camera_camera) {
            restore_group_camera("camera_changed");
            return;
        }
        if (*(void **)(camera + 0xb4u) != group_camera_target ||
            *(void **)(camera + 0xb8u) != group_camera_target) {
            restore_group_camera("native_target_changed");
            return;
        }
        if (!update_group_camera_target(
                controller_target,
                overridden_character)) {
            restore_group_camera("position_or_target_invalid");
        }
        return;
    }
    acquire_group_camera(
        camera,
        controller_target,
        overridden_character
    );
}

static void stop_buki_movement(void) {
    uint8_t *character = (uint8_t *)overridden_character;
    void *arbiter;

    if (!buki_movement_active || character == NULL) {
        return;
    }
    if (!overridden_character_is_in_active_group()) {
        SudekiMpLogWrite(
            "control_separation event=second_player_movement phase=abort reason=character_not_in_active_group\r\n"
        );
        buki_movement_active = FALSE;
        last_movement_x = 0;
        last_movement_z = 0;
        return;
    }
    arbiter = *(void **)(character + 0x90);
    if (arbiter != NULL) {
        arbiter_set_speed(arbiter, 0.0f, 1.0f);
    }
    SudekiMpLogFormat(
        "control_separation event=second_player_movement phase=stop character=0x%08lx\r\n",
        (unsigned long)(uintptr_t)character
    );
    buki_movement_active = FALSE;
    last_movement_x = 0;
    last_movement_z = 0;
}

static BOOL buki_movement_passes_separation_guard(
    void *controller,
    uint8_t *character,
    const float *direction
) {
    uint8_t *controller_target;
    uint8_t *buki_position;
    uint8_t *target_position;
    float delta_x;
    float delta_z;
    float distance_squared;
    float outward_dot;

    if (!separation_guard_enabled) {
        return TRUE;
    }
    controller_target = controller == NULL ? NULL :
        *(uint8_t **)((uint8_t *)controller + 0x248);
    buki_position = character == NULL ? NULL :
        *(uint8_t **)(character + 0x44);
    target_position = controller_target == NULL ? NULL :
        *(uint8_t **)(controller_target + 0x44);
    if (controller_target == NULL || controller_target == character ||
        buki_position == NULL || target_position == NULL) {
        if (!separation_data_missing_logged) {
            SudekiMpLogWrite(
                "control_separation event=separation_guard phase=abort reason=incomplete_position_state\r\n"
            );
            separation_data_missing_logged = TRUE;
        }
        return FALSE;
    }
    separation_data_missing_logged = FALSE;
    delta_x = *(float *)(buki_position + 0x18) -
        *(float *)(target_position + 0x18);
    delta_z = *(float *)(buki_position + 0x20) -
        *(float *)(target_position + 0x20);
    distance_squared = delta_x * delta_x + delta_z * delta_z;
    outward_dot = delta_x * direction[0] + delta_z * direction[2];
    if (distance_squared >=
            maximum_separation_distance * maximum_separation_distance &&
        outward_dot > 0.0f) {
        if (!separation_blocked) {
            uint32_t distance_bits;
            uint32_t maximum_bits;

            memcpy(&distance_bits, &distance_squared, sizeof(distance_bits));
            memcpy(&maximum_bits, &maximum_separation_distance,
                sizeof(maximum_bits));
            SudekiMpLogFormat(
                "control_separation event=separation_guard phase=block distance_squared_bits=0x%08lx maximum_bits=0x%08lx policy=allow_inward_block_outward\r\n",
                (unsigned long)distance_bits,
                (unsigned long)maximum_bits
            );
        }
        separation_blocked = TRUE;
        return FALSE;
    }
    if (separation_blocked) {
        SudekiMpLogWrite(
            "control_separation event=separation_guard phase=release reason=inward_or_within_limit\r\n"
        );
    }
    separation_blocked = FALSE;
    return TRUE;
}

static void poll_input_bridge(void) {
    BOOL connected;
    DWORD now;
    int delta_x;
    int delta_y;

    if (!input_bridge_enabled) {
        return;
    }
    connected = SudekiMpInputBridgePoll(&input_bridge_state);
    if (!connected) {
        if (input_bridge_connected) {
            weak_attack_was_down = FALSE;
            stop_buki_movement();
        }
        input_bridge_connected = FALSE;
        return;
    }
    input_bridge_connected = TRUE;
    now = GetTickCount();
    delta_x = (int)input_bridge_state.right_x -
        (int)input_bridge_last_right_x;
    delta_y = (int)input_bridge_state.right_y -
        (int)input_bridge_last_right_y;
    if ((delta_x > 4096 || delta_x < -4096 ||
         delta_y > 4096 || delta_y < -4096) &&
        (DWORD)(now - input_bridge_last_right_stick_log_tick) >= 250u) {
        SudekiMpLogFormat(
            "input_bridge event=right_stick_observed right_x=%d right_y=%d policy=captured_not_applied_independent_camera_pending\r\n",
            (int)input_bridge_state.right_x,
            (int)input_bridge_state.right_y
        );
        input_bridge_last_right_stick_log_tick = now;
        input_bridge_last_right_x = input_bridge_state.right_x;
        input_bridge_last_right_y = input_bridge_state.right_y;
    }
}

static BOOL bridge_movement(float *x, float *z, float *speed) {
    float raw_x;
    float raw_z;
    float magnitude;
    float direction_magnitude;
    float scaled_magnitude;

    if (!input_bridge_enabled || !input_bridge_connected) {
        return FALSE;
    }
    raw_x = (float)input_bridge_state.left_x / 32768.0f;
    raw_z = -(float)input_bridge_state.left_y / 32768.0f;
    magnitude = sqrtf(raw_x * raw_x + raw_z * raw_z);
    if (magnitude <= input_bridge_deadzone) {
        *x = 0.0f;
        *z = 0.0f;
        *speed = 0.0f;
        return TRUE;
    }
    direction_magnitude = magnitude;
    if (magnitude > 1.0f) {
        magnitude = 1.0f;
    }
    scaled_magnitude = (magnitude - input_bridge_deadzone) /
        (1.0f - input_bridge_deadzone);
    *x = raw_x / direction_magnitude;
    *z = raw_z / direction_magnitude;
    *speed = scaled_magnitude;
    return TRUE;
}

static void poll_buki_movement(void *controller, BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    void *arbiter;
    void *controller_target;
    int x = 0;
    int z = 0;
    float input_x;
    float input_z;
    float movement_speed;
    BOOL bridge_source;
    float direction[3];
    uint32_t direction_x_bits;
    uint32_t direction_z_bits;

    if (!second_player_movement_enabled || character == NULL) {
        return;
    }
    if (!overridden_character_is_in_active_group()) {
        stop_buki_movement();
        return;
    }
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    arbiter = *(void **)(character + 0x90);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248);
    if (!owns_foreground || component == NULL || mode_state == NULL ||
        arbiter == NULL || *(void **)(character + 0xac) == NULL ||
        *(int16_t *)(component + 0x16a) != 1 ||
        *(mode_state + 0x0b) != 0 || character == controller_target) {
        stop_buki_movement();
        return;
    }

    bridge_source = input_bridge_enabled;
    if (bridge_source) {
        if (!bridge_movement(&input_x, &input_z, &movement_speed)) {
            stop_buki_movement();
            return;
        }
    } else {
        x = ((GetAsyncKeyState('L') & 0x8000) != 0) -
            ((GetAsyncKeyState('J') & 0x8000) != 0);
        z = ((GetAsyncKeyState('I') & 0x8000) != 0) -
            ((GetAsyncKeyState('K') & 0x8000) != 0);
        input_x = (float)x;
        input_z = (float)z;
        movement_speed = 1.0f;
        if (x != 0 && z != 0) {
            input_x *= 0.70710678f;
            input_z *= 0.70710678f;
        }
    }
    if (movement_speed <= 0.0001f) {
        stop_buki_movement();
        return;
    }

    direction[0] = input_x;
    direction[1] = 0.0f;
    direction[2] = input_z;
    if (camera_relative_movement_enabled) {
        float transformed[3] = {0.0f, 0.0f, 0.0f};
        float horizontal_length;

        movement_camera_transform(controller, transformed, direction);
        transformed[1] = 0.0f;
        horizontal_length = sqrtf(
            transformed[0] * transformed[0] +
            transformed[2] * transformed[2]
        );
        if (horizontal_length <= 0.0001f) {
            stop_buki_movement();
            return;
        }
        direction[0] = transformed[0] / horizontal_length;
        direction[1] = 0.0f;
        direction[2] = transformed[2] / horizontal_length;
    }
    if (!buki_movement_passes_separation_guard(
            controller,
            character,
            direction)) {
        stop_buki_movement();
        return;
    }
    memcpy(&direction_x_bits, &direction[0], sizeof(direction_x_bits));
    memcpy(&direction_z_bits, &direction[2], sizeof(direction_z_bits));
    arbiter_movement(arbiter, direction, movement_speed, 1.0f, 0u);
    if (!buki_movement_active ||
        (bridge_source &&
         (((int)input_bridge_state.left_x - last_movement_x > 4096 ||
           (int)input_bridge_state.left_x - last_movement_x < -4096) ||
          ((int)input_bridge_state.left_y - last_movement_z > 4096 ||
           (int)input_bridge_state.left_y - last_movement_z < -4096))) ||
        (!bridge_source && (x != last_movement_x || z != last_movement_z))) {
        SudekiMpLogFormat(
            "control_separation event=second_player_movement phase=submit source=%s character=0x%08lx arbiter=0x%08lx input_x=%d input_z=%d direction_bits=%08lx,00000000,%08lx camera_relative=%s speed_bits=0x%08lx turn_rate_bits=0x3f800000 movement_mode=0\r\n",
            bridge_source ? "external_bridge" : "keyboard",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            bridge_source ? (int)input_bridge_state.left_x : x,
            bridge_source ? (int)input_bridge_state.left_y : z,
            (unsigned long)direction_x_bits,
            (unsigned long)direction_z_bits,
            camera_relative_movement_enabled ? "true" : "false",
            (unsigned long)float_bits(movement_speed)
        );
    }
    buki_movement_active = TRUE;
    last_movement_x = bridge_source ? input_bridge_state.left_x : x;
    last_movement_z = bridge_source ? input_bridge_state.left_y : z;
}

static void poll_buki_weak_attack(void *controller, BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *arbiter;
    void *controller_target;
    BOOL key_is_down;

    if (!second_player_weak_attack_enabled) {
        return;
    }
    key_is_down = input_bridge_enabled ?
        (input_bridge_connected &&
         (input_bridge_state.buttons & SUDEKIMP_BRIDGE_BUTTON_A) != 0u) :
        ((GetAsyncKeyState((int)weak_attack_virtual_key) & 0x8000) != 0);
    if (character == NULL || !overridden_character_is_in_active_group()) {
        weak_attack_was_down = key_is_down;
        return;
    }

    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    arbiter = *(uint8_t **)(character + 0x90);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248);
    if (!owns_foreground || component == NULL || mode_state == NULL ||
        arbiter == NULL || *(void **)(character + 0xac) == NULL ||
        *(int16_t *)(component + 0x16a) != 1 ||
        *(mode_state + 0x0b) != 0 || character == controller_target) {
        weak_attack_was_down = key_is_down;
        return;
    }

    if (key_is_down && !weak_attack_was_down) {
        uint32_t flags_50 = *(uint32_t *)(arbiter + 0x50);
        uint32_t state_58 = *(uint32_t *)(arbiter + 0x58);
        uint8_t flags_60 = *(uint8_t *)(arbiter + 0x60);

        SudekiMpLogFormat(
            "control_separation event=second_player_weak_attack phase=submit character=0x%08lx arbiter=0x%08lx flags_50=0x%08lx state_58=0x%08lx flags_60=0x%02x\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            (unsigned long)flags_50,
            (unsigned long)state_58,
            (unsigned int)flags_60
        );
        SudekiMpSubmitArbiterCombatInput(
            game_base + RVA_ARBITER_COMBAT_INPUT,
            arbiter,
            1,
            0,
            0,
            0,
            0,
            0
        );
    }
    weak_attack_was_down = key_is_down;
}

static void poll_second_player_skills(
    void *controller,
    BOOL owns_foreground
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    void *controller_target;
    unsigned int ordinal;

    if (!second_player_skills_enabled) {
        return;
    }
    component = character == NULL ? NULL :
        *(uint8_t **)(character + 0x94u);
    mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3cu);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248u);
    for (ordinal = 0u; ordinal < 4u; ++ordinal) {
        BOOL key_is_down = (GetAsyncKeyState(
            (int)second_player_skill_virtual_keys[ordinal]
        ) & 0x8000) != 0;

        if (owns_foreground && key_is_down &&
            !second_player_skill_keys_were_down[ordinal]) {
            const char *reason = "invalid_character_state";

            SudekiMpCombatContextsPollGame((HMODULE)game_base);
            if (character != NULL &&
                overridden_character_is_in_active_group() &&
                component != NULL && mode_state != NULL &&
                *(void **)(character + 0x90u) != NULL &&
                *(void **)(character + 0xacu) != NULL &&
                *(int16_t *)(component + 0x16au) == 1 &&
                *(mode_state + 0x0bu) == 0 &&
                character != controller_target &&
                SudekiMpCombatContextCanStartSkill(1u, &reason)) {
                SudekiMpSkillActivationResult result =
                    SudekiMpActivateCharacterQuickSkill(character, ordinal);
                SudekiMpCombatContextsPollGame((HMODULE)game_base);
                SudekiMpLogFormat(
                    "realtime_skill_combat event=player_skill_input player=2 ordinal=%u virtual_key=0x%02lx character=0x%08lx status=%s skill=0x%08lx skill_data=0x%08lx slot=%d validation=%d use=%u\r\n",
                    ordinal,
                    (unsigned long)second_player_skill_virtual_keys[ordinal],
                    (unsigned long)(uintptr_t)character,
                    SudekiMpSkillActivationStatusName(result.status),
                    (unsigned long)(uintptr_t)result.skill,
                    (unsigned long)(uintptr_t)result.skill_data,
                    result.slot,
                    result.validation_result,
                    (unsigned int)result.use_result
                );
            } else {
                SudekiMpLogFormat(
                    "realtime_skill_combat event=player_skill_rejected player=2 ordinal=%u virtual_key=0x%02lx reason=%s policy=fail_safe_native_targeting_serialization\r\n",
                    ordinal,
                    (unsigned long)second_player_skill_virtual_keys[ordinal],
                    reason
                );
            }
        }
        second_player_skill_keys_were_down[ordinal] = key_is_down;
    }
}

static void poll_buki_target_trace(BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *targeter;
    uint8_t *transform;
    void *target_node;
    uint32_t targeter_flags;
    uint32_t forward_bits[3] = {0u, 0u, 0u};
    int auto_enabled;
    DWORD now;

    if (!target_trace_enabled || !owns_foreground || character == NULL ||
        !overridden_character_is_in_active_group()) {
        return;
    }
    now = GetTickCount();
    if ((DWORD)(now - target_trace_last_sample_tick) < 100u) {
        return;
    }
    target_trace_last_sample_tick = now;
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    if (component == NULL || mode_state == NULL ||
        *(int16_t *)(component + 0x16a) != 1 ||
        *(mode_state + 0x0b) != 0) {
        return;
    }
    targeter = *(uint8_t **)(character + 0xac);
    transform = *(uint8_t **)(character + 0x44);
    if (targeter == NULL) {
        return;
    }
    target_node = *(void **)(targeter + 0x54);
    targeter_flags = *(uint32_t *)(targeter + 0x84);
    auto_enabled = (targeter_flags & 2u) != 0;
    if (transform != NULL) {
        memcpy(forward_bits, transform + 0x50, sizeof(forward_bits));
    }
    if (target_node != target_trace_last_node ||
        auto_enabled != target_trace_last_auto_enabled) {
        SudekiMpLogFormat(
            "control_separation event=second_player_target_trace character=0x%08lx targeter=0x%08lx target_node=0x%08lx auto_target_enabled=%d forward_bits=%08lx,%08lx,%08lx\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)targeter,
            (unsigned long)(uintptr_t)target_node,
            auto_enabled,
            (unsigned long)forward_bits[0],
            (unsigned long)forward_bits[1],
            (unsigned long)forward_bits[2]
        );
        target_trace_last_node = target_node;
        target_trace_last_auto_enabled = auto_enabled;
    }
}

static int character_resource_type(uint8_t *character) {
    void *component;
    void **vtable;
    CharacterResourceTypeFunction get_resource_type;

    if (character == NULL) {
        return -1;
    }
    component = character + 0x2c;
    vtable = *(void ***)component;
    if (vtable == NULL) {
        return -1;
    }
    get_resource_type = (CharacterResourceTypeFunction)vtable[4];
    if ((uint8_t *)get_resource_type < game_base ||
        (uint8_t *)get_resource_type >= game_base + SUPPORTED_IMAGE_SIZE) {
        return -1;
    }
    return get_resource_type(component);
}

static uint8_t *find_party_slot(
    uint8_t *group,
    void *wanted_character,
    int wanted_resource_type,
    unsigned int *slot_index
) {
    unsigned int index;

    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        uint8_t *character = *(uint8_t **)slot;
        if ((wanted_character != NULL && character == wanted_character) ||
            (wanted_character == NULL &&
             character_resource_type(character) == wanted_resource_type)) {
            if (slot_index != NULL) {
                *slot_index = index;
            }
            return slot;
        }
    }
    return NULL;
}

static void log_control_state(
    const char *event,
    const char *result,
    uint8_t *slot,
    unsigned int slot_index
) {
    uint8_t *character = slot == NULL ? NULL : *(uint8_t **)slot;
    uint8_t *component = character == NULL ? NULL :
        *(uint8_t **)(character + 0x94);
    uint8_t *mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3c);
    int control_ref = component == NULL ? -1 :
        (int)*(int16_t *)(component + 0x16a);
    int ai_enabled = mode_state == NULL ? -1 : (int)*(mode_state + 0x0b);

    SudekiMpLogFormat(
        "control_separation event=%s result=%s slot=%u character=0x%08lx component=0x%08lx control_ref_16a=%d ai_enabled_3c_0b=%d\r\n",
        event,
        result,
        slot_index,
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)component,
        control_ref,
        ai_enabled
    );
}

static void toggle_buki_ai(void) {
    uint8_t *group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    uint8_t *controller = *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL
    );
    void *controller_target = controller == NULL ? NULL :
        *(void **)(controller + 0x248);
    uint8_t *slot;
    uint8_t *character;
    uint8_t *component;
    uint8_t *mode_state;
    unsigned int slot_index = 0;
    int before_ref;
    int before_mode;
    int after_ref;
    int after_mode;

    if (group == NULL) {
        SudekiMpLogWrite(
            "control_separation event=toggle_abort reason=no_active_group\r\n"
        );
        return;
    }

    if (overridden_character == NULL) {
        slot = find_party_slot(group, NULL, BUKI_RESOURCE_TYPE, &slot_index);
        if (slot == NULL) {
            SudekiMpLogWrite(
                "control_separation event=toggle_abort reason=buki_not_in_party\r\n"
            );
            return;
        }
        character = *(uint8_t **)slot;
        if (slot_index == 0u || character == controller_target) {
            SudekiMpLogWrite(
                "control_separation event=toggle_abort reason=buki_is_front_character\r\n"
            );
            return;
        }
        component = *(uint8_t **)(character + 0x94);
        mode_state = component == NULL ? NULL :
            *(uint8_t **)(component + 0x3c);
        if (component == NULL || mode_state == NULL) {
            SudekiMpLogWrite(
                "control_separation event=toggle_abort reason=incomplete_ai_component\r\n"
            );
            return;
        }
        before_ref = (int)*(int16_t *)(component + 0x16a);
        before_mode = (int)*(mode_state + 0x0b);
        if (before_ref != 0 || before_mode != 1) {
            log_control_state("override_abort", "unexpected_initial_state",
                slot, slot_index);
            return;
        }

        ai_override_control(slot);
        after_ref = (int)*(int16_t *)(component + 0x16a);
        after_mode = (int)*(mode_state + 0x0b);
        if (after_ref == before_ref + 1 && after_mode == 0) {
            overridden_character = character;
            reset_target_trace_state();
            log_control_state("override", "success", slot, slot_index);
        } else {
            log_control_state("override", "verification_failed", slot,
                slot_index);
            if (after_ref > before_ref) {
                ai_default_control(slot);
                log_control_state("override_rollback", "requested", slot,
                    slot_index);
            }
        }
        return;
    }

    slot = find_party_slot(group, overridden_character, -1, &slot_index);
    if (slot == NULL) {
        SudekiMpLogWrite(
            "control_separation event=restore_abort reason=character_not_in_party\r\n"
        );
        return;
    }
    character = *(uint8_t **)slot;
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    if (component == NULL || mode_state == NULL) {
        SudekiMpLogWrite(
            "control_separation event=restore_abort reason=incomplete_ai_component\r\n"
        );
        return;
    }
    before_ref = (int)*(int16_t *)(component + 0x16a);
    before_mode = (int)*(mode_state + 0x0b);
    if (before_ref != 1) {
        log_control_state("restore_abort", "unexpected_override_count", slot,
            slot_index);
        return;
    }

    stop_buki_movement();
    ai_default_control(slot);
    after_ref = (int)*(int16_t *)(component + 0x16a);
    after_mode = (int)*(mode_state + 0x0b);
    if (after_ref == 0 &&
        ((character == controller_target && after_mode == 0) ||
         (character != controller_target && after_mode == 1))) {
        log_control_state("restore", "success", slot, slot_index);
        overridden_character = NULL;
        reset_target_trace_state();
    } else {
        log_control_state("restore", "verification_failed", slot, slot_index);
        if (after_ref == 0) {
            overridden_character = NULL;
        }
    }
}

static void SUDEKIMP_THISCALL poll_control_separation_hotkey(
    void *controller,
    void *update_data
) {
    HWND foreground;
    DWORD foreground_process_id = 0;
    BOOL hotkey_is_down;
    BOOL owns_foreground;

    original_controller_update(controller, update_data);
    poll_input_bridge();
    SudekiMpCombatContextSetCharacter(
        0u,
        controller == NULL ? NULL :
            *(void **)((uint8_t *)controller + 0x248u)
    );
    SudekiMpCombatContextSetCharacter(1u, overridden_character);
    SudekiMpCombatContextSetInputSource(
        0u,
        controller == NULL ? SUDEKIMP_COMBAT_INPUT_NONE :
            SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER,
        controller
    );
    SudekiMpCombatContextSetInputSource(
        1u,
        overridden_character == NULL ? SUDEKIMP_COMBAT_INPUT_NONE :
            (input_bridge_enabled ?
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE :
                SUDEKIMP_COMBAT_INPUT_KEYBOARD_PROTOTYPE),
        overridden_character == NULL ? NULL :
            (input_bridge_enabled ?
                (void *)SudekiMpInputBridgeIdentity() :
                (void *)second_player_skill_virtual_keys)
    );
    SudekiMpCombatContextsPollGame((HMODULE)game_base);
    hotkey_is_down =
        (GetAsyncKeyState((int)selected_virtual_key) & 0x8000) != 0;
    foreground = GetForegroundWindow();
    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &foreground_process_id);
    }
    owns_foreground = foreground_process_id == GetCurrentProcessId();
    if (owns_foreground &&
        hotkey_is_down && !hotkey_was_down) {
        toggle_buki_ai();
    }
    hotkey_was_down = hotkey_is_down;
    poll_buki_movement(controller, owns_foreground);
    poll_buki_weak_attack(controller, owns_foreground);
    poll_second_player_skills(controller, owns_foreground);
    poll_buki_target_trace(owns_foreground);
    poll_shared_group_camera(controller);
}

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT toggle_virtual_key,
    BOOL enable_second_player_movement,
    BOOL enable_camera_relative_movement,
    BOOL enable_separation_guard,
    float maximum_separation,
    BOOL enable_second_player_weak_attack,
    UINT attack_virtual_key,
    BOOL enable_second_player_skills,
    const UINT skill_virtual_keys[4],
    BOOL enable_target_trace,
    BOOL enable_shared_group_camera,
    BOOL enable_input_bridge,
    float bridge_deadzone
) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL || toggle_virtual_key == 0u ||
        toggle_virtual_key > 0xffu ||
        (enable_second_player_weak_attack &&
         (attack_virtual_key == 0u || attack_virtual_key > 0xffu)) ||
        (enable_second_player_skills && skill_virtual_keys == NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_second_player_skills) {
        unsigned int index;
        for (index = 0u; index < 4u; ++index) {
            if (skill_virtual_keys[index] == 0u ||
                skill_virtual_keys[index] > 0xffu) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
        }
    }
    base = (uint8_t *)game_module;
    if (enable_camera_relative_movement && !enable_second_player_movement) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_input_bridge &&
        (!enable_second_player_movement || bridge_deadzone < 0.0f ||
         bridge_deadzone >= 0.95f ||
         SudekiMpInputBridgeIdentity() == NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_separation_guard &&
        (!enable_second_player_movement || maximum_separation <= 0.0f ||
         maximum_separation > 1000.0f)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_camera_relative_movement &&
        memcmp(
            base + RVA_MOVEMENT_CAMERA_TRANSFORM,
            expected_movement_camera_transform_entry,
            sizeof(expected_movement_camera_transform_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_shared_group_camera &&
        (memcmp(
            base + RVA_CAMERA_TARGET_INSTALL,
            expected_camera_target_install_entry,
            sizeof(expected_camera_target_install_entry)) != 0 ||
         memcmp(
            base + RVA_MATRIX_TARGET_CREATE,
            expected_matrix_target_create_entry,
            sizeof(expected_matrix_target_create_entry)) != 0 ||
         memcmp(
            base + RVA_CAMERA_TARGET_RELEASE,
            expected_camera_target_release_entry,
            sizeof(expected_camera_target_release_entry)) != 0)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_second_player_weak_attack &&
        memcmp(
            base + RVA_ARBITER_COMBAT_INPUT,
            expected_arbiter_combat_input_entry,
            sizeof(expected_arbiter_combat_input_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    slot = (void **)(base + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
    game_base = base;
    selected_virtual_key = toggle_virtual_key;
    hotkey_was_down = FALSE;
    overridden_character = NULL;
    second_player_movement_enabled = enable_second_player_movement;
    camera_relative_movement_enabled = enable_camera_relative_movement;
    separation_guard_enabled = enable_separation_guard;
    maximum_separation_distance = maximum_separation;
    separation_blocked = FALSE;
    separation_data_missing_logged = FALSE;
    second_player_weak_attack_enabled = enable_second_player_weak_attack;
    weak_attack_virtual_key = attack_virtual_key;
    weak_attack_was_down = FALSE;
    input_bridge_enabled = enable_input_bridge;
    input_bridge_deadzone = bridge_deadzone;
    ZeroMemory(&input_bridge_state, sizeof(input_bridge_state));
    input_bridge_connected = FALSE;
    input_bridge_last_right_stick_log_tick = 0u;
    input_bridge_last_right_x = 0;
    input_bridge_last_right_y = 0;
    second_player_skills_enabled = enable_second_player_skills;
    ZeroMemory(
        second_player_skill_virtual_keys,
        sizeof(second_player_skill_virtual_keys)
    );
    ZeroMemory(
        second_player_skill_keys_were_down,
        sizeof(second_player_skill_keys_were_down)
    );
    if (enable_second_player_skills) {
        memcpy(
            second_player_skill_virtual_keys,
            skill_virtual_keys,
            sizeof(second_player_skill_virtual_keys)
        );
    }
    target_trace_enabled = enable_target_trace;
    shared_group_camera_enabled = enable_shared_group_camera;
    matrix_target_create = (MatrixTargetCreateFunction)(
        base + RVA_MATRIX_TARGET_CREATE
    );
    camera_target_install = base + RVA_CAMERA_TARGET_INSTALL;
    camera_target_release = base + RVA_CAMERA_TARGET_RELEASE;
    group_camera_camera = NULL;
    group_camera_target = NULL;
    group_camera_original_targets[0] = NULL;
    group_camera_original_targets[1] = NULL;
    group_camera_target_list = NULL;
    reset_target_trace_state();
    buki_movement_active = FALSE;
    last_movement_x = 0;
    last_movement_z = 0;
    original_controller_update = (ControllerUpdateFunction)(
        base + RVA_CONTROLLER_UPDATE
    );
    ai_override_control = (AiControlFunction)(base + RVA_AI_OVERRIDE_CONTROL);
    ai_default_control = (AiControlFunction)(base + RVA_AI_DEFAULT_CONTROL);
    arbiter_movement = (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    arbiter_set_speed = (ArbiterSetSpeedFunction)(
        base + RVA_ARBITER_SET_SPEED
    );
    movement_camera_transform = (MovementCameraTransformFunction)(
        base + RVA_MOVEMENT_CAMERA_TRANSFORM
    );

    if (!SudekiMpInstallPointerHook(
            &controller_update_vtable_hook,
            slot,
            original_controller_update,
            poll_control_separation_hotkey)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }
    SudekiMpLogFormat(
        "control_separation_install=success target_resource_type=0x%02x virtual_key=0x%02lx second_player_movement=%s camera_relative_movement=%s separation_guard=%s maximum_separation_bits=0x%08lx second_player_weak_attack=%s weak_attack_virtual_key=0x%02lx second_player_skills=%s skill_keys=0x%02lx,0x%02lx,0x%02lx,0x%02lx target_trace=%s shared_group_camera=%s external_input_bridge=%s bridge_deadzone_bits=0x%08lx combat_input_rva=0x000db0e0\r\n",
        BUKI_RESOURCE_TYPE,
        (unsigned long)selected_virtual_key,
        second_player_movement_enabled ? "true" : "false",
        camera_relative_movement_enabled ? "true" : "false",
        separation_guard_enabled ? "true" : "false",
        (unsigned long)float_bits(maximum_separation_distance),
        second_player_weak_attack_enabled ? "true" : "false",
        (unsigned long)weak_attack_virtual_key,
        second_player_skills_enabled ? "true" : "false",
        (unsigned long)second_player_skill_virtual_keys[0],
        (unsigned long)second_player_skill_virtual_keys[1],
        (unsigned long)second_player_skill_virtual_keys[2],
        (unsigned long)second_player_skill_virtual_keys[3],
        target_trace_enabled ? "true" : "false",
        shared_group_camera_enabled ? "true" : "false",
        input_bridge_enabled ? "true" : "false",
        (unsigned long)float_bits(input_bridge_deadzone)
    );
    return TRUE;
}

void SudekiMpUninstallControlSeparation(void) {
    SudekiMpRestorePointerHook(&controller_update_vtable_hook);
    restore_group_camera("module_uninstall");
    original_controller_update = NULL;
    ai_override_control = NULL;
    ai_default_control = NULL;
    arbiter_movement = NULL;
    arbiter_set_speed = NULL;
    movement_camera_transform = NULL;
    game_base = NULL;
    overridden_character = NULL;
    selected_virtual_key = 0;
    hotkey_was_down = FALSE;
    second_player_movement_enabled = FALSE;
    camera_relative_movement_enabled = FALSE;
    separation_guard_enabled = FALSE;
    maximum_separation_distance = 0.0f;
    separation_blocked = FALSE;
    separation_data_missing_logged = FALSE;
    second_player_weak_attack_enabled = FALSE;
    weak_attack_virtual_key = 0;
    weak_attack_was_down = FALSE;
    input_bridge_enabled = FALSE;
    input_bridge_deadzone = 0.0f;
    ZeroMemory(&input_bridge_state, sizeof(input_bridge_state));
    input_bridge_connected = FALSE;
    input_bridge_last_right_stick_log_tick = 0u;
    input_bridge_last_right_x = 0;
    input_bridge_last_right_y = 0;
    second_player_skills_enabled = FALSE;
    ZeroMemory(
        second_player_skill_virtual_keys,
        sizeof(second_player_skill_virtual_keys)
    );
    ZeroMemory(
        second_player_skill_keys_were_down,
        sizeof(second_player_skill_keys_were_down)
    );
    target_trace_enabled = FALSE;
    shared_group_camera_enabled = FALSE;
    matrix_target_create = NULL;
    camera_target_install = NULL;
    camera_target_release = NULL;
    group_camera_camera = NULL;
    group_camera_target = NULL;
    group_camera_original_targets[0] = NULL;
    group_camera_original_targets[1] = NULL;
    group_camera_target_list = NULL;
    reset_target_trace_state();
    buki_movement_active = FALSE;
    last_movement_x = 0;
    last_movement_z = 0;
    SudekiMpCombatContextsReset();
}
