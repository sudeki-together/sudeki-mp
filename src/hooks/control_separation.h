#ifndef SUDEKIMP_CONTROL_SEPARATION_H
#define SUDEKIMP_CONTROL_SEPARATION_H

#include <windows.h>

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT virtual_key,
    BOOL enable_second_player_movement
);
void SudekiMpUninstallControlSeparation(void);

#endif
