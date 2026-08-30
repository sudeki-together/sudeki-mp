#include "engine/talos_companion_staging_research.h"

#include <stddef.h>
#include <string.h>

static int serial64_newer(uint64_t candidate, uint64_t previous) {
    uint64_t distance = candidate - previous;

    return candidate != 0u && distance != 0u &&
        distance < UINT64_C(0x8000000000000000);
}

static int authority_flags_valid(
    const SudekiMpTalosStagingResearchSnapshot *value,
    int reload_required
) {
    return value != NULL && value->production_authority == 0u &&
        value->carry_authority == 0u &&
        value->actor_lifetime_authority == 0u &&
        value->reload_required == (uint8_t)(reload_required ? 1u : 0u);
}

static int ordinary_world_discipline_valid(
    const SudekiMpTalosStagingResearchSnapshot *value
) {
    return value->exact_executable_hash == 1u &&
        value->exact_sol_hash == 1u && value->foreground == 1u &&
        value->all_pending_loaded == 1u &&
        value->camera_scene_consistent == 1u &&
        value->controller_current_mode == 1u &&
        value->controller_requested_mode == 1u &&
        value->in_combat == 0u && value->group_armed == 0u &&
        value->async_active == 0u &&
        value->tsa_active == 0u && value->paused == 0u;
}

static int exact_closure_valid(
    const SudekiMpTalosStagingResearchSnapshot *value
) {
    uint32_t arbiter_state;
    uint32_t arbiter_armed;

    if (value->controller_callback_exact != 1u ||
        value->game_thread_exact != 1u ||
        value->transaction_exclusive != 1u ||
        value->no_yield_window_exact != 1u ||
        value->listener_callback_closure_exact != 1u ||
        value->ui_hud_closure_exact != 1u ||
        value->hero_hud_state_converged != 1u ||
        value->elco_arbiter_safe != 1u || value->listener_count != 1u ||
        value->group_armed > 1u ||
        value->group_armed != value->in_combat ||
        (value->elco_arbiter_flags_60_masked &
            ~((uint32_t)SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK))
            != 0u) return 0;
    arbiter_state = value->elco_arbiter_state_58 & 0x0fu;
    arbiter_armed = value->elco_arbiter_flags_60_masked &
        SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK;
    if ((arbiter_armed != 0u) != (value->group_armed != 0u)) return 0;
    return !((arbiter_state == 2u && value->group_armed == 0u) ||
        (arbiter_state == 4u && value->group_armed != 0u));
}

static int continuity_tokens_valid(
    const SudekiMpTalosStagingResearchSnapshot *value
) {
    return value->observation_serial != 0u && value->process_token != 0u &&
        value->native_thread_token != 0u && value->source_token != 0u &&
        value->world_token != 0u && value->group_token != 0u &&
        value->formation_owner_token != 0u &&
        value->formation_token != 0u && value->controller_token != 0u &&
        value->controller_callback_token != 0u &&
        value->transaction_token != 0u &&
        value->listener_storage_token != 0u &&
        value->listener_token != 0u && value->ui_controller_token != 0u &&
        value->hud_owner_token != 0u && value->ui_scene_token != 0u &&
        value->elco_arbiter_token != 0u &&
        value->front_actor_token != 0u && value->camera_token != 0u &&
        value->current_render_camera_token != 0u &&
        value->render_state_token != 0u &&
        value->scene_manager_token != 0u &&
        value->scene_renderer_token != 0u;
}

static int hero_hud_evidence_valid(
    const SudekiMpTalosStagingResearchHeroEvidence *hero
) {
    return hero->gizmo_token != 0u && hero->stat_display_token != 0u &&
        hero->gizmo_label_hash != 0u && hero->gizmo_label_length != 0u &&
        hero->gizmo_label_length <=
            SUDEKIMP_TALOS_STAGING_RESEARCH_HUD_LABEL_MAX_LENGTH &&
        hero->fill_cache_primary_bits == hero->fill_cache_secondary_bits;
}

static int order_valid(
    const uint8_t order[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT],
    unsigned int count,
    uint8_t expected_mask
) {
    unsigned int i;
    uint8_t mask = 0u;

    if (order == NULL ||
        count > SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT) return 0;
    for (i = 0u; i < count; ++i) {
        if (order[i] >= SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT ||
            (mask & (uint8_t)(1u << order[i])) != 0u) return 0;
        mask |= (uint8_t)(1u << order[i]);
    }
    for (; i < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++i) {
        if (order[i] != SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE)
            return 0;
    }
    return mask == expected_mask;
}

