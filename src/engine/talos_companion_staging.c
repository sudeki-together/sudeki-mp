#include "engine/talos_companion_staging.h"

#include <stddef.h>
#include <string.h>

static int serial32_newer(uint32_t candidate, uint32_t previous) {
    uint32_t distance = candidate - previous;

    return candidate != 0u && distance != 0u &&
        distance < UINT32_C(0x80000000);
}

static int serial64_newer(uint64_t candidate, uint64_t previous) {
    uint64_t distance = candidate - previous;

    return candidate != 0u && distance != 0u &&
        distance < UINT64_C(0x8000000000000000);
}

static int provenance_valid(const SudekiMpTalosEncounterProvenance *value) {
    unsigned int i;
    unsigned int j;
    uint8_t seen_heroes = 0u;
    uint8_t seen_controllers = 0u;

    if (value == NULL || value->serial == 0u ||
        value->transition_serial == 0u || value->world_generation == 0u ||
        value->source_generation == 0u || value->host_actor == 0u ||
        value->host_actor_generation == 0u ||
        value->host_lease_generation == 0u || value->combatant_count != 4u ||
        value->talos_health_target != 180000u ||
        (value->active_human_mask & (uint8_t)~0x0fu) != 0u ||
        (value->active_human_mask & 0x01u) == 0u ||
        value->hero_by_seat[0] != SUDEKIMP_TALOS_HERO_TAL) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (value->hero_actor_identity[i] == 0u ||
            value->hero_actor_generation[i] == 0u) return 0;
        for (j = i + 1u; j < SUDEKIMP_TALOS_HERO_COUNT; ++j) {
            if (value->hero_actor_identity[i] ==
                value->hero_actor_identity[j]) return 0;
        }
    }
    if (value->hero_actor_identity[SUDEKIMP_TALOS_HERO_TAL] !=
            value->host_actor ||
        value->hero_actor_generation[SUDEKIMP_TALOS_HERO_TAL] !=
            value->host_actor_generation) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_SEAT_COUNT; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        uint8_t hero = value->hero_by_seat[i];
        uint8_t controller = value->controller_slot_by_seat[i];

        if ((value->active_human_mask & bit) == 0u) {
            if (hero != SUDEKIMP_TALOS_HERO_NONE ||
                controller != SUDEKIMP_TALOS_CONTROLLER_NONE ||
                value->input_identity_by_seat[i] != 0u ||
                value->input_generation_by_seat[i] != 0u) return 0;
            continue;
        }
        if (hero >= SUDEKIMP_TALOS_HERO_COUNT ||
            (seen_heroes & (uint8_t)(1u << hero)) != 0u ||
            (i != 0u && hero == SUDEKIMP_TALOS_HERO_TAL) ||
            value->input_identity_by_seat[i] == 0u ||
            value->input_generation_by_seat[i] == 0u) return 0;
        for (j = 0u; j < i; ++j) {
            if ((value->active_human_mask & (uint8_t)(1u << j)) != 0u &&
                value->input_identity_by_seat[j] ==
                    value->input_identity_by_seat[i]) return 0;
        }
        if ((i == 0u && controller != SUDEKIMP_TALOS_CONTROLLER_NONE) ||
            (i != 0u && (controller >= 4u ||
             (seen_controllers & (uint8_t)(1u << controller)) != 0u)))
            return 0;
        if (i != 0u) seen_controllers |= (uint8_t)(1u << controller);
        seen_heroes |= (uint8_t)(1u << hero);
    }
    return 1;
}

