#include "hooks/lan_arena_runtime.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/lan_arena_client_input.h"
#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "hooks/lan_arena_collision_debug.h"
#include "hooks/lan_arena_host_input.h"
#include "network/lan_arena_authority.h"
#include "network/lan_arena_endpoint.h"
#include "network/lan_arena_replica.h"
#include "network/lan_arena_shared_simulation.h"
#include "network/lan_arena_tal_combo_graph.h"

#include <math.h>
#include <string.h>
#include <stdint.h>

typedef void (*FrameEndFunction)(void);
typedef void (*RenderStartFunction)(void);

enum {
    RVA_RENDER_START = 0x001dce30u,
    RVA_RENDER_START_CALL = 0x0028d443u,
    RVA_RENDER_PRE_WORLD_CALL = 0x0028d539u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu,
    TAL_WORLD_IDLE_SELECTOR = 4,
    TAL_WORLD_MOVE_PRIMARY_SELECTOR = 8,
    TAL_WORLD_MOVE_SECONDARY_SELECTOR = 9,
    TAL_WORLD_IDLE_VARIANT_ONE_SELECTOR = 10,
    TAL_WORLD_IDLE_VARIANT_TWO_SELECTOR = 11,
    AILISH_WORLD_MOVE_PRIMARY_SELECTOR = 7,
    AILISH_WORLD_MOVE_SECONDARY_SELECTOR = 8,
    AILISH_WORLD_IDLE_SELECTOR = 1,
    AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR = 4,
    AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR = 5,
    /* Exact native presentation captured from the supported cleanroom combat
     * transition. These renderer selectors remain local to each process. */
    TAL_COMBAT_ENTRY_SELECTOR = 3,
    TAL_COMBAT_IDLE_SELECTOR = 17,
    TAL_COMBAT_MOVE_PRIMARY_SELECTOR = 36,
    TAL_COMBAT_MOVE_SECONDARY_SELECTOR = 32,
    AILISH_COMBAT_ENTRY_SELECTOR = 12,
    AILISH_COMBAT_IDLE_SELECTOR = 20,
    AILISH_COMBAT_MOVE_PRIMARY_SELECTOR = 22,
    AILISH_COMBAT_MOVE_SECONDARY_SELECTOR = 23,
    AILISH_COMBAT_WEAK_SELECTOR = 59
};

static SudekiMpRelativeCallHook lan_arena_frame_end_hook;
static SudekiMpRelativeCallHook lan_arena_render_start_hook;
static SudekiMpRelativeCallHook lan_arena_render_pre_world_hook;
static FrameEndFunction original_frame_end;
static RenderStartFunction original_render_start;
static BOOL runtime_installed;
static SudekiMpLanArenaSharedSimulation canonical_simulation;
static BOOL host_remote_ailish_owned;
static BOOL host_ailish_spawn_attempted;
static BOOL client_remote_tal_owned;
static BOOL client_tal_spawn_attempted;
static BOOL arena_dummy_spawn_attempted;
static BOOL tal_initialized;
static BOOL ailish_initialized;
static DWORD host_last_snapshot_at_ms;
static BOOL host_snapshot_stream_logged;
static BOOL client_replica_stream_logged;
static BOOL client_replica_discard_logged;
static BOOL host_release_pending_logged;
static BOOL client_release_pending_logged;
static int client_replica_stop_state = -1;
static int client_animation_basis_publish_state = -1;
static int client_presentation_reassert_state = -1;
static int client_visible_transform_publish_state = -1;
static BOOL runtime_hook_restore_failed_logged;
static BOOL dummy_release_pending_logged;
static float host_previous_actor_position[2][3];
static BOOL host_previous_actor_position_valid[2];
static DWORD host_actor_last_translation_at_ms[2];
static float host_replica_idle_position[2][2];
static BOOL host_replica_idle_position_valid[2];
static BOOL host_actor_was_moving[2];
static uint8_t host_ailish_idle_variant_state;
static DWORD host_ailish_idle_variant_seen_at_ms;
static BOOL host_ailish_idle_variant_armed = TRUE;
static DWORD host_ailish_weak_attack_until_ms;
static uint8_t host_actor_action_variant[2];
static uint16_t host_actor_action_sequence[2];
static SudekiMpLanArenaActionEvent
    host_actor_action_history[2][SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY];
static uint8_t host_actor_action_history_count[2];
static uint8_t host_actor_previous_action_variant[2];
static uint8_t host_actor_previous_native_action_state[2];
static BOOL host_actor_previous_action_active[2];
static BOOL host_actor_previous_native_action_active[2];
typedef struct HostActorActionRetirement {
    uint16_t action_sequence;
    uint16_t action_terminal_phase_q8;
    uint16_t idle_entry_phase_q8;
    int32_t action_terminal_selector;
    int32_t idle_entry_selector;
    uint8_t action_terminal_state;
    uint8_t idle_entry_state;
    BOOL pending;
} HostActorActionRetirement;
static HostActorActionRetirement host_actor_action_retirement[2];
static DWORD host_last_remote_input_at_ms;
static BOOL host_remote_input_quiesced;
static BOOL host_remote_input_logged;
static BOOL host_remote_weak_logged;
static BOOL host_remote_weak_blocked_logged;
static BOOL host_remote_ailish_moving;
static BOOL host_auto_rehost_enabled;
static int16_t host_remote_direction_x;
static int16_t host_remote_direction_z;
static int16_t host_remote_aim_x;
static int16_t host_remote_aim_y;
static int16_t host_remote_aim_z;
static BOOL host_remote_weak_held;
static BOOL host_remote_first_person_active;
static BOOL host_remote_weak_cycle_pending;
static BOOL host_remote_weak_cycle_seen_active;
static BOOL host_remote_weak_cycle_timeout_logged;
static DWORD host_remote_weak_cycle_started_at_ms;
static DWORD host_remote_ranged_repeat_not_before_ms;
static SudekiMpCleanroomActorPresentation host_actor_presentation[2];
static BOOL host_actor_presentation_valid[2];
static DWORD host_actor_presentation_last_trace_at[2];
static SudekiMpCleanroomActorPresentation client_actor_presentation[2];
static BOOL client_actor_presentation_valid[2];
static DWORD client_actor_presentation_last_trace_at[2];
static DWORD client_replica_diagnostics_last_trace_at;
static uint32_t client_tal_timeline_action_sequence;
static uint32_t client_tal_timeline_last_render_host_tick;
static unsigned int client_tal_timeline_idle_samples_remaining;
static char lan_arena_control_observer_owner;
static SudekiMpControlUpdateObserverGate lan_arena_control_observer_gate;
static SudekiMpLanArenaSessionConfig runtime_config;
static char runtime_remote_ipv4[64];
static HMODULE runtime_game_module;
static HANDLE network_pump_stop_event;
static HANDLE network_pump_thread;

static DWORD WINAPI lan_arena_network_pump(void *context) {
    HANDLE stop_event = (HANDLE)context;
    SudekiMpLogWrite(
        "lan_arena_runtime event=network_pump phase=active "
        "policy=socket_only_no_game_memory_window_focus_independent\r\n");
    while (WaitForSingleObject(stop_event, 5u) == WAIT_TIMEOUT) {
        SudekiMpLanArenaSessionPoll(GetTickCount());
    }
    SudekiMpLogWrite("lan_arena_runtime event=network_pump phase=stopped\r\n");
    return 0u;
}

static BOOL start_network_pump(void) {
    if (network_pump_thread != NULL || network_pump_stop_event != NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    network_pump_stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (network_pump_stop_event == NULL) return FALSE;
    network_pump_thread = CreateThread(
        NULL, 0u, lan_arena_network_pump, network_pump_stop_event, 0u, NULL);
    if (network_pump_thread == NULL) {
        DWORD error = GetLastError();
        CloseHandle(network_pump_stop_event);
        network_pump_stop_event = NULL;
        SetLastError(error);
        return FALSE;
    }
    return TRUE;
}

static void stop_network_pump(void) {
    if (network_pump_stop_event != NULL) SetEvent(network_pump_stop_event);
    if (network_pump_thread != NULL) {
        WaitForSingleObject(network_pump_thread, INFINITE);
        CloseHandle(network_pump_thread);
    }
    if (network_pump_stop_event != NULL) CloseHandle(network_pump_stop_event);
    network_pump_thread = NULL;
    network_pump_stop_event = NULL;
}

static BOOL restore_lan_arena_frame_hooks(void) {
    DWORD first_error = ERROR_SUCCESS;
    if (!SudekiMpRestoreRelativeCallHook(
            &lan_arena_render_pre_world_hook)) {
        first_error = GetLastError();
    }
    if (!SudekiMpRestoreRelativeCallHook(&lan_arena_render_start_hook) &&
        first_error == ERROR_SUCCESS) {
        first_error = GetLastError();
    }
    if (!SudekiMpRestoreRelativeCallHook(&lan_arena_frame_end_hook) &&
        first_error == ERROR_SUCCESS) {
        first_error = GetLastError();
    }
    if (first_error != ERROR_SUCCESS) {
        SetLastError(first_error);
        return FALSE;
    }
    return TRUE;
}

static void retain_runtime_after_hook_restore_failure(DWORD error) {
    HMODULE pinned_module = NULL;
    BOOL pinned = GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpUninstallLanArenaRuntime,
        &pinned_module);
    if (!runtime_hook_restore_failed_logged) {
        runtime_hook_restore_failed_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_runtime event=frame_hook_restore state=quarantined "
            "win32_error=%lu module_pinned=%s "
            "policy=retain_callbacks_and_runtime_ownership_no_dangling_detour\r\n",
            (unsigned long)error,
            pinned ? "true" : "false");
    }
    SetLastError(error);
}

static BOOL rollback_lan_arena_frame_hooks(void) {
    DWORD restore_error;
    if (restore_lan_arena_frame_hooks()) return TRUE;
    restore_error = GetLastError();
    retain_runtime_after_hook_restore_failure(restore_error);
    return FALSE;
}

static uint32_t resource_snapshot_value(float value) {
    if (value <= 0.0f) return 0u;
    if (value >= (float)SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE) {
        return SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE;
    }
    return (uint32_t)value;
}

