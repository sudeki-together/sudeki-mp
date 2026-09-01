#ifndef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H
#define SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H

#include <windows.h>
#include <stdint.h>

/* Pure exact-image presentation policy used by tests and the replica adapter.
 * The returned selector is local-only and never enters a LAN packet. */
BOOL SudekiMpLanArenaClientIdleVariantSelector(
    uint8_t actor_type,
    uint8_t animation_state,
    int *selector
);

/* Applies authenticated host transforms/facing/resources for local Ailish,
 * the AI-disabled Tal replica, and the fixed training dummy. */
BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module);
/* Discard interpolation history without removing the exact-image native
 * adapters. Used synchronously when transport authority is lost. */
void SudekiMpLanArenaClientReplicaDiscardSnapshots(void);
void SudekiMpResetLanArenaClientReplica(void);
BOOL SudekiMpLanArenaClientReplicaApplyLatest(void);

#endif
