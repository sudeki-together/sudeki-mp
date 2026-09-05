#include "hooks/lan_arena_spirit_vfx.h"

#include <windows.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TEST_RVA_SFX_PLAY = 0x00018de0u,
    TEST_RVA_SFX_PRE_CACHE = 0x00019540u,
    TEST_RVA_SFX_UN_CACHE = 0x00019650u,
    TEST_RVA_SFX_GET_MANAGER = 0x00019770u,
    TEST_RVA_SFX_MANAGER_GLOBAL = 0x00408d48u,
    TEST_FIXTURE_IMAGE_SIZE = 0x00410000u
};

static const uint8_t test_sfx_play_body[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x83u, 0x38u, 0x00u, 0x74u,
    0x39u, 0xd9u, 0x44u, 0x24u, 0x1cu, 0x8bu, 0x54u, 0x24u,
    0x10u, 0x6au, 0x00u, 0x83u, 0xecu, 0x10u, 0xd9u, 0x5cu,
    0x24u, 0x0cu, 0xd9u, 0x44u, 0x24u, 0x2cu, 0xd9u, 0x5cu,
    0x24u, 0x08u, 0xd9u, 0x44u, 0x24u, 0x28u, 0xd9u, 0x5cu,
    0x24u, 0x04u, 0xd9u, 0xe8u, 0xd9u, 0x1cu, 0x24u, 0x6au,
    0x00u, 0x52u, 0x8bu, 0x54u, 0x24u, 0x28u, 0x52u, 0x8bu,
    0x54u, 0x24u, 0x28u, 0x52u, 0x50u, 0xe8u, 0x4eu, 0x00u,
    0x00u, 0x00u, 0xc2u, 0x1cu, 0x00u
};

static const uint8_t test_pre_cache_prefix[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x0cu, 0x8bu, 0x45u, 0x08u, 0x81u, 0x78u, 0x04u, 0xffu,
    0xffu, 0x07u, 0x00u, 0x53u, 0x56u, 0x57u, 0x89u, 0x4cu,
    0x24u, 0x10u, 0x0fu, 0x84u, 0xc5u, 0x00u, 0x00u, 0x00u
};

static const uint8_t test_pre_cache_new_tail[] = {
    0x24u, 0x10u, 0x89u, 0x4cu, 0x24u, 0x14u, 0xe8u, 0xf5u,
    0x1du, 0x1cu, 0x00u, 0x33u, 0xffu, 0x6au, 0x00u, 0x56u,
    0x8du, 0x57u, 0x04u, 0x8du, 0x44u, 0x24u, 0x18u, 0xe8u,
    0xb4u, 0x60u, 0x1cu, 0x00u, 0x83u, 0xc4u, 0x08u, 0xffu,
    0x83u, 0x24u, 0x04u, 0x00u, 0x00u, 0x32u, 0xc0u, 0x5fu,
    0x5eu, 0x5bu, 0x8bu, 0xe5u, 0x5du, 0xc2u, 0x04u, 0x00u
};

static const uint8_t test_pre_cache_existing_tail[] = {
    0x8bu, 0x44u, 0x24u, 0x10u, 0x8du, 0x14u, 0x5bu, 0xffu,
    0x84u, 0xd0u, 0x24u, 0x04u, 0x00u, 0x00u, 0x5fu, 0x8du,
    0x84u, 0xd0u, 0x24u, 0x04u, 0x00u, 0x00u, 0x5eu, 0xb0u,
    0x01u, 0x5bu, 0x8bu, 0xe5u, 0x5du, 0xc2u, 0x04u, 0x00u
};

static const uint8_t test_un_cache_prefix[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u, 0x8bu, 0x70u, 0x04u,
    0x81u, 0xfeu, 0xffu, 0xffu, 0x07u, 0x00u, 0x74u, 0x15u,
    0x33u, 0xc0u, 0x8du, 0x91u, 0x1cu, 0x04u, 0x00u, 0x00u,
    0x39u, 0x32u, 0x74u, 0x0fu, 0x40u, 0x83u, 0xc2u, 0x18u,
    0x83u, 0xf8u, 0x40u, 0x7cu, 0xf3u, 0x32u, 0xc0u, 0x5eu,
    0xc2u, 0x04u, 0x00u, 0x8du, 0x14u, 0x40u, 0xffu, 0x8cu,
    0xd1u, 0x24u, 0x04u, 0x00u, 0x00u
};

static const uint8_t test_un_cache_suffix[] = {
    0xc7u, 0x87u, 0x24u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x5fu, 0xb0u, 0x01u, 0x5eu, 0xc2u, 0x04u,
    0x00u
};

typedef struct SpiritVfxTestState {
    void *tal;
    void *resolved_tal;
    void *resolved_tal_after_drift;
    unsigned int resolve_tal_drift_on_call;
    void *manager;
    BOOL tal_is_ready;
    BOOL resource_name_result;
    uint32_t resource_identifier;
    uint32_t constructed_resource_identifier;
    uint32_t resource_text_reference;
    BOOL snapshot_result;
    unsigned int snapshot_fail_on_call;
    SudekiMpLanArenaSpiritVfxCacheSnapshot snapshot;
    BOOL mutate_snapshot_on_pre_cache;
    SudekiMpLanArenaSpiritVfxCacheSnapshot after_pre_cache;
    BOOL pre_cache_result;
    BOOL un_cache_result;
    BOOL mutate_snapshot_on_un_cache;
    SudekiMpLanArenaSpiritVfxCacheSnapshot after_un_cache;
    SudekiMpLanArenaSpiritVfxCacheLease *reentrant_lease;
    const SudekiMpLanArenaSpiritVfxReplayApi *reentrant_api;
    unsigned int resolve_tal_calls;
    unsigned int tal_ready_calls;
    unsigned int get_manager_calls;
    unsigned int resource_name_calls;
    unsigned int release_resource_name_calls;
    unsigned int cache_snapshot_calls;
    unsigned int pre_cache_calls;
    unsigned int un_cache_calls;
    unsigned int play_sfx_calls;
    char requested_resource[64];
    char call_order[32];
    size_t call_order_length;
    void *played_manager;
    SudekiMpLanArenaSpiritVfxTransientTPtr played_actor;
    uint32_t played_resource_identifier;
    BOOL played_follow_character;
    BOOL played_real_time;
    float played_x;
    float played_y;
    float played_z;
    LONG active_calls_during_play;
    LONG active_calls_during_uncache;
    unsigned int reentrant_checks;
} SpiritVfxTestState;

