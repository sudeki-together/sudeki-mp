#ifndef SUDEKIMP_LAN_STORY_RUNTIME_H
#define SUDEKIMP_LAN_STORY_RUNTIME_H
#include "network/lan_party_session.h"
#include "network/title_lobby.h"

/* Private story profiles: 1 observes sparse native scenes; 2 loads the pinned
 * saved world and contains client simulation for noncombat playback. Native
 * party control and swaps require independent leases and fresh view ACKs;
 * creation is limited to the separately validated Lighthouse recruitment. */
BOOL SudekiMpInstallLanStoryRuntime(HMODULE module,const SudekiMpLanPartyConfig *config);
BOOL SudekiMpUninstallLanStoryRuntime(void);
/* Lobby-owned terminal UI transaction: prepare restoration while paused,
 * retire observers/transport, balance pause and synchronously exit the world,
 * then remove native input/menu/trigger fences. May need render preparation. */
BOOL SudekiMpLanStoryRuntimeExitToTitle(void);
unsigned SudekiMpLanStoryRuntimePort(void);
/* Positive native saved-load and presentation readiness, not connectivity.
 * Reserved but unavailable characters do not become input-enabled here. */
BOOL SudekiMpLanStoryRuntimeReady(void);
void SudekiMpLanStoryRuntimeLobbyService(SudekiMpLobby *lobby);
/* Lobby player name holding character 0..3 (Buki, Elco, Tal, Ailish) from the
 * last lobby status copy; FALSE when the character is not held by a present
 * member with a non-empty name. Runtime thread only; copy-only. */
BOOL SudekiMpLanStoryRuntimePlayerName(unsigned character,char out[32]);
#endif
