#ifndef SUDEKIMP_LAN_STORY_XP_VISUAL_H
#define SUDEKIMP_LAN_STORY_XP_VISUAL_H
#include "network/lan_story_xp.h"
BOOL SudekiMpLanStoryXpInitialize(HMODULE image,BOOL host);
BOOL SudekiMpLanStoryXpCapture(uint32_t epoch,uint32_t revision,uint32_t tick,
    const char current[40],SudekiMpStoryXp *out);
BOOL SudekiMpLanStoryXpPresent(const SudekiMpStoryXp *,uint32_t now,uint32_t received_age);
void SudekiMpLanStoryXpReset(void);
#endif