static int hero_preflight_valid(
    const SudekiMpTalosStagingResearchHeroEvidence *hero,
    unsigned int index
) {
    int tal = index == SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL;
    int elco = index == SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO;

    return ((elco && hero->wrapper_token != 0u) ||
            (!elco && hero->wrapper_token == 0u)) &&
        hero->actor_token != 0u && hero->control_component_token != 0u &&
        hero->control_owner_actor_token == hero->actor_token &&
        hero->formation_backpointer_token != 0u &&
        hero_hud_evidence_valid(hero) &&
        hero->native_ai_enabled == (uint8_t)(tal ? 0u : 1u) &&
        hero->human_control_owned == (uint8_t)(tal ? 1u : 0u) &&
        hero->override_active == 0u &&
        hero->control_mode == (uint8_t)(tal ?
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI);
}

static int preflight_valid(
    const SudekiMpTalosStagingResearchSnapshot *value
) {
    static const uint8_t expected_group[4] = {
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO
    };
    static const uint8_t expected_formation[4] = {
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI
    };
    uint64_t formation_token;
    unsigned int i;
    unsigned int j;

    if (!authority_flags_valid(value, 0) ||
        !continuity_tokens_valid(value) ||
        !ordinary_world_discipline_valid(value) ||
        !exact_closure_valid(value) ||
        value->front_actor_token !=
            value->hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL].actor_token ||
        value->group_count != 4u || value->formation_count != 4u ||
        memcmp(value->group_order, expected_group,
            sizeof(expected_group)) != 0 ||
        memcmp(value->formation_order, expected_formation,
            sizeof(expected_formation)) != 0) return 0;
    formation_token = value->formation_token;
    for (i = 0u; i < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++i) {
        if (!hero_preflight_valid(&value->hero[i], i) ||
            value->hero[i].formation_backpointer_token != formation_token)
            return 0;
        for (j = i + 1u;
             j < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++j) {
            if (value->hero[i].actor_token == value->hero[j].actor_token ||
                value->hero[i].control_component_token ==
                    value->hero[j].control_component_token ||
                value->hero[i].gizmo_token ==
                    value->hero[j].gizmo_token ||
                value->hero[i].stat_display_token ==
                    value->hero[j].stat_display_token) return 0;
        }
    }
    return 1;
}

static int continuity_matches(
    const SudekiMpTalosStagingResearchSnapshot *original,
    const SudekiMpTalosStagingResearchSnapshot *current,
    int reload_required
) {
    return authority_flags_valid(current, reload_required) &&
        continuity_tokens_valid(current) &&
        ordinary_world_discipline_valid(current) &&
        exact_closure_valid(current) &&
        original->process_token == current->process_token &&
        original->native_thread_token == current->native_thread_token &&
        original->source_token == current->source_token &&
        original->world_token == current->world_token &&
        original->group_token == current->group_token &&
        original->formation_owner_token == current->formation_owner_token &&
        original->formation_token == current->formation_token &&
        original->controller_token == current->controller_token &&
        original->controller_callback_token ==
            current->controller_callback_token &&
        original->transaction_token == current->transaction_token &&
        original->listener_storage_token ==
            current->listener_storage_token &&
        original->listener_token == current->listener_token &&
        original->ui_controller_token == current->ui_controller_token &&
        original->hud_owner_token == current->hud_owner_token &&
        original->ui_scene_token == current->ui_scene_token &&
        original->elco_arbiter_token == current->elco_arbiter_token &&
        original->front_actor_token == current->front_actor_token &&
        original->camera_token == current->camera_token &&
        original->current_render_camera_token ==
            current->current_render_camera_token &&
        original->render_state_token == current->render_state_token &&
        original->scene_manager_token == current->scene_manager_token &&
        original->scene_renderer_token == current->scene_renderer_token &&
        original->listener_count == current->listener_count &&
        original->group_armed == current->group_armed &&
        original->elco_arbiter_state_58 ==
            current->elco_arbiter_state_58 &&
        original->elco_arbiter_flags_60_masked ==
            current->elco_arbiter_flags_60_masked;
}

static int hero_matches(
    const SudekiMpTalosStagingResearchHeroEvidence *original,
    const SudekiMpTalosStagingResearchHeroEvidence *current,
    int compare_formation_backpointer
) {
    return original->wrapper_token == current->wrapper_token &&
        original->actor_token == current->actor_token &&
        original->control_component_token ==
            current->control_component_token &&
        original->control_owner_actor_token ==
            current->control_owner_actor_token &&
        original->gizmo_token == current->gizmo_token &&
        original->stat_display_token == current->stat_display_token &&
        original->gizmo_label_hash == current->gizmo_label_hash &&
        original->gizmo_state == current->gizmo_state &&
        original->gizmo_flags_masked == current->gizmo_flags_masked &&
        original->gizmo_label_length == current->gizmo_label_length &&
        original->current_hp_bits == current->current_hp_bits &&
        original->fill_cache_primary_bits ==
            current->fill_cache_primary_bits &&
        original->fill_cache_secondary_bits ==
            current->fill_cache_secondary_bits &&
        (!compare_formation_backpointer ||
         original->formation_backpointer_token ==
             current->formation_backpointer_token) &&
        original->native_ai_enabled == current->native_ai_enabled &&
        original->human_control_owned == current->human_control_owned &&
        original->override_active == current->override_active &&
        original->control_mode == current->control_mode;
}

