#define SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING 1
#define SUDEKIMP_TALOS_MEMBERSHIP_ABI_TESTING 1

#include "hooks/control_separation.h"
#include "hooks/talos_companion_membership_abi.h"
#include "hooks/talos_companion_staging_research_adapter.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
            #expression); \
        return 0; \
    } \
} while (0)

enum {
    EVENT_HOTKEY = 1,
    EVENT_SAMPLE_BASE = 20,
    EVENT_PREVALIDATE = 40,
    EVENT_GET_PC = 41,
    EVENT_INSPECT = 42,
    EVENT_RESOLVE = 43,
    EVENT_CLEANUP = 44,
    EVENT_IS_PLAYER = 45,
    EVENT_GET_INDEX = 46,
    EVENT_REMOVE = 47,
    EVENT_ADD = 48,
    EVENT_DESTROY = 49,
    EVENT_PUBLISH = 50,
    EVENT_CAPACITY = 128
};

typedef enum MockNarrowTamper {
    MOCK_TAMPER_NONE = 0,
    MOCK_TAMPER_CONTROLLER_CALLBACK_EXACT,
    MOCK_TAMPER_GAME_THREAD_EXACT,
    MOCK_TAMPER_TRANSACTION_EXCLUSIVE,
    MOCK_TAMPER_NO_YIELD_WINDOW_EXACT,
    MOCK_TAMPER_LISTENER_CALLBACK_CLOSURE_EXACT,
    MOCK_TAMPER_UI_HUD_CLOSURE_EXACT,
    MOCK_TAMPER_HERO_HUD_STATE_CONVERGED,
    MOCK_TAMPER_ELCO_ARBITER_SAFE,
    MOCK_TAMPER_CONTROLLER_CALLBACK_POINTER,
    MOCK_TAMPER_TRANSACTION_POINTER,
    MOCK_TAMPER_CONTROLLER_CALLBACK_ALIASES_CONTROLLER,
    MOCK_TAMPER_TRANSACTION_ALIASES_UPDATE_DATA,
    MOCK_TAMPER_LISTENER_STORAGE_POINTER,
    MOCK_TAMPER_LISTENER_POINTER,
    MOCK_TAMPER_UI_CONTROLLER_POINTER,
    MOCK_TAMPER_HUD_OWNER_POINTER,
    MOCK_TAMPER_UI_SCENE_POINTER,
    MOCK_TAMPER_ELCO_ARBITER_POINTER,
    MOCK_TAMPER_PAUSED,
    MOCK_TAMPER_LISTENER_COUNT,
    MOCK_TAMPER_GROUP_COMBAT_D4,
    MOCK_TAMPER_ELCO_ARBITER_STATE,
    MOCK_TAMPER_ELCO_ARBITER_FLAGS,
    MOCK_TAMPER_GIZMO_POINTER,
    MOCK_TAMPER_STAT_DISPLAY_POINTER,
    MOCK_TAMPER_GIZMO_STATE,
    MOCK_TAMPER_GIZMO_FLAGS,
    MOCK_TAMPER_GIZMO_LABEL_HASH,
    MOCK_TAMPER_GIZMO_LABEL_LENGTH,
    MOCK_TAMPER_CURRENT_HP_BITS,
    MOCK_TAMPER_FILL_CACHE_PRIMARY,
    MOCK_TAMPER_FILL_CACHE_SECONDARY
} MockNarrowTamper;

typedef struct MockContext {
    int events[EVENT_CAPACITY];
    unsigned int event_count;
    uint64_t observation_serial;
    int hotkey_down;
    int detached;
    int get_pc_null;
    int wrapper_invalid;
    int resolve_failure_call;
    int resolve_calls;
    int is_player_false;
    int bad_index;
    int prevalidate_false;
    int destroy_false;
    int bad_preflight;
    int bad_detach_immediate;
    int bad_detach_independent;
    int bad_restore;
    int bad_stability;
    int bad_topology_stage;
    int bad_mutation_stage;
    int bad_listener_stage;
    int bad_callback_stage;
    int narrow_tamper_stage;
    MockNarrowTamper narrow_tamper;
    unsigned int narrow_tamper_hero;
    SudekiMpControlUpdateDispatchWitness witness;
    const SudekiMpControlUpdateDispatchWitness *observed_witness;
    int witness_identity_mismatch;
    void *transaction_cookie;
    unsigned int remove_calls;
    unsigned int add_calls;
    unsigned int destroy_calls;
    unsigned int destroy_flags;
    unsigned int publish_calls;
    char get_pc_name[16];
    SudekiMpTalosStagingResearchAdapterStatus published;
} MockContext;

static void *fake_pointer(uintptr_t value) {
    return (void *)value;
}

static void record(MockContext *context, int event) {
    if (context->event_count < EVENT_CAPACITY) {
        context->events[context->event_count++] = event;
    }
    SetLastError(0x7000u + (DWORD)event);
}

static BOOL mock_hotkey_down(void *opaque, UINT virtual_key) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_HOTKEY);
    return virtual_key == VK_F11 && context->hotkey_down;
}

static void apply_narrow_tamper(
    MockContext *context,
    SudekiMpTalosStagingResearchAdapterRawSample *sample
) {
    unsigned int hero = context->narrow_tamper_hero;

    switch (context->narrow_tamper) {
        case MOCK_TAMPER_CONTROLLER_CALLBACK_EXACT:
            sample->controller_callback_exact = 0u;
            break;
        case MOCK_TAMPER_GAME_THREAD_EXACT:
            sample->game_thread_exact = 0u;
            break;
        case MOCK_TAMPER_TRANSACTION_EXCLUSIVE:
            sample->transaction_exclusive = 0u;
            break;
        case MOCK_TAMPER_NO_YIELD_WINDOW_EXACT:
            sample->no_yield_window_exact = 0u;
            break;
        case MOCK_TAMPER_LISTENER_CALLBACK_CLOSURE_EXACT:
            sample->listener_callback_closure_exact = 0u;
            break;
        case MOCK_TAMPER_UI_HUD_CLOSURE_EXACT:
            sample->ui_hud_closure_exact = 0u;
            break;
        case MOCK_TAMPER_HERO_HUD_STATE_CONVERGED:
            sample->hero_hud_state_converged = 0u;
            break;
        case MOCK_TAMPER_ELCO_ARBITER_SAFE:
            sample->elco_arbiter_safe = 0u;
            break;
        case MOCK_TAMPER_CONTROLLER_CALLBACK_POINTER:
            sample->controller_callback = fake_pointer(0x61000u);
            break;
        case MOCK_TAMPER_TRANSACTION_POINTER:
            sample->transaction = fake_pointer(0x62000u);
            break;
        case MOCK_TAMPER_CONTROLLER_CALLBACK_ALIASES_CONTROLLER:
            sample->controller_callback = sample->controller;
            break;
        case MOCK_TAMPER_TRANSACTION_ALIASES_UPDATE_DATA:
            sample->transaction = fake_pointer(0x10b00u);
            break;
        case MOCK_TAMPER_LISTENER_STORAGE_POINTER:
            sample->listener_storage = fake_pointer(0x63000u);
            break;
        case MOCK_TAMPER_LISTENER_POINTER:
            sample->listener = fake_pointer(0x64000u);
            break;
        case MOCK_TAMPER_UI_CONTROLLER_POINTER:
            sample->ui_controller = fake_pointer(0x65000u);
            break;
        case MOCK_TAMPER_HUD_OWNER_POINTER:
            sample->hud_owner = fake_pointer(0x66000u);
            break;
        case MOCK_TAMPER_UI_SCENE_POINTER:
            sample->ui_scene = fake_pointer(0x67000u);
            break;
        case MOCK_TAMPER_ELCO_ARBITER_POINTER:
            sample->elco_arbiter = fake_pointer(0x68000u);
            break;
        case MOCK_TAMPER_PAUSED:
            sample->snapshot.paused = 1u;
            break;
        case MOCK_TAMPER_LISTENER_COUNT:
            sample->snapshot.listener_count = 2u;
            break;
        case MOCK_TAMPER_GROUP_COMBAT_D4:
            sample->group_combat_d4 = 1u;
            sample->snapshot.elco_arbiter_flags_60_masked =
                SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
            break;
        case MOCK_TAMPER_ELCO_ARBITER_STATE:
            sample->snapshot.elco_arbiter_state_58 ^= UINT32_C(0x100);
            break;
        case MOCK_TAMPER_ELCO_ARBITER_FLAGS:
            sample->snapshot.elco_arbiter_flags_60_masked =
                SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
            break;
        case MOCK_TAMPER_GIZMO_POINTER:
            sample->gizmo[hero] = fake_pointer(0x69000u + hero * 0x100u);
            break;
        case MOCK_TAMPER_STAT_DISPLAY_POINTER:
            sample->stat_display[hero] =
                fake_pointer(0x6a000u + hero * 0x100u);
            break;
        case MOCK_TAMPER_GIZMO_STATE:
            ++sample->snapshot.hero[hero].gizmo_state;
            break;
        case MOCK_TAMPER_GIZMO_FLAGS:
            ++sample->snapshot.hero[hero].gizmo_flags_masked;
            break;
        case MOCK_TAMPER_GIZMO_LABEL_HASH:
            ++sample->snapshot.hero[hero].gizmo_label_hash;
            break;
        case MOCK_TAMPER_GIZMO_LABEL_LENGTH:
            ++sample->snapshot.hero[hero].gizmo_label_length;
            break;
        case MOCK_TAMPER_CURRENT_HP_BITS:
            ++sample->snapshot.hero[hero].current_hp_bits;
            break;
        case MOCK_TAMPER_FILL_CACHE_PRIMARY:
            ++sample->snapshot.hero[hero].fill_cache_primary_bits;
            break;
        case MOCK_TAMPER_FILL_CACHE_SECONDARY:
            ++sample->snapshot.hero[hero].fill_cache_secondary_bits;
            break;
        case MOCK_TAMPER_NONE:
        default:
            break;
    }
}

