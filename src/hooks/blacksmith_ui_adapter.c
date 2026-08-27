#include "hooks/blacksmith_ui_adapter.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "engine/player_statehood.h"
#include "hooks/blacksmith_read_adapter.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/split_screen_render.h"
#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"

#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Blacksmith UI adapter requires the 32-bit Windows target"
#endif

/* Both exports are C++ bool functions. The exact native implementations only
 * define AL, so widening either callback to Win32 BOOL would consume dirty
 * upper EAX bits on the fallback path. */
typedef uint8_t (__attribute__((cdecl)) *BlacksmithStartFunction)(void);
typedef uint8_t (__attribute__((cdecl)) *BlacksmithActiveFunction)(void);

enum {
    RVA_BLACKSMITH_START = 0x00092c40u,
    RVA_BLACKSMITH_ACTIVE = 0x00092c60u,
    RVA_WORLD_SCENE_GLOBAL = 0x00408d1cu,
    RVA_BLACKSMITH_LAYER_GLOBAL = 0x003c2f74u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    RVA_INVENTORY_VTABLE = 0x002ca378u,
    INVENTORY_MONEY_OFFSET = 0x134u,
    BLACKSMITH_SIGNATURE_IMAGE_SIZE = 0x0030d41cu,
    BLACKSMITH_READ_CAPTURE_INTERVAL_MS = 100u,
    P1_INPUT_COUNT = 8u
};

static uint8_t *game_base;
static BOOL adapter_enabled;
static BOOL intercepted_lifecycle;
static BOOL bridge_suppression_owned;
static BOOL p1_key_was_down[P1_INPUT_COUNT];
static uint32_t p2_buttons_were_down;
static uint32_t last_read_capture_ms;
static uintptr_t intercepted_world_scene;
static uintptr_t intercepted_world_descriptor;
static uint32_t intercepted_world_descriptor_state;
static uintptr_t intercepted_actor_leases[
    SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
static SudekiMpBlacksmithUiSession ui_session;
static SudekiMpInlineHook blacksmith_start_hook;
static SudekiMpInlineHook blacksmith_active_hook;
static BlacksmithStartFunction original_blacksmith_start;
static BlacksmithActiveFunction original_blacksmith_active;
#if defined(SUDEKIMP_BLACKSMITH_UI_ADAPTER_TESTING)
static BOOL fail_second_hook_for_test;
#endif

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL owns_foreground(void) {
    HWND foreground = GetForegroundWindow();
    DWORD process_id = 0u;

    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &process_id);
    }
    return process_id == GetCurrentProcessId();
}

static BOOL read_shared_money(uint32_t *money) {
    uint8_t *inventory;

    if (money == NULL || game_base == NULL || !readable_memory(
            game_base + RVA_INVENTORY_GLOBAL, sizeof(inventory))) {
        return FALSE;
    }
    inventory = *(uint8_t **)(game_base + RVA_INVENTORY_GLOBAL);
    if (!readable_memory(
            inventory, INVENTORY_MONEY_OFFSET + sizeof(*money)) ||
        *(void **)inventory != game_base + RVA_INVENTORY_VTABLE) {
        return FALSE;
    }
    *money = *(uint32_t *)(inventory + INVENTORY_MONEY_OFFSET);
    return TRUE;
}

static BOOL read_world_lease(
    uintptr_t *world_scene,
    uintptr_t *world_descriptor,
    uint32_t *descriptor_state
) {
    uint8_t *scene;
    uint8_t *descriptor;
    uint32_t state;

    if (world_scene == NULL || world_descriptor == NULL ||
        descriptor_state == NULL || game_base == NULL || !readable_memory(
            game_base + RVA_WORLD_SCENE_GLOBAL, sizeof(scene))) {
        return FALSE;
    }
    scene = *(uint8_t **)(game_base + RVA_WORLD_SCENE_GLOBAL);
    if (!readable_memory(scene, 0x39bu)) {
        return FALSE;
    }
    descriptor = *(uint8_t **)(scene + 0x0cu);
    if (!readable_memory(descriptor, 0x38u) ||
        *(void **)(scene + 0x14u) != NULL ||
        *(uint8_t *)(scene + 0x399u) == 0u ||
        *(uint8_t *)(scene + 0x39au) == 0u) {
        return FALSE;
    }
    state = *(const uint32_t *)(descriptor + 0x34u);
    if (state != 3u && state != 4u) {
        return FALSE;
    }
    *world_scene = (uintptr_t)scene;
    *world_descriptor = (uintptr_t)descriptor;
    *descriptor_state = state;
    return TRUE;
}

