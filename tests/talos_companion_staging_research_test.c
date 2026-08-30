#include "engine/talos_companion_staging_research.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpTalosStagingResearchSnapshot snapshot(void) {
    static const uint8_t group[4] = {
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO
    };
    static const uint8_t formation[4] = {
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI
    };
    SudekiMpTalosStagingResearchSnapshot value;
    unsigned int i;

    memset(&value, 0, sizeof(value));
    value.observation_serial = 100u;
    value.process_token = UINT64_C(0x1001);
    value.native_thread_token = UINT64_C(0x1002);
    value.source_token = UINT64_C(0x1003);
    value.world_token = UINT64_C(0x1004);
    value.group_token = UINT64_C(0x1005);
    value.formation_owner_token = UINT64_C(0x1006);
    value.formation_token = UINT64_C(0x1007);
    value.controller_token = UINT64_C(0x1008);
    value.controller_callback_token = UINT64_C(0x1009);
    value.transaction_token = UINT64_C(0x100a);
    value.listener_storage_token = UINT64_C(0x100b);
    value.listener_token = UINT64_C(0x100c);
    value.ui_controller_token = UINT64_C(0x100d);
    value.hud_owner_token = UINT64_C(0x100e);
    value.ui_scene_token = UINT64_C(0x100f);
    value.elco_arbiter_token = UINT64_C(0x1010);
    value.camera_token = UINT64_C(0x1011);
    value.current_render_camera_token = UINT64_C(0x1012);
    value.render_state_token = UINT64_C(0x1013);
    value.scene_manager_token = UINT64_C(0x1014);
    value.scene_renderer_token = UINT64_C(0x1015);
    value.group_count = 4u;
    value.formation_count = 4u;
    value.exact_executable_hash = 1u;
    value.exact_sol_hash = 1u;
    value.foreground = 1u;
    value.all_pending_loaded = 1u;
    value.camera_scene_consistent = 1u;
    value.controller_callback_exact = 1u;
    value.game_thread_exact = 1u;
    value.transaction_exclusive = 1u;
    value.no_yield_window_exact = 1u;
    value.listener_callback_closure_exact = 1u;
    value.ui_hud_closure_exact = 1u;
    value.hero_hud_state_converged = 1u;
    value.elco_arbiter_safe = 1u;
    value.group_armed = 0u;
    value.listener_count = 1u;
    value.elco_arbiter_state_58 = 3u;
    value.elco_arbiter_flags_60_masked = 0u;
    value.controller_current_mode = 1u;
    value.controller_requested_mode = 1u;
    memcpy(value.group_order, group, sizeof(group));
    memcpy(value.formation_order, formation, sizeof(formation));
    for (i = 0u; i < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++i) {
        int tal = i == SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;

        value.hero[i].actor_token = UINT64_C(0x2000) + i;
        value.hero[i].control_component_token = UINT64_C(0x3000) + i;
        value.hero[i].control_owner_actor_token = value.hero[i].actor_token;
        value.hero[i].formation_backpointer_token = value.formation_token;
        value.hero[i].gizmo_token = UINT64_C(0x5000) + i;
        value.hero[i].stat_display_token = UINT64_C(0x6000) + i;
        value.hero[i].gizmo_label_hash = UINT64_C(0x7000) + i;
        value.hero[i].gizmo_state = 0x20u + i;
        value.hero[i].gizmo_flags_masked = 0x30u + i;
        value.hero[i].gizmo_label_length = 4u + i;
        value.hero[i].current_hp_bits = UINT32_C(0x42c80000) + i;
        value.hero[i].fill_cache_primary_bits =
            UINT32_C(0x3f000000) + i;
        value.hero[i].fill_cache_secondary_bits =
            value.hero[i].fill_cache_primary_bits;
        value.hero[i].native_ai_enabled = (uint8_t)(tal ? 0u : 1u);
        value.hero[i].human_control_owned = (uint8_t)(tal ? 1u : 0u);
        value.hero[i].control_mode = (uint8_t)(tal ?
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI);
    }
    value.hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO].wrapper_token =
        UINT64_C(0x4000);
    value.front_actor_token =
        value.hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL].actor_token;
    return value;
}

static SudekiMpTalosStagingResearchSnapshot later_full(
    const SudekiMpTalosStagingResearchSnapshot *original,
    uint64_t serial,
    int reload_required
) {
    SudekiMpTalosStagingResearchSnapshot value = *original;

    value.observation_serial = serial;
    value.reload_required = (uint8_t)(reload_required ? 1u : 0u);
    return value;
}

