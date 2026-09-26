#include "network/lan_arena_replica.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
    snapshot.seat[0].actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot.seat[0].native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot.seat[0].x = x;
    snapshot.seat[0].facing_z = 1.0f;
    snapshot.seat[0].hp = 10u;
    snapshot.seat[0].animation_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE :
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    snapshot.seat[0].combat_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE :
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    snapshot.seat[0].action_variant = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ACTION_NONE :
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO;
    snapshot.seat[0].action_phase_valid = sequence == 1u ? 0u : 1u;
    snapshot.seat[0].action_phase_q8 = sequence == 1u ? 0u :
        (uint16_t)(tick / 10u);
    snapshot.seat[1].actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot.seat[1].native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot.seat[1].x = x * 2.0f;
    snapshot.seat[1].facing_z = 1.0f;
    snapshot.seat[1].hp = 10u;
    snapshot.seat[1].animation_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE :
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    snapshot.seat[1].combat_state = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_COMBAT_IDLE :
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    snapshot.seat[1].action_variant = sequence == 1u ?
        SUDEKIMP_LAN_ARENA_ACTION_NONE :
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    snapshot.seat[1].action_phase_valid = sequence == 1u ? 0u : 1u;
    snapshot.seat[1].action_phase_q8 = sequence == 1u ? 0u :
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

static void test_ranged_aim_interpolation(void) {
    SudekiMpLanArenaReplica replica={0};
    SudekiMpLanArenaSnapshot first=make_snapshot(1,100,0), second, sample;
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    first.seat[1].actor_type=first.seat[1].native_entity_id=SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    first.seat[1].ranged_aim_valid=1;
    first.seat[1].ranged_aim[2]=32767;
    second=first; second.sequence=2; second.host_tick=200;
    second.seat[1].ranged_aim[1]=32767; second.seat[1].ranged_aim[2]=0;
    CHECK(SudekiMpLanArenaReplicaPush(&replica,&first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica,&second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica,150,&sample));
    CHECK(sample.seat[1].ranged_aim_valid && sample.seat[1].ranged_aim[0]==0);
    CHECK(abs(sample.seat[1].ranged_aim[1]-23170)<=1 && abs(sample.seat[1].ranged_aim[2]-23170)<=1);
    replica.latest.seat[1].ranged_aim[1]=0;
    replica.latest.seat[1].ranged_aim[2]=-32767;
    CHECK(SudekiMpLanArenaReplicaSample(&replica,150,&sample));
    CHECK(sample.seat[1].ranged_aim[2]==-32767); /* no zero ray at antipodal midpoint */
    replica.latest.seat[1].native_entity_id++;
    CHECK(SudekiMpLanArenaReplicaSample(&replica,150,&sample));
    CHECK(sample.seat[1].ranged_aim[2]==-32767); /* no blend across actor replacement */
    replica.latest.seat[1].ranged_aim_valid=0;
    memset(replica.latest.seat[1].ranged_aim,0,sizeof(replica.latest.seat[1].ranged_aim));
    CHECK(SudekiMpLanArenaReplicaSample(&replica,150,&sample));
    CHECK(!sample.seat[1].ranged_aim_valid && sample.seat[1].ranged_aim[2]==0);
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}

static void test_protected_clock_recovers_discarded_history(void) {
    SudekiMpLanArenaReplica replica = {0};
    SudekiMpLanArenaReplicaRenderClock clock = {0};
    SudekiMpLanArenaSnapshot frame, sample;
    uint32_t tick, i;
    for (i = 0; i < 4u; ++i) {
        frame = make_snapshot(i + 10u, 4000u + i * 50u, (float)i);
        CHECK(SudekiMpLanArenaReplicaPush(&replica, &frame));
    }
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1000u, FALSE, &tick));
    clock.host_tick = 3700u; /* No remaining sample can represent this time. */
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1013u, FALSE, &tick));
    CHECK(tick == 4100u);
    CHECK(tick <= replica.latest.host_tick);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1026u, FALSE, &tick));
    CHECK(tick == 4113u); /* Not a persistent 2x catch-up during the cast. */
    CHECK(SudekiMpLanArenaReplicaSample(&replica, tick, &sample));
    CHECK(fabsf(sample.seat[1].x - 4.52f) < 0.001f);
    /* The inclusive earliest boundary is still usable; no lost-frame reset. */
    clock.host_tick = 3987u;
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1039u, FALSE, &tick));
    CHECK(tick == 4000u);
    /* An elapsed frame already returning to buffered history needs no reset. */
    clock.host_tick = 3990u;
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1052u, FALSE, &tick));
    CHECK(tick == 4003u);
    /* Same recovery across the GetTickCount wrap. */
    replica.earliest.host_tick = UINT32_MAX - 99u;
    replica.oldest.host_tick = UINT32_MAX - 49u;
    replica.previous.host_tick = 0u;
    replica.latest.host_tick = 50u;
    clock.host_tick = UINT32_MAX - 300u;
    clock.local_tick = UINT32_MAX - 5u;
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 7u, FALSE, &tick));
    CHECK(tick == 0u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 20u, FALSE, &tick));
    CHECK(tick == 13u);
}

