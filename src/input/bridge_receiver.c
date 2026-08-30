#include <winsock2.h>
#include <ws2tcpip.h>
#include <xinput.h>

#include "input/bridge_receiver.h"
#include "input/local_input_hub.h"

#include "engine/log.h"

#include <stdint.h>
#include <string.h>

static SOCKET bridge_socket = INVALID_SOCKET;
typedef enum SudekiMpInputBridgeTransport {
    SUDEKIMP_INPUT_TRANSPORT_NONE = 0,
    SUDEKIMP_INPUT_TRANSPORT_UDP,
    SUDEKIMP_INPUT_TRANSPORT_XINPUT
} SudekiMpInputBridgeTransport;

typedef DWORD(WINAPI *SudekiMpXInputGetStateFunction)(
    DWORD user_index,
    XINPUT_STATE *state
);

static SudekiMpInputBridgeTransport bridge_transport;
static HMODULE xinput_module;
static SudekiMpXInputGetStateFunction xinput_get_state;
static unsigned int xinput_slot;
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
    bridge_transport = SUDEKIMP_INPUT_TRANSPORT_NONE;
    xinput_slot = 0u;
}

static uint32_t xinput_buttons_to_bridge(uint16_t buttons) {
    uint32_t result = 0u;

    if ((buttons & XINPUT_GAMEPAD_A) != 0u) result |= SUDEKIMP_BRIDGE_BUTTON_A;
    if ((buttons & XINPUT_GAMEPAD_B) != 0u) result |= SUDEKIMP_BRIDGE_BUTTON_B;
    if ((buttons & XINPUT_GAMEPAD_X) != 0u) result |= SUDEKIMP_BRIDGE_BUTTON_X;
    if ((buttons & XINPUT_GAMEPAD_Y) != 0u) result |= SUDEKIMP_BRIDGE_BUTTON_Y;
    if ((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER;
    if ((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER;
    if ((buttons & XINPUT_GAMEPAD_BACK) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_BACK;
    if ((buttons & XINPUT_GAMEPAD_START) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_START;
    if ((buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_LEFT_STICK;
    if ((buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_RIGHT_STICK;
    if ((buttons & XINPUT_GAMEPAD_DPAD_UP) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_DPAD_UP;
    if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN;
    if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT;
    if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0u) result |=
        SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT;
    return result;
}

static uint16_t xinput_trigger_to_bridge(BYTE value) {
    return (uint16_t)value * 257u;
}

static BOOL poll_xinput_state(SudekiMpInputBridgeState *state) {
    XINPUT_STATE native_state;
    DWORD result;

    if (state == NULL || xinput_get_state == NULL ||
        bridge_transport != SUDEKIMP_INPUT_TRANSPORT_XINPUT) {
        if (state != NULL) ZeroMemory(state, sizeof(*state));
        return FALSE;
    }
    ZeroMemory(&native_state, sizeof(native_state));
    result = xinput_get_state((DWORD)xinput_slot, &native_state);
    if (result != ERROR_SUCCESS) {
        if (connection_logged) {
            SudekiMpLogFormat(
                "input_bridge event=disconnected transport=xinput slot=%u error=%lu policy=neutralize_player_two_input\r\n",
                xinput_slot, (unsigned long)result
            );
        }
        connection_logged = FALSE;
        packet_received = FALSE;
        ZeroMemory(&last_state, sizeof(last_state));
        ZeroMemory(state, sizeof(*state));
        return FALSE;
    }
    ZeroMemory(&last_state, sizeof(last_state));
    last_state.sequence = native_state.dwPacketNumber;
    last_state.sender_timestamp_ms = GetTickCount();
    last_state.left_x = native_state.Gamepad.sThumbLX;
    last_state.left_y = native_state.Gamepad.sThumbLY;
    last_state.right_x = native_state.Gamepad.sThumbRX;
    last_state.right_y = native_state.Gamepad.sThumbRY;
    last_state.left_trigger = xinput_trigger_to_bridge(
        native_state.Gamepad.bLeftTrigger);
    last_state.right_trigger = xinput_trigger_to_bridge(
        native_state.Gamepad.bRightTrigger);
    last_state.buttons = xinput_buttons_to_bridge(native_state.Gamepad.wButtons);
    packet_received = TRUE;
    if (!connection_logged) {
        SudekiMpLogFormat(
            "input_bridge event=connected transport=xinput slot=%u packet=%lu\r\n",
            xinput_slot, (unsigned long)last_state.sequence
        );
        connection_logged = TRUE;
    }
    *state = last_state;
    return TRUE;
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

    if (bridge_transport != SUDEKIMP_INPUT_TRANSPORT_NONE ||
        bridge_socket != INVALID_SOCKET || port > 65535u ||
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
    bridge_transport = SUDEKIMP_INPUT_TRANSPORT_UDP;
    SudekiMpLogFormat(
        "input_bridge event=receiver_start transport=udp address=127.0.0.1 port=%u timeout_ms=%lu protocol_version=%u\r\n",
        bound_port,
        (unsigned long)bridge_timeout_ms,
        SUDEKIMP_INPUT_BRIDGE_PROTOCOL_VERSION
    );
    return TRUE;
}

BOOL SudekiMpInputBridgeStartXInput(unsigned int slot) {
    union {
        FARPROC generic;
        SudekiMpXInputGetStateFunction typed;
    } resolver;
    XINPUT_STATE initial_state;

    if (bridge_transport != SUDEKIMP_INPUT_TRANSPORT_NONE ||
        slot >= XUSER_MAX_COUNT) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    reset_receiver_state();
    xinput_module = LoadLibraryW(L"xinput1_2.dll");
    if (xinput_module == NULL) return FALSE;
    resolver.generic = GetProcAddress(xinput_module, "XInputGetState");
    xinput_get_state = resolver.typed;
    if (xinput_get_state == NULL) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        SudekiMpInputBridgeStop();
        return FALSE;
    }
    ZeroMemory(&initial_state, sizeof(initial_state));
    if (xinput_get_state((DWORD)slot, &initial_state) != ERROR_SUCCESS) {
        SetLastError(ERROR_DEVICE_NOT_CONNECTED);
        SudekiMpInputBridgeStop();
        return FALSE;
    }
    xinput_slot = slot;
    bridge_transport = SUDEKIMP_INPUT_TRANSPORT_XINPUT;
    SudekiMpLogFormat(
        "input_bridge event=receiver_start transport=xinput slot=%u protocol_version=%u\r\n",
        slot, SUDEKIMP_INPUT_BRIDGE_PROTOCOL_VERSION
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
    if (xinput_module != NULL) {
        FreeLibrary(xinput_module);
        xinput_module = NULL;
    }
    xinput_get_state = NULL;
    reset_receiver_state();
    /* The expanded local-seat bank is opt-in and currently has no production
     * starter, but it shares the input lifetime. Keep teardown complete before
     * any future activation path is allowed to reserve P3/P4 resources. */
    SudekiMpLocalInputHubStop();
}

BOOL SudekiMpInputBridgePollRaw(SudekiMpInputBridgeState *state) {
    uint8_t packet[64];
    struct sockaddr_in sender;
    int sender_size;
    int received;
    DWORD now;

    if ((SudekiMpLocalInputHubRequestedMask() & 0x02u) != 0u) {
        return SudekiMpLocalInputHubPollRaw(1u, state);
    }
    if (bridge_transport == SUDEKIMP_INPUT_TRANSPORT_XINPUT) {
        return poll_xinput_state(state);
    }
    if (state == NULL || bridge_transport != SUDEKIMP_INPUT_TRANSPORT_UDP ||
        bridge_socket == INVALID_SOCKET) {
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
    if ((SudekiMpLocalInputHubRequestedMask() & 0x02u) != 0u) {
        return SudekiMpLocalInputHubPoll(1u, state);
    }
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
    SudekiMpLocalInputHubSetGameplaySuppressed(requested);
}

BOOL SudekiMpInputBridgeGameplaySuppressed(void) {
    return gameplay_suppressed ||
        SudekiMpLocalInputHubGameplaySuppressed();
}

unsigned int SudekiMpInputBridgeBoundPort(void) {
    return bound_port;
}

unsigned int SudekiMpInputBridgeXInputSlot(void) {
    if ((SudekiMpLocalInputHubRequestedMask() & 0x02u) != 0u) {
        return SudekiMpLocalInputHubSeatController(1u);
    }
    return bridge_transport == SUDEKIMP_INPUT_TRANSPORT_XINPUT ?
        xinput_slot : XUSER_MAX_COUNT;
}

BOOL SudekiMpInputBridgeUsesXInput(void) {
    if ((SudekiMpLocalInputHubRequestedMask() & 0x02u) != 0u) {
        return SudekiMpLocalInputHubSeatController(1u) < XUSER_MAX_COUNT;
    }
    return bridge_transport == SUDEKIMP_INPUT_TRANSPORT_XINPUT;
}

const void *SudekiMpInputBridgeIdentity(void) {
    if ((SudekiMpLocalInputHubRequestedMask() & 0x02u) != 0u) {
        return SudekiMpLocalInputHubSeatIdentity(1u);
    }
    if (bridge_transport == SUDEKIMP_INPUT_TRANSPORT_UDP) {
        return bridge_socket == INVALID_SOCKET ? NULL : &bridge_socket;
    }
    if (bridge_transport == SUDEKIMP_INPUT_TRANSPORT_XINPUT) {
        return xinput_get_state == NULL ? NULL : (const void *)&xinput_module;
    }
    return NULL;
}
