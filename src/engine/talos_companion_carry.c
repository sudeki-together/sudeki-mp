#include "engine/talos_companion_carry.h"

#include <stddef.h>
#include <string.h>

static const uint32_t delete_logical[3] = {
    SUDEKIMP_TALOS_CARRY_BUKI_LOGICAL,
    SUDEKIMP_TALOS_CARRY_AILISH_LOGICAL,
    SUDEKIMP_TALOS_CARRY_ELCO_LOGICAL
};
static const uint32_t delete_raw[3] = {
    SUDEKIMP_TALOS_CARRY_BUKI_RAW,
    SUDEKIMP_TALOS_CARRY_AILISH_RAW,
    SUDEKIMP_TALOS_CARRY_ELCO_RAW
};
static const uint8_t delete_resource[3] = {
    SUDEKIMP_TALOS_CARRY_RESOURCE_BUKI,
    SUDEKIMP_TALOS_CARRY_RESOURCE_AILISH,
    SUDEKIMP_TALOS_CARRY_RESOURCE_ELCO
};

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
        value->hero_by_seat[0] != SUDEKIMP_TALOS_HERO_TAL) {
        return 0;
    }
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
        if (i != 0u)
            seen_controllers |= (uint8_t)(1u << controller);
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
            a->input_generation_by_seat[i] != b->input_generation_by_seat[i])
            return 0;
    }
    return 1;
}

static int lineage_exact(const SudekiMpTalosCarryLineage *value) {
    return value != NULL && value->source_task != 0u &&
        value->load_void_task != 0u && value->source_task_generation != 0u &&
        value->load_void_task_generation != 0u &&
        value->runtime_generation != 0u && value->descendant_proven &&
        value->source_hash == SUDEKIMP_TALOS_CARRY_SOURCE_HASH &&
        value->source_start == SUDEKIMP_TALOS_CARRY_SOURCE_START &&
        value->source_opcode == SUDEKIMP_TALOS_CARRY_SOURCE_OPCODE &&
        value->load_void_hash == SUDEKIMP_TALOS_CARRY_LOAD_VOID_HASH &&
        value->load_void_start == SUDEKIMP_TALOS_CARRY_LOAD_VOID_START;
}

static int lineage_matches(
    const SudekiMpTalosCarryLineage *a,
    const SudekiMpTalosCarryLineage *b
) {
    return lineage_exact(a) && lineage_exact(b) &&
        a->source_task == b->source_task &&
        a->load_void_task == b->load_void_task &&
        a->source_task_generation == b->source_task_generation &&
        a->load_void_task_generation == b->load_void_task_generation &&
        a->runtime_generation == b->runtime_generation;
}

static int formation_valid(
    const SudekiMpTalosCarryFormation *formation,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    unsigned int i;

    if (formation == NULL || !formation->observed ||
        formation->group_identity == 0u || formation->group_generation == 0u ||
        formation->formation_owner_identity == 0u ||
        formation->formation_owner_generation == 0u ||
        formation->formation_identity == 0u ||
        formation->formation_generation == 0u ||
        formation->group_member_mask != 0x0fu ||
        formation->formation_member_mask != 0x0fu) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (formation->actor_identity[i] !=
                provenance->hero_actor_identity[i] ||
            formation->actor_generation[i] !=
                provenance->hero_actor_generation[i]) return 0;
    }
    return 1;
}

static int formation_matches(
    const SudekiMpTalosCarryFormation *a,
    const SudekiMpTalosCarryFormation *b
) {
    unsigned int i;

    if (a->group_identity != b->group_identity ||
        a->formation_owner_identity != b->formation_owner_identity ||
        a->formation_identity != b->formation_identity ||
        a->group_generation != b->group_generation ||
        a->formation_owner_generation != b->formation_owner_generation ||
        a->formation_generation != b->formation_generation ||
        a->group_member_mask != b->group_member_mask ||
        a->formation_member_mask != b->formation_member_mask) return 0;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        if (a->actor_identity[i] != b->actor_identity[i] ||
            a->actor_generation[i] != b->actor_generation[i]) return 0;
    }
    return 1;
}

