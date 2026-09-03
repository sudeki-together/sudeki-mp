#include "network/lan_arena_session.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static uint8_t game_hash[SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE];

static SOCKET make_bound_socket(unsigned int *port) {
    SOCKET socket_value;
    struct sockaddr_in address;
    int address_size = sizeof(address);
    DWORD timeout = 500u;
    socket_value = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_value == INVALID_SOCKET) return INVALID_SOCKET;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0u;
    if (bind(socket_value, (const struct sockaddr *)&address,
            sizeof(address)) == SOCKET_ERROR ||
        getsockname(socket_value, (struct sockaddr *)&address,
            &address_size) == SOCKET_ERROR ||
        setsockopt(socket_value, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        closesocket(socket_value);
        return INVALID_SOCKET;
    }
    *port = ntohs(address.sin_port);
    return socket_value;
}

static BOOL send_packet(
    SOCKET socket_value,
    const struct sockaddr_in *destination,
    const SudekiMpLanArenaPacket *packet
) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    return SudekiMpLanArenaEncodePacket(bytes, &size, packet) &&
        sendto(socket_value, (const char *)bytes, (int)size, 0,
            (const struct sockaddr *)destination, sizeof(*destination)) ==
            (int)size;
}

static BOOL receive_packet(
    SOCKET socket_value,
    SudekiMpLanArenaPacket *packet,
    struct sockaddr_in *source
) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    int source_size = sizeof(*source);
    int received = recvfrom(socket_value, (char *)bytes, sizeof(bytes), 0,
        (struct sockaddr *)source, &source_size);
    return received > 0 &&
        SudekiMpLanArenaDecodePacket(bytes, (size_t)received, packet);
}

static void fill_hello(
    SudekiMpLanArenaPacket *packet,
    SudekiMpLanArenaPacketType type,
    SudekiMpLanArenaRole role,
    SudekiMpLanArenaSimulationNodeRole simulation_node_role,
    uint32_t sequence,
    uint64_t token
) {
    memset(packet, 0, sizeof(*packet));
    packet->type = type;
    packet->sequence = sequence;
    packet->session_token = token;
    packet->body.hello.sequence = sequence;
    packet->body.hello.build_id = SUDEKIMP_LAN_ARENA_BUILD_ID;
    memcpy(packet->body.hello.game_hash, game_hash, sizeof(game_hash));
    packet->body.hello.map_id = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
    packet->body.hello.role = (uint8_t)role;
    packet->body.hello.simulation_node_role =
        (uint8_t)simulation_node_role;
    packet->body.hello.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet->body.hello.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet->body.hello.session_token = token;
}

static void fill_snapshot(
    SudekiMpLanArenaPacket *packet,
    uint32_t sequence,
    uint64_t token,
    uint32_t acknowledged_input
) {
    memset(packet, 0, sizeof(*packet));
    packet->type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    packet->sequence = sequence;
    packet->session_token = token;
    packet->body.snapshot.sequence = sequence;
    packet->body.snapshot.acknowledged_input = acknowledged_input;
    packet->body.snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    packet->body.snapshot.tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet->body.snapshot.tal.native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet->body.snapshot.tal.facing_z = 1.0f;
    packet->body.snapshot.tal.hp = 1u;
    packet->body.snapshot.ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet->body.snapshot.ailish.native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet->body.snapshot.ailish.facing_z = 1.0f;
    packet->body.snapshot.ailish.hp = 1u;
}