static int failures;
static void *native_stub_tal;

void SudekiMpLogFormat(const char *format, ...) { (void)format; }

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s (error=%lu)\n", \
            __FILE__, __LINE__, #condition, (unsigned long)GetLastError()); \
        ++failures; \
    } \
} while (0)

/* The production adapter references these cleanroom helpers from its native
 * bridge. Seam tests never enter that bridge, but deterministic stubs keep
 * this focused test independently linkable. */
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    return actor == SUDEKIMP_CLEANROOM_TAL ? native_stub_tal : NULL;
}

BOOL SudekiMpCleanroomEngineResourceNameFromText(
    SudekiMpResourceName *resource_name,
    const char *text
) {
    (void)resource_name;
    (void)text;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

void SudekiMpCleanroomEngineReleaseResourceName(
    SudekiMpResourceName *resource_name
) {
    (void)resource_name;
}

static void append_call(SpiritVfxTestState *state, char call) {
    if (state->call_order_length + 1u >= sizeof(state->call_order)) return;
    state->call_order[state->call_order_length++] = call;
    state->call_order[state->call_order_length] = '\0';
}

static void reset_state(SpiritVfxTestState *state) {
    memset(state, 0, sizeof(*state));
    state->tal = (void *)(uintptr_t)0x10001000u;
    state->resolved_tal = state->tal;
    state->manager = (void *)(uintptr_t)0x20002000u;
    state->tal_is_ready = TRUE;
    state->resource_name_result = TRUE;
    state->resource_identifier = UINT32_C(0x3cef3b8f);
    state->constructed_resource_identifier = state->resource_identifier;
    state->resource_text_reference = 1u;
    state->snapshot_result = TRUE;
    state->pre_cache_result = FALSE;
    state->un_cache_result = TRUE;
}

static void reset_counts(SpiritVfxTestState *state) {
    state->resolve_tal_calls = 0u;
    state->tal_ready_calls = 0u;
    state->get_manager_calls = 0u;
    state->resource_name_calls = 0u;
    state->release_resource_name_calls = 0u;
    state->cache_snapshot_calls = 0u;
    state->pre_cache_calls = 0u;
    state->un_cache_calls = 0u;
    state->play_sfx_calls = 0u;
    state->requested_resource[0] = '\0';
    state->call_order[0] = '\0';
    state->call_order_length = 0u;
    state->active_calls_during_play = 0;
    state->active_calls_during_uncache = 0;
}

static void *test_resolve_tal(void *context) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->resolve_tal_calls;
    append_call(state, 'T');
    if (state->resolve_tal_drift_on_call != 0u &&
        state->resolve_tal_calls >= state->resolve_tal_drift_on_call) {
        return state->resolved_tal_after_drift;
    }
    return state->resolved_tal;
}

static BOOL test_tal_ready(void *context, void *tal) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->tal_ready_calls;
    append_call(state, 'A');
    return tal == state->tal && state->tal_is_ready;
}

static void *test_get_sfx_manager(void *context) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->get_manager_calls;
    append_call(state, 'M');
    return state->manager;
}

static BOOL test_resource_name_from_text(
    void *context,
    SudekiMpResourceName *resource_name,
    const char *text
) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->resource_name_calls;
    append_call(state, 'N');
    if (text != NULL) {
        strncpy(state->requested_resource, text,
            sizeof(state->requested_resource) - 1u);
        state->requested_resource[sizeof(state->requested_resource) - 1u] =
            '\0';
    }
    if (!state->resource_name_result || resource_name == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    memset(resource_name, 0, sizeof(*resource_name));
    resource_name->identifier = state->constructed_resource_identifier;
    resource_name->text_reference = &state->resource_text_reference;
    return TRUE;
}

static void test_release_resource_name(
    void *context,
    SudekiMpResourceName *resource_name
) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->release_resource_name_calls;
    append_call(state, 'R');
    if (resource_name != NULL) memset(resource_name, 0, sizeof(*resource_name));
}

static BOOL test_cache_snapshot(
    void *context,
    void *manager,
    uint32_t resource_identifier,
    SudekiMpLanArenaSpiritVfxCacheSnapshot *snapshot
) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->cache_snapshot_calls;
    append_call(state, 'S');
    if (!state->snapshot_result ||
        state->cache_snapshot_calls == state->snapshot_fail_on_call ||
        manager != state->manager ||
        resource_identifier != state->resource_identifier ||
        snapshot == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    *snapshot = state->snapshot;
    return TRUE;
}

static BOOL test_pre_cache_effect(
    void *context,
    void *manager,
    SudekiMpResourceName *resource_name
) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->pre_cache_calls;
    append_call(state, 'C');
    if (manager != state->manager || resource_name == NULL ||
        resource_name->identifier != state->resource_identifier) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (state->mutate_snapshot_on_pre_cache) {
        state->snapshot = state->after_pre_cache;
    }
    return state->pre_cache_result;
}

static BOOL test_un_cache_effect(
    void *context,
    void *manager,
    SudekiMpResourceName *resource_name
) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->un_cache_calls;
    state->active_calls_during_uncache =
        SudekiMpLanArenaSpiritVfxReplayActiveCalls();
    append_call(state, 'U');
    if (manager != state->manager || resource_name == NULL ||
        resource_name->identifier != state->resource_identifier) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (state->mutate_snapshot_on_un_cache) {
        state->snapshot = state->after_un_cache;
    }
    return state->un_cache_result;
}

static void test_play_sfx(
    void *context,
    void *manager,
    SudekiMpLanArenaSpiritVfxTransientTPtr *actor,
    SudekiMpResourceName *resource_name,
    BOOL follow_character,
    BOOL real_time,
    float x,
    float y,
    float z
) {
    SpiritVfxTestState *state = (SpiritVfxTestState *)context;
    ++state->play_sfx_calls;
    state->active_calls_during_play =
        SudekiMpLanArenaSpiritVfxReplayActiveCalls();
    append_call(state, 'P');
    state->played_manager = manager;
    if (actor != NULL) state->played_actor = *actor;
    state->played_resource_identifier = resource_name != NULL ?
        resource_name->identifier : UINT32_MAX;
    state->played_follow_character = follow_character;
    state->played_real_time = real_time;
    state->played_x = x;
    state->played_y = y;
    state->played_z = z;
    if (state->reentrant_lease != NULL && state->reentrant_api != NULL) {
        SudekiMpLanArenaSpiritVfxCacheLease before =
            *state->reentrant_lease;
        unsigned int pre_cache_calls = state->pre_cache_calls;
        unsigned int un_cache_calls = state->un_cache_calls;
        unsigned int play_sfx_calls = state->play_sfx_calls;

        CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(
            state->reentrant_lease, state->reentrant_api));
        CHECK(GetLastError() == ERROR_BUSY);
        CHECK(!SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
            state->reentrant_lease, state->reentrant_api));
        CHECK(GetLastError() == ERROR_BUSY);
        CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
            state->tal, state->reentrant_lease, state->reentrant_api));
        CHECK(GetLastError() == ERROR_BUSY);
        CHECK(memcmp(&before, state->reentrant_lease,
            sizeof(before)) == 0);
        CHECK(state->pre_cache_calls == pre_cache_calls);
        CHECK(state->un_cache_calls == un_cache_calls);
        CHECK(state->play_sfx_calls == play_sfx_calls);
        ++state->reentrant_checks;
    }
}

