#include "network/lan_arena_replica.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); ++failures; \
} } while (0)

static SudekiMpLanArenaSnapshot make_snapshot(uint32_t sequence, uint32_t tick, float x) {
    SudekiMpLanArenaSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.sequence = sequence;
    snapshot.host_tick = tick;
    snapshot.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    snapshot.tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot.tal.native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot.tal.x = x;
    snapshot.tal.facing_z = 1.0f;
    snapshot.tal.hp = 10u;
    snapshot.tal.animation_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE :
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    snapshot.tal.combat_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE :
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    snapshot.tal.action_variant = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ACTION_NONE :
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO;
    snapshot.tal.action_phase_valid = sequence == 1u ? 0u : 1u;
    snapshot.tal.action_phase_q8 = sequence == 1u ? 0u :
        (uint16_t)(tick / 10u);
    snapshot.ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot.ailish.native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot.ailish.x = x * 2.0f;
    snapshot.ailish.facing_z = 1.0f;
    snapshot.ailish.hp = 10u;
    snapshot.ailish.animation_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE :
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    snapshot.ailish.combat_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE :
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    snapshot.ailish.action_variant = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ACTION_NONE :
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    snapshot.ailish.action_phase_valid = sequence == 1u ? 0u : 1u;
    snapshot.ailish.action_phase_q8 = sequence == 1u ? 0u :
        (uint16_t)(tick / 10u);
    snapshot.enemy_count = 1u;
    snapshot.enemies[0].native_entity_id =
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    snapshot.enemies[0].z = x * 3.0f;
    snapshot.enemies[0].hp = 10u;
    return snapshot;
}

static void clear_actor_action(SudekiMpLanArenaActorSnapshot *actor) {
    actor->animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    actor->combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    actor->action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    actor->action_sequence = 0u;
    actor->action_phase_q8 = 0u;
    actor->action_phase_valid = 0u;
    actor->action_terminal_phase_q8 = 0u;
    actor->idle_entry_phase_q8 = 0u;
    actor->action_retirement_valid = 0u;
    actor->action_history_count = 0u;
    memset(actor->action_history, 0, sizeof(actor->action_history));
}

static void add_spirit_visual(
    SudekiMpLanArenaSnapshot *snapshot, uint32_t instance, uint32_t emitted,
    float x, float phase
) {
    SudekiMpLanArenaSpiritVfxSnapshot *visual;
    snapshot->tal.skill_sequence = 1u;
    snapshot->tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot->spirit_vfx_observed = 1u;
    visual = &snapshot->spirit_vfx[snapshot->spirit_vfx_count++];
    memset(visual, 0, sizeof(*visual));
    visual->instance_sequence = instance;
    visual->skill_sequence = 1u;
    visual->kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_INITIATE;
    visual->emitted_host_tick = emitted;
    visual->phase_valid = 1u;
    visual->phase = phase;
    visual->position[0] = x;
    visual->rotation_xyzw[3] = 1.0f;
    visual->scale[0] = visual->scale[1] = visual->scale[2] = 1.0f;
}

