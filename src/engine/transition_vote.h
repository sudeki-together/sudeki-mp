#ifndef SUDEKIMP_TRANSITION_VOTE_H
#define SUDEKIMP_TRANSITION_VOTE_H

#include <stdint.h>

enum {
    SUDEKIMP_TRANSITION_VOTE_MAX_PLAYERS = 4u,
    SUDEKIMP_TRANSITION_VOTE_PLAYER_MASK = 0x0fu,
    SUDEKIMP_TRANSITION_VOTE_DEFAULT_TIMEOUT_MS = 5000u
};

typedef enum SudekiMpTransitionVoteState {
    SUDEKIMP_TRANSITION_VOTE_IDLE = 0,
    SUDEKIMP_TRANSITION_VOTE_WAITING = 1,
    SUDEKIMP_TRANSITION_VOTE_READY = 2,
    SUDEKIMP_TRANSITION_VOTE_COMMITTING = 3,
    SUDEKIMP_TRANSITION_VOTE_CANCELLED = 4
} SudekiMpTransitionVoteState;

typedef enum SudekiMpTransitionVoteResult {
    SUDEKIMP_TRANSITION_VOTE_NO_CHANGE = 0,
    SUDEKIMP_TRANSITION_VOTE_OPENED = 1,
    SUDEKIMP_TRANSITION_VOTE_READY_NOW = 2,
    SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW = 3,
    SUDEKIMP_TRANSITION_VOTE_COMMIT_STARTED = 4,
    SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID = 5,
    SUDEKIMP_TRANSITION_VOTE_REJECTED_BUSY = 6,
    SUDEKIMP_TRANSITION_VOTE_REJECTED_STALE = 7
} SudekiMpTransitionVoteResult;

typedef struct SudekiMpTransitionVote {
    SudekiMpTransitionVoteState state;
    uint32_t serial;
    uint32_t started_at_ms;
    uint32_t deadline_ms;
    uint8_t requester_index;
    uint8_t participant_mask;
    uint8_t accepted_mask;
    uint8_t cancelled_mask;
} SudekiMpTransitionVote;

/* Initialize once. Reset preserves the serial so delayed input from an older
 * prompt cannot answer a newer vote accidentally. */
void SudekiMpTransitionVoteInitialize(SudekiMpTransitionVote *vote);
void SudekiMpTransitionVoteReset(SudekiMpTransitionVote *vote);

/* active_human_mask contains only players who are currently dropped in and
 * own live input. AI companions and selected-but-not-yet-present characters
 * must not be included. The requester implicitly accepts its own request.
 * One active human therefore returns READY_NOW and never opens a prompt. */
SudekiMpTransitionVoteResult SudekiMpTransitionVoteRequest(
    SudekiMpTransitionVote *vote,
    unsigned int requester_index,
    uint8_t active_human_mask,
    uint32_t now_ms,
    uint32_t timeout_ms
);

/* serial must be copied from the visible prompt. This rejects a delayed
 * accept/cancel edge from a prompt that has already closed. */
SudekiMpTransitionVoteResult SudekiMpTransitionVoteRespond(
    SudekiMpTransitionVote *vote,
    uint32_t serial,
    unsigned int player_index,
    int accept
);

/* Service on the gameplay thread. Dropouts are removed from the participant
 * snapshot and never block progress. New drop-ins do not join an in-flight
 * vote. If the requester drops out, the request is cancelled. */
SudekiMpTransitionVoteResult SudekiMpTransitionVoteUpdate(
    SudekiMpTransitionVote *vote,
    uint8_t active_human_mask,
    uint32_t now_ms
);

/* Claim READY immediately before quiescing player input/cameras and calling
 * the one saved native transition. Only one caller can move to COMMITTING. */
SudekiMpTransitionVoteResult SudekiMpTransitionVoteBeginCommit(
    SudekiMpTransitionVote *vote,
    uint32_t serial
);

/* Restart the full countdown when the prompt has actually been submitted by
 * the in-game renderer. Hidden/unrendered requests are not allowed to consume
 * consent time or auto-commit. */
int SudekiMpTransitionVoteRestartDeadline(
    SudekiMpTransitionVote *vote,
    uint32_t serial,
    uint32_t now_ms,
    uint32_t timeout_ms
);

uint32_t SudekiMpTransitionVoteRemainingMs(
    const SudekiMpTransitionVote *vote,
    uint32_t now_ms
);

#endif
