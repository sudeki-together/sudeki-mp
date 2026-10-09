#ifndef SUDEKIMP_LAN_STORY_AREA_STATE_H
#define SUDEKIMP_LAN_STORY_AREA_STATE_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

/* Host-authoritative native area residency (#42): resident world zone
 * descriptors by authored name, the current zone and the background-load
 * target. A state report, not an event: the newest one replaces the last. */
#define SUDEKIMP_STORY_AREA_NAME 40u
#define SUDEKIMP_STORY_AREA_MAX 8u
#define SUDEKIMP_STORY_AREA_WIRE_SIZE (4u+2u*SUDEKIMP_STORY_AREA_NAME+1u+SUDEKIMP_STORY_AREA_MAX*(SUDEKIMP_STORY_AREA_NAME+1u))

typedef struct SudekiMpStoryAreaEntry {
    char name[SUDEKIMP_STORY_AREA_NAME];
    uint8_t state;
} SudekiMpStoryAreaEntry;

typedef struct SudekiMpStoryAreaState {
    uint32_t observed_tick;
    char current[SUDEKIMP_STORY_AREA_NAME];
    char target[SUDEKIMP_STORY_AREA_NAME];
    uint8_t count;
    SudekiMpStoryAreaEntry entries[SUDEKIMP_STORY_AREA_MAX];
} SudekiMpStoryAreaState;

BOOL SudekiMpStoryAreaStateValid(const SudekiMpStoryAreaState *state);
/* Same residency (ignores observed_tick). */
BOOL SudekiMpStoryAreaStateSame(const SudekiMpStoryAreaState *a,const SudekiMpStoryAreaState *b);
BOOL SudekiMpStoryAreaStateEncode(const SudekiMpStoryAreaState *state,uint8_t *bytes,size_t size);
BOOL SudekiMpStoryAreaStateDecode(const uint8_t *bytes,size_t size,SudekiMpStoryAreaState *state);

#endif
