#include "hooks/lan_arena_client_skill_handoff.h"
#include "network/lan_arena_protocol.h"

#include <stdio.h>

static int failures;

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); \
    ++failures; \
} } while (0)

int main(void) {
    {
        SudekiMpLanArenaClientSkillRetryGate gate = { 0 };
        unsigned int attempt;

        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 10u, 2u, 1000u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT);
        CHECK(gate.attempt_count == 1u);
        /* ApplyLatest plus repeated render boundaries for the same frame do
         * not allocate duplicate validator/resource/Use attempts. */
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 10u, 2u, 1000u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_WAIT);
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 10u, 2u, 1099u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_WAIT);
        CHECK(gate.attempt_count == 1u);
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 10u, 2u, 1100u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT);
        CHECK(gate.attempt_count == 2u);

        /* Either half of the transaction identity resets the finite budget
         * and permits one immediate attempt. */
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 11u, 2u, 1100u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT);
        CHECK(gate.sequence == 11u && gate.slot == 2u &&
            gate.attempt_count == 1u);
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 11u, 3u, 1100u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT);
        CHECK(gate.sequence == 11u && gate.slot == 3u &&
            gate.attempt_count == 1u);

        /* Once Use has returned STARTED, no later service boundary may
         * allocate a second native task for the transaction. */
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 11u, 3u, 1200u, 1) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ALREADY_STARTED);
        CHECK(gate.attempt_count == 1u);

        SudekiMpLanArenaClientSkillRetryReset(&gate);
        for (attempt = 0u;
             attempt < SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_MAX_ATTEMPTS;
             ++attempt) {
            CHECK(SudekiMpLanArenaClientSkillRetryDecide(
                &gate, 12u, 1u,
                attempt *
                    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_BACKOFF_MS,
                0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT);
        }
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 12u, 1u,
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_MAX_ATTEMPTS *
                SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_BACKOFF_MS,
            0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_EXHAUSTED);
        CHECK(gate.attempt_count ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_MAX_ATTEMPTS);
        /* Exhaustion is transaction-local; it neither consumes nor poisons
         * the next host-approved sequence. */
        CHECK(SudekiMpLanArenaClientSkillRetryDecide(
            &gate, 13u, 1u, 3000u, 0) ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT);
        CHECK(gate.sequence == 13u && gate.attempt_count == 1u);
    }

    CHECK(!SudekiMpLanArenaClientSkillNativeLeaseMayRetire(0, 0, 0, 0));
    CHECK(!SudekiMpLanArenaClientSkillNativeLeaseMayRetire(1, 0, 1, 0));
    CHECK(!SudekiMpLanArenaClientSkillNativeLeaseMayRetire(1, 1, 0, 0));
    CHECK(!SudekiMpLanArenaClientSkillNativeLeaseMayRetire(1, 1, 1, 1));
    CHECK(SudekiMpLanArenaClientSkillNativeLeaseMayRetire(1, 1, 1, 0));

    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        0, 0, 0u, 9u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
        0, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_ACCEPT);

    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 0, 9u, 9u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
        1, 1) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_CONTINUE);

    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
        1, 1) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
        0, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
        1, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETIRE_THEN_ACCEPT);
    /* STARTED followed by an initial exact inactive observation is a native
     * startup gap. It cannot retire or hand off until this task has first
     * been positively observed active. */
    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 0, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
        1, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN);

    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT,
        1, 1) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT,
        0, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 10u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT,
        1, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETIRE_THEN_ACCEPT);

    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 0u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE,
        1, 1) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientSkillHandoffDecide(
        1, 1, 9u, 0u, SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE,
        1, 0) == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETIRE_THEN_ACCEPT);

    CHECK(SudekiMpLanArenaClientOtherSkillDecide(0, 0, 0, 0) ==
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_READY);
    CHECK(SudekiMpLanArenaClientOtherSkillDecide(1, 1, 0, 0) ==
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientOtherSkillDecide(1, 1, 1, 1) ==
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_DRAIN);
    CHECK(SudekiMpLanArenaClientOtherSkillDecide(1, 1, 1, 0) ==
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_RETIRE);
    CHECK(SudekiMpLanArenaClientOtherSkillDecide(1, 0, 1, 0) ==
        SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_DRAIN);

    {
        uint16_t committed_sequence = 9u;
        /* A transient first failure does not consume sequence 10. The next
         * active snapshot for that exact sequence is therefore an attempt,
         * not an already-handled transaction. */
        committed_sequence =
            SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
                committed_sequence, 10u, 1, 0);
        CHECK(committed_sequence == 9u);
        committed_sequence =
            SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
                committed_sequence, 10u, 1, 0);
        CHECK(committed_sequence == 9u);
        /* STARTED commits even if the immediate native observation is still
         * in the startup gap; the retained task/active_seen lease then owns
         * conservative retirement. */
        committed_sequence =
            SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
                committed_sequence, 10u, 1, 1);
        CHECK(committed_sequence == 10u);
        CHECK(SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
            committed_sequence, 11u, 0, 0) == 11u);
    }

    CHECK(!SudekiMpLanArenaClientSkillDamageContainmentRequired(
        0, 0, 0, 0));
    CHECK(SudekiMpLanArenaClientSkillDamageContainmentRequired(
        1, 0, 0, 0));
    CHECK(SudekiMpLanArenaClientSkillDamageContainmentRequired(
        0, 1, 0, 0));
    CHECK(SudekiMpLanArenaClientSkillDamageContainmentRequired(
        0, 0, 1, 0));
    CHECK(SudekiMpLanArenaClientSkillDamageContainmentRequired(
        0, 0, 0, 1));

    CHECK(!SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
        0, 0, 0, 0));
    CHECK(SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
        1, 0, 0, 0));
    /* Authority loss alone must not restore the retail alternate speed while
     * an activation entry, asynchronous task/action, or teardown is live. */
    CHECK(SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
        0, 1, 0, 0));
    CHECK(SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
        0, 0, 1, 0));
    CHECK(SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
        0, 0, 0, 1));

    if (failures != 0) {
        fprintf(stderr, "%d LAN client skill handoff test(s) failed\n",
            failures);
        return 1;
    }
    puts("LAN client skill handoff tests passed");
    return 0;
}
