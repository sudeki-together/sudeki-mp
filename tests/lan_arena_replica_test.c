#include "network/lan_arena_replica.h"

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
    snapshot.enemy_count = 1u;
    snapshot.enemies[0].native_entity_id =
        SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    snapshot.enemies[0].z = x * 3.0f;
    snapshot.enemies[0].hp = 10u;
    return snapshot;
}

int main(void) {
    SudekiMpLanArenaReplica replica;
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
    SudekiMpLanArenaReplicaReset(&replica);
    first = make_snapshot(10u, 1000u, 2.0f);
    second = make_snapshot(11u, 1100u, 12.0f);
    first.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    first.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    second.tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    second.tal.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &first));
    CHECK(SudekiMpLanArenaReplicaPush(&replica, &second));
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1050u, &sample));
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING);
    CHECK(sample.tal.x > 6.99f && sample.tal.x < 7.01f);
    CHECK(sample.ailish.x > 13.99f && sample.ailish.x < 14.01f);
    CHECK(SudekiMpLanArenaReplicaSample(&replica, 1100u, &sample));
    CHECK(sample.tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE);
    CHECK(sample.tal.x == 12.0f);
    if (failures != 0) return 1;
    puts("lan arena replica tests passed");
    return 0;
}
