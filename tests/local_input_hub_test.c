#include <winsock2.h>
#include <ws2tcpip.h>

#include "input/bridge_protocol.h"
#include "input/local_input_hub.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static int send_state(SOCKET sender, unsigned int port,
                      const SudekiMpInputBridgeState *state) {
    struct sockaddr_in destination;
    uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];

    ZeroMemory(&destination, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons((u_short)port);
    return SudekiMpEncodeInputBridgePacket(packet, state) &&
        sendto(sender, (const char *)packet, sizeof(packet), 0,
            (const struct sockaddr *)&destination, sizeof(destination)) ==
                (int)sizeof(packet);
}

static int wait_for_seat(unsigned int seat_index, uint32_t sequence,
                         SudekiMpInputBridgeState *state) {
    DWORD deadline = GetTickCount() + 1000u;

    do {
        if (SudekiMpLocalInputHubPollRaw(seat_index, state) &&
            state->sequence == sequence) {
            return 1;
        }
        Sleep(1u);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    return 0;
}

int main(void) {
    static const uint8_t full_mask = 0x0fu;
    static const unsigned int xinput_slots[3] = {2u, 0u, 3u};
    static const unsigned int duplicate_slots[3] = {1u, 1u, 2u};
    WSADATA data;
    SOCKET sender = INVALID_SOCKET;
    SudekiMpInputBridgeState sent;
    SudekiMpInputBridgeState received;
    unsigned int base_port = 0u;
    unsigned int candidate;
    unsigned int seat_index;
    uint32_t last_seat_three_generation = 0u;

    CHECK(!SudekiMpLocalInputHubStartUdp(0x00u, 42000u, 250u));
    CHECK(!SudekiMpLocalInputHubStartUdp(0x10u, 42000u, 250u));
    CHECK(!SudekiMpLocalInputHubStartUdp(0x02u, 42000u, 250u));
    CHECK(SudekiMpLocalInputHubRequestedMask() == 0u);

    for (candidate = 42000u; candidate <= 52000u; candidate += 17u) {
        if (SudekiMpLocalInputHubStartUdp(full_mask, candidate, 250u)) {
            base_port = candidate;
            break;
        }
    }
    CHECK(base_port != 0u);
    if (base_port == 0u) return 1;
    CHECK(SudekiMpLocalInputHubRequestedMask() == full_mask);
    CHECK(SudekiMpLocalInputHubSeatPort(1u) == base_port);
    CHECK(SudekiMpLocalInputHubSeatPort(2u) == base_port + 1u);
    CHECK(SudekiMpLocalInputHubSeatPort(3u) == base_port + 2u);
    CHECK(SudekiMpLocalInputHubSeatIdentity(0u) == NULL);
    CHECK(SudekiMpLocalInputHubSeatIdentity(1u) == NULL);
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(1u) == 0u);

    CHECK(WSAStartup(MAKEWORD(2, 2), &data) == 0);
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(sender != INVALID_SOCKET);
    if (sender == INVALID_SOCKET) goto cleanup;

    for (seat_index = 1u; seat_index < 4u; ++seat_index) {
        ZeroMemory(&sent, sizeof(sent));
        sent.sequence = 100u + seat_index;
        sent.sender_timestamp_ms = 1000u + seat_index;
        sent.left_x = (int16_t)(1000 * (int)seat_index);
        sent.right_y = (int16_t)(-2000 * (int)seat_index);
        sent.buttons = 1u << seat_index;
        CHECK(send_state(sender, base_port + seat_index - 1u, &sent));
        CHECK(wait_for_seat(seat_index, sent.sequence, &received));
        CHECK(received.left_x == sent.left_x);
        CHECK(received.right_y == sent.right_y);
        CHECK(received.buttons == sent.buttons);
        CHECK(SudekiMpLocalInputHubSeatIdentity(seat_index) != NULL);
        CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(seat_index) == 1u);
    }
    CHECK(SudekiMpLocalInputHubConnectedMask() == full_mask);

    SudekiMpLocalInputHubSetGameplaySuppressed(TRUE);
    CHECK(SudekiMpLocalInputHubGameplaySuppressed());
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.left_x == 0 && received.right_y == 0 &&
        received.buttons == 0u);
    SudekiMpLocalInputHubSetGameplaySuppressed(FALSE);
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.left_x == 0 && received.right_y == 0 &&
        received.buttons == 0u);

    ZeroMemory(&sent, sizeof(sent));
    sent.sequence = 104u;
    sent.sender_timestamp_ms = 1004u;
    CHECK(send_state(sender, base_port + 2u, &sent));
    CHECK(wait_for_seat(3u, sent.sequence, &received));
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.buttons == 0u && received.left_x == 0);

    Sleep(275u);
    CHECK(!SudekiMpLocalInputHubPollRaw(3u, &received));
    CHECK(SudekiMpLocalInputHubSeatIdentity(3u) == NULL);
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(3u) == 0u);
    sent.sequence = 105u;
    sent.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    CHECK(send_state(sender, base_port + 2u, &sent));
    CHECK(wait_for_seat(3u, sent.sequence, &received));
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(3u) == 2u);
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.buttons == 0u);

    sent.sequence = 106u;
    sent.buttons = 0u;
    CHECK(send_state(sender, base_port + 2u, &sent));
    CHECK(wait_for_seat(3u, sent.sequence, &received));
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.buttons == 0u);

    sent.sequence = 107u;
    sent.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    CHECK(send_state(sender, base_port + 2u, &sent));
    CHECK(wait_for_seat(3u, sent.sequence, &received));
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.buttons == SUDEKIMP_BRIDGE_BUTTON_A);
    last_seat_three_generation =
        SudekiMpLocalInputHubSeatIdentityGeneration(3u);

