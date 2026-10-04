#ifndef SUDEKIMP_LOBBY_LAUNCH_H
#define SUDEKIMP_LOBBY_LAUNCH_H
#include <windows.h>
#include <stdint.h>

/* Local, ephemeral launcher IPC, never a network packet or save sidecar.
 * Paths come exclusively from the running executable/DLL and its launcher. */
#define SUDEKIMP_LOBBY_LAUNCH_ENV L"SUDEKIMP_LOBBY_LAUNCH"
enum { SUDEKIMP_LAUNCH_WAIT, SUDEKIMP_LAUNCH_RUN, SUDEKIMP_LAUNCH_CANCEL,
    SUDEKIMP_LAUNCH_DETACH };
enum { SUDEKIMP_LAUNCH_NEW, SUDEKIMP_LAUNCH_PREPARED, SUDEKIMP_LAUNCH_LOADED,
    SUDEKIMP_LAUNCH_FAILED };
typedef struct SudekiMpLobbyLaunchPlan {
    uint32_t revision;
    uint64_t generation;
    uint8_t seat, members; /* Connected load barrier, includes host. */
    uint8_t reserved_mask; /* Concrete character claims, including offline players. */
    uint8_t character[4]; /* Canonical identity; nonlocal members may spectate (4). */
    uint16_t port;
    char host_ipv4[16];
    uint64_t nonce[4];
} SudekiMpLobbyLaunchPlan;
typedef struct SudekiMpLobbyLaunchShared {
    uint32_t magic, version, size, owner_pid, child_pid;
    SudekiMpLobbyLaunchPlan plan;
    wchar_t game[MAX_PATH], dll[MAX_PATH];
    char dll_hash[65];
    volatile LONG command, state, bound_port, error;
} SudekiMpLobbyLaunchShared;
typedef struct SudekiMpLobbyLaunch {
    HANDLE mapping, launcher;
    SudekiMpLobbyLaunchShared *shared;
} SudekiMpLobbyLaunch;

BOOL SudekiMpLobbyLaunchAvailable(void);
BOOL SudekiMpLobbyLaunchPlanValid(const SudekiMpLobbyLaunchPlan *);
BOOL SudekiMpLobbyLaunchPrepare(SudekiMpLobbyLaunch *, const SudekiMpLobbyLaunchPlan *);
unsigned SudekiMpLobbyLaunchPoll(SudekiMpLobbyLaunch *, unsigned *port);
void SudekiMpLobbyLaunchCommand(SudekiMpLobbyLaunch *, unsigned command);
/* Cancellation retains live processes; FALSE requires a later retry. */
BOOL SudekiMpLobbyLaunchRelease(SudekiMpLobbyLaunch *);
BOOL SudekiMpLobbyLaunchOpen(const wchar_t *name, HANDLE *, SudekiMpLobbyLaunchShared **);
/* 0 ordinary profile, 1 exact local launcher handoff, -1 invalid request. */
int SudekiMpLobbyLaunchChildConfig(HMODULE dll, SudekiMpLobbyLaunchPlan *);
BOOL SudekiMpLobbyLaunchChildRunning(void);
void SudekiMpLobbyLaunchChildPort(unsigned port);
void SudekiMpLobbyLaunchChildLoaded(void);
#endif
