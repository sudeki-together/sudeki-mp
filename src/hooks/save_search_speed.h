#ifndef SUDEKIMP_SAVE_SEARCH_SPEED_H
#define SUDEKIMP_SAVE_SEARCH_SPEED_H

#include <windows.h>

/* In-game save book (update FUN_004810a0, vtable slot VA 0x6ca8a4), state 5
 * "Finding saves...": the page first waits page+0x394 (copied from +0x390 on
 * entry, VA 0x483d9d) and then calls the native catalog step FUN_00500e30
 * once every 0.1 s (call at VA 0x481246). Each step counts two slot folders or
 * reads one slot entry, so 13 slots took about 2.2 s after a 2.6 s wait.
 *
 * Two exact-byte seams owned here:
 *  - the entry wait load reads `wait_seconds` instead of +0x390;
 *  - the step call repeats the native step on the game thread until the
 *    catalog reports done (bounded), so the whole scan finishes in one tick.
 * The catalog code, its order, sorting and the page's states are native. */
BOOL SudekiMpSaveSearchSpeedInstall(HMODULE game_module, float wait_seconds);
BOOL SudekiMpSaveSearchSpeedUninstall(void);

#endif
