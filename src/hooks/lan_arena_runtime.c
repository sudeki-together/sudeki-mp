#include "hooks/lan_arena_runtime.h"
#include "hooks/lan_arena_cast_context.h"

#include "cleanroom/engine.h"
#include "cleanroom/menu.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "engine/spirit_activation_abi.h"
#include "engine/spirit_instance_abi.h"
#include "hooks/quick_skill_input.h"
#include "engine/weapon_activation_abi.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/lan_arena_client_input.h"
#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "hooks/lan_arena_collision_debug.h"
#include "hooks/lan_arena_host_input.h"
#include "hooks/lan_arena_owner_view.h"
#include "hooks/lan_arena_skill_fade.h"
#include "hooks/lan_arena_pause_panel.h"
#include "hooks/lan_arena_spirit_audio.h"
#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/noncaster_skill_locomotion.h"
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
typedef BOOL (__attribute__((thiscall)) *CameraManagerSetRenderCameraFunction)(
    void *manager, const char *name);
typedef void (__attribute__((thiscall)) *GameSpeedSetModeFunction)(
    void *game_speed, int mode);

typedef enum HostSnapshotFailureStage {
    HOST_SNAPSHOT_FAILURE_NONE = 0,
    HOST_SNAPSHOT_FAILURE_COMBAT_OBSERVATION,
    HOST_SNAPSHOT_FAILURE_TAL_ACTOR_FILL,
    HOST_SNAPSHOT_FAILURE_AILISH_ACTOR_FILL,
    HOST_SNAPSHOT_FAILURE_TAL_SKILL_OBSERVATION,
    HOST_SNAPSHOT_FAILURE_AILISH_SKILL_OBSERVATION,
    HOST_SNAPSHOT_FAILURE_SPIRIT_OBSERVATION,
    HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION,
    HOST_SNAPSHOT_FAILURE_CANONICAL_BEGIN,
    HOST_SNAPSHOT_FAILURE_CANONICAL_COMMIT,
    HOST_SNAPSHOT_FAILURE_CANONICAL_READ,
    HOST_SNAPSHOT_FAILURE_SEND
} HostSnapshotFailureStage;

typedef struct HostSnapshotFailureTelemetry {
    uint64_t session_token;
    DWORD last_logged_at_ms;
    uint32_t consecutive_failures;
    HostSnapshotFailureStage stage;
} HostSnapshotFailureTelemetry;

enum {
    RVA_RENDER_START = 0x001dce30u,
    RVA_RENDER_START_CALL = 0x0028d443u,
    RVA_RENDER_PRE_WORLD_CALL = 0x0028d539u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu,
    RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_GAME_SPEED_SET_MODE = 0x00207560u,
    RVA_FIXED_ALTERNATE_SPEED = 0x002c4018u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_SCENE_MANAGER_GLOBAL = 0x00408d58u,
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
    AILISH_COMBAT_WEAK_SELECTOR = 59,
    /* Buki (melee) resolves her own renderer ids over the same gait shape:
     * world idle 1 (variants 4/3/2), world move 6/7, combat idle 20,
     * combat move 23/24. Captured live via move_selector_diag. */
    BUKI_WORLD_IDLE_SELECTOR = 1,
    BUKI_WORLD_IDLE_VARIANT_ONE_SELECTOR = 4,
    BUKI_WORLD_IDLE_VARIANT_TWO_SELECTOR = 3,
    BUKI_WORLD_MOVE_PRIMARY_SELECTOR = 6,
    BUKI_WORLD_MOVE_SECONDARY_SELECTOR = 7,
    BUKI_COMBAT_IDLE_SELECTOR = 20,
    BUKI_COMBAT_MOVE_PRIMARY_SELECTOR = 23,
    BUKI_COMBAT_MOVE_SECONDARY_SELECTOR = 24
};

static SudekiMpRelativeCallHook lan_arena_frame_end_hook;
static SudekiMpRelativeCallHook lan_arena_render_start_hook;
static SudekiMpRelativeCallHook lan_arena_render_pre_world_hook;
static SudekiMpInlineHook host_skill_camera_hook;
static SudekiMpInlineHook host_skill_speed_hook;
static FrameEndFunction original_frame_end;
static RenderStartFunction original_render_start;
static BOOL runtime_installed;
static SudekiMpLanArenaSharedSimulation canonical_simulation;
static BOOL host_remote_ailish_owned;
static BOOL host_ailish_spawn_attempted;
static SudekiMpLanArenaSkillFade host_skill_fade;
static uint64_t host_skill_fade_session;
static void *host_skill_fade_actor;
static BOOL client_remote_tal_owned;
static BOOL client_tal_spawn_attempted;
static BOOL client_remote_tal_request_owned;
static BOOL client_remote_tal_remove_pending;
static uint32_t client_remote_tal_generation_counter;
static uint32_t client_remote_tal_active_generation;
static void *client_remote_tal_generation_actor;
static BOOL arena_dummy_spawn_attempted;
static BOOL tal_initialized;
static BOOL ailish_initialized;
static DWORD host_last_snapshot_at_ms;
static BOOL host_snapshot_stream_logged;
static HostSnapshotFailureTelemetry host_snapshot_failure_telemetry;
static BOOL client_replica_stream_logged;
static BOOL client_replica_discard_logged;
static BOOL host_release_pending_logged;
static BOOL client_release_pending_logged;
static int client_replica_stop_state = -1;
static BOOL client_replica_frame_attempted;
static BOOL client_replica_frame_ready;

static void prepare_client_replica_frame(void) {
    if (client_replica_frame_attempted) return;
    client_replica_frame_attempted = TRUE;
    client_replica_frame_ready = SudekiMpLanArenaClientReplicaApplyLatest();
}
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

static uint32_t advance_client_remote_tal_generation(void) {
    ++client_remote_tal_generation_counter;
    if (client_remote_tal_generation_counter == 0u) {
        ++client_remote_tal_generation_counter;
    }
    return client_remote_tal_generation_counter;
}

static void invalidate_client_remote_tal_generation(const char *reason) {
    BOOL had_lifecycle = client_remote_tal_generation_actor != NULL ||
        client_remote_tal_active_generation != 0u;
    if (!had_lifecycle) return;
    SudekiMpLanArenaClientReplicaSetRemoteTalLease(NULL, 0u);
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    tal_initialized = FALSE;
    SudekiMpLogFormat(
        "lan_arena_runtime event=client_tal_lifecycle state=invalidated "
        "reason=%s policy=release_before_address_reuse\r\n",
        reason != NULL ? reason : "unspecified");
}

static BOOL claim_client_remote_tal_generation(void *tal) {
    uint32_t generation;
    if (tal == NULL) return FALSE;
    if (client_remote_tal_generation_actor == tal &&
        client_remote_tal_active_generation != 0u) {
        return TRUE;
    }
    invalidate_client_remote_tal_generation("actor_identity_changed");
    generation = advance_client_remote_tal_generation();
    client_remote_tal_generation_actor = tal;
    client_remote_tal_active_generation = generation;
    tal_initialized = FALSE;
    SudekiMpLanArenaClientReplicaSetRemoteTalLease(tal, generation);
    return TRUE;
}

static BOOL client_tal_missing_actor_requires_release(void) {
    /* SpawnActor is asynchronous. A successful request with no published Tal
     * yet is pending creation, not a lost actor: clearing its attempt flag and
     * requesting another spawn can enqueue duplicate group insertions. */
    return client_remote_tal_owned ||
        client_remote_tal_request_owned ||
        client_remote_tal_remove_pending ||
        client_remote_tal_generation_actor != NULL ||
        client_remote_tal_active_generation != 0u;
}
static BOOL host_actor_was_moving[2];
static BOOL host_actor_locomotion_moving[2];
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
static uint16_t host_actor_skill_sequence[2];
static uint8_t host_actor_skill_kind[2];
static uint8_t host_actor_skill_slot[2];
static uint32_t host_actor_skill_cost[2];
static BOOL host_actor_previous_skill_active[2];
static BOOL host_spirit_previous_active;
static unsigned int host_spirit_actor_index;
static uint8_t host_spirit_camera_kind;
static int host_spirit_previous_state;
static SudekiMpLanArenaSpiritAudioSemanticEvent
    host_spirit_audio_history[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY];
static uint8_t host_spirit_audio_history_count;
static uint16_t host_spirit_audio_event_sequence;
static uint32_t host_spirit_audio_trace_sequence;
static BOOL host_spirit_audio_trace_sequence_initialized;
typedef struct HostSpiritAudioStage {
    SudekiMpLanArenaSpiritAudioSemanticEvent history[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY];
    uint8_t history_count;
    uint16_t event_sequence;
    uint32_t trace_sequence;
    uint32_t journaled_raw_trace_sequence;
    BOOL trace_sequence_initialized;
} HostSpiritAudioStage;
typedef struct HostOperatorSpiritIntent {
    void *seat[SUDEKIMP_LAN_ARENA_SEAT_COUNT];
    uint64_t session_token;
    unsigned int variant;
    unsigned int actor_index;
    BOOL pending;
} HostOperatorSpiritIntent;
static HostOperatorSpiritIntent host_operator_spirit_intent;
static uint64_t host_operator_spirit_session_token;
static volatile LONG host_operator_spirit_activation_depth;
typedef struct HostNativeSkillLease {
    void *character;
    void *skill;
    int slot;
    uint16_t wire_sequence;
    BOOL pending;
    BOOL active_seen;
    BOOL sequence_allocated;
} HostNativeSkillLease;
static HostNativeSkillLease host_native_skill_leases[2];
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
static struct {
    void *actor;
    uint64_t session_token;
    uint16_t sequence;
    SudekiMpLanArenaLocomotion previous;
} host_ailish_locomotion;
static struct {
    void *actor;
    uint64_t session_token;
    uint16_t sequence;
    SudekiMpLanArenaLocomotion previous;
} host_tal_locomotion;
static DWORD host_actor_presentation_last_trace_at[2];
static SudekiMpCleanroomActorPresentation client_actor_presentation[2];
static BOOL client_actor_presentation_valid[2];
static DWORD client_actor_presentation_last_trace_at[2];
static DWORD client_replica_diagnostics_last_trace_at;
static DWORD client_cleanroom_maintenance_last_at;
static uint32_t client_tal_timeline_action_sequence;
static uint32_t client_tal_timeline_last_render_host_tick;
static unsigned int client_tal_timeline_idle_samples_remaining;
static char lan_arena_control_observer_owner;
static SudekiMpControlUpdateObserverGate lan_arena_control_observer_gate;
static SudekiMpLanArenaSessionConfig runtime_config;

static uint8_t seat_host_type(void) {
    return runtime_config.host_actor_type == 0u
        ? SUDEKIMP_LAN_ARENA_TAL_TYPE : runtime_config.host_actor_type;
}
static uint8_t seat_client_type(void) {
    return runtime_config.client_actor_type == 0u
        ? SUDEKIMP_LAN_ARENA_AILISH_TYPE : runtime_config.client_actor_type;
}
static SudekiMpCleanroomActor seat_host_actor(void) {
    SudekiMpCleanroomActor actor;
    return SudekiMpCleanroomActorFromType(seat_host_type(), &actor)
        ? actor : SUDEKIMP_CLEANROOM_TAL;
}
static SudekiMpCleanroomActor seat_client_actor(void) {
    SudekiMpCleanroomActor actor;
    return SudekiMpCleanroomActorFromType(seat_client_type(), &actor)
        ? actor : SUDEKIMP_CLEANROOM_AILISH;
}
static char runtime_remote_ipv4[64];
static HMODULE runtime_game_module;
static HANDLE network_pump_stop_event;
static HANDLE network_pump_thread;
static volatile LONG host_remote_skill_activation_depth;
static BOOL host_remote_skill_camera_active;
static BOOL host_remote_skill_camera_suppression_logged;
static SudekiMpLanArenaOwnerViewLease host_tal_skill_view_lease;
static uint8_t host_tal_skill_view_slot;
static BOOL host_tal_skill_view_basis_safe_this_frame;
static int host_tal_skill_view_trace_state = -1;
static BOOL host_skill_speed_override_active;
static uint32_t host_skill_original_alternate_speed_bits;
static int host_skill_speed_trace_state = -1;
static SudekiMpNoncasterSkillLocomotionLease
    host_noncaster_locomotion_leases[2];
static int host_noncaster_locomotion_trace_state[2] = { -1, -1 };

static const uint8_t expected_camera_manager_set_render_camera_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t expected_game_speed_set_mode_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x89u, 0x41u, 0x24u
};
static const uint8_t expected_fixed_alternate_speed[] = {
    0x29u, 0x5cu, 0x8fu, 0x3du
};

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

static BOOL set_host_skill_realtime_scale(BOOL enabled) {
    uint32_t *scale;
    uint32_t desired;
    DWORD old_protection;
    DWORD ignored_protection;
    BOOL protection_restored;

    if (runtime_game_module == NULL) return FALSE;
    scale = (uint32_t *)((uint8_t *)runtime_game_module +
        RVA_FIXED_ALTERNATE_SPEED);
    desired = enabled ? 0x3f800000u :
        host_skill_original_alternate_speed_bits;
    if (*scale == desired) {
        host_skill_speed_override_active = enabled;
        return TRUE;
    }
    if (!VirtualProtect(scale, sizeof(*scale), PAGE_EXECUTE_READWRITE,
            &old_protection)) return FALSE;
    *scale = desired;
    FlushInstructionCache(GetCurrentProcess(), scale, sizeof(*scale));
    protection_restored = VirtualProtect(scale, sizeof(*scale),
        old_protection, &ignored_protection);
    if (protection_restored && *scale == desired) {
        host_skill_speed_override_active = enabled;
        return TRUE;
    }
    return FALSE;
}

static BOOL host_remote_skill_camera_owned(void) {
    SudekiMpLanCastOwner owner;
    if (SudekiMpLanCastContextCurrent(&owner)) {
        /* An attributed host script keeps its camera even when a remote
         * script is alive. Delayed child calls retain the original caster. */
        if (owner.actor_type == seat_host_type()) return FALSE;
        if (owner.actor_type == seat_client_type()) return TRUE;
    }
    /* Native camera updates outside the VM still use the existing single-cast
     * lease. Overlap admission stays closed until those are isolated too. */
    return InterlockedCompareExchange(
            &host_remote_skill_activation_depth, 0, 0) > 0 ||
        host_remote_skill_camera_active || host_tal_skill_view_lease.valid;
}

static BOOL runtime_readable_memory(const void *pointer, size_t length) {
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

/* ANIMID read offsets (CONFIRMED_STATIC). The model is CNewGameModelAnimation,
 * reached from the character entity at +0x134 and verified by its +0x10
 * backpointer to the character. Its per-channel state array is at +0xF8 (4-byte
 * records, byte +2 = current ANIMID); +0x131 == 3 is the animation-mode flag the
 * game itself checks in TsaGetCurrentAnimation (0x43AF10), bound < 0xC4. */
enum {
    /* +0x130 is the uniform character->model link (Ghidra FUN_00561960 loads it
     * as MOV EDI,[param+0x130]; live scan confirmed 0x130 for BOTH melee and
     * ranged, while 0x134 is ranged-only). */
    HOST_CHARACTER_ANIMATION_COMPONENT_OFFSET = 0x130u,
    HOST_COMPONENT_CHARACTER_BACKPOINTER_OFFSET = 0x10u,
    HOST_MODEL_ANIMATION_MODE_OFFSET = 0x131u,
    HOST_MODEL_ANIMATION_STATE_ARRAY_OFFSET = 0xF8u,
    HOST_MODEL_ANIMATION_MODE_READY = 3u,
    HOST_ANIM_ID_UPPER_BOUND = 0xC4u
};

/* Read the host actor's current semantic ANIMID from its model, mirroring the
 * game's own preconditions. Every failure path returns 0 (UNKNOWN), never a
 * synthesised id -- observation failure is unknown, not inactive. */
static uint8_t host_read_actor_anim_id(SudekiMpCleanroomActor actor) {
    uint8_t *character;
    uint8_t *model;
    uint8_t *state;
    uint8_t anim_id;
    unsigned int stage = 0u;
    if (actor < 0 || actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) return 0u;
    character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(actor);
    if (character == NULL) { stage = 1u; goto done; }
    if (!runtime_readable_memory(character, 0x138u)) { stage = 2u; goto done; }
    model = *(uint8_t **)(character + HOST_CHARACTER_ANIMATION_COMPONENT_OFFSET);
    if (model == NULL) { stage = 3u; goto done; }
    if (!runtime_readable_memory(model, 0x138u)) { stage = 4u; goto done; }
    if (*(uint8_t **)(model + HOST_COMPONENT_CHARACTER_BACKPOINTER_OFFSET) !=
            character) { stage = 5u; goto done; }
    /* NOTE: no model[0x131]==3 gate here. TsaGetCurrentAnimation (0x43AF10)
     * uses that guard for its own "current animation" concept, but live capture
     * showed 0x131 never latches during combat while channel 0 (state[2]) DOES
     * carry the combat ANIMID (0x72..0x7F) — so the guard would wrongly force
     * UNKNOWN during every combo. Read channel 0 directly instead. */
    state = *(uint8_t **)(model + HOST_MODEL_ANIMATION_STATE_ARRAY_OFFSET);
    if (state == NULL) { stage = 6u; goto done; }
    if (!runtime_readable_memory(state, 3u)) { stage = 7u; goto done; }
    anim_id = state[2u]; /* channel 0: [[model+0xF8] + channel*4 + 2] */
    if (anim_id >= HOST_ANIM_ID_UPPER_BOUND) { stage = 8u; goto done; }
    return anim_id;
done:
    {
        /* TEMPORARY diagnostic: which hop of the model read failed. stage=0 is
         * full success (unreachable here); 1..9 name the exact guard that
         * bailed. REMOVE after Phase 5 capture. */
        static DWORD last_host_anim_stage_diag[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
        DWORD stage_now = GetTickCount();
        unsigned int slot = (unsigned int)actor;
        if (slot >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) slot = 0u;
        if (last_host_anim_stage_diag[slot] == 0u ||
            (DWORD)(stage_now - last_host_anim_stage_diag[slot]) >= 1000u) {
            last_host_anim_stage_diag[slot] = stage_now;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_anim_id_stage actor=%u stage=%u "
                "character=%p model=%p model_0x131=%u state=%p\r\n",
                (unsigned)actor, stage, (void *)character, (void *)model,
                model != NULL ? (unsigned)model[
                    HOST_MODEL_ANIMATION_MODE_OFFSET] : 0u,
                (void *)state);
            /* TEMPORARY: scan the character entity for every field whose
             * pointer has the model's +0x10 backpointer, so the real
             * character->model offset is found empirically (Ghidra pointed at
             * 0x130; this confirms it). REMOVE after Phase 5 capture. */
            {
                unsigned int scan_x;
                for (scan_x = 0u; scan_x <= 0x200u; scan_x += 4u) {
                    uint8_t *candidate;
                    if (!runtime_readable_memory(
                            character + scan_x, sizeof(void *))) continue;
                    candidate = *(uint8_t **)(character + scan_x);
                    if (candidate == NULL ||
                        !runtime_readable_memory(candidate, 0x138u)) continue;
                    if (*(void **)(candidate + 0x10u) != character) continue;
                    SudekiMpLogFormat(
                        "lan_arena_runtime event=host_model_scan actor=%u "
                        "offset=0x%x model=%p mode_131=%u state=%p\r\n",
                        (unsigned)actor, scan_x, (void *)candidate,
                        (unsigned)candidate[HOST_MODEL_ANIMATION_MODE_OFFSET],
                        (void *)(runtime_readable_memory(candidate, 0xFCu) ?
                            *(uint8_t **)(
                                candidate + HOST_MODEL_ANIMATION_STATE_ARRAY_OFFSET) :
                            NULL));
                }
            }
            /* TEMPORARY: dump per-channel ANIMIDs (byte+2) + flags (byte+3) of
             * the state array to find which channel carries the combat combo
             * (play writes channel 2; TsaGetCurrentAnimation reads channel 0).
             * REMOVE after Phase 5 capture. */
            if (model != NULL && runtime_readable_memory(
                    model, HOST_MODEL_ANIMATION_STATE_ARRAY_OFFSET + 4u)) {
                uint8_t *sarr = *(uint8_t **)(
                    model + HOST_MODEL_ANIMATION_STATE_ARRAY_OFFSET);
                if (sarr != NULL) {
                    unsigned int ch;
                    for (ch = 0u; ch < 8u; ++ch) {
                        if (!runtime_readable_memory(sarr + ch * 4u, 4u)) break;
                        SudekiMpLogFormat(
                            "lan_arena_runtime event=host_anim_channel actor=%u "
                            "ch=%u id=%u flags=%u\r\n",
                            (unsigned)actor, ch,
                            (unsigned)sarr[ch * 4u + 2u],
                            (unsigned)sarr[ch * 4u + 3u]);
                    }
                }
            }
        }
    }
    return 0u;
}

static BOOL current_host_owner_view(
    void **camera_mode,
    void **scene_manager
) {
    uint8_t *base = (uint8_t *)runtime_game_module;
    if (camera_mode == NULL || scene_manager == NULL || base == NULL ||
        !runtime_readable_memory(
            base + RVA_GAME_CAMERA_MODE_GLOBAL, sizeof(*camera_mode)) ||
        !runtime_readable_memory(
            base + RVA_SCENE_MANAGER_GLOBAL, sizeof(*scene_manager))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *camera_mode = *(void **)(base + RVA_GAME_CAMERA_MODE_GLOBAL);
    *scene_manager = *(void **)(base + RVA_SCENE_MANAGER_GLOBAL);
    if (*camera_mode == NULL || *scene_manager == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    return TRUE;
}

static BOOL service_host_tal_skill_view(
    SudekiMpLanArenaOwnerViewBoundary boundary
) {
    void *camera_mode;
    void *scene_manager;
    if (!host_tal_skill_view_lease.valid) return TRUE;
    if (!current_host_owner_view(&camera_mode, &scene_manager)) return FALSE;
    return SudekiMpLanArenaOwnerViewService(
        &host_tal_skill_view_lease,
        camera_mode, scene_manager, boundary);
}

static BOOL capture_host_tal_skill_view(uint8_t remote_skill_slot) {
    void *camera_mode;
    void *scene_manager;
    if (host_tal_skill_view_lease.valid) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (!current_host_owner_view(&camera_mode, &scene_manager) ||
        !SudekiMpLanArenaOwnerViewCapture(
            &host_tal_skill_view_lease,
            camera_mode, scene_manager)) {
        return FALSE;
    }
    host_tal_skill_view_slot = remote_skill_slot;
    /* Capture happens immediately before the bounded remote Use call, so it
     * is a valid fallback if activation begins between the two RenderStarts. */
    host_tal_skill_view_basis_safe_this_frame = TRUE;
    host_tal_skill_view_trace_state = -1;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_remote_skill_view state=captured "
        "owner=Tal remote_caster=Ailish slot=%u camera=0x%08lx "
        "render_state=0x%08lx revision=%lu "
        "policy=exact_render_only_owner_view_lease_dynamic_basis\r\n",
        (unsigned int)remote_skill_slot,
        (unsigned long)(uintptr_t)host_tal_skill_view_lease.camera,
        (unsigned long)(uintptr_t)host_tal_skill_view_lease.render_state,
        (unsigned long)host_tal_skill_view_lease.refresh_revision);
    return TRUE;
}

static BOOL retire_host_tal_skill_view(const char *reason) {
    BOOL restored;
    uint8_t slot;
    if (!host_tal_skill_view_lease.valid) return TRUE;
    slot = host_tal_skill_view_slot;
    restored = service_host_tal_skill_view(
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE);
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_remote_skill_view state=%s "
        "owner=Tal remote_caster=Ailish slot=%u reason=%s "
        "policy=restore_exact_latest_owner_basis_before_releasing_lease\r\n",
        restored ? "restored" : "restore_rejected",
        (unsigned int)slot,
        reason != NULL ? reason : "unspecified");
    if (!restored) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    host_tal_skill_view_slot = 0u;
    host_tal_skill_view_basis_safe_this_frame = FALSE;
    host_tal_skill_view_trace_state = -1;
    return TRUE;
}

static BOOL __attribute__((thiscall)) preserve_host_tal_camera(
    void *manager,
    const char *name
) {
    CameraManagerSetRenderCameraFunction original =
        (CameraManagerSetRenderCameraFunction)
            host_skill_camera_hook.trampoline;
    if (!host_remote_skill_camera_owned()) {
        host_remote_skill_camera_suppression_logged = FALSE;
        return original(manager, name);
    }
    if (seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE && name != NULL) {
        if (runtime_readable_memory(name, 8u) && memcmp(name, "InitCam", 8u) == 0)
            host_spirit_camera_kind = 1u;
        else if (runtime_readable_memory(name, 9u) && memcmp(name, "SkillCam", 9u) == 0)
            host_spirit_camera_kind = 2u;
        else host_spirit_camera_kind = 0u;
    }
    if (!host_remote_skill_camera_suppression_logged) {
        host_remote_skill_camera_suppression_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_remote_skill_camera "
            "state=preserved owner=Tal remote_caster=Ailish requested=%s "
            "policy=authoritative_effect_does_not_steal_other_players_view\r\n",
            name == NULL ? "(null)" : name);
    }
    return TRUE;
}

