#ifndef SUDEKIMP_LAN_STORY_SHOTS_H
#define SUDEKIMP_LAN_STORY_SHOTS_H
#include "hooks/lan_story_observer.h"
#include "network/lan_story_frame.h"
#include "network/lan_story_handoff.h"
#include "network/lan_story_vfx.h"
/* Host only. Caller already proved the supported executable identity. This
 * observer never submits combat input, spends charge or creates a missile. */
BOOL SudekiMpLanStoryShotsInstall(HMODULE image);
/* Fresh game-thread roster + successfully captured generation namespace.
 * Publishes only native emissions witnessed under that same namespace. */
BOOL SudekiMpLanStoryShotsCapture(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,
    SudekiMpLanStoryFrame *frame);
/* Called only under the runtime's exact remote control lease. NULL clears
 * admission; the emission callback independently checks scene/actor/age. */
BOOL SudekiMpLanStoryShotsAim(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,unsigned character,
    const SudekiMpLanStoryRanged *input,uint32_t receipt);
BOOL SudekiMpLanStoryShotsAppendVisuals(const SudekiMpLanStoryNativeRoster *roster,SudekiMpStoryVfx *state);
void SudekiMpLanStoryShotsCloseAdmission(void);
BOOL SudekiMpLanStoryShotsUninstall(void);
#endif
