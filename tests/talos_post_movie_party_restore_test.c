#include "hooks/talos_post_movie_party_restore.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
            #expression); \
        return 0; \
    } \
} while (0)

static int advance(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    SudekiMpTalosPostMoviePartyRestoreEvent event,
    uint32_t value,
    uint32_t now_ms
) {
    return SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
        machine, event, value, now_ms) != FALSE;
}

static int reach_waiting_party(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    uint32_t now_ms
) {
    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(machine, TRUE);
    CHECK(advance(machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED,
        1u, now_ms));
    CHECK(advance(machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_ACCEPTED,
        1u, now_ms));
    CHECK(advance(machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SPAWN_RESULT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK, now_ms));
    CHECK(machine->state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY);
    return 1;
}

static int test_disabled_is_inert(void) {
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    SudekiMpTalosPostMoviePartyRestoreMachine before;

    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(&machine, FALSE);
    before = machine;
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_DISABLED);
    CHECK(!advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED,
        1u, 1u));
    CHECK(memcmp(&machine, &before, sizeof(machine)) == 0);
    return 1;
}

static int test_exact_happy_path_is_one_shot(void) {
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    SudekiMpTalosPostMoviePartyRestoreMachine active;
    const uint32_t start = 100u;

    CHECK(reach_waiting_party(&machine, start));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK,
        start + 999u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK,
        start + 1000u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_INITIALIZE_ACTORS);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK,
        start + 1000u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REQUEST_PLAYER_TWO);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT,
        1u, start + 1000u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PLAYER_TWO);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED,
        0u, start + 1100u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PLAYER_TWO);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED,
        1u, start + 1200u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REFRESH_COMBAT);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_COMBAT_REFRESH_RESULT,
        1u, start + 1200u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE);
    CHECK(machine.ticket_claimed == 1u);
    CHECK(machine.spawn_accepted_mask ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK);
    CHECK(machine.initialized_mask ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK);
    CHECK(machine.player_two_requested == 1u);
    CHECK(machine.player_two_active == 1u);
    CHECK(machine.combat_refreshed == 1u);

    active = machine;
    CHECK(!advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED,
        1u, start + 1300u));
    CHECK(memcmp(&machine, &active, sizeof(machine)) == 0);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SESSION_ENDED,
        1u, start + 1400u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED);
    active = machine;
    CHECK(!advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SESSION_ENDED,
        1u, start + 1500u));
    CHECK(memcmp(&machine, &active, sizeof(machine)) == 0);
    return 1;
}

static int test_spawn_rejection_is_sticky(void) {
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    SudekiMpTalosPostMoviePartyRestoreMachine failed;

    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(&machine, TRUE);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED,
        1u, 10u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_ACCEPTED,
        1u, 10u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SPAWN_RESULT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_AILISH_MASK |
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_BUKI_MASK,
        10u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_SPAWN_REJECTED);
    failed = machine;
    CHECK(!advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SPAWN_RESULT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK,
        11u));
    CHECK(memcmp(&machine, &failed, sizeof(machine)) == 0);
    return 1;
}

static int test_async_timeouts_are_wrap_safe(void) {
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    uint32_t start = UINT32_MAX - 500u;

    CHECK(reach_waiting_party(&machine, start));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        0u, start + 9999u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        0u, start + 10001u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_POSTSPAWN_TIMEOUT);
    return 1;
}

static int test_initialization_and_control_fail_closed(void) {
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    const uint32_t all =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK;

    CHECK(reach_waiting_party(&machine, 20u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        all, 1020u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_AILISH_MASK, 1020u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_ACTOR_INITIALIZE);

    CHECK(reach_waiting_party(&machine, 30u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        all, 1030u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
        all, 1030u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT,
        0u, 1030u));
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_REQUEST);

    CHECK(reach_waiting_party(&machine, 40u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        all, 1040u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
        all, 1040u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT,
        1u, 1040u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED,
        0u, 6041u));
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_TIMEOUT);

    CHECK(reach_waiting_party(&machine, 50u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
        all, 1050u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
        all, 1050u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT,
        1u, 1050u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED,
        1u, 1051u));
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_COMBAT_REFRESH_RESULT,
        0u, 1051u));
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_COMBAT_REFRESH);
    return 1;
}

static int test_invalid_events_do_not_mutate(void) {
    SudekiMpTalosPostMoviePartyRestoreMachine machine;
    SudekiMpTalosPostMoviePartyRestoreMachine before;

    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(&machine, TRUE);
    before = machine;
    CHECK(!advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED,
        2u, 1u));
    CHECK(memcmp(&machine, &before, sizeof(machine)) == 0);
    CHECK(!advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT,
        999u, 1u));
    CHECK(memcmp(&machine, &before, sizeof(machine)) == 0);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_WITNESS_LOST,
        1u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_WITNESS_LOST);

    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(&machine, TRUE);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY,
        2u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);

    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(&machine, TRUE);
    CHECK(advance(&machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE,
        3u));
    CHECK(machine.state ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    CHECK(machine.failure ==
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE);
    return 1;
}

