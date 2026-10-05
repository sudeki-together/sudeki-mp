#ifndef SUDEKIMP_LAN_STORY_LOAD_H
#define SUDEKIMP_LAN_STORY_LOAD_H

#include <windows.h>
#include "ui/save_catalog.h"

typedef enum SudekiMpStoryLoadState {
    SUDEKIMP_STORY_LOAD_IDLE,
    SUDEKIMP_STORY_LOAD_PREPARED,
    SUDEKIMP_STORY_LOAD_WAITING_PAGE,
    SUDEKIMP_STORY_LOAD_FADING,
    SUDEKIMP_STORY_LOAD_LOADING,
    SUDEKIMP_STORY_LOAD_RETURNED,
    SUDEKIMP_STORY_LOAD_FAILED
} SudekiMpStoryLoadState;

typedef enum SudekiMpStoryLoadRoute {
    SUDEKIMP_STORY_LOAD_ROUTE_NONE,
    SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX,
    SUDEKIMP_STORY_LOAD_ROUTE_PAGE_RECORD
} SudekiMpStoryLoadRoute;

typedef struct SudekiMpStoryLoadResult {
    uint32_t attempt;
    SudekiMpStoryLoadState state;
    SudekiMpStoryLoadRoute route;
    DWORD result;
    unsigned folder_slot;
    int native_index;
    BOOL native_called, files_retired;
} SudekiMpStoryLoadResult;

BOOL SudekiMpInstallLanStoryLoad(HMODULE game);
BOOL SudekiMpUninstallLanStoryLoad(void);
/* Prepare, arm, service and cancel run only on the verified title update
 * thread. Prepare pins the reviewed files; it does not invoke native loading. */
BOOL SudekiMpLanStoryLoadPrepare(SudekiMpSaveCatalog *catalog, unsigned index,
    const SudekiMpSaveFingerprint *fingerprint);
BOOL SudekiMpLanStoryLoadNeedsTitle(void);
/* Arm before dispatching the original title Continue action. */
BOOL SudekiMpLanStoryLoadArmTitle(void *title);
/* Call after EVERY original native title update, including child pages. */
void SudekiMpLanStoryLoadServiceTitle(void *title);
/* RETURNED means either exact accepted route's synchronous native reader
 * returned success and the reviewed file lease retired. It is NOT
 * world readiness, campaign authority, party availability, or join permission.
 * result is the native return code, or a Win32 validation error on FAILED. */
SudekiMpStoryLoadState SudekiMpLanStoryLoadPoll(DWORD *result);
/* Same native thread, outside callbacks. Identifies this prepared attempt and
 * the route actually guarded; it adds no broader readiness/authority claim. */
BOOL SudekiMpLanStoryLoadGetResult(SudekiMpStoryLoadResult *result);
/* Local trust root for future story-account binding. Copies the fingerprint
 * reverified by this exact load's file lease, only AFTER native success and
 * file retirement. The caller must separately prove world/task readiness.
 * A network-provided fingerprint, preview, or matching area name is not a
 * substitute. Failure leaves output unchanged; folder_slot is local metadata
 * and must not participate in a cross-machine content identity. */
BOOL SudekiMpLanStoryLoadGetFingerprint(uint32_t attempt,SudekiMpSaveFingerprint *out);
/* Never force-cancels a native page fade or synchronous load. Busy retains
 * the exact file lease and hook dependencies for a later safe retry. */
BOOL SudekiMpLanStoryLoadCancel(void);

#endif