static void populate_full_sample(
    MockContext *context,
    SudekiMpTalosStagingResearchAdapterStage stage,
    void *callback_controller,
    void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpTalosStagingResearchAdapterRawSample *sample
) {
    static const uint8_t full_group[4] = {0u, 1u, 2u, 3u};
    static const uint8_t full_formation[4] = {0u, 3u, 1u, 2u};
    static const uint8_t detached_group[4] = {
        0u, 1u, 2u, SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE
    };
    static const uint8_t detached_formation[4] = {
        2u, 0u, 1u, SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE
    };
    unsigned int index;
    int detached = stage ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE ||
        stage == SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_INDEPENDENT;

    ZeroMemory(sample, sizeof(*sample));
    sample->snapshot.observation_serial = ++context->observation_serial;
    sample->snapshot.foreground = 1u;
    sample->snapshot.all_pending_loaded = 1u;
    sample->snapshot.camera_scene_consistent = 1u;
    sample->snapshot.controller_current_mode = 1u;
    sample->snapshot.controller_requested_mode = 1u;
    sample->source_descriptor = fake_pointer(0x10100u);
    sample->world = fake_pointer(0x10200u);
    sample->group = fake_pointer(0x10300u);
    sample->formation_owner = fake_pointer(0x10400u);
    sample->formation = fake_pointer(0x104f4u);
    sample->controller = fake_pointer(0x10500u);
    sample->controller_callback = (void *)witness;
    sample->transaction = context->transaction_cookie;
    sample->listener_storage = fake_pointer(0x10d00u);
    sample->listener = fake_pointer(0x10e00u);
    sample->ui_controller = fake_pointer(0x10f00u);
    sample->hud_owner = fake_pointer(0x11000u);
    sample->ui_scene = fake_pointer(0x11100u);
    sample->elco_arbiter = fake_pointer(0x11200u);
    sample->front_actor = fake_pointer(0x20000u);
    sample->camera_manager = fake_pointer(0x10600u);
    sample->current_render_camera = fake_pointer(0x10700u);
    sample->render_state = fake_pointer(0x10800u);
    sample->scene_manager = fake_pointer(0x10900u);
    sample->scene_renderer = fake_pointer(0x10a00u);
    sample->topology_ranges_prevalidated = 1u;
    sample->mutation_ranges_prevalidated = 1u;
    sample->controller_callback_exact =
        callback_controller == sample->controller &&
        callback_update_data == fake_pointer(0x10b00u) &&
        witness == &context->witness;
    sample->game_thread_exact = 1u;
    sample->transaction_exclusive = 1u;
    sample->no_yield_window_exact = 1u;
    sample->listener_callback_closure_exact = 1u;
    sample->ui_hud_closure_exact = 1u;
    sample->hero_hud_state_converged = 1u;
    sample->elco_arbiter_safe = 1u;
    sample->group_combat_d4 = 0u;
    sample->snapshot.in_combat = UINT8_C(0xa5);
    sample->snapshot.group_armed = UINT8_C(0x5a);
    sample->snapshot.listener_count = 1u;
    sample->snapshot.elco_arbiter_state_58 = 3u;
    sample->snapshot.elco_arbiter_flags_60_masked = 0u;
    sample->snapshot.modal_active = (uint8_t)(0x80u + (unsigned int)stage);
    sample->snapshot.transition_active =
        (uint8_t)(0x40u - (unsigned int)stage);
    if (context->bad_topology_stage == (int)stage) {
        sample->topology_ranges_prevalidated = 0u;
    }
    if (context->bad_mutation_stage == (int)stage) {
        sample->mutation_ranges_prevalidated = 0u;
    }
    if (context->bad_listener_stage == (int)stage) {
        sample->listener_callback_closure_exact = 0u;
    }
    if (context->bad_callback_stage == (int)stage) {
        sample->controller_callback_exact = 0u;
    }
    for (index = 0u; index < 4u; ++index) {
        int tal = index == 0u;

        sample->actor[index] = fake_pointer(0x20000u + index * 0x1000u);
        sample->control_component[index] =
            fake_pointer(0x30000u + index * 0x1000u);
        sample->control_owner_actor[index] = sample->actor[index];
        sample->formation_backpointer[index] =
            detached && index == 3u ? NULL : sample->formation;
        sample->gizmo[index] =
            fake_pointer(0x40000u + index * 0x1000u);
        sample->stat_display[index] =
            fake_pointer(0x50000u + index * 0x1000u);
        sample->snapshot.hero[index].gizmo_label_hash =
            UINT64_C(0x7000) + index;
        sample->snapshot.hero[index].gizmo_state = 0x20u + index;
        sample->snapshot.hero[index].gizmo_flags_masked = 0x30u + index;
        sample->snapshot.hero[index].gizmo_label_length = 4u + index;
        sample->snapshot.hero[index].current_hp_bits =
            UINT32_C(0x42c80000) + index;
        sample->snapshot.hero[index].fill_cache_primary_bits =
            UINT32_C(0x3f000000) + index;
        sample->snapshot.hero[index].fill_cache_secondary_bits =
            sample->snapshot.hero[index].fill_cache_primary_bits;
        sample->snapshot.hero[index].native_ai_enabled = tal ? 0u : 1u;
        sample->snapshot.hero[index].human_control_owned = tal ? 1u : 0u;
        sample->snapshot.hero[index].control_mode = tal ?
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI;
    }
    if (detached) {
        sample->snapshot.group_count = 3u;
        sample->snapshot.formation_count = 3u;
        memcpy(sample->snapshot.group_order, detached_group,
            sizeof(detached_group));
        memcpy(sample->snapshot.formation_order, detached_formation,
            sizeof(detached_formation));
    } else {
        sample->snapshot.group_count = 4u;
        sample->snapshot.formation_count = 4u;
        memcpy(sample->snapshot.group_order, full_group,
            sizeof(full_group));
        memcpy(sample->snapshot.formation_order, full_formation,
            sizeof(full_formation));
    }
    if (context->narrow_tamper_stage == (int)stage) {
        apply_narrow_tamper(context, sample);
    }
}

