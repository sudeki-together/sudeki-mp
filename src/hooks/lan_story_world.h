#ifndef SUDEKIMP_LAN_STORY_WORLD_H
#define SUDEKIMP_LAN_STORY_WORLD_H

#include "hooks/lan_story_replica.h"
#include "network/lan_party_session.h"
#include "network/lan_story_world_frame.h"

/* One host or contained client per module lifetime. No hooks, entity creation,
 * AI updates or native task calls are installed here. Missing animation
 * resources may be acquired through the exact synchronous native loader. */
BOOL SudekiMpInitializeLanStoryWorld(HMODULE image);
/* Optional Dev Play profile-19 identity route, set before first catalog and
 * immutable until WorldUninstall. For each exact Ally native target, resolver
 * must freshly prove the current world/epoch/player/spawn generation/actor
 * through AvatarSpawnObserve. FALSE is unknown and refuses the whole batch.
 * No fallback to name or registry order. No native calls/writes in resolver.
 * Callback/context lifetime must outlive world adapter use. NULL selects the
 * existing native-resource identity route before that lifetime starts. */
typedef BOOL (*SudekiMpLanStoryWorldAvatarResolver)(void *context,
    const SudekiMpLanStoryNativeRoster *roster,void *actor,unsigned *player,uint32_t *spawn_generation);
BOOL SudekiMpLanStoryWorldSetAvatarResolver(SudekiMpLanStoryWorldAvatarResolver resolver,void *context);
/* Read only, on the exact post-controller observer dispatch. Every record is
 * an already present NPCEntity, generic scenery entity or exact canonical party member. Canonical PCs
 * use a portable PC category rather than native lazy-resolution kind bits.
 * Four channels must be uniform across submodels; the ranged fifth channel
 * and its fourth blend must be positively inactive on both peers. Generic
 * scenery has one channel whose selector zero is a real authored clip.
 * The complete batch shares the already captured party frame's identity. */
/* Host only, before capture: native zone data of an occupied split-area TEMP
 * (or NULL). Entities spawned by that zone's authored catalog are left out
 * of the exterior world frame. Copy-only identities; recomputed per call. */
void SudekiMpLanStoryWorldSetExcludedZone(const void *zone_data);
/* Host only. TRUE freezes the exterior catalog as the (kind,identifier) set of
 * the last successful capture; while frozen, other registry entities (an
 * occupied split-area TEMP's objects) are left out. FALSE releases it. */
void SudekiMpLanStoryWorldSetSplitAllowlist(BOOL frozen);
/* Host split animation pass (copy-only): the last captured exterior catalog
 * plus party characters while the split allowlist is frozen. Pointers are
 * re-validated by the caller before any native use. */
typedef struct SudekiMpLanStoryWorldAnimateTarget {
    void *object,*renderer; uint32_t identifier; uint16_t kind; uint8_t character;
} SudekiMpLanStoryWorldAnimateTarget;
unsigned SudekiMpLanStoryWorldSplitAnimateTargets(SudekiMpLanStoryWorldAnimateTarget *out,unsigned max);
/* Renderers owned by native registry entities (entity CPosition render
 * wrapper, plus party-character model banks). Copy-only pointers for the
 * ambient adapter's exclusion list; re-validated by nobody, compared only. */
unsigned SudekiMpLanStoryWorldOwnedRenderers(void **out,unsigned max);
/* Dev Play mirrored monster (configuration thread, before the runtime runs).
 * The one monster-class entity whose ResourceName equals resource (first per
 * identifier) joins the catalog as SUDEKIMP_LAN_STORY_WORLD_ENEMY_KIND: the
 * host captures it, the client presents its own copy. Client enemy targets
 * without a host record, and host records without a client copy, are left out
 * of the matched set; unmatched client copies are hidden. NULL/empty = off. */
void SudekiMpLanStoryWorldSetMirrorEnemy(const char *resource);
/* Client: party characters in a different host area keep their last pose. */
void SudekiMpLanStoryWorldSetForeignCharacters(uint8_t mask);
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
/* Render-only aim witness; called inside a fresh contained client pose scope.
 * Reads the last complete body observation and revalidates its native bank,
 * actor and resource residency. Does not load resources or mutate any object. */
BOOL SudekiMpLanStoryWorldAimDirection(const SudekiMpLanStoryNativeRoster *roster,
    void *actor,SudekiMpLanStoryReplicaExact exact,void *context,float direction[3]);
/* No mod-owned native reference is held; native renderer destruction releases
 * normal animation dependencies. Retires plain observations only, outside the
 * capture/apply stack and on the native thread after the runtime stops use. */
BOOL SudekiMpUninstallLanStoryWorld(void);

#endif
