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

/* One local native controller (Elco, Tal OR Ailish); three presentation-only
 * AI leases. The client never calls ControlMove or admits authoritative input.
 * The owning runtime must install input/damage containment before servicing
 * this coordinator. No callbacks or game pointers are touched by Create. */
SudekiMpLanPartyClientControl *SudekiMpLanPartyClientControlCreate(
    SudekiMpLanPartySession *session, unsigned int local_seat,
    SudekiMpLanPartyControlDrainProbe drained);
BOOL SudekiMpLanPartyClientControlService(SudekiMpLanPartyClientControl *,
    const SudekiMpControlUpdateDispatchWitness *, SudekiMpLanPartyClientControlReport *);
void SudekiMpLanPartyClientControlRequestStop(SudekiMpLanPartyClientControl *);
/* Disable/unregister/drain the owner callback before destruction. Failed
 * native release retains this object/session and the control-hook dependency. */
BOOL SudekiMpLanPartyClientControlDestroy(SudekiMpLanPartyClientControl *);
#endif
