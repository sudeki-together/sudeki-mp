#include "hooks/lan_arena_window_policy.h"

#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "hooks/call_hook.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    RVA_WINDOW_ACTIVATE_POLICY = 0x0028d6a3u,
    RVA_FOCUS_LOSS_DEVICE_POLICY = 0x0028d74au,
    RVA_WINDOW_ACTIVATE_APP_POLICY = 0x0028d7ceu,
    RVA_KILL_FOCUS_SHOW_WINDOW = 0x0028d780u,
    RVA_DEVICE_FOCUS_STATE_GLOBAL = 0x003c3110u,
    RVA_SHOW_WINDOW_IAT = 0x0029a224u,
    RVA_FOCUS_CONTROLLER_RESET_CALL = 0x0028c432u,
    RVA_CONTROLLER_RESET = 0x000276c0u,
    RVA_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_GROUP_GLOBAL = 0x00408d94u,
    ACTIVE_COMPARE_OFFSET = 4u,
    DEVICE_BOOL_OPCODE_OFFSET = 0u,
    DEVICE_BOOL_VALUE_OFFSET = 1u,
    DEVICE_FOCUS_STATE_OPERAND_OFFSET = 4u,
    DEVICE_FOCUS_STATE_OFFSET = 8u,
    ACTIVATE_APP_COMPARE_OFFSET = 3u,
    SHOW_COMMAND_OFFSET = 4u,
    SHOW_WINDOW_OPERAND_OFFSET = 8u,
    NATIVE_INACTIVE_COMPARE = 0u,
    LAN_UNREACHABLE_ACTIVATION = 0xffu,
    NATIVE_CLEAR_DL_OPCODE = 0x32u,
    NATIVE_CLEAR_DL_OPERAND = 0xd2u,
    LAN_SET_DL_OPCODE = 0xb2u,
    LAN_SET_DL_VALUE = 0x01u,
    NATIVE_DEVICE_INACTIVE = 0u,
    LAN_DEVICE_ACTIVE = 1u,
    NATIVE_SW_MINIMIZE = 6u,
    LAN_SW_SHOWNA = 8u
};

static const uint8_t expected_activation_policy[] = {
    0x66u, 0x83u, 0x7du, 0x10u, NATIVE_INACTIVE_COMPARE,
    0x0fu, 0x95u, 0xc0u, 0x88u, 0x41u, 0x11u
};
static const uint8_t expected_focus_device_prefix[] = {
    NATIVE_CLEAR_DL_OPCODE, NATIVE_CLEAR_DL_OPERAND, 0xc6u, 0x05u
};
static const uint8_t expected_focus_device_suffix[] = {
    NATIVE_DEVICE_INACTIVE,
    0xe8u, 0xb8u, 0xecu, 0xffu, 0xffu
};
static const uint8_t expected_activate_app_policy[] = {
    0x83u, 0x7du, 0x10u, NATIVE_INACTIVE_COMPARE,
    0xe9u, 0xd1u, 0xfeu, 0xffu, 0xffu
};
static const uint8_t expected_prefix[] = {
    0x8bu, 0x55u, 0x08u, 0x6au, NATIVE_SW_MINIMIZE,
    0x52u, 0xffu, 0x15u
};
static const uint8_t expected_suffix[] = {
    0xe9u, 0x1du, 0xffu, 0xffu, 0xffu
};

static SudekiMpBytePatch active_compare_patch;
static SudekiMpBytePatch device_focus_state_patch;
static SudekiMpBytePatch device_bool_opcode_patch;
static SudekiMpBytePatch device_bool_value_patch;
static SudekiMpBytePatch activate_app_compare_patch;
static SudekiMpBytePatch show_command_patch;
static SudekiMpRelativeCallHook focus_reset_hook;
static uint8_t *focus_reset_base;
static void *original_focus_reset __attribute__((used));

static BOOL memory_range(const void *pointer, size_t size, BOOL write) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t start = (uintptr_t)pointer;
    if (pointer == NULL || size == 0u || start + size < start ||
        VirtualQuery(pointer, &info, sizeof(info)) == 0 ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        (write && (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0)) return FALSE;
    return start + size <= (uintptr_t)info.BaseAddress + info.RegionSize;
}

BOOL SudekiMpLanArenaWindowPreserveCastMovementPolicy(
    BOOL owner_exact, BOOL skill_active, int movement_mode, float direct_speed
) {
    return owner_exact && skill_active && movement_mode == 1 &&
        isfinite(direct_speed) && direct_speed > 0.0f;
}

