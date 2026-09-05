#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Keep this lifecycle test deterministic.  The runtime's worker owns only
 * transport polling, which is outside the callsite contract exercised here.
 * Fake kernel handles let installation and teardown run without a live
 * thread while the production runtime implementation remains unchanged. */
static HANDLE WINAPI test_create_event(
    LPSECURITY_ATTRIBUTES attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCWSTR name
);
static HANDLE WINAPI test_create_thread(
    LPSECURITY_ATTRIBUTES attributes,
    SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start,
    LPVOID parameter,
    DWORD flags,
    LPDWORD thread_id
);
static BOOL WINAPI test_set_event(HANDLE handle);
static DWORD WINAPI test_wait_for_single_object(HANDLE handle, DWORD timeout);
static BOOL WINAPI test_close_handle(HANDLE handle);

#define CreateEventW test_create_event
#define CreateThread test_create_thread
#define SetEvent test_set_event
#define WaitForSingleObject test_wait_for_single_object
#define CloseHandle test_close_handle
#include "../src/hooks/lan_arena_runtime.c"
#undef CloseHandle
#undef WaitForSingleObject
#undef SetEvent
#undef CreateThread
#undef CreateEventW

enum {
    TEST_IMAGE_SIZE = 0x002d0000u,
    TEST_RVA_RENDER_START = 0x001dce30u,
    TEST_RVA_RENDER_START_CALL = 0x0028d443u,
    TEST_RVA_RENDER_PRE_WORLD_CALL = 0x0028d539u,
    TEST_RVA_FRAME_END = 0x001dd540u,
    TEST_RVA_FRAME_END_CALL = 0x0028d58cu,
    TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    TEST_RVA_GAME_SPEED_SET_MODE = 0x00207560u,
    TEST_RVA_FIXED_ALTERNATE_SPEED = 0x002c4018u
};

static int failures;
static BOOL session_start_result = TRUE;
static BOOL campaign_guard_install_result = TRUE;
static BOOL campaign_guard_uninstall_result = TRUE;
static BOOL collision_debug_install_result = TRUE;
static BOOL spirit_audio_install_result = TRUE;
static BOOL spirit_audio_uninstall_result = TRUE;
static BOOL spirit_audio_installed;
static BOOL spirit_audio_physically_installed;
static BOOL spirit_visual_install_result = TRUE;
static BOOL spirit_visual_reset_result = TRUE;
static BOOL spirit_visual_capture_result = TRUE;
static BOOL spirit_visual_capture_observed;
static BOOL spirit_visual_install_leaves_lease;
static BOOL spirit_visual_installed;
static BOOL host_input_install_result = TRUE;
static BOOL client_input_install_result = TRUE;
static BOOL client_replica_initialize_result = TRUE;
static BOOL client_replica_reset_result = TRUE;
static BOOL client_tal_release_ready_result = TRUE;
static BOOL observer_gate_enable_result = TRUE;
static BOOL observer_register_result = TRUE;
static BOOL replica_apply_result = TRUE;
static BOOL visible_publish_result = TRUE;
static BOOL spirit_presentation_state_result = TRUE;
static BOOL character_skill_observe_result = TRUE;
static BOOL ranged_combat_prime_pending;
static BOOL ranged_combat_prime_result = TRUE;
static BOOL cleanroom_combat_enabled;
static BOOL cleanroom_pause_active;
static BOOL cleanroom_menu_active;
static BOOL host_spirit_request_available;
static unsigned int host_spirit_request_variant;
static BOOL host_spirit_options_result = TRUE;
static BOOL host_spirit_option_available = TRUE;
static BOOL host_spirit_activation_started = TRUE;
static BOOL host_tal_controller_lease_exact = TRUE;
static BOOL session_status_result;
static SudekiMpLanArenaSessionStatus session_status;
static BOOL player_two_active;
static void *player_two_character;
static BOOL player_two_request_result = TRUE;
static BOOL player_two_release_result = TRUE;
static BOOL actor_position_result;
static BOOL actor_facing_result;
static BOOL actor_resources_result;
static BOOL snapshot_send_result;
static BOOL actor_present_results[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
static BOOL spawn_actor_result;
static BOOL remove_actor_result = TRUE;
static BOOL remove_actor_clears_presence = TRUE;
static BOOL host_spirit_activation_probe_teardown;
static BOOL host_spirit_activation_probe_saw_busy;
static LONG host_spirit_activation_probe_depth;
static BOOL host_spirit_reproof_probe_teardown;
static BOOL host_spirit_reproof_probe_saw_busy;
static LONG host_spirit_reproof_probe_depth;
static BOOL host_spirit_describe_probe_teardown;
static BOOL host_spirit_describe_probe_saw_busy;
static LONG host_spirit_describe_probe_depth;
static int spirit_presentation_state;
static SudekiMpCharacterSkillState character_skill_observation;
static void *cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
static BOOL player_two_skill_isolation_enabled;
static unsigned int player_two_skill_isolation_call_count;
static unsigned int ranged_combat_prime_call_count;
static unsigned int maintain_resources_call_count;
static unsigned int host_spirit_request_take_count;
static unsigned int host_spirit_request_discard_count;
static unsigned int host_spirit_activation_call_count;
static unsigned int host_spirit_last_activated_variant;
static SudekiMpLanArenaHostNativeSkillStartObserver
    host_native_skill_start_observer;
static unsigned int session_start_count;
static unsigned int session_stop_count;
static unsigned int replica_initialize_count;
static unsigned int client_input_uninstall_count;
static unsigned int host_input_uninstall_count;
static unsigned int replica_reset_count;
static unsigned int client_tal_lease_publish_count;
static unsigned int client_tal_release_ready_count;
static void *client_tal_published_actor;
static uint32_t client_tal_published_generation;
static unsigned int request_player_two_count;
static unsigned int release_player_two_count;
static unsigned int spawn_actor_count;
static unsigned int remove_actor_count;
static unsigned int initialize_party_actor_count;
static char client_tal_teardown_events[8];
static size_t client_tal_teardown_event_count;
static unsigned int campaign_guard_uninstall_count;
static unsigned int collision_debug_uninstall_count;
static unsigned int spirit_audio_install_count;
static unsigned int spirit_audio_uninstall_count;
static unsigned int spirit_audio_physical_patch_count;
static unsigned int spirit_visual_install_count;
static unsigned int spirit_visual_reset_count;
static unsigned int spirit_visual_capture_count;
static unsigned int snapshot_send_count;
static SudekiMpLanArenaSnapshot last_sent_snapshot;
static SudekiMpLanArenaSpiritVisualHostWitness spirit_visual_witness;
static void *spirit_visual_witness_context;
static char spirit_observer_teardown_events[16];
static size_t spirit_observer_teardown_event_count;
static SudekiMpLanArenaSpiritActiveWitness spirit_audio_witness;
static void *spirit_audio_witness_context;
static SudekiMpLanArenaSpiritAudioEvent spirit_audio_events[
    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
static size_t spirit_audio_event_count;
static char callback_events[32];
static size_t callback_event_count;
static unsigned int log_format_call_count;

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error=%lu)\n", message,
            (unsigned long)GetLastError());
        ++failures;
    }
}

static void record_callback_event(char event) {
    if (callback_event_count + 1u < sizeof(callback_events)) {
        callback_events[callback_event_count++] = event;
        callback_events[callback_event_count] = '\0';
    }
}

static void record_client_tal_teardown_event(char event) {
    if (client_tal_teardown_event_count + 1u <
            sizeof(client_tal_teardown_events)) {
        client_tal_teardown_events[client_tal_teardown_event_count++] = event;
        client_tal_teardown_events[client_tal_teardown_event_count] = '\0';
    }
}

static void record_spirit_observer_teardown(char event) {
    if (spirit_observer_teardown_event_count + 1u <
            sizeof(spirit_observer_teardown_events)) {
        spirit_observer_teardown_events[spirit_observer_teardown_event_count++] = event;
        spirit_observer_teardown_events[spirit_observer_teardown_event_count] = '\0';
    }
}

static void reset_stub_policy(void) {
    session_start_result = TRUE;
    campaign_guard_install_result = TRUE;
    campaign_guard_uninstall_result = TRUE;
    collision_debug_install_result = TRUE;
    spirit_audio_install_result = TRUE;
    spirit_audio_uninstall_result = TRUE;
    spirit_visual_install_result = TRUE;
    spirit_visual_reset_result = TRUE;
    spirit_visual_capture_result = TRUE;
    spirit_visual_capture_observed = FALSE;
    spirit_visual_install_leaves_lease = FALSE;
    host_input_install_result = TRUE;
    client_input_install_result = TRUE;
    client_replica_initialize_result = TRUE;
    client_replica_reset_result = TRUE;
    client_tal_release_ready_result = TRUE;
    observer_gate_enable_result = TRUE;
    observer_register_result = TRUE;
    replica_apply_result = TRUE;
    visible_publish_result = TRUE;
    spirit_presentation_state_result = TRUE;
    character_skill_observe_result = TRUE;
    ranged_combat_prime_pending = FALSE;
    ranged_combat_prime_result = TRUE;
    cleanroom_combat_enabled = FALSE;
    cleanroom_pause_active = FALSE;
    cleanroom_menu_active = FALSE;
    host_spirit_request_available = FALSE;
    host_spirit_request_variant = 0u;
    host_spirit_options_result = TRUE;
    host_spirit_option_available = TRUE;
    host_spirit_activation_started = TRUE;
    host_tal_controller_lease_exact = TRUE;
    session_status_result = FALSE;
    memset(&session_status, 0, sizeof(session_status));
    player_two_active = FALSE;
    player_two_character = NULL;
    player_two_request_result = TRUE;
    player_two_release_result = TRUE;
    actor_position_result = FALSE;
    actor_facing_result = FALSE;
    actor_resources_result = FALSE;
    snapshot_send_result = FALSE;
    memset(actor_present_results, 0, sizeof(actor_present_results));
    spawn_actor_result = FALSE;
    remove_actor_result = TRUE;
    remove_actor_clears_presence = TRUE;
    host_spirit_activation_probe_teardown = FALSE;
    host_spirit_activation_probe_saw_busy = FALSE;
    host_spirit_activation_probe_depth = 0;
    host_spirit_reproof_probe_teardown = FALSE;
    host_spirit_reproof_probe_saw_busy = FALSE;
    host_spirit_reproof_probe_depth = 0;
    host_spirit_describe_probe_teardown = FALSE;
    host_spirit_describe_probe_saw_busy = FALSE;
    host_spirit_describe_probe_depth = 0;
    spirit_presentation_state = 0;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    memset(cleanroom_actor_entities, 0, sizeof(cleanroom_actor_entities));
    player_two_skill_isolation_enabled = FALSE;
    host_native_skill_start_observer = NULL;
}

static void reset_stub_counts(void) {
    session_start_count = 0u;
    session_stop_count = 0u;
    replica_initialize_count = 0u;
    client_input_uninstall_count = 0u;
    host_input_uninstall_count = 0u;
    replica_reset_count = 0u;
    client_tal_lease_publish_count = 0u;
    client_tal_release_ready_count = 0u;
    client_tal_published_actor = NULL;
    client_tal_published_generation = 0u;
    request_player_two_count = 0u;
    release_player_two_count = 0u;
    spawn_actor_count = 0u;
    remove_actor_count = 0u;
    initialize_party_actor_count = 0u;
    client_tal_teardown_event_count = 0u;
    client_tal_teardown_events[0] = '\0';
    campaign_guard_uninstall_count = 0u;
    collision_debug_uninstall_count = 0u;
    spirit_audio_install_count = 0u;
    spirit_audio_uninstall_count = 0u;
    spirit_visual_install_count = 0u;
    spirit_visual_reset_count = 0u;
    spirit_visual_capture_count = 0u;
    snapshot_send_count = 0u;
    memset(&last_sent_snapshot, 0, sizeof(last_sent_snapshot));
    spirit_observer_teardown_event_count = 0u;
    spirit_observer_teardown_events[0] = '\0';
    player_two_skill_isolation_call_count = 0u;
    ranged_combat_prime_call_count = 0u;
    maintain_resources_call_count = 0u;
    host_spirit_request_take_count = 0u;
    host_spirit_request_discard_count = 0u;
    host_spirit_activation_call_count = 0u;
    host_spirit_last_activated_variant = 0u;
    spirit_audio_event_count = 0u;
    memset(spirit_audio_events, 0, sizeof(spirit_audio_events));
    log_format_call_count = 0u;
}

static void write_call(
    uint8_t *image,
    uint32_t call_rva,
    uint32_t target_rva
) {
    int32_t displacement = (int32_t)(
        (image + target_rva) - (image + call_rva + 5u));
    image[call_rva] = 0xe8u;
    memcpy(image + call_rva + 1u, &displacement, sizeof(displacement));
}

static uint8_t *call_target(uint8_t *image, uint32_t call_rva) {
    int32_t displacement;
    memcpy(&displacement, image + call_rva + 1u, sizeof(displacement));
    return image + call_rva + 5u + displacement;
}

