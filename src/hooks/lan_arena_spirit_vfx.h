#ifndef SUDEKIMP_LAN_ARENA_SPIRIT_VFX_H
#define SUDEKIMP_LAN_ARENA_SPIRIT_VFX_H

#include "cleanroom/engine.h"
#include "network/lan_arena_protocol.h"

#include <stdint.h>
#include <windows.h>

/* Exact x86 layout accepted by CSFXManager::PlaySfx. This adapter only lends
 * a stack instance for the duration of the native call; it never links that
 * instance into Sudeki's observer list. */
typedef struct SudekiMpLanArenaSpiritVfxTransientTPtr {
    void *object;
    void *previous_observer;
    void *next_observer;
} SudekiMpLanArenaSpiritVfxTransientTPtr;

typedef enum SudekiMpLanArenaSpiritVfxCacheState {
    SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY = 0,
    SUDEKIMP_SPIRIT_VFX_CACHE_LOADING = 1,
    SUDEKIMP_SPIRIT_VFX_CACHE_READY = 2,
    SUDEKIMP_SPIRIT_VFX_CACHE_POISONED = 3,
    SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING = 4
} SudekiMpLanArenaSpiritVfxCacheState;

/* Caller-owned state for focused seam tests. Zero initialization is EMPTY.
 * Production uses one private instance and never transfers a lease between
 * manager identities. */
typedef struct SudekiMpLanArenaSpiritVfxCacheLease {
    void *manager;
    uint32_t resource_identifier;
    LONG slot_index;
    LONG acquired_ref_count;
    SudekiMpLanArenaSpiritVfxCacheState state;
} SudekiMpLanArenaSpiritVfxCacheLease;

typedef struct SudekiMpLanArenaSpiritVfxCacheSnapshot {
    unsigned int matching_slots;
    unsigned int slot_index;
    LONG ref_count;
    BOOL pending;
    BOOL loaded;
} SudekiMpLanArenaSpiritVfxCacheSnapshot;

/* Injectable synchronous seam for focused admission/lifecycle tests. */
typedef struct SudekiMpLanArenaSpiritVfxReplayApi {
    void *context;
    void *(*resolve_tal)(void *context);
    BOOL (*tal_ready)(void *context, void *tal);
    void *(*get_sfx_manager)(void *context);
    BOOL (*resource_name_from_text)(
        void *context,
        SudekiMpResourceName *resource_name,
        const char *text
    );
    void (*release_resource_name)(
        void *context,
        SudekiMpResourceName *resource_name
    );
    BOOL (*cache_snapshot)(
        void *context,
        void *manager,
        uint32_t resource_identifier,
        SudekiMpLanArenaSpiritVfxCacheSnapshot *snapshot
    );
    BOOL (*pre_cache_effect)(
        void *context,
        void *manager,
        SudekiMpResourceName *resource_name
    );
    BOOL (*un_cache_effect)(
        void *context,
        void *manager,
        SudekiMpResourceName *resource_name
    );
    void (*play_sfx)(
        void *context,
        void *manager,
        SudekiMpLanArenaSpiritVfxTransientTPtr *actor,
        SudekiMpResourceName *resource_name,
        BOOL follow_character,
        BOOL real_time,
        float x,
        float y,
        float z
    );
} SudekiMpLanArenaSpiritVfxReplayApi;

/* Exact supported-image preflight for the ASLR-relocated GetSFXManager
 * singleton load plus the native PreCache, UnCache, and complete PlaySfx
 * entry bodies used by this adapter. */
BOOL SudekiMpLanArenaSpiritVfxReplayImageMatches(HMODULE game_module);

/* Acquire at most one authored SFXSS250_Initiate pre-cache lease for the
 * current exact manager identity. FALSE with ERROR_IO_PENDING means the one
 * lease is proven but still loading; repeated calls only poll it. A retained
 * post-replay cleanup obligation must be released before another acquire.
 * TRUE means the resource is ready for synchronous replay. */
BOOL SudekiMpLanArenaSpiritVfxPrepareTalInitiate(HMODULE game_module);

/* Pure cache observation: this never calls PreCache or UnCache. */
BOOL SudekiMpLanArenaSpiritVfxTalInitiateCacheReady(HMODULE game_module);

/* Release a pending or ready lease. No lease is a successful no-op. FALSE
 * guarantees native UnCache was not entered and same-generation state remains
 * unchanged: a valid lease may be retried, while poison remains fail-closed.
 * Once UnCache is entered, local ownership is cleared and TRUE is returned so
 * teardown can never double-release. Manager drift is forgotten without
 * dereferencing or calling the stale manager. */
BOOL SudekiMpLanArenaSpiritVfxReleaseTalInitiateCache(HMODULE game_module);

