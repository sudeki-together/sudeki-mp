#include "hooks/talos_companion_staging_research_adapter.h"

#include "engine/log.h"
#include "hooks/control_separation.h"
#include "hooks/talos_companion_membership_abi.h"

#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Talos companion staging research adapter requires 32-bit Windows"
#endif

enum {
    HERO_COUNT = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT,
    HERO_ELCO = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO
};

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
typedef SudekiMpTalosStagingResearchAdapterRawSample AdapterRawSample;
typedef SudekiMpTalosStagingResearchAdapterTestBackend AdapterBackend;
#else
typedef struct AdapterBackend {
    void *context;
    void (*publish)(
        void *context,
        const SudekiMpTalosStagingResearchAdapterStatus *status
    );
} AdapterBackend;
#endif

static uint8_t *game_base;
static HMODULE pinned_module;
static UINT proof_virtual_key;
static BOOL installed_exact_executable_hash;
static BOOL installed_exact_sol_hash;
static volatile LONG adapter_active;
static volatile LONG attempt_claimed;
static volatile LONG status_sequence;
static volatile LONG status_writer_lock;
static BOOL hotkey_was_down;
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
static uint64_t identity_token_key;
static uint64_t next_attempt_serial;
#endif
static SudekiMpTalosStagingResearchAdapterStatus public_status;
static AdapterBackend active_backend;

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
static AdapterBackend injected_backend;
static BOOL injected_backend_set;
#endif

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
static uint64_t mix_identity(uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value == 0u ? UINT64_C(1) : value;
}

static uint64_t pointer_token(const void *pointer) {
    if (pointer == NULL || identity_token_key == 0u) return 0u;
    return mix_identity((uint64_t)(uintptr_t)pointer ^ identity_token_key);
}

static uint64_t integer_token(uint64_t value) {
    if (value == 0u || identity_token_key == 0u) return 0u;
    return mix_identity(value ^ identity_token_key);
}
#endif

static uint64_t advance_nonzero(uint64_t value) {
    ++value;
    if (value == 0u) ++value;
    return value;
}

static void begin_status_write(void) {
    while (InterlockedCompareExchange(&status_writer_lock, 1, 0) != 0) {
        /* Publishing happens outside the mutation window. A CPU-only spin
         * avoids making writer exclusion itself a scheduler contract. */
    }
    (void)InterlockedIncrement(&status_sequence);
    MemoryBarrier();
}

static void end_status_write(void) {
    MemoryBarrier();
    (void)InterlockedIncrement(&status_sequence);
    (void)InterlockedExchange(&status_writer_lock, 0);
}

static void publish_status(
    SudekiMpTalosStagingResearchAdapterStatus *status
) {
    LONG active;

    begin_status_write();
    active = InterlockedCompareExchange(&adapter_active, 0, 0);
    status->active = active != 0 ? 1u : 0u;
    if (active == 0) {
        status->installed = 0u;
        status->enabled = 0u;
    }
    status->status_serial = advance_nonzero(public_status.status_serial);
    public_status = *status;
    end_status_write();
}

static BOOL copy_status(
    SudekiMpTalosStagingResearchAdapterStatus *status
) {
    unsigned int attempt;

    if (status == NULL) return FALSE;
    for (attempt = 0u; attempt < 32u; ++attempt) {
        LONG before = InterlockedCompareExchange(&status_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) continue;
        MemoryBarrier();
        *status = public_status;
        MemoryBarrier();
        after = InterlockedCompareExchange(&status_sequence, 0, 0);
        if (before == after && (after & 1) == 0) return TRUE;
    }
    ZeroMemory(status, sizeof(*status));
    return FALSE;
}

static BOOL readable_image(const uint8_t *base, size_t size) {
    uintptr_t cursor;
    uintptr_t end;

    if (base == NULL || size == 0u) return FALSE;
    cursor = (uintptr_t)base;
    end = cursor + size;
    if (end < cursor) return FALSE;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information;
        uintptr_t region_end;

        if (VirtualQuery((const void *)cursor, &information,
                sizeof(information)) == 0u ||
            information.State != MEM_COMMIT ||
            (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
            return FALSE;
        }
        region_end = (uintptr_t)information.BaseAddress +
            information.RegionSize;
        if (region_end <= cursor) return FALSE;
        cursor = region_end < end ? region_end : end;
    }
    return TRUE;
}

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
static void initialize_identity_key(void) {
    LARGE_INTEGER counter;
    uint64_t seed;

    if (identity_token_key != 0u) return;
    QueryPerformanceCounter(&counter);
    seed = (uint64_t)counter.QuadPart;
    seed ^= (uint64_t)GetTickCount() << 32u;
    seed ^= (uint64_t)GetCurrentProcessId() << 17u;
    seed ^= (uint64_t)GetCurrentThreadId();
    seed ^= (uint64_t)(uintptr_t)game_base;
    seed ^= (uint64_t)(uintptr_t)&public_status;
    identity_token_key = mix_identity(seed);
}
#endif

static void production_publish(
    void *context,
    const SudekiMpTalosStagingResearchAdapterStatus *status
) {
    (void)context;
    if (status == NULL) return;
    SudekiMpLogFormat(
        "talos_staging_research event=observation valid=%s "
        "sample_failure=%lu failed_address=0x%08lx failed_size=%lu "
        "failed_write=%lu checks=%lu ranges=%lu/%lu bytes=%lu/%lu "
        "state=%lu result=%lu failure=%lu stage=%lu samples=%lu "
        "remove_calls=%lu add_calls=%lu "
        "modal_active=%lu transition_active=%lu "
        "reload_required=%s quarantine=%s success=%s "
        "policy=research_only_production_membership_mutation_not_compiled\r\n",
        status->observation_valid ? "true" : "false",
        (unsigned long)status->observation_failure,
        (unsigned long)status->observation_failed_address,
        (unsigned long)status->observation_failed_size,
        (unsigned long)status->observation_failed_write_required,
        (unsigned long)status->observation_checks_completed,
        (unsigned long)status->observation_first_range_count,
        (unsigned long)status->observation_second_range_count,
        (unsigned long)status->observation_first_capture_bytes,
        (unsigned long)status->observation_second_capture_bytes,
        (unsigned long)status->coordinator_state,
        (unsigned long)status->coordinator_result,
        (unsigned long)status->failure,
        (unsigned long)status->last_stage,
        (unsigned long)status->sample_calls,
        (unsigned long)status->remove_calls,
        (unsigned long)status->add_calls,
        (unsigned long)status->modal_active,
        (unsigned long)status->transition_active,
        status->reload_required ? "true" : "false",
        status->quarantined ? "true" : "false",
        status->success ? "true" : "false"
    );
}

static void select_production_backend(AdapterBackend *backend) {
    ZeroMemory(backend, sizeof(*backend));
    backend->publish = production_publish;
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
    backend->mutation_authority_available = 0u;
#endif
}

