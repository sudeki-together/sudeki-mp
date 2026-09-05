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

static SudekiMpLanArenaPacket make_hello(
    uint8_t role,
    uint8_t simulation_node_role,
    uint64_t token
) {
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
    packet.body.hello.simulation_node_role = simulation_node_role;
    packet.body.hello.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet.body.hello.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet.body.hello.session_token = token;
    return packet;
}

static void test_hello_round_trip_and_rejection(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source = make_hello(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
        0x0123456789abcdefULL);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaHandshakeExpectation expectation;
    SudekiMpLanArenaRejectReason reason = SUDEKIMP_LAN_ARENA_REJECT_NONE;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 73u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.type == SUDEKIMP_LAN_ARENA_PACKET_HELLO);
    CHECK(decoded.session_token == source.session_token);
    expectation.build_id = SUDEKIMP_LAN_ARENA_BUILD_ID;
    expectation.game_hash = source.body.hello.game_hash;
    expectation.map_id = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
    expectation.expected_sender_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    expectation.expected_sender_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    expectation.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    expectation.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    expectation.expected_session_token = source.session_token;
    CHECK(SudekiMpLanArenaHandshakeValid(&decoded.body.hello, &expectation, &reason));
    decoded.body.hello.simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    CHECK(!SudekiMpLanArenaHandshakeValid(
        &decoded.body.hello, &expectation, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY);
    decoded.body.hello.simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    decoded.body.hello.map_id = 99u;
    CHECK(!SudekiMpLanArenaHandshakeValid(&decoded.body.hello, &expectation, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_MAP);
    decoded.body.hello.map_id = expectation.map_id;
    decoded.body.hello.game_hash[0] ^= 0xffu;
    CHECK(!SudekiMpLanArenaHandshakeValid(&decoded.body.hello, &expectation, &reason));
    CHECK(reason == SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH);
    source.body.hello.simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_INVALID;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.hello.simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
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
    source.body.input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.input.world_direction_x = -32767;
    source.body.input.world_direction_z = 32767;
    source.body.input.aim_direction_x = 16384;
    source.body.input.aim_direction_y = -4096;
    source.body.input.aim_direction_z = 28000;
    source.body.input.weak_attack_pressed = 1u;
    source.body.input.weak_attack_held = 1u;
    source.body.input.ranged_first_person_active = 1u;
    source.body.input.cleanroom_combat_test_pressed = 1u;
    source.body.input.skill_pressed = 1u;
    source.body.input.skill_slot = 4u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 51u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.input.actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(decoded.body.input.world_direction_x == -32767);
    CHECK(decoded.body.input.aim_direction_x == 16384);
    CHECK(decoded.body.input.aim_direction_y == -4096);
    CHECK(decoded.body.input.aim_direction_z == 28000);
    CHECK(decoded.body.input.weak_attack_pressed == 1u);
    CHECK(decoded.body.input.weak_attack_held == 1u);
    CHECK(decoded.body.input.ranged_first_person_active == 1u);
    CHECK(decoded.body.input.cleanroom_combat_test_pressed == 1u);
    CHECK(decoded.body.input.skill_pressed == 1u);
    CHECK(decoded.body.input.skill_slot == 4u);
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size - 1u, &decoded));
    bytes[20] ^= 0xffu;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[20] ^= 0xffu;
    source.body.input.weak_attack_held = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.weak_attack_held = 1u;
    source.body.input.ranged_first_person_active = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.ranged_first_person_active = 1u;
    source.body.input.cleanroom_combat_test_pressed = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.cleanroom_combat_test_pressed = 1u;
    source.body.input.skill_pressed = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.skill_pressed = 1u;
    source.body.input.skill_slot = 6u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.skill_pressed = 0u;
    source.body.input.skill_slot = 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.skill_slot = 0u;
    source.body.input.actor_type = 0xffu;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.input.aim_direction_x = 1;
    source.body.input.aim_direction_y = 0;
    source.body.input.aim_direction_z = 0;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    memset(&source, 0, sizeof(source));
    source.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    source.sequence = 20u;
    source.session_token = 42u;
    source.body.snapshot.sequence = 20u;
    source.body.snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    source.body.snapshot.combat_enabled = 1u;
    source.body.snapshot.tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    source.body.snapshot.ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.snapshot.tal.native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    source.body.snapshot.ailish.native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.snapshot.tal.facing_z = 1.0f;
    source.body.snapshot.ailish.facing_z = 1.0f;
    source.body.snapshot.tal.hp = 10u;
    source.body.snapshot.ailish.hp = 20u;
    source.body.snapshot.tal.action_sequence = 0x1234u;
    source.body.snapshot.ailish.action_sequence = 0xabcdu;
    source.body.snapshot.tal.skill_sequence = 7u;
    source.body.snapshot.tal.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    source.body.snapshot.tal.skill_slot = 2u;
    source.body.snapshot.tal.skill_active = 1u;
    source.body.snapshot.tal.skill_cost = 125u;
    source.body.snapshot.tal.skill_presentation_valid = 1u;
    source.body.snapshot.tal.skill_presentation_channel_count = 2u;
    source.body.snapshot.tal.skill_presentation_selector[0] = 103;
    source.body.snapshot.tal.skill_presentation_state[0] = 1u;
    source.body.snapshot.tal.skill_presentation_rate[0] = 24.0f;
    source.body.snapshot.tal.skill_presentation_time[0] = 9.5f;
    source.body.snapshot.tal.skill_presentation_blend[0] = 0.75f;
    source.body.snapshot.enemy_count = 1u;
    source.body.snapshot.enemies[0].native_entity_id =
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    source.body.snapshot.enemies[0].hp = 55u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 917u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.combat_enabled == 1u);
    CHECK(decoded.body.snapshot.tal.action_variant ==
        SUDEKIMP_LAN_ARENA_ACTION_NONE);
    CHECK(decoded.body.snapshot.tal.action_sequence == 0x1234u);
    CHECK(decoded.body.snapshot.ailish.action_sequence == 0xabcdu);
    CHECK(decoded.body.snapshot.tal.skill_sequence == 7u);
    CHECK(decoded.body.snapshot.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER);
    CHECK(decoded.body.snapshot.tal.skill_slot == 2u);
    CHECK(decoded.body.snapshot.tal.skill_active == 1u);
    CHECK(decoded.body.snapshot.tal.skill_cost == 125u);
    CHECK(decoded.body.snapshot.tal.skill_presentation_valid == 1u);
    CHECK(decoded.body.snapshot.tal.skill_presentation_channel_count == 2u);
    CHECK(decoded.body.snapshot.tal.skill_presentation_selector[0] == 103);
    CHECK(decoded.body.snapshot.tal.skill_presentation_state[0] == 1u);
    CHECK(fabsf(decoded.body.snapshot.tal.skill_presentation_rate[0] -
        24.0f) < 0.001f);
    CHECK(fabsf(decoded.body.snapshot.tal.skill_presentation_time[0] -
        9.5f) < 0.001f);
    CHECK(fabsf(decoded.body.snapshot.tal.skill_presentation_blend[0] -
        0.75f) < 0.001f);
    CHECK(decoded.body.snapshot.enemy_count == 1u);
    CHECK(decoded.body.snapshot.enemies[0].native_entity_id ==
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID);
    source.body.snapshot.tal.skill_kind = 3u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_slot = 0u;
    source.body.snapshot.tal.skill_cost = 0u;
    source.body.snapshot.tal.skill_presentation_selector[0] = 75;
    source.body.snapshot.tal.skill_presentation_state[1] = 192u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(decoded.body.snapshot.tal.skill_slot == 0u);
    CHECK(decoded.body.snapshot.tal.skill_cost == 0u);
    source.body.snapshot.tal.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    source.body.snapshot.tal.skill_slot = 2u;
    source.body.snapshot.tal.skill_cost = 125u;
    source.body.snapshot.tal.skill_presentation_selector[0] = 103;
    source.body.snapshot.tal.skill_presentation_state[1] = 0u;
    source.body.snapshot.tal.skill_presentation_valid = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_presentation_valid = 1u;
    source.body.snapshot.tal.skill_presentation_channel_count = 5u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_presentation_channel_count = 2u;
    source.body.snapshot.tal.skill_presentation_selector[0] = 4096;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_presentation_selector[0] = 103;
    source.body.snapshot.tal.skill_presentation_time[0] = NAN;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_presentation_time[0] = 9.5f;
    source.body.snapshot.tal.skill_active = 0u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.skill_active = 1u;
    source.body.snapshot.tal.action_terminal_phase_q8 = 49u * 256u;
    source.body.snapshot.tal.idle_entry_phase_q8 = 2u * 256u;
    source.body.snapshot.tal.action_retirement_valid = 1u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.action_terminal_phase_q8 ==
        49u * 256u);
    CHECK(decoded.body.snapshot.tal.idle_entry_phase_q8 == 2u * 256u);
    CHECK(decoded.body.snapshot.tal.action_retirement_valid == 1u);
    source.body.snapshot.tal.action_retirement_valid = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.action_retirement_valid = 0u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.action_terminal_phase_q8 = 0u;
    source.body.snapshot.tal.idle_entry_phase_q8 = 0u;
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    source.body.snapshot.tal.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    source.body.snapshot.tal.action_phase_valid = 1u;
    source.body.snapshot.tal.action_phase_q8 = 18u * 256u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    source.body.snapshot.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    source.body.snapshot.tal.action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    source.body.snapshot.tal.action_phase_valid = 0u;
    source.body.snapshot.tal.action_phase_q8 = 0u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    bytes[33] = 2u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[33] = 1u;
    bytes[size - 1u] = 2u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[size - 1u] = 3u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    source.body.snapshot.tal.animation_state = 6u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.combat_enabled = 2u;
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.combat_enabled = 1u;
    source.body.snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_WAITING;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ENDED;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    source.body.snapshot.ailish.combat_state = 6u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    source.body.snapshot.ailish.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    source.body.snapshot.ailish.action_phase_valid = 1u;
    source.body.snapshot.ailish.action_phase_q8 = 18u * 256u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.ailish.action_variant ==
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(decoded.body.snapshot.ailish.action_phase_valid == 1u);
    CHECK(decoded.body.snapshot.ailish.action_phase_q8 == 18u * 256u);
    source.body.snapshot.ailish.action_phase_valid = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.action_phase_valid = 1u;
    source.body.snapshot.ailish.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_STRONG;
    source.body.snapshot.ailish.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    source.body.snapshot.ailish.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    source.body.snapshot.ailish.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    source.body.snapshot.ailish.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_NONE;
    source.body.snapshot.ailish.action_phase_valid = 0u;
    source.body.snapshot.ailish.action_phase_q8 = 0u;
    source.body.snapshot.ailish.action_phase_q8 = 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.action_phase_q8 = 0u;
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    source.body.snapshot.tal.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK;
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    source.body.snapshot.tal.action_phase_valid = 1u;
    source.body.snapshot.tal.action_phase_q8 = 35u * 256u;
    source.body.snapshot.tal.action_sequence = 0x1237u;
    source.body.snapshot.tal.action_history_count = 3u;
    source.body.snapshot.tal.action_history[0].sequence = 0x1235u;
    source.body.snapshot.tal.action_history[0].variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    source.body.snapshot.tal.action_history[0].host_tick = 100u;
    source.body.snapshot.tal.action_history[1].sequence = 0x1236u;
    source.body.snapshot.tal.action_history[1].variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO;
    source.body.snapshot.tal.action_history[1].host_tick = 120u;
    source.body.snapshot.tal.action_history[2].sequence = 0x1237u;
    source.body.snapshot.tal.action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    source.body.snapshot.tal.action_history[2].host_tick = 140u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.combat_state ==
        SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK);
    CHECK(decoded.body.snapshot.tal.action_variant ==
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS);
    CHECK(decoded.body.snapshot.tal.action_phase_valid == 1u);
    CHECK(decoded.body.snapshot.tal.action_phase_q8 == 35u * 256u);
    CHECK(decoded.body.snapshot.tal.action_history_count == 3u);
    CHECK(decoded.body.snapshot.tal.action_history[1].sequence == 0x1236u);
    CHECK(decoded.body.snapshot.tal.action_history[1].variant ==
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO);
    CHECK(decoded.body.snapshot.tal.action_history[2].host_tick == 140u);
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE;
    source.body.snapshot.tal.action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.action_variant ==
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE);
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    source.body.snapshot.tal.action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    source.body.snapshot.tal.action_history[1].sequence = 0x1235u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.action_history[1].sequence = 0x1236u;
    source.body.snapshot.tal.action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_MAX + 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    source.body.snapshot.tal.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_SWEEP;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.combat_state ==
        SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK);
    CHECK(decoded.body.snapshot.tal.action_variant ==
        SUDEKIMP_LAN_ARENA_ACTION_SWEEP);
    source.body.snapshot.tal.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_BLOCK;
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_BLOCK;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.combat_state ==
        SUDEKIMP_LAN_ARENA_COMBAT_BLOCK);
    CHECK(decoded.body.snapshot.tal.action_variant ==
        SUDEKIMP_LAN_ARENA_ACTION_BLOCK);
    source.body.snapshot.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO;
    source.body.snapshot.tal.combat_state =
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    source.body.snapshot.tal.action_variant =
        SUDEKIMP_LAN_ARENA_ACTION_NONE;
    source.body.snapshot.tal.action_phase_valid = 0u;
    source.body.snapshot.tal.action_phase_q8 = 0u;
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