static SudekiMpLanArenaSpiritVfxReplayApi make_api(
    SpiritVfxTestState *state
) {
    SudekiMpLanArenaSpiritVfxReplayApi api;
    memset(&api, 0, sizeof(api));
    api.context = state;
    api.resolve_tal = test_resolve_tal;
    api.tal_ready = test_tal_ready;
    api.get_sfx_manager = test_get_sfx_manager;
    api.resource_name_from_text = test_resource_name_from_text;
    api.release_resource_name = test_release_resource_name;
    api.cache_snapshot = test_cache_snapshot;
    api.pre_cache_effect = test_pre_cache_effect;
    api.un_cache_effect = test_un_cache_effect;
    api.play_sfx = test_play_sfx;
    return api;
}

static void configure_new_pending_lease(SpiritVfxTestState *state) {
    memset(&state->snapshot, 0, sizeof(state->snapshot));
    memset(&state->after_pre_cache, 0, sizeof(state->after_pre_cache));
    state->after_pre_cache.matching_slots = 1u;
    state->after_pre_cache.slot_index = 7u;
    state->after_pre_cache.ref_count = 1;
    state->after_pre_cache.pending = TRUE;
    state->after_pre_cache.loaded = FALSE;
    state->mutate_snapshot_on_pre_cache = TRUE;
    state->pre_cache_result = FALSE;
}

static void test_new_lease_polls_then_replays_once(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    configure_new_pending_lease(&state);

    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(GetLastError() == ERROR_IO_PENDING);
    CHECK(state.pre_cache_calls == 1u);
    /* Two stable pre-admission observations bracket a manager identity
     * recheck; the third observation proves the native refcount delta. */
    CHECK(state.cache_snapshot_calls == 3u);
    CHECK(state.resource_name_calls == 1u);
    CHECK(state.release_resource_name_calls == 1u);
    CHECK(strcmp(state.requested_resource, "SFXSS250_Initiate.HOM") == 0);
    CHECK(lease.manager == state.manager);
    CHECK(lease.resource_identifier == state.resource_identifier);
    CHECK(lease.slot_index == 7);
    CHECK(lease.acquired_ref_count == 1);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_LOADING);

    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(GetLastError() == ERROR_IO_PENDING);
    CHECK(state.pre_cache_calls == 1u);
    CHECK(state.resource_name_calls == 1u);
    CHECK(state.release_resource_name_calls == 1u);

    state.snapshot.pending = FALSE;
    state.snapshot.loaded = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
    CHECK(state.pre_cache_calls == 1u);
    CHECK(SudekiMpLanArenaSpiritVfxTalInitiateCacheReadyWithApi(
        &lease, &api));
    CHECK(state.pre_cache_calls == 1u);

    reset_counts(&state);
    CHECK(SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.play_sfx_calls == 1u);
    CHECK(state.un_cache_calls == 1u);
    CHECK(state.resource_name_calls == 1u);
    CHECK(state.release_resource_name_calls == 1u);
    CHECK(strcmp(state.requested_resource, "SFXSS250_Initiate.HOM") == 0);
    CHECK(state.played_manager == state.manager);
    CHECK(state.played_actor.object == state.tal);
    CHECK(state.played_actor.previous_observer == NULL);
    CHECK(state.played_actor.next_observer == NULL);
    CHECK(state.played_resource_identifier == state.resource_identifier);
    CHECK(!state.played_follow_character && !state.played_real_time);
    CHECK(state.played_x == 0.0f && state.played_y == 0.0f &&
        state.played_z == 0.0f);
    CHECK(state.active_calls_during_play == 1);
    CHECK(state.active_calls_during_uncache == 1);
    {
        const char *play = strchr(state.call_order, 'P');
        const char *uncache = strchr(state.call_order, 'U');
        const char *release = strrchr(state.call_order, 'R');
        CHECK(play != NULL && uncache != NULL && release != NULL &&
            play < uncache && uncache < release);
    }
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);
    CHECK(lease.manager == NULL);
    CHECK(SudekiMpLanArenaSpiritVfxReplayActiveCalls() == 0);

    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.play_sfx_calls == 1u);
    CHECK(state.un_cache_calls == 1u);
}

static void test_existing_ready_lease_and_explicit_release(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    state.snapshot.matching_slots = 1u;
    state.snapshot.slot_index = 4u;
    state.snapshot.ref_count = 3;
    state.snapshot.loaded = TRUE;
    state.after_pre_cache = state.snapshot;
    state.after_pre_cache.ref_count = 4;
    state.mutate_snapshot_on_pre_cache = TRUE;
    state.pre_cache_result = TRUE;

    CHECK(SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.pre_cache_calls == 1u);
    CHECK(lease.slot_index == 4);
    CHECK(lease.acquired_ref_count == 4);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 1u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);
    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 1u);
}

static void test_acquisition_requires_exact_refcount_witness(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    state.snapshot.matching_slots = 2u;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.pre_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    configure_new_pending_lease(&state);
    state.after_pre_cache.ref_count = 2;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.pre_cache_calls == 1u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_POISONED);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.pre_cache_calls == 1u);

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    state.snapshot.matching_slots = 1u;
    state.snapshot.slot_index = 1u;
    state.snapshot.ref_count = LONG_MAX;
    state.snapshot.loaded = TRUE;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.pre_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);
}