static SudekiMpTalosStagingResearchSnapshot detached(
    const SudekiMpTalosStagingResearchSnapshot *original,
    uint64_t serial
) {
    SudekiMpTalosStagingResearchSnapshot value =
        later_full(original, serial, 1);

    value.group_count = 3u;
    value.group_order[0] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;
    value.group_order[1] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH;
    value.group_order[2] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI;
    value.group_order[3] = SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE;
    value.formation_count = 3u;
    value.formation_order[0] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI;
    value.formation_order[1] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH;
    value.formation_order[2] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;
    value.formation_order[3] = SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE;
    value.hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO]
        .formation_backpointer_token = 0u;
    return value;
}

static void initialize_enabled(
    SudekiMpTalosCompanionStagingResearch *research
) {
    SudekiMpTalosCompanionStagingResearchInitialize(research);
    CHECK(research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED);
    CHECK(research->production_authority == 0u);
    CHECK(research->carry_authority == 0u);
    CHECK(research->actor_lifetime_authority == 0u);
    CHECK(research->reload_required == 0u);
    SudekiMpTalosCompanionStagingResearchConfigure(research, 1);
    CHECK(research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_IDLE);
}

static void begin_ready(
    SudekiMpTalosCompanionStagingResearch *research,
    SudekiMpTalosStagingResearchSnapshot *original
) {
    *original = snapshot();
    initialize_enabled(research);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(research, 1u,
        original) == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED);
}

static void claim_remove_ready(
    SudekiMpTalosCompanionStagingResearch *research,
    SudekiMpTalosStagingResearchSnapshot *original,
    SudekiMpTalosStagingResearchTicket *ticket
) {
    SudekiMpTalosStagingResearchSnapshot immediate;

    begin_ready(research, original);
    immediate = later_full(original, original->observation_serial + 1u, 0);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(research,
        &immediate, ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED);
}

static void reach_detached(
    SudekiMpTalosCompanionStagingResearch *research,
    SudekiMpTalosStagingResearchSnapshot *original,
    SudekiMpTalosStagingResearchSnapshot *detached_value
) {
    SudekiMpTalosStagingResearchTicket ticket;

    claim_remove_ready(research, original, &ticket);
    *detached_value = detached(original,
        ticket.authorized_observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(research,
        &ticket, detached_value, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED);
}

static void claim_add_ready(
    SudekiMpTalosCompanionStagingResearch *research,
    SudekiMpTalosStagingResearchSnapshot *original,
    SudekiMpTalosStagingResearchSnapshot *detached_value,
    SudekiMpTalosStagingResearchTicket *ticket
) {
    SudekiMpTalosStagingResearchSnapshot immediate;

    reach_detached(research, original, detached_value);
    immediate = detached(original, detached_value->observation_serial + 1u);
    immediate.formation_order[0] =
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH;
    immediate.formation_order[1] =
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;
    immediate.formation_order[2] =
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI;
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(research,
        &immediate, ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_AUTHORIZED);
}

static void test_default_off_happy_path_and_replay(void) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original = snapshot();
    SudekiMpTalosStagingResearchSnapshot immediate;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchSnapshot stable;
    SudekiMpTalosStagingResearchTicket remove_ticket;
    SudekiMpTalosStagingResearchTicket add_ticket;

    SudekiMpTalosCompanionStagingResearchInitialize(&research);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 1u,
        &original) == SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED_RESULT);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 1u,
        &original) == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, 101u, 0);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &remove_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED);
    CHECK(remove_ticket.native_function_rva ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_PLAYER_RVA);
    CHECK(remove_ticket.hero == SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO);
    CHECK(remove_ticket.wrapper_token == original.hero[3].wrapper_token);
    CHECK(remove_ticket.actor_token == original.hero[3].actor_token);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &remove_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
    detached_value = detached(&original, 102u);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
        &remove_ticket, &detached_value, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED);
    CHECK(research.reload_required == 1u);
    CHECK(research.detached.production_authority == 0u);
    CHECK(research.detached.carry_authority == 0u);
    CHECK(research.detached.actor_lifetime_authority == 0u);
    CHECK(research.detached.reload_required == 1u);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
        &remove_ticket, &detached_value, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
    immediate = detached(&original, 103u);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(&research,
        &immediate, &add_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_AUTHORIZED);
    CHECK(add_ticket.native_function_rva ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_PLAYER_RVA);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(&research,
        &immediate, &add_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
    restored = later_full(&original, 104u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &add_ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &add_ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
    stable = later_full(&original, 105u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchObserveStability(&research,
        &stable) == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS_RESULT);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS);
    CHECK(research.reload_required == 1u);
    CHECK(research.stable.production_authority == 0u);
    CHECK(research.stable.carry_authority == 0u);
    CHECK(research.stable.actor_lifetime_authority == 0u);
    CHECK(research.stable.reload_required == 1u);
    CHECK(SudekiMpTalosCompanionStagingResearchObserveStability(&research,
        &stable) == SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS);
    CHECK(research.enabled == 1u);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 2u,
        &original) == SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
}

