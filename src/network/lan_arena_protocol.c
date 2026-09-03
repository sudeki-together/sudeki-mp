#include "network/lan_arena_protocol.h"

#include <math.h>
#include <string.h>

#define LAN_HEADER_SIZE 20u
#define LAN_HELLO_SIZE 53u
#define LAN_INPUT_SIZE 27u
#define LAN_ACTION_EVENT_SIZE 7u
#define LAN_ACTOR_ACTION_HISTORY_OFFSET 47u
#define LAN_ACTOR_SIZE (LAN_ACTOR_ACTION_HISTORY_OFFSET + \
    (SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY * LAN_ACTION_EVENT_SIZE))
#define LAN_ENEMY_SIZE 21u
#define LAN_SNAPSHOT_ACTORS_OFFSET 14u
#define LAN_SNAPSHOT_FIXED_SIZE (LAN_SNAPSHOT_ACTORS_OFFSET + (2u * LAN_ACTOR_SIZE) + 1u)

static const uint8_t lan_magic[4] = {'S', 'M', 'P', 'N'};

static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value & 0xffu);
    output[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void write_u32(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value & 0xffu);
    output[1] = (uint8_t)((value >> 8) & 0xffu);
    output[2] = (uint8_t)((value >> 16) & 0xffu);
    output[3] = (uint8_t)((value >> 24) & 0xffu);
}

static void write_u64(uint8_t *output, uint64_t value) {
    write_u32(output, (uint32_t)(value & 0xffffffffu));
    write_u32(output + 4u, (uint32_t)(value >> 32));
}

