#include "engine/coop_roster_assignment.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpCoopRosterAssignment assignment_for_mask(uint32_t mask) {
    static const uint32_t actor_types[SUDEKIMP_COOP_ROSTER_MAX_SEATS] = {
        SUDEKIMP_COOP_ROSTER_ACTOR_TAL,
        SUDEKIMP_COOP_ROSTER_ACTOR_AILISH,
        SUDEKIMP_COOP_ROSTER_ACTOR_BUKI,
        SUDEKIMP_COOP_ROSTER_ACTOR_ELCO
    };
    SudekiMpCoopRosterAssignment assignment;
    unsigned int seat_index;

    memset(&assignment, 0, sizeof(assignment));
    assignment.active_human_mask = mask;
    for (seat_index = 0u;
         seat_index < SUDEKIMP_COOP_ROSTER_MAX_SEATS;
         ++seat_index) {
        if ((mask & (UINT32_C(1) << seat_index)) != 0u) {
            assignment.actor_type_by_seat[seat_index] =
                actor_types[seat_index];
        }
    }
    return assignment;
}

static int assignment_equal(
    const SudekiMpCoopRosterAssignment *left,
    const SudekiMpCoopRosterAssignment *right
) {
    unsigned int seat_index;

    if (left->active_human_mask != right->active_human_mask) {
        return 0;
    }
    for (seat_index = 0u;
         seat_index < SUDEKIMP_COOP_ROSTER_MAX_SEATS;
         ++seat_index) {
        if (left->actor_type_by_seat[seat_index] !=
                right->actor_type_by_seat[seat_index]) {
            return 0;
        }
    }
    return 1;
}

static void test_known_actor_types(void) {
    CHECK(SudekiMpCoopRosterActorTypeKnown(
        SUDEKIMP_COOP_ROSTER_ACTOR_AILISH));
    CHECK(SudekiMpCoopRosterActorTypeKnown(
        SUDEKIMP_COOP_ROSTER_ACTOR_BUKI));
    CHECK(SudekiMpCoopRosterActorTypeKnown(
        SUDEKIMP_COOP_ROSTER_ACTOR_ELCO));
    CHECK(SudekiMpCoopRosterActorTypeKnown(
        SUDEKIMP_COOP_ROSTER_ACTOR_TAL));
    CHECK(!SudekiMpCoopRosterActorTypeKnown(
        SUDEKIMP_COOP_ROSTER_ACTOR_NONE));
    CHECK(!SudekiMpCoopRosterActorTypeKnown(0x04u));
    CHECK(!SudekiMpCoopRosterActorTypeKnown(UINT32_MAX));
}

static void test_assignment_validation(void) {
    static const uint32_t valid_masks[] = {
        0x01u, 0x03u, 0x05u, 0x07u, 0x09u, 0x0bu, 0x0du, 0x0fu
    };
    SudekiMpCoopRosterAssignment assignment;
    unsigned int index;

    for (index = 0u;
         index < sizeof(valid_masks) / sizeof(valid_masks[0]);
         ++index) {
        assignment = assignment_for_mask(valid_masks[index]);
        CHECK(SudekiMpCoopRosterAssignmentValid(&assignment));
    }
    CHECK(!SudekiMpCoopRosterAssignmentValid(NULL));

    assignment = assignment_for_mask(0x01u);
    assignment.active_human_mask = 0u;
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x02u);
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x01u);
    assignment.active_human_mask = 0x11u;
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x03u);
    assignment.actor_type_by_seat[1] = SUDEKIMP_COOP_ROSTER_ACTOR_NONE;
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x03u);
    assignment.actor_type_by_seat[1] = 0x44u;
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x03u);
    assignment.actor_type_by_seat[1] = assignment.actor_type_by_seat[0];
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x03u);
    assignment.actor_type_by_seat[2] = SUDEKIMP_COOP_ROSTER_ACTOR_BUKI;
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));

    assignment = assignment_for_mask(0x0fu);
    assignment.actor_type_by_seat[3] = assignment.actor_type_by_seat[1];
    CHECK(!SudekiMpCoopRosterAssignmentValid(&assignment));
}

