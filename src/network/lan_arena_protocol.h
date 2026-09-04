#ifndef SUDEKIMP_LAN_ARENA_PROTOCOL_H
#define SUDEKIMP_LAN_ARENA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* This protocol is deliberately separate from input/bridge_protocol.h.  The
 * latter is trusted loopback transport for local pads; LAN packets are
 * untrusted and must carry a session token, role, map, and build identity. */
#define SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION 22u
#define SUDEKIMP_LAN_ARENA_DEFAULT_PORT 26770u
#define SUDEKIMP_LAN_ARENA_BUILD_ID 0x4c413232u /* "LA22" */
#define SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE 32u
#define SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE 768u
#define SUDEKIMP_LAN_ARENA_MAX_ENEMIES 16u
#define SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE 10000000u
#define SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE 256.0f
#define SUDEKIMP_LAN_ARENA_MAP_CLEANROOM 1u
#define SUDEKIMP_LAN_ARENA_TAL_TYPE 0x23u
#define SUDEKIMP_LAN_ARENA_AILISH_TYPE 0x01u
#define SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID 1u
#define SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY 4u
#define SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS 5u
#define SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_BLENDS 4u
#define SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY 8u
#define SUDEKIMP_LAN_ARENA_MAX_SNAPSHOT_PACKET_SIZE 746u

typedef enum SudekiMpLanArenaMatchState {
    SUDEKIMP_LAN_ARENA_MATCH_WAITING = 0,
    SUDEKIMP_LAN_ARENA_MATCH_ACTIVE = 1,
    SUDEKIMP_LAN_ARENA_MATCH_ENDED = 2
} SudekiMpLanArenaMatchState;

/* Ordinary locomotion/action presentation remains process-independent: never
 * send arbiter enums or use a native selector as gameplay authority. The one
 * exact-build exception is an authenticated active CSkill or Spirit Strike,
 * whose bounded actor-local renderer channels are copied below as
 * presentation-only data.
 * The client still maps all ordinary states through verified adapters. */
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
    SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED = 2,
    SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK = 3,
    SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK = 4,
    SUDEKIMP_LAN_ARENA_COMBAT_BLOCK = 5
} SudekiMpLanArenaCombatState;

/* Character skills and Spirit Strikes share an exact-build presentation
 * payload, but they do not share an execution path. A character skill may
 * start a validated local CSkill presentation task. A Spirit Strike is a
 * host-owned global transaction and is presentation-only on the client. */
typedef enum SudekiMpLanArenaSkillPresentationKind {
    SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE = 0,
    SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER = 1,
    SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT = 2
} SudekiMpLanArenaSkillPresentationKind;

/* Spirit audio crosses the LAN only as this closed semantic allowlist. Raw
 * XACT cue strings remain host-process trace evidence and can never become
 * packet-controlled arguments to the replica's sound engine. */
typedef enum SudekiMpLanArenaSpiritAudioCue {
    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_NONE = 0,
    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START = 1
} SudekiMpLanArenaSpiritAudioCue;

/* Bounded action variants remain process-independent. The host translates
 * verified native selectors into these values and each client translates
 * them back through its actor-specific presentation adapter. */
typedef enum SudekiMpLanArenaActionVariant {
    SUDEKIMP_LAN_ARENA_ACTION_NONE = 0,
    SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE = 1,
    SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO = 2,
    SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE = 3,
    SUDEKIMP_LAN_ARENA_ACTION_STRONG = 4,
    SUDEKIMP_LAN_ARENA_ACTION_SWEEP = 5,
    SUDEKIMP_LAN_ARENA_ACTION_BLOCK = 6,
    /* Sudeki resolves melee clips from the complete W/S history. These are
     * presentation identities, not requests to execute another attack. */
    SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO = 7,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS = 8,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW = 9,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS = 10,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS = 11,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW = 12,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW = 13,
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS = 14,
    /* The same WSS history has a second authored result selected by native
     * timing/target/direction gates. Preserve its distinct presentation
     * identity without pretending the client executed another attack. */
    SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE = 15,
    SUDEKIMP_LAN_ARENA_ACTION_MAX =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE
} SudekiMpLanArenaActionVariant;

