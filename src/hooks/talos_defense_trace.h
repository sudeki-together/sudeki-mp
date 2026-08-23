#ifndef SUDEKIMP_TALOS_DEFENSE_TRACE_H
#define SUDEKIMP_TALOS_DEFENSE_TRACE_H

#include <windows.h>

/* Observation-only trace for the exact GOG Talos damage/knockback path. */
BOOL SudekiMpInstallTalosDefenseTrace(HMODULE game_module);
void SudekiMpUninstallTalosDefenseTrace(void);

#endif
