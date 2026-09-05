#include "network/lan_arena_session.h"

#include "engine/log.h"
#include "network/lan_arena_authority.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    LAN_ARENA_DEFAULT_TIMEOUT_MS = 1500u,
    LAN_ARENA_HELLO_INTERVAL_MS = 300u,
    LAN_ARENA_KEEPALIVE_INTERVAL_MS = 250u,
    LAN_ARENA_MAX_PACKETS_PER_POLL = 64u,
    LAN_ARENA_SNAPSHOT_QUEUE_CAPACITY = 8u
};

typedef struct SudekiMpLanArenaSession {
    SOCKET socket;
    struct sockaddr_in peer_address;
    SudekiMpLanArenaSessionConfig config;
    char remote_ipv4[64];
    SudekiMpLanArenaConnectionState connection;
    SudekiMpLanArenaRejectReason failure;
    uint32_t next_sequence;
    uint32_t last_hello_at_ms;
    uint32_t last_keepalive_at_ms;
    uint32_t last_input_sequence;
    uint32_t last_snapshot_sequence;
    uint32_t last_sent_input_sequence;
    uint32_t last_sent_snapshot_sequence;
    uint8_t winsock_started;
    uint8_t peer_pinned;
    uint8_t input_pending;
    uint8_t snapshot_queue_head;
    uint8_t snapshot_queue_count;
    uint8_t keepalive_logged;
    SudekiMpLanArenaInput latest_input;
    SudekiMpLanArenaSnapshot snapshot_queue[LAN_ARENA_SNAPSHOT_QUEUE_CAPACITY];
} SudekiMpLanArenaSession;

static SudekiMpLanArenaSession session = {.socket = INVALID_SOCKET};
static SRWLOCK session_lock = SRWLOCK_INIT;

static void session_stop_unlocked(BOOL notify_peer);

static BOOL address_equal(const struct sockaddr_in *left, const struct sockaddr_in *right) {
    return left != NULL && right != NULL &&
        left->sin_family == AF_INET && right->sin_family == AF_INET &&
        left->sin_port == right->sin_port &&
        left->sin_addr.s_addr == right->sin_addr.s_addr;
}

static BOOL remote_role_for_local(
    SudekiMpLanArenaRole local_role,
    uint8_t *remote_role
) {
    if (remote_role == NULL) return FALSE;
    if (local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        *remote_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
        return TRUE;
    }
    if (local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        *remote_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
        return TRUE;
    }
    return FALSE;
}

static BOOL remote_simulation_node_role_for_local(
    SudekiMpLanArenaSimulationNodeRole local_role,
    uint8_t *remote_role
) {
    if (remote_role == NULL) return FALSE;
    if (local_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) {
        *remote_role = SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
        return TRUE;
    }
    if (local_role == SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) {
        *remote_role =
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
        return TRUE;
    }
    return FALSE;
}

static BOOL local_topology_supported(
    const SudekiMpLanArenaSessionConfig *config
) {
    return config != NULL &&
        ((config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
          config->local_simulation_node_role ==
              SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) ||
         (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
          config->local_simulation_node_role ==
              SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA));
}

static void reset_session(void) {
    if (session.socket != INVALID_SOCKET) {
        closesocket(session.socket);
    }
    if (session.winsock_started) {
        WSACleanup();
    }
    memset(&session, 0, sizeof(session));
    session.socket = INVALID_SOCKET;
}

static BOOL socket_send_packet(const SudekiMpLanArenaPacket *packet) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    size_t size = 0u;
    int sent;
    if (!session.peer_pinned || session.socket == INVALID_SOCKET || packet == NULL ||
        !SudekiMpLanArenaEncodePacket(bytes, &size, packet)) {
        return FALSE;
    }
    sent = sendto(
        session.socket,
        (const char *)bytes,
        (int)size,
        0,
        (const struct sockaddr *)&session.peer_address,
        sizeof(session.peer_address)
    );
    return sent == (int)size;
}

