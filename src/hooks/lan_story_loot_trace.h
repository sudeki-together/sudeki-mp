#ifndef SUDEKIMP_LAN_STORY_LOOT_TRACE_H
#define SUDEKIMP_LAN_STORY_LOOT_TRACE_H
#include "hooks/lan_story_observer.h"

/* Passive, saved-story HOST diagnostic owner. Native calls run exactly once;
 * it does not admit interaction, change actors, award items or retire objects.
 * The journal links the ACTUAL nested actor event -> break event -> drop setup
 * and observes setup completion/destruction separately. This is not account
 * authority: native pointers and sequence IDs are process-local diagnostics. */
BOOL SudekiMpLanStoryLootTraceInstall(HMODULE image);
BOOL SudekiMpLanStoryLootTraceBind(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *);
/* Quiescent native thread only. Pending setups retain observers until their
 * native destructor has positively returned. Failure preserves dependencies. */
BOOL SudekiMpLanStoryLootTraceUninstall(void);
#endif
