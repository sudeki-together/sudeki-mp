#include "engine/orbit_camera.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float left, float right) {
    return fabsf(left - right) < 0.0005f;
}

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "orbit_camera_test: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    float matrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 2.0f, 10.0f, 1.0f
    };
    const float target[3] = {0.0f, 0.0f, 0.0f};
    const float forward[3] = {0.0f, 0.0f, 1.0f};
    float world[3];
    float original_distance = sqrtf(104.0f);
    float result_distance;
    float first_person_matrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        9.0f, 9.0f, 9.0f, 1.0f
    };
    const float eye[3] = {2.0f, 3.5f, -4.0f};

    if (!require(SudekiMpOrbitCameraTransform(
                     matrix, target, 1.57079632679f, 0.0f),
                 "yaw transform failed") ||
        !require(close_enough(matrix[12], 10.0f) &&
                     close_enough(matrix[13], 2.0f) &&
                     close_enough(matrix[14], 0.0f),
                 "yaw did not orbit translation") ||
        !require(SudekiMpCameraTransformHorizontalDirection(
                     matrix, forward, world),
                 "direction transform failed") ||
        !require(close_enough(world[0], 1.0f) &&
                     close_enough(world[2], 0.0f),
                 "direction did not follow yaw") ||
        !require(SudekiMpOrbitCameraTransform(
                     matrix, target, 0.0f, 0.25f),
                 "pitch transform failed")) {
        return 1;
    }
    result_distance = sqrtf(
        matrix[12] * matrix[12] +
        matrix[13] * matrix[13] +
        matrix[14] * matrix[14]
    );
    if (!require(close_enough(result_distance, original_distance),
                 "orbit changed camera distance")) {
        return 1;
    }
    if (!require(SudekiMpFirstPersonCameraTransform(
                     first_person_matrix,
                     eye,
                     1.57079632679f,
                     0.25f),
                 "first-person transform failed") ||
        !require(close_enough(first_person_matrix[12], eye[0]) &&
                     close_enough(first_person_matrix[13], eye[1]) &&
                     close_enough(first_person_matrix[14], eye[2]),
                 "first-person transform moved away from eye") ||
        !require(SudekiMpCameraTransformHorizontalDirection(
                     first_person_matrix, forward, world),
                 "first-person direction transform failed") ||
        !require(close_enough(world[0], 1.0f) &&
                     close_enough(world[2], 0.0f),
                 "first-person direction did not follow yaw")) {
        return 1;
    }
    puts("orbit camera tests passed");
    return 0;
}
