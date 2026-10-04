#ifndef SUDEKIMP_LAN_ARENA_CAMPAIGN_GUARD_H
#define SUDEKIMP_LAN_ARENA_CAMPAIGN_GUARD_H

#include <windows.h>

/* LAN arena processes are ephemeral cleanroom terminals.  These exact hooks
 * suppress the native save-book entry and final slot operation so neither
 * host nor client can read or write campaign state while this profile lives.
 * They also suppress native Previous/Next rotation: v1 roles remain Tal host
 * and Ailish client for the complete authenticated session. */
BOOL SudekiMpInstallLanArenaCampaignGuard(HMODULE game_module);
/* The verified multiplayer game-thread coordinator may invoke one native
 * switch through this patch owner after closing input and draining actor
 * leases. The public Previous/Next RET patches stay installed throughout.
 * Caller owns full image/function, group/controller and eligibility proof;
 * TRUE means invoked, not that the native validator accepted the rotation. */
BOOL SudekiMpLanArenaCampaignGuardSwitchCharacter(void *group,BOOL previous);
/* FALSE means at least one exact patch or detour still owns its native seam.
 * The guard pins its module and retains all callback/base state so teardown can
 * be retried without leaving a live jump into unloaded code. */
BOOL SudekiMpUninstallLanArenaCampaignGuard(void);

#endif