static void test_modal_and_transition_are_diagnostic_only(void) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original = snapshot();
    SudekiMpTalosStagingResearchSnapshot immediate;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchSnapshot stable;
    SudekiMpTalosStagingResearchTicket remove_ticket;
    SudekiMpTalosStagingResearchTicket add_ticket;

    original.modal_active = UINT8_MAX;
    original.transition_active = 2u;
    initialize_enabled(&research);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 1u,
        &original) == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, 101u, 0);
    immediate.modal_active = 0u;
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &remove_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED);
    detached_value = detached(&original, 102u);
    detached_value.transition_active = UINT8_MAX;
    CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
        &remove_ticket, &detached_value, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED);
    immediate = detached(&original, 103u);
    immediate.modal_active = 0u;
    immediate.transition_active = 0u;
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(&research,
        &immediate, &add_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_AUTHORIZED);
    restored = later_full(&original, 104u, 1);
    restored.transition_active = 3u;
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &add_ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED);
    stable = later_full(&original, 105u, 1);
    stable.modal_active = 0u;
    CHECK(SudekiMpTalosCompanionStagingResearchObserveStability(&research,
        &stable) == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS_RESULT);
    CHECK(research.original.modal_active == UINT8_MAX);
    CHECK(research.detached.transition_active == UINT8_MAX);
    CHECK(research.stable.modal_active == 0u);
}

typedef void (*SnapshotMutation)(SudekiMpTalosStagingResearchSnapshot *value);

