#ifndef SUDEKIMP_ORBIT_CAMERA_H
#define SUDEKIMP_ORBIT_CAMERA_H

int SudekiMpOrbitCameraTransform(
    float matrix[16],
    const float target[3],
    float yaw_radians,
    float pitch_radians
);

int SudekiMpFirstPersonCameraTransform(
    float matrix[16],
    const float eye[3],
    float yaw_radians,
    float pitch_radians
);

int SudekiMpCameraTransformHorizontalDirection(
    const float matrix[16],
    const float local_direction[3],
    float world_direction[3]
);

#endif
