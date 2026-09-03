#ifndef SUDEKIMP_LAN_ARENA_SHARED_SIMULATION_H
#define SUDEKIMP_LAN_ARENA_SHARED_SIMULATION_H

#include "network/lan_arena_protocol.h"

#include <stdint.h>

/* The simulation role is deliberately independent of the player role.  The
 * current listen server gives the canonical role to Tal's process, but a
 * later dedicated process can own it without changing actor ownership. */
typedef enum SudekiMpLanArenaSimulationNodeRole {
    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_INVALID = 0,
    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD = 1,
    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA = 2
} SudekiMpLanArenaSimulationNodeRole;

typedef struct SudekiMpLanArenaNativeWorldObservation {
    uint32_t host_tick;
    uint8_t match_state;
    uint8_t combat_enabled;
    uint8_t native_combat_observed;
} SudekiMpLanArenaNativeWorldObservation;

typedef struct SudekiMpLanArenaSharedSimulation {
    SudekiMpLanArenaSnapshot frame;
    uint64_t session_token;
    uint32_t revision;
    uint32_t last_host_tick;
    uint8_t node_role;
    uint8_t frame_valid;
    uint8_t tick_initialized;
} SudekiMpLanArenaSharedSimulation;

void SudekiMpLanArenaSharedSimulationReset(
    SudekiMpLanArenaSharedSimulation *simulation
);
int SudekiMpLanArenaSharedSimulationBegin(
    SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSimulationNodeRole node_role,
    uint64_t session_token
);
int SudekiMpLanArenaSharedSimulationSessionExact(
    const SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSimulationNodeRole node_role,
    uint64_t session_token
);

/* Canonical frames are admitted only with a matching native-world
 * observation.  The observation overwrites match/combat fields so neither a
 * player input nor an adapter-generated candidate can author those states. */
int SudekiMpLanArenaSharedSimulationCommitNativeFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaNativeWorldObservation *observation,
    const SudekiMpLanArenaSnapshot *candidate
);

/* Replicas may consume authenticated canonical frames but cannot commit a
 * native-world observation of their own. */
int SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaSnapshot *frame
);
int SudekiMpLanArenaSharedSimulationReadFrame(
    const SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSnapshot *frame,
    uint32_t *revision
);

#endif
