#include "engine/build_identity.h"
#include "cleanroom/engine.h"
#include "cleanroom/menu.h"
#include "engine/log.h"
#include "engine/player_combat_context.h"
#include "engine/player_statehood.h"
#include "engine/sha256.h"
#include "engine/skill_activation_abi.h"
#include "engine/spirit_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "engine/item_activation_abi.h"
#include "engine/local_quick_menu.h"
#include "hooks/accelerator_cache.h"
#include "hooks/blacksmith_ui_adapter.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/interaction_provenance.h"
#include "hooks/lan_arena_runtime.h"
#include "hooks/lan_arena_pause_panel.h"
#include "hooks/lan_arena_window_policy.h"
#include "hooks/merchant_provenance_adapter.h"
#include "hooks/pattern_scan.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_menu.h"
#include "hooks/quick_skill_input.h"
#include "hooks/save_book_intercept.h"
#include "hooks/skill_trace.h"
#include "hooks/split_screen_render.h"
#include "hooks/spirit_strike_input.h"
#include "hooks/talos_companion_membership_abi.h"
#include "hooks/talos_companion_staging_native_capture.h"
#include "hooks/talos_companion_staging_research_adapter.h"
#include "hooks/talos_native_lifecycle_trace.h"
#include "hooks/talos_post_movie_party_restore.h"
#include "hooks/talos_defense_trace.h"
#include "hooks/xinput_player_two.h"
#include "hooks/zone_transition_trace.h"
#include "input/bridge_receiver.h"
#include "input/key_binding.h"
#include "input/local_input_hub.h"
#include "loader/fixed_three_profile.h"
#include "loader/lan_arena_profile.h"

#include <windows.h>
#include <wincrypt.h>
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
#define SUDEKIMP_INIT_SAVE_BOOK_VOTE_FAILED 18u
#define SUDEKIMP_INIT_TALOS_LIFECYCLE_TRACE_FAILED 19u
#define SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED 20u
#define SUDEKIMP_INIT_TALOS_POST_MOVIE_RESTORE_FAILED 21u
#define SUDEKIMP_INIT_LAN_ARENA_FAILED 22u
#define SUDEKIMP_TALOS_EXACT_SOL_SHA256 \
    "e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9"

static HMODULE dll_module;
static char cleanroom_update_observer_owner;
static SudekiMpControlUpdateObserverGate cleanroom_update_observer_gate;
static char talos_staging_observation_owner;
static SudekiMpControlUpdateObserverGate talos_staging_observation_gate;
static volatile LONG talos_staging_observation_install_started;
static uint64_t talos_staging_observation_last_logged_attempts;
static uint8_t lan_arena_game_hash[32];

static BOOL talos_post_movie_dual_camera_authorized(void) {
    DWORD entry_error = GetLastError();
    SudekiMpTalosPostMoviePartyRestoreStatus status;
    BOOL authorized =
        SudekiMpTalosPostMoviePartyRestoreGetStatus(&status) &&
        SudekiMpTalosPostMoviePartyRestoreCameraAuthorized(&status);

    SetLastError(entry_error);
    return authorized;
}

static BOOL build_sol_world_path(
    const wchar_t *game_path,
    wchar_t *sol_path,
    size_t sol_path_capacity
) {
    static const wchar_t suffix[] = L"Data\\SOLWORLDM.gex";
    const wchar_t *slash;
    size_t prefix_length;
    size_t suffix_length = (sizeof(suffix) / sizeof(suffix[0])) - 1u;

    if (game_path == NULL || sol_path == NULL || sol_path_capacity == 0u) {
        return FALSE;
    }
    slash = wcsrchr(game_path, L'\\');
    if (slash == NULL) slash = wcsrchr(game_path, L'/');
    prefix_length = slash == NULL ? 0u : (size_t)(slash - game_path) + 1u;
    if (prefix_length + suffix_length + 1u > sol_path_capacity) {
        return FALSE;
    }
    if (prefix_length != 0u) {
        memcpy(sol_path, game_path, prefix_length * sizeof(sol_path[0]));
    }
    memcpy(sol_path + prefix_length, suffix, sizeof(suffix));
    return TRUE;
}

static BOOL generate_talos_staging_observation_identity(
    uint64_t *process_token,
    uint64_t *identity_salt
) {
    HCRYPTPROV provider = 0;
    uint8_t random_bytes[16];
    BOOL generated;

    if (process_token == NULL || identity_salt == NULL) return FALSE;
    generated = CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
        CRYPT_VERIFYCONTEXT | CRYPT_SILENT) &&
        CryptGenRandom(provider, (DWORD)sizeof(random_bytes), random_bytes);
    if (provider != 0) CryptReleaseContext(provider, 0);
    if (!generated) return FALSE;
    memcpy(process_token, random_bytes, sizeof(*process_token));
    memcpy(identity_salt, random_bytes + sizeof(*process_token),
        sizeof(*identity_salt));
    *process_token |= UINT64_C(1);
    *identity_salt |= UINT64_C(1);
    if (*identity_salt == *process_token) {
        *identity_salt ^= UINT64_C(0x9e3779b97f4a7c15);
        if (*identity_salt == 0u) *identity_salt = UINT64_C(1);
    }
    SecureZeroMemory(random_bytes, sizeof(random_bytes));
    return TRUE;
}

static BOOL talos_staging_observation_sink(
    void *context,
    const SudekiMpTalosStagingNativeCaptureStatus *status,
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)context;
    (void)status;
    return SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
        result, witness);
}

static void talos_staging_observation_control_update_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD entry_error = GetLastError();
    SudekiMpTalosStagingNativeCaptureStatus status;

    if (!SudekiMpControlUpdateObserverGateTryEnter(
            &talos_staging_observation_gate)) {
        SetLastError(entry_error);
        return;
    }
    SudekiMpTalosCompanionStagingNativeCaptureService(
        controller, update_data, witness);
    /* Logging is deliberately outside CaptureService's final immutable
     * capture window. Only real attempts, not throttled callbacks, emit. */
    if (SudekiMpTalosCompanionStagingNativeCaptureGetStatus(&status, NULL) &&
        status.active == 0u &&
        status.attempts != talos_staging_observation_last_logged_attempts) {
        talos_staging_observation_last_logged_attempts = status.attempts;
        SudekiMpLogFormat(
            "talos_companion_staging_observation attempt=%lu "
            "failure=%lu sampler_failure=%lu planning_passes=%lu ranges=%lu "
            "capture_bytes=%lu witness_exact=%s sink_published=%s "
            "valid=%s policy=read_only_no_membership_calls\r\n",
            (unsigned long)status.attempts,
            (unsigned long)status.failure,
            (unsigned long)status.sampler_failure,
            (unsigned long)status.planning_passes,
            (unsigned long)status.final_range_count,
            (unsigned long)status.final_capture_bytes,
            status.witness_revalidated_exact ? "true" : "false",
            status.sink_published ? "true" : "false",
            status.completed_valid ? "true" : "false"
        );
    }
    SudekiMpControlUpdateObserverGateLeave(
        &talos_staging_observation_gate);
    SetLastError(entry_error);
}

static void uninstall_talos_staging_observation(void) {
    DWORD entry_error = GetLastError();

    if (InterlockedCompareExchange(
            &talos_staging_observation_install_started, 0, 0) == 0) {
        SetLastError(entry_error);
        return;
    }
    SudekiMpControlUpdateObserverGateDisable(
        &talos_staging_observation_gate);
    (void)SudekiMpControlSeparationUnregisterUpdateObserver(
        &talos_staging_observation_owner);
    SudekiMpControlUpdateObserverGateDrain(
        &talos_staging_observation_gate);
    (void)SudekiMpTalosCompanionStagingNativeCaptureReset();
    SudekiMpUninstallTalosCompanionStagingResearchAdapter();
    SudekiMpUninstallControlSeparation();
    (void)InterlockedExchange(
        &talos_staging_observation_install_started, 0);
    talos_staging_observation_last_logged_attempts = 0u;
    SetLastError(entry_error);
}

static void cleanroom_control_update_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)controller;
    (void)update_data;
    if (!SudekiMpControlUpdateObserverGateTryEnter(
            &cleanroom_update_observer_gate)) {
        return;
    }
    if (!SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness)) {
        SudekiMpControlUpdateObserverGateLeave(
            &cleanroom_update_observer_gate);
        return;
    }
    SudekiMpCleanroomMenuUpdate();
    SudekiMpControlUpdateObserverGateLeave(
        &cleanroom_update_observer_gate);
}

