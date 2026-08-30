#include "engine/talos_encounter_session.h"

#include <stddef.h>
#include <string.h>

static uint32_t next_serial(SudekiMpTalosEncounterSession *session) {
    ++session->next_serial;
    if (session->next_serial == 0u) {
        ++session->next_serial;
    }
    return session->next_serial;
}

static int serial_is_newer(uint32_t candidate, uint32_t baseline) {
    uint32_t distance = candidate - baseline;

    return distance != 0u && distance < UINT32_C(0x80000000);
}

static int hero_identifier_valid(uint8_t hero) {
    return hero < SUDEKIMP_TALOS_HERO_COUNT;
}

unsigned int SudekiMpTalosEncounterHumanCount(uint8_t active_human_mask) {
    unsigned int count = 0u;
    unsigned int seat;

    active_human_mask &= SUDEKIMP_TALOS_HUMAN_MASK;
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        if ((active_human_mask & (uint8_t)(1u << seat)) != 0u) {
            ++count;
        }
    }
    return count;
}

uint32_t SudekiMpTalosEncounterHealthTarget(
    unsigned int combatant_count
) {
    if (combatant_count == 0u ||
        combatant_count > SUDEKIMP_TALOS_EXPANDED_COMBATANTS) {
        return 0u;
    }
    return SUDEKIMP_TALOS_BASE_HEALTH * combatant_count;
}

static int request_valid(const SudekiMpTalosEncounterRequest *request) {
    uint8_t seen_heroes = 0u;
    uint8_t seen_controller_slots = 0u;
    unsigned int seat;
    unsigned int hero;

    if (request == NULL || request->transition_serial == 0u ||
        request->world_generation == 0u ||
        request->source_generation == 0u || request->host_actor == 0u ||
        request->host_actor_generation == 0u ||
        request->host_lease_generation == 0u ||
        request->combatant_count != SUDEKIMP_TALOS_EXPANDED_COMBATANTS ||
        (request->active_human_mask &
            (uint8_t)~SUDEKIMP_TALOS_HUMAN_MASK) != 0u ||
        (request->active_human_mask & 0x01u) == 0u ||
        request->hero_by_seat[0] != SUDEKIMP_TALOS_HERO_TAL) {
        return 0;
    }

    for (hero = 0u; hero < SUDEKIMP_TALOS_HERO_COUNT; ++hero) {
        unsigned int prior;

        if (request->hero_actor_identity[hero] == 0u ||
            request->hero_actor_generation[hero] == 0u) {
            return 0;
        }
        for (prior = 0u; prior < hero; ++prior) {
            if (request->hero_actor_identity[prior] ==
                    request->hero_actor_identity[hero]) {
                return 0;
            }
        }
    }
    if (request->hero_actor_identity[SUDEKIMP_TALOS_HERO_TAL] !=
            request->host_actor ||
        request->hero_actor_generation[SUDEKIMP_TALOS_HERO_TAL] !=
            request->host_actor_generation) {
        return 0;
    }

    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        const uint8_t seat_bit = (uint8_t)(1u << seat);
        const uint8_t hero = request->hero_by_seat[seat];
        const uint8_t controller = request->controller_slot_by_seat[seat];
        unsigned int prior;

        if ((request->active_human_mask & seat_bit) == 0u) {
            if (hero != SUDEKIMP_TALOS_HERO_NONE ||
                controller != SUDEKIMP_TALOS_CONTROLLER_NONE ||
                request->input_identity_by_seat[seat] != 0u ||
                request->input_generation_by_seat[seat] != 0u) {
                return 0;
            }
            continue;
        }
        if (!hero_identifier_valid(hero) ||
            (seen_heroes & (uint8_t)(1u << hero)) != 0u ||
            (seat != 0u && hero == SUDEKIMP_TALOS_HERO_TAL)) {
            return 0;
        }
        if (request->input_identity_by_seat[seat] == 0u ||
            request->input_generation_by_seat[seat] == 0u) {
            return 0;
        }
        for (prior = 0u; prior < seat; ++prior) {
            if ((request->active_human_mask &
                    (uint8_t)(1u << prior)) != 0u &&
                request->input_identity_by_seat[prior] ==
                    request->input_identity_by_seat[seat]) {
                return 0;
            }
        }
        if (seat == 0u) {
            if (controller != SUDEKIMP_TALOS_CONTROLLER_NONE) {
                return 0;
            }
        } else {
            if (controller >= 4u ||
                (seen_controller_slots & (uint8_t)(1u << controller)) != 0u) {
                return 0;
            }
            seen_controller_slots |= (uint8_t)(1u << controller);
        }
        seen_heroes |= (uint8_t)(1u << hero);
    }
    return 1;
}

