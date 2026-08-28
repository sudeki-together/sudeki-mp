#ifndef SUDEKIMP_SAVE_BOOK_VOTE_H
#define SUDEKIMP_SAVE_BOOK_VOTE_H

#include <stdint.h>

enum {
    SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS = 4u,
    SUDEKIMP_SAVE_BOOK_VOTE_PLAYER_MASK = 0x0fu,
    SUDEKIMP_SAVE_BOOK_VOTE_HOST_INDEX = 0u,
    SUDEKIMP_SAVE_BOOK_VOTE_TIMEOUT_MS = 10000u,
    SUDEKIMP_SAVE_BOOK_OVERLAY_REPORT_TIMEOUT_MS = 1000u,
    SUDEKIMP_SAVE_BOOK_NATIVE_ENTER_TIMEOUT_MS = 2000u,
    SUDEKIMP_SAVE_BOOK_NATIVE_CLOSE_STABLE_SAMPLES = 2u,
    SUDEKIMP_SAVE_BOOK_NATIVE_UI_MODE = 12u
};

typedef enum SudekiMpSaveBookVoteState {
    SUDEKIMP_SAVE_BOOK_VOTE_IDLE = 0,
    SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE,
    SUDEKIMP_SAVE_BOOK_VOTE_WAITING,
    SUDEKIMP_SAVE_BOOK_VOTE_READY,
    SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING,
    SUDEKIMP_SAVE_BOOK_VOTE_REPLAYED,
    SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED
} SudekiMpSaveBookVoteState;

typedef enum SudekiMpSaveBookVoteResult {
    SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE = 0,
    SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
    SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW,
    SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW,
    SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_STARTED,
    SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_FINISHED,
    SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID,
    SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY,
    SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE
} SudekiMpSaveBookVoteResult;

typedef struct SudekiMpSaveBookVote {
    SudekiMpSaveBookVoteState state;
    uint32_t serial;
    uint32_t requested_at_ms;
    uint32_t overlay_report_deadline_ms;
    uint32_t visible_at_ms;
    uint32_t deadline_ms;
    uint8_t participant_mask;
    uint8_t accepted_mask;
    uint8_t cancelled_mask;
    uint8_t reserved;
} SudekiMpSaveBookVote;

typedef enum SudekiMpSaveBookNativeLifecycleResult {
    SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT = 0,
    SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTERED,
    SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED,
    SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTER_FAILED
} SudekiMpSaveBookNativeLifecycleResult;

typedef struct SudekiMpSaveBookNativeLifecycle {
    uint32_t started_at_ms;
    uint8_t mode_seen;
    uint8_t stable_non_mode_samples;
    uint8_t terminal;
    uint8_t reserved;
} SudekiMpSaveBookNativeLifecycle;

void SudekiMpSaveBookVoteInitialize(SudekiMpSaveBookVote *vote);
void SudekiMpSaveBookVoteReset(SudekiMpSaveBookVote *vote);

/* The participant mask is snapshotted when the exact host SaveMenuShow call is
 * intercepted. The host is always participant zero and implicitly accepts.
 * A lone host reaches READY without opening a prompt. */
SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteRequest(
    SudekiMpSaveBookVote *vote,
    uint8_t active_human_mask,
    uint32_t now_ms
);

/* Consent time starts only after the renderer reports the exact serial as
 * visible. An explicit draw failure cancels without running native save UI. */
SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteReportOverlay(
    SudekiMpSaveBookVote *vote,
    uint32_t serial,
    int visible,
    uint32_t now_ms
);

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteRespond(
    SudekiMpSaveBookVote *vote,
    uint32_t serial,
    unsigned int player_index,
    int accept
);

/* New joins cannot enter an in-flight vote. Dropouts are removed from the
 * participant snapshot; loss of the host cancels. Silence reaches READY after
 * the full visible ten-second window. */
SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteUpdate(
    SudekiMpSaveBookVote *vote,
    uint8_t active_human_mask,
    uint32_t now_ms
);

/* READY can be claimed once. FinishReplay records whether the one native
 * continuation completed; a failed proof/call becomes terminal CANCELLED. */
SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteBeginReplay(
    SudekiMpSaveBookVote *vote,
    uint32_t serial
);
SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteFinishReplay(
    SudekiMpSaveBookVote *vote,
    uint32_t serial,
    int succeeded
);

void SudekiMpSaveBookVoteCancel(
    SudekiMpSaveBookVote *vote,
    uint8_t cancelled_mask
);
uint32_t SudekiMpSaveBookVoteRemainingMs(
    const SudekiMpSaveBookVote *vote,
    uint32_t now_ms
);

/* The native continuation is successful only after the exact UI controller
 * reports save mode 12. Failure to enter is bounded; after entry, unreadable
 * controller samples can never synthesize a close. */
void SudekiMpSaveBookNativeLifecycleBegin(
    SudekiMpSaveBookNativeLifecycle *lifecycle,
    uint32_t now_ms
);
SudekiMpSaveBookNativeLifecycleResult
SudekiMpSaveBookNativeLifecycleUpdate(
    SudekiMpSaveBookNativeLifecycle *lifecycle,
    int inspection_valid,
    unsigned int current_mode,
    unsigned int next_mode,
    uint32_t now_ms
);
SudekiMpSaveBookNativeLifecycleResult
SudekiMpSaveBookNativeLifecycleSourceChanged(
    SudekiMpSaveBookNativeLifecycle *lifecycle
);

#endif
