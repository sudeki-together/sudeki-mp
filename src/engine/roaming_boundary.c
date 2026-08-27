#include "engine/roaming_boundary.h"

#include <math.h>
#include <string.h>

void SudekiMpRoamingBoundaryEvaluate(
    int active,
    float player_one_x,
    float player_one_z,
    float player_two_x,
    float player_two_z,
    float maximum_distance,
    float warning_fraction,
    SudekiMpRoamingBoundaryEvaluation *evaluation
) {
    float distance_squared;

    if (evaluation == NULL) {
        return;
    }
    memset(evaluation, 0, sizeof(*evaluation));
    if (!active || !isfinite(player_one_x) || !isfinite(player_one_z) ||
        !isfinite(player_two_x) || !isfinite(player_two_z) ||
        !isfinite(maximum_distance) || maximum_distance <= 0.0f ||
        !isfinite(warning_fraction) || warning_fraction <= 0.0f ||
        warning_fraction >= 1.0f) {
        return;
    }
    evaluation->player_two_from_player_one_x =
        player_two_x - player_one_x;
    evaluation->player_two_from_player_one_z =
        player_two_z - player_one_z;
    distance_squared =
        evaluation->player_two_from_player_one_x *
            evaluation->player_two_from_player_one_x +
        evaluation->player_two_from_player_one_z *
            evaluation->player_two_from_player_one_z;
    if (!isfinite(distance_squared)) {
        memset(evaluation, 0, sizeof(*evaluation));
        return;
    }
    evaluation->distance = sqrtf(distance_squared);
    evaluation->maximum_distance = maximum_distance;
    evaluation->warning_distance = maximum_distance * warning_fraction;
    evaluation->progress = evaluation->distance / maximum_distance;
    if (evaluation->progress > 1.0f) {
        evaluation->progress = 1.0f;
    }
    if (evaluation->distance >= maximum_distance) {
        evaluation->phase = SUDEKIMP_ROAMING_BOUNDARY_LIMIT;
    } else if (evaluation->distance >= evaluation->warning_distance) {
        evaluation->phase = SUDEKIMP_ROAMING_BOUNDARY_WARNING;
    } else {
        evaluation->phase = SUDEKIMP_ROAMING_BOUNDARY_SAFE;
    }
}

int SudekiMpRoamingBoundaryMovementAllowed(
    const SudekiMpRoamingBoundaryEvaluation *evaluation,
    int presentation_ready,
    unsigned int player_index,
    float direction_x,
    float direction_z
) {
    float constrained_x;
    float constrained_z;

    return SudekiMpRoamingBoundaryConstrainMovement(
        evaluation,
        presentation_ready,
        player_index,
        direction_x,
        direction_z,
        &constrained_x,
        &constrained_z
    );
}

int SudekiMpRoamingBoundaryConstrainMovement(
    const SudekiMpRoamingBoundaryEvaluation *evaluation,
    int presentation_ready,
    unsigned int player_index,
    float direction_x,
    float direction_z,
    float *constrained_x,
    float *constrained_z
) {
    float outward_x;
    float outward_z;
    float outward_dot;
    float direction_length;
    float length_squared;

    /* Reject numerically tangential input at the hard boundary.  A small
     * angular margin keeps camera-relative rounding from turning sideways
     * travel into an accidental escape route. */
    const float minimum_inward_fraction = 0.05f;

    if (constrained_x == NULL || constrained_z == NULL) {
        return 0;
    }
    *constrained_x = direction_x;
    *constrained_z = direction_z;
    if (evaluation == NULL || !presentation_ready ||
        evaluation->phase != SUDEKIMP_ROAMING_BOUNDARY_LIMIT ||
        player_index > 1u || !isfinite(direction_x) ||
        !isfinite(direction_z)) {
        return 1;
    }
    if (!isfinite(evaluation->distance) || evaluation->distance <= 0.0f) {
        return 1;
    }
    outward_x = evaluation->player_two_from_player_one_x /
        evaluation->distance;
    outward_z = evaluation->player_two_from_player_one_z /
        evaluation->distance;
    if (player_index == 0u) {
        outward_x = -outward_x;
        outward_z = -outward_z;
    }
    length_squared = direction_x * direction_x + direction_z * direction_z;
    if (!isfinite(length_squared) || length_squared <= 0.00000001f) {
        *constrained_x = 0.0f;
        *constrained_z = 0.0f;
        return 0;
    }
    direction_length = sqrtf(length_squared);
    outward_dot = outward_x * direction_x + outward_z * direction_z;
    if (outward_dot < -minimum_inward_fraction * direction_length) {
        return 1;
    }
    *constrained_x = 0.0f;
    *constrained_z = 0.0f;
    return 0;
}