static int provenance_matches(
    const SudekiMpTalosEncounterProvenance *a,
    const SudekiMpTalosEncounterProvenance *b
) {
    unsigned int i;

    if (!provenance_valid(a) || !provenance_valid(b) ||
        a->serial != b->serial ||
        a->transition_serial != b->transition_serial ||
        a->world_generation != b->world_generation ||
        a->source_generation != b->source_generation ||
        a->host_actor != b->host_actor ||
        a->host_actor_generation != b->host_actor_generation ||
        a->host_lease_generation != b->host_lease_generation ||
        a->talos_health_target != b->talos_health_target ||
        a->active_human_mask != b->active_human_mask ||
        a->combatant_count != b->combatant_count) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (a->hero_actor_identity[i] != b->hero_actor_identity[i] ||
            a->hero_actor_generation[i] != b->hero_actor_generation[i] ||
            a->hero_by_seat[i] != b->hero_by_seat[i] ||
            a->controller_slot_by_seat[i] != b->controller_slot_by_seat[i] ||
            a->input_identity_by_seat[i] != b->input_identity_by_seat[i] ||
            a->input_generation_by_seat[i] !=
                b->input_generation_by_seat[i]) return 0;
    }
    return 1;
}

static int hero_is_human(
    const SudekiMpTalosEncounterProvenance *provenance,
    unsigned int hero
) {
    unsigned int seat;

    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        if ((provenance->active_human_mask & (uint8_t)(1u << seat)) != 0u &&
            provenance->hero_by_seat[seat] == hero) return 1;
    }
    return 0;
}

static int order_valid(const uint8_t order[SUDEKIMP_TALOS_HERO_COUNT],
    unsigned int count, uint8_t expected_mask) {
    unsigned int i;
    uint8_t mask = 0u;

    if (order == NULL || count > SUDEKIMP_TALOS_HERO_COUNT) return 0;
    for (i = 0u; i < count; ++i) {
        if (order[i] >= SUDEKIMP_TALOS_HERO_COUNT ||
            (mask & (uint8_t)(1u << order[i])) != 0u) return 0;
        mask |= (uint8_t)(1u << order[i]);
    }
    for (; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (order[i] != SUDEKIMP_TALOS_STAGING_MEMBER_NONE) return 0;
    }
    return mask == expected_mask;
}

static int hero_record_valid(
    const SudekiMpTalosStagingHeroRecord *record,
    const SudekiMpTalosEncounterProvenance *provenance,
    unsigned int hero
) {
    int human = hero_is_human(provenance, hero);
    uint8_t expected_override = (uint8_t)(human &&
        hero != SUDEKIMP_TALOS_HERO_TAL ? 1u : 0u);

    return ((hero == SUDEKIMP_TALOS_HERO_ELCO &&
             record->wrapper_identity != 0u &&
             record->wrapper_generation != 0u) ||
            (hero != SUDEKIMP_TALOS_HERO_ELCO &&
             record->wrapper_identity == 0u &&
             record->wrapper_generation == 0u)) &&
        record->actor_identity == provenance->hero_actor_identity[hero] &&
        record->actor_generation == provenance->hero_actor_generation[hero] &&
        record->formation_backpointer_identity != 0u &&
        record->formation_backpointer_generation != 0u &&
        record->control_owner_identity != 0u &&
        record->control_owner_generation != 0u &&
        record->override_active == expected_override &&
        record->human_control_owned == (uint8_t)(human ? 1u : 0u) &&
        record->native_ai_enabled == (uint8_t)(human ? 0u : 1u) &&
        record->control_mode == (uint8_t)(human ?
            SUDEKIMP_TALOS_STAGING_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_CONTROL_NATIVE_AI);
}