static int snapshot_core_matches(
    const SudekiMpTalosStagingResearchSnapshot *original,
    const SudekiMpTalosStagingResearchSnapshot *current,
    int compare_elco_backpointer,
    int reload_required
) {
    unsigned int i;

    if (!continuity_matches(original, current, reload_required) ||
        current->front_actor_token !=
            current->hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL].actor_token)
        return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT; ++i) {
        if (!hero_matches(&original->hero[i], &current->hero[i],
            i != SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO ||
                compare_elco_backpointer)) return 0;
    }
    return 1;
}

static int full_snapshot_matches(
    const SudekiMpTalosStagingResearchSnapshot *original,
    const SudekiMpTalosStagingResearchSnapshot *current,
    int reload_required
) {
    return snapshot_core_matches(original, current, 1, reload_required) &&
        current->group_count == original->group_count &&
        current->formation_count == original->formation_count &&
        memcmp(current->group_order, original->group_order,
            sizeof(original->group_order)) == 0 &&
        memcmp(current->formation_order, original->formation_order,
            sizeof(original->formation_order)) == 0;
}

static int detached_snapshot_valid(
    const SudekiMpTalosStagingResearchSnapshot *original,
    const SudekiMpTalosStagingResearchSnapshot *current
) {
    static const uint8_t expected_group[4] = {
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH,
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI,
        SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE
    };

    return snapshot_core_matches(original, current, 0, 1) &&
        current->hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO]
            .formation_backpointer_token == 0u &&
        current->group_count == 3u && current->formation_count == 3u &&
        memcmp(current->group_order, expected_group,
            sizeof(expected_group)) == 0 &&
        order_valid(current->formation_order, 3u, 0x07u);
}

static int ticket_matches(
    const SudekiMpTalosStagingResearchTicket *a,
    const SudekiMpTalosStagingResearchTicket *b
) {
    return a != NULL && b != NULL &&
        a->attempt_serial == b->attempt_serial &&
        a->authorization_serial == b->authorization_serial &&
        a->authorized_observation_serial == b->authorized_observation_serial &&
        a->process_token == b->process_token &&
        a->native_thread_token == b->native_thread_token &&
        a->source_token == b->source_token &&
        a->world_token == b->world_token &&
        a->group_token == b->group_token &&
        a->wrapper_token == b->wrapper_token &&
        a->actor_token == b->actor_token &&
        a->native_function_rva == b->native_function_rva &&
        a->hero == b->hero && a->reserved[0] == b->reserved[0] &&
        a->reserved[1] == b->reserved[1] &&
        a->reserved[2] == b->reserved[2];
}

static SudekiMpTalosCompanionStagingResearchResult consume_safely(
    SudekiMpTalosCompanionStagingResearch *research
) {
    research->one_attempt_consumed = 1u;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED;
}

static SudekiMpTalosCompanionStagingResearchResult quarantine(
    SudekiMpTalosCompanionStagingResearch *research
) {
    research->one_attempt_consumed = 1u;
    research->reload_required = 1u;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
}

static int next_ticket(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *snapshot,
    uint32_t rva,
    SudekiMpTalosStagingResearchTicket *ticket
) {
    const SudekiMpTalosStagingResearchHeroEvidence *elco;

    if (research->next_authorization_serial == UINT64_MAX) return 0;
    ++research->next_authorization_serial;
    elco = &research->original.hero[
        SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO];
    memset(ticket, 0, sizeof(*ticket));
    ticket->attempt_serial = research->attempt_serial;
    ticket->authorization_serial = research->next_authorization_serial;
    ticket->authorized_observation_serial = snapshot->observation_serial;
    ticket->process_token = snapshot->process_token;
    ticket->native_thread_token = snapshot->native_thread_token;
    ticket->source_token = snapshot->source_token;
    ticket->world_token = snapshot->world_token;
    ticket->group_token = snapshot->group_token;
    ticket->wrapper_token = elco->wrapper_token;
    ticket->actor_token = elco->actor_token;
    ticket->native_function_rva = rva;
    ticket->hero = SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO;
    return 1;
}

void SudekiMpTalosCompanionStagingResearchInitialize(
    SudekiMpTalosCompanionStagingResearch *research
) {
    if (research == NULL) return;
    memset(research, 0, sizeof(*research));
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED;
}

