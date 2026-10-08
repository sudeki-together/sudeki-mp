#ifndef SUDEKIMP_TALOS_SEAT_PROBE_H
#define SUDEKIMP_TALOS_SEAT_PROBE_H
#include <windows.h>
/* Research probe ([TalosSeatProbe] Enabled=true, single machine): drives a
 * spawned ALLY_TALOS entity through the same native entries the mod uses for
 * player heroes. Numpad 8/2/4/6 = arbiter movement request (world axes),
 * F8 weak, F9 strong, F11 sweep = combat-input submission on his arbiter.
 * Runs on the game thread from the flight controlled-character observer. */
BOOL SudekiMpTalosSeatProbeInstall(HMODULE game_module);
BOOL SudekiMpTalosSeatProbeUninstall(void);
#endif
