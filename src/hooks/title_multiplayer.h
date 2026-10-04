#ifndef SUDEKIMP_TITLE_MULTIPLAYER_H
#define SUDEKIMP_TITLE_MULTIPLAYER_H
#include <windows.h>
#include "ui/save_catalog.h"

/* Independent front-end only: no fixed-seat actor or transport is selected.
 * Create/Join remain disabled until a session owner is wired to this page. */
BOOL SudekiMpInstallTitleMultiplayer(HMODULE module);
/* Private exact-save load observation. Queue before the first native title
 * update; the verified title thread owns catalog pinning and native dispatch.
 * This does not enable lobby saved-game Start or multiplayer story control. */
BOOL SudekiMpTitleMultiplayerQueueSavedLoadProbe(const SudekiMpSaveFingerprint *);
/* Checked, retryable teardown. Busy render callbacks retain dependencies. */
BOOL SudekiMpUninstallTitleMultiplayer(void);
#endif
