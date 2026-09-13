#include "network/lan_arena_shared_simulation.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpLanArenaActorSnapshot actor(uint8_t type) {
    SudekiMpLanArenaActorSnapshot result;
    memset(&result, 0, sizeof(result));
    result.actor_type = type;
    result.native_entity_id = type;
    result.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    result.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    result.facing_z = 1.0f;
    result.hp = 100u;
    result.sp = 20u;
    return result;
}

static SudekiMpLanArenaSnapshot frame(uint32_t host_tick) {
    SudekiMpLanArenaSnapshot result;
    memset(&result, 0, sizeof(result));
    result.host_tick = host_tick;
    result.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    result.seat[0] = actor(SUDEKIMP_LAN_ARENA_TAL_TYPE);
    result.seat[1] = actor(SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    return result;
}

static SudekiMpLanArenaInput player_input(
    uint8_t actor_type,
    uint32_t sequence
) {
    SudekiMpLanArenaInput result;
    memset(&result, 0, sizeof(result));
    result.sequence = sequence;
    result.client_tick = sequence * 10u;
    result.actor_type = actor_type;
    result.world_direction_z = 32767;
    return result;
}

static SudekiMpLanArenaNativeWorldObservation native_observation(
    const SudekiMpLanArenaSnapshot *source,
    uint8_t match_state,
    uint8_t combat_enabled
) {
    SudekiMpLanArenaNativeWorldObservation result;
    memset(&result, 0, sizeof(result));
    result.host_tick = source->host_tick;
    result.tal_hp = source->seat[0].hp;
    result.tal_sp = source->seat[0].sp;
    result.ailish_hp = source->seat[1].hp;
    result.ailish_sp = source->seat[1].sp;
    result.match_state = match_state;
    result.combat_enabled = combat_enabled;
    result.enemy_count = source->enemy_count;
    memcpy(result.enemies, source->enemies, sizeof(result.enemies));
    result.spirit_audio_history_count =
        source->spirit_audio_history_count;
    memcpy(result.spirit_audio_history,
        source->spirit_audio_history,
        sizeof(result.spirit_audio_history));
    result.spirit_vfx_observed = source->spirit_vfx_observed;
    result.spirit_view = source->spirit_view;
    result.skill_fade = source->skill_fade;
    result.spirit_vfx_count = source->spirit_vfx_count;
    memcpy(result.spirit_vfx, source->spirit_vfx, sizeof(result.spirit_vfx));
    result.native_combat_observed = 1u;
    result.native_resources_observed = 1u;
    result.native_enemies_observed = 1u;
    return result;
}

static SudekiMpLanArenaActorObservation actor_observation(
    const SudekiMpLanArenaActorSnapshot *source
) {
    SudekiMpLanArenaActorObservation result;
    memset(&result, 0, sizeof(result));
    result.actor = *source;
    result.native_actor_observed = 1u;
    return result;
}

static int commit_native_frame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaNativeWorldObservation *world,
    const SudekiMpLanArenaSnapshot *source
) {
    SudekiMpLanArenaActorObservation tal =
        actor_observation(&source->seat[0]);
    SudekiMpLanArenaActorObservation ailish =
        actor_observation(&source->seat[1]);
    return SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        simulation, session_token, world, &tal, &ailish);
}