static void prepare_native_calls(uint8_t *image) {
    static const uint8_t camera_entry[] = {
        0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
    };
    static const uint8_t speed_entry[] = {
        0x8bu, 0x44u, 0x24u, 0x04u, 0x89u, 0x41u, 0x24u
    };
    static const uint8_t native_scale[] = {
        0x29u, 0x5cu, 0x8fu, 0x3du
    };
    write_call(
        image, TEST_RVA_RENDER_START_CALL, TEST_RVA_RENDER_START);
    write_call(
        image, TEST_RVA_RENDER_PRE_WORLD_CALL, TEST_RVA_RENDER_START);
    write_call(image, TEST_RVA_FRAME_END_CALL, TEST_RVA_FRAME_END);
    memcpy(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
        camera_entry, sizeof(camera_entry));
    memcpy(image + TEST_RVA_GAME_SPEED_SET_MODE,
        speed_entry, sizeof(speed_entry));
    memcpy(image + TEST_RVA_FIXED_ALTERNATE_SPEED,
        native_scale, sizeof(native_scale));
}

static SudekiMpLanArenaSessionConfig make_config(
    SudekiMpLanArenaRole role
) {
    SudekiMpLanArenaSessionConfig config;
    memset(&config, 0, sizeof(config));
    config.local_role = role;
    config.local_simulation_node_role =
        role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD :
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    config.port = SUDEKIMP_LAN_ARENA_DEFAULT_PORT;
    config.timeout_ms = 1000u;
    return config;
}

static void fixture_render_start(void) {
    record_callback_event('R');
}

static void fixture_frame_end(void) {
    record_callback_event('F');
}

static void verify_client_install_and_uninstall(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client runtime installs over exact native calls");
    check(SudekiMpLanArenaRuntimeInstalled(),
        "client runtime reports installed");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_start_entry,
        "client redirects first RenderStart call");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_pre_world_entry,
        "client redirects pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "client redirects frame-end call");
    check(spirit_audio_install_count == 0u && !spirit_audio_installed,
        "client profile never installs the host-only Spirit audio trace");
    check(spirit_visual_install_count == 0u && !spirit_visual_installed,
        "client profile never installs the host-only Spirit visual observer");

    SudekiMpUninstallLanArenaRuntime();
    check(!SudekiMpLanArenaRuntimeInstalled(),
        "client runtime reports uninstalled");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "client uninstall restores first RenderStart call");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START,
        "client uninstall restores pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "client uninstall restores frame-end call");
}

static void verify_host_hook_scope(uint8_t *image) {
    uint8_t original_camera[sizeof(expected_camera_manager_set_render_camera_entry)];
    uint8_t original_speed[sizeof(expected_game_speed_set_mode_entry)];
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    prepare_native_calls(image);
    memcpy(original_camera,
        image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
        sizeof(original_camera));
    memcpy(original_speed,
        image + TEST_RVA_GAME_SPEED_SET_MODE,
        sizeof(original_speed));
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host runtime installs over exact native calls");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_start_entry,
        "host redirects first RenderStart for safe owner-basis capture");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_pre_world_entry,
        "host redirects pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "host redirects frame-end call");
    check(memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) != 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) != 0,
        "host redirects remote-skill camera and realtime speed seams");
    check(host_native_skill_start_observer ==
            host_native_tal_skill_started,
        "host runtime registers the exact native Tal skill-start observer");
    check(spirit_audio_install_count == 1u && spirit_audio_installed &&
          spirit_audio_physical_patch_count == 1u &&
          spirit_audio_witness == host_spirit_audio_active_witness &&
          spirit_audio_witness_context == &runtime_config,
        "host runtime installs the exact Spirit-active PlayCue witness");
    check(spirit_visual_install_count == 1u && spirit_visual_installed &&
          spirit_visual_witness == host_spirit_visual_active_witness &&
          spirit_visual_witness_context == &runtime_config,
        "host runtime installs the exact Spirit visual sequence witness");
    tal_initialized = TRUE;
    spirit_presentation_state = 4;
    {
        int native_state = 0;
        check(spirit_audio_witness != NULL &&
              spirit_audio_witness(spirit_audio_witness_context,
                  &native_state) && native_state == 4,
            "host Spirit audio witness admits only readable active native state");
    }
    spirit_presentation_state = 0;
    tal_initialized = FALSE;
    SudekiMpUninstallLanArenaRuntime();
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "host uninstall restores first RenderStart call");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START,
        "host uninstall restores pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "host uninstall restores frame-end call");
    check(memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "host uninstall restores remote-skill camera and speed seams");
    check(host_native_skill_start_observer == NULL,
        "host input teardown releases the native skill-start observer");
    check(!spirit_audio_installed && spirit_audio_uninstall_count == 1u,
        "host uninstall restores the Spirit PlayCue trace exactly once");
    check(!spirit_visual_installed,
        "host uninstall releases the Spirit visual observer");
}

static void verify_host_spirit_audio_rollback(uint8_t *image) {
    uint8_t original_camera[
        sizeof(expected_camera_manager_set_render_camera_entry)];
    uint8_t original_speed[sizeof(expected_game_speed_set_mode_entry)];
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    memcpy(original_camera,
        image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
        sizeof(original_camera));
    memcpy(original_speed,
        image + TEST_RVA_GAME_SPEED_SET_MODE,
        sizeof(original_speed));
    spirit_audio_install_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_INVALID_DATA,
        "Spirit audio preflight failure rejects the host runtime");
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          spirit_audio_install_count == 1u &&
          spirit_audio_uninstall_count == 0u &&
          !spirit_audio_installed,
        "failed Spirit audio install leaves no false hook ownership");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END &&
          memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "Spirit audio preflight failure restores earlier host hooks");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    host_input_install_result = FALSE;
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host input failure rolls the earlier Spirit trace back");
    check(spirit_audio_install_count == 1u &&
          spirit_audio_uninstall_count == 1u &&
          !spirit_audio_installed &&
          host_input_uninstall_count == 1u,
        "downstream host failure releases input then Spirit trace");
    check(memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "downstream host failure restores skill isolation after Spirit trace");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host runtime reinstalls before Spirit restore retry test");
    check(SudekiMpUninstallLanArenaRuntime(),
        "host runtime logically unbinds the pinned Spirit trace");
    check(!SudekiMpLanArenaRuntimeInstalled() && !spirit_audio_installed &&
          spirit_audio_uninstall_count == 1u &&
          spirit_audio_physical_patch_count == 1u &&
          memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "logical Spirit unbind releases every ordinary host hook");

    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "later host runtime rebinds the process-lifetime Spirit trace");
    check(spirit_audio_installed &&
          spirit_audio_physical_patch_count == 1u,
        "host rebind does not install a second physical PlayCue patch");
    check(SudekiMpUninstallLanArenaRuntime(),
        "rebound host runtime unbinds cleanly");
    reset_stub_policy();
}

static void verify_host_spirit_visual_rollback(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    spirit_visual_install_result = FALSE;
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_BAD_FORMAT,
        "visual observer preflight failure preserves its install error");
    check(spirit_audio_install_count == 1u &&
          spirit_audio_uninstall_count == 1u &&
          spirit_visual_install_count == 1u && spirit_visual_reset_count == 1u &&
          !spirit_audio_installed && !spirit_visual_installed,
        "failed visual install resets partial visual state and unbinds prior audio");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
              image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
              image + TEST_RVA_FRAME_END,
        "visual preflight rollback restores earlier runtime callsites");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    spirit_visual_install_result = FALSE;
    spirit_visual_install_leaves_lease = TRUE;
    spirit_visual_reset_result = FALSE;
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_BUSY,
        "visual partial-install reset failure supersedes the install error");
    check(spirit_visual_installed && !spirit_audio_installed &&
          spirit_audio_uninstall_count == 1u &&
          strcmp(spirit_observer_teardown_events, "VA") == 0 &&
          original_render_start != NULL && original_frame_end != NULL &&
          call_target(image, TEST_RVA_RENDER_START_CALL) !=
              image + TEST_RVA_RENDER_START,
        "visual rollback failure still attempts audio and retains live dependencies");
    spirit_visual_reset_result = TRUE;
    spirit_visual_install_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime() && !spirit_visual_installed,
        "public uninstall retries retained partial visual installation");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host installs before independent observer restoration failures");
    spirit_visual_reset_result = FALSE;
    spirit_audio_uninstall_result = FALSE;
    check(!SudekiMpUninstallLanArenaRuntime() && GetLastError() == ERROR_BUSY,
        "combined observer teardown preserves the first visual reset failure");
    check(spirit_visual_reset_count == 1u && spirit_audio_uninstall_count == 1u &&
          spirit_visual_installed && spirit_audio_installed &&
          strcmp(spirit_observer_teardown_events, "VA") == 0 &&
          SudekiMpLanArenaRuntimeInstalled() && original_render_start != NULL,
        "both independent observer restorations run before runtime quarantine");
    spirit_visual_reset_result = TRUE;
    check(!SudekiMpUninstallLanArenaRuntime() &&
          GetLastError() == ERROR_WRITE_FAULT &&
          !spirit_visual_installed && spirit_audio_installed,
        "retry releases visual ownership but preserves failed audio ownership");
    spirit_audio_uninstall_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime() &&
          !spirit_visual_installed && !spirit_audio_installed &&
          !SudekiMpLanArenaRuntimeInstalled(),
        "final observer retry completes runtime teardown");
    reset_stub_policy();
}

static void verify_host_visual_witness_and_capture(uint8_t *image) {
    static uint8_t tal_character;
    static uint8_t ailish_character;
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    uint64_t token = 0u;
    uint16_t skill = 0u;
    uint32_t tick = 0u;
    SudekiMpLanArenaSpiritVfxSnapshot empty[SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY];
    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host installs before visual witness and optional capture checks");
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    session_status.peer_simulation_node_role = SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.session_token = 456u;
    tal_initialized = TRUE;
    ailish_initialized = TRUE;
    spirit_presentation_state = 4;
    host_actor_skill_sequence[0] = UINT16_MAX;
    host_spirit_previous_active = FALSE;
    check(spirit_visual_witness(spirit_visual_witness_context, &token, &skill, &tick) &&
          token == 456u && skill == 1u && host_actor_skill_sequence[0] == UINT16_MAX,
        "visual witness predicts the next nonzero Spirit sequence without mutation");
    host_actor_skill_sequence[0] = 7u;
    host_spirit_previous_active = TRUE;
    check(spirit_visual_witness(spirit_visual_witness_context, &token, &skill, &tick) &&
          skill == 7u,
        "visual witness preserves the current observed Spirit sequence");
    session_status.peer_connected = FALSE;
    check(!spirit_visual_witness(spirit_visual_witness_context, &token, &skill, &tick),
        "visual witness rejects disconnected authority");
    session_status.peer_connected = TRUE;
    spirit_presentation_state = 0;
    check(!spirit_visual_witness(spirit_visual_witness_context, &token, &skill, &tick),
        "visual witness rejects an inactive native Spirit");
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    reset_host_skill_tracking();
    actor_position_result = actor_facing_result = actor_resources_result = TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_character;
    snapshot_send_result = TRUE;
    spirit_visual_capture_result = FALSE;
    host_last_snapshot_at_ms = 0u;
    host_publish_snapshot(1000u);
    memset(empty, 0, sizeof(empty));
    check(spirit_visual_capture_count == 1u && snapshot_send_count == 1u &&
          last_sent_snapshot.spirit_vfx_observed == 0u &&
          last_sent_snapshot.spirit_vfx_count == 0u &&
          memcmp(last_sent_snapshot.spirit_vfx, empty, sizeof(empty)) == 0,
        "failed partially written visual sample becomes UNKNOWN without blocking gameplay");
    spirit_visual_capture_result = TRUE;
    spirit_visual_capture_observed = TRUE;
    host_publish_snapshot(1050u);
    check(spirit_visual_capture_count == 2u && snapshot_send_count == 2u &&
          last_sent_snapshot.spirit_vfx_observed == 1u &&
          last_sent_snapshot.spirit_vfx_count == 0u,
        "complete empty visual roster reaches the canonical snapshot as positive removal");
    check(SudekiMpUninstallLanArenaRuntime(),
        "visual witness/capture fixture tears down cleanly");
    reset_stub_policy();
}

static void verify_second_render_mismatch_rollback(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    prepare_native_calls(image);
    write_call(
        image,
        TEST_RVA_RENDER_PRE_WORLD_CALL,
        TEST_RVA_RENDER_START + 1u);
    reset_stub_counts();
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client rejects mismatched second RenderStart call");
    check(!SudekiMpLanArenaRuntimeInstalled(),
        "mismatched second RenderStart leaves runtime uninstalled");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "second RenderStart mismatch rolls first render hook back");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "second RenderStart mismatch rolls frame-end hook back");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START + 1u,
        "mismatch rollback preserves foreign second-call bytes");
    check(session_stop_count == 1u &&
            campaign_guard_uninstall_count == 1u &&
            collision_debug_uninstall_count == 1u,
        "second RenderStart mismatch rolls downstream ownership back");

    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client reinstall succeeds after second-call mismatch rollback");
    SudekiMpUninstallLanArenaRuntime();
}

static void verify_downstream_failure_rollback(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    prepare_native_calls(image);
    reset_stub_counts();
    client_replica_initialize_result = FALSE;
    SetLastError(ERROR_NOT_READY);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client replica init failure rejects runtime install");
    check(!SudekiMpLanArenaRuntimeInstalled(),
        "downstream failure leaves runtime uninstalled");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "downstream failure restores all three native calls");
    check(client_input_uninstall_count == 1u &&
            host_input_uninstall_count == 1u &&
            replica_reset_count == 1u &&
            session_stop_count == 1u,
        "downstream failure releases input replica and session state");

    client_replica_initialize_result = TRUE;
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client reinstall succeeds after downstream rollback");
    SudekiMpUninstallLanArenaRuntime();
}