static void add_spirit_visual(
    SudekiMpLanArenaSnapshot *snapshot, uint32_t instance, uint32_t emitted,
    float x, float phase
) {
    SudekiMpLanArenaSpiritVfxSnapshot *visual;
    snapshot->seat[0].skill_sequence = 1u;
    snapshot->seat[0].skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
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
    SudekiMpLanArenaLocomotion *motion = &first.seat[1].locomotion;
    unsigned int i;
    clear_actor_action(&first.seat[0]);
    clear_actor_action(&first.seat[1]);
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
        second.seat[1].locomotion.time[i] = 12.0f;
        second.seat[1].locomotion.rate[i] = 26.0f;
    }
    second.seat[1].locomotion.blend[0] = 0.75f;
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    CHECK(sample.seat[1].locomotion.clip[0] == 4u);
    CHECK(sample.seat[1].locomotion.time[0] == 11.0f);
    CHECK(sample.seat[1].locomotion.rate[0] == 25.0f);
    CHECK(sample.seat[1].locomotion.blend[0] == 0.5f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 999u, &sample));
    CHECK(sample.seat[1].locomotion.time[0] == 12.0f); /* No extrapolation. */

    replica.latest.seat[1].locomotion.sequence = 2u;
    replica.latest.seat[1].locomotion.time[0] = 0.5f;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[1].locomotion.time[0] == 10.0f);
    CHECK(sample.seat[1].locomotion.sequence == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.seat[1].locomotion.time[0] == 0.5f);
    CHECK(sample.seat[1].locomotion.sequence == 2u);
    replica.latest.seat[1].locomotion.sequence = 1u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[1].locomotion.time[0] == 10.0f); /* Malformed wrap also fenced. */
    replica.latest = second;
    replica.latest.seat[1].locomotion.clip[0] = 8u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[1].locomotion.clip[0] == 4u);
    replica.latest = second;
    replica.latest.seat[1].locomotion.state[0] = 128u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[1].locomotion.state[0] == 0u);

    memset(&replica.latest.seat[1].locomotion, 0, sizeof(*motion));
    replica.latest.seat[1].skill_sequence = 1u;
    replica.latest.seat[1].skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    replica.latest.seat[1].skill_active = 1u;
    CHECK(SudekiMpLanArenaSnapshotValid(&replica.latest));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(!sample.seat[1].skill_active && sample.seat[1].locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.seat[1].skill_active && !sample.seat[1].locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));

    replica.latest.seat[1].skill_active = 0u;
    replica.latest.combat_enabled = 0u;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(!sample.seat[1].locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    replica.latest = second;
    replica.latest.seat[1].hp = 0u;
    replica.latest.seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED;
    replica.latest.seat[1].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_INCAPACITATED;
    memset(&replica.latest.seat[1].locomotion, 0, sizeof(*motion));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(!sample.seat[1].locomotion.valid);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));

    /* A firing layer must not replace the observed locomotion underneath. */
    replica.latest = second;
    replica.latest.seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    replica.latest.seat[1].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    replica.latest.seat[1].action_variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[1].locomotion.time[0] == 11.0f);
    CHECK(sample.seat[1].action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(!SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
}

static void test_buki_combat_stop_crossfade(void) {
    SudekiMpLanArenaReplica replica;
    SudekiMpLanArenaSnapshot a = make_snapshot(1u, 100u, 0.0f), b, sample;
    SudekiMpLanArenaLocomotion *m = &a.seat[0].locomotion;
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_BUKI_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    clear_actor_action(&a.seat[0]);
    clear_actor_action(&a.seat[1]);
    a.seat[0].actor_type = a.seat[0].native_entity_id = SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    a.combat_enabled = 1u;
    m->valid = 1u;
    m->sequence = 8u;
    /* The moving pair remains alive while channel 2 brings in combat idle. */
    m->clip[0] = 2; m->clip[1] = 3; m->clip[2] = 1;
    m->state[3] = 192;
    m->rate[0] = 37.0f; m->rate[1] = 31.0f;
    m->time[0] = 35.0f; m->time[1] = 29.0f;
    m->blend[0] = 0.99f; m->blend[2] = 0.1f;
    b = a; b.sequence = 2u; b.host_tick = 150u;
    b.seat[0].locomotion.time[0] += 1.85f;
    b.seat[0].locomotion.time[1] += 1.55f;
    b.seat[0].locomotion.blend[2] = 0.9f;
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &a));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &b));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 125u, &sample));
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    CHECK(sample.seat[0].locomotion.clip[2] == 1u);
    CHECK(fabsf(sample.seat[0].locomotion.blend[2] - 0.5f) < 0.0001f);
    CHECK(sample.seat[0].locomotion.rate[2] == 0.0f);
    /* The settled pair is a discrete handoff, never an interpolation into
     * empty/foreign channels or a replay of the outgoing attack. */
    a = b; b.sequence = 3u; b.host_tick = 200u;
    memset(&b.seat[0].locomotion, 0, sizeof(*m));
    m = &b.seat[0].locomotion;
    m->valid = 1; m->sequence = 9; m->clip[0] = 1;
    m->rate[0] = 12; m->time[0] = 0.2f;
    m->state[1] = m->state[2] = m->state[3] = 192;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &b));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 175u, &sample));
    CHECK(sample.seat[0].locomotion.sequence == 8u);
    CHECK(sample.seat[0].locomotion.clip[2] == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.seat[0].locomotion.sequence == 9u);
    CHECK(sample.seat[0].locomotion.clip[0] == 1u);
    CHECK(sample.seat[0].locomotion.clip[2] == 0u);
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}