static void test_native_world_owns_combat_state(void) {
    SudekiMpLanArenaSharedSimulation simulation;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaActorObservation tal_observation;
    SudekiMpLanArenaActorObservation ailish_observation;
    SudekiMpLanArenaSnapshot candidate = frame(100u);
    SudekiMpLanArenaSnapshot result;
    uint32_t revision = 0u;
    memset(&simulation, 0xa5, sizeof(simulation));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 77u));
    candidate.combat_enabled = 0u;
    observation = native_observation(
        &candidate, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    candidate.seat[0].hp = 999u;
    candidate.seat[1].sp = 999u;
    observation.tal_hp = 77u;
    observation.ailish_sp = 33u;
    observation.enemy_count = 1u;
    observation.enemies[0].native_entity_id =
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    observation.enemies[0].hp = 50u;
    observation.enemies[0].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    tal_observation = actor_observation(&candidate.seat[0]);
    ailish_observation = actor_observation(&candidate.seat[1]);
    tal_observation.native_actor_observed = 0u;
    CHECK(!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 77u, &observation,
        &tal_observation, &ailish_observation));
    candidate.seat[0].x = 3.0f;
    candidate.seat[1].x = -4.0f;
    candidate.seat[1].locomotion.valid = 1u;
    candidate.seat[1].locomotion.sequence = 17u;
    candidate.seat[1].locomotion.clip[0] = 4u;
    candidate.seat[1].locomotion.time[0] = 12.0f;
    candidate.seat[1].locomotion.rate[0] = 24.0f;
    CHECK(commit_native_frame(
        &simulation, 77u, &observation, &candidate));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &result, &revision));
    CHECK(result.combat_enabled == 1u);
    CHECK(result.match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE);
    CHECK(result.host_tick == 100u);
    CHECK(result.seat[0].hp == 77u);
    CHECK(result.seat[1].sp == 33u);
    CHECK(result.enemy_count == 1u);
    CHECK(result.enemies[0].native_entity_id ==
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID);
    CHECK(result.enemies[0].hp == 50u);
    CHECK(result.seat[0].x == 3.0f);
    CHECK(result.seat[1].x == -4.0f);
    CHECK(result.seat[1].locomotion.sequence == 17u);
    CHECK(result.seat[1].locomotion.clip[0] == 4u);
    CHECK(result.seat[1].locomotion.time[0] == 12.0f);
    CHECK(result.seat[1].locomotion.rate[0] == 24.0f);
    CHECK(revision == 1u);
}

static void test_roles_tokens_and_ticks_fail_closed(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(0xfffffff0u);
    SudekiMpLanArenaSnapshot output;
    uint32_t revision = 0u;
    CHECK(!SudekiMpLanArenaSharedSimulationBegin(
        &canonical, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_INVALID, 1u));
    CHECK(!SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 0u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 1u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 1u));
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 0u);
    CHECK(!commit_native_frame(
        &replica, 1u, &observation, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &canonical, 1u, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 2u, &source));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 1u, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 1u, &source));
    source.host_tick = 0x00000020u;
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 1u, &source));
    source.host_tick = 0xfffffff1u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 1u, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &replica, &output, &revision));
    CHECK(output.host_tick == 0x00000020u);
    CHECK(revision == 2u);
}

static void test_rejected_frame_is_transactional(void) {
    SudekiMpLanArenaSharedSimulation simulation;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output;
    uint32_t revision = 0u;
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 9u));
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 0u);
    CHECK(commit_native_frame(
        &simulation, 9u, &observation, &source));
    source.host_tick = 101u;
    observation.host_tick = 101u;
    observation.native_combat_observed = 0u;
    CHECK(!commit_native_frame(
        &simulation, 9u, &observation, &source));
    observation.native_combat_observed = 1u;
    observation.native_resources_observed = 0u;
    CHECK(!commit_native_frame(
        &simulation, 9u, &observation, &source));
    observation.native_resources_observed = 1u;
    observation.native_enemies_observed = 0u;
    CHECK(!commit_native_frame(
        &simulation, 9u, &observation, &source));
    observation.native_enemies_observed = 1u;
    source.host_tick = 101u;
    source.seat[0].facing_x = 0.0f;
    source.seat[0].facing_z = 0.0f;
    observation.host_tick = 101u;
    observation.combat_enabled = 1u;
    CHECK(!commit_native_frame(
        &simulation, 9u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, &revision));
    CHECK(output.host_tick == 100u);
    CHECK(output.combat_enabled == 0u);
    CHECK(revision == 1u);
    SudekiMpLanArenaSharedSimulationReset(&simulation);
    CHECK(!SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, &revision));
}

static void test_fresh_session_invalidates_old_frame(void) {
    SudekiMpLanArenaSharedSimulation simulation;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(500u);
    SudekiMpLanArenaSnapshot output;
    uint32_t revision = 99u;
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 11u));
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(
        &simulation, 11u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 12u));
    CHECK(!SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, &revision));
    CHECK(!commit_native_frame(
        &simulation, 11u, &observation, &source));
    source.host_tick = 1u;
    source.combat_enabled = 1u;
    observation.host_tick = 1u;
    observation.combat_enabled = 0u;
    CHECK(commit_native_frame(
        &simulation, 12u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, &revision));
    CHECK(output.combat_enabled == 0u);
    CHECK(revision == 1u);
}

