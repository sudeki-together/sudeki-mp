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
    TEST_IMAGE_SIZE = 0x00290000u,
    TEST_RVA_RENDER_START = 0x001dce30u,
    TEST_RVA_RENDER_START_CALL = 0x0028d443u,
    TEST_RVA_RENDER_PRE_WORLD_CALL = 0x0028d539u,
    TEST_RVA_FRAME_END = 0x001dd540u,
    TEST_RVA_FRAME_END_CALL = 0x0028d58cu
};

static int failures;
static BOOL session_start_result = TRUE;
static BOOL campaign_guard_install_result = TRUE;
static BOOL collision_debug_install_result = TRUE;
static BOOL host_input_install_result = TRUE;
static BOOL client_input_install_result = TRUE;
static BOOL client_replica_initialize_result = TRUE;
static BOOL observer_gate_enable_result = TRUE;
static BOOL observer_register_result = TRUE;
static BOOL replica_apply_result = TRUE;
static BOOL visible_publish_result = TRUE;
static unsigned int session_stop_count;
static unsigned int client_input_uninstall_count;
static unsigned int host_input_uninstall_count;
static unsigned int replica_reset_count;
static unsigned int campaign_guard_uninstall_count;
static unsigned int collision_debug_uninstall_count;
static char callback_events[32];
static size_t callback_event_count;

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

static void reset_stub_policy(void) {
    session_start_result = TRUE;
    campaign_guard_install_result = TRUE;
    collision_debug_install_result = TRUE;
    host_input_install_result = TRUE;
    client_input_install_result = TRUE;
    client_replica_initialize_result = TRUE;
    observer_gate_enable_result = TRUE;
    observer_register_result = TRUE;
    replica_apply_result = TRUE;
    visible_publish_result = TRUE;
}

static void reset_stub_counts(void) {
    session_stop_count = 0u;
    client_input_uninstall_count = 0u;
    host_input_uninstall_count = 0u;
    replica_reset_count = 0u;
    campaign_guard_uninstall_count = 0u;
    collision_debug_uninstall_count = 0u;
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
    write_call(
        image, TEST_RVA_RENDER_START_CALL, TEST_RVA_RENDER_START);
    write_call(
        image, TEST_RVA_RENDER_PRE_WORLD_CALL, TEST_RVA_RENDER_START);
    write_call(image, TEST_RVA_FRAME_END_CALL, TEST_RVA_FRAME_END);
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
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host runtime installs over exact native calls");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "host leaves first RenderStart call native");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START,
        "host leaves pre-world RenderStart call native");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "host redirects frame-end call only");
    SudekiMpUninstallLanArenaRuntime();
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "host uninstall restores frame-end call");
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
    check(strcmp(callback_events, "APRP") == 0,
        "first render callback orders apply/publish/native/publish");

    callback_event_count = 0u;
    callback_events[0] = '\0';
    lan_arena_render_pre_world_entry();
    check(strcmp(callback_events, "RSP") == 0,
        "pre-world callback orders native render, semantic reassert, publish");

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
    verify_second_render_mismatch_rollback(image);
    verify_downstream_failure_rollback(image);
    verify_callback_order();
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
    if (status != NULL) memset(status, 0, sizeof(*status));
    return FALSE;
}

BOOL SudekiMpLanArenaSessionTakeRemoteInput(SudekiMpLanArenaInput *input) {
    (void)input;
    return FALSE;
}

BOOL SudekiMpLanArenaSessionSendSnapshot(
    const SudekiMpLanArenaSnapshot *snapshot
) {
    (void)snapshot;
    return FALSE;
}

BOOL SudekiMpInstallLanArenaCampaignGuard(HMODULE game_module) {
    (void)game_module;
    if (!campaign_guard_install_result) SetLastError(ERROR_INVALID_DATA);
    return campaign_guard_install_result;
}

void SudekiMpUninstallLanArenaCampaignGuard(void) {
    ++campaign_guard_uninstall_count;
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

BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module) {
    (void)game_module;
    if (!host_input_install_result) SetLastError(ERROR_INVALID_DATA);
    return host_input_install_result;
}

void SudekiMpUninstallLanArenaHostInput(void) {
    ++host_input_uninstall_count;
}

void SudekiMpLanArenaHostInputServiceCombatToggle(void) {}

void SudekiMpLanArenaHostInputNotifyNativeActionObserved(void) {}

BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void) {
    return FALSE;
}

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module) {
    (void)game_module;
    if (!client_input_install_result) SetLastError(ERROR_INVALID_DATA);
    return client_input_install_result;
}

void SudekiMpUninstallLanArenaClientInput(void) {
    ++client_input_uninstall_count;
}

void SudekiMpLanArenaClientInputService(void) {}

BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module) {
    (void)game_module;
    if (!client_replica_initialize_result) SetLastError(ERROR_NOT_READY);
    return client_replica_initialize_result;
}

void SudekiMpLanArenaClientReplicaDiscardSnapshots(void) {}

void SudekiMpResetLanArenaClientReplica(void) {
    ++replica_reset_count;
}

BOOL SudekiMpLanArenaClientReplicaApplyLatest(void) {
    record_callback_event('A');
    return replica_apply_result;
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

BOOL SudekiMpControlSeparationReleasePlayerTwoNow(void) {
    return TRUE;
}

BOOL SudekiMpControlSeparationForceStopCharacter(void *character) {
    (void)character;
    return TRUE;
}

BOOL SudekiMpControlSeparationPlayerTwoActive(void) {
    return FALSE;
}

void *SudekiMpControlSeparationPlayerTwoCharacter(void) {
    return NULL;
}

BOOL SudekiMpControlSeparationRequestPlayerTwoCharacter(void *character) {
    (void)character;
    return TRUE;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
    float world_direction_x,
    float world_direction_z,
    float aim_direction_x,
    float aim_direction_z,
    BOOL aim_direction_valid,
    BOOL weak_attack_active
) {
    (void)world_direction_x;
    (void)world_direction_z;
    (void)aim_direction_x;
    (void)aim_direction_z;
    (void)aim_direction_valid;
    (void)weak_attack_active;
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
    (void)text;
    (void)default_port;
    (void)ipv4;
    (void)ipv4_capacity;
    (void)port;
    return 0;
}

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor) {
    (void)actor;
    return "actor";
}

void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    (void)actor;
    return NULL;
}

BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor) {
    (void)actor;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineCombatMode(BOOL *enabled) {
    if (enabled != NULL) *enabled = FALSE;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorPosition(
    SudekiMpCleanroomActor actor,
    float position[3]
) {
    (void)actor;
    (void)position;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineActorFacing(
    SudekiMpCleanroomActor actor,
    float facing[2]
) {
    (void)actor;
    (void)facing;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineActorResources(
    SudekiMpCleanroomActor actor,
    float *hit_points,
    float *skill_points
) {
    (void)actor;
    (void)hit_points;
    (void)skill_points;
    return FALSE;
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
    return FALSE;
}

BOOL SudekiMpCleanroomEngineInitializePartyActor(
    SudekiMpCleanroomActor actor
) {
    (void)actor;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineRemoveActor(SudekiMpCleanroomActor actor) {
    (void)actor;
    return TRUE;
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
}