static void test_failed_post_snapshot_poison_is_stable(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    configure_new_pending_lease(&state);
    /* The callback deliberately leaves its output untouched on the failed
     * post-admission observation. Production must not read stack garbage. */
    state.snapshot_fail_on_call = 3u;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    CHECK(state.pre_cache_calls == 1u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_POISONED);
    CHECK(lease.manager == state.manager);
    CHECK(lease.resource_identifier == state.resource_identifier);
    CHECK(lease.slot_index == -1);
    CHECK(lease.acquired_ref_count == 0);

    state.snapshot_fail_on_call = 0u;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_STATE);
    CHECK(state.pre_cache_calls == 1u);
}

static void acquire_ready_lease(
    SpiritVfxTestState *state,
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    memset(lease, 0, sizeof(*lease));
    configure_new_pending_lease(state);
    state->after_pre_cache.pending = FALSE;
    state->after_pre_cache.loaded = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(lease, api));
    CHECK(lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
}

static void test_post_replay_cleanup_retains_obligation_and_retries(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    api = make_api(&state);
    acquire_ready_lease(&state, &lease, &api);
    reset_counts(&state);
    state.reentrant_lease = &lease;
    state.reentrant_api = &api;
    /* Replay has two pre-admission cache observations. Fail only the third,
     * after native PlaySfx has consumed the event but before UnCache entry. */
    state.snapshot_fail_on_call = 3u;
    CHECK(SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.reentrant_checks == 1u);
    CHECK(state.play_sfx_calls == 1u);
    CHECK(state.un_cache_calls == 0u);
    CHECK(state.pre_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING);
    CHECK(lease.manager == state.manager);
    CHECK(lease.resource_identifier == state.resource_identifier);
    CHECK(lease.slot_index == 7 && lease.acquired_ref_count == 1);
    CHECK(SudekiMpLanArenaSpiritVfxReplayActiveCalls() == 0);

    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.play_sfx_calls == 1u);
    CHECK(!SudekiMpLanArenaSpiritVfxTalInitiateCacheReadyWithApi(
        &lease, &api));
    CHECK(GetLastError() == ERROR_BUSY);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING);

    state.snapshot_result = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.pre_cache_calls == 0u && state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING);

    state.snapshot_result = TRUE;
    state.snapshot_fail_on_call = 0u;
    state.mutate_snapshot_on_un_cache = TRUE;
    memset(&state.after_un_cache, 0, sizeof(state.after_un_cache));
    CHECK(SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.un_cache_calls == 1u);
    CHECK(state.pre_cache_calls == 1u);
    CHECK(state.play_sfx_calls == 1u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
    CHECK(lease.acquired_ref_count == 1);

    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 2u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);
    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 2u);
}

static void test_release_failure_and_manager_drift(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;
    void *manager_a;

    reset_state(&state);
    api = make_api(&state);
    acquire_ready_lease(&state, &lease, &api);
    state.resource_name_result = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
    state.resource_name_result = TRUE;
    state.un_cache_result = FALSE;
    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 1u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);

    reset_state(&state);
    api = make_api(&state);
    acquire_ready_lease(&state, &lease, &api);
    manager_a = state.manager;
    state.manager = (void *)(uintptr_t)0x30003000u;
    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);
    CHECK(lease.manager == NULL);
    CHECK(manager_a != state.manager);

    reset_state(&state);
    api = make_api(&state);
    memset(&lease, 0, sizeof(lease));
    configure_new_pending_lease(&state);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_LOADING);
    CHECK(SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(state.un_cache_calls == 1u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);
}

static void test_replay_rejections_preserve_unconsumed_lease(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    api = make_api(&state);
    acquire_ready_lease(&state, &lease, &api);
    reset_counts(&state);
    state.resolved_tal = (void *)(uintptr_t)0x11111000u;
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.play_sfx_calls == 0u && state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);

    state.resolved_tal = state.tal;
    state.resolved_tal_after_drift =
        (void *)(uintptr_t)0x11111000u;
    state.resolve_tal_calls = 0u;
    state.resolve_tal_drift_on_call = 2u;
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.resolve_tal_calls == 2u);
    CHECK(state.resource_name_calls == 0u);
    CHECK(state.release_resource_name_calls == 0u);
    CHECK(state.play_sfx_calls == 0u && state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);

    state.resolve_tal_drift_on_call = 0u;
    state.resolved_tal_after_drift = NULL;

    state.resolved_tal = state.tal;
    state.tal_is_ready = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.play_sfx_calls == 0u && state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);

    state.tal_is_ready = TRUE;
    state.constructed_resource_identifier = UINT32_C(0x7ffff);
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.play_sfx_calls == 0u && state.un_cache_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
}

static void test_base_name_identifier_rejected(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    configure_new_pending_lease(&state);
    /* A generic ResourceName accepts the basename, but that valid-looking ID
     * has no archive entry. Only the exact .HOM resource may acquire a lease. */
    state.constructed_resource_identifier = UINT32_C(0x15fef04d);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(state.resource_name_calls == 1u);
    CHECK(state.release_resource_name_calls == 1u);
    CHECK(strcmp(state.requested_resource, "SFXSS250_Initiate.HOM") == 0);
    CHECK(state.cache_snapshot_calls == 0u);
    CHECK(state.pre_cache_calls == 0u);
    CHECK(state.un_cache_calls == 0u);
    CHECK(state.play_sfx_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY);

    reset_state(&state);
    api = make_api(&state);
    acquire_ready_lease(&state, &lease, &api);
    reset_counts(&state);
    state.constructed_resource_identifier = UINT32_C(0x15fef04d);
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, &lease, &api));
    CHECK(state.resource_name_calls == 1u);
    CHECK(state.release_resource_name_calls == 1u);
    CHECK(state.pre_cache_calls == 0u);
    CHECK(state.un_cache_calls == 0u);
    CHECK(state.play_sfx_calls == 0u);
    CHECK(lease.state == SUDEKIMP_SPIRIT_VFX_CACHE_READY);
}