static BOOL fill_actor_snapshot(
    SudekiMpCleanroomActor actor,
    uint8_t actor_type,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    float position[3];
    float facing[2];
    float hit_points;
    float skill_points;
    if (snapshot == NULL ||
        !SudekiMpCleanroomEngineActorPosition(actor, position) ||
        !SudekiMpCleanroomEngineActorFacing(actor, facing) ||
        !SudekiMpCleanroomEngineActorResources(actor, &hit_points, &skill_points)) {
        return FALSE;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->actor_type = actor_type;
    /* Actor type is intentionally the stable cross-process identity. Native
     * pointers are process-local and must never enter LAN packets. */
    snapshot->native_entity_id = actor_type;
    snapshot->x = position[0];
    snapshot->y = position[1];
    snapshot->z = position[2];
    snapshot->facing_x = facing[0];
    snapshot->facing_z = facing[1];
    snapshot->hp = resource_snapshot_value(hit_points);
    snapshot->sp = resource_snapshot_value(skill_points);
    return TRUE;
}

enum {
    /* Host snapshots are emitted every 50 ms.  Require at least 0.005 world
     * units of horizontal displacement, then retain locomotion for three
     * snapshots so collision settling cannot flicker RUN/IDLE at a wall. */
    HOST_LOCOMOTION_STOP_GRACE_MS = 150u,
    /* Native Ailish idle variants cross-fade between animation channels 0
     * and 2 with short base-idle gaps between them. */
    AILISH_IDLE_VARIANT_CHANNEL_GRACE_MS = 250u,
    HOST_REMOTE_WEAK_START_TIMEOUT_MS = 1000u,
    /* Fail-safe used only if the selected weapon's exact authored half-float
     * delay is absent or malformed. The supported Ailish weapon reports 2.0
     * seconds (0x4000); never return to the old guessed 500 ms cadence. */
    HOST_REMOTE_RANGED_REPEAT_FALLBACK_MS = 2000u
};

BOOL SudekiMpLanArenaRangedRepeatReady(
    uint32_t now_ms,
    uint32_t not_before_ms
) {
    return not_before_ms == 0u ||
        (int32_t)(now_ms - not_before_ms) >= 0;
}

uint32_t SudekiMpLanArenaRangedRepeatIntervalMs(
    uint16_t encoded_half_seconds
) {
    uint32_t exponent = (encoded_half_seconds >> 10u) & 0x1fu;
    uint32_t mantissa = encoded_half_seconds & 0x03ffu;
    float seconds;
    if ((encoded_half_seconds & 0x8000u) != 0u || exponent == 0x1fu) {
        return HOST_REMOTE_RANGED_REPEAT_FALLBACK_MS;
    }
    if (exponent == 0u) {
        seconds = ldexpf((float)mantissa, -24);
    } else {
        seconds = ldexpf(1.0f + (float)mantissa / 1024.0f,
            (int)exponent - 15);
    }
    if (!isfinite(seconds) || seconds < 0.1f || seconds > 10.0f) {
        return HOST_REMOTE_RANGED_REPEAT_FALLBACK_MS;
    }
    return (uint32_t)(seconds * 1000.0f + 0.5f);
}

static uint8_t host_actor_idle_variant_state(
    unsigned int actor_index,
    DWORD now_ms
) {
    const SudekiMpCleanroomActorPresentation *presentation;
    int selector;
    uint8_t selector_state;
    if (actor_index >= 2u || !host_actor_presentation_valid[actor_index]) {
        return SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    }
    presentation = &host_actor_presentation[actor_index];
    if (actor_index == 0u) {
        if (presentation->selector[0] == TAL_WORLD_IDLE_VARIANT_ONE_SELECTOR) {
            return SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE;
        }
        if (presentation->selector[0] == TAL_WORLD_IDLE_VARIANT_TWO_SELECTOR) {
            return SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO;
        }
        return SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    }
    selector = presentation->selector[0];
    selector_state = presentation->state[0];
    if (selector != AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR &&
        selector != AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR) {
        selector = presentation->selector[2];
        selector_state = presentation->state[2];
    }
    if (selector == AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR ||
        selector == AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR) {
        uint8_t observed_variant = selector ==
                AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR ?
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE :
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO;
        /* Ailish keeps a retired idle variant on channel 2 underneath
         * locomotion. After movement, state 65 is the old hidden animation,
         * not a new request. Only a native state-1 entry may arm a fresh
         * variant; an already-latched variant may continue through state 65. */
        if (host_ailish_idle_variant_state == observed_variant ||
            (host_ailish_idle_variant_armed && selector_state == 1u)) {
            host_ailish_idle_variant_state = observed_variant;
            host_ailish_idle_variant_seen_at_ms = now_ms;
        }
    } else if (host_ailish_idle_variant_state != 0u &&
        (DWORD)(now_ms - host_ailish_idle_variant_seen_at_ms) >
            AILISH_IDLE_VARIANT_CHANNEL_GRACE_MS) {
        host_ailish_idle_variant_state = 0u;
        host_ailish_idle_variant_seen_at_ms = 0u;
    } else if (host_ailish_idle_variant_state == 0u &&
               !host_ailish_idle_variant_armed) {
        /* A clean base-idle witness separates a cancelled variant from the
         * next native state-1 start. */
        host_ailish_idle_variant_armed = TRUE;
    }
    return host_ailish_idle_variant_state != 0u ?
        host_ailish_idle_variant_state : SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
}

static BOOL host_tal_native_locomotion_state(
    BOOL combat_enabled,
    BOOL *moving
) {
    const SudekiMpCleanroomActorPresentation *presentation;
    int selector;
    if (moving == NULL || !host_actor_presentation_valid[0]) return FALSE;
    presentation = &host_actor_presentation[0];
    selector = presentation->selector[0];
    if (selector == (combat_enabled ? TAL_COMBAT_MOVE_PRIMARY_SELECTOR :
            TAL_WORLD_MOVE_PRIMARY_SELECTOR)) {
        *moving = TRUE;
        return TRUE;
    }
    if ((combat_enabled && selector == TAL_COMBAT_IDLE_SELECTOR) ||
        (!combat_enabled && (selector == TAL_WORLD_IDLE_SELECTOR ||
        selector == TAL_WORLD_IDLE_VARIANT_ONE_SELECTOR ||
        selector == TAL_WORLD_IDLE_VARIANT_TWO_SELECTOR))) {
        *moving = FALSE;
        return TRUE;
    }
    return FALSE;
}

static uint8_t host_actor_native_action_variant(
    unsigned int actor_index,
    BOOL combat_enabled
) {
    const SudekiMpCleanroomActorPresentation *presentation;
    int selector;
    uint8_t state;
    if (!combat_enabled || actor_index >= 2u ||
        !host_actor_presentation_valid[actor_index]) {
        return SUDEKIMP_LAN_ARENA_ACTION_NONE;
    }
    presentation = &host_actor_presentation[actor_index];
    if (actor_index == 0u) {
        uint8_t action_variant;
        selector = presentation->selector[0];
        state = presentation->state[0];
        if (SudekiMpLanArenaTalActionFromNativePresentation(
                selector, state, &action_variant)) return action_variant;
        /* State 128 is Tal's completed/terminal pose, but the action selector
         * is still the visible native clip until Sudeki replaces it with
         * combat idle. Preserve the already-admitted semantic action through
         * that terminal frame so the wire cannot retire before the host. */
        if (state == 128u && host_actor_previous_action_active[actor_index] &&
            SudekiMpLanArenaTalActionFromNativePresentation(
                selector, 1u, &action_variant) &&
            action_variant ==
                host_actor_previous_action_variant[actor_index]) {
            return action_variant;
        }
        /* Selector 3 is the native draw/enter-combat transition, not a weak
         * attack. Treating it as an action made Tal attack while merely
         * moving and interrupted the weapon attachment transaction. */
        return SUDEKIMP_LAN_ARENA_ACTION_NONE;
    }
    selector = presentation->selector[4];
    state = presentation->state[4];
    /* Ailish's authored ranged clip moves through state 0 while blending,
     * then 1/65 while playing. The selector remains 59 for the complete
     * native action. Treating state 0 as an action gap generated a new LAN
     * action edge every few snapshots and made a held shot restart rapidly. */
    return selector == AILISH_COMBAT_WEAK_SELECTOR && state != 192u ?
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE :
        SUDEKIMP_LAN_ARENA_ACTION_NONE;
}

static uint8_t combat_state_for_action(uint8_t action_variant) {
    uint8_t combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    (void)SudekiMpLanArenaTalActionCombatState(
        action_variant, &combat_state);
    return combat_state;
}

static uint8_t host_actor_native_action_state(unsigned int actor_index) {
    if (actor_index >= 2u || !host_actor_presentation_valid[actor_index]) {
        return 0u;
    }
    return actor_index == 0u ? host_actor_presentation[0].state[0] :
        host_actor_presentation[1].state[4];
}

static void host_advance_actor_action_sequence(
    unsigned int actor_index,
    uint8_t action_variant,
    DWORD now_ms
) {
    uint8_t count;
    SudekiMpLanArenaActionEvent *event;
    if (actor_index >= 2u ||
        action_variant == SUDEKIMP_LAN_ARENA_ACTION_NONE) return;
    ++host_actor_action_sequence[actor_index];
    if (host_actor_action_sequence[actor_index] == 0u) {
        host_actor_action_sequence[actor_index] = 1u;
    }
    ZeroMemory(&host_actor_action_retirement[actor_index],
        sizeof(host_actor_action_retirement[actor_index]));
    count = host_actor_action_history_count[actor_index];
    if (count == SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY) {
        memmove(&host_actor_action_history[actor_index][0],
            &host_actor_action_history[actor_index][1],
            (SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY - 1u) *
                sizeof(host_actor_action_history[actor_index][0]));
        --count;
    }
    event = &host_actor_action_history[actor_index][count++];
    event->sequence = host_actor_action_sequence[actor_index];
    event->variant = action_variant;
    event->host_tick = now_ms;
    host_actor_action_history_count[actor_index] = count;
    host_actor_previous_action_active[actor_index] = TRUE;
    host_actor_previous_action_variant[actor_index] = action_variant;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_action_sequence actor=%s "
        "sequence=%u variant=%u host_tick=%lu "
        "policy=native_presentation_edge\r\n",
        actor_index == 0u ? "Tal" : "Ailish",
        (unsigned int)host_actor_action_sequence[actor_index],
        (unsigned int)action_variant,
        (unsigned long)now_ms);
    if (actor_index == 0u) {
        SudekiMpLanArenaHostInputNotifyNativeActionObserved();
    }
}

static void host_track_actor_action_sequence(
    unsigned int actor_index,
    DWORD now_ms,
    BOOL action_active,
    BOOL native_action_active,
    uint8_t action_variant,
    uint8_t native_action_state,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    BOOL new_action = FALSE;
    if (actor_index >= 2u || snapshot == NULL) return;
    if (action_active) {
        new_action = !host_actor_previous_action_active[actor_index] ||
            action_variant != host_actor_previous_action_variant[actor_index];
        if (new_action) {
            host_advance_actor_action_sequence(
                actor_index, action_variant, now_ms);
        }
        host_actor_previous_action_active[actor_index] = TRUE;
        host_actor_previous_action_variant[actor_index] = action_variant;
    } else {
        host_actor_previous_action_active[actor_index] = FALSE;
        host_actor_previous_action_variant[actor_index] =
            SUDEKIMP_LAN_ARENA_ACTION_NONE;
    }
    host_actor_previous_native_action_active[actor_index] =
        native_action_active;
    host_actor_previous_native_action_state[actor_index] =
        native_action_active ? native_action_state : 0u;
    snapshot->action_sequence = host_actor_action_sequence[actor_index];
    snapshot->action_history_count =
        host_actor_action_history_count[actor_index];
    memcpy(snapshot->action_history,
        host_actor_action_history[actor_index],
        sizeof(snapshot->action_history));
}

static void reset_host_action_tracking(void) {
    ZeroMemory(host_actor_action_variant,
        sizeof(host_actor_action_variant));
    ZeroMemory(host_actor_action_sequence,
        sizeof(host_actor_action_sequence));
    ZeroMemory(host_actor_action_history,
        sizeof(host_actor_action_history));
    ZeroMemory(host_actor_action_history_count,
        sizeof(host_actor_action_history_count));
    ZeroMemory(host_actor_previous_action_variant,
        sizeof(host_actor_previous_action_variant));
    ZeroMemory(host_actor_previous_native_action_state,
        sizeof(host_actor_previous_native_action_state));
    ZeroMemory(host_actor_previous_action_active,
        sizeof(host_actor_previous_action_active));
    ZeroMemory(host_actor_previous_native_action_active,
        sizeof(host_actor_previous_native_action_active));
    ZeroMemory(host_actor_action_retirement,
        sizeof(host_actor_action_retirement));
}

static BOOL encode_animation_phase(float phase_time, uint16_t *encoded) {
    if (encoded == NULL || !isfinite(phase_time) || phase_time < 0.0f ||
        phase_time > 65535.0f / SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE) {
        return FALSE;
    }
    *encoded = (uint16_t)(phase_time *
        SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE + 0.5f);
    return TRUE;
}

