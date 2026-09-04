#ifndef SUDEKIMP_LAN_ARENA_STARTUP_MOVIE_SKIP_H
#define SUDEKIMP_LAN_ARENA_STARTUP_MOVIE_SKIP_H

#include <windows.h>

/* LAN-only startup presentation adapter. It skips exactly the three known
 * publisher/logo movies and forwards every other MoviePlay call unchanged.
 * The ordinary cleanroom/roster movie policy remains owned by menu.c. */
BOOL SudekiMpInstallLanArenaStartupMovieSkip(HMODULE game_module);
BOOL SudekiMpUninstallLanArenaStartupMovieSkip(void);
BOOL SudekiMpLanArenaStartupMovieSkipInstalled(void);

/* Exact supported-image preflight for the MoviePlay entry used by the hook
 * transaction. The LAN launcher initializes the DLL before resuming Sudeki's
 * main thread, so no active native movie is stopped speculatively. */
BOOL SudekiMpLanArenaStartupMovieSkipImageMatches(HMODULE game_module);

/* Safe exact-name policy query. Invalid or unreadable pointers are rejected. */
BOOL SudekiMpLanArenaStartupMovieShouldSkip(const char *movie_name);

#endif