static uint16_t read_u16(const uint8_t *input) {
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint32_t read_u32(const uint8_t *input) {
    return (uint32_t)input[0] |
        ((uint32_t)input[1] << 8) |
        ((uint32_t)input[2] << 16) |
        ((uint32_t)input[3] << 24);
}

static uint64_t read_u64(const uint8_t *input) {
    return (uint64_t)read_u32(input) | ((uint64_t)read_u32(input + 4u) << 32);
}

static void write_s16(uint8_t *output, int16_t value) {
    uint16_t encoded;
    memcpy(&encoded, &value, sizeof(encoded));
    write_u16(output, encoded);
}

static int16_t read_s16(const uint8_t *input) {
    uint16_t encoded = read_u16(input);
    int16_t value;
    memcpy(&value, &encoded, sizeof(value));
    return value;
}

static void write_float(uint8_t *output, float value) {
    uint32_t encoded;
    memcpy(&encoded, &value, sizeof(encoded));
    write_u32(output, encoded);
}

static float read_float(const uint8_t *input) {
    uint32_t encoded = read_u32(input);
    float value;
    memcpy(&value, &encoded, sizeof(value));
    return value;
}

static int valid_role(uint8_t role) {
    return role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
}

static int valid_simulation_node_role(uint8_t role) {
    return role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        role == SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
}

static int valid_coordinate(float value) {
    return isfinite(value) && value > -1000000.0f && value < 1000000.0f;
}

static int valid_input_aim(const SudekiMpLanArenaInput *input) {
    double x;
    double y;
    double z;
    double length_squared;
    if (input == NULL) return 0;
    x = (double)input->aim_direction_x / 32767.0;
    y = (double)input->aim_direction_y / 32767.0;
    z = (double)input->aim_direction_z / 32767.0;
    length_squared = x * x + y * y + z * z;
    /* An all-zero vector explicitly means that the client camera was not yet
     * available. Otherwise accept only a bounded near-unit direction. */
    return length_squared == 0.0 ||
        (length_squared >= 0.25 && length_squared <= 2.25);
}

int SudekiMpLanArenaInputValid(const SudekiMpLanArenaInput *input) {
    return input != NULL &&
        (input->actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE ||
         input->actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE) &&
        input->weak_attack_pressed <= 1u &&
        input->weak_attack_held <= 1u &&
        input->ranged_first_person_active <= 1u &&
        input->cleanroom_combat_test_pressed <= 1u &&
        valid_input_aim(input);
}

static int valid_actor_action_pair(
    uint8_t combat_state,
    uint8_t action_variant
) {
    switch (combat_state) {
    case SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK:
        return action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW;
    case SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK:
        return action_variant == SUDEKIMP_LAN_ARENA_ACTION_STRONG ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS ||
            action_variant ==
                SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS ||
            action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS;
    case SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK:
        return action_variant == SUDEKIMP_LAN_ARENA_ACTION_SWEEP;
    case SUDEKIMP_LAN_ARENA_COMBAT_BLOCK:
        return action_variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK;
    default:
        return action_variant == SUDEKIMP_LAN_ARENA_ACTION_NONE;
    }
}

static int action_sequence_newer(uint16_t candidate, uint16_t previous) {
    return candidate != previous &&
        (uint16_t)(candidate - previous) < 0x8000u;
}

static int valid_actor_action_history(
    const SudekiMpLanArenaActorSnapshot *actor,
    uint8_t expected_type
) {
    unsigned int index;
    if (actor == NULL ||
        actor->action_history_count >
            SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY) return 0;
    for (index = 0u; index < actor->action_history_count; ++index) {
        const SudekiMpLanArenaActionEvent *event =
            &actor->action_history[index];
        if (event->sequence == 0u ||
            event->variant < SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE ||
            event->variant > SUDEKIMP_LAN_ARENA_ACTION_MAX ||
            (expected_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE &&
             event->variant != SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE) ||
            (index != 0u && !action_sequence_newer(
                event->sequence,
                actor->action_history[index - 1u].sequence))) {
            return 0;
        }
    }
    return actor->action_history_count == 0u ||
        actor->action_history[actor->action_history_count - 1u].sequence ==
            actor->action_sequence;
}

static int valid_actor_snapshot(
    const SudekiMpLanArenaActorSnapshot *actor,
    uint8_t expected_type
) {
    float facing_length;
    if (actor == NULL || actor->actor_type != expected_type ||
        actor->native_entity_id != expected_type ||
        actor->action_variant > SUDEKIMP_LAN_ARENA_ACTION_MAX ||
        actor->action_phase_valid > 1u ||
        actor->action_retirement_valid > 1u ||
        actor->animation_state > SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO ||
        actor->combat_state > SUDEKIMP_LAN_ARENA_COMBAT_BLOCK ||
        actor->hp > SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE ||
        actor->sp > SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE ||
        !valid_coordinate(actor->x) || !valid_coordinate(actor->y) ||
        !valid_coordinate(actor->z) || !isfinite(actor->facing_x) ||
        !isfinite(actor->facing_z)) return 0;
    if ((actor->hp == 0u &&
         (actor->animation_state !=
              SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED ||
          actor->combat_state != SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED)) ||
        (actor->hp != 0u &&
         (actor->animation_state ==
              SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED ||
          actor->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED)) ||
        ((actor->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION) !=
         (actor->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK ||
          actor->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK ||
          actor->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK ||
          actor->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK)) ||
        ((actor->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION) !=
         (actor->action_variant != SUDEKIMP_LAN_ARENA_ACTION_NONE)) ||
        !valid_actor_action_pair(
            actor->combat_state, actor->action_variant) ||
        (expected_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE &&
         actor->action_variant > SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE) ||
        !valid_actor_action_history(actor, expected_type) ||
        (actor->action_phase_valid &&
         actor->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION) ||
        (!actor->action_phase_valid && actor->action_phase_q8 != 0u) ||
        (actor->action_retirement_valid &&
         (actor->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_IDLE ||
          actor->combat_state != SUDEKIMP_LAN_ARENA_COMBAT_IDLE ||
          actor->action_variant != SUDEKIMP_LAN_ARENA_ACTION_NONE ||
          actor->action_sequence == 0u)) ||
        (!actor->action_retirement_valid &&
         (actor->action_terminal_phase_q8 != 0u ||
          actor->idle_entry_phase_q8 != 0u))) {
        return 0;
    }
    facing_length = sqrtf(actor->facing_x * actor->facing_x +
        actor->facing_z * actor->facing_z);
    return isfinite(facing_length) && facing_length >= 0.5f &&
        facing_length <= 1.5f;
}

int SudekiMpLanArenaSnapshotValid(
    const SudekiMpLanArenaSnapshot *snapshot
) {
    unsigned int index;
    unsigned int other;
    if (snapshot == NULL ||
        snapshot->match_state > SUDEKIMP_LAN_ARENA_MATCH_ENDED ||
        snapshot->combat_enabled > 1u ||
        (snapshot->match_state != SUDEKIMP_LAN_ARENA_MATCH_ACTIVE &&
         snapshot->combat_enabled != 0u) ||
        snapshot->enemy_count > 1u ||
        !valid_actor_snapshot(&snapshot->tal, SUDEKIMP_LAN_ARENA_TAL_TYPE) ||
        !valid_actor_snapshot(
            &snapshot->ailish, SUDEKIMP_LAN_ARENA_AILISH_TYPE)) return 0;
    for (index = 0u; index < snapshot->enemy_count; ++index) {
        const SudekiMpLanArenaEnemySnapshot *enemy = &snapshot->enemies[index];
        if (enemy->native_entity_id !=
                SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID ||
            enemy->combat_state > SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED ||
            enemy->hp > SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE ||
            !valid_coordinate(enemy->x) || !valid_coordinate(enemy->y) ||
            !valid_coordinate(enemy->z) ||
            (enemy->hp == 0u) !=
                (enemy->combat_state ==
                    SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED)) return 0;
        for (other = 0u; other < index; ++other) {
            if (snapshot->enemies[other].native_entity_id ==
                enemy->native_entity_id) return 0;
        }
    }
    return 1;
}

static int write_actor(uint8_t *output, const SudekiMpLanArenaActorSnapshot *actor) {
    unsigned int index;
    if (output == NULL || actor == NULL ||
        (actor->actor_type != SUDEKIMP_LAN_ARENA_TAL_TYPE &&
         actor->actor_type != SUDEKIMP_LAN_ARENA_AILISH_TYPE)) {
        return 0;
    }
    output[0] = actor->actor_type;
    output[1] = actor->animation_state;
    output[2] = actor->combat_state;
    output[3] = actor->action_variant;
    write_u32(output + 4u, actor->native_entity_id);
    write_float(output + 8u, actor->x);
    write_float(output + 12u, actor->y);
    write_float(output + 16u, actor->z);
    write_float(output + 20u, actor->facing_x);
    write_float(output + 24u, actor->facing_z);
    write_u32(output + 28u, actor->hp);
    write_u32(output + 32u, actor->sp);
    write_u16(output + 36u, actor->action_sequence);
    write_u16(output + 38u, actor->action_phase_q8);
    output[40] = actor->action_phase_valid;
    write_u16(output + 41u, actor->action_terminal_phase_q8);
    write_u16(output + 43u, actor->idle_entry_phase_q8);
    output[45] = actor->action_retirement_valid;
    output[46] = actor->action_history_count;
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY;
         ++index) {
        uint8_t *entry = output + LAN_ACTOR_ACTION_HISTORY_OFFSET +
            index * LAN_ACTION_EVENT_SIZE;
        if (index < actor->action_history_count) {
            write_u16(entry, actor->action_history[index].sequence);
            entry[2] = actor->action_history[index].variant;
            write_u32(entry + 3u, actor->action_history[index].host_tick);
        } else {
            memset(entry, 0, LAN_ACTION_EVENT_SIZE);
        }
    }
    return 1;
}