static void host_capture_actor_action_retirement(
    unsigned int actor_index,
    const SudekiMpCleanroomActorPresentation *previous,
    const SudekiMpCleanroomActorPresentation *current
) {
    HostActorActionRetirement retirement;
    uint8_t previous_variant;
    if (actor_index != 0u || previous == NULL || current == NULL ||
        host_actor_action_sequence[actor_index] == 0u ||
        current->selector[0] != TAL_COMBAT_IDLE_SELECTOR ||
        !SudekiMpLanArenaTalActionFromNativePresentation(
            previous->selector[0], 1u, &previous_variant)) {
        return;
    }
    ZeroMemory(&retirement, sizeof(retirement));
    if (!encode_animation_phase(
            previous->time[0], &retirement.action_terminal_phase_q8) ||
        !encode_animation_phase(
            current->time[0], &retirement.idle_entry_phase_q8)) {
        return;
    }
    retirement.action_sequence = host_actor_action_sequence[actor_index];
    retirement.action_terminal_selector = previous->selector[0];
    retirement.idle_entry_selector = current->selector[0];
    retirement.action_terminal_state = previous->state[0];
    retirement.idle_entry_state = current->state[0];
    retirement.pending = TRUE;
    host_actor_action_retirement[actor_index] = retirement;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_action_retirement actor=Tal "
        "action_sequence=%u terminal_phase_q8=%u idle_entry_phase_q8=%u "
        "terminal_selector=%ld terminal_state=%u "
        "idle_selector=%ld idle_state=%u "
        "policy=host_observed_terminal_and_idle_handoff\r\n",
        (unsigned int)retirement.action_sequence,
        (unsigned int)retirement.action_terminal_phase_q8,
        (unsigned int)retirement.idle_entry_phase_q8,
        (long)retirement.action_terminal_selector,
        (unsigned int)retirement.action_terminal_state,
        (long)retirement.idle_entry_selector,
        (unsigned int)retirement.idle_entry_state);
}

static void reset_host_remote_weak_cycle(void) {
    host_remote_weak_cycle_pending = FALSE;
    host_remote_weak_cycle_seen_active = FALSE;
    host_remote_weak_cycle_timeout_logged = FALSE;
    host_remote_weak_cycle_started_at_ms = 0u;
}

static BOOL host_actor_translation_moving(
    unsigned int actor_index,
    DWORD now_ms,
    const SudekiMpLanArenaActorSnapshot *snapshot
) {
    float dx;
    float dz;
    if (snapshot == NULL || actor_index >= 2u) return FALSE;
    if (host_previous_actor_position_valid[actor_index]) {
        dx = snapshot->x - host_previous_actor_position[actor_index][0];
        dz = snapshot->z - host_previous_actor_position[actor_index][2];
        if (dx * dx + dz * dz > 0.000025f) {
            host_actor_last_translation_at_ms[actor_index] = now_ms;
        }
    }
    /* Slow authoritative movement can advance less than 0.005 units during
     * every 50 ms snapshot while accumulating a large distance overall. The
     * old per-snapshot-only test kept the replica latched for meters, then
     * released that entire backlog as one teleport. Also measure from the
     * last position actually exposed to the client while stationary. */
    if (host_replica_idle_position_valid[actor_index] &&
        !host_actor_was_moving[actor_index]) {
        dx = snapshot->x - host_replica_idle_position[actor_index][0];
        dz = snapshot->z - host_replica_idle_position[actor_index][1];
        if (dx * dx + dz * dz > 0.000025f) {
            host_actor_last_translation_at_ms[actor_index] = now_ms;
        }
    }
    host_previous_actor_position[actor_index][0] = snapshot->x;
    host_previous_actor_position[actor_index][1] = snapshot->y;
    host_previous_actor_position[actor_index][2] = snapshot->z;
    host_previous_actor_position_valid[actor_index] = TRUE;
    return host_actor_last_translation_at_ms[actor_index] != 0u &&
        (DWORD)(now_ms - host_actor_last_translation_at_ms[actor_index]) <=
            HOST_LOCOMOTION_STOP_GRACE_MS;
}

static void host_filter_stationary_replica_position(
    unsigned int actor_index,
    BOOL moving,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    if (snapshot == NULL || actor_index >= 2u) return;
    /* Native animation/collision settling can continue to alter X/Z by a few
     * thousandths after gameplay movement has stopped. Sending every one of
     * those changes makes a kinematic client replica rock forward and back.
     * Capture the final authoritative stop point once, then hold it until
     * host translation becomes real movement again. */
    if (moving || !host_replica_idle_position_valid[actor_index] ||
        host_actor_was_moving[actor_index]) {
        host_replica_idle_position[actor_index][0] = snapshot->x;
        host_replica_idle_position[actor_index][1] = snapshot->z;
        host_replica_idle_position_valid[actor_index] = TRUE;
    } else {
        snapshot->x = host_replica_idle_position[actor_index][0];
        snapshot->z = host_replica_idle_position[actor_index][1];
    }
    host_actor_was_moving[actor_index] = moving;
}

static void host_apply_presentation_state(
    unsigned int actor_index,
    DWORD now_ms,
    BOOL combat_enabled,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    BOOL moving;
    BOOL native_tal_moving;
    BOOL action_active;
    uint8_t native_action_variant;
    uint8_t native_action_state;
    uint8_t action_variant;
    uint8_t idle_variant_state;
    if (snapshot == NULL || actor_index >= 2u) return;
    /* Locomotion is an authoritative presentation fact, not an input fact.
     * A held client stick can keep requesting movement forever against a
     * wall, while Sudeki's host collision has already stopped the actor.
     * Likewise native selector retirement can trail Tal's visible stop.
     * Horizontal host translation plus a bounded grace gives both replicas
     * the same deterministic stop edge. */
    moving = host_actor_translation_moving(actor_index, now_ms, snapshot);
    /* Tal's native idle and idle-variant animation can carry small root
     * translation after the player releases movement. Translation-only
     * classification therefore kept the remote Tal running until that native
     * animation settled. When the verified host renderer exposes a known Tal
     * locomotion selector, mirror that visible state immediately; retain the
     * collision-safe translation classifier for unrecognized action states. */
    if (actor_index == 0u &&
        host_tal_native_locomotion_state(
            combat_enabled, &native_tal_moving)) {
        moving = native_tal_moving;
    }
    /* Ailish's native idle variants can contain a short root-motion lunge.
     * It changes the host CPosition enough to look like movement, then returns
     * to the idle origin. Do not transmit that as player locomotion unless an
     * authenticated client movement direction is actually active. Collision
     * still decides whether held input produces host translation. */
    if (actor_index == 1u && !host_remote_ailish_moving) {
        moving = FALSE;
    }
    native_action_variant = host_actor_native_action_variant(
        actor_index, combat_enabled);
    native_action_state = host_actor_native_action_state(actor_index);
    if (native_action_variant != SUDEKIMP_LAN_ARENA_ACTION_NONE) {
        host_actor_action_variant[actor_index] = native_action_variant;
    }
    action_active = native_action_variant != SUDEKIMP_LAN_ARENA_ACTION_NONE ||
        (actor_index == 1u &&
            (LONG)(host_ailish_weak_attack_until_ms - now_ms) > 0);
    action_variant = action_active ? host_actor_action_variant[actor_index] :
        SUDEKIMP_LAN_ARENA_ACTION_NONE;
    if (action_active && action_variant == SUDEKIMP_LAN_ARENA_ACTION_NONE) {
        action_variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
        host_actor_action_variant[actor_index] = action_variant;
    } else if (!action_active) {
        host_actor_action_variant[actor_index] =
            SUDEKIMP_LAN_ARENA_ACTION_NONE;
    }
    if (actor_index == 1u &&
        (moving || snapshot->hp == 0u || action_active || combat_enabled)) {
        host_ailish_idle_variant_state = 0u;
        host_ailish_idle_variant_seen_at_ms = 0u;
        host_ailish_idle_variant_armed = FALSE;
    }
    host_filter_stationary_replica_position(
        actor_index, moving || action_active, snapshot);
    if (snapshot->hp == 0u) {
        snapshot->action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
        snapshot->animation_state =
            SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED;
        snapshot->combat_state =
            SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED;
    } else if (action_active) {
        snapshot->animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
        snapshot->combat_state = combat_state_for_action(action_variant);
        snapshot->action_variant = action_variant;
    } else if (moving) {
        snapshot->action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
        snapshot->animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
        snapshot->combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    } else if (!combat_enabled &&
               (idle_variant_state = host_actor_idle_variant_state(
                    actor_index, now_ms)) !=
               SUDEKIMP_LAN_ARENA_ANIMATION_IDLE) {
        snapshot->animation_state = idle_variant_state;
        snapshot->combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
        snapshot->action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    } else {
        snapshot->animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
        snapshot->combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
        snapshot->action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    }
    host_track_actor_action_sequence(
        actor_index,
        now_ms,
        snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
        native_action_variant != SUDEKIMP_LAN_ARENA_ACTION_NONE,
        snapshot->action_variant,
        native_action_state,
        snapshot);
    snapshot->action_phase_valid = 0u;
    snapshot->action_phase_q8 = 0u;
    if (snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        native_action_variant == snapshot->action_variant &&
        host_actor_presentation_valid[actor_index]) {
        float phase_time = actor_index == 0u ?
            host_actor_presentation[0].time[0] :
            host_actor_presentation[1].time[4];
        if (encode_animation_phase(
                phase_time, &snapshot->action_phase_q8)) {
            snapshot->action_phase_valid = 1u;
        }
    }
    snapshot->action_terminal_phase_q8 = 0u;
    snapshot->idle_entry_phase_q8 = 0u;
    snapshot->action_retirement_valid = 0u;
    if (snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
        host_actor_action_retirement[actor_index].pending &&
        host_actor_action_retirement[actor_index].action_sequence ==
            snapshot->action_sequence) {
        snapshot->action_terminal_phase_q8 =
            host_actor_action_retirement[actor_index].action_terminal_phase_q8;
        snapshot->idle_entry_phase_q8 =
            host_actor_action_retirement[actor_index].idle_entry_phase_q8;
        snapshot->action_retirement_valid = 1u;
    }
}

static void host_trace_native_presentation(
    unsigned int actor_index,
    SudekiMpCleanroomActor actor
) {
    SudekiMpCleanroomActorPresentation current;
    DWORD now = GetTickCount();
    DWORD trace_interval_ms;
    if (actor_index >= 2u ||
        !SudekiMpCleanroomEngineActorPresentation(actor, &current)) return;
    if (host_actor_presentation_valid[actor_index] &&
        memcmp(&host_actor_presentation[actor_index],
            &current, sizeof(current)) == 0) return;
    if (host_actor_presentation_valid[actor_index]) {
        host_capture_actor_action_retirement(
            actor_index, &host_actor_presentation[actor_index], &current);
    }
    host_actor_presentation[actor_index] = current;
    host_actor_presentation_valid[actor_index] = TRUE;
    trace_interval_ms =
        SudekiMpLanArenaCollisionDebugMode() !=
            SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF ||
        SudekiMpLanArenaHostInputDiagnosticTraceActive() ? 50u : 1000u;
    if (host_actor_presentation_last_trace_at[actor_index] != 0u &&
        (DWORD)(now - host_actor_presentation_last_trace_at[actor_index]) <
            trace_interval_ms) return;
    host_actor_presentation_last_trace_at[actor_index] = now;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_native_presentation actor=%s "
        "submodels=%lu selectors=%ld,%ld,%ld,%ld,%ld "
        "states=%u,%u,%u,%u,%u "
        "rates=%.5f,%.5f,%.5f,%.5f,%.5f "
        "times=%.5f,%.5f,%.5f,%.5f,%.5f "
        "blends=%.5f,%.5f,%.5f,%.5f "
        "policy=read_only_semantic_mapping_capture_not_network_payload\r\n",
        SudekiMpCleanroomActorLabel(actor),
        (unsigned long)current.submodel_count,
        (long)current.selector[0], (long)current.selector[1],
        (long)current.selector[2], (long)current.selector[3],
        (long)current.selector[4],
        current.state[0], current.state[1], current.state[2],
        current.state[3], current.state[4],
        current.rate[0], current.rate[1], current.rate[2],
        current.rate[3], current.rate[4],
        current.time[0], current.time[1], current.time[2],
        current.time[3], current.time[4],
        current.blend[0], current.blend[1],
        current.blend[2], current.blend[3]
    );
}

