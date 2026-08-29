#ifndef SUDEKIMP_BRIDGE_RECEIVER_H
#define SUDEKIMP_BRIDGE_RECEIVER_H

#include "input/bridge_protocol.h"

#include <windows.h>

BOOL SudekiMpInputBridgeStart(unsigned int port, DWORD timeout_ms);
/* Windows-only direct input source. It normalizes XInput into the exact same
 * state contract as the Linux UDP bridge, so gameplay consumers remain
 * transport-neutral. `slot` is the Windows XInput user index (0..3). */
BOOL SudekiMpInputBridgeStartXInput(unsigned int slot);
void SudekiMpInputBridgeStop(void);
/* Serial-number ordering uses the usual uint32 half-range rule.  An exact
 * duplicate and the ambiguous half-range value are both not newer. */
BOOL SudekiMpInputBridgeSequenceIsNewer(
    uint32_t candidate,
    uint32_t baseline
);
/* Raw device state is reserved for narrow UI/consent handling. Gameplay
 * consumers must use SudekiMpInputBridgePoll so an in-flight transition vote
 * can neutralize Player 2 without making the controller appear disconnected. */
BOOL SudekiMpInputBridgePollRaw(SudekiMpInputBridgeState *state);
BOOL SudekiMpInputBridgePoll(SudekiMpInputBridgeState *state);
void SudekiMpInputBridgeSetGameplaySuppressed(BOOL suppressed);
BOOL SudekiMpInputBridgeGameplaySuppressed(void);
unsigned int SudekiMpInputBridgeBoundPort(void);
unsigned int SudekiMpInputBridgeXInputSlot(void);
BOOL SudekiMpInputBridgeUsesXInput(void);
const void *SudekiMpInputBridgeIdentity(void);

#endif
