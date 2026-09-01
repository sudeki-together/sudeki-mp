#include "hooks/lan_arena_runtime.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/lan_arena_client_input.h"
#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "hooks/lan_arena_host_input.h"
#include "network/lan_arena_authority.h"
#include "network/lan_arena_endpoint.h"

#include <string.h>
#include <stdint.h>

typedef void (*FrameEndFunction)(void);

enum {
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu
};

static SudekiMpRelativeCallHook lan_arena_frame_end_hook;
static FrameEndFunction original_frame_end;
static BOOL runtime_installed;
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
static BOOL dummy_release_pending_logged;
static float host_previous_actor_position[2][3];
static BOOL host_previous_actor_position_valid[2];
static DWORD host_ailish_weak_attack_until_ms;
static DWORD host_tal_weak_attack_until_ms;
static DWORD host_last_remote_input_at_ms;
static BOOL host_remote_input_quiesced;
static BOOL host_remote_input_logged;
static BOOL host_remote_weak_logged;
static BOOL host_remote_ailish_moving;
static DWORD host_actor_moving_until_ms[2];
static SudekiMpCleanroomActorPresentation host_actor_presentation[2];
static BOOL host_actor_presentation_valid[2];
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

static void host_apply_presentation_state(
    unsigned int actor_index,
    DWORD now_ms,
    SudekiMpLanArenaActorSnapshot *snapshot
) {
    float dx;
    float dy;
    float dz;
    BOOL moving = FALSE;
    if (snapshot == NULL || actor_index >= 2u) return;
    if (host_previous_actor_position_valid[actor_index]) {
        dx = snapshot->x - host_previous_actor_position[actor_index][0];
        dy = snapshot->y - host_previous_actor_position[actor_index][1];
        dz = snapshot->z - host_previous_actor_position[actor_index][2];
        if (dx * dx + dy * dy + dz * dz > 0.000025f) {
            host_actor_moving_until_ms[actor_index] = now_ms + 125u;
        }
    }
    moving = actor_index == 1u && host_remote_ailish_owned ?
        host_remote_ailish_moving :
        (LONG)(host_actor_moving_until_ms[actor_index] - now_ms) > 0;
    host_previous_actor_position[actor_index][0] = snapshot->x;
    host_previous_actor_position[actor_index][1] = snapshot->y;
    host_previous_actor_position[actor_index][2] = snapshot->z;
    host_previous_actor_position_valid[actor_index] = TRUE;
    if (snapshot->hp == 0u) {
        snapshot->animation_state =
            SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED;
        snapshot->combat_state =
            SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED;
    } else if ((actor_index == 0u &&
                (LONG)(host_tal_weak_attack_until_ms - now_ms) > 0) ||
               (actor_index == 1u &&
                (LONG)(host_ailish_weak_attack_until_ms - now_ms) > 0)) {
        snapshot->animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
        snapshot->combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    } else {
        snapshot->animation_state = moving ?
            SUDEKIMP_LAN_ARENA_ANIMATION_MOVING :
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
        snapshot->combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    }
}

static void host_trace_native_presentation(
    unsigned int actor_index,
    SudekiMpCleanroomActor actor
) {
    SudekiMpCleanroomActorPresentation current;
    if (actor_index >= 2u ||
        !SudekiMpCleanroomEngineActorPresentation(actor, &current)) return;
    if (host_actor_presentation_valid[actor_index] &&
        memcmp(&host_actor_presentation[actor_index],
            &current, sizeof(current)) == 0) return;
    host_actor_presentation[actor_index] = current;
    host_actor_presentation_valid[actor_index] = TRUE;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_native_presentation actor=%s "
        "submodels=%lu selectors=%ld,%ld,%ld,%ld,%ld "
        "states=%u,%u,%u,%u,%u "
        "rates=%.5f,%.5f,%.5f,%.5f,%.5f "
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
        current.blend[0], current.blend[1],
        current.blend[2], current.blend[3]
    );
}