static int read_actor(const uint8_t *input, SudekiMpLanArenaActorSnapshot *actor) {
    unsigned int index;
    if (input == NULL || actor == NULL ||
        (input[0] != SUDEKIMP_LAN_ARENA_TAL_TYPE &&
         input[0] != SUDEKIMP_LAN_ARENA_AILISH_TYPE)) {
        return 0;
    }
    actor->actor_type = input[0];
    actor->animation_state = input[1];
    actor->combat_state = input[2];
    actor->action_variant = input[3];
    actor->native_entity_id = read_u32(input + 4u);
    actor->x = read_float(input + 8u);
    actor->y = read_float(input + 12u);
    actor->z = read_float(input + 16u);
    actor->facing_x = read_float(input + 20u);
    actor->facing_z = read_float(input + 24u);
    actor->hp = read_u32(input + 28u);
    actor->sp = read_u32(input + 32u);
    actor->action_sequence = read_u16(input + 36u);
    actor->action_phase_q8 = read_u16(input + 38u);
    actor->action_phase_valid = input[40];
    actor->action_terminal_phase_q8 = read_u16(input + 41u);
    actor->idle_entry_phase_q8 = read_u16(input + 43u);
    actor->action_retirement_valid = input[45];
    actor->action_history_count = input[46];
    if (actor->action_history_count >
            SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY) return 0;
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY;
         ++index) {
        const uint8_t *entry = input + LAN_ACTOR_ACTION_HISTORY_OFFSET +
            index * LAN_ACTION_EVENT_SIZE;
        actor->action_history[index].sequence = read_u16(entry);
        actor->action_history[index].variant = entry[2];
        actor->action_history[index].host_tick = read_u32(entry + 3u);
    }
    return 1;
}