static void set_failed(SudekiMpLanArenaRejectReason reason, const char *event) {
    session.failure = reason;
    session.connection.phase = reason == SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT ?
        SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT :
        SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED;
    session.input_pending = 0u;
    session.snapshot_queue_head = 0u;
    session.snapshot_queue_count = 0u;
    SudekiMpLogFormat(
        "lan_arena_session event=%s phase=%u reason=%u policy=fail_closed_no_reconnect\r\n",
        event == NULL ? "failed" : event,
        (unsigned int)session.connection.phase,
        (unsigned int)reason
    );
}

static uint64_t generate_client_token(void) {
    LARGE_INTEGER counter;
    uint64_t token;
    QueryPerformanceCounter(&counter);
    token = ((uint64_t)(uint32_t)counter.LowPart << 32) ^
        (uint32_t)counter.HighPart ^ ((uint64_t)GetCurrentProcessId() << 16) ^
        GetTickCount();
    return token == 0u ? UINT64_C(1) : token;
}

static void fill_hello(SudekiMpLanArenaPacket *packet, SudekiMpLanArenaPacketType type) {
    memset(packet, 0, sizeof(*packet));
    packet->type = type;
    packet->sequence = ++session.next_sequence;
    packet->session_token = session.connection.session_token;
    packet->body.hello.sequence = packet->sequence;
    packet->body.hello.build_id = SUDEKIMP_LAN_ARENA_BUILD_ID;
    memcpy(packet->body.hello.game_hash, session.config.game_hash,
        SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE);
    packet->body.hello.map_id = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
    packet->body.hello.role = (uint8_t)session.config.local_role;
    packet->body.hello.simulation_node_role =
        (uint8_t)session.config.local_simulation_node_role;
    packet->body.hello.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    packet->body.hello.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    packet->body.hello.session_token = packet->session_token;
}

static BOOL handshake_valid_for_local(
    const SudekiMpLanArenaPacket *packet,
    uint64_t expected_token,
    SudekiMpLanArenaRejectReason *reason
) {
    SudekiMpLanArenaHandshakeExpectation expectation;
    uint8_t remote_role;
    uint8_t remote_simulation_node_role;
    if (packet == NULL ||
        !remote_role_for_local(session.config.local_role, &remote_role) ||
        !remote_simulation_node_role_for_local(
            session.config.local_simulation_node_role,
            &remote_simulation_node_role)) {
        if (reason != NULL) *reason = SUDEKIMP_LAN_ARENA_REJECT_MALFORMED;
        return FALSE;
    }
    expectation.build_id = SUDEKIMP_LAN_ARENA_BUILD_ID;
    expectation.game_hash = session.config.game_hash;
    expectation.map_id = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
    expectation.expected_sender_role = remote_role;
    expectation.expected_sender_simulation_node_role =
        remote_simulation_node_role;
    expectation.tal_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    expectation.ailish_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    expectation.expected_session_token = expected_token;
    return SudekiMpLanArenaHandshakeValid(&packet->body.hello, &expectation, reason);
}

static void send_reject(SudekiMpLanArenaRejectReason reason) {
    SudekiMpLanArenaPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_REJECT;
    packet.sequence = ++session.next_sequence;
    packet.session_token = session.connection.session_token;
    packet.body.reject_reason = reason;
    (void)socket_send_packet(&packet);
}

