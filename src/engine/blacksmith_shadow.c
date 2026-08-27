#include "engine/blacksmith_shadow.h"

#include <string.h>

static int valid_snapshot(
    const SudekiMpBlacksmithSharedSnapshot *snapshot
) {
    return snapshot != NULL && snapshot->world_generation != 0u &&
        snapshot->catalog_generation != 0u &&
        snapshot->inventory_generation != 0u &&
        snapshot->economy_generation != 0u;
}

static int same_snapshot(
    const SudekiMpBlacksmithSharedSnapshot *left,
    const SudekiMpBlacksmithSharedSnapshot *right
) {
    return left->world_generation == right->world_generation &&
        left->catalog_generation == right->catalog_generation &&
        left->inventory_generation == right->inventory_generation &&
        left->economy_generation == right->economy_generation;
}

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) {
        ++value;
    }
    return value;
}

static void advance_revision(SudekiMpBlacksmithPlayerShadow *shadow) {
    shadow->revision = advance_nonzero(shadow->revision);
}

static void initialize_player_slot(
    SudekiMpBlacksmithPlayerShadow *shadow,
    uint32_t player_index
) {
    memset(shadow, 0, sizeof(*shadow));
    shadow->player_index = player_index;
}

static void close_player(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index
) {
    initialize_player_slot(&coordinator->players[player_index], player_index);
}

static void invalidate_quote(
    SudekiMpBlacksmithPlayerShadow *shadow,
    int refresh_required
) {
    shadow->quoted_cost = 0u;
    shadow->quote_valid = 0;
    shadow->confirmed = 0;
    if (refresh_required) {
        shadow->needs_refresh = 1;
    }
}

static void require_refresh(SudekiMpBlacksmithPlayerShadow *shadow) {
    if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
        return;
    }
    invalidate_quote(shadow, 1);
    advance_revision(shadow);
}

static void require_refresh_for_all(
    SudekiMpBlacksmithShadowCoordinator *coordinator
) {
    uint32_t player_index;

    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS;
         ++player_index) {
        require_refresh(&coordinator->players[player_index]);
    }
}

static void quarantine_commit_lane(
    SudekiMpBlacksmithShadowCoordinator *coordinator
) {
    SudekiMpBlacksmithPlayerShadow *owner;

    coordinator->commit_lane_state =
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED;
    if (coordinator->commit_ticket.player_index >=
        SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS) {
        return;
    }
    owner = &coordinator->players[
        coordinator->commit_ticket.player_index];
    owner->state = SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED;
    invalidate_quote(owner, 1);
    advance_revision(owner);
}

static SudekiMpBlacksmithShadowResult validate_browsing_shadow(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision
) {
    const SudekiMpBlacksmithActorLease *actor;
    const SudekiMpBlacksmithPlayerShadow *shadow;

    if (coordinator == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS ||
        session_serial == 0u || expected_revision == 0u) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    shadow = &coordinator->players[player_index];
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_CLOSED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_CLOSED;
    }
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED ||
        coordinator->commit_lane_state ==
            SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY;
    }
    if (shadow->session_serial != session_serial ||
        shadow->revision != expected_revision) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE;
    }
    actor = &coordinator->actors[player_index];
    if (!actor->human_present || actor->character_id == 0u ||
        actor->actor_generation == 0u ||
        shadow->character_id != actor->character_id ||
        shadow->actor_generation != actor->actor_generation) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE;
    }
    if (!coordinator->shared_snapshot_valid || shadow->needs_refresh ||
        !same_snapshot(
            &shadow->snapshot,
            &coordinator->shared_snapshot)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_REFRESH_REQUIRED;
    }
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

void SudekiMpBlacksmithShadowInitialize(
    SudekiMpBlacksmithShadowCoordinator *coordinator
) {
    uint32_t player_index;

    if (coordinator == NULL) {
        return;
    }
    memset(coordinator, 0, sizeof(*coordinator));
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS;
         ++player_index) {
        coordinator->players[player_index].player_index = player_index;
    }
}

