#ifndef SUDEKIMP_LAN_STORY_AREA_CLOSE_H
#define SUDEKIMP_LAN_STORY_AREA_CLOSE_H
#include "hooks/control_separation.h"
#include <stdint.h>

typedef enum SudekiMpStoryAreaClosePhase {
    SUDEKIMP_AREA_CLOSE_HELD=1, SUDEKIMP_AREA_CLOSE_REQUESTED,
    SUDEKIMP_AREA_CLOSE_POSTED, SUDEKIMP_AREA_CLOSE_DISPATCHED
} SudekiMpStoryAreaClosePhase;
typedef struct SudekiMpStoryAreaCloseReceipt {
    uint64_t ticket;
    HWND window;
    unsigned phase, requests;
} SudekiMpStoryAreaCloseReceipt;
/* Experimental, NOT installed by runtime. One adapter owns the supported
 * WndProc WM_CLOSE admission before its stop-running write and native cleanup.
 * Suspended pre-world startup only. Other messages retain native behavior.
 * This is ONE destruction path, not a world/resource lease: save loading,
 * frontend quit, script cleanup and area retirement remain separate owners. */
BOOL SudekiMpLanStoryAreaCloseInstall(HMODULE);
/* Same native dispatch thread after adoption; NULL only before native events
 * during suspended startup. Held/pending/posted/unknown state refuses removal.
 * Failed restoration retains originals for retry. No explicit unload policy. */
BOOL SudekiMpLanStoryAreaCloseUninstall(const SudekiMpControlUpdateDispatchWitness *);
/* One coordinator holds close admission for its entire owned area session.
 * Window must belong to this process/thread and use the exact native WndProc.
 * A private window property binds its incarnation; foreign properties are not
 * overwritten. The opaque marker is removed on release or admitted cleanup.
 * Repeated identical acquisition returns the same ticket; tickets never repeat.
 * No native object is dereferenced or made safe by this hold alone. */
BOOL SudekiMpLanStoryAreaCloseHold(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,HWND,uint64_t *ticket);
BOOL SudekiMpLanStoryAreaCloseRead(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t,SudekiMpStoryAreaCloseReceipt *);
/* Only after coordinator-proven dependency drain. Dispatch witness establishes
 * the calling thread/boundary, NOT drain. Release refuses a pending close;
 * PostDrainedClose preserves that user intent and posts a new WM_CLOSE instead
 * of destroying native owners inside a controller callback. Posting failure is
 * retryable and retains the hold. Once posted, acquisition/release/uninstall
 * remain closed. First matching delivery runs native cleanup once; nested or
 * repeated delivery remains suppressed. DISPATCHED is NOT cleanup-return proof.
 * Read refuses once its window marker is removed for dispatch or otherwise lost.
 * Do not enable this adapter until a coordinator can actually service requests. */
BOOL SudekiMpLanStoryAreaCloseRelease(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t);
BOOL SudekiMpLanStoryAreaClosePostDrainedClose(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t);
#endif