static BOOL mock_sample(
    void *opaque,
    SudekiMpTalosStagingResearchAdapterStage stage,
    void *callback_controller,
    void *callback_update_data,
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpTalosStagingResearchAdapterRawSample *sample
) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_SAMPLE_BASE + (int)stage);
    context->observed_witness = witness;
    if (witness != &context->witness) context->witness_identity_mismatch = 1;
    populate_full_sample(context, stage, callback_controller,
        callback_update_data, witness, sample);
    if (context->bad_preflight && stage ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT) {
        sample->snapshot.group_count = 3u;
    }
    if (context->bad_detach_immediate && stage ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE) {
        sample->formation_backpointer[3] = sample->formation;
    }
    if (context->bad_detach_independent && stage ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_INDEPENDENT) {
        sample->snapshot.group_count = 4u;
    }
    if (context->bad_restore && stage ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_RESTORE_IMMEDIATE) {
        sample->snapshot.formation_order[0] = 1u;
    }
    if (context->bad_stability && stage ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_STABILITY) {
        sample->snapshot.controller_requested_mode = 2u;
    }
    return TRUE;
}

static BOOL mock_prevalidate(
    void *opaque,
    void *group,
    void *wrapper,
    void *actor
) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_PREVALIDATE);
    return !context->prevalidate_false && group == fake_pointer(0x10300u) &&
        wrapper == fake_pointer(0x90000u) && actor == fake_pointer(0x23000u);
}

static void *mock_get_pc(void *opaque, const char *resource_name) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_GET_PC);
    lstrcpynA(context->get_pc_name, resource_name,
        (int)sizeof(context->get_pc_name));
    return context->get_pc_null ? NULL : fake_pointer(0x90000u);
}

static BOOL mock_inspect_wrapper(
    void *opaque,
    void *wrapper,
    void **embedded_actor
) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_INSPECT);
    *embedded_actor = fake_pointer(0x23000u);
    return !context->wrapper_invalid && wrapper == fake_pointer(0x90000u);
}

static BOOL mock_resolve_wrapper(
    void *opaque,
    void *wrapper,
    void *tracked_tptr_12,
    void **actor
) {
    MockContext *context = (MockContext *)opaque;
    static const uint8_t zero[12] = {0};

    record(context, EVENT_RESOLVE);
    ++context->resolve_calls;
    if (wrapper != fake_pointer(0x90000u) ||
        memcmp(tracked_tptr_12, zero, sizeof(zero)) != 0 ||
        context->resolve_failure_call == context->resolve_calls) {
        return FALSE;
    }
    *(void **)tracked_tptr_12 = fake_pointer(0x23000u);
    *actor = fake_pointer(0x23000u);
    return TRUE;
}

static void mock_cleanup_tptr(void *opaque, void *tracked_tptr_12) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_CLEANUP);
    ZeroMemory(tracked_tptr_12, 12u);
}

static BOOL mock_is_player(void *opaque, void *group, void *wrapper) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_IS_PLAYER);
    return !context->is_player_false && group == fake_pointer(0x10300u) &&
        wrapper == fake_pointer(0x90000u);
}

static int mock_get_index(void *opaque, void *group, void *wrapper) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_GET_INDEX);
    if (group != fake_pointer(0x10300u) ||
        wrapper != fake_pointer(0x90000u)) return -1;
    return context->bad_index ? 2 : 3;
}

static void mock_remove_player(void *opaque, void *group, void *wrapper) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_REMOVE);
    if (group == fake_pointer(0x10300u) &&
        wrapper == fake_pointer(0x90000u)) {
        ++context->remove_calls;
        context->detached = 1;
    }
}

static void mock_add_player(void *opaque, void *group, void *wrapper) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_ADD);
    if (group == fake_pointer(0x10300u) &&
        wrapper == fake_pointer(0x90000u)) {
        ++context->add_calls;
        context->detached = 0;
    }
}

static BOOL mock_destroy_wrapper(
    void *opaque,
    void *wrapper,
    unsigned int flags
) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_DESTROY);
    ++context->destroy_calls;
    context->destroy_flags = flags;
    return !context->destroy_false && wrapper == fake_pointer(0x90000u) &&
        flags == 1u;
}

static void mock_publish(
    void *opaque,
    const SudekiMpTalosStagingResearchAdapterStatus *status
) {
    MockContext *context = (MockContext *)opaque;

    record(context, EVENT_PUBLISH);
    ++context->publish_calls;
    context->published = *status;
}

static SudekiMpTalosStagingResearchAdapterTestBackend backend_for(
    MockContext *context
) {
    SudekiMpTalosStagingResearchAdapterTestBackend backend;

    ZeroMemory(&backend, sizeof(backend));
    backend.context = context;
    backend.hotkey_down = mock_hotkey_down;
    backend.sample = mock_sample;
    backend.prevalidate_mutation = mock_prevalidate;
    backend.get_pc = mock_get_pc;
    backend.inspect_wrapper = mock_inspect_wrapper;
    backend.resolve_wrapper = mock_resolve_wrapper;
    backend.cleanup_tptr = mock_cleanup_tptr;
    backend.is_player = mock_is_player;
    backend.get_index = mock_get_index;
    backend.remove_player = mock_remove_player;
    backend.add_player = mock_add_player;
    backend.destroy_wrapper = mock_destroy_wrapper;
    backend.publish = mock_publish;
    backend.mutation_authority_available = 1u;
    return backend;
}

static uint8_t *fixture_image(void) {
    SudekiMpTalosMembershipAbiDescriptor descriptor =
        SudekiMpTalosCompanionMembershipAbiDescribe();
    uint8_t *image = (uint8_t *)VirtualAlloc(NULL,
        descriptor.mapped_image_size, MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);

    if (image == NULL || !
        SudekiMpTalosCompanionMembershipAbiPopulateFixtureForTesting(
            image, descriptor.mapped_image_size,
            (uint32_t)(uintptr_t)image)) {
        if (image != NULL) VirtualFree(image, 0u, MEM_RELEASE);
        return NULL;
    }
    return image;
}

static void initialize_exact_witness(
    SudekiMpControlUpdateDispatchWitness *witness,
    uint64_t dispatch_serial
) {
    ZeroMemory(witness, sizeof(*witness));
    witness->dispatch_serial = dispatch_serial;
    witness->native_thread_id = GetCurrentThreadId();
    witness->outer_update_depth = 1u;
    witness->active_dispatch_count = 1u;
    witness->original_call_count = 1u;
    witness->observer_snapshot_count = 1u;
    witness->observer_registry_generation = 7u;
    witness->dispatch_overlap_generation = 11u;
    witness->hook_owned_exact = 1u;
    witness->slot_owned_exact = 1u;
    witness->service_only = 1u;
    witness->post_original = 1u;
    witness->source =
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL;
    witness->source_exact = 1u;
    witness->service_post_original_exact = 1u;
    witness->sole_observer = 1u;
    witness->registry_generation_stable = 1u;
}

