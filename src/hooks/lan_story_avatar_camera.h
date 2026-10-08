#ifndef SUDEKIMP_LAN_STORY_AVATAR_CAMERA_H
#define SUDEKIMP_LAN_STORY_AVATAR_CAMERA_H
#include <windows.h>
#include <stdint.h>

/* Exact local avatar/world/scene identity, never a network permission. The
 * original hero is provenance for the native target pair at first Bind;
 * subsequent camera matrices come only from the avatar's own CPosition.
 * No function selects a hero, changes AI, runs a camera tick or grants input. */
typedef struct SudekiMpLanStoryAvatarCameraIdentity {
    void *actor, *original_hero, *world, *scene_manager;
    uint64_t session_generation;
    uint32_t actor_generation, world_epoch, scene_epoch, scene_revision;
} SudekiMpLanStoryAvatarCameraIdentity;
typedef enum SudekiMpLanStoryAvatarCameraOperation {
    SUDEKIMP_AVATAR_CAMERA_BIND,
    SUDEKIMP_AVATAR_CAMERA_UPDATE,
    SUDEKIMP_AVATAR_CAMERA_PRESENT,
    SUDEKIMP_AVATAR_CAMERA_DIRECTION,
    SUDEKIMP_AVATAR_CAMERA_NATIVE_DIRECTION,
    SUDEKIMP_AVATAR_CAMERA_RESTORE,
    SUDEKIMP_AVATAR_CAMERA_WORLD_DESTROYED,
    SUDEKIMP_AVATAR_CAMERA_NATIVE_TRANSITION
} SudekiMpLanStoryAvatarCameraOperation;
/* Called synchronously on the caller's verified game-thread seam; never
 * retained. BIND/UPDATE must freshly prove actor generation, world/scene and
 * avatar ownership. PRESENT/DIRECTION additionally require the caller's
 * contained/paused client world and exclusive local view publication lease;
 * NATIVE_DIRECTION instead proves the active host's native camera ownership;
 * it reads current native geometry without publishing a view. NATIVE_DIRECTION
 * and NATIVE_TRANSITION must be rejected by client validators. The latter proves
 * host-only authority to adopt a newly installed native state/data pair. On
 * cleanup it may prove the retained host world/scene after actor retirement;
 * the adapter separately requires RESTORE. During play it also requires the
 * requested UPDATE or NATIVE_DIRECTION witness with a fresh avatar generation.
 * Neither transition nor restoration permits overwriting foreign target slots.
 * No operation requires a selected hero or native controller target. Retire any
 * replica/spectator view owner before Bind. RESTORE must prove the retained world/scene; the retired
 * avatar need not remain alive. WORLD_DESTROYED must positively witness the
 * native Quit/destruction return for this exact retained world/session tuple,
 * including camera and target-manager destruction. A timeout/disconnect,
 * stale pointer, failed observation, or changed scene is NOT that witness.
 * Callbacks must not mutate these objects or reenter this adapter. */
typedef BOOL (*SudekiMpLanStoryAvatarCameraExact)(
    const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraOperation, void *context);

/* Loader composition only; exact-build and native-entry gates, no patches.
 * Binding admits only native Exploration with no outstanding state-data target
 * at +0x470. Host update/direction/restoration may adopt exact registered
 * Exploration, Combat or BossCombat state/data pairs with that same condition.
 * Cinematics and other modes retain for retry; paused-client views never adopt
 * a changed state/data pair. No native state transition is initiated here. */
BOOL SudekiMpLanStoryAvatarCameraInstall(HMODULE image);
BOOL SudekiMpLanStoryAvatarCameraBind(const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraExact,void *context);
BOOL SudekiMpLanStoryAvatarCameraUpdate(const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraExact,void *context);
/* Paused-client mathematical view only. Inputs are sensitivity-scaled axis
 * seconds (bounded to +/-0.5); authored native camera settings frame the
 * avatar's own anchor. No native camera/state tick or hero selection occurs.
 * A foreign render-state write fails closed rather than being overwritten. */
BOOL SudekiMpLanStoryAvatarCameraPresent(const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraExact,void *context,float horizontal_seconds,float vertical_seconds);
BOOL SudekiMpLanStoryAvatarCameraDirection(const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraExact,void *context,float local_x,float local_z,
    float *world_x,float *world_z);
/* Active-host read only: current native camera geometry, with both target
 * slots still owned by this avatar. Does not require or create a paused view. */
BOOL SudekiMpLanStoryAvatarCameraNativeDirection(const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraExact,void *context,float local_x,float local_z,
    float *world_x,float *world_z);
/* Reverse slot restoration and reference release. FALSE retains dependencies
 * for retry; a foreign slot is never overwritten. Bind cannot replace any
 * retained lease, including a partially restored one. */
BOOL SudekiMpLanStoryAvatarCameraRestore(const SudekiMpLanStoryAvatarCameraIdentity *,
    SudekiMpLanStoryAvatarCameraExact,void *context);
BOOL SudekiMpLanStoryAvatarCameraRetains(void);
/* No dereferences of old objects. Explicit positive destruction witness only. */
BOOL SudekiMpLanStoryAvatarCameraNativeExitReturned(
    const SudekiMpLanStoryAvatarCameraIdentity *,SudekiMpLanStoryAvatarCameraExact,void *context);
BOOL SudekiMpLanStoryAvatarCameraUninstall(void);
#endif
