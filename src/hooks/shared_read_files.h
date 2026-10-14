#ifndef SUDEKIMP_SHARED_READ_FILES_H
#define SUDEKIMP_SHARED_READ_FILES_H
#include <windows.h>
/* Second game instance on one PC ([TitleMenu] AllowSecondInstance): the game
 * opens data files (its sound banks among them) read-only with share mode 0,
 * so whichever instance opens a file first locks every other instance out of
 * it. Through the game's own CreateFileA import, a read-only OPEN_EXISTING
 * open with share mode 0 also allows shared reading. Writes, other access
 * masks and other dispositions pass through unchanged. */
BOOL SudekiMpSharedReadFilesInstall(HMODULE game);
BOOL SudekiMpSharedReadFilesUninstall(void);
#endif
