#ifndef SUDEKIMP_LAN_PARTY_SESSION_H
#define SUDEKIMP_LAN_PARTY_SESSION_H

#include "network/lan_arena_protocol.h"
#include "network/lan_party_story.h"
#include "network/lan_story_frame.h"
#include "network/lan_story_world_frame.h"
#include "network/lan_story_handoff.h"
#include "network/lan_story_presentation.h"
#include "network/lan_story_catchup.h"
#include "network/lan_story_loot.h"
#include "engine/party_ownership.h"
#include <windows.h>

/* Opt-in fixed roster; neither seat numbers nor character identities confer
 * simulation authority. Only seat zero owns the native world. The legacy
 * two-player session and LA42 packet format remain available unchanged. */
#define SUDEKIMP_LAN_PARTY_PLAYERS 4u
#define SUDEKIMP_LAN_PARTY_CHUNKS 2u
#define SUDEKIMP_LAN_PARTY_VERSION 14u
#define SUDEKIMP_LAN_PARTY_BUILD_ID 0x3450000eu
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
    uint8_t local_seat; /* immutable connection slot; zero is authority */
    uint8_t game_hash[SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE];
    const char *host_ipv4; /* clients only */
    unsigned int port; /* host may request an ephemeral port with zero */
    uint32_t timeout_ms;
    /* Explicit private profile, negotiated before a session is allocated.
     * Zero preserves testroom behavior; one admits scene observation only;
     * two is the distinct saved-story runtime profile. Story transport stays
     * OBSERVING; saved-story movement requires its separate native ownership,
     * presentation ACK and fresh frame fence. Connectivity grants no input. */
    uint8_t story_observation;
    /* Optional title-lobby admission. Fixed launch tickets use the existing
     * HELLO nonce field; ordinary SMP4 profiles keep random nonces. A new
     * native lease still requires the host's fresh token/generation/ACK. */
    uint8_t lobby_members;
    uint64_t lobby_nonce[4];
    /* Explicit canonical character map; 4 is unassigned/spectating. Zero
     * assignment_enabled retains canonical profile defaults until the host
     * publishes authoritative presence. Never reinterpret a transport lease. */
    uint8_t assignment_enabled;
    uint8_t reserved_mask; /* optional explicit reservations, including offline players */
    uint8_t character[4];
    /* Dev Play lobby choices are separate from the unique native-party map.
     * 0..3 heroes, 4 none, 5 Talos. Duplicate heroes and Talos project to
     * character=4; this configuration alone grants no avatar input lease. */
    uint8_t dev_play;
    uint8_t dev_play_leader; /* Fingerprint-verified native save leader; no control authority. */
    uint8_t avatar[4];
} SudekiMpLanPartyConfig;

typedef enum SudekiMpLanPartyCommandKind {
    SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE = 1, /* value bit0 menu, bit1 Away */
    SUDEKIMP_LAN_PARTY_COMMAND_SWAP,        /* target = character */
    SUDEKIMP_LAN_PARTY_COMMAND_POLICY,      /* host; value = absence policy */
    SUDEKIMP_LAN_PARTY_COMMAND_PAUSE,       /* host; value = paused */
    SUDEKIMP_LAN_PARTY_COMMAND_RELEASE,     /* host; target = player */
    SUDEKIMP_LAN_PARTY_COMMAND_REASSIGN,    /* host; target = player, value = character */
    SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK,
    SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK,    /* transaction = original swap request */
    SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE
} SudekiMpLanPartyCommandKind;
typedef struct SudekiMpLanPartyCommand {
    /* Transport fills the verified sender lease when dequeuing. Host-local
     * commands have a zero lease. No native pointers cross this boundary. */
    SudekiMpLanPartyLease lease;
    uint32_t request, world, revision, generation, transaction;
    uint8_t kind, player, target, value;
} SudekiMpLanPartyCommand;
typedef struct SudekiMpLanPartyPresence {
    uint32_t sequence, observed_tick;
    SudekiMpPartyOwnership ownership;
    uint32_t ack_request[4];
    uint8_t ack_result[4]; /* SudekiMpPartySwapResult */
} SudekiMpLanPartyPresence;
BOOL SudekiMpLanPartyCommandValid(const SudekiMpLanPartyCommand *command);
BOOL SudekiMpLanPartyPresenceValid(const SudekiMpLanPartyPresence *presence);

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
    uint32_t world, revision, actor_generation; /* v13 assignment fence */
} SudekiMpLanPartyInput;

typedef struct SudekiMpLanPartySession SudekiMpLanPartySession;

