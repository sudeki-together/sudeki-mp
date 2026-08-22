#ifndef SUDEKIMP_CHARACTER_SWITCH_TRACE_H
#define SUDEKIMP_CHARACTER_SWITCH_TRACE_H

#include <windows.h>

BOOL SudekiMpInstallCharacterSwitchTrace(
    HMODULE game_module,
    BOOL enable_talos_party_prototype
);
void SudekiMpUninstallCharacterSwitchTrace(void);

#endif