static void verify_campaign_guard_teardown_containment(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client runtime installs before campaign teardown containment");
    campaign_guard_uninstall_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpUninstallLanArenaRuntime() &&
          GetLastError() == ERROR_BUSY,
        "campaign restore failure rejects runtime uninstall");
    check(SudekiMpLanArenaRuntimeInstalled() &&
          runtime_game_module == (HMODULE)image &&
          runtime_config.local_role ==
              SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
          campaign_guard_uninstall_count == 1u,
        "campaign restore failure retains runtime base config and ownership");

    campaign_guard_uninstall_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime(),
        "campaign restore retry completes runtime uninstall");
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          runtime_game_module == NULL &&
          runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_INVALID &&
          campaign_guard_uninstall_count == 2u,
        "successful campaign retry clears retained runtime state once safe");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    collision_debug_install_result = FALSE;
    campaign_guard_uninstall_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_BUSY,
        "campaign rollback failure supersedes downstream install error");
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          runtime_game_module == (HMODULE)image &&
          runtime_config.local_role ==
              SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
          campaign_guard_uninstall_count == 1u &&
          session_stop_count == 0u,
        "failed install rollback retains runtime state and live session lease");

    campaign_guard_uninstall_result = TRUE;
    collision_debug_install_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime(),
        "public runtime uninstall retries partial-install campaign rollback");
    check(runtime_game_module == NULL &&
          runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_INVALID &&
          campaign_guard_uninstall_count == 2u,
        "partial-install rollback retry clears retained runtime state");
    reset_stub_policy();
}

static void verify_client_busy_reset_containment(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    unsigned int start_count;
    unsigned int initialize_count;

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client runtime installs before BUSY reset containment checks");
    start_count = session_start_count;
    initialize_count = replica_initialize_count;
    client_remote_tal_owned = TRUE;
    client_tal_spawn_attempted = TRUE;
    client_replica_reset_result = FALSE;

    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpLanArenaRuntimeEndSession() &&
          GetLastError() == ERROR_BUSY,
        "End Session reports BUSY while the client CSkill drain is pending");
    check(SudekiMpLanArenaRuntimeInstalled() && client_remote_tal_owned &&
          release_player_two_count == 0u &&
          replica_initialize_count == initialize_count &&
          session_start_count == start_count,
        "BUSY End Session retains Tal and cannot reinitialize the session");

    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpLanArenaRuntimeJoinEndpoint("127.0.0.1:26770") &&
          GetLastError() == ERROR_BUSY,
        "Join reports BUSY while the prior client CSkill drain is pending");
    check(SudekiMpLanArenaRuntimeInstalled() && client_remote_tal_owned &&
          release_player_two_count == 0u &&
          replica_initialize_count == initialize_count &&
          session_start_count == start_count,
        "BUSY Join retains Tal and admits no replica or session reinit");

    SetLastError(ERROR_SUCCESS);
    SudekiMpUninstallLanArenaRuntime();
    check(GetLastError() == ERROR_BUSY &&
          SudekiMpLanArenaRuntimeInstalled() && client_remote_tal_owned &&
          release_player_two_count == 0u &&
          client_input_uninstall_count == 0u &&
          host_input_uninstall_count == 0u,
        "BUSY uninstall retains Tal, input adapters, and runtime ownership");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_start_entry &&
          call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_pre_world_entry &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "BUSY uninstall leaves all client frame callbacks installed");

    client_replica_reset_result = TRUE;
    SudekiMpUninstallLanArenaRuntime();
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          !client_remote_tal_owned && release_player_two_count == 1u,
        "confirmed drain permits exactly one Tal release and final uninstall");
    check(replica_initialize_count == initialize_count &&
          session_start_count == start_count,
        "final uninstall still performs no client session reinitialization");
}

static void verify_callback_order(void) {
    callback_event_count = 0u;
    callback_events[0] = '\0';
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    tal_initialized = TRUE;
    ailish_initialized = TRUE;
    original_render_start = fixture_render_start;
    replica_apply_result = TRUE;
    visible_publish_result = TRUE;
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "APXRVSCP") == 0,
        "first render orders visible Spirit VFX before native render and remote reassert");

    callback_event_count = 0u;
    callback_events[0] = '\0';
    lan_arena_render_pre_world_entry();
    check(strcmp(callback_events, "RSCP") == 0,
        "pre-world reasserts first-render basis without second refresh");

    callback_event_count = 0u;
    callback_events[0] = '\0';
    visible_publish_result = FALSE;
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "APRVSCP") == 0,
        "failed visible publication skips Spirit VFX admission");
    visible_publish_result = TRUE;

    callback_event_count = 0u;
    callback_events[0] = '\0';
    replica_apply_result = FALSE;
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "ARVC") == 0,
        "unapplied replica frame skips publication and Spirit VFX admission");
    replica_apply_result = TRUE;

    callback_event_count = 0u;
    callback_events[0] = '\0';
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    original_frame_end = fixture_frame_end;
    lan_arena_frame_end_entry();
    check(strcmp(callback_events, "FH") == 0,
        "frame-end callback preserves native-before-service ordering");

    original_render_start = NULL;
    original_frame_end = NULL;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
}

static void verify_client_tal_lifecycle_generation(void) {
    static uint8_t tal_character;
    uint32_t first_generation;
    uint32_t rearmed_generation;
    unsigned int publish_count;

    client_remote_tal_owned = FALSE;
    client_tal_spawn_attempted = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_generation_counter = 0u;
    client_remote_tal_active_generation = 0u;
    client_remote_tal_generation_actor = NULL;
    client_tal_release_ready_result = TRUE;
    client_tal_lease_publish_count = 0u;
    client_tal_published_actor = NULL;
    client_tal_published_generation = 0u;
    tal_initialized = TRUE;

    client_tal_spawn_attempted = TRUE;
    check(!client_tal_missing_actor_requires_release(),
        "pending asynchronous Tal spawn is not misclassified as actor loss");
    client_remote_tal_owned = TRUE;
    check(client_tal_missing_actor_requires_release(),
        "owned missing Tal requires lifecycle release");
    client_remote_tal_owned = FALSE;
    client_tal_spawn_attempted = FALSE;

    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation != 0u &&
          client_tal_published_actor == &tal_character &&
          client_tal_published_generation ==
              client_remote_tal_active_generation &&
          !tal_initialized,
        "Tal claim publishes a nonzero runtime lifecycle generation");
    first_generation = client_remote_tal_active_generation;
    publish_count = client_tal_lease_publish_count;
    tal_initialized = TRUE;
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation == first_generation &&
          client_tal_lease_publish_count == publish_count &&
          tal_initialized,
        "repeated exact Tal claim preserves one lifecycle generation");

    invalidate_client_remote_tal_generation("test_loss");
    check(client_remote_tal_active_generation == 0u &&
          client_remote_tal_generation_actor == NULL &&
          client_tal_published_actor == NULL &&
          client_tal_published_generation == 0u &&
          !tal_initialized,
        "Tal loss invalidates presentation before address reuse");
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation != first_generation,
        "same-address Tal replacement receives a fresh generation");

    invalidate_client_remote_tal_generation("test_wrap");
    client_remote_tal_generation_counter = UINT32_MAX;
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation == 1u,
        "Tal lifecycle generation skips zero on wrap");

    first_generation = client_remote_tal_active_generation;
    client_remote_tal_owned = TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    player_two_release_result = FALSE;
    check(!release_client_remote_tal("test_native_release_retry") &&
          client_remote_tal_owned &&
          client_remote_tal_active_generation == 0u &&
          client_tal_published_actor == NULL,
        "failed native Tal release keeps ownership but revokes presentation");
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation != first_generation &&
          client_tal_published_actor == &tal_character,
        "an exact retained PlayerTwo claim rearms a revoked Tal generation");
    rearmed_generation = client_remote_tal_active_generation;

    tal_initialized = TRUE;
    client_tal_release_ready_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!release_client_remote_tal("test_busy") &&
          GetLastError() == ERROR_BUSY &&
          client_remote_tal_active_generation == rearmed_generation &&
          tal_initialized,
        "busy native presentation defers Tal lifecycle invalidation");
    client_tal_release_ready_result = TRUE;
    player_two_release_result = TRUE;
    check(release_client_remote_tal("test_release") &&
          client_remote_tal_active_generation == 0u &&
          !tal_initialized,
        "confirmed Tal release invalidates generation and initialization");

    tal_initialized = TRUE;
    check(release_client_remote_tal("host_role") && tal_initialized,
        "no-client Tal release leaves host initialization unchanged");
}

static void verify_client_tal_pending_request_teardown(void) {
    static uint8_t tal_character;
    static uint8_t ailish_character;
    SudekiMpControlUpdateDispatchWitness witness;

    reset_stub_policy();
    reset_stub_counts();
    memset(&witness, 0, sizeof(witness));
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] =
        &ailish_character;
    actor_position_result = TRUE;
    spawn_actor_result = TRUE;
    client_remote_tal_owned = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_tal_spawn_attempted = FALSE;
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    client_release_pending_logged = FALSE;

    lan_arena_control_update_observer(NULL, NULL, &witness);
    check(client_tal_spawn_attempted && spawn_actor_count == 1u &&
          cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] == NULL &&
          !client_remote_tal_request_owned,
        "accepted asynchronous Tal spawn remains pending before publication");

    SetLastError(ERROR_SUCCESS);
    check(!release_client_remote_tal("test_spawn_publication_pending") &&
          GetLastError() == ERROR_BUSY && client_tal_spawn_attempted &&
          spawn_actor_count == 1u && release_player_two_count == 0u &&
          remove_actor_count == 0u,
        "unpublished Tal spawn blocks release without clearing its request");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = TRUE;
    check(release_client_remote_tal("test_spawn_published") &&
          !client_tal_spawn_attempted &&
          !client_remote_tal_remove_pending && remove_actor_count == 1u &&
          release_player_two_count == 0u &&
          cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] == NULL &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "published pending Tal spawn permits immediate confirmed removal");

    reset_stub_policy();
    reset_stub_counts();
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    client_tal_spawn_attempted = TRUE;
    client_remote_tal_owned = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    client_release_pending_logged = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = TRUE;
    remove_actor_clears_presence = FALSE;

    SetLastError(ERROR_SUCCESS);
    check(!release_client_remote_tal("test_remove_disappearance_pending") &&
          GetLastError() == ERROR_BUSY && client_tal_spawn_attempted &&
          client_remote_tal_remove_pending && remove_actor_count == 1u &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "successful Tal remove request waits for positive native disappearance");

    SetLastError(ERROR_SUCCESS);
    lan_arena_control_update_observer(NULL, NULL, &witness);
    check(GetLastError() == ERROR_BUSY && client_tal_spawn_attempted &&
          client_remote_tal_remove_pending && remove_actor_count == 1u &&
          request_player_two_count == 0u &&
          client_tal_lease_publish_count == 0u &&
          initialize_party_actor_count == 0u && spawn_actor_count == 0u &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "authenticated observer services pending removal without reclaiming Tal");

    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    check(release_client_remote_tal("test_remove_disappeared") &&
          !client_tal_spawn_attempted &&
          !client_remote_tal_remove_pending && remove_actor_count == 1u &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "positive Tal disappearance retires pending spawn and remove state");

    reset_stub_policy();
    reset_stub_counts();
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = TRUE;
    client_tal_spawn_attempted = TRUE;
    client_remote_tal_owned = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    client_release_pending_logged = FALSE;
    player_two_active = FALSE;
    player_two_character = NULL;
    player_two_request_result = TRUE;

    lan_arena_control_update_observer(NULL, NULL, &witness);
    check(request_player_two_count == 1u &&
          client_remote_tal_request_owned &&
          !client_remote_tal_owned &&
          client_remote_tal_active_generation == 0u,
        "accepted pre-publication PlayerTwo request acquires teardown ownership");

    player_two_release_result = FALSE;
    check(!release_client_remote_tal("test_player_two_request_pending") &&
          client_remote_tal_request_owned && client_tal_spawn_attempted &&
          release_player_two_count == 1u && remove_actor_count == 0u &&
          strcmp(client_tal_teardown_events, "L") == 0,
        "failed pre-publication PlayerTwo release retains request before actor removal");

    player_two_release_result = TRUE;
    check(release_client_remote_tal("test_player_two_request_released") &&
          !client_remote_tal_request_owned &&
          !client_remote_tal_owned && !client_tal_spawn_attempted &&
          !client_remote_tal_remove_pending &&
          release_player_two_count == 2u && remove_actor_count == 1u &&
          strcmp(client_tal_teardown_events, "LLR") == 0,
        "confirmed pre-publication PlayerTwo release precedes Tal removal");
    reset_stub_policy();
    reset_stub_counts();
}

