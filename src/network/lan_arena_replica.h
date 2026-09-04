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
/* Advances monotonically from local elapsed time, with bounded catch-up toward
 * one authoritative snapshot behind latest after client throttling. It never
 * rewinds or extrapolates. Returns FALSE until a full three-snapshot jitter
 * buffer exists. */
BOOL SudekiMpLanArenaReplicaRenderClockAdvance(
    const SudekiMpLanArenaReplica *replica,
    SudekiMpLanArenaReplicaRenderClock *clock,
    uint32_t local_tick,
    uint32_t *host_tick
);
/* The replica adapter disables backlog catch-up while an authoritative action
 * is buffered. That keeps one local millisecond equal to one host animation
 * millisecond instead of visibly compressing a combo at up to 2x speed. */
BOOL SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
    const SudekiMpLanArenaReplica *replica,
    SudekiMpLanArenaReplicaRenderClock *clock,
    uint32_t local_tick,
    BOOL allow_catchup,
    uint32_t *host_tick
);
BOOL SudekiMpLanArenaReplicaActionTimelineBuffered(
    const SudekiMpLanArenaReplica *replica
);

/* The fixed Ailish client may create presentation-only native skill tasks for
 * either arena actor after host authorization. Damage and resources remain
 * host-owned; the client skill hooks keep world time realtime and preserve
 * Ailish's camera when the remote Tal task requests its cinematic camera. */
BOOL SudekiMpLanArenaClientNativeSkillTaskAllowed(
    uint8_t actor_type,
    uint8_t local_actor_type
);

/* A host-approved cast still has to enter CSkill::Use through a genuinely
 * valid local actor state. Result 2 is the known ranged-strafe case; result 3
 * may be repaired only when the authenticated host says combat is active.
 * Neither result is safe to bypass inside CSkill::Use. */
BOOL SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(
    int native_result,
    BOOL host_combat_authorized
);

#endif
