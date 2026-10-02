#ifndef SUDEKIMP_LOBBY_GAMEPLAY_H
#define SUDEKIMP_LOBBY_GAMEPLAY_H
#include "loader/lobby_launch.h"
/* Same-process title handoff. All native operations stay on the title thread.
 * Preparation reserves transport; the native menu owns fade, loading and spawn. */
BOOL SudekiMpInstallLobbyGameplay(HMODULE);
BOOL SudekiMpUninstallLobbyGameplay(void);
BOOL SudekiMpLobbyGameplayPrepare(const SudekiMpLobbyLaunchPlan *);
unsigned SudekiMpLobbyGameplayPoll(unsigned *port);
BOOL SudekiMpLobbyGameplayMatches(uint32_t revision,uint64_t generation);
BOOL SudekiMpLobbyGameplayActive(void);
BOOL SudekiMpLobbyGameplayStarted(void);
void SudekiMpLobbyGameplayRequestStart(void);
BOOL SudekiMpLobbyGameplayNeedsTitle(void);
BOOL SudekiMpLobbyGameplayArmTitle(void *owner);
/* FALSE retains native dependencies for a later game-thread drain. */
BOOL SudekiMpLobbyGameplayCancel(void);
BOOL SudekiMpLobbyGameplayRunning(void); /* plain atomic worker gate */
void SudekiMpLobbyGameplayLoaded(void); /* positively observed native roster */
#endif
