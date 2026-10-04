#ifndef SUDEKIMP_LAN_PARTY_LOCAL_CONTROL_H
#define SUDEKIMP_LAN_PARTY_LOCAL_CONTROL_H

#include "hooks/lan_party_control.h"

/* Owns temporary AI execution for this process's controller-target actor.
 * The AI lease never changes native actor/controller/camera identities.
 * The coordinator must positively prove its persistent local input fence and
 * native action/task drain. FALSE/unknown closes admission. These callbacks
 * run synchronously inside the exact control-update dispatch witness. */
BOOL SudekiMpLanPartyLocalControlInstall(HMODULE image,
    SudekiMpLanPartyControlDrainProbe input_fenced,
    SudekiMpLanPartyControlDrainProbe actions_drained);
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
