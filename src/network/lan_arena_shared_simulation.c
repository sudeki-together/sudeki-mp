#include "network/lan_arena_shared_simulation.h"

#include <string.h>

static int valid_node_role(SudekiMpLanArenaSimulationNodeRole node_role) {
    return node_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        node_role == SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
}

static int valid_observation(
    const SudekiMpLanArenaNativeWorldObservation *observation
) {
    return observation != NULL &&
        observation->match_state <= SUDEKIMP_LAN_ARENA_MATCH_ENDED &&
        observation->combat_enabled <= 1u &&
        observation->native_combat_observed == 1u;
}

static int next_tick_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint32_t host_tick
) {
    return !simulation->tick_initialized ||
        SudekiMpLanArenaSequenceNewer(host_tick, simulation->last_host_tick);
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
        !next_tick_allowed(simulation, observation->host_tick)) return 0;
    committed = *candidate;
    committed.host_tick = observation->host_tick;
    committed.match_state = observation->match_state;
    committed.combat_enabled = observation->combat_enabled;
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