static SudekiMpTalosStagingNativeSamplerResult valid_native_observation(void) {
    static const uint8_t group_order[4] = {0u, 1u, 2u, 3u};
    static const uint8_t formation_order[4] = {0u, 3u, 1u, 2u};
    SudekiMpTalosStagingNativeSamplerResult result;
    unsigned int hero;

    ZeroMemory(&result, sizeof(result));
    result.snapshot.observation_serial = UINT64_C(0x1234);
    result.snapshot.process_token = UINT64_C(0x1001);
    result.snapshot.native_thread_token = UINT64_C(0x1002);
    result.snapshot.source_token = UINT64_C(0x1003);
    result.snapshot.world_token = UINT64_C(0x1004);
    result.snapshot.group_token = UINT64_C(0x1005);
    result.snapshot.formation_owner_token = UINT64_C(0x1006);
    result.snapshot.formation_token = UINT64_C(0x1007);
    result.snapshot.controller_token = UINT64_C(0x1008);
    result.snapshot.controller_callback_token = UINT64_C(0x1009);
    result.snapshot.transaction_token = UINT64_C(0x100a);
    result.snapshot.listener_storage_token = UINT64_C(0x100b);
    result.snapshot.listener_token = UINT64_C(0x100c);
    result.snapshot.ui_controller_token = UINT64_C(0x100d);
    result.snapshot.hud_owner_token = UINT64_C(0x100e);
    result.snapshot.ui_scene_token = UINT64_C(0x100f);
    result.snapshot.elco_arbiter_token = UINT64_C(0x1010);
    result.snapshot.camera_token = UINT64_C(0x1011);
    result.snapshot.current_render_camera_token = UINT64_C(0x1012);
    result.snapshot.render_state_token = UINT64_C(0x1013);
    result.snapshot.scene_manager_token = UINT64_C(0x1014);
    result.snapshot.scene_renderer_token = UINT64_C(0x1015);
    result.snapshot.group_count = 4u;
    result.snapshot.formation_count = 4u;
    memcpy(result.snapshot.group_order, group_order, sizeof(group_order));
    memcpy(result.snapshot.formation_order, formation_order,
        sizeof(formation_order));
    result.snapshot.listener_count = 1u;
    result.snapshot.controller_current_mode = 1u;
    result.snapshot.controller_requested_mode = 1u;
    result.snapshot.exact_executable_hash = 1u;
    result.snapshot.exact_sol_hash = 1u;
    result.snapshot.foreground = 1u;
    result.snapshot.all_pending_loaded = 1u;
    result.snapshot.camera_scene_consistent = 1u;
    result.snapshot.controller_callback_exact = 1u;
    result.snapshot.game_thread_exact = 1u;
    result.snapshot.transaction_exclusive = 1u;
    result.snapshot.no_yield_window_exact = 1u;
    result.snapshot.listener_callback_closure_exact = 1u;
    result.snapshot.ui_hud_closure_exact = 1u;
    result.snapshot.hero_hud_state_converged = 1u;
    result.snapshot.elco_arbiter_safe = 1u;
    result.snapshot.modal_active = 1u;
    for (hero = 0u; hero < 4u; ++hero) {
        int tal = hero == 0u;

        result.snapshot.hero[hero].actor_token =
            UINT64_C(0x2000) + hero * UINT64_C(0x10);
        result.snapshot.hero[hero].control_component_token =
            UINT64_C(0x2001) + hero * UINT64_C(0x10);
        result.snapshot.hero[hero].control_owner_actor_token =
            result.snapshot.hero[hero].actor_token;
        result.snapshot.hero[hero].formation_backpointer_token =
            result.snapshot.formation_token;
        result.snapshot.hero[hero].gizmo_token =
            UINT64_C(0x2002) + hero * UINT64_C(0x10);
        result.snapshot.hero[hero].stat_display_token =
            UINT64_C(0x2003) + hero * UINT64_C(0x10);
        result.snapshot.hero[hero].gizmo_label_hash =
            UINT64_C(0x3000) + hero;
        result.snapshot.hero[hero].gizmo_label_length = 4u + hero;
        result.snapshot.hero[hero].fill_cache_primary_bits =
            UINT32_C(0x3f000000) + hero;
        result.snapshot.hero[hero].fill_cache_secondary_bits =
            result.snapshot.hero[hero].fill_cache_primary_bits;
        result.snapshot.hero[hero].native_ai_enabled = tal ? 0u : 1u;
        result.snapshot.hero[hero].human_control_owned = tal ? 1u : 0u;
        result.snapshot.hero[hero].control_mode = tal ?
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI;
    }
    result.snapshot.front_actor_token = result.snapshot.hero[0].actor_token;
    result.valid = 1u;
    result.observation_only = 1u;
    result.external_sha256_required = 1u;
    result.membership_abi_required = 1u;
    result.witness_entry_exact = 1u;
    result.witness_revalidated_exact = 1u;
    result.transaction_lease_exact = 1u;
    result.capture_no_yield_exact = 1u;
    result.checks_completed = 321u;
    result.first_range_count = 24u;
    result.second_range_count = 24u;
    result.first_capture_bytes = 0x4000u;
    result.second_capture_bytes = 0x4000u;
    result.witness_dispatch_serial = UINT64_C(303);
    result.witness_native_thread_id = GetCurrentThreadId();
    result.witness_observer_registry_generation = 7u;
    result.witness_dispatch_overlap_generation = 11u;
    result.witness_source =
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL;
    return result;
}

static int install_mock(MockContext *context, uint8_t **image) {
    static uintptr_t next_cookie = 0x70000u;
    SudekiMpTalosStagingResearchAdapterTestBackend backend =
        backend_for(context);

    SudekiMpTalosCompanionStagingResearchAdapterResetForTesting();
    next_cookie += 0x100u;
    initialize_exact_witness(&context->witness, UINT64_C(303));
    context->transaction_cookie = fake_pointer(next_cookie + 0x40u);
    *image = fixture_image();
    CHECK(*image != NULL);
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterSetBackendForTesting(
        &backend));
    CHECK(SudekiMpInstallTalosCompanionStagingResearchAdapter(
        (HMODULE)*image, TRUE, VK_F11, TRUE, TRUE));
    return 1;
}

static void release_fixture(uint8_t *image) {
    SudekiMpUninstallTalosCompanionStagingResearchAdapter();
    if (image != NULL) VirtualFree(image, 0u, MEM_RELEASE);
}

static int run_once(
    MockContext *context,
    SudekiMpTalosStagingResearchAdapterStatus *status
) {
    context->hotkey_down = 1;
    SetLastError(0xbeefu);
    SudekiMpTalosCompanionStagingResearchAdapterService(
        fake_pointer(0x10500u), fake_pointer(0x10b00u), &context->witness);
    CHECK(GetLastError() == 0xbeefu);
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterGetStatus(status));
    CHECK(GetLastError() == 0xbeefu);
    return 1;
}

static int test_default_off_and_install_gates(void) {
    SudekiMpTalosStagingResearchAdapterStatus status;
    SudekiMpControlUpdateDispatchWitness witness;
    uint8_t *image;

    SudekiMpTalosCompanionStagingResearchAdapterResetForTesting();
    CHECK(SudekiMpInstallTalosCompanionStagingResearchAdapter(
        NULL, FALSE, 0u, FALSE, FALSE));
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterGetStatus(&status));
    CHECK(status.installed == 0u && status.remove_calls == 0u);
    image = fixture_image();
    CHECK(image != NULL);
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosCompanionStagingResearchAdapter(
        (HMODULE)image, TRUE, VK_F11, FALSE, TRUE));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    image[0x104480u] ^= 1u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallTalosCompanionStagingResearchAdapter(
        (HMODULE)image, TRUE, VK_F11, TRUE, TRUE));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    VirtualFree(image, 0u, MEM_RELEASE);

    /* The production observation-only profile needs no hotkey and has no raw
     * sampler or transaction backend behind its three-argument callback. */
    SudekiMpTalosCompanionStagingResearchAdapterResetForTesting();
    image = fixture_image();
    CHECK(image != NULL);
    CHECK(SudekiMpInstallTalosCompanionStagingResearchAdapter(
        (HMODULE)image, TRUE, 0u, TRUE, TRUE));
    initialize_exact_witness(&witness, UINT64_C(0x55));
    SetLastError(0xa55au);
    SudekiMpTalosCompanionStagingResearchAdapterService(
        fake_pointer(0x10500u), fake_pointer(0x10b00u), &witness);
    CHECK(GetLastError() == 0xa55au);
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterGetStatus(&status));
    CHECK(GetLastError() == 0xa55au);
    CHECK(status.service_calls == 1u && status.sample_calls == 0u &&
        status.get_pc_calls == 0u && status.remove_calls == 0u &&
        status.add_calls == 0u && status.wrapper_destroy_calls == 0u &&
        status.production_mutation_compiled == 0u &&
        status.observation_only == 1u);
    release_fixture(image);
    return 1;
}

