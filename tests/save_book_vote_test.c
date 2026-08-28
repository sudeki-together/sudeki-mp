#include "engine/save_book_vote.h"

#include <stdio.h>

static int failures;

static void check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void open_two_player_vote(
    SudekiMpSaveBookVote *vote,
    uint32_t now_ms
) {
    SudekiMpSaveBookVoteInitialize(vote);
    check(SudekiMpSaveBookVoteRequest(vote, 0x03u, now_ms) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "two-player save request awaits visible prompt");
    check(vote->participant_mask == 0x03u &&
            vote->accepted_mask == 0x01u,
        "host is snapshotted and implicitly accepted");
}

static void test_single_host_replays_once(void) {
    SudekiMpSaveBookVote vote;
    uint32_t serial;

    SudekiMpSaveBookVoteInitialize(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x01u, 10u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW,
        "single host bypasses prompt");
    serial = vote.serial;
    check(SudekiMpSaveBookVoteBeginReplay(&vote, serial) ==
            SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_STARTED,
        "ready request claims native continuation");
    check(SudekiMpSaveBookVoteBeginReplay(&vote, serial) ==
            SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY,
        "native continuation cannot be claimed twice");
    check(SudekiMpSaveBookVoteFinishReplay(&vote, serial, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_FINISHED,
        "one native continuation records completion");
    check(SudekiMpSaveBookVoteBeginReplay(&vote, serial) ==
            SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY,
        "completed continuation remains one-shot");
}

static void test_visible_ack_starts_full_ten_seconds(void) {
    SudekiMpSaveBookVote vote;
    uint32_t serial;

    open_two_player_vote(&vote, 100u);
    serial = vote.serial;
    check(SudekiMpSaveBookVoteRemainingMs(&vote, 999u) == 0u,
        "hidden prompt has no running consent timer");
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x03u, 999u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "hidden prompt cannot time out into consent");
    check(SudekiMpSaveBookVoteReportOverlay(&vote, serial, 1, 1000u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "visible prompt is acknowledged");
    check(vote.state == SUDEKIMP_SAVE_BOOK_VOTE_WAITING &&
            SudekiMpSaveBookVoteRemainingMs(&vote, 1000u) == 10000u,
        "first visible frame starts full ten-second window");
    check(SudekiMpSaveBookVoteReportOverlay(&vote, serial, 1, 5000u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE &&
            SudekiMpSaveBookVoteRemainingMs(&vote, 5000u) == 6000u,
        "later draws do not restart countdown");
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x03u, 10999u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "silence cannot commit before full visible window");
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x03u, 11000u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW,
        "silence accepts exactly at ten-second deadline");
}