static void __attribute__((thiscall)) preserve_host_realtime(
    void *game_speed,
    int requested_mode
) {
    GameSpeedSetModeFunction original =
        (GameSpeedSetModeFunction)host_skill_speed_hook.trampoline;
    BOOL success = set_host_skill_realtime_scale(TRUE);
    int trace_state = success ? 1 : 0;

    if (trace_state != host_skill_speed_trace_state || requested_mode != 0) {
        host_skill_speed_trace_state = trace_state;
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_skill_speed state=%s "
            "requested_mode=%d applied_scale=1.0 "
            "policy=lan_world_and_all_player_views_remain_realtime\r\n",
            success ? "realtime" : "rejected", requested_mode);
    }
    original(game_speed, requested_mode);
}

static BOOL host_cast_owner_witness(void *actor, uint8_t kind,
    uint64_t *session, uint8_t *type) {
    SudekiMpLanArenaSessionStatus status;
    void *local, *remote;
    if (!actor || !session || !type || (kind != 1u && kind != 2u) ||
        !runtime_installed || !tal_initialized || !ailish_initialized ||
        !host_remote_ailish_owned ||
        runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        runtime_config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        !SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
        !status.session_token || status.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status.peer_simulation_node_role != SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA)
        return FALSE;
    local=SudekiMpCleanroomEngineActorEntity(seat_host_actor());
    remote=SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    if (!local || !remote || local==remote || (actor!=local && actor!=remote)) return FALSE;
    *session=status.session_token;
    *type=(uint8_t)(actor==local ? seat_host_type() : seat_client_type());
    return TRUE;
}

static BOOL runtime_cast_owner_witness(void *actor,uint8_t kind,uint64_t *session,uint8_t *type) {
    SudekiMpLanArenaSessionStatus status;
    void *local,*remote;
    if(runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL)
        return host_cast_owner_witness(actor,kind,session,type);
    if(!actor || !session || !type || (kind!=1 && kind!=2) || !runtime_installed ||
        !tal_initialized || !ailish_initialized || !client_remote_tal_owned ||
        runtime_config.local_role!=SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ||
        !SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected || !status.session_token ||
        status.local_simulation_node_role!=SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA ||
        status.peer_simulation_node_role!=SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD)
        return FALSE;
    local=SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    remote=SudekiMpCleanroomEngineActorEntity(seat_host_actor());
    if(!local || !remote || local==remote || (actor!=local && actor!=remote)) return FALSE;
    *session=status.session_token;
    *type=(uint8_t)(actor==local ? seat_client_type():seat_host_type());
    return TRUE;
}

static BOOL runtime_skill_ui_initialized,runtime_skill_ui_bound;
static void *runtime_skill_ui_local,*runtime_skill_ui_remote;
static uint64_t runtime_skill_ui_session;
/* The host retains two native manager/camera lifetimes, not a scope held over
 * a frame. Activation, VM tasks and each native scheduled tick enter/leave
 * their exact owner. Client Spirit remains presentation-only. */
static SudekiMpSpiritInstance runtime_spirit_instances[2];
static BOOL runtime_spirit_entry_routed,runtime_spirit_skill_routed;
static BOOL runtime_spirit_tasks_routed,runtime_spirit_observer_bound;
static BOOL runtime_spirit_menu_owned,runtime_skill_ui_retiring;
static int runtime_spirit_last_strike;
static BOOL runtime_skill_ui_retained(void *actor,uint64_t session) {
    BOOL host=runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    return runtime_installed && session && session==runtime_skill_ui_session &&
        (actor==runtime_skill_ui_local || actor==runtime_skill_ui_remote) &&
        runtime_skill_ui_local==SudekiMpCleanroomEngineActorEntity(host ? seat_host_actor():seat_client_actor()) &&
        runtime_skill_ui_remote==SudekiMpCleanroomEngineActorEntity(host ? seat_client_actor():seat_host_actor());
}
static BOOL runtime_skill_ui_task(void *actor,uint64_t session) {
    SudekiMpLanCastOwner owner;
    return runtime_skill_ui_retained(actor,session) &&
        SudekiMpLanCastContextCurrentRetained(&owner) && owner.kind==1 &&
        owner.actor==actor && owner.session==session;
}
static BOOL runtime_skill_ui_idle(void) {
    SudekiMpCharacterSkillState state;
    int spirit;
    if(!runtime_installed || !SudekiMpLanCastContextDrained() ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit) return FALSE;
    for(unsigned int i=0;i<2;++i) {
        SudekiMpSpiritInstanceState owned;
        void *actor=SudekiMpCleanroomEngineActorEntity(i ? seat_client_actor():seat_host_actor());
        if(!actor || !SudekiMpObserveCharacterSkill(actor,&state) || state.active) return FALSE;
        if(runtime_spirit_instances[i].generation &&
            (!SudekiMpObserveSpiritInstance(&runtime_spirit_instances[i],&owned) || !owned.idle))
            return FALSE;
    }
    return TRUE;
}
static BOOL runtime_spirit_presentation(int *state,int *strike) {
    unsigned int active=0;
    int observed=0,id=runtime_spirit_last_strike;
    if(!state || !strike || !runtime_skill_ui_initialized) return FALSE;
    for(unsigned int i=0;i<2;++i) {
        SudekiMpSpiritInstanceState owned;
        if(!SudekiMpObserveSpiritInstance(&runtime_spirit_instances[i],&owned)) return FALSE;
        if(owned.state) {
            int first;
            if(++active>1 || !SudekiMpResolveSpiritStrikeId(i ? seat_client_type():seat_host_type(),1,&first) ||
                (owned.strike_id!=(uint32_t)first && owned.strike_id!=(uint32_t)(first+1))) return FALSE;
            observed=(int)owned.state; id=(int)owned.strike_id;
        }
    }
    /* LA35 is still serialized. More than one active manager is UNKNOWN,
     * never an arbitrary winner or an artificial retirement edge. */
    *state=observed; *strike=id;
    if(active) runtime_spirit_last_strike=id;
    return TRUE;
}
static BOOL runtime_spirit_admission(void *actor,uint8_t kind,
    SudekiMpSpiritInstance *instance) {
    uint64_t session;
    uint8_t type;
    return runtime_skill_ui_bound && !runtime_skill_ui_retiring &&
        host_cast_owner_witness(actor,kind,&session,&type) && session==runtime_skill_ui_session &&
        SudekiMpResolveSpiritInstanceCaster(actor,session,instance);
}
static uint32_t runtime_spirit_entry_enter(void *actor,int strike,BOOL activating,void **manager) {
    SudekiMpSpiritInstance instance;
    int first;
    unsigned int type;
    if(!actor) actor=SudekiMpCleanroomEngineActorEntity(seat_host_actor());
    if(!manager || !runtime_spirit_admission(actor,2,&instance)) return 0;
    type=actor==runtime_skill_ui_local ? seat_host_type():seat_client_type();
    if(!SudekiMpResolveSpiritStrikeId(type,1,&first) || (strike!=first && strike!=first+1) ||
        (activating && !runtime_skill_ui_idle())) return 0;
    uint32_t cookie=SudekiMpEnterSpiritInstance(&instance);
    if(cookie) *manager=instance.manager;
    return cookie;
}
static uint32_t runtime_spirit_skill_enter(void *skill,int slot,BOOL using_skill) {
    SudekiMpSpiritInstance instance;
    SudekiMpCharacterSkillState state;
    void *actor;
    if(!runtime_readable_memory(skill,0x78) || slot<0 || slot>=6) return 0;
    actor=*(void **)((uint8_t *)skill+0x10);
    if(!runtime_spirit_admission(actor,1,&instance) ||
        !SudekiMpObserveCharacterSkill(actor,&state) || state.skill!=skill ||
        (using_skill && !runtime_skill_ui_idle())) return 0;
    return SudekiMpEnterSpiritInstance(&instance);
}
static uint32_t runtime_spirit_task_enter(const SudekiMpLanCastOwner *owner) {
    SudekiMpSpiritInstance instance;
    if(!owner) return SudekiMpEnterSpiritInstance(NULL);
    /* Cleanup uses the retained actor/session, including after disconnect.
     * It is not authority to activate a new action in that old session. */
    if((owner->kind!=1 && owner->kind!=2) || !owner->cast_id ||
        !runtime_skill_ui_retained(owner->actor,owner->session) ||
        !SudekiMpResolveSpiritInstanceCaster(owner->actor,owner->session,&instance)) return 0;
    return SudekiMpEnterSpiritInstance(&instance);
}
static BOOL prepare_runtime_spirit_instances(void) {
    for(unsigned int i=0;i<2;++i) {
        if(!SudekiMpCreateSpiritInstance(&runtime_spirit_instances[i]) ||
            !SudekiMpBindSpiritInstanceCaster(&runtime_spirit_instances[i],
                i ? runtime_skill_ui_remote:runtime_skill_ui_local,
                (uint8_t)(i ? seat_client_type():seat_host_type()),runtime_skill_ui_session,
                runtime_skill_ui_retained)) return FALSE;
    }
    if(!SudekiMpEnableSpiritInstanceRemoteUi(&runtime_spirit_instances[1]) ||
        !SudekiMpEnableSpiritInstanceRemoteSkillUi(&runtime_spirit_instances[1]) ||
        !SudekiMpEnableSpiritInstanceRemoteSkillInput(&runtime_spirit_instances[1],
            runtime_skill_ui_local,runtime_skill_ui_retained) ||
        !SudekiMpEnableSpiritInstanceCastGates() || !SudekiMpEnableSpiritInstanceSharedSsp()) return FALSE;
    for(unsigned int i=0;i<2;++i)
        if(!SudekiMpScheduleSpiritInstanceManager(&runtime_spirit_instances[i])) return FALSE;
    if(!SudekiMpQuickSkillSpiritRoutingReady()) {
        if(!SudekiMpInstallQuickSkillInputTrace(runtime_game_module,FALSE,FALSE)) return FALSE;
        runtime_spirit_menu_owned=TRUE;
        if(!SudekiMpQuickSkillSpiritRoutingReady()) return FALSE;
    }
    if(!SudekiMpSetSpiritActivationRouting(runtime_skill_ui_idle,
        runtime_spirit_entry_enter,SudekiMpLeaveSpiritInstance)) return FALSE;
    runtime_spirit_entry_routed=TRUE;
    if(!SudekiMpSetSkillActivationRouting(runtime_skill_ui_idle,
        runtime_spirit_skill_enter,SudekiMpLeaveSpiritInstance)) return FALSE;
    runtime_spirit_skill_routed=TRUE;
    if(!SudekiMpLanCastContextSetTaskRouting(runtime_spirit_task_enter,SudekiMpLeaveSpiritInstance)) return FALSE;
    runtime_spirit_tasks_routed=TRUE;
    if(!SudekiMpCleanroomEngineSetSpiritPresentationObserver(runtime_spirit_presentation,
        runtime_skill_ui_idle)) return FALSE;
    runtime_spirit_observer_bound=TRUE;
    return TRUE;
}
static BOOL release_runtime_skill_ui(void) {
    if(!runtime_skill_ui_initialized) return TRUE;
    runtime_skill_ui_retiring=TRUE; /* Close activation even if restoration needs retry. */
    if(!runtime_skill_ui_idle()) return FALSE;
    if(runtime_spirit_menu_owned) {
        if(!SudekiMpUninstallQuickSkillInputTrace()) return FALSE;
        runtime_spirit_menu_owned=FALSE;
    }
    if(runtime_spirit_tasks_routed) {
        if(!SudekiMpLanCastContextSetTaskRouting(NULL,NULL)) return FALSE;
        runtime_spirit_tasks_routed=FALSE;
    }
    if(runtime_spirit_observer_bound) {
        if(!SudekiMpCleanroomEngineSetSpiritPresentationObserver(NULL,NULL)) return FALSE;
        runtime_spirit_observer_bound=FALSE;
    }
    /* Keep entry callbacks installed (and rejecting new admission) until all
     * native lifetimes and ABI hooks are gone. Failed deletion retains actor
     * witnesses; it never frees objects beneath scheduled/native work. */
    for(unsigned int i=2;i>0;--i)
        if(runtime_spirit_instances[i-1].generation &&
            !SudekiMpDestroySpiritInstance(&runtime_spirit_instances[i-1])) return FALSE;
    if(!SudekiMpResetSpiritInstanceAbi()) return FALSE;
    if(runtime_spirit_skill_routed) {
        if(!SudekiMpSetSkillActivationRouting(NULL,NULL,NULL)) return FALSE;
        runtime_spirit_skill_routed=FALSE;
    }
    if(runtime_spirit_entry_routed) {
        if(!SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL)) return FALSE;
        runtime_spirit_entry_routed=FALSE;
    }
    runtime_skill_ui_initialized=runtime_skill_ui_bound=FALSE;
    runtime_skill_ui_retiring=FALSE; runtime_spirit_last_strike=0;
    runtime_skill_ui_local=runtime_skill_ui_remote=NULL; runtime_skill_ui_session=0;
    SudekiMpLogWrite("lan_cast_ui event=unbound policy=drained_before_actor_or_session_replacement\r\n");
    return TRUE;
}
static BOOL service_runtime_skill_ui(void) {
#if defined(SUDEKIMP_CAST_INSTANCE_PROBE) || defined(SUDEKIMP_CAST_ACTIVE_PROBE)
    /* Diagnostics own this same ABI; no competing hook/namespace owners. */
    return TRUE;
#else
    SudekiMpLanArenaSessionStatus status;
    uint64_t session;
    uint8_t type;
    BOOL host=runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    void *remote=SudekiMpCleanroomEngineActorEntity(host ? seat_client_actor():seat_host_actor());
    if(runtime_skill_ui_retiring) return release_runtime_skill_ui();
    if(runtime_skill_ui_bound) {
        int spirit,id;
        if(host ? (!SudekiMpSpiritActivationRoutingHealthy() ||
                !SudekiMpSkillActivationRoutingHealthy() || !runtime_spirit_presentation(&spirit,&id)):
            !SudekiMpRemoteCharacterSkillUiHealthy()) return FALSE;
        if(!SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
            status.session_token!=runtime_skill_ui_session) return release_runtime_skill_ui();
        return TRUE;
    }
    if(!runtime_cast_owner_witness(remote,1,&session,&type) || !runtime_skill_ui_idle()) return TRUE;
    if(runtime_skill_ui_initialized && !release_runtime_skill_ui()) return FALSE;
    runtime_skill_ui_local=SudekiMpCleanroomEngineActorEntity(host ? seat_host_actor():seat_client_actor());
    runtime_skill_ui_remote=remote; runtime_skill_ui_session=session;
    if(!(host ? SudekiMpInitializeSpiritInstanceAbi(runtime_game_module,runtime_skill_ui_idle):
        SudekiMpInitializeSpiritInstanceAbiWithInputOwner(runtime_game_module,runtime_skill_ui_idle,
            SudekiMpLanArenaClientCharacterInputOwnerExact))) return FALSE;
    runtime_skill_ui_initialized=TRUE;
    if(!SudekiMpInstallSpiritInstanceUpdates() ||
        !(host ? prepare_runtime_spirit_instances():
            SudekiMpBindRemoteCharacterSkillUi(runtime_skill_ui_local,remote,type,session,
                runtime_skill_ui_retained,runtime_skill_ui_task))) {
        SudekiMpLogFormat("lan_cast_ui event=bind_failed error=%lu policy=retain_partial_hooks_until_safe_restore\r\n",
            (unsigned long)GetLastError());
        return FALSE;
    }
    runtime_skill_ui_bound=TRUE;
    SudekiMpLogFormat("lan_cast_ui event=bound role=%s remote_type=%u policy=owned_host_spirit_lifetimes_client_skill_ui_serialized_casts\r\n",
        host ? "host":"client",type);
    return TRUE;
#endif
}

