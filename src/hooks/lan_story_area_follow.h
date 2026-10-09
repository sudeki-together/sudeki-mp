#ifndef SUDEKIMP_LAN_STORY_AREA_FOLLOW_H
#define SUDEKIMP_LAN_STORY_AREA_FOLLOW_H

#include <windows.h>
#include <stdint.h>

/* Host-authoritative native area residency (#42). The host publishes which
 * world zone descriptors it has resident (state 1..4), which one is current
 * and which one is the background-load target (world+0x394). The client
 * steers its own native world toward that state with the game's exported
 * zone calls: SwitchZoneNOW (background load of the target, which also
 * unloads the previous target) and EnterZone (marks a loaded zone as the
 * next current one; the native world update performs the swap).
 * Zones are identified by their authored descriptor names (desc+0x24). */
#include "network/lan_story_area_state.h"

BOOL SudekiMpLanStoryAreaFollowInitialize(HMODULE image);
/* Native game thread. Reads the current native residency; FALSE when the
 * world/descriptor table is not exactly readable. */
BOOL SudekiMpLanStoryAreaCapture(SudekiMpStoryAreaState *out);
/* Client, native game thread, once per dispatch with the newest host state.
 * Issues at most one native zone call per call, rate limited. */
void SudekiMpLanStoryAreaFollow(const SudekiMpStoryAreaState *host,uint32_t now);
/* TRUE while a follow call is inside a native zone export (the observer lets
 * these through when client zone decisions are held). */
BOOL SudekiMpLanStoryAreaFollowCalling(void);
/* GetTickCount of the last native zone call this follower issued (0 = none). */
uint32_t SudekiMpLanStoryAreaFollowLastCall(void);

#endif
