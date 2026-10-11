#ifndef SUDEKIMP_LAN_STORY_COMBO_HUD_H
#define SUDEKIMP_LAN_STORY_COMBO_HUD_H
#include "hooks/lan_story_replica.h"
/* Native accepted-input UI observations only. No admission or simulation.
 * Host observers borrow the current roster on its established native thread;
 * the contained client publishes only the existing native HUD widget. */
BOOL SudekiMpLanStoryComboHudInitialize(HMODULE image,BOOL host);
uint16_t SudekiMpLanStoryComboHudCapture(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,unsigned character);
BOOL SudekiMpLanStoryComboHudPresent(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryFrame *frame,BOOL visible,
    SudekiMpLanStoryReplicaExact exact,void *context);
BOOL SudekiMpLanStoryComboHudRetains(void);
BOOL SudekiMpLanStoryComboHudRelease(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryReplicaExact exact,void *context);
BOOL SudekiMpLanStoryComboHudUninstall(void);
#endif
