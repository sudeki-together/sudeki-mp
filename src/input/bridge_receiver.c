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
static unsigned int invalid_packet_count;
static unsigned int bound_port;

static void reset_receiver_state(void) {
    bridge_timeout_ms = 0u;
    last_packet_tick = 0u;
    ZeroMemory(&last_state, sizeof(last_state));
    packet_received = FALSE;
    connection_logged = FALSE;
    invalid_packet_count = 0u;
    bound_port = 0u;
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

BOOL SudekiMpInputBridgePoll(SudekiMpInputBridgeState *state) {
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
        last_state = decoded;
        last_packet_tick = GetTickCount();
        packet_received = TRUE;
    }

    now = GetTickCount();
    if (!packet_received ||
        (DWORD)(now - last_packet_tick) > bridge_timeout_ms) {
        if (connection_logged) {
            SudekiMpLogFormat(
                "input_bridge event=disconnected timeout_ms=%lu policy=neutralize_player_two_input\r\n",
                (unsigned long)bridge_timeout_ms
            );
            connection_logged = FALSE;
        }
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

unsigned int SudekiMpInputBridgeBoundPort(void) {
    return bound_port;
}

const void *SudekiMpInputBridgeIdentity(void) {
    return bridge_socket == INVALID_SOCKET ? NULL : &bridge_socket;
}
