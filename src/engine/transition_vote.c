#include "engine/transition_vote.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static int tick_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint8_t player_bit(unsigned int player_index) {
    if (player_index >= SUDEKIMP_TRANSITION_VOTE_MAX_PLAYERS) {
        return 0u;
    }
    return (uint8_t)(1u << player_index);
}

static void mark_ready(SudekiMpTransitionVote *vote) {
    vote->state = SUDEKIMP_TRANSITION_VOTE_READY;
}

static void mark_cancelled(
    SudekiMpTransitionVote *vote,
    uint8_t cancelled_bit
) {
    vote->cancelled_mask = cancelled_bit;
    vote->state = SUDEKIMP_TRANSITION_VOTE_CANCELLED;
}

void SudekiMpTransitionVoteInitialize(SudekiMpTransitionVote *vote) {
    if (vote != NULL) {
        memset(vote, 0, sizeof(*vote));
    }
}

void SudekiMpTransitionVoteReset(SudekiMpTransitionVote *vote) {
    uint32_t serial;

    if (vote == NULL) {
        return;
    }
    serial = vote->serial;
    memset(vote, 0, sizeof(*vote));
    vote->serial = serial;
}

SudekiMpTransitionVoteResult SudekiMpTransitionVoteRequest(
    SudekiMpTransitionVote *vote,
    unsigned int requester_index,
    uint8_t active_human_mask,
    uint32_t now_ms,
    uint32_t timeout_ms
) {
    uint8_t requester_bit;

    if (vote == NULL || timeout_ms == 0u || timeout_ms > INT32_MAX) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID;
    }
    if (vote->state != SUDEKIMP_TRANSITION_VOTE_IDLE) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_BUSY;
    }
    requester_bit = player_bit(requester_index);
    active_human_mask &= SUDEKIMP_TRANSITION_VOTE_PLAYER_MASK;
    if (requester_bit == 0u ||
        (active_human_mask & requester_bit) == 0u) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID;
    }

    ++vote->serial;
    if (vote->serial == 0u) {
        vote->serial = 1u;
    }
    vote->started_at_ms = now_ms;
    vote->deadline_ms = now_ms + timeout_ms;
    vote->requester_index = (uint8_t)requester_index;
    vote->participant_mask = active_human_mask;
    vote->accepted_mask = requester_bit;
    vote->cancelled_mask = 0u;

    if ((active_human_mask & (uint8_t)~requester_bit) == 0u) {
        mark_ready(vote);
        return SUDEKIMP_TRANSITION_VOTE_READY_NOW;
    }
    vote->state = SUDEKIMP_TRANSITION_VOTE_WAITING;
    return SUDEKIMP_TRANSITION_VOTE_OPENED;
}

SudekiMpTransitionVoteResult SudekiMpTransitionVoteRespond(
    SudekiMpTransitionVote *vote,
    uint32_t serial,
    unsigned int player_index,
    int accept
) {
    uint8_t bit;

    if (vote == NULL) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID;
    }
    if (serial == 0u || serial != vote->serial) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_STALE;
    }
    if (vote->state != SUDEKIMP_TRANSITION_VOTE_WAITING) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_BUSY;
    }
    bit = player_bit(player_index);
    if (bit == 0u || (vote->participant_mask & bit) == 0u) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID;
    }

    if (!accept) {
        mark_cancelled(vote, bit);
        return SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW;
    }
    vote->accepted_mask |= bit;
    if ((vote->accepted_mask & vote->participant_mask) ==
        vote->participant_mask) {
        mark_ready(vote);
        return SUDEKIMP_TRANSITION_VOTE_READY_NOW;
    }
    return SUDEKIMP_TRANSITION_VOTE_NO_CHANGE;
}

SudekiMpTransitionVoteResult SudekiMpTransitionVoteUpdate(
    SudekiMpTransitionVote *vote,
    uint8_t active_human_mask,
    uint32_t now_ms
) {
    uint8_t requester_bit;

    if (vote == NULL) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID;
    }
    if (vote->state != SUDEKIMP_TRANSITION_VOTE_WAITING) {
        return SUDEKIMP_TRANSITION_VOTE_NO_CHANGE;
    }
    active_human_mask &= SUDEKIMP_TRANSITION_VOTE_PLAYER_MASK;
    requester_bit = player_bit(vote->requester_index);
    if ((active_human_mask & requester_bit) == 0u) {
        mark_cancelled(vote, requester_bit);
        return SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW;
    }

    /* A join after the request cannot extend or veto this vote. A dropout
     * leaves the participant snapshot and its missing response disappears. */
    vote->participant_mask &= active_human_mask;
    vote->accepted_mask &= vote->participant_mask;
    if ((vote->accepted_mask & vote->participant_mask) ==
        vote->participant_mask || tick_reached(now_ms, vote->deadline_ms)) {
        mark_ready(vote);
        return SUDEKIMP_TRANSITION_VOTE_READY_NOW;
    }
    return SUDEKIMP_TRANSITION_VOTE_NO_CHANGE;
}

SudekiMpTransitionVoteResult SudekiMpTransitionVoteBeginCommit(
    SudekiMpTransitionVote *vote,
    uint32_t serial
) {
    if (vote == NULL) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID;
    }
    if (serial == 0u || serial != vote->serial) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_STALE;
    }
    if (vote->state != SUDEKIMP_TRANSITION_VOTE_READY) {
        return SUDEKIMP_TRANSITION_VOTE_REJECTED_BUSY;
    }
    vote->state = SUDEKIMP_TRANSITION_VOTE_COMMITTING;
    return SUDEKIMP_TRANSITION_VOTE_COMMIT_STARTED;
}

int SudekiMpTransitionVoteRestartDeadline(
    SudekiMpTransitionVote *vote,
    uint32_t serial,
    uint32_t now_ms,
    uint32_t timeout_ms
) {
    if (vote == NULL || serial == 0u || serial != vote->serial ||
        vote->state != SUDEKIMP_TRANSITION_VOTE_WAITING ||
        timeout_ms == 0u || timeout_ms > INT32_MAX) {
        return 0;
    }
    vote->started_at_ms = now_ms;
    vote->deadline_ms = now_ms + timeout_ms;
    return 1;
}

uint32_t SudekiMpTransitionVoteRemainingMs(
    const SudekiMpTransitionVote *vote,
    uint32_t now_ms
) {
    if (vote == NULL || vote->state != SUDEKIMP_TRANSITION_VOTE_WAITING ||
        tick_reached(now_ms, vote->deadline_ms)) {
        return 0u;
    }
    return vote->deadline_ms - now_ms;
}
