#include <winsock2.h>
#include <ws2tcpip.h>

#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"
#include "input/local_input_hub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "input_bridge_receiver_test: %s\n", message);
        return 0;
    }
    return 1;
}

static int send_packet(
    SOCKET sender,
    unsigned int port,
    const uint8_t *packet,
    int size
) {
    struct sockaddr_in destination;
    ZeroMemory(&destination, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons((u_short)port);
    return sendto(sender, (const char *)packet, size, 0,
                  (const struct sockaddr *)&destination,
                  sizeof(destination));
}

static int bridge_states_equal(
    const SudekiMpInputBridgeState *left,
    const SudekiMpInputBridgeState *right
) {
    return left->sequence == right->sequence &&
        left->sender_timestamp_ms == right->sender_timestamp_ms &&
        left->left_x == right->left_x && left->left_y == right->left_y &&
        left->right_x == right->right_x &&
        left->right_y == right->right_y &&
        left->left_trigger == right->left_trigger &&
        left->right_trigger == right->right_trigger &&
        left->buttons == right->buttons;
}

static int send_state_packet(
    SOCKET sender,
    unsigned int port,
    const SudekiMpInputBridgeState *state
) {
    uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];

    return SudekiMpEncodeInputBridgePacket(packet, state) &&
        send_packet(sender, port, packet, (int)sizeof(packet)) ==
            (int)sizeof(packet);
}

