#ifndef SUDEKIMP_SPIRIT_STRIKE_INPUT_H
#define SUDEKIMP_SPIRIT_STRIKE_INPUT_H

#include <windows.h>

BOOL SudekiMpInstallSpiritStrikeInput(
    HMODULE game_module,
    int strike_id,
    unsigned int variant,
    UINT virtual_key
);
void SudekiMpUninstallSpiritStrikeInput(void);

#endif