static SudekiMpLanArenaPacket make_minimal_snapshot_packet(
    uint32_t sequence
) {
    SudekiMpLanArenaPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    packet.sequence = sequence;
    packet.session_token = UINT64_C(0x445566778899aabb);
    packet.body.snapshot.sequence = sequence;
    packet.body.snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    packet.body.snapshot.combat_enabled = 1u;
    packet.body.snapshot.tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet.body.snapshot.tal.native_entity_id =
        SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet.body.snapshot.tal.facing_z = 1.0f;
    packet.body.snapshot.tal.hp = 100u;
    packet.body.snapshot.ailish.actor_type =
        SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet.body.snapshot.ailish.native_entity_id =
        SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet.body.snapshot.ailish.facing_z = 1.0f;
    packet.body.snapshot.ailish.hp = 100u;
    return packet;
}

static void test_character_presentation_optional_sidecar(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(91u);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaActorSnapshot *ailish = &source.body.snapshot.ailish;
    unsigned int channel;

    ailish->skill_sequence = 6u;
    ailish->skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    ailish->skill_slot = 5u;
    ailish->skill_active = 1u;
    ailish->skill_cost = 40u;
    CHECK(SudekiMpLanArenaSkillPresentationValid(
        ailish, SUDEKIMP_LAN_ARENA_AILISH_TYPE));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.ailish.skill_sequence == 6u);
    CHECK(decoded.body.snapshot.ailish.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER);
    CHECK(decoded.body.snapshot.ailish.skill_slot == 5u);
    CHECK(decoded.body.snapshot.ailish.skill_active == 1u);
    CHECK(decoded.body.snapshot.ailish.skill_cost == 40u);
    CHECK(decoded.body.snapshot.ailish.skill_presentation_valid == 0u);

    ailish->skill_presentation_valid = 1u;
    ailish->skill_presentation_channel_count =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS;
    for (channel = 0u;
         channel < SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS;
         ++channel) {
        ailish->skill_presentation_selector[channel] =
            channel == 0u ? 20 : 0;
        ailish->skill_presentation_state[channel] =
            channel == 0u ? 0u : 192u;
        ailish->skill_presentation_rate[channel] = 24.0f;
        ailish->skill_presentation_time[channel] = 40.8f;
    }
    ailish->skill_presentation_time[4] = 4096.0f;
    CHECK(SudekiMpLanArenaSkillPresentationValid(
        ailish, SUDEKIMP_LAN_ARENA_AILISH_TYPE));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));

    ailish->skill_presentation_time[4] = 4161.93896f;
    CHECK(!SudekiMpLanArenaSkillPresentationValid(
        ailish, SUDEKIMP_LAN_ARENA_AILISH_TYPE));
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));

    ailish->skill_presentation_valid = 0u;
    ailish->skill_presentation_channel_count = 0u;
    memset(ailish->skill_presentation_selector, 0,
        sizeof(ailish->skill_presentation_selector));
    memset(ailish->skill_presentation_state, 0,
        sizeof(ailish->skill_presentation_state));
    memset(ailish->skill_presentation_rate, 0,
        sizeof(ailish->skill_presentation_rate));
    memset(ailish->skill_presentation_time, 0,
        sizeof(ailish->skill_presentation_time));
    memset(ailish->skill_presentation_blend, 0,
        sizeof(ailish->skill_presentation_blend));
    CHECK(SudekiMpLanArenaSkillPresentationValid(
        ailish, SUDEKIMP_LAN_ARENA_AILISH_TYPE));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.ailish.skill_sequence == 6u);
    CHECK(decoded.body.snapshot.ailish.skill_slot == 5u);
    CHECK(decoded.body.snapshot.ailish.skill_active == 1u);
    CHECK(decoded.body.snapshot.ailish.skill_presentation_valid == 0u);
}