static BOOL exact_boolean(uint8_t value) {
    return value == 0u || value == 1u;
}

static BOOL borrowed_witness_entry_shape_exact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    return witness != NULL && witness->dispatch_serial != 0u &&
        witness->native_thread_id == GetCurrentThreadId() &&
        witness->outer_update_depth == 1u &&
        witness->active_dispatch_count == 1u &&
        witness->original_call_count == 1u &&
        witness->observer_snapshot_count == 1u &&
        witness->observer_registry_generation != 0u &&
        witness->hook_owned_exact == 1u &&
        witness->slot_owned_exact == 1u &&
        witness->service_only == 1u && witness->post_original == 1u &&
        witness->source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL &&
        witness->source_exact == 1u &&
        witness->service_post_original_exact == 1u &&
        witness->sole_observer == 1u &&
        witness->registry_generation_stable == 1u &&
        witness->reserved[0] == 0u && witness->reserved[1] == 0u &&
        witness->reserved[2] == 0u;
}

static BOOL native_observation_booleans_exact(
    const SudekiMpTalosStagingNativeSamplerResult *result
) {
    const SudekiMpTalosStagingResearchSnapshot *snapshot;
    unsigned int index;

    if (result == NULL || !exact_boolean(result->valid) ||
        !exact_boolean(result->observation_only) ||
        !exact_boolean(result->native_engine_calls_permitted) ||
        !exact_boolean(result->hooks_permitted) ||
        !exact_boolean(result->actor_lifetime_authority) ||
        !exact_boolean(result->mutation_authority) ||
        !exact_boolean(result->external_sha256_required) ||
        !exact_boolean(result->membership_abi_required) ||
        !exact_boolean(result->failed_write_required) ||
        !exact_boolean(result->witness_entry_exact) ||
        !exact_boolean(result->witness_revalidated_exact) ||
        !exact_boolean(result->transaction_lease_exact) ||
        !exact_boolean(result->capture_no_yield_exact) ||
        result->reserved[0] != 0u || result->reserved[1] != 0u) {
        return FALSE;
    }
    snapshot = &result->snapshot;
    if (!exact_boolean(snapshot->exact_executable_hash) ||
        !exact_boolean(snapshot->exact_sol_hash) ||
        !exact_boolean(snapshot->foreground) ||
        !exact_boolean(snapshot->all_pending_loaded) ||
        !exact_boolean(snapshot->camera_scene_consistent) ||
        !exact_boolean(snapshot->controller_callback_exact) ||
        !exact_boolean(snapshot->game_thread_exact) ||
        !exact_boolean(snapshot->transaction_exclusive) ||
        !exact_boolean(snapshot->no_yield_window_exact) ||
        !exact_boolean(snapshot->listener_callback_closure_exact) ||
        !exact_boolean(snapshot->ui_hud_closure_exact) ||
        !exact_boolean(snapshot->hero_hud_state_converged) ||
        !exact_boolean(snapshot->elco_arbiter_safe) ||
        !exact_boolean(snapshot->in_combat) ||
        !exact_boolean(snapshot->async_active) ||
        !exact_boolean(snapshot->tsa_active) ||
        !exact_boolean(snapshot->paused) ||
        !exact_boolean(snapshot->transition_active) ||
        !exact_boolean(snapshot->modal_active) ||
        !exact_boolean(snapshot->group_armed) ||
        !exact_boolean(snapshot->production_authority) ||
        !exact_boolean(snapshot->carry_authority) ||
        !exact_boolean(snapshot->actor_lifetime_authority) ||
        !exact_boolean(snapshot->reload_required)) {
        return FALSE;
    }
    for (index = 0u; index < HERO_COUNT; ++index) {
        if (!exact_boolean(snapshot->hero[index].native_ai_enabled) ||
            !exact_boolean(snapshot->hero[index].human_control_owned) ||
            !exact_boolean(snapshot->hero[index].override_active)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL native_observation_snapshot_shape_valid(
    const SudekiMpTalosStagingResearchSnapshot *snapshot
) {
    static const uint8_t expected_group[HERO_COUNT] = {0u, 1u, 2u, 3u};
    static const uint8_t expected_formation[HERO_COUNT] = {0u, 3u, 1u, 2u};
    unsigned int index;
    unsigned int other;
    unsigned int field;

    if (snapshot == NULL || snapshot->observation_serial == 0u ||
        snapshot->process_token == 0u || snapshot->native_thread_token == 0u ||
        snapshot->source_token == 0u || snapshot->world_token == 0u ||
        snapshot->group_token == 0u ||
        snapshot->formation_owner_token == 0u ||
        snapshot->formation_token == 0u ||
        snapshot->controller_token == 0u ||
        snapshot->controller_callback_token == 0u ||
        snapshot->transaction_token == 0u ||
        snapshot->listener_storage_token == 0u ||
        snapshot->listener_token == 0u ||
        snapshot->ui_controller_token == 0u ||
        snapshot->hud_owner_token == 0u || snapshot->ui_scene_token == 0u ||
        snapshot->elco_arbiter_token == 0u ||
        snapshot->front_actor_token == 0u || snapshot->camera_token == 0u ||
        snapshot->current_render_camera_token == 0u ||
        snapshot->render_state_token == 0u ||
        snapshot->scene_manager_token == 0u ||
        snapshot->scene_renderer_token == 0u ||
        snapshot->controller_callback_token == snapshot->controller_token ||
        snapshot->transaction_token == snapshot->controller_token ||
        snapshot->transaction_token ==
            snapshot->controller_callback_token ||
        snapshot->group_count != HERO_COUNT ||
        snapshot->formation_count != HERO_COUNT ||
        memcmp(snapshot->group_order, expected_group,
            sizeof(expected_group)) != 0 ||
        memcmp(snapshot->formation_order, expected_formation,
            sizeof(expected_formation)) != 0 ||
        snapshot->listener_count != 1u ||
        snapshot->controller_current_mode != 1u ||
        snapshot->controller_requested_mode != 1u ||
        snapshot->front_actor_token !=
            snapshot->hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL].actor_token) {
        return FALSE;
    }
    for (index = 0u; index < HERO_COUNT; ++index) {
        const SudekiMpTalosStagingResearchHeroEvidence *hero =
            &snapshot->hero[index];
        const uint64_t key_tokens[4] = {
            hero->actor_token,
            hero->control_component_token,
            hero->gizmo_token,
            hero->stat_display_token
        };
        int tal = index == SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;

        if (hero->wrapper_token != 0u || hero->actor_token == 0u ||
            hero->control_component_token == 0u ||
            hero->control_owner_actor_token != hero->actor_token ||
            hero->formation_backpointer_token != snapshot->formation_token ||
            hero->gizmo_token == 0u || hero->stat_display_token == 0u ||
            hero->gizmo_label_hash == 0u || hero->gizmo_label_length == 0u ||
            hero->gizmo_label_length >
                SUDEKIMP_TALOS_STAGING_RESEARCH_HUD_LABEL_MAX_LENGTH ||
            hero->fill_cache_primary_bits !=
                hero->fill_cache_secondary_bits ||
            hero->native_ai_enabled != (uint8_t)(tal ? 0u : 1u) ||
            hero->human_control_owned != (uint8_t)(tal ? 1u : 0u) ||
            hero->override_active != 0u ||
            hero->control_mode != (uint8_t)(tal ?
                SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
                SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI)) {
            return FALSE;
        }
        for (field = 0u; field < 4u; ++field) {
            unsigned int later_field;

            for (later_field = field + 1u; later_field < 4u;
                    ++later_field) {
                if (key_tokens[field] == key_tokens[later_field]) return FALSE;
            }
            for (other = index + 1u; other < HERO_COUNT; ++other) {
                const SudekiMpTalosStagingResearchHeroEvidence *later =
                    &snapshot->hero[other];
                const uint64_t later_tokens[4] = {
                    later->actor_token,
                    later->control_component_token,
                    later->gizmo_token,
                    later->stat_display_token
                };
                unsigned int later_field;

                for (later_field = 0u; later_field < 4u; ++later_field) {
                    if (key_tokens[field] == later_tokens[later_field]) {
                        return FALSE;
                    }
                }
            }
        }
    }
    return TRUE;
}

/* This validates only the sampler's observation envelope. It intentionally
 * does not reuse exact_preflight_shape() or any mutation coordinator gate:
 * a wrapperless observation is evidence, never transaction admission. */
static BOOL native_observation_contract_valid(
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    const SudekiMpTalosStagingResearchSnapshot *snapshot;
    unsigned int index;

    if (!native_observation_booleans_exact(result) ||
        !native_observation_snapshot_shape_valid(&result->snapshot) ||
        !borrowed_witness_entry_shape_exact(witness)) {
        return FALSE;
    }
    snapshot = &result->snapshot;
    if (!(result->valid == 1u &&
        result->failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK &&
        result->failed_address == 0u && result->failed_size == 0u &&
        result->checks_completed != 0u &&
        result->first_range_count != 0u &&
        result->first_range_count <= SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_RANGES &&
        result->second_range_count == result->first_range_count &&
        result->first_capture_bytes != 0u &&
        result->first_capture_bytes <=
            SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_CAPTURE_BYTES &&
        result->second_capture_bytes == result->first_capture_bytes &&
        result->observation_only == 1u &&
        result->native_engine_calls_permitted == 0u &&
        result->hooks_permitted == 0u &&
        result->actor_lifetime_authority == 0u &&
        result->mutation_authority == 0u &&
        result->external_sha256_required == 1u &&
        result->membership_abi_required == 1u &&
        result->failed_write_required == 0u &&
        result->witness_entry_exact == 1u &&
        result->witness_revalidated_exact == 1u &&
        result->transaction_lease_exact == 1u &&
        result->capture_no_yield_exact == 1u &&
        result->witness_dispatch_serial == witness->dispatch_serial &&
        result->witness_native_thread_id == witness->native_thread_id &&
        result->witness_observer_registry_generation ==
            witness->observer_registry_generation &&
        result->witness_dispatch_overlap_generation ==
            witness->dispatch_overlap_generation &&
        result->witness_source == witness->source &&
        snapshot->observation_serial != 0u &&
        snapshot->exact_executable_hash == 1u &&
        snapshot->exact_sol_hash == 1u && snapshot->foreground == 1u &&
        snapshot->all_pending_loaded == 1u &&
        snapshot->camera_scene_consistent == 1u &&
        snapshot->controller_callback_exact == 1u &&
        snapshot->game_thread_exact == 1u &&
        snapshot->transaction_exclusive == 1u &&
        snapshot->no_yield_window_exact == 1u &&
        snapshot->listener_callback_closure_exact == 1u &&
        snapshot->ui_hud_closure_exact == 1u &&
        snapshot->hero_hud_state_converged == 1u &&
        snapshot->elco_arbiter_safe == 1u &&
        snapshot->in_combat == 0u && snapshot->group_armed == 0u &&
        snapshot->async_active == 0u && snapshot->tsa_active == 0u &&
        snapshot->paused == 0u &&
        snapshot->production_authority == 0u &&
        snapshot->carry_authority == 0u &&
        snapshot->actor_lifetime_authority == 0u &&
        snapshot->reload_required == 0u)) {
        return FALSE;
    }
    for (index = 0u; index < HERO_COUNT; ++index) {
        if (snapshot->hero[index].wrapper_token != 0u) return FALSE;
    }
    return TRUE;
}

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
static BOOL raw_exact_closure_flags_valid(const AdapterRawSample *raw) {
    return raw != NULL && raw->controller_callback_exact == 1u &&
        raw->game_thread_exact == 1u &&
        raw->transaction_exclusive == 1u &&
        raw->no_yield_window_exact == 1u &&
        raw->listener_callback_closure_exact == 1u &&
        raw->ui_hud_closure_exact == 1u &&
        raw->hero_hud_state_converged == 1u &&
        raw->elco_arbiter_safe == 1u;
}

static BOOL raw_sample_to_snapshot(
    const AdapterRawSample *raw,
    const void *wrapper,
    int reload_required,
    uint64_t process_token,
    uint64_t native_thread_token,
    SudekiMpTalosStagingResearchSnapshot *snapshot
) {
    unsigned int index;

    if (raw == NULL || snapshot == NULL) return FALSE;
    *snapshot = raw->snapshot;
    snapshot->process_token = process_token;
    snapshot->native_thread_token = native_thread_token;
    snapshot->source_token = pointer_token(raw->source_descriptor);
    snapshot->world_token = pointer_token(raw->world);
    snapshot->group_token = pointer_token(raw->group);
    snapshot->formation_owner_token = pointer_token(raw->formation_owner);
    snapshot->formation_token = pointer_token(raw->formation);
    snapshot->controller_token = pointer_token(raw->controller);
    snapshot->controller_callback_token =
        pointer_token(raw->controller_callback);
    snapshot->transaction_token = pointer_token(raw->transaction);
    snapshot->listener_storage_token =
        pointer_token(raw->listener_storage);
    snapshot->listener_token = pointer_token(raw->listener);
    snapshot->ui_controller_token = pointer_token(raw->ui_controller);
    snapshot->hud_owner_token = pointer_token(raw->hud_owner);
    snapshot->ui_scene_token = pointer_token(raw->ui_scene);
    snapshot->elco_arbiter_token = pointer_token(raw->elco_arbiter);
    snapshot->front_actor_token = pointer_token(raw->front_actor);
    snapshot->camera_token = pointer_token(raw->camera_manager);
    snapshot->current_render_camera_token =
        pointer_token(raw->current_render_camera);
    snapshot->render_state_token = pointer_token(raw->render_state);
    snapshot->scene_manager_token = pointer_token(raw->scene_manager);
    snapshot->scene_renderer_token = pointer_token(raw->scene_renderer);
    snapshot->exact_executable_hash =
        installed_exact_executable_hash ? 1u : 0u;
    snapshot->exact_sol_hash = installed_exact_sol_hash ? 1u : 0u;
    snapshot->controller_callback_exact =
        raw->controller_callback_exact;
    snapshot->game_thread_exact = raw->game_thread_exact;
    snapshot->transaction_exclusive = raw->transaction_exclusive;
    snapshot->no_yield_window_exact = raw->no_yield_window_exact;
    snapshot->listener_callback_closure_exact =
        raw->listener_callback_closure_exact;
    snapshot->ui_hud_closure_exact = raw->ui_hud_closure_exact;
    snapshot->hero_hud_state_converged =
        raw->hero_hud_state_converged;
    snapshot->elco_arbiter_safe = raw->elco_arbiter_safe;
    snapshot->in_combat = raw->group_combat_d4;
    snapshot->group_armed = raw->group_combat_d4;
    snapshot->production_authority = 0u;
    snapshot->carry_authority = 0u;
    snapshot->actor_lifetime_authority = 0u;
    snapshot->reload_required = reload_required ? 1u : 0u;
    for (index = 0u; index < HERO_COUNT; ++index) {
        snapshot->hero[index].wrapper_token =
            index == HERO_ELCO ? pointer_token(wrapper) : 0u;
        snapshot->hero[index].actor_token = pointer_token(raw->actor[index]);
        snapshot->hero[index].control_component_token =
            pointer_token(raw->control_component[index]);
        snapshot->hero[index].control_owner_actor_token =
            pointer_token(raw->control_owner_actor[index]);
        snapshot->hero[index].formation_backpointer_token =
            pointer_token(raw->formation_backpointer[index]);
        snapshot->hero[index].gizmo_token =
            pointer_token(raw->gizmo[index]);
        snapshot->hero[index].stat_display_token =
            pointer_token(raw->stat_display[index]);
    }
    return snapshot->observation_serial != 0u;
}

static BOOL snapshot_arbiter_shape_valid(
    const SudekiMpTalosStagingResearchSnapshot *snapshot
) {
    uint32_t state;
    uint32_t armed;

    if (snapshot == NULL || snapshot->group_armed > 1u ||
        (snapshot->elco_arbiter_flags_60_masked &
            ~((uint32_t)SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK))
            != 0u) return FALSE;
    state = snapshot->elco_arbiter_state_58 & 0x0fu;
    armed = snapshot->elco_arbiter_flags_60_masked &
        SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
    if ((armed != 0u) != (snapshot->group_armed != 0u)) return FALSE;
    return !((state == 2u && snapshot->group_armed == 0u) ||
        (state == 4u && snapshot->group_armed != 0u));
}

static BOOL snapshot_hero_hud_shape_valid(
    const SudekiMpTalosStagingResearchSnapshot *snapshot,
    unsigned int index
) {
    const SudekiMpTalosStagingResearchHeroEvidence *hero;

    if (snapshot == NULL || index >= HERO_COUNT) return FALSE;
    hero = &snapshot->hero[index];
    return hero->gizmo_token != 0u && hero->stat_display_token != 0u &&
        hero->gizmo_label_hash != 0u && hero->gizmo_label_length != 0u &&
        hero->gizmo_label_length <=
            SUDEKIMP_TALOS_STAGING_RESEARCH_HUD_LABEL_MAX_LENGTH &&
        hero->fill_cache_primary_bits == hero->fill_cache_secondary_bits;
}

static BOOL exact_preflight_shape(
    const AdapterRawSample *raw,
    const SudekiMpTalosStagingResearchSnapshot *snapshot,
    void *callback_controller,
    void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const void *wrapper
) {
    static const uint8_t expected_group[HERO_COUNT] = {0u, 1u, 2u, 3u};
    static const uint8_t expected_formation[HERO_COUNT] = {0u, 3u, 1u, 2u};
    unsigned int i;
    unsigned int j;

    if (raw == NULL || snapshot == NULL ||
        raw->topology_ranges_prevalidated != 1u ||
        !raw_exact_closure_flags_valid(raw) ||
        raw->group_combat_d4 > 1u ||
        callback_controller == NULL || callback_update_data == NULL ||
        !borrowed_witness_entry_shape_exact(witness) ||
        raw->controller != callback_controller ||
        raw->controller_callback != (const void *)witness ||
        raw->controller_callback == raw->controller ||
        raw->transaction == NULL || raw->transaction == raw->controller ||
        raw->transaction == raw->controller_callback ||
        raw->transaction == callback_update_data ||
        raw->listener_storage == NULL || raw->listener == NULL ||
        raw->ui_controller == NULL || raw->hud_owner == NULL ||
        raw->ui_scene == NULL || raw->elco_arbiter == NULL ||
        snapshot->observation_serial == 0u ||
        snapshot->process_token == 0u || snapshot->native_thread_token == 0u ||
        snapshot->source_token == 0u || snapshot->world_token == 0u ||
        snapshot->group_token == 0u ||
        snapshot->formation_owner_token == 0u ||
        snapshot->formation_token == 0u ||
        snapshot->controller_token == 0u ||
        snapshot->controller_callback_token == 0u ||
        snapshot->transaction_token == 0u ||
        snapshot->controller_callback_token == snapshot->controller_token ||
        snapshot->transaction_token == snapshot->controller_token ||
        snapshot->transaction_token ==
            snapshot->controller_callback_token ||
        snapshot->listener_storage_token == 0u ||
        snapshot->listener_token == 0u ||
        snapshot->ui_controller_token == 0u ||
        snapshot->hud_owner_token == 0u ||
        snapshot->ui_scene_token == 0u ||
        snapshot->elco_arbiter_token == 0u ||
        snapshot->front_actor_token == 0u || snapshot->camera_token == 0u ||
        snapshot->current_render_camera_token == 0u ||
        snapshot->render_state_token == 0u ||
        snapshot->scene_manager_token == 0u ||
        snapshot->scene_renderer_token == 0u ||
        snapshot->group_count != HERO_COUNT ||
        snapshot->formation_count != HERO_COUNT ||
        memcmp(snapshot->group_order, expected_group,
            sizeof(expected_group)) != 0 ||
        memcmp(snapshot->formation_order, expected_formation,
            sizeof(expected_formation)) != 0 ||
        snapshot->front_actor_token != snapshot->hero[0].actor_token ||
        snapshot->exact_executable_hash == 0u ||
        snapshot->exact_sol_hash == 0u || snapshot->foreground == 0u ||
        snapshot->all_pending_loaded == 0u ||
        snapshot->camera_scene_consistent == 0u ||
        snapshot->in_combat != raw->group_combat_d4 ||
        snapshot->group_armed != raw->group_combat_d4 ||
        snapshot->in_combat != snapshot->group_armed ||
        snapshot->in_combat != 0u || snapshot->async_active != 0u ||
        snapshot->tsa_active != 0u || snapshot->paused != 0u ||
        snapshot->controller_callback_exact != 1u ||
        snapshot->game_thread_exact != 1u ||
        snapshot->transaction_exclusive != 1u ||
        snapshot->no_yield_window_exact != 1u ||
        snapshot->listener_callback_closure_exact != 1u ||
        snapshot->ui_hud_closure_exact != 1u ||
        snapshot->hero_hud_state_converged != 1u ||
        snapshot->elco_arbiter_safe != 1u ||
        snapshot->listener_count != 1u ||
        !snapshot_arbiter_shape_valid(snapshot) ||
        snapshot->controller_current_mode != 1u ||
        snapshot->controller_requested_mode != 1u) return FALSE;
    for (i = 0u; i < HERO_COUNT; ++i) {
        int tal = i == SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;

        if (raw->actor[i] == NULL || raw->control_component[i] == NULL ||
            raw->gizmo[i] == NULL || raw->stat_display[i] == NULL ||
            raw->control_owner_actor[i] != raw->actor[i] ||
            raw->formation_backpointer[i] != raw->formation ||
            snapshot->hero[i].actor_token == 0u ||
            snapshot->hero[i].control_component_token == 0u ||
            snapshot->hero[i].control_owner_actor_token !=
                snapshot->hero[i].actor_token ||
            snapshot->hero[i].formation_backpointer_token !=
                snapshot->formation_token ||
            !snapshot_hero_hud_shape_valid(snapshot, i) ||
            snapshot->hero[i].native_ai_enabled != (uint8_t)(tal ? 0u : 1u) ||
            snapshot->hero[i].human_control_owned != (uint8_t)(tal ? 1u : 0u) ||
            snapshot->hero[i].override_active != 0u ||
            snapshot->hero[i].control_mode != (uint8_t)(tal ?
                SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
                SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI) ||
            (i == HERO_ELCO && wrapper != NULL &&
             snapshot->hero[i].wrapper_token == 0u) ||
            (i != HERO_ELCO && snapshot->hero[i].wrapper_token != 0u)) {
            return FALSE;
        }
        for (j = i + 1u; j < HERO_COUNT; ++j) {
            if (raw->actor[i] == raw->actor[j] ||
                raw->control_component[i] == raw->control_component[j] ||
                raw->gizmo[i] == raw->gizmo[j] ||
                raw->stat_display[i] == raw->stat_display[j]) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static void fill_status_from_coordinator(
    SudekiMpTalosStagingResearchAdapterStatus *status,
    const SudekiMpTalosCompanionStagingResearch *research
) {
    if (status == NULL || research == NULL) return;
    status->coordinator_state = (uint32_t)research->state;
    status->original = research->original;
    status->detached = research->detached;
    status->restored = research->restored;
    status->stable = research->stable;
    if (research->reload_required != 0u) status->reload_required = 1u;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED) {
        status->quarantined = 1u;
    }
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS) {
        status->success = 1u;
    }
}

static void finish_attempt(
    SudekiMpTalosStagingResearchAdapterStatus *status,
    const SudekiMpTalosCompanionStagingResearch *research
) {
    if (status == NULL) return;
    status->attempt_consumed = 1u;
    fill_status_from_coordinator(status, research);
    if (status->remove_reported != 0u) status->reload_required = 1u;
    if (status->failure != SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
        status->success = 0u;
        if (status->remove_reported != 0u) status->quarantined = 1u;
    }
    if (active_backend.publish != NULL) {
        active_backend.publish(active_backend.context, status);
    }
    publish_status(status);
}

static void consume_exact_closure_failure(
    SudekiMpTalosStagingResearchAdapterStatus *status,
    const SudekiMpTalosStagingResearchSnapshot *snapshot
) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosCompanionStagingResearchResult result;

    SudekiMpTalosCompanionStagingResearchInitialize(&research);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 1);
    result = SudekiMpTalosCompanionStagingResearchBegin(
        &research, status->attempt_serial, snapshot);
    status->coordinator_result = (uint32_t)result;
    status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE;
    finish_attempt(status, &research);
}

static BOOL testing_backend_complete(const AdapterBackend *backend) {
    return backend != NULL && backend->hotkey_down != NULL &&
        backend->sample != NULL && backend->prevalidate_mutation != NULL &&
        backend->get_pc != NULL && backend->inspect_wrapper != NULL &&
        backend->resolve_wrapper != NULL && backend->cleanup_tptr != NULL &&
        backend->is_player != NULL && backend->get_index != NULL &&
        backend->remove_player != NULL && backend->add_player != NULL &&
        backend->destroy_wrapper != NULL && backend->publish != NULL &&
        backend->mutation_authority_available != 0u;
}

static BOOL resolve_wrapper_once(
    SudekiMpTalosStagingResearchAdapterStatus *status,
    void *wrapper,
    void **actor
) {
    uint8_t tracked_tptr[SUDEKIMP_TALOS_MEMBERSHIP_EMBEDDED_TPTR_SIZE];
    BOOL result;

    ZeroMemory(tracked_tptr, sizeof(tracked_tptr));
    *actor = NULL;
    ++status->resolve_calls;
    result = active_backend.resolve_wrapper(
        active_backend.context, wrapper, tracked_tptr, actor);
    active_backend.cleanup_tptr(active_backend.context, tracked_tptr);
    ++status->tptr_cleanup_calls;
    return result && *actor != NULL;
}

static BOOL sample_for_transaction(
    SudekiMpTalosStagingResearchAdapterStatus *status,
    SudekiMpTalosStagingResearchAdapterStage stage,
    void *callback_controller,
    void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    void *wrapper,
    int reload_required,
    uint64_t process_token,
    uint64_t native_thread_token,
    AdapterRawSample *raw,
    SudekiMpTalosStagingResearchSnapshot *snapshot
) {
    ZeroMemory(raw, sizeof(*raw));
    ++status->sample_calls;
    status->last_stage = (uint32_t)stage;
    if (!active_backend.sample(active_backend.context, stage,
            callback_controller, callback_update_data, witness, raw)) {
        return FALSE;
    }
    if (!raw_exact_closure_flags_valid(raw)) {
        status->failure =
            SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE;
        return FALSE;
    }
    if (raw->topology_ranges_prevalidated != 1u ||
        raw->mutation_ranges_prevalidated != 1u ||
        !raw_sample_to_snapshot(raw, wrapper, reload_required,
            process_token, native_thread_token, snapshot)) {
        return FALSE;
    }
    return TRUE;
}

static void quarantine_after_remove(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchTicket *remove_ticket,
    const SudekiMpTalosStagingResearchTicket *add_ticket,
    int add_was_called
) {
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_ISSUED) {
        (void)SudekiMpTalosCompanionStagingResearchFinishRemove(
            research, remove_ticket, NULL, 1u);
    } else if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_DETACHED) {
        (void)SudekiMpTalosCompanionStagingResearchClaimAdd(
            research, NULL, NULL);
    } else if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_ISSUED) {
        (void)SudekiMpTalosCompanionStagingResearchFinishAdd(
            research, add_ticket, NULL, add_was_called ? 1u : 0u);
    } else if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORED) {
        (void)SudekiMpTalosCompanionStagingResearchObserveStability(
            research, NULL);
    }
}