static void zero_process(SudekiMpTalosStagingResearchSnapshot *v) {
    v->process_token = 0u;
}
static void change_process(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->process_token;
}
static void change_thread(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->native_thread_token;
}
static void change_source(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->source_token;
}
static void change_world(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->world_token;
}
static void change_group_token(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->group_token;
}
static void change_formation_owner(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->formation_owner_token;
}
static void change_formation_token(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->formation_token;
}
static void change_controller(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->controller_token;
}
static void zero_controller_callback_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->controller_callback_token = 0u;
}
static void change_controller_callback_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->controller_callback_token;
}
static void zero_transaction_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->transaction_token = 0u;
}
static void change_transaction_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->transaction_token;
}
static void zero_listener_storage_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->listener_storage_token = 0u;
}
static void change_listener_storage_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->listener_storage_token;
}
static void zero_listener_token(SudekiMpTalosStagingResearchSnapshot *v) {
    v->listener_token = 0u;
}
static void change_listener_token(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->listener_token;
}
static void zero_ui_controller_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->ui_controller_token = 0u;
}
static void change_ui_controller_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->ui_controller_token;
}
static void zero_hud_owner_token(SudekiMpTalosStagingResearchSnapshot *v) {
    v->hud_owner_token = 0u;
}
static void change_hud_owner_token(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->hud_owner_token;
}
static void zero_ui_scene_token(SudekiMpTalosStagingResearchSnapshot *v) {
    v->ui_scene_token = 0u;
}
static void change_ui_scene_token(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->ui_scene_token;
}
static void zero_elco_arbiter_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->elco_arbiter_token = 0u;
}
static void change_elco_arbiter_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->elco_arbiter_token;
}
static void change_front_actor(SudekiMpTalosStagingResearchSnapshot *v) {
    v->front_actor_token = v->hero[1].actor_token;
}
static void change_camera(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->camera_token;
}
static void change_render_camera(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->current_render_camera_token;
}
static void change_render_state(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->render_state_token;
}
static void change_scene_manager(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->scene_manager_token;
}
static void change_scene_renderer(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->scene_renderer_token;
}
static void bad_controller_current(SudekiMpTalosStagingResearchSnapshot *v) {
    v->controller_current_mode = 0u;
}
static void bad_controller_requested(SudekiMpTalosStagingResearchSnapshot *v) {
    v->controller_requested_mode = 0u;
}
static void bad_camera_scene(SudekiMpTalosStagingResearchSnapshot *v) {
    v->camera_scene_consistent = 0u;
}
static void bad_controller_callback_exact(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->controller_callback_exact = 0u;
}
static void bad_game_thread_exact(SudekiMpTalosStagingResearchSnapshot *v) {
    v->game_thread_exact = 0u;
}
static void bad_transaction_exclusive(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->transaction_exclusive = 0u;
}
static void bad_no_yield_window_exact(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->no_yield_window_exact = 0u;
}
static void bad_listener_callback_closure_exact(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->listener_callback_closure_exact = 0u;
}
static void bad_ui_hud_closure_exact(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->ui_hud_closure_exact = 0u;
}
static void bad_hero_hud_state_converged(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero_hud_state_converged = 0u;
}
static void bad_elco_arbiter_safe(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->elco_arbiter_safe = 0u;
}
static void bad_exe(SudekiMpTalosStagingResearchSnapshot *v) {
    v->exact_executable_hash = 0u;
}
static void bad_sol(SudekiMpTalosStagingResearchSnapshot *v) {
    v->exact_sol_hash = 0u;
}
static void bad_foreground(SudekiMpTalosStagingResearchSnapshot *v) {
    v->foreground = 0u;
}
static void bad_pending(SudekiMpTalosStagingResearchSnapshot *v) {
    v->all_pending_loaded = 0u;
}
static void bad_combat(SudekiMpTalosStagingResearchSnapshot *v) {
    v->in_combat = 1u;
}
static void bad_async(SudekiMpTalosStagingResearchSnapshot *v) {
    v->async_active = 1u;
}
static void bad_tsa(SudekiMpTalosStagingResearchSnapshot *v) {
    v->tsa_active = 1u;
}
static void bad_paused(SudekiMpTalosStagingResearchSnapshot *v) {
    v->paused = 1u;
}
static void bad_listener_count(SudekiMpTalosStagingResearchSnapshot *v) {
    v->listener_count = 2u;
}
static void unsafe_elco_arbiter_state(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->elco_arbiter_state_58 = 2u;
}
static void change_elco_arbiter_state_safely(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->elco_arbiter_state_58 ^= UINT32_C(0x100);
}
static void bad_elco_arbiter_flags(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->elco_arbiter_flags_60_masked =
        SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
}
static void bad_elco_arbiter_unmasked_flags(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->elco_arbiter_flags_60_masked |= UINT32_C(0x04);
}
static void bad_group_armed(SudekiMpTalosStagingResearchSnapshot *v) {
    v->group_armed = 2u;
}
static void mismatch_group_armed_and_in_combat(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->group_armed = 1u;
    v->elco_arbiter_flags_60_masked =
        SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
}
static void mismatch_in_combat_and_group_armed(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->in_combat = 1u;
}
static void claim_production_authority(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->production_authority = 1u;
}
static void claim_carry_authority(SudekiMpTalosStagingResearchSnapshot *v) {
    v->carry_authority = 1u;
}
static void claim_lifetime_authority(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->actor_lifetime_authority = 1u;
}
static void premature_reload(SudekiMpTalosStagingResearchSnapshot *v) {
    v->reload_required = 1u;
}
static void missing_reload(SudekiMpTalosStagingResearchSnapshot *v) {
    v->reload_required = 0u;
}
static void wrong_group_order(SudekiMpTalosStagingResearchSnapshot *v) {
    v->group_order[1] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI;
    v->group_order[2] = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH;
}
static void duplicate_formation(SudekiMpTalosStagingResearchSnapshot *v) {
    v->formation_order[0] = v->formation_order[1];
}
static void wrong_formation_order(SudekiMpTalosStagingResearchSnapshot *v) {
    uint8_t temporary = v->formation_order[0];

    v->formation_order[0] = v->formation_order[1];
    v->formation_order[1] = temporary;
}
static void change_wrapper(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->hero[3].wrapper_token;
}
static void change_actor(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->hero[3].actor_token;
    v->hero[3].control_owner_actor_token = v->hero[3].actor_token;
}
static void change_control(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->hero[3].control_component_token;
}
static void change_control_owner(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->hero[3].control_owner_actor_token;
}
static void change_control_mode(SudekiMpTalosStagingResearchSnapshot *v) {
    v->hero[3].control_mode =
        SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN;
}
static unsigned int selected_hero;

