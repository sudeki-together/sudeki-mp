#ifndef SUDEKIMP_BRIDGE_PROTOCOL_H
#define SUDEKIMP_BRIDGE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE 32u
#define SUDEKIMP_INPUT_BRIDGE_PROTOCOL_VERSION 1u
#define SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM 1u

enum SudekiMpInputBridgeButton {
    SUDEKIMP_BRIDGE_BUTTON_A = 1u << 0,
    SUDEKIMP_BRIDGE_BUTTON_B = 1u << 1,
    SUDEKIMP_BRIDGE_BUTTON_X = 1u << 2,
    SUDEKIMP_BRIDGE_BUTTON_Y = 1u << 3,
    SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER = 1u << 4,
    SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER = 1u << 5,
    SUDEKIMP_BRIDGE_BUTTON_BACK = 1u << 6,
    SUDEKIMP_BRIDGE_BUTTON_START = 1u << 7,
    SUDEKIMP_BRIDGE_BUTTON_LEFT_STICK = 1u << 8,
    SUDEKIMP_BRIDGE_BUTTON_RIGHT_STICK = 1u << 9,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_UP = 1u << 10,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN = 1u << 11,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT = 1u << 12,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT = 1u << 13
};

typedef struct SudekiMpInputBridgeState {
    uint32_t sequence;
    uint32_t sender_timestamp_ms;
    int16_t left_x;
    int16_t left_y;
    int16_t right_x;
    int16_t right_y;
    uint16_t left_trigger;
    uint16_t right_trigger;
    uint32_t buttons;
} SudekiMpInputBridgeState;

int SudekiMpEncodeInputBridgePacket(
    uint8_t output[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE],
    const SudekiMpInputBridgeState *state
);
int SudekiMpDecodeInputBridgePacket(
    const uint8_t *packet,
    size_t packet_size,
    SudekiMpInputBridgeState *state
);

#endif