static BOOL install_host_skill_isolation(uint8_t *base) {
    uint32_t initial_scale_bits;
    BOOL speed_restored;
    BOOL camera_restored;
    if (base == NULL || host_skill_camera_hook.installed ||
        host_skill_speed_hook.installed ||
        memcmp(base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            expected_camera_manager_set_render_camera_entry,
            sizeof(expected_camera_manager_set_render_camera_entry)) != 0 ||
        memcmp(base + RVA_GAME_SPEED_SET_MODE,
            expected_game_speed_set_mode_entry,
            sizeof(expected_game_speed_set_mode_entry)) != 0 ||
        memcmp(base + RVA_FIXED_ALTERNATE_SPEED,
            expected_fixed_alternate_speed,
            sizeof(expected_fixed_alternate_speed)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    memcpy(&initial_scale_bits, base + RVA_FIXED_ALTERNATE_SPEED,
        sizeof(initial_scale_bits));
    host_skill_original_alternate_speed_bits = initial_scale_bits;
    host_skill_speed_override_active = FALSE;
    host_remote_skill_camera_active = FALSE;
    host_remote_skill_camera_suppression_logged = FALSE;
    SudekiMpLanArenaOwnerViewClear(&host_tal_skill_view_lease);
    host_tal_skill_view_slot = 0u;
    host_tal_skill_view_basis_safe_this_frame = FALSE;
    host_tal_skill_view_trace_state = -1;
    host_skill_speed_trace_state = -1;
    InterlockedExchange(&host_remote_skill_activation_depth, 0);
    if (!SudekiMpInstallInlineHook(
            &host_skill_camera_hook,
            base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            expected_camera_manager_set_render_camera_entry,
            sizeof(expected_camera_manager_set_render_camera_entry),
            preserve_host_tal_camera)) {
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(
            &host_skill_speed_hook,
            base + RVA_GAME_SPEED_SET_MODE,
            expected_game_speed_set_mode_entry,
            sizeof(expected_game_speed_set_mode_entry),
            preserve_host_realtime) ||
        !SudekiMpInstallLanCastContext((HMODULE)base, host_cast_owner_witness)) {
        DWORD error = GetLastError();
        DWORD restore_error = ERROR_SUCCESS;
        if (!SudekiMpUninstallLanCastContext()) return FALSE;
        speed_restored = SudekiMpRestoreInlineHook(&host_skill_speed_hook);
        if (!speed_restored) restore_error = GetLastError();
        camera_restored = SudekiMpRestoreInlineHook(&host_skill_camera_hook);
        if (!camera_restored && restore_error == ERROR_SUCCESS) {
            restore_error = GetLastError();
        }
        if (!speed_restored || !camera_restored) {
            SetLastError(restore_error == ERROR_SUCCESS ?
                ERROR_WRITE_FAULT : restore_error);
        } else {
            SetLastError(error);
        }
        return FALSE;
    }
    return TRUE;
}

static BOOL restore_host_skill_isolation(void) {
    BOOL speed_restored;
    BOOL camera_restored;
    DWORD first_error = ERROR_SUCCESS;

    if(!release_runtime_skill_ui()) return FALSE;
#if defined(SUDEKIMP_CAST_INSTANCE_PROBE) || defined(SUDEKIMP_CAST_ACTIVE_PROBE)
    if (!SudekiMpResetSpiritInstanceAbi()) return FALSE;
#endif
    if (!SudekiMpUninstallLanCastContext()) return FALSE;

    if (InterlockedCompareExchange(
            &host_remote_skill_activation_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (host_tal_skill_view_lease.valid &&
        !retire_host_tal_skill_view("host_skill_isolation_restore")) {
        return FALSE;
    }
    if (host_skill_speed_override_active &&
        !set_host_skill_realtime_scale(FALSE)) {
        return FALSE;
    }
    speed_restored = SudekiMpRestoreInlineHook(&host_skill_speed_hook);
    if (!speed_restored) first_error = GetLastError();
    camera_restored = SudekiMpRestoreInlineHook(&host_skill_camera_hook);
    if (!camera_restored && first_error == ERROR_SUCCESS) {
        first_error = GetLastError();
    }
    if (!speed_restored || !camera_restored) {
        SetLastError(first_error == ERROR_SUCCESS ?
            ERROR_WRITE_FAULT : first_error);
        return FALSE;
    }
    host_skill_original_alternate_speed_bits = 0u;
    host_skill_speed_override_active = FALSE;
    host_remote_skill_camera_active = host_tal_skill_view_lease.valid;
    host_remote_skill_camera_suppression_logged = FALSE;
    host_skill_speed_trace_state = -1;
    return TRUE;
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

static BOOL rollback_host_spirit_audio_trace(void) {
    DWORD error = ERROR_SUCCESS;
    /* Both observers are independent restoration obligations. Never clear
     * either one's callback dependency if its native lease is still live. */
    if (!SudekiMpLanArenaSpiritVisualHostReset()) {
        error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_WRITE_FAULT;
    }
    if (SudekiMpLanArenaSpiritAudioTraceInstalled() &&
        !SudekiMpUninstallLanArenaSpiritAudioTrace() && error == ERROR_SUCCESS) {
        error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_WRITE_FAULT;
    }
    if (error != ERROR_SUCCESS) {
        retain_runtime_after_hook_restore_failure(error);
        return FALSE;
    }
    return TRUE;
}

static BOOL rollback_lan_arena_frame_hooks(void) {
    DWORD restore_error;
    if (!SudekiMpUninstallLanArenaSkillFade()) {
        retain_runtime_after_hook_restore_failure(GetLastError());
        return FALSE;
    }
    if (restore_lan_arena_frame_hooks()) return TRUE;
    restore_error = GetLastError();
    retain_runtime_after_hook_restore_failure(restore_error);
    return FALSE;
}

static BOOL rollback_lan_arena_campaign_guard(void) {
    DWORD restore_error;
    if (SudekiMpUninstallLanArenaCampaignGuard()) return TRUE;
    restore_error = GetLastError();
    retain_runtime_after_hook_restore_failure(
        restore_error == ERROR_SUCCESS ? ERROR_WRITE_FAULT : restore_error);
    return FALSE;
}

static BOOL uninstall_lan_arena_input_adapters(void) {
    BOOL client_restored;
    BOOL host_restored;
    DWORD first_error = ERROR_SUCCESS;

    client_restored = SudekiMpUninstallLanArenaClientInput();
    if (!client_restored) first_error = GetLastError();
    host_restored = SudekiMpUninstallLanArenaHostInput();
    if (!host_restored && first_error == ERROR_SUCCESS) {
        first_error = GetLastError();
    }
    if (!client_restored || !host_restored) {
        SetLastError(first_error == ERROR_SUCCESS ?
            ERROR_WRITE_FAULT : first_error);
        return FALSE;
    }
    return TRUE;
}

static BOOL host_spirit_audio_active_witness(
    void *context,
    int *native_state
) {
    int state;
    if (context != &runtime_config || native_state == NULL ||
        runtime_game_module == NULL || !runtime_installed ||
        runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        runtime_config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        !tal_initialized ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&state)) {
        return FALSE;
    }
    *native_state = state;
    return state != 0;
}

static BOOL host_spirit_owner(unsigned int *actor_index) {
    int id, first;
    unsigned int i;
    if (actor_index == NULL || !SudekiMpCleanroomEngineSpiritStrikeId(&id))
        return FALSE;
    for (i = 0u; i < 2u; ++i) {
        unsigned int type = i == 0u ? seat_host_type() : seat_client_type();
        if (SudekiMpResolveSpiritStrikeId(type, 1u, &first) &&
            (id == first || id == first + 1)) {
            *actor_index = i;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL host_spirit_visual_active_witness(
    void *context, uint64_t *session_token, uint16_t *skill_sequence,
    uint32_t *host_tick, uint8_t *owner_type
) {
    SudekiMpLanArenaSessionStatus status;
    uint16_t sequence;
    int native_state;
    unsigned int actor_index;
    if (session_token == NULL || skill_sequence == NULL || host_tick == NULL || owner_type == NULL ||
        !ailish_initialized ||
        !host_spirit_audio_active_witness(context, &native_state) ||
        !SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
        status.session_token == 0u ||
        status.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status.peer_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA ||
        !host_spirit_owner(&actor_index) ||
        (host_spirit_previous_active && actor_index != host_spirit_actor_index)) return FALSE;
    sequence = host_actor_skill_sequence[actor_index];
    if (!host_spirit_previous_active) {
        /* Native creation may precede the next 20Hz observation. Predict
         * exactly the same next nonzero sequence host_apply_spirit_state
         * will allocate, without changing native gameplay in an observer. */
        ++sequence;
        if (sequence == 0u) sequence = 1u;
    }
    if (sequence == 0u) return FALSE;
    *session_token = status.session_token;
    *skill_sequence = sequence;
    *host_tick = GetTickCount();
    *owner_type = actor_index == 0u && seat_host_type() != SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        0u : (actor_index == 0u ? seat_host_type() : seat_client_type());
    return TRUE;
}

static void host_capture_spirit_view(SudekiMpLanArenaSnapshot *snapshot) {
    uint8_t *base = (uint8_t *)runtime_game_module, *manager, *camera = NULL;
    uint8_t *state, *actor, *position, *wrapper, *body;
    SudekiMpLanArenaSpiritView view = {0};
    const char *name = host_spirit_camera_kind == 1u ? "InitCam" : "SkillCam";
    unsigned int i;
    if (!base || host_spirit_camera_kind == 0u ||
        snapshot->seat[1].actor_type != SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        !snapshot->seat[1].skill_active || snapshot->seat[1].skill_kind !=
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT ||
        !runtime_readable_memory(base + 0x409d7cu, sizeof(void *))) return;
    manager = *(uint8_t **)(base + 0x409d7cu);
    if (!runtime_readable_memory(manager, 0x4cu) ||
        *(void **)manager != base + 0x2c7b80u) return;
    /* Same bounded named table as exact-build GELGetCamera; observation only. */
    for (i = 0u; i < 10u; ++i) {
        uint8_t *candidate = *(uint8_t **)(manager + 0x24u + i*4u);
        if (runtime_readable_memory(candidate, 0x108u) &&
            *(void **)candidate == base + 0x2cce5cu &&
            memcmp(candidate + 0x4cu, name, strlen(name)+1u) == 0) {
            camera = candidate; break;
        }
    }
    if (!camera) return;
    state = *(uint8_t **)(camera + 0x34u);
    actor = SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    if (!runtime_readable_memory(state, 0xdcu) || !runtime_readable_memory(actor, 0x48u)) return;
    position = *(uint8_t **)(actor + 0x44u);
    if (!runtime_readable_memory(position, 0xb8u) || *(void **)(position + 0x10u) != actor) return;
    wrapper = *(uint8_t **)(position + 0xb4u);
    if (!runtime_readable_memory(wrapper, 0x14u)) return;
    body = *(uint8_t **)(wrapper + 8u);
    if (!runtime_readable_memory(body, 0x38u)) return;
    view.kind = host_spirit_camera_kind; view.owner_seat = 1u;
    view.skill_sequence = snapshot->seat[1].skill_sequence;
    view.body_hidden = (*(uint32_t *)(body + 0x34u) & 4u) != 0u;
    memcpy(view.matrix, state + 0x90u, sizeof(view.matrix));
    memcpy(view.projection, state + 0xd0u, sizeof(view.projection));
    if (SudekiMpLanArenaSpiritViewValid(&view)) snapshot->spirit_view = view;
    {
        static DWORD last_trace;
        DWORD now = GetTickCount();
        if ((DWORD)(now - last_trace) >= 1000u) {
            last_trace = now;
            SudekiMpLogFormat("lan_arena_runtime event=host_spirit_view kind=%u sequence=%u admitted=%u hidden=%u\r\n",
                view.kind, view.skill_sequence, snapshot->spirit_view.kind != 0u, view.body_hidden);
        }
    }
}

static void host_capture_skill_fade(SudekiMpLanArenaSnapshot *snapshot) {
    SudekiMpLanArenaSessionStatus status;
    float current[3], baseline[3];
    unsigned int i, count=0u, owner=0u;
    if (!snapshot || !SudekiMpLanArenaSessionGetStatus(&status) ||
        !status.peer_connected || !status.session_token ||
        !SudekiMpLanArenaReadSkillLight(current, baseline)) return;
    if (host_skill_fade_session != status.session_token) {
        memset(&host_skill_fade, 0, sizeof(host_skill_fade));
        host_skill_fade_actor=NULL; host_skill_fade_session=status.session_token;
    }
    for(i=0u;i<2u;++i) if(snapshot->seat[i].skill_active) { ++count; owner=i; }
    /* A global native stack cannot identify the contributor during overlap.
     * Publish UNKNOWN rather than assign another actor's fade to the viewer. */
    if(count>1u) return;
    if(count==1u) {
        host_skill_fade.owner_seat=(uint8_t)owner;
        host_skill_fade.kind=snapshot->seat[owner].skill_kind;
        host_skill_fade.skill_sequence=snapshot->seat[owner].skill_sequence;
        host_skill_fade_actor=SudekiMpCleanroomEngineActorEntity(
            owner==0u ? seat_host_actor() : seat_client_actor());
    }
    owner=host_skill_fade.owner_seat;
    if(!host_skill_fade.kind || !host_skill_fade_actor ||
        host_skill_fade_actor!=SudekiMpCleanroomEngineActorEntity(
            owner==0u ? seat_host_actor() : seat_client_actor()) ||
        host_skill_fade.skill_sequence!=snapshot->seat[owner].skill_sequence ||
        host_skill_fade.kind!=snapshot->seat[owner].skill_kind) return;
    if(!count && !memcmp(current,baseline,sizeof(current))) {
        memset(&host_skill_fade,0,sizeof(host_skill_fade)); host_skill_fade_actor=NULL;
        return;
    }
    memcpy(host_skill_fade.rgb,current,sizeof(current));
    if(SudekiMpLanArenaSkillFadeValid(&host_skill_fade)) snapshot->skill_fade=host_skill_fade;
}

static BOOL caster_view_light(float rgb[3]) {
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaSkillFade fade;
    float current[3], baseline[3];
    unsigned int owner=0u, count=0u, i;
    int spirit_state;
    if(!tal_initialized || !ailish_initialized ||
        !SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
        !status.session_token || !SudekiMpLanArenaReadSkillLight(current,baseline)) return FALSE;
    if(runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        if(!SudekiMpLanArenaClientReplicaGetSkillFade(&fade)) return FALSE;
        memcpy(rgb,fade.owner_seat==1u ? fade.rgb : baseline,12);
        return TRUE;
    }
    /* Observe live native ownership at the draw boundary: the first dark
     * frame can precede the next 20Hz snapshot. Do not allocate sequences here. */
    if(!SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state)) return FALSE;
    if(spirit_state) {
        if(!host_spirit_owner(&owner)) return FALSE;
        count=1u;
    }
    for(i=0u;i<2u;++i) {
        SudekiMpCharacterSkillState skill;
        void *actor=SudekiMpCleanroomEngineActorEntity(i==0u ? seat_host_actor() : seat_client_actor());
        if(!actor || !SudekiMpObserveCharacterSkill(actor,&skill)) return FALSE;
        if(skill.active) { ++count; owner=i; }
    }
    if(count>1u) return FALSE;
    if(!count) {
        if(!host_skill_fade.kind || host_skill_fade_session!=status.session_token ||
            host_skill_fade_actor!=SudekiMpCleanroomEngineActorEntity(
                host_skill_fade.owner_seat==0u ? seat_host_actor() : seat_client_actor())) return FALSE;
        owner=host_skill_fade.owner_seat;
    }
    if(owner==0u) return FALSE; /* Host caster keeps the unmodified native fade. */
    memcpy(rgb,baseline,12);
    return TRUE;
}

static BOOL reset_client_replica_for_teardown(const char *reason) {
    DWORD error;
    if (runtime_config.local_role !=
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        return TRUE;
    }
    SudekiMpLanArenaClientReplicaDiscardSnapshots();
    if(!SudekiMpLanCastContextDrained() || !release_runtime_skill_ui()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if (SudekiMpResetLanArenaClientReplica()) return TRUE;
    error = GetLastError();
    SudekiMpLogFormat(
        "lan_arena_runtime event=client_replica_teardown state=deferred "
        "reason=%s win32_error=%lu "
        "policy=retain_actor_frame_callbacks_and_damage_containment_until_native_skill_drains\r\n",
        reason != NULL ? reason : "unspecified",
        (unsigned long)error);
    SetLastError(error);
    return FALSE;
}

static uint32_t resource_snapshot_value(float value) {
    if (value <= 0.0f) return 0u;
    if (value >= (float)SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE) {
        return SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE;
    }
    return (uint32_t)value;
}

static BOOL initialize_party_actor_equipment(SudekiMpCleanroomActor actor) {
    void *character = SudekiMpCleanroomEngineActorEntity(actor);
    /* Equip from this hero's bounded category before the legacy cleanroom
     * initializer can invoke the exported Ailish-only SetWeapon(int). */
    return character != NULL &&
        SudekiMpGrantTestroomCharacterWeapons(character, GetCommandLineA()) &&
        SudekiMpEnsureCharacterStarterWeapon(character) &&
        SudekiMpCleanroomEngineInitializePartyActor(actor);
}

static uint16_t host_advance_skill_sequence(unsigned int actor_index) {
    if (actor_index >= 2u) return 0u;
    ++host_actor_skill_sequence[actor_index];
    if (host_actor_skill_sequence[actor_index] == 0u) {
        host_actor_skill_sequence[actor_index] = 1u;
    }
    return host_actor_skill_sequence[actor_index];
}

static BOOL host_begin_character_skill_sequence(
    unsigned int actor_index,
    void *character,
    void *skill,
    int slot,
    uint32_t cost,
    BOOL active_seen,
    const char *source
) {
    const SudekiMpCleanroomActor actors[2] = {
        seat_host_actor(),
        seat_client_actor()
    };
    HostNativeSkillLease *lease;
    uint16_t sequence;
    if (actor_index >= 2u || character == NULL || skill == NULL ||
        slot < 0 || slot >= 6 ||
        SudekiMpCleanroomEngineActorEntity(actors[actor_index]) != character) {
        return FALSE;
    }
    sequence = host_advance_skill_sequence(actor_index);
    if (sequence == 0u) return FALSE;
    lease = &host_native_skill_leases[actor_index];
    lease->character = character;
    lease->skill = skill;
    lease->slot = slot;
    lease->wire_sequence = sequence;
    lease->pending = TRUE;
    lease->active_seen = active_seen;
    lease->sequence_allocated = TRUE;
    host_actor_skill_kind[actor_index] =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    host_actor_skill_slot[actor_index] = (uint8_t)slot;
    host_actor_skill_cost[actor_index] = cost;
    if (actor_index == 0u) {
        (void)SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
            TRUE);
    }
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_skill phase=started actor=%s "
        "sequence=%u slot=%u cost=%lu source=%s "
        "policy=exact_start_allocates_once_snapshot_observation_adopts_lease\r\n",
        actor_index == 0u ? "Tal" : "Ailish",
        (unsigned int)sequence,
        (unsigned int)slot,
        (unsigned long)cost,
        source != NULL ? source : "unknown");
    return TRUE;
}

static BOOL host_native_skill_startup_pending(
    unsigned int actor_index,
    void *character,
    const SudekiMpCharacterSkillState *state
) {
    HostNativeSkillLease *lease;
    if (actor_index >= 2u || character == NULL || state == NULL) return FALSE;
    lease = &host_native_skill_leases[actor_index];
    return lease->pending && !lease->active_seen &&
        lease->sequence_allocated && lease->character == character &&
        lease->skill != NULL && state->skill == lease->skill &&
        state->active == 0u && lease->slot >= 0 && lease->slot < 6 &&
        lease->wire_sequence != 0u &&
        lease->wire_sequence == host_actor_skill_sequence[actor_index] &&
        host_actor_skill_kind[actor_index] ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
        host_actor_skill_slot[actor_index] == (uint8_t)lease->slot;
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
    SudekiMpWeaponQuickList weapons;
    unsigned int slot;
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
    /* LA27 carries the actor's own bounded family slot for melee too. */
    if (SudekiMpDescribeCharacterWeapons(
            SudekiMpCleanroomEngineActorEntity(actor), &weapons)) {
        for (slot = 0u; slot < weapons.row_count && slot < 12u; ++slot) {
            if (weapons.rows[slot].equipped) {
                snapshot->weapon_slot_plus_one = (uint8_t)(slot + 1u);
                break;
            }
        }
    }
    return TRUE;
}

static void host_track_started_native_skill(
    unsigned int actor_index,
    void *character,
    const SudekiMpSkillActivationResult *result
) {
    if (actor_index >= 2u || character == NULL || result == NULL ||
        result->status != SUDEKIMP_SKILL_ACTIVATION_STARTED) return;
    (void)host_begin_character_skill_sequence(
        actor_index, character, result->skill, result->slot, 0u,
        FALSE, actor_index == 0u ?
            "host_operator_started" : "remote_operator_started");
}

static void host_native_tal_skill_started(
    void *character,
    void *skill,
    int slot,
    uint32_t cost,
    BOOL active_seen
) {
    if (runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) return;
    (void)host_begin_character_skill_sequence(
        0u, character, skill, slot, cost, active_seen,
        "native_tal_admission_callsite");
}

static BOOL host_reconcile_native_skill_lease(
    unsigned int actor_index,
    void *character,
    const SudekiMpCharacterSkillState *state
) {
    HostNativeSkillLease *lease;
    BOOL active;
    if (actor_index >= 2u || character == NULL || state == NULL) return FALSE;
    active = state->active != 0u;
    if (active && (state->slot < 0 || state->slot >= 6)) return FALSE;
    lease = &host_native_skill_leases[actor_index];
    if (!lease->pending) {
        if (!active) return TRUE;
        lease->character = character;
        lease->skill = state->skill;
        lease->slot = state->slot;
        lease->pending = TRUE;
        lease->active_seen = TRUE;
        lease->sequence_allocated = FALSE;
        lease->wire_sequence = 0u;
        return state->skill != NULL;
    }
    if (lease->character != character || lease->skill == NULL ||
        state->skill != lease->skill) return FALSE;
    if (active) {
        if (state->slot != lease->slot) {
            /* A different exact active slot is a positive new transaction
             * even if an unhooked native source hid the intervening inactive
             * edge.  Same-slot cycles are covered by the two exact host input
             * admission callsites. */
            lease->slot = state->slot;
            lease->wire_sequence = 0u;
            lease->active_seen = TRUE;
            lease->sequence_allocated = FALSE;
            return TRUE;
        }
        lease->active_seen = TRUE;
        return TRUE;
    }
    /* A STARTED return followed by an early inactive read is a startup gap,
     * not completion. Require the exact task to have been observed active
     * before its later inactive state may retire the host lease. */
    if (!lease->active_seen) return FALSE;
    ZeroMemory(lease, sizeof(*lease));
    return TRUE;
}

static BOOL host_apply_skill_state(
    unsigned int actor_index,
    SudekiMpCleanroomActor actor,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    void *character;
    SudekiMpCharacterSkillState state;
    BOOL active;

    if (actor_index >= 2u || snapshot == NULL) return FALSE;
    character = SudekiMpCleanroomEngineActorEntity(actor);
    if (character == NULL ||
        !SudekiMpObserveCharacterSkill(character, &state)) {
        /* Unknown is not an inactive edge. Preserve the current sequence,
         * camera/input isolation, and ownership until a readable native
         * observation proves retirement. The caller drops this snapshot. */
        return FALSE;
    }
    active = state.active != 0u && state.slot >= 0 && state.slot < 6;
    if (!host_reconcile_native_skill_lease(
            actor_index, character, &state)) return FALSE;
    if (active) {
        HostNativeSkillLease *lease =
            &host_native_skill_leases[actor_index];
        if (!lease->sequence_allocated) {
            if (!host_begin_character_skill_sequence(
                    actor_index, character, state.skill, state.slot,
                    state.cost, TRUE, "snapshot_active_fallback")) {
                return FALSE;
            }
            lease = &host_native_skill_leases[actor_index];
        }
        if (lease->wire_sequence == 0u ||
            lease->wire_sequence != host_actor_skill_sequence[actor_index] ||
            host_actor_skill_kind[actor_index] !=
                SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER ||
            host_actor_skill_slot[actor_index] != (uint8_t)state.slot) {
            return FALSE;
        }
        /* STARTED may precede the first readable active/cost observation.
         * Fill the exact native cost without allocating another sequence. */
        host_actor_skill_cost[actor_index] = state.cost;
    }
    if (actor_index == 1u && host_remote_skill_camera_active && !active &&
        host_actor_previous_skill_active[actor_index]) {
        if (!retire_host_tal_skill_view("native_remote_skill_completed")) {
            /* The native task is gone, but camera/input ownership remains
             * leased until the exact latest Tal basis is restored. */
            return FALSE;
        }
        host_remote_skill_camera_active = FALSE;
        host_remote_skill_camera_suppression_logged = FALSE;
        (void)SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(FALSE);
        SudekiMpLogWrite(
            "lan_arena_runtime event=host_remote_skill_camera "
            "state=released owner=Tal remote_caster=Ailish "
            "policy=native_remote_skill_task_completed\r\n");
    }
    if (actor_index == 0u && !active &&
        host_actor_previous_skill_active[actor_index]) {
        (void)SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
            FALSE);
    }
    host_actor_previous_skill_active[actor_index] = active;
    snapshot->skill_sequence = host_actor_skill_sequence[actor_index];
    if (snapshot->skill_sequence != 0u) {
        snapshot->skill_kind = host_actor_skill_kind[actor_index];
        snapshot->skill_slot = host_actor_skill_slot[actor_index];
        snapshot->skill_cost = host_actor_skill_cost[actor_index];
        snapshot->skill_active = active ? 1u : 0u;
    }
    return TRUE;
}

static BOOL host_apply_spirit_state(
    SudekiMpLanArenaSnapshot *snapshot
) {
    int state;
    BOOL active;
    unsigned int i;
    if (snapshot == NULL ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&state)) return FALSE;
    active = state != 0;
    if (active) {
        unsigned int owner;
        if (!host_spirit_owner(&owner) ||
            (host_spirit_previous_active && owner != host_spirit_actor_index)) return FALSE;
        host_spirit_actor_index = owner;
    }
    i = host_spirit_actor_index;
    if (active && !host_spirit_previous_active) {
        ++host_actor_skill_sequence[i];
        if (host_actor_skill_sequence[i] == 0u) {
            host_actor_skill_sequence[i] = 1u;
        }
        host_actor_skill_kind[i] =
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
        host_actor_skill_slot[i] = 0u;
        host_actor_skill_cost[i] = 0u;
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_spirit phase=started actor_type=%u "
            "sequence=%u native_state=%d "
            "policy=global_host_transaction_distinct_from_cskill\r\n",
            snapshot->seat[i].actor_type, (unsigned int)host_actor_skill_sequence[i], state);
    }
    if (active) {
        host_actor_skill_kind[i] =
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
        host_actor_skill_slot[i] = 0u;
        host_actor_skill_cost[i] = 0u;
        if (i == 0u)
            (void)SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(TRUE);
    } else if (host_spirit_previous_active) {
        if (i == 1u && host_remote_skill_camera_active) {
            if (!retire_host_tal_skill_view("native_remote_spirit_completed")) return FALSE;
            host_remote_skill_camera_active = FALSE;
            host_remote_skill_camera_suppression_logged = FALSE;
            (void)SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(FALSE);
        }
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_spirit phase=completed actor_type=%u "
            "sequence=%u previous_native_state=%d "
            "policy=global_host_transaction_retired\r\n",
            snapshot->seat[i].actor_type, (unsigned int)host_actor_skill_sequence[i],
            host_spirit_previous_state);
    }
    host_spirit_previous_active = active;
    host_spirit_previous_state = state;
    if (host_actor_skill_sequence[i] != 0u &&
        host_actor_skill_kind[i] ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
        snapshot->seat[i].skill_sequence = host_actor_skill_sequence[i];
        snapshot->seat[i].skill_kind =
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
        snapshot->seat[i].skill_slot = 0u;
        snapshot->seat[i].skill_cost = 0u;
        snapshot->seat[i].skill_active = active ? 1u : 0u;
    }
    return TRUE;
}

static void reset_host_operator_spirit_intent(void) {
    ZeroMemory(&host_operator_spirit_intent,
        sizeof(host_operator_spirit_intent));
}

static void discard_host_operator_spirit_requests(const char *reason) {
    BOOL retained_intent = host_operator_spirit_intent.pending;
    BOOL raw_request = SudekiMpLanArenaHostInputDiscardSpiritRequests();
    reset_host_operator_spirit_intent();
    if (retained_intent || raw_request) {
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=discarded "
            "reason=%s retained_intent=%s raw_request=%s "
            "policy=no_cross_session_operator_admission\r\n",
            reason != NULL ? reason : "authority_reset",
            retained_intent ? "true" : "false",
            raw_request ? "true" : "false");
    }
}

static BOOL host_operator_spirit_session_ready(
    const SudekiMpLanArenaSessionStatus *status
) {
    if (status == NULL ||
        status->phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED ||
        !status->peer_connected || status->session_token == 0u ||
        status->local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        status->local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status->peer_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) {
        host_operator_spirit_session_token = 0u;
        discard_host_operator_spirit_requests(
            "authenticated_session_generation_inactive");
        return FALSE;
    }
    if (host_operator_spirit_session_token == status->session_token) {
        return TRUE;
    }
    discard_host_operator_spirit_requests(
        "authenticated_session_generation_changed");
    host_operator_spirit_session_token = status->session_token;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_operator_spirit "
        "phase=session_armed session_token=%08lx%08lx "
        "policy=first_authenticated_pass_discards_pre_session_events\r\n",
        (unsigned long)(status->session_token >> 32),
        (unsigned long)status->session_token);
    return FALSE;
}

static const char *host_operator_spirit_context_rejection(
    const SudekiMpLanArenaSessionStatus *status,
    void *expected_tal,
    void *expected_ailish,
    uint64_t expected_session_token,
    LONG expected_activation_depth,
    void **tal_result,
    void **ailish_result
) {
    void *tal;
    void *ailish;
    SudekiMpCharacterSkillState tal_skill;
    SudekiMpCharacterSkillState ailish_skill;
    BOOL combat_enabled = FALSE;
    int spirit_state = 0;

    if (tal_result != NULL) *tal_result = NULL;
    if (ailish_result != NULL) *ailish_result = NULL;
    if (!runtime_installed || runtime_game_module == NULL || status == NULL ||
        runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        runtime_config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status->phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED ||
        !status->peer_connected || status->session_token == 0u ||
        status->local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        status->local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        status->peer_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) {
        return "authenticated_canonical_host_required";
    }
    if (host_operator_spirit_session_token != status->session_token) {
        return "session_generation_not_armed";
    }
    if (expected_session_token != 0u &&
        status->session_token != expected_session_token) {
        return "session_token_changed";
    }
    if (!tal_initialized || !ailish_initialized ||
        !host_remote_ailish_owned) {
        return "party_control_lease_not_ready";
    }
    if (SudekiMpLanArenaPausePanelActive() ||
        SudekiMpCleanroomMenuActive()) {
        return "menu_or_pause_active";
    }
    if (SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        return "native_ui_transition_active";
    }
    if (!SudekiMpCleanroomEngineCombatMode(&combat_enabled) ||
        !combat_enabled) {
        return "native_combat_inactive";
    }
    tal = SudekiMpCleanroomEngineActorEntity(seat_host_actor());
    ailish = SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    if (tal == NULL || ailish == NULL) {
        return "party_identity_unavailable";
    }
    if (expected_tal != NULL && tal != expected_tal) {
        return "tal_identity_changed";
    }
    if (expected_ailish != NULL && ailish != expected_ailish) {
        return "ailish_identity_changed";
    }
    if (!SudekiMpLanArenaHostInputTalControllerLeaseExact(tal)) {
        return "tal_controller_lease_not_exact";
    }
    if (!SudekiMpControlSeparationPlayerTwoActive() ||
        SudekiMpControlSeparationPlayerTwoCharacter() != ailish) {
        return "ailish_player_two_lease_not_exact";
    }
    if (InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0) !=
                expected_activation_depth) {
        return "operator_spirit_activation_in_progress";
    }
    if (InterlockedCompareExchange(
            &host_remote_skill_activation_depth, 0, 0) != 0 ||
        host_native_skill_leases[0].pending ||
        host_native_skill_leases[1].pending ||
        host_actor_previous_skill_active[0] ||
        host_actor_previous_skill_active[1] ||
        !SudekiMpObserveCharacterSkill(tal, &tal_skill) ||
        !SudekiMpObserveCharacterSkill(ailish, &ailish_skill) ||
        tal_skill.active != 0u || ailish_skill.active != 0u) {
        return "native_character_skill_active_or_unknown";
    }
    if (!SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state) ||
        spirit_state != 0 || host_spirit_previous_active) {
        return "native_spirit_active_or_unknown";
    }
    if (tal_result != NULL) *tal_result = tal;
    if (ailish_result != NULL) *ailish_result = ailish;
    return NULL;
}