static int test_happy_path_and_order(void) {
    MockContext context;
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;
    unsigned int i;
    int saw_remove = 0;
    int saw_add = 0;

    ZeroMemory(&context, sizeof(context));
    CHECK(install_mock(&context, &image));
    CHECK(run_once(&context, &status));
    CHECK(status.success == 1u && status.quarantined == 0u);
    CHECK(status.observation_only == 1u);
    CHECK(status.reload_required == 1u && status.remove_reported == 1u);
    CHECK(status.coordinator_state ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS);
    CHECK(status.get_pc_calls == 1u && status.remove_calls == 1u &&
        status.add_calls == 1u && status.wrapper_destroy_calls == 1u);
    CHECK(status.resolve_calls == 3u && status.tptr_cleanup_calls == 3u);
    CHECK(status.is_player_calls == 1u && status.get_index_calls == 1u);
    CHECK(context.remove_calls == 1u && context.add_calls == 1u &&
        context.destroy_calls == 1u && context.destroy_flags == 1u);
    CHECK(context.observed_witness == &context.witness &&
        context.witness_identity_mismatch == 0);
    CHECK(strcmp(context.get_pc_name, "PC_ELCO") == 0);
    CHECK(context.events[context.event_count - 1u] == EVENT_PUBLISH);
    for (i = 0u; i < context.event_count; ++i) {
        if (context.events[i] == EVENT_REMOVE) saw_remove = 1;
        else if (context.events[i] == EVENT_ADD) {
            CHECK(saw_remove && !saw_add);
            saw_add = 1;
        } else if (saw_remove && !saw_add) {
            CHECK(context.events[i] != EVENT_PUBLISH &&
                context.events[i] != EVENT_DESTROY &&
                context.events[i] != EVENT_HOTKEY &&
                context.events[i] != EVENT_PREVALIDATE);
        }
    }
    CHECK(saw_remove && saw_add);
    CHECK(status.original.hero[3].actor_token ==
        status.detached.hero[3].actor_token);
    CHECK(status.original.production_authority == 0u &&
        status.original.carry_authority == 0u &&
        status.original.actor_lifetime_authority == 0u);
    CHECK(status.original.in_combat == 0u &&
        status.original.group_armed == 0u &&
        status.detached.in_combat == status.detached.group_armed &&
        status.restored.in_combat == status.restored.group_armed &&
        status.stable.in_combat == status.stable.group_armed);
    CHECK(status.original.controller_callback_token != 0u &&
        status.original.transaction_token != 0u &&
        status.original.controller_callback_token !=
            status.original.controller_token &&
        status.original.transaction_token !=
            status.original.controller_token &&
        status.original.transaction_token !=
            status.original.controller_callback_token);
    CHECK(status.original.controller_callback_token ==
            status.stable.controller_callback_token &&
        status.original.transaction_token == status.stable.transaction_token &&
        status.original.listener_storage_token ==
            status.stable.listener_storage_token &&
        status.original.hud_owner_token == status.stable.hud_owner_token &&
        status.original.elco_arbiter_token ==
            status.stable.elco_arbiter_token);
    CHECK(status.original.hero[0].gizmo_token != 0u &&
        status.original.hero[0].stat_display_token != 0u &&
        status.original.hero[0].fill_cache_primary_bits ==
            status.original.hero[0].fill_cache_secondary_bits &&
        status.original.hero[0].current_hp_bits ==
            status.stable.hero[0].current_hp_bits);
    CHECK(status.original.hero[3].formation_backpointer_token != 0u &&
        status.detached.hero[3].formation_backpointer_token == 0u &&
        status.restored.hero[3].formation_backpointer_token ==
            status.original.hero[3].formation_backpointer_token);
    CHECK(status.original.modal_active == 0x81u &&
        status.detached.modal_active == 0x83u &&
        status.restored.modal_active == 0x85u &&
        status.stable.modal_active == 0x86u);
    CHECK(status.original.transition_active == 0x3fu &&
        status.detached.transition_active == 0x3du &&
        status.restored.transition_active == 0x3bu &&
        status.stable.transition_active == 0x3au);

    context.hotkey_down = 0;
    SudekiMpTalosCompanionStagingResearchAdapterService(
        fake_pointer(0x10500u), fake_pointer(0x10b00u), &context.witness);
    context.hotkey_down = 1;
    SudekiMpTalosCompanionStagingResearchAdapterService(
        fake_pointer(0x10500u), fake_pointer(0x10b00u), &context.witness);
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterGetStatus(&status));
    CHECK(status.remove_calls == 1u && status.add_calls == 1u);
    release_fixture(image);
    return 1;
}

static int test_preflight_failures_make_zero_native_calls(void) {
    MockContext context;
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;

    ZeroMemory(&context, sizeof(context));
    context.bad_preflight = 1;
    CHECK(install_mock(&context, &image));
    CHECK(run_once(&context, &status));
    CHECK(status.failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY);
    CHECK(status.get_pc_calls == 0u && status.resolve_calls == 0u &&
        status.remove_calls == 0u && status.add_calls == 0u &&
        status.wrapper_destroy_calls == 0u);
    release_fixture(image);

    ZeroMemory(&context, sizeof(context));
    context.narrow_tamper_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT;
    context.narrow_tamper = MOCK_TAMPER_GAME_THREAD_EXACT;
    CHECK(install_mock(&context, &image));
    CHECK(run_once(&context, &status));
    CHECK(status.failure ==
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE);
    CHECK(status.get_pc_calls == 0u && status.remove_calls == 0u &&
        status.add_calls == 0u);
    release_fixture(image);

    ZeroMemory(&context, sizeof(context));
    context.narrow_tamper_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT;
    context.narrow_tamper =
        MOCK_TAMPER_CONTROLLER_CALLBACK_ALIASES_CONTROLLER;
    CHECK(install_mock(&context, &image));
    CHECK(run_once(&context, &status));
    CHECK(status.failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY);
    CHECK(status.get_pc_calls == 0u && status.remove_calls == 0u);
    release_fixture(image);

    ZeroMemory(&context, sizeof(context));
    context.narrow_tamper_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_PREFLIGHT;
    context.narrow_tamper = MOCK_TAMPER_TRANSACTION_ALIASES_UPDATE_DATA;
    CHECK(install_mock(&context, &image));
    CHECK(run_once(&context, &status));
    CHECK(status.failure == SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_TOPOLOGY);
    CHECK(status.get_pc_calls == 0u && status.remove_calls == 0u);
    release_fixture(image);
    return 1;
}

static int run_wrapper_failure_case(
    MockContext *context,
    uint32_t expected_failure,
    uint32_t expected_resolves,
    uint32_t expected_cleanups,
    uint32_t expected_destroys
) {
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;

    CHECK(install_mock(context, &image));
    CHECK(run_once(context, &status));
    CHECK(status.failure == expected_failure);
    CHECK(status.resolve_calls == expected_resolves);
    CHECK(status.tptr_cleanup_calls == expected_cleanups);
    CHECK(status.wrapper_destroy_calls == expected_destroys);
    CHECK(status.remove_calls == 0u && status.add_calls == 0u &&
        status.reload_required == 0u);
    release_fixture(image);
    return 1;
}

static int test_wrapper_and_membership_failures(void) {
    MockContext context;

    ZeroMemory(&context, sizeof(context));
    context.get_pc_null = 1;
    CHECK(run_wrapper_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_GET_PC, 0u, 0u, 0u));
    ZeroMemory(&context, sizeof(context));
    context.wrapper_invalid = 1;
    CHECK(run_wrapper_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_WRAPPER, 0u, 0u, 1u));
    ZeroMemory(&context, sizeof(context));
    context.resolve_failure_call = 1;
    CHECK(run_wrapper_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESOLVE, 1u, 1u, 1u));
    ZeroMemory(&context, sizeof(context));
    context.is_player_false = 1;
    CHECK(run_wrapper_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_MEMBERSHIP, 1u, 1u, 1u));
    ZeroMemory(&context, sizeof(context));
    context.bad_index = 1;
    CHECK(run_wrapper_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_MEMBERSHIP, 1u, 1u, 1u));
    return 1;
}

static int run_transaction_failure_case(
    MockContext *context,
    uint32_t expected_failure,
    uint32_t expected_remove,
    uint32_t expected_add,
    int expect_quarantine
) {
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;

    CHECK(install_mock(context, &image));
    CHECK(run_once(context, &status));
    CHECK(status.failure == expected_failure);
    CHECK(status.remove_calls == expected_remove &&
        status.add_calls == expected_add);
    CHECK(status.wrapper_destroy_calls == 1u);
    CHECK(status.reload_required == (uint8_t)(expected_remove ? 1u : 0u));
    CHECK(status.quarantined == (uint8_t)(expect_quarantine ? 1u : 0u));
    CHECK(status.success == 0u);
    release_fixture(image);
    return 1;
}

static int narrow_tamper_is_exact_flag(MockNarrowTamper tamper) {
    return tamper >= MOCK_TAMPER_CONTROLLER_CALLBACK_EXACT &&
        tamper <= MOCK_TAMPER_ELCO_ARBITER_SAFE;
}

