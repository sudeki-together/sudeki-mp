#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/player_combat_context.h"
#include "engine/skill_activation_abi.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/pattern_scan.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_menu.h"
#include "hooks/quick_skill_input.h"
#include "hooks/skill_trace.h"
#include "hooks/split_screen_render.h"
#include "hooks/spirit_strike_input.h"
#include "input/bridge_receiver.h"
#include "input/key_binding.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUDEKIMP_INIT_OK 0u
#define SUDEKIMP_INIT_BAD_PATH 1u
#define SUDEKIMP_INIT_BAD_BUILD 2u
#define SUDEKIMP_INIT_BAD_SIGNATURE 3u
#define SUDEKIMP_INIT_PATCH_FAILED 4u
#define SUDEKIMP_INIT_TRACE_FAILED 5u
#define SUDEKIMP_INIT_BAD_CONFIG 6u
#define SUDEKIMP_INIT_INPUT_TRACE_FAILED 7u
#define SUDEKIMP_INIT_SPIRIT_INPUT_FAILED 8u
#define SUDEKIMP_INIT_CHARACTER_SWITCH_TRACE_FAILED 9u
#define SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED 10u
#define SUDEKIMP_INIT_PLAYER_INPUT_TRACE_FAILED 11u
#define SUDEKIMP_INIT_FREEROAM_CAMERA_FAILED 12u
#define SUDEKIMP_INIT_SPLIT_SCREEN_RENDER_FAILED 13u
#define SUDEKIMP_INIT_INPUT_BRIDGE_FAILED 14u

static HMODULE dll_module;

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static BOOL read_config_boolean(
    const wchar_t *path,
    const wchar_t *section,
    const wchar_t *key
) {
    wchar_t value[16];

    GetPrivateProfileStringW(section, key, L"false", value,
        (DWORD)(sizeof(value) / sizeof(value[0])), path);
    return _wcsicmp(value, L"true") == 0 ||
        _wcsicmp(value, L"yes") == 0 ||
        _wcsicmp(value, L"on") == 0 ||
        wcscmp(value, L"1") == 0;
}

static BOOL read_config_float(
    const wchar_t *path,
    const wchar_t *section,
    const wchar_t *key,
    float default_value,
    float minimum,
    float maximum,
    float *result
) {
    wchar_t value[32];
    wchar_t default_text[32];
    wchar_t *end = NULL;
    double parsed;

    if (result == NULL) {
        return FALSE;
    }
    _snwprintf(
        default_text,
        sizeof(default_text) / sizeof(default_text[0]),
        L"%.3f",
        (double)default_value
    );
    default_text[(sizeof(default_text) / sizeof(default_text[0])) - 1] = L'\0';
    GetPrivateProfileStringW(
        section,
        key,
        default_text,
        value,
        (DWORD)(sizeof(value) / sizeof(value[0])),
        path
    );
    parsed = wcstod(value, &end);
    if (end == value || *end != L'\0' || parsed < minimum || parsed > maximum) {
        return FALSE;
    }
    *result = (float)parsed;
    return TRUE;
}

static BOOL read_config_integer(
    const wchar_t *path,
    const wchar_t *section,
    const wchar_t *key,
    int default_value,
    int minimum,
    int maximum,
    int *result
) {
    wchar_t value[32];
    wchar_t default_text[32];
    wchar_t *end = NULL;
    long parsed;

    if (result == NULL) {
        return FALSE;
    }
    _snwprintf(default_text, 32, L"%d", default_value);
    default_text[31] = L'\0';
    GetPrivateProfileStringW(section, key, default_text, value, 32, path);
    parsed = wcstol(value, &end, 10);
    if (end == value || *end != L'\0' || parsed < minimum || parsed > maximum) {
        return FALSE;
    }
    *result = (int)parsed;
    return TRUE;
}

static BOOL get_text_section(
    HMODULE module,
    const uint8_t **section_base,
    size_t *section_size
) {
    const uint8_t *base = (const uint8_t *)module;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *section;
    WORD index;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return FALSE;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return FALSE;
    }

    section = IMAGE_FIRST_SECTION(nt);
    for (index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        if (memcmp(section[index].Name, ".text", 5) == 0) {
            *section_base = base + section[index].VirtualAddress;
            *section_size = section[index].Misc.VirtualSize;
            return TRUE;
        }
    }
    return FALSE;
}