static void zero_gizmo_token(SudekiMpTalosStagingResearchSnapshot *v) {
    v->hero[selected_hero].gizmo_token = 0u;
}
static void change_gizmo_token(SudekiMpTalosStagingResearchSnapshot *v) {
    v->hero[selected_hero].gizmo_token += UINT64_C(0x100);
}
static void duplicate_gizmo_token(SudekiMpTalosStagingResearchSnapshot *v) {
    v->hero[selected_hero].gizmo_token =
        v->hero[(selected_hero + 1u) %
            SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT].gizmo_token;
}
static void zero_stat_display_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[selected_hero].stat_display_token = 0u;
}
static void change_stat_display_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[selected_hero].stat_display_token += UINT64_C(0x100);
}
static void duplicate_stat_display_token(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[selected_hero].stat_display_token =
        v->hero[(selected_hero + 1u) %
            SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT].stat_display_token;
}
static void zero_gizmo_label_hash(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[selected_hero].gizmo_label_hash = 0u;
}
static void change_gizmo_label_hash(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].gizmo_label_hash;
}
static void change_gizmo_state(SudekiMpTalosStagingResearchSnapshot *v) {
    ++v->hero[selected_hero].gizmo_state;
}
static void change_gizmo_flags_masked(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].gizmo_flags_masked;
}
static void zero_gizmo_label_length(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[selected_hero].gizmo_label_length = 0u;
}
static void long_gizmo_label(SudekiMpTalosStagingResearchSnapshot *v) {
    v->hero[selected_hero].gizmo_label_length =
        SUDEKIMP_TALOS_STAGING_RESEARCH_HUD_LABEL_MAX_LENGTH + 1u;
}
static void change_gizmo_label_length(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].gizmo_label_length;
}
static void change_current_hp_bits(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].current_hp_bits;
}
static void change_primary_fill_only(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].fill_cache_primary_bits;
}
static void change_secondary_fill_only(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].fill_cache_secondary_bits;
}
static void change_fill_caches_safely(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[selected_hero].fill_cache_primary_bits;
    ++v->hero[selected_hero].fill_cache_secondary_bits;
}
static void change_survivor_backpointer(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    ++v->hero[2].formation_backpointer_token;
}
static void elco_backpointer_present(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[3].formation_backpointer_token = v->formation_token;
}
static void elco_backpointer_missing(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->hero[3].formation_backpointer_token = 0u;
}
static void partial_group_count(SudekiMpTalosStagingResearchSnapshot *v) {
    v->group_count = (uint8_t)(v->group_count == 3u ? 4u : 3u);
}
static void partial_formation_count(
    SudekiMpTalosStagingResearchSnapshot *v
) {
    v->formation_count = (uint8_t)(v->formation_count == 3u ? 4u : 3u);
}

static void expect_bad_preflight(SnapshotMutation mutation) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot bad = snapshot();

    mutation(&bad);
    initialize_enabled(&research);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 1u, &bad) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED);
    CHECK(research.one_attempt_consumed == 1u);
    CHECK(research.reload_required == 0u);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 1);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 2u, &bad) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY);
}

static void test_preflight_gates_consume_once(void) {
    static const SnapshotMutation mutations[] = {
        zero_process, change_front_actor, bad_controller_current,
        bad_controller_requested, bad_camera_scene, bad_exe, bad_sol,
        bad_foreground, bad_pending, bad_combat, bad_async, bad_tsa,
        bad_paused, zero_controller_callback_token, zero_transaction_token,
        zero_listener_storage_token, zero_listener_token,
        zero_ui_controller_token, zero_hud_owner_token, zero_ui_scene_token,
        zero_elco_arbiter_token, bad_controller_callback_exact,
        bad_game_thread_exact, bad_transaction_exclusive,
        bad_no_yield_window_exact, bad_listener_callback_closure_exact,
        bad_ui_hud_closure_exact, bad_hero_hud_state_converged,
        bad_elco_arbiter_safe, bad_listener_count,
        unsafe_elco_arbiter_state, bad_elco_arbiter_flags,
        bad_elco_arbiter_unmasked_flags, bad_group_armed,
        mismatch_group_armed_and_in_combat,
        mismatch_in_combat_and_group_armed,
        claim_production_authority,
        claim_carry_authority, claim_lifetime_authority, premature_reload,
        wrong_group_order, duplicate_formation, wrong_formation_order,
        change_control_owner,
        change_control_mode, change_survivor_backpointer
    };
    unsigned int i;

    for (i = 0u; i < sizeof(mutations) / sizeof(mutations[0]); ++i)
        expect_bad_preflight(mutations[i]);
    for (selected_hero = 0u;
         selected_hero < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT;
         ++selected_hero) {
        static const SnapshotMutation hero_mutations[] = {
            zero_gizmo_token, duplicate_gizmo_token,
            zero_stat_display_token, duplicate_stat_display_token,
            zero_gizmo_label_hash, zero_gizmo_label_length,
            long_gizmo_label, change_primary_fill_only,
            change_secondary_fill_only
        };
        unsigned int j;

        for (j = 0u;
             j < sizeof(hero_mutations) / sizeof(hero_mutations[0]); ++j)
            expect_bad_preflight(hero_mutations[j]);
    }
    {
        SudekiMpTalosCompanionStagingResearch research;
        SudekiMpTalosStagingResearchSnapshot good = snapshot();

        initialize_enabled(&research);
        CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 0u,
            &good) == SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED);
        CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED);
    }
}