static void service_host_operator_spirit(
    const SudekiMpLanArenaSessionStatus *status, unsigned int remote_variant
) {
    HostOperatorSpiritIntent intent;
    SudekiMpSpiritQuickOptionList options;
    SudekiMpSpiritActivationResult result;
    unsigned int requested_variant;
    void *tal = NULL;
    void *ailish = NULL;
    const char *rejection;
    unsigned int actor_index = remote_variant != 0u ? 1u : 0u;

    if (host_operator_spirit_intent.pending) {
        if (remote_variant != 0u) {
            SudekiMpLogWrite("lan_arena_runtime event=host_remote_spirit phase=rejected reason=concurrent_request\r\n");
            return;
        }
        if (SudekiMpLanArenaHostInputTakeSpiritVariant(&requested_variant)) {
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_operator_spirit phase=rejected "
                "variant=%u reason=concurrent_request "
                "policy=one_two_phase_native_intent_at_a_time\r\n",
                requested_variant);
        }
        if (SudekiMpCleanroomEngineRangedCombatPrimePending()) return;
        if (InterlockedCompareExchange(
                &host_operator_spirit_activation_depth, 0, 0) != 0) {
            return;
        }

        /* The cleanroom engine positively retired its game-thread UI lease.
         * Publish a distinct in-flight barrier before any native reproof call.
         * Keep the intent visible through reproof, then erase it immediately
         * before activation; either witness independently blocks teardown. */
        intent = host_operator_spirit_intent;
        InterlockedIncrement(&host_operator_spirit_activation_depth);
        rejection = host_operator_spirit_context_rejection(
            status, intent.seat[0], intent.seat[1], intent.session_token,
            1, &tal, &ailish);
        if (rejection == NULL &&
            (!host_operator_spirit_intent.pending ||
             host_operator_spirit_intent.seat[0] != intent.seat[0] ||
             host_operator_spirit_intent.seat[1] != intent.seat[1] ||
             host_operator_spirit_intent.session_token !=
                 intent.session_token ||
             host_operator_spirit_intent.actor_index != intent.actor_index ||
             intent.actor_index >= 2u ||
             host_operator_spirit_intent.variant != intent.variant)) {
            rejection = "intent_revoked_during_reproof";
        }
        if (rejection != NULL) {
            reset_host_operator_spirit_intent();
            InterlockedDecrement(&host_operator_spirit_activation_depth);
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_operator_spirit phase=rejected "
                "variant=%u reason=%s stage=post_prime_reproof "
                "policy=no_stale_or_reentrant_native_entry\r\n",
                intent.variant, rejection);
            return;
        }
        reset_host_operator_spirit_intent();
        if (intent.actor_index == 1u &&
            (!SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(TRUE) ||
             !capture_host_tal_skill_view(0u))) {
            if (!host_tal_skill_view_lease.valid)
                (void)SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(FALSE);
            InterlockedDecrement(&host_operator_spirit_activation_depth);
            SudekiMpLogWrite("lan_arena_runtime event=host_remote_spirit phase=rejected reason=owner_view_not_exact\r\n");
            return;
        }
        if (intent.actor_index == 1u) host_spirit_camera_kind = 0u;
        result = SudekiMpActivateCharacterSpirit(
            intent.actor_index == 0u ? tal : ailish, intent.variant);
        if (intent.actor_index == 1u) {
            if (result.status == SUDEKIMP_SPIRIT_ACTIVATION_STARTED) {
                host_remote_skill_camera_active = TRUE;
                (void)service_host_tal_skill_view(
                    SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION);
            } else if (retire_host_tal_skill_view("remote_spirit_activation_rejected")) {
                (void)SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(FALSE);
            } else {
                host_remote_skill_camera_active = TRUE;
            }
        }
        InterlockedDecrement(&host_operator_spirit_activation_depth);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=%s "
            "actor_type=%u variant=%u strike_id=%d validation=%d activation=%d "
            "policy=native_second_validation_and_activation_wire_edge_observed_later\r\n",
            SudekiMpSpiritActivationStatusName(result.status),
            intent.actor_index == 0u ? seat_host_type() : seat_client_type(),
            intent.variant, result.strike_id, result.validation_result,
            result.activation_result);
        return;
    }

    requested_variant = remote_variant;
    if (remote_variant == 0u &&
        !SudekiMpLanArenaHostInputTakeSpiritVariant(&requested_variant)) return;
    if (InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 1, 0) != 0) {
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=rejected "
            "variant=%u reason=operator_spirit_entry_in_progress "
            "stage=initial_admission\r\n",
            requested_variant);
        return;
    }
    rejection = host_operator_spirit_context_rejection(
        status, NULL, NULL, 0u, 1, &tal, &ailish);
    if (rejection != NULL) {
        InterlockedDecrement(&host_operator_spirit_activation_depth);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=rejected "
            "variant=%u reason=%s stage=initial_admission\r\n",
            requested_variant, rejection);
        return;
    }
    ZeroMemory(&options, sizeof(options));
    if (!SudekiMpDescribeCharacterSpiritOptions(actor_index == 0u ? tal : ailish, &options) ||
        options.resource_type != (actor_index == 0u ? seat_host_type() : seat_client_type()) ||
        options.option_count != 2u || requested_variant < 1u ||
        requested_variant > options.option_count ||
        options.options[requested_variant - 1u].variant != requested_variant ||
        options.options[requested_variant - 1u].available == 0u ||
        options.options[requested_variant - 1u].validation_result != 0) {
        InterlockedDecrement(&host_operator_spirit_activation_depth);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=rejected "
            "variant=%u reason=native_option_unavailable "
            "resource_type=%u option_count=%u validation=%d\r\n",
            requested_variant, options.resource_type, options.option_count,
            requested_variant >= 1u && requested_variant <= 2u ?
                options.options[requested_variant - 1u].validation_result : -1);
        return;
    }
    /* Native option validation is a synchronous manager entry. Re-prove the
     * entire actor/session/control context after it returns and before the UI
     * lease is acquired; the in-flight depth keeps teardown fail-closed for
     * both validation passes. */
    rejection = host_operator_spirit_context_rejection(
        status, tal, ailish, status->session_token, 1, &tal, &ailish);
    if (rejection != NULL) {
        InterlockedDecrement(&host_operator_spirit_activation_depth);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=rejected "
            "variant=%u reason=%s stage=pre_prime_reproof\r\n",
            requested_variant, rejection);
        return;
    }
    if (SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEnginePrimeRangedCombat() ||
        !SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        InterlockedDecrement(&host_operator_spirit_activation_depth);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=rejected "
            "variant=%u reason=native_ui_prime_not_owned error=%lu\r\n",
            requested_variant, (unsigned long)GetLastError());
        return;
    }
    if (!runtime_installed || runtime_game_module == NULL ||
        host_operator_spirit_session_token != status->session_token) {
        InterlockedDecrement(&host_operator_spirit_activation_depth);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_spirit phase=rejected "
            "variant=%u reason=authority_revoked_during_ui_prime "
            "policy=engine_prime_remains_teardown_barrier_until_retired\r\n",
            requested_variant);
        return;
    }
    host_operator_spirit_intent.seat[0] = tal;
    host_operator_spirit_intent.seat[1] = ailish;
    host_operator_spirit_intent.session_token = status->session_token;
    host_operator_spirit_intent.variant = requested_variant;
    host_operator_spirit_intent.actor_index = actor_index;
    host_operator_spirit_intent.pending = TRUE;
    InterlockedDecrement(&host_operator_spirit_activation_depth);
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_operator_spirit phase=primed "
        "actor_type=%u variant=%u session_token=%08lx%08lx delay_ms=75 "
        "policy=callback_free_game_thread_positive_retirement_required\r\n",
        (unsigned int)(actor_index == 0u ? seat_host_type() : seat_client_type()),
        requested_variant,
        (unsigned long)(status->session_token >> 32),
        (unsigned long)status->session_token);
}

static void host_apply_skill_presentation(
    unsigned int actor_index,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    const SudekiMpCleanroomActorPresentation *presentation;
    unsigned int channel;
    unsigned int channel_count;
    if (actor_index >= 2u || snapshot == NULL ||
        snapshot->skill_sequence == 0u || snapshot->skill_active == 0u ||
        !host_actor_presentation_valid[actor_index]) return;
    presentation = &host_actor_presentation[actor_index];
    channel_count = SudekiMpCleanroomActorIsRanged(
            actor_index == 0u ? seat_host_actor() : seat_client_actor()) ?
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS :
        snapshot->actor_type == SUDEKIMP_LAN_ARENA_BUKI_TYPE ? 4u : 2u;
    snapshot->skill_presentation_valid = 1u;
    snapshot->skill_presentation_channel_count = (uint8_t)channel_count;
    for (channel = 0u; channel < channel_count; ++channel) {
        snapshot->skill_presentation_selector[channel] =
            presentation->selector[channel];
        snapshot->skill_presentation_state[channel] =
            presentation->state[channel];
        snapshot->skill_presentation_rate[channel] =
            presentation->rate[channel];
        snapshot->skill_presentation_time[channel] =
            presentation->time[channel];
    }
    for (channel = 0u;
         channel < SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_BLENDS;
         ++channel) {
        snapshot->skill_presentation_blend[channel] =
            presentation->blend[channel];
    }
    /* Character CSkill runs locally on the replica; this host renderer sample
     * is only an optional witness. A long-lived dormant native channel can
     * exceed the bounded wire clock even while a new skill is healthy. Never
     * let that optional value suppress the authoritative sequence/start edge.
     * Spirit is different: its client path is presentation-only, so retain
     * the strict invalid snapshot behavior when its required sidecar is bad. */
    if (snapshot->skill_kind ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
        !SudekiMpLanArenaSkillPresentationValid(
            snapshot,
            actor_index == 0u ? seat_host_type() :
                seat_client_type())) {
        snapshot->skill_presentation_valid = 0u;
        snapshot->skill_presentation_channel_count = 0u;
        ZeroMemory(snapshot->skill_presentation_selector,
            sizeof(snapshot->skill_presentation_selector));
        ZeroMemory(snapshot->skill_presentation_state,
            sizeof(snapshot->skill_presentation_state));
        ZeroMemory(snapshot->skill_presentation_rate,
            sizeof(snapshot->skill_presentation_rate));
        ZeroMemory(snapshot->skill_presentation_time,
            sizeof(snapshot->skill_presentation_time));
        ZeroMemory(snapshot->skill_presentation_blend,
            sizeof(snapshot->skill_presentation_blend));
    }
}

static BOOL host_spirit_audio_raw_start(
    const SudekiMpLanArenaSpiritAudioEvent *event
) {
    static const char cue[] = "spiritstrike_start";
    return event != NULL && event->native_state != 0 &&
        event->cue_length == sizeof(cue) - 1u &&
        memcmp(event->cue, cue, sizeof(cue) - 1u) == 0;
}

static void reset_host_spirit_audio_tracking(void) {
    SudekiMpLanArenaSpiritAudioEvent events[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
    size_t count = 0u;
    if (SudekiMpLanArenaSpiritAudioTraceInstalled()) {
        count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
            events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, NULL);
    }
    ZeroMemory(host_spirit_audio_history,
        sizeof(host_spirit_audio_history));
    host_spirit_audio_history_count = 0u;
    host_spirit_audio_event_sequence = 0u;
    host_spirit_audio_trace_sequence =
        count != 0u ? events[count - 1u].sequence : 0u;
    host_spirit_audio_trace_sequence_initialized = count != 0u;
}

static void host_capture_spirit_audio(
    SudekiMpLanArenaActorSnapshot *tal_snapshot,
    SudekiMpLanArenaSnapshot *snapshot,
    HostSpiritAudioStage *stage
) {
    SudekiMpLanArenaSpiritAudioEvent events[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
    const SudekiMpLanArenaSpiritAudioEvent *latest_start = NULL;
    size_t count;
    size_t index;

    if (tal_snapshot == NULL || snapshot == NULL || stage == NULL) return;
    ZeroMemory(stage, sizeof(*stage));
    memcpy(stage->history, host_spirit_audio_history,
        sizeof(stage->history));
    stage->history_count = host_spirit_audio_history_count;
    stage->event_sequence = host_spirit_audio_event_sequence;
    stage->trace_sequence = host_spirit_audio_trace_sequence;
    stage->trace_sequence_initialized =
        host_spirit_audio_trace_sequence_initialized;
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, NULL);
    for (index = 0u; index < count; ++index) {
        const SudekiMpLanArenaSpiritAudioEvent *event = &events[index];
        BOOL fresh = !stage->trace_sequence_initialized ||
            SudekiMpLanArenaSequenceNewer(
                event->sequence, stage->trace_sequence);
        if (!fresh) continue;
        stage->trace_sequence = event->sequence;
        stage->trace_sequence_initialized = TRUE;
        if (host_spirit_audio_raw_start(event)) latest_start = event;
    }
    if (latest_start != NULL && tal_snapshot->skill_active != 0u &&
        tal_snapshot->skill_kind ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
        tal_snapshot->skill_sequence != 0u &&
        (stage->history_count == 0u ||
         stage->history[
             stage->history_count - 1u].skill_sequence !=
                tal_snapshot->skill_sequence ||
         stage->history[stage->history_count - 1u].owner_seat != host_spirit_actor_index)) {
        SudekiMpLanArenaSpiritAudioSemanticEvent *wire_event;
        if (stage->history_count ==
                SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY) {
            memmove(&stage->history[0],
                &stage->history[1],
                (SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY - 1u) *
                    sizeof(stage->history[0]));
            --stage->history_count;
        }
        wire_event = &stage->history[stage->history_count++];
        ZeroMemory(wire_event, sizeof(*wire_event));
        ++stage->event_sequence;
        if (stage->event_sequence == 0u) ++stage->event_sequence;
        wire_event->event_sequence = stage->event_sequence;
        wire_event->skill_sequence = tal_snapshot->skill_sequence;
        wire_event->cue = SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
        wire_event->owner_seat = (uint8_t)host_spirit_actor_index;
        stage->journaled_raw_trace_sequence = latest_start->sequence;
    }
    snapshot->spirit_audio_history_count =
        stage->history_count;
    memcpy(snapshot->spirit_audio_history,
        stage->history,
        sizeof(snapshot->spirit_audio_history));
}

static void commit_host_spirit_audio_stage(
    const HostSpiritAudioStage *stage
) {
    const SudekiMpLanArenaSpiritAudioSemanticEvent *wire_event;
    if (stage == NULL) return;
    wire_event = stage->history_count != 0u ?
        &stage->history[stage->history_count - 1u] : NULL;
    if (stage->journaled_raw_trace_sequence != 0u && wire_event != NULL) {
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_spirit_audio state=journaled "
            "event_sequence=%u skill_sequence=%u raw_trace_sequence=%lu "
            "cue=start policy=canonical_commit_semantic_allowlist_no_wire_string\r\n",
            (unsigned int)wire_event->event_sequence,
            (unsigned int)wire_event->skill_sequence,
            (unsigned long)stage->journaled_raw_trace_sequence);
    }
    memcpy(host_spirit_audio_history, stage->history,
        sizeof(host_spirit_audio_history));
    host_spirit_audio_history_count = stage->history_count;
    host_spirit_audio_event_sequence = stage->event_sequence;
    host_spirit_audio_trace_sequence = stage->trace_sequence;
    host_spirit_audio_trace_sequence_initialized =
        stage->trace_sequence_initialized;
}

static void reset_host_skill_tracking(void) {
    unsigned int actor_index;
    ZeroMemory(&host_ailish_locomotion, sizeof(host_ailish_locomotion));
    ZeroMemory(&host_tal_locomotion, sizeof(host_tal_locomotion));
    ZeroMemory(host_actor_skill_sequence,
        sizeof(host_actor_skill_sequence));
    ZeroMemory(host_actor_skill_kind, sizeof(host_actor_skill_kind));
    ZeroMemory(host_actor_skill_slot, sizeof(host_actor_skill_slot));
    ZeroMemory(host_actor_skill_cost, sizeof(host_actor_skill_cost));
    ZeroMemory(host_actor_previous_skill_active,
        sizeof(host_actor_previous_skill_active));
    host_spirit_previous_active = FALSE;
    host_spirit_actor_index = 0u;
    host_spirit_camera_kind = 0u;
    host_spirit_previous_state = 0;
    reset_host_spirit_audio_tracking();
    reset_host_operator_spirit_intent();
    host_operator_spirit_session_token = 0u;
    ZeroMemory(host_native_skill_leases,
        sizeof(host_native_skill_leases));
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        SudekiMpNoncasterSkillLocomotionRelease(
            &host_noncaster_locomotion_leases[actor_index]);
        host_noncaster_locomotion_trace_state[actor_index] = -1;
    }
    host_remote_skill_camera_active = host_tal_skill_view_lease.valid;
    host_remote_skill_camera_suppression_logged = FALSE;
    if (!host_tal_skill_view_lease.valid) {
        (void)SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(FALSE);
    }
    (void)SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
        FALSE);
    InterlockedExchange(&host_remote_skill_activation_depth, 0);
    /* Every production caller reaches this reset only before hook publication
     * or after host_native_tasks_drained positively proved no native entry. */
    InterlockedExchange(&host_operator_spirit_activation_depth, 0);
}

static BOOL host_native_tasks_drained(void) {
    SudekiMpCharacterSkillState state;
    const SudekiMpCleanroomActor actors[2] = {
        seat_host_actor(),
        seat_client_actor()
    };
    int spirit_state = 0;
    BOOL tal_present = FALSE;
    unsigned int actor_index;

    if (runtime_config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        return TRUE;
    }
    if (!SudekiMpLanCastContextDrained()) return FALSE;
    if (host_operator_spirit_intent.pending) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (InterlockedCompareExchange(
            &host_remote_skill_activation_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    /* Actor presence, not our later initialization flags, is the native-task
     * lifetime witness. Tal can start a local skill or Spirit while the arena
     * is still waiting for its peer. */
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        HostNativeSkillLease *lease =
            &host_native_skill_leases[actor_index];
        void *character = SudekiMpCleanroomEngineActorEntity(
            actors[actor_index]);
        if (actor_index == 0u && character != NULL) tal_present = TRUE;
        if (character == NULL && !lease->pending) continue;
        if (character == NULL ||
            !SudekiMpObserveCharacterSkill(character, &state) ||
            !host_reconcile_native_skill_lease(
                actor_index, character, &state) ||
            host_native_skill_leases[actor_index].pending ||
            state.active != 0u) {
            SetLastError(ERROR_BUSY);
            return FALSE;
        }
    }
    if ((tal_present || host_spirit_previous_active) &&
        (!SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state) ||
         spirit_state != 0)) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (host_tal_skill_view_lease.valid &&
        !retire_host_tal_skill_view("host_native_tasks_drained")) {
        return FALSE;
    }
    return TRUE;
}

#if defined(SUDEKIMP_CAST_INSTANCE_PROBE)
/* Private, opt-in native lifetime test. Never grants cast/menu admission.
 * Run at the verified game-thread observer only after connected actor/task
 * leases prove idle. Failure retains instances and prevents adapter teardown. */