static void verify_host_spirit_lifecycle(void) {
    static uint8_t tal_character;
    SudekiMpLanArenaActorSnapshot tal;

    memset(&tal, 0, sizeof(tal));
    memset(host_actor_skill_sequence, 0, sizeof(host_actor_skill_sequence));
    memset(host_actor_skill_kind, 0, sizeof(host_actor_skill_kind));
    memset(host_actor_skill_slot, 0, sizeof(host_actor_skill_slot));
    memset(host_actor_skill_cost, 0, sizeof(host_actor_skill_cost));
    memset(host_actor_previous_skill_active, 0,
        sizeof(host_actor_previous_skill_active));
    host_spirit_previous_active = FALSE;
    host_spirit_previous_state = 0;
    spirit_presentation_state_result = TRUE;
    spirit_presentation_state = 0;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    player_two_skill_isolation_enabled = FALSE;
    player_two_skill_isolation_call_count = 0u;

    check(host_apply_spirit_state(&tal) &&
          host_actor_skill_sequence[0] == 0u &&
          tal.skill_sequence == 0u &&
          player_two_skill_isolation_call_count == 0u,
        "inactive Spirit observation does not fabricate a transaction");

    spirit_presentation_state = 1;
    check(host_apply_spirit_state(&tal) &&
          host_actor_skill_sequence[0] == 1u &&
          tal.skill_sequence == 1u &&
          tal.skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
          tal.skill_slot == 0u && tal.skill_cost == 0u &&
          tal.skill_active == 1u &&
          player_two_skill_isolation_enabled,
        "Spirit start publishes one host-authoritative Tal transaction");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state = 2;
    check(host_apply_spirit_state(&tal) &&
          host_actor_skill_sequence[0] == 1u &&
          tal.skill_sequence == 1u && tal.skill_active == 1u,
        "Spirit native phase changes preserve one transaction sequence");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state = 0;
    check(host_apply_spirit_state(&tal) &&
          tal.skill_sequence == 1u &&
          tal.skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
          tal.skill_active == 0u,
        "Spirit completion publishes retirement for the same sequence");
    refresh_host_player_two_skill_isolation(&tal_character);
    check(!player_two_skill_isolation_enabled,
        "Spirit completion releases Ailish input isolation on refresh");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state = 3;
    check(host_apply_spirit_state(&tal) &&
          tal.skill_sequence == 2u && tal.skill_active == 1u &&
          player_two_skill_isolation_enabled,
        "a later Spirit activation advances exactly one sequence");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state_result = FALSE;
    check(!host_apply_spirit_state(&tal) &&
          tal.skill_sequence == 0u &&
          host_actor_skill_sequence[0] == 2u,
        "failed Spirit observation neither publishes nor advances state");

    spirit_presentation_state_result = TRUE;
    spirit_presentation_state = 0;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    reset_host_skill_tracking();
}

static void append_spirit_audio_event(
    uint32_t sequence,
    int native_state,
    const char *cue
) {
    SudekiMpLanArenaSpiritAudioEvent *event;
    size_t length = strlen(cue);
    check(spirit_audio_event_count <
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY &&
          length < SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY,
        "Spirit audio fixture remains bounded");
    if (spirit_audio_event_count >=
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY ||
        length >= SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY) return;
    event = &spirit_audio_events[spirit_audio_event_count++];
    memset(event, 0, sizeof(*event));
    event->sequence = sequence;
    event->native_state = native_state;
    event->cue_length = (uint8_t)length;
    memcpy(event->cue, cue, length + 1u);
}

static void verify_host_spirit_audio_semantic_journal(void) {
    BOOL previous_installed = spirit_audio_installed;
    SudekiMpLanArenaActorSnapshot tal;
    SudekiMpLanArenaSnapshot snapshot;
    HostSpiritAudioStage stage;

    spirit_audio_installed = TRUE;
    spirit_audio_event_count = 0u;
    memset(spirit_audio_events, 0, sizeof(spirit_audio_events));
    reset_host_spirit_audio_tracking();
    memset(&tal, 0, sizeof(tal));
    tal.skill_sequence = 7u;
    tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    tal.skill_active = 1u;
    append_spirit_audio_event(1u, 2, "stop_tal");
    append_spirit_audio_event(2u, 2, "spiritstrike_start");
    memset(&snapshot, 0, sizeof(snapshot));
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u &&
          snapshot.spirit_audio_history[0].event_sequence == 1u &&
          snapshot.spirit_audio_history[0].skill_sequence == 7u &&
          snapshot.spirit_audio_history[0].cue ==
              SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START,
        "host maps only the allowlisted raw start cue to exact Spirit sequence 7");
    check(host_spirit_audio_history_count == 0u,
        "host audio capture remains staged before canonical commit");
    commit_host_spirit_audio_stage(&stage);

    memset(&snapshot, 0, sizeof(snapshot));
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "re-reading the trace cannot duplicate a semantic start event");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(3u, 2, "spiritstrike_start");
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "a repeated raw start cannot duplicate one Spirit transaction");
    commit_host_spirit_audio_stage(&stage);

    tal.skill_sequence = 8u;
    tal.skill_active = 0u;
    append_spirit_audio_event(4u, 2, "spiritstrike_start");
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "raw start without the matching active Spirit transaction is discarded");
    commit_host_spirit_audio_stage(&stage);
    tal.skill_active = 1u;
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "discarded raw start cannot be rebound to a later active state");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(5u, 2, "spiritstrike_start");
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 2u &&
          snapshot.spirit_audio_history[1].event_sequence == 2u &&
          snapshot.spirit_audio_history[1].skill_sequence == 8u,
        "a later Spirit transaction receives one newer semantic start event");
    commit_host_spirit_audio_stage(&stage);

    reset_host_spirit_audio_tracking();
    memset(&snapshot, 0, sizeof(snapshot));
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 0u,
        "session reset drains old raw cues and clears the wire journal");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(6u, 2, "spiritstrike_start");
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u &&
          snapshot.spirit_audio_history[0].event_sequence == 1u &&
          snapshot.spirit_audio_history[0].skill_sequence == 8u,
        "fresh post-reset raw cue starts a fresh semantic sequence");
    commit_host_spirit_audio_stage(&stage);

    /* If every active snapshot fails, its staged start must not poison the
     * persistent journal. The first retired frame consumes the raw trace edge
     * without audio, commits an empty journal, and later frames continue. */
    reset_host_spirit_audio_tracking();
    tal.skill_sequence = 9u;
    tal.skill_active = 1u;
    append_spirit_audio_event(7u, 2, "spiritstrike_start");
    memset(&snapshot, 0, sizeof(snapshot));
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u &&
          host_spirit_audio_history_count == 0u,
        "failed active snapshot leaves persistent audio journal unchanged");
    tal.skill_active = 0u;
    memset(&snapshot, 0, sizeof(snapshot));
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 0u,
        "retired snapshot drops a start never admitted while active");
    commit_host_spirit_audio_stage(&stage);
    memset(&snapshot, 0, sizeof(snapshot));
    host_capture_spirit_audio(&tal, &snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 0u,
        "post-retirement snapshots continue after failed active publication");
    commit_host_spirit_audio_stage(&stage);
    spirit_audio_installed = previous_installed;
    reset_host_spirit_audio_tracking();
}

static SudekiMpLanArenaSessionStatus prepare_host_spirit_operator_fixture(
    void *tal,
    void *ailish,
    uint64_t session_token
) {
    SudekiMpLanArenaSessionStatus status;
    reset_stub_policy();
    reset_stub_counts();
    runtime_installed = TRUE;
    runtime_game_module = (HMODULE)(uintptr_t)1u;
    runtime_config = make_config(SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    tal_initialized = TRUE;
    ailish_initialized = TRUE;
    host_remote_ailish_owned = TRUE;
    player_two_active = TRUE;
    player_two_character = ailish;
    cleanroom_combat_enabled = TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = tal;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = ailish;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
    host_operator_spirit_session_token = session_token;
    memset(&status, 0, sizeof(status));
    status.phase = SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
    status.peer_connected = 1u;
    status.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    status.session_token = session_token;
    return status;
}

static void queue_host_spirit_request(unsigned int variant) {
    host_spirit_request_available = TRUE;
    host_spirit_request_variant = variant;
}

static void verify_host_spirit_operator_two_phase(void) {
    char tal_one;
    char tal_two;
    char ailish;
    char ailish_two;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaActorSnapshot snapshot;
    const uint64_t token = UINT64_C(0x1122334455667788);

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_operator_spirit_session_token = 0u;
    queue_host_spirit_request(2u);
    check(!host_operator_spirit_session_ready(&status) &&
          host_operator_spirit_session_token == token &&
          !host_spirit_request_available &&
          host_spirit_request_discard_count == 1u,
        "first authenticated Spirit generation discards a pre-session request");
    queue_host_spirit_request(1u);
    check(host_operator_spirit_session_ready(&status) &&
          host_spirit_request_available,
        "armed Spirit generation preserves a later same-session request");
    discard_host_operator_spirit_requests("test_generation_cleanup");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_spirit_describe_probe_teardown = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(host_operator_spirit_intent.pending &&
          host_operator_spirit_intent.tal == &tal_one &&
          host_operator_spirit_intent.ailish == &ailish &&
          host_operator_spirit_intent.session_token == token &&
          host_operator_spirit_intent.variant == 1u &&
          ranged_combat_prime_pending &&
          ranged_combat_prime_call_count == 1u &&
          host_spirit_activation_call_count == 0u &&
          host_spirit_describe_probe_saw_busy &&
          host_spirit_describe_probe_depth == 1,
        "host Spirit operator admits one exact intent and starts only the UI prime");
    service_host_operator_spirit(&status);
    check(host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "host Spirit operator cannot activate while native UI prime is pending");
    ranged_combat_prime_pending = FALSE;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 1u &&
          host_spirit_last_activated_variant == 1u &&
          host_actor_skill_sequence[0] == 0u,
        "positive UI retirement activates exactly once without fabricating a wire edge");
    service_host_operator_spirit(&status);
    check(host_spirit_activation_call_count == 1u,
        "retired host Spirit intent cannot execute twice");
    memset(&snapshot, 0, sizeof(snapshot));
    spirit_presentation_state = 3;
    check(host_apply_spirit_state(&snapshot) &&
          host_actor_skill_sequence[0] == 1u &&
          snapshot.skill_sequence == 1u &&
          snapshot.skill_active == 1u,
        "only the later positive native manager edge starts the Spirit wire transaction");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    host_spirit_activation_probe_teardown = TRUE;
    host_spirit_reproof_probe_teardown = TRUE;
    service_host_operator_spirit(&status);
    check(host_spirit_activation_call_count == 1u &&
          host_spirit_reproof_probe_saw_busy &&
          host_spirit_reproof_probe_depth == 1 &&
          host_spirit_activation_probe_saw_busy &&
          host_spirit_activation_probe_depth == 1 &&
          InterlockedCompareExchange(
              &host_operator_spirit_activation_depth, 0, 0) == 0,
        "native Spirit activation publishes an in-flight teardown barrier until return");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_spirit_option_available = FALSE;
    queue_host_spirit_request(2u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          ranged_combat_prime_call_count == 0u &&
          host_spirit_activation_call_count == 0u,
        "unavailable Tal Spirit variant is rejected before UI prime");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    spirit_presentation_state = 1;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "already-active native Spirit manager rejects operator admission");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "wrong LAN role rejects host Spirit operator admission");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    cleanroom_menu_active = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending,
        "active cleanroom menu rejects host Spirit operator admission");
    cleanroom_menu_active = FALSE;
    cleanroom_pause_active = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending,
        "active LAN pause panel rejects host Spirit operator admission");
    cleanroom_pause_active = FALSE;
    ranged_combat_prime_pending = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          ranged_combat_prime_call_count == 0u,
        "an existing native UI transition rejects a new Spirit intent");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    queue_host_spirit_request(2u);
    service_host_operator_spirit(&status);
    check(host_operator_spirit_intent.pending &&
          host_spirit_request_take_count == 2u &&
          host_spirit_activation_call_count == 0u,
        "concurrent Spirit request is consumed without replacing pending intent");
    ranged_combat_prime_pending = FALSE;
    service_host_operator_spirit(&status);
    check(host_spirit_activation_call_count == 1u &&
          host_spirit_last_activated_variant == 1u,
        "concurrent request cannot change the admitted Spirit variant");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    status.session_token = token + 1u;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed session token cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed authority role cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_two;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed Tal identity cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_two;
    player_two_character = &ailish_two;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed Ailish identity cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    player_two_active = FALSE;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "lost Ailish Player-2 lease cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    host_tal_controller_lease_exact = FALSE;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed Tal controller lease cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    cleanroom_menu_active = TRUE;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "menu entry after prime cancels Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    cleanroom_pause_active = TRUE;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "pause entry after prime cancels Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    ranged_combat_prime_pending = FALSE;
    cleanroom_combat_enabled = FALSE;
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "combat retirement cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_native_skill_leases[0].pending = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "concurrent native CSkill lease rejects Spirit operator admission");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_operator_spirit_intent.pending = TRUE;
    host_operator_spirit_intent.tal = &tal_one;
    host_operator_spirit_intent.ailish = &ailish;
    host_operator_spirit_intent.session_token = token;
    host_operator_spirit_intent.variant = 1u;
    queue_host_spirit_request(2u);
    discard_host_operator_spirit_requests("test_session_loss");
    check(!host_operator_spirit_intent.pending &&
          !host_spirit_request_available &&
          host_spirit_request_discard_count == 1u,
        "authority reset clears retained and raw Spirit operator requests");

    host_operator_spirit_intent.pending = TRUE;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "retained Spirit operator intent is a host teardown barrier");
    reset_host_operator_spirit_intent();
    ranged_combat_prime_pending = TRUE;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "engine-owned Spirit UI prime remains a teardown barrier after intent clear");

    reset_host_skill_tracking();
    runtime_installed = FALSE;
    runtime_game_module = NULL;
    host_remote_ailish_owned = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    ranged_combat_prime_pending = FALSE;
    spirit_presentation_state = 0;
}

