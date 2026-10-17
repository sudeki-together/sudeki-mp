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
 * owner lease. During this owned Exploration call only, an exact-image hook
 * retains tiny finite height corrections the native camera would discard;
 * the native spring, error bound and collision continuation are unchanged. */
BOOL SudekiMpLanStoryClientCameraInstall(HMODULE image);
/* Read-only preflight for an established independent camera during a bounded
 * missing-frame interval. Requires its already established native thread,
 * ordinary Exploration state and complete native target chain;
 * never requests a state or changes camera/input/time. Caller separately
 * proves current selected-actor/view ownership and client pause containment. */
BOOL SudekiMpLanStoryClientCameraContinuityReady(const SudekiMpLanArenaOwnerViewLease *owner);
/* Client game thread, once per presentation when no cast/door override is
 * active. TRUE: the native camera advanced this frame and the render state
 * holds its view. The normal route may request Exploration and return FALSE
 * while that native state transition is pending. */
BOOL SudekiMpLanStoryClientCameraTick(const SudekiMpLanArenaOwnerViewLease *owner,
    float dt_seconds,float look_x,float look_y);
/* After a cast/door override hands the view back, re-enter Exploration through
 * the manager (the retail zone-entry bootstrap call) so the native camera is
 * re-seated behind its target instead of recovering from behind a door. */
BOOL SudekiMpLanStoryClientCameraReseat(const SudekiMpLanArenaOwnerViewLease *owner);
/* H1b: settled in a house whose cluster script sets a static camera (the
 * client holds that script): call SetCameraStaticPosition('default',
 * position, 0, false, 0) natively; the transition ends in Aimed (fixed eye
 * aimed at the character), which Tick then treats as ordinary. */
BOOL SudekiMpLanStoryClientCameraHouseStatic(const SudekiMpLanArenaOwnerViewLease *owner,const char *position);
/* Left the house: Exploration (0, false, 0) as the vanilla exit asks. */
BOOL SudekiMpLanStoryClientCameraHouseRelease(const SudekiMpLanArenaOwnerViewLease *owner);
/* FALSE retains hook dependencies; runtime must keep the module and retry
 * before destroying the camera lease or unloading the DLL. */
BOOL SudekiMpLanStoryClientCameraUninstall(void);
#endif