static void uninstall_runtime_hooks(void) {
    SudekiMpUninstallLanArenaPausePanel();
    SudekiMpUninstallLanArenaRuntime();
    (void)SudekiMpUninstallLanArenaWindowPolicy();
    SudekiMpUninstallTalosPostMoviePartyRestore();
    uninstall_talos_staging_observation();
    SudekiMpControlUpdateObserverGateDisable(
        &cleanroom_update_observer_gate);
    (void)SudekiMpControlSeparationUnregisterUpdateObserver(
        &cleanroom_update_observer_owner);
    SudekiMpControlUpdateObserverGateDrain(
        &cleanroom_update_observer_gate);
    SudekiMpUninstallTalosNativeLifecycleTrace();
    SudekiMpUninstallSaveBookIntercept();
    SudekiMpUninstallMerchantProvenanceAdapter();
    SudekiMpSplitScreenSetModOwnedBlacksmithActiveQuery(NULL);
    SudekiMpUninstallBlacksmithUiAdapter();
    SudekiMpUninstallSpiritStrikeInput();
    SudekiMpUninstallPlayerInputTrace();
    SudekiMpUninstallCleanroomMenu();
    SudekiMpUninstallSplitScreenRender();
    SudekiMpUninstallControlSeparation();
    SudekiMpUninstallXInputPlayerTwoReservation();
    SudekiMpInputBridgeStop();
    SudekiMpUninstallInteractionProvenance();
    SudekiMpUninstallZoneTransitionTrace();
    SudekiMpUninstallFreeRoamCameraInput();
    SudekiMpUninstallTalosDefenseTrace();
    SudekiMpUninstallCharacterSwitchTrace();
    SudekiMpUninstallQuickSkillInputTrace();
    SudekiMpUninstallSkillTrace();
    SudekiMpResetItemActivationAbi();
    SudekiMpResetWeaponActivationAbi();
    SudekiMpResetSpiritActivationAbi();
    SudekiMpResetSkillActivationAbi();
    (void)SudekiMpUninstallAcceleratorCache();
}

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
    return SudekiMpConfigBooleanTextIsTrue(value);
}

static void copy_fixed_three_profile_key(
    wchar_t *destination,
    size_t capacity,
    const wchar_t *key
) {
    size_t length;

    if (destination == NULL || capacity == 0u) return;
    if (key == NULL) key = L"<none>";
    length = wcslen(key);
    if (length >= capacity) length = capacity - 1u;
    memcpy(destination, key, length * sizeof(destination[0]));
    destination[length] = L'\0';
}

