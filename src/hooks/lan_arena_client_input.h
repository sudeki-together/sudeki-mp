#ifndef SUDEKIMP_LAN_ARENA_CLIENT_INPUT_H
#define SUDEKIMP_LAN_ARENA_CLIENT_INPUT_H

#include <windows.h>

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module);
void SudekiMpUninstallLanArenaClientInput(void);
/* Called once from the post-controller game-thread observer. A held native
 * direction is refreshed at a bounded cadence; missing controller samples
 * become an explicit neutral packet before the host's safety timeout. */
void SudekiMpLanArenaClientInputService(void);

#endif