static int run_narrow_tamper_case(
    MockNarrowTamper tamper,
    unsigned int hero,
    SudekiMpTalosStagingResearchAdapterStage stage
) {
    MockContext context;
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;
    uint32_t expected_remove;
    uint32_t expected_add;

    ZeroMemory(&context, sizeof(context));
    context.narrow_tamper = tamper;
    context.narrow_tamper_hero = hero;
    context.narrow_tamper_stage = (int)stage;
    CHECK(install_mock(&context, &image));
    CHECK(run_once(&context, &status));
    expected_remove = stage >=
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE ? 1u : 0u;
    expected_add = stage >=
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_RESTORE_IMMEDIATE ? 1u : 0u;
    CHECK(status.failure != SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_NONE);
    if (narrow_tamper_is_exact_flag(tamper)) {
        CHECK(status.failure ==
            SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE);
    }
    CHECK(status.remove_calls == expected_remove);
    CHECK(status.add_calls == expected_add);
    CHECK(status.wrapper_destroy_calls == 1u);
    CHECK(status.reload_required == (uint8_t)expected_remove);
    CHECK(status.quarantined == (uint8_t)expected_remove);
    CHECK(status.success == 0u);
    release_fixture(image);
    return 1;
}

static int test_every_narrow_field_at_every_transaction_stage(void) {
    static const MockNarrowTamper global_tampers[] = {
        MOCK_TAMPER_CONTROLLER_CALLBACK_EXACT,
        MOCK_TAMPER_GAME_THREAD_EXACT,
        MOCK_TAMPER_TRANSACTION_EXCLUSIVE,
        MOCK_TAMPER_NO_YIELD_WINDOW_EXACT,
        MOCK_TAMPER_LISTENER_CALLBACK_CLOSURE_EXACT,
        MOCK_TAMPER_UI_HUD_CLOSURE_EXACT,
        MOCK_TAMPER_HERO_HUD_STATE_CONVERGED,
        MOCK_TAMPER_ELCO_ARBITER_SAFE,
        MOCK_TAMPER_CONTROLLER_CALLBACK_POINTER,
        MOCK_TAMPER_TRANSACTION_POINTER,
        MOCK_TAMPER_LISTENER_STORAGE_POINTER,
        MOCK_TAMPER_LISTENER_POINTER,
        MOCK_TAMPER_UI_CONTROLLER_POINTER,
        MOCK_TAMPER_HUD_OWNER_POINTER,
        MOCK_TAMPER_UI_SCENE_POINTER,
        MOCK_TAMPER_ELCO_ARBITER_POINTER,
        MOCK_TAMPER_PAUSED,
        MOCK_TAMPER_LISTENER_COUNT,
        MOCK_TAMPER_GROUP_COMBAT_D4,
        MOCK_TAMPER_ELCO_ARBITER_STATE,
        MOCK_TAMPER_ELCO_ARBITER_FLAGS
    };
    static const MockNarrowTamper hero_tampers[] = {
        MOCK_TAMPER_GIZMO_POINTER,
        MOCK_TAMPER_STAT_DISPLAY_POINTER,
        MOCK_TAMPER_GIZMO_STATE,
        MOCK_TAMPER_GIZMO_FLAGS,
        MOCK_TAMPER_GIZMO_LABEL_HASH,
        MOCK_TAMPER_GIZMO_LABEL_LENGTH,
        MOCK_TAMPER_CURRENT_HP_BITS,
        MOCK_TAMPER_FILL_CACHE_PRIMARY,
        MOCK_TAMPER_FILL_CACHE_SECONDARY
    };
    static const SudekiMpTalosStagingResearchAdapterStage stages[] = {
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_REMOVE_AUTHORIZATION,
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE,
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_INDEPENDENT,
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_RESTORE_IMMEDIATE,
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_STABILITY
    };
    unsigned int i;
    unsigned int j;
    unsigned int hero;

    for (i = 0u;
         i < sizeof(global_tampers) / sizeof(global_tampers[0]); ++i) {
        for (j = 0u; j < sizeof(stages) / sizeof(stages[0]); ++j) {
            CHECK(run_narrow_tamper_case(
                global_tampers[i], 0u, stages[j]));
        }
    }
    for (hero = 0u; hero <
            SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++hero) {
        for (i = 0u;
             i < sizeof(hero_tampers) / sizeof(hero_tampers[0]); ++i) {
            for (j = 0u; j < sizeof(stages) / sizeof(stages[0]); ++j) {
                CHECK(run_narrow_tamper_case(
                    hero_tampers[i], hero, stages[j]));
            }
        }
    }
    return 1;
}

static int test_transaction_failures_are_sticky(void) {
    MockContext context;

    ZeroMemory(&context, sizeof(context));
    context.prevalidate_false = 1;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RANGE_PREFLIGHT,
        0u, 0u, 0));
    ZeroMemory(&context, sizeof(context));
    context.bad_detach_immediate = 1;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH,
        1u, 0u, 1));
    ZeroMemory(&context, sizeof(context));
    context.resolve_failure_call = 2;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH,
        1u, 0u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_detach_independent = 1;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH,
        1u, 0u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_restore = 1;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESTORE,
        1u, 1u, 1));
    ZeroMemory(&context, sizeof(context));
    context.resolve_failure_call = 3;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_RESTORE,
        1u, 1u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_stability = 1;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_STABILITY,
        1u, 1u, 1));
    ZeroMemory(&context, sizeof(context));
    context.destroy_false = 1;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_WRAPPER_DESTROY,
        1u, 1u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_topology_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_IMMEDIATE;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_DETACH,
        1u, 0u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_listener_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_DETACH_INDEPENDENT;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE,
        1u, 0u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_callback_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_RESTORE_IMMEDIATE;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_EXACT_CLOSURE,
        1u, 1u, 1));
    ZeroMemory(&context, sizeof(context));
    context.bad_mutation_stage =
        SUDEKIMP_TALOS_STAGING_ADAPTER_STAGE_STABILITY;
    CHECK(run_transaction_failure_case(&context,
        SUDEKIMP_TALOS_STAGING_ADAPTER_FAILURE_STABILITY,
        1u, 1u, 1));
    return 1;
}

static int get_observation_status(
    SudekiMpTalosStagingResearchAdapterStatus *status
) {
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterGetStatus(status));
    CHECK(GetLastError() == 0xd00du);
    CHECK(status->get_pc_calls == 0u && status->resolve_calls == 0u &&
        status->tptr_cleanup_calls == 0u && status->is_player_calls == 0u &&
        status->get_index_calls == 0u && status->remove_calls == 0u &&
        status->add_calls == 0u && status->wrapper_destroy_calls == 0u);
    CHECK(status->coordinator_state == 0u &&
        status->coordinator_result == 0u && status->success == 0u &&
        status->production_mutation_compiled == 0u &&
        status->observation_only == 1u);
    return 1;
}

static int expect_invalid_observation(
    const SudekiMpTalosStagingNativeSamplerResult *result,
    const SudekiMpControlUpdateDispatchWitness *witness,
    int valid_already_published,
    uint64_t sticky_serial
) {
    SudekiMpTalosStagingResearchAdapterStatus status;

    SetLastError(0xd00du);
    CHECK(!SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
        result, witness));
    CHECK(GetLastError() == 0xd00du);
    CHECK(get_observation_status(&status));
    CHECK(status.observation_last_contract_valid == 0u);
    CHECK(status.observation_valid ==
        (uint8_t)(valid_already_published ? 1u : 0u));
    CHECK(status.original.observation_serial ==
        (valid_already_published ? sticky_serial : 0u));
    return 1;
}