static void verify_host_character_skill_observation_gap(void) {
    static uint8_t tal_character;
    SudekiMpLanArenaActorSnapshot tal;

    memset(&tal, 0, sizeof(tal));
    memset(host_actor_skill_sequence, 0, sizeof(host_actor_skill_sequence));
    memset(host_actor_skill_kind, 0, sizeof(host_actor_skill_kind));
    memset(host_actor_skill_slot, 0, sizeof(host_actor_skill_slot));
    memset(host_actor_skill_cost, 0, sizeof(host_actor_skill_cost));
    memset(host_actor_previous_skill_active, 0,
        sizeof(host_actor_previous_skill_active));
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    character_skill_observation.skill = &tal_character;
    character_skill_observation.slot = 2;
    character_skill_observation.cost = 45u;
    character_skill_observation.active = 1u;
    player_two_skill_isolation_enabled = FALSE;
    player_two_skill_isolation_call_count = 0u;

    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 1u &&
          host_actor_skill_sequence[0] == 1u &&
          host_actor_previous_skill_active[0] &&
          player_two_skill_isolation_enabled,
        "host starts one Tal CSkill sequence from an exact active observation");

    memset(&tal, 0, sizeof(tal));
    character_skill_observe_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          host_actor_skill_sequence[0] == 1u &&
          host_actor_previous_skill_active[0] &&
          host_actor_skill_slot[0] == 2u &&
          player_two_skill_isolation_enabled,
        "failed host CSkill observation preserves sequence and active ownership");

    memset(&tal, 0, sizeof(tal));
    character_skill_observe_result = TRUE;
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 1u &&
          host_actor_skill_sequence[0] == 1u &&
          player_two_skill_isolation_call_count == 1u,
        "active observation after a gap resumes the original CSkill sequence");

    memset(&tal, 0, sizeof(tal));
    character_skill_observation.active = 0u;
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 0u &&
          !host_actor_previous_skill_active[0] &&
          !player_two_skill_isolation_enabled,
        "positive inactive observation retires the retained Tal sequence");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
}

static void verify_host_exact_character_skill_sequences(void) {
    static uint8_t tal_character;
    static uint8_t tal_skill;
    static uint8_t ailish_character;
    static uint8_t ailish_skill;
    static uint8_t foreign_character;
    SudekiMpSkillActivationResult started;
    SudekiMpLanArenaActorSnapshot tal;

    reset_host_skill_tracking();
    memset(&tal, 0, sizeof(tal));
    memset(&started, 0, sizeof(started));
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    character_skill_observation.skill = &tal_skill;
    character_skill_observation.slot = 2;
    character_skill_observation.cost = 45u;
    character_skill_observation.active = 1u;
    started.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    started.skill = &tal_skill;
    started.slot = 2;

    host_track_started_native_skill(0u, &tal_character, &started);
    check(host_actor_skill_sequence[0] == 1u &&
          host_native_skill_leases[0].sequence_allocated &&
          host_native_skill_leases[0].wire_sequence == 1u &&
          !host_native_skill_leases[0].active_seen,
        "exact STARTED return allocates the first Tal wire sequence before observation");
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 1u &&
          tal.skill_cost == 45u &&
          host_actor_skill_sequence[0] == 1u,
        "active snapshot adopts an exact STARTED sequence without double increment");

    /* This is the problematic 1 -> 0 -> 1 same-slot cycle with both the
     * inactive edge and the second active edge hidden between snapshots.
     * The second exact admission must still create a distinct transaction. */
    host_track_started_native_skill(0u, &tal_character, &started);
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[0] == 2u &&
          host_native_skill_leases[0].wire_sequence == 2u &&
          host_native_skill_leases[0].sequence_allocated &&
          host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 2u && tal.skill_active == 1u &&
          host_actor_skill_sequence[0] == 2u,
        "same-slot activation hidden between 20 Hz samples keeps its exact second sequence");

    character_skill_observation.active = 0u;
    host_native_tal_skill_started(
        &tal_character, &tal_skill, 2, 45u, FALSE);
    check(host_actor_skill_sequence[0] == 3u &&
          host_native_skill_leases[0].pending &&
          !host_native_skill_leases[0].active_seen &&
          host_native_skill_startup_pending(
              0u, &tal_character, &character_skill_observation),
        "native Tal Use STARTED allocates during the inactive-byte startup gap");
    character_skill_observation.active = 1u;
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[0] == 3u &&
          host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          host_native_skill_leases[0].active_seen &&
          tal.skill_sequence == 3u &&
          host_actor_skill_sequence[0] == 3u,
        "host native UI startup-gap sequence is adopted once active appears");
    host_native_tal_skill_started(
        &foreign_character, &tal_skill, 2, 45u, TRUE);
    check(host_actor_skill_sequence[0] == 3u,
        "host native UI admission rejects a foreign actor identity");

    character_skill_observation.active = 0u;
    memset(&tal, 0, sizeof(tal));
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 3u && tal.skill_active == 0u &&
          !host_native_skill_leases[0].pending,
        "exact inactive observation retires the latest admitted sequence");

    reset_host_skill_tracking();
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_character;
    character_skill_observation.skill = &ailish_skill;
    character_skill_observation.slot = 4;
    character_skill_observation.cost = 30u;
    character_skill_observation.active = 1u;
    started.skill = &ailish_skill;
    started.slot = 4;
    host_track_started_native_skill(1u, &ailish_character, &started);
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[1] == 1u &&
          host_native_skill_leases[1].wire_sequence == 1u &&
          host_apply_skill_state(
              1u, SUDEKIMP_CLEANROOM_AILISH, &tal) &&
          tal.skill_sequence == 1u && tal.skill_cost == 30u &&
          host_actor_skill_sequence[1] == 1u,
        "remote Ailish exact STARTED sequence is adopted without snapshot double increment");
    host_track_started_native_skill(1u, &ailish_character, &started);
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[1] == 2u &&
          host_apply_skill_state(
              1u, SUDEKIMP_CLEANROOM_AILISH, &tal) &&
          tal.skill_sequence == 2u &&
          host_actor_skill_sequence[1] == 2u,
        "remote Ailish same-slot replay hidden between snapshots gets a distinct sequence");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
}

static void verify_host_snapshot_failure_telemetry_policy(void) {
    SudekiMpLanArenaSnapshot snapshot;
    unsigned int baseline;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.tal.skill_sequence = 7u;
    snapshot.tal.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.tal.skill_active = 1u;
    snapshot.tal.skill_presentation_selector[0] = 112;
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
    baseline = log_format_call_count;

    host_snapshot_publish_failed(
        100u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    check(log_format_call_count == baseline + 1u &&
          host_snapshot_failure_telemetry.stage ==
              HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION &&
          host_snapshot_failure_telemetry.consecutive_failures == 1u,
        "first unsupported Spirit selector snapshot logs its validation stage");
    host_snapshot_publish_failed(
        200u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    host_snapshot_publish_failed(
        1099u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    check(log_format_call_count == baseline + 1u &&
          host_snapshot_failure_telemetry.consecutive_failures == 3u,
        "repeated snapshot validation failures are quiet inside one second");
    host_snapshot_publish_failed(
        1100u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    check(log_format_call_count == baseline + 2u &&
          host_snapshot_failure_telemetry.consecutive_failures == 4u,
        "sustained snapshot validation failures aggregate once per second");

    host_snapshot_publish_failed(
        1101u, 33u, HOST_SNAPSHOT_FAILURE_SEND, &snapshot);
    check(log_format_call_count == baseline + 3u &&
          host_snapshot_failure_telemetry.stage ==
              HOST_SNAPSHOT_FAILURE_SEND &&
          host_snapshot_failure_telemetry.consecutive_failures == 1u,
        "a changed first-failure stage logs its transition immediately");
    host_snapshot_publish_succeeded(1102u, 33u);
    check(log_format_call_count == baseline + 4u &&
          host_snapshot_failure_telemetry.stage ==
              HOST_SNAPSHOT_FAILURE_NONE,
        "the first successful send closes and clears failure telemetry");
    host_snapshot_publish_succeeded(1103u, 33u);
    check(log_format_call_count == baseline + 4u,
        "an already-healthy snapshot stream emits no recovery spam");
}

static void verify_host_character_skill_sidecar_wire_fallback(void) {
    SudekiMpLanArenaActorSnapshot snapshot;
    SudekiMpLanArenaActorSnapshot zero;
    unsigned int channel;

    memset(&snapshot, 0, sizeof(snapshot));
    memset(&zero, 0, sizeof(zero));
    memset(host_actor_presentation, 0, sizeof(host_actor_presentation));
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
    host_actor_presentation_valid[1] = TRUE;
    for (channel = 0u;
         channel < SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS;
         ++channel) {
        host_actor_presentation[1].selector[channel] =
            channel == 0u ? AILISH_COMBAT_IDLE_SELECTOR : 0;
        host_actor_presentation[1].state[channel] =
            channel == 0u ? 0u : 192u;
        host_actor_presentation[1].rate[channel] = 24.0f;
        host_actor_presentation[1].time[channel] = 40.8f;
    }
    snapshot.skill_sequence = 6u;
    snapshot.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    snapshot.skill_slot = 5u;
    snapshot.skill_active = 1u;
    snapshot.skill_cost = 40u;
    host_actor_presentation[1].time[4] = 4096.0f;
    host_apply_skill_presentation(1u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u &&
          snapshot.skill_presentation_channel_count == 5u &&
          snapshot.skill_presentation_time[4] == 4096.0f &&
          SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_AILISH_TYPE),
        "bounded Ailish character-skill renderer sidecar survives at the exact wire clock limit");

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.skill_sequence = 6u;
    snapshot.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    snapshot.skill_slot = 5u;
    snapshot.skill_active = 1u;
    snapshot.skill_cost = 40u;
    host_actor_presentation[1].time[4] = 4161.93896f;
    host_apply_skill_presentation(1u, &snapshot);
    check(snapshot.skill_sequence == 6u &&
          snapshot.skill_kind ==
              SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
          snapshot.skill_slot == 5u && snapshot.skill_active == 1u &&
          snapshot.skill_cost == 40u &&
          snapshot.skill_presentation_valid == 0u &&
          snapshot.skill_presentation_channel_count == 0u &&
          memcmp(snapshot.skill_presentation_selector,
              zero.skill_presentation_selector,
              sizeof(snapshot.skill_presentation_selector)) == 0 &&
          memcmp(snapshot.skill_presentation_state,
              zero.skill_presentation_state,
              sizeof(snapshot.skill_presentation_state)) == 0 &&
          memcmp(snapshot.skill_presentation_rate,
              zero.skill_presentation_rate,
              sizeof(snapshot.skill_presentation_rate)) == 0 &&
          memcmp(snapshot.skill_presentation_time,
              zero.skill_presentation_time,
              sizeof(snapshot.skill_presentation_time)) == 0 &&
          memcmp(snapshot.skill_presentation_blend,
              zero.skill_presentation_blend,
              sizeof(snapshot.skill_presentation_blend)) == 0 &&
          SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_AILISH_TYPE),
        "out-of-range dormant Ailish channel omits only the optional sidecar and preserves sequence 6 slot 5");

    memset(&snapshot, 0, sizeof(snapshot));
    memset(&host_actor_presentation[0], 0,
        sizeof(host_actor_presentation[0]));
    host_actor_presentation_valid[0] = TRUE;
    host_actor_presentation[0].selector[0] = 113;
    host_actor_presentation[0].state[0] = 1u;
    host_actor_presentation[0].state[1] = 192u;
    host_actor_presentation[0].rate[0] = 24.0f;
    host_actor_presentation[0].time[0] = 64.0f;
    snapshot.skill_sequence = 7u;
    snapshot.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.skill_active = 1u;
    host_apply_skill_presentation(0u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u &&
          snapshot.skill_presentation_selector[0] == 113 &&
          SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_TAL_TYPE),
        "host keeps authored Spirit selector 113 publishable during the active middle stage");

    memset(&snapshot, 0, sizeof(snapshot));
    host_actor_presentation[0].selector[0] = 112;
    snapshot.skill_sequence = 7u;
    snapshot.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.skill_active = 1u;
    host_apply_skill_presentation(0u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u &&
          !SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_TAL_TYPE),
        "host retains a future unsupported Spirit selector for fail-closed snapshot diagnostics");

    memset(&snapshot, 0, sizeof(snapshot));
    host_actor_presentation[0].selector[0] = 75;
    host_actor_presentation[0].time[0] = 4161.93896f;
    snapshot.skill_sequence = 7u;
    snapshot.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.skill_active = 1u;
    host_apply_skill_presentation(0u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u,
        "active Spirit retains its required host renderer sidecar");
    check(snapshot.skill_presentation_time[0] > 4096.0f,
        "active Spirit keeps the observed out-of-range renderer clock");
    check(!SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_TAL_TYPE),
        "active Spirit keeps its required sidecar fail-closed instead of silently degrading");

    memset(host_actor_presentation, 0, sizeof(host_actor_presentation));
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
}