static void test_buki_failed_combo_retry_timeline(void) {
    /* A failed host branch is an authored pose, not a new client combo.
     * Follow failure, cleanup, a successful retry, then running again. */
    static const int selectors[] = {53,54,70,20,53,54,55,20,23};
    SudekiMpLanArenaReplica r;
    SudekiMpLanArenaSnapshot a=make_snapshot(1,100,0), b, sample;
    unsigned int i;
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_BUKI_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    clear_actor_action(&a.seat[0]); clear_actor_action(&a.seat[1]);
    a.seat[0].actor_type=a.seat[0].native_entity_id=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    a.combat_enabled=1;
    a.seat[0].locomotion.valid=1;
    a.seat[0].locomotion.sequence=1;
    a.seat[0].locomotion.clip[0]=(uint8_t)SudekiMpLanArenaLocomotionClip(selectors[0],2);
    a.seat[0].locomotion.rate[0]=30;
    a.seat[0].locomotion.time[0]=3;
    a.seat[0].locomotion.state[1]=a.seat[0].locomotion.state[2]=
        a.seat[0].locomotion.state[3]=192;
    SudekiMpLanArenaReplicaReset(&r);
    CHECK(SudekiMpLanArenaReplicaPush(&r,&a));
    for (i=1; i<sizeof(selectors)/sizeof(selectors[0]); ++i) {
        b=a; ++b.sequence; b.host_tick+=100;
        ++b.seat[0].locomotion.sequence;
        b.seat[0].locomotion.clip[0]=(uint8_t)SudekiMpLanArenaLocomotionClip(selectors[i],2);
        b.seat[0].locomotion.time[0]=0.25f;
        CHECK(SudekiMpLanArenaSnapshotValid(&b));
        CHECK(SudekiMpLanArenaReplicaPush(&r,&b));
        CHECK(!SudekiMpLanArenaReplicaPush(&r,&a)); /* A delayed pose cannot rewind. */
        CHECK(!SudekiMpLanArenaReplicaPush(&r,&b)); /* Nor can a duplicate restart. */
        if (selectors[i]==70)
            CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&r));
        CHECK(SudekiMpLanArenaReplicaSample(&r,a.host_tick+50,&sample));
        CHECK(sample.seat[0].locomotion.sequence==a.seat[0].locomotion.sequence);
        CHECK(sample.seat[0].locomotion.clip[0]==a.seat[0].locomotion.clip[0]);
        CHECK(sample.seat[0].locomotion.time[0]==a.seat[0].locomotion.time[0]);
        CHECK(SudekiMpLanArenaReplicaSample(&r,b.host_tick,&sample));
        CHECK(sample.seat[0].locomotion.clip[0]==b.seat[0].locomotion.clip[0]);
        CHECK(sample.seat[0].locomotion.time[0]==0.25f);
        CHECK(SudekiMpLanArenaSnapshotValid(&sample));
        a=b;
    }
    /* Once only idle/run frames remain, normal backlog recovery is safe. */
    for (i=0; i<4; ++i) {
        ++a.sequence; a.host_tick+=100;
        CHECK(SudekiMpLanArenaReplicaPush(&r,&a));
    }
    CHECK(!SudekiMpLanArenaReplicaActionTimelineBuffered(&r));
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}

