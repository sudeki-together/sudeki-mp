#include "hooks/lan_arena_client_input.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_pause_panel.h"
#include "network/lan_arena_session.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter, const float *direction, float speed, float turn_rate,
    uint32_t movement_mode
);
typedef void (__stdcall *ControllerCombatFunction)(void *controller);
#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#endif
typedef uint8_t (SUDEKIMP_THISCALL *QuickMenuInputFunction)(
    void *quick_menu,
    unsigned int event_kind,
    unsigned int command,
    unsigned int value
);

enum {
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    RVA_QUICK_MENU_NATIVE_TOGGLE = 0x0000a080u,
    RVA_QUICK_MENU_NATIVE_TOGGLE_CALL = 0x00028228u,
    RVA_QUICK_MENU_INPUT = 0x00098b40u,
    RVA_QUICK_MENU_INPUT_VTABLE_SLOT = 0x002caf48u,
    RVA_QUICK_MENU_GLOBAL = 0x003c2f84u,
    RVA_QUICK_MENU_VTABLE = 0x002caf1cu,
    CHARACTER_ARBITER_OWNER_OFFSET = 0x10u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    CONTROLLER_WEAK_OFFSET = 0x8cu,
    CONTROLLER_STRONG_OFFSET = 0x94u,
    CONTROLLER_SWEEP_OFFSET = 0x9cu,
    CONTROLLER_BLOCK_OFFSET = 0xa4u,
    CONTROLLER_WEAPON_NEXT_OFFSET = 0xacu,
    CONTROLLER_WEAPON_PREVIOUS_OFFSET = 0xb4u,
    CONTROLLER_MOVE_X_OFFSET = 0x1a0u,
    CONTROLLER_MOVE_Y_OFFSET = 0x1a4u,
    QUICK_MENU_INPUT_EVENT_DOWN = 5u,
    QUICK_MENU_INPUT_EVENT_UP = 6u,
    QUICK_MENU_INPUT_EVENT_POINTER = 0x19u,
    QUICK_MENU_COMMAND_CONFIRM = 0u,
    QUICK_MENU_COMMAND_SECONDARY_CONFIRM = 2u,
    QUICK_MENU_ACTIVE_OFFSET = 0x29u
};

static const uint8_t expected_controller_combat_entry[] = {
    0x55u, 0x8bu, 0x6cu, 0x24u, 0x08u,
    0x83u, 0xbdu, 0x48u, 0x02u, 0x00u, 0x00u, 0x00u
};
static const uint8_t expected_quick_menu_native_toggle_entry[] = {
    0x80u, 0xb8u, 0x8cu, 0x00u, 0x00u,
    0x00u, 0x00u, 0x74u, 0x46u
};
static const uint8_t expected_quick_menu_input_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x55u, 0x56u, 0x57u, 0x8bu,
    0xe9u, 0x83u, 0xf8u, 0x19u
};

static SudekiMpRelativeCallHook alternate_movement_hook;
static SudekiMpRelativeCallHook normal_movement_hook;
static SudekiMpInlineHook controller_combat_hook;
static SudekiMpPointerHook quick_menu_input_hook;
static ArbiterMovementFunction original_arbiter_movement;
static ControllerCombatFunction original_controller_combat;
static QuickMenuInputFunction original_quick_menu_input;
static int16_t last_direction_x;
static int16_t last_direction_z;
static BOOL weak_was_down;
static BOOL movement_send_logged;
static BOOL weak_send_logged;
static BOOL quick_menu_action_block_logged;
static int quick_menu_visible_state;
static DWORD last_input_send_at;
static int16_t last_transmitted_direction_x;
static int16_t last_transmitted_direction_z;
static uint8_t *client_game_base;

enum {
    CLIENT_INPUT_SEND_INTERVAL_MS = 50u
};