/* Client presentation only. The caller must own a positively proven Sudeki
 * game-thread boundary and an authenticated lifecycle lease for expected_tal.
 * This adapter requires a READY pre-cache lease, re-resolves Tal immediately
 * before entry, and refuses identity drift. It always maps to the fixed
 * authored SFXSS250_Initiate resource and (follow=false, realTime=false,
 * xyz=0) tuple.
 *
 * FALSE guarantees CSFXManager::PlaySfx was never entered. Once PlaySfx is
 * entered this returns TRUE regardless of its handleless visual outcome or
 * post-call UnCache verification. The caller ResourceName is released and
 * the proven pre-cache lease is released exactly once or retained as
 * RELEASE_PENDING if same-manager cleanup cannot yet be verified. That state
 * cannot replay, and Prepare must finish its cleanup before reacquiring.
 * Native SfxSetup owns its ResourceName references and transform snapshot
 * independently. */
BOOL SudekiMpLanArenaSpiritVfxReplayTalInitiate(
    HMODULE game_module,
    void *expected_tal
);

/* Focused seam variants. lease must remain exclusively caller-owned and
 * zero-initialized before first use. They preserve the production contracts. */
BOOL SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
);
BOOL SudekiMpLanArenaSpiritVfxTalInitiateCacheReadyWithApi(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
);
BOOL SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
);
BOOL SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
    void *expected_tal,
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
);

/* Query barrier for every adapter operation, including cache transitions. */
LONG SudekiMpLanArenaSpiritVfxReplayActiveCalls(void);

/* Roster replay is parent-free: native SpecialEffect clones have no player
 * animation-event listener. Caller owns authentication and a game-thread
 * publication boundary. UNKNOWN rosters never imply removal. */
BOOL SudekiMpLanArenaSpiritVfxServiceVisuals(
    HMODULE game_module,
    const SudekiMpLanArenaSnapshot *snapshot,
    uint64_t session_token
);
BOOL SudekiMpLanArenaSpiritVfxResetVisuals(HMODULE game_module);
BOOL SudekiMpLanArenaSpiritVfxVisualImageMatches(HMODULE game_module);
/* Read-only exact owned-effect shape, including its isolated sound listener. */
BOOL SudekiMpLanArenaSpiritVfxVisualIdentityMatches(HMODULE game_module, void *effect);

typedef struct SudekiMpLanArenaSpiritVfxVisualSlot {
    SudekiMpLanArenaSpiritVfxSnapshot identity;
    SudekiMpLanArenaSpiritVfxTransientTPtr observer;
    BOOL occupied;
    BOOL spawn_entered;
    BOOL retire_entered;
    BOOL cleanup_failure_reported;
} SudekiMpLanArenaSpiritVfxVisualSlot;

/* Stable-address storage: never copy/move this while observers are attached. */
typedef struct SudekiMpLanArenaSpiritVfxVisualState {
    uint64_t session_token;
    uint32_t newest_instance_sequence;
    SudekiMpLanArenaSpiritVfxVisualSlot slots[
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY];
    SudekiMpLanArenaSpiritVfxCacheLease caches[SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST];
} SudekiMpLanArenaSpiritVfxVisualState;

typedef struct SudekiMpLanArenaSpiritVfxVisualApi {
    SudekiMpLanArenaSpiritVfxReplayApi cache;
    /* FALSE means no native spawn entry. TRUE consumes the instance, even
     * when native creation failed and left observer.object NULL. */
    BOOL (*spawn)(void *context, void *manager,
        const SudekiMpLanArenaSpiritVfxSnapshot *visual,
        SudekiMpLanArenaSpiritVfxTransientTPtr *observer);
    BOOL (*synchronize)(void *context,
        const SudekiMpLanArenaSpiritVfxSnapshot *visual,
        SudekiMpLanArenaSpiritVfxTransientTPtr *observer);
    /* TRUE confirms native retire was entered or native destruction already
     * nulled the observer. FALSE retains the obligation for retry. */
    BOOL (*retire)(void *context,
        SudekiMpLanArenaSpiritVfxTransientTPtr *observer);
    BOOL (*detach)(void *context,
        SudekiMpLanArenaSpiritVfxTransientTPtr *observer);
} SudekiMpLanArenaSpiritVfxVisualApi;

BOOL SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(
    SudekiMpLanArenaSpiritVfxVisualState *state,
    const SudekiMpLanArenaSnapshot *snapshot,
    uint64_t session_token,
    const SudekiMpLanArenaSpiritVfxVisualApi *api
);
BOOL SudekiMpLanArenaSpiritVfxResetVisualsWithApi(
    SudekiMpLanArenaSpiritVfxVisualState *state,
    const SudekiMpLanArenaSpiritVfxVisualApi *api
);
BOOL SudekiMpLanArenaSpiritVfxVisualMatrix(
    const SudekiMpLanArenaSpiritVfxSnapshot *visual, float matrix[16]);
/* Retail opening particles advance natively: only seek them forward. Other
 * kinds retain absolute host-phase correction, including authored loop wraps.
 * Invalid input returns FALSE without changing apply; this never owns removal. */
BOOL SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
    unsigned int kind, float native_phase, float host_phase, BOOL *apply);

#endif
