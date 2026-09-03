#include "hooks/lan_arena_host_input.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_pause_panel.h"
#include "network/lan_arena_operator.h"
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
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
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
static BOOL combat_toggle_was_down;
static HANDLE operator_weak_attack_event;
static HANDLE operator_strong_attack_event;
static HANDLE operator_sweep_attack_event;
static HANDLE operator_block_event;
static HANDLE operator_action_ack_event;
static HANDLE operator_skill_events[6];
static DWORD operator_trace_until_ms;
static uint8_t *host_game_base;

typedef enum LanArenaHostOperatorAction {
    LAN_ARENA_HOST_OPERATOR_NONE = 0,
    LAN_ARENA_HOST_OPERATOR_WEAK,
    LAN_ARENA_HOST_OPERATOR_STRONG,
    LAN_ARENA_HOST_OPERATOR_SWEEP,
    LAN_ARENA_HOST_OPERATOR_BLOCK
} LanArenaHostOperatorAction;

static BOOL local_process_owns_foreground(void) {
    DWORD process_id = 0u;
    HWND window = GetForegroundWindow();
    if (window != NULL) GetWindowThreadProcessId(window, &process_id);
    return process_id == GetCurrentProcessId();
}

static BOOL readable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && length != 0u &&
        VirtualQuery(pointer, &information, sizeof(information)) != 0u &&
        information.State == MEM_COMMIT &&
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0u &&
        address + length >= address &&
        address + length <=
            (uintptr_t)information.BaseAddress + information.RegionSize;
}

static LanArenaHostOperatorAction take_operator_action(void) {
    LanArenaHostOperatorAction action = LAN_ARENA_HOST_OPERATOR_NONE;
#define TAKE_OPERATOR_EVENT(handle, candidate) \
    do { \
        if ((handle) != NULL && \
            WaitForSingleObject((handle), 0u) == WAIT_OBJECT_0 && \
            action == LAN_ARENA_HOST_OPERATOR_NONE) { \
            action = (candidate); \
        } \
    } while (0)
    TAKE_OPERATOR_EVENT(operator_weak_attack_event,
        LAN_ARENA_HOST_OPERATOR_WEAK);
    TAKE_OPERATOR_EVENT(operator_strong_attack_event,
        LAN_ARENA_HOST_OPERATOR_STRONG);
    TAKE_OPERATOR_EVENT(operator_sweep_attack_event,
        LAN_ARENA_HOST_OPERATOR_SWEEP);
    TAKE_OPERATOR_EVENT(operator_block_event,
        LAN_ARENA_HOST_OPERATOR_BLOCK);
#undef TAKE_OPERATOR_EVENT
    return action;
}

BOOL SudekiMpLanArenaHostInputTakeSkillSlot(unsigned int *slot) {
    unsigned int index;
    if (slot == NULL) return FALSE;
    for (index = 0u; index < 6u; ++index) {
        if (operator_skill_events[index] != NULL &&
            WaitForSingleObject(
                operator_skill_events[index], 0u) == WAIT_OBJECT_0) {
            *slot = index;
            return TRUE;
        }
    }
    return FALSE;
}

static void service_operator_action(void) {
    LanArenaHostOperatorAction action = take_operator_action();
    uint8_t *controller;
    void *tal;
    size_t offset;
    int saved_state;
    const char *label;
    SudekiMpLanArenaSessionStatus status;
    if (action == LAN_ARENA_HOST_OPERATOR_NONE) return;
    if (SudekiMpLanArenaPausePanelActive() ||
        !SudekiMpLanArenaSessionGetStatus(&status) ||
        !status.peer_connected ||
        status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        host_game_base == NULL || original_controller_combat == NULL ||
        !readable_memory(host_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=operator_action phase=rejected "
            "reason=host_controller_not_ready\r\n");
        return;
    }
    controller = *(uint8_t **)(
        host_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    if (!readable_memory(
            controller, CONTROLLER_TARGET_OFFSET + sizeof(void *)) ||
        tal == NULL || *(void **)(controller + CONTROLLER_TARGET_OFFSET) != tal) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=operator_action phase=rejected "
            "reason=tal_controller_lease_mismatch\r\n");
        return;
    }
    switch (action) {
    case LAN_ARENA_HOST_OPERATOR_WEAK:
        offset = CONTROLLER_WEAK_OFFSET;
        label = "weak";
        break;
    case LAN_ARENA_HOST_OPERATOR_STRONG:
        offset = CONTROLLER_STRONG_OFFSET;
        label = "strong";
        break;
    case LAN_ARENA_HOST_OPERATOR_SWEEP:
        offset = CONTROLLER_SWEEP_OFFSET;
        label = "sweep";
        break;
    case LAN_ARENA_HOST_OPERATOR_BLOCK:
        offset = CONTROLLER_BLOCK_OFFSET;
        label = "block";
        break;
    default:
        return;
    }
    saved_state = *(int *)(controller + offset);
    *(int *)(controller + offset) = 1;
    operator_trace_until_ms = GetTickCount() + 1500u;
    SudekiMpLogFormat(
        "lan_arena_host_input event=operator_action phase=submitted "
        "action=%s policy=one_native_controller_transition\r\n",
        label);
    original_controller_combat(controller);
    *(int *)(controller + offset) = saved_state;
}

void SudekiMpLanArenaHostInputNotifyNativeActionObserved(void) {
    if (operator_action_ack_event != NULL) {
        SetEvent(operator_action_ack_event);
    }
}