static void test_player_input_admission_owns_snapshot_ack(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaInput ailish = player_input(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE, 20u);
    SudekiMpLanArenaInput tal = player_input(
        SUDEKIMP_LAN_ARENA_TAL_TYPE, 4u);
    SudekiMpLanArenaInput output_input;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output_frame;
    uint32_t revision = 0u;
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 55u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 55u));
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &replica, 55u, SUDEKIMP_LAN_ARENA_AILISH_TYPE, &ailish));
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 54u, SUDEKIMP_LAN_ARENA_AILISH_TYPE, &ailish));
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 55u, 0xffu, &ailish));
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 55u, SUDEKIMP_LAN_ARENA_TAL_TYPE, &ailish));
    ailish.weak_attack_pressed = 2u;
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 55u, SUDEKIMP_LAN_ARENA_AILISH_TYPE, &ailish));
    ailish.weak_attack_pressed = 1u;
    CHECK(SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 55u, SUDEKIMP_LAN_ARENA_AILISH_TYPE, &ailish));
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 55u, SUDEKIMP_LAN_ARENA_AILISH_TYPE, &ailish));
    CHECK(SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &canonical, 55u, SUDEKIMP_LAN_ARENA_TAL_TYPE, &tal));
    CHECK(SudekiMpLanArenaSharedSimulationReadPlayerInput(
        &canonical, SUDEKIMP_LAN_ARENA_AILISH_TYPE,
        &output_input, &revision));
    CHECK(output_input.sequence == 20u);
    CHECK(output_input.weak_attack_pressed == 1u);
    CHECK(revision == 1u);
    CHECK(SudekiMpLanArenaSharedSimulationReadPlayerInput(
        &canonical, SUDEKIMP_LAN_ARENA_TAL_TYPE,
        &output_input, &revision));
    CHECK(output_input.sequence == 4u);
    CHECK(revision == 1u);
    source.acknowledged_input = 999u;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 0u);
    CHECK(commit_native_frame(
        &canonical, 55u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output_frame, NULL));
    CHECK(output_frame.acknowledged_input == 20u);
}

static void test_replica_rejects_acknowledgement_regression(void) {
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output;
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 88u));
    source.acknowledged_input = 20u;
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 88u, &source));
    source.host_tick = 101u;
    source.acknowledged_input = 19u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 88u, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &replica, &output, NULL));
    CHECK(output.host_tick == 100u);
    CHECK(output.acknowledged_input == 20u);
    source.acknowledged_input = 21u;
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 88u, &source));
}

static void test_match_lifecycle_is_monotonic(void) {
    SudekiMpLanArenaSharedSimulation simulation;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output;
    SudekiMpLanArenaInput input = player_input(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 99u));
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(
        &simulation, 99u, &observation, &source));
    source.host_tick = 101u;
    observation.host_tick = 101u;
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_WAITING;
    observation.combat_enabled = 0u;
    CHECK(!commit_native_frame(
        &simulation, 99u, &observation, &source));
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ENDED;
    CHECK(commit_native_frame(
        &simulation, 99u, &observation, &source));
    source.host_tick = 102u;
    observation.host_tick = 102u;
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    CHECK(!commit_native_frame(
        &simulation, 99u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, NULL));
    CHECK(output.host_tick == 101u);
    CHECK(output.match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED);
    CHECK(output.combat_enabled == 0u);
    CHECK(!SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
        &simulation, 99u, SUDEKIMP_LAN_ARENA_AILISH_TYPE, &input));
}

static void set_character_skill(
    SudekiMpLanArenaActorSnapshot *value,
    uint16_t sequence,
    uint8_t slot,
    uint32_t cost,
    uint8_t active
) {
    value->skill_sequence = sequence;
    value->skill_kind = sequence == 0u ?
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE :
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    value->skill_slot = sequence == 0u ? 0u : slot;
    value->skill_cost = sequence == 0u ? 0u : cost;
    value->skill_active = sequence == 0u ? 0u : active;
}

static void set_spirit_skill(
    SudekiMpLanArenaActorSnapshot *value,
    uint16_t sequence,
    int32_t selector,
    uint8_t active
) {
    value->skill_sequence = sequence;
    value->skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    value->skill_slot = 0u;
    value->skill_cost = 0u;
    value->skill_active = active;
    value->skill_presentation_valid = active;
    value->skill_presentation_channel_count = active ? 2u : 0u;
    memset(value->skill_presentation_selector, 0,
        sizeof(value->skill_presentation_selector));
    memset(value->skill_presentation_state, 0,
        sizeof(value->skill_presentation_state));
    memset(value->skill_presentation_rate, 0,
        sizeof(value->skill_presentation_rate));
    memset(value->skill_presentation_time, 0,
        sizeof(value->skill_presentation_time));
    memset(value->skill_presentation_blend, 0,
        sizeof(value->skill_presentation_blend));
    if (!active) return;
    value->skill_presentation_selector[0] = selector;
    value->skill_presentation_state[0] = 1u;
    value->skill_presentation_state[1] = 192u;
    value->skill_presentation_rate[0] = 24.0f;
}

