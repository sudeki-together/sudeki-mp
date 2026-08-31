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

static void test_resume_neutral_policy(void) {
    SudekiMpInputBridgeState state;

    ZeroMemory(&state, sizeof(state));
    state.left_y = 1113;
    state.right_x = -437;
    state.right_y = -2180;
    CHECK(SudekiMpLocalInputHubResumeNeutralPolicy(&state));

    state.left_x = SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM;
    state.left_y = -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM;
    state.right_x = SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM;
    state.right_y = -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM;
    CHECK(SudekiMpLocalInputHubResumeNeutralPolicy(&state));

    state.left_x = SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM + 1;
    CHECK(!SudekiMpLocalInputHubResumeNeutralPolicy(&state));
    state.left_x = 0;
    state.right_y = -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM - 1;
    CHECK(!SudekiMpLocalInputHubResumeNeutralPolicy(&state));
    state.right_y = 0;
    state.left_trigger =
        SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM + 1u;
    CHECK(!SudekiMpLocalInputHubResumeNeutralPolicy(&state));
    state.left_trigger = 0u;
    state.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    CHECK(!SudekiMpLocalInputHubResumeNeutralPolicy(&state));
    CHECK(!SudekiMpLocalInputHubResumeNeutralPolicy(NULL));
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

    test_resume_neutral_policy();

    CHECK(!SudekiMpLocalInputHubStartUdp(0x00u, 42000u, 250u));
    CHECK(!SudekiMpLocalInputHubStartUdp(0x10u, 42000u, 250u));
    CHECK(!SudekiMpLocalInputHubStartUdp(0x02u, 42000u, 250u));
    CHECK(!SudekiMpLocalInputHubStartUdp(
        SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK, 65534u, 250u));
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
    sent.left_x = 1113;
    sent.left_y = -437;
    sent.right_x = -2180;
    CHECK(send_state(sender, base_port + 2u, &sent));
    CHECK(wait_for_seat(3u, sent.sequence, &received));
    CHECK(SudekiMpLocalInputHubPoll(3u, &received));
    CHECK(received.buttons == 0u && received.left_x == 1113 &&
        received.left_y == -437 && received.right_x == -2180);

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

    CHECK(SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK == 0x07u);
    CHECK(SudekiMpLocalInputHubStartUdp(
        SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK, base_port, 250u));
    CHECK(SudekiMpLocalInputHubRequestedMask() ==
        SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK);
    CHECK(SudekiMpLocalInputHubSeatPort(1u) == base_port);
    CHECK(SudekiMpLocalInputHubSeatPort(2u) == base_port + 1u);
    CHECK(SudekiMpLocalInputHubSeatPort(3u) == 0u);
    CHECK(WSAStartup(MAKEWORD(2, 2), &data) == 0);
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(sender != INVALID_SOCKET);
    if (sender != INVALID_SOCKET) {
        const void *player_two_identity;
        const void *player_three_identity;

        ZeroMemory(&sent, sizeof(sent));
        sent.sequence = 300u;
        sent.left_x = 2300;
        sent.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
        CHECK(send_state(sender, base_port, &sent));
        CHECK(wait_for_seat(1u, sent.sequence, &received));
        CHECK(received.left_x == 2300);

        ZeroMemory(&sent, sizeof(sent));
        sent.sequence = 301u;
        sent.left_x = -3100;
        sent.buttons = SUDEKIMP_BRIDGE_BUTTON_B;
        CHECK(send_state(sender, base_port + 1u, &sent));
        CHECK(wait_for_seat(2u, sent.sequence, &received));
        CHECK(received.left_x == -3100);

        player_two_identity = SudekiMpLocalInputHubSeatIdentity(1u);
        player_three_identity = SudekiMpLocalInputHubSeatIdentity(2u);
        CHECK(player_two_identity != NULL);
        CHECK(player_three_identity != NULL);
        CHECK(player_two_identity != player_three_identity);
        CHECK(SudekiMpLocalInputHubSeatIdentity(3u) == NULL);
        CHECK(SudekiMpLocalInputHubConnectedMask() ==
            SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK);
        closesocket(sender);
        sender = INVALID_SOCKET;
    }
    WSACleanup();
    SudekiMpLocalInputHubStop();

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
