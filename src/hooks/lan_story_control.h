#ifndef SUDEKIMP_LAN_STORY_CONTROL_H
#define SUDEKIMP_LAN_STORY_CONTROL_H

#include "hooks/lan_story_observer.h"
#include "network/lan_party_session.h"

/* HOST saved-story scope, deliberately separate from the four-member Test
 * Room adapter. This adapter admits only validated loaded party members, ordinary
 * movement, Tal's native melee input, and exact independent native AI leases.
 * The caller owns authenticated offer/ACK/freshness. None of these functions
 * authorizes a network player or creates a missing native character.
 *
 * Install closes only the controller's two Q/E rotation callsites. The native
 * Prev/Next entries remain pristine for the witnessed host-restoration path.
 * Every actor operation requires a fresh roster from the current exact
 * StoryObserver post-controller dispatch. A lease key's seat is CHARACTER;
 * its generation is native ownership, independent of a connection key. */
BOOL SudekiMpLanStoryControlInstall(HMODULE image,unsigned locked_character);
BOOL SudekiMpLanStoryControlBegin(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *);
BOOL SudekiMpLanStoryControlNextLease(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanPartyLease *connection,unsigned character,
    SudekiMpLanPartyLease *native_key);
BOOL SudekiMpLanStoryControlAcquire(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key);
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key);
BOOL SudekiMpLanStoryControlMove(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key,
    float world_x,float world_z,BOOL *temporarily_held);
unsigned SudekiMpLanStoryControlMelee(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,unsigned kind);
/* Closes input first, then retries native speed/action drain and precisely
 * one DefaultControl. TRUE proves ref0/mode1; a failure retains ownership.
 * Never release a transport ticket, replace the scene, or destroy the actor
 * merely because the socket disconnected. */
BOOL SudekiMpLanStoryControlDrain(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *native_key);
/* Native world load pending (scene LOADING, no roster): release-only drain. */
BOOL SudekiMpLanStoryControlLoadDrain(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanPartyLease *);
/* Native thread, before a whole-world zone call runs: release only. */
BOOL SudekiMpLanStoryControlReleaseBeforeWorldChange(const SudekiMpLanPartyLease *);
/* Positive native binding observation for HostControlReady. A draining
 * ref1/mode0 actor is still owned, but is never eligible for new Move calls. */
BOOL SudekiMpLanStoryControlActorOwned(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanStoryNativeRoster *,unsigned character);
BOOL SudekiMpLanStoryControlRetainsKey(const SudekiMpLanPartyLease *key);
BOOL SudekiMpLanStoryControlRetains(void);
/* Ends the empty native scope and restores both Q/E callsites. Must precede
 * StoryObserver/ControlSeparation teardown. Failures retain dependencies. */
BOOL SudekiMpLanStoryControlUninstall(void);

#endif