static void test_host_session(void) {
    SOCKET peer;
    unsigned int peer_port;
    unsigned int host_port;
    SOCKET reservation;
    struct sockaddr_in host_address;
    struct sockaddr_in source;
    SudekiMpLanArenaSessionConfig config;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaPacket packet;
    SudekiMpLanArenaInput input;
    SudekiMpLanArenaSnapshot snapshot;
    const uint64_t token = UINT64_C(0x1122334455667788);

    peer = make_bound_socket(&peer_port);
    reservation = make_bound_socket(&host_port);
    CHECK(peer != INVALID_SOCKET && reservation != INVALID_SOCKET);
    CHECK(peer_port != 0u && host_port != 0u);
    if (peer == INVALID_SOCKET || reservation == INVALID_SOCKET) return;
    closesocket(reservation);
    memset(&config, 0, sizeof(config));
    config.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    config.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    config.port = host_port;
    config.timeout_ms = 1500u;
    config.game_hash = game_hash;
    CHECK(!SudekiMpLanArenaSessionStart(&config));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    config.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    CHECK(SudekiMpLanArenaSessionStart(&config));
    memset(&host_address, 0, sizeof(host_address));
    host_address.sin_family = AF_INET;
    host_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    host_address.sin_port = htons((u_short)host_port);
    fill_hello(&packet, SUDEKIMP_LAN_ARENA_PACKET_HELLO,
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
        1u, token);
    CHECK(send_packet(peer, &host_address, &packet));
    SudekiMpLanArenaSessionPoll(90u);
    CHECK(receive_packet(peer, &packet, &source));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_REJECT);
    CHECK(packet.body.reject_reason == SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(!status.peer_connected);
    fill_hello(&packet, SUDEKIMP_LAN_ARENA_PACKET_HELLO,
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 1u, token);
    CHECK(send_packet(peer, &host_address, &packet));
    SudekiMpLanArenaSessionPoll(100u);
    CHECK(receive_packet(peer, &packet, &source));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK);
    CHECK(packet.session_token == token);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.peer_connected);
    CHECK(status.local_simulation_node_role ==
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD);
    CHECK(status.peer_simulation_node_role ==
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA);

    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    packet.sequence = 2u;
    packet.session_token = token;
    packet.body.input.sequence = 2u;
    packet.body.input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet.body.input.world_direction_x = 1234;
    packet.body.input.world_direction_z = -2345;
    packet.body.input.aim_direction_x = 32767;
    packet.body.input.weak_attack_pressed = 1u;
    packet.body.input.weak_attack_held = 1u;
    packet.body.input.ranged_first_person_active = 1u;
    packet.body.input.cleanroom_combat_test_pressed = 1u;
    CHECK(send_packet(peer, &host_address, &packet));
    packet.sequence = 3u;
    packet.body.input.sequence = 3u;
    packet.body.input.world_direction_x = 4321;
    packet.body.input.world_direction_z = -1234;
    packet.body.input.aim_direction_x = 0;
    packet.body.input.aim_direction_z = 32767;
    packet.body.input.weak_attack_pressed = 0u;
    packet.body.input.weak_attack_held = 0u;
    packet.body.input.ranged_first_person_active = 0u;
    packet.body.input.cleanroom_combat_test_pressed = 0u;
    CHECK(send_packet(peer, &host_address, &packet));
    SudekiMpLanArenaSessionPoll(110u);
    CHECK(SudekiMpLanArenaSessionTakeRemoteInput(&input));
    CHECK(input.actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(input.world_direction_x == 4321);
    CHECK(input.aim_direction_x == 0);
    CHECK(input.aim_direction_z == 32767);
    CHECK(input.weak_attack_pressed == 1u);
    CHECK(input.weak_attack_held == 0u);
    CHECK(input.ranged_first_person_active == 0u);
    CHECK(input.cleanroom_combat_test_pressed == 1u);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    snapshot.tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot.tal.native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot.tal.facing_z = 1.0f;
    snapshot.tal.hp = 1u;
    snapshot.ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot.ailish.native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot.ailish.facing_z = 1.0f;
    snapshot.ailish.hp = 1u;
    snapshot.acknowledged_input = 2u;
    CHECK(SudekiMpLanArenaSessionSendSnapshot(&snapshot));
    CHECK(receive_packet(peer, &packet, &source));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT);
    CHECK(packet.body.snapshot.acknowledged_input == 2u);
    snapshot.acknowledged_input = 4u;
    CHECK(!SudekiMpLanArenaSessionSendSnapshot(&snapshot));
    CHECK(GetLastError() == ERROR_INVALID_DATA);

    SudekiMpLanArenaSessionPoll(400u);
    CHECK(receive_packet(peer, &packet, &source));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE);
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE;
    packet.sequence = 4u;
    packet.session_token = token;
    CHECK(send_packet(peer, &host_address, &packet));
    SudekiMpLanArenaSessionPoll(410u);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.peer_connected);
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    packet.sequence = 5u;
    packet.session_token = token;
    packet.body.input.sequence = 5u;
    packet.body.input.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    CHECK(send_packet(peer, &host_address, &packet));
    SudekiMpLanArenaSessionPoll(420u);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED);
    CHECK(status.failure == SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY);
    SudekiMpLanArenaSessionStop(FALSE);
    closesocket(peer);
}

