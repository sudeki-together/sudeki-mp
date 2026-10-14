#ifndef SUDEKIMP_LAN_STORY_CLIENT_CAMERA_H
#define SUDEKIMP_LAN_STORY_CLIENT_CAMERA_H
#include <windows.h>
#include <stdint.h>
#include "hooks/lan_arena_owner_view.h"

/* Client native Exploration camera (#71). The contained client keeps the
 * native pause, so the scheduled camera update never runs and the mod has
 * been publishing a mathematical orbit view. This module drives the real
 * camera instead: each presentation it feeds the local right stick to
 * CCamera::InputEvent (RVA 0xE85F0, actions 0x69/0x6A, owner wildcard) and
 * calls the camera's own scheduled update (node vtable slot 1, RVA 0xE7660)
 * with the real frame time, lifting only the speed-global pause byte that
 * gates that one call. The camera must be in Exploration; when it is not,
 * CCameraManager::SetCameraState (RVA 0x37530) requests it as the retail
 * bootstrap does (0.0, false, 99999.9). The leased camera, its target (the
 * client's own character) and render state come from the local-control
 * owner lease; nothing else is written. Exact-image gated. */
BOOL SudekiMpLanStoryClientCameraInstall(HMODULE image);
/* Client game thread, once per presentation when no cast/door override is
 * active. TRUE: the native camera advanced this frame and the render state
 * holds its view. FALSE: nothing was changed (caller presents as before). */
BOOL SudekiMpLanStoryClientCameraTick(const SudekiMpLanArenaOwnerViewLease *owner,
    float dt_seconds,float look_x,float look_y);
/* After a cast/door override hands the view back, re-enter Exploration through
 * the manager (the retail zone-entry bootstrap call) so the native camera is
 * re-seated behind its target instead of recovering from behind a door. */
BOOL SudekiMpLanStoryClientCameraReseat(const SudekiMpLanArenaOwnerViewLease *owner);
BOOL SudekiMpLanStoryClientCameraUninstall(void);
#endif
