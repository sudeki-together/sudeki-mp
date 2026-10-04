#ifndef SUDEKIMP_LOBBY_GAMEPLAY_H
#define SUDEKIMP_LOBBY_GAMEPLAY_H
#include "loader/lobby_launch.h"
#include "network/title_lobby.h"
#include "ui/save_catalog.h"
/* Same-process title handoff. All native operations stay on the title thread.
 * Preparation reserves transport; the native menu owns fade, loading and spawn. */
BOOL SudekiMpInstallLobbyGameplay(HMODULE);
BOOL SudekiMpUninstallLobbyGameplay(void);
BOOL SudekiMpLobbyGameplayPrepare(const SudekiMpLobbyLaunchPlan *);
/* Separate same-process story preparation. Launcher IPC stays Test Room-only;
 * the selected native catalog entry is identified by both reviewed hashes. */
BOOL SudekiMpLobbyGameplayPrepareSaved(const SudekiMpLobbyLaunchPlan *,
    const SudekiMpLobbySavedGame *);
/* Composition enables this only when the complete saved-story runtime has
 * been installed. The default is closed, including diagnostic profiles. */
BOOL SudekiMpLobbyGameplayEnableSaved(BOOL enabled);
BOOL SudekiMpLobbyGameplaySavedAvailable(void);
/* Startup composition may reserve incompatible native seams for the explicit
 * saved-story profile. Ordinary entry defaults to Test Room enabled. */
BOOL SudekiMpLobbyGameplayEnableTestroom(BOOL enabled);
BOOL SudekiMpLobbyGameplayTestroomAvailable(void);
BOOL SudekiMpLobbyGameplaySavedGame(void);
void SudekiMpLobbyGameplayService(SudekiMpLobby *);
unsigned SudekiMpLobbyGameplayPoll(unsigned *port);
BOOL SudekiMpLobbyGameplayMatches(uint32_t revision,uint64_t generation);
BOOL SudekiMpLobbyGameplayActive(void);
BOOL SudekiMpLobbyGameplayStarted(void);
void SudekiMpLobbyGameplayRequestStart(void);
BOOL SudekiMpLobbyGameplayNeedsTitle(void);
BOOL SudekiMpLobbyGameplayArmTitle(void *owner);
/* FALSE retains native dependencies for a later game-thread drain. */
BOOL SudekiMpLobbyGameplayCancel(void);
/* Saved-story terminal UI transaction only. Runtime first retires render and
 * actor leases, then balances its pause with containment hooks still present.
 * 0 = native quit not entered; 1 = native quit returned with the exact world
 * reset/frontend-entry postcondition; 2 = entered but postcondition unknown.
 * Once entered, repeated calls never submit another native quit. Status is a
 * read-only outcome for this prepared session; it does not initiate cleanup. */
unsigned SudekiMpLobbyGameplayStoryExit(void);
unsigned SudekiMpLobbyGameplayStoryExitStatus(void);
BOOL SudekiMpLobbyGameplayRunning(void); /* plain atomic worker gate */
void SudekiMpLobbyGameplayLoaded(void); /* positively observed native roster */
/* Runtime invokes only after native load tasks and local host/replica
 * readiness are proven; the synchronous file reader alone is insufficient. */
void SudekiMpLobbyGameplayStoryLoaded(void);
#endif