static void run_test_transaction(
    SudekiMpTalosStagingResearchAdapterStatus *status,
    void *callback_controller,
    void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    AdapterRawSample *raw,
    SudekiMpTalosStagingResearchSnapshot *preflight
) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosCompanionStagingResearchResult result;
    SudekiMpTalosStagingResearchTicket remove_ticket;
    SudekiMpTalosStagingResearchTicket add_ticket;
    SudekiMpTalosStagingResearchSnapshot snapshot;
    AdapterRawSample current_raw;
    void *group = raw->group;
    void *wrapper = NULL;
    void *embedded_actor = NULL;
    void *resolved_actor = NULL;
    int wrapper_needs_destroy = 0;
    int remove_was_called = 0;
    int add_was_called = 0;
    int coordinator_started = 0;

    ZeroMemory(&research, sizeof(research));
    ZeroMemory(&remove_ticket, sizeof(remove_ticket));
    ZeroMemory(&add_ticket, sizeof(add_ticket));
    SudekiMpTalosCompanionStagingResearchInitialize(&research);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 1);

    ++status->get_pc_calls;
    wrapper = active_backend.get_pc(active_backend.context, "PC_ELCO");
    if (wrapper == NULL) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_GET_PC;
        goto cleanup;
    }
    wrapper_needs_destroy = 1;
    if (!active_backend.inspect_wrapper(
            active_backend.context, wrapper, &embedded_actor) ||
        embedded_actor == NULL) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_WRAPPER;
        goto cleanup;
    }
    if (!resolve_wrapper_once(status, wrapper, &resolved_actor) ||
        resolved_actor != embedded_actor || resolved_actor != raw->actor[HERO_ELCO]) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESOLVE;
        goto cleanup;
    }
    ++status->is_player_calls;
    if (!active_backend.is_player(active_backend.context, group, wrapper)) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_MEMBERSHIP;
        goto cleanup;
    }
    ++status->get_index_calls;
    if (active_backend.get_index(active_backend.context, group, wrapper) != 3) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_MEMBERSHIP;
        goto cleanup;
    }
    if (!raw_sample_to_snapshot(raw, wrapper, 0,
            preflight->process_token, preflight->native_thread_token,
            preflight) ||
        !exact_preflight_shape(raw, preflight, callback_controller,
            callback_update_data, witness, wrapper)) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY;
        goto cleanup;
    }
    result = SudekiMpTalosCompanionStagingResearchBegin(
        &research, status->attempt_serial, preflight);
    status->coordinator_result = (uint32_t)result;
    if (result != SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_COORDINATOR;
        goto cleanup;
    }
    coordinator_started = 1;

    if (!sample_for_transaction(status,
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_REMOVE_AUTHORIZATION,
            callback_controller, callback_update_data, witness, wrapper, 0,
            preflight->process_token, preflight->native_thread_token,
            &current_raw, &snapshot) || current_raw.group != group ||
        current_raw.actor[HERO_ELCO] != resolved_actor ||
        !exact_preflight_shape(&current_raw, &snapshot, callback_controller,
            callback_update_data, witness, wrapper)) {
        if (status->failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
            status->failure =
                SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY;
        }
        goto cleanup;
    }
    result = SudekiMpTalosCompanionStagingResearchClaimRemove(
        &research, &snapshot, &remove_ticket);
    status->coordinator_result = (uint32_t)result;
    if (result != SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_COORDINATOR;
        goto cleanup;
    }
    if (current_raw.mutation_ranges_prevalidated == 0u ||
        !active_backend.prevalidate_mutation(active_backend.context,
            group, wrapper, resolved_actor)) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RANGE_PREFLIGHT;
        (void)SudekiMpTalosCompanionStagingResearchFinishRemove(
            &research, &remove_ticket, NULL, 0u);
        goto cleanup;
    }

    active_backend.remove_player(active_backend.context, group, wrapper);
    ++status->remove_calls;
    status->remove_reported = 1u;
    status->reload_required = 1u;
    remove_was_called = 1;

    /* From this point through Add/stability/wrapper destruction the adapter
     * performs no log, yield, hotkey, foreground, VirtualQuery, or other OS
     * call. Test backends model direct, prevalidated synchronous reads. */
    if (!sample_for_transaction(status,
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE,
            callback_controller, callback_update_data, witness, wrapper, 1,
            preflight->process_token, preflight->native_thread_token,
            &current_raw, &snapshot) || current_raw.group != group ||
        current_raw.actor[HERO_ELCO] != resolved_actor ||
        !resolve_wrapper_once(status, wrapper, &embedded_actor) ||
        embedded_actor != resolved_actor) {
        if (status->failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
            status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH;
        }
        goto cleanup;
    }
    result = SudekiMpTalosCompanionStagingResearchFinishRemove(
        &research, &remove_ticket, &snapshot, 1u);
    status->coordinator_result = (uint32_t)result;
    if (result != SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH;
        goto cleanup;
    }
    if (!sample_for_transaction(status,
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_INDEPENDENT,
            callback_controller, callback_update_data, witness, wrapper, 1,
            preflight->process_token, preflight->native_thread_token,
            &current_raw, &snapshot) || current_raw.group != group ||
        current_raw.actor[HERO_ELCO] != resolved_actor) {
        if (status->failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
            status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH;
        }
        goto cleanup;
    }
    result = SudekiMpTalosCompanionStagingResearchClaimAdd(
        &research, &snapshot, &add_ticket);
    status->coordinator_result = (uint32_t)result;
    if (result != SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_AUTHORIZED) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH;
        goto cleanup;
    }

    active_backend.add_player(active_backend.context, group, wrapper);
    ++status->add_calls;
    add_was_called = 1;
    if (!sample_for_transaction(status,
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_RESTORE_IMMEDIATE,
            callback_controller, callback_update_data, witness, wrapper, 1,
            preflight->process_token, preflight->native_thread_token,
            &current_raw, &snapshot) || current_raw.group != group ||
        current_raw.actor[HERO_ELCO] != resolved_actor ||
        !resolve_wrapper_once(status, wrapper, &embedded_actor) ||
        embedded_actor != resolved_actor) {
        if (status->failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
            status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESTORE;
        }
        goto cleanup;
    }
    result = SudekiMpTalosCompanionStagingResearchFinishAdd(
        &research, &add_ticket, &snapshot, 1u);
    status->coordinator_result = (uint32_t)result;
    if (result != SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESTORE;
        goto cleanup;
    }
    if (!sample_for_transaction(status,
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_STABILITY,
            callback_controller, callback_update_data, witness, wrapper, 1,
            preflight->process_token, preflight->native_thread_token,
            &current_raw, &snapshot) || current_raw.group != group ||
        current_raw.actor[HERO_ELCO] != resolved_actor) {
        if (status->failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
            status->failure =
                SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_STABILITY;
        }
        goto cleanup;
    }
    result = SudekiMpTalosCompanionStagingResearchObserveStability(
        &research, &snapshot);
    status->coordinator_result = (uint32_t)result;
    if (result != SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS_RESULT) {
        status->failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_STABILITY;
        goto cleanup;
    }

