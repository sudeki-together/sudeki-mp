#ifndef SUDEKIMP_LAN_ARENA_HOST_INPUT_H
#define SUDEKIMP_LAN_ARENA_HOST_INPUT_H

#include <windows.h>

/* Read-only observation of Tal's already-authoritative native weak attack.
 * The hook never consumes or changes controller state; it only emits a
 * process-local edge for the next host snapshot. */
BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module);
void SudekiMpUninstallLanArenaHostInput(void);
void SudekiMpLanArenaHostInputServiceCombatToggle(void);
BOOL SudekiMpLanArenaHostInputTakeTalWeakAttack(void);

#endif