static void finish_intercepted_lifecycle(const char *reason) {
    if (!intercepted_lifecycle) {
        return;
    }
    intercepted_lifecycle = FALSE;
    if (bridge_suppression_owned) {
        SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
        bridge_suppression_owned = FALSE;
    }
    ZeroMemory(intercepted_actor_leases,
        sizeof(intercepted_actor_leases));
    intercepted_world_scene = 0u;
    intercepted_world_descriptor = 0u;
    intercepted_world_descriptor_state = 0u;
    last_read_capture_ms = 0u;
    SudekiMpLogFormat(
        "blacksmith_ui event=lifecycle phase=end reason=%s "
        "native_modal_queued=0 native_commit=disabled "
        "script_active_contract=released\r\n",
        reason == NULL ? "closed" : reason);
}

static void close_open_seats(void) {
    uint32_t player_index;

    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        (void)SudekiMpBlacksmithUiSessionApplyInput(
            &ui_session,
            player_index,
            SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE);
    }
}

static BOOL p1_rising(unsigned int index, int virtual_key) {
    BOOL down;
    BOOL rising;

    if (index >= P1_INPUT_COUNT) {
        return FALSE;
    }
    down = (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
    rising = down && !p1_key_was_down[index];
    p1_key_was_down[index] = down;
    return rising;
}

static void arm_input_edges(void) {
    static const int keys[P1_INPUT_COUNT] = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
        VK_PRIOR, VK_NEXT, VK_RETURN, VK_ESCAPE
    };
    SudekiMpInputBridgeState bridge;
    unsigned int index;

    for (index = 0u; index < P1_INPUT_COUNT; ++index) {
        p1_key_was_down[index] =
            (GetAsyncKeyState(keys[index]) & 0x8000) != 0;
    }
    p2_buttons_were_down = SudekiMpInputBridgePollRaw(&bridge) ?
        bridge.buttons : 0u;
}

