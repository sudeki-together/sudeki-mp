#include "engine/save_book_vote.h"

#include <stddef.h>
#include <string.h>

static int tick_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint8_t player_bit(unsigned int player_index) {
    if (player_index >= SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS) {
        return 0u;
    }
    return (uint8_t)(1u << player_index);
}

static void mark_ready(SudekiMpSaveBookVote *vote) {
    vote->state = SUDEKIMP_SAVE_BOOK_VOTE_READY;
}

void SudekiMpSaveBookVoteInitialize(SudekiMpSaveBookVote *vote) {
    if (vote != NULL) {
        memset(vote, 0, sizeof(*vote));
    }
}

void SudekiMpSaveBookVoteReset(SudekiMpSaveBookVote *vote) {
    uint32_t serial;

    if (vote == NULL) {
        return;
    }
    serial = vote->serial;
    memset(vote, 0, sizeof(*vote));
    vote->serial = serial;
}

void SudekiMpSaveBookVoteCancel(
    SudekiMpSaveBookVote *vote,
    uint8_t cancelled_mask
) {
    if (vote == NULL || vote->state == SUDEKIMP_SAVE_BOOK_VOTE_IDLE ||
        vote->state == SUDEKIMP_SAVE_BOOK_VOTE_REPLAYED) {
        return;
    }
    vote->cancelled_mask = cancelled_mask &
        SUDEKIMP_SAVE_BOOK_VOTE_PLAYER_MASK;
    vote->state = SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED;
}

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteRequest(
    SudekiMpSaveBookVote *vote,
    uint8_t active_human_mask,
    uint32_t now_ms
) {
    const uint8_t host_bit = player_bit(
        SUDEKIMP_SAVE_BOOK_VOTE_HOST_INDEX);

    if (vote == NULL) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (vote->state != SUDEKIMP_SAVE_BOOK_VOTE_IDLE) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY;
    }
    active_human_mask &= SUDEKIMP_SAVE_BOOK_VOTE_PLAYER_MASK;
    if ((active_human_mask & host_bit) == 0u) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }

    ++vote->serial;
    if (vote->serial == 0u) {
        ++vote->serial;
    }
    vote->requested_at_ms = now_ms;
    vote->overlay_report_deadline_ms = now_ms +
        SUDEKIMP_SAVE_BOOK_OVERLAY_REPORT_TIMEOUT_MS;
    vote->visible_at_ms = 0u;
    vote->deadline_ms = 0u;
    vote->participant_mask = active_human_mask;
    vote->accepted_mask = host_bit;
    vote->cancelled_mask = 0u;

    if (active_human_mask == host_bit) {
        mark_ready(vote);
        return SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW;
    }
    vote->state = SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE;
    return SUDEKIMP_SAVE_BOOK_VOTE_OPENED;
}

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteReportOverlay(
    SudekiMpSaveBookVote *vote,
    uint32_t serial,
    int visible,
    uint32_t now_ms
) {
    if (vote == NULL) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (serial == 0u || serial != vote->serial) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE;
    }
    if (vote->state != SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE &&
        vote->state != SUDEKIMP_SAVE_BOOK_VOTE_WAITING) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY;
    }
    if (!visible) {
        SudekiMpSaveBookVoteCancel(vote, 0u);
        return SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW;
    }
    if (vote->state == SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE) {
        vote->visible_at_ms = now_ms;
        vote->deadline_ms = now_ms + SUDEKIMP_SAVE_BOOK_VOTE_TIMEOUT_MS;
        vote->state = SUDEKIMP_SAVE_BOOK_VOTE_WAITING;
    }
    return SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE;
}

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteRespond(
    SudekiMpSaveBookVote *vote,
    uint32_t serial,
    unsigned int player_index,
    int accept
) {
    uint8_t bit;

    if (vote == NULL) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (serial == 0u || serial != vote->serial) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE;
    }
    if (vote->state != SUDEKIMP_SAVE_BOOK_VOTE_WAITING) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY;
    }
    bit = player_bit(player_index);
    if (bit == 0u || (vote->participant_mask & bit) == 0u) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (!accept) {
        SudekiMpSaveBookVoteCancel(vote, bit);
        return SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW;
    }
    vote->accepted_mask |= bit;
    if ((vote->accepted_mask & vote->participant_mask) ==
        vote->participant_mask) {
        mark_ready(vote);
        return SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW;
    }
    return SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE;
}

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteUpdate(
    SudekiMpSaveBookVote *vote,
    uint8_t active_human_mask,
    uint32_t now_ms
) {
    const uint8_t host_bit = player_bit(
        SUDEKIMP_SAVE_BOOK_VOTE_HOST_INDEX);

    if (vote == NULL) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (vote->state != SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE &&
        vote->state != SUDEKIMP_SAVE_BOOK_VOTE_WAITING) {
        return SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE;
    }
    active_human_mask &= SUDEKIMP_SAVE_BOOK_VOTE_PLAYER_MASK;
    if ((active_human_mask & host_bit) == 0u) {
        SudekiMpSaveBookVoteCancel(vote, host_bit);
        return SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW;
    }

    /* The bitwise intersection removes dropouts without adding later joins. */
    vote->participant_mask &= active_human_mask;
    vote->accepted_mask &= vote->participant_mask;
    if ((vote->accepted_mask & vote->participant_mask) ==
        vote->participant_mask) {
        mark_ready(vote);
        return SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW;
    }
    if (vote->state == SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE) {
        if (tick_reached(now_ms, vote->overlay_report_deadline_ms)) {
            SudekiMpSaveBookVoteCancel(vote, 0u);
            return SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW;
        }
        return SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE;
    }
    if (tick_reached(now_ms, vote->deadline_ms)) {
        mark_ready(vote);
        return SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW;
    }
    return SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE;
}

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteBeginReplay(
    SudekiMpSaveBookVote *vote,
    uint32_t serial
) {
    if (vote == NULL) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (serial == 0u || serial != vote->serial) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE;
    }
    if (vote->state != SUDEKIMP_SAVE_BOOK_VOTE_READY) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY;
    }
    vote->state = SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING;
    return SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_STARTED;
}

