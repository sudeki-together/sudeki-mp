#ifndef SUDEKIMP_QUICK_MENU_H
#define SUDEKIMP_QUICK_MENU_H

#include <windows.h>
#include <stdint.h>

/*
 * Changes the confirmed Quick Menu activation instruction from requesting
 * CGameSpeed mode 1 to requesting mode 0. The caller must first validate the
 * supported executable and ensure activation_signature is a unique match.
 */
BOOL SudekiMpEnableQuickMenuNormalSpeed(
    const uint8_t *activation_signature
);

#endif
