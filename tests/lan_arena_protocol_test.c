#include "network/lan_arena_protocol.h"
#include "network/lan_arena_endpoint.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpLanArenaPacket make_hello(uint8_t role, uint64_t token) {
    SudekiMpLanArenaPacket packet;
    size_t i;
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_HELLO;
    packet.sequence = 7u;
    packet.session_token = token;
    packet.body.hello.sequence = 7u;
    packet.body.hello.build_id = SUDEKIMP_LAN_ARENA_BUILD_ID;
    for (i = 0u; i < SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE; ++i) {
        packet.body.hello.game_hash[i] = (uint8_t)(i + 1u);
    }
    packet.body.hello.map_id = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
    packet.body.hello.role = role;
    packet.body.hello.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet.body.hello.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet.body.hello.session_token = token;
    return packet;
}

static void test_hello_round_trip_and_rejection(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source = make_hello(SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH, 0x0123456789abcdefULL);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaHandshakeExpectation expectation;
    SudekiMpLanArenaRejectReason reason = SUDEKIMP_LAN_ARENA_REJECT_NONE;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 72u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.type == SUDEKIMP_LAN_ARENA_PACKET_HELLO);
    CHECK(decoded.session_token == source.session_token);
    expectation.build_id = SUDEKIMP_LAN_ARENA_BUILD_ID;
    expectation.game_hash = source.body.hello.game_hash;
    expectation.map_id = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
    expectation.expected_sender_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    expectation.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    expectation.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    expectation.expected_session_token = source.session_token;
    CHECK(SudekiMpLanArenaHandshakeValid(&decoded.body.hello, &expectation, &reason));
    decoded.body.hello.map_id = 99u;
    CHECK(!SudekiMpLanArenaHandshakeValid(&decoded.body.hello, &expectation, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_MAP);
    decoded.body.hello.map_id = expectation.map_id;
    decoded.body.hello.game_hash[0] ^= 0xffu;
    CHECK(!SudekiMpLanArenaHandshakeValid(&decoded.body.hello, &expectation, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH);
    bytes[7] = 1u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
}

static void test_input_snapshot_and_malformed_lengths(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source;
    SudekiMpLanArenaPacket decoded;
    memset(&source, 0, sizeof(source));
    source.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    source.sequence = 19u;
    source.session_token = 42u;
    source.body.input.sequence = 19u;
    source.body.input.acknowledged_snapshot = 18u;
    source.body.input.client_tick = 123u;
    source.body.input.world_direction_x = -32767;
    source.body.input.world_direction_z = 32767;
    source.body.input.weak_attack_pressed = 1u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.input.world_direction_x == -32767);
    CHECK(decoded.body.input.weak_attack_pressed == 1u);
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size - 1u, &decoded));
    bytes[20] ^= 0xffu;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[20] ^= 0xffu;
    memset(&source, 0, sizeof(source));
    source.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    source.sequence = 20u;
    source.session_token = 42u;
    source.body.snapshot.sequence = 20u;
    source.body.snapshot.tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    source.body.snapshot.ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.snapshot.tal.native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    source.body.snapshot.ailish.native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.snapshot.tal.facing_z = 1.0f;
    source.body.snapshot.ailish.facing_z = 1.0f;
    source.body.snapshot.tal.hp = 10u;
    source.body.snapshot.ailish.hp = 20u;
    source.body.snapshot.enemy_count = 1u;
    source.body.snapshot.enemies[0].native_entity_id =
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    source.body.snapshot.enemies[0].hp = 55u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.enemy_count == 1u);
    CHECK(decoded.body.snapshot.enemies[0].native_entity_id ==
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID);
    bytes[size - 1u] = 2u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[size - 1u] = 3u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    source.body.snapshot.tal.animation_state = 6u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    source.body.snapshot.ailish.combat_state = 3u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    source.body.snapshot.ailish.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    source.body.snapshot.ailish.hp =
        SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE + 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.hp = 20u;
    source.body.snapshot.enemies[0].hp =
        SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE + 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.enemies[0].hp = 55u;
    source.body.snapshot.tal.x = NAN;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.x = 0.0f;
    source.body.snapshot.enemy_count = 2u;
    source.body.snapshot.enemies[1] = source.body.snapshot.enemies[0];
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
}

