#ifndef SUDEKIMP_LAN_ARENA_OWNER_VIEW_H
#define SUDEKIMP_LAN_ARENA_OWNER_VIEW_H

#include <windows.h>
#include <stdint.h>

/* A native CSkill may write the active camera's render-state matrix without
 * selecting another named camera.  Keep ownership exact while allowing the
 * real local camera to publish a new orientation every frame.  The leased 12
 * floats are matrix basis only; translation remains owned by the live camera.
 */
typedef struct SudekiMpLanArenaOwnerViewLease {
    void *camera_mode;
    void *camera;
    void *render_state;
    void *scene_manager;
    void *scene_renderer;
    void **scene_render_state_slot;
    float owner_basis[12];
    uint32_t refresh_revision;
    BOOL valid;
} SudekiMpLanArenaOwnerViewLease;

typedef enum SudekiMpLanArenaOwnerViewBoundary {
    /* Before RenderStart, validate identity but never write an older basis
     * over camera input that may have arrived during this native frame. */
    SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER = 0,
    /* Immediately after native RenderStart publishes the local owner. */
    SUDEKIMP_LAN_ARENA_OWNER_VIEW_REFRESH_AFTER_OWNER_RENDER = 1,
    /* After a bounded remote CSkill/presentation mutation. */
    SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION = 2,
    /* Retryable final restore; clears the lease only after exact success. */
    SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE = 3
} SudekiMpLanArenaOwnerViewBoundary;

BOOL SudekiMpLanArenaOwnerViewCapture(
    SudekiMpLanArenaOwnerViewLease *lease,
    void *camera_mode,
    void *scene_manager
);

BOOL SudekiMpLanArenaOwnerViewService(
    SudekiMpLanArenaOwnerViewLease *lease,
    void *camera_mode,
    void *scene_manager,
    SudekiMpLanArenaOwnerViewBoundary boundary
);

void SudekiMpLanArenaOwnerViewClear(
    SudekiMpLanArenaOwnerViewLease *lease
);

#endif