static int wait_for_raw_state(
    const SudekiMpInputBridgeState *expected,
    SudekiMpInputBridgeState *received,
    DWORD timeout_ms
) {
    DWORD deadline = GetTickCount() + timeout_ms;

    do {
        if (SudekiMpInputBridgePollRaw(received) &&
            bridge_states_equal(received, expected)) {
            return 1;
        }
        Sleep(1u);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    return 0;
}

static int raw_state_remains(
    const SudekiMpInputBridgeState *expected,
    DWORD duration_ms
) {
    SudekiMpInputBridgeState received;
    DWORD deadline = GetTickCount() + duration_ms;

    do {
        if (!SudekiMpInputBridgePollRaw(&received) ||
            !bridge_states_equal(&received, expected)) {
            return 0;
        }
        Sleep(1u);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    return 1;
}

static int run_sequence_ordering_test(void) {
    WSADATA data;
    SOCKET sender = INVALID_SOCKET;
    SudekiMpInputBridgeState source;
    SudekiMpInputBridgeState accepted;
    SudekiMpInputBridgeState received;
    unsigned int port;
    int sender_winsock_started = 0;
    int passed = 0;

    if (!require(SudekiMpInputBridgeSequenceIsNewer(101u, 100u),
                 "ordinary newer sequence was rejected") ||
        !require(!SudekiMpInputBridgeSequenceIsNewer(100u, 100u),
                 "duplicate sequence was considered newer") ||
        !require(!SudekiMpInputBridgeSequenceIsNewer(99u, 100u),
                 "older sequence was considered newer") ||
        !require(SudekiMpInputBridgeSequenceIsNewer(0u, 0xffffffffu),
                 "uint32 wrap was not considered newer") ||
        !require(!SudekiMpInputBridgeSequenceIsNewer(0xffffffffu, 0u),
                 "pre-wrap sequence was considered newer") ||
        !require(!SudekiMpInputBridgeSequenceIsNewer(
                     0x80000064u, 100u),
                 "ambiguous half-range sequence was considered newer")) {
        return 0;
    }

    if (!require(SudekiMpInputBridgeStart(0u, 100u),
                 "sequence receiver start failed")) {
        return 0;
    }
    port = SudekiMpInputBridgeBoundPort();
    if (!require(port != 0u, "sequence receiver port unavailable") ||
        !require(WSAStartup(MAKEWORD(2, 2), &data) == 0,
                 "sequence sender WSAStartup failed")) {
        goto cleanup;
    }
    sender_winsock_started = 1;
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!require(sender != INVALID_SOCKET,
                 "sequence sender socket failed")) {
        goto cleanup;
    }

    ZeroMemory(&source, sizeof(source));
    source.sequence = 100u;
    source.sender_timestamp_ms = 1000u;
    source.left_x = 100;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    if (!require(send_state_packet(sender, port, &source),
                 "initial sequence packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "initial sequence packet was not accepted")) {
        goto cleanup;
    }
    accepted = source;

    source.sender_timestamp_ms = 1001u;
    source.left_x = 101;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_B;
    if (!require(send_state_packet(sender, port, &source),
                 "duplicate sequence packet send failed") ||
        !require(raw_state_remains(&accepted, 20u),
                 "duplicate sequence replaced accepted state")) {
        goto cleanup;
    }

    source.sequence = 99u;
    source.sender_timestamp_ms = 1002u;
    source.left_x = 102;
    if (!require(send_state_packet(sender, port, &source),
                 "out-of-order packet send failed") ||
        !require(raw_state_remains(&accepted, 20u),
                 "out-of-order sequence replaced accepted state")) {
        goto cleanup;
    }

    source.sequence = 101u;
    source.sender_timestamp_ms = 1003u;
    source.left_x = 103;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_X;
    if (!require(send_state_packet(sender, port, &source),
                 "newer sequence packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "newer sequence packet was not accepted")) {
        goto cleanup;
    }
    accepted = source;

    source.sequence = 0x80000065u;
    source.sender_timestamp_ms = 1004u;
    source.left_x = 104;
    if (!require(send_state_packet(sender, port, &source),
                 "half-range packet send failed") ||
        !require(raw_state_remains(&accepted, 20u),
                 "ambiguous half-range sequence replaced accepted state")) {
        goto cleanup;
    }

    /* Leave the expired packet queued before polling.  This verifies that
     * timeout processing resets the epoch before socket draining. */
    Sleep(130u);
    ZeroMemory(&source, sizeof(source));
    source.sequence = 0xfffffffeu;
    source.sender_timestamp_ms = 2000u;
    source.left_x = 200;
    if (!require(send_state_packet(sender, port, &source),
                 "new epoch packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "first packet after timeout did not start a new epoch")) {
        goto cleanup;
    }

    source.sequence = 0xffffffffu;
    source.sender_timestamp_ms = 2001u;
    source.left_x = 201;
    if (!require(send_state_packet(sender, port, &source),
                 "pre-wrap packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "pre-wrap packet was not accepted")) {
        goto cleanup;
    }

    source.sequence = 0u;
    source.sender_timestamp_ms = 2002u;
    source.left_x = 202;
    if (!require(send_state_packet(sender, port, &source),
                 "wrapped packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "wrapped packet was not accepted")) {
        goto cleanup;
    }

    source.sequence = 1u;
    source.sender_timestamp_ms = 2003u;
    source.left_x = 203;
    if (!require(send_state_packet(sender, port, &source),
                 "post-wrap packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "post-wrap packet was not accepted")) {
        goto cleanup;
    }
    accepted = source;

    source.sequence = 0xffffffffu;
    source.sender_timestamp_ms = 2004u;
    source.left_x = 204;
    if (!require(send_state_packet(sender, port, &source),
                 "post-wrap stale packet send failed") ||
        !require(raw_state_remains(&accepted, 20u),
                 "post-wrap stale sequence replaced accepted state")) {
        goto cleanup;
    }

    Sleep(130u);
    source = accepted;
    source.sender_timestamp_ms = 3000u;
    source.left_x = 300;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_Y;
    if (!require(send_state_packet(sender, port, &source),
                 "duplicate-valued new epoch packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "timeout did not reset the sequence epoch")) {
        goto cleanup;
    }

    passed = 1;

cleanup:
    if (sender != INVALID_SOCKET) {
        closesocket(sender);
    }
    if (sender_winsock_started) {
        WSACleanup();
    }
    SudekiMpInputBridgeStop();
    return passed;
}

static int run_external_sender_test(unsigned int port) {
    SudekiMpInputBridgeState received;
    DWORD deadline;

    if (!require(SudekiMpInputBridgeStart(port, 500u),
                 "external receiver start failed")) {
        return 1;
    }
    deadline = GetTickCount() + 3000u;
    while (!SudekiMpInputBridgePoll(&received) &&
           (LONG)(GetTickCount() - deadline) < 0) {
        Sleep(1u);
    }
    if (!require((LONG)(GetTickCount() - deadline) < 0,
                 "no Linux bridge packet received")) {
        SudekiMpInputBridgeStop();
        return 1;
    }
    printf(
        "external input bridge packet received: sequence=%lu left=%d,%d right=%d,%d buttons=0x%08lx\n",
        (unsigned long)received.sequence,
        (int)received.left_x,
        (int)received.left_y,
        (int)received.right_x,
        (int)received.right_y,
        (unsigned long)received.buttons
    );
    SudekiMpInputBridgeStop();
    return 0;
}

static int run_local_hub_adapter_test(void) {
    WSADATA data;
    SOCKET sender = INVALID_SOCKET;
    SudekiMpInputBridgeState source;
    SudekiMpInputBridgeState received;
    unsigned int base_port = 0u;
    unsigned int candidate;
    int sender_winsock_started = 0;
    int passed = 0;

    for (candidate = 53000u; candidate <= 55000u; candidate += 13u) {
        if (SudekiMpLocalInputHubStartUdp(
                SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK,
                candidate,
                250u)) {
            base_port = candidate;
            break;
        }
    }
    if (!require(base_port != 0u,
                 "fixed-three LocalInputHub start failed") ||
        !require(SudekiMpInputBridgeBoundPort() == base_port,
                 "legacy adapter did not expose the P2 hub port") ||
        !require(WSAStartup(MAKEWORD(2, 2), &data) == 0,
                 "hub adapter sender WSAStartup failed")) {
        goto cleanup;
    }
    sender_winsock_started = 1;
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!require(sender != INVALID_SOCKET,
                 "hub adapter sender socket failed")) {
        goto cleanup;
    }

    ZeroMemory(&source, sizeof(source));
    source.sequence = 900u;
    source.sender_timestamp_ms = 9000u;
    source.left_x = 900;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    if (!require(send_state_packet(sender, base_port, &source),
                 "hub adapter P2 packet send failed") ||
        !require(wait_for_raw_state(&source, &received, 250u),
                 "legacy adapter did not poll the P2 hub seat") ||
        !require(SudekiMpInputBridgeIdentity() ==
                     SudekiMpLocalInputHubSeatIdentity(1u),
                 "legacy adapter did not expose the P2 hub identity")) {
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (sender != INVALID_SOCKET) {
        closesocket(sender);
    }
    if (sender_winsock_started) {
        WSACleanup();
    }
    SudekiMpInputBridgeStop();
    if (!require(SudekiMpLocalInputHubRequestedMask() == 0u,
                 "legacy bridge stop did not release the hub bank")) {
        passed = 0;
    }
    return passed;
}

int main(int argc, char **argv) {
    WSADATA data;
    SOCKET sender;
    SudekiMpInputBridgeState source;
    SudekiMpInputBridgeState received;
    uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];
    unsigned int port;
    DWORD deadline;

    if (argc == 3 && strcmp(argv[1], "--external") == 0) {
        char *end = NULL;
        unsigned long external_port = strtoul(argv[2], &end, 10);
        if (argv[2][0] == '\0' || end == NULL || *end != '\0' ||
            external_port < 1024u || external_port > 65535u) {
            fputs("input_bridge_receiver_test: invalid external port\n",
                  stderr);
            return 2;
        }
        return run_external_sender_test((unsigned int)external_port);
    }
    if (argc != 1) {
        fputs("usage: SudekiMP.InputBridgeReceiverTest.exe [--external port]\n",
              stderr);
        return 2;
    }

    if (!require(SudekiMpInputBridgeStart(0u, 60u),
                 "receiver start failed")) {
        return 1;
    }
    port = SudekiMpInputBridgeBoundPort();
    if (!require(port != 0u, "ephemeral port unavailable") ||
        !require(WSAStartup(MAKEWORD(2, 2), &data) == 0,
                 "sender WSAStartup failed")) {
        SudekiMpInputBridgeStop();
        return 1;
    }
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!require(sender != INVALID_SOCKET, "sender socket failed")) {
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    ZeroMemory(&source, sizeof(source));
    source.sequence = 42u;
    source.sender_timestamp_ms = 500u;
    source.left_x = 12000;
    source.left_y = -22000;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    SudekiMpEncodeInputBridgePacket(packet, &source);
    if (!require(send_packet(sender, port, packet, sizeof(packet)) ==
                     (int)sizeof(packet),
                 "valid send failed")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    deadline = GetTickCount() + 250u;
    while (!SudekiMpInputBridgePoll(&received) &&
           (LONG)(GetTickCount() - deadline) < 0) {
        Sleep(1u);
    }
    if (!require(received.sequence == source.sequence,
                 "valid packet not received") ||
        !require(received.left_x == source.left_x &&
                     received.left_y == source.left_y,
                 "axis state mismatch") ||
        !require(received.buttons == source.buttons,
                 "button state mismatch")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    source.sequence = 43u;
    source.sender_timestamp_ms = 600u;
    source.left_x = -12345;
    source.left_y = 23456;
    source.right_x = 3210;
    source.right_y = -4321;
    source.left_trigger = 100u;
    source.right_trigger = 200u;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A |
        SUDEKIMP_BRIDGE_BUTTON_B;
    SudekiMpEncodeInputBridgePacket(packet, &source);
    SudekiMpInputBridgeSetGameplaySuppressed(TRUE);
    if (!require(send_packet(sender, port, packet, sizeof(packet)) ==
                     (int)sizeof(packet),
                 "suppressed send failed")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }
    deadline = GetTickCount() + 250u;
    do {
        SudekiMpInputBridgePollRaw(&received);
        if (received.sequence == source.sequence) {
            break;
        }
        Sleep(1u);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    if (!require(received.sequence == source.sequence,
                 "raw vote packet not received") ||
        !require(received.left_x == source.left_x &&
                     received.right_y == source.right_y &&
                     received.buttons == source.buttons,
                 "raw vote state was filtered") ||
        !require(SudekiMpInputBridgePoll(&received),
                 "suppressed gameplay poll disconnected controller") ||
        !require(received.sequence == source.sequence &&
                     received.sender_timestamp_ms ==
                         source.sender_timestamp_ms,
                 "suppressed gameplay poll lost packet identity") ||
        !require(received.left_x == 0 && received.left_y == 0 &&
                     received.right_x == 0 && received.right_y == 0 &&
                     received.left_trigger == 0u &&
                     received.right_trigger == 0u &&
                     received.buttons == 0u,
                 "suppressed gameplay state was not neutral")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
    if (!require(SudekiMpInputBridgePoll(&received),
                 "release guard disconnected controller") ||
        !require(received.buttons == 0u && received.left_x == 0 &&
                     received.right_y == 0,
                 "held vote input leaked after suppression release")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    ZeroMemory(&source, sizeof(source));
    source.sequence = 44u;
    source.sender_timestamp_ms = 700u;
    source.left_trigger =
        SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM;
    source.right_trigger =
        SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM;
    SudekiMpEncodeInputBridgePacket(packet, &source);
    if (!require(send_packet(sender, port, packet, sizeof(packet)) ==
                     (int)sizeof(packet),
                 "neutral release packet send failed")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }
    deadline = GetTickCount() + 250u;
    do {
        SudekiMpInputBridgePoll(&received);
        if (received.sequence == source.sequence) {
            break;
        }
        Sleep(1u);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    if (!require(received.sequence == source.sequence,
                 "one-count trigger-neutral release packet not received") ||
        !require(received.left_trigger == source.left_trigger &&
                     received.right_trigger == source.right_trigger,
                 "one-count trigger-neutral release packet was filtered")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    source.sequence = 45u;
    source.sender_timestamp_ms = 800u;
    source.left_x = 7777;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    SudekiMpEncodeInputBridgePacket(packet, &source);
    if (!require(send_packet(sender, port, packet, sizeof(packet)) ==
                     (int)sizeof(packet),
                 "resumed gameplay packet send failed")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }
    deadline = GetTickCount() + 250u;
    do {
        SudekiMpInputBridgePoll(&received);
        if (received.sequence == source.sequence) {
            break;
        }
        Sleep(1u);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    if (!require(received.sequence == source.sequence &&
                     received.left_x == source.left_x &&
                     received.buttons == source.buttons,
                 "gameplay did not resume after a neutral packet")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    Sleep(80u);
    if (!require(!SudekiMpInputBridgePoll(&received),
                 "stale packet remained connected") ||
        !require(received.buttons == 0u && received.left_x == 0,
                 "stale state was not neutralized")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    closesocket(sender);
    WSACleanup();
    SudekiMpInputBridgeStop();
    if (!run_sequence_ordering_test()) {
        return 1;
    }
    if (!run_local_hub_adapter_test()) {
        return 1;
    }
    puts("input bridge receiver tests passed");
    return 0;
}