static int preflight_valid(
    const SudekiMpTalosStagingSnapshot *value,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    static const uint8_t group[SUDEKIMP_TALOS_HERO_COUNT] = {
        SUDEKIMP_TALOS_HERO_TAL,
        SUDEKIMP_TALOS_HERO_AILISH,
        SUDEKIMP_TALOS_HERO_BUKI,
        SUDEKIMP_TALOS_HERO_ELCO
    };
    unsigned int i;
    unsigned int j;

    if (value == NULL || value->observation_serial == 0u ||
        value->continuity_fingerprint == 0u ||
        value->world_generation != provenance->world_generation ||
        value->source_generation != provenance->source_generation ||
        value->runtime_generation == 0u ||
        value->native_thread_identity == 0u ||
        value->group_identity == 0u || value->group_generation == 0u ||
        value->formation_owner_identity == 0u ||
        value->formation_owner_generation == 0u ||
        value->formation_identity == 0u ||
        value->formation_generation == 0u ||
        value->tal_controller_identity == 0u ||
        value->tal_controller_generation == 0u ||
        value->front_actor_identity != provenance->host_actor ||
        value->front_actor_generation != provenance->host_actor_generation ||
        value->camera_identity == 0u || value->camera_generation == 0u ||
        value->camera_target_actor_identity != provenance->host_actor ||
        value->camera_target_actor_generation !=
            provenance->host_actor_generation ||
        value->ownership_identity == 0u ||
        value->ownership_generation == 0u ||
        value->group_count != 4u || value->formation_count != 4u ||
        !value->exact_executable || !value->exact_asset ||
        !value->ownership_frozen || value->in_combat || value->tsa_active ||
        value->transition_active || value->modal_active ||
        memcmp(value->group_order, group, sizeof(group)) != 0 ||
        !order_valid(value->formation_order, 4u, 0x0fu) ||
        value->formation_order[0] != SUDEKIMP_TALOS_HERO_TAL ||
        value->formation_order[0] == SUDEKIMP_TALOS_HERO_ELCO) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (!hero_record_valid(&value->hero[i], provenance, i) ||
            value->hero[i].formation_backpointer_identity !=
                value->formation_identity ||
            value->hero[i].formation_backpointer_generation !=
                value->formation_generation) return 0;
        for (j = i + 1u; j < SUDEKIMP_TALOS_HERO_COUNT; ++j) {
            if (value->hero[i].actor_identity ==
                    value->hero[j].actor_identity ||
                value->hero[i].control_owner_identity ==
                    value->hero[j].control_owner_identity) return 0;
        }
    }
    return 1;
}

static int hero_record_matches(
    const SudekiMpTalosStagingHeroRecord *a,
    const SudekiMpTalosStagingHeroRecord *b,
    int compare_ai
) {
    return a->wrapper_identity == b->wrapper_identity &&
        a->wrapper_generation == b->wrapper_generation &&
        a->actor_identity == b->actor_identity &&
        a->actor_generation == b->actor_generation &&
        (!compare_ai ||
         (a->formation_backpointer_identity ==
              b->formation_backpointer_identity &&
          a->formation_backpointer_generation ==
              b->formation_backpointer_generation)) &&
        a->control_owner_identity == b->control_owner_identity &&
        a->control_owner_generation == b->control_owner_generation &&
        a->native_ai_enabled == b->native_ai_enabled &&
        a->human_control_owned == b->human_control_owned &&
        a->override_active == b->override_active &&
        a->control_mode == b->control_mode;
}

static int snapshot_core_matches(
    const SudekiMpTalosStagingSnapshot *original,
    const SudekiMpTalosStagingSnapshot *current,
    int compare_elco_ai
) {
    unsigned int i;

    if (original == NULL || current == NULL ||
        original->continuity_fingerprint != current->continuity_fingerprint ||
        original->world_generation != current->world_generation ||
        original->source_generation != current->source_generation ||
        original->runtime_generation != current->runtime_generation ||
        original->native_thread_identity != current->native_thread_identity ||
        original->group_identity != current->group_identity ||
        original->group_generation != current->group_generation ||
        original->formation_owner_identity !=
            current->formation_owner_identity ||
        original->formation_owner_generation !=
            current->formation_owner_generation ||
        original->formation_identity != current->formation_identity ||
        original->formation_generation != current->formation_generation ||
        original->tal_controller_identity != current->tal_controller_identity ||
        original->tal_controller_generation !=
            current->tal_controller_generation ||
        original->front_actor_identity != current->front_actor_identity ||
        original->front_actor_generation != current->front_actor_generation ||
        original->camera_identity != current->camera_identity ||
        original->camera_generation != current->camera_generation ||
        original->camera_target_actor_identity !=
            current->camera_target_actor_identity ||
        original->camera_target_actor_generation !=
            current->camera_target_actor_generation ||
        original->ownership_identity != current->ownership_identity ||
        original->ownership_generation != current->ownership_generation ||
        !current->exact_executable || !current->exact_asset ||
        !current->ownership_frozen || current->in_combat ||
        current->tsa_active || current->transition_active ||
        current->modal_active) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (!hero_record_matches(&original->hero[i], &current->hero[i],
            i != SUDEKIMP_TALOS_HERO_ELCO || compare_elco_ai)) return 0;
    }
    return 1;
}

