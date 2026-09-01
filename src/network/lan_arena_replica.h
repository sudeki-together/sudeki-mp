#ifndef SUDEKIMP_LAN_ARENA_REPLICA_H
#define SUDEKIMP_LAN_ARENA_REPLICA_H

#include "network/lan_arena_protocol.h"

#include <windows.h>

typedef struct SudekiMpLanArenaReplica {
    SudekiMpLanArenaSnapshot previous;
    SudekiMpLanArenaSnapshot latest;
    uint8_t previous_valid;
    uint8_t latest_valid;
} SudekiMpLanArenaReplica;

/* Client-side presentation only. This module never accepts input, mutates an
 * actor, or creates damage; it turns authenticated host snapshots into a
 * display sample for the eventual native replica adapter. */
void SudekiMpLanArenaReplicaReset(SudekiMpLanArenaReplica *replica);
BOOL SudekiMpLanArenaReplicaPush(
    SudekiMpLanArenaReplica *replica,
    const SudekiMpLanArenaSnapshot *snapshot
);
BOOL SudekiMpLanArenaReplicaSample(
    const SudekiMpLanArenaReplica *replica,
    uint32_t host_tick,
    SudekiMpLanArenaSnapshot *sample
);

#endif
