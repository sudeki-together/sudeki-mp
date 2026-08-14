#ifndef SUDEKIMP_PLAYER_INPUT_TRACE_H
#define SUDEKIMP_PLAYER_INPUT_TRACE_H

#include <windows.h>

BOOL SudekiMpInstallPlayerInputTrace(HMODULE game_module);
void SudekiMpUninstallPlayerInputTrace(void);

#endif