static void test_spirit_presentation_wire_lifecycle(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(90u);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaActorSnapshot *tal = &source.body.snapshot.tal;

    CHECK(SudekiMpLanArenaSpiritPresentationSelectorValid(75));
    CHECK(SudekiMpLanArenaSpiritPresentationSelectorValid(113));
    CHECK(SudekiMpLanArenaSpiritPresentationSelectorValid(114));
    CHECK(!SudekiMpLanArenaSpiritPresentationSelectorValid(112));

    /* A Spirit transaction deliberately shares only the bounded renderer
     * witness with a character skill. Its discriminator and zero slot/cost
     * survive the wire so the client cannot route it through CSkill::Use. */
    tal->skill_sequence = 41u;
    tal->skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    tal->skill_active = 1u;
    tal->skill_presentation_valid = 1u;
    tal->skill_presentation_channel_count = 2u;
    tal->skill_presentation_selector[0] = 75;
    tal->skill_presentation_state[0] = 1u;
    tal->skill_presentation_state[1] = 192u;
    tal->skill_presentation_rate[0] = 24.0f;
    tal->skill_presentation_time[0] = 12.5f;
    tal->skill_presentation_blend[0] = 0.25f;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.skill_sequence == 41u);
    CHECK(decoded.body.snapshot.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(decoded.body.snapshot.tal.skill_slot == 0u);
    CHECK(decoded.body.snapshot.tal.skill_cost == 0u);
    CHECK(decoded.body.snapshot.tal.skill_active == 1u);
    CHECK(decoded.body.snapshot.tal.skill_presentation_selector[0] == 75);

    /* The exact native transaction changes channel-zero topology while the
     * same Spirit sequence remains active. Selector 113 is its observed
     * middle stage, not an untrusted request for an arbitrary animation. */
    tal->skill_presentation_selector[0] = 113;
    tal->skill_presentation_time[0] = 0.5f;
    CHECK(SudekiMpLanArenaSkillPresentationValid(
        tal, SUDEKIMP_LAN_ARENA_TAL_TYPE));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.skill_sequence == 41u);
    CHECK(decoded.body.snapshot.tal.skill_active == 1u);
    CHECK(decoded.body.snapshot.tal.skill_presentation_selector[0] == 113);

    tal->skill_presentation_selector[0] = 114;
    tal->skill_presentation_time[0] = 1.0f;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.skill_presentation_selector[0] == 114);
    tal->skill_presentation_selector[0] = 75;
    tal->skill_presentation_time[0] = 12.5f;

    source.body.snapshot.ailish = *tal;
    source.body.snapshot.ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.snapshot.ailish.native_entity_id =
        SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish = decoded.body.snapshot.ailish;

    tal->skill_presentation_state[0] = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    tal->skill_presentation_state[0] = 1u;
    tal->skill_presentation_selector[0] = 112;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    tal->skill_presentation_selector[0] = 75;

    tal->skill_slot = 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    tal->skill_slot = 0u;
    tal->skill_cost = 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    tal->skill_cost = 0u;

    /* Retirement preserves sequence/kind as the authoritative terminal edge,
     * but no active renderer payload may leak beyond that edge. */
    tal->skill_active = 0u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    tal->skill_presentation_valid = 0u;
    tal->skill_presentation_channel_count = 0u;
    memset(tal->skill_presentation_selector, 0,
        sizeof(tal->skill_presentation_selector));
    memset(tal->skill_presentation_state, 0,
        sizeof(tal->skill_presentation_state));
    memset(tal->skill_presentation_rate, 0,
        sizeof(tal->skill_presentation_rate));
    memset(tal->skill_presentation_time, 0,
        sizeof(tal->skill_presentation_time));
    memset(tal->skill_presentation_blend, 0,
        sizeof(tal->skill_presentation_blend));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.tal.skill_sequence == 41u);
    CHECK(decoded.body.snapshot.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(decoded.body.snapshot.tal.skill_active == 0u);
    CHECK(decoded.body.snapshot.tal.skill_presentation_valid == 0u);

    tal->skill_sequence = 0u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
}

