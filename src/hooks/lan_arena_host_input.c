#include "hooks/lan_arena_host_input.h"

#include "cleanroom/engine.h"
#include "cleanroom/menu.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
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
typedef uint8_t (__attribute__((fastcall)) *QuickMenuSkillUseFunction)(
    void *skill, void *ignored_edx, int slot);

enum {
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_QUICK_SKILL_USE_CALL = 0x00027cb1u,
    RVA_QUICK_MENU_SKILL_USE_CALL = 0x000998a1u,
    RVA_SKILL_USE = 0x000b4810u,
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
static SudekiMpRelativeCallHook quick_skill_use_hook;
static SudekiMpRelativeCallHook quick_menu_skill_use_hook;
static ControllerCombatFunction original_controller_combat;
static ArbiterMovementFunction original_arbiter_movement;
static QuickMenuSkillUseFunction original_quick_menu_skill_use;
static SudekiMpLanArenaHostNativeSkillStartObserver
    native_skill_start_observer;
static BOOL combat_toggle_was_down;
static HANDLE operator_combat_toggle_event;
static HANDLE operator_combat_on_event;
static HANDLE operator_combat_off_event;
static HANDLE operator_weak_attack_event;
static HANDLE operator_forward_hold_event;
static HANDLE operator_strong_attack_event;
static HANDLE operator_sweep_attack_event;
static HANDLE operator_block_event;
static HANDLE operator_action_ack_event;
static HANDLE operator_skill_events[6];
static HANDLE operator_spirit_events[2];
static DWORD operator_trace_until_ms;
static int operator_forward_trace_state;
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

static BOOL operator_player_one_skill_direction(
    float local_direction[3]
) {
    if (local_direction == NULL || operator_forward_hold_event == NULL ||
        WaitForSingleObject(
            operator_forward_hold_event, 0u) != WAIT_OBJECT_0) {
        return FALSE;
    }
    local_direction[0] = 0.0f;
    local_direction[1] = 0.0f;
    local_direction[2] = 1.0f;
    return TRUE;
}

static void service_operator_forward_trace(void) {
    BOOL held;
    if (operator_forward_hold_event == NULL) return;
    held = WaitForSingleObject(
        operator_forward_hold_event, 0u) == WAIT_OBJECT_0;
    if ((int)held == operator_forward_trace_state) return;
    operator_forward_trace_state = (int)held;
    SudekiMpLogFormat(
        "lan_arena_host_input event=operator_forward state=%s "
        "policy=manual_reset_test_rail_neutral_physical_axes_only\r\n",
        held ? "held" : "released");
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

void SudekiMpLanArenaHostInputSetNativeSkillStartObserver(
    SudekiMpLanArenaHostNativeSkillStartObserver observer
) {
    native_skill_start_observer = observer;
}

static void notify_exact_tal_skill_start(
    void *tal,
    void *skill,
    int slot,
    uint32_t cost
) {
    SudekiMpCharacterSkillState state;
    BOOL active_seen = FALSE;
    if (native_skill_start_observer == NULL || tal == NULL || skill == NULL ||
        slot < 0 || slot >= 6) return;
    if (SudekiMpObserveCharacterSkill(tal, &state)) {
        if (state.skill != skill ||
            (state.active != 0u && state.slot != slot)) return;
        if (state.active != 0u) {
            active_seen = TRUE;
            cost = state.cost;
        }
    }
    native_skill_start_observer(
        tal, skill, slot, cost, active_seen);
}

static uint8_t __attribute__((fastcall)) observe_host_quick_menu_skill_use(
    void *skill,
    void *ignored_edx,
    int slot
) {
    void *owner = readable_memory(skill, 0x14u) ?
        *(void **)((uint8_t *)skill + 0x10u) : NULL;
    void *tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    SudekiMpCharacterSkillState before;
    SudekiMpSkillQuickSkillRow row;
    BOOL admission_exact = owner != NULL && owner == tal &&
        slot >= 0 && slot < 6 &&
        SudekiMpObserveCharacterSkill(tal, &before) &&
        before.active == 0u && before.skill == skill &&
        SudekiMpDescribeCharacterSkillSlot(tal, slot, &row) &&
        row.slot == slot;
    uint8_t result = original_quick_menu_skill_use(
        skill, ignored_edx, slot);
    /* The native nonzero Use return is the exact STARTED edge.  Do not wait
     * for CSkill.active: Sudeki can expose it on a later frame. */
    if (result != 0u && admission_exact) {
        notify_exact_tal_skill_start(tal, skill, slot, row.cost);
    }
    return result;
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

static unsigned int take_spirit_request_mask(void) {
    unsigned int mask = 0u;
    if (operator_spirit_events[0] != NULL &&
        WaitForSingleObject(operator_spirit_events[0], 0u) == WAIT_OBJECT_0) {
        mask |= 1u;
    }
    if (operator_spirit_events[1] != NULL &&
        WaitForSingleObject(operator_spirit_events[1], 0u) == WAIT_OBJECT_0) {
        mask |= 2u;
    }
    return mask;
}

BOOL SudekiMpLanArenaHostInputTakeSpiritVariant(unsigned int *variant) {
    unsigned int mask;
    if (variant == NULL) return FALSE;
    mask = take_spirit_request_mask();
    if (mask == 3u) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=operator_spirit phase=rejected "
            "reason=ambiguous_simultaneous_variants "
            "policy=drain_both_auto_reset_events_without_native_entry\r\n");
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (mask == 0u) return FALSE;
    *variant = mask == 1u ? 1u : 2u;
    return TRUE;
}

BOOL SudekiMpLanArenaHostInputDiscardSpiritRequests(void) {
    return take_spirit_request_mask() != 0u;
}

BOOL SudekiMpLanArenaHostInputTalControllerLeaseExact(void *tal) {
    uint8_t *controller;
    if (tal == NULL || host_game_base == NULL ||
        !readable_memory(host_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return FALSE;
    }
    controller = *(uint8_t **)(
        host_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    return readable_memory(
            controller, CONTROLLER_TARGET_OFFSET + sizeof(void *)) &&
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) == tal;
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
        SudekiMpCleanroomMenuActive() ||
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

static BOOL apply_combat_state(BOOL desired, const char *source) {
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
    if (enabled == desired) {
        SudekiMpLogFormat(
            "lan_arena_host_input event=cleanroom_combat_test result=confirmed "
            "state=%s source=%s policy=explicit_idempotent_host_state\r\n",
            desired ? "enabled" : "disabled", source);
        return TRUE;
    }
    if (!SudekiMpCleanroomEngineSetCombatMode(desired)) {
        SudekiMpLogFormat(
            "lan_arena_host_input event=cleanroom_combat_test result=rejected "
            "reason=native_transition_failed requested=%s key=F8 error=%lu\r\n",
            desired ? "enabled" : "disabled",
            (unsigned long)GetLastError());
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_host_input event=cleanroom_combat_test result=confirmed "
        "state=%s source=%s "
        "policy=test_only_native_group_transition_not_combat_authority\r\n",
        desired ? "enabled" : "disabled", source);
    return TRUE;
}

static BOOL apply_combat_toggle(const char *source) {
    BOOL enabled = FALSE;
    if (!SudekiMpCleanroomEngineCombatMode(&enabled)) {
        SudekiMpLogWrite(
            "lan_arena_host_input event=cleanroom_combat_test result=rejected "
            "reason=native_combat_not_ready key=F8\r\n");
        return FALSE;
    }
    return apply_combat_state(!enabled, source);
}

void SudekiMpLanArenaHostInputServiceCombatToggle(void) {
    /* GetKeyState follows this process' message queue. GetAsyncKeyState is a
     * desktop-global query under Wine and can leak keys between the isolated
     * host/client prefixes. */
    BOOL down = (GetKeyState(VK_F8) & (SHORT)0x8000) != 0;
    BOOL rising = down && !combat_toggle_was_down;
    BOOL operator_requested = operator_combat_toggle_event != NULL &&
        WaitForSingleObject(operator_combat_toggle_event, 0u) == WAIT_OBJECT_0;
    BOOL operator_on_requested = operator_combat_on_event != NULL &&
        WaitForSingleObject(operator_combat_on_event, 0u) == WAIT_OBJECT_0;
    BOOL operator_off_requested = operator_combat_off_event != NULL &&
        WaitForSingleObject(operator_combat_off_event, 0u) == WAIT_OBJECT_0;
    service_operator_forward_trace();
    combat_toggle_was_down = down;
    if (rising && local_process_owns_foreground() &&
        !SudekiMpCleanroomMenuInstalled()) {
        (void)apply_combat_toggle("local_F8");
    }
    if (operator_requested) {
        (void)apply_combat_toggle("local_operator_api");
    }
    if (operator_on_requested) {
        (void)apply_combat_state(TRUE, "local_operator_combat_on");
    }
    if (operator_off_requested) {
        (void)apply_combat_state(FALSE, "local_operator_combat_off");
    }
    service_operator_action();
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
    if ((SudekiMpLanArenaPausePanelActive() ||
         SudekiMpCleanroomMenuActive()) && character != NULL &&
        character == tal) {
        original_arbiter_movement(
            arbiter, stopped_direction, 0.0f, turn_rate, movement_mode);
        return;
    }
    original_arbiter_movement(
        arbiter, direction, speed, turn_rate, movement_mode);
    /* The host-input module is the sole owner of the two native Player-1
     * movement callsites.  During an authenticated remote Ailish skill,
     * control separation may have virtualized Tal's exact non-caster lock;
     * let its scoped adapter add the proven collision-aware delta without
     * stacking another call hook or changing ordinary movement. */
    (void)SudekiMpControlSeparationApplyLanArenaPlayerOneSkillMovement(
        arbiter, direction, speed);
}

static void __stdcall observe_host_combat(void *controller) {
    uint8_t *state = (uint8_t *)controller;
    void *tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    BOOL owns_tal = state != NULL && tal != NULL &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == tal;
    if (owns_tal && (SudekiMpLanArenaPausePanelActive() ||
                    SudekiMpCleanroomMenuActive())) {
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
        original_quick_menu_skill_use != NULL ||
        memcmp(base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry)) != 0) {
        SetLastError(base == NULL || original_controller_combat != NULL ||
            original_arbiter_movement != NULL ||
            original_quick_menu_skill_use != NULL ?
            ERROR_INVALID_PARAMETER : ERROR_INVALID_DATA);
        return FALSE;
    }
    original_arbiter_movement =
        (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    original_quick_menu_skill_use =
        (QuickMenuSkillUseFunction)(base + RVA_SKILL_USE);
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
        !SudekiMpInstallRelativeCallHook(
            &quick_skill_use_hook,
            base + RVA_QUICK_SKILL_USE_CALL,
            original_quick_menu_skill_use,
            observe_host_quick_menu_skill_use) ||
        !SudekiMpInstallRelativeCallHook(
            &quick_menu_skill_use_hook,
            base + RVA_QUICK_MENU_SKILL_USE_CALL,
            original_quick_menu_skill_use,
            observe_host_quick_menu_skill_use) ||
        !SudekiMpInstallInlineHook(
            &controller_combat_hook,
            base + RVA_CONTROLLER_COMBAT,
            expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry),
            observe_host_combat)) {
        DWORD install_error = GetLastError();
        if (!SudekiMpUninstallLanArenaHostInput()) return FALSE;
        SetLastError(install_error == ERROR_SUCCESS ?
            ERROR_GEN_FAILURE : install_error);
        return FALSE;
    }
    original_controller_combat =
        (ControllerCombatFunction)controller_combat_hook.trampoline;
    operator_combat_toggle_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_COMBAT_TOGGLE_EVENT);
    operator_combat_on_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_COMBAT_ON_EVENT);
    operator_combat_off_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_HOST_COMBAT_OFF_EVENT);
    operator_weak_attack_event = CreateEventW(
        NULL, FALSE, FALSE, SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT);
    operator_forward_hold_event = CreateEventW(
        NULL, TRUE, FALSE, SUDEKIMP_LAN_ARENA_HOST_FORWARD_HOLD_EVENT);
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
    operator_spirit_events[0] = CreateEventW(
        NULL, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_HOST_SPIRIT_VARIANT_ONE_EVENT);
    operator_spirit_events[1] = CreateEventW(
        NULL, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_HOST_SPIRIT_VARIANT_TWO_EVENT);
    if (operator_combat_toggle_event == NULL ||
        operator_combat_on_event == NULL ||
        operator_combat_off_event == NULL ||
        operator_weak_attack_event == NULL ||
        operator_forward_hold_event == NULL ||
        operator_strong_attack_event == NULL ||
        operator_sweep_attack_event == NULL || operator_block_event == NULL ||
        operator_action_ack_event == NULL ||
        operator_skill_events[0] == NULL ||
        operator_skill_events[1] == NULL ||
        operator_skill_events[2] == NULL ||
        operator_skill_events[3] == NULL ||
        operator_skill_events[4] == NULL ||
        operator_skill_events[5] == NULL ||
        operator_spirit_events[0] == NULL ||
        operator_spirit_events[1] == NULL) {
        DWORD install_error = GetLastError();
        if (!SudekiMpUninstallLanArenaHostInput()) return FALSE;
        SetLastError(install_error == ERROR_SUCCESS ?
            ERROR_GEN_FAILURE : install_error);
        return FALSE;
    }
    combat_toggle_was_down =
        (GetKeyState(VK_F8) & (SHORT)0x8000) != 0;
    ResetEvent(operator_forward_hold_event);
    ResetEvent(operator_spirit_events[0]);
    ResetEvent(operator_spirit_events[1]);
    operator_forward_trace_state = 0;
    operator_trace_until_ms = 0u;
    host_game_base = base;
    SudekiMpControlSeparationSetLanArenaPlayerOneSkillDirectionOverride(
        operator_player_one_skill_direction);
    return TRUE;
}

