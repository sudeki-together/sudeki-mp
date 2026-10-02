#ifndef SUDEKIMP_TITLE_MULTIPLAYER_H
#define SUDEKIMP_TITLE_MULTIPLAYER_H
#include <windows.h>

/* Independent front-end only: no fixed-seat actor or transport is selected.
 * Create/Join remain disabled until a session owner is wired to this page. */
BOOL SudekiMpInstallTitleMultiplayer(HMODULE module);
/* Checked, retryable teardown. Busy render callbacks retain dependencies. */
BOOL SudekiMpUninstallTitleMultiplayer(void);
#endif
