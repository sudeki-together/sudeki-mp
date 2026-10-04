#ifndef SUDEKIMP_LAN_STORY_WORLD_H
#define SUDEKIMP_LAN_STORY_WORLD_H

#include "hooks/lan_story_replica.h"
#include "network/lan_party_session.h"
#include "network/lan_story_world_frame.h"

/* One host or contained client per module lifetime. No hooks, entity creation,
 * AI updates or native task calls are installed here. Missing animation
 * resources may be acquired through the exact synchronous native loader. */
BOOL SudekiMpInitializeLanStoryWorld(HMODULE image);
/* Read only, on the exact post-controller observer dispatch. Every record is
 * an already present NPCEntity, generic scenery entity or exact canonical party member. Canonical PCs
 * use a portable PC category rather than native lazy-resolution kind bits.
 * Four channels must be uniform across submodels; the ranged fifth channel
 * and its fourth blend must be positively inactive on both peers. Generic
 * scenery has one channel whose selector zero is a real authored clip.
 * The complete batch shares the already captured party frame's identity. */
BOOL SudekiMpLanStoryWorldCapture(SudekiMpLanPartySession *session,
    void *controller, const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene, const SudekiMpLanStoryFrame *party,
    SudekiMpLanStoryWorldFrame *frame);
/* Called only inside ClientPresent after authenticated complete-batch and
 * local/remote scene matching. The callback must retain the whole paused
 * registry/input/trigger/scheduler proof. Preflights every target before any
 * pose write, then rechecks its exact owner around each native setter. No wire
 * selector is trusted: portable clip handle/occurrence pairs resolve uniquely
 * within the fingerprinted ordered loaded bank, including authored aliases.
 * First binding persists for the module lifetime. A different scene, entity
 * identity set, generation or native owner requires an explicit native drain
 * and new module lifetime, not merely a newer packet epoch. */
BOOL SudekiMpLanStoryWorldApply(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame, SudekiMpLanStoryReplicaExact exact,
    void *context);
/* Synchronous split for composing camera and native entity presentation at
 * admission: Prepare validates all NPC and external PC poses, then the caller
 * applies remaining party state, then ApplyPrepared publishes native poses. Pass the identical
 * arguments within the same ClientPresent callback. If player playback fails,
 * CancelPrepared retires these borrowed observations. Missing animation data
 * may trigger one verified synchronous native resource load; Prepare then
 * returns FALSE/ERROR_IO_PENDING before any pose write. The next presentation
 * attempt must repeat full preflight with a still-fresh authenticated host frame;
 * no native observation from before the load is retained. Native renderer lifetime
 * owns acquired resource references. No yield or messages
 * may occur while prepared; successful Prepare keeps the module active.
 * independent_view hides the exact authored telescope lens overlay from a
 * companion's own camera; shared spectators follow host visibility. */
BOOL SudekiMpLanStoryWorldPrepare(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame, SudekiMpLanStoryReplicaExact exact,
    void *context,BOOL independent_view);
BOOL SudekiMpLanStoryWorldApplyPrepared(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame, SudekiMpLanStoryReplicaExact exact,
    void *context);
BOOL SudekiMpLanStoryWorldCancelPrepared(void);
/* Explicit native Lighthouse transition. Retire the one exact NPC borrow
 * before native deletion; preserve every other bound renderer. Finish only
 * after native recruitment and containment enrollment, adopting exactly the
 * newly created canonical Ailish and preserving every survivor, using a fresh complete
 * authenticated post-recruitment frame. No generic epoch reset is exposed. */
BOOL SudekiMpLanStoryWorldBeginRecruitment(const SudekiMpLanStoryNativeRoster *roster,
    void *npc,uint32_t remote_before_epoch,SudekiMpLanStoryReplicaExact exact,void *context);
BOOL SudekiMpLanStoryWorldFinishRecruitment(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryWorldFrame *frame,uint32_t remote_after_epoch,
    SudekiMpLanStoryReplicaExact exact,void *context);
BOOL SudekiMpLanStoryWorldRecruiting(void);
/* No mod-owned native reference is held; native renderer destruction releases
 * normal animation dependencies. Retires plain observations only, outside the
 * capture/apply stack and on the native thread after the runtime stops use. */
BOOL SudekiMpUninstallLanStoryWorld(void);

#endif
