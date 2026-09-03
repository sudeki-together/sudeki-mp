#include "network/lan_arena_replica.h"
#include "network/lan_arena_tal_combo_graph.h"

#include <math.h>
#include <string.h>

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static BOOL tick_after(uint32_t candidate, uint32_t reference) {
    return (int32_t)(candidate - reference) > 0;
}

static BOOL tick_before(uint32_t candidate, uint32_t reference) {
    return tick_after(reference, candidate);
}

static float interpolate_float(float before, float after, float alpha) {
    return before + (after - before) * alpha;
}

static BOOL action_sequence16_newer(uint16_t candidate, uint16_t reference) {
    return candidate != reference &&
        (uint16_t)(candidate - reference) < 0x8000u;
}

static uint8_t combat_state_for_action_event(uint8_t variant) {
    uint8_t combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    (void)SudekiMpLanArenaTalActionCombatState(variant, &combat_state);
    return combat_state;
}

static void replay_action_history(
    const SudekiMpLanArenaActorSnapshot *before,
    const SudekiMpLanArenaActorSnapshot *after,
    uint32_t host_tick,
    uint32_t after_host_tick,
    SudekiMpLanArenaActorSnapshot *output
) {
    const SudekiMpLanArenaActionEvent *selected = NULL;
    BOOL has_new_event = FALSE;
    unsigned int index;
    if (before == NULL || after == NULL || output == NULL) return;
    for (index = 0u; index < after->action_history_count; ++index) {
        const SudekiMpLanArenaActionEvent *event =
            &after->action_history[index];
        if (!action_sequence16_newer(
                event->sequence, before->action_sequence)) continue;
        has_new_event = TRUE;
        if (!tick_after(event->host_tick, host_tick)) selected = event;
    }
    if (!has_new_event) return;
    if (selected == NULL) {
        output->animation_state = before->animation_state;
        output->combat_state = before->combat_state;
        output->action_variant = before->action_variant;
        output->action_sequence = before->action_sequence;
        return;
    }
    output->animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    output->combat_state = combat_state_for_action_event(selected->variant);
    output->action_variant = selected->variant;
    output->action_sequence = selected->sequence;
    if (selected->sequence == after->action_sequence &&
        after->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        after->action_phase_valid &&
        tick_after(after_host_tick, selected->host_tick)) {
        uint32_t action_span = after_host_tick - selected->host_tick;
        uint32_t action_elapsed = host_tick - selected->host_tick;
        float action_alpha = clamp01(
            (float)action_elapsed / (float)action_span);
        output->action_phase_valid = 1u;
        output->action_phase_q8 = (uint16_t)(
            (float)after->action_phase_q8 * action_alpha + 0.5f);
    } else if (selected->sequence != after->action_sequence ||
        after->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION) {
        output->action_phase_valid = 0u;
        output->action_phase_q8 = 0u;
    }
}

static void normalized_facing(
    const SudekiMpLanArenaActorSnapshot *before,
    const SudekiMpLanArenaActorSnapshot *after,
    float alpha,
    float *output_x,
    float *output_z
) {
    float before_length = sqrtf(before->facing_x * before->facing_x +
        before->facing_z * before->facing_z);
    float after_length = sqrtf(after->facing_x * after->facing_x +
        after->facing_z * after->facing_z);
    float before_x = before->facing_x / before_length;
    float before_z = before->facing_z / before_length;
    float after_x = after->facing_x / after_length;
    float after_z = after->facing_z / after_length;
    float dot = before_x * after_x + before_z * after_z;
    float x;
    float z;
    float length;

    /* An exact/opposite turn has no unique interpolation arc. Never pass the
     * zero vector to Position::SetForward: hold the nearer authoritative
     * endpoint and switch once at the temporal midpoint. */
    if (dot <= -0.999f) {
        *output_x = alpha < 0.5f ? before_x : after_x;
        *output_z = alpha < 0.5f ? before_z : after_z;
        return;
    }
    x = interpolate_float(before_x, after_x, alpha);
    z = interpolate_float(before_z, after_z, alpha);
    length = sqrtf(x * x + z * z);
    if (!isfinite(length) || length <= 0.0001f) {
        *output_x = alpha < 0.5f ? before_x : after_x;
        *output_z = alpha < 0.5f ? before_z : after_z;
        return;
    }
    *output_x = x / length;
    *output_z = z / length;
}