static SudekiMpTalosPostMoviePartyRestoreStatus camera_ready_status(void) {
    SudekiMpTalosPostMoviePartyRestoreStatus status;

    memset(&status, 0, sizeof(status));
    status.installed = 1u;
    status.enabled = 1u;
    status.observer_registered = 1u;
    status.ai_filter_installed = 1u;
    status.cleanroom_initialized = 1u;
    status.machine.state =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE;
    status.machine.failure =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_NONE;
    status.machine.player_two_active = 1u;
    status.machine.combat_refreshed = 1u;
    status.last_party_count = 4u;
    status.target_policy_active = 1u;
    status.boss_filter_identity_ready = 1u;
    status.combat_ready = 1u;
    status.party_topology_exact = 1u;
    status.control_state_exact = 1u;
    status.player_two_input_ready = 1u;
    status.valid = 1u;
    return status;
}

static int test_dual_camera_authorization_is_exact_and_fail_closed(void) {
    SudekiMpTalosPostMoviePartyRestoreStatus status = camera_ready_status();
    SudekiMpTalosPostMoviePartyRestoreStatus before;

    CHECK(!SudekiMpTalosPostMoviePartyRestoreCameraAuthorized(NULL));
    CHECK(SudekiMpTalosPostMoviePartyRestoreCameraAuthorized(&status));

#define REJECT_CAMERA_FIELD(field, value) do { \
    before = status; \
    status.field = (value); \
    CHECK(!SudekiMpTalosPostMoviePartyRestoreCameraAuthorized(&status)); \
    status = before; \
} while (0)
    REJECT_CAMERA_FIELD(installed, 0u);
    REJECT_CAMERA_FIELD(enabled, 0u);
    REJECT_CAMERA_FIELD(observer_registered, 0u);
    REJECT_CAMERA_FIELD(ai_filter_installed, 0u);
    REJECT_CAMERA_FIELD(cleanroom_initialized, 0u);
    REJECT_CAMERA_FIELD(machine.state,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_TICKET);
    REJECT_CAMERA_FIELD(machine.state,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED);
    REJECT_CAMERA_FIELD(machine.state,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED);
    REJECT_CAMERA_FIELD(machine.failure,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE);
    REJECT_CAMERA_FIELD(machine.player_two_active, 0u);
    REJECT_CAMERA_FIELD(machine.combat_refreshed, 0u);
    REJECT_CAMERA_FIELD(last_party_count, 3u);
    REJECT_CAMERA_FIELD(target_policy_active, 0u);
    REJECT_CAMERA_FIELD(boss_filter_identity_ready, 0u);
    REJECT_CAMERA_FIELD(combat_ready, 0u);
    REJECT_CAMERA_FIELD(party_topology_exact, 0u);
    REJECT_CAMERA_FIELD(control_state_exact, 0u);
    REJECT_CAMERA_FIELD(player_two_input_ready, 0u);
    REJECT_CAMERA_FIELD(valid, 0u);
    REJECT_CAMERA_FIELD(reload_required, 1u);
#undef REJECT_CAMERA_FIELD
    return 1;
}

static int test_dual_camera_bundle_is_all_or_nothing(void) {
    unsigned int mask;

    for (mask = 0u; mask < 16u; ++mask) {
        const int expected = mask == 0u ||
            mask == SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_EXACT;
        CHECK((SudekiMpTalosPostMoviePartyRestoreCameraBundleAllowed(mask) !=
            FALSE) == expected);
    }
    return 1;
}

static int test_dual_camera_navigation_profile_is_exact(void) {
    unsigned int mask;

    for (mask = 0u; mask < 16u; ++mask) {
        CHECK((SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
            mask, FALSE) != FALSE) == (mask == 0u));
        CHECK((SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
            mask, TRUE) != FALSE) ==
            (mask == SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_EXACT));
    }
    CHECK(!SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
        SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_EXACT, (BOOL)2));
    return 1;
}

int main(void) {
    CHECK(test_disabled_is_inert());
    CHECK(test_exact_happy_path_is_one_shot());
    CHECK(test_spawn_rejection_is_sticky());
    CHECK(test_async_timeouts_are_wrap_safe());
    CHECK(test_initialization_and_control_fail_closed());
    CHECK(test_invalid_events_do_not_mutate());
    CHECK(test_dual_camera_authorization_is_exact_and_fail_closed());
    CHECK(test_dual_camera_bundle_is_all_or_nothing());
    CHECK(test_dual_camera_navigation_profile_is_exact());
    puts("talos post-movie party restore tests passed");
    return 0;
}
