#ifndef SUDEKIMP_BRIDGE_RECEIVER_H
#define SUDEKIMP_BRIDGE_RECEIVER_H

#include "input/bridge_protocol.h"

#include <windows.h>

BOOL SudekiMpInputBridgeStart(unsigned int port, DWORD timeout_ms);
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
const void *SudekiMpInputBridgeIdentity(void);

#endif