static void test_spirit_middle_stage_frames_continue(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(300u);
    SudekiMpLanArenaSnapshot output;
    uint32_t canonical_revision = 0u;
    uint32_t replica_revision = 0u;

    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 147u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 147u));

    set_spirit_skill(&source.seat[0], 12u, 75, 1u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(
        &canonical, 147u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, &canonical_revision));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 147u, &output));

    source.host_tick = 350u;
    set_spirit_skill(&source.seat[0], 12u, 113, 1u);
    source.seat[0].skill_presentation_time[0] = 1.0f;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(
        &canonical, 147u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, &canonical_revision));
    CHECK(output.host_tick == 350u &&
          output.seat[0].skill_sequence == 12u &&
          output.seat[0].skill_active == 1u &&
          output.seat[0].skill_presentation_selector[0] == 113);
    CHECK(canonical_revision == 2u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 147u, &output));

    source.host_tick = 400u;
    set_spirit_skill(&source.seat[0], 12u, 112, 1u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(!commit_native_frame(
        &canonical, 147u, &observation, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 147u, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, &canonical_revision));
    CHECK(output.host_tick == 350u &&
          output.seat[0].skill_presentation_selector[0] == 113 &&
          canonical_revision == 2u);

    set_spirit_skill(&source.seat[0], 12u, 114, 1u);
    source.seat[0].skill_presentation_time[0] = 2.0f;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(
        &canonical, 147u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, &canonical_revision));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 147u, &output));

    source.host_tick = 450u;
    set_spirit_skill(&source.seat[0], 12u, 0, 0u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(
        &canonical, 147u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, &canonical_revision));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 147u, &output));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &replica, &output, &replica_revision));
    CHECK(output.host_tick == 450u &&
          output.seat[0].skill_sequence == 12u &&
          output.seat[0].skill_active == 0u &&
          output.seat[0].skill_presentation_valid == 0u);
    CHECK(canonical_revision == 4u && replica_revision == 4u);
}

static void append_spirit_audio(
    SudekiMpLanArenaSnapshot *value,
    uint16_t event_sequence,
    uint16_t skill_sequence
) {
    SudekiMpLanArenaSpiritAudioSemanticEvent *event;
    if (value->spirit_audio_history_count ==
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY) {
        memmove(&value->spirit_audio_history[0],
            &value->spirit_audio_history[1],
            (SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY - 1u) *
                sizeof(value->spirit_audio_history[0]));
        --value->spirit_audio_history_count;
    }
    event = &value->spirit_audio_history[
        value->spirit_audio_history_count++];
    memset(event, 0, sizeof(*event));
    event->event_sequence = event_sequence;
    event->skill_sequence = skill_sequence;
    event->cue = SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START;
}

