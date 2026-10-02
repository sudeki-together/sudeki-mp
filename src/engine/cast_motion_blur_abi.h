#ifndef SUDEKIMP_CAST_MOTION_BLUR_ABI_H
#define SUDEKIMP_CAST_MOTION_BLUR_ABI_H
#include <windows.h>
#include <stdint.h>

enum {
    SUDEKIMP_CAST_BLUR_UNKNOWN=-1,
    SUDEKIMP_CAST_BLUR_NEUTRAL,
    SUDEKIMP_CAST_BLUR_LOCAL,
    SUDEKIMP_CAST_BLUR_REMOTE
};
/* The retained native scope, not the last active cast or network authority.
 * Neutral means positively unrelated work; failed observation is UNKNOWN.
 * Local/remote results include the exact nonzero native instance generation. */
typedef int (*SudekiMpCastMotionBlurScope)(uint32_t *generation);
BOOL SudekiMpCastMotionBlurImageMatches(HMODULE image);
/* Install on the game thread before cast admission, remove after every cast
 * and scoped callback drains. Remote requests (including clears) never reach
 * the local scene manager. Local and neutral requests run the full native
 * setter. No render callback, history texture or completion flag is changed. */
BOOL SudekiMpInstallCastMotionBlur(HMODULE image,SudekiMpCastMotionBlurScope scope);
BOOL SudekiMpUninstallCastMotionBlur(void);
#endif
