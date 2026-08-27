#ifndef SUDEKIMP_BLACKSMITH_SHADOW_H
#define SUDEKIMP_BLACKSMITH_SHADOW_H

#include <stdint.h>

enum {
    SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS = 4u
};

typedef enum SudekiMpBlacksmithShadowState {
    SUDEKIMP_BLACKSMITH_SHADOW_CLOSED = 0,
    SUDEKIMP_BLACKSMITH_SHADOW_BROWSING,
    SUDEKIMP_BLACKSMITH_SHADOW_COMMITTING,
    SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED
} SudekiMpBlacksmithShadowState;

typedef enum SudekiMpBlacksmithCommitLaneState {
    SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE = 0,
    SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED,
    SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED
} SudekiMpBlacksmithCommitLaneState;

typedef enum SudekiMpBlacksmithShadowResult {
    SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE = 0,
    SUDEKIMP_BLACKSMITH_SHADOW_APPLIED,
    SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_INVALID,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_CLOSED,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_STALE,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_REFRESH_REQUIRED,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNQUOTED,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNCONFIRMED,
    SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_QUARANTINED
} SudekiMpBlacksmithShadowResult;

typedef enum SudekiMpBlacksmithCommitOutcome {
    /* The future native adapter proved that no mutation was attempted. */
    SUDEKIMP_BLACKSMITH_COMMIT_NOT_APPLIED = 0,
    /* Inventory/equipment and party money were both verified after commit. */
    SUDEKIMP_BLACKSMITH_COMMIT_VERIFIED,
    /* Mutation may have partially occurred. Only a full coordinator reset is
     * allowed to release the lane after this result. */
    SUDEKIMP_BLACKSMITH_COMMIT_AMBIGUOUS
} SudekiMpBlacksmithCommitOutcome;

typedef struct SudekiMpBlacksmithSharedSnapshot {
    uint32_t world_generation;
    uint32_t catalog_generation;
    /* Changes whenever shared item quantities, equipped definitions, socket
     * bytes, or another augmentation-bearing inventory field changes. */
    uint32_t inventory_generation;
    uint32_t economy_generation;
} SudekiMpBlacksmithSharedSnapshot;

typedef struct SudekiMpBlacksmithActorLease {
    /* Stable character identity, never a native CCharacter pointer or the
     * native Blacksmith layer's transient party ordinal. */
    uint32_t character_id;
    uint32_t actor_generation;
    int human_present;
} SudekiMpBlacksmithActorLease;

typedef struct SudekiMpBlacksmithBuildSelection {
    uint32_t page;
    uint32_t category;
    uint32_t cursor;
    uint32_t equipment_item_id;
    uint32_t component_item_id;
    uint32_t socket_index;
    int socket_bank;
} SudekiMpBlacksmithBuildSelection;

typedef struct SudekiMpBlacksmithPlayerShadow {
    SudekiMpBlacksmithShadowState state;
    uint32_t player_index;
    uint32_t session_serial;
    uint32_t revision;
    uint32_t character_id;
    uint32_t actor_generation;
    uint32_t merchant_id;
    uint32_t merchant_generation;
    SudekiMpBlacksmithSharedSnapshot snapshot;
    SudekiMpBlacksmithBuildSelection selection;
    uint32_t quoted_cost;
    /* Native equipment and component IDs may both legitimately be zero. */
    int build_valid;
    int needs_refresh;
    int quote_valid;
    int confirmed;
} SudekiMpBlacksmithPlayerShadow;

typedef struct SudekiMpBlacksmithCommitTicket {
    /* This is a native-inert immutable request. Claiming it does not call the
     * game, debit money, alter inventory, or copy the native UI singleton. */
    uint32_t serial;
    uint32_t player_index;
    uint32_t session_serial;
    uint32_t shadow_revision;
    uint32_t character_id;
    uint32_t actor_generation;
    uint32_t merchant_id;
    uint32_t merchant_generation;
    SudekiMpBlacksmithSharedSnapshot snapshot;
    SudekiMpBlacksmithBuildSelection selection;
    uint32_t quoted_cost;
} SudekiMpBlacksmithCommitTicket;

typedef struct SudekiMpBlacksmithShadowCoordinator {
    SudekiMpBlacksmithActorLease
        actors[SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS];
    SudekiMpBlacksmithPlayerShadow
        players[SUDEKIMP_BLACKSMITH_SHADOW_MAX_PLAYERS];
    SudekiMpBlacksmithSharedSnapshot shared_snapshot;
    int shared_snapshot_valid;
    SudekiMpBlacksmithCommitLaneState commit_lane_state;
    SudekiMpBlacksmithCommitTicket commit_ticket;
    uint32_t next_session_serial;
    uint32_t next_commit_serial;
} SudekiMpBlacksmithShadowCoordinator;

/* This coordinator is a game-thread-only, native-inert model. It deliberately
 * has no native callback or pointer field. A later adapter may consume one
 * commit ticket only after re-resolving every stable ID and generation. */
void SudekiMpBlacksmithShadowInitialize(
    SudekiMpBlacksmithShadowCoordinator *coordinator
);
void SudekiMpBlacksmithShadowReset(
    SudekiMpBlacksmithShadowCoordinator *coordinator
);

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowPublishPlayer(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t character_id,
    uint32_t actor_generation,
    int human_present
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowPublishSharedSnapshot(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    const SudekiMpBlacksmithSharedSnapshot *snapshot
);

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowOpen(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t merchant_id,
    uint32_t merchant_generation
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowClose(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowRefresh(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t merchant_id,
    uint32_t merchant_generation
);

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetNavigation(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t page,
    uint32_t category,
    uint32_t cursor
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetBuild(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t equipment_item_id,
    uint32_t component_item_id,
    uint32_t socket_index,
    int socket_bank
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetQuote(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    uint32_t quoted_cost
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowSetConfirmation(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    int confirmed
);

SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowClaimCommitTicket(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t session_serial,
    uint32_t expected_revision,
    SudekiMpBlacksmithCommitTicket *ticket
);
SudekiMpBlacksmithShadowResult SudekiMpBlacksmithShadowResolveCommitTicket(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t ticket_serial,
    SudekiMpBlacksmithCommitOutcome outcome,
    const SudekiMpBlacksmithSharedSnapshot *observed_snapshot
);

int SudekiMpBlacksmithShadowGetPlayer(
    const SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    SudekiMpBlacksmithPlayerShadow *shadow
);
int SudekiMpBlacksmithShadowGetCommitTicket(
    const SudekiMpBlacksmithShadowCoordinator *coordinator,
    SudekiMpBlacksmithCommitTicket *ticket
);

#endif