typedef struct SpiritAudioSinkState {
    unsigned int calls;
    SudekiMpLanArenaSpiritAudioCue cue;
    int accept;
} SpiritAudioSinkState;

static int spirit_audio_sink(
    void *context,
    SudekiMpLanArenaSpiritAudioCue cue
) {
    SpiritAudioSinkState *state = (SpiritAudioSinkState *)context;
    if (state == NULL) return 0;
    ++state->calls;
    state->cue = cue;
    return state->accept;
}

static void set_active_tal_spirit(
    SudekiMpLanArenaSnapshot *snapshot,
    uint16_t skill_sequence
) {
    SudekiMpLanArenaActorSnapshot *tal = &snapshot->tal;
    tal->skill_sequence = skill_sequence;
    tal->skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    tal->skill_active = 1u;
    tal->skill_presentation_valid = 1u;
    tal->skill_presentation_channel_count = 2u;
    tal->skill_presentation_selector[0] = 75;
    tal->skill_presentation_state[0] = 1u;
    tal->skill_presentation_state[1] = 192u;
    tal->skill_presentation_rate[0] = 24.0f;
}

static void test_spirit_audio_semantic_journal(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(92u);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaSnapshot *snapshot = &source.body.snapshot;
    SudekiMpLanArenaSpiritAudioCursor cursor;
    SpiritAudioSinkState sink = {0u, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_NONE, 1};
    unsigned int replayed = 99u;

    CHECK(SUDEKIMP_LAN_ARENA_MAX_SNAPSHOT_PACKET_SIZE == 1232u);
    CHECK(SUDEKIMP_LAN_ARENA_MAX_SNAPSHOT_PACKET_SIZE <=
        SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE);
    set_active_tal_spirit(snapshot, 41u);
    snapshot->spirit_audio_history_count = 2u;
    snapshot->spirit_audio_history[0].event_sequence = UINT16_MAX;
    snapshot->spirit_audio_history[0].skill_sequence = 40u;
    snapshot->spirit_audio_history[0].cue =
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
    snapshot->spirit_audio_history[1].event_sequence = 1u;
    snapshot->spirit_audio_history[1].skill_sequence = 41u;
    snapshot->spirit_audio_history[1].cue =
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
    CHECK(SudekiMpLanArenaSpiritAudioJournalValid(snapshot));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 896u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_audio_history_count == 2u);
    CHECK(decoded.body.snapshot.spirit_audio_history[0].event_sequence ==
        UINT16_MAX);
    CHECK(decoded.body.snapshot.spirit_audio_history[1].event_sequence == 1u);
    CHECK(decoded.body.snapshot.spirit_audio_history[1].skill_sequence == 41u);
    CHECK(decoded.body.snapshot.spirit_audio_history[1].cue ==
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START);

    SudekiMpLanArenaSpiritAudioCursorReset(&cursor);
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, &decoded.body.snapshot,
        spirit_audio_sink, &sink, &replayed));
    CHECK(replayed == 1u && sink.calls == 1u &&
        sink.cue == SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START &&
        cursor.initialized != 0u && cursor.last_event_sequence == 1u);
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, &decoded.body.snapshot,
        spirit_audio_sink, &sink, &replayed));
    CHECK(replayed == 0u && sink.calls == 1u);

    /* A fresh event that does not name the exact current active Spirit is
     * consumed without audio. It cannot replay later when actor state changes. */
    snapshot = &decoded.body.snapshot;
    snapshot->spirit_audio_history_count = 3u;
    snapshot->spirit_audio_history[2].event_sequence = 2u;
    snapshot->spirit_audio_history[2].skill_sequence = 42u;
    snapshot->spirit_audio_history[2].cue =
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
    snapshot->tal.skill_active = 0u;
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));
    CHECK(replayed == 0u && sink.calls == 1u &&
        cursor.last_event_sequence == 2u);
    snapshot->tal.skill_active = 1u;
    snapshot->tal.skill_sequence = 42u;
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));
    CHECK(replayed == 0u && sink.calls == 1u);

    snapshot->spirit_audio_history_count = 4u;
    snapshot->spirit_audio_history[3].event_sequence = 3u;
    snapshot->spirit_audio_history[3].skill_sequence = 43u;
    snapshot->spirit_audio_history[3].cue =
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
    snapshot->tal.skill_sequence = 43u;
    sink.accept = 0;
    CHECK(!SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));
    CHECK(sink.calls == 2u && cursor.last_event_sequence == 2u);
    sink.accept = 1;
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));
    CHECK(replayed == 1u && sink.calls == 3u &&
        cursor.last_event_sequence == 3u);

    snapshot->spirit_audio_history_count = 3u;
    CHECK(!SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));
    CHECK(sink.calls == 3u);
    snapshot->spirit_audio_history_count = 0u;
    CHECK(!SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));
    SudekiMpLanArenaSpiritAudioCursorReset(&cursor);
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(
        &cursor, snapshot, spirit_audio_sink, &sink, &replayed));

    snapshot->spirit_audio_history_count = 2u;
    snapshot->spirit_audio_history[1].event_sequence =
        snapshot->spirit_audio_history[0].event_sequence;
    CHECK(!SudekiMpLanArenaSpiritAudioJournalValid(snapshot));
    snapshot->spirit_audio_history[1].event_sequence = 1u;
    snapshot->spirit_audio_history[1].skill_sequence = 40u;
    CHECK(!SudekiMpLanArenaSpiritAudioJournalValid(snapshot));
    snapshot->spirit_audio_history[1].skill_sequence = 41u;
    snapshot->spirit_audio_history[1].cue = 2u;
    CHECK(!SudekiMpLanArenaSpiritAudioJournalValid(snapshot));
    snapshot->spirit_audio_history[1].cue =
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
    snapshot->spirit_audio_history[0].event_sequence = 0u;
    CHECK(!SudekiMpLanArenaSpiritAudioJournalValid(snapshot));
    snapshot->spirit_audio_history[0].event_sequence = UINT16_MAX;
    snapshot->spirit_audio_history_count =
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY + 1u;
    CHECK(!SudekiMpLanArenaSpiritAudioJournalValid(snapshot));
}

