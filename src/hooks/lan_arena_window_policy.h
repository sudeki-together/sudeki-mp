#ifndef SUDEKIMP_LAN_ARENA_WINDOW_POLICY_H
#define SUDEKIMP_LAN_ARENA_WINDOW_POLICY_H

#include <windows.h>

/* Sudeki's exact WM_KILLFOCUS handler calls ShowWindow(SW_MINIMIZE), and its
 * WM_ACTIVATE and WM_ACTIVATEAPP handlers clear the byte that gates the main
 * update/render loop.
 * Focus loss also deactivates every registered graphics device. LAN processes
 * stay visible, continue simulation, and keep presenting in the background
 * while physical keyboard/mouse and DirectInput focus remain native. */
BOOL SudekiMpInstallLanArenaWindowPolicy(HMODULE game_module);
BOOL SudekiMpUninstallLanArenaWindowPolicy(void);
BOOL SudekiMpLanArenaWindowPolicyInstalled(void);

/* Focus changes release physical input, but must not end an actor-owned cast's
 * authored direct-movement mode. This is not an action or input admission gate. */
BOOL SudekiMpLanArenaWindowPreserveCastMovementPolicy(
    BOOL owner_exact, BOOL skill_active, int movement_mode, float direct_speed);

#endif