static void expect_bad_immediate(SnapshotMutation mutation) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot immediate;
    SudekiMpTalosStagingResearchTicket ticket;

    begin_ready(&research, &original);
    immediate = later_full(&original, 101u, 0);
    mutation(&immediate);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED);
    CHECK(research.reload_required == 0u);
}

static void test_pre_remove_drift_and_zero_call_are_safe(void) {
    static const SnapshotMutation mutations[] = {
        change_process, change_thread, change_source, change_world,
        change_group_token, change_formation_owner, change_formation_token,
        change_controller, change_front_actor, change_camera,
        change_render_camera, change_render_state, change_scene_manager,
        change_scene_renderer, bad_controller_current,
        bad_controller_requested, bad_camera_scene, bad_foreground,
        bad_pending, bad_combat, bad_async, bad_tsa, bad_paused,
        claim_production_authority, claim_carry_authority,
        claim_lifetime_authority, premature_reload, wrong_group_order,
        wrong_formation_order, change_wrapper, change_actor, change_control,
        change_control_owner, change_control_mode,
        change_survivor_backpointer
    };
    unsigned int i;

    for (i = 0u; i < sizeof(mutations) / sizeof(mutations[0]); ++i)
        expect_bad_immediate(mutations[i]);
    {
        SudekiMpTalosCompanionStagingResearch research;
        SudekiMpTalosStagingResearchSnapshot original;
        SudekiMpTalosStagingResearchTicket ticket;

        claim_remove_ready(&research, &original, &ticket);
        CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
            NULL, NULL, 0u) ==
            SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED);
        CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED);
        CHECK(research.reload_required == 0u);
    }
}

typedef void (*TicketMutation)(SudekiMpTalosStagingResearchTicket *ticket);

static void ticket_attempt(SudekiMpTalosStagingResearchTicket *v) {
    ++v->attempt_serial;
}
static void ticket_authorization(SudekiMpTalosStagingResearchTicket *v) {
    ++v->authorization_serial;
}
static void ticket_observation(SudekiMpTalosStagingResearchTicket *v) {
    ++v->authorized_observation_serial;
}
static void ticket_process(SudekiMpTalosStagingResearchTicket *v) {
    ++v->process_token;
}
static void ticket_thread(SudekiMpTalosStagingResearchTicket *v) {
    ++v->native_thread_token;
}
static void ticket_source(SudekiMpTalosStagingResearchTicket *v) {
    ++v->source_token;
}
static void ticket_world(SudekiMpTalosStagingResearchTicket *v) {
    ++v->world_token;
}
static void ticket_group(SudekiMpTalosStagingResearchTicket *v) {
    ++v->group_token;
}
static void ticket_wrapper(SudekiMpTalosStagingResearchTicket *v) {
    ++v->wrapper_token;
}
static void ticket_actor(SudekiMpTalosStagingResearchTicket *v) {
    ++v->actor_token;
}
static void ticket_rva(SudekiMpTalosStagingResearchTicket *v) {
    v->native_function_rva = SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_PLAYER_RVA;
}
static void ticket_hero(SudekiMpTalosStagingResearchTicket *v) {
    v->hero = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI;
}
static void ticket_reserved(SudekiMpTalosStagingResearchTicket *v) {
    v->reserved[2] = 1u;
}

static void test_remove_ticket_tamper_and_partial_calls(void) {
    static const TicketMutation mutations[] = {
        ticket_attempt, ticket_authorization, ticket_observation,
        ticket_process, ticket_thread, ticket_source, ticket_world,
        ticket_group, ticket_wrapper, ticket_actor, ticket_rva, ticket_hero,
        ticket_reserved
    };
    unsigned int i;

    for (i = 0u; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        SudekiMpTalosCompanionStagingResearch research;
        SudekiMpTalosStagingResearchSnapshot original;
        SudekiMpTalosStagingResearchSnapshot completion;
        SudekiMpTalosStagingResearchTicket ticket;

        claim_remove_ready(&research, &original, &ticket);
        completion = detached(&original,
            ticket.authorized_observation_serial + 1u);
        mutations[i](&ticket);
        CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
            &ticket, &completion, 1u) ==
            SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
        CHECK(research.state ==
            SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
        CHECK(research.reload_required == 1u);
        SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
        SudekiMpTalosCompanionStagingResearchConfigure(&research, 1);
        CHECK(research.state ==
            SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
    }
    {
        SudekiMpTalosCompanionStagingResearch research;
        SudekiMpTalosStagingResearchSnapshot original;
        SudekiMpTalosStagingResearchSnapshot completion;
        SudekiMpTalosStagingResearchTicket ticket;

        claim_remove_ready(&research, &original, &ticket);
        completion = detached(&original,
            ticket.authorized_observation_serial + 1u);
        CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
            &ticket, &completion, 2u) ==
            SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
        CHECK(research.reload_required == 1u);
    }
}