static int encode_payload(uint8_t *output, size_t *size, const SudekiMpLanArenaPacket *packet) {
    size_t i;
    if (output == NULL || size == NULL || packet == NULL) {
        return 0;
    }
    switch (packet->type) {
        case SUDEKIMP_LAN_ARENA_PACKET_HELLO:
        case SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK:
            if (!valid_role(packet->body.hello.role) ||
                !valid_simulation_node_role(
                    packet->body.hello.simulation_node_role) ||
                packet->body.hello.map_id != SUDEKIMP_LAN_ARENA_MAP_CLEANROOM ||
                packet->body.hello.tal_type != SUDEKIMP_LAN_ARENA_TAL_TYPE ||
                packet->body.hello.ailish_type != SUDEKIMP_LAN_ARENA_AILISH_TYPE) {
                return 0;
            }
            write_u32(output, packet->body.hello.sequence);
            write_u32(output + 4u, packet->body.hello.build_id);
            memcpy(output + 8u, packet->body.hello.game_hash, SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE);
            output[40] = packet->body.hello.map_id;
            output[41] = packet->body.hello.role;
            output[42] = packet->body.hello.simulation_node_role;
            output[43] = packet->body.hello.tal_type;
            output[44] = packet->body.hello.ailish_type;
            write_u64(output + 45u, packet->body.hello.session_token);
            *size = LAN_HELLO_SIZE;
            return 1;
        case SUDEKIMP_LAN_ARENA_PACKET_REJECT:
            if (packet->body.reject_reason == SUDEKIMP_LAN_ARENA_REJECT_NONE ||
                packet->body.reject_reason > SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY) {
                return 0;
            }
            write_u32(output, (uint32_t)packet->body.reject_reason);
            *size = 4u;
            return 1;
        case SUDEKIMP_LAN_ARENA_PACKET_INPUT:
            if (!SudekiMpLanArenaInputValid(&packet->body.input)) {
                return 0;
            }
            write_u32(output, packet->body.input.sequence);
            write_u32(output + 4u, packet->body.input.acknowledged_snapshot);
            write_u32(output + 8u, packet->body.input.client_tick);
            output[12] = packet->body.input.actor_type;
            write_s16(output + 13u, packet->body.input.world_direction_x);
            write_s16(output + 15u, packet->body.input.world_direction_z);
            write_s16(output + 17u, packet->body.input.aim_direction_x);
            write_s16(output + 19u, packet->body.input.aim_direction_y);
            write_s16(output + 21u, packet->body.input.aim_direction_z);
            output[23] = packet->body.input.weak_attack_pressed;
            output[24] = packet->body.input.weak_attack_held;
            output[25] = packet->body.input.ranged_first_person_active;
            output[26] = packet->body.input.cleanroom_combat_test_pressed;
            *size = LAN_INPUT_SIZE;
            return 1;
        case SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT:
            if (!SudekiMpLanArenaSnapshotValid(&packet->body.snapshot) ||
                !write_actor(output + LAN_SNAPSHOT_ACTORS_OFFSET,
                    &packet->body.snapshot.tal) ||
                !write_actor(output + LAN_SNAPSHOT_ACTORS_OFFSET + LAN_ACTOR_SIZE,
                    &packet->body.snapshot.ailish) ||
                packet->body.snapshot.tal.actor_type != SUDEKIMP_LAN_ARENA_TAL_TYPE ||
                packet->body.snapshot.ailish.actor_type != SUDEKIMP_LAN_ARENA_AILISH_TYPE) {
                return 0;
            }
            write_u32(output, packet->body.snapshot.sequence);
            write_u32(output + 4u, packet->body.snapshot.acknowledged_input);
            write_u32(output + 8u, packet->body.snapshot.host_tick);
            output[12] = packet->body.snapshot.match_state;
            output[13] = packet->body.snapshot.combat_enabled;
            output[LAN_SNAPSHOT_ACTORS_OFFSET + (2u * LAN_ACTOR_SIZE)] =
                packet->body.snapshot.enemy_count;
            for (i = 0u; i < packet->body.snapshot.enemy_count; ++i) {
                const SudekiMpLanArenaEnemySnapshot *enemy = &packet->body.snapshot.enemies[i];
                uint8_t *entry = output + LAN_SNAPSHOT_FIXED_SIZE + (i * LAN_ENEMY_SIZE);
                write_u32(entry, enemy->native_entity_id);
                write_float(entry + 4u, enemy->x);
                write_float(entry + 8u, enemy->y);
                write_float(entry + 12u, enemy->z);
                write_u32(entry + 16u, enemy->hp);
                entry[20] = enemy->combat_state;
            }
            *size = LAN_SNAPSHOT_FIXED_SIZE + (packet->body.snapshot.enemy_count * LAN_ENEMY_SIZE);
            return 1;
        case SUDEKIMP_LAN_ARENA_PACKET_END:
        case SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE:
            *size = 0u;
            return 1;
        default:
            return 0;
    }
}