static void interpolate_actor(
    const SudekiMpLanArenaActorSnapshot *before,
    const SudekiMpLanArenaActorSnapshot *after,
    float alpha,
    uint32_t host_tick,
    uint32_t after_host_tick,
    SudekiMpLanArenaActorSnapshot *output
) {
    *output = *after;
    if (before->native_entity_id != after->native_entity_id ||
        before->actor_type != after->actor_type) return;
    /* Keep the final locomotion pose while consuming the remaining buffered
     * distance. Switching to idle before the endpoint produces foot sliding;
     * snapping directly to the endpoint produces a visible position pop. */
    if (before->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
        after->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
        after->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_IDLE) {
        output->animation_state = before->animation_state;
        output->combat_state = before->combat_state;
    }
    /* The host's first non-action snapshot is the retirement boundary. Keep
     * the last authoritative action pose until that endpoint instead of
     * asking the client renderer to guess when the native clip has ended. */
    if (alpha < 1.0f &&
        before->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        after->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        before->action_sequence == after->action_sequence) {
        output->animation_state = before->animation_state;
        output->combat_state = before->combat_state;
        output->action_variant = before->action_variant;
        output->action_sequence = before->action_sequence;
        output->action_phase_valid = before->action_phase_valid;
        output->action_phase_q8 = before->action_phase_q8;
    }
    output->x = interpolate_float(before->x, after->x, alpha);
    output->y = interpolate_float(before->y, after->y, alpha);
    output->z = interpolate_float(before->z, after->z, alpha);
    normalized_facing(before, after, alpha,
        &output->facing_x, &output->facing_z);
    replay_action_history(
        before, after, host_tick, after_host_tick, output);
    if (output->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        before->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        after->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        before->action_sequence == after->action_sequence &&
        output->action_sequence == after->action_sequence &&
        before->action_phase_valid && after->action_phase_valid) {
        output->action_phase_valid = 1u;
        output->action_phase_q8 = (uint16_t)(interpolate_float(
            (float)before->action_phase_q8,
            (float)after->action_phase_q8, alpha) + 0.5f);
    } else if (output->animation_state !=
            SUDEKIMP_LAN_ARENA_ANIMATION_ACTION) {
        output->action_phase_valid = 0u;
        output->action_phase_q8 = 0u;
    }
}