static void test_spirit_audio_journal_lifecycle(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output;
    unsigned int sequence;

    source.combat_enabled = 1u;
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 151u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 151u));
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, NULL));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &output));

    source.host_tick = 101u;
    set_spirit_skill(&source.seat[0], UINT16_MAX, 75, 1u);
    append_spirit_audio(&source, UINT16_MAX, UINT16_MAX);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, NULL));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &output));

    source.host_tick = 102u;
    source.seat[0].skill_presentation_time[0] = 1.0f;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, NULL));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &output));

    source.host_tick = 103u;
    source.spirit_audio_history[0].skill_sequence = UINT16_MAX - 1u;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(!commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &source));
    source.spirit_audio_history[0].skill_sequence = UINT16_MAX;

    set_spirit_skill(&source.seat[0], 1u, 75, 1u);
    append_spirit_audio(&source, 1u, 1u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, NULL));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &output));

    source.host_tick = 104u;
    set_spirit_skill(&source.seat[0], 3u, 75, 1u);
    append_spirit_audio(&source, 2u, 2u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(!commit_native_frame(&canonical, 151u, &observation, &source));
    /* Replica late-retained recovery intentionally differs here and is
     * exercised in test_replica_accepts_late_retained_spirit_audio(). */
    --source.spirit_audio_history_count;

    set_spirit_skill(&source.seat[0], 2u, 75, 1u);
    append_spirit_audio(&source, 2u, 2u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, NULL));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &output));

    /* Fill and then roll the bounded suffix. The latest accepted entry must
     * remain the overlap anchor when the oldest entry is evicted. */
    for (sequence = 3u; sequence <= 9u; ++sequence) {
        source.host_tick = 103u + sequence;
        set_spirit_skill(
            &source.seat[0], (uint16_t)sequence, 75, 1u);
        append_spirit_audio(
            &source, (uint16_t)sequence, (uint16_t)sequence);
        observation = native_observation(
            &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
        CHECK(commit_native_frame(
            &canonical, 151u, &observation, &source));
        CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
            &canonical, &output, NULL));
        CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
            &replica, 151u, &output));
    }
    CHECK(output.spirit_audio_history_count ==
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY);
    CHECK(output.spirit_audio_history[0].event_sequence == 2u);
    CHECK(output.spirit_audio_history[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY - 1u]
            .event_sequence == 9u);

    source.host_tick = 113u;
    source.spirit_audio_history[0].skill_sequence = 1u;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(!commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &source));
    source.spirit_audio_history[0].skill_sequence = 2u;
    source.spirit_audio_history_count = 0u;
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(!commit_native_frame(&canonical, 151u, &observation, &source));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 151u, &source));

    /* A new token resets both reducers, including their journal generation. */
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 152u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 152u));
    source = frame(1u);
    source.combat_enabled = 1u;
    set_spirit_skill(&source.seat[0], 1u, 75, 1u);
    append_spirit_audio(&source, 1u, 1u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&canonical, 152u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &canonical, &output, NULL));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 152u, &output));
}

static void test_replica_accepts_late_retained_spirit_audio(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot baseline = frame(200u);
    SudekiMpLanArenaSnapshot retired = frame(201u);
    SudekiMpLanArenaSnapshot stale_window;
    unsigned int index;

    baseline.combat_enabled = 1u;
    retired.combat_enabled = 1u;
    set_spirit_skill(&retired.seat[0], 7u, 75, 0u);
    append_spirit_audio(&retired, 4u, 7u);

    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 153u));
    observation = native_observation(
        &retired, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(!commit_native_frame(
        &canonical, 153u, &observation, &retired));

    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 153u));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 153u, &baseline));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 153u, &retired));
    retired.host_tick = 202u;
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 153u, &retired));

    /* A wholly replaced packet-gap window must advance both sequence
     * domains. A newer event window cannot smuggle stale skill identities. */
    stale_window = retired;
    stale_window.host_tick = 203u;
    set_spirit_skill(&stale_window.seat[0], 9u, 0, 0u);
    stale_window.spirit_audio_history_count = 0u;
    for (index = 0u;
         index < SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_HISTORY_CAPACITY;
         ++index) {
        append_spirit_audio(
            &stale_window, (uint16_t)(12u + index),
            (uint16_t)(1u + index));
    }
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 153u, &stale_window));

    /* A join-in-progress replica may receive the same retained journal in
     * its first datagram after the native Spirit transaction has retired. */
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 154u));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 154u, &retired));
}

static void test_replica_skill_lifecycle_is_monotonic(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output;
    uint32_t revision = 0u;

    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 144u));
    set_character_skill(&source.seat[0], 10u, 2u, 20u, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));

    source.host_tick = 101u;
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));
    source.host_tick = 102u;
    source.seat[0].skill_slot = 3u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));
    source.seat[0].skill_slot = 2u;
    source.seat[0].skill_cost = 21u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));
    source.seat[0].skill_cost = 20u;
    source.seat[0].skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    source.seat[0].skill_slot = 0u;
    source.seat[0].skill_cost = 0u;
    source.seat[0].skill_active = 0u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));

    set_character_skill(&source.seat[0], 10u, 2u, 20u, 0u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));
    source.host_tick = 103u;
    source.seat[0].skill_active = 1u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));

    set_character_skill(&source.seat[0], 11u, 2u, 20u, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));
    source.host_tick = 104u;
    set_character_skill(&source.seat[0], 10u, 2u, 20u, 1u);
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 144u, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &replica, &output, &revision));
    CHECK(output.host_tick == 103u && output.seat[0].skill_sequence == 11u);
    CHECK(revision == 4u);

    /* Skill sequences reserve zero and wrap in their own 16-bit domain. */
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &replica, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 145u));
    source = frame(0xfffffff0u);
    set_character_skill(&source.seat[1], UINT16_MAX, 5u, 40u, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 145u, &source));
    source.host_tick = 0x20u;
    set_character_skill(&source.seat[1], 1u, 5u, 40u, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 145u, &source));

    /* The canonical reducer enforces the same rule before a malformed local
     * observation can be serialized for any replica. */
    source = frame(200u);
    set_character_skill(&source.seat[0], 7u, 1u, 15u, 1u);
    observation = native_observation(
        &source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 0u);
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 146u));
    CHECK(commit_native_frame(
        &canonical, 146u, &observation, &source));
    source.host_tick = 201u;
    source.seat[0].skill_cost = 16u;
    observation.host_tick = 201u;
    CHECK(!commit_native_frame(
        &canonical, 146u, &observation, &source));
}

