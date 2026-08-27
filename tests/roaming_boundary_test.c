#include "engine/roaming_boundary.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float left, float right) {
    return fabsf(left - right) <= 0.0001f;
}

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "roaming boundary test failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    SudekiMpRoamingBoundaryEvaluation evaluation;
    float x;
    float z;

    SudekiMpRoamingBoundaryEvaluate(
        0, 0.0f, 0.0f, 10.0f, 0.0f, 10.0f, 0.8f,
        &evaluation);
    if (!require(
            evaluation.phase == SUDEKIMP_ROAMING_BOUNDARY_INACTIVE,
            "an unstable/non-roaming context is inactive")) return 1;

    SudekiMpRoamingBoundaryEvaluate(
        1, 0.0f, 0.0f, 7.9f, 0.0f, 10.0f, 0.8f,
        &evaluation);
    if (!require(evaluation.phase == SUDEKIMP_ROAMING_BOUNDARY_SAFE,
            "distance below 80 percent is safe")) return 1;

    SudekiMpRoamingBoundaryEvaluate(
        1, 0.0f, 0.0f, 8.0f, 0.0f, 10.0f, 0.8f,
        &evaluation);
    if (!require(evaluation.phase == SUDEKIMP_ROAMING_BOUNDARY_WARNING,
            "warning begins at 80 percent")) return 1;
    if (!require(SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, 1.0f, 0.0f, &x, &z) &&
            nearly_equal(x, 1.0f) && nearly_equal(z, 0.0f),
            "warning never changes movement")) return 1;

    SudekiMpRoamingBoundaryEvaluate(
        1, 0.0f, 0.0f, 10.0f, 0.0f, 10.0f, 0.8f,
        &evaluation);
    if (!require(evaluation.phase == SUDEKIMP_ROAMING_BOUNDARY_LIMIT,
            "hard limit begins at maximum")) return 1;
    if (!require(!SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, 1.0f, 0.0f, &x, &z) &&
            nearly_equal(x, 0.0f) && nearly_equal(z, 0.0f),
            "Player 2 pure outward movement is blocked")) return 1;
    if (!require(SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, -1.0f, 0.0f, &x, &z) &&
            nearly_equal(x, -1.0f) && nearly_equal(z, 0.0f),
            "Player 2 inward movement remains available")) return 1;
    if (!require(!SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, 0.0f, 1.0f, &x, &z) &&
            nearly_equal(x, 0.0f) && nearly_equal(z, 0.0f),
            "Player 2 lateral movement is blocked")) return 1;
    if (!require(!SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 0u, -1.0f, 0.0f, &x, &z),
            "Player 1 uses the symmetric outward gate")) return 1;
    if (!require(SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 0u, 1.0f, 0.0f, &x, &z) &&
            nearly_equal(x, 1.0f),
            "Player 1 inward movement remains available")) return 1;
    if (!require(!SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 0u, 0.0f, -1.0f, &x, &z) &&
            nearly_equal(x, 0.0f) && nearly_equal(z, 0.0f),
            "Player 1 lateral movement is blocked symmetrically")) return 1;
    if (!require(!SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, 0.70710678f, 0.70710678f, &x, &z) &&
            nearly_equal(x, 0.0f) && nearly_equal(z, 0.0f),
            "outward diagonal movement is blocked rather than projected"))
        return 1;
    if (!require(SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, -0.70710678f, 0.70710678f, &x, &z) &&
            nearly_equal(x, -0.70710678f) &&
            nearly_equal(z, 0.70710678f),
            "a clearly inward diagonal remains available"))
        return 1;
    if (!require(!SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 1, 1u, -0.01f, 1.0f, &x, &z) &&
            nearly_equal(x, 0.0f) && nearly_equal(z, 0.0f),
            "near-lateral rounding is blocked"))
        return 1;
    if (!require(SudekiMpRoamingBoundaryConstrainMovement(
            &evaluation, 0, 1u, 1.0f, 0.0f, &x, &z) &&
            nearly_equal(x, 1.0f) && nearly_equal(z, 0.0f),
            "a missing warning overlay disables the hard clamp")) return 1;

    puts("roaming boundary policy tests passed");
    return 0;
}