static void test_buki_block_phase_timeline(void) {
    SudekiMpLanArenaReplica r;
    SudekiMpLanArenaSnapshot a=make_snapshot(1,100,0),b,c,sample;
    SudekiMpLanArenaActorSnapshot *actor=&a.seat[0];
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_BUKI_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    clear_actor_action(actor); clear_actor_action(&a.seat[1]);
    actor->actor_type=actor->native_entity_id=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    a.combat_enabled=1;
    actor->animation_state=SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    actor->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_BLOCK;
    actor->action_variant=SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD;
    actor->action_sequence=10; actor->action_history_count=1;
    actor->action_history[0].sequence=10;
    actor->action_history[0].variant=actor->action_variant;
    actor->action_history[0].host_tick=100;
    actor->locomotion.valid=1; actor->locomotion.sequence=4;
    actor->locomotion.clip[0]=11; actor->locomotion.rate[0]=24;
    actor->locomotion.time[0]=30;
    actor->locomotion.state[1]=actor->locomotion.state[2]=actor->locomotion.state[3]=192;
    b=a; b.sequence=2; b.host_tick=150;
    b.seat[0].locomotion.time[0]=31.2f;
    SudekiMpLanArenaReplicaReset(&r);
    CHECK(SudekiMpLanArenaReplicaPush(&r,&a));
    CHECK(SudekiMpLanArenaReplicaPush(&r,&b));
    CHECK(SudekiMpLanArenaReplicaSample(&r,125,&sample));
    CHECK(fabsf(sample.seat[0].locomotion.time[0]-30.6f)<0.001f);
    CHECK(sample.seat[0].action_sequence==10); /* Hold is not repeated presses. */
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    b.seat[0].locomotion.sequence=5; b.seat[0].locomotion.time[0]=0.1f;
    SudekiMpLanArenaReplicaReset(&r);
    CHECK(SudekiMpLanArenaReplicaPush(&r,&a));
    CHECK(SudekiMpLanArenaReplicaPush(&r,&b));
    CHECK(SudekiMpLanArenaReplicaSample(&r,125,&sample));
    CHECK(sample.seat[0].locomotion.time[0]==30); /* Do not rewind a loop early. */
    b.seat[0].locomotion.clip[0]=12;
    b.seat[0].action_variant=SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE;
    b.seat[0].action_sequence=11; b.seat[0].action_history_count=2;
    b.seat[0].action_history[1].sequence=11;
    b.seat[0].action_history[1].variant=b.seat[0].action_variant;
    b.seat[0].action_history[1].host_tick=150;
    SudekiMpLanArenaReplicaReset(&r);
    CHECK(SudekiMpLanArenaReplicaPush(&r,&a));
    CHECK(SudekiMpLanArenaReplicaPush(&r,&b));
    CHECK(SudekiMpLanArenaReplicaSample(&r,125,&sample));
    CHECK(sample.seat[0].action_variant==SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD);
    CHECK(sample.seat[0].locomotion.clip[0]==11);
    CHECK(sample.seat[0].combat_state==SUDEKIMP_LAN_ARENA_COMBAT_BLOCK);
    CHECK(SudekiMpLanArenaReplicaSample(&r,150,&sample));
    CHECK(sample.seat[0].action_variant==SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE);
    CHECK(sample.seat[0].combat_state==SUDEKIMP_LAN_ARENA_COMBAT_BLOCK);
    CHECK(sample.seat[0].locomotion.clip[0]==12);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    c=b; c.sequence=3; c.host_tick=200;
    c.seat[0].animation_state=SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    c.seat[0].combat_state=SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    c.seat[0].action_variant=SUDEKIMP_LAN_ARENA_ACTION_NONE;
    c.seat[0].locomotion.sequence=6; c.seat[0].locomotion.clip[0]=1;
    CHECK(SudekiMpLanArenaReplicaPush(&r,&c));
    CHECK(SudekiMpLanArenaReplicaSample(&r,175,&sample));
    CHECK(sample.seat[0].action_variant==SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE);
    CHECK(sample.seat[0].locomotion.clip[0]==12);
    CHECK(SudekiMpLanArenaReplicaSample(&r,200,&sample));
    CHECK(sample.seat[0].action_variant==SUDEKIMP_LAN_ARENA_ACTION_NONE);
    CHECK(sample.seat[0].locomotion.clip[0]==1);
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    /* Resetting the replica discards the old session's hold timeline. */
    SudekiMpLanArenaReplicaReset(&r);
    CHECK(!SudekiMpLanArenaReplicaSample(&r,210,&sample));
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}

static void test_elco_spirit_view_interpolation(void) {
    SudekiMpLanArenaReplica replica = {0};
    SudekiMpLanArenaSnapshot a = make_snapshot(1u, 1000u, 0.0f), b, sample;
    unsigned int i;
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    a.combat_enabled = 1u;
    clear_actor_action(&a.seat[0]); clear_actor_action(&a.seat[1]);
    a.seat[1].actor_type = a.seat[1].native_entity_id = SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    a.seat[1].skill_sequence = 1u; a.seat[1].skill_active = 1u;
    a.seat[1].skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    a.seat[1].skill_presentation_valid = 1u; a.seat[1].skill_presentation_channel_count = 5u;
    a.seat[1].skill_presentation_selector[0] = 73; a.seat[1].skill_presentation_rate[0] = 24.0f;
    for (i = 1u; i < 5u; ++i) a.seat[1].skill_presentation_state[i] = 192u;
    a.cast[1].spirit_view.kind = 1u; a.cast[1].spirit_view.owner_seat = 1u; a.cast[1].spirit_view.skill_sequence = 1u;
    a.cast[1].spirit_view.matrix[0] = -1.0f;
    a.cast[1].spirit_view.matrix[5] = a.cast[1].spirit_view.matrix[10] = a.cast[1].spirit_view.matrix[15] = 1.0f;
    a.cast[1].spirit_view.matrix[12] = 2.0f;
    a.cast[1].spirit_view.projection[0] = 1.0f; a.cast[1].spirit_view.projection[1] = .1f; a.cast[1].spirit_view.projection[2] = 100.0f;
    a.cast[1].skill_fade.kind=2; a.cast[1].skill_fade.owner_seat=1; a.cast[1].skill_fade.skill_sequence=1;
    a.cast[1].skill_fade.rgb[0]=a.cast[1].skill_fade.rgb[1]=a.cast[1].skill_fade.rgb[2]=1.f;
    b = a; b.sequence = 2u; b.host_tick = 1050u;
    b.cast[1].skill_fade.rgb[0]=b.cast[1].skill_fade.rgb[1]=b.cast[1].skill_fade.rgb[2]=.2f;
    b.cast[1].spirit_view.matrix[12] = 4.0f; b.cast[1].spirit_view.body_hidden = 1u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &a));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &b));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1025u, &sample));
    CHECK(fabsf(sample.cast[1].spirit_view.matrix[12] - 3.0f) < .001f);
    CHECK(sample.cast[1].spirit_view.body_hidden == 0u);
    CHECK(fabsf(sample.cast[1].skill_fade.rgb[0]-.6f)<.001f && sample.cast[1].skill_fade.owner_seat==1);
    CHECK(SudekiMpLanArenaSpiritViewValid(&sample.cast[1].spirit_view));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1050u, &sample));
    CHECK(sample.cast[1].spirit_view.body_hidden == 1u);
    b.sequence = 3u; b.host_tick = 1100u; b.cast[1].spirit_view.kind = 2u;
    b.cast[1].spirit_view.matrix[12] = 10.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &b));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1075u, &sample));
    CHECK(sample.cast[1].spirit_view.kind == 1u && sample.cast[1].spirit_view.matrix[12] == 4.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1100u, &sample));
    CHECK(sample.cast[1].spirit_view.kind == 2u && sample.cast[1].spirit_view.matrix[12] == 10.0f);
    b.sequence=4; b.host_tick=1150;
    b.seat[1].skill_sequence=2; b.cast[1].spirit_view.skill_sequence=2;
    b.cast[1].skill_fade.skill_sequence=2; b.cast[1].skill_fade.rgb[0]=1.f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica,&b));
    CHECK(SudekiMpLanArenaReplicaSample(&replica,1125,&sample));
    CHECK(sample.cast[1].skill_fade.skill_sequence==1 &&
        !memcmp(&sample.cast[1].skill_fade.rgb[0],&b.cast[1].skill_fade.rgb[1],sizeof(float)));
    CHECK(SudekiMpLanArenaSnapshotValid(&sample));
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}

