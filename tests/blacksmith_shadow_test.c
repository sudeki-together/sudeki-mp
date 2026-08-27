#include "engine/blacksmith_shadow.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static const SudekiMpBlacksmithSharedSnapshot initial_snapshot = {
    10u,
    20u,
    25u,
    30u
};

static void initialize_players(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_count
) {
    uint32_t player_index;

    SudekiMpBlacksmithShadowInitialize(coordinator);
    CHECK(SudekiMpBlacksmithShadowPublishSharedSnapshot(
        coordinator,
        &initial_snapshot
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    for (player_index = 0u; player_index < player_count; ++player_index) {
        CHECK(SudekiMpBlacksmithShadowPublishPlayer(
            coordinator,
            player_index,
            100u + player_index,
            1000u + player_index,
            1
        ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
        CHECK(SudekiMpBlacksmithShadowOpen(
            coordinator,
            player_index,
            900u,
            90u
        ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    }
}

static void prepare_confirmed_build(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t equipment_item_id,
    uint32_t component_item_id,
    uint32_t socket_index,
    uint32_t cost
) {
    SudekiMpBlacksmithPlayerShadow *shadow =
        &coordinator->players[player_index];

    CHECK(SudekiMpBlacksmithShadowSetNavigation(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        2u + player_index,
        4u + player_index,
        6u + player_index
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetBuild(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        equipment_item_id,
        component_item_id,
        socket_index,
        player_index & 1u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetQuote(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        cost
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetConfirmation(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
}

static void test_four_independent_player_shadows(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithPlayerShadow copy;
    uint32_t player_index;
    uint32_t player_two_revision;

    initialize_players(&coordinator, 4u);
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS;
         ++player_index) {
        CHECK(coordinator.players[player_index].state ==
            SUDEKIMP_BLACKSMITH_SHADOW_BROWSING);
        CHECK(coordinator.players[player_index].player_index == player_index);
        CHECK(coordinator.players[player_index].session_serial != 0u);
        if (player_index != 0u) {
            CHECK(coordinator.players[player_index].session_serial !=
                coordinator.players[player_index - 1u].session_serial);
        }
    }

    player_two_revision = coordinator.players[1].revision;
    CHECK(SudekiMpBlacksmithShadowSetNavigation(
        &coordinator,
        0u,
        coordinator.players[0].session_serial,
        coordinator.players[0].revision,
        11u,
        12u,
        13u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetBuild(
        &coordinator,
        0u,
        coordinator.players[0].session_serial,
        coordinator.players[0].revision,
        1001u,
        2001u,
        2u,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.players[0].selection.cursor == 13u);
    CHECK(coordinator.players[0].selection.component_item_id == 2001u);
    CHECK(coordinator.players[0].selection.socket_index == 2u);
    CHECK(coordinator.players[0].selection.socket_bank == 1);
    CHECK(coordinator.players[1].revision == player_two_revision);
    CHECK(coordinator.players[1].selection.cursor == 0u);
    CHECK(coordinator.players[1].selection.component_item_id == 0u);

    CHECK(SudekiMpBlacksmithShadowSetBuild(
        &coordinator,
        1u,
        coordinator.players[1].session_serial,
        coordinator.players[1].revision,
        1002u,
        2002u,
        0u,
        0
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.players[0].selection.component_item_id == 2001u);
    CHECK(coordinator.players[1].selection.component_item_id == 2002u);
    CHECK(SudekiMpBlacksmithShadowGetPlayer(
        &coordinator,
        3u,
        &copy
    ));
    CHECK(copy.character_id == 103u && copy.actor_generation == 1003u);
}

static void test_quote_and_confirmation_are_revision_bound(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithPlayerShadow *shadow;
    uint32_t stale_revision;

    initialize_players(&coordinator, 1u);
    shadow = &coordinator.players[0];
    CHECK(SudekiMpBlacksmithShadowSetQuote(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        100u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID);
    prepare_confirmed_build(&coordinator, 0u, 1001u, 2001u, 1u, 400u);
    CHECK(shadow->quote_valid && shadow->confirmed &&
        shadow->quoted_cost == 400u);

    stale_revision = shadow->revision;
    CHECK(SudekiMpBlacksmithShadowSetNavigation(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        shadow->selection.page,
        shadow->selection.category,
        shadow->selection.cursor + 1u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(!shadow->quote_valid && !shadow->confirmed &&
        shadow->quoted_cost == 0u);
    CHECK(SudekiMpBlacksmithShadowSetConfirmation(
        &coordinator,
        0u,
        shadow->session_serial,
        stale_revision,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE);
    CHECK(SudekiMpBlacksmithShadowSetConfirmation(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNQUOTED);
}

static void test_zero_native_ids_are_not_unset_sentinels(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithPlayerShadow *shadow;
    SudekiMpBlacksmithCommitTicket ticket;

    initialize_players(&coordinator, 1u);
    shadow = &coordinator.players[0];
    CHECK(!shadow->build_valid);
    CHECK(SudekiMpBlacksmithShadowSetBuild(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        0u,
        0u,
        0u,
        0
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(shadow->build_valid);
    CHECK(SudekiMpBlacksmithShadowSetQuote(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        25u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetConfirmation(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        0u,
        shadow->session_serial,
        shadow->revision,
        &ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    CHECK(ticket.selection.equipment_item_id == 0u);
    CHECK(ticket.selection.component_item_id == 0u);
}

static void test_first_commit_wins_then_everyone_refreshes(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithCommitTicket first_ticket;
    SudekiMpBlacksmithCommitTicket second_ticket;
    SudekiMpBlacksmithSharedSnapshot after_first = {
        10u,
        20u,
        26u,
        31u
    };
    SudekiMpBlacksmithSharedSnapshot incomplete_verification = {
        10u,
        21u,
        25u,
        31u
    };
    SudekiMpBlacksmithPlayerShadow *player_one;
    SudekiMpBlacksmithPlayerShadow *player_two;

    initialize_players(&coordinator, 2u);
    player_one = &coordinator.players[0];
    player_two = &coordinator.players[1];
    prepare_confirmed_build(&coordinator, 0u, 1001u, 2001u, 0u, 400u);
    prepare_confirmed_build(&coordinator, 1u, 1002u, 2002u, 1u, 600u);

    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        0u,
        player_one->session_serial,
        player_one->revision,
        &first_ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    CHECK(first_ticket.character_id == 100u);
    CHECK(first_ticket.actor_generation == 1000u);
    CHECK(first_ticket.merchant_id == 900u);
    CHECK(first_ticket.selection.equipment_item_id == 1001u);
    CHECK(first_ticket.selection.component_item_id == 2001u);
    CHECK(first_ticket.quoted_cost == 400u);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        &second_ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY);

    CHECK(SudekiMpBlacksmithShadowResolveCommitTicket(
        &coordinator,
        first_ticket.serial,
        SUDEKIMP_BLACKSMITH_COMMIT_VERIFIED,
        &incomplete_verification
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID);
    CHECK(coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED);
    CHECK(SudekiMpBlacksmithShadowResolveCommitTicket(
        &coordinator,
        first_ticket.serial,
        SUDEKIMP_BLACKSMITH_COMMIT_VERIFIED,
        &after_first
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE);
    CHECK(player_one->state == SUDEKIMP_BLACKSMITH_SHADOW_BROWSING);
    CHECK(player_one->needs_refresh && player_two->needs_refresh);
    CHECK(!player_one->quote_valid && !player_one->confirmed);
    CHECK(!player_two->quote_valid && !player_two->confirmed);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        &second_ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_REFRESH_REQUIRED);

    CHECK(SudekiMpBlacksmithShadowRefresh(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        player_two->merchant_id,
        player_two->merchant_generation
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(!player_two->needs_refresh && !player_two->quote_valid &&
        !player_two->confirmed);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        &second_ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNQUOTED);
    CHECK(SudekiMpBlacksmithShadowSetQuote(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        650u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        &second_ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNCONFIRMED);
    CHECK(SudekiMpBlacksmithShadowSetConfirmation(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        1u,
        player_two->session_serial,
        player_two->revision,
        &second_ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    CHECK(second_ticket.serial != first_ticket.serial);
    CHECK(second_ticket.snapshot.catalog_generation == 20u);
    CHECK(second_ticket.snapshot.inventory_generation == 26u);
    CHECK(second_ticket.snapshot.economy_generation == 31u);
    CHECK(SudekiMpBlacksmithShadowResolveCommitTicket(
        &coordinator,
        second_ticket.serial,
        SUDEKIMP_BLACKSMITH_COMMIT_NOT_APPLIED,
        &after_first
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(player_two->needs_refresh && !player_two->confirmed);
}

static void test_dropout_and_world_change_invalidate_sessions(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithSharedSnapshot next_world = {
        11u,
        1u,
        1u,
        1u
    };
    uint32_t old_session;

    initialize_players(&coordinator, 2u);
    old_session = coordinator.players[1].session_serial;
    CHECK(SudekiMpBlacksmithShadowPublishPlayer(
        &coordinator,
        1u,
        0u,
        0u,
        0
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.players[1].state ==
        SUDEKIMP_BLACKSMITH_SHADOW_CLOSED);
    CHECK(SudekiMpBlacksmithShadowClose(
        &coordinator,
        1u,
        old_session,
        1u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_CLOSED);

    CHECK(SudekiMpBlacksmithShadowPublishSharedSnapshot(
        &coordinator,
        &next_world
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.players[0].state ==
        SUDEKIMP_BLACKSMITH_SHADOW_CLOSED);
    CHECK(!coordinator.actors[0].human_present);
    CHECK(SudekiMpBlacksmithShadowOpen(
        &coordinator,
        0u,
        900u,
        91u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID);
    CHECK(SudekiMpBlacksmithShadowPublishPlayer(
        &coordinator,
        0u,
        100u,
        1001u,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowOpen(
        &coordinator,
        0u,
        900u,
        91u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.players[0].session_serial != old_session);
}

static void test_uncertain_commit_quarantines_until_reset(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithCommitTicket ticket;
    SudekiMpBlacksmithCommitTicket retained;
    uint32_t old_ticket_serial;

    initialize_players(&coordinator, 1u);
    prepare_confirmed_build(&coordinator, 0u, 1001u, 2001u, 0u, 400u);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        0u,
        coordinator.players[0].session_serial,
        coordinator.players[0].revision,
        &ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    old_ticket_serial = ticket.serial;
    CHECK(SudekiMpBlacksmithShadowResolveCommitTicket(
        &coordinator,
        ticket.serial,
        SUDEKIMP_BLACKSMITH_COMMIT_AMBIGUOUS,
        NULL
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED);
    CHECK(coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED);
    CHECK(coordinator.players[0].state ==
        SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED);
    CHECK(SudekiMpBlacksmithShadowGetCommitTicket(
        &coordinator,
        &retained
    ));
    CHECK(retained.serial == old_ticket_serial);
    CHECK(SudekiMpBlacksmithShadowPublishSharedSnapshot(
        &coordinator,
        &initial_snapshot
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED);

    SudekiMpBlacksmithShadowReset(&coordinator);
    CHECK(coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE);
    CHECK(!SudekiMpBlacksmithShadowGetCommitTicket(
        &coordinator,
        &retained
    ));
    CHECK(SudekiMpBlacksmithShadowPublishSharedSnapshot(
        &coordinator,
        &initial_snapshot
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(coordinator.next_commit_serial == old_ticket_serial);
}

static void test_owner_dropout_or_external_mutation_quarantines(void) {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithCommitTicket ticket;
    SudekiMpBlacksmithSharedSnapshot changed_snapshot = {
        10u,
        20u,
        25u,
        31u
    };

    initialize_players(&coordinator, 1u);
    prepare_confirmed_build(&coordinator, 0u, 1001u, 2001u, 0u, 400u);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        0u,
        coordinator.players[0].session_serial,
        coordinator.players[0].revision,
        &ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    CHECK(SudekiMpBlacksmithShadowPublishPlayer(
        &coordinator,
        0u,
        0u,
        0u,
        0
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED);
    CHECK(coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED);

    SudekiMpBlacksmithShadowReset(&coordinator);
    initialize_players(&coordinator, 1u);
    prepare_confirmed_build(&coordinator, 0u, 1001u, 2001u, 0u, 400u);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &coordinator,
        0u,
        coordinator.players[0].session_serial,
        coordinator.players[0].revision,
        &ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    CHECK(SudekiMpBlacksmithShadowPublishSharedSnapshot(
        &coordinator,
        &changed_snapshot
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED);
    CHECK(coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED);
}

int main(void) {
    test_four_independent_player_shadows();
    test_quote_and_confirmation_are_revision_bound();
    test_zero_native_ids_are_not_unset_sentinels();
    test_first_commit_wins_then_everyone_refreshes();
    test_dropout_and_world_change_invalidate_sessions();
    test_uncertain_commit_quarantines_until_reset();
    test_owner_dropout_or_external_mutation_quarantines();

    if (failures != 0) {
        fprintf(stderr, "%d blacksmith-shadow checks failed\n", failures);
        return 1;
    }
    puts("blacksmith-shadow checks passed");
    return 0;
}
