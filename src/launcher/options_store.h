#ifndef SUDEKIMP_OPTIONS_STORE_H
#define SUDEKIMP_OPTIONS_STORE_H

#include <windows.h>

#include "player_options.h"

typedef enum SudekiMpOptionsSource {
    SUDEKIMP_OPTIONS_SOURCE_NONE = 0,
    /* %APPDATA%\Sudeki\PlayerOptions.xml written by the game's launcher. */
    SUDEKIMP_OPTIONS_SOURCE_PLAYER = 1,
    /* PlayerOptions.xml is absent; the game's launcherdata defaults are loaded
       and re-encoded as UTF-16LE so Save creates the file the game expects. */
    SUDEKIMP_OPTIONS_SOURCE_DEFAULTS = 2
} SudekiMpOptionsSource;

BOOL SudekiMpOptionsStorePlayerPath(WCHAR *path, size_t path_count);
/* Loads PlayerOptions.xml, or the game's defaults when it does not exist.
   A present but unreadable/malformed file is a failure, never replaced. */
SudekiMpOptionsSource SudekiMpOptionsStoreLoad(const WCHAR *game_directory,
                                               SudekiMpPlayerOptions *options);
BOOL SudekiMpOptionsStoreLoadDefaults(const WCHAR *game_directory,
                                      SudekiMpPlayerOptions *options);
/* Writes a temporary file, keeps a one-time backup of the original, replaces
   the file, then reads it back and compares bytes. */
BOOL SudekiMpOptionsStoreSave(const SudekiMpPlayerOptions *options);

#endif
