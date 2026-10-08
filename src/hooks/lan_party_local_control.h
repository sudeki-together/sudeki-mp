#ifndef SUDEKIMP_LAN_PARTY_LOCAL_CONTROL_H
#define SUDEKIMP_LAN_PARTY_LOCAL_CONTROL_H

#include "hooks/lan_party_control.h"
#include "hooks/lan_story_observer.h"

/* Owns temporary AI execution for this process's controller-target actor.
 * The AI lease never changes native actor/controller/camera identities.
 * The coordinator must positively prove its persistent local input fence and
 * native action/task drain. FALSE/unknown closes admission. These callbacks
 * run synchronously inside the exact control-update dispatch witness. */
BOOL SudekiMpLanPartyLocalControlInstall(HMODULE image,
    SudekiMpLanPartyControlDrainProbe input_fenced,
    SudekiMpLanPartyControlDrainProbe actions_drained);
/* Explicit sparse saved-story route. Mutually exclusive with the SMP4
 * installation above; verifies the same native AI-mode function, but does
 * not acquire or use a party-switch adapter. The caller must independently
 * own a persistent host input fence and positive native action/task drain.
 * No runtime caller may replace those proofs with avatar selection alone. */
BOOL SudekiMpLanPartyLocalControlInstallStory(HMODULE image,
    SudekiMpLanPartyControlDrainProbe input_fenced,
    SudekiMpLanPartyControlDrainProbe actions_drained);
/* Fresh roster from this exact story-observer dispatch. key->seat is the
 * native saved leader's canonical hero index, not the avatar/player index.
 * Binds epoch/world/descriptor/group/controller/actor/AI/mode/directory.
 * Native mode changes neither override references nor party/controller/
 * camera identity; the caller keeps its input fence until restore succeeds. */
BOOL SudekiMpLanPartyLocalControlStorySetAi(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *assignment,const SudekiMpLanStoryNativeRoster *roster,
    BOOL enabled);
BOOL SudekiMpLanPartyLocalControlStoryAiExact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *assignment,const SudekiMpLanStoryNativeRoster *roster);
BOOL SudekiMpLanPartyLocalControlSetAi(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *assignment,void *actor,BOOL enabled);
BOOL SudekiMpLanPartyLocalControlAiExact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *assignment,void *actor);
BOOL SudekiMpLanPartyLocalControlRetains(void);
/* A native rotation was entered and has not yet been positively rebound.
 * Retry SwitchStep with its retained assignment, without fresh old-roster
 * admission. This is independent of the temporary local AI lease above. */
BOOL SudekiMpLanPartyLocalControlSwitchPending(void);
/* One ordinary native party rotation toward an approved character. Requires
 * all actor leases/tasks drained, local AI returned, and the owned persistent
 * None input fence. Native validators/camera/HUD/listeners still run. A
 * coherent result rebinds only the control adapter's physical identity and
 * reports it; root retains closed admission through any intermediate actor
 * and commits session/cast/replica ownership separately. */
BOOL SudekiMpLanPartyLocalControlSwitchStep(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *assignment,unsigned target_character,
    unsigned *observed_character);
/* No implicit restore or task cancellation; call SetAi(FALSE) on the game
 * thread and wait for positive restoration before removing the input fence. */
BOOL SudekiMpLanPartyLocalControlUninstall(void);

#endif
