#ifndef SUDEKIMP_LAN_STORY_RENDER_H
#define SUDEKIMP_LAN_STORY_RENDER_H

#include <windows.h>

/* Sole owner of primary CALL68D45B -> 5D48C0, before camera-dependent scene
 * preparation. Install/remove only at the runtime's quiescent native boundary.
 * The dispatcher runs synchronously before the original preparation, once.
 * It must establish a fresh retained-pause/roster/session transaction through
 * Witness. This adapter never acquires or releases a native pause. */
typedef void (*SudekiMpLanStoryRenderDispatch)(void *context);
BOOL SudekiMpLanStoryRenderInstall(HMODULE image,
    SudekiMpLanStoryRenderDispatch dispatch,void *context);

/* Valid only inside the dispatcher on the bound native thread, with the exact
 * CALL, current native world-render owner and primary scene, world mode zero,
 * and zero incoming simulation delta. This is a lexical seam witness, not an
 * actor/camera lease; callers must still validate every borrowed native owner.
 * Other native world modes skip this primary branch and cannot use it. */
BOOL SudekiMpLanStoryRenderWitness(void *unused);

/* Outside callbacks on the bound native thread. Failed restoration retains
 * the complete owner state and pins the module. The immutable original target
 * remains available after successful removal for a late native tail return. */
BOOL SudekiMpLanStoryRenderUninstall(void);

#endif
