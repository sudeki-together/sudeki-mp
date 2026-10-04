#ifndef SUDEKIMP_LAN_PARTY_RUNTIME_H
#define SUDEKIMP_LAN_PARTY_RUNTIME_H
#include "network/lan_party_session.h"
#include "network/title_lobby.h"
#include "hooks/control_separation.h"
/* Private fixed-four movement validation runtime. Not the gameplay milestone.
 * Caller initializes the exact cleanroom/weapon/skill adapters and installs
 * SERVICE_ONLY control separation. Mutations remain on its borrowed seam. */
BOOL SudekiMpInstallLanPartyRuntime(HMODULE,const SudekiMpLanPartyConfig *);
/* Retry while native cleanup is pending. Never unload retained dependencies. */
BOOL SudekiMpUninstallLanPartyRuntime(void);
unsigned SudekiMpLanPartyRuntimePort(void);
/* Same UI/game thread; advances plain lobby admission, never native actors. */
void SudekiMpLanPartyRuntimeLobbyService(SudekiMpLobby *);
BOOL SudekiMpLanPartyRuntimeHostLocalInput(
    const SudekiMpControlUpdateDispatchWitness *,unsigned character,
    uint32_t now,SudekiMpLanArenaInput *);
#endif