static int target_index(const SudekiMpTalosCarryDeleteObservation *value) {
    int logical = -1;
    int raw = -1;
    unsigned int i;

    for (i = 0u; i < 3u; ++i) {
        if (value->logical_opcode == delete_logical[i]) logical = (int)i;
        if (value->raw_opcode == delete_raw[i]) raw = (int)i;
    }
    if (logical < 0 && raw < 0) return -1;
    return logical == raw ? logical : -2;
}

static void mark_terminal(
    SudekiMpTalosCompanionCarry *carry,
    SudekiMpTalosCarryState state
);

static SudekiMpTalosCarryDeleteAction fail_delete(
    SudekiMpTalosCompanionCarry *carry
) {
    if (carry->delete_cursor == 0u) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_ABORTED);
        return SUDEKIMP_TALOS_CARRY_DELETE_ABORT_TO_VANILLA;
    }
    mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
    return SUDEKIMP_TALOS_CARRY_DELETE_QUARANTINE;
}

static int fenced(
    const SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *provenance
) {
    uint32_t distance = provenance->transition_serial -
        carry->terminal_transition_serial;

    return carry->terminal_transition_serial != 0u &&
        provenance->world_generation == carry->terminal_world_generation &&
        provenance->source_generation == carry->terminal_source_generation &&
        (distance == 0u || distance >= UINT32_C(0x80000000));
}

static void mark_terminal(
    SudekiMpTalosCompanionCarry *carry,
    SudekiMpTalosCarryState state
) {
    if (carry->provenance.serial > carry->highest_terminal_encounter_serial ||
        (carry->provenance.serial == carry->highest_terminal_encounter_serial &&
         carry->provenance.transition_serial > carry->terminal_transition_serial)) {
        carry->highest_terminal_encounter_serial = carry->provenance.serial;
    }
    carry->terminal_transition_serial = carry->provenance.transition_serial;
    carry->terminal_world_generation = carry->provenance.world_generation;
    carry->terminal_source_generation = carry->provenance.source_generation;
    carry->state = state;
}

void SudekiMpTalosCompanionCarryInitialize(SudekiMpTalosCompanionCarry *carry) {
    if (carry == NULL) return;
    memset(carry, 0, sizeof(*carry));
    carry->state = SUDEKIMP_TALOS_CARRY_DISABLED;
}

void SudekiMpTalosCompanionCarryConfigure(
    SudekiMpTalosCompanionCarry *carry,
    int enabled
) {
    uint32_t serial;
    uint32_t terminal_encounter;
    uint32_t terminal_transition;
    uint32_t terminal_world;
    uint32_t terminal_source;
    uint8_t requested;

    if (carry == NULL) return;
    requested = enabled ? 1u : 0u;
    if (carry->enabled == requested) return;
    if (carry->skipped_mask != 0u &&
        carry->state != SUDEKIMP_TALOS_CARRY_RELEASED) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
        return;
    }
    serial = carry->next_authorization_serial;
    terminal_encounter = carry->highest_terminal_encounter_serial;
    terminal_transition = carry->terminal_transition_serial;
    terminal_world = carry->terminal_world_generation;
    terminal_source = carry->terminal_source_generation;
    memset(carry, 0, sizeof(*carry));
    carry->next_authorization_serial = serial;
    carry->highest_terminal_encounter_serial = terminal_encounter;
    carry->terminal_transition_serial = terminal_transition;
    carry->terminal_world_generation = terminal_world;
    carry->terminal_source_generation = terminal_source;
    carry->enabled = requested;
    carry->state = enabled ? SUDEKIMP_TALOS_CARRY_IDLE :
        SUDEKIMP_TALOS_CARRY_DISABLED;
}

SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryBegin(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *provenance,
    const SudekiMpTalosCarryPreflight *preflight
) {
    if (carry == NULL || !carry->enabled)
        return SUDEKIMP_TALOS_CARRY_DISABLED_RESULT;
    if (carry->state != SUDEKIMP_TALOS_CARRY_IDLE)
        return SUDEKIMP_TALOS_CARRY_REJECTED_STATE;
    if (!provenance_valid(provenance))
        return SUDEKIMP_TALOS_CARRY_ABORT_TO_VANILLA;
    if (fenced(carry, provenance))
        return SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY;
    carry->provenance = *provenance;
    if (preflight == NULL || !preflight->exact_executable ||
        !preflight->exact_asset ||
        !preflight->host_authority || !lineage_exact(&preflight->lineage) ||
        !formation_valid(&preflight->formation, provenance)) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_ABORTED);
        return SUDEKIMP_TALOS_CARRY_ABORT_TO_VANILLA;
    }
    carry->preflight = *preflight;
    carry->state = SUDEKIMP_TALOS_CARRY_ARMED;
    return SUDEKIMP_TALOS_CARRY_STARTED;
}

