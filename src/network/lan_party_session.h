#ifndef SUDEKIMP_LAN_PARTY_SESSION_H
#define SUDEKIMP_LAN_PARTY_SESSION_H

#include "network/lan_arena_protocol.h"
#include "network/lan_party_story.h"
#include <windows.h>

/* Opt-in fixed roster; neither seat numbers nor character identities confer
 * simulation authority. Only seat zero owns the native world. The legacy
 * two-player session and LA42 packet format remain available unchanged. */
#define SUDEKIMP_LAN_PARTY_PLAYERS 4u
#define SUDEKIMP_LAN_PARTY_CHUNKS 2u
#define SUDEKIMP_LAN_PARTY_VERSION 12u
#define SUDEKIMP_LAN_PARTY_BUILD_ID 0x3450000cu
#define SUDEKIMP_LAN_PARTY_MODE_MAX_AGE_MS 500u
#define SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS 250u
#define SUDEKIMP_LAN_PARTY_EXTENSION_VERSION 3u
#define SUDEKIMP_LAN_PARTY_EXTENSION_COMBAT_INPUT 0x01u
#define SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON 0x02u
#define SUDEKIMP_LAN_PARTY_EXTENSION_SUPPORTED 0x03u

typedef enum SudekiMpLanPartyPhase {
    SUDEKIMP_LAN_PARTY_FREE = 0,
    SUDEKIMP_LAN_PARTY_JOINING,
    SUDEKIMP_LAN_PARTY_PENDING,
    SUDEKIMP_LAN_PARTY_ACTIVE,
    SUDEKIMP_LAN_PARTY_DRAINING,
    SUDEKIMP_LAN_PARTY_REJECTED,
    /* Transport-only story observer. Never a native actor/input lease. */
    SUDEKIMP_LAN_PARTY_OBSERVING
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
    /* Explicit private profile, negotiated before a session is allocated.
     * Zero preserves testroom behavior; one admits scene observation only. */
    uint8_t story_observation;
    /* Optional title-lobby admission. Fixed launch tickets use the existing
     * HELLO nonce field; ordinary SMP4 profiles keep random nonces. A new
     * native lease still requires the host's fresh token/generation/ACK. */
    uint8_t lobby_members;
    uint64_t lobby_nonce[4];
} SudekiMpLanPartyConfig;

/* Optional negotiated SMP4 sidecar. The nested LA42 input is unchanged;
 * SMP4 v5 requires matching builds for independent ranged shot journals.
 * Strong/Sweep are rising edges; block is a held state. */
typedef struct SudekiMpLanPartyCombatInputExtension {
    uint8_t strong_pressed;
    uint8_t sweep_pressed;
    uint8_t block_held;
    uint8_t flight_held; /* v10: Elco action held outside combat, never fuel/position */
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
    uint8_t shot_count;
    SudekiMpLanWeaponShot shots[SUDEKIMP_LAN_WEAPON_SHOT_HISTORY];
} SudekiMpLanPartyAilishWeaponState;
BOOL SudekiMpLanPartyAilishWeaponJournal(
    const SudekiMpLanPartyAilishWeaponState *state, SudekiMpLanWeaponState *journal);

/* Independently negotiated presentation sidecar. SMP4 v5 admits the three
 * exact native firing clips for each ranged actor without changing LA42. */
typedef struct SudekiMpLanPartyRangedPresentation {
    uint8_t valid, held, clip, state; /* 0 empty; 1..3 validated actor world fires */
    uint16_t sequence;
    float rate, time, blend;
} SudekiMpLanPartyRangedPresentation;
BOOL SudekiMpLanPartyRangedPresentationValid(
    const SudekiMpLanPartyRangedPresentation *presentation);

/* v10: host-owned fuel, native flight phase, and testroom fixtures.
 * No native pointers or client fuel/position claims. */
typedef struct SudekiMpLanPartyJetpackState {
    uint8_t valid, crystal_present, crystal_active, infinite;
    float fuel, maximum, rate;
    float crystal_position[3];
    uint8_t flight_phase, platform_present;
    float platform_position[3];
} SudekiMpLanPartyJetpackState;
BOOL SudekiMpLanPartyJetpackStateValid(const SudekiMpLanPartyJetpackState *state);

/* A single canonical tick encoded as two MTU-bounded actor chunks. Chunk 0
 * carries Buki/Elco and the world/dummy journal; chunk 1 carries Tal/Ailish
 * and no enemies. Cast/audio/VFX seat indices are chunk-local, NOT network
 * roles. Complete visual rosters are per chunk. No partial frame may reach
 * native presentation. Each chunk retains LA42's authored animation/weapon
 * histories and independent cast camera/fade observations. */
typedef struct SudekiMpLanPartyFrame {
    SudekiMpLanArenaSnapshot chunk[SUDEKIMP_LAN_PARTY_CHUNKS];
    SudekiMpLanPartyAilishWeaponState ailish_weapon;
    SudekiMpLanPartyJetpackState jetpack;
    SudekiMpLanPartyRangedPresentation ranged[2]; /* Elco, Ailish */
} SudekiMpLanPartyFrame;

/* Host observation, independent of animation-frame readiness. Sequence and
 * host_tick identify the transition; observed_tick advances only when the
 * host game thread positively observes the current mode. Packet repetition
 * alone cannot renew an expired observation. Native readiness is separate. */
typedef struct SudekiMpLanPartyCombatMode {
    uint32_t sequence, host_tick, observed_tick;
    uint8_t enabled;
} SudekiMpLanPartyCombatMode;
BOOL SudekiMpLanPartyCombatModeFrameReady(
    const SudekiMpLanPartyCombatMode *mode, const SudekiMpLanPartyFrame *frame);

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
BOOL SudekiMpLanPartyStoryObservation(SudekiMpLanPartySession *session);
/* Game-thread observation only; the worker cannot refresh host evidence.
 * No actor, scene load, camera or input permission is conferred by these APIs. */
BOOL SudekiMpLanPartyPublishStoryScene(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease, uint32_t now,
    SudekiMpLanStoryScene *scene);
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
/* Client owner only, after native/presentation cleanup. Ordinary profiles
 * use a fresh nonce; a lobby launch retains its seat's admission ticket.
 * Both require a fresh host token/generation on the same endpoint. */
BOOL SudekiMpLanPartyClientRejoin(SudekiMpLanPartySession *session);
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
/* Host game-thread observation; sends immediately on change/new ownership,
 * otherwise repeats at most every 100ms. Transport stores plain data only. */
BOOL SudekiMpLanPartyPublishCombatMode(SudekiMpLanPartySession *session,
    uint8_t enabled, uint32_t host_tick);
/* Client only, exact active lease and fresh local receipt required. Failure
 * leaves output untouched; it never means noncombat or safe native release. */
BOOL SudekiMpLanPartyGetCombatMode(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease, uint32_t now_ms,
    SudekiMpLanPartyCombatMode *mode);
BOOL SudekiMpLanPartySendFrame(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyFrame *frame);
BOOL SudekiMpLanPartyTakeFrame(SudekiMpLanPartySession *session,
    SudekiMpLanPartyFrame *frame);

#endif
