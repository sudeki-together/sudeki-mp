#include "engine/orbit_camera.h"

#include <math.h>
#include <stddef.h>

static void rotate_y(float vector[3], float angle) {
    float sine = sinf(angle);
    float cosine = cosf(angle);
    float x = vector[0];
    float z = vector[2];
    vector[0] = cosine * x + sine * z;
    vector[2] = -sine * x + cosine * z;
}

static int normalize(float vector[3]) {
    float length = sqrtf(
        vector[0] * vector[0] +
        vector[1] * vector[1] +
        vector[2] * vector[2]
    );
    if (!isfinite(length) || length <= 0.0001f) {
        return 0;
    }
    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return 1;
}

static void rotate_axis(float vector[3], const float axis[3], float angle) {
    float sine = sinf(angle);
    float cosine = cosf(angle);
    float one_minus_cosine = 1.0f - cosine;
    float dot = vector[0] * axis[0] +
        vector[1] * axis[1] + vector[2] * axis[2];
    float cross[3] = {
        axis[1] * vector[2] - axis[2] * vector[1],
        axis[2] * vector[0] - axis[0] * vector[2],
        axis[0] * vector[1] - axis[1] * vector[0]
    };
    float original[3] = {vector[0], vector[1], vector[2]};
    vector[0] = original[0] * cosine + cross[0] * sine +
        axis[0] * dot * one_minus_cosine;
    vector[1] = original[1] * cosine + cross[1] * sine +
        axis[1] * dot * one_minus_cosine;
    vector[2] = original[2] * cosine + cross[2] * sine +
        axis[2] * dot * one_minus_cosine;
}

static int rotate_camera_basis(
    float matrix[16],
    float yaw_radians,
    float pitch_radians
) {
    float right[3];
    unsigned int row;
    unsigned int column;

    for (row = 0u; row < 3u; ++row) {
        float basis[3] = {
            matrix[row * 4u],
            matrix[row * 4u + 1u],
            matrix[row * 4u + 2u]
        };
        rotate_y(basis, yaw_radians);
        for (column = 0u; column < 3u; ++column) {
            matrix[row * 4u + column] = basis[column];
        }
    }
    right[0] = matrix[0];
    right[1] = matrix[1];
    right[2] = matrix[2];
    if (!normalize(right)) {
        return 0;
    }
    for (row = 0u; row < 3u; ++row) {
        float basis[3] = {
            matrix[row * 4u],
            matrix[row * 4u + 1u],
            matrix[row * 4u + 2u]
        };
        rotate_axis(basis, right, pitch_radians);
        for (column = 0u; column < 3u; ++column) {
            matrix[row * 4u + column] = basis[column];
        }
    }
    return 1;
}

int SudekiMpOrbitCameraTransform(
    float matrix[16],
    const float target[3],
    float yaw_radians,
    float pitch_radians
) {
    float offset[3];
    unsigned int column;

    if (matrix == NULL || target == NULL ||
        !isfinite(yaw_radians) || !isfinite(pitch_radians)) {
        return 0;
    }
    offset[0] = matrix[12] - target[0];
    offset[1] = matrix[13] - target[1];
    offset[2] = matrix[14] - target[2];
    rotate_y(offset, yaw_radians);
    if (!rotate_camera_basis(matrix, yaw_radians, pitch_radians)) {
        return 0;
    }
    {
        float right[3] = {matrix[0], matrix[1], matrix[2]};
        if (!normalize(right)) {
            return 0;
        }
        rotate_axis(offset, right, pitch_radians);
    }
    matrix[12] = target[0] + offset[0];
    matrix[13] = target[1] + offset[1];
    matrix[14] = target[2] + offset[2];
    matrix[15] = 1.0f;
    for (column = 0u; column < 16u; ++column) {
        if (!isfinite(matrix[column])) {
            return 0;
        }
    }
    return 1;
}

int SudekiMpFirstPersonCameraTransform(
    float matrix[16],
    const float eye[3],
    float yaw_radians,
    float pitch_radians
) {
    unsigned int column;

    if (matrix == NULL || eye == NULL ||
        !isfinite(eye[0]) || !isfinite(eye[1]) || !isfinite(eye[2]) ||
        !isfinite(yaw_radians) || !isfinite(pitch_radians) ||
        !rotate_camera_basis(matrix, yaw_radians, pitch_radians)) {
        return 0;
    }
    matrix[12] = eye[0];
    matrix[13] = eye[1];
    matrix[14] = eye[2];
    matrix[15] = 1.0f;
    for (column = 0u; column < 16u; ++column) {
        if (!isfinite(matrix[column])) {
            return 0;
        }
    }
    return 1;
}

int SudekiMpCameraTransformHorizontalDirection(
    const float matrix[16],
    const float local_direction[3],
    float world_direction[3]
) {
    float x;
    float z;
    float length;

    if (matrix == NULL || local_direction == NULL ||
        world_direction == NULL) {
        return 0;
    }
    x = local_direction[0] * matrix[0] +
        local_direction[2] * matrix[8];
    z = local_direction[0] * matrix[2] +
        local_direction[2] * matrix[10];
    length = sqrtf(x * x + z * z);
    if (!isfinite(length) || length <= 0.0001f) {
        return 0;
    }
    world_direction[0] = x / length;
    world_direction[1] = 0.0f;
    world_direction[2] = z / length;
    return 1;
}