/* Loot remains inert until the native coordinator binds a LOCAL verified
 * save identity. Packets cannot choose/rebind it. Snapshots carry absolute
 * character accounts and consumed world sources; Get never awards a delta.
 * These functions do not enable client interaction or touch native inventory. */
BOOL SudekiMpLanPartyBindStoryLootSave(SudekiMpLanPartySession *,const uint8_t save_identity[32]);
BOOL SudekiMpLanPartySendStoryLoot(SudekiMpLanPartySession *,const SudekiMpStoryLootState *,
    const SudekiMpLanStoryScene *fresh_native_scene);
BOOL SudekiMpLanPartyGetStoryLoot(SudekiMpLanPartySession *,const SudekiMpLanPartyLease *,
    uint32_t now,SudekiMpStoryLootState *);

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
/* Immutable local configuration, never inferred from received metadata. */
SudekiMpLanStoryPolicy SudekiMpLanPartyStoryPolicy(const SudekiMpLanPartySession *session);
unsigned int SudekiMpLanPartyLocalCharacter(SudekiMpLanPartySession *session);
unsigned int SudekiMpLanPartyPlayerCharacter(SudekiMpLanPartySession *session,
    unsigned int player);
BOOL SudekiMpLanPartyDevPlay(SudekiMpLanPartySession *session);
unsigned int SudekiMpLanPartyPlayerAvatar(SudekiMpLanPartySession *session,
    unsigned int player);
unsigned int SudekiMpLanPartyCharacterPlayer(SudekiMpLanPartySession *session,
    unsigned int character);
BOOL SudekiMpLanPartyStoryObservation(SudekiMpLanPartySession *session);
/* Plain-data mailboxes only. The verified game-thread coordinator validates
 * policy and native readiness, then publishes the confirmed result. Commands
 * retry until the host's matching ack; at most one is outstanding per player. */
BOOL SudekiMpLanPartyQueueCommand(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyCommand *command);
BOOL SudekiMpLanPartyTakeCommand(SudekiMpLanPartySession *session,
    SudekiMpLanPartyCommand *command);
BOOL SudekiMpLanPartyPublishPresence(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyPresence *presence);
BOOL SudekiMpLanPartyGetPresence(SudekiMpLanPartySession *session,
    SudekiMpLanPartyPresence *presence);
/* Host-only assignment publication closes queued input. Presence publication
 * validates/replaces this same map and additionally grants input eligibility. */
BOOL SudekiMpLanPartySetAssignment(SudekiMpLanPartySession *session,
    const SudekiMpPartyAssignment *assignment);
/* Game-thread registration of a freshly host-issued lobby admission ticket.
 * Existing active/draining peers cannot be replaced. Rejoin still needs a new
 * native approval and transport token. No story load or ownership is implied. */
BOOL SudekiMpLanPartyRegisterAdmission(SudekiMpLanPartySession *session,
    unsigned int player, uint64_t nonce);
/* Saved-story late admission sets only this FREE player's character map and
 * ticket. Native ownership remains AI until catch-up and control ACKs. */
BOOL SudekiMpLanPartyRegisterStoryAdmission(SudekiMpLanPartySession *session,
    unsigned player,unsigned character,uint64_t nonce);
BOOL SudekiMpLanPartyPublishStoryCatchup(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryCatchup *snapshot);
BOOL SudekiMpLanPartyGetStoryCatchup(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryCatchup *snapshot);
BOOL SudekiMpLanPartyAcknowledgeStoryCatchup(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t transaction,uint32_t presented_sequence);
/* Initial peers require no catch-up; late peers require the exact current
 * connection's ACK. A scene change cannot undo a completed admission. */
BOOL SudekiMpLanPartyStoryCatchupComplete(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease);
/* Also available to saved-story departure cleanup. Requires a FREE transport
 * slot and the exact retired ticket; does not admit a new story participant. */
BOOL SudekiMpLanPartyRevokeAdmission(SudekiMpLanPartySession *session,
    unsigned int player, uint64_t nonce);
/* Host-only plain-data observation. Lobby tickets are invalidated atomically
 * when gameplay starts draining, before the old transport becomes FREE. */
BOOL SudekiMpLanPartyAdmissionMatches(SudekiMpLanPartySession *session,
    unsigned int player, uint64_t nonce);
/* Game-thread observation only; the worker cannot refresh host evidence.
 * No actor, scene load, camera or input permission is conferred by these APIs. */
BOOL SudekiMpLanPartyPublishStoryScene(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease, uint32_t now,
    SudekiMpLanStoryScene *scene);
/* Sparse, noncombat story observations. These require the separate story
 * profile and a fresh host scene. Receipt grants no native/input authority. */