static void test_invalid_contracts(void) {
    SpiritVfxTestState state;
    SudekiMpLanArenaSpiritVfxCacheLease lease;
    SudekiMpLanArenaSpiritVfxReplayApi api;

    reset_state(&state);
    memset(&lease, 0, sizeof(lease));
    api = make_api(&state);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(NULL, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    api.cache_snapshot = NULL;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);

    api = make_api(&state);
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        NULL, &lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(!SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        state.tal, NULL, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);

    memset(&lease, 0, sizeof(lease));
    lease.state = SUDEKIMP_SPIRIT_VFX_CACHE_LOADING;
    lease.slot_index = 1;
    lease.resource_identifier = state.resource_identifier;
    CHECK(!SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(&lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(state.pre_cache_calls == 0u);

    memset(&lease, 0, sizeof(lease));
    lease.manager = state.manager;
    lease.resource_identifier = state.resource_identifier;
    lease.slot_index = 64;
    lease.state = SUDEKIMP_SPIRIT_VFX_CACHE_READY;
    CHECK(!SudekiMpLanArenaSpiritVfxTalInitiateCacheReadyWithApi(
        &lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(state.cache_snapshot_calls == 0u);

    memset(&lease, 0, sizeof(lease));
    lease.state = (SudekiMpLanArenaSpiritVfxCacheState)99;
    CHECK(!SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &lease, &api));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(state.un_cache_calls == 0u);
}

static uint8_t *allocate_image_fixture(void) {
    uint8_t *image = (uint8_t *)VirtualAlloc(NULL,
        TEST_FIXTURE_IMAGE_SIZE, MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    uint32_t relocated_manager_global;

    if (image == NULL) return NULL;
    memcpy(image + TEST_RVA_SFX_PLAY,
        test_sfx_play_body, sizeof(test_sfx_play_body));
    memcpy(image + TEST_RVA_SFX_PRE_CACHE,
        test_pre_cache_prefix, sizeof(test_pre_cache_prefix));
    memcpy(image + TEST_RVA_SFX_PRE_CACHE + 0xc0u,
        test_pre_cache_new_tail, sizeof(test_pre_cache_new_tail));
    memcpy(image + TEST_RVA_SFX_PRE_CACHE + 0xf0u,
        test_pre_cache_existing_tail,
        sizeof(test_pre_cache_existing_tail));
    memcpy(image + TEST_RVA_SFX_UN_CACHE,
        test_un_cache_prefix, sizeof(test_un_cache_prefix));
    memcpy(image + TEST_RVA_SFX_UN_CACHE + 0x5eu,
        test_un_cache_suffix, sizeof(test_un_cache_suffix));
    image[TEST_RVA_SFX_GET_MANAGER] = 0xa1u;
    image[TEST_RVA_SFX_GET_MANAGER + 5u] = 0xc3u;
    relocated_manager_global = (uint32_t)(uintptr_t)(
        image + TEST_RVA_SFX_MANAGER_GLOBAL);
    memcpy(image + TEST_RVA_SFX_GET_MANAGER + 1u,
        &relocated_manager_global, sizeof(relocated_manager_global));
    return image;
}

static void expect_mutation_rejected(
    uint8_t *image,
    size_t offset
) {
    uint8_t saved = image[offset];
    image[offset] ^= 1u;
    CHECK(!SudekiMpLanArenaSpiritVfxReplayImageMatches((HMODULE)image));
    image[offset] = saved;
    CHECK(SudekiMpLanArenaSpiritVfxReplayImageMatches((HMODULE)image));
}

static void test_exact_image_preflight(void) {
    uint8_t *image = allocate_image_fixture();
    uint32_t manager_operand;

    CHECK(image != NULL);
    if (image == NULL) return;
    CHECK(SudekiMpLanArenaSpiritVfxReplayImageMatches((HMODULE)image));
    expect_mutation_rejected(image, TEST_RVA_SFX_PLAY + 0x44u);
    expect_mutation_rejected(image, TEST_RVA_SFX_PRE_CACHE + 0x00u);
    expect_mutation_rejected(image, TEST_RVA_SFX_PRE_CACHE + 0xc0u);
    expect_mutation_rejected(image, TEST_RVA_SFX_PRE_CACHE + 0xf0u);
    expect_mutation_rejected(image, TEST_RVA_SFX_UN_CACHE + 0x00u);
    expect_mutation_rejected(image, TEST_RVA_SFX_UN_CACHE + 0x5eu);
    expect_mutation_rejected(image, TEST_RVA_SFX_GET_MANAGER + 5u);

    memcpy(&manager_operand, image + TEST_RVA_SFX_GET_MANAGER + 1u,
        sizeof(manager_operand));
    ++manager_operand;
    memcpy(image + TEST_RVA_SFX_GET_MANAGER + 1u,
        &manager_operand, sizeof(manager_operand));
    CHECK(!SudekiMpLanArenaSpiritVfxReplayImageMatches((HMODULE)image));
    VirtualFree(image, 0u, MEM_RELEASE);
}

static uint8_t *read_file(const char *path, DWORD *size) {
    HANDLE file;
    DWORD length;
    DWORD read;
    uint8_t *bytes;

    if (path == NULL || size == NULL) return NULL;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    length = GetFileSize(file, NULL);
    if (length == INVALID_FILE_SIZE || length == 0u) {
        CloseHandle(file);
        return NULL;
    }
    bytes = (uint8_t *)malloc(length);
    if (bytes == NULL || !ReadFile(file, bytes, length, &read, NULL) ||
        read != length) {
        free(bytes);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    *size = length;
    return bytes;
}

static uint8_t *map_pe_image(const uint8_t *file, DWORD file_size) {
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *sections;
    uint8_t *image;
    unsigned int index;

    if (file == NULL || file_size < sizeof(IMAGE_DOS_HEADER)) return NULL;
    dos = (const IMAGE_DOS_HEADER *)file;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (DWORD)dos->e_lfanew > file_size - sizeof(IMAGE_NT_HEADERS32)) {
        return NULL;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage == 0u ||
        nt->OptionalHeader.SizeOfHeaders > file_size) {
        return NULL;
    }
    image = (uint8_t *)VirtualAlloc(NULL, nt->OptionalHeader.SizeOfImage,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (image == NULL) return NULL;
    memcpy(image, file, nt->OptionalHeader.SizeOfHeaders);
    sections = IMAGE_FIRST_SECTION(nt);
    for (index = 0u; index < nt->FileHeader.NumberOfSections; ++index) {
        DWORD source = sections[index].PointerToRawData;
        DWORD length = sections[index].SizeOfRawData;
        DWORD destination = sections[index].VirtualAddress;

        if (length == 0u) continue;
        if (source > file_size || length > file_size - source ||
            destination > nt->OptionalHeader.SizeOfImage ||
            length > nt->OptionalHeader.SizeOfImage - destination) {
            VirtualFree(image, 0u, MEM_RELEASE);
            return NULL;
        }
        memcpy(image + destination, file + source, length);
    }
    return image;
}

static void test_exact_supported_image(const char *path) {
    DWORD file_size = 0u;
    uint8_t *file = read_file(path, &file_size);
    uint8_t *image;
    uint32_t relocated_manager_global;
    unsigned int listener_offset;

    CHECK(file != NULL);
    if (file == NULL) return;
    image = map_pe_image(file, file_size);
    CHECK(image != NULL);
    if (image != NULL) {
        relocated_manager_global = (uint32_t)(uintptr_t)(
            image + TEST_RVA_SFX_MANAGER_GLOBAL);
        memcpy(image + TEST_RVA_SFX_GET_MANAGER + 1u,
            &relocated_manager_global, sizeof(relocated_manager_global));
        CHECK(SudekiMpLanArenaSpiritVfxReplayImageMatches((HMODULE)image));
        *(void **)(image + 0x2df8ecu + 0xf8u) = image + 0x21bb10u;
        *(void **)(image + 0x2df8ecu + 0x10cu) = image + 0x223180u;
        *(void **)(image + 0x2df8ecu + 0x110u) = image + 0x223220u;
        for (listener_offset = 4u; listener_offset < 0x3cu; listener_offset += 4u) {
            uint32_t target = *(uint32_t *)(image + 0x2cd0f4u + listener_offset);
            *(void **)(image + 0x2cd0f4u + listener_offset) =
                image + (target - 0x400000u);
        }
        CHECK(SudekiMpLanArenaSpiritVfxVisualImageMatches((HMODULE)image));
        *(void **)(image + 0x2cd0f4u + 4u) = image + 0x131d20u;
        CHECK(!SudekiMpLanArenaSpiritVfxVisualImageMatches((HMODULE)image));
        *(void **)(image + 0x2cd0f4u + 4u) = image + 0x3b870u;
        image[0x1750u] ^= 1u;
        CHECK(!SudekiMpLanArenaSpiritVfxVisualImageMatches((HMODULE)image));
        image[0x1750u] ^= 1u;
        image[0x17da0u + 0xaau] ^= 1u;
        CHECK(!SudekiMpLanArenaSpiritVfxVisualImageMatches((HMODULE)image));
        VirtualFree(image, 0u, MEM_RELEASE);
    }
    free(file);
}

typedef struct VisualTestContext {
    SpiritVfxTestState cache;
    unsigned int spawn_calls, sync_calls, retire_calls, detach_calls;
    BOOL spawn_allowed, spawn_null, retire_allowed, detach_allowed;
    BOOL test_reentry;
    SudekiMpLanArenaSpiritVfxVisualState *state;
    const SudekiMpLanArenaSpiritVfxVisualApi *api;
    const SudekiMpLanArenaSnapshot *snapshot;
} VisualTestContext;

static BOOL test_visual_spawn(void *context, void *manager,
    const SudekiMpLanArenaSpiritVfxSnapshot *visual,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    VisualTestContext *test = (VisualTestContext *)context;
    CHECK(manager == test->cache.manager);
    CHECK(SudekiMpLanArenaSpiritVfxReplayActiveCalls() == 1);
    if (!test->spawn_allowed) return FALSE;
    ++test->spawn_calls;
    if (!test->spawn_null)
        observer->object = (void *)(uintptr_t)(0x400000u + visual->instance_sequence);
    if (test->test_reentry) {
        SudekiMpLanArenaSpiritVfxVisualState before = *test->state;
        CHECK(!SudekiMpLanArenaSpiritVfxResetVisualsWithApi(test->state, test->api));
        CHECK(GetLastError() == ERROR_BUSY);
        CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(test->state,
            test->snapshot, 10u, test->api));
        CHECK(GetLastError() == ERROR_BUSY);
        CHECK(memcmp(&before, test->state, sizeof(before)) == 0);
    }
    return TRUE;
}

static BOOL test_visual_sync(void *context,
    const SudekiMpLanArenaSpiritVfxSnapshot *visual,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    VisualTestContext *test = (VisualTestContext *)context;
    CHECK(observer->object != NULL); CHECK(visual->instance_sequence != 0u);
    ++test->sync_calls; return TRUE;
}

static BOOL test_visual_retire(void *context,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    VisualTestContext *test = (VisualTestContext *)context;
    (void)observer; ++test->retire_calls; return test->retire_allowed;
}

static BOOL test_visual_detach(void *context,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    VisualTestContext *test = (VisualTestContext *)context;
    ++test->detach_calls;
    if (!test->detach_allowed) return FALSE;
    ZeroMemory(observer, sizeof(*observer)); return TRUE;
}

static void setup_visual_test(VisualTestContext *test,
    SudekiMpLanArenaSpiritVfxVisualState *state,
    SudekiMpLanArenaSpiritVfxVisualApi *api, SudekiMpLanArenaSnapshot *snapshot) {
    ZeroMemory(test, sizeof(*test)); ZeroMemory(state, sizeof(*state));
    ZeroMemory(api, sizeof(*api)); ZeroMemory(snapshot, sizeof(*snapshot));
    reset_state(&test->cache); configure_new_pending_lease(&test->cache);
    test->cache.after_pre_cache.pending = FALSE;
    test->cache.after_pre_cache.loaded = TRUE;
    api->cache = make_api(&test->cache);
    api->spawn = test_visual_spawn; api->synchronize = test_visual_sync;
    api->retire = test_visual_retire; api->detach = test_visual_detach;
    test->spawn_allowed = TRUE; test->retire_allowed = TRUE;
    test->detach_allowed = TRUE; test->state = state; test->api = api;
    test->snapshot = snapshot;
    snapshot->spirit_vfx_observed = 1u; snapshot->spirit_vfx_count = 1u;
    snapshot->spirit_vfx[0].instance_sequence = 1u;
    snapshot->spirit_vfx[0].skill_sequence = 5u;
    snapshot->spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_INITIATE;
    snapshot->spirit_vfx[0].rotation_xyzw[3] = 1.0f;
    snapshot->spirit_vfx[0].scale[0] = 1.0f;
    snapshot->spirit_vfx[0].scale[1] = 1.0f;
    snapshot->spirit_vfx[0].scale[2] = 1.0f;
}

static void test_visual_roster_once_unknown_and_retirement(void) {
    VisualTestContext test; SudekiMpLanArenaSpiritVfxVisualState state;
    SudekiMpLanArenaSpiritVfxVisualApi api; SudekiMpLanArenaSnapshot snapshot;
    setup_visual_test(&test, &state, &api, &snapshot);
    test.test_reentry = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.sync_calls == 1u);
    CHECK(test.cache.play_sfx_calls == 0u && test.cache.resolve_tal_calls == 0u);
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.sync_calls == 2u);
    snapshot.spirit_vfx_observed = 0u; snapshot.spirit_vfx_count = 0u;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.retire_calls == 0u && state.slots[0].occupied);
    snapshot.spirit_vfx_observed = 1u; snapshot.spirit_vfx_count = 1u;
    /* Native destruction nulls the stable weak node: never respawn. */
    state.slots[0].observer.object = NULL;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.sync_calls == 2u);
    snapshot.spirit_vfx_count = 0u;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.retire_calls == 1u && test.detach_calls == 1u);
    snapshot.spirit_vfx_count = 1u;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u); /* Retired identity cannot resurrect. */
    CHECK(SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
    CHECK(test.cache.un_cache_calls == 1u && state.session_token == 0u);
}