static BOOL spirit_instance_probe_attempted;
static SudekiMpSpiritInstance spirit_instance_probe[2];
static DWORD spirit_instance_update_probe_started;
static BOOL spirit_instance_update_probe_pending;
static void *spirit_instance_probe_actors[2];
static uint64_t spirit_instance_probe_session;
static void *spirit_instance_probe_named_manager;
static void *spirit_instance_probe_named_originals[2];
static BOOL spirit_instance_probe_caster_witness(void *actor,uint64_t session) {
    return runtime_installed && tal_initialized && session &&
        session==spirit_instance_probe_session &&
        ((actor==spirit_instance_probe_actors[0] &&
          actor==SudekiMpCleanroomEngineActorEntity(seat_host_actor())) ||
         (actor==spirit_instance_probe_actors[1] &&
          actor==SudekiMpCleanroomEngineActorEntity(seat_client_actor())));
}
typedef void *(__attribute__((thiscall)) *ProbeNamedCameraGet)(void *,const char *);
static BOOL prepare_spirit_probe_named_cameras(uint8_t *image) {
    unsigned int i;
    ProbeNamedCameraGet get=(ProbeNamedCameraGet)(image+0x36ed0);
    if(!SudekiMpSpiritInstanceNamedCameraAbiReady() ||
        !runtime_readable_memory(image+0x409d7c,4)) return FALSE;
    spirit_instance_probe_named_manager=*(void **)(image+0x409d7c);
    if(!runtime_readable_memory(spirit_instance_probe_named_manager,0x60) ||
        *(void **)spirit_instance_probe_named_manager!=image+0x2c7b80) return FALSE;
    for(i=0;i<2;++i) {
        uint8_t type;
        uint64_t session;
        void *actor=SudekiMpCleanroomEngineActorEntity(i ? seat_client_actor():seat_host_actor());
        if(!host_cast_owner_witness(actor,2,&session,&type) ||
            (i && session!=spirit_instance_probe_session)) return FALSE;
        spirit_instance_probe_session=session; spirit_instance_probe_actors[i]=actor;
        if(!SudekiMpBindSpiritInstanceCaster(&spirit_instance_probe[i],actor,type,session,
            spirit_instance_probe_caster_witness) ||
            !SudekiMpEnableSpiritInstanceNamedCameras(&spirit_instance_probe[i])) return FALSE;
    }
    /* Creation checked the entire named registry, not just its manager. No
     * callback occurs between that check and these original-name lookups. */
    spirit_instance_probe_named_originals[0]=get(spirit_instance_probe_named_manager,"InitCam");
    spirit_instance_probe_named_originals[1]=get(spirit_instance_probe_named_manager,"SkillCam");
    return spirit_instance_probe_named_originals[0] && spirit_instance_probe_named_originals[1];
}
static BOOL check_spirit_probe_named_lookup(uint8_t *image,unsigned int owner) {
    unsigned int k;
    ProbeNamedCameraGet get=(ProbeNamedCameraGet)(image+0x36ed0);
    if(!runtime_readable_memory(image+0x409d7c,4) ||
        *(void **)(image+0x409d7c)!=spirit_instance_probe_named_manager ||
        !runtime_readable_memory(spirit_instance_probe_named_manager,0x60) ||
        *(void **)spirit_instance_probe_named_manager!=image+0x2c7b80) return FALSE;
    for(k=0;k<2;++k) {
        void *expected=spirit_instance_probe_named_originals[k];
        void *owned=NULL;
        /* This validates every retained slot/name before entering the retail
         * lookup, including the neutral-scope checks after a failed leave. */
        if(!SudekiMpObserveSpiritInstanceNamedCamera(&spirit_instance_probe[owner<2 ? owner:0],k+1,&owned)) return FALSE;
        if(owner<2) expected=owned;
        if(!expected || get(spirit_instance_probe_named_manager,k ? "SkillCam":"InitCam")!=expected) return FALSE;
    }
    return TRUE;
}
static BOOL spirit_instance_probe_idle(void) {
    uint64_t session;
    uint8_t type;
    return host_cast_owner_witness(SudekiMpCleanroomEngineActorEntity(seat_host_actor()),
        2,&session,&type) && host_native_tasks_drained();
}
static void poll_spirit_instance_update_probe(void) {
    uint32_t cameras[2]={0}, souls[2]={0};
    BOOL observed=TRUE, destroyed=TRUE, reset;
    unsigned int i;
    DWORD elapsed=GetTickCount()-spirit_instance_update_probe_started;
    for(i=0;i<2;++i) if(!SudekiMpObserveSpiritInstanceUpdates(
            &spirit_instance_probe[i],&cameras[i],&souls[i])) observed=FALSE;
    if(observed && elapsed<10000u &&
        (elapsed<1000u || !cameras[0] || !cameras[1] || !souls[0] || !souls[1])) return;
    /* Even a failed measurement must wait for unrelated gameplay to drain;
     * it is not permission to cancel somebody's newly started native cast. */
    if(!spirit_instance_probe_idle()) return;
    for(i=2;i>0;--i) if(spirit_instance_probe[i-1].generation &&
        !SudekiMpDestroySpiritInstance(&spirit_instance_probe[i-1])) destroyed=FALSE;
    reset=SudekiMpResetSpiritInstanceAbi();
    SudekiMpLogFormat("lan_spirit_instance_probe phase=native_updates observed=%u "
        "camera0=%lu camera1=%lu souls0=%lu souls1=%lu elapsed_ms=%lu destroyed=%u reset=%u "
        "policy=registered_idle_updates_only_no_overlap_claim\r\n",observed,
        (unsigned long)cameras[0],(unsigned long)cameras[1],(unsigned long)souls[0],
        (unsigned long)souls[1],(unsigned long)elapsed,destroyed,reset);
    spirit_instance_update_probe_pending=FALSE;
}
static void run_spirit_instance_probe(void) {
    uint8_t before_manager[0xc4], before_camera[0x1f0];
    uint8_t *image=(uint8_t *)GetModuleHandleW(NULL);
    void *manager,*camera;
    BOOL created, destroyed=TRUE, unchanged, routed=FALSE, reset;
    BOOL named_requested=FALSE,named_routed=FALSE;
    uint8_t named_before[0x60]={0};
    unsigned int i;
    if(spirit_instance_update_probe_pending) { poll_spirit_instance_update_probe(); return; }
    if(spirit_instance_probe_attempted || !spirit_instance_probe_idle()) return;
    {
        char option[2]={0};
        DWORD length=GetEnvironmentVariableA("SUDEKIMP_CAST_PROBE_NAMED",option,sizeof(option));
        if(length && (length!=1 || option[0]!='1')) {
            spirit_instance_probe_attempted=TRUE;
            SudekiMpLogWrite("lan_spirit_instance_probe phase=disabled reason=invalid_named_camera_option\r\n");
            return;
        }
        named_requested=length==1;
    }
    manager=*(void **)(image+0x408d30); camera=*(void **)(image+0x408d38);
    if(!runtime_readable_memory(manager,sizeof(before_manager)) ||
        !runtime_readable_memory(camera,sizeof(before_camera)) ||
        *(void **)manager!=image+0x2ca30c || *(void **)camera!=image+0x2c5630 ||
        *(uint32_t *)((uint8_t *)camera+0x1a0)) return;
    spirit_instance_probe_attempted=TRUE;
    if(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,spirit_instance_probe_idle)) {
        SudekiMpLogFormat("lan_spirit_instance_probe phase=init_failed error=%lu\r\n",(unsigned long)GetLastError());
        return;
    }
    memcpy(before_manager,manager,sizeof(before_manager));
    memcpy(before_camera,camera,sizeof(before_camera));
    SudekiMpLogWrite("lan_spirit_instance_probe phase=begin policy=idle_native_lifetime_only\r\n");
    created=SudekiMpCreateSpiritInstance(&spirit_instance_probe[0]);
    if(created) created=SudekiMpCreateSpiritInstance(&spirit_instance_probe[1]);
    if(created && named_requested) {
        void *registry=*(void **)(image+0x409d7c);
        if(!runtime_readable_memory(registry,sizeof(named_before)) || *(void **)registry!=image+0x2c7b80) created=FALSE;
        else {
            memcpy(named_before,registry,sizeof(named_before));
            created=prepare_spirit_probe_named_cameras(image);
        }
    }
    SudekiMpLogFormat("lan_spirit_instance_probe phase=constructed success=%u error=%lu manager0=%p manager1=%p camera0=%p camera1=%p\r\n",
        created,(unsigned long)GetLastError(),spirit_instance_probe[0].manager,spirit_instance_probe[1].manager,
        spirit_instance_probe[0].camera,spirit_instance_probe[1].camera);
    if(created) {
        uint32_t outer=SudekiMpEnterSpiritInstance(&spirit_instance_probe[0]);
        if(named_requested) named_routed=outer && check_spirit_probe_named_lookup(image,0);
        uint32_t inner=outer ? SudekiMpEnterSpiritInstance(&spirit_instance_probe[1]):0;
        if(named_requested && (!inner || !check_spirit_probe_named_lookup(image,1))) named_routed=FALSE;
        uint32_t neutral=inner ? SudekiMpEnterSpiritInstance(NULL):0;
        if(named_requested && (!neutral || !check_spirit_probe_named_lookup(image,2))) named_routed=FALSE;
        routed=outer && inner && neutral && *(void **)(image+0x408d30)==manager &&
            *(void **)(image+0x408d38)==camera;
        if(neutral && !SudekiMpLeaveSpiritInstance(neutral)) routed=FALSE;
        if(named_requested && !check_spirit_probe_named_lookup(image,1)) named_routed=FALSE;
        if(inner && !SudekiMpLeaveSpiritInstance(inner)) routed=FALSE;
        if(named_requested && !check_spirit_probe_named_lookup(image,0)) named_routed=FALSE;
        if(outer && !SudekiMpLeaveSpiritInstance(outer)) routed=FALSE;
        if(named_requested && !check_spirit_probe_named_lookup(image,2)) named_routed=FALSE;
    }
    for(i=2;i>0;--i) if(spirit_instance_probe[i-1].generation &&
        !SudekiMpDestroySpiritInstance(&spirit_instance_probe[i-1])) destroyed=FALSE;
    unchanged=*(void **)(image+0x408d30)==manager && *(void **)(image+0x408d38)==camera &&
        !memcmp(manager,before_manager,sizeof(before_manager)) &&
        !memcmp(camera,before_camera,sizeof(before_camera));
    if(named_requested && (!runtime_readable_memory(spirit_instance_probe_named_manager,sizeof(named_before)) ||
        *(void **)(image+0x409d7c)!=spirit_instance_probe_named_manager ||
        memcmp(named_before,spirit_instance_probe_named_manager,sizeof(named_before)))) unchanged=FALSE;
    reset=SudekiMpResetSpiritInstanceAbi();
    SudekiMpLogFormat("lan_spirit_instance_probe phase=completed created=%u routed=%u destroyed=%u original_objects_unchanged=%u reset=%u policy=no_overlap_claim\r\n",
        created,routed,destroyed,unchanged,reset);
    if(named_requested) {
        SudekiMpLogFormat("lan_spirit_instance_probe phase=named_cameras native_lookup_routed=%u "
            "created=%u destroyed=%u original_registry_unchanged=%u reset=%u "
            "policy=idle_factory_namespace_and_retirement_only_no_cast_or_render_claim\r\n",
            named_routed,created,destroyed,unchanged,reset);
        return; /* No scheduled updates or hidden gameplay in this smoke test. */
    }
    if(!created || !routed || !destroyed || !unchanged || !reset) return;
    /* Fresh idle nodes default to native period -1 (disabled). Explicitly
     * schedule these test-only nodes via the native setter, without starting a
     * camera/model animation, then let UpdateMgr supply every tick. */
    created=SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,spirit_instance_probe_idle) &&
        SudekiMpInstallSpiritInstanceUpdates() &&
        SudekiMpCreateSpiritInstance(&spirit_instance_probe[0]) &&
        SudekiMpCreateSpiritInstance(&spirit_instance_probe[1]) &&
        SudekiMpScheduleIdleSpiritInstanceProbe(&spirit_instance_probe[0]) &&
        SudekiMpScheduleIdleSpiritInstanceProbe(&spirit_instance_probe[1]);
    if(!created) {
        for(i=2;i>0;--i) if(spirit_instance_probe[i-1].generation)
            (void)SudekiMpDestroySpiritInstance(&spirit_instance_probe[i-1]);
        reset=SudekiMpResetSpiritInstanceAbi();
        SudekiMpLogFormat("lan_spirit_instance_probe phase=update_setup_failed reset=%u error=%lu\r\n",
            reset,(unsigned long)GetLastError());
        return;
    }
    spirit_instance_update_probe_started=GetTickCount();
    spirit_instance_update_probe_pending=TRUE;
}
#endif

#if defined(SUDEKIMP_CAST_ACTIVE_PROBE)
/* One private testroom cast through constructor-owned native objects. Keep
 * LA35's single-cast admission and existing observation path unchanged. This
 * deliberately selects one context across the test's frames; it is NOT the
 * per-caster concurrent runtime or permission to open overlapping admission.
 * All native UpdateMgr calls still enter their object's own context. */
static SudekiMpSpiritInstance active_spirit_probe;
static SudekiMpSpiritInstance active_spirit_probe_peer;
static void *active_spirit_probe_peer_actor;
static uint8_t active_spirit_probe_peer_type;
static BOOL active_spirit_probe_gates,active_spirit_probe_gates_observed;
static BOOL active_spirit_probe_menu_adapter_owned;
static uint32_t active_spirit_probe_entry_queries,active_spirit_probe_entry_activations;
static uint32_t active_spirit_probe_menu_queries,active_spirit_probe_menu_activations;
static uint32_t active_spirit_probe_scope;
static BOOL active_spirit_probe_attempted, active_spirit_probe_seen;
static BOOL active_spirit_probe_retained_logged;
static DWORD active_spirit_probe_started;
static uint32_t active_spirit_probe_cast_id, active_spirit_probe_task_steps;
static uint32_t active_spirit_probe_neutral_steps;
static uint64_t active_spirit_probe_session;
static void *active_spirit_probe_actor;
static BOOL active_spirit_probe_owner_selected, active_spirit_probe_remote;
static void *active_spirit_probe_owner(void) {
    return SudekiMpCleanroomEngineActorEntity(active_spirit_probe_remote ?
        seat_client_actor() : seat_host_actor());
}
static BOOL active_spirit_probe_caster_witness(void *actor,uint64_t session) {
    /* This is a retained native lifetime witness, not new network authority.
     * Scene teardown waits for the probe's task/object drain, even if its peer
     * disconnects. No other actor/session may borrow this private binding. */
    return runtime_installed && tal_initialized && actor==active_spirit_probe_actor &&
        session==active_spirit_probe_session && session &&
        runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        actor==active_spirit_probe_owner();
}
static BOOL active_spirit_probe_peer_witness(void *actor,uint64_t session) {
    return runtime_installed && tal_initialized && actor==active_spirit_probe_peer_actor &&
        session==active_spirit_probe_session && session &&
        runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        actor==SudekiMpCleanroomEngineActorEntity(active_spirit_probe_remote ?
            seat_host_actor():seat_client_actor());
}
static BOOL prepare_active_spirit_probe_gates(void) {
    uint64_t session;
    if(!active_spirit_probe_gates) return TRUE;
    active_spirit_probe_peer_actor=SudekiMpCleanroomEngineActorEntity(active_spirit_probe_remote ?
        seat_host_actor():seat_client_actor());
    return host_cast_owner_witness(active_spirit_probe_peer_actor,2,&session,&active_spirit_probe_peer_type) &&
        session==active_spirit_probe_session &&
        SudekiMpCreateSpiritInstance(&active_spirit_probe_peer) &&
        SudekiMpBindSpiritInstanceCaster(&active_spirit_probe_peer,active_spirit_probe_peer_actor,
            active_spirit_probe_peer_type,session,active_spirit_probe_peer_witness) &&
        SudekiMpEnableSpiritInstanceRemoteUi(active_spirit_probe_remote ?
            &active_spirit_probe:&active_spirit_probe_peer) &&
        SudekiMpEnableSpiritInstanceRemoteSkillUi(active_spirit_probe_remote ?
            &active_spirit_probe:&active_spirit_probe_peer) &&
        SudekiMpEnableSpiritInstanceRemoteSkillInput(active_spirit_probe_remote ?
            &active_spirit_probe:&active_spirit_probe_peer,
            active_spirit_probe_remote ? active_spirit_probe_peer_actor:active_spirit_probe_actor,
            active_spirit_probe_remote ? active_spirit_probe_peer_witness:active_spirit_probe_caster_witness) &&
        SudekiMpEnableSpiritInstanceCastGates();
}
static uint32_t active_spirit_probe_entry_enter(void *actor,int strike,BOOL activating,void **manager) {
    SudekiMpSpiritInstance *instance;
    SudekiMpCharacterSkillState skill;
    uint64_t session;
    uint8_t type;
    int first;
    uint32_t cookie;
    unsigned int i;
    BOOL from_menu=actor==NULL;
    if(!actor) actor=SudekiMpCleanroomEngineActorEntity(seat_host_actor()); /* Native Q is local. */
    if(actor==active_spirit_probe_actor) instance=&active_spirit_probe;
    else if(active_spirit_probe_gates && actor==active_spirit_probe_peer_actor) instance=&active_spirit_probe_peer;
    else return 0;
    if(!manager || !host_cast_owner_witness(actor,2,&session,&type) ||
        session!=active_spirit_probe_session || !SudekiMpResolveSpiritStrikeId(type,1,&first) ||
        (strike!=first && strike!=first+1)) return 0;
    if(activating) {
        /* Retain LA35's serialized admission in this one-cast test. Routing
         * is not authority to start the peer while cameras/wire are singleton. */
        if(instance!=&active_spirit_probe || active_spirit_probe_seen) return 0;
        for(i=0;i<2;++i) {
            void *a=SudekiMpCleanroomEngineActorEntity(i ? seat_client_actor():seat_host_actor());
            if(!SudekiMpObserveCharacterSkill(a,&skill) || skill.active) return 0;
        }
        if(!runtime_readable_memory(instance->manager,0xc4) ||
            *(uint32_t *)((uint8_t *)instance->manager+0x5c)) return 0;
    }
    cookie=SudekiMpEnterSpiritInstance(instance);
    if(!cookie) return 0;
    *manager=instance->manager;
    if(activating) ++active_spirit_probe_entry_activations;
    else ++active_spirit_probe_entry_queries;
    if(from_menu) {
        if(activating) ++active_spirit_probe_menu_activations;
        else ++active_spirit_probe_menu_queries;
    }
    return cookie;
}
static void observe_active_spirit_probe_gates(void) {
    uint8_t *base=(uint8_t *)runtime_game_module,*gate,*manager=active_spirit_probe_peer.manager;
    SudekiMpSpiritValidateFunction validate=(SudekiMpSpiritValidateFunction)(base+0x10940);
    uint64_t session;
    uint8_t type,before_bit,restored_bit;
    SudekiMpSpiritQuickOptionList options;
    int strike,shared[2],owned[2];
    if(!active_spirit_probe_gates || active_spirit_probe_gates_observed ||
        !active_spirit_probe_seen || GetTickCount()-active_spirit_probe_started<2000 ||
        !host_cast_owner_witness(active_spirit_probe_peer_actor,2,&session,&type) ||
        session!=active_spirit_probe_session || type!=active_spirit_probe_peer_type ||
        !runtime_readable_memory(active_spirit_probe.manager,0xc4) ||
        *(uint32_t *)((uint8_t *)active_spirit_probe.manager+0x5c)!=10 ||
        !runtime_readable_memory(manager,0xc4) || *(void **)manager!=base+0x2ca30c ||
        *(uint32_t *)(manager+0x5c) ||
        !SudekiMpResolveSpiritStrikeId(type,1,&strike)) return;
    gate=*(void **)(base+0x408dd0);
    if(!runtime_readable_memory(gate,0xa19) || *(void **)gate!=base+0x2c7ad0) return;
    before_bit=gate[0xa18]&8;
    /* Same idle manager and strike IDs before/inside the peer scope. These
     * are retail eligibility queries, not activation or menu mutation. */
    shared[0]=validate(manager,strike); shared[1]=validate(manager,strike+1);
    /* Exercise the real actor API, initially seeing the other caster's
     * singleton. Its shared entry router must select both peer manager and
     * busy bank; this is no longer a hand-scoped direct-validator shortcut. */
    if(!SudekiMpDescribeCharacterSpiritOptions(active_spirit_probe_peer_actor,&options) ||
        options.resource_type!=type || options.option_count!=2 ||
        options.options[0].strike_id!=strike || options.options[1].strike_id!=strike+1) return;
    owned[0]=options.options[0].validation_result;
    owned[1]=options.options[1].validation_result;
    active_spirit_probe_gates_observed=TRUE;
    restored_bit=gate[0xa18]&8;
    SudekiMpLogFormat("lan_spirit_active_probe phase=peer_native_eligibility type=%u strike=%d "
        "shared=%d,%d owned=%d,%d busy_before=%u busy_restored=%u caster_state=%lu "
        "policy=actor_api_routed_validator_only_no_second_activation\r\n",type,strike,
        shared[0],shared[1],owned[0],owned[1],before_bit,restored_bit,
        (unsigned long)*(uint32_t *)((uint8_t *)active_spirit_probe.manager+0x5c));
}
static uint32_t active_spirit_probe_task_enter(const SudekiMpLanCastOwner *owner) {
    SudekiMpSpiritInstance instance;
    if(!owner) {
        ++active_spirit_probe_neutral_steps;
        return SudekiMpEnterSpiritInstance(NULL);
    }
    /* The lineage adapter has already matched the pinned native task and
     * cast generation. Select its retained actor/session binding, NOT the
     * diagnostic's chosen caster or the last global Spirit manager. Both
     * CSkill's VM/cleanup and Spirit tasks use this coordinator path. */
    if((owner->kind!=1 && owner->kind!=2) || !owner->cast_id ||
        !SudekiMpResolveSpiritInstanceCaster(owner->actor,owner->session,&instance)) {
        SetLastError(ERROR_INVALID_DATA); return 0;
    }
    if(owner->kind==2 && owner->actor==active_spirit_probe_actor && !active_spirit_probe_cast_id)
        active_spirit_probe_cast_id=owner->cast_id;
    ++active_spirit_probe_task_steps;
    return SudekiMpEnterSpiritInstance(&instance);
}
static uint32_t active_spirit_probe_skill_enter(void *skill,int slot,BOOL using_skill) {
    SudekiMpSpiritInstance instance;
    SudekiMpSpiritInstanceState spirit;
    SudekiMpCharacterSkillState state;
    uint64_t session;
    uint8_t type;
    void *actor;
    unsigned int i;
    if(!runtime_readable_memory(skill,0x78) || slot<0 || slot>=6) return 0;
    actor=*(void **)((uint8_t *)skill+0x10);
    if(!host_cast_owner_witness(actor,1,&session,&type) ||
        !SudekiMpObserveCharacterSkill(actor,&state) || state.skill!=skill ||
        !SudekiMpResolveSpiritInstanceCaster(actor,session,&instance)) return 0;
    if(using_skill) {
        /* Presentation is still serialized in LA35. Owner-aware routing is
         * required before opening overlap, but does not itself authorize it.
         * Recheck both native lifetimes before the first SP/UI mutation. */
        if(state.active || !SudekiMpLanCastContextActorDrained(actor,session)) return 0;
        for(i=0;i<2;++i) {
            void *other=SudekiMpCleanroomEngineActorEntity(i ? seat_client_actor():seat_host_actor());
            SudekiMpSpiritInstance other_instance;
            if(!SudekiMpObserveCharacterSkill(other,&state) || state.active ||
                !SudekiMpResolveSpiritInstanceCaster(other,session,&other_instance) ||
                !SudekiMpObserveSpiritInstance(&other_instance,&spirit) || !spirit.idle) return 0;
        }
    }
    return SudekiMpEnterSpiritInstance(&instance);
}
static BOOL active_spirit_probe_idle(void) {
    uint64_t session;
    uint8_t type;
    return host_cast_owner_witness(active_spirit_probe_owner(),
        2,&session,&type) && host_native_tasks_drained();
}
static BOOL prepare_active_spirit_probe_menu_adapter(void) {
    if(SudekiMpQuickSkillSpiritRoutingReady()) return TRUE;
    /* The closed LAN profile disables optional input tracing. Acquire its
     * existing seam owner at this verified idle boundary, with both optional
     * input-behavior prototypes OFF. Never replace an already installed or
     * partially retained adapter; its installer rejects that case. */
    if(!SudekiMpInstallQuickSkillInputTrace(runtime_game_module,FALSE,FALSE)) return FALSE;
    active_spirit_probe_menu_adapter_owned=TRUE;
    SudekiMpLogWrite("lan_spirit_active_probe phase=menu_adapter_acquired policy=existing_owner_optional_input_prototypes_off\r\n");
    return SudekiMpQuickSkillSpiritRoutingReady();
}
static void run_active_spirit_probe(void) {
    if(!SudekiMpSpiritActivationRoutingHealthy() && !SudekiMpRetrySpiritActivationRoutingLeave()) return;
    if(!SudekiMpSkillActivationRoutingHealthy() && !SudekiMpRetrySkillActivationRoutingLeave()) return;
    if(!active_spirit_probe_attempted) {
        uint8_t type;
        if(!active_spirit_probe_owner_selected) {
            char value[2]={0};
            DWORD length=GetEnvironmentVariableA("SUDEKIMP_CAST_PROBE_REMOTE",value,sizeof(value));
            active_spirit_probe_owner_selected=TRUE;
            if(length && (length!=1 || value[0]!='1')) {
                active_spirit_probe_attempted=TRUE;
                SudekiMpLogWrite("lan_spirit_active_probe phase=disabled reason=invalid_private_owner_option\r\n");
                return;
            }
            active_spirit_probe_remote=length==1;
            value[0]=value[1]=0;
            length=GetEnvironmentVariableA("SUDEKIMP_CAST_PROBE_GATES",value,sizeof(value));
            if(length && (length!=1 || value[0]!='1')) {
                active_spirit_probe_attempted=TRUE;
                SudekiMpLogWrite("lan_spirit_active_probe phase=disabled reason=invalid_private_gate_option\r\n");
                return;
            }
            active_spirit_probe_gates=length==1;
        }
        if(!active_spirit_probe_idle()) return;
        active_spirit_probe_attempted=TRUE;
        active_spirit_probe_actor=active_spirit_probe_owner();
        if(!prepare_active_spirit_probe_menu_adapter() ||
            !SudekiMpLanCastContextEnableUiTrace() ||
            !SudekiMpInitializeSpiritInstanceAbi(runtime_game_module,active_spirit_probe_idle) ||
            !SudekiMpInstallSpiritInstanceUpdates() ||
            !SudekiMpCreateSpiritInstance(&active_spirit_probe) ||
            !SudekiMpScheduleSpiritInstanceManager(&active_spirit_probe) ||
            !host_cast_owner_witness(active_spirit_probe_actor,2,&active_spirit_probe_session,&type) ||
            !SudekiMpBindSpiritInstanceCaster(&active_spirit_probe,active_spirit_probe_actor,type,
                active_spirit_probe_session,active_spirit_probe_caster_witness) ||
            !prepare_active_spirit_probe_gates() ||
            !SudekiMpQuickSkillSpiritRoutingReady() ||
            !SudekiMpSetSpiritActivationRouting(active_spirit_probe_idle,
                active_spirit_probe_entry_enter,SudekiMpLeaveSpiritInstance) ||
            (active_spirit_probe_gates && !SudekiMpSetSkillActivationRouting(active_spirit_probe_idle,
                active_spirit_probe_skill_enter,SudekiMpLeaveSpiritInstance)) ||
            !SudekiMpLanCastContextSetTaskRouting(active_spirit_probe_task_enter,SudekiMpLeaveSpiritInstance) ||
            !(active_spirit_probe_scope=SudekiMpEnterSpiritInstance(&active_spirit_probe))) {
            SudekiMpLogFormat("lan_spirit_active_probe phase=setup_failed error=%lu policy=retain_native_lifetime\r\n",
                (unsigned long)GetLastError());
            return;
        }
        SudekiMpLogFormat("lan_spirit_active_probe phase=ready owner=%s manager=%p camera=%p generation=%lu gates=%u "
            "policy=one_manual_cast_caster_only_native_locks_no_overlap\r\n",
            active_spirit_probe_remote ? "client" : "host",active_spirit_probe.manager,active_spirit_probe.camera,
            (unsigned long)active_spirit_probe.generation,active_spirit_probe_gates);
    }
    if(active_spirit_probe_scope) {
        uint8_t *m=active_spirit_probe.manager,*c=active_spirit_probe.camera;
        uint8_t *base=(uint8_t *)runtime_game_module;
        if(!runtime_readable_memory(m,0xc4) || !runtime_readable_memory(c,0x1f0) ||
            *(void **)m!=base+0x2ca30c || *(void **)c!=base+0x2c5630 ||
            *(void **)(base+0x408d30)!=m || *(void **)(base+0x408d38)!=c) return;
        if(*(uint32_t *)(m+0x5c) && !active_spirit_probe_seen) {
            active_spirit_probe_seen=TRUE;
            active_spirit_probe_started=GetTickCount();
            SudekiMpLogFormat("lan_spirit_active_probe phase=active state=%lu strike=%lu\r\n",
                (unsigned long)*(uint32_t *)(m+0x5c),(unsigned long)*(uint32_t *)(m+0x98));
        }
        observe_active_spirit_probe_gates();
        if(!active_spirit_probe_seen || *(uint32_t *)(m+0x5c) ||
            *(uint32_t *)(c+0x1a0) || !host_native_tasks_drained()) return;
        if(!SudekiMpLeaveSpiritInstance(active_spirit_probe_scope)) return;
        active_spirit_probe_scope=0;
        {
            uint32_t mt=0,ct=0,st=0;
            BOOL observed=SudekiMpObserveSpiritInstanceManagerTicks(&active_spirit_probe,&mt) &&
                SudekiMpObserveSpiritInstanceUpdates(&active_spirit_probe,&ct,&st);
            SudekiMpLogFormat("lan_spirit_active_probe phase=completed elapsed_ms=%lu observed=%u "
                "manager_ticks=%lu camera_ticks=%lu soul_ticks=%lu task_steps=%lu neutral_steps=%lu "
                "entry_queries=%lu entry_activations=%lu menu_queries=%lu menu_activations=%lu "
                "policy=native_completion_no_forced_cancel\r\n",
                (unsigned long)(GetTickCount()-active_spirit_probe_started),observed,
                (unsigned long)mt,(unsigned long)ct,(unsigned long)st,
                (unsigned long)active_spirit_probe_task_steps,(unsigned long)active_spirit_probe_neutral_steps,
                (unsigned long)active_spirit_probe_entry_queries,(unsigned long)active_spirit_probe_entry_activations,
                (unsigned long)active_spirit_probe_menu_queries,(unsigned long)active_spirit_probe_menu_activations);
        }
    }
    if((active_spirit_probe.generation || active_spirit_probe_peer.generation ||
        active_spirit_probe_menu_adapter_owned) && !active_spirit_probe_scope) {
        BOOL destroyed=TRUE;
        if(!SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL)) return;
        if(active_spirit_probe_gates && !SudekiMpSetSkillActivationRouting(NULL,NULL,NULL)) return;
        if(!SudekiMpLanCastContextSetTaskRouting(NULL,NULL)) return;
        if(active_spirit_probe_peer.generation && !SudekiMpDestroySpiritInstance(&active_spirit_probe_peer)) destroyed=FALSE;
        if(active_spirit_probe.generation && !SudekiMpDestroySpiritInstance(&active_spirit_probe)) destroyed=FALSE;
        if(destroyed) {
            BOOL reset=SudekiMpResetSpiritInstanceAbi();
            if(!reset) return;
            if(active_spirit_probe_menu_adapter_owned) {
                if(!SudekiMpUninstallQuickSkillInputTrace()) return;
                active_spirit_probe_menu_adapter_owned=FALSE;
                SudekiMpLogWrite("lan_spirit_active_probe phase=menu_adapter_released policy=restored_only_probe_owned_adapter\r\n");
            }
            SudekiMpLogFormat("lan_spirit_active_probe phase=released reset=%u\r\n",reset);
        } else if(!active_spirit_probe_retained_logged) {
            active_spirit_probe_retained_logged=TRUE;
            SudekiMpLogFormat("lan_spirit_active_probe phase=retained error=%lu policy=await_native_reference_drain\r\n",
                (unsigned long)GetLastError());
        }
    }
}
#endif

