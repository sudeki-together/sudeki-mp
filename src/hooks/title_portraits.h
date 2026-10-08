#ifndef SUDEKIMP_TITLE_PORTRAITS_H
#define SUDEKIMP_TITLE_PORTRAITS_H
#include <windows.h>

/* Game-thread title update only. The caller has already proved its active
 * title lease. This adapter additionally checks the title/scene/device tuple.
 * Native texture requests own their resident wrappers independently of the
 * title page; no stock widget, scene, title state, or asset pixels are changed.
 * Native pending work is drained through its verified wait operation. */
BOOL SudekiMpTitlePortraitsService(HMODULE image,void *owner,void *scene,void *device);
/* Read-only, same thread. Borrowed texture is valid only across this paint;
 * never retain it in the UI model. Lobby IDs: Buki=0, Elco=1, Tal=2, Ailish=3,
 * Talos=5. Unknown IDs, stale owners, or incomplete native residency return NULL. */
void *SudekiMpTitlePortraitsResolve(HMODULE image,void *owner,void *scene,
    void *device,unsigned character);
/* Same native thread. Does not dereference a retired title or scene. If the
 * device/native resource system or a retained wrapper cannot be proved, return
 * FALSE and retain all dependencies for retry. No asynchronous cancellation. */
BOOL SudekiMpTitlePortraitsRelease(void);
BOOL SudekiMpTitlePortraitsRetains(void);
BOOL SudekiMpTitlePortraitsImageMatches(HMODULE image);
#endif