static void test_visual_pending_cleanup_and_invalid_roster(void) {
    VisualTestContext test; SudekiMpLanArenaSpiritVfxVisualState state, before;
    SudekiMpLanArenaSpiritVfxVisualApi api; SudekiMpLanArenaSnapshot snapshot;
    setup_visual_test(&test, &state, &api, &snapshot);
    test.cache.after_pre_cache.pending = TRUE;
    test.cache.after_pre_cache.loaded = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 0u && test.cache.pre_cache_calls == 1u);
    test.cache.snapshot.pending = FALSE; test.cache.snapshot.loaded = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.cache.pre_cache_calls == 1u);
    before = state; snapshot.spirit_vfx[0].skill_sequence++;
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    snapshot.spirit_vfx[0].skill_sequence--;
    snapshot.spirit_vfx_count = 2u; snapshot.spirit_vfx[1] = snapshot.spirit_vfx[0];
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    snapshot.spirit_vfx_count = 1u; test.retire_allowed = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
    CHECK(test.retire_calls == 1u && test.cache.un_cache_calls == 0u);
    test.retire_allowed = TRUE; test.detach_allowed = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
    CHECK(test.retire_calls == 2u && test.cache.un_cache_calls == 0u);
    test.detach_allowed = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
    CHECK(test.retire_calls == 2u && test.detach_calls == 2u);
    CHECK(test.cache.un_cache_calls == 1u);
}