static void client_trace_native_presentation(
    unsigned int actor_index,
    SudekiMpCleanroomActor actor
) {
    SudekiMpCleanroomActorPresentation current;
    DWORD now = GetTickCount();
    DWORD trace_interval_ms;
    if (actor_index >= 2u ||
        !SudekiMpCleanroomEngineActorPresentation(actor, &current)) return;
    if (client_actor_presentation_valid[actor_index] &&
        memcmp(&client_actor_presentation[actor_index],
            &current, sizeof(current)) == 0) return;
    client_actor_presentation[actor_index] = current;
    client_actor_presentation_valid[actor_index] = TRUE;
    trace_interval_ms = SudekiMpLanArenaCollisionDebugMode() ==
            SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF ? 1000u : 100u;
    if (client_actor_presentation_last_trace_at[actor_index] != 0u &&
        (DWORD)(now - client_actor_presentation_last_trace_at[actor_index]) <
            trace_interval_ms) return;
    client_actor_presentation_last_trace_at[actor_index] = now;
    SudekiMpLogFormat(
        "lan_arena_runtime event=client_native_presentation actor=%s "
        "submodels=%lu selectors=%ld,%ld,%ld,%ld,%ld "
        "states=%u,%u,%u,%u,%u "
        "rates=%.5f,%.5f,%.5f,%.5f,%.5f "
        "times=%.5f,%.5f,%.5f,%.5f,%.5f "
        "blends=%.5f,%.5f,%.5f,%.5f "
        "policy=read_only_replica_blend_validation\r\n",
        SudekiMpCleanroomActorLabel(actor),
        (unsigned long)current.submodel_count,
        (long)current.selector[0], (long)current.selector[1],
        (long)current.selector[2], (long)current.selector[3],
        (long)current.selector[4],
        current.state[0], current.state[1], current.state[2],
        current.state[3], current.state[4],
        current.rate[0], current.rate[1], current.rate[2],
        current.rate[3], current.rate[4],
        current.time[0], current.time[1], current.time[2],
        current.time[3], current.time[4],
        current.blend[0], current.blend[1],
        current.blend[2], current.blend[3]
    );
}

static void reset_client_tal_action_timeline(void) {
    client_tal_timeline_action_sequence = 0u;
    client_tal_timeline_last_render_host_tick = 0u;
    client_tal_timeline_idle_samples_remaining = 0u;
}

static void client_trace_tal_action_timeline(void) {
    SudekiMpLanArenaReplicaDiagnostics diagnostics;
    SudekiMpCleanroomActorPresentation presentation;
    BOOL action_active;
    BOOL retirement_tail;
    if (!SudekiMpLanArenaClientReplicaGetDiagnostics(&diagnostics) ||
        !SudekiMpCleanroomEngineActorPresentation(
            SUDEKIMP_CLEANROOM_TAL, &presentation)) return;
    action_active = diagnostics.tal_animation_state ==
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    if (action_active && diagnostics.tal_action_sequence !=
            client_tal_timeline_action_sequence) {
        client_tal_timeline_action_sequence =
            diagnostics.tal_action_sequence;
        client_tal_timeline_idle_samples_remaining = 8u;
    }
    retirement_tail = !action_active &&
        diagnostics.tal_action_retirement_valid != 0u &&
        diagnostics.tal_action_sequence ==
            client_tal_timeline_action_sequence &&
        client_tal_timeline_idle_samples_remaining != 0u;
    if ((!action_active && !retirement_tail) ||
        diagnostics.render_host_tick ==
            client_tal_timeline_last_render_host_tick) return;
    client_tal_timeline_last_render_host_tick =
        diagnostics.render_host_tick;
    if (retirement_tail) {
        --client_tal_timeline_idle_samples_remaining;
    }
    SudekiMpLogFormat(
        "lan_arena_runtime event=client_tal_action_timeline "
        "local_tick=%lu render_host_tick=%lu upper_host_tick=%lu "
        "local_elapsed_ms=%lu render_advance_ms=%lu clock_protected=%u "
        "animation=%u action_sequence=%lu variant=%u "
        "sample_phase_q8=%u phase_valid=%u retirement=%u "
        "terminal_phase_q8=%u idle_entry_phase_q8=%u "
        "native_selector=%ld native_state=%u native_time=%.5f "
        "native_rate=%.5f boundary=post_second_render_start_pre_world "
        "policy=bounded_action_sample_to_native_phase_witness\r\n",
        (unsigned long)diagnostics.sampled_at_ms,
        (unsigned long)diagnostics.render_host_tick,
        (unsigned long)diagnostics.upper_snapshot_host_tick,
        (unsigned long)diagnostics.render_clock_local_elapsed_ms,
        (unsigned long)diagnostics.render_clock_advance_ms,
        (unsigned int)diagnostics.action_clock_protected,
        (unsigned int)diagnostics.tal_animation_state,
        (unsigned long)diagnostics.tal_action_sequence,
        (unsigned int)diagnostics.tal_action_variant,
        (unsigned int)diagnostics.tal_action_phase_q8,
        (unsigned int)diagnostics.tal_action_phase_valid,
        (unsigned int)diagnostics.tal_action_retirement_valid,
        (unsigned int)diagnostics.tal_action_terminal_phase_q8,
        (unsigned int)diagnostics.tal_idle_entry_phase_q8,
        (long)presentation.selector[0],
        (unsigned int)presentation.state[0],
        presentation.time[0], presentation.rate[0]);
}

static void host_capture_native_action_edges(DWORD now_ms) {
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaActorSnapshot scratch;
    BOOL combat_enabled = FALSE;
    unsigned int actor_index;
    if (runtime_config.local_role !=
            SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        !SudekiMpLanArenaSessionGetStatus(&status) ||
        !status.peer_connected ||
        status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        status.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status.peer_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA ||
        !SudekiMpCleanroomEngineCombatMode(&combat_enabled)) return;
    if (tal_initialized) {
        host_trace_native_presentation(0u, SUDEKIMP_CLEANROOM_TAL);
    }
    if (ailish_initialized) {
        host_trace_native_presentation(1u, SUDEKIMP_CLEANROOM_AILISH);
    }
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        uint8_t variant = host_actor_native_action_variant(
            actor_index, combat_enabled);
        uint8_t state = host_actor_native_action_state(actor_index);
        BOOL synthetic_action_active = actor_index == 0u ? FALSE :
            (LONG)(host_ailish_weak_attack_until_ms - now_ms) > 0;
        BOOL action_active =
            variant != SUDEKIMP_LAN_ARENA_ACTION_NONE ||
            synthetic_action_active;
        if (synthetic_action_active &&
            variant == SUDEKIMP_LAN_ARENA_ACTION_NONE) {
            variant = host_actor_action_variant[actor_index];
            if (variant == SUDEKIMP_LAN_ARENA_ACTION_NONE) {
                variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
            }
        }
        ZeroMemory(&scratch, sizeof(scratch));
        host_track_actor_action_sequence(
            actor_index, now_ms,
            action_active,
            host_actor_native_action_variant(
                actor_index, combat_enabled) !=
                SUDEKIMP_LAN_ARENA_ACTION_NONE,
            variant, state, &scratch);
    }
}

static BOOL ensure_canonical_simulation(uint64_t session_token) {
    return SudekiMpLanArenaSharedSimulationSessionExact(
            &canonical_simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
            session_token) ||
        SudekiMpLanArenaSharedSimulationBegin(
            &canonical_simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
            session_token);
}

static void host_publish_snapshot(DWORD now_ms) {
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaSnapshot snapshot;
    SudekiMpLanArenaNativeWorldObservation world_observation;
    SudekiMpLanArenaActorObservation tal_observation;
    SudekiMpLanArenaActorObservation ailish_observation;
    BOOL combat_enabled = FALSE;

    if (host_last_snapshot_at_ms != 0u &&
        (DWORD)(now_ms - host_last_snapshot_at_ms) <
            SUDEKIMP_LAN_ARENA_SNAPSHOT_INTERVAL_MS) return;
    if (!SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
        status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        status.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status.peer_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) return;
    if (!SudekiMpCleanroomEngineCombatMode(&combat_enabled)) return;
    memset(&snapshot, 0, sizeof(snapshot));
    if (!fill_actor_snapshot(
            SUDEKIMP_CLEANROOM_TAL, SUDEKIMP_LAN_ARENA_TAL_TYPE,
            &snapshot.tal) ||
        !fill_actor_snapshot(
            SUDEKIMP_CLEANROOM_AILISH, SUDEKIMP_LAN_ARENA_AILISH_TYPE,
            &snapshot.ailish)) {
        return;
    }
    if (tal_initialized) {
        host_trace_native_presentation(0u, SUDEKIMP_CLEANROOM_TAL);
    }
    if (ailish_initialized) {
        host_trace_native_presentation(1u, SUDEKIMP_CLEANROOM_AILISH);
    }
    host_apply_presentation_state(
        0u, now_ms, combat_enabled, &snapshot.tal);
    host_apply_presentation_state(
        1u, now_ms, combat_enabled, &snapshot.ailish);
    snapshot.host_tick = now_ms;
    snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    snapshot.combat_enabled = combat_enabled ? 1u : 0u;
    {
        float dummy_position[3];
        float dummy_hp;
        if (SudekiMpCleanroomEngineDummySnapshot(
                dummy_position, &dummy_hp)) {
            snapshot.enemy_count = 1u;
            snapshot.enemies[0].native_entity_id =
                SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
            snapshot.enemies[0].x = dummy_position[0];
            snapshot.enemies[0].y = dummy_position[1];
            snapshot.enemies[0].z = dummy_position[2];
            snapshot.enemies[0].hp = resource_snapshot_value(dummy_hp);
            snapshot.enemies[0].combat_state = dummy_hp <= 0.0f ?
                SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED :
                SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
        }
    }
    if (!ensure_canonical_simulation(status.session_token)) return;
    ZeroMemory(&world_observation, sizeof(world_observation));
    world_observation.host_tick = now_ms;
    world_observation.tal_hp = snapshot.tal.hp;
    world_observation.tal_sp = snapshot.tal.sp;
    world_observation.ailish_hp = snapshot.ailish.hp;
    world_observation.ailish_sp = snapshot.ailish.sp;
    world_observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    world_observation.combat_enabled = combat_enabled ? 1u : 0u;
    world_observation.enemy_count = snapshot.enemy_count;
    memcpy(world_observation.enemies, snapshot.enemies,
        sizeof(world_observation.enemies));
    world_observation.native_combat_observed = 1u;
    world_observation.native_resources_observed = 1u;
    world_observation.native_enemies_observed = 1u;
    ZeroMemory(&tal_observation, sizeof(tal_observation));
    ZeroMemory(&ailish_observation, sizeof(ailish_observation));
    tal_observation.actor = snapshot.tal;
    tal_observation.native_actor_observed = 1u;
    ailish_observation.actor = snapshot.ailish;
    ailish_observation.native_actor_observed = 1u;
    if (!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
            &canonical_simulation, status.session_token,
            &world_observation, &tal_observation, &ailish_observation) ||
        !SudekiMpLanArenaSharedSimulationReadFrame(
            &canonical_simulation, &snapshot, NULL)) {
        return;
    }
    if (SudekiMpLanArenaSessionSendSnapshot(&snapshot)) {
        host_last_snapshot_at_ms = now_ms;
        /* Keep the most recent retirement handoff latched through the idle
         * interval. A render stall or an overwritten UDP snapshot must not
         * make the replica miss the only ACTION -> IDLE clock transition.
         * The next admitted action clears this record, and the client
         * consumes it only when its presentation lease crosses that exact
         * sequence boundary. */
        if (!host_snapshot_stream_logged) {
            host_snapshot_stream_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_snapshot_stream phase=active "
                "cadence_ms=50 actors=Tal,Ailish enemies=%u "
                "policy=canonical_simulation_native_world_observation_"
                "and_admitted_player_input_ack_replicated\r\n",
                (unsigned int)snapshot.enemy_count);
        }
    }
}