void SudekiMpBlacksmithShadowReset(
    SudekiMpBlacksmithShadowCoordinator *coordinator
) {
    uint32_t next_session_serial;
    uint32_t next_commit_serial;

    if (coordinator == NULL) {
        return;
    }
    next_session_serial = coordinator->next_session_serial;
    next_commit_serial = coordinator->next_commit_serial;
    SudekiMpBlacksmithShadowInitialize(coordinator);
    coordinator->next_session_serial = next_session_serial;
    coordinator->next_commit_serial = next_commit_serial;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowPublishPlayer(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t character_id,
    uint32_t actor_generation,
    int human_present
) {
    SudekiMpBlacksmithActorLease *actor;
    SudekiMpBlacksmithPlayerShadow *shadow;
    int present;
    int identity_changed;

    if (coordinator == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    present = human_present != 0;
    if (present && (character_id == 0u || actor_generation == 0u)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    if (!present) {
        character_id = 0u;
        actor_generation = 0u;
    }
    actor = &coordinator->actors[player_index];
    if (actor->human_present == present &&
        actor->character_id == character_id &&
        actor->actor_generation == actor_generation) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }

    shadow = &coordinator->players[player_index];
    identity_changed = !present ||
        shadow->character_id != character_id ||
        shadow->actor_generation != actor_generation;
    actor->human_present = present;
    actor->character_id = character_id;
    actor->actor_generation = actor_generation;

    if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_CLOSED &&
        identity_changed) {
        if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED ||
            coordinator->commit_lane_state ==
                SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
            return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
        }
        if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_COMMITTING ||
            (coordinator->commit_lane_state ==
                SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED &&
             coordinator->commit_ticket.player_index == player_index)) {
            quarantine_commit_lane(coordinator);
            return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
        }
        close_player(coordinator, player_index);
    }
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowPublishSharedSnapshot(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    const SudekiMpBlacksmithSharedSnapshot *snapshot
) {
    uint32_t player_index;
    int world_changed;

    if (coordinator == NULL || !valid_snapshot(snapshot)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    if (coordinator->commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (!coordinator->shared_snapshot_valid) {
        coordinator->shared_snapshot = *snapshot;
        coordinator->shared_snapshot_valid = 1;
        return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
    }
    if (same_snapshot(&coordinator->shared_snapshot, snapshot)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }

    world_changed = coordinator->shared_snapshot.world_generation !=
        snapshot->world_generation;
    coordinator->shared_snapshot = *snapshot;
    if (coordinator->commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED) {
        if (world_changed) {
            for (player_index = 0u;
                 player_index < SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS;
                 ++player_index) {
                if (player_index !=
                    coordinator->commit_ticket.player_index) {
                    close_player(coordinator, player_index);
                }
                memset(
                    &coordinator->actors[player_index],
                    0,
                    sizeof(coordinator->actors[player_index])
                );
            }
        } else {
            require_refresh_for_all(coordinator);
        }
        quarantine_commit_lane(coordinator);
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (world_changed) {
        for (player_index = 0u;
             player_index < SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS;
             ++player_index) {
            close_player(coordinator, player_index);
            memset(
                &coordinator->actors[player_index],
                0,
                sizeof(coordinator->actors[player_index])
            );
        }
    } else {
        require_refresh_for_all(coordinator);
    }
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowOpen(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t merchant_id,
    uint32_t merchant_generation
) {
    const SudekiMpBlacksmithActorLease *actor;
    SudekiMpBlacksmithPlayerShadow *shadow;

    if (coordinator == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS ||
        merchant_id == 0u || merchant_generation == 0u ||
        !coordinator->shared_snapshot_valid) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    if (coordinator->commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    actor = &coordinator->actors[player_index];
    if (!actor->human_present || actor->character_id == 0u ||
        actor->actor_generation == 0u) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    shadow = &coordinator->players[player_index];
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_CLOSED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY;
    }

    initialize_player_slot(shadow, player_index);
    shadow->state = SUDEKIMP_BLACKSMITH_SHADOW_BROWSING;
    coordinator->next_session_serial = advance_nonzero(
        coordinator->next_session_serial);
    shadow->session_serial = coordinator->next_session_serial;
    shadow->revision = 1u;
    shadow->character_id = actor->character_id;
    shadow->actor_generation = actor->actor_generation;
    shadow->merchant_id = merchant_id;
    shadow->merchant_generation = merchant_generation;
    shadow->snapshot = coordinator->shared_snapshot;
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowClose(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision
) {
    SudekiMpBlacksmithPlayerShadow *shadow;

    if (coordinator == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS ||
        session_serial == 0u || expected_revision == 0u) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    shadow = &coordinator->players[player_index];
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_CLOSED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_CLOSED;
    }
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED ||
        coordinator->commit_lane_state ==
            SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_COMMITTING) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY;
    }
    if (shadow->session_serial != session_serial ||
        shadow->revision != expected_revision) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE;
    }
    close_player(coordinator, player_index);
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowRefresh(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t merchant_id,
    uint32_t merchant_generation
) {
    const SudekiMpBlacksmithActorLease *actor;
    SudekiMpBlacksmithPlayerShadow *shadow;

    if (coordinator == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS ||
        session_serial == 0u || expected_revision == 0u ||
        merchant_id == 0u || merchant_generation == 0u) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    shadow = &coordinator->players[player_index];
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_CLOSED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_CLOSED;
    }
    if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED ||
        coordinator->commit_lane_state ==
            SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY;
    }
    if (shadow->session_serial != session_serial ||
        shadow->revision != expected_revision ||
        shadow->merchant_id != merchant_id ||
        shadow->merchant_generation != merchant_generation) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE;
    }
    actor = &coordinator->actors[player_index];
    if (!actor->human_present || shadow->character_id !=
            actor->character_id || shadow->actor_generation !=
            actor->actor_generation ||
        !coordinator->shared_snapshot_valid ||
        shadow->snapshot.world_generation !=
            coordinator->shared_snapshot.world_generation) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE;
    }
    if (!shadow->needs_refresh && same_snapshot(
            &shadow->snapshot,
            &coordinator->shared_snapshot)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }
    shadow->snapshot = coordinator->shared_snapshot;
    shadow->needs_refresh = 0;
    invalidate_quote(shadow, 0);
    advance_revision(shadow);
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetNavigation(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t page,
    uint32_t category,
    uint32_t cursor
) {
    SudekiMpBlacksmithPlayerShadow *shadow;
    SudekiMpBlacksmithShadowResult validation;

    validation = validate_browsing_shadow(
        coordinator,
        player_index,
        session_serial,
        expected_revision
    );
    if (validation != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return validation;
    }
    shadow = &coordinator->players[player_index];
    if (shadow->selection.page == page &&
        shadow->selection.category == category &&
        shadow->selection.cursor == cursor) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }
    shadow->selection.page = page;
    shadow->selection.category = category;
    shadow->selection.cursor = cursor;
    invalidate_quote(shadow, 0);
    advance_revision(shadow);
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetBuild(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t equipment_item_id,
    uint32_t component_item_id,
    uint32_t socket_index,
    int socket_bank
) {
    SudekiMpBlacksmithPlayerShadow *shadow;
    SudekiMpBlacksmithShadowResult validation;
    int bank;

    validation = validate_browsing_shadow(
        coordinator,
        player_index,
        session_serial,
        expected_revision
    );
    if (validation != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return validation;
    }
    shadow = &coordinator->players[player_index];
    bank = socket_bank != 0;
    if (shadow->build_valid &&
        shadow->selection.equipment_item_id == equipment_item_id &&
        shadow->selection.component_item_id == component_item_id &&
        shadow->selection.socket_index == socket_index &&
        shadow->selection.socket_bank == bank) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }
    shadow->selection.equipment_item_id = equipment_item_id;
    shadow->selection.component_item_id = component_item_id;
    shadow->selection.socket_index = socket_index;
    shadow->selection.socket_bank = bank;
    shadow->build_valid = 1;
    invalidate_quote(shadow, 0);
    advance_revision(shadow);
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetQuote(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t quoted_cost
) {
    SudekiMpBlacksmithPlayerShadow *shadow;
    SudekiMpBlacksmithShadowResult validation;

    validation = validate_browsing_shadow(
        coordinator,
        player_index,
        session_serial,
        expected_revision
    );
    if (validation != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return validation;
    }
    shadow = &coordinator->players[player_index];
    if (!shadow->build_valid) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    if (shadow->quote_valid && !shadow->confirmed &&
        shadow->quoted_cost == quoted_cost) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }
    shadow->quoted_cost = quoted_cost;
    shadow->quote_valid = 1;
    shadow->confirmed = 0;
    advance_revision(shadow);
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetConfirmation(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    int confirmed
) {
    SudekiMpBlacksmithPlayerShadow *shadow;
    SudekiMpBlacksmithShadowResult validation;
    int value;

    validation = validate_browsing_shadow(
        coordinator,
        player_index,
        session_serial,
        expected_revision
    );
    if (validation != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return validation;
    }
    shadow = &coordinator->players[player_index];
    value = confirmed != 0;
    if (value && !shadow->quote_valid) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNQUOTED;
    }
    if (shadow->confirmed == value) {
        return SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
    }
    shadow->confirmed = value;
    advance_revision(shadow);
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowClaimCommitTicket(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    SudekiMpBlacksmithCommitTicket *ticket
) {
    SudekiMpBlacksmithPlayerShadow *shadow;
    SudekiMpBlacksmithShadowResult validation;

    if (ticket == NULL) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    validation = validate_browsing_shadow(
        coordinator,
        player_index,
        session_serial,
        expected_revision
    );
    if (validation != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return validation;
    }
    if (coordinator->commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY;
    }
    if (coordinator->commit_lane_state !=
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    shadow = &coordinator->players[player_index];
    if (!shadow->quote_valid) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNQUOTED;
    }
    if (!shadow->confirmed) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNCONFIRMED;
    }
    if (!shadow->build_valid) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }

    shadow->state = SUDEKIMP_BLACKSMITH_SHADOW_COMMITTING;
    advance_revision(shadow);
    coordinator->next_commit_serial = advance_nonzero(
        coordinator->next_commit_serial);
    memset(&coordinator->commit_ticket, 0,
        sizeof(coordinator->commit_ticket));
    coordinator->commit_ticket.serial = coordinator->next_commit_serial;
    coordinator->commit_ticket.player_index = player_index;
    coordinator->commit_ticket.session_serial = shadow->session_serial;
    coordinator->commit_ticket.shadow_revision = shadow->revision;
    coordinator->commit_ticket.character_id = shadow->character_id;
    coordinator->commit_ticket.actor_generation = shadow->actor_generation;
    coordinator->commit_ticket.merchant_id = shadow->merchant_id;
    coordinator->commit_ticket.merchant_generation =
        shadow->merchant_generation;
    coordinator->commit_ticket.snapshot = shadow->snapshot;
    coordinator->commit_ticket.selection = shadow->selection;
    coordinator->commit_ticket.quoted_cost = shadow->quoted_cost;
    coordinator->commit_lane_state =
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED;
    *ticket = coordinator->commit_ticket;
    return SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED;
}

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowResolveCommitTicket(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t ticket_serial,
    SudekiMpBlacksmithCommitOutcome outcome,
    const SudekiMpBlacksmithSharedSnapshot *observed_snapshot
) {
    SudekiMpBlacksmithPlayerShadow *owner;
    int shared_changed;

    if (coordinator == NULL || ticket_serial == 0u ||
        outcome < SUDEKIMP_BLACKSMITH_COMMIT_NOT_APPLIED ||
        outcome > SUDEKIMP_BLACKSMITH_COMMIT_AMBIGUOUS) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    if (coordinator->commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (coordinator->commit_lane_state !=
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_CLOSED;
    }
    if (coordinator->commit_ticket.serial != ticket_serial) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE;
    }
    if (coordinator->commit_ticket.player_index >=
        SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS) {
        quarantine_commit_lane(coordinator);
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    owner = &coordinator->players[
        coordinator->commit_ticket.player_index];
    if (owner->state != SUDEKIMP_BLACKSMITH_SHADOW_COMMITTING ||
        owner->session_serial !=
            coordinator->commit_ticket.session_serial ||
        owner->revision != coordinator->commit_ticket.shadow_revision) {
        quarantine_commit_lane(coordinator);
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (outcome == SUDEKIMP_BLACKSMITH_COMMIT_AMBIGUOUS) {
        quarantine_commit_lane(coordinator);
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    if (!valid_snapshot(observed_snapshot)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }
    if (observed_snapshot->world_generation !=
        coordinator->commit_ticket.snapshot.world_generation ||
        !same_snapshot(
            &coordinator->shared_snapshot,
            &coordinator->commit_ticket.snapshot)) {
        quarantine_commit_lane(coordinator);
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED;
    }
    shared_changed = !same_snapshot(
        &coordinator->shared_snapshot,
        observed_snapshot);
    if (outcome == SUDEKIMP_BLACKSMITH_COMMIT_VERIFIED &&
        (observed_snapshot->inventory_generation ==
            coordinator->commit_ticket.snapshot.inventory_generation ||
         observed_snapshot->economy_generation ==
            coordinator->commit_ticket.snapshot.economy_generation)) {
        return SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID;
    }

    coordinator->shared_snapshot = *observed_snapshot;
    owner->state = SUDEKIMP_BLACKSMITH_SHADOW_BROWSING;
    if (shared_changed ||
        outcome == SUDEKIMP_BLACKSMITH_COMMIT_VERIFIED) {
        require_refresh_for_all(coordinator);
    } else {
        require_refresh(owner);
    }
    coordinator->commit_lane_state =
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE;
    memset(&coordinator->commit_ticket, 0,
        sizeof(coordinator->commit_ticket));
    return SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
}

int SudekiMpBlacksmithShadowGetPlayer(
    const SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    SudekiMpBlacksmithPlayerShadow *shadow
) {
    if (coordinator == NULL || shadow == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS) {
        return 0;
    }
    *shadow = coordinator->players[player_index];
    return 1;
}

int SudekiMpBlacksmithShadowGetCommitTicket(
    const SudekiMpBlacksmithShadowCoordinator *coordinator,
    SudekiMpBlacksmithCommitTicket *ticket
) {
    if (coordinator == NULL || ticket == NULL ||
        coordinator->commit_lane_state ==
            SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE ||
        coordinator->commit_ticket.serial == 0u) {
        return 0;
    }
    *ticket = coordinator->commit_ticket;
    return 1;
}
