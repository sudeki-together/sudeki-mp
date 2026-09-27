#ifndef SUDEKIMP_LAN_PARTY_SESSION_H
#define SUDEKIMP_LAN_PARTY_SESSION_H

#include "network/lan_arena_protocol.h"
#include <windows.h>

/* Opt-in fixed roster; neither seat numbers nor character identities confer
 * simulation authority. Only seat zero owns the native world. The legacy
 * two-player session and LA42 packet format remain available unchanged. */
#define SUDEKIMP_LAN_PARTY_PLAYERS 4u
#define SUDEKIMP_LAN_PARTY_CHUNKS 2u
#define SUDEKIMP_LAN_PARTY_VERSION 3u
#define SUDEKIMP_LAN_PARTY_BUILD_ID 0x34500003u
#define SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS 250u
#define SUDEKIMP_LAN_PARTY_EXTENSION_VERSION 1u
#define SUDEKIMP_LAN_PARTY_EXTENSION_COMBAT_INPUT 0x01u
#define SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON 0x02u
#define SUDEKIMP_LAN_PARTY_EXTENSION_SUPPORTED 0x03u

typedef enum SudekiMpLanPartyPhase {
    SUDEKIMP_LAN_PARTY_FREE = 0,
    SUDEKIMP_LAN_PARTY_JOINING,
    SUDEKIMP_LAN_PARTY_PENDING,
    SUDEKIMP_LAN_PARTY_ACTIVE,
    SUDEKIMP_LAN_PARTY_DRAINING,
    SUDEKIMP_LAN_PARTY_REJECTED
} SudekiMpLanPartyPhase;

typedef struct SudekiMpLanPartyLease {
    uint64_t token;
    uint32_t generation;
    uint8_t seat;
} SudekiMpLanPartyLease;

typedef struct SudekiMpLanPartyPeerStatus {
    SudekiMpLanPartyLease lease;
    SudekiMpLanPartyPhase phase;
    SudekiMpLanArenaRejectReason failure;
    uint32_t last_input_sequence;
    uint32_t admitted_input_sequence;
    uint32_t last_input_received_at_ms;
    uint8_t transport_confirmed; /* peer echoed the host-issued token */
} SudekiMpLanPartyPeerStatus;

typedef struct SudekiMpLanPartyConfig {
    uint8_t local_seat; /* 0 Buki host, 1 Elco, 2 Tal, 3 Ailish */
    uint8_t game_hash[SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE];
    const char *host_ipv4; /* clients only */
    unsigned int port; /* host may request an ephemeral port with zero */
    uint32_t timeout_ms;
} SudekiMpLanPartyConfig;

/* Optional negotiated SMP4 sidecar. The existing SMP4 v3 envelope and
 * nested LA42 input remain byte-for-byte unchanged for peers without these
 * capabilities. Strong/Sweep are rising edges; block is a held state. */
typedef struct SudekiMpLanPartyCombatInputExtension {
    uint8_t strong_pressed;
    uint8_t sweep_pressed;
    uint8_t block_held;
} SudekiMpLanPartyCombatInputExtension;

/* Buki-host observation for Ailish's own first-person renderer. This never
 * grants client weapon or projectile authority. */
typedef struct SudekiMpLanPartyAilishWeaponState {
    uint8_t valid;
    uint8_t stage;
    uint8_t item;
    uint16_t charge_q8;
    uint16_t reload_ms;
    uint16_t reload_sequence;
} SudekiMpLanPartyAilishWeaponState;

/* A single canonical tick encoded as two MTU-bounded actor chunks. Chunk 0
 * carries Buki/Elco and the world/dummy journal; chunk 1 carries Tal/Ailish
 * and no enemies. Cast/audio/VFX seat indices are chunk-local, NOT network
 * roles. Complete visual rosters are per chunk. No partial frame may reach
 * native presentation. Each chunk retains LA42's authored animation/weapon
 * histories and independent cast camera/fade observations. */
typedef struct SudekiMpLanPartyFrame {
    SudekiMpLanArenaSnapshot chunk[SUDEKIMP_LAN_PARTY_CHUNKS];
    SudekiMpLanPartyAilishWeaponState ailish_weapon;
} SudekiMpLanPartyFrame;

typedef struct SudekiMpLanPartyInput {
    SudekiMpLanPartyLease lease;
    SudekiMpLanArenaInput input;
    SudekiMpLanPartyCombatInputExtension combat;
    uint32_t received_at_ms; /* host clock, never a client-supplied timestamp */
} SudekiMpLanPartyInput;

typedef struct SudekiMpLanPartySession SudekiMpLanPartySession;

uint8_t SudekiMpLanPartyActorType(unsigned int seat);
BOOL SudekiMpLanPartyFrameValid(const SudekiMpLanPartyFrame *frame);

/* All methods synchronize plain transport data only. No native pointers,
 * callbacks, or game objects enter this module. Destroy requires the caller
 * to stop/join its network worker first. */
SudekiMpLanPartySession *SudekiMpLanPartyCreate(const SudekiMpLanPartyConfig *config);
void SudekiMpLanPartyDestroy(SudekiMpLanPartySession *session, BOOL notify);
unsigned int SudekiMpLanPartyPort(SudekiMpLanPartySession *session);
/* Immutable endpoint role, not inferred from a remote peer's status. */
unsigned int SudekiMpLanPartyLocalSeat(SudekiMpLanPartySession *session);
void SudekiMpLanPartyPoll(SudekiMpLanPartySession *session, uint32_t now_ms);
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *session,
    unsigned int seat, SudekiMpLanPartyPeerStatus *status);

/* The host's game-thread coordinator must acquire the actor's exact native
 * control lease BEFORE approval. Transport connection alone never grants
 * input. Disconnect closes admission immediately but leaves the peer in
 * DRAINING until native tasks/ownership have positively retired. */
BOOL SudekiMpLanPartyApprove(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease);
BOOL SudekiMpLanPartyDisconnect(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease);
BOOL SudekiMpLanPartyReleaseDrained(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease);
BOOL SudekiMpLanPartyLeaseActive(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease);
BOOL SudekiMpLanPartyTakeInput(SudekiMpLanPartySession *session,
    unsigned int seat, SudekiMpLanPartyInput *input);
BOOL SudekiMpLanPartyAdmitInput(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyInput *input);
BOOL SudekiMpLanPartySendInput(SudekiMpLanPartySession *session,
    const SudekiMpLanArenaInput *input);
BOOL SudekiMpLanPartySendInputExtended(SudekiMpLanPartySession *session,
    const SudekiMpLanArenaInput *input,
    const SudekiMpLanPartyCombatInputExtension *combat);
BOOL SudekiMpLanPartyExtensionReady(SudekiMpLanPartySession *session);
BOOL SudekiMpLanPartySendFrame(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyFrame *frame);
BOOL SudekiMpLanPartyTakeFrame(SudekiMpLanPartySession *session,
    SudekiMpLanPartyFrame *frame);

#endif
