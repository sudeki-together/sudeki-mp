#include "input/bridge_protocol.h"

#include <string.h>

static const uint8_t bridge_magic[4] = {'S', 'M', 'P', 'B'};

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

static uint16_t read_u16(const uint8_t *input) {
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint32_t read_u32(const uint8_t *input) {
    return (uint32_t)input[0] |
        ((uint32_t)input[1] << 8) |
        ((uint32_t)input[2] << 16) |
        ((uint32_t)input[3] << 24);
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

int SudekiMpEncodeInputBridgePacket(
    uint8_t output[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE],
    const SudekiMpInputBridgeState *state
) {
    if (output == NULL || state == NULL) {
        return 0;
    }
    memcpy(output, bridge_magic, sizeof(bridge_magic));
    write_u16(output + 4u, SUDEKIMP_INPUT_BRIDGE_PROTOCOL_VERSION);
    write_u16(output + 6u, SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE);
    write_u32(output + 8u, state->sequence);
    write_u32(output + 12u, state->sender_timestamp_ms);
    write_s16(output + 16u, state->left_x);
    write_s16(output + 18u, state->left_y);
    write_s16(output + 20u, state->right_x);
    write_s16(output + 22u, state->right_y);
    write_u16(output + 24u, state->left_trigger);
    write_u16(output + 26u, state->right_trigger);
    write_u32(output + 28u, state->buttons);
    return 1;
}

int SudekiMpDecodeInputBridgePacket(
    const uint8_t *packet,
    size_t packet_size,
    SudekiMpInputBridgeState *state
) {
    if (packet == NULL || state == NULL ||
        packet_size != SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE ||
        memcmp(packet, bridge_magic, sizeof(bridge_magic)) != 0 ||
        read_u16(packet + 4u) != SUDEKIMP_INPUT_BRIDGE_PROTOCOL_VERSION ||
        read_u16(packet + 6u) != SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE) {
        return 0;
    }
    state->sequence = read_u32(packet + 8u);
    state->sender_timestamp_ms = read_u32(packet + 12u);
    state->left_x = read_s16(packet + 16u);
    state->left_y = read_s16(packet + 18u);
    state->right_x = read_s16(packet + 20u);
    state->right_y = read_s16(packet + 22u);
    state->left_trigger = read_u16(packet + 24u);
    state->right_trigger = read_u16(packet + 26u);
    state->buttons = read_u32(packet + 28u);
    return 1;
}
