#ifndef SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H
#define SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H

#include <windows.h>

/* LAN-only sibling page on the shipped Esc screen.  Multiplayer is appended
 * after every native row.  The native pause transaction is released while
 * the sibling page is open so authenticated peers and snapshots keep moving;
 * local gameplay adapters use Active() to suppress only this process's input. */
BOOL SudekiMpInstallLanArenaPausePanel(HMODULE game_module);
BOOL SudekiMpLanArenaPausePanelActive(void);
void SudekiMpUninstallLanArenaPausePanel(void);

#endif
