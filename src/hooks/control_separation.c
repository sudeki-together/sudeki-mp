#include "hooks/control_separation.h"

#include "engine/arbiter_combat_input.h"
#include "engine/log.h"
#include "hooks/call_hook.h"

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
typedef void *(SUDEKIMP_THISCALL *TargeterGetCurrentTargetFunction)(
    void *targeter
);

enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_AI_OVERRIDE_CONTROL = 0x000f60d0u,
    RVA_AI_DEFAULT_CONTROL = 0x000f6100u,
    RVA_MOVEMENT_CAMERA_TRANSFORM = 0x000291a0u,
    RVA_TARGETER_GET_CURRENT_TARGET = 0x000b9dc0u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_ARBITER_SET_SPEED = 0x000db070u,
    RVA_ARBITER_COMBAT_INPUT = 0x000db0e0u,
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
static TargeterGetCurrentTargetFunction targeter_get_current_target;
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
static BOOL target_trace_enabled;
static DWORD target_trace_last_sample_tick;
static void *target_trace_last_node;
static void *target_trace_last_gel;
static int target_trace_last_auto_enabled;
static BOOL buki_movement_active;
static int last_movement_x;
static int last_movement_z;

static const uint8_t expected_arbiter_combat_input_entry[] = {
    0x55, 0x8b, 0x6c, 0x24, 0x08, 0x56, 0x57, 0x8b, 0xf8, 0x8b, 0xf1
};
static const uint8_t expected_movement_camera_transform_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x8b, 0x55, 0x08, 0xd9, 0xee
};
static const uint8_t expected_targeter_get_current_target_entry[] = {
    0x83, 0xec, 0x0c, 0x56, 0x83, 0xc1, 0x54, 0x8d, 0x44, 0x24, 0x04
};

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void reset_target_trace_state(void) {
    target_trace_last_sample_tick = 0;
    target_trace_last_node = NULL;
    target_trace_last_gel = NULL;
    target_trace_last_auto_enabled = -1;
}

static BOOL overridden_character_is_in_active_group(void) {
    uint8_t *group;
    unsigned int index;

    if (game_base == NULL || overridden_character == NULL) {
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group == NULL) {
        return FALSE;
    }
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)slot == overridden_character) {
            return TRUE;
        }
    }
    return FALSE;
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

static void poll_buki_movement(void *controller, BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    void *arbiter;
    void *controller_target;
    int x;
    int z;
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

    x = ((GetAsyncKeyState('L') & 0x8000) != 0) -
        ((GetAsyncKeyState('J') & 0x8000) != 0);
    z = ((GetAsyncKeyState('I') & 0x8000) != 0) -
        ((GetAsyncKeyState('K') & 0x8000) != 0);
    if (x == 0 && z == 0) {
        stop_buki_movement();
        return;
    }

    direction[0] = (float)x;
    direction[1] = 0.0f;
    direction[2] = (float)z;
    if (x != 0 && z != 0) {
        direction[0] *= 0.70710678f;
        direction[2] *= 0.70710678f;
    }
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
    arbiter_movement(arbiter, direction, 1.0f, 1.0f, 0u);
    if (!buki_movement_active || x != last_movement_x ||
        z != last_movement_z) {
        SudekiMpLogFormat(
            "control_separation event=second_player_movement phase=submit character=0x%08lx arbiter=0x%08lx input_x=%d input_z=%d direction_bits=%08lx,00000000,%08lx camera_relative=%s speed_bits=0x3f800000 turn_rate_bits=0x3f800000 movement_mode=0\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            x,
            z,
            (unsigned long)direction_x_bits,
            (unsigned long)direction_z_bits,
            camera_relative_movement_enabled ? "true" : "false"
        );
    }
    buki_movement_active = TRUE;
    last_movement_x = x;
    last_movement_z = z;
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
    key_is_down =
        (GetAsyncKeyState((int)weak_attack_virtual_key) & 0x8000) != 0;
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

static void poll_buki_target_trace(BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *targeter;
    uint8_t *transform;
    void *target_node;
    void *target_gel;
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
    target_gel = targeter_get_current_target(targeter);
    if (transform != NULL) {
        memcpy(forward_bits, transform + 0x50, sizeof(forward_bits));
    }
    if (target_node != target_trace_last_node ||
        target_gel != target_trace_last_gel ||
        auto_enabled != target_trace_last_auto_enabled) {
        SudekiMpLogFormat(
            "control_separation event=second_player_target_trace character=0x%08lx targeter=0x%08lx target_node=0x%08lx target_gel=0x%08lx auto_target_enabled=%d forward_bits=%08lx,%08lx,%08lx\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)targeter,
            (unsigned long)(uintptr_t)target_node,
            (unsigned long)(uintptr_t)target_gel,
            auto_enabled,
            (unsigned long)forward_bits[0],
            (unsigned long)forward_bits[1],
            (unsigned long)forward_bits[2]
        );
        target_trace_last_node = target_node;
        target_trace_last_gel = target_gel;
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
    poll_buki_target_trace(owns_foreground);
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
    BOOL enable_target_trace
) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL || toggle_virtual_key == 0u ||
        toggle_virtual_key > 0xffu ||
        (enable_second_player_weak_attack &&
         (attack_virtual_key == 0u || attack_virtual_key > 0xffu))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (enable_camera_relative_movement && !enable_second_player_movement) {
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
    if (enable_second_player_weak_attack &&
        memcmp(
            base + RVA_ARBITER_COMBAT_INPUT,
            expected_arbiter_combat_input_entry,
            sizeof(expected_arbiter_combat_input_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_target_trace &&
        memcmp(
            base + RVA_TARGETER_GET_CURRENT_TARGET,
            expected_targeter_get_current_target_entry,
            sizeof(expected_targeter_get_current_target_entry)) != 0) {
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
    target_trace_enabled = enable_target_trace;
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
    targeter_get_current_target = (TargeterGetCurrentTargetFunction)(
        base + RVA_TARGETER_GET_CURRENT_TARGET
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
        "control_separation_install=success target_resource_type=0x%02x virtual_key=0x%02lx second_player_movement=%s camera_relative_movement=%s separation_guard=%s maximum_separation_bits=0x%08lx second_player_weak_attack=%s weak_attack_virtual_key=0x%02lx target_trace=%s combat_input_rva=0x000db0e0\r\n",
        BUKI_RESOURCE_TYPE,
        (unsigned long)selected_virtual_key,
        second_player_movement_enabled ? "true" : "false",
        camera_relative_movement_enabled ? "true" : "false",
        separation_guard_enabled ? "true" : "false",
        (unsigned long)float_bits(maximum_separation_distance),
        second_player_weak_attack_enabled ? "true" : "false",
        (unsigned long)weak_attack_virtual_key,
        target_trace_enabled ? "true" : "false"
    );
    return TRUE;
}

void SudekiMpUninstallControlSeparation(void) {
    SudekiMpRestorePointerHook(&controller_update_vtable_hook);
    original_controller_update = NULL;
    ai_override_control = NULL;
    ai_default_control = NULL;
    arbiter_movement = NULL;
    arbiter_set_speed = NULL;
    movement_camera_transform = NULL;
    targeter_get_current_target = NULL;
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
    target_trace_enabled = FALSE;
    reset_target_trace_state();
    buki_movement_active = FALSE;
    last_movement_x = 0;
    last_movement_z = 0;
}
