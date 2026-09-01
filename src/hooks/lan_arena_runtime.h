#ifndef SUDEKIMP_LAN_ARENA_RUNTIME_H
#define SUDEKIMP_LAN_ARENA_RUNTIME_H

#include "network/lan_arena_session.h"

#include <windows.h>

/* Owns only the game-thread network pump. Actor control, replica application,
 * and the pause panel are intentionally separate adapters so a failed network
 * setup can never leave a partial native-control lease behind. */
BOOL SudekiMpInstallLanArenaRuntime(
    HMODULE game_module,
    const SudekiMpLanArenaSessionConfig *config
);
void SudekiMpUninstallLanArenaRuntime(void);
BOOL SudekiMpLanArenaRuntimeInstalled(void);
/* Pause-panel/UI adapters call this only on the game thread. It sends one END
 * packet when connected, stops client ingress immediately, and leaves both
 * processes in their cleanroom baseline. */
BOOL SudekiMpLanArenaRuntimeEndSession(void);
/* Replaces only the client's direct IPv4 endpoint and starts a fresh strict
 * handshake. No previous token or partially connected replica survives. */
BOOL SudekiMpLanArenaRuntimeJoinAddress(const char *remote_ipv4);
BOOL SudekiMpLanArenaRuntimeJoinEndpoint(const char *endpoint);
BOOL SudekiMpLanArenaRuntimeHostArena(void);
BOOL SudekiMpLanArenaRuntimeGetStatus(
    SudekiMpLanArenaSessionStatus *status
);

#endif
