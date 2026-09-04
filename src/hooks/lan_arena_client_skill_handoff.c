#include "hooks/lan_arena_client_skill_handoff.h"

#include "network/lan_arena_protocol.h"

#include <string.h>

void SudekiMpLanArenaClientSkillRetryReset(
    SudekiMpLanArenaClientSkillRetryGate *gate
) {
    if (gate == NULL) return;
    memset(gate, 0, sizeof(*gate));
}

SudekiMpLanArenaClientSkillRetryDecision
SudekiMpLanArenaClientSkillRetryDecide(
    SudekiMpLanArenaClientSkillRetryGate *gate,
    uint16_t incoming_sequence,
    uint8_t incoming_slot,
    uint32_t now_ms,
    int native_use_started
) {
    if (native_use_started) {
        return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ALREADY_STARTED;
    }
    if (gate == NULL || incoming_sequence == 0u) {
        return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_WAIT;
    }
    if (!gate->bound || gate->sequence != incoming_sequence ||
        gate->slot != incoming_slot) {
        SudekiMpLanArenaClientSkillRetryReset(gate);
        gate->sequence = incoming_sequence;
        gate->slot = incoming_slot;
        gate->bound = 1u;
    }
    if (gate->attempt_count >=
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_MAX_ATTEMPTS) {
        return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_EXHAUSTED;
    }
    if (gate->attempt_count != 0u &&
        now_ms - gate->last_attempt_at_ms <
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_BACKOFF_MS) {
        return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_WAIT;
    }
    gate->last_attempt_at_ms = now_ms;
    ++gate->attempt_count;
    return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT;
}

int SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
    int native_started,
    int native_active_seen,
    int local_state_observed,
    int local_native_active
) {
    return native_started && native_active_seen && local_state_observed &&
        !local_native_active;
}

SudekiMpLanArenaClientSkillHandoffDecision
SudekiMpLanArenaClientSkillHandoffDecide(
    int native_started,
    int native_active_seen,
    uint16_t current_sequence,
    uint16_t incoming_sequence,
    uint8_t incoming_kind,
    int local_state_observed,
    int local_native_active
) {
    if (!native_started) {
        return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_ACCEPT;
    }
    if (incoming_sequence != 0u &&
        incoming_sequence == current_sequence &&
        incoming_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER) {
        return SUDEKIMP_LAN_ARENA_CLIENT_SKILL_CONTINUE;
    }
    return SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
        native_started, native_active_seen, local_state_observed,
        local_native_active) ?
        SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETIRE_THEN_ACCEPT :
        SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN;
}

SudekiMpLanArenaClientOtherSkillDecision
SudekiMpLanArenaClientOtherSkillDecide(
    int other_native_started,
    int other_native_active_seen,
    int other_state_observed,
    int other_native_active
) {
    if (!other_native_started) {
        return SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_READY;
    }
    return SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
        other_native_started, other_native_active_seen,
        other_state_observed, other_native_active) ?
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_RETIRE :
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_DRAIN;
}

uint16_t SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
    uint16_t committed_sequence,
    uint16_t incoming_sequence,
    int incoming_active,
    int native_use_started
) {
    if (incoming_sequence == 0u || incoming_sequence == committed_sequence) {
        return committed_sequence;
    }
    if (incoming_active && !native_use_started) return committed_sequence;
    return incoming_sequence;
}

static int client_skill_containment_required(
    int session_authenticated,
    int activation_entry_in_flight,
    int native_task_retained,
    int reset_pending
) {
    return session_authenticated || activation_entry_in_flight ||
        native_task_retained || reset_pending;
}

int SudekiMpLanArenaClientSkillDamageContainmentRequired(
    int session_authenticated,
    int activation_entry_in_flight,
    int native_task_retained,
    int reset_pending
) {
    return client_skill_containment_required(
        session_authenticated, activation_entry_in_flight,
        native_task_retained, reset_pending);
}

int SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
    int session_authenticated,
    int activation_entry_in_flight,
    int native_transaction_retained,
    int reset_pending
) {
    return client_skill_containment_required(
        session_authenticated, activation_entry_in_flight,
        native_transaction_retained, reset_pending);
}