static void test_countdown_release_is_timeline_edge(void) {
    SudekiMpLanArenaReplica r={0};
    SudekiMpLanArenaSnapshot a=make_snapshot(1,1000,0),b,out;
    a.combat_enabled=1;
    clear_actor_action(&a.seat[0]); clear_actor_action(&a.seat[1]);
    a.seat[0].skill_kind=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    a.seat[0].skill_sequence=7; a.seat[0].skill_active=1;
    a.seat[0].skill_target_phase=2; a.seat[0].skill_target_remaining_ms=50;
    b=a; b.sequence=2; b.host_tick=1050;
    b.seat[0].skill_target_phase=3; b.seat[0].skill_target_remaining_ms=0;
    CHECK(SudekiMpLanArenaReplicaPush(&r,&a)); CHECK(SudekiMpLanArenaReplicaPush(&r,&b));
    CHECK(SudekiMpLanArenaReplicaSample(&r,1025,&out));
    CHECK(out.seat[0].skill_target_phase==2 && out.seat[0].skill_target_remaining_ms==50);
    {
        uint8_t phase=0; uint16_t ms=999;
        CHECK(SudekiMpLanArenaReplicaLatestSkillTiming(&r,0,&out.seat[0],&phase,&ms));
        CHECK(phase==3 && ms==0);
        CHECK(out.seat[0].skill_target_phase==2); /* does not change pose timeline */
        CHECK(!SudekiMpLanArenaReplicaLatestSkillTiming(&r,1,&out.seat[0],&phase,&ms));
        out.seat[0].skill_sequence++;
        CHECK(!SudekiMpLanArenaReplicaLatestSkillTiming(&r,0,&out.seat[0],&phase,&ms));
        out.seat[0].skill_sequence--; out.seat[0].skill_slot++;
        CHECK(!SudekiMpLanArenaReplicaLatestSkillTiming(&r,0,&out.seat[0],&phase,&ms));
    }
    CHECK(SudekiMpLanArenaReplicaSample(&r,1050,&out));
    CHECK(out.seat[0].skill_target_phase==3 && out.seat[0].skill_target_remaining_ms==0);
}