static void verify_host_noncaster_startup_gap_ownership(void) {
    static uint8_t tal_character;
    static uint8_t ailish_character;
    static uint8_t ailish_skill;
    static uint8_t foreign_skill;
    SudekiMpSkillActivationResult started;
    SudekiMpCharacterSkillState state;

    reset_host_skill_tracking();
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_character;
    memset(&started, 0, sizeof(started));
    started.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    started.skill = &ailish_skill;
    started.slot = 4;
    host_track_started_native_skill(1u, &ailish_character, &started);
    memset(&state, 0, sizeof(state));
    state.skill = &ailish_skill;
    state.slot = -1;
    state.active = 0u;

    check(host_native_skill_startup_pending(
              1u, &ailish_character, &state),
        "pre-world ownership retains Tal locomotion during Ailish STARTED active-byte gap");
    state.skill = &foreign_skill;
    check(!host_native_skill_startup_pending(
              1u, &ailish_character, &state),
        "pre-world startup ownership rejects a mismatched native skill lease");
    state.skill = &ailish_skill;
    check(!host_native_skill_startup_pending(
              1u, &tal_character, &state),
        "pre-world startup ownership rejects a mismatched caster actor");
    host_native_skill_leases[1].active_seen = TRUE;
    check(!host_native_skill_startup_pending(
              1u, &ailish_character, &state),
        "pre-world startup-only ownership ends after exact active observation");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    reset_host_skill_tracking();
}

static void verify_host_native_task_drain_without_peer(void) {
    static uint8_t tal_character;
    static uint8_t tal_skill;
    SudekiMpSkillActivationResult started;

    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    host_remote_ailish_owned = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    character_skill_observation.skill = &tal_skill;
    character_skill_observation.slot = 2;
    character_skill_observation.active = 1u;
    spirit_presentation_state_result = TRUE;
    spirit_presentation_state = 0;
    reset_host_skill_tracking();

    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "present no-peer Tal CSkill blocks host teardown");

    memset(&started, 0, sizeof(started));
    started.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    started.skill = &tal_skill;
    started.slot = 2u;
    host_track_started_native_skill(0u, &tal_character, &started);
    character_skill_observation.active = 0u;
    player_two_skill_isolation_enabled = FALSE;
    refresh_host_player_two_skill_isolation(&tal_character);
    check(player_two_skill_isolation_enabled,
        "Tal STARTED lease retains Ailish input through the active-byte startup gap");
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY &&
          host_native_skill_leases[0].pending &&
          !host_native_skill_leases[0].active_seen,
        "STARTED followed by an early inactive read retains the host task lease");

    character_skill_observation.active = 1u;
    check(!host_native_tasks_drained() &&
          host_native_skill_leases[0].pending &&
          host_native_skill_leases[0].active_seen,
        "exact active observation arms host task retirement");
    character_skill_observation.active = 0u;
    check(host_native_tasks_drained() &&
          !host_native_skill_leases[0].pending,
        "exact inactive observation after active retires host task lease");
    refresh_host_player_two_skill_isolation(&tal_character);
    check(!player_two_skill_isolation_enabled,
        "retired Tal task releases Ailish input isolation");

    spirit_presentation_state = 1;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "present no-peer Tal Spirit transaction blocks host teardown");
    spirit_presentation_state = 0;
    check(host_native_tasks_drained(),
        "positive inactive no-peer Tal and Spirit observations permit teardown");

    ranged_combat_prime_pending = TRUE;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "pending native ranged-prime timer blocks host teardown");
    ranged_combat_prime_pending = FALSE;

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
}

static void verify_authoritative_locomotion_stop_policy(void) {
    SudekiMpLanArenaActorSnapshot tal;
    SudekiMpLanArenaActorSnapshot ailish;
    memset(&tal, 0, sizeof(tal));
    memset(&ailish, 0, sizeof(ailish));
    tal.hp = 100u;
    ailish.hp = 100u;
    memset(host_previous_actor_position, 0,
        sizeof(host_previous_actor_position));
    memset(host_previous_actor_position_valid, 0,
        sizeof(host_previous_actor_position_valid));
    memset(host_actor_last_translation_at_ms, 0,
        sizeof(host_actor_last_translation_at_ms));
    memset(host_replica_idle_position, 0,
        sizeof(host_replica_idle_position));
    memset(host_replica_idle_position_valid, 0,
        sizeof(host_replica_idle_position_valid));
    memset(host_actor_was_moving, 0,
        sizeof(host_actor_was_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
    reset_host_action_tracking();

    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "first Tal sample begins idle without fabricated translation");
    tal.x = 0.01f;
    host_apply_presentation_state(0u, 150u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "authoritative Tal horizontal translation starts locomotion");
    host_apply_presentation_state(0u, 300u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "Tal stop grace includes its exact bounded endpoint");
    host_apply_presentation_state(0u, 301u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Tal becomes idle immediately after bounded stop grace");
    tal.x = 0.012f;
    host_apply_presentation_state(0u, 351u, FALSE, &tal);
    check(fabsf(tal.x - 0.01f) < 0.00001f,
        "stationary Tal replica suppresses sub-threshold native settling");
    tal.x = 0.014f;
    host_apply_presentation_state(0u, 376u, FALSE, &tal);
    check(fabsf(tal.x - 0.01f) < 0.00001f,
        "successive sub-threshold Tal steps remain bounded by idle latch");
    tal.x = 0.016f;
    host_apply_presentation_state(0u, 401u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
          fabsf(tal.x - 0.016f) < 0.00001f,
        "cumulative Tal translation releases latch without hidden backlog");

    host_actor_presentation_valid[0] = TRUE;
    host_actor_presentation[0].selector[0] = TAL_WORLD_IDLE_SELECTOR;
    tal.x = 0.03f;
    host_apply_presentation_state(0u, 451u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "native Tal idle selector ends replica locomotion despite root motion");
    host_actor_presentation[0].selector[0] = TAL_WORLD_MOVE_PRIMARY_SELECTOR;
    host_apply_presentation_state(0u, 1000u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "native Tal movement selector preserves visible host locomotion");
    host_actor_presentation_valid[0] = FALSE;

    host_remote_ailish_owned = TRUE;
    host_remote_ailish_moving = TRUE;
    host_apply_presentation_state(1u, 1000u, FALSE, &ailish);
    ailish.y = 1.0f;
    host_apply_presentation_state(1u, 1050u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "held Ailish input and vertical floor motion cannot fabricate running");
    ailish.z = 0.01f;
    host_apply_presentation_state(1u, 1100u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "authoritative Ailish horizontal translation starts locomotion");
    host_apply_presentation_state(1u, 1251u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Ailish stops against a wall despite continuously held input");
    ailish.z = 0.012f;
    host_apply_presentation_state(1u, 1301u, FALSE, &ailish);
    check(fabsf(ailish.z - 0.01f) < 0.00001f,
        "stationary Ailish replica suppresses native root-motion settling");
    host_remote_ailish_owned = FALSE;
    host_remote_ailish_moving = FALSE;
    ailish.z = 0.25f;
    host_apply_presentation_state(1u, 1351u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          fabsf(ailish.z - 0.01f) < 0.00001f,
        "Ailish idle root-motion lunge cannot release latch without input");

    memset(&host_actor_presentation[1], 0,
        sizeof(host_actor_presentation[1]));
    host_actor_presentation_valid[1] = TRUE;
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    host_actor_presentation[1].selector[0] = AILISH_WORLD_IDLE_SELECTOR;
    host_actor_presentation[1].selector[2] =
        AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR;
    host_actor_presentation[1].state[2] = 1u;
    host_apply_presentation_state(1u, 2000u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        "Ailish idle variant is recognized while cross-fading on channel two");
    host_actor_presentation[1].selector[2] = 0;
    host_actor_presentation[1].state[2] = 192u;
    host_apply_presentation_state(1u, 2250u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        "Ailish idle variant survives exact channel-transition grace endpoint");
    host_apply_presentation_state(1u, 2251u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Ailish idle variant retires after stable base-idle evidence");
    host_actor_presentation[1].selector[0] =
        AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    host_actor_presentation[1].state[0] = 1u;
    host_apply_presentation_state(1u, 2300u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO,
        "Ailish second idle variant remains recognized on channel zero");

    host_remote_ailish_moving = TRUE;
    ailish.z += 0.02f;
    host_apply_presentation_state(1u, 2350u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
          !host_ailish_idle_variant_armed,
        "Ailish movement cancels and disarms the active idle variant");
    host_remote_ailish_moving = FALSE;
    host_actor_presentation[1].selector[0] = AILISH_WORLD_IDLE_SELECTOR;
    host_actor_presentation[1].state[0] = 128u;
    host_actor_presentation[1].selector[2] =
        AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    host_actor_presentation[1].state[2] = 65u;
    host_apply_presentation_state(1u, 2400u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          !host_ailish_idle_variant_armed,
        "hidden state-65 Ailish variant cannot resume after movement");
    host_actor_presentation[1].selector[2] = 0;
    host_actor_presentation[1].state[2] = 192u;
    host_apply_presentation_state(1u, 2450u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          host_ailish_idle_variant_armed,
        "clean Ailish base idle rearms future native variants");
    host_actor_presentation[1].selector[2] =
        AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR;
    host_actor_presentation[1].state[2] = 1u;
    host_apply_presentation_state(1u, 2500u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        "new native state-1 Ailish variant starts after clean rearm");

    memset(host_actor_presentation, 0, sizeof(host_actor_presentation));
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
    host_actor_presentation_valid[0] = TRUE;
    host_actor_presentation[0].selector[0] = TAL_COMBAT_IDLE_SELECTOR;
    host_actor_presentation[0].state[0] = 128u;
    tal.x += 0.03f;
    host_apply_presentation_state(0u, 3000u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Tal combat idle overrides residual authoritative root translation");
    host_actor_presentation[0].selector[0] =
        TAL_COMBAT_MOVE_PRIMARY_SELECTOR;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3050u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "Tal combat locomotion maps to semantic movement");
    host_actor_presentation[0].selector[0] = TAL_COMBAT_ENTRY_SELECTOR;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3100u, TRUE, &tal);
    check(tal.animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_NONE,
        "Tal draw-weapon transition is not mislabeled as a weak attack");
    host_actor_presentation[0].selector[0] = 50;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3350u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE &&
          tal.action_sequence == 1u && tal.action_phase_valid == 1u &&
          tal.action_phase_q8 == 0u,
        "Tal first native weak variant is transmitted semantically");
    host_actor_presentation[0].state[0] = 1u;
    host_actor_presentation[0].time[0] = 17.5f;
    host_apply_presentation_state(0u, 3360u, TRUE, &tal);
    check(tal.action_sequence == 1u && tal.action_phase_valid == 1u &&
          tal.action_phase_q8 == 17u * 256u + 128u,
        "Tal internal clip state cycling preserves one action and host phase");
    host_actor_presentation[0].selector[0] = 51;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3375u, TRUE, &tal);
    check(tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO &&
          tal.action_sequence == 2u,
        "Tal second native weak variant remains distinct");
    host_actor_presentation[0].time[0] = 49.5f;
    host_apply_presentation_state(0u, 3390u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO &&
          tal.action_sequence == 2u,
        "Tal terminal action remains sequenced before retirement");
    {
        SudekiMpCleanroomActorPresentation terminal =
            host_actor_presentation[0];
        SudekiMpCleanroomActorPresentation idle;
        terminal.time[0] = 49.5f;
        memset(&idle, 0, sizeof(idle));
        idle.selector[0] = TAL_COMBAT_IDLE_SELECTOR;
        idle.state[0] = 0u;
        idle.time[0] = 2.25f;
        host_capture_actor_action_retirement(0u, &terminal, &idle);
        host_actor_presentation[0] = idle;
    }
    host_apply_presentation_state(0u, 3400u, TRUE, &tal);
    check(tal.animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_sequence == 2u && tal.action_phase_valid == 0u &&
          tal.action_phase_q8 == 0u &&
          tal.action_retirement_valid == 1u &&
          tal.action_terminal_phase_q8 == 49u * 256u + 128u &&
          tal.idle_entry_phase_q8 == 2u * 256u + 64u,
        "Tal combat action retirement carries terminal and idle clocks");
    host_apply_presentation_state(0u, 3410u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          tal.action_sequence == 2u &&
          tal.action_retirement_valid == 1u &&
          tal.action_terminal_phase_q8 == 49u * 256u + 128u &&
          tal.idle_entry_phase_q8 == 2u * 256u + 64u,
        "Tal retirement handoff remains latched throughout idle");
    host_actor_presentation[0].selector[0] = 52;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3425u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_STRONG &&
          tal.action_sequence == 3u,
        "Tal native strong attack is transmitted semantically");
    host_actor_presentation[0].selector[0] = 53;
    host_apply_presentation_state(0u, 3435u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO &&
          tal.action_sequence == 4u,
        "Tal native second-stage strong attack remains distinct");
    host_actor_presentation[0].selector[0] = 54;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3440u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS &&
          tal.action_sequence == 5u,
        "Tal WWS heavy finisher is transmitted by exact combo identity");
    host_actor_presentation[0].selector[0] = 71;
    host_actor_presentation[0].state[0] = 1u;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3450u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_SWEEP &&
          tal.action_sequence == 6u,
        "Tal native sweep attack is transmitted semantically");
    host_actor_presentation[0].selector[0] = 20;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3475u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK &&
          tal.action_sequence == 7u,
        "Tal native block entry is transmitted semantically");
    host_actor_presentation[0].selector[0] = 21;
    host_actor_presentation[0].state[0] = 128u;
    host_apply_presentation_state(0u, 3490u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK &&
          tal.action_sequence == 7u,
        "Tal native block hold preserves the same semantic action edge");

    host_actor_presentation_valid[1] = TRUE;
    host_actor_presentation[1].selector[4] = AILISH_COMBAT_WEAK_SELECTOR;
    host_actor_presentation[1].state[4] = 1u;
    host_actor_presentation[1].time[4] = 9.25f;
    host_apply_presentation_state(1u, 3500u, TRUE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          ailish.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK &&
          ailish.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE &&
          ailish.action_sequence == 1u && ailish.action_phase_valid == 1u &&
          ailish.action_phase_q8 == 9u * 256u + 64u,
        "Ailish combat shot remains active for its native clip");
    host_actor_presentation[1].state[4] = 65u;
    host_apply_presentation_state(1u, 3800u, TRUE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          ailish.action_sequence == 1u,
        "Ailish combat shot survives beyond the old 250ms pulse");
    host_actor_presentation[1].selector[4] = 0;
    host_actor_presentation[1].state[4] = 192u;
    host_apply_presentation_state(1u, 3850u, TRUE, &ailish);
    check(ailish.animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          ailish.action_sequence == 1u,
        "Ailish combat shot retires with the native action layer");

    reset_host_action_tracking();
    memset(&ailish, 0, sizeof(ailish));
    host_track_actor_action_sequence(
        1u, 4000u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    host_track_actor_action_sequence(
        1u, 4050u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    host_track_actor_action_sequence(
        1u, 4100u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    check(host_actor_action_sequence[1] == 1u &&
          host_actor_action_history_count[1] == 1u,
        "one fallback ranged pulse journals one Ailish action edge");
    host_track_actor_action_sequence(
        1u, 4300u, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_NONE, 0u, &ailish);
    host_track_actor_action_sequence(
        1u, 4350u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    check(host_actor_action_sequence[1] == 2u &&
          host_actor_action_history_count[1] == 2u,
        "a later fallback ranged pulse journals exactly one new edge");

    check(SudekiMpLanArenaRangedRepeatReady(5000u, 0u) &&
          !SudekiMpLanArenaRangedRepeatReady(5499u, 5500u) &&
          SudekiMpLanArenaRangedRepeatReady(5500u, 5500u) &&
          !SudekiMpLanArenaRangedRepeatReady(0xfffffff0u, 0x00000010u) &&
          SudekiMpLanArenaRangedRepeatReady(0x00000010u, 0x00000010u),
        "Ailish ranged repeat cadence is wrap-safe and inclusive");
    check(SudekiMpLanArenaRangedRepeatIntervalMs(0x4000u) == 2000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0x3c00u) == 1000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0x0000u) == 2000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0x7c00u) == 2000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0xbc00u) == 2000u,
        "Ailish ranged cadence decodes safe authored half-float seconds");
}

