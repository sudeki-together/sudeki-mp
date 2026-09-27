#ifndef SUDEKIMP_LAN_PARTY_RUNTIME_H
#define SUDEKIMP_LAN_PARTY_RUNTIME_H
#include "network/lan_party_session.h"
/* Private fixed-four movement validation runtime. Not the gameplay milestone.
 * Caller initializes the exact cleanroom/weapon/skill adapters and installs
 * SERVICE_ONLY control separation. Mutations remain on its borrowed seam. */
BOOL SudekiMpInstallLanPartyRuntime(HMODULE,const SudekiMpLanPartyConfig *);
/* Retry while native cleanup is pending. Never unload retained dependencies. */
BOOL SudekiMpUninstallLanPartyRuntime(void);
#endif
