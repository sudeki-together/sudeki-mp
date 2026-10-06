#ifndef SUDEKIMP_LAN_STORY_AREA_INTENT_H
#define SUDEKIMP_LAN_STORY_AREA_INTENT_H
#include "hooks/control_separation.h"
#include <stdint.h>
enum { SUDEKIMP_AREA_INTENTS=32 };
enum { SUDEKIMP_AREA_INTENT_RELOAD=1,SUDEKIMP_AREA_INTENT_QUIT,SUDEKIMP_AREA_INTENT_LOAD_SAVE,
    SUDEKIMP_AREA_INTENT_REMOVE_AREAS };
enum { SUDEKIMP_AREA_INTENT_MENU=1,SUDEKIMP_AREA_INTENT_QUIT_EXPORT,SUDEKIMP_AREA_INTENT_LOAD_EXPORT,
    SUDEKIMP_AREA_INTENT_REMOVE_EXPORT };
enum { SUDEKIMP_AREA_INTENT_DISPATCHING=1,SUDEKIMP_AREA_INTENT_DEFERRED };
enum { SUDEKIMP_AREA_INTENT_UNKNOWN=0,SUDEKIMP_AREA_INTENT_RUN,SUDEKIMP_AREA_INTENT_DEFER };
typedef struct SudekiMpLanStoryAreaIntentReceipt {
    uint64_t ticket;
    unsigned kind,phase,source;
    int32_t save_index; /* Copied only for LOAD_EXPORT; -1 otherwise. NOT file identity. */
} SudekiMpLanStoryAreaIntentReceipt;
/* Synchronous native-game-thread admission. Copy receipt data if needed; never
 * retain this borrowed pointer. No menu/catalog/native pointer is supplied.
 * Consumer must own every DEFERRED whole request (including exact reload files)
 * and later execute a separately verified exit/load route after dependency
 * drain. It must not call native game code or reenter admission here.
 * RUN admits THIS original native action now, not a queued/replayed action.
 * UNKNOWN, invalid result, reentry, capacity or ownership loss quarantines.
 * This callback is not itself native ownership, drain or authority proof. */
typedef unsigned (*SudekiMpLanStoryAreaIntentAdmission)(const void *consumer,
    const SudekiMpLanStoryAreaIntentReceipt *);
/* Experimental, NOT installed by runtime. Owns the native quit-menu action
 * entry before its script/UI/loading effects, for reload(0) and quit(2) from
 * the exact native input caller. Other selectors keep native behavior. Also owns
 * QuitToFrontEnd after its initial scene-pointer load, before dereferencing the
 * scene or starting cleanup. Its copied source distinguishes that public export;
 * a source is not caller authentication, script continuation or exit completion.
 * Uses the existing save-book patch owner for LoadGameSave, before any catalog
 * or loading effects. Its copied signed index is not a pinned save/file lease.
 * Also gates the public RemoveAllZones wrapper before reading the global world
 * or clearing any area. Direct internal cleanup/reset/destructor calls bypass
 * that wrapper and are NOT covered. Deferral of these void exports does not
 * suspend their callers or certify that cleanup completed.
 * Covers neither earlier input-side cues nor callers'
 * earlier/later effects, direct inner quit, window close, save pages, other world
 * cleanup or door transitions. Do NOT enable until a
 * coordinator owns deferred intents and all other destruction routes. */
BOOL SudekiMpLanStoryAreaIntentInstall(HMODULE);
/* Only suspended pre-native startup restoration is supported; any native action
 * keeps hooks/dependencies for process lifetime. Not an explicit-DLL-unload policy. */
BOOL SudekiMpLanStoryAreaIntentUninstall(void);
BOOL SudekiMpLanStoryAreaIntentAttach(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,SudekiMpLanStoryAreaIntentAdmission);
/* Fresh post-controller boundary, same consumer/thread, outside callbacks.
 * Output/count unchanged on refusal. No snapshot dereferences a menu. */
BOOL SudekiMpLanStoryAreaIntentSnapshot(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,SudekiMpLanStoryAreaIntentReceipt *,
    unsigned capacity,unsigned *count);
/* Consume only after adopting the whole deferred request into the coordinator's
 * owned plan. Clears this journal receipt only: NO native call, release, replay,
 * cancellation, successful quit/reload or world-drain proof. Pending receipts
 * prevent detach and prevent a newer RUN from overtaking deferred work.
 * All replay/orchestration and menu/save lifetime obligations remain external. */
BOOL SudekiMpLanStoryAreaIntentAcknowledge(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t ticket);
BOOL SudekiMpLanStoryAreaIntentDetach(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *);
#endif