SudekiMpTalosCarryDeleteAction SudekiMpTalosCompanionCarryObserveDelete(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryDeleteObservation *observation
) {
    int index;

    if (observation == NULL) return SUDEKIMP_TALOS_CARRY_DELETE_PASS_NATIVE;
    index = target_index(observation);
    if (index == -1) return SUDEKIMP_TALOS_CARRY_DELETE_PASS_NATIVE;
    if (carry == NULL || !carry->enabled ||
        carry->state == SUDEKIMP_TALOS_CARRY_DISABLED ||
        carry->state == SUDEKIMP_TALOS_CARRY_IDLE ||
        carry->state == SUDEKIMP_TALOS_CARRY_ABORTED)
        return SUDEKIMP_TALOS_CARRY_DELETE_PASS_NATIVE;
    if (carry->state == SUDEKIMP_TALOS_CARRY_QUARANTINED)
        return SUDEKIMP_TALOS_CARRY_DELETE_QUARANTINE;
    if ((carry->state != SUDEKIMP_TALOS_CARRY_ARMED &&
         carry->state != SUDEKIMP_TALOS_CARRY_PARTIAL_SKIP) || index < 0 ||
        !provenance_matches(&carry->provenance, current) ||
        !observation->exact_executable || !observation->exact_asset ||
        !lineage_matches(&carry->preflight.lineage, &observation->lineage) ||
        observation->binding_hash != SUDEKIMP_TALOS_CARRY_DELETE_PC_HASH ||
        (unsigned int)index != carry->delete_cursor ||
        observation->resource_id != delete_resource[index])
        return fail_delete(carry);
    carry->skipped_mask |= (uint8_t)(1u << index);
    ++carry->delete_cursor;
    carry->state = carry->delete_cursor == 3u ?
        SUDEKIMP_TALOS_CARRY_PRESERVED : SUDEKIMP_TALOS_CARRY_PARTIAL_SKIP;
    return SUDEKIMP_TALOS_CARRY_DELETE_SKIP_NATIVE;
}

static int exact_void(const char destination[16]) {
    static const char expected[] = "Void";

    return destination != NULL &&
        memcmp(destination, expected, sizeof(expected)) == 0;
}

static int set_zone_exact(
    const SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosCarrySetZoneObservation *observation
) {
    return observation->encounter_serial == carry->provenance.serial &&
        observation->transition_serial == carry->provenance.transition_serial &&
        observation->logical_opcode == SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL &&
        observation->raw_opcode == SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW &&
        observation->binding_hash == SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH &&
        observation->exact_executable && observation->exact_asset &&
        exact_void(observation->destination) &&
        lineage_matches(&carry->preflight.lineage, &observation->lineage);
}

