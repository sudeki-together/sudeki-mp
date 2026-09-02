#ifndef SUDEKIMP_LAN_ARENA_PROTOCOL_H
#define SUDEKIMP_LAN_ARENA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* This protocol is deliberately separate from input/bridge_protocol.h.  The
 * latter is trusted loopback transport for local pads; LAN packets are
 * untrusted and must carry a session token, role, map, and build identity. */
#define SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION 6u
#define SUDEKIMP_LAN_ARENA_DEFAULT_PORT 26770u
#define SUDEKIMP_LAN_ARENA_BUILD_ID 0x4c414e36u /* "LAN6" */
#define SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE 32u
#define SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE 512u
#define SUDEKIMP_LAN_ARENA_MAX_ENEMIES 16u
#define SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE 10000000u
#define SUDEKIMP_LAN_ARENA_MAP_CLEANROOM 1u
#define SUDEKIMP_LAN_ARENA_TAL_TYPE 0x23u
#define SUDEKIMP_LAN_ARENA_AILISH_TYPE 0x01u
#define SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID 1u

typedef enum SudekiMpLanArenaMatchState {
    SUDEKIMP_LAN_ARENA_MATCH_WAITING = 0,
    SUDEKIMP_LAN_ARENA_MATCH_ACTIVE = 1,
    SUDEKIMP_LAN_ARENA_MATCH_ENDED = 2
} SudekiMpLanArenaMatchState;

/* Process-independent presentation states. Never send native animation
 * selectors or arbiter enums across the wire: those are engine-owned and can
 * differ by actor. The client maps these bounded states through verified
 * presentation adapters only. */
typedef enum SudekiMpLanArenaAnimationState {
    SUDEKIMP_LAN_ARENA_ANIMATION_IDLE = 0,
    SUDEKIMP_LAN_ARENA_ANIMATION_MOVING = 1,
    SUDEKIMP_LAN_ARENA_ANIMATION_ACTION = 2,
    SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED = 3,
    SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE = 4,
    SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO = 5
} SudekiMpLanArenaAnimationState;

typedef enum SudekiMpLanArenaCombatState {
    SUDEKIMP_LAN_ARENA_COMBAT_IDLE = 0,
    SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK = 1,
    SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED = 2
} SudekiMpLanArenaCombatState;

/* Bounded action variants remain process-independent. The host translates
 * verified native selectors into these values and each client translates
 * them back through its actor-specific presentation adapter. */
typedef enum SudekiMpLanArenaActionVariant {
    SUDEKIMP_LAN_ARENA_ACTION_NONE = 0,
    SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE = 1,
    SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO = 2,
    SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE = 3
} SudekiMpLanArenaActionVariant;

typedef enum SudekiMpLanArenaRole {
    SUDEKIMP_LAN_ARENA_ROLE_INVALID = 0,
    SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL = 1,
    SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH = 2
} SudekiMpLanArenaRole;

typedef enum SudekiMpLanArenaPacketType {
    SUDEKIMP_LAN_ARENA_PACKET_INVALID = 0,
    SUDEKIMP_LAN_ARENA_PACKET_HELLO = 1,
    SUDEKIMP_LAN_ARENA_PACKET_HELLO_ACK = 2,
    SUDEKIMP_LAN_ARENA_PACKET_REJECT = 3,
    SUDEKIMP_LAN_ARENA_PACKET_INPUT = 4,
    SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT = 5,
    SUDEKIMP_LAN_ARENA_PACKET_END = 6,
    SUDEKIMP_LAN_ARENA_PACKET_KEEPALIVE = 7
} SudekiMpLanArenaPacketType;

typedef enum SudekiMpLanArenaRejectReason {
    SUDEKIMP_LAN_ARENA_REJECT_NONE = 0,
    SUDEKIMP_LAN_ARENA_REJECT_VERSION,
    SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH,
    SUDEKIMP_LAN_ARENA_REJECT_BUILD,
    SUDEKIMP_LAN_ARENA_REJECT_MAP,
    SUDEKIMP_LAN_ARENA_REJECT_ROLE,
    SUDEKIMP_LAN_ARENA_REJECT_TOKEN,
    SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE,
    SUDEKIMP_LAN_ARENA_REJECT_MALFORMED,
    SUDEKIMP_LAN_ARENA_REJECT_BUSY,
    SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT,
    SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY
} SudekiMpLanArenaRejectReason;

