#ifndef SUDEKIMP_LAN_ARENA_HIT_FEEDBACK_H
#define SUDEKIMP_LAN_ARENA_HIT_FEEDBACK_H

#include "network/lan_arena_protocol.h"
#include <windows.h>

/* Fresh, exact named-dummy lease supplied only on the game thread. */
typedef struct SudekiMpLanHitTarget {
    void *entity;
    void *combat;
    void *ui;
    void *arbiter;
    uint64_t session;
} SudekiMpLanHitTarget;
typedef BOOL (*SudekiMpLanHitWitness)(SudekiMpLanHitTarget *target);

typedef struct SudekiMpLanHitCursor {
    uint64_t session;
    uintptr_t target;
    uint32_t generation;
    uint32_t sequence;
    uint8_t initialized;
} SudekiMpLanHitCursor;
typedef BOOL (*SudekiMpLanHitReplay)(void *context,
    const SudekiMpLanArenaHitFeedback *hit);

/* Baseline/replaced targets don't replay old history. Successfully replayed or
 * expired events retire once; a failed replay remains retryable. */
BOOL SudekiMpLanHitConsume(SudekiMpLanHitCursor *cursor, uint64_t session,
    uintptr_t target, uint32_t host_tick,
    const SudekiMpLanArenaEnemySnapshot *enemy,
    SudekiMpLanHitReplay replay, void *context);

BOOL SudekiMpLanHitImageMatches(HMODULE image);
BOOL SudekiMpLanHitHostInstall(HMODULE image, SudekiMpLanHitWitness witness);
BOOL SudekiMpLanHitHostUninstall(void);
void SudekiMpLanHitHostSnapshot(SudekiMpLanArenaEnemySnapshot *enemy);
BOOL SudekiMpLanHitReplayNative(HMODULE image,
    const SudekiMpLanHitTarget *target, const SudekiMpLanArenaHitFeedback *hit);
BOOL SudekiMpLanHitTargetValid(const SudekiMpLanHitTarget *target);
BOOL SudekiMpLanHitResolveTarget(void *entity, uint64_t session,
    SudekiMpLanHitTarget *target);

#endif