static SudekiMpLanArenaSpiritVfxSnapshot vfx_instance(uint32_t sequence) {
    SudekiMpLanArenaSpiritVfxSnapshot result;
    memset(&result, 0, sizeof(result));
    result.instance_sequence = sequence;
    result.skill_sequence = 7u;
    result.kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_SOUL_TRANSFER;
    result.emitted_host_tick = 90u;
    result.phase_valid = 1u;
    result.phase = 10.0f;
    result.position[1] = 5.0f;
    result.rotation_xyzw[3] = 1.0f;
    result.scale[0] = result.scale[1] = result.scale[2] = 1.0f;
    return result;
}

static void clear_vfx_roster(SudekiMpLanArenaSnapshot *value, uint8_t observed) {
    value->spirit_vfx_observed = observed;
    value->spirit_vfx_count = 0u;
    memset(value->spirit_vfx, 0, sizeof(value->spirit_vfx));
}

static void test_spirit_vfx_world_provenance(void) {
    SudekiMpLanArenaSharedSimulation canonical;
    SudekiMpLanArenaSharedSimulation before;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot source = frame(100u);
    SudekiMpLanArenaSnapshot output;
    set_spirit_skill(&source.seat[0], 7u, 0, 0u);
    observation = native_observation(&source, source.match_state, 1u);
    observation.spirit_vfx_observed = 1u;
    observation.spirit_vfx_count = 1u;
    observation.spirit_vfx[0] = vfx_instance(10u);
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&canonical,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 211u));
    CHECK(commit_native_frame(&canonical, 211u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(&canonical, &output, NULL));
    CHECK(source.spirit_vfx_count == 0u);
    CHECK(output.spirit_vfx_count == 1u);
    CHECK(output.spirit_vfx[0].instance_sequence == 10u);
    CHECK(canonical.spirit_vfx_instance_high_watermark == 10u);

    before = canonical;
    observation.host_tick = 101u;
    observation.spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_MORPH;
    CHECK(!commit_native_frame(&canonical, 211u, &observation, &source));
    CHECK(memcmp(&canonical, &before, sizeof(before)) == 0);
    observation.spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_SOUL_TRANSFER;
    observation.spirit_vfx_observed = 0u;
    CHECK(!commit_native_frame(&canonical, 211u, &observation, &source));
    observation.spirit_vfx_count = 0u;
    memset(observation.spirit_vfx, 0, sizeof(observation.spirit_vfx));
    CHECK(commit_native_frame(&canonical, 211u, &observation, &source));
    CHECK(canonical.spirit_vfx_last_observed_count == 1u);
    CHECK(canonical.spirit_vfx_instance_high_watermark == 10u);
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(&canonical, &output, NULL));
    CHECK(output.spirit_vfx_observed == 0u);
    CHECK(output.spirit_vfx_count == 0u);
}

