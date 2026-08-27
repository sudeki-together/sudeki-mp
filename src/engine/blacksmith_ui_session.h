#ifndef SUDEKIMP_BLACKSMITH_UI_SESSION_H
#define SUDEKIMP_BLACKSMITH_UI_SESSION_H

#include "engine/blacksmith_read_model.h"
#include "engine/blacksmith_shadow.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT = 2u,
    SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT = 6u,
    SUDEKIMP_BLACKSMITH_UI_PAGE_COUNT = 3u,
    SUDEKIMP_BLACKSMITH_UI_CATEGORY_COUNT = 11u,
    SUDEKIMP_BLACKSMITH_UI_OVERLAY_DEADLINE_MS = 1000u
};

typedef enum SudekiMpBlacksmithUiInput {
    SUDEKIMP_BLACKSMITH_UI_INPUT_NONE = 0,
    SUDEKIMP_BLACKSMITH_UI_INPUT_UP,
    SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN,
    SUDEKIMP_BLACKSMITH_UI_INPUT_LEFT,
    SUDEKIMP_BLACKSMITH_UI_INPUT_RIGHT,
    SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIOUS_PAGE,
    SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE,
    SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIEW,
    SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE
} SudekiMpBlacksmithUiInput;

typedef enum SudekiMpBlacksmithUiNotice {
    SUDEKIMP_BLACKSMITH_UI_NOTICE_NAVIGATE = 0,
    SUDEKIMP_BLACKSMITH_UI_NOTICE_COMMIT_DISABLED,
    SUDEKIMP_BLACKSMITH_UI_NOTICE_DROPPED_OUT
} SudekiMpBlacksmithUiNotice;

typedef struct SudekiMpBlacksmithUiSeatSnapshot {
    int open;
    uint32_t character_id;
    uint32_t actor_generation;
    uint32_t session_serial;
    uint32_t revision;
    uint32_t page;
    uint32_t category;
    uint32_t cursor;
    uint32_t selected_equipment_index;
    uint32_t selected_socket_index;
    uint32_t selected_component_index;
    SudekiMpBlacksmithUiNotice notice;
} SudekiMpBlacksmithUiSeatSnapshot;

typedef struct SudekiMpBlacksmithUiSnapshot {
    int active;
    uint32_t presentation_serial;
    uint32_t shared_money;
    uint32_t catalog_generation;
    uint32_t inventory_generation;
    uint32_t economy_generation;
    int shared_money_valid;
    int overlay_acknowledged;
    int merchant_target_resolved;
    int native_commit_enabled;
    SudekiMpBlacksmithReadSnapshot read_model;
    SudekiMpBlacksmithUiSeatSnapshot
        seats[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
} SudekiMpBlacksmithUiSnapshot;

typedef struct SudekiMpBlacksmithUiSession {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    uint32_t presentation_serial;
    uint32_t presentation_merchant_id;
    uint32_t shared_money;
    uint32_t overlay_deadline_ms;
    uint32_t selected_equipment_indices[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t selected_socket_indices[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t selected_component_indices[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    int active;
    int shared_money_valid;
    int overlay_acknowledged;
    SudekiMpBlacksmithReadSnapshot read_model;
    SudekiMpBlacksmithUiNotice
        notices[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
} SudekiMpBlacksmithUiSession;

/* Exact preferred-base disk-image gate for the two exported script
 * boundaries. Loaded modules must use the relocation-aware variant. */
int SudekiMpBlacksmithUiStartSignaturesMatch(
    const uint8_t *image,
    size_t image_size
);
int SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
    const uint8_t *image,
    size_t image_size,
    uintptr_t loaded_image_base
);

void SudekiMpBlacksmithUiSessionInitialize(
    SudekiMpBlacksmithUiSession *session
);
int SudekiMpBlacksmithUiSessionBegin(
    SudekiMpBlacksmithUiSession *session,
    const uint32_t character_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    const uint32_t actor_generations[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    uint32_t shared_money,
    const SudekiMpBlacksmithReadSnapshot *read_model,
    uint32_t now_ms
);
int SudekiMpBlacksmithUiSessionApplyInput(
    SudekiMpBlacksmithUiSession *session,
    uint32_t player_index,
    SudekiMpBlacksmithUiInput input
);
int SudekiMpBlacksmithUiSessionDropPlayer(
    SudekiMpBlacksmithUiSession *session,
    uint32_t player_index
);
int SudekiMpBlacksmithUiSessionObserveMoney(
    SudekiMpBlacksmithUiSession *session,
    uint32_t shared_money
);
int SudekiMpBlacksmithUiSessionObserveReadModel(
    SudekiMpBlacksmithUiSession *session,
    const SudekiMpBlacksmithReadSnapshot *read_model
);
void SudekiMpBlacksmithUiSessionReportOverlay(
    SudekiMpBlacksmithUiSession *session,
    int visible
);
int SudekiMpBlacksmithUiSessionService(
    SudekiMpBlacksmithUiSession *session,
    uint32_t now_ms
);
int SudekiMpBlacksmithUiSessionGetSnapshot(
    const SudekiMpBlacksmithUiSession *session,
    SudekiMpBlacksmithUiSnapshot *snapshot
);

#endif