static SudekiMpLanArenaSpiritVfxSnapshot make_spirit_vfx(uint32_t instance) {
    SudekiMpLanArenaSpiritVfxSnapshot result;
    memset(&result, 0, sizeof(result));
    result.instance_sequence = instance;
    result.skill_sequence = 7u;
    result.kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_SOUL_TRANSFER;
    result.emitted_host_tick = 90u;
    result.phase_valid = 1u;
    result.phase = 3.5f;
    result.position[0] = -10.0f;
    result.position[1] = 20.0f;
    result.position[2] = 30.0f;
    result.rotation_xyzw[3] = 1.0f;
    result.scale[0] = 1.0f;
    result.scale[1] = 2.0f;
    result.scale[2] = 3.0f;
    return result;
}

static void test_status_visual_owner_wire(void) {
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(94u), decoded;
    SudekiMpLanArenaSnapshot *snapshot = &source.body.snapshot;
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u, offset;
    snapshot->host_tick = 100u;
    snapshot->spirit_vfx_observed = 1u;
    snapshot->spirit_vfx_count = 2u;
    snapshot->spirit_vfx[0] = make_spirit_vfx(100u);
    snapshot->spirit_vfx[0].skill_sequence = 0u;
    snapshot->spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST;
    snapshot->spirit_vfx[0].owner_actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot->spirit_vfx[1] = snapshot->spirit_vfx[0];
    snapshot->spirit_vfx[1].instance_sequence = 101u;
    snapshot->spirit_vfx[1].owner_actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    /* Status lifetime is valid before any cast, after a cast, and while a
     * different character skill is active. Never manufacture cast provenance. */
    CHECK(SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_vfx[0].owner_actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE);
    CHECK(decoded.body.snapshot.spirit_vfx[1].owner_actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(decoded.body.snapshot.spirit_vfx[0].skill_sequence == 0u);
    offset = size - 1u - SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY * 57u;
    bytes[offset + 56u] = 0u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[offset + 56u] = 0xffu;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    snapshot->tal.skill_sequence = 123u;
    snapshot->tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    CHECK(SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
    snapshot->spirit_vfx[0].skill_sequence = 123u;
    CHECK(!SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
    snapshot->spirit_vfx[0].skill_sequence = 0u;
    snapshot->spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE;
    CHECK(!SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
}

static void test_spirit_vfx_roster_wire(void) {
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(93u);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaSnapshot *snapshot = &source.body.snapshot;
    SudekiMpLanArenaSpiritVfxSnapshot valid = make_spirit_vfx(100u);
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    size_t entries_offset;
    unsigned int index;

    CHECK(SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION == 26u);
    CHECK(SUDEKIMP_LAN_ARENA_BUILD_ID == UINT32_C(0x4c413236));
    CHECK(SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE == 1232u);
    snapshot->host_tick = 100u;
    snapshot->tal.skill_sequence = 7u;
    snapshot->tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    /* A finite tail is valid after the originating native transaction ends. */
    snapshot->tal.skill_active = 0u;
    snapshot->spirit_vfx_observed = 1u;
    snapshot->spirit_vfx_count = 1u;
    snapshot->spirit_vfx[0] = valid;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 896u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_vfx_observed == 1u);
    CHECK(decoded.body.snapshot.spirit_vfx_count == 1u);
    CHECK(decoded.body.snapshot.spirit_vfx[0].instance_sequence == 100u);
    CHECK(decoded.body.snapshot.spirit_vfx[0].skill_sequence == 7u);
    CHECK(decoded.body.snapshot.spirit_vfx[0].kind == valid.kind);
    CHECK(decoded.body.snapshot.spirit_vfx[0].emitted_host_tick == 90u);
    CHECK(decoded.body.snapshot.spirit_vfx[0].phase == 3.5f);
    CHECK(memcmp(decoded.body.snapshot.spirit_vfx[0].position,
        valid.position, sizeof(valid.position)) == 0);
    CHECK(memcmp(decoded.body.snapshot.spirit_vfx[0].rotation_xyzw,
        valid.rotation_xyzw, sizeof(valid.rotation_xyzw)) == 0);
    CHECK(memcmp(decoded.body.snapshot.spirit_vfx[0].scale,
        valid.scale, sizeof(valid.scale)) == 0);

    entries_offset = size - 1u - SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY * 57u;
    bytes[entries_offset + 57u] = 1u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[entries_offset + 57u] = 0u;
    /* Even a nonzero sign bit in an inactive floating field is noncanonical. */
    bytes[entries_offset + 57u + 15u] = 0x80u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[entries_offset + 57u + 15u] = 0u;
    bytes[entries_offset - 2u] = 0u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[entries_offset - 2u] = 1u;
    bytes[entries_offset - 1u] = 9u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[entries_offset - 1u] = 1u;
    bytes[4] = 22u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[4] = 23u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[4] = 24u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[4] = 25u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[4] = 26u;
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size - 1u, &decoded));
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size + 1u, &decoded));