typedef enum SudekiMpLanArenaRole {
    SUDEKIMP_LAN_ARENA_ROLE_INVALID = 0,
    SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL = 1,
    SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH = 2
} SudekiMpLanArenaRole;

/* Simulation authority is deliberately independent of player identity. The
 * current listen-server topology pairs Tal with the canonical native world
 * and Ailish with a replica, but the wire contract does not conflate them. */
typedef enum SudekiMpLanArenaSimulationNodeRole {
    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_INVALID = 0,
    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD = 1,
    SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA = 2
} SudekiMpLanArenaSimulationNodeRole;

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
    uint8_t simulation_node_role;
    uint8_t tal_type;
    uint8_t ailish_type;
    uint64_t session_token;
} SudekiMpLanArenaHello;

typedef struct SudekiMpLanArenaInput {
    uint32_t sequence;
    uint32_t acknowledged_snapshot;
    uint32_t client_tick;
    /* Every contribution names its player-owned actor. The authenticated
     * session role must match this field before canonical admission. */
    uint8_t actor_type;
    int16_t world_direction_x;
    int16_t world_direction_z;
    int16_t aim_direction_x;
    int16_t aim_direction_y;
    int16_t aim_direction_z;
    uint8_t weak_attack_pressed;
    uint8_t weak_attack_held;
    uint8_t ranged_first_person_active;
    /* Cleanroom-only test edge. Campaign combat begins from Sudeki's native
     * dungeon/world triggers; snapshots then replicate the observed native
     * flag. This field merely lets either arena window exercise that same
     * transition without pretending a player owns combat state. */
    uint8_t cleanroom_combat_test_pressed;
    /* One edge names a bounded actor-local CSkill slot. The host resolves it
     * against its own Ailish object and still runs Sudeki's validator. */
    uint8_t skill_pressed;
    uint8_t skill_slot;
} SudekiMpLanArenaInput;

typedef struct SudekiMpLanArenaActionEvent {
    uint16_t sequence;
    uint8_t variant;
    uint32_t host_tick;
} SudekiMpLanArenaActionEvent;

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
    /* Monotonic per actor and authored only by the shared simulation. This
     * distinguishes repeated clips that reuse the same semantic variant. */
    uint16_t action_sequence;
    /* Host-observed clock for the current semantic action. The exact game
     * hash in the handshake makes these presentation-time units compatible,
     * while the selector itself remains actor-local and never crosses the
     * wire. A synthetic/history-only action may leave this invalid. */
    uint16_t action_phase_q8;
    uint8_t action_phase_valid;
    /* The first non-action snapshot after a native clip carries both sides
     * of the host-observed handoff. The replica advances to the terminal
     * action pose during the buffered segment, then enters idle at the same
     * clock the host used instead of freezing and restarting from zero. */
    uint16_t action_terminal_phase_q8;
    uint16_t idle_entry_phase_q8;
    uint8_t action_retirement_valid;
    /* Host-observed skill presentation transaction. `skill_sequence`
     * advances once per admitted CSkill or Spirit activation. `skill_kind`
     * decides whether the replica may run a local CSkill task or must remain
     * presentation-only for the host-owned global Spirit transaction. */
    uint16_t skill_sequence;
    uint8_t skill_kind;
    uint8_t skill_slot;
    uint8_t skill_active;
    uint32_t skill_cost;
    /* Exact-build, actor-local presentation sampled from the authoritative
     * renderer while the corresponding native CSkill or host-owned Spirit
     * transaction is active. The
     * handshake pins both peers to the same executable and actor tuple, so
     * these selectors never cross a build or actor boundary.  They carry no
     * gameplay authority; the client may use them only for the matching
     * host-approved skill_sequence. */
    uint8_t skill_presentation_valid;
    uint8_t skill_presentation_channel_count;
    int32_t skill_presentation_selector[
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS];
    uint8_t skill_presentation_state[
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS];
    float skill_presentation_rate[
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS];
    float skill_presentation_time[
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS];
    float skill_presentation_blend[
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_BLENDS];
    uint8_t action_history_count;
    /* The latest semantic state alone can skip a fast combo stage between
     * 20 Hz snapshots. Keep a bounded chronological journal of host-observed
     * native action edges so the client can present every authored move once.
     * Four entries cover Tal's three-hit weak chain plus an adjacent action
     * while keeping a maximum-enemy packet below the bounded LAN datagram. */
    SudekiMpLanArenaActionEvent
        action_history[SUDEKIMP_LAN_ARENA_ACTION_HISTORY_CAPACITY];
} SudekiMpLanArenaActorSnapshot;