static void accept_host_hello(
    const SudekiMpLanArenaPacket *packet,
    const struct sockaddr_in *source,
    uint32_t now_ms
) {
    SudekiMpLanArenaRejectReason reason;
    SudekiMpLanArenaPacket ack;
    if (!handshake_valid_for_local(packet, 0u, &reason)) {
        session.peer_address = *source;
        session.peer_pinned = 1u;
        session.connection.session_token = packet->session_token;
        send_reject(reason);
        session.peer_pinned = 0u;
        session.connection.session_token = 0u;
        return;
    }
    session.peer_address = *source;
    session.peer_pinned = 1u;
    session.connection.session_token = packet->session_token;
    session.connection.peer_role = packet->body.hello.role;
    session.connection.peer_simulation_node_role =
        packet->body.hello.simulation_node_role;
    session.connection.phase = SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
    session.connection.last_received_sequence = packet->sequence;
    session.connection.last_received_at_ms = now_ms;
    session.connection.sequence_initialized = 1u;
    session.last_keepalive_at_ms = now_ms;
    fill_hello(&ack, SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK);
    if (!socket_send_packet(&ack)) {
        set_failed(SUDEKIMP_LAN_ARENA_REJECT_MALFORMED, "ack_send_failed");
        return;
    }
    SudekiMpLogFormat(
        "lan_arena_session event=host_connected token=0x%08lx%08lx port=%u "
        "role=Tal_host node=canonical_native_world peer=Ailish_client "
        "peer_node=replica\r\n",
        (unsigned long)(session.connection.session_token >> 32),
        (unsigned long)session.connection.session_token,
        session.config.port
    );
}

static void accept_client_ack(const SudekiMpLanArenaPacket *packet, uint32_t now_ms) {
    SudekiMpLanArenaRejectReason reason;
    if (!handshake_valid_for_local(packet, session.connection.session_token, &reason)) {
        set_failed(reason, "client_ack_rejected");
        return;
    }
    session.connection.peer_role = packet->body.hello.role;
    session.connection.peer_simulation_node_role =
        packet->body.hello.simulation_node_role;
    session.connection.phase = SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
    session.connection.last_received_sequence = packet->sequence;
    session.connection.last_received_at_ms = now_ms;
    session.connection.sequence_initialized = 1u;
    session.last_keepalive_at_ms = now_ms;
    SudekiMpLogFormat(
        "lan_arena_session event=client_connected token=0x%08lx%08lx "
        "role=Ailish_client node=replica peer=Tal_host "
        "peer_node=canonical_native_world\r\n",
        (unsigned long)(session.connection.session_token >> 32),
        (unsigned long)session.connection.session_token
    );
}

