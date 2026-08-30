#ifndef SUDEKIMP_XINPUT_PLAYER_TWO_H
#define SUDEKIMP_XINPUT_PLAYER_TWO_H

#include <windows.h>
#include <stdint.h>

#define SUDEKIMP_XINPUT_GET_STATE_IAT_RVA 0x0029a268u
#define SUDEKIMP_XINPUT_GET_STATE_IMPORT_NAME_RVA 0x0030b220u

/* Reserves one or more XInput users for mod-owned local seats.  Bit N hides
 * XInput user N from Sudeki's native polling while the local input hub keeps
 * reading it directly.  The supported game still receives every unreserved
 * user normally.  Installation is exact-build and IAT-gated. */
BOOL SudekiMpInstallXInputReservationMask(HMODULE game_module,
                                          uint8_t slot_mask);

/* Compatibility wrapper for the existing single-controller P2 profile. */
BOOL SudekiMpInstallXInputPlayerTwoReservation(HMODULE game_module,
                                                unsigned int slot);
void SudekiMpUninstallXInputPlayerTwoReservation(void);

#endif