static int full_snapshot_matches(
    const SudekiMpTalosStagingSnapshot *original,
    const SudekiMpTalosStagingSnapshot *current
) {
    return snapshot_core_matches(original, current, 1) &&
        original->group_count == current->group_count &&
        original->formation_count == current->formation_count &&
        memcmp(original->group_order, current->group_order,
            sizeof(original->group_order)) == 0 &&
        memcmp(original->formation_order, current->formation_order,
            sizeof(original->formation_order)) == 0;
}

static int detached_snapshot_valid(
    const SudekiMpTalosStagingSnapshot *original,
    const SudekiMpTalosStagingSnapshot *current
) {
    static const uint8_t group[SUDEKIMP_TALOS_HERO_COUNT] = {
        SUDEKIMP_TALOS_HERO_TAL,
        SUDEKIMP_TALOS_HERO_AILISH,
        SUDEKIMP_TALOS_HERO_BUKI,
        SUDEKIMP_TALOS_STAGING_MEMBER_NONE
    };

    return snapshot_core_matches(original, current, 0) &&
        current->hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_identity ==
            0u &&
        current->hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_generation ==
            0u &&
        current->group_count == 3u && current->formation_count == 3u &&
        memcmp(current->group_order, group, sizeof(group)) == 0 &&
        order_valid(current->formation_order, 3u, 0x07u);
}

static int ticket_matches(
    const SudekiMpTalosStagingTicket *a,
    const SudekiMpTalosStagingTicket *b
) {
    return a != NULL && b != NULL &&
        a->operation_serial == b->operation_serial &&
        a->authorization_serial == b->authorization_serial &&
        a->authorized_observation_serial == b->authorized_observation_serial &&
        a->encounter_serial == b->encounter_serial &&
        a->transition_serial == b->transition_serial &&
        a->world_generation == b->world_generation &&
        a->source_generation == b->source_generation &&
        a->runtime_generation == b->runtime_generation &&
        a->native_function_rva == b->native_function_rva &&
        a->native_thread_identity == b->native_thread_identity &&
        a->wrapper_identity == b->wrapper_identity &&
        a->wrapper_generation == b->wrapper_generation &&
        a->actor_identity == b->actor_identity &&
        a->actor_generation == b->actor_generation && a->hero == b->hero;
}

static void mark_terminal(
    SudekiMpTalosCompanionStaging *staging,
    SudekiMpTalosCompanionStagingState state
) {
    if (staging->highest_terminal_operation_serial == 0u ||
        serial64_newer(staging->operation_serial,
            staging->highest_terminal_operation_serial))
        staging->highest_terminal_operation_serial = staging->operation_serial;
    staging->terminal_encounter_serial = staging->provenance.serial;
    staging->terminal_transition_serial = staging->provenance.transition_serial;
    staging->terminal_world_generation = staging->provenance.world_generation;
    staging->terminal_source_generation = staging->provenance.source_generation;
    staging->state = state;
}

static SudekiMpTalosCompanionStagingResult quarantine(
    SudekiMpTalosCompanionStaging *staging
) {
    mark_terminal(staging, SUDEKIMP_TALOS_STAGING_QUARANTINED);
    return SUDEKIMP_TALOS_STAGING_QUARANTINE;
}

