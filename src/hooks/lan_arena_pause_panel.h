#ifndef SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H
#define SUDEKIMP_LAN_ARENA_PAUSE_PANEL_H

#include <windows.h>

/* LAN-only augmentation of the shipped Esc screen. The native pause renderer
 * remains the owner; this adapter adds status text and a local End/Leave
 * command without installing any split-screen or custom QuickMenu hooks. */
BOOL SudekiMpInstallLanArenaPausePanel(HMODULE game_module);
void SudekiMpUninstallLanArenaPausePanel(void);

#endif