static BOOL runtime_prerequisites(
    uint32_t character_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    uint32_t actor_generations[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    SudekiMpBlacksmithReadSeatRequest
        requests[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    SudekiMpBlacksmithReadSnapshot *read_model,
    uint32_t *money,
    uintptr_t *world_scene,
    uintptr_t *world_descriptor,
    uint32_t *world_descriptor_state,
    const char **reason
) {
    SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    unsigned int player_one_type;
    unsigned int player_two_type;
    uint32_t player_index;

    if (reason != NULL) {
        *reason = "ready";
    }
    if (!adapter_enabled || game_base == NULL) {
        if (reason != NULL) *reason = "adapter_disabled";
        return FALSE;
    }
    if (!SudekiMpSplitScreenRuntimeEnabled() ||
        !SudekiMpSplitScreenRolesLocked() ||
        !SudekiMpSplitScreenRosterParticipationRequested() ||
        !SudekiMpControlSeparationPlayerTwoActive()) {
        if (reason != NULL) *reason = "split_roster_not_active";
        return FALSE;
    }
    if (!SudekiMpControlSeparationInputReady() ||
        SudekiMpInputBridgeGameplaySuppressed()) {
        if (reason != NULL) *reason = "input_not_exclusively_available";
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineWorldReady()) {
        if (reason != NULL) *reason = "world_not_ready";
        return FALSE;
    }
    if (!SudekiMpSplitScreenGetRosterTypes(
            &player_one_type, &player_two_type) ||
        player_one_type == 0u || player_two_type == 0u ||
        player_one_type == player_two_type) {
        if (reason != NULL) *reason = "stable_roster_ids_unavailable";
        return FALSE;
    }
    character_ids[0] = player_one_type;
    character_ids[1] = player_two_type;
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        const SudekiMpPlayerLease *lease =
            &statehood->players[player_index];
        if (!lease->human_present || lease->actor == 0u ||
            lease->actor_generation == 0u) {
            if (reason != NULL) *reason = "live_actor_lease_unavailable";
            return FALSE;
        }
        if (!SudekiMpSplitScreenRosterActorIdentityMatches(
                player_index, (const void *)lease->actor,
                character_ids[player_index])) {
            if (reason != NULL) *reason =
                "active_group_actor_identity_unproven";
            return FALSE;
        }
        actor_generations[player_index] = lease->actor_generation;
        requests[player_index].actor = lease->actor;
        requests[player_index].character_id = character_ids[player_index];
        requests[player_index].actor_generation = lease->actor_generation;
        requests[player_index].active_group_proven = 1;
    }
    if (!read_shared_money(money)) {
        if (reason != NULL) *reason = "shared_money_unavailable";
        return FALSE;
    }
    if (!read_world_lease(world_scene, world_descriptor,
            world_descriptor_state)) {
        if (reason != NULL) *reason = "world_scene_lease_unavailable";
        return FALSE;
    }
    if (!SudekiMpBlacksmithReadAdapterCapture(
            requests, SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT, read_model)) {
        if (reason != NULL) *reason = "native_read_model_unavailable";
        return FALSE;
    }
    return TRUE;
}

static BOOL begin_intercepted_lifecycle(void) {
    uint32_t character_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t actor_generations[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    SudekiMpBlacksmithReadSeatRequest
        requests[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    SudekiMpBlacksmithReadSnapshot read_model;
    uint32_t money;
    uintptr_t world_scene;
    uintptr_t world_descriptor;
    uint32_t world_descriptor_state;
    const char *reason;

    if (intercepted_lifecycle) {
        return TRUE;
    }
    if (!runtime_prerequisites(
            character_ids, actor_generations, requests, &read_model,
            &money, &world_scene, &world_descriptor,
            &world_descriptor_state, &reason)) {
        SudekiMpLogFormat(
            "blacksmith_ui event=start phase=fallback reason=%s "
            "policy=call_native_UIBlackSmithStart_unchanged\r\n",
            reason);
        return FALSE;
    }
    if (!SudekiMpBlacksmithUiSessionBegin(
            &ui_session,
            character_ids,
            actor_generations,
            money,
            &read_model,
            GetTickCount())) {
        SudekiMpLogWrite(
            "blacksmith_ui event=start phase=fallback "
            "reason=shadow_session_rejected "
            "policy=call_native_UIBlackSmithStart_unchanged\r\n");
        return FALSE;
    }
    intercepted_actor_leases[0] = requests[0].actor;
    intercepted_actor_leases[1] = requests[1].actor;
    intercepted_world_scene = world_scene;
    intercepted_world_descriptor = world_descriptor;
    intercepted_world_descriptor_state = world_descriptor_state;
    last_read_capture_ms = GetTickCount();
    SudekiMpInputBridgeSetGameplaySuppressed(TRUE);
    if (!SudekiMpInputBridgeGameplaySuppressed()) {
        close_open_seats();
        ZeroMemory(intercepted_actor_leases,
            sizeof(intercepted_actor_leases));
        intercepted_world_scene = 0u;
        intercepted_world_descriptor = 0u;
        intercepted_world_descriptor_state = 0u;
        last_read_capture_ms = 0u;
        SudekiMpLogWrite(
            "blacksmith_ui event=start phase=fallback "
            "reason=gameplay_input_freeze_failed "
            "policy=call_native_UIBlackSmithStart_unchanged\r\n");
        return FALSE;
    }
    bridge_suppression_owned = TRUE;
    intercepted_lifecycle = TRUE;
    arm_input_edges();
    SudekiMpLogFormat(
        "blacksmith_ui event=lifecycle phase=begin serial=%lu "
        "p1_character_id=0x%02lx p1_actor_generation=%lu "
        "p2_character_id=0x%02lx p2_actor_generation=%lu "
        "shared_money=%lu presentation=two_viewport_panels "
        "native_modal_queued=0 native_commit=disabled "
        "merchant_target=unresolved_script_export_has_no_argument "
        "input_policy=freeze_world_controls_route_keyboard_and_raw_bridge\r\n",
        (unsigned long)ui_session.presentation_serial,
        (unsigned long)character_ids[0],
        (unsigned long)actor_generations[0],
        (unsigned long)character_ids[1],
        (unsigned long)actor_generations[1],
        (unsigned long)money);
    return TRUE;
}

static uint8_t __attribute__((cdecl)) blacksmith_start_dispatch(void) {
    if (begin_intercepted_lifecycle()) {
        return 1u;
    }
    return original_blacksmith_start != NULL ?
        original_blacksmith_start() : 0u;
}

static uint8_t __attribute__((cdecl)) blacksmith_active_dispatch(void) {
    if (intercepted_lifecycle) {
        SudekiMpBlacksmithUiAdapterService();
        if (intercepted_lifecycle) {
            return 1u;
        }
        /* This invocation belongs to the intercepted lifecycle. No native
         * modal was queued, so report its clean completion directly. */
        return 0u;
    }
    return original_blacksmith_active != NULL ?
        original_blacksmith_active() : 0u;
}

static void apply_p1_input(void) {
    static const SudekiMpBlacksmithUiInput inputs[P1_INPUT_COUNT] = {
        SUDEKIMP_BLACKSMITH_UI_INPUT_UP,
        SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN,
        SUDEKIMP_BLACKSMITH_UI_INPUT_LEFT,
        SUDEKIMP_BLACKSMITH_UI_INPUT_RIGHT,
        SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIOUS_PAGE,
        SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE,
        SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIEW,
        SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE
    };
    static const int keys[P1_INPUT_COUNT] = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
        VK_PRIOR, VK_NEXT, VK_RETURN, VK_ESCAPE
    };
    unsigned int index;

    for (index = 0u; index < P1_INPUT_COUNT; ++index) {
        if (p1_rising(index, keys[index])) {
            (void)SudekiMpBlacksmithUiSessionApplyInput(
                &ui_session, 0u, inputs[index]);
        }
    }
}

static void apply_p2_button(
    uint32_t rising,
    uint32_t button,
    SudekiMpBlacksmithUiInput input
) {
    if ((rising & button) != 0u) {
        (void)SudekiMpBlacksmithUiSessionApplyInput(
            &ui_session, 1u, input);
    }
}

static void apply_p2_input(void) {
    SudekiMpInputBridgeState bridge;
    uint32_t rising;

    if (!SudekiMpInputBridgePollRaw(&bridge)) {
        (void)SudekiMpBlacksmithUiSessionDropPlayer(&ui_session, 1u);
        return;
    }
    rising = bridge.buttons & ~p2_buttons_were_down;
    p2_buttons_were_down = bridge.buttons;
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_DPAD_UP,
        SUDEKIMP_BLACKSMITH_UI_INPUT_UP);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN,
        SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT,
        SUDEKIMP_BLACKSMITH_UI_INPUT_LEFT);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT,
        SUDEKIMP_BLACKSMITH_UI_INPUT_RIGHT);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER,
        SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIOUS_PAGE);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER,
        SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_A,
        SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIEW);
    apply_p2_button(rising, SUDEKIMP_BRIDGE_BUTTON_B,
        SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE);
}

