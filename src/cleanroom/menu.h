#ifndef SUDEKIMP_CLEANROOM_MENU_H
#define SUDEKIMP_CLEANROOM_MENU_H

#include <windows.h>

BOOL SudekiMpInstallCleanroomMenu(HMODULE game_module, UINT toggle_key);
BOOL SudekiMpInstallIntegratedCleanroomMenu(
    HMODULE game_module,
    UINT toggle_key
);
BOOL SudekiMpInstallZoneTraversalMenu(
    HMODULE game_module,
    UINT toggle_key,
    BOOL skip_startup_movies
);
BOOL SudekiMpInstallCoopRosterMenu(
    HMODULE game_module,
    UINT toggle_key,
    BOOL integrated_multiplayer,
    BOOL skip_startup_movies,
    BOOL story_test_boost_enabled,
    UINT story_test_boost_key,
    float story_test_boost_multiplier
);
void SudekiMpCleanroomMenuUpdate(void);
void SudekiMpCleanroomMenuRender(void);
/* Read-only bridge for code that already depends on the cleanroom presenter;
 * the save-book interceptor owns exact native opening/closed observation. */
BOOL SudekiMpCleanroomNativeSaveModalActive(void);
void SudekiMpUninstallCleanroomMenu(void);

#endif
