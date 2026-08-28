#include "engine/save_book_vote_input.h"

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

static SudekiMpInputBridgeState sample(
    uint32_t sequence,
    uint32_t buttons
) {
    SudekiMpInputBridgeState state;

    memset(&state, 0, sizeof(state));
    state.sequence = sequence;
    state.buttons = buttons;
    return state;
}

static void test_trigger_one_is_neutral(void) {
    SudekiMpSaveBookVoteInputFence fence;
    SudekiMpInputBridgeState baseline = sample(
        10u, SUDEKIMP_BRIDGE_BUTTON_A);
    SudekiMpInputBridgeState neutral = sample(11u, 0u);

    SudekiMpSaveBookVoteInputBegin(&fence, 7u, 1, &baseline);
    neutral.left_trigger =
        SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM;
    neutral.right_trigger =
        SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM;
    CHECK(SudekiMpSaveBookVoteInputIsNeutral(&neutral));
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 7u, &neutral, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED);
    CHECK(fence.consent_armed == 1u);

    neutral.sequence = 12u;
    neutral.left_trigger =
        SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM + 1u;
    CHECK(!SudekiMpSaveBookVoteInputIsNeutral(&neutral));
}

static void test_stale_and_duplicate_sequences_do_not_arm(void) {
    SudekiMpSaveBookVoteInputFence fence;
    SudekiMpInputBridgeState baseline = sample(100u, 0u);
    SudekiMpInputBridgeState packet = sample(100u, 0u);

    SudekiMpSaveBookVoteInputBegin(&fence, 8u, 1, &baseline);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 8u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    CHECK(fence.consent_armed == 0u && fence.last_sequence == 100u);
    packet.sequence = 99u;
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 8u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    CHECK(fence.consent_armed == 0u && fence.last_sequence == 100u);
    packet.sequence = UINT32_C(0x80000064);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 8u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    CHECK(fence.consent_armed == 0u && fence.last_sequence == 100u);
    packet.sequence = 101u;
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 8u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED);
}

static void check_held_at_open(uint32_t held_button) {
    SudekiMpSaveBookVoteInputFence fence;
    SudekiMpInputBridgeState packet = sample(200u, held_button);

    SudekiMpSaveBookVoteInputBegin(&fence, 9u, 1, &packet);
    packet.sequence = 201u;
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 9u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    CHECK(fence.consent_armed == 0u);
    packet = sample(202u, 0u);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 9u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED);
    packet = sample(203u, held_button);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 9u, &packet, 1) ==
        (held_button == SUDEKIMP_BRIDGE_BUTTON_B ?
            SUDEKIMP_SAVE_BOOK_VOTE_INPUT_VETO :
            SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ACCEPT));
}

static void test_held_buttons_require_newer_neutral(void) {
    check_held_at_open(SUDEKIMP_BRIDGE_BUTTON_A);
    check_held_at_open(SUDEKIMP_BRIDGE_BUTTON_B);
}

static void test_missing_baseline_and_hidden_edges_are_fenced(void) {
    SudekiMpSaveBookVoteInputFence fence;
    SudekiMpInputBridgeState packet = sample(300u, 0u);

    SudekiMpSaveBookVoteInputBegin(&fence, 10u, 0, NULL);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 10u, &packet, 0) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    CHECK(fence.baseline_valid == 1u && fence.consent_armed == 0u);
    packet.sequence = 301u;
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 10u, &packet, 0) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED);

    packet = sample(302u, SUDEKIMP_BRIDGE_BUTTON_A);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 10u, &packet, 0) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    packet.sequence = 303u;
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 10u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    packet = sample(304u, 0u);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 10u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    packet = sample(305u,
        SUDEKIMP_BRIDGE_BUTTON_A | SUDEKIMP_BRIDGE_BUTTON_B);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 10u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_VETO);

    packet = sample(306u, 0u);
    CHECK(SudekiMpSaveBookVoteInputAdvance(
        &fence, 11u, &packet, 1) ==
        SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE);
    CHECK(fence.last_sequence == 305u);
}

int main(void) {
    test_trigger_one_is_neutral();
    test_stale_and_duplicate_sequences_do_not_arm();
    test_held_buttons_require_newer_neutral();
    test_missing_baseline_and_hidden_edges_are_fenced();

    if (failures != 0) {
        fprintf(stderr, "%d save-book vote input test(s) failed\n",
            failures);
        return 1;
    }
    puts("save-book vote input tests passed");
    return 0;
}