void SudekiMpBlacksmithUiAdapterService(void) {
    SudekiMpPlayerStatehood *statehood;
    SudekiMpBlacksmithReadSeatRequest
        requests[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    SudekiMpBlacksmithReadSnapshot read_model;
    uint32_t now_ms;
    uint32_t player_index;
    uint32_t money;
    uintptr_t world_scene;
    uintptr_t world_descriptor;
    uint32_t world_descriptor_state;

    if (!intercepted_lifecycle) {
        return;
    }
    now_ms = GetTickCount();
    if (!SudekiMpBlacksmithUiSessionService(&ui_session, now_ms)) {
        finish_intercepted_lifecycle("overlay_timeout_or_all_seats_closed");
        return;
    }
    if (!SudekiMpCleanroomEngineWorldReady() ||
        !SudekiMpSplitScreenRuntimeEnabled() ||
        !SudekiMpSplitScreenRolesLocked() ||
        !SudekiMpSplitScreenRosterParticipationRequested() ||
        !SudekiMpControlSeparationPlayerTwoActive() ||
        !SudekiMpControlSeparationInputReady() ||
        !bridge_suppression_owned ||
        !SudekiMpInputBridgeGameplaySuppressed()) {
        close_open_seats();
        finish_intercepted_lifecycle(
            "world_split_participation_or_input_lease_lost");
        return;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    if (!read_world_lease(&world_scene, &world_descriptor,
            &world_descriptor_state) ||
        world_scene != intercepted_world_scene ||
        world_descriptor != intercepted_world_descriptor ||
        world_descriptor_state != intercepted_world_descriptor_state) {
        close_open_seats();
        finish_intercepted_lifecycle("world_scene_lease_changed");
        return;
    }
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        const SudekiMpPlayerLease *lease =
            &statehood->players[player_index];
        if (!lease->human_present || lease->actor == 0u ||
            lease->actor != intercepted_actor_leases[player_index] ||
            lease->actor_generation !=
                ui_session.coordinator.actors[player_index].actor_generation ||
            !SudekiMpSplitScreenRosterActorIdentityMatches(
                player_index, (const void *)lease->actor,
                ui_session.coordinator.actors[player_index].character_id)) {
            close_open_seats();
            finish_intercepted_lifecycle("actor_lease_changed");
            return;
        }
        requests[player_index].actor = lease->actor;
        requests[player_index].character_id =
            ui_session.coordinator.actors[player_index].character_id;
        requests[player_index].actor_generation = lease->actor_generation;
        requests[player_index].active_group_proven = 1;
    }
    if (!read_shared_money(&money) ||
        !SudekiMpBlacksmithUiSessionObserveMoney(&ui_session, money)) {
        close_open_seats();
        finish_intercepted_lifecycle("shared_money_read_lost");
        return;
    }
    /* Active polling and menu rendering can both service this lifecycle in
     * one frame. Rebuild bounded native labels/stats at 10 Hz while lease,
     * world, input, and money checks remain per-call. No mutation can be
     * issued from this preview; a future commit lane must force a fresh
     * capture immediately before claiming a ticket. */
    if ((uint32_t)(now_ms - last_read_capture_ms) >=
            BLACKSMITH_READ_CAPTURE_INTERVAL_MS) {
        if (!SudekiMpBlacksmithReadAdapterCapture(
                requests, SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT,
                &read_model) ||
            !SudekiMpBlacksmithUiSessionObserveReadModel(
                &ui_session, &read_model)) {
            close_open_seats();
            finish_intercepted_lifecycle("native_read_model_lost");
            return;
        }
        last_read_capture_ms = now_ms;
    }
    if (owns_foreground()) {
        apply_p1_input();
        apply_p2_input();
    }
    if (!ui_session.active) {
        finish_intercepted_lifecycle("all_seats_closed");
    }
}

BOOL SudekiMpBlacksmithUiAdapterActive(void) {
    return intercepted_lifecycle && ui_session.active;
}

BOOL SudekiMpBlacksmithUiAdapterGetSnapshot(
    SudekiMpBlacksmithUiSnapshot *snapshot
) {
    return SudekiMpBlacksmithUiSessionGetSnapshot(
        &ui_session, snapshot) != 0;
}

void SudekiMpBlacksmithUiAdapterReportOverlay(BOOL visible) {
    if (!intercepted_lifecycle) {
        return;
    }
    if (!visible) {
        close_open_seats();
        finish_intercepted_lifecycle("overlay_draw_failed");
        return;
    }
    SudekiMpBlacksmithUiSessionReportOverlay(&ui_session, 1);
}

BOOL SudekiMpInstallBlacksmithUiAdapter(HMODULE game_module, BOOL enabled) {
    uint8_t *base = (uint8_t *)game_module;
    uint8_t start_entry[5] = {0xa1u, 0u, 0u, 0u, 0u};
    uint8_t active_entry[5] = {0xa1u, 0u, 0u, 0u, 0u};
    uint32_t relocated_operand;

    if (!enabled) {
        return TRUE;
    }
    if (base == NULL || game_base != NULL || (uintptr_t)base >
        (uintptr_t)(UINT32_MAX - RVA_WORLD_SCENE_GLOBAL)) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    relocated_operand = (uint32_t)(uintptr_t)(
        base + RVA_WORLD_SCENE_GLOBAL);
    memcpy(start_entry + 1u, &relocated_operand,
        sizeof(relocated_operand));
    relocated_operand = (uint32_t)(uintptr_t)(
        base + RVA_BLACKSMITH_LAYER_GLOBAL);
    memcpy(active_entry + 1u, &relocated_operand,
        sizeof(relocated_operand));
    if (!readable_memory(base + RVA_BLACKSMITH_START,
            7u) ||
        !readable_memory(base + RVA_BLACKSMITH_ACTIVE,
            7u) ||
        !readable_memory(base + 0x0030d414u, 8u) ||
        !SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
            base, BLACKSMITH_SIGNATURE_IMAGE_SIZE,
            (uintptr_t)base) ||
        memcmp(base + RVA_BLACKSMITH_START,
            start_entry, sizeof(start_entry)) != 0 ||
        memcmp(base + RVA_BLACKSMITH_ACTIVE,
            active_entry, sizeof(active_entry)) != 0) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    /* Process attach reaches this installer only after the exact executable
     * file SHA-256 and loaded PE identity gates in dllmain.c. The relocated
     * Start/Active check above is the local atomic hook gate and test seam;
     * it does not replace that whole-image prerequisite for native reads. */
    if (!SudekiMpBlacksmithReadAdapterInitialize(game_module)) {
        return FALSE;
    }
    game_base = base;
    adapter_enabled = TRUE;
    SudekiMpBlacksmithUiSessionInitialize(&ui_session);
    if (!SudekiMpInstallInlineHook(
            &blacksmith_start_hook,
            base + RVA_BLACKSMITH_START,
            start_entry,
            sizeof(start_entry),
            (const void *)blacksmith_start_dispatch)) {
        SudekiMpUninstallBlacksmithUiAdapter();
        return FALSE;
    }
    original_blacksmith_start =
        (BlacksmithStartFunction)blacksmith_start_hook.trampoline;
#if defined(SUDEKIMP_BLACKSMITH_UI_ADAPTER_TESTING)
    if (fail_second_hook_for_test) {
        fail_second_hook_for_test = FALSE;
        SudekiMpUninstallBlacksmithUiAdapter();
        SetLastError(ERROR_GEN_FAILURE);
        return FALSE;
    }
#endif
    if (!SudekiMpInstallInlineHook(
            &blacksmith_active_hook,
            base + RVA_BLACKSMITH_ACTIVE,
            active_entry,
            sizeof(active_entry),
            (const void *)blacksmith_active_dispatch)) {
        SudekiMpUninstallBlacksmithUiAdapter();
        return FALSE;
    }
    original_blacksmith_active =
        (BlacksmithActiveFunction)blacksmith_active_hook.trampoline;
    SudekiMpLogWrite(
        "blacksmith_ui install=success gate=explicit_default_false "
        "start_rva=0x00092c40 active_rva=0x00092c60 "
        "entry_policy=intercept_only_with_live_split_roster_actor_leases_money_and_input "
        "fallback=native_exports_unchanged mutation=disabled\r\n");
    return TRUE;
}

void SudekiMpUninstallBlacksmithUiAdapter(void) {
    close_open_seats();
    finish_intercepted_lifecycle("adapter_uninstall");
    SudekiMpRestoreInlineHook(&blacksmith_active_hook);
    SudekiMpRestoreInlineHook(&blacksmith_start_hook);
    original_blacksmith_active = NULL;
    original_blacksmith_start = NULL;
    adapter_enabled = FALSE;
    game_base = NULL;
    SudekiMpBlacksmithReadAdapterReset();
    p2_buttons_were_down = 0u;
    last_read_capture_ms = 0u;
    intercepted_world_scene = 0u;
    intercepted_world_descriptor = 0u;
    intercepted_world_descriptor_state = 0u;
    ZeroMemory(intercepted_actor_leases,
        sizeof(intercepted_actor_leases));
    ZeroMemory(p1_key_was_down, sizeof(p1_key_was_down));
    SudekiMpBlacksmithUiSessionInitialize(&ui_session);
}

#if defined(SUDEKIMP_BLACKSMITH_UI_ADAPTER_TESTING)
void SudekiMpBlacksmithUiAdapterInjectSecondHookFailureForTest(BOOL enabled) {
    fail_second_hook_for_test = enabled;
}
#endif
