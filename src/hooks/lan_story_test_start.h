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
/* Optional health-only test start: use the configured fractions once after
 * load readiness, without placement or zone/TEMP operations. Clears any
 * previous start configuration; no valid fraction leaves the aid unarmed.
 * Startup-only, before service begins; used when TestStartLeader is absent. */
BOOL SudekiMpLanStoryTestStartConfigureHealthOnly(HMODULE image);
/* Optional [StoryAreas] TestStartZone: native zone (descriptor name) made
 * current before placing, via the area follower's native calls. */
void SudekiMpLanStoryTestStartSetZone(const char *zone);
/* Optional test-only split start. First place the party in TestStartZone;
 * then enter this native TEMP with its authored arrival marker and place only
 * the leader at the interior coordinates after the split has settled.
 * Missing/invalid configuration disables this extra step. */
BOOL SudekiMpLanStoryTestStartSetTemporary(const char *zone,const char *marker,
    const float leader[3]);
/* Optional [StoryAreas] TestStartLeaderHealth = fraction (0 < f < 1): after
 * placement, or at the unchanged save position in health-only mode, the host
 * leader's HP is lowered to that fraction of its current value through the
 * cleanroom resource setter, so item/heal gates can be played from a fresh
 * load. Test aid only; ignored outside (0,1). */
void SudekiMpLanStoryTestStartSetLeaderHealth(float fraction);
/* Same for every other available party member ([StoryAreas] TestStartFollowerHealth). */
void SudekiMpLanStoryTestStartSetFollowerHealth(float fraction);
/* Native game thread, from the host runtime service with a proved roster. */
void SudekiMpLanStoryTestStartService(const SudekiMpLanStoryNativeRoster *roster);
/* Research (H1b): place non-leaders in a background-resident zone. */
void SudekiMpLanStoryTestStartSetResearchPlace(const char *zone,const float xyz[3]);
void SudekiMpLanStoryTestStartResearchService(const SudekiMpLanStoryNativeRoster *roster);

#endif
