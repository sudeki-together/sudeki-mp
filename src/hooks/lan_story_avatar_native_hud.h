#ifndef SUDEKIMP_LAN_STORY_AVATAR_NATIVE_HUD_H
#define SUDEKIMP_LAN_STORY_AVATAR_NATIVE_HUD_H
#include "ui/story_avatar_stats_view.h"

/* Values are presentation data only. No actor, component or party pointers
 * enter the model. Character uses the lobby numbering (0..3 heroes,5 Talos). */
typedef struct SudekiMpLanStoryAvatarNativeHudSnapshot {
    SudekiMpStoryAvatarStatsSnapshot stats;
    uint8_t character[4];
} SudekiMpLanStoryAvatarNativeHudSnapshot;
typedef struct SudekiMpLanStoryAvatarNativeHudIdentity {
    /* scene_manager is the native UI/world-render owner at RVA408D1C.
     * epoch/revision are authoritative status IDs; a paused client's native
     * observer IDs may differ and must be proved separately by its callback. */
    void *world,*scene_manager;
    uint64_t session_generation;
    uint32_t epoch,revision;
} SudekiMpLanStoryAvatarNativeHudIdentity;
typedef enum SudekiMpLanStoryAvatarNativeHudOperation {
    SUDEKIMP_AVATAR_NATIVE_HUD_PRESENT,
    SUDEKIMP_AVATAR_NATIVE_HUD_RESTORE,
    SUDEKIMP_AVATAR_NATIVE_HUD_DESTROYED
} SudekiMpLanStoryAvatarNativeHudOperation;
/* Synchronous native-thread callback, retained until successful Unbind.
 * PRESENT freshly validates this session/world/scene, returning only current
 * authoritative rows; absent/stale players have present=FALSE. RESTORE needs
 * the same world/scene but permits retired actors. DESTROYED is a positive
 * native destruction witness for this HUD layer, not an observation failure.
 * Neither callback nor a missing row grants input or changes gameplay stats. */
typedef BOOL (*SudekiMpLanStoryAvatarNativeHudObserve)(
    const SudekiMpLanStoryAvatarNativeHudIdentity *,
    SudekiMpLanStoryAvatarNativeHudOperation,
    SudekiMpLanStoryAvatarNativeHudSnapshot *,void *context);

BOOL SudekiMpLanStoryAvatarNativeHudInstall(HMODULE);
BOOL SudekiMpLanStoryAvatarNativeHudBind(
    const SudekiMpLanStoryAvatarNativeHudIdentity *,
    SudekiMpLanStoryAvatarNativeHudObserve,void *context);
/* Establish/update the exact native widget lease on a verified game-thread
 * seam. Native update/draw hooks independently refresh the model afterward.
 * TRUE means all mapped portraits/bar widgets are available; only then should
 * the caller retire its separate-card presentation. */
BOOL SudekiMpLanStoryAvatarNativeHudService(void);
BOOL SudekiMpLanStoryAvatarNativeHudActive(void);
BOOL SudekiMpLanStoryAvatarNativeHudUnbind(void);
BOOL SudekiMpLanStoryAvatarNativeHudNativeExitReturned(void);
BOOL SudekiMpLanStoryAvatarNativeHudRetains(void);
BOOL SudekiMpLanStoryAvatarNativeHudUninstall(void);
#endif
