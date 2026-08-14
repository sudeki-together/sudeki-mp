#include "hooks/control_separation.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>

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

enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_AI_OVERRIDE_CONTROL = 0x000f60d0u,
    RVA_AI_DEFAULT_CONTROL = 0x000f6100u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_ARBITER_SET_SPEED = 0x000db070u,
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
static uint8_t *game_base;
static void *overridden_character;
static UINT selected_virtual_key;
static BOOL hotkey_was_down;
static BOOL second_player_movement_enabled;
static BOOL buki_movement_active;
static int last_movement_x;
static int last_movement_z;

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

static void poll_buki_movement(void *controller, BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    void *arbiter;
    void *controller_target;
    int x;
    int z;
    float direction[3];

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
    arbiter_movement(arbiter, direction, 1.0f, 1.0f, 0u);
    if (!buki_movement_active || x != last_movement_x ||
        z != last_movement_z) {
        SudekiMpLogFormat(
            "control_separation event=second_player_movement phase=submit character=0x%08lx arbiter=0x%08lx input_x=%d input_z=%d speed_bits=0x3f800000 turn_rate_bits=0x3f800000 movement_mode=0\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            x,
            z
        );
    }
    buki_movement_active = TRUE;
    last_movement_x = x;
    last_movement_z = z;
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
}

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT virtual_key,
    BOOL enable_second_player_movement
) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL || virtual_key == 0u || virtual_key > 0xffu) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    slot = (void **)(base + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
    game_base = base;
    selected_virtual_key = virtual_key;
    hotkey_was_down = FALSE;
    overridden_character = NULL;
    second_player_movement_enabled = enable_second_player_movement;
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

    if (!SudekiMpInstallPointerHook(
            &controller_update_vtable_hook,
            slot,
            original_controller_update,
            poll_control_separation_hotkey)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }
    SudekiMpLogFormat(
        "control_separation_install=success target_resource_type=0x%02x virtual_key=0x%02lx second_player_movement=%s\r\n",
        BUKI_RESOURCE_TYPE,
        (unsigned long)selected_virtual_key,
        second_player_movement_enabled ? "true" : "false"
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
    game_base = NULL;
    overridden_character = NULL;
    selected_virtual_key = 0;
    hotkey_was_down = FALSE;
    second_player_movement_enabled = FALSE;
    buki_movement_active = FALSE;
    last_movement_x = 0;
    last_movement_z = 0;
}
