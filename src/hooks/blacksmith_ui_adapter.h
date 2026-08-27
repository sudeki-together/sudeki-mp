#ifndef SUDEKIMP_BLACKSMITH_UI_ADAPTER_H
#define SUDEKIMP_BLACKSMITH_UI_ADAPTER_H

#include "engine/blacksmith_ui_session.h"

#include <windows.h>

/* Disabled-by-default exact-build experiment. When every runtime prerequisite
 * is valid, UIBlackSmithStart is satisfied by two native-inert seat shadows
 * and the native global blacksmith mode is not queued. All other calls tail
 * through Sudeki's original export unchanged. */
BOOL SudekiMpInstallBlacksmithUiAdapter(HMODULE game_module, BOOL enabled);
void SudekiMpBlacksmithUiAdapterService(void);
BOOL SudekiMpBlacksmithUiAdapterActive(void);
BOOL SudekiMpBlacksmithUiAdapterGetSnapshot(
    SudekiMpBlacksmithUiSnapshot *snapshot
);
void SudekiMpBlacksmithUiAdapterReportOverlay(BOOL visible);
void SudekiMpUninstallBlacksmithUiAdapter(void);

#if defined(SUDEKIMP_BLACKSMITH_UI_ADAPTER_TESTING)
/* Deterministically exercises rollback after Start is hooked but before the
 * paired Active hook is installed. Never compiled into the production DLL. */
void SudekiMpBlacksmithUiAdapterInjectSecondHookFailureForTest(BOOL enabled);
#endif

#endif