static BOOL release_host_remote_ailish(const char *reason) {
    if (!host_remote_ailish_owned) return TRUE;
    if (!SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(FALSE) ||
        !SudekiMpControlSeparationReleasePlayerTwoNow()) {
        if (!host_release_pending_logged) {
            host_release_pending_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_ailish_release phase=pending "
                "reason=%s policy=retain_owned_lease_and_retry_exact_native_restore\r\n",
                reason == NULL ? "session_inactive" : reason);
        }
        return FALSE;
    }
    host_remote_ailish_owned = FALSE;
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_weak_blocked_logged = FALSE;
    host_remote_ailish_moving = FALSE;
    host_remote_direction_x = 0;
    host_remote_direction_z = 0;
    host_remote_aim_x = 0;
    host_remote_aim_y = 0;
    host_remote_aim_z = 0;
    host_remote_weak_held = FALSE;
    host_remote_first_person_active = FALSE;
    host_remote_ranged_repeat_not_before_ms = 0u;
    reset_host_remote_weak_cycle();
    host_release_pending_logged = FALSE;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_ailish_release phase=confirmed reason=%s "
        "policy=stop_input_then_native_ai_restore\r\n",
        reason == NULL ? "session_inactive" : reason);
    return TRUE;
}

static BOOL release_client_remote_tal(const char *reason) {
    if (client_remote_tal_owned) {
        void *tal = SudekiMpCleanroomEngineActorEntity(
            SUDEKIMP_CLEANROOM_TAL);
        if (tal != NULL) {
            (void)SudekiMpControlSeparationForceStopCharacter(tal);
        }
        if (!SudekiMpControlSeparationReleasePlayerTwoNow()) {
            if (!client_release_pending_logged) {
                client_release_pending_logged = TRUE;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=client_tal_replica_release phase=pending "
                    "reason=%s policy=retain_owned_lease_and_retry_before_actor_remove\r\n",
                    reason == NULL ? "session_inactive" : reason);
            }
            return FALSE;
        }
        client_remote_tal_owned = FALSE;
        client_replica_stop_state = -1;
        client_release_pending_logged = FALSE;
        SudekiMpLogFormat(
            "lan_arena_runtime event=client_tal_replica_release phase=confirmed reason=%s "
            "policy=native_ai_restore_before_replica_discard\r\n",
            reason == NULL ? "session_inactive" : reason);
    }
    if (client_tal_spawn_attempted &&
        SudekiMpCleanroomEngineActorPresent(SUDEKIMP_CLEANROOM_TAL)) {
        if (!SudekiMpCleanroomEngineRemoveActor(SUDEKIMP_CLEANROOM_TAL)) {
            return FALSE;
        }
    }
    client_tal_spawn_attempted = FALSE;
    ZeroMemory(client_actor_presentation_valid,
        sizeof(client_actor_presentation_valid));
    return TRUE;
}

static BOOL release_arena_dummy(void) {
    if (arena_dummy_spawn_attempted &&
        SudekiMpCleanroomEngineDummyPresent()) {
        if (!SudekiMpCleanroomEngineRemoveDummy()) {
            if (!dummy_release_pending_logged) {
                dummy_release_pending_logged = TRUE;
                SudekiMpLogWrite(
                    "lan_arena_runtime event=training_dummy_release phase=pending "
                    "policy=retain_identity_and_retry_exact_remove\r\n");
            }
            return FALSE;
        }
    }
    arena_dummy_spawn_attempted = FALSE;
    dummy_release_pending_logged = FALSE;
    return TRUE;
}

