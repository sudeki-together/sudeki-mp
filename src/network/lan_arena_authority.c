#include "network/lan_arena_authority.h"

BOOL SudekiMpLanArenaHostRemoteInputAllowed(
    BOOL session_authenticated,
    BOOL player_two_requested,
    BOOL player_two_lease_exact,
    BOOL character_in_active_group,
    BOOL native_control_state_exact,
    BOOL direction_finite
) {
    return session_authenticated && player_two_requested &&
        player_two_lease_exact && character_in_active_group &&
        native_control_state_exact && direction_finite;
}

BOOL SudekiMpLanArenaPacketAllowedForNode(
    SudekiMpLanArenaSimulationNodeRole local_node_role,
    SudekiMpLanArenaPacketType packet_type
) {
    if (local_node_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) {
        return packet_type == SUDEKIMP_LAN_ARENA_PACKET_INPUT ||
            packet_type == SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE ||
            packet_type == SUDEKIMP_LAN_ARENA_PACKET_END;
    }
    if (local_node_role == SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) {
        return packet_type == SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT ||
            packet_type == SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE ||
            packet_type == SUDEKIMP_LAN_ARENA_PACKET_END;
    }
    return FALSE;
}

uint8_t SudekiMpLanArenaActorTypeForPlayerRole(
    SudekiMpLanArenaRole player_role
) {
    if (player_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        return SUDEKIMP_LAN_ARENA_TAL_TYPE;
    }
    if (player_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        return SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    }
    return 0u;
}

BOOL SudekiMpLanArenaPlayerOwnsActor(
    SudekiMpLanArenaRole player_role,
    uint8_t actor_type
) {
    uint8_t owned_actor =
        SudekiMpLanArenaActorTypeForPlayerRole(player_role);
    return owned_actor != 0u && actor_type == owned_actor;
}

BOOL SudekiMpLanArenaRemoteInputFresh(
    uint32_t last_input_at_ms,
    uint32_t now_ms,
    uint32_t maximum_age_ms
) {
    return last_input_at_ms != 0u && maximum_age_ms != 0u &&
        (uint32_t)(now_ms - last_input_at_ms) <= maximum_age_ms;
}
