#ifndef SUDEKIMP_LAN_ARENA_CAMPAIGN_GUARD_H
#define SUDEKIMP_LAN_ARENA_CAMPAIGN_GUARD_H

#include <windows.h>

/* LAN arena processes are ephemeral cleanroom terminals.  These exact hooks
 * suppress the native save-book entry and final slot operation so neither
 * host nor client can read or write campaign state while this profile lives.
 * They also suppress native Previous/Next rotation: v1 roles remain Tal host
 * and Ailish client for the complete authenticated session. */
BOOL SudekiMpInstallLanArenaCampaignGuard(HMODULE game_module);
void SudekiMpUninstallLanArenaCampaignGuard(void);

#endif
