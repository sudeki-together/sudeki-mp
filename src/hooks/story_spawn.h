#ifndef SUDEKIMP_STORY_SPAWN_H
#define SUDEKIMP_STORY_SPAWN_H

#include <windows.h>

/* Research spawn key: pressing the configured key spawns the configured
 * definition (e.g. "BOSS_Mystril") at a fixed position or at the controlled
 * character, through the native generic entity spawn. [Spawn] in SudekiMP.ini;
 * requires the flight module's controlled-character seam. */
BOOL SudekiMpInstallStorySpawn(HMODULE game_module, const wchar_t *config_path);
BOOL SudekiMpUninstallStorySpawn(void);

#endif
