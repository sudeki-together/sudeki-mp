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
/* FALSE means a native task, actor lease, or hook restoration is still
 * pending. Callers must retain every downstream dependency and retry from a
 * game-thread boundary; treating it as uninstalled is unsafe. */
BOOL SudekiMpUninstallLanArenaRuntime(void);
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
/* Retained native presentation ownership, not authority to start a cast.
 * Also remains true during disconnect drain until native namespaces retire. */
BOOL SudekiMpLanArenaClientPrivateCastCamerasOwned(void);
/* Closed Buki/Elco ordinary-skill experiment. This reports complete native
 * ownership plumbing, not permission to Use or to overlap Spirit strikes.
 * Activation still revalidates the specific actor/session and native state. */
BOOL SudekiMpLanArenaOrdinarySkillOverlapOwned(void);
/* Called only after replica sequence/previous-task handoff admission. */
BOOL SudekiMpLanArenaApplyClientSkillTiming(unsigned int seat,
    const SudekiMpLanArenaActorSnapshot *snapshot);
/* Existing host/client camera detours share this router; no second hook.
 * 0 legacy path, 1 handled remote, 2 positively local/neutral, -1 unknown. */
int SudekiMpLanArenaRouteCastCamera(void *manager,const char *name);
/* Wrap-safe admission used by the host's held Ailish fire route.  The
 * non-front world-combat fallback retires much faster than Ailish's authored
 * first-person weapon cycle, so a separate cadence gate is required. */
BOOL SudekiMpLanArenaRangedRepeatReady(
    uint32_t now_ms,
    uint32_t not_before_ms
);
uint32_t SudekiMpLanArenaRangedRepeatIntervalMs(uint16_t encoded_half_seconds);

#endif