static void refresh_host_player_two_skill_isolation(void *tal) {
    SudekiMpCharacterSkillState state;
    int spirit_state = 0;
    BOOL active;
    unsigned int spirit_owner = 0u;

    if (tal == NULL || !SudekiMpObserveCharacterSkill(tal, &state) ||
        (state.active != 0u && (state.slot < 0 || state.slot >= 6)) ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state)) {
        /* Unknown is not a retirement edge. Preserve the previous isolation
         * until both native owners are readable again. */
        return;
    }
    /* A successful native STARTED return owns a conservative lease before
     * CSkill's active byte becomes observable. Do not briefly release the
     * non-caster during that startup gap; host_apply_skill_state retires the
     * lease only after the exact task has been seen active and later inactive. */
    if (spirit_state != 0 && !host_spirit_owner(&spirit_owner)) return;
    active = state.active != 0u || (spirit_state != 0 && spirit_owner == 0u) ||
        host_native_skill_startup_pending(0u, tal, &state);

    (void)SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
        active);
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
    /* Conservative fallback for weapons without a validated FP cycle.
     * Record B8 is reload duration, not rapid-fire shot spacing; the guarded
     * base-weapon path below uses the actor's own FP animation instead. */
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
    if (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
        if (selector == (combat_enabled ? BUKI_COMBAT_MOVE_PRIMARY_SELECTOR :
                BUKI_WORLD_MOVE_PRIMARY_SELECTOR)) {
            *moving = TRUE;
            return TRUE;
        }
        if ((combat_enabled && selector == BUKI_COMBAT_IDLE_SELECTOR) ||
            (!combat_enabled && (selector == BUKI_WORLD_IDLE_SELECTOR ||
            selector == BUKI_WORLD_IDLE_VARIANT_ONE_SELECTOR ||
            selector == BUKI_WORLD_IDLE_VARIANT_TWO_SELECTOR))) {
            *moving = FALSE;
            return TRUE;
        }
        return FALSE;
    }
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
        /* Read-only edge capture: the renderer alone cannot distinguish
         * Buki's first weak/strong swings. Observe the semantic channel from
         * the same actor, without invoking generic native animation play. */
        if (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
            static int last_selector = -1, last_state = -1, last_anim_id = -1;
            uint8_t anim_id = host_read_actor_anim_id(seat_host_actor());
            if (last_selector != selector || last_state != state ||
                last_anim_id != anim_id) {
                last_selector = selector;
                last_state = state;
                last_anim_id = anim_id;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=host_buki_action_selector_diag "
                    "selector=%d state=%u anim_id=%u tick=%lu\r\n",
                    selector, (unsigned)state, (unsigned)anim_id,
                    (unsigned long)GetTickCount());
            }
        }
        if (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
            /* Preserve Buki's own bank and distinguish her shared 53/54
             * selectors using the same actor's observed semantic channel. */
            if (selector == 20) {
                return SUDEKIMP_LAN_ARENA_ACTION_NONE;
            }
            if (SudekiMpLanArenaBukiActionFromNativeAnimation(
                    host_read_actor_anim_id(seat_host_actor()),
                    selector, state, &action_variant)) {
                return action_variant;
            }
            return SUDEKIMP_LAN_ARENA_ACTION_NONE;
        }
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
    return selector == SudekiMpLanArenaRangedCombatSelector(
            seat_client_type(), 0x85u) && state != 192u ?
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE :
        SUDEKIMP_LAN_ARENA_ACTION_NONE;
}

static uint8_t combat_state_for_action(uint8_t action_variant) {
    uint8_t combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    (void)SudekiMpLanArenaActionCombatState(
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
    /* The ANIMID is the character-independent semantic animation id, read from
     * the host actor's model with the game's own guard (TsaGetCurrentAnimation).
     * Any failure yields UNKNOWN (0), never a synthesised id. */
    snapshot->anim_id = host_read_actor_anim_id(
        actor_index == 0u ? seat_host_actor() : seat_client_actor());
    {
        /* TEMPORARY diagnostic: is the host model resolving, and what ANIMID is
         * it playing? Answers whether character+0x134 reaches the model for a
         * melee host actor (anim_id stuck at 0 = offset wrong). REMOVE after
         * Phase 5 capture. */
        static DWORD last_host_anim_id_diag[2];
        DWORD anim_diag_now = GetTickCount();
        if (last_host_anim_id_diag[actor_index] == 0u ||
            (DWORD)(anim_diag_now - last_host_anim_id_diag[actor_index]) >= 300u) {
            last_host_anim_id_diag[actor_index] = anim_diag_now;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_anim_id_diag actor=%s anim_id=%u\r\n",
                actor_index == 0u ? "host" : "client",
                (unsigned)snapshot->anim_id);
        }
    }
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
     * collision-safe translation classifier for unrecognized action states.
     * A remote Spirit also suppresses native noncaster locomotion; CSkill's
     * active byte stays clear throughout it. In that scope translation must
     * drive the compositor, not its previous idle pose (a feedback lock). The
     * Spirit owner was validated by host_apply_spirit_state for this frame. */
    if (actor_index == 0u &&
        !host_actor_previous_skill_active[1] &&
        !(host_spirit_previous_active && host_spirit_actor_index == 1u) &&
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
    /* Preserve the host-authoritative locomotion fact before ACTION is folded
     * into the stationary-position filter below. The non-caster compositor
     * owns only RUN/IDLE and must never infer locomotion from an attack. */
    host_actor_locomotion_moving[actor_index] = moving;
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
    if (actor_index == 0u) {
        static DWORD last_host_pos_diag;
        DWORD pos_now = GetTickCount();
        if (last_host_pos_diag == 0u ||
            (DWORD)(pos_now - last_host_pos_diag) >= 300u) {
            last_host_pos_diag = pos_now;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_pos_diag actor=0 moving=%u "
                "action=%u anim=%u x=%.3f z=%.3f\r\n",
                moving ? 1u : 0u, action_active ? 1u : 0u,
                (unsigned)snapshot->animation_state,
                (double)snapshot->x, (double)snapshot->z);
        }
    }
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
    {
        /* TEMPORARY diagnostic: correlate the SEMANTIC moving/action state with
         * the raw renderer selector/state/rate PLUS the remote (client-actor)
         * movement direction, to map Elco's directional walk/strafe/block ids.
         * REMOVE after capture. */
        if ((snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING ||
             snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION) &&
            host_actor_presentation_valid[actor_index]) {
            static DWORD last_move_selector_diag[2];
            DWORD diag_now = GetTickCount();
            const SudekiMpCleanroomActorPresentation *diag_p =
                &host_actor_presentation[actor_index];
            if (last_move_selector_diag[actor_index] == 0u ||
                (DWORD)(diag_now - last_move_selector_diag[actor_index]) >= 300u) {
                int dir_x = 0;
                int dir_z = 0;
                if (actor_index == 1u) {
                    dir_x = (int)host_remote_direction_x;
                    dir_z = (int)host_remote_direction_z;
                }
                last_move_selector_diag[actor_index] = diag_now;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=move_selector_diag actor=%s "
                    "combat=%u dir=%d,%d action=%u sel=%ld,%ld,%ld,%ld,%ld "
                    "state=%u,%u,%u,%u,%u rate=%.3f,%.3f,%.3f,%.3f,%.3f\r\n",
                    actor_index == 0u ? "host" : "client",
                    combat_enabled ? 1u : 0u,
                    dir_x, dir_z,
                    (unsigned)snapshot->action_variant,
                    (long)diag_p->selector[0], (long)diag_p->selector[1],
                    (long)diag_p->selector[2], (long)diag_p->selector[3],
                    (long)diag_p->selector[4],
                    diag_p->state[0], diag_p->state[1], diag_p->state[2],
                    diag_p->state[3], diag_p->state[4],
                    (double)diag_p->rate[0], (double)diag_p->rate[1],
                    (double)diag_p->rate[2], (double)diag_p->rate[3],
                    (double)diag_p->rate[4]);
            }
        }
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
        "policy=read_only_semantic_default_exact_skill_payload_only_while_active\r\n",
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
            seat_host_actor(), &presentation)) return;
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
        host_trace_native_presentation(0u, seat_host_actor());
    }
    if (ailish_initialized) {
        host_trace_native_presentation(1u, seat_client_actor());
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

static const char *host_snapshot_failure_stage_name(
    HostSnapshotFailureStage stage
) {
    switch (stage) {
        case HOST_SNAPSHOT_FAILURE_COMBAT_OBSERVATION:
            return "combat_observation";
        case HOST_SNAPSHOT_FAILURE_TAL_ACTOR_FILL:
            return "tal_actor_fill";
        case HOST_SNAPSHOT_FAILURE_AILISH_ACTOR_FILL:
            return "ailish_actor_fill";
        case HOST_SNAPSHOT_FAILURE_TAL_SKILL_OBSERVATION:
            return "tal_skill_observation";
        case HOST_SNAPSHOT_FAILURE_AILISH_SKILL_OBSERVATION:
            return "ailish_skill_observation";
        case HOST_SNAPSHOT_FAILURE_SPIRIT_OBSERVATION:
            return "spirit_observation";
        case HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION:
            return "snapshot_validation";
        case HOST_SNAPSHOT_FAILURE_CANONICAL_BEGIN:
            return "canonical_begin";
        case HOST_SNAPSHOT_FAILURE_CANONICAL_COMMIT:
            return "canonical_commit";
        case HOST_SNAPSHOT_FAILURE_CANONICAL_READ:
            return "canonical_read";
        case HOST_SNAPSHOT_FAILURE_SEND:
            return "send";
        default:
            return "none";
    }
}

static void host_snapshot_publish_failed(
    DWORD now_ms,
    uint64_t session_token,
    HostSnapshotFailureStage stage,
    const SudekiMpLanArenaSnapshot *snapshot
) {
    BOOL transition;
    BOOL should_log;
    if (stage == HOST_SNAPSHOT_FAILURE_NONE) return;
    transition = host_snapshot_failure_telemetry.session_token !=
            session_token ||
        host_snapshot_failure_telemetry.stage != stage;
    if (transition) {
        host_snapshot_failure_telemetry.session_token = session_token;
        host_snapshot_failure_telemetry.stage = stage;
        host_snapshot_failure_telemetry.consecutive_failures = 1u;
    } else if (host_snapshot_failure_telemetry.consecutive_failures !=
            UINT32_MAX) {
        ++host_snapshot_failure_telemetry.consecutive_failures;
    }
    should_log = transition ||
        (DWORD)(now_ms -
            host_snapshot_failure_telemetry.last_logged_at_ms) >= 1000u;
    if (!should_log) return;
    host_snapshot_failure_telemetry.last_logged_at_ms = now_ms;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_snapshot_publish phase=suppressed "
        "stage=%s consecutive=%lu session_token=%08lx%08lx "
        "tal_skill_sequence=%u tal_skill_kind=%u tal_skill_active=%u "
        "tal_selector0=%ld ailish_skill_sequence=%u "
        "ailish_skill_kind=%u ailish_skill_active=%u ailish_selector0=%ld "
        "policy=first_failed_stage_transition_or_one_second_aggregate\r\n",
        host_snapshot_failure_stage_name(stage),
        (unsigned long)
            host_snapshot_failure_telemetry.consecutive_failures,
        (unsigned long)(session_token >> 32),
        (unsigned long)session_token,
        snapshot != NULL ?
            (unsigned int)snapshot->seat[0].skill_sequence : 0u,
        snapshot != NULL ?
            (unsigned int)snapshot->seat[0].skill_kind : 0u,
        snapshot != NULL ?
            (unsigned int)snapshot->seat[0].skill_active : 0u,
        snapshot != NULL ?
            (long)snapshot->seat[0].skill_presentation_selector[0] : 0l,
        snapshot != NULL ?
            (unsigned int)snapshot->seat[1].skill_sequence : 0u,
        snapshot != NULL ?
            (unsigned int)snapshot->seat[1].skill_kind : 0u,
        snapshot != NULL ?
            (unsigned int)snapshot->seat[1].skill_active : 0u,
        snapshot != NULL ?
            (long)snapshot->seat[1].skill_presentation_selector[0] : 0l);
}

static void host_snapshot_publish_succeeded(
    DWORD now_ms,
    uint64_t session_token
) {
    if (host_snapshot_failure_telemetry.stage !=
            HOST_SNAPSHOT_FAILURE_NONE &&
        host_snapshot_failure_telemetry.session_token == session_token) {
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_snapshot_publish phase=recovered "
            "previous_stage=%s consecutive=%lu recovery_tick=%lu "
            "policy=first_successful_send_closes_failure_interval\r\n",
            host_snapshot_failure_stage_name(
                host_snapshot_failure_telemetry.stage),
            (unsigned long)
                host_snapshot_failure_telemetry.consecutive_failures,
            (unsigned long)now_ms);
    }
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
}

static void host_capture_ailish_locomotion(
    uint64_t session_token,
    BOOL combat_enabled,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    SudekiMpCleanroomActorPresentation native;
    SudekiMpLanArenaLocomotion current;
    void *actor = SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    unsigned int channel;
    BOOL transition;
    ZeroMemory(&snapshot->locomotion, sizeof(snapshot->locomotion));
    if (host_ailish_locomotion.actor != actor ||
        host_ailish_locomotion.session_token != session_token) {
        ZeroMemory(&host_ailish_locomotion, sizeof(host_ailish_locomotion));
        host_ailish_locomotion.actor = actor;
        host_ailish_locomotion.session_token = session_token;
    }
    /* Observe freshly here: the trace cache may retain a last-good frame on
     * observation failure. Unknown ordinary presentation must not stall the
     * gameplay snapshot or be mistaken for a fresh animation witness. */
    if (!actor || !combat_enabled || snapshot->skill_active || !snapshot->hp ||
        !SudekiMpCleanroomEngineActorPresentation(seat_client_actor(),
            &native) ||
        actor != SudekiMpCleanroomEngineActorEntity(seat_client_actor())) {
        host_ailish_locomotion.previous.valid = 0u;
        return;
    }
    ZeroMemory(&current, sizeof(current));
    current.valid = 1u;
    current.sequence = 1u; /* Validate content before allocating an epoch. */
    for (channel = 0u; channel < 4u; ++channel) {
        int clip = SudekiMpLanArenaLocomotionClip(native.selector[channel],
            seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE);
        if (clip < 0) {
            host_ailish_locomotion.previous.valid = 0u;
            return;
        }
        current.clip[channel] = (uint8_t)clip;
        current.state[channel] = native.state[channel];
        if (clip != 0) {
            current.rate[channel] = native.rate[channel];
            current.time[channel] = native.time[channel];
        }
    }
    memcpy(current.blend, native.blend, sizeof(current.blend));
    if (!SudekiMpLanArenaLocomotionValid(&current)) {
        host_ailish_locomotion.previous.valid = 0u;
        return;
    }
    transition = !host_ailish_locomotion.previous.valid ||
        memcmp(current.clip, host_ailish_locomotion.previous.clip,
            sizeof(current.clip)) != 0 ||
        memcmp(current.state, host_ailish_locomotion.previous.state,
            sizeof(current.state)) != 0;
    for (channel = 0u; channel < 4u; ++channel) {
        if (current.time[channel] < host_ailish_locomotion.previous.time[channel])
            transition = TRUE; /* Native loop/restart: never interpolate across it. */
    }
    if (transition && ++host_ailish_locomotion.sequence == 0u)
        ++host_ailish_locomotion.sequence;
    current.sequence = host_ailish_locomotion.sequence;
    if (!host_ailish_locomotion.previous.valid ||
        memcmp(current.clip, host_ailish_locomotion.previous.clip,
            sizeof(current.clip)) != 0) {
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_locomotion actor=Ailish epoch=%u "
            "clips=%u,%u,%u,%u policy=observed_native_base_channels\r\n",
            current.sequence, current.clip[0], current.clip[1],
            current.clip[2], current.clip[3]);
    }
    host_ailish_locomotion.previous = current;
    snapshot->locomotion = current;
}

static void host_capture_tal_locomotion(
    uint64_t session_token,
    BOOL combat_enabled,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    SudekiMpCleanroomActorPresentation native;
    SudekiMpLanArenaLocomotion current;
    void *actor = SudekiMpCleanroomEngineActorEntity(seat_host_actor());
    unsigned int channel;
    BOOL transition;
    ZeroMemory(&snapshot->locomotion, sizeof(snapshot->locomotion));
    /* Buki's body frame includes movement plus LA30 block/run-attack phases.
     * Ordinary combos retain their native action lease; Tal is unchanged. */
    if (seat_host_type() != SUDEKIMP_LAN_ARENA_BUKI_TYPE) return;
    if (host_tal_locomotion.actor != actor ||
        host_tal_locomotion.session_token != session_token) {
        ZeroMemory(&host_tal_locomotion, sizeof(host_tal_locomotion));
        host_tal_locomotion.actor = actor;
        host_tal_locomotion.session_token = session_token;
    }
    if (!actor || !combat_enabled || snapshot->skill_active || !snapshot->hp ||
        (snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
         !SudekiMpLanArenaBukiBodyAction(snapshot->action_variant)) ||
        !SudekiMpCleanroomEngineActorPresentation(seat_host_actor(), &native) ||
        actor != SudekiMpCleanroomEngineActorEntity(seat_host_actor())) {
        static DWORD last_tal_loco_diag;
        DWORD ln = GetTickCount();
        if (last_tal_loco_diag == 0u ||
            (DWORD)(ln - last_tal_loco_diag) >= 500u) {
            last_tal_loco_diag = ln;
            SudekiMpLogFormat(
                "lan_arena_runtime event=tal_loco_diag state=bail "
                "actor=%u combat=%u skill_active=%u hp=%u pres=%u "
                "policy=host_actor_locomotion_capture\r\n",
                actor ? 1u : 0u, combat_enabled ? 1u : 0u,
                (unsigned)snapshot->skill_active, (unsigned)snapshot->hp,
                (unsigned)SudekiMpCleanroomEngineActorPresentation(
                    seat_host_actor(), &native));
        }
        host_tal_locomotion.previous.valid = 0u;
        return;
    }
    ZeroMemory(&current, sizeof(current));
    current.valid = 1u;
    current.sequence = 1u;
    for (channel = 0u; channel < 4u; ++channel) {
        int clip = SudekiMpLanArenaLocomotionClip(
            native.selector[channel], 2u);
        if (clip < 0) {
            static DWORD last_tal_loco_unmapped;
            DWORD un = GetTickCount();
            if (last_tal_loco_unmapped == 0u ||
                (DWORD)(un - last_tal_loco_unmapped) >= 500u) {
                last_tal_loco_unmapped = un;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=tal_loco_diag state=unmapped "
                    "channel=%u selector=%ld state=%u "
                    "policy=host_actor_locomotion_capture\r\n",
                    channel, (long)native.selector[channel],
                    (unsigned)native.state[channel]);
            }
            host_tal_locomotion.previous.valid = 0u;
            return;
        }
        current.clip[channel] = (uint8_t)clip;
        current.state[channel] = native.state[channel];
        if (clip != 0) {
            current.rate[channel] = native.rate[channel];
            current.time[channel] = native.time[channel];
        }
    }
    memcpy(current.blend, native.blend, sizeof(current.blend));
    if (!SudekiMpLanArenaLocomotionValid(&current)) {
        host_tal_locomotion.previous.valid = 0u;
        return;
    }
    transition = !host_tal_locomotion.previous.valid ||
        memcmp(current.clip, host_tal_locomotion.previous.clip,
            sizeof(current.clip)) != 0 ||
        memcmp(current.state, host_tal_locomotion.previous.state,
            sizeof(current.state)) != 0;
    for (channel = 0u; channel < 4u; ++channel) {
        if (current.time[channel] < host_tal_locomotion.previous.time[channel])
            transition = TRUE;
    }
    if (transition && ++host_tal_locomotion.sequence == 0u)
        ++host_tal_locomotion.sequence;
    current.sequence = host_tal_locomotion.sequence;
    host_tal_locomotion.previous = current;
    snapshot->locomotion = current;
    {
        static DWORD last_tal_loco_ok;
        DWORD on = GetTickCount();
        if (last_tal_loco_ok == 0u ||
            (DWORD)(on - last_tal_loco_ok) >= 500u) {
            last_tal_loco_ok = on;
            SudekiMpLogFormat(
                "lan_arena_runtime event=tal_loco_diag state=ok "
                "clips=%u,%u,%u,%u states=%u,%u,%u,%u "
                "policy=host_actor_locomotion_capture\r\n",
                current.clip[0], current.clip[1], current.clip[2],
                current.clip[3], current.state[0], current.state[1],
                current.state[2], current.state[3]);
        }
    }
}

