#ifndef SUDEKIMP_LAN_STORY_ACTIVITY_H
#define SUDEKIMP_LAN_STORY_ACTIVITY_H
#include "hooks/lan_story_observer.h"

/* Authoritative saved-story activity for resident, unarmed native NPCs.
 * mask contains only characters with current authenticated native control
 * leases. This changes neither the front actor nor the story cluster manager.
 * Service(0) must drain before actor/world/observer teardown. */
BOOL SudekiMpLanStoryActivityInitialize(HMODULE image);
BOOL SudekiMpLanStoryActivityService(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,unsigned remote_character_mask);
/* Native world load pending: release every lease without a roster (#42). */
BOOL SudekiMpLanStoryActivityReleaseForLoad(const SudekiMpControlUpdateDispatchWitness *);
/* Native thread inside a whole-world zone call, before its original runs. */
BOOL SudekiMpLanStoryActivityReleaseBeforeWorldChange(void);
BOOL SudekiMpLanStoryActivityRetains(void);
BOOL SudekiMpLanStoryActivityNativeExitReturned(void);
BOOL SudekiMpLanStoryActivityUninstall(void);
#endif
