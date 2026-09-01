#ifndef SUDEKIMP_LAN_ARENA_SESSION_H
#define SUDEKIMP_LAN_ARENA_SESSION_H

#include "network/lan_arena_protocol.h"

#include <windows.h>
#include <stddef.h>

typedef struct SudekiMpLanArenaSessionConfig {
    SudekiMpLanArenaRole local_role;
    const char *remote_ipv4; /* client only; host binds INADDR_ANY */
    unsigned int port;
    uint32_t timeout_ms;
    const uint8_t *game_hash;
} SudekiMpLanArenaSessionConfig;

typedef struct SudekiMpLanArenaSessionStatus {
    SudekiMpLanArenaConnectionPhase phase;
    SudekiMpLanArenaRejectReason failure;
    uint64_t session_token;
    uint8_t peer_connected;
    uint8_t local_role;
} SudekiMpLanArenaSessionStatus;

/* Lifecycle is deliberately independent from local input bridge sockets. The
 * transport API is internally synchronized so a network-only pump may call
 * `Poll` while the native game thread sends/takes packets. No transport method
 * may read or mutate game state; callers apply authenticated input/snapshots
 * on the native game thread only. */
BOOL SudekiMpLanArenaSessionStart(const SudekiMpLanArenaSessionConfig *config);
void SudekiMpLanArenaSessionStop(BOOL notify_peer);
void SudekiMpLanArenaSessionPoll(uint32_t now_ms);
BOOL SudekiMpLanArenaSessionGetStatus(SudekiMpLanArenaSessionStatus *status);
BOOL SudekiMpLanArenaSessionTakeRemoteInput(SudekiMpLanArenaInput *input);
BOOL SudekiMpLanArenaSessionTakeRemoteSnapshot(SudekiMpLanArenaSnapshot *snapshot);
BOOL SudekiMpLanArenaSessionSendInput(const SudekiMpLanArenaInput *input);
BOOL SudekiMpLanArenaSessionSendSnapshot(const SudekiMpLanArenaSnapshot *snapshot);
BOOL SudekiMpLanArenaSessionConnected(void);
/* Read-only UI projection of the immutable startup endpoint. Hosts report a
 * best-effort non-loopback LAN address; clients report their configured host.
 * Failure leaves `address` empty but still returns the bound/configured port. */
BOOL SudekiMpLanArenaSessionGetDisplayEndpoint(
    char *address,
    size_t address_capacity,
    unsigned int *port
);

#endif