static int provenance_equal(
    const SudekiMpTalosEncounterProvenance *left,
    const SudekiMpTalosEncounterProvenance *right
) {
    unsigned int seat;

    if (left == NULL || right == NULL || left->serial != right->serial ||
        left->transition_serial != right->transition_serial ||
        left->world_generation != right->world_generation ||
        left->source_generation != right->source_generation ||
        left->host_actor != right->host_actor ||
        left->host_actor_generation != right->host_actor_generation ||
        left->host_lease_generation != right->host_lease_generation ||
        left->talos_health_target != right->talos_health_target ||
        left->active_human_mask != right->active_human_mask ||
        left->combatant_count != right->combatant_count) {
        return 0;
    }
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        if (left->hero_by_seat[seat] != right->hero_by_seat[seat] ||
            left->controller_slot_by_seat[seat] !=
                right->controller_slot_by_seat[seat] ||
            left->input_identity_by_seat[seat] !=
                right->input_identity_by_seat[seat] ||
            left->input_generation_by_seat[seat] !=
                right->input_generation_by_seat[seat]) {
            return 0;
        }
    }
    for (seat = 0u; seat < SUDEKIMP_TALOS_HERO_COUNT; ++seat) {
        if (left->hero_actor_identity[seat] !=
                right->hero_actor_identity[seat] ||
            left->hero_actor_generation[seat] !=
                right->hero_actor_generation[seat]) {
            return 0;
        }
    }
    return 1;
}

static void clear_current(SudekiMpTalosEncounterSession *session) {
    memset(&session->provenance, 0, sizeof(session->provenance));
    session->state = SUDEKIMP_TALOS_ENCOUNTER_IDLE;
    session->quarantine_reason = SUDEKIMP_TALOS_QUARANTINE_NONE;
}

static void retire_current_transition(
    SudekiMpTalosEncounterSession *session
) {
    if (session->provenance.transition_serial == 0u) {
        return;
    }
    session->highest_terminal_transition_serial =
        session->provenance.transition_serial;
    session->terminal_world_generation =
        session->provenance.world_generation;
    session->terminal_source_generation =
        session->provenance.source_generation;
}

static SudekiMpTalosEncounterResult check_exact_provenance(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterProvenance *candidate
) {
    if (candidate == NULL || candidate->serial == 0u) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (candidate->serial != session->provenance.serial) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE;
    }
    if (!provenance_equal(&session->provenance, candidate)) {
        session->state = SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED;
        session->quarantine_reason =
            SUDEKIMP_TALOS_QUARANTINE_PROVENANCE_MISMATCH;
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_MISMATCH;
    }
    return SUDEKIMP_TALOS_ENCOUNTER_NO_CHANGE;
}

void SudekiMpTalosEncounterInitialize(
    SudekiMpTalosEncounterSession *session
) {
    if (session != NULL) {
        memset(session, 0, sizeof(*session));
    }
}

SudekiMpTalosEncounterResult SudekiMpTalosEncounterOpenPrompt(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterRequest *request
) {
    unsigned int seat;

    if (session == NULL || !request_valid(request)) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED;
    }
    if (session->state != SUDEKIMP_TALOS_ENCOUNTER_IDLE) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY;
    }
    if (session->highest_terminal_transition_serial != 0u &&
        request->world_generation == session->terminal_world_generation &&
        request->source_generation == session->terminal_source_generation &&
        !serial_is_newer(request->transition_serial,
            session->highest_terminal_transition_serial)) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY;
    }

    memset(&session->provenance, 0, sizeof(session->provenance));
    session->provenance.serial = next_serial(session);
    session->provenance.transition_serial = request->transition_serial;
    session->provenance.world_generation = request->world_generation;
    session->provenance.source_generation = request->source_generation;
    session->provenance.host_actor = request->host_actor;
    session->provenance.host_actor_generation =
        request->host_actor_generation;
    session->provenance.host_lease_generation =
        request->host_lease_generation;
    for (seat = 0u; seat < SUDEKIMP_TALOS_HERO_COUNT; ++seat) {
        session->provenance.hero_actor_identity[seat] =
            request->hero_actor_identity[seat];
        session->provenance.hero_actor_generation[seat] =
            request->hero_actor_generation[seat];
    }
    session->provenance.active_human_mask = request->active_human_mask;
    session->provenance.combatant_count = request->combatant_count;
    session->provenance.talos_health_target =
        SudekiMpTalosEncounterHealthTarget(request->combatant_count);
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        session->provenance.hero_by_seat[seat] =
            request->hero_by_seat[seat];
        session->provenance.controller_slot_by_seat[seat] =
            request->controller_slot_by_seat[seat];
        session->provenance.input_identity_by_seat[seat] =
            request->input_identity_by_seat[seat];
        session->provenance.input_generation_by_seat[seat] =
            request->input_generation_by_seat[seat];
    }
    session->state = SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPEN;
    return SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED;
}

SudekiMpTalosEncounterResult SudekiMpTalosEncounterConfirm(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterProvenance *visible_prompt
) {
    SudekiMpTalosEncounterResult exact;

    if (session == NULL) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED;
    }
    exact = check_exact_provenance(session, visible_prompt);
    if (exact != SUDEKIMP_TALOS_ENCOUNTER_NO_CHANGE) {
        return exact;
    }
    if (session->state != SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPEN) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY;
    }
    session->state = SUDEKIMP_TALOS_ENCOUNTER_CONFIRMED;
    return SUDEKIMP_TALOS_ENCOUNTER_CONFIRM_ACCEPTED;
}