static int test_native_observation_ingestion(void) {
    MockContext context;
    SudekiMpTalosStagingNativeSamplerResult result;
    SudekiMpTalosStagingNativeSamplerResult second;
    SudekiMpTalosStagingResearchAdapterStatus status;
    SudekiMpTalosStagingResearchSnapshot sticky_snapshot;
    uint8_t *image;

    ZeroMemory(&context, sizeof(context));
    CHECK(install_mock(&context, &image));
    result = valid_native_observation();
    SetLastError(0xd00du);
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
        &result, &context.witness));
    CHECK(GetLastError() == 0xd00du);
    CHECK(get_observation_status(&status));
    CHECK(status.observation_calls == 1u && status.observation_valid == 1u &&
        status.observation_last_contract_valid == 1u);
    CHECK(status.observation_failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_OK &&
        status.observation_failed_address == 0u &&
        status.observation_failed_size == 0u &&
        status.observation_failed_write_required == 0u &&
        status.observation_checks_completed == result.checks_completed);
    CHECK(status.observation_first_range_count == result.first_range_count &&
        status.observation_second_range_count == result.second_range_count &&
        status.observation_first_capture_bytes ==
            result.first_capture_bytes &&
        status.observation_second_capture_bytes ==
            result.second_capture_bytes);
    CHECK(status.observation_witness_present == 1u &&
        status.observation_witness_entry_exact == 1u &&
        status.observation_witness_revalidated_exact == 1u &&
        status.observation_witness_dispatch_serial ==
            context.witness.dispatch_serial &&
        status.observation_witness_native_thread_id ==
            context.witness.native_thread_id &&
        status.observation_witness_registry_generation ==
            context.witness.observer_registry_generation &&
        status.observation_witness_overlap_generation ==
            context.witness.dispatch_overlap_generation &&
        status.observation_witness_source == context.witness.source);
    CHECK(memcmp(&status.original, &result.snapshot,
        sizeof(status.original)) == 0);
    sticky_snapshot = status.original;
    CHECK(context.publish_calls == 1u && context.event_count == 1u &&
        context.events[0] == EVENT_PUBLISH);

    /* The first valid observation is one-shot: a later valid sample cannot
     * replace it, and a later invalid diagnostic cannot erase it. */
    second = result;
    second.snapshot.observation_serial++;
    SetLastError(0xd00du);
    CHECK(!SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
        &second, &context.witness));
    CHECK(GetLastError() == 0xd00du);
    CHECK(get_observation_status(&status));
    CHECK(status.observation_calls == 2u && status.observation_valid == 1u &&
        status.observation_last_contract_valid == 1u &&
        memcmp(&status.original, &sticky_snapshot,
            sizeof(status.original)) == 0);

    second.failure = SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP;
    second.failed_address = 0x408d94u;
    second.failed_size = 12u;
    second.failed_write_required = 1u;
    second.valid = 0u;
    CHECK(expect_invalid_observation(&second, &context.witness, 1,
        result.snapshot.observation_serial));
    CHECK(get_observation_status(&status));
    CHECK(status.observation_failure == SUDEKIMP_TALOS_NATIVE_SAMPLE_GROUP &&
        status.observation_failed_address == 0x408d94u &&
        status.observation_failed_size == 12u &&
        status.observation_failed_write_required == 1u);
    CHECK(context.remove_calls == 0u && context.add_calls == 0u &&
        context.destroy_calls == 0u && context.observed_witness == NULL);
    release_fixture(image);
    return 1;
}

static int test_invalid_native_observation_contract(void) {
    MockContext context;
    SudekiMpTalosStagingNativeSamplerResult result;
    SudekiMpControlUpdateDispatchWitness witness;
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;
    uint8_t *booleans[64];
    uint8_t *required[32];
    uint64_t *continuity_tokens[32];
    unsigned int count = 0u;
    unsigned int required_count = 0u;
    unsigned int token_count = 0u;
    unsigned int index;
    unsigned int hero;

    ZeroMemory(&context, sizeof(context));
    CHECK(install_mock(&context, &image));
    result = valid_native_observation();
    witness = context.witness;

#define ADD_BOOLEAN(field) booleans[count++] = &(field)
    ADD_BOOLEAN(result.valid);
    ADD_BOOLEAN(result.observation_only);
    ADD_BOOLEAN(result.native_engine_calls_permitted);
    ADD_BOOLEAN(result.hooks_permitted);
    ADD_BOOLEAN(result.actor_lifetime_authority);
    ADD_BOOLEAN(result.mutation_authority);
    ADD_BOOLEAN(result.external_sha256_required);
    ADD_BOOLEAN(result.membership_abi_required);
    ADD_BOOLEAN(result.failed_write_required);
    ADD_BOOLEAN(result.witness_entry_exact);
    ADD_BOOLEAN(result.witness_revalidated_exact);
    ADD_BOOLEAN(result.transaction_lease_exact);
    ADD_BOOLEAN(result.capture_no_yield_exact);
    ADD_BOOLEAN(result.snapshot.exact_executable_hash);
    ADD_BOOLEAN(result.snapshot.exact_sol_hash);
    ADD_BOOLEAN(result.snapshot.foreground);
    ADD_BOOLEAN(result.snapshot.all_pending_loaded);
    ADD_BOOLEAN(result.snapshot.camera_scene_consistent);
    ADD_BOOLEAN(result.snapshot.controller_callback_exact);
    ADD_BOOLEAN(result.snapshot.game_thread_exact);
    ADD_BOOLEAN(result.snapshot.transaction_exclusive);
    ADD_BOOLEAN(result.snapshot.no_yield_window_exact);
    ADD_BOOLEAN(result.snapshot.listener_callback_closure_exact);
    ADD_BOOLEAN(result.snapshot.ui_hud_closure_exact);
    ADD_BOOLEAN(result.snapshot.hero_hud_state_converged);
    ADD_BOOLEAN(result.snapshot.elco_arbiter_safe);
    ADD_BOOLEAN(result.snapshot.in_combat);
    ADD_BOOLEAN(result.snapshot.async_active);
    ADD_BOOLEAN(result.snapshot.tsa_active);
    ADD_BOOLEAN(result.snapshot.paused);
    ADD_BOOLEAN(result.snapshot.transition_active);
    ADD_BOOLEAN(result.snapshot.modal_active);
    ADD_BOOLEAN(result.snapshot.group_armed);
    ADD_BOOLEAN(result.snapshot.production_authority);
    ADD_BOOLEAN(result.snapshot.carry_authority);
    ADD_BOOLEAN(result.snapshot.actor_lifetime_authority);
    ADD_BOOLEAN(result.snapshot.reload_required);
    for (hero = 0u; hero <
            SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++hero) {
        ADD_BOOLEAN(result.snapshot.hero[hero].native_ai_enabled);
        ADD_BOOLEAN(result.snapshot.hero[hero].human_control_owned);
        ADD_BOOLEAN(result.snapshot.hero[hero].override_active);
    }
#undef ADD_BOOLEAN
    CHECK(count <= sizeof(booleans) / sizeof(booleans[0]));
    for (index = 0u; index < count; ++index) {
        uint8_t saved = *booleans[index];

        *booleans[index] = 2u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        *booleans[index] = saved;
    }

#define ADD_REQUIRED(field) required[required_count++] = &(field)
    ADD_REQUIRED(result.valid);
    ADD_REQUIRED(result.observation_only);
    ADD_REQUIRED(result.external_sha256_required);
    ADD_REQUIRED(result.membership_abi_required);
    ADD_REQUIRED(result.witness_entry_exact);
    ADD_REQUIRED(result.witness_revalidated_exact);
    ADD_REQUIRED(result.transaction_lease_exact);
    ADD_REQUIRED(result.capture_no_yield_exact);
    ADD_REQUIRED(result.snapshot.exact_executable_hash);
    ADD_REQUIRED(result.snapshot.exact_sol_hash);
    ADD_REQUIRED(result.snapshot.foreground);
    ADD_REQUIRED(result.snapshot.all_pending_loaded);
    ADD_REQUIRED(result.snapshot.camera_scene_consistent);
    ADD_REQUIRED(result.snapshot.controller_callback_exact);
    ADD_REQUIRED(result.snapshot.game_thread_exact);
    ADD_REQUIRED(result.snapshot.transaction_exclusive);
    ADD_REQUIRED(result.snapshot.no_yield_window_exact);
    ADD_REQUIRED(result.snapshot.listener_callback_closure_exact);
    ADD_REQUIRED(result.snapshot.ui_hud_closure_exact);
    ADD_REQUIRED(result.snapshot.hero_hud_state_converged);
    ADD_REQUIRED(result.snapshot.elco_arbiter_safe);
#undef ADD_REQUIRED
    for (index = 0u; index < required_count; ++index) {
        uint8_t saved = *required[index];

        *required[index] = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        *required[index] = saved;
    }

    required_count = 0u;
#define ADD_PROHIBITED(field) required[required_count++] = &(field)
    ADD_PROHIBITED(result.native_engine_calls_permitted);
    ADD_PROHIBITED(result.hooks_permitted);
    ADD_PROHIBITED(result.actor_lifetime_authority);
    ADD_PROHIBITED(result.mutation_authority);
    ADD_PROHIBITED(result.snapshot.in_combat);
    ADD_PROHIBITED(result.snapshot.group_armed);
    ADD_PROHIBITED(result.snapshot.async_active);
    ADD_PROHIBITED(result.snapshot.tsa_active);
    ADD_PROHIBITED(result.snapshot.paused);
    ADD_PROHIBITED(result.snapshot.production_authority);
    ADD_PROHIBITED(result.snapshot.carry_authority);
    ADD_PROHIBITED(result.snapshot.actor_lifetime_authority);
    ADD_PROHIBITED(result.snapshot.reload_required);
#undef ADD_PROHIBITED
    for (index = 0u; index < required_count; ++index) {
        *required[index] = 1u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        *required[index] = 0u;
    }

    result.checks_completed = 0u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.first_range_count = 0u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.first_range_count = SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_RANGES + 1u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.second_range_count++;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.first_capture_bytes = 0u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.first_capture_bytes =
        SUDEKIMP_TALOS_NATIVE_SAMPLE_MAX_CAPTURE_BYTES + 1u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.second_capture_bytes++;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    for (hero = 0u; hero < 4u; ++hero) {
        result.snapshot.hero[hero].wrapper_token = 1u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result.snapshot.hero[hero].wrapper_token = 0u;
    }
    result = valid_native_observation();
    result.snapshot.reload_required = 1u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.reserved[0] = 1u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));

    result = valid_native_observation();