int SudekiMpLanArenaEncodePacket(
    uint8_t output[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE],
    size_t *output_size,
    const SudekiMpLanArenaPacket *packet
) {
    size_t payload_size;
    if (output == NULL || output_size == NULL || packet == NULL ||
        packet->type == SUDEKIMP_LAN_ARENA_PACKET_INVALID ||
        packet->type > SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE ||
        packet->session_token == 0u ||
        !encode_payload(output + LAN_HEADER_SIZE, &payload_size, packet) ||
        LAN_HEADER_SIZE + payload_size > SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE) {
        return 0;
    }
    memcpy(output, lan_magic, sizeof(lan_magic));
    write_u16(output + 4u, SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION);
    output[6] = (uint8_t)packet->type;
    output[7] = 0u;
    write_u32(output + 8u, packet->sequence);
    write_u64(output + 12u, packet->session_token);
    *output_size = LAN_HEADER_SIZE + payload_size;
    return 1;
}

int SudekiMpLanArenaDecodePacket(
    const uint8_t *packet_bytes,
    size_t packet_size,
    SudekiMpLanArenaPacket *packet
) {
    size_t payload_size;
    size_t i;
    const uint8_t *payload;
    if (packet_bytes == NULL || packet == NULL || packet_size < LAN_HEADER_SIZE ||
        packet_size > SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE ||
        memcmp(packet_bytes, lan_magic, sizeof(lan_magic)) != 0 ||
        read_u16(packet_bytes + 4u) != SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION ||
        packet_bytes[7] != 0u || packet_bytes[6] == SUDEKIMP_LAN_ARENA_PACKET_INVALID ||
        packet_bytes[6] > SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE || read_u64(packet_bytes + 12u) == 0u) {
        return 0;
    }
    memset(packet, 0, sizeof(*packet));
    packet->type = (SudekiMpLanArenaPacketType)packet_bytes[6];
    packet->sequence = read_u32(packet_bytes + 8u);
    packet->session_token = read_u64(packet_bytes + 12u);
    payload = packet_bytes + LAN_HEADER_SIZE;
    payload_size = packet_size - LAN_HEADER_SIZE;
    switch (packet->type) {
        case SUDEKIMP_LAN_ARENA_PACKET_HELLO:
        case SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK:
            if (payload_size != LAN_HELLO_SIZE || !valid_role(payload[41]) ||
                !valid_simulation_node_role(payload[42]) ||
                payload[40] != SUDEKIMP_LAN_ARENA_MAP_CLEANROOM ||
                payload[43] != SUDEKIMP_LAN_ARENA_TAL_TYPE ||
                payload[44] != SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
                read_u64(payload + 45u) != packet->session_token) {
                return 0;
            }
            packet->body.hello.sequence = read_u32(payload);
            packet->body.hello.build_id = read_u32(payload + 4u);
            memcpy(packet->body.hello.game_hash, payload + 8u, SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE);
            packet->body.hello.map_id = payload[40];
            packet->body.hello.role = payload[41];
            packet->body.hello.simulation_node_role = payload[42];
            packet->body.hello.tal_type = payload[43];
            packet->body.hello.ailish_type = payload[44];
            packet->body.hello.session_token = packet->session_token;
            return packet->body.hello.sequence == packet->sequence;
        case SUDEKIMP_LAN_ARENA_PACKET_REJECT:
            if (payload_size != 4u || read_u32(payload) == SUDEKIMP_LAN_ARENA_REJECT_NONE ||
                read_u32(payload) > SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY) {
                return 0;
            }
            packet->body.reject_reason = (SudekiMpLanArenaRejectReason)read_u32(payload);
            return 1;
        case SUDEKIMP_LAN_ARENA_PACKET_INPUT:
            if (payload_size != LAN_INPUT_SIZE || payload[23] > 1u ||
                payload[24] > 1u || payload[25] > 1u || payload[26] > 1u) {
                return 0;
            }
            packet->body.input.sequence = read_u32(payload);
            packet->body.input.acknowledged_snapshot = read_u32(payload + 4u);
            packet->body.input.client_tick = read_u32(payload + 8u);
            packet->body.input.actor_type = payload[12];
            packet->body.input.world_direction_x = read_s16(payload + 13u);
            packet->body.input.world_direction_z = read_s16(payload + 15u);
            packet->body.input.aim_direction_x = read_s16(payload + 17u);
            packet->body.input.aim_direction_y = read_s16(payload + 19u);
            packet->body.input.aim_direction_z = read_s16(payload + 21u);
            packet->body.input.weak_attack_pressed = payload[23];
            packet->body.input.weak_attack_held = payload[24];
            packet->body.input.ranged_first_person_active = payload[25];
            packet->body.input.cleanroom_combat_test_pressed = payload[26];
            return packet->body.input.sequence == packet->sequence &&
                SudekiMpLanArenaInputValid(&packet->body.input);
        case SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT:
            if (payload_size < LAN_SNAPSHOT_FIXED_SIZE ||
                payload[13] > 1u ||
                !read_actor(payload + LAN_SNAPSHOT_ACTORS_OFFSET,
                    &packet->body.snapshot.tal) ||
                !read_actor(payload + LAN_SNAPSHOT_ACTORS_OFFSET + LAN_ACTOR_SIZE,
                    &packet->body.snapshot.ailish) ||
                packet->body.snapshot.tal.actor_type != SUDEKIMP_LAN_ARENA_TAL_TYPE ||
                packet->body.snapshot.ailish.actor_type != SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
                payload[LAN_SNAPSHOT_ACTORS_OFFSET + (2u * LAN_ACTOR_SIZE)] >
                    SUDEKIMP_LAN_ARENA_MAX_ENEMIES ||
                payload_size != LAN_SNAPSHOT_FIXED_SIZE +
                    ((size_t)payload[LAN_SNAPSHOT_ACTORS_OFFSET +
                        (2u * LAN_ACTOR_SIZE)] * LAN_ENEMY_SIZE)) {
                return 0;
            }
            packet->body.snapshot.sequence = read_u32(payload);
            packet->body.snapshot.acknowledged_input = read_u32(payload + 4u);
            packet->body.snapshot.host_tick = read_u32(payload + 8u);
            packet->body.snapshot.match_state = payload[12];
            packet->body.snapshot.combat_enabled = payload[13];
            packet->body.snapshot.enemy_count =
                payload[LAN_SNAPSHOT_ACTORS_OFFSET + (2u * LAN_ACTOR_SIZE)];
            for (i = 0u; i < packet->body.snapshot.enemy_count; ++i) {
                SudekiMpLanArenaEnemySnapshot *enemy = &packet->body.snapshot.enemies[i];
                const uint8_t *entry = payload + LAN_SNAPSHOT_FIXED_SIZE + (i * LAN_ENEMY_SIZE);
                enemy->native_entity_id = read_u32(entry);
                enemy->x = read_float(entry + 4u);
                enemy->y = read_float(entry + 8u);
                enemy->z = read_float(entry + 12u);
                enemy->hp = read_u32(entry + 16u);
                enemy->combat_state = entry[20];
            }
            return packet->body.snapshot.sequence == packet->sequence &&
                SudekiMpLanArenaSnapshotValid(&packet->body.snapshot);
        case SUDEKIMP_LAN_ARENA_PACKET_END:
        case SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE:
            return payload_size == 0u;
        default:
            return 0;
    }
}

