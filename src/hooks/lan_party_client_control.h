#ifndef SUDEKIMP_LAN_PARTY_CLIENT_CONTROL_H
#define SUDEKIMP_LAN_PARTY_CLIENT_CONTROL_H
#include "hooks/lan_party_roster.h"

typedef struct SudekiMpLanPartyClientControl SudekiMpLanPartyClientControl;
typedef struct SudekiMpLanPartyClientControlReport {
    SudekiMpLanPartyLease lease;
    SudekiMpLanPartyRosterStatus roster_status;
    uint8_t owned_mask, draining_mask, failed_mask;
    BOOL ready;
} SudekiMpLanPartyClientControlReport;

/* One selected local native controller; three presentation-only
 * AI leases. The client never calls ControlMove or admits authoritative input.
 * The owning runtime must install input/damage containment before servicing
 * this coordinator. No callbacks or game pointers are touched by Create. */
SudekiMpLanPartyClientControl *SudekiMpLanPartyClientControlCreate(
    SudekiMpLanPartySession *session, unsigned int local_seat,
    SudekiMpLanPartyControlDrainProbe drained);
BOOL SudekiMpLanPartyClientControlService(SudekiMpLanPartyClientControl *,
    const SudekiMpControlUpdateDispatchWitness *, SudekiMpLanPartyClientControlReport *);
void SudekiMpLanPartyClientControlRequestStop(SudekiMpLanPartyClientControl *);
/* A character handoff drains native presentation bindings without disconnecting
 * the ACTIVE transport. Service while suspended until BindingsDrained is TRUE.
 * Resume only after the native controller transaction and this coordinator's
 * exact rebind have committed. Away alone does not suspend replica playback. */
void SudekiMpLanPartyClientControlSuspendBindings(SudekiMpLanPartyClientControl *,BOOL);
BOOL SudekiMpLanPartyClientControlBindingsDrained(const SudekiMpLanPartyClientControl *);
BOOL SudekiMpLanPartyClientControlSetLocalCharacter(SudekiMpLanPartyClientControl *,
    const SudekiMpControlUpdateDispatchWitness *,unsigned character);
/* A transport loss during a native switch must not strand the physical view
 * on an actor this coordinator still excludes incorrectly. Requires suspended
 * and positively drained bindings, a disconnected transport, exact unchanged
 * roster, new physical controller, owned input fence and idle Cast namespaces.
 * Updates presentation identity only; retains retiring and the old lease. The
 * runtime may then clear suspension and allow its ordinary explicit F7 retry.
 * This neither acknowledges an assignment nor grants input/transport control. */
BOOL SudekiMpLanPartyClientControlFinishDisconnectedSwitch(SudekiMpLanPartyClientControl *,
    const SudekiMpControlUpdateDispatchWitness *,unsigned character);
/* Explicit retry on the verified game thread. The runtime must first restore
 * presentation/combat state; every native AI lease must have drained. */
BOOL SudekiMpLanPartyClientControlRejoin(SudekiMpLanPartyClientControl *,
    const SudekiMpControlUpdateDispatchWitness *);
/* Disable/unregister/drain the owner callback before destruction. Failed
 * native release retains this object/session and the control-hook dependency. */
BOOL SudekiMpLanPartyClientControlDestroy(SudekiMpLanPartyClientControl *);
#endif