static BOOL readable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    if (pointer == NULL || length == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u ||
        address + length < address ||
        address + length >
            (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    return TRUE;
}

/* The client may browse Ailish's own native full-screen QuickMenu, but it is
 * a presentation terminal rather than an action authority.  Consume both
 * native confirm commands until each category has a verified host-routed
 * adapter. Navigation, category changes, Q toggle, and close remain retail. */
static uint8_t SUDEKIMP_THISCALL route_client_quick_menu_input(
    void *quick_menu,
    unsigned int event_kind,
    unsigned int command,
    unsigned int value
) {
    BOOL action_event = event_kind == QUICK_MENU_INPUT_EVENT_DOWN ||
        event_kind == QUICK_MENU_INPUT_EVENT_UP ||
        event_kind == QUICK_MENU_INPUT_EVENT_POINTER;
    if (action_event &&
        (command == QUICK_MENU_COMMAND_CONFIRM ||
         command == QUICK_MENU_COMMAND_SECONDARY_CONFIRM)) {
        if (!quick_menu_action_block_logged) {
            quick_menu_action_block_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_client_input event=quick_menu_action "
                "phase=rejected reason=host_adapter_not_implemented "
                "policy=native_browse_and_close_allowed_execution_blocked\r\n");
        }
        return 1u;
    }
    return original_quick_menu_input == NULL ? 0u :
        original_quick_menu_input(
            quick_menu, event_kind, command, value);
}

static BOOL authenticated_client(void) {
    SudekiMpLanArenaSessionStatus status;
    return SudekiMpLanArenaSessionGetStatus(&status) && status.peer_connected &&
        status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
}

static BOOL client_quick_menu_visible(void) {
    uint8_t *menu;
    if (client_game_base == NULL || !readable_memory(
            client_game_base + RVA_QUICK_MENU_GLOBAL, sizeof(menu))) {
        return FALSE;
    }
    menu = *(uint8_t **)(client_game_base + RVA_QUICK_MENU_GLOBAL);
    return readable_memory(menu, QUICK_MENU_ACTIVE_OFFSET + 1u) &&
        *(void **)menu == client_game_base + RVA_QUICK_MENU_VTABLE &&
        menu[QUICK_MENU_ACTIVE_OFFSET] != 0u;
}

static BOOL client_local_modal_active(void) {
    return SudekiMpLanArenaPausePanelActive() || client_quick_menu_visible();
}

static int16_t normalized_axis(float value) {
    if (!isfinite(value)) return 0;
    if (value >= 1.0f) return 32767;
    if (value <= -1.0f) return -32767;
    return (int16_t)(value * 32767.0f);
}

static BOOL send_client_input(
    int16_t direction_x,
    int16_t direction_z,
    BOOL weak
) {
    SudekiMpLanArenaInput input;
    input.sequence = 0u;
    input.acknowledged_snapshot = 0u;
    input.client_tick = GetTickCount();
    input.world_direction_x = direction_x;
    input.world_direction_z = direction_z;
    input.weak_attack_pressed = weak ? 1u : 0u;
    return SudekiMpLanArenaSessionSendInput(&input);
}

static BOOL send_client_input_at(
    int16_t direction_x,
    int16_t direction_z,
    BOOL weak,
    DWORD now
) {
    if (!send_client_input(direction_x, direction_z, weak)) return FALSE;
    last_input_send_at = now;
    last_transmitted_direction_x = direction_x;
    last_transmitted_direction_z = direction_z;
    return TRUE;
}

static void __stdcall capture_client_movement(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
) {
    DWORD now;
    int16_t direction_x;
    int16_t direction_z;
    BOOL changed;
    void *character = arbiter == NULL ? NULL :
        *(void **)((uint8_t *)arbiter + CHARACTER_ARBITER_OWNER_OFFSET);
    void *ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    (void)turn_rate;
    (void)movement_mode;
    if (!authenticated_client() || direction == NULL || character == NULL ||
        character != ailish) {
        original_arbiter_movement(arbiter, direction, speed, turn_rate, movement_mode);
        return;
    }
    if (client_local_modal_active()) {
        now = GetTickCount();
        last_direction_x = 0;
        last_direction_z = 0;
        if (last_transmitted_direction_x != 0 ||
            last_transmitted_direction_z != 0 || last_input_send_at == 0u) {
            (void)send_client_input_at(0, 0, FALSE, now);
        }
        return;
    }
    if (!isfinite(speed) || speed <= 0.0f) speed = 0.0f;
    if (speed > 1.0f) speed = 1.0f;
    now = GetTickCount();
    /* This direction is already the retail controller's live camera-relative
     * world vector. Preserve it exactly: recomputing or latching it in the LAN
     * layer breaks native mouse-steered arcs around actors and scenery. */
    direction_x = normalized_axis(direction[0] * speed);
    direction_z = normalized_axis(direction[2] * speed);
    changed = direction_x != last_direction_x || direction_z != last_direction_z;
    last_direction_x = direction_x;
    last_direction_z = direction_z;
    if ((changed || last_input_send_at == 0u ||
         (DWORD)(now - last_input_send_at) >= CLIENT_INPUT_SEND_INTERVAL_MS) &&
        send_client_input_at(last_direction_x, last_direction_z, FALSE, now) &&
        !movement_send_logged &&
        (last_direction_x != 0 || last_direction_z != 0)) {
        movement_send_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_input event=movement_send phase=confirmed "
            "world_direction=%d,%d policy=native_live_camera_relative_vector\r\n",
            (int)last_direction_x, (int)last_direction_z);
    }
    /* Do not call the local native arbiter. The host is the sole combat and
     * movement authority; this process only keeps Ailish's native camera/UI. */
}