void SudekiMpTalosCompanionStagingResearchConfigure(
    SudekiMpTalosCompanionStagingResearch *research,
    int enabled
) {
    uint8_t requested;

    if (research == NULL) return;
    requested = enabled ? 1u : 0u;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED ||
        research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED ||
        research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS) return;
    if (requested == research->enabled) return;
    if (requested != 0u) {
        if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED &&
            research->one_attempt_consumed == 0u) {
            research->enabled = 1u;
            research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_IDLE;
        }
        return;
    }
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_IDLE) {
        research->enabled = 0u;
        research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED;
    } else if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT) {
        (void)consume_safely(research);
    } else {
        (void)quarantine(research);
    }
}

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchBegin(
    SudekiMpTalosCompanionStagingResearch *research,
    uint64_t attempt_serial,
    const SudekiMpTalosStagingResearchSnapshot *preflight
) {
    if (research == NULL)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID;
    if (research->enabled == 0u)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED_RESULT;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
    if (research->one_attempt_consumed != 0u ||
        research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED ||
        research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY;
    if (research->state != SUDEKIMP_TALOS_STAGING_RESEARCH_IDLE)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE;
    research->one_attempt_consumed = 1u;
    research->attempt_serial = attempt_serial;
    if (attempt_serial == 0u || !preflight_valid(preflight))
        return consume_safely(research);
    research->original = *preflight;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED;
}

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchClaimRemove(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *immediate,
    SudekiMpTalosStagingResearchTicket *ticket
) {
    if (research == NULL)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_ISSUED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY;
    if (research->state != SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE;
    if (ticket == NULL || immediate == NULL ||
        !serial64_newer(immediate->observation_serial,
            research->original.observation_serial) ||
        !full_snapshot_matches(&research->original, immediate, 0) ||
        !next_ticket(research, immediate,
            SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_PLAYER_RVA,
            &research->remove_ticket)) return consume_safely(research);
    *ticket = research->remove_ticket;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_ISSUED;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED;
}

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchFinishRemove(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchTicket *ticket,
    const SudekiMpTalosStagingResearchSnapshot *completion,
    unsigned int native_call_count
) {
    if (research == NULL)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
    if (research->state != SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_ISSUED) {
        if (ticket != NULL && ticket->authorization_serial != 0u &&
            ticket->authorization_serial ==
                research->consumed_remove_authorization_serial)
            return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY;
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE;
    }
    if (native_call_count == 0u) return consume_safely(research);
    research->reload_required = 1u;
    if (native_call_count != 1u ||
        !ticket_matches(ticket, &research->remove_ticket))
        return quarantine(research);
    research->consumed_remove_authorization_serial =
        ticket->authorization_serial;
    if (completion == NULL ||
        !serial64_newer(completion->observation_serial,
            ticket->authorized_observation_serial) ||
        !detached_snapshot_valid(&research->original, completion))
        return quarantine(research);
    research->detached = *completion;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_DETACHED;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED;
}

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchClaimAdd(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *immediate,
    SudekiMpTalosStagingResearchTicket *ticket
) {
    if (research == NULL)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_ISSUED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY;
    if (research->state != SUDEKIMP_TALOS_STAGING_RESEARCH_DETACHED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE;
    if (ticket == NULL || immediate == NULL ||
        !serial64_newer(immediate->observation_serial,
            research->detached.observation_serial) ||
        !detached_snapshot_valid(&research->original, immediate) ||
        !next_ticket(research, immediate,
            SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_PLAYER_RVA,
            &research->add_ticket)) return quarantine(research);
    *ticket = research->add_ticket;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_ISSUED;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_AUTHORIZED;
}

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchFinishAdd(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchTicket *ticket,
    const SudekiMpTalosStagingResearchSnapshot *completion,
    unsigned int native_call_count
) {
    if (research == NULL)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
    if (research->state != SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_ISSUED) {
        if (ticket != NULL && ticket->authorization_serial != 0u &&
            ticket->authorization_serial ==
                research->consumed_add_authorization_serial)
            return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY;
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE;
    }
    if (native_call_count != 1u ||
        !ticket_matches(ticket, &research->add_ticket))
        return quarantine(research);
    research->consumed_add_authorization_serial = ticket->authorization_serial;
    if (completion == NULL ||
        !serial64_newer(completion->observation_serial,
            ticket->authorized_observation_serial) ||
        !full_snapshot_matches(&research->original, completion, 1))
        return quarantine(research);
    research->restored = *completion;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORED;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED;
}

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchObserveStability(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *observation
) {
    if (research == NULL)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE;
    if (research->state == SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY;
    if (research->state != SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORED)
        return SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE;
    if (observation == NULL ||
        !serial64_newer(observation->observation_serial,
            research->restored.observation_serial) ||
        !full_snapshot_matches(&research->original, observation, 1))
        return quarantine(research);
    research->stable = *observation;
    research->reload_required = 1u;
    research->state = SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS;
    return SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS_RESULT;
}
