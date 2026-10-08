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
    uint8_t seat, members, mode; /* Connected load barrier, includes host. */
    uint8_t reserved_mask; /* Concrete character claims, including offline players. */
    uint8_t character[4]; /* Hero 0..3, none 4, or Dev Play Talos 5. */
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
/* Pure projection only: the first player choosing a hero receives its native
 * slot. Talos and later duplicates project to NO_CHARACTER. The original
 * lobby choices remain in plan; outputs must not alias it. No native spawn,
 * control or camera capability follows from this projection. */
BOOL SudekiMpLobbyLaunchProjectNative(const SudekiMpLobbyLaunchPlan *,
    uint8_t character[4], uint8_t *reserved_mask);
/* Interim saved-game gate until independent avatar control/camera exist:
 * every choice must project to a native hero and host must be save leader. */
BOOL SudekiMpLobbyLaunchPlanNativeReady(const SudekiMpLobbyLaunchPlan *, unsigned save_leader);
/* Implemented Dev Play runtime routes; duplicate hero picks spectate. A
 * native host must exist in the fingerprinted save party so startup can
 * select it through the exact native rotation adapter. */
BOOL SudekiMpLobbyLaunchPlanDevPlayReady(const SudekiMpLobbyLaunchPlan *,
    unsigned save_leader,unsigned save_party_mask);
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
