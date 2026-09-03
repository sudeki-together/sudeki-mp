#include "network/lan_arena_shared_simulation.h"

#include <string.h>

static int valid_node_role(SudekiMpLanArenaSimulationNodeRole node_role) {
    return node_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        node_role == SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
}

static int actor_index(uint8_t actor_type) {
    if (actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE) return 0;
    if (actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE) return 1;
    return -1;
}

static int valid_observation(
    const SudekiMpLanArenaNativeWorldObservation *observation
) {
    return observation != NULL &&
        observation->match_state <= SUDEKIMP_LAN_ARENA_MATCH_ENDED &&
        observation->combat_enabled <= 1u &&
        observation->enemy_count <= SUDEKIMP_LAN_ARENA_MAX_ENEMIES &&
        observation->native_combat_observed == 1u &&
        observation->native_resources_observed == 1u &&
        observation->native_enemies_observed == 1u;
}

static int next_tick_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint32_t host_tick
) {
    return !simulation->tick_initialized ||
        SudekiMpLanArenaSequenceNewer(host_tick, simulation->last_host_tick);
}

static int next_match_state_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint8_t match_state
) {
    uint8_t previous;
    if (!simulation->frame_valid) {
        return match_state == SUDEKIMP_LAN_ARENA_MATCH_WAITING ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    }
    previous = simulation->frame.match_state;
    if (previous == SUDEKIMP_LAN_ARENA_MATCH_WAITING) {
        return match_state == SUDEKIMP_LAN_ARENA_MATCH_WAITING ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED;
    }
    if (previous == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE) {
        return match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED;
    }
    return previous == SUDEKIMP_LAN_ARENA_MATCH_ENDED &&
        match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED;
}

static void commit_frame(
    SudekiMpLanArenaSharedSimulation *simulation,
    const SudekiMpLanArenaSnapshot *frame
) {
    simulation->frame = *frame;
    simulation->last_host_tick = frame->host_tick;
    simulation->tick_initialized = 1u;
    simulation->frame_valid = 1u;
    ++simulation->revision;
    if (simulation->revision == 0u) simulation->revision = 1u;
}

void SudekiMpLanArenaSharedSimulationReset(
    SudekiMpLanArenaSharedSimulation *simulation
) {
    if (simulation != NULL) memset(simulation, 0, sizeof(*simulation));
}

int SudekiMpLanArenaSharedSimulationBegin(
    SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSimulationNodeRole node_role,
    uint64_t session_token
) {
    if (simulation == NULL || !valid_node_role(node_role) ||
        session_token == 0u) return 0;
    memset(simulation, 0, sizeof(*simulation));
    simulation->node_role = (uint8_t)node_role;
    simulation->session_token = session_token;
    return 1;
}

int SudekiMpLanArenaSharedSimulationSessionExact(
    const SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSimulationNodeRole node_role,
    uint64_t session_token
) {
    return simulation != NULL && valid_node_role(node_role) &&
        session_token != 0u && simulation->node_role == (uint8_t)node_role &&
        simulation->session_token == session_token;
}

int SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    uint8_t actor_type,
    const SudekiMpLanArenaInput *input
) {
    int index = actor_index(actor_type);
    uint8_t mask;
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
            session_token) || index < 0 ||
        !SudekiMpLanArenaInputValid(input) ||
        (simulation->frame_valid &&
         simulation->frame.match_state ==
             SUDEKIMP_LAN_ARENA_MATCH_ENDED)) return 0;
    mask = (uint8_t)(1u << (unsigned int)index);
    if ((simulation->player_input_valid_mask & mask) != 0u &&
        !SudekiMpLanArenaSequenceNewer(
            input->sequence,
            simulation->player_input[index].sequence)) return 0;
    simulation->player_input[index] = *input;
    simulation->player_input_valid_mask |= mask;
    ++simulation->player_input_revision[index];
    if (simulation->player_input_revision[index] == 0u) {
        simulation->player_input_revision[index] = 1u;
    }
    return 1;
}

int SudekiMpLanArenaSharedSimulationReadPlayerInput(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint8_t actor_type,
    SudekiMpLanArenaInput *input,
    uint32_t *revision
) {
    int index = actor_index(actor_type);
    uint8_t mask;
    if (simulation == NULL || input == NULL || index < 0 ||
        simulation->session_token == 0u ||
        simulation->node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) {
        return 0;
    }
    mask = (uint8_t)(1u << (unsigned int)index);
    if ((simulation->player_input_valid_mask & mask) == 0u) return 0;
    *input = simulation->player_input[index];
    if (revision != NULL) {
        *revision = simulation->player_input_revision[index];
    }
    return 1;
}

int SudekiMpLanArenaSharedSimulationCommitNativeFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaNativeWorldObservation *observation,
    const SudekiMpLanArenaSnapshot *candidate
) {
    SudekiMpLanArenaSnapshot committed;
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
            session_token) ||
        !valid_observation(observation) || candidate == NULL ||
        candidate->host_tick != observation->host_tick ||
        !next_match_state_allowed(simulation, observation->match_state) ||
        !next_tick_allowed(simulation, observation->host_tick)) return 0;
    committed = *candidate;
    committed.host_tick = observation->host_tick;
    committed.match_state = observation->match_state;
    committed.combat_enabled = observation->combat_enabled;
    committed.tal.hp = observation->tal_hp;
    committed.tal.sp = observation->tal_sp;
    committed.ailish.hp = observation->ailish_hp;
    committed.ailish.sp = observation->ailish_sp;
    committed.enemy_count = observation->enemy_count;
    memcpy(committed.enemies, observation->enemies,
        sizeof(committed.enemies));
    if ((simulation->player_input_valid_mask & 0x02u) != 0u) {
        committed.acknowledged_input =
            simulation->player_input[1].sequence;
    } else {
        committed.acknowledged_input = 0u;
    }
    if (!SudekiMpLanArenaSnapshotValid(&committed)) return 0;
    commit_frame(simulation, &committed);
    return 1;
}

int SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaSnapshot *frame
) {
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            simulation, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
            session_token) || frame == NULL ||
        !SudekiMpLanArenaSnapshotValid(frame) ||
        !next_match_state_allowed(simulation, frame->match_state) ||
        (simulation->frame_valid &&
         simulation->frame.acknowledged_input != 0u &&
         frame->acknowledged_input !=
             simulation->frame.acknowledged_input &&
         !SudekiMpLanArenaSequenceNewer(
             frame->acknowledged_input,
             simulation->frame.acknowledged_input)) ||
        !next_tick_allowed(simulation, frame->host_tick)) return 0;
    commit_frame(simulation, frame);
    return 1;
}

int SudekiMpLanArenaSharedSimulationReadFrame(
    const SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSnapshot *frame,
    uint32_t *revision
) {
    if (simulation == NULL || frame == NULL || !simulation->frame_valid ||
        !valid_node_role((SudekiMpLanArenaSimulationNodeRole)
            simulation->node_role) || simulation->session_token == 0u) {
        return 0;
    }
    *frame = simulation->frame;
    if (revision != NULL) *revision = simulation->revision;
    return 1;
}
