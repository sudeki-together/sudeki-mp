#ifndef SUDEKIMP_LAN_PARTY_HOST_CONTROL_H
#define SUDEKIMP_LAN_PARTY_HOST_CONTROL_H
#include "hooks/lan_party_control.h"
#include "hooks/lan_party_roster.h"

typedef struct SudekiMpLanPartyHostControl SudekiMpLanPartyHostControl;
typedef struct SudekiMpLanPartyHostControlReport {
    uint8_t owned_mask, waiting_mask, draining_mask, failed_mask;
    uint8_t unsupported_input_mask;
    SudekiMpLanPartyRosterStatus roster_status;
    uint8_t roster_present_mask, roster_initialized_mask, roster_pending_mask;
} SudekiMpLanPartyHostControlReport;

#ifdef SUDEKIMP_LAN_PARTY_CONTROL_TESTING
typedef BOOL (*SudekiMpLanPartyHostTestCombatMode)(BOOL *enabled);
void SudekiMpLanPartyHostControlTestCombatMode(
    SudekiMpLanPartyHostTestCombatMode mode);
#endif

/* One coordinator per host session. The owning runtime supplies a positively
 * observed actor-local action/camera/effect drain probe and calls Service on
 * its existing control-update observer. Service prepares the exact Buki-led
 * testroom roster through asynchronous native spawns and actor-family weapon
 * setup before approving ANY peer. Cleanroom engine and skill/weapon ABIs must
 * already be initialized against the same supported image. The transport
 * worker only polls the session. Session and probe must outlive this
 * coordinator and its callbacks. No game objects are touched by Create.
 *
 * The basic route admits movement, actor-local weak attacks, and validated
 * native weapon changes. Other action-bearing packets are rejected without
 * ACK. Creation/service do not grant cast or skill authority. */
SudekiMpLanPartyHostControl *SudekiMpLanPartyHostControlCreate(
    SudekiMpLanPartySession *session, SudekiMpLanPartyControlDrainProbe drained);
BOOL SudekiMpLanPartyHostControlService(SudekiMpLanPartyHostControl *host,
    const SudekiMpControlUpdateDispatchWitness *witness, uint32_t now_ms,
    SudekiMpLanPartyHostControlReport *report);
/* Runtime entry, once per witnessed native update. Supply that update's
 * simulation delta (not wall time) for remote Elco's native rapid recharge.
 * Zero/invalid deltas skip recharge; the ordinary Service wrapper supplies
 * zero for callers that do not own a native update-data observation. */
BOOL SudekiMpLanPartyHostControlServiceFrame(SudekiMpLanPartyHostControl *host,
    const SudekiMpControlUpdateDispatchWitness *witness, uint32_t now_ms,
    float frame_delta_seconds, SudekiMpLanPartyHostControlReport *report);
/* Copy the newest host-timestamped input after the game-thread adapter
 * validated its exact actor lease. Used to snapshot aim and by native aim
 * callbacks; a NULL witness is allowed only on the verified game thread and
 * rechecks the retained native lease without opening mutation authority. */
BOOL SudekiMpLanPartyHostControlLatestInput(SudekiMpLanPartyHostControl *host,
    const SudekiMpControlUpdateDispatchWitness *witness,unsigned seat,
    uint32_t now_ms,SudekiMpLanArenaInput *input);
/* Plain-data admission closure; safe on the runtime's shutdown thread.
 * Continue game-thread Service calls until native ownership positively drains. */
void SudekiMpLanPartyHostControlRequestStop(SudekiMpLanPartyHostControl *host);
/* Caller must first disable/unregister/drain its observer and stop new calls.
 * Fails while native identities remain; never frees retained callback state. */
BOOL SudekiMpLanPartyHostControlDestroy(SudekiMpLanPartyHostControl *host);
#endif