cleanup:
    if (remove_was_called && research.state !=
            SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS) {
        quarantine_after_remove(
            &research, &remove_ticket, &add_ticket, add_was_called);
    } else if (!remove_was_called && coordinator_started &&
            research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT) {
        SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
    }
    if (wrapper_needs_destroy) {
        ++status->wrapper_destroy_calls;
        if (!active_backend.destroy_wrapper(
                active_backend.context, wrapper, 1u)) {
            status->failure =
                SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_WRAPPER_DESTROY;
            if (remove_was_called) {
                status->quarantined = 1u;
                status->success = 0u;
            }
        }
        wrapper = NULL;
    }
    fill_status_from_coordinator(status, &research);
    if (status->failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE &&
        research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS) {
        status->success = 1u;
    } else if (status->failure !=
            SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE) {
        status->success = 0u;
        if (remove_was_called) status->quarantined = 1u;
    }
    if (remove_was_called && research.state !=
            SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS) {
        status->quarantined = 1u;
        status->success = 0u;
    }
    finish_attempt(status, &research);
}
#endif

BOOL SudekiMpInstallTalosCompanionStagingResearchAdapter(
    HMODULE game_module,
    BOOL enabled,
    UINT virtual_key,
    BOOL exact_executable_hash,
    BOOL exact_sol_hash
) {
    SudekiMpTalosMembershipAbiDescriptor descriptor;
    SudekiMpTalosMembershipValidationResult validation;
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *base;

    if (!enabled) return TRUE;
    if (game_module == NULL || virtual_key > 0xffu ||
        !exact_executable_hash || !exact_sol_hash ||
        InterlockedCompareExchange(&adapter_active, 0, 0) != 0 ||
        InterlockedCompareExchange(&attempt_claimed, 0, 0) != 0) {
        SetLastError(game_module == NULL || virtual_key > 0xffu ||
            !exact_executable_hash || !exact_sol_hash ?
            ERROR_INVALID_PARAMETER : ERROR_BUSY);
        return FALSE;
    }
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
    if (injected_backend_set && virtual_key == 0u) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
#endif
    base = (uint8_t *)game_module;
    descriptor = SudekiMpTalosCompanionMembershipAbiDescribe();
    if (!readable_image(base, descriptor.mapped_image_size)) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    validation = SudekiMpTalosCompanionMembershipAbiValidateMappedImage(
        base, descriptor.mapped_image_size, (uint32_t)(uintptr_t)base);
    if (validation.seams_valid == 0u ||
        validation.validated_symbol_mask != validation.required_symbol_mask ||
        validation.external_sha256_required == 0u) {
        ZeroMemory(&status, sizeof(status));
        status.failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_IMAGE_ABI;
        status.abi_failure = validation.failure;
        status.abi_failed_symbol = validation.failed_symbol;
        status.abi_checks_completed = validation.checks_completed;
        status.observation_only = 1u;
        publish_status(&status);
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
    if (injected_backend_set) {
        if (!testing_backend_complete(&injected_backend)) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        active_backend = injected_backend;
    } else {
        select_production_backend(&active_backend);
    }
#else
    select_production_backend(&active_backend);
#endif
    game_base = base;
    proof_virtual_key = virtual_key;
    installed_exact_executable_hash = TRUE;
    installed_exact_sol_hash = TRUE;
    hotkey_was_down = FALSE;
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
    initialize_identity_key();
#endif
    if (pinned_module == NULL && !GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)
                &SudekiMpTalosCompanionStagingResearchAdapterService,
            &pinned_module)) {
        game_base = NULL;
        proof_virtual_key = 0u;
        SetLastError(ERROR_MOD_NOT_FOUND);
        return FALSE;
    }
    ZeroMemory(&status, sizeof(status));
    status.installed = 1u;
    status.enabled = 1u;
    status.active = 1u;
    status.exact_executable_hash = 1u;
    status.exact_sol_hash = 1u;
    status.abi_seams_valid = 1u;
    status.external_sha256_required = 1u;
    status.production_mutation_compiled = 0u;
    status.observation_only = 1u;
    status.abi_failure = validation.failure;
    status.abi_failed_symbol = validation.failed_symbol;
    status.abi_checks_completed = validation.checks_completed;
    MemoryBarrier();
    (void)InterlockedExchange(&adapter_active, 1);
    publish_status(&status);
    return TRUE;
}

