#ifndef SUDEKIMP_TITLE_LOBBY_H
#define SUDEKIMP_TITLE_LOBBY_H
#include <windows.h>
#include <stdint.h>

/* Persistent lobby/control transport. SLB1 is separate from SMP4/LA42 gameplay.
 * A lobby slot is a connection, NEVER an actor or gameplay permission. */
#define SUDEKIMP_LOBBY_CAPACITY 4
#define SUDEKIMP_LOBBY_SERVERS 16
#define SUDEKIMP_LOBBY_NAME 32
#define SUDEKIMP_LOBBY_PORT 26770
#define SUDEKIMP_LOBBY_DISCOVERY_PORT 26771
/* Matching saved-story builds now require the NPC world presentation stream.
 * Reject older lobbies before either side starts loading a saved game. */
#define SUDEKIMP_LOBBY_VERSION 11
#define SUDEKIMP_LOBBY_SAVE_LABEL 48
enum { SUDEKIMP_LOBBY_NO_CHARACTER = 4 };
enum { SUDEKIMP_LOBBY_AI_COVER, SUDEKIMP_LOBBY_SHARED_PAUSE };
typedef enum SudekiMpLobbyPhase {
    SUDEKIMP_LOBBY_IDLE, SUDEKIMP_LOBBY_HOSTING, SUDEKIMP_LOBBY_CONNECTING,
    SUDEKIMP_LOBBY_CONNECTED, SUDEKIMP_LOBBY_ERROR
} SudekiMpLobbyPhase;
typedef struct SudekiMpLobbyMember {
    uint8_t present, ready, reserved, locked, character;
    char name[SUDEKIMP_LOBBY_NAME];
} SudekiMpLobbyMember;
typedef struct SudekiMpLobbyServer {
    char name[SUDEKIMP_LOBBY_NAME], ipv4[16];
    uint16_t port;
    uint8_t players, running;
    uint32_t seen_at;
    uint64_t instance;
} SudekiMpLobbyServer;
enum { SUDEKIMP_LOBBY_DEST_NONE, SUDEKIMP_LOBBY_DEST_TESTROOM,
    SUDEKIMP_LOBBY_DEST_SAVEDGAME };
typedef struct SudekiMpLobbySavedGame {
    /* Host's SAVESLOTdddd suffix, never a native Load Game row/index. */
    uint32_t folder_slot;
    uint8_t fish_sha256[32], bunny_sha256[32];
    char label[SUDEKIMP_LOBBY_SAVE_LABEL];
    /* Preview from the reviewed native save. Confirm against the loaded
     * roster before native control; portrait IDs have already been mapped. */
    uint8_t party_count, leader, party_mask, party_order[4];
} SudekiMpLobbySavedGame;
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
enum { SUDEKIMP_LOBBY_ADMISSION_NONE, SUDEKIMP_LOBBY_ADMISSION_OFFERED,
    SUDEKIMP_LOBBY_ADMISSION_PREPARED, SUDEKIMP_LOBBY_ADMISSION_LOADED,
    SUDEKIMP_LOBBY_ADMISSION_COMPLETE, SUDEKIMP_LOBBY_ADMISSION_FAILED };
