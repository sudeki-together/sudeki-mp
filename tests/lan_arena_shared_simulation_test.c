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
    result.tal = actor(SUDEKIMP_LAN_ARENA_TAL_TYPE);
    result.ailish = actor(SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    return result;
}

static void test_native_world_owns_combat_state(void) {
    SudekiMpLanArenaSharedSimulation simulation;
    SudekiMpLanArenaNativeWorldObservation observation;
    SudekiMpLanArenaSnapshot candidate = frame(100u);
    SudekiMpLanArenaSnapshot result;
    uint32_t revision = 0u;
    memset(&simulation, 0xa5, sizeof(simulation));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 77u));
    candidate.combat_enabled = 0u;
    observation.host_tick = 100u;
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    observation.combat_enabled = 1u;
    observation.native_combat_observed = 1u;
    CHECK(SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 77u, &observation, &candidate));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &result, &revision));
    CHECK(result.combat_enabled == 1u);
    CHECK(result.match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE);
    CHECK(result.host_tick == 100u);
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
    observation.host_tick = source.host_tick;
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    observation.combat_enabled = 0u;
    observation.native_combat_observed = 1u;
    CHECK(!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
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
    observation.host_tick = 100u;
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    observation.combat_enabled = 0u;
    observation.native_combat_observed = 1u;
    CHECK(SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 9u, &observation, &source));
    source.host_tick = 101u;
    observation.host_tick = 101u;
    observation.native_combat_observed = 0u;
    CHECK(!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 9u, &observation, &source));
    observation.native_combat_observed = 1u;
    source.host_tick = 101u;
    source.tal.facing_x = 0.0f;
    source.tal.facing_z = 0.0f;
    observation.host_tick = 101u;
    observation.combat_enabled = 1u;
    CHECK(!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
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
    observation.host_tick = source.host_tick;
    observation.match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    observation.combat_enabled = 1u;
    observation.native_combat_observed = 1u;
    CHECK(SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 11u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationBegin(
        &simulation,
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD, 12u));
    CHECK(!SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, &revision));
    CHECK(!SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 11u, &observation, &source));
    source.host_tick = 1u;
    source.combat_enabled = 1u;
    observation.host_tick = 1u;
    observation.combat_enabled = 0u;
    CHECK(SudekiMpLanArenaSharedSimulationCommitNativeFrame(
        &simulation, 12u, &observation, &source));
    CHECK(SudekiMpLanArenaSharedSimulationReadFrame(
        &simulation, &output, &revision));
    CHECK(output.combat_enabled == 0u);
    CHECK(revision == 1u);
}

int main(void) {
    test_native_world_owns_combat_state();
    test_roles_tokens_and_ticks_fail_closed();
    test_rejected_frame_is_transactional();
    test_fresh_session_invalidates_old_frame();
    if (failures != 0) {
        fprintf(stderr, "%d shared simulation test(s) failed\n", failures);
        return 1;
    }
    puts("lan arena shared simulation tests passed");
    return 0;
}
