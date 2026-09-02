#include <windows.h>

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
    check(strcmp(callback_events, "RP") == 0,
        "pre-world callback orders native render before final publish");

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

BOOL SudekiMpLanArenaHostInputTakeTalWeakAttack(void) {
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
    BOOL weak_attack_edge
) {
    (void)world_direction_x;
    (void)world_direction_z;
    (void)weak_attack_edge;
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