static BOOL session_start_unlocked(const SudekiMpLanArenaSessionConfig *config) {
    WSADATA wsa;
    struct sockaddr_in bind_address;
    u_long nonblocking = 1u;
    if (config == NULL || config->game_hash == NULL ||
        !local_topology_supported(config) ||
        config->port == 0u || config->port > 65535u ||
        (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
         (config->remote_ipv4 == NULL || config->remote_ipv4[0] == '\0'))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    session_stop_unlocked(FALSE);
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        SetLastError(ERROR_NETWORK_UNREACHABLE);
        return FALSE;
    }
    memset(&session, 0, sizeof(session));
    session.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    session.winsock_started = 1u;
    if (session.socket == INVALID_SOCKET) {
        reset_session();
        SetLastError(ERROR_NETWORK_UNREACHABLE);
        return FALSE;
    }
    memset(&bind_address, 0, sizeof(bind_address));
    bind_address.sin_family = AF_INET;
    bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_address.sin_port = htons((u_short)(config->local_role ==
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ? config->port : 0u));
    if (bind(session.socket, (const struct sockaddr *)&bind_address, sizeof(bind_address)) == SOCKET_ERROR ||
        ioctlsocket(session.socket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        reset_session();
        SetLastError(ERROR_NETWORK_UNREACHABLE);
        return FALSE;
    }
    session.config = *config;
    if (config->remote_ipv4 != NULL) {
        if (strlen(config->remote_ipv4) + 1u > sizeof(session.remote_ipv4)) {
            reset_session();
            SetLastError(ERROR_INVALID_ADDRESS);
            return FALSE;
        }
        strcpy(session.remote_ipv4, config->remote_ipv4);
        session.config.remote_ipv4 = session.remote_ipv4;
    }
    session.config.timeout_ms = config->timeout_ms == 0u ?
        LAN_ARENA_DEFAULT_TIMEOUT_MS : config->timeout_ms;
    session.connection.phase = config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
        SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING : SUDEKIMP_LAN_ARENA_CONNECTION_JOINING;
    session.next_sequence = 0u;
    if (config->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        session.connection.session_token = generate_client_token();
        memset(&session.peer_address, 0, sizeof(session.peer_address));
        session.peer_address.sin_family = AF_INET;
        session.peer_address.sin_port = htons((u_short)config->port);
        if (InetPtonA(AF_INET, config->remote_ipv4, &session.peer_address.sin_addr) != 1) {
            reset_session();
            SetLastError(ERROR_INVALID_ADDRESS);
            return FALSE;
        }
        session.peer_pinned = 1u;
    }
    SudekiMpLogFormat(
        "lan_arena_session event=start role=%s node=%s port=%u "
        "policy=direct_ipv4_udp_cleanroom_only\r\n",
        config->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ? "Tal_host" : "Ailish_client",
        config->local_simulation_node_role ==
                SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ?
            "canonical_native_world" : "replica",
        config->port
    );
    return TRUE;
}

static void session_stop_unlocked(BOOL notify_peer) {
    if (notify_peer && session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED) {
        SudekiMpLanArenaPacket end;
        memset(&end, 0, sizeof(end));
        end.type = SUDEKIMP_LAN_ARENA_PACKET_END;
        end.sequence = ++session.next_sequence;
        end.session_token = session.connection.session_token;
        (void)socket_send_packet(&end);
    }
    reset_session();
}

static void session_poll_unlocked(uint32_t now_ms) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    struct sockaddr_in source;
    int source_size;
    int received;
    unsigned int processed_packets;
    SudekiMpLanArenaPacket packet;
    if (session.socket == INVALID_SOCKET ||
        session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED ||
        session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT ||
        session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_ENDED) {
        return;
    }
    if (session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_JOINING &&
        (session.last_hello_at_ms == 0u ||
         (uint32_t)(now_ms - session.last_hello_at_ms) >= LAN_ARENA_HELLO_INTERVAL_MS)) {
        SudekiMpLanArenaPacket hello;
        fill_hello(&hello, SUDEKIMP_LAN_ARENA_PACKET_HELLO);
        if (!socket_send_packet(&hello)) {
            set_failed(SUDEKIMP_LAN_ARENA_REJECT_MALFORMED, "hello_send_failed");
            return;
        }
        session.last_hello_at_ms = now_ms;
    }
    for (processed_packets = 0u;
         processed_packets < LAN_ARENA_MAX_PACKETS_PER_POLL;
         ++processed_packets) {
        source_size = sizeof(source);
        received = recvfrom(session.socket, (char *)bytes, sizeof(bytes), 0,
            (struct sockaddr *)&source, &source_size);
        if (received == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
            set_failed(SUDEKIMP_LAN_ARENA_REJECT_MALFORMED, "receive_failed");
            return;
        }
        if (!SudekiMpLanArenaDecodePacket(bytes, (size_t)received, &packet)) {
            continue;
        }
        if (session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING) {
            if (packet.type == SUDEKIMP_LAN_ARENA_PACKET_HELLO) {
                accept_host_hello(&packet, &source, now_ms);
            }
            continue;
        }
        if (!session.peer_pinned || !address_equal(&source, &session.peer_address)) {
            continue;
        }
        if (session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_JOINING) {
            if (packet.type == SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK) {
                accept_client_ack(&packet, now_ms);
            } else if (packet.type == SUDEKIMP_LAN_ARENA_PACKET_REJECT) {
                set_failed(packet.body.reject_reason, "client_rejected");
            }
            continue;
        }
        if (session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED) {
            SudekiMpLanArenaRejectReason reason;
            if (!SudekiMpLanArenaPacketAllowedForNode(
                    session.config.local_simulation_node_role,
                    packet.type)) {
                set_failed(SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY,
                    "packet_node_authority");
                return;
            }
            if (!SudekiMpLanArenaConnectionAcceptPacket(&session.connection, &packet, now_ms, &reason)) {
                continue;
            }
            if (packet.type == SUDEKIMP_LAN_ARENA_PACKET_INPUT &&
                session.config.local_simulation_node_role ==
                    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) {
                uint8_t latched_weak_attack = session.input_pending ?
                    session.latest_input.weak_attack_pressed : 0u;
                uint8_t latched_combat_toggle = session.input_pending ?
                    session.latest_input.cleanroom_combat_test_pressed : 0u;
                uint8_t latched_skill = session.input_pending ?
                    session.latest_input.skill_pressed : 0u;
                uint8_t latched_skill_slot = session.input_pending ?
                    session.latest_input.skill_slot : 0u;
                uint8_t latched_kit = session.input_pending ?
                    session.latest_input.kit_action : 0u;
                uint8_t latched_kit_slot = session.input_pending ?
                    session.latest_input.kit_slot : 0u;
                if (!SudekiMpLanArenaPlayerOwnsActor(
                        (SudekiMpLanArenaRole)session.connection.peer_role,
                        packet.body.input.actor_type)) {
                    set_failed(SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY,
                        "input_actor_authority");
                    return;
                }
                if ((packet.body.input.acknowledged_snapshot != 0u &&
                     session.last_sent_snapshot_sequence == 0u) ||
                    SudekiMpLanArenaSequenceNewer(
                        packet.body.input.acknowledged_snapshot,
                        session.last_sent_snapshot_sequence)) {
                    set_failed(SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE,
                        "input_ack_ahead_of_host");
                    return;
                }
                session.latest_input = packet.body.input;
                if (latched_weak_attack != 0u) {
                    session.latest_input.weak_attack_pressed = 1u;
                }
                if (latched_combat_toggle != 0u) {
                    session.latest_input.cleanroom_combat_test_pressed = 1u;
                }
                if (latched_skill != 0u) {
                    session.latest_input.skill_pressed = 1u;
                    session.latest_input.skill_slot = latched_skill_slot;
                    session.latest_input.kit_action = 0u;
                    session.latest_input.kit_slot = 0u;
                }
                if (latched_kit != 0u) {
                    session.latest_input.kit_action = latched_kit;
                    session.latest_input.kit_slot = latched_kit_slot;
                }
                if (session.latest_input.kit_action != 0u) {
                    session.latest_input.skill_pressed = 0u;
                    session.latest_input.skill_slot = 0u;
                    session.latest_input.weak_attack_pressed = 0u;
                    session.latest_input.weak_attack_held = 0u;
                }
                session.input_pending = 1u;
                session.last_input_sequence = packet.sequence;
            } else if (packet.type == SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT &&
                       session.config.local_simulation_node_role ==
                           SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA) {
                if ((packet.body.snapshot.acknowledged_input != 0u &&
                     session.last_sent_input_sequence == 0u) ||
                    SudekiMpLanArenaSequenceNewer(
                        packet.body.snapshot.acknowledged_input,
                        session.last_sent_input_sequence)) {
                    set_failed(SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE,
                        "snapshot_ack_ahead_of_client");
                    return;
                }
                if (session.snapshot_queue_count ==
                        LAN_ARENA_SNAPSHOT_QUEUE_CAPACITY) {
                    session.snapshot_queue_head = (uint8_t)(
                        (session.snapshot_queue_head + 1u) %
                        LAN_ARENA_SNAPSHOT_QUEUE_CAPACITY);
                    --session.snapshot_queue_count;
                    SudekiMpLogWrite(
                        "lan_arena_session event=snapshot_queue phase=drop_oldest "
                        "reason=game_thread_stall capacity=8 "
                        "policy=retain_newest_ordered_history\r\n");
                }
                session.snapshot_queue[(session.snapshot_queue_head +
                    session.snapshot_queue_count) %
                    LAN_ARENA_SNAPSHOT_QUEUE_CAPACITY] = packet.body.snapshot;
                ++session.snapshot_queue_count;
                session.last_snapshot_sequence = packet.sequence;
            } else if (packet.type != SUDEKIMP_LAN_ARENA_PACKET_END &&
                       packet.type != SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE) {
                set_failed(SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY, "wrong_authority_packet");
                return;
            }
        }
    }
    if (session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED &&
        (uint32_t)(now_ms - session.last_keepalive_at_ms) >=
            LAN_ARENA_KEEPALIVE_INTERVAL_MS) {
        SudekiMpLanArenaPacket keepalive;
        memset(&keepalive, 0, sizeof(keepalive));
        keepalive.type = SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE;
        keepalive.sequence = ++session.next_sequence;
        keepalive.session_token = session.connection.session_token;
        if (!socket_send_packet(&keepalive)) {
            set_failed(SUDEKIMP_LAN_ARENA_REJECT_MALFORMED,
                "keepalive_send_failed");
            return;
        }
        session.last_keepalive_at_ms = now_ms;
        if (!session.keepalive_logged) {
            session.keepalive_logged = 1u;
            SudekiMpLogFormat(
                "lan_arena_session event=keepalive_active role=%s interval_ms=%u "
                "policy=authenticated_transport_health_independent_of_gameplay_activity\r\n",
                session.config.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
                    "Tal_host" : "Ailish_client",
                (unsigned int)LAN_ARENA_KEEPALIVE_INTERVAL_MS);
        }
    }
    if (SudekiMpLanArenaConnectionTimedOut(
            &session.connection, now_ms, session.config.timeout_ms)) {
        set_failed(SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT, "peer_timeout");
    }
}

