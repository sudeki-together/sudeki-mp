#ifndef SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H
#define SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H

#include <windows.h>

/* LAN-only sibling page on the shipped Esc screen.  Multiplayer is appended
 * after every native row.  The native pause transaction is released while
 * the sibling page is open so authenticated peers and snapshots keep moving;
 * local gameplay adapters use Active() to suppress only this process's input. */
BOOL SudekiMpInstallLanArenaPausePanel(HMODULE game_module);
BOOL SudekiMpLanArenaPausePanelActive(void);
/* Returns FALSE if any native callsite could not be restored. In that case
 * every callback/resource lease remains owned and a later call may retry. */
BOOL SudekiMpUninstallLanArenaPausePanel(void);

enum {
    SUDEKIMP_LAN_ARENA_PAUSE_PANEL_RESTORE_NAVIGATE = 1u,
    SUDEKIMP_LAN_ARENA_PAUSE_PANEL_RESTORE_ANALOG_NAVIGATE = 2u,
    SUDEKIMP_LAN_ARENA_PAUSE_PANEL_RESTORE_BACK = 3u,
    SUDEKIMP_LAN_ARENA_PAUSE_PANEL_RESTORE_SELECT = 4u,
    SUDEKIMP_LAN_ARENA_PAUSE_PANEL_RESTORE_RENDER = 5u
};
#if defined(SUDEKIMP_LAN_ARENA_PAUSE_PANEL_TESTING)
void SudekiMpLanArenaPausePanelInjectRestoreFailureForTest(
    unsigned int restore_id);
#endif

#endif
