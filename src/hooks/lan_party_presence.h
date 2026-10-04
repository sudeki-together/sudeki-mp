#ifndef SUDEKIMP_LAN_PARTY_PRESENCE_H
#define SUDEKIMP_LAN_PARTY_PRESENCE_H
#include "network/lan_party_session.h"

typedef enum SudekiMpLanPartyPresenceSwapResult {
    SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_NONE,
    SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_COMMITTED,
    SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_RESTORED
} SudekiMpLanPartyPresenceSwapResult;
/* Every mask is PLAYER-indexed except busy_characters. These observations
 * must be positively proved for the currently assigned character and policy
 * generation. Missing bits mean unknown, never successful native cleanup. */
typedef struct SudekiMpLanPartyPresenceNativeState {
    SudekiMpPartyAssignment assignment; /* identity observed with these masks */
    uint8_t connected_mask, native_owned_mask, native_ai_mask, local_bound_mask;
    uint8_t busy_characters, world_busy, swap_ready;
    SudekiMpLanPartyPresenceSwapResult swap_result;
    uint32_t swap_request; /* result/ready belongs to this exact transaction */
} SudekiMpLanPartyPresenceNativeState;
typedef struct SudekiMpLanPartyPresenceCoordinator {
    SudekiMpLanPartySession *session;
    SudekiMpLanPartyPresence current;
    SudekiMpLanPartyPresenceNativeState observed;
    uint32_t next_request;
    uint8_t local_player, initial_character[4], pause_requests;
    /* Session-local proof that this connection has completed a native view
     * binding. Initial/loading clients cannot be reassigned under their first
     * cast/presentation binding; reconnect requires another exact ACK. */
    uint8_t bound_players;
    BOOL initialized;
} SudekiMpLanPartyPresenceCoordinator;
/* Caller owns game-thread lifetime. Init/Service/UI calls never dereference
 * actors or invoke native functions. The native executor reads phases and
 * returns exact positive observations on its next verified update. */
BOOL SudekiMpLanPartyPresenceInitialize(SudekiMpLanPartyPresenceCoordinator *,
    SudekiMpLanPartySession *, const SudekiMpLanPartyConfig *);
BOOL SudekiMpLanPartyPresenceRead(SudekiMpLanPartyPresenceCoordinator *,
    SudekiMpLanPartyPresence *);
BOOL SudekiMpLanPartyPresenceService(SudekiMpLanPartyPresenceCoordinator *,
    const SudekiMpLanPartyPresenceNativeState *, uint32_t now_ms);
/* Same already-verified UI/game thread only; no native completion is inferred
 * while paused. Publishing observed_tick here means policy-service freshness,
 * never advancing the world's combat/animation observation clock. */
BOOL SudekiMpLanPartyPresenceUiFrame(SudekiMpLanPartyPresenceCoordinator *, uint32_t now_ms);
/* Assigns a fresh monotonic request and stamps latest confirmed ownership.
 * target/value follow the transport enum; transaction is used for swap ACK. */
BOOL SudekiMpLanPartyPresenceQueue(SudekiMpLanPartyPresenceCoordinator *,
    SudekiMpLanPartyCommandKind kind, unsigned target, unsigned value,
    uint32_t transaction);
/* Builds a local intent/policy command with no character/player target using
 * the canonical NO_CHARACTER sentinel. Targeted operations and binding ACKs
 * are rejected; an ACK must retain the identity of its native observation. */
BOOL SudekiMpLanPartyPresenceQueueLocal(SudekiMpLanPartyPresenceCoordinator *,
    SudekiMpLanPartyCommandKind kind, unsigned value, uint32_t transaction);
/* CONTROL_ACK/SWAP_ACK only. Stamps the exact snapshot that the caller proved
 * natively, never a newer revision/generation received during that proof. */
BOOL SudekiMpLanPartyPresenceQueueAcknowledgment(SudekiMpLanPartyPresenceCoordinator *,
    const SudekiMpLanPartyPresence *observed, SudekiMpLanPartyCommandKind kind,
    uint32_t transaction);
/* Called only after host-confirmed lobby admission. Reservation is plain data;
 * callers still prove native AI and issue/validate the separate transport ticket. */
BOOL SudekiMpLanPartyPresenceReserve(SudekiMpLanPartyPresenceCoordinator *,
    unsigned player, unsigned character, uint32_t now_ms);
/* Lobby bridge confirms member departure and closes the old admission first.
 * Rechecks transport FREE and exact native AI observation before publishing
 * the released claim. Never use mere initial handshake absence as departure. */
BOOL SudekiMpLanPartyPresenceReleaseDeparted(SudekiMpLanPartyPresenceCoordinator *,
    unsigned player, uint32_t now_ms);
void SudekiMpLanPartyPresenceReset(SudekiMpLanPartyPresenceCoordinator *);
#endif
