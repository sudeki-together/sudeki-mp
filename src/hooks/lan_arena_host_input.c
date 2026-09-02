#include "hooks/lan_arena_host_input.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_pause_panel.h"
#include "network/lan_arena_session.h"

#include <stdint.h>
#include <string.h>

typedef void (__stdcall *ControllerCombatFunction)(void *controller);
typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter, const float *direction, float speed, float turn_rate,
    uint32_t movement_mode
);

enum {
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    CHARACTER_ARBITER_OWNER_OFFSET = 0x10u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    CONTROLLER_WEAK_OFFSET = 0x8cu,
    CONTROLLER_STRONG_OFFSET = 0x94u,
    CONTROLLER_SWEEP_OFFSET = 0x9cu,
    CONTROLLER_BLOCK_OFFSET = 0xa4u,
    CONTROLLER_WEAPON_NEXT_OFFSET = 0xacu,
    CONTROLLER_WEAPON_PREVIOUS_OFFSET = 0xb4u
};

static const uint8_t expected_controller_combat_entry[] = {
    0x55u, 0x8bu, 0x6cu, 0x24u, 0x08u,
    0x83u, 0xbdu, 0x48u, 0x02u, 0x00u, 0x00u, 0x00u
};

static SudekiMpInlineHook controller_combat_hook;
static SudekiMpRelativeCallHook alternate_movement_hook;
static SudekiMpRelativeCallHook normal_movement_hook;
static ControllerCombatFunction original_controller_combat;
static ArbiterMovementFunction original_arbiter_movement;
static BOOL tal_weak_was_down;
static BOOL tal_weak_pending;
static BOOL combat_toggle_was_down;

static BOOL local_process_owns_foreground(void) {
    DWORD process_id = 0u;
    HWND window = GetForegroundWindow();
    if (window != NULL) GetWindowThreadProcessId(window, &process_id);
    return process_id == GetCurrentProcessId();
}

void SudekiMpLanArenaHostInputServiceCombatToggle(void) {
    SudekiMpLanArenaSessionStatus status;
    BOOL enabled = FALSE;
    BOOL down = (GetAsyncKeyState(VK_F8) & (SHORT)0x8000) != 0;
    BOOL rising = down && !combat_toggle_was_down;
    combat_toggle_was_down = down;
    if (!rising || !local_process_owns_foreground()) return;
    if (!SudekiMpLanArenaSessionGetStatus(&status) ||
        status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        !status.peer_connected) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=combat_toggle result=rejected "
            "reason=authenticated_client_required key=F8\r\n");
        return;
    }
    if (!SudekiMpCleanroomEngineCombatMode(&enabled)) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=combat_toggle result=rejected "
            "reason=native_combat_not_ready key=F8\r\n");
        return;
    }
    if (!SudekiMpCleanroomEngineSetCombatMode(!enabled)) {
        SudekiMpLogFormat(
            "lan_arena_host_input event=combat_toggle result=rejected "
            "reason=native_transition_failed requested=%s key=F8 error=%lu\r\n",
            enabled ? "disabled" : "enabled",
            (unsigned long)GetLastError());
        return;
    }
    SudekiMpLogFormat(
        "lan_arena_host_input event=combat_toggle result=confirmed "
        "state=%s key=F8 policy=host_only_native_group_transition\r\n",
        enabled ? "disabled" : "enabled");
}

static void __stdcall gate_host_movement(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
) {
    static const float stopped_direction[3] = {0.0f, 0.0f, 0.0f};
    void *character = arbiter == NULL ? NULL :
        *(void **)((uint8_t *)arbiter + CHARACTER_ARBITER_OWNER_OFFSET);
    void *tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    if (SudekiMpLanArenaPausePanelActive() && character != NULL &&
        character == tal) {
        original_arbiter_movement(
            arbiter, stopped_direction, 0.0f, turn_rate, movement_mode);
        return;
    }
    original_arbiter_movement(
        arbiter, direction, speed, turn_rate, movement_mode);
}

static void __stdcall observe_host_combat(void *controller) {
    uint8_t *state = (uint8_t *)controller;
    void *tal;
    tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    BOOL owns_tal = state != NULL && tal != NULL &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == tal;
    BOOL weak_down = owns_tal &&
        *(int *)(state + CONTROLLER_WEAK_OFFSET) == 1;
    if (owns_tal && SudekiMpLanArenaPausePanelActive()) {
        *(int *)(state + CONTROLLER_WEAK_OFFSET) = 0;
        *(int *)(state + CONTROLLER_STRONG_OFFSET) = 0;
        *(int *)(state + CONTROLLER_SWEEP_OFFSET) = 0;
        *(int *)(state + CONTROLLER_BLOCK_OFFSET) = 0;
        *(int *)(state + CONTROLLER_WEAPON_NEXT_OFFSET) = 0;
        *(int *)(state + CONTROLLER_WEAPON_PREVIOUS_OFFSET) = 0;
        weak_down = FALSE;
    }
    if (weak_down && !tal_weak_was_down) tal_weak_pending = TRUE;
    tal_weak_was_down = weak_down;
    original_controller_combat(controller);
}

BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    if (base == NULL || original_controller_combat != NULL ||
        original_arbiter_movement != NULL ||
        memcmp(base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry)) != 0) {
        SetLastError(base == NULL || original_controller_combat != NULL ||
            original_arbiter_movement != NULL ?
            ERROR_INVALID_PARAMETER : ERROR_INVALID_DATA);
        return FALSE;
    }
    original_arbiter_movement =
        (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    if (!SudekiMpInstallRelativeCallHook(
            &alternate_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE,
            original_arbiter_movement,
            gate_host_movement) ||
        !SudekiMpInstallRelativeCallHook(
            &normal_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL,
            original_arbiter_movement,
            gate_host_movement) ||
        !SudekiMpInstallInlineHook(
            &controller_combat_hook,
            base + RVA_CONTROLLER_COMBAT,
            expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry),
            observe_host_combat)) {
        SudekiMpUninstallLanArenaHostInput();
        return FALSE;
    }
    original_controller_combat =
        (ControllerCombatFunction)controller_combat_hook.trampoline;
    tal_weak_was_down = FALSE;
    tal_weak_pending = FALSE;
    combat_toggle_was_down =
        (GetAsyncKeyState(VK_F8) & (SHORT)0x8000) != 0;
    return TRUE;
}

void SudekiMpUninstallLanArenaHostInput(void) {
    SudekiMpRestoreInlineHook(&controller_combat_hook);
    SudekiMpRestoreRelativeCallHook(&normal_movement_hook);
    SudekiMpRestoreRelativeCallHook(&alternate_movement_hook);
    original_controller_combat = NULL;
    original_arbiter_movement = NULL;
    tal_weak_was_down = FALSE;
    tal_weak_pending = FALSE;
    combat_toggle_was_down = FALSE;
}

BOOL SudekiMpLanArenaHostInputTakeTalWeakAttack(void) {
    BOOL pending = tal_weak_pending;
    tal_weak_pending = FALSE;
    return pending;
}
