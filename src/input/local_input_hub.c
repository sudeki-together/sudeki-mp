#include <winsock2.h>
#include <ws2tcpip.h>
#include <xinput.h>

#include "input/local_input_hub.h"

#include "engine/log.h"

#include <stdint.h>
#include <string.h>

typedef DWORD(WINAPI *SudekiMpHubXInputGetState)(DWORD, XINPUT_STATE *);

typedef enum SudekiMpLocalInputTransport {
    SUDEKIMP_LOCAL_INPUT_TRANSPORT_NONE = 0,
    SUDEKIMP_LOCAL_INPUT_TRANSPORT_UDP,
    SUDEKIMP_LOCAL_INPUT_TRANSPORT_XINPUT
} SudekiMpLocalInputTransport;

typedef struct SudekiMpLocalInputSeatRuntime {
    SOCKET socket;
    unsigned int port;
    unsigned int controller_slot;
    DWORD timeout_ms;
    DWORD last_packet_tick;
    SudekiMpInputBridgeState state;
    BOOL packet_received;
    BOOL connection_logged;
    BOOL resume_requires_neutral;
    unsigned int rejected_packets;
    unsigned int stale_packets;
    uint32_t identity_generation;
} SudekiMpLocalInputSeatRuntime;

static SudekiMpLocalInputSeatRuntime seats[SUDEKIMP_LOCAL_INPUT_MAX_SEATS];
static SudekiMpLocalInputTransport transport;
static uint8_t requested_mask;
static BOOL gameplay_suppressed;
static BOOL winsock_started;
static HMODULE xinput_module;
static SudekiMpHubXInputGetState xinput_get_state;

static BOOL valid_requested_mask(uint8_t mask) {
    return (mask & SUDEKIMP_LOCAL_INPUT_HOST_MASK) != 0u &&
        (mask & (uint8_t)~0x0fu) == 0u;
}

static BOOL sequence_is_newer(uint32_t candidate, uint32_t baseline) {
    uint32_t distance = candidate - baseline;

    return distance != 0u && distance < UINT32_C(0x80000000);
}

static void clear_runtime(void) {
    uint32_t identity_generations[SUDEKIMP_LOCAL_INPUT_MAX_SEATS];
    unsigned int seat_index;

    for (seat_index = 0u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
            ++seat_index) {
        identity_generations[seat_index] =
            seats[seat_index].identity_generation;
    }
    ZeroMemory(seats, sizeof(seats));
    for (seat_index = 0u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
            ++seat_index) {
        seats[seat_index].socket = INVALID_SOCKET;
        seats[seat_index].controller_slot = XUSER_MAX_COUNT;
        seats[seat_index].identity_generation =
            identity_generations[seat_index];
    }
    transport = SUDEKIMP_LOCAL_INPUT_TRANSPORT_NONE;
    requested_mask = 0u;
    gameplay_suppressed = FALSE;
}

