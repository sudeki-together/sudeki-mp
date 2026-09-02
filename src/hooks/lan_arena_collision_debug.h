#ifndef SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_H
#define SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_H

#include <windows.h>

enum {
    SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF = 0u,
    SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES = 1u,
    SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES_AND_MESH = 2u
};

/* Exact-image, LAN-only presentation diagnostic.  Installation captures the
 * three native debug bytes and establishes an OFF baseline.  The caller must
 * invoke ServiceHotkey from one stable game/render thread; it accepts one F9
 * rising edge only while this Sudeki process owns the foreground window. */
BOOL SudekiMpInstallLanArenaCollisionDebug(HMODULE game_module);
void SudekiMpLanArenaCollisionDebugServiceHotkey(void);
unsigned int SudekiMpLanArenaCollisionDebugMode(void);
void SudekiMpUninstallLanArenaCollisionDebug(void);

#if defined(SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_TESTING)
BOOL SudekiMpLanArenaCollisionDebugAdvanceForTesting(void);
BOOL SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
    BOOL foreground,
    BOOL key_down,
    BOOL *was_down
);
#endif

#endif
