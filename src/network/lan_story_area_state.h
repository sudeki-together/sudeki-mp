#ifndef SUDEKIMP_LAN_STORY_AREA_STATE_H
#define SUDEKIMP_LAN_STORY_AREA_STATE_H
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#define SUDEKIMP_STORY_AREA_NAME 40u
#define SUDEKIMP_STORY_AREA_MAX 8u
#define SUDEKIMP_STORY_AREA_CODEC_VERSION 2u
#define SUDEKIMP_STORY_AREA_WIRE_SIZE (12u+2u*SUDEKIMP_STORY_AREA_NAME+1u+SUDEKIMP_STORY_AREA_MAX*(SUDEKIMP_STORY_AREA_NAME+1u)+2u*SUDEKIMP_STORY_AREA_NAME)
/* Only zone residency. Presentation has its own atomic, sequenced channel. */
typedef struct SudekiMpStoryAreaEntry {
    char name[SUDEKIMP_STORY_AREA_NAME];
    uint8_t state;
} SudekiMpStoryAreaEntry;
typedef struct SudekiMpStoryAreaState {
    uint32_t sequence,observed_tick;
    char current[SUDEKIMP_STORY_AREA_NAME],target[SUDEKIMP_STORY_AREA_NAME];
    uint8_t count;
    SudekiMpStoryAreaEntry entries[SUDEKIMP_STORY_AREA_MAX];
    /* H1b: the temporary zone the host's last native TEMP entry used and its
     * authored arrival marker (ResourceName text), so a client can enter the
     * same interior locally. Both empty when unknown. Not residency. */
    char temporary_zone[SUDEKIMP_STORY_AREA_NAME],temporary_marker[SUDEKIMP_STORY_AREA_NAME];
} SudekiMpStoryAreaState;
BOOL SudekiMpStoryAreaStateValid(const SudekiMpStoryAreaState *state);
/* Positive ordinary-zone evidence for SwitchZoneNOW/EnterZone following.
 * State 4 is a temporary interior: residency is not permission to move another
 * character there. Unknown/ambiguous current descriptors also grant no move. */
BOOL SudekiMpStoryAreaStateExterior(const SudekiMpStoryAreaState *state);
/* Ignores transport sequence and observation tick. */
BOOL SudekiMpStoryAreaStateSame(const SudekiMpStoryAreaState *a,const SudekiMpStoryAreaState *b);
BOOL SudekiMpStoryAreaStateEncode(const SudekiMpStoryAreaState *state,uint8_t *bytes,size_t size);
BOOL SudekiMpStoryAreaStateDecode(const uint8_t *bytes,size_t size,SudekiMpStoryAreaState *state);
#endif