static BOOL focus_cast_owner(uint8_t *controller, void **actor,
    SudekiMpCharacterSkillState *skill) {
    uint8_t *group;
    uint8_t *character;
    uintptr_t vtable;
    if (focus_reset_base == NULL ||
        !memory_range(controller, 0x24cu, TRUE) ||
        *(void **)(focus_reset_base + RVA_CONTROLLER_GLOBAL) != controller)
        return FALSE;
    group = *(uint8_t **)(focus_reset_base + RVA_GROUP_GLOBAL);
    character = *(uint8_t **)(controller + 0x248u);
    if (!memory_range(group, 0xd0u, FALSE) ||
        *(unsigned int *)(group + 0xccu) == 0u ||
        *(unsigned int *)(group + 0xccu) > 4u ||
        *(void **)(group + 0x90u) != character ||
        !memory_range(character, 0xdcu, FALSE)) return FALSE;
    vtable = (uintptr_t)*(void **)character - (uintptr_t)focus_reset_base;
    /* Supported-image hero vtables, shared with lifecycle identity evidence. */
    if (vtable != 0x002d5010u && vtable != 0x002d555cu &&
        vtable != 0x002d5a88u && vtable != 0x002d66fcu) return FALSE;
    if (!SudekiMpObserveCharacterSkill(character, skill) || !skill->active)
        return FALSE;
    *actor = character;
    return TRUE;
}

__attribute__((naked, noinline))
static void call_focus_reset(void *controller __attribute__((unused))) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "call *_original_focus_reset\n\t"
        "popl %esi\n\t"
        "ret\n\t");
}

static void __attribute__((stdcall, used)) focus_reset_scoped(uint8_t *controller) {
    DWORD incoming_error = GetLastError();
    SudekiMpCharacterSkillState before, after;
    void *actor = NULL;
    void *after_actor = NULL;
    int mode = 0;
    float speed = 0.0f;
    float targeting = -1.0f;
    BOOL preserve_movement = FALSE, preserve_targeting = FALSE;
    void *task = NULL;
    void *thread = NULL;
    BOOL preserve = focus_cast_owner(controller, &actor, &before);
    DWORD native_error;
    if (preserve) {
        mode = *(int *)(controller + 0x23cu);
        speed = *(float *)(controller + 0x1d4u);
        targeting = *(float *)(controller + 0x1d8u);
        preserve_movement = SudekiMpLanArenaWindowPreserveCastMovementPolicy(
            TRUE, before.active, mode, speed);
        preserve_targeting = isfinite(targeting) && targeting > 0.0f &&
            (*(uint32_t *)(controller + 0x1d0u) & 2u) == 0u;
        preserve = (preserve_movement || preserve_targeting) &&
            memory_range(before.skill, 0x78u, FALSE);
        if (preserve) {
            task = *(void **)((uint8_t *)before.skill + 0x74u);
            preserve = memory_range(task, sizeof(void *), FALSE);
            if (preserve) {
                thread = *(void **)task;
                preserve = thread != NULL;
            }
        }
    }
    SetLastError(incoming_error);
    /* Always clear held keys, mouse transitions, axes and wheel state through
     * retail's reset. Only script-authored movement/targeting are restored, and
     * only across this synchronous focus callback for the same active cast. */
    call_focus_reset(controller);
    native_error = GetLastError();
    if (preserve && focus_cast_owner(controller, &after_actor, &after) &&
        after_actor == actor && after.skill == before.skill &&
        after.slot == before.slot &&
        memory_range(after.skill, 0x78u, FALSE) &&
        *(void **)((uint8_t *)after.skill + 0x74u) == task &&
        memory_range(task, sizeof(void *), FALSE) && *(void **)task == thread) {
        if (preserve_movement && *(int *)(controller + 0x23cu) == 0 &&
            *(float *)(controller + 0x1d4u) == 1.0f) {
            *(int *)(controller + 0x23cu) = mode;
            *(float *)(controller + 0x1d4u) = speed;
        }
        if (preserve_targeting && *(float *)(controller + 0x1d8u) == -1.0f &&
            *(uint32_t *)(controller + 0x1d0u) == 15u) {
            *(float *)(controller + 0x1d8u) = targeting;
            /* Restore only the targeting-owned action gate, never held input. */
            *(uint32_t *)(controller + 0x1d0u) &= ~2u;
        }
        SudekiMpLogWrite("lan_arena_window_policy event=focus_reset "
            "cast_settings=preserved physical_input=cleared\r\n");
    }
    SetLastError(native_error);
}

