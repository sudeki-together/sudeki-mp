#ifndef SUDEKIMP_SAVE_BOOK_INTERCEPT_H
#define SUDEKIMP_SAVE_BOOK_INTERCEPT_H

#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpSaveBookVoteSnapshot {
    BOOL active;
    unsigned int state;
    uint32_t serial;
    uint32_t remaining_ms;
    uint8_t participant_mask;
    uint8_t accepted_mask;
    uint8_t cancelled_mask;
    uint8_t reserved;
} SudekiMpSaveBookVoteSnapshot;

/* Exact supported-image hooks: SaveMenuShow() at RVA 0x84F10 is a native
 * pass-through/lifecycle marker; LoadGameSave(int) at RVA 0x101690 is the
 * final deferred-confirmation boundary. Disabled mode performs no signature
 * check or mutation. */
BOOL SudekiMpInstallSaveBookIntercept(
    HMODULE game_module,
    BOOL enabled
);
void SudekiMpUninstallSaveBookIntercept(void);

/* Experimental story-only use of this SAME LoadGameSave patch owner, mutually
 * exclusive with the local vote/other profile installers. Startup-only install;
 * no SaveMenuShow hook, vote, UI/global mutation or native load is initiated.
 * begin/end are paired around each intercepted call, including a deferred one.
 * begin receives only the copied native sorted index, NOT a save identity.
 * Caller owns thread/admission/deferred-plan/file/native-lifetime verification.
 * TRUE admits this native export now; FALSE returns from its void entry without
 * executing it. This does not suspend the C/script caller or replay anything.
 * Callbacks must not reenter native code or retain an index as a file lease. */
typedef BOOL (*SudekiMpSaveBookStoryLoadBegin)(const void *,int);
typedef void (*SudekiMpSaveBookStoryLoadEnd)(const void *);
BOOL SudekiMpSaveBookStoryLoadInstall(HMODULE,const void *owner,
    SudekiMpSaveBookStoryLoadBegin,SudekiMpSaveBookStoryLoadEnd);
BOOL SudekiMpSaveBookStoryLoadExact(HMODULE,const void *owner);
/* Only pre-native startup restoration; observed native entry retains callbacks
 * for process lifetime. Not an explicit DLL-unload policy. Wrong owner refuses.
 * Failed restoration preserves dependencies and permits startup retry. */
BOOL SudekiMpSaveBookStoryLoadUninstall(const void *owner);

/* Pointer-free overlay/input contract. The first successful visible report
 * starts the full ten-second timer. player_index zero is the host; index one
 * is the currently supported Player 2 input seat. */
BOOL SudekiMpSaveBookGetVoteSnapshot(
    SudekiMpSaveBookVoteSnapshot *snapshot
);
BOOL SudekiMpSaveBookReportVoteOverlay(
    uint32_t serial,
    BOOL visible
);
BOOL SudekiMpSaveBookRespondVote(
    uint32_t serial,
    unsigned int player_index,
    BOOL accept
);

/* Call once from the gameplay update thread. This owns dropout, timeout,
 * provenance revalidation, and the one saved native continuation. */
void SudekiMpSaveBookService(void);

/* The confirmed native save-page close edge releases the split-render/modal
 * latch. It never synthesizes a close from an uncertain inspector state. */
void SudekiMpSaveBookObserveNativeClosed(void);
BOOL SudekiMpSaveBookVoteActive(void);

#if defined(SUDEKIMP_SAVE_BOOK_INTERCEPT_TESTING)
const void *SudekiMpSaveBookInterceptOriginalForTesting(void);
const void *SudekiMpSaveBookInterceptLoadGameSaveOriginalForTesting(void);
#endif

#endif
