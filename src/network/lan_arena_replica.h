#ifndef SUDEKIMP_LAN_ARENA_REPLICA_H
#define SUDEKIMP_LAN_ARENA_REPLICA_H

#include "network/lan_arena_protocol.h"

#include <windows.h>

typedef struct SudekiMpLanArenaReplica {
    SudekiMpLanArenaSnapshot earliest;
    SudekiMpLanArenaSnapshot oldest;
    SudekiMpLanArenaSnapshot previous;
    SudekiMpLanArenaSnapshot latest;
    uint8_t earliest_valid;
    uint8_t oldest_valid;
    uint8_t previous_valid;
    uint8_t latest_valid;
    uint32_t stream_generation;
} SudekiMpLanArenaReplica;

typedef struct SudekiMpLanArenaReplicaRenderClock {
    uint32_t host_tick;
    uint32_t local_tick;
    uint32_t stream_generation;
    uint8_t initialized;
} SudekiMpLanArenaReplicaRenderClock;

/* The host publishes at this fixed cadence. The client starts after three
 * samples and retains a fourth history slot so ordinary packet-arrival jitter
 * cannot evict the segment beneath its monotonic presentation clock. */
enum { SUDEKIMP_LAN_ARENA_SNAPSHOT_INTERVAL_MS = 50u };

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
void SudekiMpLanArenaReplicaRenderClockReset(
    SudekiMpLanArenaReplicaRenderClock *clock
);
/* Advances strictly by local elapsed time and never re-anchors to packet
 * arrival. Returns FALSE until a full three-snapshot jitter buffer exists. */
BOOL SudekiMpLanArenaReplicaRenderClockAdvance(
    const SudekiMpLanArenaReplica *replica,
    SudekiMpLanArenaReplicaRenderClock *clock,
    uint32_t local_tick,
    uint32_t *host_tick
);

#endif
