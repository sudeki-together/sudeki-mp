#ifndef SUDEKIMP_ROAMING_BOUNDARY_H
#define SUDEKIMP_ROAMING_BOUNDARY_H

enum {
    SUDEKIMP_ROAMING_BOUNDARY_INACTIVE = 0u,
    SUDEKIMP_ROAMING_BOUNDARY_SAFE = 1u,
    SUDEKIMP_ROAMING_BOUNDARY_WARNING = 2u,
    SUDEKIMP_ROAMING_BOUNDARY_LIMIT = 3u
};

typedef struct SudekiMpRoamingBoundaryEvaluation {
    unsigned int phase;
    float distance;
    float maximum_distance;
    float warning_distance;
    float progress;
    float player_two_from_player_one_x;
    float player_two_from_player_one_z;
} SudekiMpRoamingBoundaryEvaluation;

void SudekiMpRoamingBoundaryEvaluate(
    int active,
    float player_one_x,
    float player_one_z,
    float player_two_x,
    float player_two_z,
    float maximum_distance,
    float warning_fraction,
    SudekiMpRoamingBoundaryEvaluation *evaluation
);

int SudekiMpRoamingBoundaryMovementAllowed(
    const SudekiMpRoamingBoundaryEvaluation *evaluation,
    int presentation_ready,
    unsigned int player_index,
    float direction_x,
    float direction_z
);

/* At the visible hard limit, accept only input with a clearly inward radial
 * component.  Outward, lateral, and numerically near-lateral input is blocked
 * symmetrically; the output remains unchanged whenever the boundary is
 * inactive or its presentation is unavailable. */
int SudekiMpRoamingBoundaryConstrainMovement(
    const SudekiMpRoamingBoundaryEvaluation *evaluation,
    int presentation_ready,
    unsigned int player_index,
    float direction_x,
    float direction_z,
    float *constrained_x,
    float *constrained_z
);

#endif
