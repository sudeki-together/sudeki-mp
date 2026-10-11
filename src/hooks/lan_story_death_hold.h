#ifndef SUDEKIMP_LAN_STORY_DEATH_HOLD_H
#define SUDEKIMP_LAN_STORY_DEATH_HOLD_H
#include <windows.h>
/* Co-op story (#54): a dead player's character keeps the native lead slot
 * while a remote player is connected, so control never moves to the host
 * from a dead player; the dead player spectates their own character.
 * Replaces only the death path's rotation call (FUN_00424220 -> 423F60). */
BOOL SudekiMpLanStoryDeathHoldInstall(HMODULE image);
BOOL SudekiMpLanStoryDeathHoldUninstall(void);
/* Host service: TRUE while at least one remote seat drives a party member. */
void SudekiMpLanStoryDeathHoldSetActive(BOOL active);
/* Host service: actors driven by connected clients (NULL = none). A scripted
 * lead change to one of them is held while active. */
void SudekiMpLanStoryDeathHoldSetRemoteActors(void *const actors[3]);
/* Game-thread service seam: log holds recorded inside the native callback. */
void SudekiMpLanStoryDeathHoldReport(void);
#endif
