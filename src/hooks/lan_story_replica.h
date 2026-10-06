#ifndef SUDEKIMP_LAN_STORY_REPLICA_H
#define SUDEKIMP_LAN_STORY_REPLICA_H

#include "hooks/lan_story_observer.h"
#include "network/lan_story_frame.h"

/* Fresh paused-render ownership proof supplied by the containment owner.
 * An old controller dispatch witness is never a render-time permission. */
typedef BOOL (*SudekiMpLanStoryReplicaExact)(
    const SudekiMpLanStoryNativeRoster *roster, void *context);

BOOL SudekiMpInitializeLanStoryReplica(HMODULE image);
/* Inside the client's contained native presentation scope, before world
 * preflight. Resolve the host item ID through this actor's owned inventory;
 * never grant an item, choose a starter, or repeat an uncertain activation.
 * Native equipment creation is separate from body pose publication. */
BOOL SudekiMpLanStoryReplicaPrepareEquipment(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryFrame *frame,SudekiMpLanStoryReplicaExact exact,void *context);
/* Caller proves authenticated freshness and matching native/remote world.
 * Both rosters must contain exactly the same canonical actors. Missing actors
 * are never spawned here. Writes position/facing and known world locomotion;
 * it submits no input, native task, combat, inventory or resource operation.
 * The optional host view changes only the already-owned render camera state.
 * TRUE is native setter/readback proof, NOT visible paused bone-update proof. */
/* Characters (bit=character) whose host area differs from the local player's.
 * Their poses are not applied; they keep their last same-area presentation. */
void SudekiMpLanStoryReplicaSetForeignCharacters(uint8_t mask);
BOOL SudekiMpLanStoryReplicaApply(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryFrame *frame, BOOL apply_host_view,SudekiMpLanStoryReplicaExact exact,
    void *context);
/* Restore the exact retained camera before releasing the client's pause.
 * Unknown or replaced owners retain the lease; failure is retryable. */
BOOL SudekiMpLanStoryReplicaRestoreView(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryReplicaExact exact, void *context);
BOOL SudekiMpLanStoryReplicaRetainsView(void);
/* Original local camera and original leader position captured together before
 * the first host view/pose publication. Transfer this plain seed before view
 * retirement; subtracting today's replicated Tal from an old camera is wrong. */
BOOL SudekiMpLanStoryReplicaViewSeed(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryReplicaExact exact,void *context,SudekiMpLanStoryView *view,float anchor[3]);
BOOL SudekiMpUninstallLanStoryReplica(void);

#endif