static void test_spirit_vfx_roster_identity_and_unknown(void) {
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaSharedSimulation before;
    SudekiMpLanArenaSnapshot original = frame(100u);
    SudekiMpLanArenaSnapshot candidate;
    set_spirit_skill(&original.seat[0], 7u, 0, 0u);
    original.spirit_vfx_observed = 1u;
    original.spirit_vfx_count = 2u;
    original.spirit_vfx[0] = vfx_instance(10u);
    original.spirit_vfx[1] = vfx_instance(11u);
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&replica,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 212u));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &original));
    CHECK(replica.spirit_vfx_last_observed_count == 2u);
    CHECK(replica.spirit_vfx_instance_high_watermark == 11u);

    candidate = original;
    candidate.host_tick = 101u;
    clear_vfx_roster(&candidate, 0u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    CHECK(replica.spirit_vfx_last_observed_count == 2u);
    CHECK(replica.spirit_vfx_instance_high_watermark == 11u);
    before = replica;
    candidate = original;
    candidate.host_tick = 102u;
    candidate.spirit_vfx[0].kind = SUDEKIMP_LAN_ARENA_SPIRIT_VFX_END;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    CHECK(memcmp(&replica, &before, sizeof(before)) == 0);
    candidate.spirit_vfx[0] = original.spirit_vfx[0];
    candidate.spirit_vfx[0].skill_sequence = 6u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    candidate.spirit_vfx[0] = original.spirit_vfx[0];
    candidate.spirit_vfx[0].emitted_host_tick = 89u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    candidate.spirit_vfx[0] = original.spirit_vfx[0];
    candidate.spirit_vfx[0].phase = 1.0f; /* Authored loops may wrap. */
    candidate.spirit_vfx[0].position[0] = 99.0f;
    candidate.spirit_vfx[0].scale[1] = 2.0f;
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));

    candidate.host_tick = 103u;
    clear_vfx_roster(&candidate, 1u);
    candidate.spirit_vfx_count = 1u;
    candidate.spirit_vfx[0] = original.spirit_vfx[1];
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    CHECK(replica.spirit_vfx_last_observed_count == 1u);
    candidate = original;
    candidate.host_tick = 104u;
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate)); /* Removed 10 cannot reappear. */
    candidate.spirit_vfx[0] = vfx_instance(12u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate)); /* Array order is not serial order. */
    CHECK(replica.spirit_vfx_instance_high_watermark == 12u);
    candidate.host_tick = 105u;
    clear_vfx_roster(&candidate, 0u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    CHECK(replica.spirit_vfx_last_observed_count == 2u);
    candidate.host_tick = 106u;
    clear_vfx_roster(&candidate, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    CHECK(replica.spirit_vfx_last_observed_count == 0u);
    CHECK(replica.spirit_vfx_instance_high_watermark == 12u);
    candidate.host_tick = 107u;
    candidate.spirit_vfx_count = 1u;
    candidate.spirit_vfx[0] = vfx_instance(12u);
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    candidate.spirit_vfx[0] = vfx_instance(13u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &candidate));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&replica,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 213u));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 212u, &original));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 213u, &original));
}

static void test_spirit_vfx_instance_wraparound(void) {
    SudekiMpLanArenaSharedSimulation replica;
    SudekiMpLanArenaSnapshot candidate = frame(100u);
    set_spirit_skill(&candidate.seat[0], 7u, 0, 0u);
    candidate.spirit_vfx_observed = 1u;
    candidate.spirit_vfx_count = 2u;
    candidate.spirit_vfx[0] = vfx_instance(UINT32_MAX);
    candidate.spirit_vfx[1] = vfx_instance(UINT32_MAX - 1u);
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&replica,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 214u));
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 214u, &candidate));
    CHECK(replica.spirit_vfx_instance_high_watermark == UINT32_MAX);
    candidate.host_tick = 101u;
    candidate.spirit_vfx[1] = vfx_instance(1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 214u, &candidate));
    CHECK(replica.spirit_vfx_instance_high_watermark == 1u);
    candidate.host_tick = 102u;
    clear_vfx_roster(&candidate, 1u);
    CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 214u, &candidate));
    candidate.host_tick = 103u;
    candidate.spirit_vfx_count = 1u;
    candidate.spirit_vfx[0] = vfx_instance(UINT32_MAX);
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 214u, &candidate));
    candidate.spirit_vfx[0] = vfx_instance(UINT32_C(0x80000001));
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 214u, &candidate));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&replica,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 215u));
    candidate.spirit_vfx_count = 2u;
    candidate.spirit_vfx[1] = vfx_instance(1u);
    CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
        &replica, 215u, &candidate));
}