static BOOL enemy_layout_matches(
    const SudekiMpLanArenaSnapshot *before,
    const SudekiMpLanArenaSnapshot *after
) {
    unsigned int index;
    if (before->enemy_count != after->enemy_count) return FALSE;
    for (index = 0u; index < after->enemy_count; ++index) {
        if (before->enemies[index].native_entity_id !=
                after->enemies[index].native_entity_id) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL snapshot_stream_continuous(
    const SudekiMpLanArenaSnapshot *before,
    const SudekiMpLanArenaSnapshot *after
) {
    return before != NULL && after != NULL &&
        tick_after(after->host_tick, before->host_tick) &&
        before->match_state == after->match_state &&
        before->combat_enabled == after->combat_enabled &&
        before->tal.actor_type == after->tal.actor_type &&
        before->tal.native_entity_id == after->tal.native_entity_id &&
        before->ailish.actor_type == after->ailish.actor_type &&
        before->ailish.native_entity_id == after->ailish.native_entity_id &&
        enemy_layout_matches(before, after);
}

void SudekiMpLanArenaReplicaReset(SudekiMpLanArenaReplica *replica) {
    if (replica != NULL) memset(replica, 0, sizeof(*replica));
}

BOOL SudekiMpLanArenaReplicaPush(
    SudekiMpLanArenaReplica *replica,
    const SudekiMpLanArenaSnapshot *snapshot
) {
    if (replica == NULL || snapshot == NULL ||
        !SudekiMpLanArenaSnapshotValid(snapshot) ||
        (replica->latest_valid &&
         !SudekiMpLanArenaSequenceNewer(snapshot->sequence, replica->latest.sequence))) {
        return FALSE;
    }
    if (replica->latest_valid &&
        !snapshot_stream_continuous(&replica->latest, snapshot)) {
        replica->earliest_valid = 0u;
        replica->oldest_valid = 0u;
        replica->previous_valid = 0u;
        replica->latest = *snapshot;
        replica->latest_valid = 1u;
        ++replica->stream_generation;
        if (replica->stream_generation == 0u) {
            replica->stream_generation = 1u;
        }
        return TRUE;
    }
    if (replica->latest_valid) {
        if (replica->previous_valid) {
            if (replica->oldest_valid) {
                replica->earliest = replica->oldest;
                replica->earliest_valid = 1u;
            }
            replica->oldest = replica->previous;
            replica->oldest_valid = 1u;
        }
        replica->previous = replica->latest;
        replica->previous_valid = 1u;
    }
    replica->latest = *snapshot;
    replica->latest_valid = 1u;
    if (replica->stream_generation == 0u) {
        replica->stream_generation = 1u;
    }
    return TRUE;
}

BOOL SudekiMpLanArenaReplicaSample(
    const SudekiMpLanArenaReplica *replica,
    uint32_t host_tick,
    SudekiMpLanArenaSnapshot *sample
) {
    float alpha;
    uint32_t elapsed;
    uint32_t span;
    unsigned int index;
    if (replica == NULL || sample == NULL || !replica->latest_valid) return FALSE;
    *sample = replica->latest;
    if (replica->earliest_valid &&
        tick_after(replica->oldest.host_tick,
            replica->earliest.host_tick) &&
        tick_before(host_tick, replica->oldest.host_tick) &&
        enemy_layout_matches(&replica->earliest, &replica->oldest)) {
        if (!tick_after(host_tick, replica->earliest.host_tick)) {
            *sample = replica->earliest;
            return TRUE;
        }
        span = replica->oldest.host_tick - replica->earliest.host_tick;
        elapsed = host_tick - replica->earliest.host_tick;
        alpha = clamp01((float)elapsed / (float)span);
        *sample = replica->oldest;
        interpolate_actor(&replica->earliest.tal, &replica->oldest.tal,
            alpha, host_tick, replica->oldest.host_tick, &sample->tal);
        interpolate_actor(&replica->earliest.ailish, &replica->oldest.ailish,
            alpha, host_tick, replica->oldest.host_tick, &sample->ailish);
        for (index = 0u; index < sample->enemy_count; ++index) {
            sample->enemies[index].x = interpolate_float(
                replica->earliest.enemies[index].x,
                replica->oldest.enemies[index].x, alpha);
            sample->enemies[index].y = interpolate_float(
                replica->earliest.enemies[index].y,
                replica->oldest.enemies[index].y, alpha);
            sample->enemies[index].z = interpolate_float(
                replica->earliest.enemies[index].z,
                replica->oldest.enemies[index].z, alpha);
        }
        return TRUE;
    }
    if (replica->oldest_valid &&
        tick_after(replica->previous.host_tick,
            replica->oldest.host_tick) &&
        tick_before(host_tick, replica->previous.host_tick) &&
        enemy_layout_matches(&replica->oldest, &replica->previous)) {
        if (!tick_after(host_tick, replica->oldest.host_tick)) {
            *sample = replica->oldest;
            return TRUE;
        }
        span = replica->previous.host_tick - replica->oldest.host_tick;
        elapsed = host_tick - replica->oldest.host_tick;
        alpha = clamp01((float)elapsed / (float)span);
        *sample = replica->previous;
        interpolate_actor(&replica->oldest.tal, &replica->previous.tal,
            alpha, host_tick, replica->previous.host_tick, &sample->tal);
        interpolate_actor(&replica->oldest.ailish, &replica->previous.ailish,
            alpha, host_tick, replica->previous.host_tick, &sample->ailish);
        for (index = 0u; index < sample->enemy_count; ++index) {
            sample->enemies[index].x = interpolate_float(
                replica->oldest.enemies[index].x,
                replica->previous.enemies[index].x, alpha);
            sample->enemies[index].y = interpolate_float(
                replica->oldest.enemies[index].y,
                replica->previous.enemies[index].y, alpha);
            sample->enemies[index].z = interpolate_float(
                replica->oldest.enemies[index].z,
                replica->previous.enemies[index].z, alpha);
        }
        return TRUE;
    }
    if (!replica->previous_valid ||
        !tick_after(replica->latest.host_tick,
            replica->previous.host_tick) ||
        !enemy_layout_matches(&replica->previous, &replica->latest)) {
        return TRUE;
    }
    if (!tick_after(host_tick, replica->previous.host_tick)) {
        *sample = replica->previous;
        return TRUE;
    }
    if (!tick_before(host_tick, replica->latest.host_tick)) return TRUE;
    span = replica->latest.host_tick - replica->previous.host_tick;
    elapsed = host_tick - replica->previous.host_tick;
    alpha = clamp01((float)elapsed / (float)span);
    interpolate_actor(&replica->previous.tal, &replica->latest.tal,
        alpha, host_tick, replica->latest.host_tick, &sample->tal);
    interpolate_actor(&replica->previous.ailish, &replica->latest.ailish,
        alpha, host_tick, replica->latest.host_tick, &sample->ailish);
    for (index = 0u; index < sample->enemy_count; ++index) {
        sample->enemies[index].x = interpolate_float(
            replica->previous.enemies[index].x,
            replica->latest.enemies[index].x, alpha);
        sample->enemies[index].y = interpolate_float(
            replica->previous.enemies[index].y,
            replica->latest.enemies[index].y, alpha);
        sample->enemies[index].z = interpolate_float(
            replica->previous.enemies[index].z,
            replica->latest.enemies[index].z, alpha);
    }
    return TRUE;
}

void SudekiMpLanArenaReplicaRenderClockReset(
    SudekiMpLanArenaReplicaRenderClock *clock
) {
    if (clock != NULL) memset(clock, 0, sizeof(*clock));
}

BOOL SudekiMpLanArenaReplicaRenderClockAdvance(
    const SudekiMpLanArenaReplica *replica,
    SudekiMpLanArenaReplicaRenderClock *clock,
    uint32_t local_tick,
    uint32_t *host_tick
) {
    uint32_t elapsed;
    uint32_t candidate;
    uint32_t catchup;
    uint32_t target;
    if (replica == NULL || clock == NULL || host_tick == NULL ||
        !replica->oldest_valid || !replica->previous_valid ||
        !replica->latest_valid ||
        !tick_after(replica->previous.host_tick,
            replica->oldest.host_tick) ||
        !tick_after(replica->latest.host_tick,
            replica->previous.host_tick)) {
        return FALSE;
    }
    if (!clock->initialized ||
        clock->stream_generation != replica->stream_generation) {
        /* Start one host interval behind the newest packet.  The retained
         * oldest sample still gives an interval of arrival-jitter headroom,
         * but beginning two intervals behind adds avoidable control latency. */
        clock->host_tick = replica->previous.host_tick;
        clock->local_tick = local_tick;
        clock->stream_generation = replica->stream_generation;
        clock->initialized = 1u;
        *host_tick = clock->host_tick;
        return TRUE;
    }
    elapsed = local_tick - clock->local_tick;
    clock->local_tick = local_tick;
    candidate = clock->host_tick + elapsed;
    if (tick_after(candidate, replica->latest.host_tick)) {
        candidate = replica->latest.host_tick;
    }

    /* Local elapsed time alone preserves any backlog accumulated while this
     * process is throttled.  Converge at at most 2x real time toward the
     * previous authoritative sample (normally 50 ms behind latest).  This is
     * monotonic and bounded: it never rewinds and never extrapolates beyond a
     * packet the host actually sent. */
    target = replica->previous.host_tick;
    if (tick_before(candidate, target)) {
        catchup = target - candidate;
        if (catchup > elapsed) catchup = elapsed;
        candidate += catchup;
    }
    clock->host_tick = candidate;
    *host_tick = candidate;
    return TRUE;
}
