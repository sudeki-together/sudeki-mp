#ifndef SUDEKIMP_LAN_STORY_TEST_START_H
#define SUDEKIMP_LAN_STORY_TEST_START_H

#include <windows.h>
#include "hooks/lan_story_observer.h"

/* Developer test start (host, saved story): once the saved game is loaded and
 * the native party is proved, place the leader and every other party member
 * at configured coordinates, exactly once per run. Uses the native position
 * setter the game's own SetPlayerPosition debug command uses (0x403050 on the
 * actor's position component). Configured by [StoryAreas] TestStartLeader /
 * TestStartFollower = x,y,z. Not a gameplay feature. */
BOOL SudekiMpLanStoryTestStartConfigure(HMODULE image, const float leader[3],
    const float follower[3]);
/* Optional [StoryAreas] TestStartZone: native zone (descriptor name) made
 * current before placing, via the area follower's native calls. */
void SudekiMpLanStoryTestStartSetZone(const char *zone);
/* Native game thread, from the host runtime service with a proved roster. */
void SudekiMpLanStoryTestStartService(const SudekiMpLanStoryNativeRoster *roster);

#endif
