#ifndef SUDEKIMP_LAN_STORY_CLIENT_CAMERA_HEIGHT_H
#define SUDEKIMP_LAN_STORY_CLIENT_CAMERA_HEIGHT_H
#include <windows.h>

/* Borrowed only for one already-owned native Exploration update. The proof
 * callback must freshly validate the complete owner/render/selected-target
 * lease; matching pointers alone do not establish that lease. */
typedef struct SudekiMpLanStoryClientCameraHeightScope {
    void *camera, *state, *data, *outer, *inner;
} SudekiMpLanStoryClientCameraHeightScope;
typedef BOOL (*SudekiMpLanStoryClientCameraHeightProof)(
    const SudekiMpLanStoryClientCameraHeightScope *scope);

BOOL SudekiMpLanStoryClientCameraHeightImageExact(HMODULE image);
BOOL SudekiMpLanStoryClientCameraHeightInstall(HMODULE image,
    SudekiMpLanStoryClientCameraHeightProof proof);
BOOL SudekiMpLanStoryClientCameraHeightBegin(
    const SudekiMpLanStoryClientCameraHeightScope *scope);
void SudekiMpLanStoryClientCameraHeightEnd(void);
/* Refuses an active call, foreign bound thread or changed patch. On refusal
 * all callback/image/trampoline dependencies remain owned for a later retry. */
BOOL SudekiMpLanStoryClientCameraHeightUninstall(void);
#endif