cleanup:
    if (sender != INVALID_SOCKET) {
        closesocket(sender);
        sender = INVALID_SOCKET;
    }
    WSACleanup();
    SudekiMpLocalInputHubStop();
    CHECK(SudekiMpLocalInputHubRequestedMask() == 0u);
    CHECK(SudekiMpLocalInputHubConnectedMask() == 0u);
    CHECK(SudekiMpLocalInputHubSeatIdentity(1u) == NULL);
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(1u) == 0u);

    CHECK(SudekiMpLocalInputHubStartUdp(0x09u, base_port, 250u));
    CHECK(SudekiMpLocalInputHubSeatIdentity(3u) == NULL);
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(3u) == 0u);
    CHECK(WSAStartup(MAKEWORD(2, 2), &data) == 0);
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(sender != INVALID_SOCKET);
    if (sender != INVALID_SOCKET) {
        ZeroMemory(&sent, sizeof(sent));
        sent.sequence = 200u;
        CHECK(send_state(sender, base_port + 2u, &sent));
        CHECK(wait_for_seat(3u, sent.sequence, &received));
        CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(3u) >
            last_seat_three_generation);
        closesocket(sender);
        sender = INVALID_SOCKET;
    }
    WSACleanup();
    SudekiMpLocalInputHubStop();
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(3u) == 0u);

    CHECK(!SudekiMpLocalInputHubStartXInput(full_mask, duplicate_slots));
    CHECK(SudekiMpLocalInputHubRequestedMask() == 0u);
    CHECK(SudekiMpLocalInputHubStartXInput(full_mask, xinput_slots));
    CHECK(SudekiMpLocalInputHubRequestedMask() == full_mask);
    CHECK(SudekiMpLocalInputHubSeatController(1u) == 2u);
    CHECK(SudekiMpLocalInputHubSeatController(2u) == 0u);
    CHECK(SudekiMpLocalInputHubSeatController(3u) == 3u);
    CHECK(SudekiMpLocalInputHubSeatIdentityGeneration(1u) == 0u);
    SudekiMpLocalInputHubStop();

    if (failures != 0) {
        fprintf(stderr, "%d local input hub test(s) failed\n", failures);
        return 1;
    }
    puts("local input hub tests passed");
    return 0;
}