static void test_spirit_visual_render_timeline(void) {
    SudekiMpLanArenaReplica replica;
    SudekiMpLanArenaSnapshot first = make_snapshot(1u, 100u, 0.0f);
    SudekiMpLanArenaSnapshot second = make_snapshot(2u, 200u, 0.0f);
    SudekiMpLanArenaSnapshot sample;
    unsigned int index;
    add_spirit_visual(&first, 1u, 90u, 0.0f, 10.0f);
    add_spirit_visual(&second, 1u, 90u, 10.0f, 30.0f);
    second.spirit_vfx[0].rotation_xyzw[3] = -1.0f;
    second.spirit_vfx[0].scale[0] = 3.0f;
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.spirit_vfx_observed == 1u && sample.spirit_vfx_count == 1u);
    CHECK(fabsf(sample.spirit_vfx[0].position[0] - 5.0f) < 0.001f);
    CHECK(fabsf(sample.spirit_vfx[0].phase - 20.0f) < 0.001f);
    CHECK(fabsf(sample.spirit_vfx[0].scale[0] - 2.0f) < 0.001f);
    CHECK(fabsf(sample.spirit_vfx[0].rotation_xyzw[3]) > 0.999f);

    /* An effect's captured birth, rather than packet arrival, controls its
     * first eligible render time; unknown observation never means stop. */
    add_spirit_visual(&second, 2u, 175u, 7.0f, 5.0f);
    replica.latest = second;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.spirit_vfx_count == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 180u, &sample));
    CHECK(sample.spirit_vfx_count == 2u);
    CHECK(fabsf(sample.spirit_vfx[1].phase - 5.0f) < 0.001f);
    replica.latest.spirit_vfx_observed = 0u;
    replica.latest.spirit_vfx_count = 0u;
    memset(replica.latest.spirit_vfx, 0, sizeof(replica.latest.spirit_vfx));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 180u, &sample));
    CHECK(sample.spirit_vfx_observed == 0u && sample.spirit_vfx_count == 0u);
    replica.latest.spirit_vfx_observed = 1u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 180u, &sample));
    CHECK(sample.spirit_vfx_observed == 1u && sample.spirit_vfx_count == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.spirit_vfx_observed == 1u && sample.spirit_vfx_count == 0u);

    /* Replacing an entire full roster inside one segment cannot truncate
     * its union into a purportedly complete list. */
    first.spirit_vfx_count = second.spirit_vfx_count = 0u;
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY; ++index) {
        add_spirit_visual(&first, index + 1u, 90u, 0.0f, 10.0f);
        add_spirit_visual(&second, index + 9u, 125u, 1.0f, 15.0f);
    }
    replica.previous = first;
    replica.latest = second;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.spirit_vfx_observed == 0u && sample.spirit_vfx_count == 0u);

    first.spirit_vfx_count = second.spirit_vfx_count = 0u;
    first.host_tick = 0xfffffff0u;
    second.host_tick = 0x22u;
    add_spirit_visual(&first, 20u, 0xffffff00u, 0.0f, 30.0f);
    add_spirit_visual(&second, 20u, 0xffffff00u, 10.0f, 5.0f);
    replica.previous = first;
    replica.latest = second;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 9u, &sample));
    CHECK(fabsf(sample.spirit_vfx[0].position[0] - 5.0f) < 0.001f);
    CHECK(sample.spirit_vfx[0].phase == 30.0f); /* native loop wrap */

    first = make_snapshot(1u, 100u, 0.0f);
    second = make_snapshot(2u, 200u, 10.0f);
    add_spirit_visual(&first, 1u, 90u, 0.0f, 10.0f);
    add_spirit_visual(&second, 1u, 90u, 10.0f, 30.0f);
    first.spirit_vfx[0].kind = second.spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST;
    first.spirit_vfx[0].skill_sequence = second.spirit_vfx[0].skill_sequence = 0u;
    first.spirit_vfx[0].owner_actor_type = second.spirit_vfx[0].owner_actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    replica.previous = first;
    replica.latest = second;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.spirit_vfx_observed == 1u && sample.spirit_vfx_count == 1u);
    CHECK(sample.spirit_vfx[0].owner_actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(fabsf(sample.spirit_vfx[0].position[0] - 5.0f) < 0.001f);
    replica.latest.spirit_vfx[0].owner_actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.spirit_vfx_observed == 0u && sample.spirit_vfx_count == 0u);
}