#define REJECT_VFX_CHANGE(statement) do { \
    snapshot->spirit_vfx[0] = valid; \
    statement; \
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source)); \
} while (0)
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].instance_sequence = 0u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].skill_sequence = 0u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].skill_sequence = 8u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].skill_sequence = 0x8007u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].kind = 0u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].kind =
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].phase_valid = 2u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].phase_valid = 0u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].phase = NAN);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].phase = INFINITY);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].phase = -1.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].phase = 1000001.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].emitted_host_tick = 101u);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].emitted_host_tick =
        snapshot->host_tick + UINT32_C(0x80000000));
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].position[0] = NAN);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].position[2] = 1000000.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].rotation_xyzw[1] = INFINITY);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].rotation_xyzw[3] = 0.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].rotation_xyzw[3] = 2.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].scale[0] = 0.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].scale[1] = -1.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].scale[2] = 1001.0f);
    REJECT_VFX_CHANGE(snapshot->spirit_vfx[0].scale[2] = NAN);
#undef REJECT_VFX_CHANGE
    snapshot->spirit_vfx[0] = valid;
    snapshot->spirit_vfx[0].phase_valid = 0u;
    snapshot->spirit_vfx[0].phase = 0.0f;
    snapshot->spirit_vfx[0].rotation_xyzw[3] = -1.0f;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    snapshot->spirit_vfx[0] = valid;
    snapshot->spirit_vfx_count = 2u;
    snapshot->spirit_vfx[1] = valid;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    snapshot->spirit_vfx_count = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY;
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY; ++index) {
        snapshot->spirit_vfx[index] = make_spirit_vfx(100u + index);
    }
    for (index = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_INITIATE;
         index <= SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST; ++index) {
        snapshot->spirit_vfx[0].kind = (uint8_t)index;
        snapshot->spirit_vfx[0].owner_actor_type = index == SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST ?
            SUDEKIMP_LAN_ARENA_TAL_TYPE : 0u;
        snapshot->spirit_vfx[0].skill_sequence = index == SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST ? 0u : 7u;
        CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
        CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
        CHECK(decoded.body.snapshot.spirit_vfx_count == 8u);
    }
    snapshot->spirit_vfx_count = 9u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    snapshot->spirit_vfx_count = 1u;
    memset(snapshot->spirit_vfx, 0, sizeof(snapshot->spirit_vfx));
    snapshot->spirit_vfx[0] = valid;
    snapshot->host_tick = 5u;
    snapshot->tal.skill_sequence = 1u;
    snapshot->spirit_vfx[0].skill_sequence = UINT16_MAX;
    snapshot->spirit_vfx[0].emitted_host_tick = UINT32_MAX - 4u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    snapshot->spirit_vfx_count = 0u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    memset(snapshot->spirit_vfx, 0, sizeof(snapshot->spirit_vfx));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_vfx_observed == 1u);
    CHECK(decoded.body.snapshot.spirit_vfx_count == 0u);
    snapshot->spirit_vfx_observed = 0u;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_vfx_observed == 0u);
    snapshot->spirit_vfx_observed = 2u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
}

