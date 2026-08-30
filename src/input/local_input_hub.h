#ifndef SUDEKIMP_LOCAL_INPUT_HUB_H
#define SUDEKIMP_LOCAL_INPUT_HUB_H

#include "input/bridge_protocol.h"

#include <windows.h>
#include <stdint.h>

enum {
    SUDEKIMP_LOCAL_INPUT_MAX_SEATS = 4u,
    SUDEKIMP_LOCAL_INPUT_HOST_MASK = 0x01u,
    SUDEKIMP_LOCAL_INPUT_CONTROLLER_MASK = 0x0eu
};

/* A separate, opt-in input bank for expanded local sessions.  P1 remains
 * keyboard/mouse-owned; bits 1..3 select P2..P4.  XInput slots are stable
 * seat bindings and may be disconnected at startup. */
BOOL SudekiMpLocalInputHubStartXInput(
    uint8_t requested_human_mask,
    const unsigned int controller_slots[3]
);

/* Linux/Wine transport.  P2 listens on base_port, P3 on base_port+1, and P4
 * on base_port+2.  Only ports for requested seats are opened. */
BOOL SudekiMpLocalInputHubStartUdp(
    uint8_t requested_human_mask,
    unsigned int base_port,
    DWORD timeout_ms
);

void SudekiMpLocalInputHubStop(void);
BOOL SudekiMpLocalInputHubPollRaw(
    unsigned int seat_index,
    SudekiMpInputBridgeState *state
);
BOOL SudekiMpLocalInputHubPoll(
    unsigned int seat_index,
    SudekiMpInputBridgeState *state
);
void SudekiMpLocalInputHubSetGameplaySuppressed(BOOL suppressed);
BOOL SudekiMpLocalInputHubGameplaySuppressed(void);
uint8_t SudekiMpLocalInputHubRequestedMask(void);
uint8_t SudekiMpLocalInputHubConnectedMask(void);
unsigned int SudekiMpLocalInputHubSeatPort(unsigned int seat_index);
unsigned int SudekiMpLocalInputHubSeatController(unsigned int seat_index);
/* Identity and generation are public authority only while the requested seat
 * is currently connected.  The internal monotonic generation survives Stop
 * so a later connection can never recreate an old seat lease. */
const void *SudekiMpLocalInputHubSeatIdentity(unsigned int seat_index);
uint32_t SudekiMpLocalInputHubSeatIdentityGeneration(unsigned int seat_index);

#endif
