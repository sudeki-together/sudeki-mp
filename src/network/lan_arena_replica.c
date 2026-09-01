#include "network/lan_arena_replica.h"

#include <string.h>

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float interpolate_float(float before, float after, float alpha) {
    return before + (after - before) * alpha;
}

static void interpolate_actor(
    const SudekiMpLanArenaActorSnapshot *before,
    const SudekiMpLanArenaActorSnapshot *after,
    float alpha,
    SudekiMpLanArenaActorSnapshot *output
) {
    *output = *after;
    if (before->native_entity_id != after->native_entity_id ||
        before->actor_type != after->actor_type) return;
    output->x = interpolate_float(before->x, after->x, alpha);
    output->y = interpolate_float(before->y, after->y, alpha);
    output->z = interpolate_float(before->z, after->z, alpha);
    output->facing_x = interpolate_float(before->facing_x, after->facing_x, alpha);
    output->facing_z = interpolate_float(before->facing_z, after->facing_z, alpha);
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
    if (replica->latest_valid) {
        replica->previous = replica->latest;
        replica->previous_valid = 1u;
    }
    replica->latest = *snapshot;
    replica->latest_valid = 1u;
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
    if (!replica->previous_valid ||
        replica->previous.host_tick >= replica->latest.host_tick ||
        !enemy_layout_matches(&replica->previous, &replica->latest)) {
        return TRUE;
    }
    if (host_tick <= replica->previous.host_tick) {
        *sample = replica->previous;
        return TRUE;
    }
    if (host_tick >= replica->latest.host_tick) return TRUE;
    span = replica->latest.host_tick - replica->previous.host_tick;
    elapsed = host_tick - replica->previous.host_tick;
    alpha = clamp01((float)elapsed / (float)span);
    interpolate_actor(&replica->previous.tal, &replica->latest.tal,
        alpha, &sample->tal);
    interpolate_actor(&replica->previous.ailish, &replica->latest.ailish,
        alpha, &sample->ailish);
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