void SudekiMpLanArenaClientInputService(void) {
    DWORD now;
    DWORD foreground_process_id = 0u;
    HWND foreground;
    uint8_t *controller;
    void *ailish;
    float raw_x;
    float raw_y;
    BOOL keyboard_direction_held;
    BOOL local_direction_held;
    int16_t desired_x;
    int16_t desired_z;
    if (!authenticated_client()) return;
    now = GetTickCount();
    {
        BOOL visible = client_quick_menu_visible();
        if ((int)visible != quick_menu_visible_state) {
            quick_menu_visible_state = (int)visible;
            if (!visible) quick_menu_action_block_logged = FALSE;
            SudekiMpLogFormat(
                "lan_arena_client_input event=native_quick_menu state=%s "
                "authority=browse_only local_gameplay=quiesced\r\n",
                visible ? "open" : "closed");
        }
    }
    if (client_local_modal_active()) {
        last_direction_x = 0;
        last_direction_z = 0;
        if (last_transmitted_direction_x != 0 ||
            last_transmitted_direction_z != 0 || last_input_send_at == 0u) {
            (void)send_client_input_at(0, 0, FALSE, now);
        }
        return;
    }
    foreground = GetForegroundWindow();
    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &foreground_process_id);
    }
    controller = client_game_base != NULL && readable_memory(
            client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller)) ?
        *(uint8_t **)(client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL) : NULL;
    ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    if (readable_memory(
            controller, CONTROLLER_TARGET_OFFSET + sizeof(void *)) &&
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) == ailish) {
        raw_x = *(float *)(controller + CONTROLLER_MOVE_X_OFFSET);
        raw_y = *(float *)(controller + CONTROLLER_MOVE_Y_OFFSET);
    } else {
        raw_x = 0.0f;
        raw_y = 0.0f;
    }
    keyboard_direction_held =
        (GetAsyncKeyState('W') & 0x8000) != 0 ||
        (GetAsyncKeyState('A') & 0x8000) != 0 ||
        (GetAsyncKeyState('S') & 0x8000) != 0 ||
        (GetAsyncKeyState('D') & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_UP) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    local_direction_held = foreground_process_id == GetCurrentProcessId() &&
        ((isfinite(raw_x) && isfinite(raw_y) &&
          (fabsf(raw_x) > 0.0001f || fabsf(raw_y) > 0.0001f)) ||
         keyboard_direction_held);
    desired_x = local_direction_held ? last_direction_x : 0;
    desired_z = local_direction_held ? last_direction_z : 0;
    if (desired_x != last_transmitted_direction_x ||
        desired_z != last_transmitted_direction_z ||
        ((desired_x != 0 || desired_z != 0) &&
            (DWORD)(now - last_input_send_at) >=
                CLIENT_INPUT_SEND_INTERVAL_MS) ||
        last_input_send_at == 0u) {
        (void)send_client_input_at(desired_x, desired_z, FALSE, now);
    }
}