BOOL SudekiMpLanPartySendStoryFrame(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryFrame *frame);
BOOL SudekiMpLanPartyPopStoryFrame(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,
    SudekiMpLanStoryFrame *frame);
/* Preserve authenticated local receipt time across render/minimize stalls.
 * Dequeuing a packet never refreshes its age. */
BOOL SudekiMpLanPartyPopStoryFrameReceived(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,
    SudekiMpLanStoryFrame *frame,uint32_t *received_at);
/* Saved-story only. Complete, bounded world batches share the corresponding
 * party frame's epoch/revision/sequence/tick. Partial batches never escape
 * transport; no native objects are touched by the network worker. */
BOOL SudekiMpLanPartySendStoryWorld(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryWorldFrame *frame);
BOOL SudekiMpLanPartyPopStoryWorld(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,
    SudekiMpLanStoryWorldFrame *frame,uint32_t *received_at);
/* Separate saved-story native ownership exchange. OBSERVING remains transport
 * connectivity only. Host publishes PREPARE after acquiring a native lease;
 * client ACK follows actual local actor/view/input preparation. READY requires
 * that exact ACK. Input admission still requires a fresh game-thread native
 * lease check; the network worker never changes a character or native world. */
BOOL SudekiMpLanPartyPublishStoryControl(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlState *state);
BOOL SudekiMpLanPartyGetStoryControl(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryControlState *state);
BOOL SudekiMpLanPartyAcknowledgeStoryControl(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence);
/* Host renewal query: acknowledgment remains tied to the current connection,
 * scene and unrevoked fence across a publication gap. This does not admit
 * movement: publish a fresh READY after validating the native lease first. */
BOOL SudekiMpLanPartyStoryControlAcknowledged(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence);
BOOL SudekiMpLanPartySendStoryMovement(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryMovement *input);
BOOL SudekiMpLanPartyTakeStoryMovement(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryMovement *input,
    uint32_t *received_at);
/* Authenticated saved-story requests, bounded to one outstanding action per
 * player. Identical retries recover the immutable host result. Take consumes
 * admission once; only the game thread may execute, then publish its observed
 * outcome. A never-sent request whose displayed frame already expired may
 * receive a local EXPIRED result; a possibly sent request always recovers the
 * host result. A transport ACK cannot authorize a native cast. */
BOOL SudekiMpLanPartySendStoryAction(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryActionRequest *request);
BOOL SudekiMpLanPartyTakeStoryAction(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryActionRequest *request);
BOOL SudekiMpLanPartyPublishStoryActionResult(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryActionResult *result);
BOOL SudekiMpLanPartyGetStoryActionResult(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,SudekiMpLanStoryActionResult *result);
/* Dev Play ally seat combo reader (host -> client, latest wins, resent by the
 * host while bound). Accepted only under the peer's current ally control fence. */
BOOL SudekiMpLanPartyPublishStoryAllyHud(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryAllyHud *hud);
BOOL SudekiMpLanPartyGetStoryAllyHud(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryAllyHud *hud);
/* Dev Play only: host game-thread snapshots broadcast to every observing
 * peer, including spectators; host-local Get uses the same fresh plain cache.
 * A present=0 tombstone closes display immediately. Publish must increase
 * sequence on each fresh observation; identical retries cannot renew age.
 * Get returns only configured Talos players in the current exact scene.
 * received_tick comes from the local clock, never the remote host clock. */
BOOL SudekiMpLanPartyPublishAvatarStatus(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryAvatarStatus *status);
BOOL SudekiMpLanPartyGetAvatarStatus(SudekiMpLanPartySession *session,
    unsigned player,uint32_t now,SudekiMpLanStoryAvatarStatus *status);
/* Closes this exact offer and input queue before native drain. It grants no
 * native retirement; ReleaseDrained is still the coordinator's final step. */
BOOL SudekiMpLanPartyRevokeStoryControl(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease);
BOOL SudekiMpLanPartyPublishStoryRecruitment(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryRecruitment *recruitment);
BOOL SudekiMpLanPartyGetStoryRecruitment(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryRecruitment *recruitment);
BOOL SudekiMpLanPartySendStoryPresentation(SudekiMpLanPartySession *session,
    const SudekiMpLanStoryPresentation *presentation);
BOOL SudekiMpLanPartyPopStoryPresentation(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryPresentation *presentation,
    uint32_t *received_at);
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
/* Read-only validation of a previously dequeued input against current policy;
 * unlike AdmitInput this does not acknowledge it or consume its sequence. */
BOOL SudekiMpLanPartyInputCurrent(SudekiMpLanPartySession *session,
    const SudekiMpLanPartyInput *input);
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
