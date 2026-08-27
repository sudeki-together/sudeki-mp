#include <winsock2.h>
#include <ws2tcpip.h>

#include "input/bridge_receiver.h"

#include "engine/log.h"

#include <stdint.h>
#include <string.h>

static SOCKET bridge_socket = INVALID_SOCKET;
static DWORD bridge_timeout_ms;
static DWORD last_packet_tick;
static SudekiMpInputBridgeState last_state;
static BOOL winsock_started;
static BOOL packet_received;
static BOOL connection_logged;
static BOOL gameplay_suppressed;
static BOOL gameplay_resume_requires_neutral;
static unsigned int invalid_packet_count;
static unsigned int stale_sequence_count;
static unsigned int bound_port;

static void reset_receiver_state(void) {
    bridge_timeout_ms = 0u;
    last_packet_tick = 0u;
    ZeroMemory(&last_state, sizeof(last_state));
    packet_received = FALSE;
    connection_logged = FALSE;
    gameplay_suppressed = FALSE;
    gameplay_resume_requires_neutral = FALSE;
    invalid_packet_count = 0u;
    stale_sequence_count = 0u;
    bound_port = 0u;
}

BOOL SudekiMpInputBridgeSequenceIsNewer(
    uint32_t candidate,
    uint32_t baseline
) {
    uint32_t distance = candidate - baseline;

    return distance != 0u && distance < 0x80000000u;
}

static void expire_sequence_epoch(DWORD now) {
    if (!packet_received ||
        (DWORD)(now - last_packet_tick) <= bridge_timeout_ms) {
        return;
    }
    if (connection_logged) {
        SudekiMpLogFormat(
            "input_bridge event=disconnected timeout_ms=%lu policy=neutralize_player_two_input\r\n",
            (unsigned long)bridge_timeout_ms
        );
    }
    ZeroMemory(&last_state, sizeof(last_state));
    last_packet_tick = 0u;
    packet_received = FALSE;
    connection_logged = FALSE;
}

BOOL SudekiMpInputBridgeStart(unsigned int port, DWORD timeout_ms) {
    WSADATA data;
    struct sockaddr_in address;
    int address_size;
    u_long nonblocking = 1u;

    if (bridge_socket != INVALID_SOCKET || port > 65535u ||
        timeout_ms < 50u || timeout_ms > 5000u) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    reset_receiver_state();
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    winsock_started = TRUE;
    bridge_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bridge_socket == INVALID_SOCKET) {
        SetLastError((DWORD)WSAGetLastError());
        SudekiMpInputBridgeStop();
        return FALSE;
    }
    ZeroMemory(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((u_short)port);
    if (bind(bridge_socket, (const struct sockaddr *)&address,
             sizeof(address)) == SOCKET_ERROR ||
        ioctlsocket(bridge_socket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        SetLastError((DWORD)WSAGetLastError());
        SudekiMpInputBridgeStop();
        return FALSE;
    }
    address_size = sizeof(address);
    if (getsockname(bridge_socket, (struct sockaddr *)&address,
                    &address_size) == SOCKET_ERROR) {
        SetLastError((DWORD)WSAGetLastError());
        SudekiMpInputBridgeStop();
        return FALSE;
    }
    bridge_timeout_ms = timeout_ms;
    bound_port = (unsigned int)ntohs(address.sin_port);
    SudekiMpLogFormat(
        "input_bridge event=receiver_start transport=udp address=127.0.0.1 port=%u timeout_ms=%lu protocol_version=%u\r\n",
        bound_port,
        (unsigned long)bridge_timeout_ms,
        SUDEKIMP_INPUT_BRIDGE_PROTOCOL_VERSION
    );
    return TRUE;
}

void SudekiMpInputBridgeStop(void) {
    if (bridge_socket != INVALID_SOCKET) {
        closesocket(bridge_socket);
        bridge_socket = INVALID_SOCKET;
    }
    if (winsock_started) {
        WSACleanup();
        winsock_started = FALSE;
    }
    reset_receiver_state();
}

BOOL SudekiMpInputBridgePollRaw(SudekiMpInputBridgeState *state) {
    uint8_t packet[64];
    struct sockaddr_in sender;
    int sender_size;
    int received;
    DWORD now;

    if (state == NULL || bridge_socket == INVALID_SOCKET) {
        if (state != NULL) {
            ZeroMemory(state, sizeof(*state));
        }
        return FALSE;
    }
    /* Expire before draining the socket so a sender restart can begin at any
     * sequence, even when its first new-epoch packet is already queued. */
    expire_sequence_epoch(GetTickCount());
    for (;;) {
        SudekiMpInputBridgeState decoded;
        sender_size = sizeof(sender);
        ZeroMemory(&sender, sizeof(sender));
        received = recvfrom(
            bridge_socket,
            (char *)packet,
            (int)sizeof(packet),
            0,
            (struct sockaddr *)&sender,
            &sender_size
        );
        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                SudekiMpLogFormat(
                    "input_bridge event=receive_error winsock_error=%d\r\n",
                    error
                );
            }
            break;
        }
        if (sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK) ||
            !SudekiMpDecodeInputBridgePacket(
                packet, (size_t)received, &decoded)) {
            ++invalid_packet_count;
            if (invalid_packet_count == 1u) {
                SudekiMpLogWrite(
                    "input_bridge event=packet_rejected reason=invalid_source_or_protocol\r\n"
                );
            }
            continue;
        }
        if (packet_received &&
            !SudekiMpInputBridgeSequenceIsNewer(
                decoded.sequence,
                last_state.sequence)) {
            ++stale_sequence_count;
            if (stale_sequence_count == 1u) {
                SudekiMpLogFormat(
                    "input_bridge event=packet_rejected reason=duplicate_or_out_of_order sequence=%lu baseline=%lu\r\n",
                    (unsigned long)decoded.sequence,
                    (unsigned long)last_state.sequence
                );
            }
            continue;
        }
        last_state = decoded;
        last_packet_tick = GetTickCount();
        packet_received = TRUE;
    }

    now = GetTickCount();
    expire_sequence_epoch(now);
    if (!packet_received) {
        ZeroMemory(state, sizeof(*state));
        return FALSE;
    }
    if (!connection_logged) {
        SudekiMpLogFormat(
            "input_bridge event=connected sequence=%lu sender_timestamp_ms=%lu\r\n",
            (unsigned long)last_state.sequence,
            (unsigned long)last_state.sender_timestamp_ms
        );
        connection_logged = TRUE;
    }
    *state = last_state;
    return TRUE;
}