static BOOL session_get_status_unlocked(SudekiMpLanArenaSessionStatus *status) {
    if (status == NULL) return FALSE;
    memset(status, 0, sizeof(*status));
    status->phase = session.connection.phase;
    status->failure = session.failure;
    status->session_token = session.connection.session_token;
    status->peer_connected = session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
    status->local_role = (uint8_t)session.config.local_role;
    status->local_simulation_node_role =
        (uint8_t)session.config.local_simulation_node_role;
    status->peer_simulation_node_role =
        session.connection.peer_simulation_node_role;
    return session.socket != INVALID_SOCKET;
}

static BOOL session_take_remote_input_unlocked(SudekiMpLanArenaInput *input) {
    if (input == NULL || !session.input_pending ||
        session.config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        session.config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        session.connection.phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED) return FALSE;
    *input = session.latest_input;
    session.input_pending = 0u;
    return TRUE;
}

static BOOL session_take_remote_snapshot_unlocked(SudekiMpLanArenaSnapshot *snapshot) {
    if (snapshot == NULL || session.snapshot_queue_count == 0u ||
        session.config.local_role != SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ||
        session.config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA ||
        session.connection.phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED) return FALSE;
    *snapshot = session.snapshot_queue[session.snapshot_queue_head];
    session.snapshot_queue_head = (uint8_t)(
        (session.snapshot_queue_head + 1u) %
        LAN_ARENA_SNAPSHOT_QUEUE_CAPACITY);
    --session.snapshot_queue_count;
    return TRUE;
}

