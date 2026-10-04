#ifndef SUDEKIMP_STORY_CINEMATIC_VIEW_H
#define SUDEKIMP_STORY_CINEMATIC_VIEW_H
#include "network/lan_story_presentation.h"

/* Existing measured title atlas/stateblock renderer. Caller supplies only a
 * fresh authenticated presentation on its owned native render callback.
 * FALSE/ERROR_NOT_SUPPORTED explicitly reports unavailable glyph/layout;
 * no truncation, guessed speaker, native dialogue action or UI state write. */
BOOL SudekiMpStoryCinematicViewRender(void *device,
    const SudekiMpLanStoryPresentation *presentation);
#endif
