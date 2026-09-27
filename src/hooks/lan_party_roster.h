#ifndef SUDEKIMP_LAN_PARTY_ROSTER_H
#define SUDEKIMP_LAN_PARTY_ROSTER_H
#include "hooks/lan_party_control.h"

typedef enum SudekiMpLanPartyRosterStatus {
    SUDEKIMP_LAN_PARTY_ROSTER_WAITING = 0,
    SUDEKIMP_LAN_PARTY_ROSTER_READY,
    SUDEKIMP_LAN_PARTY_ROSTER_REPLACED
} SudekiMpLanPartyRosterStatus;
typedef struct SudekiMpLanPartyRoster {
    SudekiMpLanPartyRosterObservation bound;
    uint8_t initialized_mask, spawn_pending_mask;
    BOOL bound_valid, replaced;
} SudekiMpLanPartyRoster;

/* Zero-initialize once per host session. Existing engine-owned actors remain
 * in the party when peers leave. This is not a despawn/cancellation owner.
 * One outstanding native spawn at a time; no timer ever retries a submission.
 * Unknown observations stop startup, and replacement permanently closes this
 * session's startup admission. The host still drains existing control leases. */
SudekiMpLanPartyRosterStatus SudekiMpLanPartyRosterService(
    SudekiMpLanPartyRoster *roster,
    const SudekiMpControlUpdateDispatchWitness *witness);
#endif
