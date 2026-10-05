#ifndef SUDEKIMP_STORY_LOOT_SIDECAR_H
#define SUDEKIMP_STORY_LOOT_SIDECAR_H
#include "engine/story_loot_codec.h"
/* Exact sidecar extension only. Native save paths are never accepted. The
 * game-thread coordinator must positively finish every native transaction
 * before taking this snapshot and binding it to a completed native save.
 * Missing file != corrupt/incompatible file; callers must distinguish them. */
typedef enum SudekiMpStoryLootFileResult {
    SUDEKIMP_STORY_LOOT_FILE_ERROR=0, SUDEKIMP_STORY_LOOT_FILE_OK,
    SUDEKIMP_STORY_LOOT_FILE_MISSING, SUDEKIMP_STORY_LOOT_FILE_INVALID
} SudekiMpStoryLootFileResult;
SudekiMpStoryLootFileResult SudekiMpStoryLootReadFile(const char *sidecar,
    const uint8_t expected_save[32],SudekiMpStoryLootState *);
SudekiMpStoryLootFileResult SudekiMpStoryLootWriteFile(const char *sidecar,
    const SudekiMpStoryLootState *);
#endif