static BOOL validate_fixed_three_enable_profile(
    const wchar_t *path,
    SudekiMpFixedThreeProfileFailure *failure,
    wchar_t *failure_key,
    size_t failure_key_capacity
) {
    enum { SECTION_CAPACITY = 32768u };
    SudekiMpFixedThreeProfileState state;
    wchar_t *section;
    wchar_t *entry;
    DWORD section_length;
    BOOL valid = TRUE;

    if (failure != NULL) {
        *failure = SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_NONE;
    }
    copy_fixed_three_profile_key(
        failure_key, failure_key_capacity, L"<none>");
    if (path == NULL) {
        if (failure != NULL) {
            *failure =
                SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_INVALID_ARGUMENT;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    section = (wchar_t *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        SECTION_CAPACITY * sizeof(section[0]));
    if (section == NULL) {
        if (failure != NULL) {
            *failure =
                SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_INVALID_ARGUMENT;
        }
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    section_length = GetPrivateProfileSectionW(
        L"SudekiMP", section, SECTION_CAPACITY, path);
    SudekiMpFixedThreeProfileInitialize(&state);
    if (section_length >= SECTION_CAPACITY - 2u) {
        state.failure =
            SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_INVALID_ARGUMENT;
        copy_fixed_three_profile_key(
            failure_key, failure_key_capacity, L"<section_truncated>");
        valid = FALSE;
    }
    entry = section;
    while (valid && *entry != L'\0') {
        size_t entry_length = wcslen(entry);
        wchar_t *next = entry + entry_length + 1u;
        wchar_t *separator = wcschr(entry, L'=');

        if (separator == NULL) {
            state.failure =
                SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_INVALID_ARGUMENT;
            copy_fixed_three_profile_key(
                failure_key, failure_key_capacity, entry);
            valid = FALSE;
            break;
        }
        *separator = L'\0';
        if (!SudekiMpFixedThreeProfileObserve(
                &state,
                entry,
                SudekiMpConfigBooleanTextIsTrue(separator + 1u))) {
            copy_fixed_three_profile_key(
                failure_key, failure_key_capacity, entry);
            valid = FALSE;
            break;
        }
        entry = next;
    }
    if (valid && !SudekiMpFixedThreeProfileComplete(&state)) {
        copy_fixed_three_profile_key(
            failure_key,
            failure_key_capacity,
            SudekiMpFixedThreeProfileFirstMissingKey(&state));
        valid = FALSE;
    }
    if (failure != NULL) *failure = state.failure;
    HeapFree(GetProcessHeap(), 0u, section);
    if (!valid) SetLastError(ERROR_INVALID_DATA);
    return valid;
}

static BOOL validate_lan_arena_enable_profile(
    const wchar_t *path,
    SudekiMpLanArenaProfileFailure *failure,
    wchar_t *failure_key,
    size_t failure_key_capacity,
    SudekiMpLanArenaProfileRole *role
) {
    enum { SECTION_CAPACITY = 32768u };
    SudekiMpLanArenaProfileState state;
    wchar_t *section;
    wchar_t *entry;
    DWORD section_length;
    BOOL valid = TRUE;

    if (failure != NULL) *failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_NONE;
    if (role != NULL) *role = SUDEKIMP_LAN_ARENA_PROFILE_ROLE_NONE;
    copy_fixed_three_profile_key(failure_key, failure_key_capacity, L"<none>");
    if (path == NULL) {
        if (failure != NULL) {
            *failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_INVALID_ARGUMENT;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    section = (wchar_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        SECTION_CAPACITY * sizeof(section[0]));
    if (section == NULL) {
        if (failure != NULL) {
            *failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_INVALID_ARGUMENT;
        }
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    section_length = GetPrivateProfileSectionW(
        L"SudekiMP", section, SECTION_CAPACITY, path);
    SudekiMpLanArenaProfileInitialize(&state);
    if (section_length >= SECTION_CAPACITY - 2u) {
        state.failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_INVALID_ARGUMENT;
        copy_fixed_three_profile_key(
            failure_key, failure_key_capacity, L"<section_truncated>");
        valid = FALSE;
    }
    entry = section;
    while (valid && *entry != L'\0') {
        size_t entry_length = wcslen(entry);
        wchar_t *next = entry + entry_length + 1u;
        wchar_t *separator = wcschr(entry, L'=');
        if (separator == NULL) {
            state.failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_INVALID_ARGUMENT;
            copy_fixed_three_profile_key(failure_key, failure_key_capacity, entry);
            valid = FALSE;
            break;
        }
        *separator = L'\0';
        if (!SudekiMpLanArenaProfileObserve(
                &state, entry, SudekiMpConfigBooleanTextIsTrue(separator + 1u))) {
            copy_fixed_three_profile_key(failure_key, failure_key_capacity, entry);
            valid = FALSE;
            break;
        }
        entry = next;
    }
    if (valid && !SudekiMpLanArenaProfileComplete(&state, role)) {
        copy_fixed_three_profile_key(failure_key, failure_key_capacity,
            L"<required_lan_profile_key>");
        valid = FALSE;
    }
    if (failure != NULL) *failure = state.failure;
    HeapFree(GetProcessHeap(), 0u, section);
    if (!valid) SetLastError(ERROR_INVALID_DATA);
    return valid;
}

static int hex_nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static BOOL decode_sha256_text(const char text[65], uint8_t bytes[32]) {
    size_t index;
    if (text == NULL || bytes == NULL || strlen(text) != 64u) return FALSE;
    for (index = 0u; index < 32u; ++index) {
        int high = hex_nibble(text[index * 2u]);
        int low = hex_nibble(text[index * 2u + 1u]);
        if (high < 0 || low < 0) return FALSE;
        bytes[index] = (uint8_t)((high << 4) | low);
    }
    return TRUE;
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

static BOOL wait_for_local_input_hub_connections(
    uint8_t required_mask,
    DWORD timeout_ms
) {
    DWORD started = GetTickCount();

    do {
        uint8_t connected = SudekiMpLocalInputHubConnectedMask();

        if ((connected & required_mask) == required_mask) {
            return TRUE;
        }
        Sleep(5u);
    } while ((DWORD)(GetTickCount() - started) < timeout_ms);
    SetLastError(ERROR_DEVICE_NOT_CONNECTED);
    return FALSE;
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
    wchar_t sol_world_path[MAX_PATH];
    char sol_world_sha256[65];
    HMODULE game_module = GetModuleHandleW(NULL);
    SudekiMpBuildCheck build;
    SudekiMpTalosStagingNativeCaptureConfiguration
        talos_staging_capture_configuration;
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
    BOOL expanded_talos_encounter_prototype_enabled;
    BOOL expanded_talos_lifecycle_trace_enabled;
    BOOL talos_post_movie_party_restore_enabled;
    BOOL lan_arena_host_enabled;
    BOOL lan_arena_client_enabled;
    BOOL lan_arena_enabled;
    BOOL talos_companion_staging_observation_enabled;
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
    BOOL fixed_three_seat_renderer_enabled;
    BOOL freeroam_camera_input_enabled;
    BOOL ranged_quick_skill_prototype_enabled;
    BOOL realtime_multiplayer_skill_combat_enabled;
    BOOL skill_camera_routing_enabled;
    BOOL direct_spirit_strike_prototype_enabled;
    BOOL external_input_bridge_enabled;
    BOOL three_seat_udp_transport_enabled;
    BOOL native_xinput_player_two_enabled;
    BOOL player_two_input_enabled;
    BOOL player_interaction_requests_enabled;
    BOOL interaction_provenance_enabled;
    BOOL merchant_checkout_trace_enabled;
    BOOL merchant_checkout_trace_requested;
    BOOL save_book_vote_enabled;
    BOOL experimental_blacksmith_ui_enabled;
    BOOL second_player_controller_camera_enabled;
    BOOL native_second_player_camera_collision_enabled;
    BOOL split_screen_ranged_model_isolation_enabled;
    BOOL spirit_strike_viewport_effect_isolation_enabled;
    BOOL zone_transition_trace_enabled;
    BOOL zone_transition_trace_environment_enabled;
    BOOL party_atomic_transitions_enabled;
    BOOL transition_vote_enabled;
    BOOL zone_traversal_enabled;
    BOOL cleanroom_menu_enabled;
    BOOL coop_roster_menu_enabled;
    BOOL loaded_save_coop_autostart_enabled;
    BOOL skip_startup_movies;
    BOOL story_test_boost_enabled;
    BOOL cleanroom_multiplayer_integration;
    BOOL defer_integrated_roster;
    BOOL talos_exact_sol_authenticated = FALSE;
    BOOL talos_post_movie_dual_camera_enabled;
    SudekiMpLanArenaProfileRole lan_arena_profile_role =
        SUDEKIMP_LAN_ARENA_PROFILE_ROLE_NONE;
    SudekiMpLanArenaSessionConfig lan_arena_config;
    unsigned int talos_post_movie_camera_bundle_mask;
    wchar_t spirit_strike_key_text[32];
    wchar_t control_separation_key_text[32];
    wchar_t second_player_weak_attack_key_text[32];
    wchar_t second_player_camera_key_text[32];
    wchar_t second_player_skill_key_text[4][32];
    wchar_t freeroam_camera_modifier_text[32];
    wchar_t cleanroom_menu_key_text[32];
    wchar_t zone_traversal_menu_key_text[32];
    wchar_t story_test_boost_key_text[32];
    wchar_t lan_arena_host_text[64];
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
    int xinput_player_two_slot = 0;
    int lan_arena_port = 26770;
    int lan_arena_timeout_ms = 1500;
    char lan_arena_host_ipv4[64];
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
    expanded_talos_encounter_prototype_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableExpandedTalosEncounterPrototype"
    );
    expanded_talos_lifecycle_trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableExpandedTalosLifecycleTrace"
    );
    talos_post_movie_party_restore_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableTalosPostMoviePartyRestorePrototype"
    );
    lan_arena_host_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableLanArenaHostPrototype"
    );
    lan_arena_client_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableLanArenaClientPrototype"
    );
    lan_arena_enabled = lan_arena_host_enabled || lan_arena_client_enabled;
    talos_companion_staging_observation_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableTalosCompanionStagingObservation"
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
    fixed_three_seat_renderer_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableFixedThreeSeatRendererPrototype"
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
    three_seat_udp_transport_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableThreeSeatUdpTransportPrototype"
    );
    native_xinput_player_two_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableNativeXInputPlayerTwoPrototype"
    );
    player_interaction_requests_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlayerInteractionRequestsPrototype"
    );
    merchant_checkout_trace_requested = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableMerchantCheckoutTracePrototype"
    );
    merchant_checkout_trace_enabled = merchant_checkout_trace_requested;
    save_book_vote_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableSaveBookVotePrototype"
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
    zone_transition_trace_environment_enabled = GetEnvironmentVariableA(
        "SUDEKIMP_ZONE_TRACE",
        NULL,
        0u
    ) > 0u;
    zone_transition_trace_enabled = zone_transition_trace_environment_enabled;
    skill_camera_routing_enabled =
        realtime_multiplayer_skill_combat_enabled ||
        spirit_strike_viewport_effect_isolation_enabled;
    talos_post_movie_camera_bundle_mask =
        (split_screen_render_enabled ?
            SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_SPLIT : 0u) |
        (second_player_camera_enabled ?
            SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_PLAYER_TWO : 0u) |
        (dual_camera_frame_cache_enabled ?
            SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_DUAL_CACHE : 0u) |
        (second_player_controller_camera_enabled ?
            SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_PLAYER_TWO_ORBIT : 0u);
    talos_post_movie_dual_camera_enabled =
        talos_post_movie_party_restore_enabled &&
        talos_post_movie_camera_bundle_mask != 0u &&
        SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
            talos_post_movie_camera_bundle_mask,
            second_player_camera_relative_movement_enabled);
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
        expanded_talos_lifecycle_trace_enabled ||
        talos_post_movie_party_restore_enabled ||
        zone_traversal_enabled || party_atomic_transitions_enabled ||
        transition_vote_enabled || save_book_vote_enabled;
    interaction_provenance_enabled =
        (player_interaction_requests_enabled ||
         expanded_talos_lifecycle_trace_enabled ||
         talos_post_movie_party_restore_enabled) &&
        zone_transition_trace_enabled;
    merchant_checkout_trace_enabled = merchant_checkout_trace_enabled &&
        interaction_provenance_enabled && !trace_enabled &&
        !animation_speed_enabled && !camera_speed_enabled;
    coop_roster_menu_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableCoopRosterMenu"
    );
    loaded_save_coop_autostart_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableLoadedSaveCoopAutostartPrototype"
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
    player_two_input_enabled = external_input_bridge_enabled ||
        three_seat_udp_transport_enabled || native_xinput_player_two_enabled;
    if (lan_arena_enabled) {
        SudekiMpLanArenaProfileFailure profile_failure;
        wchar_t profile_failure_key[96];
        char profile_failure_key_utf8[192];
        int converted;

        if (!validate_lan_arena_enable_profile(
                config_path,
                &profile_failure,
                profile_failure_key,
                sizeof(profile_failure_key) / sizeof(profile_failure_key[0]),
                &lan_arena_profile_role) ||
            zone_transition_trace_environment_enabled ||
            !decode_sha256_text(build.actual_sha256, lan_arena_game_hash) ||
            !read_config_integer(
                config_path, L"SudekiMP", L"LanArenaPort", 26770,
                1024, 65535, &lan_arena_port) ||
            !read_config_integer(
                config_path, L"SudekiMP", L"LanArenaTimeoutMs", 1500,
                250, 10000, &lan_arena_timeout_ms)) {
            converted = WideCharToMultiByte(
                CP_UTF8, 0u, profile_failure_key, -1,
                profile_failure_key_utf8, (int)sizeof(profile_failure_key_utf8),
                NULL, NULL);
            if (converted == 0) strcpy(profile_failure_key_utf8, "<conversion_failed>");
            SudekiMpLogFormat(
                "lan_arena_config=invalid reason=closed_profile_or_network_config "
                "failure=%u key=%s zone_trace=%s\r\n",
                (unsigned int)profile_failure,
                profile_failure_key_utf8,
                zone_transition_trace_environment_enabled ? "true" : "false");
            SudekiMpLogWrite("status=bad_config\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_BAD_CONFIG;
        }
        GetPrivateProfileStringW(
            L"SudekiMP", L"LanArenaHost", L"127.0.0.1", lan_arena_host_text,
            (DWORD)(sizeof(lan_arena_host_text) / sizeof(lan_arena_host_text[0])),
            config_path);
        if (WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, lan_arena_host_text, -1,
                lan_arena_host_ipv4, (int)sizeof(lan_arena_host_ipv4), NULL, NULL) == 0) {
            SudekiMpLogWrite("lan_arena_config=invalid reason=host_utf8\r\n");
            SudekiMpLogWrite("status=bad_config\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_BAD_CONFIG;
        }
        if ((lan_arena_profile_role == SUDEKIMP_LAN_ARENA_PROFILE_ROLE_HOST &&
             !lan_arena_host_enabled) ||
            (lan_arena_profile_role == SUDEKIMP_LAN_ARENA_PROFILE_ROLE_CLIENT &&
             !lan_arena_client_enabled) ||
            !control_separation_enabled) {
            SudekiMpLogWrite(
                "lan_arena_config=invalid reason=role_control_ownership_contract\r\n");
            SudekiMpLogWrite("status=bad_config\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_BAD_CONFIG;
        }
        memset(&lan_arena_config, 0, sizeof(lan_arena_config));
        lan_arena_config.local_role =
            lan_arena_profile_role == SUDEKIMP_LAN_ARENA_PROFILE_ROLE_HOST ?
                SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL :
                SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
        lan_arena_config.remote_ipv4 =
            lan_arena_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ?
                lan_arena_host_ipv4 : NULL;
        lan_arena_config.port = (unsigned int)lan_arena_port;
        lan_arena_config.timeout_ms = (uint32_t)lan_arena_timeout_ms;
        lan_arena_config.game_hash = lan_arena_game_hash;
        SudekiMpLogFormat(
            "lan_arena_requested=true role=%s port=%d host=%s "
            "policy=cleanroom_only_shared_simulation_no_campaign_state\r\n",
            lan_arena_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
                "Tal_host" : "Ailish_client",
            lan_arena_port,
            lan_arena_config.remote_ipv4 == NULL ? "bind_any" : lan_arena_host_ipv4);
    }
    /* This is a deliberately closed, ordinary-world observation profile.
     * Even passive sibling traces and the startup-movie bypass would change
     * the provenance of a captured frame, so every optional path must remain
     * disabled and the environment-owned zone hook must be absent. */
    if (talos_companion_staging_observation_enabled &&
        (patch_enabled || trace_enabled || animation_speed_enabled ||
         camera_speed_enabled || quick_skill_input_trace_enabled ||
         character_switch_trace_enabled || talos_party_prototype_enabled ||
         expanded_talos_encounter_prototype_enabled ||
         expanded_talos_lifecycle_trace_enabled ||
         talos_post_movie_party_restore_enabled ||
         talos_defense_trace_enabled || control_separation_enabled ||
         player_movement_trace_enabled || second_player_movement_enabled ||
         second_player_camera_relative_movement_enabled ||
         second_player_separation_guard_enabled ||
         second_player_weak_attack_enabled ||
         second_player_target_trace_enabled || shared_group_camera_enabled ||
         split_screen_render_enabled || second_player_camera_enabled ||
         dual_camera_frame_cache_enabled ||
         fixed_three_seat_renderer_enabled || freeroam_camera_input_enabled ||
         ranged_quick_skill_prototype_enabled ||
         realtime_multiplayer_skill_combat_enabled ||
         direct_spirit_strike_prototype_enabled ||
         external_input_bridge_enabled || three_seat_udp_transport_enabled ||
         native_xinput_player_two_enabled ||
         player_interaction_requests_enabled ||
         merchant_checkout_trace_requested || save_book_vote_enabled ||
         experimental_blacksmith_ui_enabled ||
         second_player_controller_camera_enabled ||
         native_second_player_camera_collision_enabled ||
         split_screen_ranged_model_isolation_enabled ||
         spirit_strike_viewport_effect_isolation_enabled ||
         party_atomic_transitions_enabled || transition_vote_enabled ||
         zone_traversal_enabled || cleanroom_menu_enabled ||
         coop_roster_menu_enabled || loaded_save_coop_autostart_enabled ||
         skip_startup_movies || story_test_boost_enabled ||
         zone_transition_trace_environment_enabled)) {
        SudekiMpLogWrite(
            "talos_companion_staging_observation_config=invalid "
            "reason=requires_closed_ordinary_world_profile_no_optional_"
            "gameplay_coop_menu_trace_camera_input_speed_transition_"
            "environment_hook_or_startup_movie_bypass\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        (void)SudekiMpUninstallAcceleratorCache();
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    /* This is one closed mutation-adjacent research profile. The only optional
     * runtime owners admitted beside the new restore coordinator are normal
     * control separation, Player 2 movement/weak attack, and exactly one
     * Player 2 transport. Zone/provenance observation is forced internally so
     * an environment-owned second zone hook is still rejected. */
    if (talos_post_movie_party_restore_enabled &&
        (patch_enabled || trace_enabled || animation_speed_enabled ||
         camera_speed_enabled || quick_skill_input_trace_enabled ||
         character_switch_trace_enabled || talos_party_prototype_enabled ||
         expanded_talos_encounter_prototype_enabled ||
         expanded_talos_lifecycle_trace_enabled ||
         talos_companion_staging_observation_enabled ||
         talos_defense_trace_enabled || !control_separation_enabled ||
         player_movement_trace_enabled || !second_player_movement_enabled ||
         second_player_separation_guard_enabled ||
         !second_player_weak_attack_enabled ||
         second_player_target_trace_enabled || shared_group_camera_enabled ||
         !SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
             talos_post_movie_camera_bundle_mask,
             second_player_camera_relative_movement_enabled) ||
         freeroam_camera_input_enabled ||
         ranged_quick_skill_prototype_enabled ||
         realtime_multiplayer_skill_combat_enabled ||
         direct_spirit_strike_prototype_enabled ||
         three_seat_udp_transport_enabled ||
         fixed_three_seat_renderer_enabled ||
         (external_input_bridge_enabled ==
          native_xinput_player_two_enabled) ||
         player_interaction_requests_enabled ||
         merchant_checkout_trace_requested || save_book_vote_enabled ||
         experimental_blacksmith_ui_enabled ||
         native_second_player_camera_collision_enabled ||
         split_screen_ranged_model_isolation_enabled ||
         spirit_strike_viewport_effect_isolation_enabled ||
         party_atomic_transitions_enabled || transition_vote_enabled ||
         zone_traversal_enabled || cleanroom_menu_enabled ||
         coop_roster_menu_enabled || loaded_save_coop_autostart_enabled ||
         story_test_boost_enabled || zone_transition_trace_environment_enabled)) {
        SudekiMpLogWrite(
            "talos_post_movie_party_restore_config=invalid "
            "reason=requires_closed_exact_asset_profile_normal_control_"
            "separation_p2_movement_p2_weak_attack_exactly_one_input_source_"
            "optional_exact_split_p2_camera_dual_cache_controller_orbit_"
            "bundle_paired_with_camera_relative_movement_and_no_legacy_"
            "talos_carry_staging_"
            "character_switch_or_other_hook_owner\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        (void)SudekiMpUninstallAcceleratorCache();
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
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
         !player_two_input_enabled)) {
        SudekiMpLogWrite(
            "transition_vote_config=invalid "
            "reason=requires_party_atomic_transitions_and_player_two_input_source\r\n"
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
    if (loaded_save_coop_autostart_enabled &&
        (!coop_roster_menu_enabled || !cleanroom_multiplayer_integration ||
         !player_two_input_enabled)) {
        SudekiMpLogWrite(
            "loaded_save_coop_autostart_config=invalid "
            "reason=requires_coop_roster_integrated_menu_control_split_and_player_two_input_source\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    /* The retired prototype armed from the authored zero-position PC_KAZEL
     * spawn, before the exact Kazel delete and TSA settle boundary. That early
     * trigger is implicated in the R6025 run and may never be re-enabled. The
     * separate post-movie coordinator below requires the later exact ticket. */
    if (talos_party_prototype_enabled) {
        SudekiMpLogWrite(
            "talos_party_config=invalid "
            "reason=retired_PC_KAZEL_spawn_trigger_precedes_exact_delete_and_"
            "TSA_settle_boundary\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (expanded_talos_encounter_prototype_enabled) {
        SudekiMpLogWrite(
            "expanded_talos_encounter_config=unavailable "
            "reason=pre_transition_four_hero_carry_through_and_adaptive_seat_runtime_not_proven\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    /* The lifecycle observer is a closed, one-human research profile. It owns
     * the exact SOL opcode slot that SkillTrace and the merchant observer also
     * own, and its evidence is invalid if any optional gameplay, co-op, camera,
     * input, menu, speed, or transition mutation is active. ZoneTransition and
     * InteractionProvenance are its only internally-forced dependencies. */
    if (expanded_talos_lifecycle_trace_enabled &&
        (patch_enabled || trace_enabled || animation_speed_enabled ||
         camera_speed_enabled || quick_skill_input_trace_enabled ||
         character_switch_trace_enabled || talos_defense_trace_enabled ||
         talos_post_movie_party_restore_enabled ||
         control_separation_enabled || player_movement_trace_enabled ||
         second_player_movement_enabled ||
         second_player_camera_relative_movement_enabled ||
         second_player_separation_guard_enabled ||
         second_player_weak_attack_enabled ||
         second_player_target_trace_enabled || shared_group_camera_enabled ||
         split_screen_render_enabled || second_player_camera_enabled ||
         dual_camera_frame_cache_enabled ||
         fixed_three_seat_renderer_enabled || freeroam_camera_input_enabled ||
         ranged_quick_skill_prototype_enabled ||
         realtime_multiplayer_skill_combat_enabled ||
         direct_spirit_strike_prototype_enabled ||
         external_input_bridge_enabled || three_seat_udp_transport_enabled ||
         native_xinput_player_two_enabled ||
         player_interaction_requests_enabled ||
         merchant_checkout_trace_enabled || save_book_vote_enabled ||
         experimental_blacksmith_ui_enabled ||
         second_player_controller_camera_enabled ||
         native_second_player_camera_collision_enabled ||
         split_screen_ranged_model_isolation_enabled ||
         spirit_strike_viewport_effect_isolation_enabled ||
         party_atomic_transitions_enabled || transition_vote_enabled ||
         zone_traversal_enabled || cleanroom_menu_enabled ||
         coop_roster_menu_enabled || loaded_save_coop_autostart_enabled ||
         skip_startup_movies || story_test_boost_enabled)) {
        SudekiMpLogWrite(
            "expanded_talos_lifecycle_trace_config=invalid "
            "reason=research_observer_requires_closed_native_passthrough_profile\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (save_book_vote_enabled &&
        (!coop_roster_menu_enabled || !cleanroom_multiplayer_integration ||
         !control_separation_enabled || !split_screen_render_enabled ||
         !dual_camera_frame_cache_enabled ||
         !player_two_input_enabled)) {
        SudekiMpLogWrite(
            "save_book_vote_config=invalid "
            "reason=requires_coop_roster_integrated_menu_control_split_"
            "dual_cache_and_player_two_input_source\r\n");
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
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
         !player_two_input_enabled || !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "player_interaction_requests_config=invalid "
            "reason=requires_coop_roster_integrated_menu_control_split_dual_cache_and_player_two_input_source\r\n"
        );
        SudekiMpLogWrite("status=bad_config\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (experimental_blacksmith_ui_enabled &&
        (!coop_roster_menu_enabled || !cleanroom_multiplayer_integration ||
         !player_two_input_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "per_player_blacksmith_ui_config=invalid "
            "reason=requires_integrated_coop_roster_control_split_player_two_camera_dual_cache_and_player_two_input_source\r\n");
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
    if ((external_input_bridge_enabled &&
         (three_seat_udp_transport_enabled ||
          native_xinput_player_two_enabled)) ||
        (three_seat_udp_transport_enabled &&
         native_xinput_player_two_enabled)) {
        SudekiMpLogWrite(
            "player_two_input_config=invalid reason=legacy_udp_three_seat_udp_and_native_xinput_are_mutually_exclusive\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (player_two_input_enabled &&
        (!control_separation_enabled || !second_player_movement_enabled)) {
        SudekiMpLogWrite(
            "player_two_input_config=requires_control_separation_and_second_player_movement\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if ((external_input_bridge_enabled || three_seat_udp_transport_enabled) &&
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
             &input_bridge_timeout_ms))) {
        SudekiMpLogWrite("external_input_bridge_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (three_seat_udp_transport_enabled && input_bridge_port > 65533) {
        SudekiMpLogWrite(
            "three_seat_udp_transport_config=invalid reason=base_port_exceeds_local_input_hub_port_bank_range\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (fixed_three_seat_renderer_enabled) {
        SudekiMpFixedThreeProfileFailure profile_failure;
        wchar_t profile_failure_key[96];
        char profile_failure_key_utf8[192];

        if (!validate_fixed_three_enable_profile(
                config_path,
                &profile_failure,
                profile_failure_key,
                sizeof(profile_failure_key) /
                    sizeof(profile_failure_key[0]))) {
            int converted = WideCharToMultiByte(
                CP_UTF8,
                0u,
                profile_failure_key,
                -1,
                profile_failure_key_utf8,
                (int)sizeof(profile_failure_key_utf8),
                NULL,
                NULL);

            if (converted == 0) {
                strcpy(profile_failure_key_utf8, "<conversion_failed>");
            }
            SudekiMpLogFormat(
                "fixed_three_seat_renderer_config=invalid "
                "reason=exact_enable_profile_rejected failure=%u key=%s\r\n",
                (unsigned int)profile_failure,
                profile_failure_key_utf8);
            SudekiMpLogWrite("status=config_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_BAD_CONFIG;
        }
        if (zone_transition_trace_environment_enabled) {
            SudekiMpLogWrite(
                "fixed_three_seat_renderer_config=invalid "
                "reason=inherited_zone_trace_environment_forbidden\r\n");
            SudekiMpLogWrite("status=config_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_BAD_CONFIG;
        }
    }
    if (fixed_three_seat_renderer_enabled &&
        (!three_seat_udp_transport_enabled ||
         !coop_roster_menu_enabled || !control_separation_enabled ||
         !second_player_movement_enabled ||
         !second_player_camera_relative_movement_enabled ||
         !second_player_weak_attack_enabled ||
         !split_screen_render_enabled || !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled ||
         !second_player_controller_camera_enabled ||
         !player_two_input_enabled || external_input_bridge_enabled ||
         native_xinput_player_two_enabled || skill_camera_routing_enabled ||
         zone_transition_trace_enabled || interaction_provenance_enabled ||
         merchant_checkout_trace_enabled ||
         talos_post_movie_dual_camera_enabled ||
         loaded_save_coop_autostart_enabled ||
         !skip_startup_movies ||
         !cleanroom_multiplayer_integration || !defer_integrated_roster)) {
        SudekiMpLogWrite(
            "fixed_three_seat_renderer_config=invalid "
            "reason=requires_exact_11_enable_profile_and_closed_derived_"
            "state\r\n"
        );
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (player_two_input_enabled && !read_config_float(
             config_path,
             L"SudekiMP",
             L"InputBridgeDeadzone",
             0.20f,
             0.0f,
             0.90f,
             &input_bridge_deadzone)) {
        SudekiMpLogWrite("player_two_input_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (fixed_three_seat_renderer_enabled &&
        float_bits(input_bridge_deadzone) != UINT32_C(0x3e4ccccd)) {
        SudekiMpLogWrite(
            "fixed_three_seat_renderer_config=invalid "
            "reason=requires_input_bridge_deadzone_0_20_for_reconnect_"
            "neutral_fence_and_gameplay_deadzone_agreement\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (native_xinput_player_two_enabled && !read_config_integer(
            config_path,
            L"SudekiMP",
            L"XInputPlayerTwoSlot",
            0,
            0,
            3,
            &xinput_player_two_slot)) {
        SudekiMpLogWrite("native_xinput_player_two_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (second_player_controller_camera_enabled &&
        (!player_two_input_enabled ||
         !second_player_camera_relative_movement_enabled ||
         !split_screen_render_enabled ||
         !second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled)) {
        SudekiMpLogWrite(
            "second_player_controller_camera_config=requires_player_two_input_camera_relative_movement_split_p2_camera_dual_cache\r\n"
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

    ZeroMemory(&talos_staging_capture_configuration,
        sizeof(talos_staging_capture_configuration));
    if (talos_post_movie_party_restore_enabled) {
        if (!build_sol_world_path(game_path, sol_world_path,
                sizeof(sol_world_path) / sizeof(sol_world_path[0])) ||
            !SudekiMpSha256File(sol_world_path, sol_world_sha256)) {
            SudekiMpLogFormat(
                "talos_post_movie_party_restore_error=%lu "
                "phase=SOLWORLDM_full_file_sha256\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("status=talos_post_movie_restore_error\r\n");
            (void)SudekiMpUninstallAcceleratorCache();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_POST_MOVIE_RESTORE_FAILED;
        }
        SudekiMpLogFormat("talos_post_movie_solworldm_sha256=%s\r\n",
            sol_world_sha256);
        if (strcmp(sol_world_sha256,
                SUDEKIMP_TALOS_EXACT_SOL_SHA256) != 0) {
            SudekiMpLogWrite(
                "talos_post_movie_party_restore_error=exact_SOLWORLDM_"
                "hash_mismatch\r\n");
            SudekiMpLogWrite("status=unsupported_solworldm\r\n");
            (void)SudekiMpUninstallAcceleratorCache();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_POST_MOVIE_RESTORE_FAILED;
        }
        talos_exact_sol_authenticated = TRUE;
        SudekiMpLogWrite(
            "talos_post_movie_party_restore_preflight=ready "
            "executable_hash=exact solworldm_hash=exact\r\n");
    }
    if (talos_companion_staging_observation_enabled) {
        SudekiMpTalosMembershipAbiDescriptor membership_descriptor =
            SudekiMpTalosCompanionMembershipAbiDescribe();
        SudekiMpTalosMembershipValidationResult membership_validation;

        if (!build_sol_world_path(game_path, sol_world_path,
                sizeof(sol_world_path) / sizeof(sol_world_path[0])) ||
            !SudekiMpSha256File(sol_world_path, sol_world_sha256)) {
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=SOLWORLDM_full_file_sha256\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            (void)SudekiMpUninstallAcceleratorCache();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        SudekiMpLogFormat("talos_staging_solworldm_sha256=%s\r\n",
            sol_world_sha256);
        if (strcmp(sol_world_sha256,
                SUDEKIMP_TALOS_EXACT_SOL_SHA256) != 0) {
            SudekiMpLogWrite(
                "talos_companion_staging_observation_error=exact_SOLWORLDM_"
                "hash_mismatch\r\n");
            SudekiMpLogWrite("status=unsupported_solworldm\r\n");
            (void)SudekiMpUninstallAcceleratorCache();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        membership_validation =
            SudekiMpTalosCompanionMembershipAbiValidateMappedImage(
                (const uint8_t *)game_module,
                membership_descriptor.mapped_image_size,
                (uint32_t)(uintptr_t)game_module);
        if (membership_validation.seams_valid != 1u ||
            membership_validation.validated_symbol_mask !=
                membership_validation.required_symbol_mask ||
            membership_validation.pure_validation_only != 1u ||
            membership_validation.native_calls_permitted != 0u ||
            membership_validation.external_sha256_required != 1u) {
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=membership_abi failure=%lu symbol=%lu rva=0x%08lx "
                "expected=0x%08lx observed=0x%08lx checks=%lu\r\n",
                (unsigned long)ERROR_BAD_EXE_FORMAT,
                (unsigned long)membership_validation.failure,
                (unsigned long)membership_validation.failed_symbol,
                (unsigned long)membership_validation.failed_rva,
                (unsigned long)membership_validation.expected_value,
                (unsigned long)membership_validation.observed_value,
                (unsigned long)membership_validation.checks_completed);
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            (void)SudekiMpUninstallAcceleratorCache();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        if (!generate_talos_staging_observation_identity(
                &talos_staging_capture_configuration.process_token,
                &talos_staging_capture_configuration.identity_salt)) {
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=per_run_identity_generation\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            (void)SudekiMpUninstallAcceleratorCache();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        talos_staging_capture_configuration.loaded_image_base =
            (uint32_t)(uintptr_t)game_module;
        talos_staging_capture_configuration.mapped_image_size =
            membership_descriptor.mapped_image_size;
        talos_staging_capture_configuration.
            expected_observer_registry_generation = 0u;
        talos_staging_capture_configuration.failed_retry_dispatches =
            SUDEKIMP_TALOS_NATIVE_CAPTURE_DEFAULT_RETRY_DISPATCHES;
        talos_staging_capture_configuration.exact_executable_hash = 1u;
        talos_staging_capture_configuration.exact_sol_hash = 1u;
        talos_staging_capture_configuration.membership_abi_valid = 1u;
        /* The service-only install below validates the exact controller
         * entry before this configuration is admitted. */
        talos_staging_capture_configuration.controller_abi_valid = 0u;
        /* Modal state is diagnostic only and never authorizes this sample. */
        talos_staging_capture_configuration.modal_active = 0u;
        talos_staging_capture_configuration.reload_required = 0u;
        talos_staging_capture_configuration.require_default_camera_name = 0u;
        SudekiMpLogFormat(
            "talos_companion_staging_observation_preflight=ready "
            "membership_checks=%lu sol_hash=exact identity=per_run_nonzero "
            "policy=read_only_no_native_membership_calls\r\n",
            (unsigned long)membership_validation.checks_completed);
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
        "control_separation_prototype_requested=%s virtual_key=0x%02lx target_policy=first_non_front_active_party_member second_player_movement=%s camera_relative_movement=%s separation_guard=%s maximum_separation_bits=0x%08lx second_player_weak_attack=%s weak_attack_virtual_key=0x%02lx second_player_skills=%s skill_keys=0x%02lx,0x%02lx,0x%02lx,0x%02lx target_trace=%s shared_group_camera=%s external_input_bridge=%s three_seat_udp_transport=%s requested_human_mask=0x%02x bridge_port=%d bridge_timeout_ms=%d bridge_deadzone_bits=0x%08lx\r\n",
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
        three_seat_udp_transport_enabled ? "true" : "false",
        three_seat_udp_transport_enabled ?
            SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK :
            (player_two_input_enabled ? 0x03u : 0x01u),
        input_bridge_port,
        input_bridge_timeout_ms,
        (unsigned long)float_bits(input_bridge_deadzone)
    );
    SudekiMpLogFormat(
        "native_xinput_player_two_requested=%s slot=%d transport=%s\r\n",
        native_xinput_player_two_enabled ? "true" : "false",
        xinput_player_two_slot,
        native_xinput_player_two_enabled ?
            "direct_xinput_reserved_from_native_player_one" : "disabled"
    );
    SudekiMpLogFormat(
        "interaction_provenance_and_intent_trace_requested=%s "
        "button=controller_a "
        "policy=passive_exact_actor_target_source_generation_trace_"
        "intent_only_no_targetless_request_no_native_world_action\r\n",
        player_interaction_requests_enabled ? "true" : "false"
    );
    SudekiMpLogFormat(
        "save_book_vote_requested=%s timeout_ms=10000 "
        "general_interaction_provenance=%s "
        "policy=native_SaveMenuShow_pass_through_exact_final_LoadGameSave_"
        "defer_independent_of_SOL_active_predicates\r\n",
        save_book_vote_enabled ? "true" : "false",
        interaction_provenance_enabled ? "enabled" : "disabled");
    SudekiMpLogFormat(
        "per_player_blacksmith_ui_experiment_requested=%s "
        "default=false mutation=disabled "
        "policy=exact_gated_start_and_active_script_contract_two_native_inert_panels\r\n",
        experimental_blacksmith_ui_enabled ? "true" : "false");
    if (control_separation_enabled) {
        SudekiMpCombatContextsReset();
        if (three_seat_udp_transport_enabled) {
            DWORD transport_error;

            if (!SudekiMpLocalInputHubStartUdp(
                    SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK,
                    (unsigned int)input_bridge_port,
                    (DWORD)input_bridge_timeout_ms) ||
                !wait_for_local_input_hub_connections(
                    SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK,
                    1000u)) {
                transport_error = GetLastError();
                SudekiMpInputBridgeStop();
                SudekiMpLogFormat(
                    "three_seat_udp_transport_start_error=%lu required_mask=0x%02x base_port=%d startup_timeout_ms=1000\r\n",
                    (unsigned long)transport_error,
                    SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK,
                    input_bridge_port);
                SudekiMpLogWrite("status=input_bridge_error\r\n");
                SudekiMpUninstallInteractionProvenance();
                SudekiMpLogClose();
                return SUDEKIMP_INIT_INPUT_BRIDGE_FAILED;
            }
            SudekiMpLogFormat(
                "three_seat_udp_transport_started=true profile=%s "
                "requested_mask=0x%02x connected_mask=0x%02x "
                "p2_port=%u p3_port=%u policy=%s\r\n",
                fixed_three_seat_renderer_enabled ?
                    "fixed_three_local_coop" : "transport_diagnostic",
                SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK,
                SudekiMpLocalInputHubConnectedMask(),
                SudekiMpLocalInputHubSeatPort(1u),
                SudekiMpLocalInputHubSeatPort(2u),
                fixed_three_seat_renderer_enabled ?
                    "reserve_exact_p2_p3_input_generations_before_renderer_"
                    "and_roster_admission" :
                    "transport_only_no_player_three_actor_camera_hud_or_"
                    "gameplay_authority");
        }
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
        if (native_xinput_player_two_enabled &&
            (!SudekiMpInputBridgeStartXInput(
                 (unsigned int)xinput_player_two_slot) ||
             !SudekiMpInstallXInputPlayerTwoReservation(
                 game_module, (unsigned int)xinput_player_two_slot))) {
            SudekiMpLogFormat("native_xinput_player_two_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpUninstallXInputPlayerTwoReservation();
            SudekiMpInputBridgeStop();
            SudekiMpLogWrite("status=input_bridge_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_INPUT_BRIDGE_FAILED;
        }
        if ((realtime_multiplayer_skill_combat_enabled ||
             fixed_three_seat_renderer_enabled) &&
            !SudekiMpInitializeSkillActivationAbi(game_module)) {
            SudekiMpLogFormat(
                "realtime_skill_activation_abi_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpUninstallXInputPlayerTwoReservation();
            SudekiMpInputBridgeStop();
            SudekiMpLogWrite("status=control_separation_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED;
        }
        if (fixed_three_seat_renderer_enabled &&
            !SudekiMpInitializeSpiritActivationAbi(game_module)) {
            SudekiMpLogFormat(
                "local_quick_menu_adapter_preflight adapter=spirit error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpResetItemActivationAbi();
            SudekiMpResetWeaponActivationAbi();
            SudekiMpResetSpiritActivationAbi();
            SudekiMpResetSkillActivationAbi();
            SudekiMpUninstallXInputPlayerTwoReservation();
            SudekiMpInputBridgeStop();
            SudekiMpLogWrite("status=control_separation_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED;
        }
        if (fixed_three_seat_renderer_enabled &&
            !SudekiMpInitializeWeaponActivationAbi(game_module)) {
            SudekiMpLogFormat(
                "local_quick_menu_adapter_preflight adapter=weapon error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpResetItemActivationAbi();
            SudekiMpResetWeaponActivationAbi();
            SudekiMpResetSpiritActivationAbi();
            SudekiMpResetSkillActivationAbi();
            SudekiMpUninstallXInputPlayerTwoReservation();
            SudekiMpInputBridgeStop();
            SudekiMpLogWrite("status=control_separation_error\r\n");
            SudekiMpUninstallInteractionProvenance();
            SudekiMpLogClose();
            return SUDEKIMP_INIT_CONTROL_SEPARATION_FAILED;
        }
        if (fixed_three_seat_renderer_enabled &&
            !SudekiMpInitializeItemActivationAbi(game_module)) {
            SudekiMpLogFormat(
                "local_quick_menu_adapter_preflight adapter=item error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpResetItemActivationAbi();
            SudekiMpResetWeaponActivationAbi();
            SudekiMpResetSpiritActivationAbi();
            SudekiMpResetSkillActivationAbi();
            SudekiMpUninstallXInputPlayerTwoReservation();
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
                player_two_input_enabled,
                input_bridge_deadzone)) {
            SudekiMpLogFormat("control_separation_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpResetItemActivationAbi();
            SudekiMpResetWeaponActivationAbi();
            SudekiMpResetSpiritActivationAbi();
            SudekiMpResetSkillActivationAbi();
            SudekiMpUninstallXInputPlayerTwoReservation();
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
    if (lan_arena_enabled) {
        if (!SudekiMpCleanroomEngineInitialize(game_module) ||
            !SudekiMpInstallLanArenaWindowPolicy(game_module) ||
            !SudekiMpInstallLanArenaRuntime(game_module, &lan_arena_config) ||
            !SudekiMpInstallLanArenaPausePanel(game_module)) {
            DWORD lan_error = GetLastError();
            SudekiMpLogFormat(
                "lan_arena_runtime_error=%lu phase=cleanroom_engine_window_network_or_pause_panel_install\r\n",
                (unsigned long)lan_error);
            SudekiMpLogWrite("status=lan_arena_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(lan_error);
            return SUDEKIMP_INIT_LAN_ARENA_FAILED;
        }
        SudekiMpLogWrite(
            "lan_arena_runtime_applied=true "
            "state=cleanroom_waiting_for_authenticated_peer\r\n");
    }
    SudekiMpLogFormat(
        "split_screen_render_prototype_requested=%s layout=%s camera_policy=%s dual_camera_frame_cache=%s fixed_three_seat_renderer=%s skill_camera_routing=%s second_player_controller_camera=%s native_second_player_camera_collision=%s split_screen_ranged_model_isolation=%s spirit_strike_viewport_effect_isolation=%s controller_camera_yaw_speed_bits=0x%08lx controller_camera_pitch_speed_bits=0x%08lx controller_camera_maximum_pitch_bits=0x%08lx second_player_camera_toggle_virtual_key=0x%02lx\r\n",
        split_screen_render_enabled ? "true" : "false",
        fixed_three_seat_renderer_enabled ?
            "p1_top_p2_bottom_left_p3_bottom_right" : "left_right",
        fixed_three_seat_renderer_enabled ?
            "fixed_three_seat_round_robin_frame_cache" :
            (dual_camera_frame_cache_enabled ?
                "alternating_render_state_frame_cache" :
            (second_player_camera_enabled ?
                "render_only_translated_camera_toggle" :
                "same_native_camera")),
        dual_camera_frame_cache_enabled ? "true" : "false",
        fixed_three_seat_renderer_enabled ? "true" : "false",
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
        SudekiMpSplitScreenSetRuntimeAuthorizationQuery(
            talos_post_movie_dual_camera_enabled ?
                talos_post_movie_dual_camera_authorized : NULL
        );
        SudekiMpLogFormat(
            "talos_post_movie_dual_camera_requested=%s "
            "activation=post_restore_exact_status "
            "fallback=native_full_width_until_two_fresh_frames\r\n",
            talos_post_movie_dual_camera_enabled ? "true" : "false"
        );
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
                second_player_controller_camera_maximum_pitch) ||
            (fixed_three_seat_renderer_enabled &&
             !SudekiMpSplitScreenSetFixedThreeSeatEnabled(TRUE))) {
            DWORD split_screen_error = GetLastError();

            (void)SudekiMpSplitScreenSetFixedThreeSeatEnabled(FALSE);
            SudekiMpUninstallSplitScreenRender();
            SudekiMpSplitScreenSetRuntimeAuthorizationQuery(NULL);
            SudekiMpResetItemActivationAbi();
            SudekiMpResetWeaponActivationAbi();
            SudekiMpResetSpiritActivationAbi();
            SudekiMpResetSkillActivationAbi();
            if (control_separation_enabled) {
                (void)SudekiMpControlSeparationSetInteractionRequestsEnabled(
                    FALSE);
                SudekiMpUninstallControlSeparation();
                SudekiMpUninstallXInputPlayerTwoReservation();
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
        if (fixed_three_seat_renderer_enabled) {
            SudekiMpSplitScreenSetFixedThreeCustomQuickMenuActionCapabilities(
                SUDEKIMP_LOCAL_QUICK_MENU_ALL_CATEGORIES);
            SudekiMpLogWrite(
                "fixed_three_local_quick_menu=true "
                "categories=skills,weapons,items,spirit "
                "presentation=per_viewport_compositor_panel "
                "policy=mod_owned_per_seat_no_native_singleton\r\n"
            );
        }
        SudekiMpLogWrite("split_screen_render_applied=true\r\n");
        SudekiMpLogFormat(
            "fixed_three_seat_renderer_applied=%s roster_capacity=%u "
            "hub_requested_mask=0x%02x hub_connected_mask=0x%02x\r\n",
            fixed_three_seat_renderer_enabled ? "true" : "false",
            SudekiMpSplitScreenRosterSeatCapacity(),
            SudekiMpLocalInputHubRequestedMask(),
            SudekiMpLocalInputHubConnectedMask());
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
            DWORD roster_error = GetLastError();

            SudekiMpLogFormat(
                "coop_roster_menu_error=%lu phase=deferred_integrated_install\r\n",
                (unsigned long)roster_error
            );
            SudekiMpLogWrite("coop_roster_menu_applied=false\r\n");
            SudekiMpLogWrite("status=coop_roster_menu_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(roster_error);
            return SUDEKIMP_INIT_CLEANROOM_MENU_FAILED;
        }
        SudekiMpLogWrite(
            "coop_roster_menu_applied=true phase=after_split_preflight\r\n"
        );
    }
    if (coop_roster_menu_enabled) {
        SudekiMpCleanroomMenuSetLoadedSaveCoopAutostart(
            loaded_save_coop_autostart_enabled);
    }
    SudekiMpLogFormat(
        "loaded_save_coop_autostart_requested=%s "
        "policy=Tal_host_Ailish_player_two_after_loaded_party_settles\r\n",
        loaded_save_coop_autostart_enabled ? "true" : "false");
    if (cleanroom_multiplayer_integration) {
        if (!SudekiMpControlUpdateObserverGateEnable(
                &cleanroom_update_observer_gate) ||
            !SudekiMpControlSeparationRegisterUpdateObserver(
                &cleanroom_update_observer_owner,
                cleanroom_control_update_observer)) {
            DWORD observer_error = GetLastError();

            SudekiMpControlUpdateObserverGateDisable(
                &cleanroom_update_observer_gate);
            SudekiMpControlUpdateObserverGateDrain(
                &cleanroom_update_observer_gate);
            SudekiMpLogFormat(
                "menu_multiplayer_integration_error=%lu "
                "phase=owned_controller_update_observer_registration\r\n",
                (unsigned long)observer_error
            );
            SudekiMpLogWrite(
                "menu_multiplayer_integration=failed "
                "rollback=all_runtime_hooks\r\n"
            );
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(observer_error);
            return SUDEKIMP_INIT_CLEANROOM_MENU_FAILED;
        }
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
    /* Install the native save-menu lifecycle marker plus final LoadGameSave
     * deferral near the end; the common rollback owns every later failure. */
    if (save_book_vote_enabled) {
        if (!SudekiMpInstallSaveBookIntercept(game_module, TRUE)) {
            DWORD save_book_error = GetLastError();

            SudekiMpUninstallSaveBookIntercept();
            SudekiMpLogFormat(
                "save_book_vote_error=%lu "
                "phase=exact_SaveMenuShow_and_LoadGameSave_hook_install\r\n",
                (unsigned long)save_book_error);
            SudekiMpLogWrite("save_book_vote_applied=false\r\n");
            SudekiMpLogWrite("status=save_book_vote_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(save_book_error);
            return SUDEKIMP_INIT_SAVE_BOOK_VOTE_FAILED;
        }
        SudekiMpLogWrite(
            "save_book_vote_applied=true "
            "interaction_provenance_dependency=false\r\n");
    } else {
        SudekiMpLogWrite(
            "save_book_vote_applied=false default=false\r\n");
    }
    /* This pointer-slot observer cannot safely chain the broad SkillTrace
     * opcode hook and never changes native shop behavior. */
    if (merchant_checkout_trace_enabled) {
        if (!SudekiMpInstallMerchantProvenanceAdapter(game_module, TRUE)) {
            DWORD merchant_trace_error = GetLastError();

            SudekiMpUninstallMerchantProvenanceAdapter();
            SudekiMpLogFormat(
                "merchant_checkout_trace_error=%lu "
                "phase=exact_SOL_opcode_observer_install\r\n",
                (unsigned long)merchant_trace_error);
            SudekiMpLogWrite("merchant_checkout_trace_applied=false\r\n");
            SudekiMpLogWrite("status=merchant_checkout_trace_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(merchant_trace_error);
            return SUDEKIMP_INIT_TRACE_FAILED;
        }
        SudekiMpLogWrite(
            "merchant_checkout_trace_applied=true "
            "policy=passive_ShopStart_provenance_no_native_checkout\r\n");
    } else {
        SudekiMpLogWrite(
            "merchant_checkout_trace_applied=false "
            "default=false_or_missing_interaction_zone_or_skilltrace_conflict\r\n");
    }
    /* Install the lifecycle observer after every other fallible hook. It owns
     * the same raw opcode-0x27 slot as SkillTrace and merchant provenance, and
     * the closed profile rejects those owners before initialization. */
    SudekiMpLogFormat(
        "expanded_talos_lifecycle_trace_requested=%s "
        "dependencies=zone_transition_trace,interaction_provenance "
        "coverage=opcode29_opcode27_task_constructor_nested_SetZoneNOW_"
        "DeletePC_native_RemoveAllPlayers_native_FormationPopMembers_"
        "TSA_query_group_formation_counts "
        "missing=predelete_actor_generation_global_camera_cache "
        "policy=observation_only_native_passthrough_not_acceptance_ready\r\n",
        expanded_talos_lifecycle_trace_enabled ? "true" : "false"
    );
    SudekiMpLogFormat(
        "talos_post_movie_lifecycle_ticket_requested=%s "
        "exact_solworldm_authenticated=%s "
        "trigger=exact_Kazel_delete_plus_same_session_TSA_settle\r\n",
        talos_post_movie_party_restore_enabled ? "true" : "false",
        talos_exact_sol_authenticated ? "true" : "false");
    if (!(talos_post_movie_party_restore_enabled ?
            SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
                game_module, TRUE, TRUE, talos_exact_sol_authenticated) :
            SudekiMpInstallTalosNativeLifecycleTrace(
                game_module, expanded_talos_lifecycle_trace_enabled))) {
        DWORD lifecycle_trace_error = GetLastError();

        if (talos_post_movie_party_restore_enabled) {
            SudekiMpLogFormat(
                "talos_post_movie_lifecycle_ticket_error=%lu "
                "phase=exact_lifecycle_observer_install\r\n",
                (unsigned long)lifecycle_trace_error);
            SudekiMpLogWrite(
                "talos_post_movie_lifecycle_ticket_applied=false "
                "rollback=lifecycle_control_input_provenance_zone\r\n"
                "status=talos_post_movie_restore_error\r\n");
        } else {
            SudekiMpLogFormat(
                "expanded_talos_lifecycle_trace_error=%lu "
                "phase=exact_lifecycle_observer_install\r\n",
                (unsigned long)lifecycle_trace_error);
            SudekiMpLogWrite(
                "expanded_talos_lifecycle_trace_applied=false "
                "rollback=lifecycle_interaction_zone_and_runtime_hooks\r\n"
                "status=expanded_talos_lifecycle_trace_error\r\n");
        }
        uninstall_runtime_hooks();
        SudekiMpLogClose();
        SetLastError(lifecycle_trace_error);
        return talos_post_movie_party_restore_enabled ?
            SUDEKIMP_INIT_TALOS_POST_MOVIE_RESTORE_FAILED :
            SUDEKIMP_INIT_TALOS_LIFECYCLE_TRACE_FAILED;
    }
    if (expanded_talos_lifecycle_trace_enabled) {
        SudekiMpLogWrite(
            "expanded_talos_lifecycle_trace_applied=true "
            "coverage=opcode29_opcode27_task_constructor_nested_SetZoneNOW_"
            "DeletePC_native_RemoveAllPlayers_native_FormationPopMembers_"
            "TSA_query_group_formation_counts "
            "policy=observation_only_native_passthrough_not_acceptance_ready\r\n"
        );
    } else {
        SudekiMpLogWrite(
            "expanded_talos_lifecycle_trace_applied=false default=false\r\n"
        );
    }
    if (talos_post_movie_party_restore_enabled) {
        DWORD restore_error;

        SudekiMpLogWrite(
            "talos_post_movie_lifecycle_ticket_applied=true "
            "claim=one_process_terminal_attempt exact_asset_gate=closed\r\n");
        if (!SudekiMpInstallTalosPostMoviePartyRestore(game_module, TRUE)) {
            restore_error = GetLastError();
            SudekiMpLogFormat(
                "talos_post_movie_party_restore_error=%lu "
                "phase=restore_observer_and_AI_filter_install\r\n",
                (unsigned long)restore_error);
            SudekiMpLogWrite(
                "talos_post_movie_party_restore_applied=false "
                "rollback=restore_lifecycle_control_input_provenance_zone\r\n");
            SudekiMpLogWrite("status=talos_post_movie_restore_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(restore_error);
            return SUDEKIMP_INIT_TALOS_POST_MOVIE_RESTORE_FAILED;
        }
        SudekiMpLogWrite(
            "talos_post_movie_party_restore_applied=true "
            "roster=Tal_Ailish_Buki_Elco no_fifth_actor=true "
            "player_two=Ailish automatic_claim=true "
            "remaining_companions=native_AI\r\n");
    } else {
        SudekiMpLogWrite(
            "talos_post_movie_lifecycle_ticket_applied=false default=false\r\n"
            "talos_post_movie_party_restore_applied=false default=false\r\n");
    }
    /* Install this closed profile after every other fallible runtime path.
     * Adapter admission is pointer-free; the service-only controller wrapper
     * calls the native update exactly once and owns no gameplay features. The
     * capture observer is registered last, after all backing state is ready. */
    SudekiMpLogFormat(
        "talos_companion_staging_observation_requested=%s "
        "profile=closed_ordinary_world service_key=0 "
        "membership_mutation=absent default_camera_name_required=false "
        "reload_required=false modal_authority=false\r\n",
        talos_companion_staging_observation_enabled ? "true" : "false");
    if (talos_companion_staging_observation_enabled) {
        DWORD observation_error;

        if (!SudekiMpInstallTalosCompanionStagingResearchAdapter(
                game_module, TRUE, 0u, TRUE, TRUE)) {
            observation_error = GetLastError();
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=observation_adapter_install\r\n",
                (unsigned long)observation_error);
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(observation_error);
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        (void)InterlockedExchange(
            &talos_staging_observation_install_started, 1);
        if (!SudekiMpInstallControlSeparation(
                game_module,
                0u,
                FALSE,
                FALSE,
                FALSE,
                0.0f,
                FALSE,
                0u,
                FALSE,
                NULL,
                FALSE,
                FALSE,
                FALSE,
                0.0f)) {
            observation_error = GetLastError();
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=service_only_controller_install\r\n",
                (unsigned long)observation_error);
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(observation_error);
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        talos_staging_capture_configuration.controller_abi_valid = 1u;
        if (!SudekiMpTalosCompanionStagingNativeCaptureConfigure(
                &talos_staging_capture_configuration,
                talos_staging_observation_sink,
                NULL)) {
            observation_error = GetLastError();
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=immutable_capture_configure\r\n",
                (unsigned long)observation_error);
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(observation_error);
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        talos_staging_observation_last_logged_attempts = 0u;
        if (!SudekiMpControlUpdateObserverGateEnable(
                &talos_staging_observation_gate) ||
            !SudekiMpControlSeparationRegisterUpdateObserver(
                &talos_staging_observation_owner,
                talos_staging_observation_control_update_observer)) {
            observation_error = GetLastError();
            SudekiMpLogFormat(
                "talos_companion_staging_observation_error=%lu "
                "phase=sole_capture_observer_register\r\n",
                (unsigned long)observation_error);
            SudekiMpLogWrite("status=talos_staging_observation_error\r\n");
            uninstall_runtime_hooks();
            SudekiMpLogClose();
            SetLastError(observation_error);
            return SUDEKIMP_INIT_TALOS_STAGING_OBSERVATION_FAILED;
        }
        SudekiMpLogWrite(
            "talos_companion_staging_observation_applied=true "
            "observer=sole_service_post_original capture=automatic_one_shot "
            "retry_dispatches=120 policy=read_only_no_GetPC_RemovePlayer_"
            "AddPlayer_or_destructor\r\n");
    } else {
        SudekiMpLogWrite(
            "talos_companion_staging_observation_applied=false "
            "default=false\r\n");
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
        !save_book_vote_enabled &&
        !player_movement_trace_enabled &&
        !expanded_talos_lifecycle_trace_enabled &&
        !talos_post_movie_party_restore_enabled &&
        !lan_arena_enabled &&
        !talos_companion_staging_observation_enabled &&
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
            SudekiMpUninstallLanArenaPausePanel();
            SudekiMpUninstallLanArenaRuntime();
            (void)SudekiMpUninstallLanArenaWindowPolicy();
            SudekiMpUninstallTalosPostMoviePartyRestore();
            uninstall_talos_staging_observation();
            SudekiMpUninstallTalosNativeLifecycleTrace();
            SudekiMpUninstallSaveBookIntercept();
            SudekiMpUninstallMerchantProvenanceAdapter();
            SudekiMpSplitScreenSetModOwnedBlacksmithActiveQuery(NULL);
            SudekiMpUninstallBlacksmithUiAdapter();
            SudekiMpUninstallTalosDefenseTrace();
            SudekiMpUninstallInteractionProvenance();
            SudekiMpUninstallZoneTransitionTrace();
            SudekiMpUninstallSplitScreenRender();
            SudekiMpUninstallControlSeparation();
            SudekiMpUninstallXInputPlayerTwoReservation();
            SudekiMpInputBridgeStop();
            SudekiMpUninstallAcceleratorCache();
        }
        SudekiMpLogClose();
    }
    return TRUE;
}