SudekiMpTalosCarrySetZoneAction SudekiMpTalosCompanionCarryObserveSetZone(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarrySetZoneObservation *observation,
    SudekiMpTalosCarrySetZoneTicket *ticket
) {
    int target;
    int exact;

    if (observation == NULL) return SUDEKIMP_TALOS_CARRY_SET_ZONE_PASS_NATIVE;
    target = observation->logical_opcode ==
            SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL ||
        observation->raw_opcode == SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW;
    if (!target || carry == NULL || !carry->enabled ||
        carry->state == SUDEKIMP_TALOS_CARRY_DISABLED ||
        carry->state == SUDEKIMP_TALOS_CARRY_IDLE ||
        carry->state == SUDEKIMP_TALOS_CARRY_ABORTED)
        return SUDEKIMP_TALOS_CARRY_SET_ZONE_PASS_NATIVE;
    exact = provenance_matches(&carry->provenance, current) &&
        set_zone_exact(carry, observation);
    if (carry->state == SUDEKIMP_TALOS_CARRY_ARMED &&
        carry->skipped_mask == 0u) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_ABORTED);
        return SUDEKIMP_TALOS_CARRY_SET_ZONE_PASS_NATIVE;
    }
    if (carry->state == SUDEKIMP_TALOS_CARRY_SET_ZONE_PASSED ||
        carry->state == SUDEKIMP_TALOS_CARRY_FORMATION_CLAIMED ||
        carry->state == SUDEKIMP_TALOS_CARRY_ACTIVE) {
        if (exact) return SUDEKIMP_TALOS_CARRY_SET_ZONE_BLOCK_REPLAY;
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
        return SUDEKIMP_TALOS_CARRY_SET_ZONE_QUARANTINE;
    }
    if (carry->state != SUDEKIMP_TALOS_CARRY_PRESERVED ||
        carry->skipped_mask != 0x07u || !exact || ticket == NULL) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
        return SUDEKIMP_TALOS_CARRY_SET_ZONE_QUARANTINE;
    }
    memset(&carry->set_zone_ticket, 0, sizeof(carry->set_zone_ticket));
    carry->set_zone_ticket.lineage = observation->lineage;
    carry->set_zone_ticket.encounter_serial = observation->encounter_serial;
    carry->set_zone_ticket.transition_serial = observation->transition_serial;
    if (++carry->next_authorization_serial == 0u)
        ++carry->next_authorization_serial;
    carry->set_zone_ticket.authorization_serial =
        carry->next_authorization_serial;
    *ticket = carry->set_zone_ticket;
    carry->state = SUDEKIMP_TALOS_CARRY_SET_ZONE_PASSED;
    return SUDEKIMP_TALOS_CARRY_SET_ZONE_ALLOW_NATIVE_ONCE;
}

SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryClaimFormation(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosEncounterProvenance *current,
    const SudekiMpTalosCarryFormationObservation *observation,
    SudekiMpTalosCarryFormationTicket *ticket
) {
    unsigned int i;

    if (carry == NULL || observation == NULL || ticket == NULL)
        return SUDEKIMP_TALOS_CARRY_REJECTED_INVALID;
    if (carry->state == SUDEKIMP_TALOS_CARRY_FORMATION_CLAIMED ||
        carry->state == SUDEKIMP_TALOS_CARRY_ACTIVE)
        return SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY;
    if (carry->state != SUDEKIMP_TALOS_CARRY_SET_ZONE_PASSED ||
        carry->skipped_mask != 0x07u ||
        !provenance_matches(&carry->provenance, current) ||
        observation->encounter_serial != carry->provenance.serial ||
        observation->transition_serial != carry->provenance.transition_serial ||
        observation->arrival_world_generation == 0u ||
        observation->arrival_source_generation == 0u ||
        (observation->arrival_world_generation == carry->provenance.world_generation &&
         observation->arrival_source_generation == carry->provenance.source_generation) ||
        observation->set_zone_logical != SUDEKIMP_TALOS_CARRY_SET_ZONE_LOGICAL ||
        observation->set_zone_raw != SUDEKIMP_TALOS_CARRY_SET_ZONE_RAW ||
        observation->set_zone_hash != SUDEKIMP_TALOS_CARRY_SET_ZONE_HASH ||
        observation->end_tsa_logical != SUDEKIMP_TALOS_CARRY_END_TSA_LOGICAL ||
        observation->end_tsa_raw != SUDEKIMP_TALOS_CARRY_END_TSA_RAW ||
        observation->end_tsa_hash != SUDEKIMP_TALOS_CARRY_END_TSA_HASH ||
        observation->set_zone_authorization_serial !=
            carry->set_zone_ticket.authorization_serial ||
        !observation->arrival_settled ||
        !observation->exact_release_point || !observation->tsa_active ||
        !observation->tal_final_pop_settled || !observation->item_use_settled ||
        !observation->boss_ready || !observation->no_pending_removal ||
        !formation_valid(&observation->formation, &carry->provenance) ||
        !formation_matches(&carry->preflight.formation,
            &observation->formation)) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
        return SUDEKIMP_TALOS_CARRY_QUARANTINE;
    }
    memset(&carry->ticket, 0, sizeof(carry->ticket));
    carry->ticket.encounter_serial = carry->provenance.serial;
    carry->ticket.transition_serial = carry->provenance.transition_serial;
    carry->ticket.arrival_world_generation =
        observation->arrival_world_generation;
    carry->ticket.arrival_source_generation =
        observation->arrival_source_generation;
    if (++carry->next_authorization_serial == 0u)
        ++carry->next_authorization_serial;
    carry->ticket.authorization_serial = carry->next_authorization_serial;
    carry->ticket.native_function_rva =
        SUDEKIMP_TALOS_CARRY_FORMATION_POP_RVA;
    carry->ticket.group_identity = observation->formation.group_identity;
    carry->ticket.formation_owner_identity =
        observation->formation.formation_owner_identity;
    carry->ticket.formation_identity =
        observation->formation.formation_identity;
    carry->ticket.group_generation = observation->formation.group_generation;
    carry->ticket.formation_owner_generation =
        observation->formation.formation_owner_generation;
    carry->ticket.formation_generation =
        observation->formation.formation_generation;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        carry->ticket.actor_identity[i] =
            carry->provenance.hero_actor_identity[i];
        carry->ticket.actor_generation[i] =
            carry->provenance.hero_actor_generation[i];
    }
    *ticket = carry->ticket;
    carry->state = SUDEKIMP_TALOS_CARRY_FORMATION_CLAIMED;
    return SUDEKIMP_TALOS_CARRY_FORMATION_AUTHORIZED;
}

SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryFinishFormation(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosCarryFormationTicket *ticket,
    const SudekiMpTalosCarryFormationCompletion *completion
) {
    if (carry == NULL || ticket == NULL || completion == NULL)
        return SUDEKIMP_TALOS_CARRY_REJECTED_INVALID;
    if (carry->state != SUDEKIMP_TALOS_CARRY_FORMATION_CLAIMED)
        return SUDEKIMP_TALOS_CARRY_REJECTED_REPLAY;
    if (memcmp(ticket, &carry->ticket, sizeof(*ticket)) != 0 ||
        completion->encounter_serial != ticket->encounter_serial ||
        completion->authorization_serial != ticket->authorization_serial ||
        completion->arrival_world_generation !=
            ticket->arrival_world_generation ||
        completion->arrival_source_generation !=
            ticket->arrival_source_generation ||
        !completion->native_call_completed || !completion->placement_verified ||
        !completion->no_pending_removal ||
        !formation_valid(&completion->formation, &carry->provenance) ||
        !formation_matches(&carry->preflight.formation,
            &completion->formation)) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
        return SUDEKIMP_TALOS_CARRY_QUARANTINE;
    }
    carry->state = SUDEKIMP_TALOS_CARRY_ACTIVE;
    return SUDEKIMP_TALOS_CARRY_FORMATION_COMMITTED;
}

SudekiMpTalosCarryResult SudekiMpTalosCompanionCarryObserveTeardown(
    SudekiMpTalosCompanionCarry *carry,
    const SudekiMpTalosCarryTeardownObservation *observation
) {
    if (carry == NULL || observation == NULL)
        return SUDEKIMP_TALOS_CARRY_REJECTED_INVALID;
    if (observation->binding_hash != SUDEKIMP_TALOS_CARRY_REMOVE_ALL_HASH)
        return SUDEKIMP_TALOS_CARRY_NO_CHANGE;
    if (carry->state == SUDEKIMP_TALOS_CARRY_RELEASED &&
        observation->post_native_completion && observation->verified_empty &&
        observation->member_count == 0u && observation->exact_callsite &&
        observation->encounter_serial == carry->provenance.serial)
        return SUDEKIMP_TALOS_CARRY_NO_CHANGE;
    if (!observation->post_native_completion)
        return SUDEKIMP_TALOS_CARRY_NO_CHANGE;
    if (carry->state != SUDEKIMP_TALOS_CARRY_ACTIVE ||
        !observation->exact_callsite || !observation->verified_empty ||
        observation->encounter_serial != carry->provenance.serial ||
        observation->member_count != 0u) {
        mark_terminal(carry, SUDEKIMP_TALOS_CARRY_QUARANTINED);
        return SUDEKIMP_TALOS_CARRY_QUARANTINE;
    }
    mark_terminal(carry, SUDEKIMP_TALOS_CARRY_RELEASED);
    return SUDEKIMP_TALOS_CARRY_TEARDOWN_OBSERVED;
}