void SudekiMpTalosCompanionStagingResearchAdapterService(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD entry_error = GetLastError();
    SudekiMpTalosStagingResearchAdapterStatus status;
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
    AdapterRawSample raw;
    SudekiMpTalosStagingResearchSnapshot snapshot;
    BOOL down;
    BOOL rising;
    uint64_t process_token;
    uint64_t native_thread_token;
#else
    (void)controller;
    (void)update_data;
    (void)witness;
#endif

    /* Unregister is not a quiescence barrier. This atomic check is the first
     * adapter-state access, and Uninstall retains every backing object. */
    if (InterlockedCompareExchange(&adapter_active, 0, 0) == 0) {
        SetLastError(entry_error);
        return;
    }
    if (!copy_status(&status)) {
        SetLastError(entry_error);
        return;
    }
    ++status.service_calls;
#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
    if (!injected_backend_set || proof_virtual_key == 0u) {
        publish_status(&status);
        SetLastError(entry_error);
        return;
    }
    down = active_backend.hotkey_down(
        active_backend.context, proof_virtual_key);
    rising = down && !hotkey_was_down;
    hotkey_was_down = down;
    if (!rising || InterlockedCompareExchange(
            &attempt_claimed, 1, 0) != 0) {
        publish_status(&status);
        SetLastError(entry_error);
        return;
    }
    next_attempt_serial = advance_nonzero(next_attempt_serial);
    status.attempt_serial = next_attempt_serial;
    status.attempt_consumed = 1u;
    status.last_stage = SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT;
    if (!borrowed_witness_entry_shape_exact(witness)) {
        SudekiMpTalosCompanionStagingResearch research;

        status.failure =
            SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE;
        SudekiMpTalosCompanionStagingResearchInitialize(&research);
        finish_attempt(&status, &research);
        SetLastError(entry_error);
        return;
    }
    process_token = integer_token(GetCurrentProcessId());
    native_thread_token = integer_token(GetCurrentThreadId());
    ZeroMemory(&raw, sizeof(raw));
    ++status.sample_calls;
    if (!active_backend.sample(active_backend.context,
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT,
            controller, update_data, witness, &raw) ||
        !raw_sample_to_snapshot(&raw, NULL, 0,
            process_token, native_thread_token, &snapshot)) {
        SudekiMpTalosCompanionStagingResearch research;

        status.failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_SAMPLE;
        SudekiMpTalosCompanionStagingResearchInitialize(&research);
        finish_attempt(&status, &research);
        SetLastError(entry_error);
        return;
    }
    status.modal_active = snapshot.modal_active;
    status.transition_active = snapshot.transition_active;
    if (!raw_exact_closure_flags_valid(&raw)) {
        consume_exact_closure_failure(&status, &snapshot);
        SetLastError(entry_error);
        return;
    }
    if (!exact_preflight_shape(
            &raw, &snapshot, controller, update_data, witness, NULL)) {
        SudekiMpTalosCompanionStagingResearch research;

        status.failure = SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY;
        SudekiMpTalosCompanionStagingResearchInitialize(&research);
        finish_attempt(&status, &research);
        SetLastError(entry_error);
        return;
    }
    if (active_backend.mutation_authority_available != 0u) {
        run_test_transaction(
            &status, controller, update_data, witness, &raw, &snapshot);
    } else {
        consume_exact_closure_failure(&status, &snapshot);
    }
#else
    /* The normal build is deliberately only an observer-shaped seam. Native
     * capture publishes later through IngestNativeObservation(). */
    publish_status(&status);
#endif
    SetLastError(entry_error);
}