static void lan_arena_control_update_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    SudekiMpLanArenaSessionStatus status;
    void *ailish;
    SudekiMpLanArenaInput input;
    BOOL remote_weak_requested = FALSE;
    BOOL remote_weak_allowed = FALSE;
    BOOL remote_weak_active = FALSE;
    BOOL remote_native_weak_active = FALSE;
    BOOL remote_aim_valid = FALSE;
    BOOL remote_combat_toggle_requested = FALSE;
    BOOL ranged_native_ready = TRUE;
    BOOL ranged_native_ready_known = FALSE;
    uint16_t ranged_authored_delay_half = 0u;
    uint32_t ranged_repeat_interval_ms =
        HOST_REMOTE_RANGED_REPEAT_FALLBACK_MS;
    BOOL witness_exact;
    BOOL status_available;
    DWORD now_ms;
    (void)controller;
    (void)update_data;
    if (!SudekiMpControlUpdateObserverGateTryEnter(
            &lan_arena_control_observer_gate)) return;
    SudekiMpLanArenaHostInputServiceCombatToggle();
    witness_exact =
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness);
    status_available = SudekiMpLanArenaSessionGetStatus(&status);
    if (!witness_exact || !status_available || !status.peer_connected ||
        !((status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
           status.local_simulation_node_role ==
               SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD &&
           status.peer_simulation_node_role ==
               SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) ||
          (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
           status.local_simulation_node_role ==
               SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA &&
           status.peer_simulation_node_role ==
               SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD))) {
        BOOL actors_released =
            release_host_remote_ailish("session_not_authenticated") &&
            release_client_remote_tal("session_not_authenticated");
        BOOL dummy_released = release_arena_dummy();
        if (runtime_config.local_role ==
                SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
            SudekiMpLanArenaClientReplicaDiscardSnapshots();
            client_replica_stop_state = -1;
            if (!client_replica_discard_logged) {
                client_replica_discard_logged = TRUE;
                SudekiMpLogWrite(
                    "lan_arena_runtime event=client_snapshot_replica phase=discarded "
                    "reason=transport_authority_inactive "
                    "policy=no_stale_snapshot_sampling\r\n");
            }
        }
        /* A remote END or timeout is a transport interruption, not a request
         * to shut down the host.  Once game-thread actor cleanup is exact,
         * reopen the authoritative socket with a fresh session token so the
         * client can Join again without visiting the host menu.  A local End
         * Session explicitly clears host_auto_rehost_enabled below. */
        if (witness_exact && status_available && actors_released &&
            dummy_released && host_auto_rehost_enabled &&
            runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
            (status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_ENDED ||
             status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT ||
             status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED)) {
            if (SudekiMpLanArenaRuntimeHostArena()) {
                SudekiMpLogWrite(
                    "lan_arena_runtime event=host_auto_rehost state=hosting "
                    "reason=remote_session_ended policy=fresh_token\r\n");
            } else {
                SudekiMpLogFormat(
                    "lan_arena_runtime event=host_auto_rehost state=rejected "
                    "error=%lu\r\n", (unsigned long)GetLastError());
            }
        }
        SudekiMpControlUpdateObserverGateLeave(&lan_arena_control_observer_gate);
        return;
    }
    if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        void *tal;
        void *ailish_actor;
        float ailish_position[3];
        float tal_position[3];
        client_replica_discard_logged = FALSE;
        release_host_remote_ailish("client_role");
        SudekiMpLanArenaClientInputService();
        tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
        ailish_actor = SudekiMpCleanroomEngineActorEntity(
            SUDEKIMP_CLEANROOM_AILISH);
        if (tal == NULL) {
            if (client_remote_tal_owned) {
                release_client_remote_tal("client_tal_lost");
            }
            if (!client_tal_spawn_attempted && ailish_actor != NULL &&
                SudekiMpCleanroomEngineActorPosition(
                    SUDEKIMP_CLEANROOM_AILISH, ailish_position)) {
                tal_position[0] = ailish_position[0] - 1.5f;
                tal_position[1] = ailish_position[1];
                tal_position[2] = ailish_position[2] + 1.5f;
                client_tal_spawn_attempted =
                    SudekiMpCleanroomEngineSpawnActor(
                        SUDEKIMP_CLEANROOM_TAL, tal_position);
                SudekiMpLogFormat(
                    "lan_arena_runtime event=client_tal_replica_spawn status=%s "
                    "policy=authenticated_then_wait_for_native_group_and_ai_lease\r\n",
                    client_tal_spawn_attempted ? "requested" : "rejected");
            }
            SudekiMpControlUpdateObserverGateLeave(
                &lan_arena_control_observer_gate);
            return;
        }
        if (!SudekiMpControlSeparationPlayerTwoActive() ||
            SudekiMpControlSeparationPlayerTwoCharacter() != tal) {
            (void)SudekiMpControlSeparationRequestPlayerTwoCharacter(tal);
            SudekiMpControlUpdateObserverGateLeave(
                &lan_arena_control_observer_gate);
            return;
        }
        if (!client_remote_tal_owned) {
            client_remote_tal_owned = TRUE;
            SudekiMpLogWrite(
                "lan_arena_runtime event=client_tal_replica_claim state=active "
                "policy=native_ai_disabled_snapshot_transform_only\r\n");
        }
        if (!tal_initialized) {
            tal_initialized = SudekiMpCleanroomEngineInitializePartyActor(
                SUDEKIMP_CLEANROOM_TAL);
        }
        if (!ailish_initialized) {
            ailish_initialized = SudekiMpCleanroomEngineInitializePartyActor(
                SUDEKIMP_CLEANROOM_AILISH);
        }
        if (tal_initialized && ailish_initialized) {
            BOOL tal_stopped =
                SudekiMpControlSeparationForceStopCharacter(tal);
            BOOL ailish_stopped =
                SudekiMpControlSeparationForceStopCharacter(ailish_actor);
            BOOL stopped = tal_stopped && ailish_stopped;
            if ((int)stopped != client_replica_stop_state) {
                client_replica_stop_state = (int)stopped;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=client_replica_native_movement "
                    "state=%s tal=%s ailish=%s "
                    "policy=native_target_and_current_speed_zero_residual_state_traced\r\n",
                    stopped ? "target_current_speed_quiesced" : "stop_rejected",
                    tal_stopped ? "stopped" : "rejected",
                    ailish_stopped ? "stopped" : "rejected");
            }
        }
        if (!SudekiMpCleanroomEngineDummyPresent() &&
            !arena_dummy_spawn_attempted &&
            SudekiMpCleanroomEngineActorPosition(
                SUDEKIMP_CLEANROOM_AILISH, ailish_position)) {
            tal_position[0] = ailish_position[0];
            tal_position[1] = ailish_position[1];
            tal_position[2] = ailish_position[2] + 6.0f;
            arena_dummy_spawn_attempted =
                SudekiMpCleanroomEngineSpawnDummy(tal_position);
        }
        /* The spawned Tal can already expose a renderer vtable while his
         * native weapon/model tables are still being populated.  Sudeki's
         * selector setter dereferences those tables, so presentation and
         * transform replication must wait for both native setup transactions
         * to complete rather than treating a non-NULL wrapper as readiness. */
        /* Consume and apply the replica once before native scene traversal.
         * Applying it
         * here too dirties CPosition twice per frame and lets the intervening
         * native camera/AI update fight a pose that is not client simulation. */
        SudekiMpControlUpdateObserverGateLeave(
            &lan_arena_control_observer_gate);
        return;
    }
    if (status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        release_host_remote_ailish("invalid_role");
        release_client_remote_tal("invalid_role");
        SudekiMpControlUpdateObserverGateLeave(&lan_arena_control_observer_gate);
        return;
    }
    release_client_remote_tal("host_role");
    ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    if (ailish == NULL) {
        float tal_position[3];
        float ailish_position[3];
        if (!host_ailish_spawn_attempted &&
            SudekiMpCleanroomEngineActorPosition(
                SUDEKIMP_CLEANROOM_TAL, tal_position)) {
            ailish_position[0] = tal_position[0] + 1.5f;
            ailish_position[1] = tal_position[1];
            ailish_position[2] = tal_position[2] - 1.5f;
            host_ailish_spawn_attempted =
                SudekiMpCleanroomEngineSpawnActor(
                    SUDEKIMP_CLEANROOM_AILISH, ailish_position);
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_ailish_spawn status=%s "
                "policy=authenticated_cleanroom_only_then_wait_for_native_group\r\n",
                host_ailish_spawn_attempted ? "requested" : "rejected");
        }
        release_host_remote_ailish("host_ailish_not_present");
        SudekiMpControlUpdateObserverGateLeave(&lan_arena_control_observer_gate);
        return;
    }
    if (!SudekiMpControlSeparationPlayerTwoActive() ||
        SudekiMpControlSeparationPlayerTwoCharacter() != ailish) {
        (void)SudekiMpControlSeparationRequestPlayerTwoCharacter(ailish);
        SudekiMpControlUpdateObserverGateLeave(&lan_arena_control_observer_gate);
        return;
    }
    if (!host_remote_ailish_owned &&
        SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(TRUE)) {
        host_remote_ailish_owned = TRUE;
        host_last_remote_input_at_ms = 0u;
        host_remote_input_quiesced = FALSE;
        host_remote_direction_x = 0;
        host_remote_direction_z = 0;
        host_remote_aim_x = 0;
        host_remote_aim_y = 0;
        host_remote_aim_z = 0;
        host_remote_weak_held = FALSE;
        host_remote_first_person_active = FALSE;
        host_remote_ranged_repeat_not_before_ms = 0u;
        reset_host_remote_weak_cycle();
        SudekiMpLogWrite(
            "lan_arena_runtime event=host_ailish_claim state=active "
            "policy=authenticated_client_input_native_host_arbiter\r\n");
    }
    if (!tal_initialized) {
        tal_initialized = SudekiMpCleanroomEngineInitializePartyActor(
            SUDEKIMP_CLEANROOM_TAL);
    }
    if (!ailish_initialized) {
        ailish_initialized = SudekiMpCleanroomEngineInitializePartyActor(
            SUDEKIMP_CLEANROOM_AILISH);
    }
    if (!SudekiMpCleanroomEngineDummyPresent() &&
        !arena_dummy_spawn_attempted) {
        float tal_position[3];
        float dummy_position[3];
        if (SudekiMpCleanroomEngineActorPosition(
                SUDEKIMP_CLEANROOM_TAL, tal_position)) {
            dummy_position[0] = tal_position[0];
            dummy_position[1] = tal_position[1];
            dummy_position[2] = tal_position[2] + 6.0f;
            arena_dummy_spawn_attempted =
                SudekiMpCleanroomEngineSpawnDummy(dummy_position);
        }
    }
    while (host_remote_ailish_owned &&
           SudekiMpLanArenaSessionTakeRemoteInput(&input)) {
        SudekiMpLanArenaInput admitted_input;
        if (!ensure_canonical_simulation(status.session_token) ||
            !SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
                &canonical_simulation, status.session_token,
                input.actor_type, &input) ||
            !SudekiMpLanArenaSharedSimulationReadPlayerInput(
                &canonical_simulation, input.actor_type,
                &admitted_input, NULL)) {
            SudekiMpLogFormat(
                "lan_arena_runtime event=remote_player_input phase=rejected "
                "actor=Ailish sequence=%lu "
                "policy=canonical_simulation_admission_required\r\n",
                (unsigned long)input.sequence);
            continue;
        }
        input = admitted_input;
        host_last_remote_input_at_ms = GetTickCount();
        host_remote_input_quiesced = FALSE;
        host_remote_direction_x = input.world_direction_x;
        host_remote_direction_z = input.world_direction_z;
        host_remote_aim_x = input.aim_direction_x;
        host_remote_aim_y = input.aim_direction_y;
        host_remote_aim_z = input.aim_direction_z;
        host_remote_weak_held = input.weak_attack_held != 0u;
        host_remote_first_person_active =
            input.ranged_first_person_active != 0u;
        remote_weak_requested = remote_weak_requested ||
            input.weak_attack_pressed != 0u;
        remote_combat_toggle_requested = remote_combat_toggle_requested ||
            input.cleanroom_combat_test_pressed != 0u;
    }
    if (remote_combat_toggle_requested &&
        !SudekiMpLanArenaHostInputRequestRemoteCombatToggle()) {
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_remote_cleanroom_combat_test "
            "phase=rejected win32_error=%lu\r\n",
            (unsigned long)GetLastError());
    }
    remote_aim_valid = host_remote_aim_x != 0 || host_remote_aim_y != 0 ||
        host_remote_aim_z != 0;
    now_ms = GetTickCount();
    remote_native_weak_active =
        host_actor_native_action_variant(1u, TRUE) ==
            SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    if (host_remote_weak_cycle_pending) {
        if (remote_native_weak_active) {
            host_remote_weak_cycle_seen_active = TRUE;
        } else if (host_remote_weak_cycle_seen_active) {
            reset_host_remote_weak_cycle();
        } else if ((DWORD)(now_ms - host_remote_weak_cycle_started_at_ms) >=
                HOST_REMOTE_WEAK_START_TIMEOUT_MS &&
            !host_remote_weak_cycle_timeout_logged) {
            host_remote_weak_cycle_timeout_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_runtime event=host_remote_weak_cycle phase=blocked "
                "reason=native_action_not_observed_within_1000ms "
                "policy=held_input_never_spams_native_combat_entry\r\n");
        }
    }
    if (!host_remote_weak_held && !remote_weak_requested &&
        host_remote_weak_cycle_timeout_logged) {
        reset_host_remote_weak_cycle();
    }
    if (remote_weak_requested || host_remote_weak_held) {
        BOOL combat_enabled = FALSE;
        remote_weak_allowed =
            SudekiMpCleanroomEngineCombatMode(&combat_enabled) &&
            combat_enabled;
        if (remote_weak_allowed && host_remote_first_person_active) {
            ranged_native_ready_known =
                SudekiMpControlSeparationLanArenaPlayerTwoRangedReady(
                    &ranged_native_ready, &ranged_authored_delay_half);
            if (ranged_native_ready_known) {
                ranged_repeat_interval_ms =
                    SudekiMpLanArenaRangedRepeatIntervalMs(
                        ranged_authored_delay_half);
            }
        }
        if (!remote_weak_allowed && !host_remote_weak_blocked_logged) {
            host_remote_weak_blocked_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_runtime event=host_remote_weak_attack phase=rejected "
                "reason=native_combat_inactive "
                "policy=movement_preserved_attack_edge_consumed_no_replica_pose\r\n");
        }
    }
    remote_weak_active = remote_weak_allowed &&
        !host_remote_weak_cycle_pending &&
        (!host_remote_first_person_active ||
         (SudekiMpLanArenaRangedRepeatReady(
              now_ms, host_remote_ranged_repeat_not_before_ms) &&
          (!ranged_native_ready_known || ranged_native_ready))) &&
        (remote_weak_requested || host_remote_weak_held);
    if (host_remote_ailish_owned &&
        SudekiMpLanArenaRemoteInputFresh(
            host_last_remote_input_at_ms, GetTickCount(), 250u)) {
        BOOL ranged_first_person_fire = remote_weak_active &&
            host_remote_first_person_active;
        BOOL ranged_world_fallback = FALSE;
        BOOL weak_submitted = !remote_weak_active;
        BOOL submitted =
            SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
                (float)host_remote_direction_x / 32767.0f,
                (float)host_remote_direction_z / 32767.0f,
                (float)host_remote_aim_x / 32767.0f,
                (float)host_remote_aim_z / 32767.0f,
                remote_aim_valid && host_remote_first_person_active,
                remote_weak_active && !ranged_first_person_fire);
        if (submitted && ranged_first_person_fire) {
            weak_submitted =
                SudekiMpControlSeparationSubmitLanArenaPlayerTwoRangedFire();
            /* Only the locally camera-owned Ailish receives Sudeki's
             * first-person weapon graph at character+0xf0.  The host keeps
             * Tal as its native camera owner, so its authoritative Ailish
             * legitimately has no graph for the first-person-only helper.
             * Fall back solely for that exact validation failure to the
             * already-proven actor-scoped world combat input.  The outer
             * native-action cycle admits one shot at a time, preventing a
             * held mouse button from submitting every controller tick. */
            if (!weak_submitted && GetLastError() == ERROR_INVALID_DATA) {
                weak_submitted =
                    SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
                        (float)host_remote_direction_x / 32767.0f,
                        (float)host_remote_direction_z / 32767.0f,
                        (float)host_remote_aim_x / 32767.0f,
                        (float)host_remote_aim_z / 32767.0f,
                        remote_aim_valid,
                        TRUE);
                ranged_world_fallback = weak_submitted;
            }
        } else if (submitted && remote_weak_active) {
            weak_submitted = TRUE;
        }
        if (submitted) {
            host_remote_ailish_moving = host_remote_direction_x != 0 ||
                host_remote_direction_z != 0;
        }
        if (submitted && !host_remote_input_logged &&
            host_remote_ailish_moving) {
            host_remote_input_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_remote_input phase=submitted "
                "world_direction=%d,%d "
                "policy=cached_authenticated_direction_resubmitted_each_control_tick\r\n",
                (int)host_remote_direction_x,
                (int)host_remote_direction_z);
        }
        if (submitted && remote_weak_active && weak_submitted) {
            if (ranged_first_person_fire) {
                host_remote_ranged_repeat_not_before_ms =
                    GetTickCount() + ranged_repeat_interval_ms;
            }
            host_ailish_weak_attack_until_ms = GetTickCount() + 250u;
            if (ranged_world_fallback) {
                /* The host's non-camera-owned Ailish has no first-person
                 * weapon graph, so selector 59 cannot acknowledge this
                 * actor-scoped fallback. Its exact missile-manager readiness
                 * plus the authored/fail-safe interval owns repeat cadence. */
                reset_host_remote_weak_cycle();
            } else {
                host_remote_weak_cycle_pending = TRUE;
                host_remote_weak_cycle_seen_active = FALSE;
                host_remote_weak_cycle_timeout_logged = FALSE;
                host_remote_weak_cycle_started_at_ms = GetTickCount();
            }
            host_remote_weak_blocked_logged = FALSE;
            if (!host_remote_weak_logged) {
                host_remote_weak_logged = TRUE;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=host_remote_weak_attack "
                    "phase=submitted path=%s repeat_interval_ms=%lu "
                    "authored_delay_half=0x%04x "
                    "policy=native_host_execution\r\n",
                    ranged_world_fallback ?
                        "native_world_arbiter_ranged_fallback" :
                    ranged_first_person_fire ?
                        "verified_first_person_weapon_gate" :
                        "native_arbiter_combat_input",
                    (unsigned long)(ranged_first_person_fire ?
                        ranged_repeat_interval_ms : 0u),
                    (unsigned int)(ranged_first_person_fire ?
                        ranged_authored_delay_half : 0u));
            }
        } else if (submitted && ranged_first_person_fire &&
            !weak_submitted && !host_remote_weak_blocked_logged) {
            DWORD fire_error = GetLastError();
            host_remote_weak_blocked_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_remote_weak_attack "
                "phase=rejected path=verified_first_person_weapon_gate "
                "win32_error=%lu "
                "policy=held_input_may_retry_native_cooldown_no_action_edge\r\n",
                (unsigned long)fire_error);
        }
    } else if (host_remote_ailish_owned && !host_remote_input_quiesced &&
        !SudekiMpLanArenaRemoteInputFresh(
            host_last_remote_input_at_ms, GetTickCount(), 250u) &&
        SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
            0.0f, 0.0f, 0.0f, 0.0f, FALSE, FALSE)) {
        host_remote_input_quiesced = TRUE;
        host_remote_ailish_moving = FALSE;
        host_remote_direction_x = 0;
        host_remote_direction_z = 0;
        host_remote_aim_x = 0;
        host_remote_aim_y = 0;
        host_remote_aim_z = 0;
        host_remote_weak_held = FALSE;
        host_remote_first_person_active = FALSE;
        host_remote_ranged_repeat_not_before_ms = 0u;
        reset_host_remote_weak_cycle();
        SudekiMpLogWrite(
            "lan_arena_runtime event=host_remote_input phase=quiesced "
            "reason=no_fresh_gameplay_packet_250ms "
            "policy=keepalive_never_preserves_stale_movement\r\n");
    }
    SudekiMpControlUpdateObserverGateLeave(&lan_arena_control_observer_gate);
}

