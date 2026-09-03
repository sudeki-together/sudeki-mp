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
    uint32_t tal_hp;
    uint32_t tal_sp;
    uint32_t ailish_hp;
    uint32_t ailish_sp;
    SudekiMpLanArenaEnemySnapshot
        enemies[SUDEKIMP_LAN_ARENA_MAX_ENEMIES];
    uint8_t match_state;
    uint8_t combat_enabled;
    uint8_t enemy_count;
    uint8_t native_combat_observed;
    uint8_t native_resources_observed;
    uint8_t native_enemies_observed;
} SudekiMpLanArenaNativeWorldObservation;

typedef struct SudekiMpLanArenaActorObservation {
    SudekiMpLanArenaActorSnapshot actor;
    uint8_t native_actor_observed;
} SudekiMpLanArenaActorObservation;

typedef struct SudekiMpLanArenaSharedSimulation {
    SudekiMpLanArenaSnapshot frame;
    SudekiMpLanArenaInput player_input[2];
    uint64_t session_token;
    uint32_t revision;
    uint32_t last_host_tick;
    uint32_t player_input_revision[2];
    uint8_t player_input_valid_mask;
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

/* Player processes contribute bounded intent, never world state.  Admission
 * belongs to the canonical simulation and is tracked independently for Tal
 * and Ailish so a later transport can carry either participant without
 * changing native-world ownership. */
int SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    uint8_t actor_type,
    const SudekiMpLanArenaInput *input
);
int SudekiMpLanArenaSharedSimulationReadPlayerInput(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint8_t actor_type,
    SudekiMpLanArenaInput *input,
    uint32_t *revision
);

/* The reducer composes a canonical frame from two independently observed
 * actors and the native-world consequence domain.  The world observation
 * owns match/combat, actor resources, and enemy state; actor observations own
 * only their transforms and process-independent presentation. */
int SudekiMpLanArenaSharedSimulationCommitNativeFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaNativeWorldObservation *observation,
    const SudekiMpLanArenaActorObservation *tal,
    const SudekiMpLanArenaActorObservation *ailish
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