BOOL SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD entry_error = GetLastError();
    SudekiMpTalosStagingResearchAdapterStatus status;
    BOOL valid;
    BOOL accepted;

    if (result == NULL ||
        InterlockedCompareExchange(&adapter_active, 0, 0) == 0 ||
        !copy_status(&status)) {
        SetLastError(entry_error);
        return FALSE;
    }
    ++status.observation_calls;
    status.observation_failure = result->failure;
    status.observation_failed_address = result->failed_address;
    status.observation_failed_size = result->failed_size;
    status.observation_checks_completed = result->checks_completed;
    status.observation_first_range_count = result->first_range_count;
    status.observation_second_range_count = result->second_range_count;
    status.observation_first_capture_bytes = result->first_capture_bytes;
    status.observation_second_capture_bytes = result->second_capture_bytes;
    status.observation_failed_write_required = result->failed_write_required;
    status.observation_witness_present = witness != NULL ? 1u : 0u;
    status.observation_witness_entry_exact = result->witness_entry_exact;
    status.observation_witness_revalidated_exact =
        result->witness_revalidated_exact;
    status.observation_witness_dispatch_serial =
        result->witness_dispatch_serial;
    status.observation_witness_native_thread_id =
        result->witness_native_thread_id;
    status.observation_witness_registry_generation =
        result->witness_observer_registry_generation;
    status.observation_witness_overlap_generation =
        result->witness_dispatch_overlap_generation;
    status.observation_witness_source =
        result->witness_source;
    valid = native_observation_contract_valid(result, witness);
    accepted = valid && status.observation_valid == 0u;
    status.observation_last_contract_valid = valid ? 1u : 0u;
    if (accepted) {
        status.original = result->snapshot;
        status.modal_active = result->snapshot.modal_active;
        status.transition_active = result->snapshot.transition_active;
        status.observation_valid = 1u;
    } else if (status.observation_valid == 0u) {
        ZeroMemory(&status.original, sizeof(status.original));
    }
    /* Observation validity is separate from transaction completion. */
    status.success = 0u;
    status.production_mutation_compiled = 0u;
    status.observation_only = 1u;
    if (active_backend.publish != NULL) {
        active_backend.publish(active_backend.context, &status);
    }
    publish_status(&status);
    SetLastError(entry_error);
    return accepted;
}

