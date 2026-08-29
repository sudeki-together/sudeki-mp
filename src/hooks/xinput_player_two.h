#ifndef SUDEKIMP_XINPUT_PLAYER_TWO_H
#define SUDEKIMP_XINPUT_PLAYER_TWO_H

#include <windows.h>

#define SUDEKIMP_XINPUT_GET_STATE_IAT_RVA 0x0029a268u
#define SUDEKIMP_XINPUT_GET_STATE_IMPORT_NAME_RVA 0x0030b220u

/* Reserves exactly one connected XInput user for the mod-owned Player 2
 * source. The supported game still receives all other XInput users normally.
 * Installation is exact-build and IAT-gated; any mismatch leaves the game
 * untouched and makes the co-op launch fail closed. */
BOOL SudekiMpInstallXInputPlayerTwoReservation(HMODULE game_module,
                                                unsigned int slot);
void SudekiMpUninstallXInputPlayerTwoReservation(void);

#endif