static void test_directional_locomotion_timeline(void) {
    SudekiMpLanArenaReplica replica;
    SudekiMpLanArenaSnapshot first = make_snapshot(1u, 100u, 0.0f);
    SudekiMpLanArenaSnapshot second, sample;
    SudekiMpLanArenaLocomotion *motion = &first.ailish.locomotion;
    unsigned int i;
    clear_actor_action(&first.tal);
    clear_actor_action(&first.ailish);
    first.combat_enabled = 1u;
    motion->valid = 1u;
    motion->sequence = 1u;
    for (i = 0u; i < 4u; ++i) {
        motion->clip[i] = (uint8_t)(4u + i); /* Back pair blending into left. */
        motion->rate[i] = 24.0f;
        motion->time[i] = 10.0f;
    }
    motion->blend[0] = 0.25f;
    second = first;
    second.sequence = 2u;
    second.host_tick = 200u;
    for (i = 0u; i < 4u; ++i) {
        second.ailish.locomotion.time[i] = 12.0f;
        second.ailish.locomotion.rate[i] = 26.0f;
    }
    second.ailish.locomotion.blend[0] = 0.75f;
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    CHECK(sample.ailish.locomotion.clip[0] == 4u);
    CHECK(sample.ailish.locomotion.time[0] == 11.0f);
    CHECK(sample.ailish.locomotion.rate[0] == 25.0f);
    CHECK(sample.ailish.locomotion.blend[0] == 0.5f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 999u, &sample));
    CHECK(sample.ailish.locomotion.time[0] == 12.0f); /* No extrapolation. */

    replica.latest.ailish.locomotion.sequence = 2u;
    replica.latest.ailish.locomotion.time[0] = 0.5f;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.ailish.locomotion.time[0] == 10.0f);
    CHECK(sample.ailish.locomotion.sequence == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.ailish.locomotion.time[0] == 0.5f);
    CHECK(sample.ailish.locomotion.sequence == 2u);
    replica.latest.ailish.locomotion.sequence = 1u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.ailish.locomotion.time[0] == 10.0f); /* Malformed wrap also fenced. */
    replica.latest = second;
    replica.latest.ailish.locomotion.clip[0] = 8u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.ailish.locomotion.clip[0] == 4u);
    replica.latest = second;
    replica.latest.ailish.locomotion.state[0] = 128u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.ailish.locomotion.state[0] == 0u);

    memset(&replica.latest.ailish.locomotion, 0, sizeof(*motion));
    replica.latest.ailish.skill_sequence = 1u;
    replica.latest.ailish.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    replica.latest.ailish.skill_active = 1u;
    CHECK(SudekiMpLanArenaSnapshotValid(&replica.latest));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(!sample.ailish.skill_active && sample.ailish.locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.ailish.skill_active && !sample.ailish.locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));

    replica.latest.ailish.skill_active = 0u;
    replica.latest.combat_enabled = 0u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(!sample.ailish.locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    replica.latest = second;
    replica.latest.ailish.hp = 0u;
    replica.latest.ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED;
    replica.latest.ailish.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED;
    memset(&replica.latest.ailish.locomotion, 0, sizeof(*motion));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(!sample.ailish.locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));

    /* A firing layer must not replace the observed locomotion underneath. */
    replica.latest = second;
    replica.latest.ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    replica.latest.ailish.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    replica.latest.ailish.action_variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.ailish.locomotion.time[0] == 11.0f);
    CHECK(sample.ailish.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(!SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
}

int main(void) {
    test_directional_locomotion_timeline();
    test_spirit_visual_render_timeline();
    SudekiMpLanArenaReplica replica;
    SudekiMpLanArenaReplicaRenderClock clock;

    CHECK(SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(2, FALSE));
    CHECK(SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(2, TRUE));
    CHECK(!SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(3, FALSE));
    CHECK(SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(3, TRUE));
    CHECK(!SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(0, TRUE));
    CHECK(!SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(4, TRUE));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0u, 0x01u));
    CHECK(SudekiMpLanArenaClientNativeSkillTaskAllowed(0x23u, 0x01u));
    CHECK(SudekiMpLanArenaClientNativeSkillTaskAllowed(0x01u, 0x01u));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0x01u, 0x23u));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0x05u, 0x01u));
    SudekiMpLanArenaSnapshot first = make_snapshot(1u, 100u, 0.0f);
    SudekiMpLanArenaSnapshot second = make_snapshot(2u, 200u, 10.0f);
    SudekiMpLanArenaSnapshot invalid;
    SudekiMpLanArenaSnapshot sample;
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(!SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.tal.x == 0.0f);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 99u, &sample));
    CHECK(sample.sequence == 1u);
    CHECK(sample.tal.x == 0.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 100u, &sample));
    CHECK(sample.sequence == 1u);
    CHECK(sample.tal.x == 0.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.tal.x > 4.99f && sample.tal.x < 5.01f);
    CHECK(sample.ailish.x > 9.99f && sample.ailish.x < 10.01f);
    CHECK(sample.enemies[0].z > 14.99f && sample.enemies[0].z < 15.01f);
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION);
    CHECK(sample.tal.action_phase_valid == 1u);
    CHECK(sample.tal.action_phase_q8 == 20u);
    CHECK(sample.ailish.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.sequence == 2u);
    CHECK(sample.tal.x == 10.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 250u, &sample));
    CHECK(sample.sequence == 2u);
    CHECK(sample.tal.x == 10.0f);
    CHECK(!SudekiMpLanArenaReplicaPush(&replica, &first));
    invalid = second;
    invalid.sequence = 3u;
    invalid.ailish.hp = 0u;
    CHECK(!SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(replica.latest.sequence == 2u);
    second.sequence = 3u;
    second.enemy_count = 0u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.enemy_count == 0u);
    /* Combat mode is a discrete native presentation boundary. Never blend
     * exploration and combat snapshots into one client frame. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(4u, 300u, 1.0f);
    second = make_snapshot(5u, 350u, 2.0f);
    first.combat_enabled = 0u;
    second.combat_enabled = 1u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(replica.stream_generation == 2u);
    CHECK(!replica.previous_valid);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 325u, &sample));
    CHECK(sample.sequence == 5u);
    CHECK(sample.combat_enabled == 1u);
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(10u, 1000u, 2.0f);
    second = make_snapshot(11u, 1100u, 12.0f);
    first.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    first.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    first.tal.action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    first.tal.action_phase_valid = 0u;
    first.tal.action_phase_q8 = 0u;
    second.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    second.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    second.tal.action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    second.tal.action_phase_valid = 0u;
    second.tal.action_phase_q8 = 0u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1050u, &sample));
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING);
    CHECK(sample.tal.x > 6.99f && sample.tal.x < 7.01f);
    CHECK(sample.ailish.x > 13.99f && sample.ailish.x < 14.01f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1100u, &sample));
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE);
    CHECK(sample.tal.x == 12.0f);

    /* A 20 Hz snapshot may contain several host-observed action edges. The
     * render clock must present every journaled stage at its host tick rather
     * than collapsing a fast Tal chain to the newest selector. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(12u, 1200u, 0.0f);
    second = make_snapshot(13u, 1300u, 1.0f);
    first.tal.action_sequence = 20u;
    second.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    second.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK;
    second.tal.action_variant = SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    second.tal.action_sequence = 23u;
    second.tal.action_history_count = 3u;
    second.tal.action_history[0].sequence = 21u;
    second.tal.action_history[0].variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    second.tal.action_history[0].host_tick = 1220u;
    second.tal.action_history[1].sequence = 22u;
    second.tal.action_history[1].variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO;
    second.tal.action_history[1].host_tick = 1250u;
    second.tal.action_history[2].sequence = 23u;
    second.tal.action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    second.tal.action_history[2].host_tick = 1280u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1210u, &sample));
    CHECK(sample.tal.action_sequence == 20u);
    CHECK(sample.tal.action_variant == first.tal.action_variant);
    CHECK(sample.tal.action_phase_valid == 1u);
    CHECK(sample.tal.action_phase_q8 ==
        (uint16_t)(first.tal.action_phase_q8 + 61u));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1230u, &sample));
    CHECK(sample.tal.action_sequence == 21u);
    CHECK(sample.tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1260u, &sample));
    CHECK(sample.tal.action_sequence == 22u);
    CHECK(sample.tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1290u, &sample));
    CHECK(sample.tal.action_sequence == 23u);
    CHECK(sample.tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS);
    CHECK(sample.tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK);

    CHECK(SUDEKIMP_LAN_ARENA_SNAPSHOT_INTERVAL_MS == 50u);
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(20u, 2000u, 0.0f);
    second = make_snapshot(21u, 2100u, 0.0f);
    first.tal.facing_x = 1.0f;
    first.tal.facing_z = 0.0f;
    second.tal.facing_x = 0.0f;
    second.tal.facing_z = 1.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 2050u, &sample));
    CHECK(fabsf(sample.tal.facing_x - 0.7071067f) < 0.0002f);
    CHECK(fabsf(sample.tal.facing_z - 0.7071067f) < 0.0002f);
    CHECK(fabsf(sample.tal.facing_x * sample.tal.facing_x +
        sample.tal.facing_z * sample.tal.facing_z - 1.0f) < 0.0002f);
    CHECK(sample.tal.action_phase_valid == 1u);
    CHECK(sample.tal.action_phase_q8 == 205u);

    /* The host's first idle snapshot is the semantic action-retirement edge.
     * Hold the last authoritative action phase through the buffered segment,
     * then retire exactly at the host endpoint without a client timer. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(22u, 2200u, 0.0f);
    second = make_snapshot(23u, 2250u, 0.0f);
    first.tal.action_sequence = 7u;
    first.tal.action_phase_q8 = 35u * 256u + 128u;
    second.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    second.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    second.tal.action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    second.tal.action_sequence = 7u;
    second.tal.action_phase_valid = 0u;
    second.tal.action_phase_q8 = 0u;
    second.tal.action_terminal_phase_q8 = 40u * 256u;
    second.tal.idle_entry_phase_q8 = 2u * 256u;
    second.tal.action_retirement_valid = 1u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 2225u, &sample));
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION);
    CHECK(sample.tal.action_phase_valid == 1u);
    CHECK(sample.tal.action_phase_q8 == 37u * 256u + 192u);
    CHECK(sample.tal.action_retirement_valid == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 2250u, &sample));
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE);
    CHECK(sample.tal.action_phase_valid == 0u);
    CHECK(sample.tal.action_retirement_valid == 1u);
    CHECK(sample.tal.action_terminal_phase_q8 == 40u * 256u);
    CHECK(sample.tal.idle_entry_phase_q8 == 2u * 256u);

    /* A 180-degree wall/contact correction has no unique arc. It must never
     * create the zero vector that made the client reject an entire frame. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(30u, 3000u, 0.0f);
    second = make_snapshot(31u, 3100u, 0.0f);
    first.ailish.facing_x = 1.0f;
    first.ailish.facing_z = 0.0f;
    second.ailish.facing_x = -1.0f;
    second.ailish.facing_z = 0.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 3049u, &sample));
    CHECK(sample.ailish.facing_x == 1.0f);
    CHECK(sample.ailish.facing_z == 0.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 3050u, &sample));
    CHECK(sample.ailish.facing_x == -1.0f);
    CHECK(sample.ailish.facing_z == 0.0f);

    /* Host-approved native skills carry an exact-build renderer witness.
     * Start/stop remain discrete at their authoritative snapshot boundary,
     * while one stable selector topology interpolates its native clock. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(60u, 6000u, 0.0f);
    second = make_snapshot(61u, 6050u, 0.0f);
    second.tal.skill_sequence = 1u;
    second.tal.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    second.tal.skill_slot = 0u;
    second.tal.skill_active = 1u;
    second.tal.skill_cost = 40u;
    second.tal.skill_presentation_valid = 1u;
    second.tal.skill_presentation_channel_count = 2u;
    second.tal.skill_presentation_selector[0] = 103;
    second.tal.skill_presentation_state[0] = 1u;
    second.tal.skill_presentation_rate[0] = 24.0f;
    second.tal.skill_presentation_time[0] = 10.0f;
    second.tal.skill_presentation_blend[0] = 1.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6025u, &sample));
    CHECK(sample.tal.skill_sequence == 0u);
    CHECK(sample.tal.skill_presentation_valid == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6050u, &sample));
    CHECK(sample.tal.skill_sequence == 1u);
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER);
    CHECK(sample.tal.skill_presentation_selector[0] == 103);
    invalid = second;
    invalid.sequence = 62u;
    invalid.host_tick = 6100u;
    invalid.tal.skill_presentation_time[0] = 20.0f;
    invalid.tal.skill_presentation_blend[0] = 0.5f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6075u, &sample));
    CHECK(fabsf(sample.tal.skill_presentation_time[0] - 15.0f) < 0.001f);
    CHECK(fabsf(sample.tal.skill_presentation_blend[0] - 0.75f) < 0.001f);
    CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));
    /* A character CSkill sidecar is optional. If its exact renderer witness
     * becomes unavailable, keep the same live transaction and switch to the
     * absent sidecar only at that snapshot's endpoint. */
    first = invalid;
    first.sequence = 63u;
    first.host_tick = 6150u;
    first.tal.skill_presentation_valid = 0u;
    first.tal.skill_presentation_channel_count = 0u;
    memset(first.tal.skill_presentation_selector, 0,
        sizeof(first.tal.skill_presentation_selector));
    memset(first.tal.skill_presentation_state, 0,
        sizeof(first.tal.skill_presentation_state));
    memset(first.tal.skill_presentation_rate, 0,
        sizeof(first.tal.skill_presentation_rate));
    memset(first.tal.skill_presentation_time, 0,
        sizeof(first.tal.skill_presentation_time));
    memset(first.tal.skill_presentation_blend, 0,
        sizeof(first.tal.skill_presentation_blend));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6125u, &sample));
    CHECK(sample.tal.skill_active == 1u);
    CHECK(sample.tal.skill_presentation_valid == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6150u, &sample));
    CHECK(sample.tal.skill_sequence == 1u);
    CHECK(sample.tal.skill_active == 1u);
    CHECK(sample.tal.skill_presentation_valid == 0u);

    second = first;
    second.sequence = 64u;
    second.host_tick = 6200u;
    second.tal.skill_active = 0u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6175u, &sample));
    CHECK(sample.tal.skill_sequence == 1u);
    CHECK(sample.tal.skill_active == 1u);
    CHECK(sample.tal.skill_presentation_valid == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6200u, &sample));
    CHECK(sample.tal.skill_active == 0u);
    CHECK(sample.tal.skill_presentation_valid == 0u);

    /* Spirit is a separate presentation transaction even though it reuses
     * the bounded exact-build renderer payload. Start, selector topology,
     * and retirement remain discrete; only a sustained topology interpolates
     * clocks/blends. The SPIRIT discriminator and zero slot/cost are retained
     * throughout so a client dispatcher cannot mistake this for CSkill::Use. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(80u, 8000u, 0.0f);
    second = make_snapshot(81u, 8050u, 0.0f);
    clear_actor_action(&first.tal);
    clear_actor_action(&first.ailish);
    clear_actor_action(&second.tal);
    clear_actor_action(&second.ailish);
    second.tal.skill_sequence = 9u;
    second.tal.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    second.tal.skill_active = 1u;
    second.tal.skill_presentation_valid = 1u;
    second.tal.skill_presentation_channel_count = 2u;
    second.tal.skill_presentation_selector[0] = 75;
    second.tal.skill_presentation_state[0] = 1u;
    second.tal.skill_presentation_state[1] = 192u;
    second.tal.skill_presentation_rate[0] = 24.0f;
    second.tal.skill_presentation_time[0] = 10.0f;
    second.tal.skill_presentation_blend[0] = 0.25f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8025u, &sample));
    CHECK(sample.tal.skill_sequence == 0u);
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE);
    CHECK(sample.tal.skill_active == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8050u, &sample));
    CHECK(sample.tal.skill_sequence == 9u);
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.tal.skill_slot == 0u);
    CHECK(sample.tal.skill_cost == 0u);
    CHECK(sample.tal.skill_active == 1u);
    CHECK(sample.tal.skill_presentation_selector[0] == 75);
    CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));

    invalid = second;
    invalid.sequence = 82u;
    invalid.host_tick = 8100u;
    invalid.tal.skill_presentation_time[0] = 20.0f;
    invalid.tal.skill_presentation_blend[0] = 0.75f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8075u, &sample));
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.tal.skill_presentation_selector[0] == 75);
    CHECK(fabsf(sample.tal.skill_presentation_time[0] - 15.0f) <
        0.001f);
    CHECK(fabsf(sample.tal.skill_presentation_blend[0] - 0.5f) <
        0.001f);

    first = invalid;
    first.sequence = 83u;
    first.host_tick = 8150u;
    first.tal.skill_presentation_selector[0] = 112;
    first.tal.skill_presentation_time[0] = 2.0f;
    CHECK(!SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(replica.latest.sequence == 82u);

    /* Selector 113 is an authored topology edge inside the same live Spirit
     * transaction. It must enter replica history instead of creating the
     * multi-second snapshot blackout seen when this frame was rejected. */
    first.tal.skill_presentation_selector[0] = 113;
    first.tal.skill_presentation_time[0] = 2.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8125u, &sample));
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.tal.skill_presentation_selector[0] == 75);
    CHECK(fabsf(sample.tal.skill_presentation_time[0] - 20.0f) <
        0.001f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8150u, &sample));
    CHECK(sample.tal.skill_presentation_selector[0] == 113);
    CHECK(fabsf(sample.tal.skill_presentation_time[0] - 2.0f) <
        0.001f);

    second = first;
    second.sequence = 84u;
    second.host_tick = 8200u;
    second.tal.skill_presentation_selector[0] = 114;
    second.tal.skill_presentation_time[0] = 3.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8175u, &sample));
    CHECK(sample.tal.skill_presentation_selector[0] == 113);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8200u, &sample));
    CHECK(sample.tal.skill_presentation_selector[0] == 114);
    CHECK(fabsf(sample.tal.skill_presentation_time[0] - 3.0f) <
        0.001f);

    invalid = second;
    invalid.sequence = 85u;
    invalid.host_tick = 8250u;
    invalid.tal.skill_active = 0u;
    invalid.tal.skill_presentation_valid = 0u;
    invalid.tal.skill_presentation_channel_count = 0u;
    memset(invalid.tal.skill_presentation_selector, 0,
        sizeof(invalid.tal.skill_presentation_selector));
    memset(invalid.tal.skill_presentation_state, 0,
        sizeof(invalid.tal.skill_presentation_state));
    memset(invalid.tal.skill_presentation_rate, 0,
        sizeof(invalid.tal.skill_presentation_rate));
    memset(invalid.tal.skill_presentation_time, 0,
        sizeof(invalid.tal.skill_presentation_time));
    memset(invalid.tal.skill_presentation_blend, 0,
        sizeof(invalid.tal.skill_presentation_blend));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8225u, &sample));
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.tal.skill_active == 1u);
    CHECK(sample.tal.skill_presentation_selector[0] == 114);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8250u, &sample));
    CHECK(sample.tal.skill_sequence == 9u);
    CHECK(sample.tal.skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.tal.skill_active == 0u);
    CHECK(sample.tal.skill_presentation_valid == 0u);

    /* Packet arrivals may be early or late. Presentation starts one snapshot
     * behind latest, advances monotonically, and never jumps to arrival time. */
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaReplicaRenderClockReset(&clock);
    first = make_snapshot(40u, 4000u, 0.0f);
    second = make_snapshot(41u, 4050u, 5.0f);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(!SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1000u, &first.host_tick));
    invalid = make_snapshot(42u, 4100u, 10.0f);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1017u, &first.host_tick));
    CHECK(first.host_tick == 4050u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1042u, &first.host_tick));
    CHECK(first.host_tick == 4075u);
    /* A new packet arriving at this point cannot jump or rewind the clock. */
    invalid = make_snapshot(43u, 4150u, 15.0f);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1052u, &first.host_tick));
    CHECK(first.host_tick == 4095u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, first.host_tick, &sample));
    CHECK(sample.tal.x > 9.49f && sample.tal.x < 9.51f);

    /* A client that has accumulated a large backlog catches up at a bounded
     * 2x rate instead of preserving that visible delay forever. */
    clock.host_tick = 4000u;
    clock.local_tick = 1100u;
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1116u, &first.host_tick));
    CHECK(first.host_tick == 4032u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1132u, &first.host_tick));
    CHECK(first.host_tick == 4064u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1148u, &first.host_tick));
    CHECK(first.host_tick == 4096u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1164u, &first.host_tick));
    CHECK(first.host_tick == 4112u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 1180u, &first.host_tick));
    CHECK(first.host_tick == 4128u);

    /* Backlog convergence is forbidden while an authoritative action is in
     * the interpolation window. One local millisecond must remain one host
     * animation millisecond so catch-up cannot visibly skip combo frames. */
    clock.host_tick = 4000u;
    clock.local_tick = 1200u;
    replica.oldest.tal.animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1216u, FALSE, &first.host_tick));
    CHECK(first.host_tick == 4016u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1232u, FALSE, &first.host_tick));
    CHECK(first.host_tick == 4032u);
    replica.oldest.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.previous.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.latest.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.earliest.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.oldest.ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.previous.ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.latest.ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.earliest.ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    CHECK(!SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1248u, TRUE, &first.host_tick));
    CHECK(first.host_tick == 4064u);

    /* Both the host and local clocks are GetTickCount values. Their natural
     * 32-bit wrap must preserve ordering and interpolation. */
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaReplicaRenderClockReset(&clock);
    first = make_snapshot(50u, 0xfffffff0u, 0.0f);
    second = make_snapshot(51u, 0x00000022u, 5.0f);
    invalid = make_snapshot(52u, 0x00000054u, 10.0f);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 0xfffffff8u, &first.host_tick));
    CHECK(first.host_tick == 0x00000022u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvance(
        &replica, &clock, 0x00000018u, &first.host_tick));
    CHECK(first.host_tick == 0x00000042u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, first.host_tick, &sample));
    CHECK(sample.tal.x > 8.19f && sample.tal.x < 8.21f);
    if (failures != 0) return 1;
    puts("lan arena replica tests passed");
    return 0;
}