int main(void) {
    uint8_t *image = (uint8_t *)VirtualAlloc(
        NULL,
        TEST_IMAGE_SIZE,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    check(image != NULL, "allocate synthetic supported image");
    if (image == NULL) return 1;

    memset(image, 0xcc, TEST_IMAGE_SIZE);
    reset_stub_policy();
    reset_stub_counts();

    verify_client_install_and_uninstall(image);
    verify_client_install_and_uninstall(image);
    verify_host_hook_scope(image);
    verify_host_spirit_audio_rollback(image);
    verify_host_spirit_visual_rollback(image);
    verify_host_visual_witness_and_capture(image);
    verify_second_render_mismatch_rollback(image);
    verify_downstream_failure_rollback(image);
    verify_campaign_guard_teardown_containment(image);
    verify_client_busy_reset_containment(image);
    verify_callback_order();
    verify_client_tal_lifecycle_generation();
    verify_client_tal_pending_request_teardown();
    verify_host_spirit_lifecycle();
    verify_host_spirit_audio_semantic_journal();
    verify_host_spirit_operator_two_phase();
    verify_host_character_skill_observation_gap();
    verify_host_exact_character_skill_sequences();
    verify_host_snapshot_failure_telemetry_policy();
    verify_host_character_skill_sidecar_wire_fallback();
    verify_host_noncaster_startup_gap_ownership();
    verify_host_native_task_drain_without_peer();
    verify_authoritative_locomotion_stop_policy();

    VirtualFree(image, 0u, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d LAN runtime hook test(s) failed\n", failures);
        return 1;
    }
    puts("LAN arena runtime hook tests passed");
    return 0;
}

static HANDLE WINAPI test_create_event(
    LPSECURITY_ATTRIBUTES attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCWSTR name
) {
    (void)attributes;
    (void)manual_reset;
    (void)initial_state;
    (void)name;
    return (HANDLE)(uintptr_t)1u;
}

static HANDLE WINAPI test_create_thread(
    LPSECURITY_ATTRIBUTES attributes,
    SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start,
    LPVOID parameter,
    DWORD flags,
    LPDWORD thread_id
) {
    (void)attributes;
    (void)stack_size;
    (void)start;
    (void)parameter;
    (void)flags;
    if (thread_id != NULL) *thread_id = 1u;
    return (HANDLE)(uintptr_t)2u;
}

static BOOL WINAPI test_set_event(HANDLE handle) {
    (void)handle;
    return TRUE;
}

static DWORD WINAPI test_wait_for_single_object(HANDLE handle, DWORD timeout) {
    (void)handle;
    (void)timeout;
    return WAIT_OBJECT_0;
}

static BOOL WINAPI test_close_handle(HANDLE handle) {
    (void)handle;
    return TRUE;
}

/* Runtime dependency fakes.  They deliberately expose only lifecycle state;
 * native actor/network behavior belongs to the focused adapter tests. */
BOOL SudekiMpLanArenaSessionStart(
    const SudekiMpLanArenaSessionConfig *config
) {
    (void)config;
    ++session_start_count;
    if (!session_start_result) SetLastError(ERROR_NOT_READY);
    return session_start_result;
}

void SudekiMpLanArenaSessionStop(BOOL notify_peer) {
    (void)notify_peer;
    ++session_stop_count;
}

void SudekiMpLanArenaSessionPoll(uint32_t now_ms) {
    (void)now_ms;
}

BOOL SudekiMpLanArenaSessionGetStatus(SudekiMpLanArenaSessionStatus *status) {
    if (status != NULL) {
        if (session_status_result) {
            *status = session_status;
        } else {
            memset(status, 0, sizeof(*status));
        }
    }
    return session_status_result;
}

BOOL SudekiMpLanArenaSessionTakeRemoteInput(SudekiMpLanArenaInput *input) {
    (void)input;
    return FALSE;
}

BOOL SudekiMpLanArenaSessionSendSnapshot(
    const SudekiMpLanArenaSnapshot *snapshot
) {
    ++snapshot_send_count;
    if (snapshot != NULL) last_sent_snapshot = *snapshot;
    return snapshot_send_result;
}

BOOL SudekiMpInstallLanArenaCampaignGuard(HMODULE game_module) {
    (void)game_module;
    if (!campaign_guard_install_result) SetLastError(ERROR_INVALID_DATA);
    return campaign_guard_install_result;
}

BOOL SudekiMpUninstallLanArenaCampaignGuard(void) {
    ++campaign_guard_uninstall_count;
    if (!campaign_guard_uninstall_result) SetLastError(ERROR_BUSY);
    return campaign_guard_uninstall_result;
}

BOOL SudekiMpInstallLanArenaCollisionDebug(HMODULE game_module) {
    (void)game_module;
    if (!collision_debug_install_result) SetLastError(ERROR_INVALID_DATA);
    return collision_debug_install_result;
}

void SudekiMpLanArenaCollisionDebugServiceHotkey(void) {
    record_callback_event('H');
}

unsigned int SudekiMpLanArenaCollisionDebugMode(void) {
    return SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
}

void SudekiMpUninstallLanArenaCollisionDebug(void) {
    ++collision_debug_uninstall_count;
}

BOOL SudekiMpInstallLanArenaSpiritAudioTrace(
    HMODULE game_module,
    SudekiMpLanArenaSpiritActiveWitness active_witness,
    void *witness_context
) {
    (void)game_module;
    ++spirit_audio_install_count;
    if (!spirit_audio_install_result) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!spirit_audio_physically_installed) {
        spirit_audio_physically_installed = TRUE;
        ++spirit_audio_physical_patch_count;
    }
    spirit_audio_installed = TRUE;
    spirit_audio_witness = active_witness;
    spirit_audio_witness_context = witness_context;
    return TRUE;
}

BOOL SudekiMpUninstallLanArenaSpiritAudioTrace(void) {
    if (!spirit_audio_installed) return TRUE;
    ++spirit_audio_uninstall_count;
    record_spirit_observer_teardown('A');
    if (!spirit_audio_uninstall_result) {
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    spirit_audio_installed = FALSE;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritAudioTraceInstalled(void) {
    return spirit_audio_installed;
}

BOOL SudekiMpLanArenaSpiritVisualHostImageMatches(HMODULE game_module) {
    return game_module != NULL && spirit_visual_install_result;
}

BOOL SudekiMpLanArenaSpiritVisualHostInitialize(
    HMODULE game_module,
    SudekiMpLanArenaSpiritVisualHostWitness witness,
    void *context
) {
    ++spirit_visual_install_count;
    if (!SudekiMpLanArenaSpiritVisualHostImageMatches(game_module)) {
        spirit_visual_installed = spirit_visual_install_leaves_lease;
        SetLastError(ERROR_BAD_FORMAT);
        return FALSE;
    }
    spirit_visual_installed = TRUE;
    spirit_visual_witness = witness;
    spirit_visual_witness_context = context;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVisualHostReset(void) {
    ++spirit_visual_reset_count;
    if (!spirit_visual_installed) return TRUE;
    record_spirit_observer_teardown('V');
    if (!spirit_visual_reset_result) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    spirit_visual_installed = FALSE;
    spirit_visual_witness = NULL;
    spirit_visual_witness_context = NULL;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVisualHostCapture(
    uint64_t session, uint16_t skill, uint32_t tick,
    SudekiMpLanArenaSnapshot *snapshot
) {
    (void)session;
    (void)skill;
    (void)tick;
    ++spirit_visual_capture_count;
    if (snapshot == NULL) return FALSE;
    snapshot->spirit_vfx_observed = spirit_visual_capture_observed ? 1u : 0u;
    snapshot->spirit_vfx_count = 0u;
    memset(snapshot->spirit_vfx, 0, sizeof(snapshot->spirit_vfx));
    if (!spirit_visual_capture_result) {
        /* Inject a partially written failed sample: runtime must sanitize it
         * before the optional domain reaches canonical snapshot validation. */
        snapshot->spirit_vfx_observed = 1u;
        snapshot->spirit_vfx_count = 9u;
        snapshot->spirit_vfx[0].instance_sequence = 99u;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    return TRUE;
}

size_t SudekiMpLanArenaSpiritAudioTraceSnapshot(
    SudekiMpLanArenaSpiritAudioEvent *events,
    size_t capacity,
    uint32_t *dropped_count
) {
    size_t copied = spirit_audio_event_count < capacity ?
        spirit_audio_event_count : capacity;
    if (events != NULL && copied != 0u) {
        memcpy(events,
            spirit_audio_events + spirit_audio_event_count - copied,
            copied * sizeof(*events));
    }
    if (dropped_count != NULL) *dropped_count = 0u;
    return copied;
}

BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module) {
    (void)game_module;
    if (!host_input_install_result) SetLastError(ERROR_INVALID_DATA);
    return host_input_install_result;
}

BOOL SudekiMpUninstallLanArenaHostInput(void) {
    ++host_input_uninstall_count;
    host_native_skill_start_observer = NULL;
    return TRUE;
}

void SudekiMpLanArenaHostInputSetNativeSkillStartObserver(
    SudekiMpLanArenaHostNativeSkillStartObserver observer
) {
    host_native_skill_start_observer = observer;
}

BOOL SudekiMpLanArenaHostInputTakeSkillSlot(unsigned int *slot) {
    (void)slot;
    return FALSE;
}

SudekiMpSkillActivationResult SudekiMpReplayHostApprovedCharacterSkillSlot(
    void *character,
    int slot
) {
    SudekiMpSkillActivationResult result;
    (void)character;
    (void)slot;
    memset(&result, 0, sizeof(result));
    return result;
}

const char *SudekiMpSkillActivationStatusName(
    SudekiMpSkillActivationStatus status
) {
    (void)status;
    return "inactive";
}

BOOL SudekiMpLanArenaHostInputTakeSpiritVariant(unsigned int *variant) {
    if (!host_spirit_request_available || variant == NULL) return FALSE;
    ++host_spirit_request_take_count;
    host_spirit_request_available = FALSE;
    *variant = host_spirit_request_variant;
    return TRUE;
}

BOOL SudekiMpLanArenaHostInputDiscardSpiritRequests(void) {
    BOOL discarded = host_spirit_request_available;
    if (discarded) ++host_spirit_request_discard_count;
    host_spirit_request_available = FALSE;
    return discarded;
}

BOOL SudekiMpLanArenaHostInputTalControllerLeaseExact(void *tal) {
    return host_tal_controller_lease_exact &&
        tal == cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL];
}

void SudekiMpLanArenaHostInputServiceCombatToggle(void) {}

void SudekiMpLanArenaHostInputNotifyNativeActionObserved(void) {}

BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void) {
    return FALSE;
}

BOOL SudekiMpLanArenaPausePanelActive(void) {
    return cleanroom_pause_active;
}

BOOL SudekiMpCleanroomMenuActive(void) {
    return cleanroom_menu_active;
}

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module) {
    (void)game_module;
    if (!client_input_install_result) SetLastError(ERROR_INVALID_DATA);
    return client_input_install_result;
}

BOOL SudekiMpUninstallLanArenaClientInput(void) {
    ++client_input_uninstall_count;
    return TRUE;
}

void SudekiMpLanArenaClientInputService(void) {}

BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module) {
    (void)game_module;
    ++replica_initialize_count;
    if (!client_replica_initialize_result) SetLastError(ERROR_NOT_READY);
    return client_replica_initialize_result;
}