static void test_capacity_policy(void) {
    SudekiMpCoopRosterAssignment assignment;

    assignment = assignment_for_mask(0x01u);
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 0u));
    CHECK(SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 1u));
    CHECK(SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 4u));
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 5u));

    assignment = assignment_for_mask(0x03u);
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 1u));
    CHECK(SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 2u));

    assignment = assignment_for_mask(0x05u);
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 2u));
    CHECK(SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 3u));

    assignment = assignment_for_mask(0x09u);
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 3u));
    CHECK(SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 4u));

    assignment = assignment_for_mask(0x07u);
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 2u));
    CHECK(SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 3u));
    assignment.actor_type_by_seat[2] = assignment.actor_type_by_seat[1];
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(&assignment, 3u));
    CHECK(!SudekiMpCoopRosterAssignmentFitsCapacity(NULL, 4u));
}

static void test_transactional_store(void) {
    SudekiMpCoopRosterAssignmentStore store;
    SudekiMpCoopRosterAssignment first = assignment_for_mask(0x03u);
    SudekiMpCoopRosterAssignment second = assignment_for_mask(0x07u);
    SudekiMpCoopRosterAssignment output;
    SudekiMpCoopRosterAssignment sentinel;

    memset(&store, 0xa5, sizeof(store));
    SudekiMpCoopRosterAssignmentStoreInitialize(&store);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    CHECK(!SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(assignment_equal(&output, &sentinel));

    CHECK(SudekiMpCoopRosterAssignmentStoreCommit(&store, &first, 2u));
    CHECK(store.seat_capacity == 2u);
    memset(&output, 0, sizeof(output));
    CHECK(SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(assignment_equal(&output, &first));

    first.actor_type_by_seat[1] = SUDEKIMP_COOP_ROSTER_ACTOR_ELCO;
    memset(&output, 0, sizeof(output));
    CHECK(SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(output.actor_type_by_seat[1] ==
        SUDEKIMP_COOP_ROSTER_ACTOR_AILISH);

    output.actor_type_by_seat[1] = SUDEKIMP_COOP_ROSTER_ACTOR_BUKI;
    CHECK(SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(output.actor_type_by_seat[1] ==
        SUDEKIMP_COOP_ROSTER_ACTOR_AILISH);

    CHECK(!SudekiMpCoopRosterAssignmentStoreCommit(&store, &second, 2u));
    memset(&output, 0, sizeof(output));
    CHECK(SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(output.active_human_mask == 0x03u);

    second.actor_type_by_seat[2] = second.actor_type_by_seat[1];
    CHECK(!SudekiMpCoopRosterAssignmentStoreCommit(&store, &second, 3u));
    memset(&output, 0, sizeof(output));
    CHECK(SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(output.active_human_mask == 0x03u);

    second = assignment_for_mask(0x07u);
    CHECK(SudekiMpCoopRosterAssignmentStoreCommit(&store, &second, 3u));
    CHECK(store.seat_capacity == 3u);
    memset(&output, 0, sizeof(output));
    CHECK(SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(assignment_equal(&output, &second));

    CHECK(!SudekiMpCoopRosterAssignmentStoreCommit(NULL, &second, 3u));
    CHECK(!SudekiMpCoopRosterAssignmentStoreCommit(&store, NULL, 3u));
    CHECK(!SudekiMpCoopRosterAssignmentStoreGet(NULL, &output));
    CHECK(!SudekiMpCoopRosterAssignmentStoreGet(&store, NULL));

    store.assignment.actor_type_by_seat[2] =
        store.assignment.actor_type_by_seat[1];
    output = sentinel;
    CHECK(!SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(assignment_equal(&output, &sentinel));

    store = (SudekiMpCoopRosterAssignmentStore){
        assignment_for_mask(0x07u), 2u, 1
    };
    output = sentinel;
    CHECK(!SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(assignment_equal(&output, &sentinel));

    store = (SudekiMpCoopRosterAssignmentStore){
        assignment_for_mask(0x03u), 2u, 2
    };
    output = sentinel;
    CHECK(!SudekiMpCoopRosterAssignmentStoreGet(&store, &output));
    CHECK(assignment_equal(&output, &sentinel));

    SudekiMpCoopRosterAssignmentStoreInitialize(NULL);
}

int main(void) {
    test_known_actor_types();
    test_assignment_validation();
    test_capacity_policy();
    test_transactional_store();

    if (failures != 0) {
        fprintf(stderr, "%d co-op roster assignment test(s) failed\n",
            failures);
        return 1;
    }
    puts("co-op roster assignment tests passed");
    return 0;
}