static BOOL session_send_input_unlocked(const SudekiMpLanArenaInput *input) {
    SudekiMpLanArenaPacket packet;
    if (input == NULL ||
        session.config.local_role != SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ||
        session.config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA ||
        session.connection.phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED ||
        !SudekiMpLanArenaPlayerOwnsActor(
            session.config.local_role, input->actor_type)) return FALSE;
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    packet.sequence = ++session.next_sequence;
    packet.session_token = session.connection.session_token;
    packet.body.input = *input;
    packet.body.input.sequence = packet.sequence;
    packet.body.input.acknowledged_snapshot = session.last_snapshot_sequence;
    if (!socket_send_packet(&packet)) return FALSE;
    session.last_sent_input_sequence = packet.sequence;
    return TRUE;
}

static BOOL session_send_snapshot_unlocked(const SudekiMpLanArenaSnapshot *snapshot) {
    SudekiMpLanArenaPacket packet;
    if (snapshot == NULL ||
        session.config.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ||
        session.config.local_simulation_node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        session.connection.phase != SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED) return FALSE;
    /* Receipt is not simulation admission.  The canonical game-thread
     * reducer supplies the newest input it actually admitted; transport only
     * verifies that acknowledgement cannot point beyond the receive stream. */
    if ((snapshot->acknowledged_input != 0u &&
         session.last_input_sequence == 0u) ||
        SudekiMpLanArenaSequenceNewer(
            snapshot->acknowledged_input,
            session.last_input_sequence)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    memset(&packet, 0, sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    packet.sequence = ++session.next_sequence;
    packet.session_token = session.connection.session_token;
    packet.body.snapshot = *snapshot;
    packet.body.snapshot.sequence = packet.sequence;
    if (!socket_send_packet(&packet)) return FALSE;
    session.last_sent_snapshot_sequence = packet.sequence;
    return TRUE;
}

static BOOL session_connected_unlocked(void) {
    return session.connection.phase == SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
}

static BOOL session_get_display_endpoint_unlocked(
    char *address,
    size_t address_capacity,
    unsigned int *port
) {
    char hostname[256];
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *current;
    BOOL found = FALSE;

    if (address == NULL || address_capacity == 0u || port == NULL ||
        session.socket == INVALID_SOCKET) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    address[0] = '\0';
    *port = session.config.port;
    if (session.config.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH) {
        if (session.config.remote_ipv4 == NULL ||
            strlen(session.config.remote_ipv4) + 1u > address_capacity) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        strcpy(address, session.config.remote_ipv4);
        return TRUE;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR ||
        getaddrinfo(hostname, NULL, &hints, &results) != 0) {
        strcpy(address, "0.0.0.0");
        return TRUE;
    }
    for (current = results; current != NULL; current = current->ai_next) {
        const struct sockaddr_in *candidate =
            (const struct sockaddr_in *)current->ai_addr;
        if (candidate != NULL && candidate->sin_family == AF_INET &&
            candidate->sin_addr.s_addr != htonl(INADDR_LOOPBACK) &&
            InetNtopA(AF_INET, (void *)&candidate->sin_addr, address,
                (DWORD)address_capacity) != NULL) {
            found = TRUE;
            break;
        }
    }
    freeaddrinfo(results);
    if (!found) strcpy(address, "0.0.0.0");
    return TRUE;
}

BOOL SudekiMpLanArenaSessionStart(const SudekiMpLanArenaSessionConfig *config) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_start_unlocked(config);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

void SudekiMpLanArenaSessionStop(BOOL notify_peer) {
    AcquireSRWLockExclusive(&session_lock);
    session_stop_unlocked(notify_peer);
    ReleaseSRWLockExclusive(&session_lock);
}

void SudekiMpLanArenaSessionPoll(uint32_t now_ms) {
    AcquireSRWLockExclusive(&session_lock);
    session_poll_unlocked(now_ms);
    ReleaseSRWLockExclusive(&session_lock);
}

BOOL SudekiMpLanArenaSessionGetStatus(SudekiMpLanArenaSessionStatus *status) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_get_status_unlocked(status);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

BOOL SudekiMpLanArenaSessionTakeRemoteInput(SudekiMpLanArenaInput *input) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_take_remote_input_unlocked(input);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

BOOL SudekiMpLanArenaSessionTakeRemoteSnapshot(SudekiMpLanArenaSnapshot *snapshot) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_take_remote_snapshot_unlocked(snapshot);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

BOOL SudekiMpLanArenaSessionSendInput(const SudekiMpLanArenaInput *input) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_send_input_unlocked(input);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

BOOL SudekiMpLanArenaSessionSendSnapshot(const SudekiMpLanArenaSnapshot *snapshot) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_send_snapshot_unlocked(snapshot);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

BOOL SudekiMpLanArenaSessionConnected(void) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_connected_unlocked();
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}

BOOL SudekiMpLanArenaSessionGetDisplayEndpoint(
    char *address,
    size_t address_capacity,
    unsigned int *port
) {
    BOOL result;
    AcquireSRWLockExclusive(&session_lock);
    result = session_get_display_endpoint_unlocked(
        address, address_capacity, port);
    ReleaseSRWLockExclusive(&session_lock);
    return result;
}