static int replay_fenced(
    const SudekiMpTalosCompanionStaging *staging,
    uint64_t operation_serial,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    if (operation_serial == 0u ||
        (staging->highest_terminal_operation_serial != 0u &&
         !serial64_newer(operation_serial,
            staging->highest_terminal_operation_serial)))
        return 1;
    return staging->terminal_transition_serial != 0u &&
        provenance->world_generation == staging->terminal_world_generation &&
        provenance->source_generation == staging->terminal_source_generation &&
        (!serial32_newer(provenance->transition_serial,
            staging->terminal_transition_serial) ||
         !serial32_newer(provenance->serial,
            staging->terminal_encounter_serial));
}

static int next_ticket(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosStagingSnapshot *snapshot,
    uint32_t rva,
    SudekiMpTalosStagingTicket *ticket
) {
    const SudekiMpTalosStagingHeroRecord *elco;

    if (staging->next_authorization_serial == UINT64_MAX) return 0;
    ++staging->next_authorization_serial;
    elco = &staging->original.hero[SUDEKIMP_TALOS_HERO_ELCO];
    memset(ticket, 0, sizeof(*ticket));
    ticket->operation_serial = staging->operation_serial;
    ticket->authorization_serial = staging->next_authorization_serial;
    ticket->authorized_observation_serial = snapshot->observation_serial;
    ticket->encounter_serial = staging->provenance.serial;
    ticket->transition_serial = staging->provenance.transition_serial;
    ticket->world_generation = snapshot->world_generation;
    ticket->source_generation = snapshot->source_generation;
    ticket->runtime_generation = snapshot->runtime_generation;
    ticket->native_function_rva = rva;
    ticket->native_thread_identity = snapshot->native_thread_identity;
    ticket->wrapper_identity = elco->wrapper_identity;
    ticket->wrapper_generation = elco->wrapper_generation;
    ticket->actor_identity = elco->actor_identity;
    ticket->actor_generation = elco->actor_generation;
    ticket->hero = SUDEKIMP_TALOS_HERO_ELCO;
    return 1;
}

void SudekiMpTalosCompanionStagingInitialize(
    SudekiMpTalosCompanionStaging *staging
) {
    if (staging == NULL) return;
    memset(staging, 0, sizeof(*staging));
    staging->state = SUDEKIMP_TALOS_STAGING_DISABLED;
}