static void expect_bad_detached(SnapshotMutation mutation) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot completion;
    SudekiMpTalosStagingResearchTicket ticket;

    claim_remove_ready(&research, &original, &ticket);
    completion = detached(&original,
        ticket.authorized_observation_serial + 1u);
    mutation(&completion);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
        &ticket, &completion, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
    CHECK(research.reload_required == 1u);
}

static void expect_bad_add_immediate(SnapshotMutation mutation) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot immediate;
    SudekiMpTalosStagingResearchTicket ticket;

    reach_detached(&research, &original, &detached_value);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    mutation(&immediate);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(&research,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
    CHECK(research.reload_required == 1u);
}

static void expect_bad_restored(SnapshotMutation mutation) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchTicket ticket;

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    mutation(&restored);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
    CHECK(research.reload_required == 1u);
}

static void expect_bad_stable(SnapshotMutation mutation) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchSnapshot stable;
    SudekiMpTalosStagingResearchTicket ticket;

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED);
    stable = later_full(&original, restored.observation_serial + 1u, 1);
    mutation(&stable);
    CHECK(SudekiMpTalosCompanionStagingResearchObserveStability(&research,
        &stable) == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
    CHECK(research.reload_required == 1u);
}

static void expect_bad_at_every_post_begin_stage(
    SnapshotMutation mutation
) {
    expect_bad_immediate(mutation);
    expect_bad_detached(mutation);
    expect_bad_add_immediate(mutation);
    expect_bad_restored(mutation);
    expect_bad_stable(mutation);
}

static void test_narrow_closure_and_hud_evidence_are_exact(void) {
    static const SnapshotMutation required_mutations[] = {
        bad_paused, bad_controller_callback_exact, bad_game_thread_exact,
        bad_transaction_exclusive, bad_no_yield_window_exact,
        bad_listener_callback_closure_exact, bad_ui_hud_closure_exact,
        bad_hero_hud_state_converged, bad_elco_arbiter_safe,
        bad_listener_count, unsafe_elco_arbiter_state,
        bad_elco_arbiter_flags, bad_elco_arbiter_unmasked_flags,
        bad_group_armed, mismatch_group_armed_and_in_combat,
        mismatch_in_combat_and_group_armed
    };
    static const SnapshotMutation token_and_arbiter_drift[] = {
        change_controller_callback_token, change_transaction_token,
        change_listener_storage_token, change_listener_token,
        change_ui_controller_token, change_hud_owner_token,
        change_ui_scene_token, change_elco_arbiter_token,
        change_elco_arbiter_state_safely
    };
    static const SnapshotMutation hero_drift[] = {
        change_gizmo_token, change_stat_display_token,
        change_gizmo_label_hash, change_gizmo_state,
        change_gizmo_flags_masked, change_gizmo_label_length,
        change_current_hp_bits, change_primary_fill_only,
        change_secondary_fill_only, change_fill_caches_safely
    };
    unsigned int i;

    for (i = 0u;
         i < sizeof(required_mutations) / sizeof(required_mutations[0]); ++i)
        expect_bad_at_every_post_begin_stage(required_mutations[i]);
    for (i = 0u;
         i < sizeof(token_and_arbiter_drift) /
            sizeof(token_and_arbiter_drift[0]); ++i)
        expect_bad_at_every_post_begin_stage(token_and_arbiter_drift[i]);
    for (selected_hero = 0u;
         selected_hero < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT;
         ++selected_hero) {
        for (i = 0u; i < sizeof(hero_drift) / sizeof(hero_drift[0]); ++i)
            expect_bad_at_every_post_begin_stage(hero_drift[i]);
    }
}