static void test_visual_matrix_and_native_null(void) {
    VisualTestContext test; SudekiMpLanArenaSpiritVfxVisualState state;
    SudekiMpLanArenaSpiritVfxVisualApi api; SudekiMpLanArenaSnapshot snapshot;
    float matrix[16]; unsigned int kind;
    setup_visual_test(&test, &state, &api, &snapshot);
    for (kind = 1u; kind <= SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST; ++kind) {
        snapshot.spirit_vfx[0].kind = (uint8_t)kind;
        CHECK(SudekiMpLanArenaSpiritVfxVisualMatrix(&snapshot.spirit_vfx[0], matrix));
        CHECK(matrix[0] == 1.0f && matrix[5] == 1.0f && matrix[15] == 1.0f);
    }
    snapshot.spirit_vfx[0].kind = 1u;
    snapshot.spirit_vfx[0].position[0] = 12.0f;
    snapshot.spirit_vfx[0].rotation_xyzw[1] = 0.70710678f;
    snapshot.spirit_vfx[0].rotation_xyzw[3] = 0.70710678f;
    snapshot.spirit_vfx[0].scale[0] = 2.0f;
    CHECK(SudekiMpLanArenaSpiritVfxVisualMatrix(&snapshot.spirit_vfx[0], matrix));
    CHECK(fabsf(matrix[2] + 2.0f) < 0.00001f);
    CHECK(fabsf(matrix[8] - 1.0f) < 0.00001f && matrix[12] == 12.0f);
    snapshot.spirit_vfx[0].scale[0] = -1.0f;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualMatrix(&snapshot.spirit_vfx[0], matrix));
    snapshot.spirit_vfx[0].scale[0] = 1.0f;
    test.spawn_null = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.sync_calls == 0u);
    CHECK(SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
}

static void test_visual_fixed_resource_lifecycle(
    uint8_t kind, uint32_t identifier, const char *name) {
    VisualTestContext test; SudekiMpLanArenaSpiritVfxVisualState state, before;
    SudekiMpLanArenaSpiritVfxVisualApi api; SudekiMpLanArenaSnapshot snapshot;
    float matrix[16];
    setup_visual_test(&test, &state, &api, &snapshot);
    snapshot.spirit_vfx[0].kind = kind;
    test.cache.resource_identifier = identifier;
    test.cache.constructed_resource_identifier = identifier;
    test.cache.after_pre_cache.pending = TRUE;
    test.cache.after_pre_cache.loaded = FALSE;
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.cache.pre_cache_calls == 1u && test.spawn_calls == 0u);
    CHECK(strcmp(test.cache.requested_resource, name) == 0);
    CHECK(state.caches[kind - 1u].resource_identifier == identifier);
    test.cache.snapshot.pending = FALSE; test.cache.snapshot.loaded = TRUE;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.sync_calls == 1u);
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 1u && test.cache.pre_cache_calls == 1u);
    before = state;
    snapshot.spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualMatrix(&snapshot.spirit_vfx[0], matrix));
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    snapshot.spirit_vfx[0].kind = 0u;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualMatrix(&snapshot.spirit_vfx[0], matrix));
    snapshot.spirit_vfx_count = 0u;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.retire_calls == 1u && test.detach_calls == 1u);
    CHECK(test.cache.un_cache_calls == 0u);
    CHECK(SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
    CHECK(test.cache.un_cache_calls == 1u);
    CHECK(strcmp(test.cache.requested_resource, name) == 0);

    setup_visual_test(&test, &state, &api, &snapshot);
    snapshot.spirit_vfx[0].kind = kind;
    test.cache.resource_identifier = identifier;
    /* The old opening hash must never be accepted for a different kind. */
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.cache.pre_cache_calls == 0u && test.spawn_calls == 0u);
    CHECK(SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
}

static void test_visual_identity_preflight_precedes_any_retirement(void) {
    VisualTestContext test; SudekiMpLanArenaSpiritVfxVisualState state, before;
    SudekiMpLanArenaSpiritVfxVisualApi api; SudekiMpLanArenaSnapshot snapshot;
    setup_visual_test(&test, &state, &api, &snapshot);
    snapshot.spirit_vfx_count = 2u;
    snapshot.spirit_vfx[1] = snapshot.spirit_vfx[0];
    snapshot.spirit_vfx[1].instance_sequence = 2u;
    CHECK(SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(test.spawn_calls == 2u && test.sync_calls == 2u);
    before = state;
    /* Slot zero is absent, but slot one's immutable identity is malformed:
     * reject the whole batch before retiring the absent first slot. */
    snapshot.spirit_vfx_count = 1u;
    snapshot.spirit_vfx[0] = snapshot.spirit_vfx[1];
    snapshot.spirit_vfx[0].skill_sequence++;
    CHECK(!SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(&state, &snapshot, 10u, &api));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    CHECK(test.retire_calls == 0u && test.detach_calls == 0u);
    CHECK(test.spawn_calls == 2u && test.sync_calls == 2u);
    CHECK(SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&state, &api));
}