void SudekiMpTalosCompanionStagingConfigure(
    SudekiMpTalosCompanionStaging *staging,
    int enabled
) {
    uint8_t requested;

    if (staging == NULL) return;
    requested = enabled ? 1u : 0u;
    if (requested == staging->enabled) return;
    if (staging->state == SUDEKIMP_TALOS_STAGING_QUARANTINED) return;
    if (requested) {
        if (staging->state == SUDEKIMP_TALOS_STAGING_DISABLED) {
            staging->enabled = 1u;
            staging->state = SUDEKIMP_TALOS_STAGING_IDLE;
        }
        return;
    }
    if (staging->state == SUDEKIMP_TALOS_STAGING_REMOVE_TICKET_ISSUED ||
        staging->state == SUDEKIMP_TALOS_STAGING_DETACHED_PROVEN ||
        staging->state == SUDEKIMP_TALOS_STAGING_RESTORE_TICKET_ISSUED ||
        staging->state == SUDEKIMP_TALOS_STAGING_RESTORED_PROVEN) {
        (void)quarantine(staging);
        return;
    }
    if (staging->state == SUDEKIMP_TALOS_STAGING_PREFLIGHT_PROVEN ||
        staging->state == SUDEKIMP_TALOS_STAGING_STABILITY_PROVEN)
        mark_terminal(staging, SUDEKIMP_TALOS_STAGING_RELEASED);
    staging->enabled = 0u;
    staging->state = SUDEKIMP_TALOS_STAGING_DISABLED;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingBegin(
    SudekiMpTalosCompanionStaging *staging,
    uint64_t operation_serial,
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosStagingSnapshot *preflight
) {
    if (staging == NULL || !staging->enabled)
        return SUDEKIMP_TALOS_STAGING_DISABLED_RESULT;
    if (staging->state == SUDEKIMP_TALOS_STAGING_QUARANTINED)
        return SUDEKIMP_TALOS_STAGING_QUARANTINE;
    if (staging->state != SUDEKIMP_TALOS_STAGING_IDLE &&
        staging->state != SUDEKIMP_TALOS_STAGING_RELEASED)
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    if (!provenance_valid(provenance))
        return SUDEKIMP_TALOS_STAGING_SAFE_ABORT;
    if (replay_fenced(staging, operation_serial, provenance))
        return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
    if (!preflight_valid(preflight, provenance))
        return SUDEKIMP_TALOS_STAGING_SAFE_ABORT;
    staging->operation_serial = operation_serial;
    staging->provenance = *provenance;
    staging->original = *preflight;
    memset(&staging->detached, 0, sizeof(staging->detached));
    memset(&staging->restored, 0, sizeof(staging->restored));
    memset(&staging->remove_ticket, 0, sizeof(staging->remove_ticket));
    memset(&staging->restore_ticket, 0, sizeof(staging->restore_ticket));
    staging->state = SUDEKIMP_TALOS_STAGING_PREFLIGHT_PROVEN;
    return SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingClaimRemove(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *immediate,
    SudekiMpTalosStagingTicket *ticket
) {
    if (staging == NULL || ticket == NULL)
        return SUDEKIMP_TALOS_STAGING_REJECTED_INVALID;
    if (staging->state == SUDEKIMP_TALOS_STAGING_REMOVE_TICKET_ISSUED)
        return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
    if (staging->state != SUDEKIMP_TALOS_STAGING_PREFLIGHT_PROVEN)
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    if (!provenance_matches(&staging->provenance, current) ||
        immediate == NULL ||
        !serial64_newer(immediate->observation_serial,
            staging->original.observation_serial) ||
        !full_snapshot_matches(&staging->original, immediate) ||
        !next_ticket(staging, immediate,
            SUDEKIMP_TALOS_STAGING_REMOVE_PLAYER_RVA,
            &staging->remove_ticket)) {
        mark_terminal(staging, SUDEKIMP_TALOS_STAGING_RELEASED);
        return SUDEKIMP_TALOS_STAGING_SAFE_ABORT;
    }
    *ticket = staging->remove_ticket;
    staging->state = SUDEKIMP_TALOS_STAGING_REMOVE_TICKET_ISSUED;
    return SUDEKIMP_TALOS_STAGING_REMOVE_AUTHORIZED;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingFinishRemove(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosStagingTicket *ticket,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *completion,
    unsigned int original_call_count
) {
    if (staging == NULL || ticket == NULL)
        return SUDEKIMP_TALOS_STAGING_REJECTED_INVALID;
    if (staging->state != SUDEKIMP_TALOS_STAGING_REMOVE_TICKET_ISSUED) {
        if (ticket->authorization_serial != 0u &&
            ticket->authorization_serial ==
                staging->consumed_remove_authorization_serial)
            return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    }
    if (!ticket_matches(ticket, &staging->remove_ticket))
        return quarantine(staging);
    staging->consumed_remove_authorization_serial =
        ticket->authorization_serial;
    if (original_call_count == 0u) {
        mark_terminal(staging, SUDEKIMP_TALOS_STAGING_RELEASED);
        return SUDEKIMP_TALOS_STAGING_SAFE_ABORT;
    }
    if (original_call_count != 1u ||
        !provenance_matches(&staging->provenance, current) ||
        completion == NULL ||
        !serial64_newer(completion->observation_serial,
            ticket->authorized_observation_serial) ||
        !detached_snapshot_valid(&staging->original, completion))
        return quarantine(staging);
    staging->detached = *completion;
    staging->state = SUDEKIMP_TALOS_STAGING_DETACHED_PROVEN;
    return SUDEKIMP_TALOS_STAGING_DETACH_PROVEN;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingClaimRestore(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *immediate,
    SudekiMpTalosStagingTicket *ticket
) {
    if (staging == NULL || ticket == NULL)
        return SUDEKIMP_TALOS_STAGING_REJECTED_INVALID;
    if (staging->state == SUDEKIMP_TALOS_STAGING_RESTORE_TICKET_ISSUED)
        return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
    if (staging->state != SUDEKIMP_TALOS_STAGING_DETACHED_PROVEN)
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    if (!provenance_matches(&staging->provenance, current) ||
        immediate == NULL ||
        !serial64_newer(immediate->observation_serial,
            staging->detached.observation_serial) ||
        !detached_snapshot_valid(&staging->original, immediate) ||
        !next_ticket(staging, immediate,
            SUDEKIMP_TALOS_STAGING_ADD_PLAYER_RVA,
            &staging->restore_ticket)) return quarantine(staging);
    *ticket = staging->restore_ticket;
    staging->state = SUDEKIMP_TALOS_STAGING_RESTORE_TICKET_ISSUED;
    return SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingFinishRestore(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosStagingTicket *ticket,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *completion,
    unsigned int original_call_count
) {
    if (staging == NULL || ticket == NULL)
        return SUDEKIMP_TALOS_STAGING_REJECTED_INVALID;
    if (staging->state != SUDEKIMP_TALOS_STAGING_RESTORE_TICKET_ISSUED) {
        if (ticket->authorization_serial != 0u &&
            ticket->authorization_serial ==
                staging->consumed_restore_authorization_serial)
            return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    }
    if (!ticket_matches(ticket, &staging->restore_ticket))
        return quarantine(staging);
    staging->consumed_restore_authorization_serial =
        ticket->authorization_serial;
    if (original_call_count != 1u ||
        !provenance_matches(&staging->provenance, current) ||
        completion == NULL ||
        !serial64_newer(completion->observation_serial,
            ticket->authorized_observation_serial) ||
        !full_snapshot_matches(&staging->original, completion))
        return quarantine(staging);
    staging->restored = *completion;
    staging->state = SUDEKIMP_TALOS_STAGING_RESTORED_PROVEN;
    return SUDEKIMP_TALOS_STAGING_RESTORE_PROVEN;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingObserveStability(
    SudekiMpTalosCompanionStaging *staging,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosStagingSnapshot *observation
) {
    if (staging == NULL)
        return SUDEKIMP_TALOS_STAGING_REJECTED_INVALID;
    if (staging->state == SUDEKIMP_TALOS_STAGING_STABILITY_PROVEN)
        return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
    if (staging->state != SUDEKIMP_TALOS_STAGING_RESTORED_PROVEN)
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    if (!provenance_matches(&staging->provenance, current) ||
        observation == NULL ||
        !serial64_newer(observation->observation_serial,
            staging->restored.observation_serial) ||
        !full_snapshot_matches(&staging->original, observation))
        return quarantine(staging);
    staging->state = SUDEKIMP_TALOS_STAGING_STABILITY_PROVEN;
    return SUDEKIMP_TALOS_STAGING_STABILITY_ACCEPTED;
}

SudekiMpTalosCompanionStagingResult SudekiMpTalosCompanionStagingRelease(
    SudekiMpTalosCompanionStaging *staging
) {
    if (staging == NULL)
        return SUDEKIMP_TALOS_STAGING_REJECTED_INVALID;
    if (staging->state == SUDEKIMP_TALOS_STAGING_RELEASED)
        return SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY;
    if (staging->state != SUDEKIMP_TALOS_STAGING_STABILITY_PROVEN)
        return SUDEKIMP_TALOS_STAGING_REJECTED_STATE;
    mark_terminal(staging, SUDEKIMP_TALOS_STAGING_RELEASED);
    return SUDEKIMP_TALOS_STAGING_RELEASE_ACCEPTED;
}