typedef struct SudekiMpLanArenaEnemySnapshot {
    uint32_t native_entity_id;
    float x;
    float y;
    float z;
    uint32_t hp;
    uint8_t combat_state;
} SudekiMpLanArenaEnemySnapshot;

typedef struct SudekiMpLanArenaSpiritAudioSemanticEvent {
    uint16_t event_sequence;
    uint16_t skill_sequence;
    uint8_t cue;
} SudekiMpLanArenaSpiritAudioSemanticEvent;

typedef struct SudekiMpLanArenaSpiritAudioCursor {
    uint16_t last_event_sequence;
    uint8_t initialized;
} SudekiMpLanArenaSpiritAudioCursor;

typedef int (*SudekiMpLanArenaSpiritAudioSink)(
    void *context,
    SudekiMpLanArenaSpiritAudioCue cue
);

typedef struct SudekiMpLanArenaSnapshot {
    uint32_t sequence;
    uint32_t acknowledged_input;
    uint32_t host_tick;
    uint8_t match_state;
    uint8_t combat_enabled;
    SudekiMpLanArenaActorSnapshot tal;
    SudekiMpLanArenaActorSnapshot ailish;
    /* Bounded presentation-only journal. Every event is immutable, uses a
     * session-local nonzero modular sequence, and names the exact Tal Spirit
     * transaction that emitted it. Only the transaction's start edge is
     * admitted, so no redundant timing value crosses the wire. */
    uint8_t spirit_audio_history_count;
    SudekiMpLanArenaSpiritAudioSemanticEvent spirit_audio_history[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY];
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
    uint8_t expected_sender_simulation_node_role;
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
    uint8_t peer_simulation_node_role;
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
/* The supported executable uses three channel-zero renderer selectors while
 * one native Spirit transaction remains active. Keep this exact-build
 * allowlist shared by the untrusted wire gate and the client presentation
 * sink so an authored stage cannot disappear between those boundaries. */
int SudekiMpLanArenaSpiritPresentationSelectorValid(int32_t selector);
/* Presentation fields are optional renderer witnesses for character skills,
 * but mandatory for an active Spirit transaction.  Expose the exact wire
 * predicate so a producer can omit an unsafe optional witness without
 * suppressing the authoritative lifecycle snapshot that contains it. */
int SudekiMpLanArenaSkillPresentationValid(
    const SudekiMpLanArenaActorSnapshot *actor,
    uint8_t expected_type
);
int SudekiMpLanArenaSpiritAudioJournalValid(
    const SudekiMpLanArenaSnapshot *snapshot
);
void SudekiMpLanArenaSpiritAudioCursorReset(
    SudekiMpLanArenaSpiritAudioCursor *cursor
);
/* Consumes only the newest not-yet-seen journal edge. The sink is called at
 * most once and only while that event names the snapshot's exact active Tal
 * Spirit transaction. Older retained journal entries are recovery evidence,
 * not delayed audio work. */
int SudekiMpLanArenaSpiritAudioConsumeSnapshot(
    SudekiMpLanArenaSpiritAudioCursor *cursor,
    const SudekiMpLanArenaSnapshot *snapshot,
    SudekiMpLanArenaSpiritAudioSink sink,
    void *sink_context,
    unsigned int *replayed_count
);
int SudekiMpLanArenaInputValid(const SudekiMpLanArenaInput *input);
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