#define ADD_TOKEN(field) continuity_tokens[token_count++] = &(field)
    ADD_TOKEN(result.snapshot.process_token);
    ADD_TOKEN(result.snapshot.native_thread_token);
    ADD_TOKEN(result.snapshot.source_token);
    ADD_TOKEN(result.snapshot.world_token);
    ADD_TOKEN(result.snapshot.group_token);
    ADD_TOKEN(result.snapshot.formation_owner_token);
    ADD_TOKEN(result.snapshot.formation_token);
    ADD_TOKEN(result.snapshot.controller_token);
    ADD_TOKEN(result.snapshot.controller_callback_token);
    ADD_TOKEN(result.snapshot.transaction_token);
    ADD_TOKEN(result.snapshot.listener_storage_token);
    ADD_TOKEN(result.snapshot.listener_token);
    ADD_TOKEN(result.snapshot.ui_controller_token);
    ADD_TOKEN(result.snapshot.hud_owner_token);
    ADD_TOKEN(result.snapshot.ui_scene_token);
    ADD_TOKEN(result.snapshot.elco_arbiter_token);
    ADD_TOKEN(result.snapshot.front_actor_token);
    ADD_TOKEN(result.snapshot.camera_token);
    ADD_TOKEN(result.snapshot.current_render_camera_token);
    ADD_TOKEN(result.snapshot.render_state_token);
    ADD_TOKEN(result.snapshot.scene_manager_token);
    ADD_TOKEN(result.snapshot.scene_renderer_token);
#undef ADD_TOKEN
    for (index = 0u; index < token_count; ++index) {
        uint64_t saved = *continuity_tokens[index];

        *continuity_tokens[index] = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        *continuity_tokens[index] = saved;
    }
    result.snapshot.group_order[0] = 1u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.formation_order[1] = 1u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.group_count = 3u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.formation_count = 3u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.listener_count = 0u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.controller_current_mode = 2u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.controller_requested_mode = 2u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    result = valid_native_observation();
    result.snapshot.front_actor_token = result.snapshot.hero[1].actor_token;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    for (hero = 0u; hero < 4u; ++hero) {
        result = valid_native_observation();
        result.snapshot.hero[hero].actor_token = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].control_component_token = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].control_owner_actor_token++;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].formation_backpointer_token++;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].gizmo_token = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].stat_display_token = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].gizmo_label_hash = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].gizmo_label_length = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].fill_cache_secondary_bits++;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].native_ai_enabled ^= 1u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].human_control_owned ^= 1u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
        result = valid_native_observation();
        result.snapshot.hero[hero].control_mode = 0u;
        CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    }
    result = valid_native_observation();
    result.snapshot.hero[1].actor_token = result.snapshot.hero[0].gizmo_token;
    result.snapshot.hero[1].control_owner_actor_token =
        result.snapshot.hero[1].actor_token;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));

    /* A valid result is bound to the exact dispatch that produced it. It
     * cannot be replayed against another otherwise-exact witness. */
    result = valid_native_observation();
    result.witness_dispatch_serial++;
    CHECK(expect_invalid_observation(&result, &context.witness, 0, 0u));
    result = valid_native_observation();
    result.witness_native_thread_id++;
    CHECK(expect_invalid_observation(&result, &context.witness, 0, 0u));
    result = valid_native_observation();
    result.witness_observer_registry_generation++;
    CHECK(expect_invalid_observation(&result, &context.witness, 0, 0u));
    result = valid_native_observation();
    result.witness_dispatch_overlap_generation++;
    CHECK(expect_invalid_observation(&result, &context.witness, 0, 0u));
    result = valid_native_observation();
    result.witness_source =
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL;
    CHECK(expect_invalid_observation(&result, &context.witness, 0, 0u));
    result = valid_native_observation();
    witness = context.witness;
    witness.dispatch_serial++;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    witness = context.witness;
    witness.observer_registry_generation++;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    witness = context.witness;
    witness.dispatch_overlap_generation++;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));

    result = valid_native_observation();
    CHECK(expect_invalid_observation(&result, NULL, 0, 0u));
    witness = context.witness;
    witness.sole_observer = 0u;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    witness = context.witness;
    witness.native_thread_id++;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));
    witness = context.witness;
    witness.source = SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL;
    CHECK(expect_invalid_observation(&result, &witness, 0, 0u));

    SetLastError(0xd00du);
    CHECK(!SudekiMpTalosCompanionStagingResearchAdapterIngestNativeObservation(
        NULL, &context.witness));
    CHECK(GetLastError() == 0xd00du);
    CHECK(get_observation_status(&status));
    CHECK(status.observation_valid == 0u &&
        status.original.observation_serial == 0u);
    for (index = 0u; index < context.event_count; ++index) {
        CHECK(context.events[index] == EVENT_PUBLISH);
    }
    CHECK(context.remove_calls == 0u && context.add_calls == 0u &&
        context.destroy_calls == 0u && context.observed_witness == NULL);
    release_fixture(image);
    return 1;
}

static int test_uninstall_gate_retains_inactive_status(void) {
    MockContext context;
    SudekiMpTalosStagingResearchAdapterStatus status;
    uint8_t *image;
    unsigned int events_before;

    ZeroMemory(&context, sizeof(context));
    CHECK(install_mock(&context, &image));
    SudekiMpUninstallTalosCompanionStagingResearchAdapter();
    CHECK(SudekiMpTalosCompanionStagingResearchAdapterGetStatus(&status));
    CHECK(status.active == 0u && status.installed == 0u &&
        status.enabled == 0u);
    events_before = context.event_count;
    SetLastError(0xabcdu);
    SudekiMpTalosCompanionStagingResearchAdapterService(
        fake_pointer(0x10500u), fake_pointer(0x10b00u), &context.witness);
    CHECK(GetLastError() == 0xabcdu);
    CHECK(context.event_count == events_before);
    VirtualFree(image, 0u, MEM_RELEASE);
    return 1;
}

int main(void) {
    CHECK(test_default_off_and_install_gates());
    CHECK(test_happy_path_and_order());
    CHECK(test_preflight_failures_make_zero_native_calls());
    CHECK(test_wrapper_and_membership_failures());
    CHECK(test_every_narrow_field_at_every_transaction_stage());
    CHECK(test_transaction_failures_are_sticky());
    CHECK(test_native_observation_ingestion());
    CHECK(test_invalid_native_observation_contract());
    CHECK(test_uninstall_gate_retains_inactive_status());
    puts("talos companion staging research adapter tests passed");
    return 0;
}
