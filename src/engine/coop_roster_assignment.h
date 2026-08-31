#ifndef SUDEKIMP_COOP_ROSTER_ASSIGNMENT_H
#define SUDEKIMP_COOP_ROSTER_ASSIGNMENT_H

#include <stdint.h>

enum {
    SUDEKIMP_COOP_ROSTER_MAX_SEATS = 4u,
    SUDEKIMP_COOP_ROSTER_HOST_SEAT = 0u,
    SUDEKIMP_COOP_ROSTER_HOST_MASK = 0x01u,
    SUDEKIMP_COOP_ROSTER_VALID_MASK = 0x0fu
};

/* Exact character-resource type IDs used by the existing title roster. */
typedef enum SudekiMpCoopRosterActorType {
    SUDEKIMP_COOP_ROSTER_ACTOR_NONE = 0x00u,
    SUDEKIMP_COOP_ROSTER_ACTOR_AILISH = 0x01u,
    SUDEKIMP_COOP_ROSTER_ACTOR_BUKI = 0x05u,
    SUDEKIMP_COOP_ROSTER_ACTOR_ELCO = 0x0eu,
    SUDEKIMP_COOP_ROSTER_ACTOR_TAL = 0x23u
} SudekiMpCoopRosterActorType;

/* A pointer-free seat assignment. Active seats carry one known, unique hero
 * type; every inactive seat is represented canonically by ACTOR_NONE. */
typedef struct SudekiMpCoopRosterAssignment {
    uint32_t active_human_mask;
    uint32_t actor_type_by_seat[SUDEKIMP_COOP_ROSTER_MAX_SEATS];
} SudekiMpCoopRosterAssignment;

/* Small transactional owner for a copied assignment. Commit validates the
 * complete candidate before changing this store. Get returns another value
 * copy, never a mutable pointer into the stored contract. This is an API-level
 * all-or-nothing guarantee, not a cross-thread synchronization primitive. */
typedef struct SudekiMpCoopRosterAssignmentStore {
    SudekiMpCoopRosterAssignment assignment;
    unsigned int seat_capacity;
    int has_assignment;
} SudekiMpCoopRosterAssignmentStore;

int SudekiMpCoopRosterActorTypeKnown(uint32_t actor_type);
int SudekiMpCoopRosterAssignmentValid(
    const SudekiMpCoopRosterAssignment *assignment
);

/* Capacity is the number of stable seat indices implemented by a runtime,
 * starting at P1/seat zero. A gapped P1+P3 assignment therefore requires a
 * capacity of at least three even though it contains only two humans. */
int SudekiMpCoopRosterAssignmentFitsCapacity(
    const SudekiMpCoopRosterAssignment *assignment,
    unsigned int seat_capacity
);

void SudekiMpCoopRosterAssignmentStoreInitialize(
    SudekiMpCoopRosterAssignmentStore *store
);
int SudekiMpCoopRosterAssignmentStoreCommit(
    SudekiMpCoopRosterAssignmentStore *store,
    const SudekiMpCoopRosterAssignment *candidate,
    unsigned int seat_capacity
);
int SudekiMpCoopRosterAssignmentStoreGet(
    const SudekiMpCoopRosterAssignmentStore *store,
    SudekiMpCoopRosterAssignment *assignment
);

#endif