__attribute__((naked, noinline, used))
static void focus_reset_adapter(void) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "call _focus_reset_scoped@4\n\t"
        "ret\n\t");
}

static BOOL signature_matches(uint8_t *base) {
    uint32_t device_focus_state;
    uint32_t show_window_slot;
    uint8_t *instruction;
    if (base == NULL) return FALSE;
    /* Retail focus callback passes the singleton controller in ESI. Its
     * reset clears physical input AND +23c/+1d4, unlike normal cast cleanup.
     * This callsite is not the reset's constructor or gameplay callers. */
    if (memcmp(base + 0x0028c428u, "\x8b\x35", 2u) != 0 ||
        *(void **)(base + 0x0028c42au) != base + RVA_CONTROLLER_GLOBAL ||
        memcmp(base + 0x0028c42eu, "\x85\xf6\x74\x05", 4u) != 0 ||
        memcmp(base + RVA_FOCUS_CONTROLLER_RESET_CALL,
            "\xe8\x89\xb2\xd9\xff\x5e\xc3", 7u) != 0 ||
        memcmp(base + RVA_CONTROLLER_RESET,
            "\xd9\xee\x53\xd9\x96\xa0\x01\x00\x00\x33\xdb", 11u) != 0 ||
        memcmp(base + 0x0002770fu, "\x89\x9e\x3c\x02\x00\x00", 6u) != 0 ||
        memcmp(base + 0x000276f6u, "\xc7\x86\xd0\x01\x00\x00\x0f\x00\x00\x00", 10u) != 0 ||
        memcmp(base + 0x00027740u, "\xd9\x96\xd8\x01\x00\x00", 6u) != 0 ||
        memcmp(base + 0x00027753u, "\xd9\xe8\xd9\x9e\xd4\x01\x00\x00", 8u) != 0)
        return FALSE;
    if (memcmp(base + RVA_WINDOW_ACTIVATE_POLICY,
            expected_activation_policy,
            sizeof(expected_activation_policy)) != 0) return FALSE;
    if (memcmp(base + RVA_FOCUS_LOSS_DEVICE_POLICY,
            expected_focus_device_prefix,
            sizeof(expected_focus_device_prefix)) != 0 ||
        memcmp(base + RVA_FOCUS_LOSS_DEVICE_POLICY +
                DEVICE_FOCUS_STATE_OFFSET,
            expected_focus_device_suffix,
            sizeof(expected_focus_device_suffix)) != 0) return FALSE;
    if (memcmp(base + RVA_WINDOW_ACTIVATE_APP_POLICY,
            expected_activate_app_policy,
            sizeof(expected_activate_app_policy)) != 0) return FALSE;
    memcpy(&device_focus_state,
        base + RVA_FOCUS_LOSS_DEVICE_POLICY +
            DEVICE_FOCUS_STATE_OPERAND_OFFSET,
        sizeof(device_focus_state));
    if (device_focus_state != (uint32_t)(uintptr_t)(
            base + RVA_DEVICE_FOCUS_STATE_GLOBAL)) return FALSE;
    instruction = base + RVA_KILL_FOCUS_SHOW_WINDOW;
    if (memcmp(instruction, expected_prefix, sizeof(expected_prefix)) != 0 ||
        memcmp(instruction + 12u, expected_suffix,
            sizeof(expected_suffix)) != 0) return FALSE;
    memcpy(&show_window_slot, instruction + SHOW_WINDOW_OPERAND_OFFSET,
        sizeof(show_window_slot));
    return show_window_slot ==
        (uint32_t)(uintptr_t)(base + RVA_SHOW_WINDOW_IAT);
}

static BOOL restore_patches(void) {
    if (!SudekiMpRestoreRelativeCallHook(&focus_reset_hook)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&show_command_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&activate_app_compare_patch)) return FALSE;
    /* While the opcode is B2, both D2 and 01 are valid nonzero immediates.
     * Restore the immediate before the opcode so teardown never exposes an
     * invalid two-byte instruction to the live WndProc. */
    if (!SudekiMpRestoreBytePatch(&device_bool_value_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&device_bool_opcode_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&device_focus_state_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&active_compare_patch)) return FALSE;
    original_focus_reset = NULL;
    focus_reset_base = NULL;
    return TRUE;
}