static void host_publish_snapshot(DWORD now_ms) {
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaSnapshot snapshot;

    if (host_last_snapshot_at_ms != 0u &&
        (DWORD)(now_ms - host_last_snapshot_at_ms) < 50u) return;
    if (!SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
        status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) return;
    if (SudekiMpLanArenaHostInputTakeTalWeakAttack()) {
        host_tal_weak_attack_until_ms = now_ms + 250u;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    if (!fill_actor_snapshot(
            SUDEKIMP_CLEANROOM_TAL, SUDEKIMP_LAN_ARENA_TAL_TYPE,
            &snapshot.tal) ||
        !fill_actor_snapshot(
            SUDEKIMP_CLEANROOM_AILISH, SUDEKIMP_LAN_ARENA_AILISH_TYPE,
            &snapshot.ailish)) {
        return;
    }
    host_apply_presentation_state(0u, now_ms, &snapshot.tal);
    host_apply_presentation_state(1u, now_ms, &snapshot.ailish);
    if (tal_initialized) {
        host_trace_native_presentation(0u, SUDEKIMP_CLEANROOM_TAL);
    }
    if (ailish_initialized) {
        host_trace_native_presentation(1u, SUDEKIMP_CLEANROOM_AILISH);
    }
    snapshot.host_tick = now_ms;
    snapshot.match_state = 1u;
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
    if (SudekiMpLanArenaSessionSendSnapshot(&snapshot)) {
        host_last_snapshot_at_ms = now_ms;
        if (!host_snapshot_stream_logged) {
            host_snapshot_stream_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_snapshot_stream phase=active "
                "cadence_ms=50 actors=Tal,Ailish enemies=%u "
                "policy=host_authoritative_resources_transforms_facing_match_state\r\n",
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
    host_remote_ailish_moving = FALSE;
    host_release_pending_logged = FALSE;
    SudekiMpLogFormat(
        "lan_arena_runtime event=host_ailish_release phase=confirmed reason=%s "
        "policy=stop_input_then_native_ai_restore\r\n",
        reason == NULL ? "session_inactive" : reason);
    return TRUE;
}

static BOOL release_client_remote_tal(const char *reason) {
    if (client_remote_tal_owned) {
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
    (void)controller;
    (void)update_data;
    if (!SudekiMpControlUpdateObserverGateTryEnter(
            &lan_arena_control_observer_gate)) return;
    if (!SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness) ||
        !SudekiMpLanArenaSessionGetStatus(&status) ||
        !status.peer_connected) {
        release_host_remote_ailish("session_not_authenticated");
        release_client_remote_tal("session_not_authenticated");
        release_arena_dummy();
        if (runtime_config.local_role ==
                SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
            SudekiMpLanArenaClientReplicaDiscardSnapshots();
            if (!client_replica_discard_logged) {
                client_replica_discard_logged = TRUE;
                SudekiMpLogWrite(
                    "lan_arena_runtime event=client_snapshot_replica phase=discarded "
                    "reason=transport_authority_inactive "
                    "policy=no_stale_snapshot_sampling\r\n");
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
        if (tal_initialized && ailish_initialized &&
            SudekiMpLanArenaClientReplicaApplyLatest() &&
            !client_replica_stream_logged) {
            client_replica_stream_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_runtime event=client_snapshot_replica phase=active "
                "apply_boundary=post_controller_pre_render "
                "interpolation_delay_ms=50 actors=Tal,Ailish enemy=training_dummy "
                "policy=no_client_combat_or_enemy_authority\r\n");
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
        BOOL weak_attack = input.weak_attack_pressed != 0u;
        BOOL submitted;
        host_last_remote_input_at_ms = GetTickCount();
        host_remote_input_quiesced = FALSE;
        submitted = SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
                (float)input.world_direction_x / 32767.0f,
                (float)input.world_direction_z / 32767.0f,
                weak_attack);
        if (submitted) {
            host_remote_ailish_moving =
                input.world_direction_x != 0 || input.world_direction_z != 0;
        }
        if (submitted && !host_remote_input_logged &&
            (input.world_direction_x != 0 || input.world_direction_z != 0)) {
            host_remote_input_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_runtime event=host_remote_input phase=submitted "
                "world_direction=%d,%d policy=native_ailish_arbiter\r\n",
                (int)input.world_direction_x,
                (int)input.world_direction_z);
        }
        if (submitted && weak_attack) {
            host_ailish_weak_attack_until_ms = GetTickCount() + 250u;
            if (!host_remote_weak_logged) {
                host_remote_weak_logged = TRUE;
                SudekiMpLogWrite(
                    "lan_arena_runtime event=host_remote_weak_attack "
                    "phase=submitted policy=native_host_execution\r\n");
            }
        }
    }
    if (host_remote_ailish_owned && !host_remote_input_quiesced &&
        !SudekiMpLanArenaRemoteInputFresh(
            host_last_remote_input_at_ms, GetTickCount(), 250u) &&
        SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
            0.0f, 0.0f, FALSE)) {
        host_remote_input_quiesced = TRUE;
        host_remote_ailish_moving = FALSE;
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
    host_publish_snapshot(GetTickCount());
}

BOOL SudekiMpInstallLanArenaRuntime(
    HMODULE game_module,
    const SudekiMpLanArenaSessionConfig *config
) {
    uint8_t *base;
    if (game_module == NULL || config == NULL || runtime_installed ||
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
    original_frame_end = (FrameEndFunction)(base + RVA_FRAME_END);
    if (!SudekiMpInstallRelativeCallHook(
            &lan_arena_frame_end_hook,
            base + RVA_FRAME_END_CALL,
            original_frame_end,
            lan_arena_frame_end_entry)) {
        DWORD error = GetLastError();
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        SetLastError(error);
        return FALSE;
    }
    if ((config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
         !SudekiMpInstallLanArenaHostInput(game_module)) ||
        (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
         (!SudekiMpInstallLanArenaClientInput(game_module) ||
          !SudekiMpInitializeLanArenaClientReplica(game_module)))) {
        DWORD error = GetLastError();
        SudekiMpRestoreRelativeCallHook(&lan_arena_frame_end_hook);
        SudekiMpUninstallLanArenaClientInput();
        SudekiMpUninstallLanArenaHostInput();
        SudekiMpResetLanArenaClientReplica();
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
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
        SudekiMpRestoreRelativeCallHook(&lan_arena_frame_end_hook);
        SudekiMpUninstallLanArenaClientInput();
        SudekiMpUninstallLanArenaHostInput();
        SudekiMpResetLanArenaClientReplica();
        SudekiMpLanArenaSessionStop(FALSE);
        SudekiMpUninstallLanArenaCampaignGuard();
        (void)SudekiMpControlSeparationSetManualToggleEnabled(TRUE);
        original_frame_end = NULL;
        SetLastError(error);
        return FALSE;
    }
    runtime_installed = TRUE;
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    host_release_pending_logged = FALSE;
    client_release_pending_logged = FALSE;
    dummy_release_pending_logged = FALSE;
    ZeroMemory(host_previous_actor_position,
        sizeof(host_previous_actor_position));
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_moving_until_ms,
        sizeof(host_actor_moving_until_ms));
    ZeroMemory(host_actor_presentation,
        sizeof(host_actor_presentation));
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    host_ailish_weak_attack_until_ms = 0u;
    host_tal_weak_attack_until_ms = 0u;
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_ailish_moving = FALSE;
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
        "lan_arena_runtime event=install frame_end_call_rva=0x%08lx "
        "policy=game_thread_actor_adapters_plus_synchronized_socket_worker\r\n",
        (unsigned long)RVA_FRAME_END_CALL
    );
    return TRUE;
}

void SudekiMpUninstallLanArenaRuntime(void) {
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
    SudekiMpUninstallLanArenaCampaignGuard();
    stop_network_pump();
    SudekiMpLanArenaSessionStop(runtime_installed);
    SudekiMpRestoreRelativeCallHook(&lan_arena_frame_end_hook);
    original_frame_end = NULL;
    runtime_installed = FALSE;
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    host_release_pending_logged = FALSE;
    client_release_pending_logged = FALSE;
    dummy_release_pending_logged = FALSE;
    ZeroMemory(host_previous_actor_position,
        sizeof(host_previous_actor_position));
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_moving_until_ms,
        sizeof(host_actor_moving_until_ms));
    ZeroMemory(host_actor_presentation,
        sizeof(host_actor_presentation));
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    host_ailish_weak_attack_until_ms = 0u;
    host_tal_weak_attack_until_ms = 0u;
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
    host_remote_ailish_moving = FALSE;
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
    SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(FALSE);
    actors_released = release_host_remote_ailish("local_end_session") &&
        release_client_remote_tal("local_end_session");
    dummy_released = release_arena_dummy();
    SudekiMpLanArenaSessionStop(TRUE);
    SudekiMpResetLanArenaClientReplica();
    host_snapshot_stream_logged = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    host_ailish_weak_attack_until_ms = 0u;
    host_tal_weak_attack_until_ms = 0u;
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
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
    strcpy(runtime_remote_ipv4, parsed_ipv4);
    runtime_config.remote_ipv4 = runtime_remote_ipv4;
    runtime_config.port = parsed_port;
    if (!SudekiMpLanArenaSessionStart(&runtime_config)) return FALSE;
    client_tal_spawn_attempted = FALSE;
    client_replica_stream_logged = FALSE;
    client_replica_discard_logged = FALSE;
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
    if (!release_host_remote_ailish("host_restart") ||
        !release_arena_dummy()) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    SudekiMpLanArenaSessionStop(TRUE);
    host_last_snapshot_at_ms = 0u;
    host_snapshot_stream_logged = FALSE;
    host_ailish_spawn_attempted = FALSE;
    ZeroMemory(host_previous_actor_position_valid,
        sizeof(host_previous_actor_position_valid));
    ZeroMemory(host_actor_presentation_valid,
        sizeof(host_actor_presentation_valid));
    host_ailish_weak_attack_until_ms = 0u;
    host_tal_weak_attack_until_ms = 0u;
    host_last_remote_input_at_ms = 0u;
    host_remote_input_quiesced = FALSE;
    host_remote_input_logged = FALSE;
    host_remote_weak_logged = FALSE;
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