static void test_client_session(void) {
    SOCKET host;
    unsigned int host_port;
    struct sockaddr_in client_address;
    SudekiMpLanArenaSessionConfig config;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaPacket packet;
    SudekiMpLanArenaInput input;
    SudekiMpLanArenaSnapshot snapshot;
    uint64_t token;

    host = make_bound_socket(&host_port);
    CHECK(host != INVALID_SOCKET);
    if (host == INVALID_SOCKET) return;
    memset(&config, 0, sizeof(config));
    config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    config.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    config.remote_ipv4 = "127.0.0.1";
    config.port = host_port;
    config.timeout_ms = 1500u;
    config.game_hash = game_hash;
    CHECK(SudekiMpLanArenaSessionStart(&config));
    SudekiMpLanArenaSessionPoll(1u);
    CHECK(receive_packet(host, &packet, &client_address));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_HELLO);
    token = packet.session_token;
    fill_hello(&packet, SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK,
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
        10u, token);
    CHECK(send_packet(host, &client_address, &packet));
    SudekiMpLanArenaSessionPoll(10u);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.peer_connected);
    CHECK(status.local_simulation_node_role ==
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA);
    CHECK(status.peer_simulation_node_role ==
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD);
    memset(&input, 0, sizeof(input));
    input.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    input.world_direction_z = 32767;
    CHECK(!SudekiMpLanArenaSessionSendInput(&input));
    input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    input.world_direction_z = 32767;
    CHECK(SudekiMpLanArenaSessionSendInput(&input));
    CHECK(receive_packet(host, &packet, &client_address));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_INPUT);
    CHECK(packet.body.input.actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    fill_snapshot(&packet, 11u, token, packet.sequence);
    CHECK(send_packet(host, &client_address, &packet));
    fill_snapshot(&packet, 12u, token, packet.body.snapshot.acknowledged_input);
    packet.body.snapshot.host_tick = 50u;
    CHECK(send_packet(host, &client_address, &packet));
    fill_snapshot(&packet, 13u, token, packet.body.snapshot.acknowledged_input);
    packet.body.snapshot.host_tick = 100u;
    CHECK(send_packet(host, &client_address, &packet));
    SudekiMpLanArenaSessionPoll(20u);
    CHECK(SudekiMpLanArenaSessionTakeRemoteSnapshot(&snapshot));
    CHECK(snapshot.match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE);
    CHECK(snapshot.sequence == 11u);
    CHECK(SudekiMpLanArenaSessionTakeRemoteSnapshot(&snapshot));
    CHECK(snapshot.sequence == 12u);
    CHECK(SudekiMpLanArenaSessionTakeRemoteSnapshot(&snapshot));
    CHECK(snapshot.sequence == 13u);
    CHECK(!SudekiMpLanArenaSessionTakeRemoteSnapshot(&snapshot));
    SudekiMpLanArenaSessionPoll(300u);
    CHECK(receive_packet(host, &packet, &client_address));
    CHECK(packet.type == SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE);
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE;
    packet.sequence = 14u;
    packet.session_token = token;
    CHECK(send_packet(host, &client_address, &packet));
    SudekiMpLanArenaSessionPoll(310u);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.peer_connected);
    SudekiMpLanArenaSessionPoll(1811u);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT);
    CHECK(status.failure == SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT);
    fill_hello(&packet, SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK,
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
        13u, token);
    CHECK(send_packet(host, &client_address, &packet));
    SudekiMpLanArenaSessionPoll(1820u);
    CHECK(SudekiMpLanArenaSessionGetStatus(&status));
    CHECK(status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT);
    CHECK(!status.peer_connected);
    SudekiMpLanArenaSessionStop(FALSE);
    closesocket(host);
}

int main(void) {
    WSADATA wsa;
    unsigned int index;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 2;
    for (index = 0u; index < sizeof(game_hash); ++index) {
        game_hash[index] = (uint8_t)(index + 1u);
    }
    test_host_session();
    test_client_session();
    WSACleanup();
    if (failures != 0) return 1;
    puts("lan arena session tests passed");
    return 0;
}