static uint32_t bridge_buttons(WORD buttons) {
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

BOOL SudekiMpLocalInputHubResumeNeutralPolicy(
    const SudekiMpInputBridgeState *state
) {
    return state != NULL &&
        (int)state->left_x >= -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->left_x <= SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->left_y >= -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->left_y <= SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->right_x >= -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->right_x <= SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->right_y >= -SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        (int)state->right_y <= SUDEKIMP_LOCAL_INPUT_STICK_NEUTRAL_MAXIMUM &&
        state->left_trigger <= SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM &&
        state->right_trigger <= SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM &&
        state->buttons == 0u;
}

static void neutralize(SudekiMpInputBridgeState *state) {
    uint32_t sequence;
    uint32_t timestamp;

    if (state == NULL) return;
    sequence = state->sequence;
    timestamp = state->sender_timestamp_ms;
    ZeroMemory(state, sizeof(*state));
    state->sequence = sequence;
    state->sender_timestamp_ms = timestamp;
}

static BOOL seat_requested(unsigned int seat_index) {
    return seat_index > 0u && seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS &&
        (requested_mask & (uint8_t)(1u << seat_index)) != 0u;
}

static void advance_identity_generation(SudekiMpLocalInputSeatRuntime *seat) {
    ++seat->identity_generation;
    if (seat->identity_generation == 0u) {
        ++seat->identity_generation;
    }
}

BOOL SudekiMpLocalInputHubStartXInput(
    uint8_t human_mask,
    const unsigned int controller_slots[3]
) {
    union {
        FARPROC generic;
        SudekiMpHubXInputGetState typed;
    } resolver;
    unsigned int seat_index;
    uint8_t used_slots = 0u;

    if (transport != SUDEKIMP_LOCAL_INPUT_TRANSPORT_NONE ||
        !valid_requested_mask(human_mask) || controller_slots == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    clear_runtime();
    for (seat_index = 1u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
            ++seat_index) {
        unsigned int slot = controller_slots[seat_index - 1u];
        if ((human_mask & (uint8_t)(1u << seat_index)) == 0u) continue;
        if (slot >= XUSER_MAX_COUNT || (used_slots & (uint8_t)(1u << slot))) {
            clear_runtime();
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        used_slots |= (uint8_t)(1u << slot);
        seats[seat_index].controller_slot = slot;
    }
    xinput_module = LoadLibraryW(L"xinput1_2.dll");
    if (xinput_module == NULL) {
        clear_runtime();
        return FALSE;
    }
    resolver.generic = GetProcAddress(xinput_module, "XInputGetState");
    xinput_get_state = resolver.typed;
    if (xinput_get_state == NULL) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        SudekiMpLocalInputHubStop();
        return FALSE;
    }
    requested_mask = human_mask;
    transport = SUDEKIMP_LOCAL_INPUT_TRANSPORT_XINPUT;
    SudekiMpLogFormat(
        "local_input_hub event=start transport=xinput requested_mask=0x%02x "
        "p2_slot=%u p3_slot=%u p4_slot=%u policy=stable_seat_bindings_allow_disconnected_startup\r\n",
        human_mask,
        seats[1].controller_slot,
        seats[2].controller_slot,
        seats[3].controller_slot);
    return TRUE;
}

BOOL SudekiMpLocalInputHubStartUdp(
    uint8_t human_mask,
    unsigned int base_port,
    DWORD timeout_ms
) {
    WSADATA data;
    unsigned int seat_index;

    if (transport != SUDEKIMP_LOCAL_INPUT_TRANSPORT_NONE ||
        !valid_requested_mask(human_mask) || base_port < 1024u ||
        base_port > 65533u || timeout_ms < 50u || timeout_ms > 5000u) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    clear_runtime();
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    winsock_started = TRUE;
    for (seat_index = 1u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
            ++seat_index) {
        struct sockaddr_in address;
        u_long nonblocking = 1u;
        SOCKET socket_value;

        if ((human_mask & (uint8_t)(1u << seat_index)) == 0u) continue;
        socket_value = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_value == INVALID_SOCKET) {
            SetLastError((DWORD)WSAGetLastError());
            SudekiMpLocalInputHubStop();
            return FALSE;
        }
        ZeroMemory(&address, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons((u_short)(base_port + seat_index - 1u));
        if (bind(socket_value, (const struct sockaddr *)&address,
                sizeof(address)) == SOCKET_ERROR ||
            ioctlsocket(socket_value, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            SetLastError((DWORD)WSAGetLastError());
            closesocket(socket_value);
            SudekiMpLocalInputHubStop();
            return FALSE;
        }
        seats[seat_index].socket = socket_value;
        seats[seat_index].port = base_port + seat_index - 1u;
        seats[seat_index].timeout_ms = timeout_ms;
    }
    requested_mask = human_mask;
    transport = SUDEKIMP_LOCAL_INPUT_TRANSPORT_UDP;
    SudekiMpLogFormat(
        "local_input_hub event=start transport=udp requested_mask=0x%02x "
        "base_port=%u timeout_ms=%lu policy=one_loopback_port_per_stable_controller_seat\r\n",
        human_mask, base_port, (unsigned long)timeout_ms);
    return TRUE;
}

void SudekiMpLocalInputHubStop(void) {
    unsigned int seat_index;

    if (transport == SUDEKIMP_LOCAL_INPUT_TRANSPORT_NONE &&
        !winsock_started && xinput_module == NULL) {
        clear_runtime();
        return;
    }
    for (seat_index = 1u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
            ++seat_index) {
        if (seats[seat_index].socket != INVALID_SOCKET) {
            closesocket(seats[seat_index].socket);
        }
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
    clear_runtime();
}

static BOOL poll_xinput(unsigned int seat_index,
                        SudekiMpInputBridgeState *output) {
    SudekiMpLocalInputSeatRuntime *seat = &seats[seat_index];
    XINPUT_STATE native_state;
    DWORD result;

    ZeroMemory(&native_state, sizeof(native_state));
    result = xinput_get_state((DWORD)seat->controller_slot, &native_state);
    if (result != ERROR_SUCCESS) {
        if (seat->connection_logged) {
            SudekiMpLogFormat(
                "local_input_hub event=disconnected seat=%u transport=xinput slot=%u error=%lu\r\n",
                seat_index + 1u, seat->controller_slot,
                (unsigned long)result);
        }
        seat->connection_logged = FALSE;
        seat->packet_received = FALSE;
        ZeroMemory(&seat->state, sizeof(seat->state));
        ZeroMemory(output, sizeof(*output));
        return FALSE;
    }
    ZeroMemory(&seat->state, sizeof(seat->state));
    seat->state.sequence = native_state.dwPacketNumber;
    seat->state.sender_timestamp_ms = GetTickCount();
    seat->state.left_x = native_state.Gamepad.sThumbLX;
    seat->state.left_y = native_state.Gamepad.sThumbLY;
    seat->state.right_x = native_state.Gamepad.sThumbRX;
    seat->state.right_y = native_state.Gamepad.sThumbRY;
    seat->state.left_trigger = (uint16_t)native_state.Gamepad.bLeftTrigger * 257u;
    seat->state.right_trigger = (uint16_t)native_state.Gamepad.bRightTrigger * 257u;
    seat->state.buttons = bridge_buttons(native_state.Gamepad.wButtons);
    seat->packet_received = TRUE;
    if (!seat->connection_logged) {
        advance_identity_generation(seat);
        seat->resume_requires_neutral = TRUE;
        SudekiMpLogFormat(
            "local_input_hub event=connected seat=%u transport=xinput slot=%u packet=%lu identity_generation=%lu\r\n",
            seat_index + 1u, seat->controller_slot,
            (unsigned long)seat->state.sequence,
            (unsigned long)seat->identity_generation);
        seat->connection_logged = TRUE;
    }
    *output = seat->state;
    return TRUE;
}

static void expire_udp_seat(unsigned int seat_index, DWORD now) {
    SudekiMpLocalInputSeatRuntime *seat = &seats[seat_index];

    if (!seat->packet_received ||
        (DWORD)(now - seat->last_packet_tick) <= seat->timeout_ms) return;
    if (seat->connection_logged) {
        SudekiMpLogFormat(
            "local_input_hub event=disconnected seat=%u transport=udp timeout_ms=%lu\r\n",
            seat_index + 1u, (unsigned long)seat->timeout_ms);
    }
    ZeroMemory(&seat->state, sizeof(seat->state));
    seat->last_packet_tick = 0u;
    seat->packet_received = FALSE;
    seat->connection_logged = FALSE;
}

static BOOL poll_udp(unsigned int seat_index,
                     SudekiMpInputBridgeState *output) {
    SudekiMpLocalInputSeatRuntime *seat = &seats[seat_index];
    uint8_t packet[64];
    DWORD now;

    expire_udp_seat(seat_index, GetTickCount());
    for (;;) {
        struct sockaddr_in sender;
        int sender_size = sizeof(sender);
        int received;
        SudekiMpInputBridgeState decoded;

        ZeroMemory(&sender, sizeof(sender));
        received = recvfrom(seat->socket, (char *)packet, (int)sizeof(packet),
            0, (struct sockaddr *)&sender, &sender_size);
        if (received == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                SudekiMpLogFormat(
                    "local_input_hub event=receive_error seat=%u error=%d\r\n",
                    seat_index + 1u, WSAGetLastError());
            }
            break;
        }
        if (sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK) ||
            !SudekiMpDecodeInputBridgePacket(packet, (size_t)received,
                &decoded)) {
            ++seat->rejected_packets;
            continue;
        }
        if (seat->packet_received &&
            !sequence_is_newer(
                decoded.sequence, seat->state.sequence)) {
            ++seat->stale_packets;
            continue;
        }
        seat->state = decoded;
        seat->last_packet_tick = GetTickCount();
        seat->packet_received = TRUE;
    }
    now = GetTickCount();
    expire_udp_seat(seat_index, now);
    if (!seat->packet_received) {
        ZeroMemory(output, sizeof(*output));
        return FALSE;
    }
    if (!seat->connection_logged) {
        advance_identity_generation(seat);
        seat->resume_requires_neutral = TRUE;
        SudekiMpLogFormat(
            "local_input_hub event=connected seat=%u transport=udp port=%u sequence=%lu identity_generation=%lu\r\n",
            seat_index + 1u, seat->port,
            (unsigned long)seat->state.sequence,
            (unsigned long)seat->identity_generation);
        seat->connection_logged = TRUE;
    }
    *output = seat->state;
    return TRUE;
}

BOOL SudekiMpLocalInputHubPollRaw(
    unsigned int seat_index,
    SudekiMpInputBridgeState *state
) {
    if (state == NULL || !seat_requested(seat_index)) {
        if (state != NULL) ZeroMemory(state, sizeof(*state));
        return FALSE;
    }
    if (transport == SUDEKIMP_LOCAL_INPUT_TRANSPORT_XINPUT &&
        xinput_get_state != NULL) {
        return poll_xinput(seat_index, state);
    }
    if (transport == SUDEKIMP_LOCAL_INPUT_TRANSPORT_UDP &&
        seats[seat_index].socket != INVALID_SOCKET) {
        return poll_udp(seat_index, state);
    }
    ZeroMemory(state, sizeof(*state));
    return FALSE;
}

BOOL SudekiMpLocalInputHubPoll(
    unsigned int seat_index,
    SudekiMpInputBridgeState *state
) {
    SudekiMpLocalInputSeatRuntime *seat;

    if (!SudekiMpLocalInputHubPollRaw(seat_index, state)) return FALSE;
    seat = &seats[seat_index];
    if (gameplay_suppressed) {
        neutralize(state);
        return TRUE;
    }
    if (seat->resume_requires_neutral) {
        if (SudekiMpLocalInputHubResumeNeutralPolicy(state)) {
            seat->resume_requires_neutral = FALSE;
        } else {
            neutralize(state);
        }
    }
    return TRUE;
}

void SudekiMpLocalInputHubSetGameplaySuppressed(BOOL suppressed) {
    BOOL requested = suppressed != FALSE;
    unsigned int seat_index;

    if (gameplay_suppressed && !requested) {
        for (seat_index = 1u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
                ++seat_index) {
            if (seat_requested(seat_index)) {
                seats[seat_index].resume_requires_neutral = TRUE;
            }
        }
    }
    gameplay_suppressed = requested;
}

BOOL SudekiMpLocalInputHubGameplaySuppressed(void) {
    return gameplay_suppressed;
}

uint8_t SudekiMpLocalInputHubRequestedMask(void) {
    return requested_mask;
}

uint8_t SudekiMpLocalInputHubConnectedMask(void) {
    uint8_t mask = requested_mask & SUDEKIMP_LOCAL_INPUT_HOST_MASK;
    unsigned int seat_index;
    SudekiMpInputBridgeState state;

    for (seat_index = 1u; seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS;
            ++seat_index) {
        if (SudekiMpLocalInputHubPollRaw(seat_index, &state)) {
            mask |= (uint8_t)(1u << seat_index);
        }
    }
    return mask;
}

unsigned int SudekiMpLocalInputHubSeatPort(unsigned int seat_index) {
    return seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS ?
        seats[seat_index].port : 0u;
}

unsigned int SudekiMpLocalInputHubSeatController(unsigned int seat_index) {
    return seat_index < SUDEKIMP_LOCAL_INPUT_MAX_SEATS ?
        seats[seat_index].controller_slot : XUSER_MAX_COUNT;
}

const void *SudekiMpLocalInputHubSeatIdentity(unsigned int seat_index) {
    if (!seat_requested(seat_index) ||
        !seats[seat_index].connection_logged) return NULL;
    if (transport == SUDEKIMP_LOCAL_INPUT_TRANSPORT_XINPUT) {
        return xinput_get_state == NULL ? NULL : &seats[seat_index];
    }
    if (transport == SUDEKIMP_LOCAL_INPUT_TRANSPORT_UDP) {
        return seats[seat_index].socket == INVALID_SOCKET ? NULL :
            &seats[seat_index].socket;
    }
    return NULL;
}

uint32_t SudekiMpLocalInputHubSeatIdentityGeneration(
    unsigned int seat_index
) {
    return seat_requested(seat_index) &&
        seats[seat_index].connection_logged ?
        seats[seat_index].identity_generation : 0u;
}
