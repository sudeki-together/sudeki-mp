#ifndef SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_H
#define SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_H

#include "network/lan_arena_protocol.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

enum {
    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY = 64u,
    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY = 96u
};

typedef BOOL (*SudekiMpLanArenaSpiritActiveWitness)(
    void *context,
    int *native_state
);

typedef struct SudekiMpLanArenaSpiritAudioEvent {
    uint32_t sequence;
    uint32_t elapsed_ms;
    int32_t native_state;
    uint8_t cue_length;
    char cue[SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY];
} SudekiMpLanArenaSpiritAudioEvent;

typedef struct SudekiMpLanArenaSpiritAudioReplayApi {
    void *context;
    void *(*get_sound)(void *context);
    BOOL (*play_cue)(void *context, void *sound, const char *cue);
} SudekiMpLanArenaSpiritAudioReplayApi;

/* This is an observation-only, host-side hook. `active_witness` must return
 * TRUE only for the LAN host's exact native Tal Spirit transaction. The hook
 * never starts a Spirit transaction and never replays a cue. Install and
 * uninstall are game-thread boundaries, matching CSound::PlayCue ownership. */
BOOL SudekiMpInstallLanArenaSpiritAudioTrace(
    HMODULE game_module,
    SudekiMpLanArenaSpiritActiveWitness active_witness,
    void *witness_context
);

/* Logical unbind only. The first exact-image detour pins this DLL and remains
 * inert/native-passthrough until process exit; its trampoline, lock, storage,
 * and last witness/context are never reclaimed at runtime. A later host
 * session on the same game image safely rebinds the existing detour. */
BOOL SudekiMpUninstallLanArenaSpiritAudioTrace(void);
BOOL SudekiMpLanArenaSpiritAudioTraceInstalled(void);

/* Exact supported-image preflight for the complete CSound::PlayCue body. */
BOOL SudekiMpLanArenaSpiritAudioTraceImageMatches(HMODULE game_module);

/* Copies the newest bounded host observations in chronological order. Raw cue
 * strings remain process-local research data and are never network payloads. */
size_t SudekiMpLanArenaSpiritAudioTraceSnapshot(
    SudekiMpLanArenaSpiritAudioEvent *events,
    size_t capacity,
    uint32_t *dropped_count
);

/* Client-only presentation adapter. The semantic cue is mapped to a fixed
 * process-local literal before entering CSound::PlayCue; no packet string or
 * Spirit manager/effect/camera/time/damage function is reachable here. */
/* The replay preflight verifies the ASLR-relocated GetSound singleton operand
 * as well as the complete supported-build CSound::PlayCue body. */
BOOL SudekiMpLanArenaSpiritAudioReplayImageMatches(HMODULE game_module);
BOOL SudekiMpLanArenaSpiritAudioReplayWithApi(
    SudekiMpLanArenaSpiritAudioCue cue,
    const SudekiMpLanArenaSpiritAudioReplayApi *api
);
BOOL SudekiMpLanArenaSpiritAudioReplayLocalCue(
    HMODULE game_module,
    SudekiMpLanArenaSpiritAudioCue cue
);

#endif
