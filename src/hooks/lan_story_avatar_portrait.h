#ifndef SUDEKIMP_LAN_STORY_AVATAR_PORTRAIT_H
#define SUDEKIMP_LAN_STORY_AVATAR_PORTRAIT_H
#include <windows.h>
#include <stdint.h>

/* Independent gameplay ownership; never shares the title provider's instance.
 * Native game thread, outside task callbacks, with a fresh local avatar lease.
 * One Talos texture serves all exact Talos avatars in this world/epoch/device.
 * Native creation may synchronously drain texture work. Call from update, not
 * inside a draw. A failed acquisition is not retried within the same scope. */
BOOL SudekiMpLanStoryAvatarPortraitService(HMODULE image,void *world,uint32_t epoch,
    unsigned player,uint32_t spawn_generation,void *actor,void *device);
/* Same native thread. Revalidates this player's local spawn identity and
 * current world/device. Borrow only across the immediately following paint;
 * never store the returned COM pointer in a network/UI model. */
void *SudekiMpLanStoryAvatarPortraitResolve(HMODULE image,void *world,uint32_t epoch,
    unsigned player,uint32_t spawn_generation,void *actor,void *device);
/* Before world exit/unload; same native thread. Release uses only its retained
 * native wrapper and current resource-system/device, and never dereferences
 * world or actor. It remains safe after those borrowed objects have retired.
 * FALSE means retain the module/dependencies and retry; no blind Forget. */
BOOL SudekiMpLanStoryAvatarPortraitRelease(void);
BOOL SudekiMpLanStoryAvatarPortraitRetains(void);
#endif
