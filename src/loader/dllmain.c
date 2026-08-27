#include "engine/build_identity.h"
#include "cleanroom/menu.h"
#include "engine/log.h"
#include "engine/player_combat_context.h"
#include "engine/player_statehood.h"
#include "engine/skill_activation_abi.h"
#include "hooks/accelerator_cache.h"
#include "hooks/blacksmith_ui_adapter.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/interaction_provenance.h"
#include "hooks/pattern_scan.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_menu.h"
#include "hooks/quick_skill_input.h"
#include "hooks/skill_trace.h"
#include "hooks/split_screen_render.h"
#include "hooks/spirit_strike_input.h"
#include "hooks/talos_defense_trace.h"
#include "hooks/zone_transition_trace.h"
#include "input/bridge_receiver.h"
#include "input/key_binding.h"

#include <windows.h>
#include <math.h>
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
#define SUDEKIMP_INIT_CLEANROOM_MENU_FAILED 15u
#define SUDEKIMP_INIT_ACCELERATOR_CACHE_FAILED 16u
#define SUDEKIMP_INIT_TALOS_DEFENSE_TRACE_FAILED 17u

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
    if (end == value || *end != L'\0' || !isfinite(parsed) ||
        parsed < minimum || parsed > maximum) {
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
    BOOL talos_party_prototype_enabled;
    BOOL talos_defense_trace_enabled;
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
    BOOL skill_camera_routing_enabled;
    BOOL direct_spirit_strike_prototype_enabled;
    BOOL external_input_bridge_enabled;
    BOOL player_interaction_requests_enabled;
    BOOL interaction_provenance_enabled;
    BOOL experimental_blacksmith_ui_enabled;
    BOOL second_player_controller_camera_enabled;
    BOOL native_second_player_camera_collision_enabled;
    BOOL split_screen_ranged_model_isolation_enabled;
    BOOL spirit_strike_viewport_effect_isolation_enabled;
    BOOL zone_transition_trace_enabled;
    BOOL party_atomic_transitions_enabled;
    BOOL transition_vote_enabled;
    BOOL zone_traversal_enabled;
    BOOL cleanroom_menu_enabled;
    BOOL coop_roster_menu_enabled;
    BOOL skip_startup_movies;
    BOOL story_test_boost_enabled;
    BOOL cleanroom_multiplayer_integration;
    BOOL defer_integrated_roster;
    wchar_t spirit_strike_key_text[32];
    wchar_t control_separation_key_text[32];
    wchar_t second_player_weak_attack_key_text[32];
    wchar_t second_player_camera_key_text[32];
    wchar_t second_player_skill_key_text[4][32];
    wchar_t freeroam_camera_modifier_text[32];
    wchar_t cleanroom_menu_key_text[32];
    wchar_t zone_traversal_menu_key_text[32];
    wchar_t story_test_boost_key_text[32];
    UINT spirit_strike_virtual_key = 'G';
    UINT control_separation_virtual_key = 'J';
    UINT second_player_weak_attack_virtual_key = 'U';
    UINT second_player_camera_virtual_key = VK_F9;
    UINT second_player_skill_virtual_keys[4] = {
        VK_F1, VK_F2, VK_F3, VK_F4
    };
    UINT freeroam_camera_modifier_key = VK_LCONTROL;
    UINT cleanroom_menu_virtual_key = VK_F8;
    UINT zone_traversal_menu_virtual_key = VK_F7;
    UINT story_test_boost_virtual_key = VK_F6;
    int spirit_strike_id = -1;
    int spirit_strike_variant = 1;
    int input_bridge_port = 26760;
    int input_bridge_timeout_ms = 250;
    float plasmatica_animation_speed = 1.0f;
    float plasmatica_camera_speed = 1.0f;
    float second_player_maximum_separation = 10.0f;
    float input_bridge_deadzone = 0.20f;
    float second_player_controller_camera_yaw_speed = 2.25f;
    float second_player_controller_camera_pitch_speed = 1.50f;
    float second_player_controller_camera_maximum_pitch = 0.65f;
    float story_test_boost_multiplier = 2.0f;

    (void)unused;
    if (GetModuleFileNameW(NULL, game_path, MAX_PATH) == 0) {
        return SUDEKIMP_INIT_BAD_PATH;
    }
    if (!SudekiMpLogOpenBesideGame(game_path)) {
        return SUDEKIMP_INIT_BAD_PATH;
    }

    SudekiMpLogWrite("SudekiMP 0.1.0\r\n");
    SudekiMpLogFormat(
        "event=process_attach pid=%lu\r\n",
        (unsigned long)GetCurrentProcessId());
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

    SudekiMpLogWrite("accelerator_cache_requested=true\r\n");
    if (!SudekiMpInstallAcceleratorCache(game_module)) {
        SudekiMpLogFormat("accelerator_cache_error=%lu\r\n",
            (unsigned long)GetLastError());
        SudekiMpLogWrite("accelerator_cache_applied=false\r\n");
        SudekiMpLogWrite("status=accelerator_cache_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_ACCELERATOR_CACHE_FAILED;
    }
    SudekiMpLogWrite("accelerator_cache_applied=true\r\n");

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
    talos_party_prototype_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableTalosPartyPrototype"
    );
    talos_defense_trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableTalosDefenseTrace"
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
    player_interaction_requests_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlayerInteractionRequestsPrototype"
    );
    experimental_blacksmith_ui_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePerPlayerBlacksmithUiExperiment"
    );
    second_player_controller_camera_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSecondPlayerControllerCameraPrototype"
    );
    native_second_player_camera_collision_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableNativeSecondPlayerCameraCollisionPrototype"
    );
    split_screen_ranged_model_isolation_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSplitScreenRangedModelIsolationPrototype"
    );
    spirit_strike_viewport_effect_isolation_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSpiritStrikeViewportEffectIsolationPrototype"
    );
    zone_transition_trace_enabled = GetEnvironmentVariableA(
        "SUDEKIMP_ZONE_TRACE",
        NULL,
        0u
    ) > 0u;
    skill_camera_routing_enabled =
        realtime_multiplayer_skill_combat_enabled ||
        spirit_strike_viewport_effect_isolation_enabled;
    cleanroom_menu_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableCleanroomMenu"
    );
    zone_traversal_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableZoneTraversalMenu"
    );
    party_atomic_transitions_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePartyAtomicTransitionsPrototype"
    );
    transition_vote_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableTransitionVotePrototype"
    );
    zone_transition_trace_enabled = zone_transition_trace_enabled ||
        zone_traversal_enabled || party_atomic_transitions_enabled ||
        transition_vote_enabled;
    interaction_provenance_enabled =
        player_interaction_requests_enabled && zone_transition_trace_enabled;
    coop_roster_menu_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableCoopRosterMenu"
    );
    skip_startup_movies = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"SkipStartupMovies"
    );
    story_test_boost_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableStoryTestBoost"
    );
    if (coop_roster_menu_enabled &&
        (cleanroom_menu_enabled || zone_traversal_enabled)) {
        SudekiMpLogWrite(
            "coop_roster_menu_config=invalid reason=mutually_exclusive_with_cleanroom_menu\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (party_atomic_transitions_enabled &&
        (!coop_roster_menu_enabled || !control_separation_enabled ||
         !split_screen_render_enabled || !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled || zone_traversal_enabled)) {
        SudekiMpLogWrite(
            "party_atomic_transitions_config=invalid "
            "reason=requires_roster_control_split_p2_camera_dual_cache_and_no_traversal_menu\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (transition_vote_enabled &&
        (!party_atomic_transitions_enabled ||
         !external_input_bridge_enabled)) {
        SudekiMpLogWrite(
            "transition_vote_config=invalid "
            "reason=requires_party_atomic_transitions_and_external_input_bridge\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    cleanroom_multiplayer_integration =
        (cleanroom_menu_enabled || coop_roster_menu_enabled) &&
        control_separation_enabled && split_screen_render_enabled;
    defer_integrated_roster = coop_roster_menu_enabled &&
        cleanroom_multiplayer_integration;
    if (second_player_separation_guard_enabled &&
        !cleanroom_multiplayer_integration) {
        SudekiMpLogWrite(
            "roaming_boundary_config=invalid reason=requires_integrated_menu_and_split_overlay\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (player_interaction_requests_enabled &&
        (!coop_roster_menu_enabled || !cleanroom_multiplayer_integration ||
         !external_input_bridge_enabled || !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "player_interaction_requests_config=invalid "
            "reason=requires_coop_roster_integrated_menu_control_split_dual_cache_and_external_bridge\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (experimental_blacksmith_ui_enabled &&
        (!coop_roster_menu_enabled || !cleanroom_multiplayer_integration ||
         !external_input_bridge_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "per_player_blacksmith_ui_config=invalid "
            "reason=requires_integrated_coop_roster_control_split_player_two_camera_dual_cache_and_external_bridge\r\n");
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
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
        L"ToggleSecondPlayerAi",
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
    GetPrivateProfileStringW(
        L"Bindings",
        L"ToggleCleanroomMenu",
        L"F8",
        cleanroom_menu_key_text,
        (DWORD)(sizeof(cleanroom_menu_key_text) /
            sizeof(cleanroom_menu_key_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"ToggleZoneTraversalMenu",
        L"F7",
        zone_traversal_menu_key_text,
        (DWORD)(sizeof(zone_traversal_menu_key_text) /
            sizeof(zone_traversal_menu_key_text[0])),
        config_path
    );
    GetPrivateProfileStringW(
        L"Bindings",
        L"ToggleStoryTestBoost",
        L"F6",
        story_test_boost_key_text,
        (DWORD)(sizeof(story_test_boost_key_text) /
            sizeof(story_test_boost_key_text[0])),
        config_path
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
    if (cleanroom_menu_enabled &&
        !SudekiMpParseInputKey(
            cleanroom_menu_key_text,
            &cleanroom_menu_virtual_key)) {
        SudekiMpLogWrite("cleanroom_menu_key_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (zone_traversal_enabled &&
        !SudekiMpParseInputKey(
            zone_traversal_menu_key_text,
            &zone_traversal_menu_virtual_key)) {
        SudekiMpLogWrite("zone_traversal_menu_key_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (story_test_boost_enabled &&
        (!coop_roster_menu_enabled ||
         !SudekiMpParseInputKey(
             story_test_boost_key_text,
             &story_test_boost_virtual_key) ||
         !read_config_float(
             config_path,
             L"SudekiMP",
             L"StoryTestBoostMultiplier",
             2.0f,
             1.0f,
             4.0f,
             &story_test_boost_multiplier))) {
        SudekiMpLogWrite(
            "story_test_boost_config=invalid "
            "reason=requires_coop_roster_finite_multiplier_1_to_4\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (cleanroom_menu_enabled &&
        (player_movement_trace_enabled ||
         (control_separation_enabled != split_screen_render_enabled))) {
        SudekiMpLogWrite(
            "cleanroom_menu_config=requires_standalone_or_complete_multiplayer_hook_pair\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (coop_roster_menu_enabled &&
        (control_separation_enabled != split_screen_render_enabled)) {
        SudekiMpLogWrite(
            "coop_roster_menu_config=requires_standalone_or_complete_multiplayer_hook_pair\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
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
    if (second_player_separation_guard_enabled &&
        player_movement_trace_enabled) {
        SudekiMpLogWrite(
            "roaming_boundary_config=conflicts_with_player_movement_trace_same_native_callsites\r\n"
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
    if (second_player_controller_camera_enabled &&
        (!external_input_bridge_enabled ||
         !second_player_camera_relative_movement_enabled ||
         !split_screen_render_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "second_player_controller_camera_config=requires_external_bridge_camera_relative_movement_split_p2_camera_dual_cache\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (native_second_player_camera_collision_enabled &&
        (!split_screen_render_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "native_second_player_camera_collision_config=requires_split_p2_camera_dual_cache\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (split_screen_ranged_model_isolation_enabled &&
        (!split_screen_render_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "split_screen_ranged_model_isolation_config=requires_split_p2_camera_dual_cache\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (spirit_strike_viewport_effect_isolation_enabled &&
        (!split_screen_render_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "spirit_strike_viewport_effect_isolation_config=requires_split_p2_camera_dual_cache\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_controller_camera_enabled &&
        (!read_config_float(
             config_path,
             L"SudekiMP",
             L"SecondPlayerControllerCameraYawSpeed",
             2.25f,
             0.1f,
             10.0f,
             &second_player_controller_camera_yaw_speed) ||
         !read_config_float(
             config_path,
             L"SudekiMP",
             L"SecondPlayerControllerCameraPitchSpeed",
             1.50f,
             0.1f,
             10.0f,
             &second_player_controller_camera_pitch_speed) ||
         !read_config_float(
             config_path,
             L"SudekiMP",
             L"SecondPlayerControllerCameraMaximumPitch",
             0.65f,
             0.1f,
             1.2f,
             &second_player_controller_camera_maximum_pitch))) {
        SudekiMpLogWrite(
            "second_player_controller_camera_config=invalid\r\n"
        );
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

    /* Runtime interaction provenance is process-local only.  Initialize it
     * before any menu/input/render hook can publish a player or modal lease. */
    SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehoodRuntime());

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
    SudekiMpLogFormat(
        "character_switch_trace_requested=%s talos_party_prototype=%s\r\n",
        character_switch_trace_enabled ? "true" : "false",
        talos_party_prototype_enabled ? "true" : "false");
    if (character_switch_trace_enabled || talos_party_prototype_enabled) {
        if (!SudekiMpInstallCharacterSwitchTrace(
                game_module,
                talos_party_prototype_enabled)) {
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
        "talos_defense_trace_requested=%s\r\n",
        talos_defense_trace_enabled ? "true" : "false"
    );
    if (talos_defense_trace_enabled) {
        if (!SudekiMpInstallTalosDefenseTrace(game_module)) {
            SudekiMpLogFormat("talos_defense_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("talos_defense_trace_applied=false\r\n");
            SudekiMpLogWrite("status=talos_defense_trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_DEFENSE_TRACE_FAILED;
        }
        SudekiMpLogWrite("talos_defense_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("talos_defense_trace_applied=false\r\n");
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
        "cleanroom_menu_requested=%s virtual_key=0x%02lx\r\n",
        cleanroom_menu_enabled ? "true" : "false",
        (unsigned long)cleanroom_menu_virtual_key
    );
    SudekiMpLogFormat(
        "coop_roster_menu_requested=%s virtual_key=0x%02lx\r\n",
        coop_roster_menu_enabled ? "true" : "false",
        (unsigned long)cleanroom_menu_virtual_key
    );
    SudekiMpLogFormat(
        "story_test_boost_requested=%s virtual_key=0x%02lx "
        "multiplier_bits=0x%08lx default=off "
        "policy=master_game_speed_plus_native_party_invulnerability\r\n",
        story_test_boost_enabled ? "true" : "false",
        (unsigned long)story_test_boost_virtual_key,
        (unsigned long)float_bits(story_test_boost_multiplier)
    );
    SudekiMpLogFormat(
        "zone_traversal_menu_requested=%s virtual_key=0x%02lx\r\n",
        zone_traversal_enabled ? "true" : "false",
        (unsigned long)zone_traversal_menu_virtual_key
    );
    if (coop_roster_menu_enabled && !defer_integrated_roster) {
        if (!SudekiMpInstallCoopRosterMenu(
                game_module,
                cleanroom_menu_virtual_key,
                cleanroom_multiplayer_integration,
                skip_startup_movies,
                story_test_boost_enabled,
                story_test_boost_virtual_key,
                story_test_boost_multiplier
            )) {
            SudekiMpLogFormat(
                "coop_roster_menu_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpLogWrite("coop_roster_menu_applied=false\r\n");
            SudekiMpLogWrite("status=coop_roster_menu_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CLEANROOM_MENU_FAILED;
        }
        SudekiMpLogWrite("coop_roster_menu_applied=true\r\n");
    } else if (defer_integrated_roster) {
        SudekiMpLogWrite(
            "coop_roster_menu_applied=pending_after_split_preflight\r\n"
        );
    } else {
        SudekiMpLogWrite("coop_roster_menu_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "zone_transition_trace_requested=%s "
        "party_atomic_transitions_requested=%s transition_vote_requested=%s\r\n",
        zone_transition_trace_enabled ? "true" : "false",
        party_atomic_transitions_enabled ? "true" : "false",
        transition_vote_enabled ? "true" : "false"
    );
    if (zone_transition_trace_enabled) {
        if (!SudekiMpZoneTransitionConfigureVote(
                transition_vote_enabled) ||
            !SudekiMpInstallZoneTransitionTrace(
                game_module,
                party_atomic_transitions_enabled,
                !skill_camera_routing_enabled)) {
            SudekiMpLogFormat(
                "zone_transition_trace_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpLogWrite("zone_transition_trace_applied=false\r\n");
            SudekiMpLogWrite("status=zone_transition_trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TRACE_FAILED;
        }
        SudekiMpLogWrite("zone_transition_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("zone_transition_trace_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "interaction_provenance_trace_requested=%s "
        "zone_transition_trace_ready=%s "
        "effective=%s mutation=disabled\r\n",
        player_interaction_requests_enabled ? "true" : "false",
        zone_transition_trace_enabled ? "true" : "false",
        interaction_provenance_enabled ? "true" : "false");
    if (!SudekiMpInstallInteractionProvenance(
            game_module, interaction_provenance_enabled)) {
        DWORD provenance_error = GetLastError();

        SudekiMpUninstallInteractionProvenance();
        SudekiMpUninstallZoneTransitionTrace();
        SudekiMpLogFormat(
            "interaction_provenance_trace_error=%lu "
            "phase=exact_observation_hook_install\r\n",
            (unsigned long)provenance_error);
        SudekiMpLogWrite(
            "interaction_provenance_trace_applied=false "
            "rollback=all_provenance_hooks_and_zone_generation\r\n");
        SudekiMpLogWrite("status=interaction_provenance_trace_error\r\n");
        SudekiMpLogClose();
        SetLastError(provenance_error);
        return SUDEKIMP_INIT_TRACE_FAILED;
    }
    if (interaction_provenance_enabled) {
        SudekiMpLogWrite(
            "interaction_provenance_trace_applied=true "
            "policy=passive_actor_target_observation_only_"
            "p2_activation_fail_closed\r\n");
    } else {
        SudekiMpLogFormat(
            "interaction_provenance_trace_applied=false reason=%s "
            "default=false\r\n",
            player_interaction_requests_enabled ?
                "zone_transition_trace_required" : "config_disabled");
    }
    if (cleanroom_menu_enabled || zone_traversal_enabled) {
        BOOL cleanroom_installed = zone_traversal_enabled &&
                !cleanroom_menu_enabled && !coop_roster_menu_enabled ?
            SudekiMpInstallZoneTraversalMenu(
                game_module, zone_traversal_menu_virtual_key,
                skip_startup_movies) :
            (cleanroom_multiplayer_integration ?
            SudekiMpInstallIntegratedCleanroomMenu(
                game_module,
                cleanroom_menu_virtual_key
            ) :
            SudekiMpInstallCleanroomMenu(
                game_module,
                cleanroom_menu_virtual_key));

        if (!cleanroom_installed) {
            SudekiMpLogFormat(
                "cleanroom_menu_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpLogWrite("cleanroom_menu_applied=false\r\n");
            SudekiMpLogWrite("status=cleanroom_menu_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CLEANROOM_MENU_FAILED;
        }
        SudekiMpLogFormat(
            "cleanroom_menu_applied=true traversal=%s toggle_key=0x%02lx\r\n",
            zone_traversal_enabled ? "true" : "false",
            (unsigned long)(zone_traversal_enabled && !cleanroom_menu_enabled ?
                zone_traversal_menu_virtual_key : cleanroom_menu_virtual_key));
    } else {
        SudekiMpLogWrite("cleanroom_menu_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "control_separation_prototype_requested=%s virtual_key=0x%02lx target_policy=first_non_front_active_party_member second_player_movement=%s camera_relative_movement=%s separation_guard=%s maximum_separation_bits=0x%08lx second_player_weak_attack=%s weak_attack_virtual_key=0x%02lx second_player_skills=%s skill_keys=0x%02lx,0x%02lx,0x%02lx,0x%02lx target_trace=%s shared_group_camera=%s external_input_bridge=%s bridge_port=%d bridge_timeout_ms=%d bridge_deadzone_bits=0x%08lx\r\n",
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
    SudekiMpLogFormat(
        "interaction_provenance_and_intent_trace_requested=%s "
        "button=controller_a "
        "policy=passive_exact_actor_target_source_generation_trace_"
        "intent_only_no_targetless_request_no_native_world_action\r\n",
        player_interaction_requests_enabled ? "true" : "false"
    );
    SudekiMpLogFormat(
        "per_player_blacksmith_ui_experiment_requested=%s "
        "default=false mutation=disabled "
        "policy=exact_gated_start_and_active_script_contract_two_native_inert_panels\r\n",
        experimental_blacksmith_ui_enabled ? "true" : "false");
    if (control_separation_enabled) {
        SudekiMpCombatContextsReset();
        if (external_input_bridge_enabled &&
            !SudekiMpInputBridgeStart(
                (unsigned int)input_bridge_port,
                (DWORD)input_bridge_timeout_ms)) {
            SudekiMpLogFormat("input_bridge_start_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("status=input_bridge_error\r\n");
            SudekiMpUninstallInteractionProvenance();
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
            SudekiMpUninstallInteractionProvenance();
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
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED;
        }
        SudekiMpControlSeparationSetInteractionRequestsEnabled(
            player_interaction_requests_enabled);
        SudekiMpLogWrite("control_separation_applied=true\r\n");
    } else {
        SudekiMpLogWrite("control_separation_applied=false\r\n");
    }
    SudekiMpLogFormat(
        "split_screen_render_prototype_requested=%s layout=left_right camera_policy=%s dual_camera_frame_cache=%s skill_camera_routing=%s second_player_controller_camera=%s native_second_player_camera_collision=%s split_screen_ranged_model_isolation=%s spirit_strike_viewport_effect_isolation=%s controller_camera_yaw_speed_bits=0x%08lx controller_camera_pitch_speed_bits=0x%08lx controller_camera_maximum_pitch_bits=0x%08lx second_player_camera_toggle_virtual_key=0x%02lx\r\n",
        split_screen_render_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ?
            "alternating_render_state_frame_cache" :
            (second_player_camera_enabled ?
                "render_only_translated_camera_toggle" :
                "same_native_camera"),
        dual_camera_frame_cache_enabled ? "true" : "false",
        skill_camera_routing_enabled ?
            "caster_viewport_only" : "disabled",
        second_player_controller_camera_enabled ? "true" : "false",
        native_second_player_camera_collision_enabled ? "true" : "false",
        split_screen_ranged_model_isolation_enabled ? "true" : "false",
        spirit_strike_viewport_effect_isolation_enabled ? "true" : "false",
        (unsigned long)float_bits(second_player_controller_camera_yaw_speed),
        (unsigned long)float_bits(second_player_controller_camera_pitch_speed),
        (unsigned long)float_bits(second_player_controller_camera_maximum_pitch),
        (unsigned long)second_player_camera_virtual_key
    );
    if (split_screen_render_enabled) {
        if (!SudekiMpInstallSplitScreenRender(
                game_module,
                second_player_camera_enabled,
                dual_camera_frame_cache_enabled,
                second_player_camera_virtual_key,
                skill_camera_routing_enabled,
                second_player_controller_camera_enabled,
                native_second_player_camera_collision_enabled,
                split_screen_ranged_model_isolation_enabled,
                spirit_strike_viewport_effect_isolation_enabled,
                input_bridge_deadzone,
                second_player_controller_camera_yaw_speed,
                second_player_controller_camera_pitch_speed,
                second_player_controller_camera_maximum_pitch)) {
            DWORD split_screen_error = GetLastError();

            if (control_separation_enabled) {
                (void)SudekiMpControlSeparationSetInteractionRequestsEnabled(
                    FALSE);
                SudekiMpUninstallControlSeparation();
                SudekiMpInputBridgeStop();
                SudekiMpLogWrite(
                    "control_separation_applied=false "
                    "phase=split_screen_install_rollback "
                    "policy=remove_controller_hook_and_runtime_interaction_state\r\n"
                );
            }
            SudekiMpLogFormat(
                "split_screen_render_error=%lu\r\n",
                (unsigned long)split_screen_error
            );
            SudekiMpLogWrite("split_screen_render_applied=false\r\n");
            SudekiMpLogWrite("status=split_screen_render_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            SetLastError(split_screen_error);
            return SUDEKIMP_INIT_SPLIT_SCREEN_RENDER_FAILED;
        }
        SudekiMpLogWrite("split_screen_render_applied=true\r\n");
        if (cleanroom_multiplayer_integration) {
            (void)SudekiMpSplitScreenSetRuntimeEnabled(FALSE);
        }
    } else {
        SudekiMpLogWrite("split_screen_render_applied=false\r\n");
    }
    if (defer_integrated_roster) {
        if (!SudekiMpInstallCoopRosterMenu(
                game_module,
                cleanroom_menu_virtual_key,
                TRUE,
                skip_startup_movies,
                story_test_boost_enabled,
                story_test_boost_virtual_key,
                story_test_boost_multiplier
            )) {
            SudekiMpLogFormat(
                "coop_roster_menu_error=%lu phase=deferred_integrated_install\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpLogWrite("coop_roster_menu_applied=false\r\n");
            SudekiMpLogWrite("status=coop_roster_menu_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CLEANROOM_MENU_FAILED;
        }
        SudekiMpLogWrite(
            "coop_roster_menu_applied=true phase=after_split_preflight\r\n"
        );
    }
    if (cleanroom_multiplayer_integration) {
        SudekiMpControlSeparationSetUpdateObserver(
            SudekiMpCleanroomMenuUpdate
        );
        SudekiMpSplitScreenSetOverlayRenderer(
            SudekiMpCleanroomMenuRender
        );
        SudekiMpLogWrite(
            "menu_multiplayer_integration=ready "
            "default=disabled input=external_razer_bridge\r\n"
        );
    }
    SudekiMpLogFormat("player_movement_trace_requested=%s\r\n",
        player_movement_trace_enabled ? "true" : "false");
    if (player_movement_trace_enabled) {
        if (!SudekiMpInstallPlayerInputTrace(game_module)) {
            SudekiMpLogFormat("player_movement_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("player_movement_trace_applied=false\r\n");
            SudekiMpLogWrite("status=player_input_trace_error\r\n");
            SudekiMpUninstallInteractionProvenance();
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
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_SPIRIT_INPUT_FAILED;
        }
        SudekiMpLogWrite("spirit_strike_input_applied=true\r\n");
    } else {
        SudekiMpLogWrite("spirit_strike_input_applied=false\r\n");
    }
    /* Install the paired script-export hook last so no subsequent
     * initialization failure can leave an accepted custom Active lease in a
     * partially initialized runtime. */
    if (experimental_blacksmith_ui_enabled) {
        if (!SudekiMpInstallBlacksmithUiAdapter(game_module, TRUE)) {
            SudekiMpLogFormat(
                "per_player_blacksmith_ui_error=%lu "
                "phase=exact_start_active_hook_install\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite(
                "per_player_blacksmith_ui_applied=false\r\n");
            SudekiMpLogWrite("status=cleanroom_menu_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CLEANROOM_MENU_FAILED;
        }
        SudekiMpSplitScreenSetModOwnedBlacksmithActiveQuery(
            SudekiMpBlacksmithUiAdapterActive);
        SudekiMpLogWrite(
            "per_player_blacksmith_ui_applied=true "
            "native_commit=disabled fallback=native_when_prerequisites_fail\r\n");
    } else {
        SudekiMpLogWrite(
            "per_player_blacksmith_ui_applied=false default=false\r\n");
    }
    SudekiMpLogWrite("status=ready\r\n");
    if (!trace_enabled && !animation_speed_enabled && !camera_speed_enabled &&
        !quick_skill_input_trace_enabled && !ranged_quick_skill_prototype_enabled &&
        !realtime_multiplayer_skill_combat_enabled &&
        !direct_spirit_strike_prototype_enabled &&
        !character_switch_trace_enabled &&
        !talos_defense_trace_enabled &&
        !freeroam_camera_input_enabled &&
        !control_separation_enabled &&
        !split_screen_render_enabled &&
        !cleanroom_menu_enabled &&
        !coop_roster_menu_enabled &&
        !player_movement_trace_enabled &&
        !zone_transition_trace_enabled &&
        !zone_traversal_enabled) {
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
            SudekiMpSplitScreenSetModOwnedBlacksmithActiveQuery(NULL);
            SudekiMpUninstallBlacksmithUiAdapter();
            SudekiMpUninstallTalosDefenseTrace();
            SudekiMpUninstallInteractionProvenance();
            SudekiMpUninstallZoneTransitionTrace();
            SudekiMpInputBridgeStop();
            SudekiMpUninstallAcceleratorCache();
        }
        SudekiMpLogClose();
    }
    return TRUE;
}