static void lan_arena_frame_end_entry(void) {
    /* Preserve native presentation ordering. Networking has no authority to
     * touch game memory here; later actor adapters consume authenticated state
     * from their dedicated post-controller observer. */
    original_frame_end();
    SudekiMpLanArenaCollisionDebugServiceHotkey();
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        SudekiMpLanArenaReplicaDiagnostics diagnostics;
        DWORD now = GetTickCount();
        unsigned int collision_debug_mode =
            SudekiMpLanArenaCollisionDebugMode();
        DWORD trace_interval_ms = collision_debug_mode ==
                SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF ? 1000u : 100u;
        SudekiMpLanArenaClientReplicaRefreshDiagnostics();
        if ((client_replica_diagnostics_last_trace_at == 0u ||
             (DWORD)(now - client_replica_diagnostics_last_trace_at) >=
                trace_interval_ms) &&
            SudekiMpLanArenaClientReplicaGetDiagnostics(&diagnostics)) {
            unsigned int actor_index;
            client_replica_diagnostics_last_trace_at = now;
            SudekiMpLogFormat(
                "lan_arena_replica_camera_diagnostics mode=%u sequence=%lu "
                "render_host_tick=%lu camera_pos=%.5f,%.5f,%.5f "
                "camera_fwd=%.5f,%.5f,%.5f valid=%u "
                "policy=client_camera_basis_witness\r\n",
                collision_debug_mode,
                (unsigned long)diagnostics.sequence,
                (unsigned long)diagnostics.render_host_tick,
                diagnostics.camera_position[0],
                diagnostics.camera_position[1],
                diagnostics.camera_position[2],
                diagnostics.camera_facing[0],
                diagnostics.camera_facing[1],
                diagnostics.camera_facing[2],
                (unsigned int)diagnostics.camera_valid);
            for (actor_index = 0u; actor_index < 2u; ++actor_index) {
                const SudekiMpLanArenaReplicaActorDiagnostics *actor =
                    &diagnostics.actor[actor_index];
                SudekiMpLogFormat(
                    "lan_arena_replica_diagnostics actor=%s mode=%u "
                    "sequence=%lu render_host_tick=%lu upper_host_tick=%lu "
                    "sample_pos=%.5f,%.5f,%.5f sample_fwd=%.5f,%.5f "
                    "pre_pos=%.5f,%.5f,%.5f pre_fwd=%.5f,%.5f,%.5f "
                    "pre_speed=%.5f,%.5f,%.5f "
                    "post_pos=%.5f,%.5f,%.5f post_fwd=%.5f,%.5f,%.5f "
                    "final_pos=%.5f,%.5f,%.5f final_fwd=%.5f,%.5f,%.5f "
                    "render_pos=%.5f,%.5f,%.5f render_fwd=%.5f,%.5f,%.5f "
                    "movement=%.5f,%.5f,%.5f,%.5f mode_id=%lu "
                    "accepted_dir=%.5f,%.5f,%.5f generation=%u dirty=%u "
                    "valid=%u,%u,%u policy=network_to_visible_model_witness\r\n",
                    actor_index == 0u ? "Tal" : "Ailish",
                    collision_debug_mode,
                    (unsigned long)diagnostics.sequence,
                    (unsigned long)diagnostics.render_host_tick,
                    (unsigned long)diagnostics.upper_snapshot_host_tick,
                    actor->sampled_position[0], actor->sampled_position[1],
                    actor->sampled_position[2], actor->sampled_facing[0],
                    actor->sampled_facing[1],
                    actor->pre_apply_position[0], actor->pre_apply_position[1],
                    actor->pre_apply_position[2], actor->pre_apply_facing[0],
                    actor->pre_apply_facing[1], actor->pre_apply_facing[2],
                    actor->pre_apply_target_speed,
                    actor->pre_apply_smoothed_speed,
                    actor->pre_apply_current_speed,
                    actor->post_apply_position[0], actor->post_apply_position[1],
                    actor->post_apply_position[2], actor->post_apply_facing[0],
                    actor->post_apply_facing[1], actor->post_apply_facing[2],
                    actor->native_position[0], actor->native_position[1],
                    actor->native_position[2], actor->native_facing[0],
                    actor->native_facing[1], actor->native_facing[2],
                    actor->render_position[0], actor->render_position[1],
                    actor->render_position[2], actor->render_facing[0],
                    actor->render_facing[1], actor->render_facing[2],
                    actor->movement_target_speed,
                    actor->movement_smoothed_speed,
                    actor->movement_current_speed,
                    actor->movement_run_blend,
                    (unsigned long)actor->movement_mode,
                    actor->accepted_direction[0], actor->accepted_direction[1],
                    actor->accepted_direction[2],
                    (unsigned int)actor->native_generation,
                    (unsigned int)actor->native_dirty,
                    (unsigned int)actor->position_valid,
                    (unsigned int)actor->render_valid,
                    (unsigned int)actor->movement_valid);
            }
        }
    }
    {
        DWORD now_ms = GetTickCount();
        /* Native action selectors may enter and retire between 20 Hz network
         * snapshots. Observe them at the render cadence and journal only the
         * semantic edge; packets still publish at the fixed 50 ms cadence. */
        host_capture_native_action_edges(now_ms);
        host_publish_snapshot(now_ms);
    }
}

static void lan_arena_render_start_entry(void) {
    BOOL replica_applied = FALSE;
    BOOL published_before_start = FALSE;
    BOOL published_after_start = FALSE;
    DWORD publish_error = ERROR_SUCCESS;
    /* Commit and publish immediately before the first native RenderStart,
     * then publish the same sample once more after it. The primary component
     * update at the following 0x28d45b boundary rotates animation root motion
     * through CPosition's world basis; letting that update see the stale +Z
     * basis made only one world-cardinal locomotion direction look correct.
     * This wrapper never samples the network a second time. */
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        tal_initialized && ailish_initialized) {
        replica_applied = SudekiMpLanArenaClientReplicaApplyLatest();
        if (replica_applied) {
            published_before_start =
                SudekiMpLanArenaClientReplicaPublishVisibleTransforms();
            if (!published_before_start) publish_error = GetLastError();
        }
    }
    original_render_start();
    if (replica_applied) {
        published_after_start =
            SudekiMpLanArenaClientReplicaPublishVisibleTransforms();
        if (!published_after_start && publish_error == ERROR_SUCCESS) {
            publish_error = GetLastError();
        }
        if ((int)(published_before_start && published_after_start) !=
                client_animation_basis_publish_state) {
            client_animation_basis_publish_state =
                (int)(published_before_start && published_after_start);
            SudekiMpLogFormat(
                "lan_arena_runtime event=client_animation_basis_publish "
                "state=%s boundary=before_and_after_first_render_start "
                "win32_error=%lu "
                "policy=publish_authoritative_world_basis_before_native_root_motion\r\n",
                published_before_start && published_after_start ?
                    "active" : "rejected",
                published_before_start && published_after_start ?
                    0ul : (unsigned long)publish_error);
        }
        client_trace_native_presentation(0u, SUDEKIMP_CLEANROOM_TAL);
        client_trace_native_presentation(1u, SUDEKIMP_CLEANROOM_AILISH);
        if (!client_replica_stream_logged) {
            client_replica_stream_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_runtime event=client_snapshot_replica phase=active "
                "apply_boundary=pre_first_render_start jitter_buffer_ms=100 "
                "visible_publish=before_and_after_first_plus_pre_world "
                "actors=Tal,Ailish enemy=training_dummy "
                "policy=single_network_sample_native_basis_before_root_motion_native_world_combat_state_replicated\r\n");
        }
    }
}

static void lan_arena_render_pre_world_entry(void) {
    BOOL presentation_reasserted = FALSE;
    BOOL published = FALSE;
    original_render_start();
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        tal_initialized && ailish_initialized) {
        presentation_reasserted =
            SudekiMpLanArenaClientReplicaReassertPresentation();
        if ((int)presentation_reasserted !=
                client_presentation_reassert_state) {
            client_presentation_reassert_state =
                (int)presentation_reasserted;
            SudekiMpLogFormat(
                "lan_arena_runtime event=client_presentation_reassert "
                "state=%s boundary=post_second_render_start_pre_world "
                "win32_error=%lu "
                "policy=authoritative_selector_without_resample_or_time_rewind\r\n",
                presentation_reasserted ? "active" : "rejected",
                presentation_reasserted ? 0ul :
                    (unsigned long)GetLastError());
        }
        published = SudekiMpLanArenaClientReplicaPublishVisibleTransforms();
        if ((int)published != client_visible_transform_publish_state) {
            client_visible_transform_publish_state = (int)published;
            SudekiMpLogFormat(
                "lan_arena_runtime event=client_visible_transform_publish "
                "state=%s boundary=post_second_render_start_pre_world "
                "win32_error=%lu "
                "policy=native_cposition_transform_publication_no_second_sample\r\n",
                published ? "active" : "rejected",
                published ? 0ul : (unsigned long)GetLastError());
        }
        client_trace_tal_action_timeline();
    }
}