static BOOL gameplay_state_is_neutral(
    const SudekiMpInputBridgeState *state
) {
    return state != NULL && state->left_x == 0 && state->left_y == 0 &&
        state->right_x == 0 && state->right_y == 0 &&
        state->left_trigger <=
            SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM &&
        state->right_trigger <=
            SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM &&
        state->buttons == 0u;
}

static void neutralize_gameplay_state(SudekiMpInputBridgeState *state) {
    uint32_t sequence;
    uint32_t sender_timestamp_ms;

    if (state == NULL) {
        return;
    }
    sequence = state->sequence;
    sender_timestamp_ms = state->sender_timestamp_ms;
    ZeroMemory(state, sizeof(*state));
    state->sequence = sequence;
    state->sender_timestamp_ms = sender_timestamp_ms;
}

BOOL SudekiMpInputBridgePoll(SudekiMpInputBridgeState *state) {
    if (!SudekiMpInputBridgePollRaw(state)) {
        return FALSE;
    }
    if (gameplay_suppressed) {
        neutralize_gameplay_state(state);
        return TRUE;
    }
    if (gameplay_resume_requires_neutral) {
        if (gameplay_state_is_neutral(state)) {
            gameplay_resume_requires_neutral = FALSE;
            SudekiMpLogFormat(
                "input_bridge event=gameplay_resume state=released trigger_neutral_maximum=%u policy=require_post_vote_neutral_before_gameplay\r\n",
                SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM
            );
        } else {
            neutralize_gameplay_state(state);
        }
    }
    return TRUE;
}

void SudekiMpInputBridgeSetGameplaySuppressed(BOOL suppressed) {
    BOOL requested = suppressed != FALSE;

    if (gameplay_suppressed && !requested) {
        /* Do not leak a held A/B/stick from the consent prompt into movement,
         * combat, or camera control on the first resumed frame. */
        gameplay_resume_requires_neutral = TRUE;
        SudekiMpLogFormat(
            "input_bridge event=gameplay_resume state=waiting_for_neutral trigger_neutral_maximum=%u policy=require_post_vote_neutral_before_gameplay\r\n",
            SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM
        );
    }
    gameplay_suppressed = requested;
}

BOOL SudekiMpInputBridgeGameplaySuppressed(void) {
    return gameplay_suppressed;
}

unsigned int SudekiMpInputBridgeBoundPort(void) {
    return bound_port;
}

const void *SudekiMpInputBridgeIdentity(void) {
    return bridge_socket == INVALID_SOCKET ? NULL : &bridge_socket;
}