int main(void) {
    test_ranged_aim_interpolation();
    test_countdown_release_is_timeline_edge();
    test_elco_spirit_view_interpolation();
    test_buki_block_phase_timeline();
    test_protected_clock_recovers_discarded_history();
    test_buki_combat_stop_crossfade();
    test_buki_failed_combo_retry_timeline();
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
    CHECK(SudekiMpLanArenaClientNativeSkillTaskAllowed(0x05u, 0x0eu));
    CHECK(SudekiMpLanArenaClientNativeSkillTaskAllowed(0x0eu, 0x0eu));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0u, 0x0eu));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0x23u, 0x0eu));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0x01u, 0x0eu));
    CHECK(!SudekiMpLanArenaClientNativeSkillTaskAllowed(0x0eu, 0x05u));
    SudekiMpLanArenaSnapshot first = make_snapshot(1u, 100u, 0.0f);
    SudekiMpLanArenaSnapshot second = make_snapshot(2u, 200u, 10.0f);
    SudekiMpLanArenaSnapshot invalid;
    SudekiMpLanArenaSnapshot sample;
    SudekiMpLanArenaReplicaReset(&replica);
    CHECK(!SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[0].x == 0.0f);
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 99u, &sample));
    CHECK(sample.sequence == 1u);
    CHECK(sample.seat[0].x == 0.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 100u, &sample));
    CHECK(sample.sequence == 1u);
    CHECK(sample.seat[0].x == 0.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 150u, &sample));
    CHECK(sample.seat[0].x > 4.99f && sample.seat[0].x < 5.01f);
    CHECK(sample.seat[1].x > 9.99f && sample.seat[1].x < 10.01f);
    CHECK(sample.enemies[0].z > 14.99f && sample.enemies[0].z < 15.01f);
    CHECK(sample.seat[0].animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION);
    CHECK(sample.seat[0].action_phase_valid == 1u);
    CHECK(sample.seat[0].action_phase_q8 == 20u);
    CHECK(sample.seat[1].combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 200u, &sample));
    CHECK(sample.sequence == 2u);
    CHECK(sample.seat[0].x == 10.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 250u, &sample));
    CHECK(sample.sequence == 2u);
    CHECK(sample.seat[0].x == 10.0f);
    CHECK(!SudekiMpLanArenaReplicaPush(&replica, &first));
    invalid = second;
    invalid.sequence = 3u;
    invalid.seat[1].hp = 0u;
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
    first.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    first.seat[0].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    first.seat[0].action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    first.seat[0].action_phase_valid = 0u;
    first.seat[0].action_phase_q8 = 0u;
    second.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    second.seat[0].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    second.seat[0].action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    second.seat[0].action_phase_valid = 0u;
    second.seat[0].action_phase_q8 = 0u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1050u, &sample));
    CHECK(sample.seat[0].animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING);
    CHECK(sample.seat[0].x > 6.99f && sample.seat[0].x < 7.01f);
    CHECK(sample.seat[1].x > 13.99f && sample.seat[1].x < 14.01f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1100u, &sample));
    CHECK(sample.seat[0].animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE);
    CHECK(sample.seat[0].x == 12.0f);

    /* A 20 Hz snapshot may contain several host-observed action edges. The
     * render clock must present every journaled stage at its host tick rather
     * than collapsing a fast Tal chain to the newest selector. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(12u, 1200u, 0.0f);
    second = make_snapshot(13u, 1300u, 1.0f);
    first.seat[0].action_sequence = 20u;
    second.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    second.seat[0].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK;
    second.seat[0].action_variant = SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    second.seat[0].action_sequence = 23u;
    second.seat[0].action_history_count = 3u;
    second.seat[0].action_history[0].sequence = 21u;
    second.seat[0].action_history[0].variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    second.seat[0].action_history[0].host_tick = 1220u;
    second.seat[0].action_history[1].sequence = 22u;
    second.seat[0].action_history[1].variant =
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO;
    second.seat[0].action_history[1].host_tick = 1250u;
    second.seat[0].action_history[2].sequence = 23u;
    second.seat[0].action_history[2].variant =
        SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS;
    second.seat[0].action_history[2].host_tick = 1280u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1210u, &sample));
    CHECK(sample.seat[0].action_sequence == 20u);
    CHECK(sample.seat[0].action_variant == first.seat[0].action_variant);
    CHECK(sample.seat[0].action_phase_valid == 1u);
    CHECK(sample.seat[0].action_phase_q8 ==
        (uint16_t)(first.seat[0].action_phase_q8 + 61u));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1230u, &sample));
    CHECK(sample.seat[0].action_sequence == 21u);
    CHECK(sample.seat[0].action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1260u, &sample));
    CHECK(sample.seat[0].action_sequence == 22u);
    CHECK(sample.seat[0].action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1290u, &sample));
    CHECK(sample.seat[0].action_sequence == 23u);
    CHECK(sample.seat[0].action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS);
    CHECK(sample.seat[0].combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK);

    CHECK(SUDEKIMP_LAN_ARENA_SNAPSHOT_INTERVAL_MS == 50u);
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(20u, 2000u, 0.0f);
    second = make_snapshot(21u, 2100u, 0.0f);
    first.seat[0].facing_x = 1.0f;
    first.seat[0].facing_z = 0.0f;
    second.seat[0].facing_x = 0.0f;
    second.seat[0].facing_z = 1.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 2050u, &sample));
    CHECK(fabsf(sample.seat[0].facing_x - 0.7071067f) < 0.0002f);
    CHECK(fabsf(sample.seat[0].facing_z - 0.7071067f) < 0.0002f);
    CHECK(fabsf(sample.seat[0].facing_x * sample.seat[0].facing_x +
        sample.seat[0].facing_z * sample.seat[0].facing_z - 1.0f) < 0.0002f);
    CHECK(sample.seat[0].action_phase_valid == 1u);
    CHECK(sample.seat[0].action_phase_q8 == 205u);

    /* The host's first idle snapshot is the semantic action-retirement edge.
     * Hold the last authoritative action phase through the buffered segment,
     * then retire exactly at the host endpoint without a client timer. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(22u, 2200u, 0.0f);
    second = make_snapshot(23u, 2250u, 0.0f);
    first.seat[0].action_sequence = 7u;
    first.seat[0].action_phase_q8 = 35u * 256u + 128u;
    second.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    second.seat[0].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    second.seat[0].action_variant = SUDEKIMP_LAN_ARENA_ACTION_NONE;
    second.seat[0].action_sequence = 7u;
    second.seat[0].action_phase_valid = 0u;
    second.seat[0].action_phase_q8 = 0u;
    second.seat[0].action_terminal_phase_q8 = 40u * 256u;
    second.seat[0].idle_entry_phase_q8 = 2u * 256u;
    second.seat[0].action_retirement_valid = 1u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 2225u, &sample));
    CHECK(sample.seat[0].animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION);
    CHECK(sample.seat[0].action_phase_valid == 1u);
    CHECK(sample.seat[0].action_phase_q8 == 37u * 256u + 192u);
    CHECK(sample.seat[0].action_retirement_valid == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 2250u, &sample));
    CHECK(sample.seat[0].animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE);
    CHECK(sample.seat[0].action_phase_valid == 0u);
    CHECK(sample.seat[0].action_retirement_valid == 1u);
    CHECK(sample.seat[0].action_terminal_phase_q8 == 40u * 256u);
    CHECK(sample.seat[0].idle_entry_phase_q8 == 2u * 256u);

    /* A 180-degree wall/contact correction has no unique arc. It must never
     * create the zero vector that made the client reject an entire frame. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(30u, 3000u, 0.0f);
    second = make_snapshot(31u, 3100u, 0.0f);
    first.seat[1].facing_x = 1.0f;
    first.seat[1].facing_z = 0.0f;
    second.seat[1].facing_x = -1.0f;
    second.seat[1].facing_z = 0.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 3049u, &sample));
    CHECK(sample.seat[1].facing_x == 1.0f);
    CHECK(sample.seat[1].facing_z == 0.0f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 3050u, &sample));
    CHECK(sample.seat[1].facing_x == -1.0f);
    CHECK(sample.seat[1].facing_z == 0.0f);

    /* Host-approved native skills carry an exact-build renderer witness.
     * Start/stop remain discrete at their authoritative snapshot boundary,
     * while one stable selector topology interpolates its native clock. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(60u, 6000u, 0.0f);
    second = make_snapshot(61u, 6050u, 0.0f);
    second.seat[0].skill_sequence = 1u;
    second.seat[0].skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    second.seat[0].skill_slot = 0u;
    second.seat[0].skill_active = 1u;
    second.seat[0].skill_cost = 40u;
    second.seat[0].skill_presentation_valid = 1u;
    second.seat[0].skill_presentation_channel_count = 2u;
    second.seat[0].skill_presentation_selector[0] = 103;
    second.seat[0].skill_presentation_state[0] = 1u;
    second.seat[0].skill_presentation_rate[0] = 24.0f;
    second.seat[0].skill_presentation_time[0] = 10.0f;
    second.seat[0].skill_presentation_blend[0] = 1.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6025u, &sample));
    CHECK(sample.seat[0].skill_sequence == 0u);
    CHECK(sample.seat[0].skill_presentation_valid == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6050u, &sample));
    CHECK(sample.seat[0].skill_sequence == 1u);
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER);
    CHECK(sample.seat[0].skill_presentation_selector[0] == 103);
    invalid = second;
    invalid.sequence = 62u;
    invalid.host_tick = 6100u;
    invalid.seat[0].skill_presentation_time[0] = 20.0f;
    invalid.seat[0].skill_presentation_blend[0] = 0.5f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6075u, &sample));
    CHECK(fabsf(sample.seat[0].skill_presentation_time[0] - 15.0f) < 0.001f);
    CHECK(fabsf(sample.seat[0].skill_presentation_blend[0] - 0.75f) < 0.001f);
    CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));
    /* A character CSkill sidecar is optional. If its exact renderer witness
     * becomes unavailable, keep the same live transaction and switch to the
     * absent sidecar only at that snapshot's endpoint. */
    first = invalid;
    first.sequence = 63u;
    first.host_tick = 6150u;
    first.seat[0].skill_presentation_valid = 0u;
    first.seat[0].skill_presentation_channel_count = 0u;
    memset(first.seat[0].skill_presentation_selector, 0,
        sizeof(first.seat[0].skill_presentation_selector));
    memset(first.seat[0].skill_presentation_state, 0,
        sizeof(first.seat[0].skill_presentation_state));
    memset(first.seat[0].skill_presentation_rate, 0,
        sizeof(first.seat[0].skill_presentation_rate));
    memset(first.seat[0].skill_presentation_time, 0,
        sizeof(first.seat[0].skill_presentation_time));
    memset(first.seat[0].skill_presentation_blend, 0,
        sizeof(first.seat[0].skill_presentation_blend));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6125u, &sample));
    CHECK(sample.seat[0].skill_active == 1u);
    CHECK(sample.seat[0].skill_presentation_valid == 1u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6150u, &sample));
    CHECK(sample.seat[0].skill_sequence == 1u);
    CHECK(sample.seat[0].skill_active == 1u);
    CHECK(sample.seat[0].skill_presentation_valid == 0u);

    second = first;
    second.sequence = 64u;
    second.host_tick = 6200u;
    second.seat[0].skill_active = 0u;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6175u, &sample));
    CHECK(sample.seat[0].skill_sequence == 1u);
    CHECK(sample.seat[0].skill_active == 1u);
    CHECK(sample.seat[0].skill_presentation_valid == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 6200u, &sample));
    CHECK(sample.seat[0].skill_active == 0u);
    CHECK(sample.seat[0].skill_presentation_valid == 0u);

    /* Spirit is a separate presentation transaction even though it reuses
     * the bounded exact-build renderer payload. Start, selector topology,
     * and retirement remain discrete; only a sustained topology interpolates
     * clocks/blends. The SPIRIT discriminator and zero slot/cost are retained
     * throughout so a client dispatcher cannot mistake this for CSkill::Use. */
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(80u, 8000u, 0.0f);
    second = make_snapshot(81u, 8050u, 0.0f);
    clear_actor_action(&first.seat[0]);
    clear_actor_action(&first.seat[1]);
    clear_actor_action(&second.seat[0]);
    clear_actor_action(&second.seat[1]);
    second.seat[0].skill_sequence = 9u;
    second.seat[0].skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    second.seat[0].skill_active = 1u;
    second.seat[0].skill_presentation_valid = 1u;
    second.seat[0].skill_presentation_channel_count = 2u;
    second.seat[0].skill_presentation_selector[0] = 75;
    second.seat[0].skill_presentation_state[0] = 1u;
    second.seat[0].skill_presentation_state[1] = 192u;
    second.seat[0].skill_presentation_rate[0] = 24.0f;
    second.seat[0].skill_presentation_time[0] = 10.0f;
    second.seat[0].skill_presentation_blend[0] = 0.25f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8025u, &sample));
    CHECK(sample.seat[0].skill_sequence == 0u);
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE);
    CHECK(sample.seat[0].skill_active == 0u);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8050u, &sample));
    CHECK(sample.seat[0].skill_sequence == 9u);
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.seat[0].skill_slot == 0u);
    CHECK(sample.seat[0].skill_cost == 0u);
    CHECK(sample.seat[0].skill_active == 1u);
    CHECK(sample.seat[0].skill_presentation_selector[0] == 75);
    CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));

    invalid = second;
    invalid.sequence = 82u;
    invalid.host_tick = 8100u;
    invalid.seat[0].skill_presentation_time[0] = 20.0f;
    invalid.seat[0].skill_presentation_blend[0] = 0.75f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8075u, &sample));
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.seat[0].skill_presentation_selector[0] == 75);
    CHECK(fabsf(sample.seat[0].skill_presentation_time[0] - 15.0f) <
        0.001f);
    CHECK(fabsf(sample.seat[0].skill_presentation_blend[0] - 0.5f) <
        0.001f);

    first = invalid;
    first.sequence = 83u;
    first.host_tick = 8150u;
    first.seat[0].skill_presentation_selector[0] = 112;
    first.seat[0].skill_presentation_time[0] = 2.0f;
    CHECK(!SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(replica.latest.sequence == 82u);

    /* Selector 113 is an authored topology edge inside the same live Spirit
     * transaction. It must enter replica history instead of creating the
     * multi-second snapshot blackout seen when this frame was rejected. */
    first.seat[0].skill_presentation_selector[0] = 113;
    first.seat[0].skill_presentation_time[0] = 2.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8125u, &sample));
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.seat[0].skill_presentation_selector[0] == 75);
    CHECK(fabsf(sample.seat[0].skill_presentation_time[0] - 20.0f) <
        0.001f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8150u, &sample));
    CHECK(sample.seat[0].skill_presentation_selector[0] == 113);
    CHECK(fabsf(sample.seat[0].skill_presentation_time[0] - 2.0f) <
        0.001f);

    second = first;
    second.sequence = 84u;
    second.host_tick = 8200u;
    second.seat[0].skill_presentation_selector[0] = 114;
    second.seat[0].skill_presentation_time[0] = 3.0f;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8175u, &sample));
    CHECK(sample.seat[0].skill_presentation_selector[0] == 113);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8200u, &sample));
    CHECK(sample.seat[0].skill_presentation_selector[0] == 114);
    CHECK(fabsf(sample.seat[0].skill_presentation_time[0] - 3.0f) <
        0.001f);

    invalid = second;
    invalid.sequence = 85u;
    invalid.host_tick = 8250u;
    invalid.seat[0].skill_active = 0u;
    invalid.seat[0].skill_presentation_valid = 0u;
    invalid.seat[0].skill_presentation_channel_count = 0u;
    memset(invalid.seat[0].skill_presentation_selector, 0,
        sizeof(invalid.seat[0].skill_presentation_selector));
    memset(invalid.seat[0].skill_presentation_state, 0,
        sizeof(invalid.seat[0].skill_presentation_state));
    memset(invalid.seat[0].skill_presentation_rate, 0,
        sizeof(invalid.seat[0].skill_presentation_rate));
    memset(invalid.seat[0].skill_presentation_time, 0,
        sizeof(invalid.seat[0].skill_presentation_time));
    memset(invalid.seat[0].skill_presentation_blend, 0,
        sizeof(invalid.seat[0].skill_presentation_blend));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &invalid));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8225u, &sample));
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.seat[0].skill_active == 1u);
    CHECK(sample.seat[0].skill_presentation_selector[0] == 114);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 8250u, &sample));
    CHECK(sample.seat[0].skill_sequence == 9u);
    CHECK(sample.seat[0].skill_kind ==
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
    CHECK(sample.seat[0].skill_active == 0u);
    CHECK(sample.seat[0].skill_presentation_valid == 0u);

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
    CHECK(sample.seat[0].x > 9.49f && sample.seat[0].x < 9.51f);

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
    replica.oldest.seat[0].animation_state =
        SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    CHECK(SudekiMpLanArenaReplicaActionTimelineBuffered(&replica));
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1216u, FALSE, &first.host_tick));
    CHECK(first.host_tick == 4016u);
    CHECK(SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
        &replica, &clock, 1232u, FALSE, &first.host_tick));
    CHECK(first.host_tick == 4032u);
    replica.oldest.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.previous.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.latest.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.earliest.seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.oldest.seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.previous.seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.latest.seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    replica.earliest.seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
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
    CHECK(sample.seat[0].x > 8.19f && sample.seat[0].x < 8.21f);
    if (failures != 0) return 1;
    puts("lan arena replica tests passed");
    return 0;
}
