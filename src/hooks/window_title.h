#ifndef SUDEKIMP_WINDOW_TITLE_H
#define SUDEKIMP_WINDOW_TITLE_H
#include <windows.h>
/* [SudekiMP] WindowTitle=<text>: the game's main render window is created with
 * this title instead of "Sudeki" (exact call site only; every other window the
 * game creates passes through unchanged). Used by the launcher's local host +
 * client profile so the two windows can be told apart. Empty/absent = off. */
BOOL SudekiMpWindowTitleInstall(HMODULE game,const wchar_t *config_path);
BOOL SudekiMpWindowTitleUninstall(void);
#endif