static void test_overlay_failure_and_absence_cancel(void) {
    SudekiMpSaveBookVote vote;
    uint32_t serial;

    open_two_player_vote(&vote, 500u);
    serial = vote.serial;
    check(SudekiMpSaveBookVoteReportOverlay(&vote, serial, 0, 600u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW &&
            vote.state == SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED,
        "explicit draw failure cancels fail-closed");

    SudekiMpSaveBookVoteReset(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x03u, 0xfffffff0u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "wraparound request opens");
    check(SudekiMpSaveBookVoteUpdate(
            &vote, 0x03u, 0xfffffff0u + 999u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "overlay absence waits bounded grace across wrap");
    check(SudekiMpSaveBookVoteUpdate(
            &vote, 0x03u, 0xfffffff0u + 1000u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW,
        "absent overlay cancels at bounded deadline across wrap");
}

static void test_accept_veto_and_stale_input(void) {
    SudekiMpSaveBookVote vote;
    uint32_t old_serial;

    open_two_player_vote(&vote, 0u);
    old_serial = vote.serial;
    check(SudekiMpSaveBookVoteRespond(&vote, old_serial, 1u, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY,
        "P2 cannot answer before visible ACK");
    check(SudekiMpSaveBookVoteReportOverlay(&vote, old_serial, 1, 20u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "first prompt becomes answerable");
    check(SudekiMpSaveBookVoteRespond(&vote, old_serial, 1u, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW,
        "P2 A reaches unanimous READY early");

    SudekiMpSaveBookVoteReset(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x03u, 100u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "next prompt opens with a new serial");
    check(vote.serial != old_serial,
        "reset preserves monotonic serial against delayed input");
    check(SudekiMpSaveBookVoteReportOverlay(
            &vote, old_serial, 1, 120u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE,
        "old overlay ACK cannot arm new prompt");
    check(SudekiMpSaveBookVoteReportOverlay(
            &vote, vote.serial, 1, 120u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "new prompt acknowledges exact serial");
    check(SudekiMpSaveBookVoteRespond(
            &vote, vote.serial, 1u, 0) ==
            SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW &&
            vote.cancelled_mask == 0x02u,
        "P2 B vetoes immediately");
}

static void test_dropouts_joins_and_host_loss(void) {
    SudekiMpSaveBookVote vote;

    open_two_player_vote(&vote, 0u);
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x07u, 5u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE &&
            vote.participant_mask == 0x03u,
        "later Player 3 join is excluded from snapshot");
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x05u, 6u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW &&
            vote.participant_mask == 0x01u,
        "P2 dropout is removed and cannot block host");

    SudekiMpSaveBookVoteReset(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x03u, 10u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "host-loss case opens");
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x02u, 11u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW &&
            vote.cancelled_mask == 0x01u,
        "host invalidation cancels instead of replaying");
}

static void test_four_human_unanimous_and_vetoes(void) {
    SudekiMpSaveBookVote vote;
    uint32_t serial;

    SudekiMpSaveBookVoteInitialize(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x0fu, 100u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "four-human electorate snapshots all active seats");
    serial = vote.serial;
    check(SudekiMpSaveBookVoteReportOverlay(&vote, serial, 1, 110u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "four-human prompt is visible before responses");
    check(SudekiMpSaveBookVoteRespond(&vote, serial, 1u, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE &&
            SudekiMpSaveBookVoteRespond(&vote, serial, 2u, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE &&
            SudekiMpSaveBookVoteRespond(&vote, serial, 3u, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW,
        "P2 P3 and P4 acceptance reaches unanimous READY early");

    SudekiMpSaveBookVoteReset(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x0fu, 200u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "non-host veto case opens");
    serial = vote.serial;
    (void)SudekiMpSaveBookVoteReportOverlay(&vote, serial, 1, 210u);
    check(SudekiMpSaveBookVoteRespond(&vote, serial, 2u, 0) ==
            SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW &&
            vote.cancelled_mask == 0x04u,
        "any non-host veto cancels immediately");

    SudekiMpSaveBookVoteReset(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x0fu, 300u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "host veto case opens");
    serial = vote.serial;
    (void)SudekiMpSaveBookVoteReportOverlay(&vote, serial, 1, 310u);
    check(SudekiMpSaveBookVoteRespond(&vote, serial, 0u, 0) ==
            SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW &&
            vote.cancelled_mask == 0x01u,
        "host veto cancels immediately");
}

static void test_four_human_snapshot_join_and_dropout_policy(void) {
    SudekiMpSaveBookVote vote;
    uint32_t serial;

    SudekiMpSaveBookVoteInitialize(&vote);
    check(SudekiMpSaveBookVoteRequest(&vote, 0x07u, 0u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_OPENED,
        "three-human electorate opens");
    serial = vote.serial;
    (void)SudekiMpSaveBookVoteReportOverlay(&vote, serial, 1, 1u);
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x0fu, 2u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE &&
            vote.participant_mask == 0x07u,
        "later P4 join is excluded from electorate");
    check(SudekiMpSaveBookVoteRespond(&vote, serial, 1u, 1) ==
            SUDEKIMP_SAVE_BOOK_VOTE_NO_CHANGE,
        "remaining P2 accepts");
    check(SudekiMpSaveBookVoteUpdate(&vote, 0x03u, 3u) ==
            SUDEKIMP_SAVE_BOOK_VOTE_READY_NOW &&
            vote.participant_mask == 0x03u,
        "P3 dropout removes the final blocker");
}

static void test_native_mode_lifecycle(void) {
    SudekiMpSaveBookNativeLifecycle lifecycle;

    SudekiMpSaveBookNativeLifecycleBegin(&lifecycle, 100u);
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 1, 0u, 0u, 2099u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT,
        "native entry waits for mode 12 within bounded grace");
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 0, 0u, 0u, 2100u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTER_FAILED,
        "unproven native entry fails closed at bound");

    SudekiMpSaveBookNativeLifecycleBegin(&lifecycle, 0xfffffff0u);
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 1, 0u, 12u, 4u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTERED,
        "next mode 12 proves native save entry across tick wrap");
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 0, 0u, 0u, 5u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT,
        "unreadable post-entry sample cannot synthesize close");
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 1, 0u, 0u, 6u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT,
        "first non-save sample is not a stable close");
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 1, 12u, 0u, 7u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT,
        "mode 12 recurrence resets close stability");
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 1, 0u, 0u, 8u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_WAIT &&
            SudekiMpSaveBookNativeLifecycleUpdate(
                &lifecycle, 1, 0u, 0u, 9u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED,
        "two stable non-12 samples confirm native close");

    SudekiMpSaveBookNativeLifecycleBegin(&lifecycle, 20u);
    check(SudekiMpSaveBookNativeLifecycleSourceChanged(&lifecycle) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTER_FAILED,
        "source change before mode 12 fails native entry");
    SudekiMpSaveBookNativeLifecycleBegin(&lifecycle, 30u);
    check(SudekiMpSaveBookNativeLifecycleUpdate(
            &lifecycle, 1, 12u, 12u, 31u) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTERED &&
            SudekiMpSaveBookNativeLifecycleSourceChanged(&lifecycle) ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED,
        "source change after mode 12 confirms save load teardown");
}

int main(void) {
    test_single_host_replays_once();
    test_visible_ack_starts_full_ten_seconds();
    test_overlay_failure_and_absence_cancel();
    test_accept_veto_and_stale_input();
    test_dropouts_joins_and_host_loss();
    test_four_human_unanimous_and_vetoes();
    test_four_human_snapshot_join_and_dropout_policy();
    test_native_mode_lifecycle();

    if (failures != 0) {
        fprintf(stderr, "%d save-book vote test(s) failed\n", failures);
        return 1;
    }
    puts("save-book vote tests passed");
    return 0;
}