static void test_connection_sequence_timeout_and_authority(void) {
    SudekiMpLanArenaConnectionState state;
    SudekiMpLanArenaPacket packet;
    SudekiMpLanArenaRejectReason reason;
    memset(&state, 0, sizeof(state));
    state.phase = SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
    state.session_token = 11u;
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    packet.session_token = 11u;
    packet.sequence = 100u;
    CHECK(SudekiMpLanArenaConnectionAcceptPacket(&state, &packet, 50u, &reason));
    CHECK(!SudekiMpLanArenaConnectionAcceptPacket(&state, &packet, 51u, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE);
    packet.sequence = 101u;
    packet.session_token = 12u;
    CHECK(!SudekiMpLanArenaConnectionAcceptPacket(&state, &packet, 52u, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_TOKEN);
    packet.session_token = 11u;
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_HELLO;
    CHECK(!SudekiMpLanArenaConnectionAcceptPacket(&state, &packet, 53u, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY);
    CHECK(SudekiMpLanArenaConnectionTimedOut(&state, 1600u, 1500u));
    CHECK(!SudekiMpLanArenaConnectionTimedOut(&state, 1550u, 1500u));
    CHECK(SudekiMpLanArenaSequenceNewer(0u, 0xffffffffu));
    CHECK(!SudekiMpLanArenaSequenceNewer(0xffffffffu, 0u));

    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE;
    packet.session_token = 11u;
    packet.sequence = 102u;
    CHECK(SudekiMpLanArenaConnectionAcceptPacket(
        &state, &packet, 100u, &reason));
    CHECK(state.last_received_at_ms == 100u);
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_END;
    CHECK(!SudekiMpLanArenaConnectionAcceptPacket(
        &state, &packet, 101u, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE);
    CHECK(state.phase == SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED);
    packet.sequence = 103u;
    CHECK(SudekiMpLanArenaConnectionAcceptPacket(
        &state, &packet, 102u, &reason));
    CHECK(state.phase == SUDEKIMP_LAN_ARENA_CONNECTION_ENDED);
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE;
    packet.sequence = 104u;
    CHECK(!SudekiMpLanArenaConnectionAcceptPacket(
        &state, &packet, 103u, &reason));
    CHECK(state.phase == SUDEKIMP_LAN_ARENA_CONNECTION_ENDED);
}

static void test_keepalive_round_trip(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source;
    SudekiMpLanArenaPacket decoded;
    memset(&source, 0, sizeof(source));
    source.type = SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE;
    source.sequence = 77u;
    source.session_token = UINT64_C(0x1020304050607080);
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 20u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.type == SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE);
    CHECK(decoded.sequence == 77u);
    CHECK(decoded.session_token == source.session_token);
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size + 1u, &decoded));
}

static void test_direct_endpoint_parser(void) {
    char address[16];
    uint16_t port = 0u;
    CHECK(SudekiMpLanArenaParseEndpoint(
        "192.168.1.25", 26770u, address, sizeof(address), &port));
    CHECK(strcmp(address, "192.168.1.25") == 0);
    CHECK(port == 26770u);
    CHECK(SudekiMpLanArenaParseEndpoint(
        "10.0.0.7:30000", 26770u, address, sizeof(address), &port));
    CHECK(strcmp(address, "10.0.0.7") == 0);
    CHECK(port == 30000u);
    CHECK(!SudekiMpLanArenaParseEndpoint(
        "256.0.0.1", 26770u, address, sizeof(address), &port));
    CHECK(!SudekiMpLanArenaParseEndpoint(
        "127.0.0.1:80", 26770u, address, sizeof(address), &port));
    CHECK(!SudekiMpLanArenaParseEndpoint(
        "host.local:26770", 26770u, address, sizeof(address), &port));
    CHECK(!SudekiMpLanArenaParseEndpoint(
        "127.0.0.1:26770junk", 26770u, address, sizeof(address), &port));
}

int main(void) {
    test_hello_round_trip_and_rejection();
    test_input_snapshot_and_malformed_lengths();
    test_connection_sequence_timeout_and_authority();
    test_keepalive_round_trip();
    test_direct_endpoint_parser();
    if (failures != 0) {
        return 1;
    }
    puts("lan arena protocol tests passed");
    return 0;
}
