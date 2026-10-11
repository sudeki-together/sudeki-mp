#ifndef SUDEKIMP_LAN_STORY_EFFECTS_H
#define SUDEKIMP_LAN_STORY_EFFECTS_H

#include "hooks/lan_story_replica.h"

/* Sole owner of the exact main particle-manager CALL 5D4915 -> 62FAA0.
 * Install at the runtime's quiescent native boundary. The dispatcher runs
 * synchronously at that call; it must establish a fresh ClientEffectsPresent
 * transaction before calling Advance. No previous render lease is retained. */
typedef void (*SudekiMpLanStoryEffectsDispatch)(void *context);
BOOL SudekiMpLanStoryEffectsInstall(HMODULE image,
    SudekiMpLanStoryEffectsDispatch dispatch,void *context);
/* Fresh native seam witness, valid only during this bridge's dispatcher on
 * its bound thread, with exact hook, current main-world manager and zero
 * incoming simulation delta. Remains true after the one original call so
 * ClientEffectsPresent can perform its complete post-operation check. */
BOOL SudekiMpLanStoryEffectsWitness(void *unused);
/* Cosmetic local clock only. Caller proves these host identity/freshness
 * fields belong to the last successfully applied complete presentation and
 * still match the authenticated current lease and remote/native scene.
 * This routine freshly validates all manager callback topology. It forwards
 * the original manager once in-place with bounded local render delta; it
 * never changes engine time, runs AI/GEL or duplicates a native update.
 * A FALSE result after original entry does not authorize a second call.
 * Neither host tick nor local time is accumulated across a stale/gap/reset.
 * model_clocks is a freshly collected native-model ownership list from this
 * same seam, consumed synchronously after manager membership validation. It
 * excludes only the supplemental custom clock, never native particle aging. */
BOOL SudekiMpLanStoryEffectsAdvance(const SudekiMpLanStoryNativeRoster *roster,
    uint32_t epoch,uint32_t revision,uint32_t sequence,uint32_t host_tick,
    uint32_t receipt_tick,SudekiMpLanStoryReplicaExact exact,void *context,
    void *const *model_clocks,unsigned model_clock_count);
/* Same native thread. Retires plain clock observations only; no native object
 * is released. Use whenever current authenticated presentation is rejected. */
void SudekiMpLanStoryEffectsResetClock(void);
/* Outside dispatcher/native callbacks, on its bound native thread. Failed
 * restoration retains this module's code and owner state. */
BOOL SudekiMpLanStoryEffectsUninstall(void);

#endif