static void __stdcall capture_client_combat(void *controller) {
    uint8_t *state = (uint8_t *)controller;
    void *ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    BOOL owns_ailish = state != NULL && ailish != NULL &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == ailish;
    BOOL weak_edge = owns_ailish && !client_local_modal_active() &&
        *(int *)(state + CONTROLLER_WEAK_OFFSET) == 1;
    if (!authenticated_client() || !owns_ailish) {
        original_controller_combat(controller);
        return;
    }
    if (weak_edge && !weak_was_down) {
        if (send_client_input_at(
                last_direction_x, last_direction_z, TRUE, GetTickCount()) &&
            !weak_send_logged) {
            weak_send_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_client_input event=weak_attack_send phase=confirmed "
                "policy=edge_only_host_native_execution\r\n");
        }
    }
    weak_was_down = weak_edge;
    *(int *)(state + CONTROLLER_WEAK_OFFSET) = 0;
    *(int *)(state + CONTROLLER_STRONG_OFFSET) = 0;
    *(int *)(state + CONTROLLER_SWEEP_OFFSET) = 0;
    *(int *)(state + CONTROLLER_BLOCK_OFFSET) = 0;
    *(int *)(state + CONTROLLER_WEAPON_NEXT_OFFSET) = 0;
    *(int *)(state + CONTROLLER_WEAPON_PREVIOUS_OFFSET) = 0;
    original_controller_combat(controller);
}

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module) {
    uint8_t *base;
    if (game_module == NULL || original_arbiter_movement != NULL ||
        original_controller_combat != NULL ||
        original_quick_menu_input != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (memcmp(base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry)) != 0 ||
        memcmp(base + RVA_QUICK_MENU_NATIVE_TOGGLE,
            expected_quick_menu_native_toggle_entry,
            sizeof(expected_quick_menu_native_toggle_entry)) != 0 ||
        memcmp(base + RVA_QUICK_MENU_INPUT,
            expected_quick_menu_input_entry,
            sizeof(expected_quick_menu_input_entry)) != 0 ||
        *(void **)(base + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
            base + RVA_QUICK_MENU_INPUT) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    original_arbiter_movement = (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    original_quick_menu_input =
        (QuickMenuInputFunction)(base + RVA_QUICK_MENU_INPUT);
    if (!SudekiMpInstallRelativeCallHook(&alternate_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE, original_arbiter_movement,
            capture_client_movement) ||
        !SudekiMpInstallRelativeCallHook(&normal_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL, original_arbiter_movement,
            capture_client_movement) ||
        !SudekiMpInstallInlineHook(&controller_combat_hook,
            base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry), capture_client_combat) ||
        !SudekiMpInstallPointerHook(&quick_menu_input_hook,
            (void **)(base + RVA_QUICK_MENU_INPUT_VTABLE_SLOT),
            original_quick_menu_input,
            route_client_quick_menu_input)) {
        SudekiMpUninstallLanArenaClientInput();
        return FALSE;
    }
    original_controller_combat = (ControllerCombatFunction)controller_combat_hook.trampoline;
    last_direction_x = 0;
    last_direction_z = 0;
    weak_was_down = FALSE;
    movement_send_logged = FALSE;
    weak_send_logged = FALSE;
    quick_menu_action_block_logged = FALSE;
    quick_menu_visible_state = -1;
    last_input_send_at = 0u;
    last_transmitted_direction_x = 0;
    last_transmitted_direction_z = 0;
    client_game_base = base;
    return TRUE;
}

void SudekiMpUninstallLanArenaClientInput(void) {
    SudekiMpRestorePointerHook(&quick_menu_input_hook);
    SudekiMpRestoreInlineHook(&controller_combat_hook);
    SudekiMpRestoreRelativeCallHook(&normal_movement_hook);
    SudekiMpRestoreRelativeCallHook(&alternate_movement_hook);
    original_controller_combat = NULL;
    original_arbiter_movement = NULL;
    original_quick_menu_input = NULL;
    last_direction_x = 0;
    last_direction_z = 0;
    weak_was_down = FALSE;
    movement_send_logged = FALSE;
    weak_send_logged = FALSE;
    quick_menu_action_block_logged = FALSE;
    quick_menu_visible_state = -1;
    last_input_send_at = 0u;
    last_transmitted_direction_x = 0;
    last_transmitted_direction_z = 0;
    client_game_base = NULL;
}
