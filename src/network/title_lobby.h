#ifndef SUDEKIMP_TITLE_LOBBY_H
#define SUDEKIMP_TITLE_LOBBY_H
#include <windows.h>
#include <stdint.h>

/* Additive pre-game transport. SLB1 is separate from SMP4/LA42 gameplay.
 * A lobby slot is a connection, NEVER an actor or gameplay permission. */
#define SUDEKIMP_LOBBY_CAPACITY 4
#define SUDEKIMP_LOBBY_SERVERS 16
#define SUDEKIMP_LOBBY_NAME 32
#define SUDEKIMP_LOBBY_PORT 26770
#define SUDEKIMP_LOBBY_DISCOVERY_PORT 26771
typedef enum SudekiMpLobbyPhase {
    SUDEKIMP_LOBBY_IDLE, SUDEKIMP_LOBBY_HOSTING, SUDEKIMP_LOBBY_CONNECTING,
    SUDEKIMP_LOBBY_CONNECTED, SUDEKIMP_LOBBY_ERROR
} SudekiMpLobbyPhase;
typedef struct SudekiMpLobbyMember {
    uint8_t present, ready;
    char name[SUDEKIMP_LOBBY_NAME];
} SudekiMpLobbyMember;
typedef struct SudekiMpLobbyServer {
    char name[SUDEKIMP_LOBBY_NAME], ipv4[16];
    uint16_t port;
    uint8_t players;
    uint32_t seen_at;
    uint64_t instance;
} SudekiMpLobbyServer;
enum { SUDEKIMP_LOBBY_DEST_NONE, SUDEKIMP_LOBBY_DEST_TESTROOM };
enum { SUDEKIMP_LOBBY_START_IDLE, SUDEKIMP_LOBBY_START_PREPARE,
    SUDEKIMP_LOBBY_START_LOADING, SUDEKIMP_LOBBY_START_COMPLETE, SUDEKIMP_LOBBY_START_ABORTED };
enum { SUDEKIMP_LOBBY_ACK_PREPARED=1, SUDEKIMP_LOBBY_ACK_LOADED,
    SUDEKIMP_LOBBY_ACK_COMPLETE, SUDEKIMP_LOBBY_ACK_FAILED };
typedef struct SudekiMpLobbyStart {
    uint32_t revision;
    uint64_t generation;
    uint64_t nonce[4];
    uint16_t port;
    uint8_t destination, phase, members, prepared, loaded, completed;
} SudekiMpLobbyStart;
typedef struct SudekiMpLobbyStatus {
    SudekiMpLobbyPhase phase;
    uint8_t local_slot, advertised;
    uint16_t port;
    char room[SUDEKIMP_LOBBY_NAME], error[96];
    char discovery_error[96];
    char host_ipv4[16]; /* Actual connected endpoint, never host-supplied. */
    SudekiMpLobbyStart start;
    BOOL departure_safe;
    SudekiMpLobbyMember members[SUDEKIMP_LOBBY_CAPACITY];
    unsigned server_count;
    SudekiMpLobbyServer servers[SUDEKIMP_LOBBY_SERVERS];
} SudekiMpLobbyStatus;
typedef struct SudekiMpLobby SudekiMpLobby;
SudekiMpLobby *SudekiMpLobbyCreate(void);
/* FALSE retains the object/worker: caller must retry before unloading. */
BOOL SudekiMpLobbyDestroy(SudekiMpLobby *lobby);
BOOL SudekiMpLobbyHost(SudekiMpLobby *, const char *room, const char *name,
    uint16_t port, BOOL advertised);
BOOL SudekiMpLobbyJoin(SudekiMpLobby *, const char *ipv4, uint16_t port, const char *name);
void SudekiMpLobbyLeave(SudekiMpLobby *);
void SudekiMpLobbyReady(SudekiMpLobby *, BOOL ready);
void SudekiMpLobbyBrowse(SudekiMpLobby *, BOOL enabled);
/* Host-only visibility change. Failure preserves the active lobby. */
BOOL SudekiMpLobbyAdvertise(SudekiMpLobby *, BOOL enabled);
BOOL SudekiMpLobbyDestination(SudekiMpLobby *, unsigned destination);
BOOL SudekiMpLobbyStartGame(SudekiMpLobby *);
/* Only the local native/launcher adapter reports preparation and real load. */
void SudekiMpLobbyLoadAck(SudekiMpLobby *, uint32_t revision, uint64_t generation,
    unsigned acknowledgement, unsigned bound_port);
void SudekiMpLobbyAbortStart(SudekiMpLobby *);
void SudekiMpLobbyStatusGet(SudekiMpLobby *, SudekiMpLobbyStatus *);
#endif