BOOL SudekiMpUninstallLanArenaHostInput(void) {
    BOOL restored = TRUE;
    DWORD restore_error = ERROR_SUCCESS;
    unsigned int skill_index;
#define RECORD_RESTORE_RESULT(expression) do { \
        if (!(expression)) { \
            DWORD current_error = GetLastError(); \
            if (restored) { \
                restore_error = current_error == ERROR_SUCCESS ? \
                    ERROR_WRITE_FAULT : current_error; \
            } \
            restored = FALSE; \
        } \
    } while (0)
    RECORD_RESTORE_RESULT(SudekiMpRestoreInlineHook(&controller_combat_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &quick_menu_skill_use_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &quick_skill_use_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &normal_movement_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &alternate_movement_hook));
#undef RECORD_RESTORE_RESULT
    if (!restored) {
        SudekiMpLogFormat(
            "lan_arena_host_input event=uninstall phase=restore_failed "
            "error=%lu policy=retain_callbacks_and_events_for_retry\r\n",
            (unsigned long)restore_error);
        SetLastError(restore_error);
        return FALSE;
    }
    original_controller_combat = NULL;
    original_arbiter_movement = NULL;
    original_quick_menu_skill_use = NULL;
    native_skill_start_observer = NULL;
    SudekiMpControlSeparationSetLanArenaPlayerOneSkillDirectionOverride(NULL);
    combat_toggle_was_down = FALSE;
    operator_forward_trace_state = 0;
    operator_trace_until_ms = 0u;
    host_game_base = NULL;
    if (operator_combat_toggle_event != NULL) {
        CloseHandle(operator_combat_toggle_event);
        operator_combat_toggle_event = NULL;
    }
    if (operator_combat_on_event != NULL) {
        CloseHandle(operator_combat_on_event);
        operator_combat_on_event = NULL;
    }
    if (operator_combat_off_event != NULL) {
        CloseHandle(operator_combat_off_event);
        operator_combat_off_event = NULL;
    }
    for (skill_index = 0u; skill_index < 6u; ++skill_index) {
        if (operator_skill_events[skill_index] != NULL) {
            CloseHandle(operator_skill_events[skill_index]);
            operator_skill_events[skill_index] = NULL;
        }
    }
    for (skill_index = 0u; skill_index < 2u; ++skill_index) {
        if (operator_spirit_events[skill_index] != NULL) {
            CloseHandle(operator_spirit_events[skill_index]);
            operator_spirit_events[skill_index] = NULL;
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
    if (operator_forward_hold_event != NULL) {
        ResetEvent(operator_forward_hold_event);
        CloseHandle(operator_forward_hold_event);
        operator_forward_hold_event = NULL;
    }
    return TRUE;
}

BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void) {
    return operator_trace_until_ms != 0u &&
        (LONG)(operator_trace_until_ms - GetTickCount()) > 0;
}
