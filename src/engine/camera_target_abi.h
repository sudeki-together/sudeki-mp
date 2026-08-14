#ifndef SUDEKIMP_CAMERA_TARGET_ABI_H
#define SUDEKIMP_CAMERA_TARGET_ABI_H

void SudekiMpCallCameraTargetInstall(
    void *camera,
    void *target,
    unsigned int slot,
    void *function
);

void SudekiMpCallCameraTargetRelease(
    void *target_list,
    void *target,
    void *function
);

#endif