typedef struct SudekiMpLobbyAdmission {
    uint32_t sequence;
    uint64_t ticket; /* Host sees all; clients receive only their own. */
    uint8_t phase;
} SudekiMpLobbyAdmission;
typedef struct SudekiMpLobbyStatus {
    SudekiMpLobbyPhase phase;
    uint8_t local_slot, advertised;
    uint16_t port;
    char room[SUDEKIMP_LOBBY_NAME], error[96];
    char discovery_error[96];
    char host_ipv4[16]; /* Actual connected endpoint, never host-supplied. */
    SudekiMpLobbyStart start;
    SudekiMpLobbySavedGame saved_game;
    BOOL departure_safe;
    uint8_t running, absence_policy, paused;
    uint32_t roster_revision;
    uint32_t command_sequence;
    uint8_t command_rejected;
    SudekiMpLobbyAdmission admission[SUDEKIMP_LOBBY_CAPACITY];
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
/* Retry the same observed endpoint/name as a fresh join. Departure releases
 * the former choice after native drain; this never reclaims that identity. */
BOOL SudekiMpLobbyReconnect(SudekiMpLobby *);
void SudekiMpLobbyLeave(SudekiMpLobby *);
/* Selection/lock is pre-game or an as-yet-unadmitted running joiner only.
 * A client TRUE queues a request; confirmed state arrives in StatusGet. */
BOOL SudekiMpLobbySelectCharacter(SudekiMpLobby *, unsigned character, BOOL locked);
BOOL SudekiMpLobbySetName(SudekiMpLobby *, const char *name);
void SudekiMpLobbyReady(SudekiMpLobby *, BOOL ready);
void SudekiMpLobbyBrowse(SudekiMpLobby *, BOOL enabled);
/* Host-only visibility change. Failure preserves the active lobby. */
BOOL SudekiMpLobbyAdvertise(SudekiMpLobby *, BOOL enabled);
BOOL SudekiMpLobbyDestination(SudekiMpLobby *, unsigned destination);
/* Host-only confirmed catalog selection. Both complete files are identified;
 * no path or native pointer crosses the wire. The printable ASCII label must
 * be nonempty, terminated, and zero-padded. Changing the choice resets ready
 * state atomically; ordinary ready/roster changes preserve the selection.
 * This publishes a selection only and never loads a native world. */
BOOL SudekiMpLobbySelectSavedGame(SudekiMpLobby *, const SudekiMpLobbySavedGame *);
/* Local composition capability, not a peer permission or readiness report.
 * Default FALSE. Set before starting/joining; native load/replica/task gates
 * still own PREPARED/LOADED acknowledgments. */
BOOL SudekiMpLobbyEnableSavedStart(SudekiMpLobby *, BOOL enabled);
BOOL SudekiMpLobbyStartGame(SudekiMpLobby *);
/* Only the local native/launcher adapter reports preparation and real load. */
void SudekiMpLobbyLoadAck(SudekiMpLobby *, uint32_t revision, uint64_t generation,
    unsigned acknowledgement, unsigned bound_port);
void SudekiMpLobbyAbortStart(SudekiMpLobby *);
/* Retain the service after native loading completes. This changes discovery,
 * not gameplay authority, and requires the completed initial load barrier. */
BOOL SudekiMpLobbyHostRunning(SudekiMpLobby *);
/* Host game-thread adapter reflects its confirmed policy/pause state here. */
BOOL SudekiMpLobbyHostRuntimeState(SudekiMpLobby *, unsigned policy, BOOL paused);
/* Mirror a game-thread-confirmed assignment after its native handoff. Four
 * canonical indices or NO_CHARACTER for spectators; never changes identity.
 * Only authoritative_members are mirrored. Pending lobby selections survive
 * unless they conflict with a confirmed character, which unlocks the choice. */
BOOL SudekiMpLobbyReflectAssignments(SudekiMpLobby *, const uint8_t character[4],
    unsigned authoritative_members);
/* Host runtime reports a departed gameplay connection while its lobby stream
 * may still be open. Closes only that member's stream; running slots remain
 * unavailable until ReleaseReservation confirms the native drain. */
BOOL SudekiMpLobbyDisconnectMember(SudekiMpLobby *, unsigned player);
/* After a cancelled initial start, call only after native GameplayCancel has
 * positively completed. Forbidden while running or starting; frees departed
 * members and permits another start. This transport cannot prove the drain. */
BOOL SudekiMpLobbyHostNativeDrained(SudekiMpLobby *);
/* Host must prove old native/input ownership drained before freeing a
 * departed slot. This transport never performs native handoffs. */
BOOL SudekiMpLobbyReleaseReservation(SudekiMpLobby *, unsigned player);
/* Generate a fresh running-join ticket only after the host runtime is ready
 * to register it. A ticket itself grants no native/input authority. */
BOOL SudekiMpLobbyHostAdmit(SudekiMpLobby *, unsigned player);
void SudekiMpLobbyAdmissionAck(SudekiMpLobby *, uint32_t sequence,
    uint64_t ticket, unsigned acknowledgement);
/* Success requires client LOADED plus caller-confirmed native synchronization
 * and ownership. Failure retires only this admission, never the live session. */
BOOL SudekiMpLobbyHostAdmissionComplete(SudekiMpLobby *, unsigned player,
    uint32_t sequence, uint64_t ticket, BOOL success);
void SudekiMpLobbyStatusGet(SudekiMpLobby *, SudekiMpLobbyStatus *);
#endif