SudekiMpTalosEncounterResult SudekiMpTalosEncounterCancel(
    SudekiMpTalosEncounterSession *session,
    uint32_t serial
) {
    if (session == NULL || serial == 0u) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED;
    }
    if (serial != session->provenance.serial) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE;
    }
    if (session->state != SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPEN) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY;
    }
    session->state = SUDEKIMP_TALOS_ENCOUNTER_CANCELLED;
    return SUDEKIMP_TALOS_ENCOUNTER_CANCEL_ACCEPTED;
}

SudekiMpTalosEncounterResult SudekiMpTalosEncounterClaimTransition(
    SudekiMpTalosEncounterSession *session,
    const SudekiMpTalosEncounterProvenance *current
) {
    SudekiMpTalosEncounterResult exact;

    if (session == NULL) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED;
    }
    if (current != NULL && current->serial != 0u &&
        current->serial == session->claimed_serial) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY;
    }
    exact = check_exact_provenance(session, current);
    if (exact != SUDEKIMP_TALOS_ENCOUNTER_NO_CHANGE) {
        return exact;
    }
    if (session->state == SUDEKIMP_TALOS_ENCOUNTER_CLAIMED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY;
    }
    if (session->state != SUDEKIMP_TALOS_ENCOUNTER_CONFIRMED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY;
    }
    session->state = SUDEKIMP_TALOS_ENCOUNTER_CLAIMED;
    session->claimed_serial = current->serial;
    return SUDEKIMP_TALOS_ENCOUNTER_CLAIM_ACCEPTED;
}

SudekiMpTalosEncounterResult SudekiMpTalosEncounterFinishTransition(
    SudekiMpTalosEncounterSession *session,
    uint32_t serial,
    int native_transition_succeeded
) {
    if (session == NULL || serial == 0u) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED;
    }
    if (serial != session->provenance.serial) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE;
    }
    if (session->state != SUDEKIMP_TALOS_ENCOUNTER_CLAIMED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY;
    }
    if (!native_transition_succeeded) {
        session->state = SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED;
        session->quarantine_reason =
            SUDEKIMP_TALOS_QUARANTINE_TRANSITION_FAILED;
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED;
    }
    retire_current_transition(session);
    clear_current(session);
    return SUDEKIMP_TALOS_ENCOUNTER_FINISHED;
}

int SudekiMpTalosEncounterDismissCancelled(
    SudekiMpTalosEncounterSession *session,
    uint32_t serial
) {
    if (session == NULL || serial == 0u ||
        session->state != SUDEKIMP_TALOS_ENCOUNTER_CANCELLED ||
        session->provenance.serial != serial) {
        return 0;
    }
    retire_current_transition(session);
    clear_current(session);
    return 1;
}

void SudekiMpTalosEncounterQuarantine(
    SudekiMpTalosEncounterSession *session,
    SudekiMpTalosEncounterQuarantineReason reason
) {
    if (session == NULL) {
        return;
    }
    session->state = SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED;
    session->quarantine_reason = reason == SUDEKIMP_TALOS_QUARANTINE_NONE ?
        SUDEKIMP_TALOS_QUARANTINE_NATIVE_LIFECYCLE_UNCERTAIN : reason;
}

SudekiMpTalosEncounterResult SudekiMpTalosEncounterRecover(
    SudekiMpTalosEncounterSession *session,
    uint32_t world_generation,
    uint32_t source_generation
) {
    if (session == NULL || world_generation == 0u ||
        source_generation == 0u) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID;
    }
    if (session->state != SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY;
    }
    if (world_generation == session->provenance.world_generation &&
        source_generation == session->provenance.source_generation) {
        return SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE;
    }
    retire_current_transition(session);
    clear_current(session);
    return SUDEKIMP_TALOS_ENCOUNTER_RECOVERED;
}

int SudekiMpTalosEncounterGetProvenance(
    const SudekiMpTalosEncounterSession *session,
    SudekiMpTalosEncounterProvenance *provenance
) {
    if (session == NULL || provenance == NULL ||
        session->state == SUDEKIMP_TALOS_ENCOUNTER_IDLE) {
        return 0;
    }
    *provenance = session->provenance;
    return 1;
}

unsigned int SudekiMpTalosEncounterSeatForHero(
    const SudekiMpTalosEncounterProvenance *provenance,
    SudekiMpTalosHero hero
) {
    unsigned int seat;

    if (provenance == NULL || (unsigned int)hero >=
        SUDEKIMP_TALOS_HERO_COUNT) {
        return SUDEKIMP_TALOS_SEAT_AI;
    }
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        if (provenance->hero_by_seat[seat] == (uint8_t)hero) {
            return seat;
        }
    }
    return SUDEKIMP_TALOS_SEAT_AI;
}
