#ifndef SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H
#define SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H

#include <windows.h>

/* LAN-only sibling page on the shipped Esc screen.  The Multiplayer option
 * replaces Quit To Title in the closed cleanroom profile and opens a mod-owned
 * foreground page.  Back and Exit to Windows remain native; Q/QuickMenu is
 * not part of this adapter. */
BOOL SudekiMpInstallLanArenaPausePanel(HMODULE game_module);
void SudekiMpUninstallLanArenaPausePanel(void);

#endif
