#ifndef SUDEKIMP_LAN_STORY_CONTEXT_PROMPT_H
#define SUDEKIMP_LAN_STORY_CONTEXT_PROMPT_H
#include <windows.h>
/* Contained client only. The native HUD controller (global RVA 0x3c2fc4) keeps a
 * "context menu flags" word (+0x58); trigger sweeps raise/clear prompt bits
 * (hand icon + action text) and then run the HUD mode update (RVA 0xAE430).
 * The paused client holds trigger sweeps, so a prompt raised before the pause
 * (e.g. the save point the save was made at) never clears. This clears every
 * prompt bit except the enable bit and runs the native mode update once.
 * Game thread only; returns TRUE when a prompt was cleared. */
BOOL SudekiMpLanStoryContextPromptClear(HMODULE game_module);
#endif