static void test_spirit_vfx_generic_initiate_wire(void) {
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(94u);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaSnapshot *snapshot = &source.body.snapshot;
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    size_t entry_offset;

    CHECK(SUDEKIMP_LAN_ARENA_SPIRIT_VFX_RETURN == 11u);
    CHECK(SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE == 12u);
    snapshot->host_tick = 100u;
    snapshot->tal.skill_sequence = 7u;
    snapshot->tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot->spirit_vfx_observed = 1u;
    snapshot->spirit_vfx_count = 1u;
    snapshot->spirit_vfx[0] = make_spirit_vfx(101u);
    snapshot->spirit_vfx[0].kind =
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE;
    CHECK(SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 896u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_vfx[0].kind ==
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE);
    CHECK(memcmp(&decoded.body.snapshot.spirit_vfx[0], &snapshot->spirit_vfx[0],
        sizeof(snapshot->spirit_vfx[0])) == 0);

    /* The extension keeps the 56-byte record layout and its strict next-kind
     * rejection on both encoding and decoding; no archive identity is sent. */
    entry_offset = size - 1u - SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY * 57u;
    CHECK(bytes[entry_offset + 6u] == 12u);
    bytes[entry_offset + 6u] = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[entry_offset + 6u] = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE;
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    snapshot->spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u;
    CHECK(!SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
}

static void test_spirit_vfx_tal_strike_hit_wire(void) {
    SudekiMpLanArenaPacket source = make_minimal_snapshot_packet(95u);
    SudekiMpLanArenaPacket decoded;
    SudekiMpLanArenaSnapshot *snapshot = &source.body.snapshot;
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    size_t entry_offset;

    CHECK(SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_HIT == 13u);
    CHECK(SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST ==
        SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST);
    snapshot->host_tick = 100u;
    snapshot->tal.skill_sequence = 7u;
    snapshot->tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot->spirit_vfx_observed = 1u;
    snapshot->spirit_vfx_count = 2u;
    snapshot->spirit_vfx[0] = make_spirit_vfx(102u);
    snapshot->spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_HIT;
    snapshot->spirit_vfx[1] = make_spirit_vfx(103u);
    snapshot->spirit_vfx[1].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE;
    CHECK(SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    CHECK(size == 896u);
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.spirit_vfx_count == 2u);
    CHECK(decoded.body.snapshot.spirit_vfx[0].kind ==
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_HIT);
    CHECK(decoded.body.snapshot.spirit_vfx[1].kind ==
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE);
    CHECK(memcmp(decoded.body.snapshot.spirit_vfx, snapshot->spirit_vfx,
        sizeof(snapshot->spirit_vfx)) == 0);

    entry_offset = size - 1u - SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY * 57u;
    CHECK(bytes[entry_offset + 6u] == 13u);
    CHECK(bytes[entry_offset + 57u + 6u] == 12u);
    bytes[entry_offset + 6u] = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    bytes[entry_offset + 6u] = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_HIT;
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    snapshot->spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u;
    CHECK(!SudekiMpLanArenaSpiritVfxRosterValid(snapshot));
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

static void test_training_weapon_request_and_snapshot(void) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size;
    SudekiMpLanArenaPacket source;
    SudekiMpLanArenaPacket decoded;
    unsigned int slot;
    memset(&source, 0, sizeof(source));
    source.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    source.sequence = source.body.input.sequence = 1u;
    source.session_token = 42u;
    source.body.input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.input.kit_action = SUDEKIMP_LAN_ARENA_KIT_WEAPON;
    for (slot = 0; slot < 12u; ++slot) {
        source.body.input.kit_slot = (uint8_t)slot;
        CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
        CHECK(size == 51u);
        CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
        CHECK(decoded.body.input.kit_action == SUDEKIMP_LAN_ARENA_KIT_WEAPON);
        CHECK(decoded.body.input.kit_slot == slot);
    }
    source.body.input.kit_slot = 12u;
    CHECK(!SudekiMpLanArenaInputValid(&source.body.input));
    source.body.input.kit_slot = 0u;
    source.body.input.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    CHECK(!SudekiMpLanArenaInputValid(&source.body.input));
    source.body.input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    source.body.input.weak_attack_held = 1u;
    CHECK(!SudekiMpLanArenaInputValid(&source.body.input));
    source.body.input.weak_attack_held = 0u;
    source.body.input.skill_pressed = 1u;
    CHECK(!SudekiMpLanArenaInputValid(&source.body.input));
    source.body.input.skill_pressed = 0u;
    source.body.input.kit_action = SUDEKIMP_LAN_ARENA_KIT_SPIRIT;
    source.body.input.kit_slot = 1u;
    CHECK(!SudekiMpLanArenaInputValid(&source.body.input));
    source.body.input.kit_action = SUDEKIMP_LAN_ARENA_KIT_NONE;
    CHECK(!SudekiMpLanArenaInputValid(&source.body.input));
    /* Decoder rejects an otherwise authentic input with a forged family. */
    bytes[49] = SUDEKIMP_LAN_ARENA_KIT_SPIRIT;
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));

    source = make_minimal_snapshot_packet(1u);
    for (slot = 0; slot <= 12u; ++slot) {
        source.body.snapshot.ailish.weapon_slot_plus_one = (uint8_t)slot;
        CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &source));
        CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
        CHECK(decoded.body.snapshot.ailish.weapon_slot_plus_one == slot);
    }
    source.body.snapshot.ailish.weapon_slot_plus_one = 13u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
    source.body.snapshot.ailish.weapon_slot_plus_one = 0u;
    source.body.snapshot.tal.weapon_slot_plus_one = 1u;
    CHECK(!SudekiMpLanArenaEncodePacket(bytes, &size, &source));
}