void SudekiMpLanArenaClientReplicaDiscardSnapshots(void) {}

BOOL SudekiMpResetLanArenaClientReplica(void) {
    ++replica_reset_count;
    if (!client_replica_reset_result) SetLastError(ERROR_BUSY);
    return client_replica_reset_result;
}

void SudekiMpLanArenaClientReplicaSetRemoteTalLease(
    void *actor,
    uint32_t actor_generation
) {
    ++client_tal_lease_publish_count;
    client_tal_published_actor = actor;
    client_tal_published_generation = actor_generation;
}

BOOL SudekiMpLanArenaClientReplicaRemoteTalReleaseReady(void) {
    ++client_tal_release_ready_count;
    if (!client_tal_release_ready_result) SetLastError(ERROR_BUSY);
    return client_tal_release_ready_result;
}

BOOL SudekiMpLanArenaClientReplicaApplyLatest(void) {
    record_callback_event('A');
    return replica_apply_result;
}

BOOL SudekiMpLanArenaClientReplicaRefreshOwnerViewAfterRender(void) {
    record_callback_event('V');
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaReassertOwnerViewAfterRemoteMutation(void) {
    record_callback_event('C');
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaReassertPresentation(void) {
    record_callback_event('S');
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaPublishVisibleTransforms(void) {
    record_callback_event('P');
    if (!visible_publish_result) SetLastError(ERROR_INVALID_STATE);
    return visible_publish_result;
}

BOOL SudekiMpLanArenaClientReplicaServiceSpiritVfx(void) {
    record_callback_event('X');
    return TRUE;
}

void SudekiMpLanArenaClientReplicaRefreshDiagnostics(void) {}

BOOL SudekiMpLanArenaClientReplicaGetDiagnostics(
    SudekiMpLanArenaReplicaDiagnostics *diagnostics
) {
    (void)diagnostics;
    return FALSE;
}

BOOL SudekiMpControlUpdateObserverGateEnable(
    SudekiMpControlUpdateObserverGate *gate
) {
    if (gate != NULL) gate->enabled = observer_gate_enable_result ? 1 : 0;
    if (!observer_gate_enable_result) SetLastError(ERROR_INVALID_STATE);
    return observer_gate_enable_result;
}

BOOL SudekiMpControlUpdateObserverGateTryEnter(
    SudekiMpControlUpdateObserverGate *gate
) {
    (void)gate;
    return TRUE;
}

void SudekiMpControlUpdateObserverGateLeave(
    SudekiMpControlUpdateObserverGate *gate
) {
    (void)gate;
}

void SudekiMpControlUpdateObserverGateDisable(
    SudekiMpControlUpdateObserverGate *gate
) {
    if (gate != NULL) gate->enabled = 0;
}

void SudekiMpControlUpdateObserverGateDrain(
    SudekiMpControlUpdateObserverGate *gate
) {
    (void)gate;
}

BOOL SudekiMpControlSeparationRegisterUpdateObserver(
    const void *owner,
    SudekiMpControlUpdateObserver observer
) {
    (void)owner;
    (void)observer;
    if (!observer_register_result) SetLastError(ERROR_INVALID_STATE);
    return observer_register_result;
}

BOOL SudekiMpControlSeparationUnregisterUpdateObserver(const void *owner) {
    (void)owner;
    return TRUE;
}

BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)witness;
    return TRUE;
}

BOOL SudekiMpControlSeparationSetManualToggleEnabled(BOOL enabled) {
    (void)enabled;
    return TRUE;
}

BOOL SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(BOOL enabled) {
    (void)enabled;
    return TRUE;
}

BOOL SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(BOOL enabled) {
    (void)enabled;
    return TRUE;
}

BOOL SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
    BOOL enabled
) {
    player_two_skill_isolation_enabled = enabled != FALSE;
    ++player_two_skill_isolation_call_count;
    return TRUE;
}

BOOL SudekiMpControlSeparationReleasePlayerTwoNow(void) {
    ++release_player_two_count;
    record_client_tal_teardown_event('L');
    return player_two_release_result;
}

BOOL SudekiMpControlSeparationForceStopCharacter(void *character) {
    (void)character;
    return TRUE;
}

BOOL SudekiMpControlSeparationPlayerTwoActive(void) {
    return player_two_active;
}

void *SudekiMpControlSeparationPlayerTwoCharacter(void) {
    return player_two_character;
}

BOOL SudekiMpControlSeparationRequestPlayerTwoCharacter(void *character) {
    (void)character;
    ++request_player_two_count;
    return player_two_request_result;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
    float world_direction_x,
    float world_direction_z,
    float aim_direction_x,
    float aim_direction_z,
    BOOL aim_direction_valid,
    BOOL weak_attack_active,
    float frame_delta_seconds
) {
    (void)world_direction_x;
    (void)world_direction_z;
    (void)aim_direction_x;
    (void)aim_direction_z;
    (void)aim_direction_valid;
    (void)weak_attack_active;
    (void)frame_delta_seconds;
    return TRUE;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoRangedFire(void) {
    return TRUE;
}

BOOL SudekiMpControlSeparationLanArenaPlayerTwoRangedReady(
    BOOL *ready,
    uint16_t *authored_delay_half
) {
    if (ready == NULL || authored_delay_half == NULL) return FALSE;
    *ready = TRUE;
    *authored_delay_half = 0x4000u;
    return TRUE;
}

BOOL SudekiMpLanArenaRemoteInputFresh(
    uint32_t last_input_at_ms,
    uint32_t now_ms,
    uint32_t maximum_age_ms
) {
    (void)last_input_at_ms;
    (void)now_ms;
    (void)maximum_age_ms;
    return FALSE;
}

int SudekiMpLanArenaParseEndpoint(
    const char *text,
    uint16_t default_port,
    char *ipv4,
    size_t ipv4_capacity,
    uint16_t *port
) {
    static const char loopback[] = "127.0.0.1";
    if (text == NULL || ipv4 == NULL || port == NULL ||
        ipv4_capacity < sizeof(loopback)) return 0;
    memcpy(ipv4, loopback, sizeof(loopback));
    *port = default_port;
    return 1;
}

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor) {
    (void)actor;
    return "actor";
}

void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    if ((unsigned int)actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) return NULL;
    return cleanroom_actor_entities[actor];
}

BOOL SudekiMpObserveCharacterSkill(
    void *character,
    SudekiMpCharacterSkillState *state
) {
    if (!character_skill_observe_result || character == NULL || state == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    *state = character_skill_observation;
    return TRUE;
}

void SudekiMpCleanroomMenuRender(void) {
}

BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor) {
    if ((unsigned int)actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) return FALSE;
    return actor_present_results[actor];
}

BOOL SudekiMpCleanroomEngineCombatMode(BOOL *enabled) {
    if (enabled == NULL) return FALSE;
    if (host_spirit_reproof_probe_teardown) {
        host_spirit_reproof_probe_depth = InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0);
        SetLastError(ERROR_SUCCESS);
        host_spirit_reproof_probe_saw_busy =
            !host_native_tasks_drained() && GetLastError() == ERROR_BUSY;
    }
    *enabled = cleanroom_combat_enabled;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *state) {
    if (!spirit_presentation_state_result || state == NULL) return FALSE;
    *state = spirit_presentation_state;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) {
    return ranged_combat_prime_pending;
}

BOOL SudekiMpCleanroomEnginePrimeRangedCombat(void) {
    ++ranged_combat_prime_call_count;
    if (!ranged_combat_prime_result) return FALSE;
    ranged_combat_prime_pending = TRUE;
    return TRUE;
}

void SudekiMpCleanroomEngineMaintainResources(void) {
    ++maintain_resources_call_count;
}

BOOL SudekiMpDescribeCharacterSpiritOptions(
    void *character,
    SudekiMpSpiritQuickOptionList *options
) {
    unsigned int variant;
    if (!host_spirit_options_result || character == NULL || options == NULL) {
        return FALSE;
    }
    if (host_spirit_describe_probe_teardown) {
        host_spirit_describe_probe_depth = InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0);
        SetLastError(ERROR_SUCCESS);
        host_spirit_describe_probe_saw_busy =
            !host_native_tasks_drained() && GetLastError() == ERROR_BUSY;
    }
    memset(options, 0, sizeof(*options));
    options->resource_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    options->option_count = 2u;
    for (variant = 1u; variant <= 2u; ++variant) {
        options->options[variant - 1u].variant = variant;
        options->options[variant - 1u].strike_id = (int)variant - 1;
        options->options[variant - 1u].validation_result =
            host_spirit_option_available ? 0 : 7;
        options->options[variant - 1u].available =
            host_spirit_option_available ? 1u : 0u;
    }
    return TRUE;
}

SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpirit(
    void *character,
    unsigned int variant
) {
    SudekiMpSpiritActivationResult result;
    memset(&result, 0, sizeof(result));
    ++host_spirit_activation_call_count;
    host_spirit_last_activated_variant = variant;
    if (host_spirit_activation_probe_teardown) {
        host_spirit_activation_probe_depth = InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0);
        SetLastError(ERROR_SUCCESS);
        host_spirit_activation_probe_saw_busy =
            !host_native_tasks_drained() && GetLastError() == ERROR_BUSY;
    }
    result.strike_id = (int)variant - 1;
    if (character != cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL]) {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT;
    } else if (host_spirit_activation_started) {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_STARTED;
        result.activation_result = 1;
    } else {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED;
    }
    return result;
}

const char *SudekiMpSpiritActivationStatusName(
    SudekiMpSpiritActivationStatus status
) {
    return status == SUDEKIMP_SPIRIT_ACTIVATION_STARTED ?
        "started" : "rejected";
}

BOOL SudekiMpCleanroomEngineActorPosition(
    SudekiMpCleanroomActor actor,
    float position[3]
) {
    (void)actor;
    if (!actor_position_result || position == NULL) return FALSE;
    position[0] = 1.0f;
    position[1] = 2.0f;
    position[2] = 3.0f;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorFacing(
    SudekiMpCleanroomActor actor,
    float facing[2]
) {
    (void)actor;
    if (!actor_facing_result || facing == NULL) return FALSE;
    facing[0] = 0.0f;
    facing[1] = 1.0f;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorResources(
    SudekiMpCleanroomActor actor,
    float *hit_points,
    float *skill_points
) {
    (void)actor;
    if (!actor_resources_result || hit_points == NULL || skill_points == NULL) {
        return FALSE;
    }
    *hit_points = 100.0f;
    *skill_points = 50.0f;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorPresentation(
    SudekiMpCleanroomActor actor,
    SudekiMpCleanroomActorPresentation *presentation
) {
    (void)actor;
    (void)presentation;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineSpawnActor(
    SudekiMpCleanroomActor actor,
    const float position[3]
) {
    (void)actor;
    (void)position;
    ++spawn_actor_count;
    return spawn_actor_result;
}

BOOL SudekiMpCleanroomEngineInitializePartyActor(
    SudekiMpCleanroomActor actor
) {
    (void)actor;
    ++initialize_party_actor_count;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineRemoveActor(SudekiMpCleanroomActor actor) {
    ++remove_actor_count;
    record_client_tal_teardown_event('R');
    if (remove_actor_result && remove_actor_clears_presence &&
        (unsigned int)actor < SUDEKIMP_CLEANROOM_ACTOR_COUNT) {
        actor_present_results[actor] = FALSE;
        cleanroom_actor_entities[actor] = NULL;
    }
    return remove_actor_result;
}

BOOL SudekiMpCleanroomEngineDummyPresent(void) {
    return FALSE;
}

BOOL SudekiMpCleanroomEngineDummySnapshot(
    float position[3],
    float *hit_points
) {
    (void)position;
    (void)hit_points;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineSpawnDummy(const float position[3]) {
    (void)position;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineRemoveDummy(void) {
    return TRUE;
}

void SudekiMpLogWrite(const char *message) {
    (void)message;
}

void SudekiMpLogFormat(const char *format, ...) {
    (void)format;
    ++log_format_call_count;
}