DWORD WINAPI SudekiMP_Initialize(void *unused) {
    static const uint8_t activation_pattern[] = {
        0xa1, 0, 0, 0, 0,
        0x8b, 0x35, 0, 0, 0, 0,
        0xc7, 0x40, 0x24, 0x01, 0x00, 0x00, 0x00
    };
    static const char activation_mask[] = "x????xx????xxxxxxx";
    wchar_t game_path[MAX_PATH];
    wchar_t config_path[MAX_PATH];
    HMODULE game_module = GetModuleHandleW(NULL);
    SudekiMpBuildCheck build;
    const uint8_t *text_base;
    size_t text_size;
    SudekiMpPatternResult pattern_result;
    BOOL patch_enabled;
    BOOL trace_enabled;
    BOOL animation_speed_enabled;
    BOOL camera_speed_enabled;
    BOOL quick_skill_input_trace_enabled;
    BOOL character_switch_trace_enabled;
    BOOL control_separation_enabled;
    BOOL player_movement_trace_enabled;
    BOOL second_player_movement_enabled;
    BOOL second_player_camera_relative_movement_enabled;
    BOOL second_player_separation_guard_enabled;
    BOOL second_player_weak_attack_enabled;
    BOOL second_player_target_trace_enabled;
    BOOL shared_group_camera_enabled;
    BOOL split_screen_render_enabled;
    BOOL second_player_camera_enabled;
    BOOL dual_camera_frame_cache_enabled;
    BOOL freeroam_camera_input_enabled;
    BOOL ranged_quick_skill_prototype_enabled;
    BOOL realtime_multiplayer_skill_combat_enabled;
    BOOL direct_spirit_strike_prototype_enabled;
    BOOL external_input_bridge_enabled;
    wchar_t spirit_strike_key_text[32];
    wchar_t control_separation_key_text[32];
    wchar_t second_player_weak_attack_key_text[32];
    wchar_t second_player_camera_key_text[32];
    wchar_t second_player_skill_key_text[4][32];
    wchar_t freeroam_camera_modifier_text[32];
    UINT spirit_strike_virtual_key = 'G';
    UINT control_separation_virtual_key = 'J';
    UINT second_player_weak_attack_virtual_key = 'U';
    UINT second_player_camera_virtual_key = VK_F9;
    UINT second_player_skill_virtual_keys[4] = {
        VK_F1, VK_F2, VK_F3, VK_F4
    };
    UINT freeroam_camera_modifier_key = VK_LCONTROL;
    int spirit_strike_id = -1;
    int spirit_strike_variant = 1;
    int input_bridge_port = 26760;
    int input_bridge_timeout_ms = 250;
    float plasmatica_animation_speed = 1.0f;
    float plasmatica_camera_speed = 1.0f;
    float second_player_maximum_separation = 10.0f;
    float input_bridge_deadzone = 0.20f;

    (void)unused;
    if (GetModuleFileNameW(NULL, game_path, MAX_PATH) == 0) {
        return SUDEKIMP_INIT_BAD_PATH;
    }
    if (!SudekiMpLogOpenBesideGame(game_path)) {
        return SUDEKIMP_INIT_BAD_PATH;
    }

    SudekiMpLogWrite("SudekiMP 0.1.0\r\n");
    SudekiMpLogWrite("event=process_attach\r\n");
    SudekiMpLogFormat("module_base=0x%08lx\r\n", (unsigned long)(uintptr_t)game_module);

    if (!SudekiMpCheckExecutableFile(game_path, &build)) {
        SudekiMpLogWrite("status=hash_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_BUILD;
    }
    SudekiMpLogFormat("game_sha256=%s\r\n", build.actual_sha256);

    if (!build.hash_matches || !build.pe_matches ||
        !SudekiMpCheckLoadedExecutable(game_module)) {
        SudekiMpLogWrite("status=unsupported_build\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_BUILD;
    }

    if (!get_text_section(game_module, &text_base, &text_size)) {
        SudekiMpLogWrite("status=text_section_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_SIGNATURE;
    }

    pattern_result = SudekiMpFindPattern(
        text_base,
        text_size,
        activation_pattern,
        activation_mask,
        sizeof(activation_pattern)
    );
    SudekiMpLogFormat("quick_menu_signature_matches=%lu\r\n",
        (unsigned long)pattern_result.match_count);
    if (pattern_result.match_count != 1 || pattern_result.address == NULL) {
        SudekiMpLogWrite("status=signature_mismatch\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_SIGNATURE;
    }
    SudekiMpLogFormat("quick_menu_signature_rva=0x%08lx\r\n",
        (unsigned long)(pattern_result.address - (const uint8_t *)game_module));

    if (dll_module == NULL ||
        GetModuleFileNameW(dll_module, config_path, MAX_PATH) == 0) {
        SudekiMpLogWrite("status=config_path_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_PATH;
    }
    {
        wchar_t *slash = wcsrchr(config_path, L'\\');
        if (slash != NULL) {
            slash[1] = L'\0';
        } else {
            config_path[0] = L'\0';
        }
    }
    if ((size_t)lstrlenW(config_path) + 13u < MAX_PATH) {
        lstrcatW(config_path, L"SudekiMP.ini");
    }
    patch_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableQuickMenuNormalSpeed"
    );
    trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlasmaticaTrace"
    );
    animation_speed_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlasmaticaAnimationSpeed"
    );
    camera_speed_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlasmaticaCameraSpeed"
    );
    quick_skill_input_trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableQuickSkillInputTrace"
    );
    character_switch_trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableCharacterSwitchTrace"
    );
    control_separation_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableControlSeparationPrototype"
    );
    player_movement_trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlayerMovementTrace"
    );
    second_player_movement_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerMovementPrototype"
    );
    second_player_camera_relative_movement_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerCameraRelativeMovementPrototype"
    );
    second_player_separation_guard_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerSeparationGuardPrototype"
    );
    second_player_weak_attack_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerWeakAttackPrototype"
    );
    second_player_target_trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerTargetTrace"
    );
    shared_group_camera_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSharedGroupCameraPrototype"
    );
    split_screen_render_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSplitScreenRenderPrototype"
    );
    second_player_camera_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerCameraPrototype"
    );
    dual_camera_frame_cache_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableDualCameraFrameCachePrototype"
    );
    freeroam_camera_input_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableFreeRoamCameraModifierPrototype"
    );
    ranged_quick_skill_prototype_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableRangedQuickSkillPrototype"
    );
    realtime_multiplayer_skill_combat_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableRealtimeMultiplayerSkillCombatPrototype"
    );
    direct_spirit_strike_prototype_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableDirectSpiritStrikePrototype"
    );
    external_input_bridge_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableExternalInputBridgePrototype"
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"SpiritStrike",
        L"G",
        spirit_strike_key_text,
        (DWORD)(sizeof(spirit_strike_key_text) /
            sizeof(spirit_strike_key_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"FreeRoamCameraModifier",
        L"LeftCtrl",
        freeroam_camera_modifier_text,
        (DWORD)(sizeof(freeroam_camera_modifier_text) /
            sizeof(freeroam_camera_modifier_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"ToggleBukiAi",
        L"J",
        control_separation_key_text,
        (DWORD)(sizeof(control_separation_key_text) /
            sizeof(control_separation_key_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"SecondPlayerWeakAttack",
        L"U",
        second_player_weak_attack_key_text,
        (DWORD)(sizeof(second_player_weak_attack_key_text) /
            sizeof(second_player_weak_attack_key_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"ToggleSecondPlayerCamera",
        L"F9",
        second_player_camera_key_text,
        (DWORD)(sizeof(second_player_camera_key_text) /
            sizeof(second_player_camera_key_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings", L"SecondPlayerSkill1", L"F1",
        second_player_skill_key_text[0], 32u, config_path
    );
    GetPrivateProfileStringW(
        L"Bindings", L"SecondPlayerSkill2", L"F2",
        second_player_skill_key_text[1], 32u, config_path
    );
    GetPrivateProfileStringW(
        L"Bindings", L"SecondPlayerSkill3", L"F3",
        second_player_skill_key_text[2], 32u, config_path
    );
    GetPrivateProfileStringW(
        L"Bindings", L"SecondPlayerSkill4", L"F4",
        second_player_skill_key_text[3], 32u, config_path
    );
    if (direct_spirit_strike_prototype_enabled &&
        !SudekiMpParseInputKey(
            spirit_strike_key_text,
            &spirit_strike_virtual_key)) {
        SudekiMpLogWrite("spirit_strike_key_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (control_separation_enabled &&
        !SudekiMpParseInputKey(
            control_separation_key_text,
            &control_separation_virtual_key)) {
        SudekiMpLogWrite("control_separation_key_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_weak_attack_enabled &&
        !SudekiMpParseInputKey(
            second_player_weak_attack_key_text,
            &second_player_weak_attack_virtual_key)) {
        SudekiMpLogWrite(
            "second_player_weak_attack_key_config=invalid\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_camera_enabled &&
        !SudekiMpParseInputKey(
            second_player_camera_key_text,
            &second_player_camera_virtual_key)) {
        SudekiMpLogWrite("second_player_camera_key_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (realtime_multiplayer_skill_combat_enabled) {
        unsigned int index;
        for (index = 0u; index < 4u; ++index) {
            if (!SudekiMpParseInputKey(
                    second_player_skill_key_text[index],
                    &second_player_skill_virtual_keys[index])) {
                SudekiMpLogFormat(
                    "second_player_skill_key_config=invalid ordinal=%u\r\n",
                    index
                );
                SudekiMpLogWrite("status=config_error\r\n");
                SudekiMpLogClose();
                return SUDEKIMP_INIT_BAD_CONFIG;
            }
        }
    }
    if (freeroam_camera_input_enabled &&
        !SudekiMpParseInputKey(
            freeroam_camera_modifier_text,
            &freeroam_camera_modifier_key)) {
        SudekiMpLogWrite("freeroam_camera_modifier_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_movement_enabled && !control_separation_enabled) {
        SudekiMpLogWrite(
            "second_player_movement_config=requires_control_separation\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_camera_relative_movement_enabled &&
        !second_player_movement_enabled) {
        SudekiMpLogWrite(
            "second_player_camera_relative_movement_config=requires_second_player_movement\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_separation_guard_enabled &&
        !second_player_movement_enabled) {
        SudekiMpLogWrite(
            "second_player_separation_guard_config=requires_second_player_movement\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_separation_guard_enabled && !read_config_float(
            config_path,
            L"SudekiMP",
            L"SecondPlayerMaximumSeparation",
            10.0f,
            0.25f,
            1000.0f,
            &second_player_maximum_separation)) {
        SudekiMpLogWrite(
            "second_player_maximum_separation_config=invalid\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_weak_attack_enabled && !control_separation_enabled) {
        SudekiMpLogWrite(
            "second_player_weak_attack_config=requires_control_separation\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (external_input_bridge_enabled &&
        (!control_separation_enabled || !second_player_movement_enabled)) {
        SudekiMpLogWrite(
            "external_input_bridge_config=requires_control_separation_and_second_player_movement\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (external_input_bridge_enabled &&
        (!read_config_integer(
             config_path,
             L"SudekiMP",
             L"InputBridgePort",
             26760,
             1024,
             65535,
             &input_bridge_port) ||
         !read_config_integer(
             config_path,
             L"SudekiMP",
             L"InputBridgeTimeoutMs",
             250,
             50,
             5000,
             &input_bridge_timeout_ms) ||
         !read_config_float(
             config_path,
             L"SudekiMP",
             L"InputBridgeDeadzone",
             0.20f,
             0.0f,
             0.90f,
             &input_bridge_deadzone))) {
        SudekiMpLogWrite("external_input_bridge_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_target_trace_enabled && !control_separation_enabled) {
        SudekiMpLogWrite(
            "second_player_target_trace_config=requires_control_separation\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (shared_group_camera_enabled &&
        (!control_separation_enabled || !second_player_movement_enabled)) {
        SudekiMpLogWrite(
            "shared_group_camera_config=requires_control_separation_and_second_player_movement\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_camera_enabled && !split_screen_render_enabled) {
        SudekiMpLogWrite(
            "second_player_camera_config=requires_split_screen_render\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (dual_camera_frame_cache_enabled &&
        (!split_screen_render_enabled || !second_player_camera_enabled)) {
        SudekiMpLogWrite(
            "dual_camera_frame_cache_config=requires_split_screen_render_and_second_player_camera\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (realtime_multiplayer_skill_combat_enabled &&
        (!patch_enabled || !trace_enabled ||
         !quick_skill_input_trace_enabled ||
         !ranged_quick_skill_prototype_enabled ||
         !control_separation_enabled ||
         !second_player_movement_enabled ||
         !second_player_weak_attack_enabled ||
         !split_screen_render_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "realtime_multiplayer_skill_combat_config=requires_normal_speed_plasmatica_trace_quick_skill_ranged_control_p2_movement_p2_attack_split_p2_camera_dual_cache\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (freeroam_camera_input_enabled && character_switch_trace_enabled) {
        SudekiMpLogWrite(
            "freeroam_camera_config=conflicts_with_character_switch_trace\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (direct_spirit_strike_prototype_enabled && !read_config_integer(
            config_path,
            L"SudekiMP",
            L"SpiritStrikeId",
            -1,
            -1,
            15,
            &spirit_strike_id)) {
        SudekiMpLogWrite("spirit_strike_id_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (direct_spirit_strike_prototype_enabled && !read_config_integer(
            config_path,
            L"SudekiMP",
            L"SpiritStrikeVariant",
            1,
            1,
            2,
            &spirit_strike_variant)) {
        SudekiMpLogWrite("spirit_strike_variant_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (animation_speed_enabled && !read_config_float(
            config_path,
            L"SudekiMP",
            L"PlasmaticaAnimationSpeed",
            1.5f,
            0.25f,
            4.0f,
            &plasmatica_animation_speed)) {
        SudekiMpLogWrite("plasmatica_animation_speed_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (camera_speed_enabled && !read_config_float(
            config_path,
            L"SudekiMP",
            L"PlasmaticaCameraSpeed",
            1.5f,
            0.25f,
            4.0f,
            &plasmatica_camera_speed)) {
        SudekiMpLogWrite("plasmatica_camera_speed_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }

    SudekiMpLogFormat("quick_menu_patch_requested=%s\r\n",
        patch_enabled ? "true" : "false");
    if (patch_enabled) {
        if (!SudekiMpEnableQuickMenuNormalSpeed(pattern_result.address)) {
            SudekiMpLogFormat("quick_menu_patch_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("quick_menu_patch_applied=false\r\n");
            SudekiMpLogWrite("status=patch_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_PATCH_FAILED;
        }
        SudekiMpLogWrite("quick_menu_patch_applied=true\r\n");
    } else {
        SudekiMpLogWrite("quick_menu_patch_applied=false\r\n");
    }
    SudekiMpLogFormat("plasmatica_trace_requested=%s\r\n",
        trace_enabled ? "true" : "false");
    SudekiMpLogFormat(
        "plasmatica_animation_speed_requested=%s multiplier_bits=0x%08lx\r\n",
        animation_speed_enabled ? "true" : "false",
        (unsigned long)float_bits(plasmatica_animation_speed)
    );
    SudekiMpLogFormat(
        "plasmatica_camera_speed_requested=%s multiplier_bits=0x%08lx\r\n",
        camera_speed_enabled ? "true" : "false",
        (unsigned long)float_bits(plasmatica_camera_speed)
    );
    if (trace_enabled || animation_speed_enabled || camera_speed_enabled) {
        if (!SudekiMpInstallSkillTrace(
                game_module,
                animation_speed_enabled ? plasmatica_animation_speed : 1.0f,
                camera_speed_enabled ? plasmatica_camera_speed : 1.0f)) {
            SudekiMpLogFormat("plasmatica_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("plasmatica_trace_applied=false\r\n");
            SudekiMpLogWrite("status=trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TRACE_FAILED;
        }
        SudekiMpLogWrite("plasmatica_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("plasmatica_trace_applied=false\r\n");
    }
    SudekiMpLogFormat("quick_skill_input_trace_requested=%s\r\n",
        quick_skill_input_trace_enabled ? "true" : "false");
    SudekiMpLogFormat("ranged_quick_skill_prototype_requested=%s\r\n",
        ranged_quick_skill_prototype_enabled ? "true" : "false");
    if (quick_skill_input_trace_enabled || ranged_quick_skill_prototype_enabled) {
        if (!SudekiMpInstallQuickSkillInputTrace(
                game_module,
                ranged_quick_skill_prototype_enabled,
                realtime_multiplayer_skill_combat_enabled)) {
            SudekiMpLogFormat("quick_skill_input_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("quick_skill_input_trace_applied=false\r\n");
            SudekiMpLogWrite("status=input_trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_INPUT_TRACE_FAILED;
        }
        SudekiMpLogWrite("quick_skill_input_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("quick_skill_input_trace_applied=false\r\n");
    }
    SudekiMpLogFormat("character_switch_trace_requested=%s\r\n",
        character_switch_trace_enabled ? "true" : "false");
    if (character_switch_trace_enabled) {
        if (!SudekiMpInstallCharacterSwitchTrace(game_module)) {
            SudekiMpLogFormat("character_switch_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("character_switch_trace_applied=false\r\n");
            SudekiMpLogWrite("status=character_switch_trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CHARACTER_SWITCH_TRACE_FAILED;
        }
        SudekiMpLogWrite("character_switch_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("character_switch_trace_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "freeroam_camera_requested=%s modifier_virtual_key=0x%02lx\r\n",
        freeroam_camera_input_enabled ? "true" : "false",
        (unsigned long)freeroam_camera_modifier_key
    );
    if (freeroam_camera_input_enabled) {
        if (!SudekiMpInstallFreeRoamCameraInput(
                game_module,
                freeroam_camera_modifier_key)) {
            SudekiMpLogFormat("freeroam_camera_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("freeroam_camera_applied=false\r\n");
            SudekiMpLogWrite("status=freeroam_camera_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_FREEROAM_CAMERA_FAILED;
        }
        SudekiMpLogWrite("freeroam_camera_applied=true\r\n");
    } else {
        SudekiMpLogWrite("freeroam_camera_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "control_separation_prototype_requested=%s virtual_key=0x%02lx target=buki second_player_movement=%s camera_relative_movement=%s separation_guard=%s maximum_separation_bits=0x%08lx second_player_weak_attack=%s weak_attack_virtual_key=0x%02lx second_player_skills=%s skill_keys=0x%02lx,0x%02lx,0x%02lx,0x%02lx target_trace=%s shared_group_camera=%s external_input_bridge=%s bridge_port=%d bridge_timeout_ms=%d bridge_deadzone_bits=0x%08lx\r\n",
        control_separation_enabled ? "true" : "false",
        (unsigned long)control_separation_virtual_key,
        second_player_movement_enabled ? "true" : "false",
        second_player_camera_relative_movement_enabled ? "true" : "false",
        second_player_separation_guard_enabled ? "true" : "false",
        (unsigned long)float_bits(second_player_maximum_separation),
        second_player_weak_attack_enabled ? "true" : "false",
        (unsigned long)second_player_weak_attack_virtual_key,
        realtime_multiplayer_skill_combat_enabled ? "true" : "false",
        (unsigned long)second_player_skill_virtual_keys[0],
        (unsigned long)second_player_skill_virtual_keys[1],
        (unsigned long)second_player_skill_virtual_keys[2],
        (unsigned long)second_player_skill_virtual_keys[3],
        second_player_target_trace_enabled ? "true" : "false",
        shared_group_camera_enabled ? "true" : "false",
        external_input_bridge_enabled ? "true" : "false",
        input_bridge_port,
        input_bridge_timeout_ms,
        (unsigned long)float_bits(input_bridge_deadzone)
    );
    if (control_separation_enabled) {
        SudekiMpCombatContextsReset();
        if (external_input_bridge_enabled &&
            !SudekiMpInputBridgeStart(
                (unsigned int)input_bridge_port,
                (DWORD)input_bridge_timeout_ms)) {
            SudekiMpLogFormat("input_bridge_start_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("status=input_bridge_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_INPUT_BRIDGE_FAILED;
        }
        if (realtime_multiplayer_skill_combat_enabled &&
            !SudekiMpInitializeSkillActivationAbi(game_module)) {
            SudekiMpLogFormat(
                "realtime_skill_activation_abi_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpInputBridgeStop();
            SudekiMpLogWrite("status=control_separation_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED;
        }
        if (!SudekiMpInstallControlSeparation(
                game_module,
                control_separation_virtual_key,
                second_player_movement_enabled,
                second_player_camera_relative_movement_enabled,
                second_player_separation_guard_enabled,
                second_player_maximum_separation,
                second_player_weak_attack_enabled,
                second_player_weak_attack_virtual_key,
                realtime_multiplayer_skill_combat_enabled,
                second_player_skill_virtual_keys,
                second_player_target_trace_enabled,
                shared_group_camera_enabled,
                external_input_bridge_enabled,
                input_bridge_deadzone)) {
            SudekiMpLogFormat("control_separation_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpInputBridgeStop();
            SudekiMpLogWrite("control_separation_applied=false\r\n");
            SudekiMpLogWrite("status=control_separation_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED;
        }
        SudekiMpLogWrite("control_separation_applied=true\r\n");
    } else {
        SudekiMpLogWrite("control_separation_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "split_screen_render_prototype_requested=%s layout=left_right camera_policy=%s dual_camera_frame_cache=%s skill_camera_routing=%s second_player_camera_toggle_virtual_key=0x%02lx\r\n",
        split_screen_render_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ?
            "alternating_render_state_frame_cache" :
            (second_player_camera_enabled ?
                "render_only_translated_camera_toggle" :
                "same_native_camera"),
        dual_camera_frame_cache_enabled ? "true" : "false",
        realtime_multiplayer_skill_combat_enabled ?
            "caster_viewport_only" : "disabled",
        (unsigned long)second_player_camera_virtual_key
    );
    if (split_screen_render_enabled) {
        if (!SudekiMpInstallSplitScreenRender(
                game_module,
                second_player_camera_enabled,
                dual_camera_frame_cache_enabled,
                second_player_camera_virtual_key,
                realtime_multiplayer_skill_combat_enabled)) {
            SudekiMpLogFormat(
                "split_screen_render_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpLogWrite("split_screen_render_applied=false\r\n");
            SudekiMpLogWrite("status=split_screen_render_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_SPLIT_SCREEN_RENDER_FAILED;
        }
        SudekiMpLogWrite("split_screen_render_applied=true\r\n");
    } else {
        SudekiMpLogWrite("split_screen_render_applied=false\r\n");
    }
    SudekiMpLogFormat("player_movement_trace_requested=%s\r\n",
        player_movement_trace_enabled ? "true" : "false");
    if (player_movement_trace_enabled) {
        if (!SudekiMpInstallPlayerInputTrace(game_module)) {
            SudekiMpLogFormat("player_movement_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("player_movement_trace_applied=false\r\n");
            SudekiMpLogWrite("status=player_input_trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_PLAYER_INPUT_TRACE_FAILED;
        }
        SudekiMpLogWrite("player_movement_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("player_movement_trace_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "direct_spirit_strike_prototype_requested=%s virtual_key=0x%02lx strike_id=%d variant=%d\r\n",
        direct_spirit_strike_prototype_enabled ? "true" : "false",
        (unsigned long)spirit_strike_virtual_key,
        spirit_strike_id,
        spirit_strike_variant
    );
    if (direct_spirit_strike_prototype_enabled) {
        if (!SudekiMpInstallSpiritStrikeInput(
                game_module,
                spirit_strike_id,
                (unsigned int)spirit_strike_variant,
                spirit_strike_virtual_key)) {
            SudekiMpLogFormat("spirit_strike_input_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("spirit_strike_input_applied=false\r\n");
            SudekiMpLogWrite("status=spirit_input_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_SPIRIT_INPUT_FAILED;
        }
        SudekiMpLogWrite("spirit_strike_input_applied=true\r\n");
    } else {
        SudekiMpLogWrite("spirit_strike_input_applied=false\r\n");
    }
    SudekiMpLogWrite("status=ready\r\n");
    if (!trace_enabled && !animation_speed_enabled && !camera_speed_enabled &&
        !quick_skill_input_trace_enabled && !ranged_quick_skill_prototype_enabled &&
        !realtime_multiplayer_skill_combat_enabled &&
        !direct_spirit_strike_prototype_enabled &&
        !character_switch_trace_enabled &&
        !freeroam_camera_input_enabled &&
        !control_separation_enabled &&
        !split_screen_render_enabled &&
        !player_movement_trace_enabled) {
        SudekiMpLogClose();
    }
    return SUDEKIMP_INIT_OK;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        dll_module = instance;
        DisableThreadLibraryCalls(instance);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (reserved == NULL) {
            SudekiMpInputBridgeStop();
        }
        SudekiMpLogClose();
    }
    return TRUE;
}
