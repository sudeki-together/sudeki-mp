#ifndef SUDEKIMP_STORY_DOOR_LINK_H
#define SUDEKIMP_STORY_DOOR_LINK_H

#include <windows.h>

/* Mod-defined doors between a main world and one of its temporary interiors,
 * driven by the game's own CWorld::EnterTemporaryZone / ExitTemporaryZone
 * (loading transition included). Configured from [DoorLink] in SudekiMP.ini;
 * requires the flight module's controlled-character seam. */
BOOL SudekiMpInstallStoryDoorLink(HMODULE game_module, const wchar_t *config_path);
BOOL SudekiMpUninstallStoryDoorLink(void);

#endif