static void test_client_owned_spirit_commit(void) {
    SudekiMpLanArenaSharedSimulation host, client;
    SudekiMpLanArenaSnapshot source = frame(200u), output;
    SudekiMpLanArenaNativeWorldObservation observation;
    unsigned int cast;
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_BUKI_TYPE, SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    source.seat[0] = actor(SUDEKIMP_LAN_ARENA_BUKI_TYPE);
    source.seat[1] = actor(SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    source.combat_enabled = 1u;
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&host,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 153u));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&client,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA, 153u));
    observation = native_observation(&source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
    CHECK(commit_native_frame(&host, 153u, &observation, &source));
    for (cast = 1u; cast <= 2u; ++cast) {
        source.host_tick++;
        set_spirit_skill(&source.seat[1], (uint16_t)cast, cast == 1u ? 117 : 116, 1u);
        source.seat[1].skill_presentation_channel_count = 5u;
        source.spirit_view.kind = (uint8_t)cast;
        source.spirit_view.owner_seat = 1u;
        source.spirit_view.skill_sequence = (uint16_t)cast;
        source.spirit_view.body_hidden = (uint8_t)(cast - 1u);
        source.spirit_view.matrix[0] = -1.0f;
        source.spirit_view.matrix[5] = source.spirit_view.matrix[10] =
            source.spirit_view.matrix[15] = 1.0f;
        source.spirit_view.matrix[12] = 4.0f;
        source.spirit_view.projection[0] = 1.1f;
        source.spirit_view.projection[1] = .01f;
        source.spirit_view.projection[2] = 1000.0f;
        source.skill_fade.kind=2; source.skill_fade.owner_seat=1;
        source.skill_fade.skill_sequence=(uint16_t)cast;
        source.skill_fade.rgb[0]=source.skill_fade.rgb[1]=.15f; source.skill_fade.rgb[2]=.2f;
        append_spirit_audio(&source, (uint16_t)cast, (uint16_t)cast);
        source.spirit_audio_history[source.spirit_audio_history_count - 1u].owner_seat = 1u;
        observation = native_observation(&source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
        CHECK(commit_native_frame(&host, 153u, &observation, &source));
        CHECK(SudekiMpLanArenaSharedSimulationReadFrame(&host, &output, NULL));
        CHECK(output.seat[0].skill_active == 0u && output.seat[1].skill_active == 1u);
        CHECK(memcmp(&output.spirit_view, &source.spirit_view,
            sizeof(source.spirit_view)) == 0);
        CHECK(!memcmp(&output.skill_fade,&source.skill_fade,sizeof(source.skill_fade)));
        CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client, 153u, &output));
        observation.host_tick++; /* Fresh frame: reject the camera, not a stale tick. */
        observation.skill_fade.skill_sequence++;
        CHECK(!commit_native_frame(&host,153u,&observation,&source));
        observation.skill_fade=source.skill_fade;
        observation.spirit_view.owner_seat = 0u;
        CHECK(!commit_native_frame(&host, 153u, &observation, &source));
        observation.spirit_view.owner_seat = 1u;
        observation.spirit_view.skill_sequence++;
        CHECK(!commit_native_frame(&host, 153u, &observation, &source));
        source.host_tick++;
        source.spirit_audio_history[source.spirit_audio_history_count - 1u].owner_seat = 0u;
        observation = native_observation(&source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
        CHECK(!commit_native_frame(&host, 153u, &observation, &source));
        source.spirit_audio_history[source.spirit_audio_history_count - 1u].owner_seat = 1u;
        set_spirit_skill(&source.seat[1], (uint16_t)cast, 0, 0u);
        observation = native_observation(&source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
        CHECK(!commit_native_frame(&host, 153u, &observation, &source)); /* Stale camera. */
        memset(&source.spirit_view, 0, sizeof(source.spirit_view));
        observation = native_observation(&source, SUDEKIMP_LAN_ARENA_MATCH_ACTIVE, 1u);
        CHECK(commit_native_frame(&host, 153u, &observation, &source));
        CHECK(SudekiMpLanArenaSharedSimulationReadFrame(&host, &output, NULL));
        CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client, 153u, &output));
    }
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}

int main(void) {
    test_client_owned_spirit_commit();
    test_native_world_owns_combat_state();
    test_roles_tokens_and_ticks_fail_closed();
    test_rejected_frame_is_transactional();
    test_fresh_session_invalidates_old_frame();
    test_player_input_admission_owns_snapshot_ack();
    test_replica_rejects_acknowledgement_regression();
    test_match_lifecycle_is_monotonic();
    test_spirit_middle_stage_frames_continue();
    test_spirit_audio_journal_lifecycle();
    test_replica_accepts_late_retained_spirit_audio();
    test_replica_skill_lifecycle_is_monotonic();
    test_spirit_vfx_world_provenance();
    test_spirit_vfx_roster_identity_and_unknown();
    test_spirit_vfx_instance_wraparound();
    if (failures != 0) {
        fprintf(stderr, "%d shared simulation test(s) failed\n", failures);
        return 1;
    }
    puts("lan arena shared simulation tests passed");
    return 0;
}
