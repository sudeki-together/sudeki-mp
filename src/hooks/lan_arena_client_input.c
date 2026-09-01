#include "hooks/lan_arena_client_input.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "network/lan_arena_session.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter, const float *direction, float speed, float turn_rate,
    uint32_t movement_mode
);
typedef void (__stdcall *ControllerCombatFunction)(void *controller);

enum {
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    RVA_QUICK_MENU_NATIVE_TOGGLE = 0x0000a080u,
    RVA_QUICK_MENU_NATIVE_TOGGLE_CALL = 0x00028228u,
    CHARACTER_ARBITER_OWNER_OFFSET = 0x10u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    CONTROLLER_WEAK_OFFSET = 0x8cu,
    CONTROLLER_STRONG_OFFSET = 0x94u,
    CONTROLLER_SWEEP_OFFSET = 0x9cu,
    CONTROLLER_BLOCK_OFFSET = 0xacu,
    CONTROLLER_WEAPON_NEXT_OFFSET = 0xb4u,
    CONTROLLER_MOVE_X_OFFSET = 0x1a0u,
    CONTROLLER_MOVE_Y_OFFSET = 0x1a4u
};

static const uint8_t expected_controller_combat_entry[] = {
    0x55u, 0x8bu, 0x6cu, 0x24u, 0x08u,
    0x83u, 0xbdu, 0x48u, 0x02u, 0x00u, 0x00u, 0x00u
};
static const uint8_t expected_quick_menu_native_toggle_entry[] = {
    0x80u, 0xb8u, 0x8cu, 0x00u, 0x00u,
    0x00u, 0x00u, 0x74u, 0x46u
};

static SudekiMpRelativeCallHook alternate_movement_hook;
static SudekiMpRelativeCallHook normal_movement_hook;
static SudekiMpInlineHook controller_combat_hook;
static SudekiMpRelativeCallHook quick_menu_native_toggle_hook;
static ArbiterMovementFunction original_arbiter_movement;
static ControllerCombatFunction original_controller_combat;
static void *original_quick_menu_native_toggle;
static int16_t last_direction_x;
static int16_t last_direction_z;
static BOOL weak_was_down;
static BOOL movement_send_logged;
static BOOL weak_send_logged;
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

/* A LAN client is a presentation/input terminal, never an independent native
 * gameplay authority.  In particular, opening Sudeki's singleton QuickMenu
 * would pause or mutate only the client process and could execute unverified
 * item/skill actions.  Consume its sole gameplay toggle call unconditionally
 * for this profile; the host retains its ordinary native pause/QuickMenu UI. */
__attribute__((noinline, used))
static BOOL lan_arena_client_quick_menu_suppressed(void) {
    return TRUE;
}

__attribute__((naked, noinline, used))
static void lan_arena_client_quick_menu_toggle_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "call _lan_arena_client_quick_menu_suppressed\n\t"
        "testl %eax, %eax\n\t"
        "popl %eax\n\t"
        "jnz 1f\n\t"
        "call *_original_quick_menu_native_toggle\n\t"
        "ret\n\t"
        "1:\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t"
    );
}

static BOOL authenticated_client(void) {
    SudekiMpLanArenaSessionStatus status;
    return SudekiMpLanArenaSessionGetStatus(&status) && status.peer_connected &&
        status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
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
    if (!isfinite(speed) || speed <= 0.0f) speed = 0.0f;
    if (speed > 1.0f) speed = 1.0f;
    now = GetTickCount();
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
            "world_direction=%d,%d policy=client_capture_host_authority\r\n",
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
    BOOL weak_edge = owns_ailish && *(int *)(state + CONTROLLER_WEAK_OFFSET) == 1;
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
    original_controller_combat(controller);
}

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module) {
    uint8_t *base;
    if (game_module == NULL || original_arbiter_movement != NULL ||
        original_controller_combat != NULL ||
        original_quick_menu_native_toggle != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (memcmp(base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry)) != 0 ||
        memcmp(base + RVA_QUICK_MENU_NATIVE_TOGGLE,
            expected_quick_menu_native_toggle_entry,
            sizeof(expected_quick_menu_native_toggle_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    original_arbiter_movement = (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    original_quick_menu_native_toggle = base + RVA_QUICK_MENU_NATIVE_TOGGLE;
    if (!SudekiMpInstallRelativeCallHook(&alternate_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE, original_arbiter_movement,
            capture_client_movement) ||
        !SudekiMpInstallRelativeCallHook(&normal_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL, original_arbiter_movement,
            capture_client_movement) ||
        !SudekiMpInstallInlineHook(&controller_combat_hook,
            base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry), capture_client_combat) ||
        !SudekiMpInstallRelativeCallHook(&quick_menu_native_toggle_hook,
            base + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL,
            original_quick_menu_native_toggle,
            lan_arena_client_quick_menu_toggle_entry)) {
        SudekiMpUninstallLanArenaClientInput();
        return FALSE;
    }
    original_controller_combat = (ControllerCombatFunction)controller_combat_hook.trampoline;
    last_direction_x = 0;
    last_direction_z = 0;
    weak_was_down = FALSE;
    movement_send_logged = FALSE;
    weak_send_logged = FALSE;
    last_input_send_at = 0u;
    last_transmitted_direction_x = 0;
    last_transmitted_direction_z = 0;
    client_game_base = base;
    return TRUE;
}

void SudekiMpUninstallLanArenaClientInput(void) {
    SudekiMpRestoreRelativeCallHook(&quick_menu_native_toggle_hook);
    SudekiMpRestoreInlineHook(&controller_combat_hook);
    SudekiMpRestoreRelativeCallHook(&normal_movement_hook);
    SudekiMpRestoreRelativeCallHook(&alternate_movement_hook);
    original_controller_combat = NULL;
    original_arbiter_movement = NULL;
    original_quick_menu_native_toggle = NULL;
    last_direction_x = 0;
    last_direction_z = 0;
    weak_was_down = FALSE;
    movement_send_logged = FALSE;
    weak_send_logged = FALSE;
    last_input_send_at = 0u;
    last_transmitted_direction_x = 0;
    last_transmitted_direction_z = 0;
    client_game_base = NULL;
}