BOOL SudekiMpInstallLanArenaRuntime(
    HMODULE game_module,
    const SudekiMpLanArenaSessionConfig *config
) {
    uint8_t *base;
    if (game_module == NULL || config == NULL || runtime_installed ||
        !((config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
           config->local_simulation_node_role ==
               SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) ||
          (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
           config->local_simulation_node_role ==
               SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA)) ||
        !SudekiMpLanArenaSessionStart(config)) {
        if (GetLastError() == ERROR_SUCCESS) SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    runtime_game_module = game_module;
    runtime_config = *config;
    runtime_remote_ipv4[0] = '\0';
    if (config->remote_ipv4 != NULL) {
        if (strlen(config->remote_ipv4) + 1u > sizeof(runtime_remote_ipv4)) {
            SudekiMpLanArenaSessionStop(FALSE);
            SetLastError(ERROR_INVALID_ADDRESS);
            return FALSE;
        }
        strcpy(runtime_remote_ipv4, config->remote_ipv4);
        runtime_config.remote_ipv4 = runtime_remote_ipv4;
    }
    if (!SudekiMpControlSeparationSetManualToggleEnabled(FALSE)) {
        DWORD error = GetLastError();
        SudekiMpLanArenaSessionStop(FALSE);
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpInstallLanArenaCampaignGuard(game_module)) {
        DWORD error = GetLastError();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        SudekiMpLanArenaSessionStop(FALSE);
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpInstallLanArenaCollisionDebug(game_module)) {
        DWORD error = GetLastError();
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        SudekiMpLanArenaSessionStop(FALSE);
        SetLastError(error);
        return FALSE;
    }
    original_frame_end = (FrameEndFunction)(base + RVA_FRAME_END);
    original_render_start = (RenderStartFunction)(base + RVA_RENDER_START);
    if (!SudekiMpInstallRelativeCallHook(
            &lan_arena_frame_end_hook,
            base + RVA_FRAME_END_CALL,
            original_frame_end,
            lan_arena_frame_end_entry)) {
        DWORD error = GetLastError();
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        !SudekiMpInstallRelativeCallHook(
            &lan_arena_render_start_hook,
            base + RVA_RENDER_START_CALL,
            original_render_start,
            lan_arena_render_start_entry)) {
        DWORD error = GetLastError();
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        !SudekiMpInstallRelativeCallHook(
            &lan_arena_render_pre_world_hook,
            base + RVA_RENDER_PRE_WORLD_CALL,
            original_render_start,
            lan_arena_render_pre_world_entry)) {
        DWORD error = GetLastError();
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if ((config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
         !SudekiMpInstallLanArenaHostInput(game_module)) ||
        (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
         (!SudekiMpInstallLanArenaClientInput(game_module) ||
          !SudekiMpInitializeLanArenaClientReplica(game_module)))) {
        DWORD error = GetLastError();
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        SudekiMpUninstallLanArenaClientInput();
        SudekiMpUninstallLanArenaHostInput();
        SudekiMpResetLanArenaClientReplica();
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpControlUpdateObserverGateEnable(
             &lan_arena_control_observer_gate) ||
        !SudekiMpControlSeparationRegisterUpdateObserver(
            &lan_arena_control_observer_owner,
            lan_arena_control_update_observer)) {
        DWORD error = GetLastError();
        SudekiMpControlUpdateObserverGateDisable(
            &lan_arena_control_observer_gate);
        SudekiMpControlUpdateObserverGateDrain(
            &lan_arena_control_observer_gate);
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        SudekiMpUninstallLanArenaClientInput();
        SudekiMpUninstallLanArenaHostInput();
        SudekiMpResetLanArenaClientReplica();
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    runtime_installed = TRUE;
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    host_release_pending_logged = FALSE;
    client_release_pending_logged = FALSE;
    client_replica_stop_state = -1;
    client_animation_basis_publish_state = -1;
    client_presentation_reassert_state = -1;
    client_visible_transform_publish_state = -1;
    runtime_hook_restore_failed_logged = FALSE;
    dummy_release_pending_logged = FALSE;
    ZeroMemory(host_previous_actor_position,
        sizeof(host_previous_actor_position));
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_last_translation_at_ms,
        sizeof(host_actor_last_translation_at_ms));
    ZeroMemory(host_replica_idle_position,
        sizeof(host_replica_idle_position));
    ZeroMemory(host_replica_idle_position_valid,
        sizeof(host_replica_idle_position_valid));
    ZeroMemory(host_actor_was_moving,
        sizeof(host_actor_was_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    ZeroMemory(host_actor_presentation,
        sizeof(host_actor_presentation));
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    ZeroMemory(host_actor_presentation_last_trace_at,
        sizeof(host_actor_presentation_last_trace_at));
    ZeroMemory(client_actor_presentation,
        sizeof(client_actor_presentation));
    ZeroMemory(client_actor_presentation_valid,
        sizeof(client_actor_presentation_valid));
    ZeroMemory(client_actor_presentation_last_trace_at,
        sizeof(client_actor_presentation_last_trace_at));
    client_replica_diagnostics_last_trace_at = 0u;
    reset_client_tal_action_timeline();
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_weak_blocked_logged = FALSE;
    host_remote_ailish_moving = FALSE;
    host_remote_direction_x = 0;
    host_remote_direction_z = 0;
    host_remote_aim_x = 0;
    host_remote_aim_y = 0;
    host_remote_aim_z = 0;
    host_remote_weak_held = FALSE;
    host_remote_first_person_active = FALSE;
    host_remote_ranged_repeat_not_before_ms = 0u;
    reset_host_remote_weak_cycle();
    host_auto_rehost_enabled =
        config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    host_ailish_spawn_attempted = FALSE;
    client_tal_spawn_attempted = FALSE;
    client_remote_tal_owned = FALSE;
    arena_dummy_spawn_attempted = FALSE;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    if (!start_network_pump()) {
        DWORD error = GetLastError();
        SudekiMpUninstallLanArenaRuntime();
        SetLastError(error);
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_runtime event=install render_start_call_rva=0x%08lx "
        "render_pre_world_call_rva=0x%08lx "
        "frame_end_call_rva=0x%08lx "
        "render_start_replica_reassert=%s "
        "policy=game_thread_actor_adapters_plus_synchronized_socket_worker\r\n",
        (unsigned long)RVA_RENDER_START_CALL,
        (unsigned long)RVA_RENDER_PRE_WORLD_CALL,
        (unsigned long)RVA_FRAME_END_CALL,
        config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ?
            "client_only" : "disabled_on_host"
    );
    return TRUE;
}

void SudekiMpUninstallLanArenaRuntime(void) {
    /* Detours are the lifetime boundary. Remove them before releasing any
     * callback/resource they can reach. If even one callsite cannot be
     * restored, keep the complete runtime and original callbacks alive and
     * pin this DLL rather than leaving a dangling game-code call. */
    if (!rollback_lan_arena_frame_hooks()) return;
    SudekiMpControlUpdateObserverGateDisable(&lan_arena_control_observer_gate);
    (void)SudekiMpControlSeparationUnregisterUpdateObserver(
        &lan_arena_control_observer_owner);
    SudekiMpControlUpdateObserverGateDrain(&lan_arena_control_observer_gate);
    release_host_remote_ailish("runtime_uninstall");
    release_client_remote_tal("runtime_uninstall");
    release_arena_dummy();
    (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
    SudekiMpUninstallLanArenaClientInput();
    SudekiMpUninstallLanArenaHostInput();
    SudekiMpResetLanArenaClientReplica();
    SudekiMpUninstallLanArenaCollisionDebug();
    SudekiMpUninstallLanArenaCampaignGuard();
    stop_network_pump();
    SudekiMpLanArenaSessionStop(runtime_installed);
    original_frame_end = NULL;
    original_render_start = NULL;
    runtime_installed = FALSE;
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    host_release_pending_logged = FALSE;
    client_release_pending_logged = FALSE;
    client_replica_stop_state = -1;
    client_animation_basis_publish_state = -1;
    client_presentation_reassert_state = -1;
    client_visible_transform_publish_state = -1;
    runtime_hook_restore_failed_logged = FALSE;
    dummy_release_pending_logged = FALSE;
    ZeroMemory(host_previous_actor_position,
        sizeof(host_previous_actor_position));
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_last_translation_at_ms,
        sizeof(host_actor_last_translation_at_ms));
    ZeroMemory(host_replica_idle_position_valid,
        sizeof(host_replica_idle_position_valid));
    ZeroMemory(host_actor_was_moving,
        sizeof(host_actor_was_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    ZeroMemory(host_actor_presentation,
        sizeof(host_actor_presentation));
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    ZeroMemory(host_actor_presentation_last_trace_at,
        sizeof(host_actor_presentation_last_trace_at));
    ZeroMemory(client_actor_presentation,
        sizeof(client_actor_presentation));
    ZeroMemory(client_actor_presentation_valid,
        sizeof(client_actor_presentation_valid));
    ZeroMemory(client_actor_presentation_last_trace_at,
        sizeof(client_actor_presentation_last_trace_at));
    client_replica_diagnostics_last_trace_at = 0u;
    reset_client_tal_action_timeline();
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_weak_blocked_logged = FALSE;
    host_remote_ailish_moving = FALSE;
    host_remote_direction_x = 0;
    host_remote_direction_z = 0;
    host_remote_aim_x = 0;
    host_remote_aim_y = 0;
    host_remote_aim_z = 0;
    host_remote_weak_held = FALSE;
    host_remote_first_person_active = FALSE;
    host_remote_ranged_repeat_not_before_ms = 0u;
    reset_host_remote_weak_cycle();
    host_auto_rehost_enabled = FALSE;
    host_ailish_spawn_attempted = FALSE;
    client_tal_spawn_attempted = FALSE;
    client_remote_tal_owned = FALSE;
    arena_dummy_spawn_attempted = FALSE;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    ZeroMemory(&runtime_config, sizeof(runtime_config));
    runtime_remote_ipv4[0] = '\0';
    runtime_game_module = NULL;
}

BOOL SudekiMpLanArenaRuntimeInstalled(void) {
    return runtime_installed;
}

BOOL SudekiMpLanArenaRuntimeEndSession(void) {
    BOOL actors_released;
    BOOL dummy_released;
    if (!runtime_installed) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        host_auto_rehost_enabled = FALSE;
    }
    SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(FALSE);
    actors_released = release_host_remote_ailish("local_end_session") &&
        release_client_remote_tal("local_end_session");
    dummy_released = release_arena_dummy();
    SudekiMpLanArenaSessionStop(TRUE);
    SudekiMpResetLanArenaClientReplica();
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_snapshot_stream_logged = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_last_translation_at_ms,
        sizeof(host_actor_last_translation_at_ms));
    ZeroMemory(host_replica_idle_position_valid,
        sizeof(host_replica_idle_position_valid));
    ZeroMemory(host_actor_was_moving,
        sizeof(host_actor_was_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    ZeroMemory(client_actor_presentation_valid,
        sizeof(client_actor_presentation_valid));
    reset_client_tal_action_timeline();
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_weak_blocked_logged = FALSE;
    host_remote_direction_x = 0;
    host_remote_direction_z = 0;
    host_remote_aim_x = 0;
    host_remote_aim_y = 0;
    host_remote_aim_z = 0;
    host_remote_weak_held = FALSE;
    host_remote_first_person_active = FALSE;
    host_remote_ranged_repeat_not_before_ms = 0u;
    reset_host_remote_weak_cycle();
    SudekiMpLogFormat(
        "lan_arena_runtime event=local_end_session cleanup=%s "
        "policy=stop_network_discard_replica_retry_any_unconfirmed_native_release\r\n",
        actors_released && dummy_released ? "confirmed" : "pending");
    if (!actors_released || !dummy_released) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpLanArenaRuntimeJoinAddress(const char *remote_ipv4) {
    return SudekiMpLanArenaRuntimeJoinEndpoint(remote_ipv4);
}

BOOL SudekiMpLanArenaRuntimeJoinEndpoint(const char *endpoint) {
    char parsed_ipv4[16];
    uint16_t parsed_port;
    if (!runtime_installed ||
        runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ||
        !SudekiMpLanArenaParseEndpoint(
            endpoint, (uint16_t)runtime_config.port,
            parsed_ipv4, sizeof(parsed_ipv4), &parsed_port)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!release_client_remote_tal("client_rejoin") ||
        !release_arena_dummy()) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    SudekiMpResetLanArenaClientReplica();
    if (!SudekiMpInitializeLanArenaClientReplica(runtime_game_module)) {
        return FALSE;
    }
    SudekiMpLanArenaSessionStop(TRUE);
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    strcpy(runtime_remote_ipv4, parsed_ipv4);
    runtime_config.remote_ipv4 = runtime_remote_ipv4;
    runtime_config.port = parsed_port;
    if (!SudekiMpLanArenaSessionStart(&runtime_config)) return FALSE;
    client_tal_spawn_attempted = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    ZeroMemory(client_actor_presentation_valid,
        sizeof(client_actor_presentation_valid));
    reset_client_tal_action_timeline();
    arena_dummy_spawn_attempted = FALSE;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    SudekiMpLogFormat(
        "lan_arena_runtime event=client_join_address address=%s port=%u "
        "policy=fresh_token_no_reconnect_state_reuse\r\n",
        runtime_remote_ipv4, runtime_config.port);
    return TRUE;
}

BOOL SudekiMpLanArenaRuntimeHostArena(void) {
    if (!runtime_installed ||
        runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    host_auto_rehost_enabled = TRUE;
    if (!release_host_remote_ailish("host_restart") ||
        !release_arena_dummy()) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    SudekiMpLanArenaSessionStop(TRUE);
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    host_ailish_spawn_attempted = FALSE;
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_last_translation_at_ms,
        sizeof(host_actor_last_translation_at_ms));
    ZeroMemory(host_replica_idle_position_valid,
        sizeof(host_replica_idle_position_valid));
    ZeroMemory(host_actor_was_moving,
        sizeof(host_actor_was_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_weak_blocked_logged = FALSE;
    host_remote_direction_x = 0;
    host_remote_direction_z = 0;
    host_remote_aim_x = 0;
    host_remote_aim_y = 0;
    host_remote_aim_z = 0;
    host_remote_weak_held = FALSE;
    host_remote_first_person_active = FALSE;
    host_remote_ranged_repeat_not_before_ms = 0u;
    reset_host_remote_weak_cycle();
    arena_dummy_spawn_attempted = FALSE;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    if (!SudekiMpLanArenaSessionStart(&runtime_config)) return FALSE;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_arena port=%u "
        "policy=fresh_session_wait_for_one_authenticated_client\r\n",
        runtime_config.port);
    return TRUE;
}

BOOL SudekiMpLanArenaRuntimeGetStatus(
    SudekiMpLanArenaSessionStatus *status
) {
    if (status == NULL || !runtime_installed) return FALSE;
    if (SudekiMpLanArenaSessionGetStatus(status)) return TRUE;
    ZeroMemory(status, sizeof(*status));
    status->phase = SUDEKIMP_LAN_ARENA_CONNECTION_ENDED;
    status->local_role = (uint8_t)runtime_config.local_role;
    return TRUE;
}
