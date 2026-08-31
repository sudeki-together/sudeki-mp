#include "engine/coop_roster_assignment.h"

#include <stddef.h>
#include <string.h>

int SudekiMpCoopRosterActorTypeKnown(uint32_t actor_type) {
    return actor_type == SUDEKIMP_COOP_ROSTER_ACTOR_AILISH ||
        actor_type == SUDEKIMP_COOP_ROSTER_ACTOR_BUKI ||
        actor_type == SUDEKIMP_COOP_ROSTER_ACTOR_ELCO ||
        actor_type == SUDEKIMP_COOP_ROSTER_ACTOR_TAL;
}

int SudekiMpCoopRosterAssignmentValid(
    const SudekiMpCoopRosterAssignment *assignment
) {
    uint32_t active_human_mask;
    unsigned int seat_index;

    if (assignment == NULL) {
        return 0;
    }
    active_human_mask = assignment->active_human_mask;
    if ((active_human_mask & SUDEKIMP_COOP_ROSTER_HOST_MASK) == 0u ||
        (active_human_mask & ~SUDEKIMP_COOP_ROSTER_VALID_MASK) != 0u) {
        return 0;
    }
    for (seat_index = 0u;
         seat_index < SUDEKIMP_COOP_ROSTER_MAX_SEATS;
         ++seat_index) {
        const uint32_t seat_mask = UINT32_C(1) << seat_index;
        const uint32_t actor_type =
            assignment->actor_type_by_seat[seat_index];
        unsigned int prior_seat;

        if ((active_human_mask & seat_mask) == 0u) {
            if (actor_type != SUDEKIMP_COOP_ROSTER_ACTOR_NONE) {
                return 0;
            }
            continue;
        }
        if (!SudekiMpCoopRosterActorTypeKnown(actor_type)) {
            return 0;
        }
        for (prior_seat = 0u; prior_seat < seat_index; ++prior_seat) {
            const uint32_t prior_mask = UINT32_C(1) << prior_seat;

            if ((active_human_mask & prior_mask) != 0u &&
                assignment->actor_type_by_seat[prior_seat] == actor_type) {
                return 0;
            }
        }
    }
    return 1;
}

int SudekiMpCoopRosterAssignmentFitsCapacity(
    const SudekiMpCoopRosterAssignment *assignment,
    unsigned int seat_capacity
) {
    uint32_t supported_mask;

    if (!SudekiMpCoopRosterAssignmentValid(assignment) ||
        seat_capacity == 0u ||
        seat_capacity > SUDEKIMP_COOP_ROSTER_MAX_SEATS) {
        return 0;
    }
    supported_mask = (UINT32_C(1) << seat_capacity) - UINT32_C(1);
    return (assignment->active_human_mask & ~supported_mask) == 0u;
}

void SudekiMpCoopRosterAssignmentStoreInitialize(
    SudekiMpCoopRosterAssignmentStore *store
) {
    if (store != NULL) {
        memset(store, 0, sizeof(*store));
    }
}

int SudekiMpCoopRosterAssignmentStoreCommit(
    SudekiMpCoopRosterAssignmentStore *store,
    const SudekiMpCoopRosterAssignment *candidate,
    unsigned int seat_capacity
) {
    SudekiMpCoopRosterAssignment copied;

    if (store == NULL ||
        !SudekiMpCoopRosterAssignmentFitsCapacity(
            candidate, seat_capacity)) {
        return 0;
    }
    copied = *candidate;
    store->assignment = copied;
    store->seat_capacity = seat_capacity;
    store->has_assignment = 1;
    return 1;
}

int SudekiMpCoopRosterAssignmentStoreGet(
    const SudekiMpCoopRosterAssignmentStore *store,
    SudekiMpCoopRosterAssignment *assignment
) {
    SudekiMpCoopRosterAssignment copied;

    if (store == NULL || assignment == NULL || store->has_assignment != 1 ||
        !SudekiMpCoopRosterAssignmentFitsCapacity(
            &store->assignment, store->seat_capacity)) {
        return 0;
    }
    copied = store->assignment;
    *assignment = copied;
    return 1;
}