static void test_detached_exactness(void) {
    static const SnapshotMutation mutations[] = {
        change_process, change_thread, change_source, change_world,
        change_group_token, change_formation_owner, change_formation_token,
        change_controller, change_front_actor, change_camera,
        change_render_camera, change_render_state, change_scene_manager,
        change_scene_renderer, bad_controller_current,
        bad_controller_requested, bad_camera_scene, bad_exe, bad_sol,
        bad_foreground, bad_pending, bad_combat, bad_async, bad_tsa,
        bad_paused, claim_production_authority,
        claim_carry_authority, claim_lifetime_authority, missing_reload,
        wrong_group_order, duplicate_formation, change_wrapper, change_actor,
        change_control, change_control_owner, change_control_mode,
        change_survivor_backpointer, elco_backpointer_present,
        partial_group_count, partial_formation_count
    };
    unsigned int i;

    for (i = 0u; i < sizeof(mutations) / sizeof(mutations[0]); ++i)
        expect_bad_detached(mutations[i]);
}

static void test_add_claim_finish_and_restore_exactness(void) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot immediate;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchTicket ticket;

    reach_detached(&research, &original, &detached_value);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    change_camera(&immediate);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(&research,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 0u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 2u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    ticket.native_function_rva =
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_PLAYER_RVA;
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    wrong_formation_order(&restored);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    change_control(&restored);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    elco_backpointer_missing(&restored);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    missing_reload(&restored);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
}

static void test_stability_is_independent_and_exact(void) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchSnapshot stable;
    SudekiMpTalosStagingResearchTicket ticket;

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED);
    CHECK(SudekiMpTalosCompanionStagingResearchObserveStability(&research,
        &restored) == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);

    claim_add_ready(&research, &original, &detached_value, &ticket);
    restored = later_full(&original,
        ticket.authorized_observation_serial + 1u, 1);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishAdd(&research,
        &ticket, &restored, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED);
    stable = later_full(&original, restored.observation_serial + 1u, 1);
    change_render_state(&stable);
    CHECK(SudekiMpTalosCompanionStagingResearchObserveStability(&research,
        &stable) == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
}

static void test_serial_wrap_and_authorization_exhaustion(void) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original = snapshot();
    SudekiMpTalosStagingResearchSnapshot immediate;
    SudekiMpTalosStagingResearchSnapshot detached_value;
    SudekiMpTalosStagingResearchTicket ticket;

    original.observation_serial = UINT64_MAX;
    initialize_enabled(&research);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, UINT64_MAX,
        &original) == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, 1u, 0);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED);

    original = snapshot();
    original.observation_serial = 10u;
    initialize_enabled(&research);
    CHECK(SudekiMpTalosCompanionStagingResearchBegin(&research, 1u,
        &original) == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, UINT64_C(0x800000000000000a), 0);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED);

    begin_ready(&research, &original);
    research.next_authorization_serial = UINT64_MAX;
    immediate = later_full(&original, original.observation_serial + 1u, 0);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED);

    begin_ready(&research, &original);
    research.next_authorization_serial = UINT64_MAX - 1u;
    immediate = later_full(&original, original.observation_serial + 1u, 0);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimRemove(&research,
        &immediate, &ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED);
    CHECK(ticket.authorization_serial == UINT64_MAX);
    detached_value = detached(&original,
        ticket.authorized_observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingResearchFinishRemove(&research,
        &ticket, &detached_value, 1u) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingResearchClaimAdd(&research,
        &immediate, &ticket) ==
        SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE);
}

static void test_disable_boundaries(void) {
    SudekiMpTalosCompanionStagingResearch research;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchTicket ticket;

    initialize_enabled(&research);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED);
    CHECK(research.one_attempt_consumed == 0u);

    begin_ready(&research, &original);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED);
    CHECK(research.reload_required == 0u);

    claim_remove_ready(&research, &original, &ticket);
    SudekiMpTalosCompanionStagingResearchConfigure(&research, 0);
    CHECK(research.state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED);
    CHECK(research.reload_required == 1u);
}

int main(void) {
    test_default_off_happy_path_and_replay();
    test_modal_and_transition_are_diagnostic_only();
    test_preflight_gates_consume_once();
    test_pre_remove_drift_and_zero_call_are_safe();
    test_remove_ticket_tamper_and_partial_calls();
    test_narrow_closure_and_hud_evidence_are_exact();
    test_detached_exactness();
    test_add_claim_finish_and_restore_exactness();
    test_stability_is_independent_and_exact();
    test_serial_wrap_and_authorization_exhaustion();
    test_disable_boundaries();
    if (failures != 0) {
        fprintf(stderr, "talos_companion_staging_research_test: %d failure(s)\n",
            failures);
        return 1;
    }
    puts("talos_companion_staging_research_test: ok");
    return 0;
}