BOOL SudekiMpInstallLanArenaWindowPolicy(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    DWORD install_error;
    if (base == NULL || active_compare_patch.installed ||
        device_focus_state_patch.installed ||
        device_bool_opcode_patch.installed ||
        device_bool_value_patch.installed ||
        activate_app_compare_patch.installed || show_command_patch.installed ||
        focus_reset_hook.installed) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!signature_matches(base)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!SudekiMpInstallBytePatch(
            &active_compare_patch,
            base + RVA_WINDOW_ACTIVATE_POLICY + ACTIVE_COMPARE_OFFSET,
            NATIVE_INACTIVE_COMPARE,
            LAN_UNREACHABLE_ACTIVATION)) return FALSE;
    if (!SudekiMpInstallBytePatch(
            &device_focus_state_patch,
            base + RVA_FOCUS_LOSS_DEVICE_POLICY + DEVICE_FOCUS_STATE_OFFSET,
            NATIVE_DEVICE_INACTIVE,
            LAN_DEVICE_ACTIVE)) goto rollback;
    /* Changing 32 D2 to B2 D2 first remains a valid instruction. The second
     * byte can then be narrowed to the canonical BOOL value 1 atomically. */
    if (!SudekiMpInstallBytePatch(
            &device_bool_opcode_patch,
            base + RVA_FOCUS_LOSS_DEVICE_POLICY + DEVICE_BOOL_OPCODE_OFFSET,
            NATIVE_CLEAR_DL_OPCODE,
            LAN_SET_DL_OPCODE)) goto rollback;
    if (!SudekiMpInstallBytePatch(
            &device_bool_value_patch,
            base + RVA_FOCUS_LOSS_DEVICE_POLICY + DEVICE_BOOL_VALUE_OFFSET,
            NATIVE_CLEAR_DL_OPERAND,
            LAN_SET_DL_VALUE)) goto rollback;
    if (!SudekiMpInstallBytePatch(
            &activate_app_compare_patch,
            base + RVA_WINDOW_ACTIVATE_APP_POLICY +
                ACTIVATE_APP_COMPARE_OFFSET,
            NATIVE_INACTIVE_COMPARE,
            LAN_UNREACHABLE_ACTIVATION)) goto rollback;
    if (!SudekiMpInstallBytePatch(
            &show_command_patch,
            base + RVA_KILL_FOCUS_SHOW_WINDOW + SHOW_COMMAND_OFFSET,
            NATIVE_SW_MINIMIZE,
            LAN_SW_SHOWNA)) goto rollback;
    focus_reset_base = base;
    original_focus_reset = base + RVA_CONTROLLER_RESET;
    if (!SudekiMpInstallRelativeCallHook(&focus_reset_hook,
            base + RVA_FOCUS_CONTROLLER_RESET_CALL,
            original_focus_reset, (const void *)(uintptr_t)focus_reset_adapter)) goto rollback;
    SudekiMpLogWrite(
        "lan_arena_window_policy event=install state=active "
        "wm_killfocus=stay_visible_no_activate native_command=6 "
        "replacement_command=8 wm_activate=background_updates_enabled "
        "wm_activateapp=background_updates_enabled "
        "graphics_devices=background_present_enabled inactive_sleep_ms=0 "
        "input_focus=native cast_movement=preserved_across_focus_reset "
        "policy=lan_profiles_only_exact_image\r\n");
    return TRUE;

rollback:
    install_error = GetLastError();
    if (!restore_patches()) return FALSE;
    SetLastError(install_error);
    return FALSE;
}

BOOL SudekiMpUninstallLanArenaWindowPolicy(void) {
    BOOL was_installed = active_compare_patch.installed ||
        device_focus_state_patch.installed ||
        device_bool_opcode_patch.installed ||
        device_bool_value_patch.installed || show_command_patch.installed;
    was_installed = was_installed || activate_app_compare_patch.installed ||
        focus_reset_hook.installed;
    if (!restore_patches()) return FALSE;
    if (was_installed) {
        SudekiMpLogWrite(
            "lan_arena_window_policy event=uninstall state=restored "
            "wm_killfocus=native_minimize wm_activate=native_pause "
            "wm_activateapp=native_pause "
            "graphics_devices=native_focus_activation\r\n");
    }
    return TRUE;
}

BOOL SudekiMpLanArenaWindowPolicyInstalled(void) {
    return active_compare_patch.installed ||
        device_focus_state_patch.installed ||
        device_bool_opcode_patch.installed ||
        device_bool_value_patch.installed ||
        activate_app_compare_patch.installed || show_command_patch.installed ||
        focus_reset_hook.installed;
}