static void test_visual_native_sound_listener_identity(void) {
    uint8_t *entity = (uint8_t *)calloc(1u, 0x3e4u);
    uint8_t *base = (uint8_t *)(uintptr_t)0x400000u;
    void *listeners[1], *sources[1];
    CHECK(entity != NULL);
    if (entity == NULL) return;
    *(void **)entity = base + 0x2d3c7cu;
    *(void **)(entity + 0x44u) = entity + 0x160u;
    *(void **)(entity + 0x58u) = entity + 0x270u;
    *(void **)(entity + 0x160u) = base + 0x2cdefcu;
    *(void **)(entity + 0x270u) = base + 0x2c83f4u;
    *(void **)(entity + 0x170u) = entity;
    *(void **)(entity + 0x280u) = entity;
    *(void **)(entity + 0x5cu) = entity + 0x2e8u;
    *(void **)(entity + 0x2e8u) = base + 0x2cd0acu;
    *(void **)(entity + 0x2f8u) = entity;
    *(void **)(entity + 0x288u) = base + 0x2c8464u;
    *(void **)(entity + 0x300u) = base + 0x2cd0f4u;
    *(uint32_t *)(entity + 0x290u) = 1u;
    *(uint32_t *)(entity + 0x308u) = 1u;
    *(void ***)(entity + 0x298u) = listeners;
    *(void ***)(entity + 0x310u) = sources;
    listeners[0] = entity + 0x300u; sources[0] = entity + 0x288u;
    CHECK(SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(uint32_t *)(entity + 0x290u) = 0u;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(uint32_t *)(entity + 0x290u) = 2u;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(uint32_t *)(entity + 0x290u) = 1u;
    listeners[0] = entity + 0x148u;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    listeners[0] = entity + 0x300u;
    sources[0] = entity;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    sources[0] = entity + 0x288u;
    *(void **)(entity + 0x300u) = base + 0x2d3cfcu;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(void **)(entity + 0x300u) = base + 0x2cd0f4u;
    *(void ***)(entity + 0x298u) = NULL;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(void ***)(entity + 0x298u) = listeners;
    *(void **)(entity + 0x2f8u) = NULL;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(void **)(entity + 0x2f8u) = entity;
    *(void **)(entity + 0x3d4u) = entity;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(void **)(entity + 0x3d4u) = NULL;
    *(uint32_t *)(entity + 0x150u) = 1u;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    *(uint32_t *)(entity + 0x150u) = 0u;
    CHECK(SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)base, entity));
    free(entity);
}

static void test_visual_opening_phase_never_rewinds(void) {
    BOOL apply = TRUE;
    unsigned int kind;
    float native_phase = 0.0f;
    /* Includes observed backward targets, repeated packets and forward
     * catch-up. Native playback between samples is independent of the wire. */
    static const float targets[] = {0.384f, 0.384f, 1.896f, 1.896f,
        5.184f, 5.184f, 6.408f, 6.408f, 7.2f, 7.176f};
    unsigned int i;
    CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE, 1.920f, 1.896f, &apply));
    CHECK(!apply);
    CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE, 5.496f, 5.184f, &apply));
    CHECK(!apply);
    for (i = 0u; i < sizeof(targets) / sizeof(targets[0]); ++i) {
        float before = native_phase;
        CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
            SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE,
            native_phase, targets[i], &apply));
        if (apply) native_phase = targets[i];
        CHECK(native_phase >= before);
        native_phase += 0.384f;
    }
    CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE, 0.0f, 20.0f, &apply));
    CHECK(apply); /* A newly created or late effect can catch up. */
    CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE, 20.0f, 20.0f, &apply));
    CHECK(!apply);
    CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE, 181.0f, 180.0f, &apply));
    CHECK(!apply); /* Tail cleanup belongs to positive roster removal. */
    for (kind = 1u; kind <= SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST; ++kind) {
        if (kind == SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE) continue;
        CHECK(SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(kind, 125.0f, 0.0f, &apply));
        CHECK(apply); /* Other authored wrap/correction contracts unchanged. */
    }
    apply = TRUE;
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(12u, NAN, 0.0f, &apply));
    CHECK(apply);
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(12u, 0.0f, INFINITY, &apply));
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(12u, -1.0f, 0.0f, &apply));
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(12u, 0.0f, -1.0f, &apply));
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(12u, 1000001.0f, 0.0f, &apply));
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(0u, 0.0f, 0.0f, &apply));
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST + 1u, 0.0f, 0.0f, &apply));
    CHECK(!SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(12u, 0.0f, 0.0f, NULL));
}

int main(int argc, char **argv) {
    test_visual_opening_phase_never_rewinds();
    test_visual_roster_once_unknown_and_retirement();
    test_visual_pending_cleanup_and_invalid_roster();
    test_visual_matrix_and_native_null();
    test_visual_identity_preflight_precedes_any_retirement();
    test_visual_native_sound_listener_identity();
    test_visual_fixed_resource_lifecycle(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE, UINT32_C(0x62dcc5a3),
        "SFXSS900_generic_initate.HOM");
    test_visual_fixed_resource_lifecycle(
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_HIT, UINT32_C(0xaeec0c83),
        "SFXSS351_Tal_Hit_Character.HOM");
    test_new_lease_polls_then_replays_once();
    test_existing_ready_lease_and_explicit_release();
    test_acquisition_requires_exact_refcount_witness();
    test_failed_post_snapshot_poison_is_stable();
    test_post_replay_cleanup_retains_obligation_and_retries();
    test_release_failure_and_manager_drift();
    test_replay_rejections_preserve_unconsumed_lease();
    test_base_name_identifier_rejected();
    test_invalid_contracts();
    test_exact_image_preflight();
    if (argc > 1) test_exact_supported_image(argv[1]);
    if (failures != 0) {
        fprintf(stderr, "%d lan arena spirit VFX test(s) failed\n", failures);
        return 1;
    }
    puts("lan arena spirit VFX tests passed");
    return 0;
}