BOOL SudekiMpTalosCompanionStagingResearchAdapterGetStatus(
    SudekiMpTalosStagingResearchAdapterStatus *status
) {
    DWORD error = GetLastError();
    BOOL result = copy_status(status);
    SetLastError(error);
    return result;
}

void SudekiMpUninstallTalosCompanionStagingResearchAdapter(void) {
    DWORD error = GetLastError();
    SudekiMpTalosStagingResearchAdapterStatus status;

    /* The integration layer disables, unregisters, and drains its observer
     * gate before calling this. The containing DLL and adapter backing remain
     * process-lifetime even after active admission is closed. */
    (void)InterlockedExchange(&adapter_active, 0);
    MemoryBarrier();
    if (copy_status(&status)) {
        status.active = 0u;
        status.installed = 0u;
        status.enabled = 0u;
        publish_status(&status);
    }
    SetLastError(error);
}

#if defined(SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING)
BOOL SudekiMpTalosCompanionStagingResearchAdapterSetBackendForTesting(
    const SudekiMpTalosStagingResearchAdapterTestBackend *backend
) {
    if (backend == NULL ||
        InterlockedCompareExchange(&adapter_active, 0, 0) != 0 ||
        !testing_backend_complete(backend)) {
        SetLastError(backend == NULL ? ERROR_INVALID_PARAMETER :
            ERROR_INVALID_DATA);
        return FALSE;
    }
    injected_backend = *backend;
    injected_backend_set = TRUE;
    return TRUE;
}

void SudekiMpTalosCompanionStagingResearchAdapterResetForTesting(void) {
    (void)InterlockedExchange(&adapter_active, 0);
    (void)InterlockedExchange(&attempt_claimed, 0);
    (void)InterlockedExchange(&status_sequence, 0);
    (void)InterlockedExchange(&status_writer_lock, 0);
    game_base = NULL;
    proof_virtual_key = 0u;
    installed_exact_executable_hash = FALSE;
    installed_exact_sol_hash = FALSE;
    hotkey_was_down = FALSE;
    next_attempt_serial = 0u;
    ZeroMemory(&public_status, sizeof(public_status));
    ZeroMemory(&active_backend, sizeof(active_backend));
    ZeroMemory(&injected_backend, sizeof(injected_backend));
    injected_backend_set = FALSE;
}
#endif
