#ifndef SUDEKIMP_LAN_STORY_XP_H
#define SUDEKIMP_LAN_STORY_XP_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

/* Optional visual snapshot of the native 24-slot XP popup pool. No reward
 * deltas or authority to kill, grant experience, or generate loot. */
#define SUDEKIMP_STORY_XP_SLOTS 24u
#define SUDEKIMP_STORY_XP_SIZE (60u+32u*SUDEKIMP_STORY_XP_SLOTS)
typedef struct SudekiMpStoryXpPopup {
    uint32_t serial,amount;
    float position[3],height,elapsed,duration;
} SudekiMpStoryXpPopup;
typedef struct SudekiMpStoryXp {
    uint32_t epoch,revision,tick,sequence,stream;
    char current[40];
    SudekiMpStoryXpPopup slots[SUDEKIMP_STORY_XP_SLOTS];
} SudekiMpStoryXp;
BOOL SudekiMpStoryXpValid(const SudekiMpStoryXp *);
BOOL SudekiMpStoryXpEncode(const SudekiMpStoryXp *,uint8_t *,size_t,size_t *);
BOOL SudekiMpStoryXpDecode(const uint8_t *,size_t,SudekiMpStoryXp *);
#endif