typedef struct SudekiMpLanArenaHello {
    uint32_t sequence;
    uint32_t build_id;
    uint8_t game_hash[SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE];
    uint8_t map_id;
    uint8_t role;
    uint8_t tal_type;
    uint8_t ailish_type;
    uint64_t session_token;
} SudekiMpLanArenaHello;

typedef struct SudekiMpLanArenaInput {
    uint32_t sequence;
    uint32_t acknowledged_snapshot;
    uint32_t client_tick;
    int16_t world_direction_x;
    int16_t world_direction_z;
    uint8_t weak_attack_pressed;
} SudekiMpLanArenaInput;

typedef struct SudekiMpLanArenaActorSnapshot {
    uint8_t actor_type;
    uint8_t animation_state;
    uint8_t combat_state;
    uint8_t action_variant;
    uint32_t native_entity_id;
    float x;
    float y;
    float z;
    float facing_x;
    float facing_z;
    uint32_t hp;
    uint32_t sp;
} SudekiMpLanArenaActorSnapshot;

typedef struct SudekiMpLanArenaEnemySnapshot {
    uint32_t native_entity_id;
    float x;
    float y;
    float z;
    uint32_t hp;
    uint8_t combat_state;
} SudekiMpLanArenaEnemySnapshot;

typedef struct SudekiMpLanArenaSnapshot {
    uint32_t sequence;
    uint32_t acknowledged_input;
    uint32_t host_tick;
    uint8_t match_state;
    uint8_t combat_enabled;
    SudekiMpLanArenaActorSnapshot tal;
    SudekiMpLanArenaActorSnapshot ailish;
    uint8_t enemy_count;
    SudekiMpLanArenaEnemySnapshot enemies[SUDEKIMP_LAN_ARENA_MAX_ENEMIES];
} SudekiMpLanArenaSnapshot;

typedef struct SudekiMpLanArenaPacket {
    SudekiMpLanArenaPacketType type;
    uint32_t sequence;
    uint64_t session_token;
    union {
        SudekiMpLanArenaHello hello;
        SudekiMpLanArenaInput input;
        SudekiMpLanArenaSnapshot snapshot;
        SudekiMpLanArenaRejectReason reject_reason;
    } body;
} SudekiMpLanArenaPacket;

typedef struct SudekiMpLanArenaHandshakeExpectation {
    uint32_t build_id;
    const uint8_t *game_hash;
    uint8_t map_id;
    uint8_t expected_sender_role;
    uint8_t tal_type;
    uint8_t ailish_type;
    uint64_t expected_session_token;
} SudekiMpLanArenaHandshakeExpectation;

typedef enum SudekiMpLanArenaConnectionPhase {
    SUDEKIMP_LAN_ARENA_CONNECTION_IDLE = 0,
    SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING,
    SUDEKIMP_LAN_ARENA_CONNECTION_JOINING,
    SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED,
    SUDEKIMP_LAN_ARENA_CONNECTION_ENDED,
    SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED,
    SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT
} SudekiMpLanArenaConnectionPhase;

typedef struct SudekiMpLanArenaConnectionState {
    SudekiMpLanArenaConnectionPhase phase;
    uint32_t last_received_sequence;
    uint32_t last_received_at_ms;
    uint64_t session_token;
    uint8_t peer_role;
    uint8_t sequence_initialized;
} SudekiMpLanArenaConnectionState;

int SudekiMpLanArenaEncodePacket(
    uint8_t output[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE],
    size_t *output_size,
    const SudekiMpLanArenaPacket *packet
);
int SudekiMpLanArenaDecodePacket(
    const uint8_t *packet_bytes,
    size_t packet_size,
    SudekiMpLanArenaPacket *packet
);
/* Shared defense-in-depth gate used both at the wire boundary and before a
 * client snapshot enters interpolation history. */
int SudekiMpLanArenaSnapshotValid(
    const SudekiMpLanArenaSnapshot *snapshot
);
int SudekiMpLanArenaHandshakeValid(
    const SudekiMpLanArenaHello *hello,
    const SudekiMpLanArenaHandshakeExpectation *expectation,
    SudekiMpLanArenaRejectReason *reason
);
int SudekiMpLanArenaSequenceNewer(uint32_t candidate, uint32_t previous);
int SudekiMpLanArenaConnectionAcceptPacket(
    SudekiMpLanArenaConnectionState *state,
    const SudekiMpLanArenaPacket *packet,
    uint32_t now_ms,
    SudekiMpLanArenaRejectReason *reason
);
int SudekiMpLanArenaConnectionTimedOut(
    const SudekiMpLanArenaConnectionState *state,
    uint32_t now_ms,
    uint32_t timeout_ms
);

#endif
