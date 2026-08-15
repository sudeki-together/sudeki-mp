#ifndef SUDEKIMP_BRIDGE_RECEIVER_H
#define SUDEKIMP_BRIDGE_RECEIVER_H

#include "input/bridge_protocol.h"

#include <windows.h>

BOOL SudekiMpInputBridgeStart(unsigned int port, DWORD timeout_ms);
void SudekiMpInputBridgeStop(void);
BOOL SudekiMpInputBridgePoll(SudekiMpInputBridgeState *state);
unsigned int SudekiMpInputBridgeBoundPort(void);
const void *SudekiMpInputBridgeIdentity(void);

#endif
