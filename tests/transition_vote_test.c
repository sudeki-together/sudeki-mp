#include "engine/transition_vote.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void test_single_player_bypasses_prompt(void) {
    SudekiMpTransitionVote vote;
    SudekiMpTransitionVoteResult result;

    SudekiMpTransitionVoteInitialize(&vote);
    result = SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x01u, 100u,
        SUDEKIMP_TRANSITION_VOTE_DEFAULT_TIMEOUT_MS);
    CHECK(result == SUDEKIMP_TRANSITION_VOTE_READY_NOW);
    CHECK(vote.state == SUDEKIMP_TRANSITION_VOTE_READY);
    CHECK(vote.participant_mask == 0x01u);
    CHECK(SudekiMpTransitionVoteBeginCommit(&vote, vote.serial) ==
        SUDEKIMP_TRANSITION_VOTE_COMMIT_STARTED);
}

static void test_two_players_accept_early(void) {
    SudekiMpTransitionVote vote;
    uint32_t serial;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    serial = vote.serial;
    CHECK(vote.accepted_mask == 0x01u);
    CHECK(SudekiMpTransitionVoteRespond(&vote, serial, 1u, 1) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);
    CHECK(vote.state == SUDEKIMP_TRANSITION_VOTE_READY);
    CHECK(SudekiMpTransitionVoteBeginCommit(&vote, serial) ==
        SUDEKIMP_TRANSITION_VOTE_COMMIT_STARTED);
    CHECK(SudekiMpTransitionVoteBeginCommit(&vote, serial) ==
        SUDEKIMP_TRANSITION_VOTE_REJECTED_BUSY);
}

static void test_cancel_wins(void) {
    SudekiMpTransitionVote vote;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, vote.serial, 1u, 0) ==
        SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW);
    CHECK(vote.state == SUDEKIMP_TRANSITION_VOTE_CANCELLED);
    CHECK(vote.cancelled_mask == 0x02u);
    CHECK(SudekiMpTransitionVoteBeginCommit(&vote, vote.serial) ==
        SUDEKIMP_TRANSITION_VOTE_REJECTED_BUSY);
}

static void test_silence_times_out_to_ready(void) {
    SudekiMpTransitionVote vote;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(SudekiMpTransitionVoteRemainingMs(&vote, 1100u) == 4000u);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x03u, 5099u) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x03u, 5100u) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);
}

static void test_visible_prompt_starts_full_countdown(void) {
    SudekiMpTransitionVote vote;
    uint32_t serial;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    serial = vote.serial;
    CHECK(!SudekiMpTransitionVoteRestartDeadline(
        &vote, serial + 1u, 1000u, 5000u));
    CHECK(SudekiMpTransitionVoteRestartDeadline(
        &vote, serial, 1000u, 5000u));
    CHECK(SudekiMpTransitionVoteRemainingMs(&vote, 1000u) == 5000u);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x03u, 5999u) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x03u, 6000u) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);
    CHECK(!SudekiMpTransitionVoteRestartDeadline(
        &vote, serial, 6000u, 5000u));
}

static void test_dropout_does_not_block(void) {
    SudekiMpTransitionVote vote;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x07u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, vote.serial, 1u, 1) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x03u, 200u) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);
    CHECK(vote.participant_mask == 0x03u);
}

static void test_requester_dropout_cancels(void) {
    SudekiMpTransitionVote vote;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 2u, 0x07u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x03u, 200u) ==
        SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW);
    CHECK(vote.cancelled_mask == 0x04u);
}

static void test_new_drop_in_does_not_join_vote(void) {
    SudekiMpTransitionVote vote;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(SudekiMpTransitionVoteUpdate(&vote, 0x07u, 200u) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(vote.participant_mask == 0x03u);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, vote.serial, 1u, 1) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);
}

static void test_stale_input_is_rejected_after_reset(void) {
    SudekiMpTransitionVote vote;
    uint32_t first_serial;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 100u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    first_serial = vote.serial;
    SudekiMpTransitionVoteReset(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, 200u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(vote.serial != first_serial);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, first_serial, 1u, 0) ==
        SUDEKIMP_TRANSITION_VOTE_REJECTED_STALE);
    CHECK(vote.state == SUDEKIMP_TRANSITION_VOTE_WAITING);
}

static void test_timer_wraparound(void) {
    SudekiMpTransitionVote vote;
    const uint32_t start = UINT32_MAX - 1000u;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x03u, start, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(SudekiMpTransitionVoteRemainingMs(&vote, start + 4000u) ==
        1000u);
    CHECK(SudekiMpTransitionVoteUpdate(
        &vote, 0x03u, start + 4999u) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(SudekiMpTransitionVoteUpdate(
        &vote, 0x03u, start + 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);
}

static void test_four_player_mask_and_invalid_inputs(void) {
    SudekiMpTransitionVote vote;

    SudekiMpTransitionVoteInitialize(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0xffu, 0u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_OPENED);
    CHECK(vote.participant_mask == 0x0fu);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, vote.serial, 1u, 1) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, vote.serial, 2u, 1) ==
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE);
    CHECK(SudekiMpTransitionVoteRespond(
        &vote, vote.serial, 3u, 1) ==
        SUDEKIMP_TRANSITION_VOTE_READY_NOW);

    SudekiMpTransitionVoteReset(&vote);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 4u, 0x0fu, 0u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 1u, 0x01u, 0u, 5000u) ==
        SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID);
    CHECK(SudekiMpTransitionVoteRequest(
        &vote, 0u, 0x01u, 0u, 0u) ==
        SUDEKIMP_TRANSITION_VOTE_REJECTED_INVALID);
}

int main(void) {
    test_single_player_bypasses_prompt();
    test_two_players_accept_early();
    test_cancel_wins();
    test_silence_times_out_to_ready();
    test_visible_prompt_starts_full_countdown();
    test_dropout_does_not_block();
    test_requester_dropout_cancels();
    test_new_drop_in_does_not_join_vote();
    test_stale_input_is_rejected_after_reset();
    test_timer_wraparound();
    test_four_player_mask_and_invalid_inputs();

    if (failures != 0) {
        fprintf(stderr, "%d transition-vote checks failed\n", failures);
        return 1;
    }
    puts("transition-vote checks passed");
    return 0;
}