static void test_directional_locomotion_wire(void) {
    SudekiMpLanArenaPacket packet = make_minimal_snapshot_packet(100u), decoded;
    SudekiMpLanArenaLocomotion *motion = &packet.body.snapshot.ailish.locomotion;
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    /* Header + fixed snapshot prefix + two unchanged 168-byte actors. */
    const size_t offset = 20u + 14u + 2u * 168u;
    const int selectors[] = {0,20,22,23,66,67,71,69,72,70};
    const unsigned int ids[] = {0,2,6,7,8,9,10,11,12,13};
    const uint8_t states[] = {0,1,64,65,128,192};
    unsigned int clip, i;
    CHECK(SudekiMpLanArenaLocomotionValid(motion));
    CHECK(SudekiMpLanArenaLocomotionClip(75) == -1);
    CHECK(SudekiMpLanArenaLocomotionSelector(10) == -1);
    motion->valid = 1u;
    motion->sequence = UINT16_MAX;
    motion->blend[0] = 0.25f;
    motion->blend[1] = 0.5f;
    motion->blend[2] = 1.0f;
    for (clip = 1u; clip < 10u; ++clip) {
        CHECK(SudekiMpLanArenaLocomotionClip(selectors[clip]) == (int)clip);
        CHECK(SudekiMpLanArenaLocomotionSelector(clip) == selectors[clip]);
        CHECK(SudekiMpLanArenaLocomotionAnimationId(clip) == ids[clip]);
        for (i = 0u; i < 4u; ++i) {
            motion->clip[i] = (uint8_t)clip;
            motion->state[i] = states[(clip + i) % 6u];
            motion->rate[i] = 30.681f + (float)i;
            motion->time[i] = 17.133f + (float)i;
        }
        CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &packet));
        CHECK(size == 896u);
        CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
        CHECK(decoded.body.snapshot.ailish.locomotion.sequence == UINT16_MAX);
        for (i = 0u; i < 4u; ++i) {
            const SudekiMpLanArenaLocomotion *result = &decoded.body.snapshot.ailish.locomotion;
            CHECK(result->clip[i] == clip && result->state[i] == motion->state[i]);
            CHECK(fabsf(result->rate[i] - motion->rate[i]) <= 1.0f / 512.0f);
            CHECK(fabsf(result->time[i] - motion->time[i]) <= 1.0f / 32.0f);
        }
        CHECK(fabsf(decoded.body.snapshot.ailish.locomotion.blend[1] - 0.5f) < 0.002f);
    }
    motion->rate[0] = 255.99609375f;
    motion->time[0] = 4095.9375f;
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &packet));
    CHECK(SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(decoded.body.snapshot.ailish.locomotion.rate[0] == motion->rate[0]);
    CHECK(decoded.body.snapshot.ailish.locomotion.time[0] == motion->time[0]);
    bytes[offset + 3u] |= 0x0fu; /* No arbitrary resource selector. */
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &packet));
    bytes[offset + 5u] = 6u; /* Unknown native channel state. */
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &packet));
    bytes[offset + 6u] |= 0x80u; /* Reserved state bits. */
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    CHECK(SudekiMpLanArenaEncodePacket(bytes, &size, &packet));
    bytes[offset] = 0u; /* Invalid optional block must be all zero. */
    CHECK(!SudekiMpLanArenaDecodePacket(bytes, size, &decoded));
    motion->sequence = 0u;
    CHECK(!SudekiMpLanArenaSnapshotValid(&packet.body.snapshot));
    motion->sequence = 1u;
    packet.body.snapshot.tal.locomotion = *motion;
    CHECK(!SudekiMpLanArenaSnapshotValid(&packet.body.snapshot));
    memset(&packet.body.snapshot.tal.locomotion, 0, sizeof(*motion));
    packet.body.snapshot.combat_enabled = 0u;
    CHECK(!SudekiMpLanArenaSnapshotValid(&packet.body.snapshot));
    packet.body.snapshot.combat_enabled = 1u;
    packet.body.snapshot.ailish.skill_sequence = 1u;
    packet.body.snapshot.ailish.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    packet.body.snapshot.ailish.skill_active = 1u;
    CHECK(!SudekiMpLanArenaSnapshotValid(&packet.body.snapshot));
    packet.body.snapshot.ailish.skill_active = 0u;
    CHECK(SudekiMpLanArenaSnapshotValid(&packet.body.snapshot));
    motion->rate[0] = NAN;
    CHECK(!SudekiMpLanArenaLocomotionValid(motion));
    motion->rate[0] = 256.0f;
    CHECK(!SudekiMpLanArenaLocomotionValid(motion));
    motion->rate[0] = 24.0f;
    motion->time[0] = -1.0f;
    CHECK(!SudekiMpLanArenaLocomotionValid(motion));
    motion->time[0] = 1.0f;
    motion->blend[0] = 1.1f;
    CHECK(!SudekiMpLanArenaLocomotionValid(motion));
    motion->blend[0] = 0.5f;
    motion->clip[0] = 0u; /* Empty channels cannot carry a stale clock. */
    CHECK(!SudekiMpLanArenaLocomotionValid(motion));
}

int main(void) {
    test_directional_locomotion_wire();
    test_training_weapon_request_and_snapshot();
    test_hello_round_trip_and_rejection();
    test_input_snapshot_and_malformed_lengths();
    test_character_presentation_optional_sidecar();
    test_spirit_presentation_wire_lifecycle();
    test_spirit_audio_semantic_journal();
    test_spirit_vfx_roster_wire();
    test_status_visual_owner_wire();
    test_spirit_vfx_generic_initiate_wire();
    test_spirit_vfx_tal_strike_hit_wire();
    test_connection_sequence_timeout_and_authority();
    test_keepalive_round_trip();
    test_direct_endpoint_parser();
    if (failures != 0) {
        return 1;
    }
    puts("lan arena protocol tests passed");
    return 0;
}