int SudekiMpLanArenaHandshakeValid(
    const SudekiMpLanArenaHello *hello,
    const SudekiMpLanArenaHandshakeExpectation *expectation,
    SudekiMpLanArenaRejectReason *reason
) {
    SudekiMpLanArenaRejectReason result = SUDEKIMP_LAN_ARENA_REJECT_NONE;
    if (hello == NULL || expectation == NULL || expectation->game_hash == NULL) {
        result = SUDEKIMP_LAN_ARENA_REJECT_MALFORMED;
    } else if (hello->build_id != expectation->build_id) {
        result = SUDEKIMP_LAN_ARENA_REJECT_BUILD;
    } else if (memcmp(hello->game_hash, expectation->game_hash, SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE) != 0) {
        result = SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH;
    } else if (hello->map_id != expectation->map_id) {
        result = SUDEKIMP_LAN_ARENA_REJECT_MAP;
    } else if (hello->role != expectation->expected_sender_role || !valid_role(hello->role) ||
               hello->tal_type != expectation->tal_type || hello->ailish_type != expectation->ailish_type) {
        result = SUDEKIMP_LAN_ARENA_REJECT_ROLE;
    } else if (hello->simulation_node_role !=
                   expectation->expected_sender_simulation_node_role ||
               !valid_simulation_node_role(hello->simulation_node_role)) {
        result = SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY;
    } else if (hello->session_token == 0u ||
               (expectation->expected_session_token != 0u &&
                hello->session_token != expectation->expected_session_token)) {
        result = SUDEKIMP_LAN_ARENA_REJECT_TOKEN;
    }
    if (reason != NULL) {
        *reason = result;
    }
    return result == SUDEKIMP_LAN_ARENA_REJECT_NONE;
}

