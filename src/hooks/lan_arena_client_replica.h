#ifndef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H
#define SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_H

#include <windows.h>

/* Applies authenticated host transforms/facing/resources for local Ailish,
 * the AI-disabled Tal replica, and the fixed training dummy. */
BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module);
/* Discard interpolation history without removing the exact-image native
 * adapters. Used synchronously when transport authority is lost. */
void SudekiMpLanArenaClientReplicaDiscardSnapshots(void);
void SudekiMpResetLanArenaClientReplica(void);
BOOL SudekiMpLanArenaClientReplicaApplyLatest(void);

#endif