static BOOL apply_combat_toggle(const char *source) {
    SudekiMpLanArenaSessionStatus status;
    BOOL enabled = FALSE;
    if (!SudekiMpLanArenaSessionGetStatus(&status) ||
        status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        !status.peer_connected) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=cleanroom_combat_test result=rejected "
            "reason=authenticated_client_required key=F8\r\n");
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineCombatMode(&enabled)) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=cleanroom_combat_test result=rejected "
            "reason=native_combat_not_ready key=F8\r\n");
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineSetCombatMode(!enabled)) {
        SudekiMpLogFormat(
            "lan_arena_host_input event=cleanroom_combat_test result=rejected "
            "reason=native_transition_failed requested=%s key=F8 error=%lu\r\n",
            enabled ? "disabled" : "enabled",
            (unsigned long)GetLastError());
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_host_input event=cleanroom_combat_test result=confirmed "
        "state=%s source=%s "
        "policy=test_only_native_group_transition_not_combat_authority\r\n",
        enabled ? "disabled" : "enabled", source);
    return TRUE;
}

void SudekiMpLanArenaHostInputServiceCombatToggle(void) {
    /* GetKeyState follows this process' message queue. GetAsyncKeyState is a
     * desktop-global query under Wine and can leak keys between the isolated
     * host/client prefixes. */
    BOOL down = (GetKeyState(VK_F8) & (SHORT)0x8000) != 0;
    BOOL rising = down && !combat_toggle_was_down;
    combat_toggle_was_down = down;
    if (rising && local_process_owns_foreground()) {
        (void)apply_combat_toggle("local_F8");
    }
    service_operator_action();
}

BOOL SudekiMpLanArenaHostInputRequestRemoteCombatToggle(void) {
    return apply_combat_toggle("authenticated_client_F8_test");
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
    void *tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    BOOL owns_tal = state != NULL && tal != NULL &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == tal;
    if (owns_tal && SudekiMpLanArenaPausePanelActive()) {
        *(int *)(state + CONTROLLER_WEAK_OFFSET) = 0;
        *(int *)(state + CONTROLLER_STRONG_OFFSET) = 0;
        *(int *)(state + CONTROLLER_SWEEP_OFFSET) = 0;
        *(int *)(state + CONTROLLER_BLOCK_OFFSET) = 0;
        *(int *)(state + CONTROLLER_WEAPON_NEXT_OFFSET) = 0;
        *(int *)(state + CONTROLLER_WEAPON_PREVIOUS_OFFSET) = 0;
    }
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
    operator_weak_attack_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT);
    operator_strong_attack_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_STRONG_ATTACK_EVENT);
    operator_sweep_attack_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_SWEEP_ATTACK_EVENT);
    operator_block_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_BLOCK_EVENT);
    operator_action_ack_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_ACTION_ACK_EVENT);
    {
        static const wchar_t *const skill_names[6] = {
            SUDEKIMP_LAN_ARENA_SKILL_ZERO_EVENT,
            SUDEKIMP_LAN_ARENA_SKILL_ONE_EVENT,
            SUDEKIMP_LAN_ARENA_SKILL_TWO_EVENT,
            SUDEKIMP_LAN_ARENA_SKILL_THREE_EVENT,
            SUDEKIMP_LAN_ARENA_SKILL_FOUR_EVENT,
            SUDEKIMP_LAN_ARENA_SKILL_FIVE_EVENT
        };
        unsigned int index;
        for (index = 0u; index < 6u; ++index) {
            operator_skill_events[index] = CreateEventW(
                NULL, FALSE, FALSE, skill_names[index]);
        }
    }
    if (operator_weak_attack_event == NULL ||
        operator_strong_attack_event == NULL ||
        operator_sweep_attack_event == NULL || operator_block_event == NULL ||
        operator_action_ack_event == NULL ||
        operator_skill_events[0] == NULL ||
        operator_skill_events[1] == NULL ||
        operator_skill_events[2] == NULL ||
        operator_skill_events[3] == NULL ||
        operator_skill_events[4] == NULL ||
        operator_skill_events[5] == NULL) {
        SudekiMpUninstallLanArenaHostInput();
        return FALSE;
    }
    combat_toggle_was_down =
        (GetKeyState(VK_F8) & (SHORT)0x8000) != 0;
    operator_trace_until_ms = 0u;
    host_game_base = base;
    return TRUE;
}

void SudekiMpUninstallLanArenaHostInput(void) {
    unsigned int skill_index;
    SudekiMpRestoreInlineHook(&controller_combat_hook);
    SudekiMpRestoreRelativeCallHook(&normal_movement_hook);
    SudekiMpRestoreRelativeCallHook(&alternate_movement_hook);
    original_controller_combat = NULL;
    original_arbiter_movement = NULL;
    combat_toggle_was_down = FALSE;
    operator_trace_until_ms = 0u;
    host_game_base = NULL;
    for (skill_index = 0u; skill_index < 6u; ++skill_index) {
        if (operator_skill_events[skill_index] != NULL) {
            CloseHandle(operator_skill_events[skill_index]);
            operator_skill_events[skill_index] = NULL;
        }
    }
    if (operator_action_ack_event != NULL) {
        CloseHandle(operator_action_ack_event);
        operator_action_ack_event = NULL;
    }
    if (operator_block_event != NULL) {
        CloseHandle(operator_block_event);
        operator_block_event = NULL;
    }
    if (operator_sweep_attack_event != NULL) {
        CloseHandle(operator_sweep_attack_event);
        operator_sweep_attack_event = NULL;
    }
    if (operator_strong_attack_event != NULL) {
        CloseHandle(operator_strong_attack_event);
        operator_strong_attack_event = NULL;
    }
    if (operator_weak_attack_event != NULL) {
        CloseHandle(operator_weak_attack_event);
        operator_weak_attack_event = NULL;
    }
}

BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void) {
    return operator_trace_until_ms != 0u &&
        (LONG)(operator_trace_until_ms - GetTickCount()) > 0;
}
