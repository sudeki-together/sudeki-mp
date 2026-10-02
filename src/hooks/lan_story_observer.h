#ifndef SUDEKIMP_LAN_STORY_OBSERVER_H
#define SUDEKIMP_LAN_STORY_OBSERVER_H

#include "hooks/control_separation.h"
#include "network/lan_party_story.h"

/* Observation-only owner of the seven exact zone entry hooks. Mutually
 * exclusive with zone_transition_trace: both verify unmodified entry bytes.
 * No spawn, native task, actor lease, input or camera mutation. */
BOOL SudekiMpLanStoryObserverInstall(HMODULE module);
BOOL SudekiMpLanStoryObserverSample(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpLanStoryScene *scene);
/* After a native callback has occurred, remove on that same native thread,
 * outside the synchronous zone calls. Failed restoration retains all state. */
BOOL SudekiMpLanStoryObserverUninstall(void);

#endif