static void host_publish_snapshot(DWORD now_ms) {
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaSnapshot snapshot;
    SudekiMpLanArenaNativeWorldObservation world_observation;
    SudekiMpLanArenaActorObservation tal_observation;
    SudekiMpLanArenaActorObservation ailish_observation;
    HostSpiritAudioStage spirit_audio_stage;
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
    memset(&snapshot, 0, sizeof(snapshot));
    if (!SudekiMpCleanroomEngineCombatMode(&combat_enabled)) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_COMBAT_OBSERVATION, &snapshot);
        return;
    }
    if (!fill_actor_snapshot(
            seat_host_actor(), seat_host_type(),
            &snapshot.seat[0])) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_TAL_ACTOR_FILL, &snapshot);
        return;
    }
    if (!fill_actor_snapshot(
            seat_client_actor(), seat_client_type(),
            &snapshot.seat[1])) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_AILISH_ACTOR_FILL, &snapshot);
        return;
    }
    if (tal_initialized) {
        host_trace_native_presentation(0u, seat_host_actor());
    }
    if (ailish_initialized) {
        host_trace_native_presentation(1u, seat_client_actor());
    }
    if (!host_apply_skill_state(
            0u, seat_host_actor(), &snapshot.seat[0])) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_TAL_SKILL_OBSERVATION, &snapshot);
        return;
    }
    if (!host_apply_skill_state(
            1u, seat_client_actor(), &snapshot.seat[1])) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_AILISH_SKILL_OBSERVATION, &snapshot);
        return;
    }
    /* An unreadable global Spirit manager is unknown, not inactive. Do not
     * emit an artificial retirement edge or stale actor presentation. */
    if (!host_apply_spirit_state(&snapshot)) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_SPIRIT_OBSERVATION, &snapshot);
        return;
    }
    host_capture_spirit_audio(
        &snapshot.seat[host_spirit_actor_index], &snapshot, &spirit_audio_stage);
    host_apply_presentation_state(
        0u, now_ms, combat_enabled, &snapshot.seat[0]);
    host_apply_presentation_state(
        1u, now_ms, combat_enabled, &snapshot.seat[1]);
    host_apply_skill_presentation(0u, &snapshot.seat[0]);
    host_apply_skill_presentation(1u, &snapshot.seat[1]);
    host_capture_spirit_view(&snapshot);
    host_capture_skill_fade(&snapshot);
    host_capture_ailish_locomotion(status.session_token, combat_enabled,
        &snapshot.seat[1]);
    host_capture_tal_locomotion(status.session_token, combat_enabled,
        &snapshot.seat[0]);
    snapshot.host_tick = now_ms;
    snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    snapshot.combat_enabled = combat_enabled ? 1u : 0u;
    /* A partial/failed visual observation is UNKNOWN, not an empty roster.
     * This optional presentation domain must not stall gameplay snapshots. */
    if (!SudekiMpLanArenaSpiritVisualHostCapture(status.session_token,
            snapshot.seat[0].skill_sequence, now_ms,
            SudekiMpCleanroomEngineActorEntity(seat_host_actor()),
            SudekiMpCleanroomEngineActorEntity(seat_client_actor()), &snapshot)) {
        snapshot.spirit_vfx_observed = 0u;
        snapshot.spirit_vfx_count = 0u;
        ZeroMemory(snapshot.spirit_vfx, sizeof(snapshot.spirit_vfx));
    }
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
    /* Commit validates again, but naming this boundary separately makes a
     * future authored renderer stage omission visible immediately instead of
     * masquerading as a quiet UDP or interpolation stall. */
    if (!SudekiMpLanArenaSnapshotValid(&snapshot)) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
        return;
    }
    if (!ensure_canonical_simulation(status.session_token)) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_CANONICAL_BEGIN, &snapshot);
        return;
    }
    ZeroMemory(&world_observation, sizeof(world_observation));
    world_observation.host_tick = now_ms;
    world_observation.tal_hp = snapshot.seat[0].hp;
    world_observation.tal_sp = snapshot.seat[0].sp;
    world_observation.ailish_hp = snapshot.seat[1].hp;
    world_observation.ailish_sp = snapshot.seat[1].sp;
    world_observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    world_observation.combat_enabled = combat_enabled ? 1u : 0u;
    world_observation.enemy_count = snapshot.enemy_count;
    memcpy(world_observation.enemies, snapshot.enemies,
        sizeof(world_observation.enemies));
    world_observation.spirit_audio_history_count =
        snapshot.spirit_audio_history_count;
    memcpy(world_observation.spirit_audio_history,
        snapshot.spirit_audio_history,
        sizeof(world_observation.spirit_audio_history));
    world_observation.spirit_vfx_observed = snapshot.spirit_vfx_observed;
    world_observation.spirit_view = snapshot.spirit_view;
    world_observation.skill_fade = snapshot.skill_fade;
    world_observation.spirit_vfx_count = snapshot.spirit_vfx_count;
    memcpy(world_observation.spirit_vfx, snapshot.spirit_vfx,
        sizeof(world_observation.spirit_vfx));
    world_observation.native_combat_observed = 1u;
    world_observation.native_resources_observed = 1u;
    world_observation.native_enemies_observed = 1u;
    ZeroMemory(&tal_observation, sizeof(tal_observation));
    ZeroMemory(&ailish_observation, sizeof(ailish_observation));
    tal_observation.actor = snapshot.seat[0];
    tal_observation.native_actor_observed = 1u;
    ailish_observation.actor = snapshot.seat[1];
    ailish_observation.native_actor_observed = 1u;
    if (!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
            &canonical_simulation, status.session_token,
            &world_observation, &tal_observation, &ailish_observation)) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_CANONICAL_COMMIT, &snapshot);
        return;
    }
    /* The canonical reducer now owns this exact journal version. Publish the
     * trace cursor/history immediately so even a later read/send failure is
     * retried from state consistent with the already-committed frame. No
     * pre-commit validation failure can leak a retained event into the next
     * native observation. */
    commit_host_spirit_audio_stage(&spirit_audio_stage);
    if (!SudekiMpLanArenaSharedSimulationReadFrame(
            &canonical_simulation, &snapshot, NULL)) {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_CANONICAL_READ, &snapshot);
        return;
    }
    if (SudekiMpLanArenaSessionSendSnapshot(&snapshot)) {
        host_snapshot_publish_succeeded(now_ms, status.session_token);
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
    } else {
        host_snapshot_publish_failed(
            now_ms, status.session_token,
            HOST_SNAPSHOT_FAILURE_SEND, &snapshot);
    }
}

static BOOL release_host_remote_ailish(const char *reason) {
    if (!host_remote_ailish_owned) return TRUE;
    if(!release_runtime_skill_ui()) return FALSE;
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
    BOOL had_client_tal = client_remote_tal_owned ||
        client_tal_spawn_attempted ||
        client_remote_tal_request_owned ||
        client_remote_tal_remove_pending ||
        client_remote_tal_generation_actor != NULL ||
        client_remote_tal_active_generation != 0u;
    if(had_client_tal && !release_runtime_skill_ui()) return FALSE;
    if (client_remote_tal_remove_pending) {
        if (SudekiMpCleanroomEngineActorPresent(seat_host_actor())) {
            SetLastError(ERROR_BUSY);
            return FALSE;
        }
        client_remote_tal_remove_pending = FALSE;
        client_tal_spawn_attempted = FALSE;
        client_release_pending_logged = FALSE;
        ZeroMemory(client_actor_presentation_valid,
            sizeof(client_actor_presentation_valid));
        SudekiMpLogFormat(
            "lan_arena_runtime event=client_tal_replica_remove phase=confirmed "
            "reason=%s policy=positive_native_absence_before_restart\r\n",
            reason == NULL ? "session_inactive" : reason);
        return TRUE;
    }
    /* SpawnActor publishes its entity asynchronously. Until publication is
     * positively observed there is nothing native that can be removed, but
     * the request itself is still live. Retiring its flag would let a later
     * Join/observer enqueue a duplicate Tal into the native intrusive list. */
    if (client_tal_spawn_attempted &&
        !SudekiMpCleanroomEngineActorPresent(seat_host_actor()) &&
        !client_tal_missing_actor_requires_release()) {
        if (!client_release_pending_logged) {
            client_release_pending_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=client_tal_replica_release phase=pending "
                "reason=%s blocker=native_spawn_publication "
                "policy=retain_spawn_request_and_retry_before_restart\r\n",
                reason == NULL ? "session_inactive" : reason);
        }
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (had_client_tal &&
        !SudekiMpLanArenaClientReplicaRemoteTalReleaseReady()) {
        if (!client_release_pending_logged) {
            client_release_pending_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=client_tal_replica_release phase=pending "
                "reason=%s blocker=native_presentation_lease "
                "policy=retain_actor_until_exact_native_retirement\r\n",
                reason == NULL ? "session_inactive" : reason);
        }
        return FALSE;
    }
    if (had_client_tal) {
        /* Revoke presentation before native ownership or allocation can be
         * released. A later same-address claim receives a fresh generation. */
        invalidate_client_remote_tal_generation(reason);
        tal_initialized = FALSE;
    }
    if (client_remote_tal_owned || client_remote_tal_request_owned) {
        void *tal = SudekiMpCleanroomEngineActorEntity(
            seat_host_actor());
        if (tal != NULL && client_remote_tal_owned) {
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
        client_remote_tal_request_owned = FALSE;
        client_replica_stop_state = -1;
        client_release_pending_logged = FALSE;
        SudekiMpLogFormat(
            "lan_arena_runtime event=client_tal_replica_release phase=confirmed reason=%s "
            "policy=native_ai_restore_before_replica_discard\r\n",
            reason == NULL ? "session_inactive" : reason);
    }
    if (client_tal_spawn_attempted) {
        if (SudekiMpCleanroomEngineActorPresent(seat_host_actor())) {
            if (!SudekiMpCleanroomEngineRemoveActor(
                    seat_host_actor())) {
                return FALSE;
            }
            client_remote_tal_remove_pending = TRUE;
            if (SudekiMpCleanroomEngineActorPresent(
                    seat_host_actor())) {
                if (!client_release_pending_logged) {
                    client_release_pending_logged = TRUE;
                    SudekiMpLogFormat(
                        "lan_arena_runtime event=client_tal_replica_remove "
                        "phase=pending reason=%s "
                        "policy=retain_spawn_lease_until_positive_native_absence\r\n",
                        reason == NULL ? "session_inactive" : reason);
                }
                SetLastError(ERROR_BUSY);
                return FALSE;
            }
            client_remote_tal_remove_pending = FALSE;
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
    BOOL remote_combat_toggle_rejected = FALSE;
    BOOL remote_skill_requested = FALSE;
    uint8_t remote_skill_slot = 0u;
    uint8_t remote_kit_action = 0u;
    uint8_t remote_kit_slot = 0u;
    BOOL ranged_native_ready = TRUE;
    BOOL ranged_native_ready_known = FALSE;
    BOOL rapid_weapon_cycle_known = FALSE;
    uint16_t ranged_authored_delay_half = 0u;
    uint32_t ranged_repeat_interval_ms =
        HOST_REMOTE_RANGED_REPEAT_FALLBACK_MS;
    unsigned int operator_skill_slot;
    BOOL witness_exact;
    BOOL status_available;
    DWORD now_ms;
    float frame_delta_seconds = 0.0f;
    (void)controller;
    if (update_data != NULL) {
        float candidate = *(float *)((uint8_t *)update_data + 0x0cu);
        if (isfinite(candidate) && candidate > 0.0f && candidate <= 0.25f) {
            frame_delta_seconds = candidate;
        }
    }
    if (!SudekiMpControlUpdateObserverGateTryEnter(
            &lan_arena_control_observer_gate)) return;
    (void)SudekiMpLanCastContextPoll();
#if defined(SUDEKIMP_CAST_INSTANCE_PROBE)
    run_spirit_instance_probe();
#endif
#if defined(SUDEKIMP_CAST_ACTIVE_PROBE)
    run_active_spirit_probe();
#endif
    if(!service_runtime_skill_ui()) {
        SudekiMpControlUpdateObserverGateLeave(&lan_arena_control_observer_gate);
        return;
    }
    /* The callback-free ranged-prime lease is owned by this native game
     * thread. Service it on every host observer pass, including passes that
     * later discover a lost session, so teardown can obtain a positive UI
     * retirement without leaving a TimerProc behind. */
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        SudekiMpCleanroomEngineMaintainResources();
    }
    /* Client-side skill records and inventory are presentation metadata only.
     * Keep them populated so Ailish can browse every authored training slot;
     * activation still crosses the authenticated input packet and executes
     * only in the host's native world. */
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        now_ms = GetTickCount();
        if (client_cleanroom_maintenance_last_at == 0u ||
            (DWORD)(now_ms - client_cleanroom_maintenance_last_at) >= 150u) {
            client_cleanroom_maintenance_last_at = now_ms;
            SudekiMpCleanroomEngineMaintainResources();
        }
    }
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
        BOOL actors_released = TRUE;
        BOOL dummy_released = TRUE;
        if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
            host_operator_spirit_session_token = 0u;
            discard_host_operator_spirit_requests(
                "transport_authority_inactive");
        }
        if (runtime_config.local_role ==
                SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
            SudekiMpLanArenaClientReplicaDiscardSnapshots();
            client_replica_frame_attempted = FALSE;
            client_replica_frame_ready = FALSE;
            client_replica_stop_state = -1;
            if (!client_replica_discard_logged) {
                client_replica_discard_logged = TRUE;
                SudekiMpLogWrite(
                    "lan_arena_runtime event=client_snapshot_replica phase=discarded "
                    "reason=transport_authority_inactive "
                    "policy=no_stale_snapshot_sampling\r\n");
            }
            /* A client-replayed CSkill is an asynchronous native task. Keep
             * Tal/Ailish and every containment hook alive until Sudeki itself
             * positively reports that task inactive; removing Tal first can
             * turn a routine UDP timeout into a use-after-free. */
            if (SudekiMpResetLanArenaClientReplica()) {
                actors_released =
                    release_client_remote_tal("session_not_authenticated");
                dummy_released = release_arena_dummy();
            } else {
                actors_released = FALSE;
                dummy_released = FALSE;
            }
        } else {
            if (host_native_tasks_drained()) {
                actors_released = release_host_remote_ailish(
                    "session_not_authenticated");
                dummy_released = release_arena_dummy();
            } else {
                actors_released = FALSE;
                dummy_released = FALSE;
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
    if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        /* This is the only Spirit operator service point: an exact native
         * control-update dispatch on the authenticated canonical host. It runs
         * before actor-role early returns so a stale pending intent is rejected
         * rather than surviving a changed party/control identity. */
        if (host_operator_spirit_session_ready(&status)) {
            service_host_operator_spirit(&status, 0u);
        }
    }
    if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        void *tal;
        void *ailish_actor;
        float ailish_position[3];
        float tal_position[3];
        client_replica_discard_logged = FALSE;
        release_host_remote_ailish("client_role");
        SudekiMpLanArenaClientInputService();
        /* A transient inexact observer pass can begin Tal removal without
         * ending the authenticated transport. Do not reclaim that still-
         * published actor while its asynchronous RemovePC request is live. */
        if (client_remote_tal_remove_pending) {
            (void)release_client_remote_tal("pending_remove");
            SudekiMpControlUpdateObserverGateLeave(
                &lan_arena_control_observer_gate);
            return;
        }
        tal = SudekiMpCleanroomEngineActorEntity(seat_host_actor());
        ailish_actor = SudekiMpCleanroomEngineActorEntity(
            seat_client_actor());
        if (tal == NULL) {
            BOOL tal_released = TRUE;
            if (client_tal_missing_actor_requires_release()) {
                tal_released =
                    release_client_remote_tal("client_tal_lost");
            }
            if (tal_released && !client_remote_tal_owned &&
                !client_tal_spawn_attempted && ailish_actor != NULL &&
                SudekiMpCleanroomEngineActorPosition(
                    seat_client_actor(), ailish_position)) {
                tal_position[0] = ailish_position[0] - 1.5f;
                tal_position[1] = ailish_position[1];
                tal_position[2] = ailish_position[2] + 1.5f;
                client_tal_spawn_attempted =
                    SudekiMpCleanroomEngineSpawnActor(
                        seat_host_actor(), tal_position);
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
            if (SudekiMpControlSeparationRequestPlayerTwoCharacter(tal)) {
                /* RequestPlayerTwoCharacter can succeed one or more frames
                 * before PlayerTwoActive publishes the exact lease. Track
                 * that intervening native control reference so teardown must
                 * release it before removing Tal. */
                client_remote_tal_request_owned = TRUE;
            }
            SudekiMpControlUpdateObserverGateLeave(
                &lan_arena_control_observer_gate);
            return;
        }
        if (!claim_client_remote_tal_generation(tal)) {
            SudekiMpControlUpdateObserverGateLeave(
                &lan_arena_control_observer_gate);
            return;
        }
        if (!client_remote_tal_owned) {
            client_remote_tal_owned = TRUE;
            client_remote_tal_request_owned = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=client_tal_replica_claim state=active "
                "actor_generation=%lu "
                "policy=native_ai_disabled_snapshot_transform_only\r\n",
                (unsigned long)client_remote_tal_active_generation);
        }
        if (!tal_initialized) {
            tal_initialized = initialize_party_actor_equipment(
                seat_host_actor());
        }
        if (!ailish_initialized) {
            ailish_initialized = initialize_party_actor_equipment(
                seat_client_actor());
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
                seat_client_actor(), ailish_position)) {
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
        /* Publish one authoritative sample before the native camera consumes
         * the local actor position. RenderStart must reuse it, not advance
         * CPosition again after the camera has already followed this frame. */
        if (tal_initialized && ailish_initialized) {
            prepare_client_replica_frame();
        }
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
    ailish = SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    if (ailish == NULL) {
        float tal_position[3];
        float ailish_position[3];
        if (!host_ailish_spawn_attempted &&
            SudekiMpCleanroomEngineActorPosition(
                seat_host_actor(), tal_position)) {
            ailish_position[0] = tal_position[0] + 1.5f;
            ailish_position[1] = tal_position[1];
            ailish_position[2] = tal_position[2] - 1.5f;
            host_ailish_spawn_attempted =
                SudekiMpCleanroomEngineSpawnActor(
                    seat_client_actor(), ailish_position);
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
        SudekiMpControlSeparationBindLanArenaMovementActors(
            seat_host_type(), seat_client_type()) &&
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
        tal_initialized = initialize_party_actor_equipment(
            seat_host_actor());
    }
    if (!ailish_initialized) {
        ailish_initialized = initialize_party_actor_equipment(
            seat_client_actor());
    }
    refresh_host_player_two_skill_isolation(
        SudekiMpCleanroomEngineActorEntity(seat_host_actor()));
    if (SudekiMpLanArenaHostInputTakeSkillSlot(&operator_skill_slot)) {
        void *tal = SudekiMpCleanroomEngineActorEntity(
            seat_host_actor());
        SudekiMpSkillActivationResult skill_result =
            SudekiMpReplayHostApprovedCharacterSkillSlot(
                tal, (int)operator_skill_slot);
        host_track_started_native_skill(0u, tal, &skill_result);
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_operator_skill phase=%s actor=Tal "
            "slot=%u validation=%d use=%u "
            "policy=local_test_rail_host_approved_slot_native_validator_and_use\r\n",
            SudekiMpSkillActivationStatusName(skill_result.status),
            operator_skill_slot, skill_result.validation_result,
            (unsigned int)skill_result.use_result);
        refresh_host_player_two_skill_isolation(tal);
    }
    if (!SudekiMpCleanroomEngineDummyPresent() &&
        !arena_dummy_spawn_attempted) {
        float tal_position[3];
        float dummy_position[3];
        if (SudekiMpCleanroomEngineActorPosition(
                seat_host_actor(), tal_position)) {
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
        remote_combat_toggle_rejected = remote_combat_toggle_rejected ||
            input.cleanroom_combat_test_pressed != 0u;
        if (!remote_skill_requested && input.skill_pressed != 0u) {
            remote_skill_requested = TRUE;
            remote_skill_slot = input.skill_slot;
        }
        if (remote_kit_action == 0u && input.kit_action != 0u) {
            remote_kit_action = input.kit_action;
            remote_kit_slot = input.kit_slot;
        }
    }
    {
        int native_spirit;
        unsigned int owner;
        if (!SudekiMpCleanroomEngineSpiritPresentationState(&native_spirit) ||
            (native_spirit != 0 && (!host_spirit_owner(&owner) || owner == 1u))) {
            /* The caster's gun/CSkill cannot interrupt the global transaction.
             * Do not apply this gate to the other player's movement or fire. */
            remote_weak_requested = FALSE;
            host_remote_weak_held = FALSE;
            remote_skill_requested = FALSE;
            remote_kit_action = 0u;
            host_remote_direction_x = host_remote_direction_z = 0;
        }
    }
    if (remote_kit_action != 0u) {
        SudekiMpCharacterSkillState tal_skill, ailish_skill;
        int spirit_state;
        BOOL available = !remote_skill_requested &&
            !host_remote_skill_camera_active &&
            !SudekiMpCleanroomEngineRangedCombatPrimePending() &&
            SudekiMpObserveCharacterSkill(SudekiMpCleanroomEngineActorEntity(
                seat_host_actor()), &tal_skill) && !tal_skill.active &&
            SudekiMpObserveCharacterSkill(ailish, &ailish_skill) && !ailish_skill.active &&
            SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state) &&
            spirit_state == 0;
        if (available && remote_kit_action == SUDEKIMP_LAN_ARENA_KIT_WEAPON &&
            remote_kit_slot < 12u) {
            SudekiMpWeaponActivationResult result =
                SudekiMpActivateCharacterWeapon(ailish, remote_kit_slot);
            SudekiMpLogFormat("lan_arena_runtime event=host_kit kind=weapon slot=%u status=%s policy=native_host_equip_observed_snapshot\r\n",
                remote_kit_slot, SudekiMpWeaponActivationStatusName(result.status));
        } else if (available && remote_kit_action == SUDEKIMP_LAN_ARENA_KIT_SPIRIT &&
            seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE && remote_kit_slot < 2u) {
            service_host_operator_spirit(&status, (unsigned int)remote_kit_slot + 1u);
        } else {
            SudekiMpLogWrite("lan_arena_runtime event=host_kit status=busy policy=no_action_during_native_task\r\n");
        }
        remote_weak_requested = FALSE;
        host_remote_weak_held = FALSE;
    }
    if (remote_combat_toggle_rejected) {
        SudekiMpLogWrite(
            "lan_arena_runtime event=host_remote_cleanroom_combat_test "
            "phase=rejected reason=host_world_authority "
            "policy=ignore_legacy_client_toggle_without_dropping_movement\r\n");
    }
    if (remote_skill_requested) {
        SudekiMpSkillActivationResult skill_result;
        BOOL owner_view_captured = FALSE;
        BOOL owner_view_restored = TRUE;
        host_remote_skill_camera_suppression_logged = FALSE;
        ZeroMemory(&skill_result, sizeof(skill_result));
        skill_result.status = SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT;
        if (SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(TRUE) &&
            capture_host_tal_skill_view(remote_skill_slot)) {
            owner_view_captured = TRUE;
            InterlockedIncrement(&host_remote_skill_activation_depth);
            skill_result = SudekiMpReplayHostApprovedCharacterSkillSlot(
                ailish, remote_skill_slot);
            InterlockedDecrement(&host_remote_skill_activation_depth);
        }
        if (skill_result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED) {
            host_track_started_native_skill(1u, ailish, &skill_result);
            host_remote_skill_camera_active = TRUE;
            owner_view_restored = service_host_tal_skill_view(
                SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION);
        } else {
            if (owner_view_captured) {
                owner_view_restored = retire_host_tal_skill_view(
                    "remote_skill_activation_rejected");
            }
            if (owner_view_restored &&
                !host_tal_skill_view_lease.valid) {
                (void)SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(
                    FALSE);
            } else {
                host_remote_skill_camera_active = TRUE;
            }
        }
        SudekiMpLogFormat(
            "lan_arena_runtime event=host_remote_skill phase=%s actor=Ailish "
            "slot=%u validation=%d use=%u owner_view=%s "
            "policy=host_resolves_actor_slot_runs_native_validator_and_use_exact_tal_view_bracket\r\n",
            SudekiMpSkillActivationStatusName(skill_result.status),
            (unsigned int)remote_skill_slot,
            skill_result.validation_result,
            (unsigned int)skill_result.use_result,
            owner_view_restored ? "preserved" : "restore_pending");
    }
    remote_aim_valid = host_remote_aim_x != 0 || host_remote_aim_y != 0 ||
        host_remote_aim_z != 0;
    if (host_remote_ailish_owned && host_remote_first_person_active &&
        !host_actor_previous_skill_active[1]) {
        rapid_weapon_cycle_known = SudekiMpServiceRemoteRapidWeapon(
            SudekiMpCleanroomEngineActorEntity(seat_client_actor()),
            SudekiMpCleanroomEngineActorEntity(seat_host_actor()),
            frame_delta_seconds, &ranged_repeat_interval_ms);
        if (!rapid_weapon_cycle_known)
            ranged_repeat_interval_ms = HOST_REMOTE_RANGED_REPEAT_FALLBACK_MS;
    }
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
            if (ranged_native_ready_known && !rapid_weapon_cycle_known) {
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
                remote_aim_valid,
                remote_weak_active && !ranged_first_person_fire,
                frame_delta_seconds);
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
                        TRUE,
                        frame_delta_seconds);
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
                    "reload_delay_half=0x%04x cadence=%s "
                    "policy=native_host_execution\r\n",
                    ranged_world_fallback ?
                        "native_world_arbiter_ranged_fallback" :
                    ranged_first_person_fire ?
                        "verified_first_person_weapon_gate" :
                        "native_arbiter_combat_input",
                    (unsigned long)(ranged_first_person_fire ?
                        ranged_repeat_interval_ms : 0u),
                    (unsigned int)(ranged_first_person_fire ?
                        ranged_authored_delay_half : 0u),
                    rapid_weapon_cycle_known ? "equipped_native_fp_cycle" :
                        "conservative_reload_gate");
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
            0.0f, 0.0f, 0.0f, 0.0f, FALSE, FALSE,
            frame_delta_seconds)) {
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
    /* Preserve native presentation ordering. The game-thread render adapter
     * restores its exact-owner temporary cinematic view after native drawing;
     * socket workers never access native objects. */
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        SudekiMpCleanroomMenuRender();
    }
    original_frame_end();
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH)
        (void)SudekiMpLanArenaClientSpiritViewEndFrame();
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
    BOOL prepared = client_replica_frame_ready;
    BOOL replica_applied = FALSE;
    BOOL published_before_start = FALSE;
    BOOL published_after_start = FALSE;
    BOOL client_owner_view_refreshed = TRUE;
    DWORD publish_error = ERROR_SUCCESS;
    client_replica_frame_ready = FALSE;
    client_replica_frame_attempted = FALSE;
    /* Publish the sample prepared by the exact post-controller observer,
     * then publish the same sample once more after it. The primary component
     * update at the following 0x28d45b boundary rotates animation root motion
     * through CPosition's world basis; letting that update see the stale +Z
     * basis made only one world-cardinal locomotion direction look correct.
     * This wrapper never samples the network a second time. */
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        tal_initialized && ailish_initialized) {
        SudekiMpLanArenaClientObserveFirstPersonFrame(0u);
        replica_applied = prepared;
        if (replica_applied) {
            published_before_start =
                SudekiMpLanArenaClientReplicaPublishVisibleTransforms();
            if (!published_before_start) {
                publish_error = GetLastError();
            } else {
                /* START arrives ahead of the interpolation clock. Fire the
                 * one-shot only after Tal's matching selector and visible
                 * transform are installed, still on the proven game-thread
                 * boundary before native RenderStart. Presentation failure
                 * never rejects the authenticated replica frame. */
                (void)SudekiMpLanArenaClientReplicaServiceSpiritVfx();
            }
        }
    }
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        host_tal_skill_view_basis_safe_this_frame = FALSE;
    }
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH)
        SudekiMpLanArenaClientObserveFirstPersonFrame(1u);
    original_render_start();
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH)
        SudekiMpLanArenaClientObserveFirstPersonFrame(2u);
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        host_tal_skill_view_lease.valid) {
        /* This first RenderStart precedes the primary component/CSkill update
         * at 0x28d45b. It is the only per-frame basis we may refresh from. */
        host_tal_skill_view_basis_safe_this_frame =
            service_host_tal_skill_view(
                SUDEKIMP_LAN_ARENA_OWNER_VIEW_REFRESH_AFTER_OWNER_RENDER);
    }
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        client_owner_view_refreshed =
            SudekiMpLanArenaClientReplicaRefreshOwnerViewAfterRender();
    }
    if (replica_applied && client_owner_view_refreshed) {
        (void)SudekiMpLanArenaClientReplicaReassertPresentation();
    }
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        client_owner_view_refreshed) {
        (void)SudekiMpLanArenaClientReplicaReassertOwnerViewAfterRemoteMutation();
    }
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
        client_trace_native_presentation(0u, seat_host_actor());
        client_trace_native_presentation(1u, seat_client_actor());
        /* The primary scene renders at 0x28d473, BEFORE the later secondary
         * pre-world seam. Stage the caster view for that primary pass too. */
        if (published_after_start && client_owner_view_refreshed)
            (void)SudekiMpLanArenaClientSpiritViewBeginFrame();
        if (!client_replica_stream_logged) {
            client_replica_stream_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_runtime event=client_snapshot_replica phase=active "
                "apply_boundary=post_controller_once jitter_buffer_ms=100 "
                "visible_publish=before_and_after_first_plus_pre_world "
                "actors=Tal,Ailish enemy=training_dummy "
                "policy=single_network_sample_native_basis_before_root_motion_native_world_combat_state_replicated\r\n");
        }
    }
}

