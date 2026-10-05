#ifndef SUDEKIMP_LAN_STORY_OBJECTS_H
#define SUDEKIMP_LAN_STORY_OBJECTS_H

#include "engine/story_placement.h"
#include "hooks/lan_story_observer.h"

typedef struct SudekiMpLanStoryObjectSnapshot {
    uint32_t count;
    SudekiMpStoryPlacement placements[SUDEKIMP_STORY_PLACEMENT_MAX];
    /* Diagnostics only. Absence is not proof of a break or collected reward. */
    uint8_t present[SUDEKIMP_STORY_PLACEMENT_MAX],native_phase[SUDEKIMP_STORY_PLACEMENT_MAX];
} SudekiMpLanStoryObjectSnapshot;
/* READ ONLY: authored breakable spawn records in all resident zones on the
 * verified post-controller game-thread dispatch. No native calls, hooks,
 * reference retention, input, collision or renderer writes. No pointers leave
 * this function. Rejected observations leave the output untouched.
 *
 * Authored names/transforms remain available after the entity disappears.
 * This alone does NOT prove an INITIAL area catalog: the runtime must capture
 * at its saved-load readiness boundary, before admitting mod interactions,
 * freeze that set, and establish an identical peer baseline. It must not call
 * this again after a break to manufacture a new baseline/source numbering. */
BOOL SudekiMpLanStoryObjectsObserve(HMODULE image,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *,
    SudekiMpLanStoryObjectSnapshot *);

#endif
