#include "hooks/lan_arena_window_policy.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

enum {
    RVA_WINDOW_ACTIVATE_POLICY = 0x0028d6a3u,
    RVA_FOCUS_LOSS_DEVICE_POLICY = 0x0028d74au,
    RVA_WINDOW_ACTIVATE_APP_POLICY = 0x0028d7ceu,
    RVA_KILL_FOCUS_SHOW_WINDOW = 0x0028d780u,
    RVA_DEVICE_FOCUS_STATE_GLOBAL = 0x003c3110u,
    RVA_SHOW_WINDOW_IAT = 0x0029a224u,
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

static BOOL signature_matches(uint8_t *base) {
    uint32_t device_focus_state;
    uint32_t show_window_slot;
    uint8_t *instruction;
    if (base == NULL) return FALSE;
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
    if (!SudekiMpRestoreBytePatch(&show_command_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&activate_app_compare_patch)) return FALSE;
    /* While the opcode is B2, both D2 and 01 are valid nonzero immediates.
     * Restore the immediate before the opcode so teardown never exposes an
     * invalid two-byte instruction to the live WndProc. */
    if (!SudekiMpRestoreBytePatch(&device_bool_value_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&device_bool_opcode_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&device_focus_state_patch)) return FALSE;
    if (!SudekiMpRestoreBytePatch(&active_compare_patch)) return FALSE;
    return TRUE;
}

BOOL SudekiMpInstallLanArenaWindowPolicy(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    DWORD install_error;
    if (base == NULL || active_compare_patch.installed ||
        device_focus_state_patch.installed ||
        device_bool_opcode_patch.installed ||
        device_bool_value_patch.installed ||
        activate_app_compare_patch.installed || show_command_patch.installed) {
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
    SudekiMpLogWrite(
        "lan_arena_window_policy event=install state=active "
        "wm_killfocus=stay_visible_no_activate native_command=6 "
        "replacement_command=8 wm_activate=background_updates_enabled "
        "wm_activateapp=background_updates_enabled "
        "graphics_devices=background_present_enabled inactive_sleep_ms=0 "
        "input_focus=native "
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
    was_installed = was_installed || activate_app_compare_patch.installed;
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
        activate_app_compare_patch.installed || show_command_patch.installed;
}