static void lan_arena_render_pre_world_entry(void) {
    BOOL presentation_reasserted = FALSE;
    BOOL published = FALSE;
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH)
        (void)SudekiMpLanArenaClientSpiritViewEndFrame();
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH)
        SudekiMpLanArenaClientObserveFirstPersonFrame(3u);
    original_render_start();
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH)
        SudekiMpLanArenaClientObserveFirstPersonFrame(4u);
    /* Do not refresh a camera lease here. The primary component/CSkill update
     * at 0x28d45b ran after the first RenderStart and may have replaced the
     * active render-state basis without selecting a camera. Reassert only the
     * basis proven at the first RenderStart (or captured immediately before a
     * remote Use that began between the two calls). */
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        tal_initialized && ailish_initialized) {
        SudekiMpLanArenaSessionStatus status;
        SudekiMpCharacterSkillState tal_skill_state;
        SudekiMpCharacterSkillState ailish_skill_state;
        void *tal_character = SudekiMpCleanroomEngineActorEntity(
            seat_host_actor());
        void *ailish_character = SudekiMpCleanroomEngineActorEntity(
            seat_client_actor());
        BOOL combat_enabled = FALSE;
        float tal_hp = 0.0f;
        float tal_sp = 0.0f;
        float ailish_hp = 0.0f;
        float ailish_sp = 0.0f;
        int spirit_state = 0;
        /* Refresh at the ownership boundary itself. A 20 Hz snapshot cache
         * is too old to protect native channels from an attack/reaction that
         * began later in the same frame. */
        host_trace_native_presentation(0u, seat_host_actor());
        host_trace_native_presentation(1u, seat_client_actor());
        BOOL host_exact = SudekiMpLanArenaSessionGetStatus(&status) &&
            status.peer_connected &&
            status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
            status.local_simulation_node_role ==
                SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD &&
            status.peer_simulation_node_role ==
                SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA &&
            SudekiMpCleanroomEngineCombatMode(&combat_enabled) &&
            combat_enabled;
        BOOL tal_alive = host_exact &&
            SudekiMpCleanroomEngineActorResources(
                seat_host_actor(), &tal_hp, &tal_sp) && tal_hp > 0.0f;
        BOOL ailish_alive = host_exact &&
            SudekiMpCleanroomEngineActorResources(
                seat_client_actor(),
                &ailish_hp, &ailish_sp) && ailish_hp > 0.0f;
        BOOL tal_skill_known = host_exact && tal_character != NULL &&
            SudekiMpObserveCharacterSkill(tal_character, &tal_skill_state) &&
            (tal_skill_state.active == 0u ||
             (tal_skill_state.slot >= 0 && tal_skill_state.slot < 6));
        BOOL ailish_skill_known = host_exact && ailish_character != NULL &&
            SudekiMpObserveCharacterSkill(
                ailish_character, &ailish_skill_state) &&
            (ailish_skill_state.active == 0u ||
             (ailish_skill_state.slot >= 0 &&
              ailish_skill_state.slot < 6));
        BOOL spirit_known = host_exact &&
            SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state);
        BOOL tal_skill_active = tal_skill_known &&
            tal_skill_state.active != 0u;
        BOOL ailish_skill_active = ailish_skill_known &&
            ailish_skill_state.active != 0u;
        BOOL tal_skill_startup_pending = tal_skill_known &&
            host_native_skill_startup_pending(
                0u, tal_character, &tal_skill_state);
        BOOL ailish_skill_startup_pending = ailish_skill_known &&
            host_native_skill_startup_pending(
                1u, ailish_character, &ailish_skill_state);
        BOOL tal_skill_owned = tal_skill_active ||
            tal_skill_startup_pending;
        BOOL ailish_skill_owned = ailish_skill_active ||
            ailish_skill_startup_pending;
        BOOL spirit_active = spirit_known && spirit_state != 0;
        unsigned int spirit_owner = 0u;
        if (spirit_active && !host_spirit_owner(&spirit_owner)) spirit_known = FALSE;
        BOOL tal_action_active = host_exact &&
            host_actor_native_action_variant(0u, combat_enabled) !=
                SUDEKIMP_LAN_ARENA_ACTION_NONE;
        BOOL ailish_action_active = host_exact &&
            host_actor_native_action_variant(1u, combat_enabled) !=
                SUDEKIMP_LAN_ARENA_ACTION_NONE;
        const BOOL ownership_active[2] = {
            host_exact && tal_skill_known && ailish_skill_known &&
                spirit_known && tal_alive && (ailish_skill_owned || (spirit_active && spirit_owner == 1u)) &&
                !tal_skill_owned && !(spirit_active && spirit_owner == 0u) && !tal_action_active,
            host_exact && tal_skill_known && ailish_skill_known &&
                spirit_known && ailish_alive && host_remote_ailish_owned &&
                (tal_skill_owned || (spirit_active && spirit_owner == 0u)) &&
                !ailish_skill_owned && !(spirit_active && spirit_owner == 1u) && !ailish_action_active
        };
        const SudekiMpCleanroomActor actors[2] = {
            seat_host_actor(),
            seat_client_actor()
        };
        unsigned int actor_index;
        for (actor_index = 0u; actor_index < 2u; ++actor_index) {
            const char *reason = NULL;
            SudekiMpNoncasterSkillLocomotionResult result =
                SudekiMpNoncasterSkillLocomotionService(
                    runtime_game_module,
                    SudekiMpCleanroomEngineActorEntity(actors[actor_index]),
                    actor_index == 0u ?
                        (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
                            SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_BUKI :
                            SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TAL) :
                        (seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
                            SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_ELCO :
                            SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_AILISH),
                    ownership_active[actor_index],
                    host_actor_locomotion_moving[actor_index],
                    &host_noncaster_locomotion_leases[actor_index],
                    &reason);
            int state = result ==
                SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED ? -1 :
                (ownership_active[actor_index] ?
                    (host_actor_locomotion_moving[actor_index] ? 2 : 1) : 0);
            if (state != host_noncaster_locomotion_trace_state[actor_index]) {
                host_noncaster_locomotion_trace_state[actor_index] = state;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=host_noncaster_locomotion "
                    "state=%s actor=%s moving=%u result=%u reason=%s "
                    "boundary=post_second_render_start_pre_world "
                    "policy=actor_local_base_channels_realtime_shared_simulation\r\n",
                    state < 0 ? "rejected" :
                        (state == 0 ? "inactive" : "active"),
                    actor_index == 0u ? "Tal" : "Ailish",
                    host_actor_locomotion_moving[actor_index] ? 1u : 0u,
                    (unsigned int)result,
                    reason != NULL ? reason : "unknown");
            }
        }
        if (host_tal_skill_view_lease.valid) {
            BOOL owner_view_reasserted =
                host_tal_skill_view_basis_safe_this_frame &&
                service_host_tal_skill_view(
                    SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION);
            int owner_view_state = owner_view_reasserted ? 1 : -1;
            if (owner_view_state != host_tal_skill_view_trace_state) {
                host_tal_skill_view_trace_state = owner_view_state;
                SudekiMpLogFormat(
                    "lan_arena_runtime event=host_remote_skill_view "
                    "state=%s owner=Tal remote_caster=Ailish slot=%u "
                    "revision=%lu boundary=post_second_render_start_pre_world "
                    "policy=refresh_live_owner_basis_then_reassert_after_remote_mutation\r\n",
                    owner_view_reasserted ? "active" : "rejected",
                    (unsigned int)host_tal_skill_view_slot,
                    (unsigned long)
                        host_tal_skill_view_lease.refresh_revision);
            }
        }
    }
    if (runtime_config.local_role ==
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        tal_initialized && ailish_initialized) {
        presentation_reasserted =
            SudekiMpLanArenaClientReplicaReassertPresentation();
        if (!SudekiMpLanArenaClientReplicaReassertOwnerViewAfterRemoteMutation()) {
            presentation_reasserted = FALSE;
        }
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
        if (published && presentation_reasserted)
            (void)SudekiMpLanArenaClientSpiritViewBeginFrame();
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
        SudekiMpLanArenaClientObserveFirstPersonFrame(5u);
    }
    if (runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        host_tal_skill_view_basis_safe_this_frame = FALSE;
    }
}

BOOL SudekiMpInstallLanArenaRuntime(
    HMODULE game_module,
    const SudekiMpLanArenaSessionConfig *config
) {
    uint8_t *base;
    if (!runtime_installed) {
        reset_host_operator_spirit_intent();
        host_operator_spirit_session_token = 0u;
    }
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
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
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
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpInstallRelativeCallHook(
            &lan_arena_render_start_hook,
            base + RVA_RENDER_START_CALL,
            original_render_start,
            lan_arena_render_start_entry)) {
        DWORD error = GetLastError();
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpInstallRelativeCallHook(
            &lan_arena_render_pre_world_hook,
            base + RVA_RENDER_PRE_WORLD_CALL,
            original_render_start,
            lan_arena_render_pre_world_entry)) {
        DWORD error = GetLastError();
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpInstallLanArenaSkillFade(game_module, caster_view_light)) {
        DWORD error=GetLastError();
        if(!rollback_lan_arena_frame_hooks()) return FALSE;
        if(!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end=NULL; original_render_start=NULL;
        SetLastError(error); return FALSE;
    }
    if ((config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
         !install_host_skill_isolation(base)) ||
        (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
         !SudekiMpInstallLanCastContext(game_module,runtime_cast_owner_witness))) {
        DWORD error = GetLastError();
        if(!SudekiMpUninstallLanCastContext()) {
            retain_runtime_after_hook_restore_failure(error); return FALSE;
        }
        if (host_skill_camera_hook.installed ||
            host_skill_speed_hook.installed) {
            retain_runtime_after_hook_restore_failure(error);
            return FALSE;
        }
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        (!SudekiMpInstallLanArenaSpiritAudioTrace(
            game_module,
            host_spirit_audio_active_witness,
            &runtime_config) ||
         !SudekiMpLanArenaSpiritVisualHostInitialize(game_module,
            host_spirit_visual_active_witness, &runtime_config))) {
        DWORD error = GetLastError();
        if (!rollback_host_spirit_audio_trace()) return FALSE;
        if (!restore_host_skill_isolation()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
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
        if (!uninstall_lan_arena_input_adapters()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_host_spirit_audio_trace()) return FALSE;
        if (!restore_host_skill_isolation()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        if (!SudekiMpResetLanArenaClientReplica()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        original_render_start = NULL;
        SetLastError(error);
        return FALSE;
    }
    if (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        discard_host_operator_spirit_requests("runtime_install");
        SudekiMpLanArenaHostInputSetNativeSkillStartObserver(
            host_native_tal_skill_started);
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
        if (!uninstall_lan_arena_input_adapters()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_host_spirit_audio_trace()) return FALSE;
        if (!restore_host_skill_isolation()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_lan_arena_frame_hooks()) return FALSE;
        if (!SudekiMpResetLanArenaClientReplica()) {
            retain_runtime_after_hook_restore_failure(GetLastError());
            return FALSE;
        }
        if (!rollback_lan_arena_campaign_guard()) return FALSE;
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCollisionDebug();
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
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    host_release_pending_logged = FALSE;
    client_release_pending_logged = FALSE;
    client_replica_stop_state = -1;
    client_animation_basis_publish_state = -1;
    client_presentation_reassert_state = -1;
    client_visible_transform_publish_state = -1;
    ZeroMemory(host_noncaster_locomotion_leases,
        sizeof(host_noncaster_locomotion_leases));
    host_noncaster_locomotion_trace_state[0] = -1;
    host_noncaster_locomotion_trace_state[1] = -1;
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
    ZeroMemory(host_actor_locomotion_moving,
        sizeof(host_actor_locomotion_moving));
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
    client_cleanroom_maintenance_last_at = 0u;
    reset_client_tal_action_timeline();
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    reset_host_skill_tracking();
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
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_active_generation = 0u;
    client_remote_tal_generation_actor = NULL;
    SudekiMpLanArenaClientReplicaSetRemoteTalLease(NULL, 0u);
    arena_dummy_spawn_attempted = FALSE;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    if (!start_network_pump()) {
        DWORD error = GetLastError();
        if (!SudekiMpUninstallLanArenaRuntime()) return FALSE;
        SetLastError(error);
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_runtime event=install render_start_call_rva=0x%08lx "
        "render_pre_world_call_rva=0x%08lx "
        "frame_end_call_rva=0x%08lx "
        "first_render_owner_basis=%s second_render_policy=reassert_only "
        "policy=game_thread_actor_adapters_plus_synchronized_socket_worker\r\n",
        (unsigned long)RVA_RENDER_START_CALL,
        (unsigned long)RVA_RENDER_PRE_WORLD_CALL,
        (unsigned long)RVA_FRAME_END_CALL,
        config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ?
            "client_ailish" : "host_tal"
    );
    return TRUE;
}

BOOL SudekiMpUninstallLanArenaRuntime(void) {
    BOOL actors_released;
    BOOL dummy_released;
    DWORD error;

    /* An operator intent is not a native task and must never outlive local
     * authority. The native UI prime that it may have started remains an
     * independent engine-owned teardown barrier until positively retired. */
    host_operator_spirit_session_token = 0u;
    discard_host_operator_spirit_requests("runtime_uninstall");
    /* Close transport admission first, but keep the observer and render
     * callbacks alive while a client-replayed native CSkill drains. */
    SudekiMpLanArenaSessionStop(runtime_installed);
    if (!reset_client_replica_for_teardown("runtime_uninstall")) return FALSE;
    if (!host_native_tasks_drained()) {
        error = GetLastError();
        retain_runtime_after_hook_restore_failure(error);
        return FALSE;
    }
    if (!uninstall_lan_arena_input_adapters()) {
        error = GetLastError();
        retain_runtime_after_hook_restore_failure(error);
        return FALSE;
    }
    if (!rollback_host_spirit_audio_trace()) return FALSE;
    if (!restore_host_skill_isolation()) {
        retain_runtime_after_hook_restore_failure(GetLastError());
        return FALSE;
    }
    actors_released =
        release_host_remote_ailish("runtime_uninstall") &&
        release_client_remote_tal("runtime_uninstall");
    dummy_released = release_arena_dummy();
    if (!actors_released || !dummy_released) {
        SetLastError(ERROR_RETRY);
        retain_runtime_after_hook_restore_failure(ERROR_RETRY);
        return FALSE;
    }
    /* Detours are the lifetime boundary. Remove them only after every native
     * task and actor lease is safely retired, and before clearing any callback
     * or resource they can reach. A failed restore retains all dependencies. */
    if (!rollback_lan_arena_frame_hooks()) return FALSE;
    if (!rollback_lan_arena_campaign_guard()) return FALSE;
    SudekiMpControlUpdateObserverGateDisable(&lan_arena_control_observer_gate);
    (void)SudekiMpControlSeparationUnregisterUpdateObserver(
        &lan_arena_control_observer_owner);
    SudekiMpControlUpdateObserverGateDrain(&lan_arena_control_observer_gate);
    (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
    SudekiMpUninstallLanArenaCollisionDebug();
    stop_network_pump();
    original_frame_end = NULL;
    original_render_start = NULL;
    runtime_installed = FALSE;
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
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
    ZeroMemory(host_actor_locomotion_moving,
        sizeof(host_actor_locomotion_moving));
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
    client_cleanroom_maintenance_last_at = 0u;
    reset_client_tal_action_timeline();
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    reset_host_skill_tracking();
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
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    arena_dummy_spawn_attempted = FALSE;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    ZeroMemory(&runtime_config, sizeof(runtime_config));
    runtime_remote_ipv4[0] = '\0';
    runtime_game_module = NULL;
    return TRUE;
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
    host_operator_spirit_session_token = 0u;
    discard_host_operator_spirit_requests("local_end_session");
    SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(FALSE);
    SudekiMpLanArenaSessionStop(TRUE);
    if (!reset_client_replica_for_teardown("local_end_session")) {
        SudekiMpLogWrite(
            "lan_arena_runtime event=local_end_session cleanup=deferred "
            "policy=wait_for_client_native_skill_drain_before_actor_release\r\n");
        return FALSE;
    }
    if (!host_native_tasks_drained()) {
        SudekiMpLogWrite(
            "lan_arena_runtime event=local_end_session cleanup=deferred "
            "policy=wait_for_host_native_skill_and_spirit_drain_before_actor_release\r\n");
        return FALSE;
    }
    actors_released = release_host_remote_ailish("local_end_session") &&
        release_client_remote_tal("local_end_session");
    dummy_released = release_arena_dummy();
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_snapshot_stream_logged = FALSE;
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
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
    ZeroMemory(host_actor_locomotion_moving,
        sizeof(host_actor_locomotion_moving));
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
    reset_host_skill_tracking();
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
    SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(FALSE);
    SudekiMpLanArenaSessionStop(TRUE);
    if (!reset_client_replica_for_teardown("client_rejoin")) {
        return FALSE;
    }
    if (!release_client_remote_tal("client_rejoin") ||
        !release_arena_dummy()) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    if (!SudekiMpInitializeLanArenaClientReplica(runtime_game_module)) {
        return FALSE;
    }
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
    host_operator_spirit_session_token = 0u;
    discard_host_operator_spirit_requests("host_restart");
    SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(FALSE);
    SudekiMpLanArenaSessionStop(TRUE);
    if (!host_native_tasks_drained()) return FALSE;
    if (!release_host_remote_ailish("host_restart") ||
        !release_arena_dummy()) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    SudekiMpLanArenaSharedSimulationReset(&canonical_simulation);
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
    host_ailish_spawn_attempted = FALSE;
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_last_translation_at_ms,
        sizeof(host_actor_last_translation_at_ms));
    ZeroMemory(host_replica_idle_position_valid,
        sizeof(host_replica_idle_position_valid));
    ZeroMemory(host_actor_was_moving,
        sizeof(host_actor_was_moving));
    ZeroMemory(host_actor_locomotion_moving,
        sizeof(host_actor_locomotion_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    host_ailish_weak_attack_until_ms = 0u;
    reset_host_action_tracking();
    reset_host_skill_tracking();
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
