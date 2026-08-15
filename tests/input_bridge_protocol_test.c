#include "input/bridge_protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "input_bridge_protocol_test: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    SudekiMpInputBridgeState source;
    SudekiMpInputBridgeState decoded;
    uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];
    uint8_t corrupt[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];

    memset(&source, 0, sizeof(source));
    source.sequence = 0xfedcba98u;
    source.sender_timestamp_ms = 0x12345678u;
    source.left_x = -32768;
    source.left_y = 32767;
    source.right_x = -12345;
    source.right_y = 23456;
    source.left_trigger = 1u;
    source.right_trigger = 65535u;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A |
        SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER |
        SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT;

    if (!require(SudekiMpEncodeInputBridgePacket(packet, &source),
                 "encode failed") ||
        !require(SudekiMpDecodeInputBridgePacket(
                     packet, sizeof(packet), &decoded),
                 "decode failed") ||
        !require(memcmp(&source, &decoded, sizeof(source)) == 0,
                 "round trip mismatch")) {
        return 1;
    }

    memcpy(corrupt, packet, sizeof(corrupt));
    corrupt[0] = 'X';
    if (!require(!SudekiMpDecodeInputBridgePacket(
                     corrupt, sizeof(corrupt), &decoded),
                 "bad magic accepted") ||
        !require(!SudekiMpDecodeInputBridgePacket(
                     packet, sizeof(packet) - 1u, &decoded),
                 "bad size accepted")) {
        return 1;
    }

    puts("input bridge protocol tests passed");
    return 0;
}
