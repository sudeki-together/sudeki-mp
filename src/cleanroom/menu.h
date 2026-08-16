#ifndef SUDEKIMP_CLEANROOM_MENU_H
#define SUDEKIMP_CLEANROOM_MENU_H

#include <windows.h>

BOOL SudekiMpInstallCleanroomMenu(HMODULE game_module, UINT toggle_key);
BOOL SudekiMpInstallIntegratedCleanroomMenu(
    HMODULE game_module,
    UINT toggle_key
);
void SudekiMpCleanroomMenuUpdate(void);
void SudekiMpCleanroomMenuRender(void);
void SudekiMpUninstallCleanroomMenu(void);

#endif
