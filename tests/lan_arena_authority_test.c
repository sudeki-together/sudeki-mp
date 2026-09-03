#include "network/lan_arena_authority.h"

#include <stdio.h>

static int failures;
#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); ++failures; \
} } while (0)

int main(void) {
    CHECK(SudekiMpLanArenaHostRemoteInputAllowed(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE));
    CHECK(!SudekiMpLanArenaHostRemoteInputAllowed(FALSE, TRUE, TRUE, TRUE, TRUE, TRUE));
    CHECK(!SudekiMpLanArenaHostRemoteInputAllowed(TRUE, TRUE, FALSE, TRUE, TRUE, TRUE));
    CHECK(!SudekiMpLanArenaHostRemoteInputAllowed(TRUE, TRUE, TRUE, FALSE, TRUE, TRUE));
    CHECK(!SudekiMpLanArenaHostRemoteInputAllowed(TRUE, TRUE, TRUE, TRUE, FALSE, TRUE));
    CHECK(!SudekiMpLanArenaHostRemoteInputAllowed(TRUE, TRUE, TRUE, TRUE, TRUE, FALSE));
    CHECK(SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
        SUDEKIMP_LAN_ARENA_PACKET_INPUT));
    CHECK(!SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
        SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT));
    CHECK(SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
        SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT));
    CHECK(!SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
        SUDEKIMP_LAN_ARENA_PACKET_INPUT));
    CHECK(SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
        SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE));
    CHECK(SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
        SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE));
    CHECK(!SudekiMpLanArenaPacketAllowedForNode(
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_INVALID,
        SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE));
    CHECK(SudekiMpLanArenaRemoteInputFresh(100u, 350u, 250u));
    CHECK(!SudekiMpLanArenaRemoteInputFresh(100u, 351u, 250u));
    CHECK(!SudekiMpLanArenaRemoteInputFresh(0u, 100u, 250u));
    CHECK(!SudekiMpLanArenaRemoteInputFresh(100u, 100u, 0u));
    CHECK(SudekiMpLanArenaRemoteInputFresh(
        UINT32_C(0xfffffff0), 0x20u, 0x30u));
    if (failures != 0) return 1;
    puts("lan arena authority tests passed");
    return 0;
}