int SudekiMpLanArenaSequenceNewer(uint32_t candidate, uint32_t previous) {
    return candidate != previous && (uint32_t)(candidate - previous) < 0x80000000u;
}

int SudekiMpLanArenaConnectionAcceptPacket(
    SudekiMpLanArenaConnectionState *state,
    const SudekiMpLanArenaPacket *packet,
    uint32_t now_ms,
    SudekiMpLanArenaRejectReason *reason
) {
    SudekiMpLanArenaRejectReason result = SUDEKIMP_LAN_ARENA_REJECT_NONE;
    if (state == NULL || packet == NULL || state->phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED ||
        state->session_token == 0u || packet->session_token != state->session_token) {
        result = SUDEKIMP_LAN_ARENA_REJECT_TOKEN;
    } else if (packet->type != SUDEKIMP_LAN_ARENA_PACKET_INPUT &&
               packet->type != SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT &&
               packet->type != SUDEKIMP_LAN_ARENA_PACKET_END &&
               packet->type != SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE) {
        result = SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY;
    } else if (state->sequence_initialized &&
               !SudekiMpLanArenaSequenceNewer(packet->sequence, state->last_received_sequence)) {
        result = SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE;
    }
    if (result == SUDEKIMP_LAN_ARENA_REJECT_NONE) {
        if (packet->type == SUDEKIMP_LAN_ARENA_PACKET_END) {
            state->phase = SUDEKIMP_LAN_ARENA_CONNECTION_ENDED;
        } else {
            state->last_received_sequence = packet->sequence;
            state->last_received_at_ms = now_ms;
            state->sequence_initialized = 1u;
        }
    }
    if (reason != NULL) {
        *reason = result;
    }
    return result == SUDEKIMP_LAN_ARENA_REJECT_NONE;
}

int SudekiMpLanArenaConnectionTimedOut(
    const SudekiMpLanArenaConnectionState *state,
    uint32_t now_ms,
    uint32_t timeout_ms
) {
    if (state == NULL || timeout_ms == 0u || state->phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED ||
        !state->sequence_initialized) {
        return 0;
    }
    return (uint32_t)(now_ms - state->last_received_at_ms) > timeout_ms;
}
