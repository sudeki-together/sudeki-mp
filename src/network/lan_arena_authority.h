#ifndef SUDEKIMP_LAN_ARENA_AUTHORITY_H
#define SUDEKIMP_LAN_ARENA_AUTHORITY_H

#include "network/lan_arena_protocol.h"

#include <windows.h>

BOOL SudekiMpLanArenaHostRemoteInputAllowed(
    BOOL session_authenticated,
    BOOL player_two_requested,
    BOOL player_two_lease_exact,
    BOOL character_in_active_group,
    BOOL native_control_state_exact,
    BOOL direction_finite
);
BOOL SudekiMpLanArenaPacketAllowedForNode(
    SudekiMpLanArenaSimulationNodeRole local_node_role,
    SudekiMpLanArenaPacketType packet_type
);
BOOL SudekiMpLanArenaRemoteInputFresh(
    uint32_t last_input_at_ms,
    uint32_t now_ms,
    uint32_t maximum_age_ms
);

#endif