SudekiMpSaveBookVoteResult SudekiMpSaveBookVoteFinishReplay(
    SudekiMpSaveBookVote *vote,
    uint32_t serial,
    int succeeded
) {
    if (vote == NULL) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID;
    }
    if (serial == 0u || serial != vote->serial) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE;
    }
    if (vote->state != SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING) {
        return SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY;
    }
    vote->state = succeeded ? SUDEKIMP_SAVE_BOOK_VOTE_REPLAYED :
        SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED;
    return succeeded ? SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_FINISHED :
        SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW;
}

uint32_t SudekiMpSaveBookVoteRemainingMs(
    const SudekiMpSaveBookVote *vote,
    uint32_t now_ms
) {
    if (vote == NULL || vote->state != SUDEKIMP_SAVE_BOOK_VOTE_WAITING ||
        tick_reached(now_ms, vote->deadline_ms)) {
        return 0u;
    }
    return vote->deadline_ms - now_ms;
}

void SudekiMpSaveBookNativeLifecycleBegin(
    SudekiMpSaveBookNativeLifecycle *lifecycle,
    uint32_t now_ms
) {
    if (lifecycle == NULL) {
        return;
    }
    memset(lifecycle, 0, sizeof(*lifecycle));
    lifecycle->started_at_ms = now_ms;
}

SudekiMpSaveBookNativeLifecycleResult
SudekiMpSaveBookNativeLifecycleUpdate(
    SudekiMpSaveBookNativeLifecycle *lifecycle,
    int inspection_valid,
    unsigned int current_mode,
    unsigned int next_mode,
    uint32_t now_ms
) {
    int save_mode_active;

    if (lifecycle == NULL || lifecycle->terminal) {
        return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT;
    }
    save_mode_active = inspection_valid &&
        (current_mode == SUDEKIMP_SAVE_BOOK_NATIVE_UI_MODE ||
         next_mode == SUDEKIMP_SAVE_BOOK_NATIVE_UI_MODE);
    if (!lifecycle->mode_seen) {
        if (save_mode_active) {
            lifecycle->mode_seen = 1u;
            lifecycle->stable_non_mode_samples = 0u;
            return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTERED;
        }
        if (tick_reached(
                now_ms,
                lifecycle->started_at_ms +
                    SUDEKIMP_SAVE_BOOK_NATIVE_ENTER_TIMEOUT_MS)) {
            lifecycle->terminal = 1u;
            return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTER_FAILED;
        }
        return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT;
    }
    if (!inspection_valid || save_mode_active) {
        lifecycle->stable_non_mode_samples = 0u;
        return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT;
    }
    if (lifecycle->stable_non_mode_samples < UINT8_MAX) {
        ++lifecycle->stable_non_mode_samples;
    }
    if (lifecycle->stable_non_mode_samples <
        SUDEKIMP_SAVE_BOOK_NATIVE_CLOSE_STABLE_SAMPLES) {
        return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT;
    }
    lifecycle->terminal = 1u;
    return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED;
}

SudekiMpSaveBookNativeLifecycleResult
SudekiMpSaveBookNativeLifecycleSourceChanged(
    SudekiMpSaveBookNativeLifecycle *lifecycle
) {
    SudekiMpSaveBookNativeLifecycleResult result;

    if (lifecycle == NULL || lifecycle->terminal) {
        return SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT;
    }
    result = lifecycle->mode_seen ?
        SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED :
        SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTER_FAILED;
    lifecycle->terminal = 1u;
    return result;
}
