#ifndef SUDEKIMP_LAN_STORY_CATCHUP_H
#define SUDEKIMP_LAN_STORY_CATCHUP_H
#include "network/lan_story_handoff.h"
#include "network/lan_story_world_frame.h"

/* Historical bootstrap data, deliberately separate from the fresh playback
 * queues. It cannot authorize movement, renew frame age, or select native
 * resources. The ticket/connection authenticates delivery; the matching save
 * and native containment still validate every actor and bank on application. */
typedef struct SudekiMpLanStoryCatchup {
    uint32_t transaction;
    SudekiMpLanStoryScene target,seed_scene;
    SudekiMpLanStoryRecruitment recruitment; /* zero for a directly loaded matching party */
    SudekiMpLanStoryFrame party;
    SudekiMpLanStoryWorldFrame world;
} SudekiMpLanStoryCatchup;
#define SUDEKIMP_STORY_CATCHUP_MAX_SIZE 11000u
#define SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE 1024u
#define SUDEKIMP_STORY_CATCHUP_MAX_FRAGMENTS 11u
#define SUDEKIMP_STORY_CATCHUP_HEADER_SIZE 16u
BOOL SudekiMpLanStoryCatchupValid(const SudekiMpLanStoryCatchup *snapshot);
BOOL SudekiMpLanStoryCatchupMatches(const SudekiMpLanStoryCatchup *snapshot,
    const SudekiMpLanStoryScene *current);
BOOL SudekiMpLanStoryCatchupEncode(const SudekiMpLanStoryCatchup *snapshot,
    uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpLanStoryCatchupDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryCatchup *snapshot);
#endif
