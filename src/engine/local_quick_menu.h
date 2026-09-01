#ifndef SUDEKIMP_LOCAL_QUICK_MENU_H
#define SUDEKIMP_LOCAL_QUICK_MENU_H

#include <stdint.h>

/* The custom panel is deliberately restricted to the current three-seat
 * renderer.  P4 is not a latent alias for a stale companion lease. */
enum {
    SUDEKIMP_LOCAL_QUICK_MENU_SEAT_COUNT = 3u,
    SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT = 4u,
    /* Retail's global weapon category contains the four hero families, so a
     * panel cannot assume one actor's short loadout.  The renderer displays a
     * scrolling seven-row window over this complete native snapshot. */
    SUDEKIMP_LOCAL_QUICK_MENU_MAX_ROWS = 256u,
    SUDEKIMP_LOCAL_QUICK_MENU_MAX_TARGETS = 4u,
    SUDEKIMP_LOCAL_QUICK_MENU_LABEL_CAPACITY = 40u,
    SUDEKIMP_LOCAL_QUICK_MENU_ALL_CATEGORIES = 0x0fu
};

typedef enum SudekiMpLocalQuickMenuCategory {
    SUDEKIMP_LOCAL_QUICK_MENU_SKILLS = 0,
    SUDEKIMP_LOCAL_QUICK_MENU_WEAPONS = 1,
    SUDEKIMP_LOCAL_QUICK_MENU_ITEMS = 2,
    SUDEKIMP_LOCAL_QUICK_MENU_SPIRIT = 3
} SudekiMpLocalQuickMenuCategory;

typedef enum SudekiMpLocalQuickMenuAction {
    SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CONFIRM = 0,
    SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CANCEL,
    SUDEKIMP_LOCAL_QUICK_MENU_ACTION_UP,
    SUDEKIMP_LOCAL_QUICK_MENU_ACTION_DOWN,
    SUDEKIMP_LOCAL_QUICK_MENU_ACTION_PREVIOUS_CATEGORY,
    SUDEKIMP_LOCAL_QUICK_MENU_ACTION_NEXT_CATEGORY
} SudekiMpLocalQuickMenuAction;

typedef enum SudekiMpLocalQuickMenuResult {
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_NONE = 0,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_OPENED,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CLOSED,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CATEGORY_CHANGED,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_TARGET_SELECTING,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_EXECUTE_REQUESTED,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_NOT_READY,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_LEASE,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION,
    /* A global retail transaction (for example Spirit) owns the action.
     * The requester remains open and can continue browsing; nothing queues. */
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_BUSY,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_STARTED,
    SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_REJECTED
} SudekiMpLocalQuickMenuResult;

/* Immutable generation-scoped proof copied at open time.  Native pointers are
 * opaque identities only: category adapters must reprove them before use. */
typedef struct SudekiMpLocalQuickMenuLease {
    const void *actor;
    const void *input_identity;
    uint32_t actor_generation;
    uint32_t input_generation;
    uint32_t view_revision;
} SudekiMpLocalQuickMenuLease;

/* Snapshot rows are pointer-free.  `native_identifier` is meaningful only to
 * the category adapter which emitted it, and is revalidated against the
 * immutable lease on every confirm. */
typedef struct SudekiMpLocalQuickMenuRow {
    uint32_t native_identifier;
    uint32_t cost;
    uint8_t available;
    uint8_t reserved[3];
    char label[SUDEKIMP_LOCAL_QUICK_MENU_LABEL_CAPACITY];
} SudekiMpLocalQuickMenuRow;

typedef struct SudekiMpLocalQuickMenuCategorySnapshot {
    uint32_t revision;
    uint32_t row_count;
    SudekiMpLocalQuickMenuRow rows[SUDEKIMP_LOCAL_QUICK_MENU_MAX_ROWS];
} SudekiMpLocalQuickMenuCategorySnapshot;

typedef struct SudekiMpLocalQuickMenuSession {
    uint8_t open;
    uint8_t category;
    uint16_t reserved;
    uint32_t cursor_by_category[SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT];
    SudekiMpLocalQuickMenuCategorySnapshot
        snapshot_by_category[SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT];
    uint32_t presentation_revision;
    uint32_t serial;
    SudekiMpLocalQuickMenuResult last_result;
    SudekiMpLocalQuickMenuLease lease;
    const void *targets[SUDEKIMP_LOCAL_QUICK_MENU_MAX_TARGETS];
    uint8_t target_count;
    uint8_t target_cursor;
    uint16_t target_reserved;
} SudekiMpLocalQuickMenuSession;

typedef struct SudekiMpLocalQuickMenuState {
    uint32_t action_capable_category_mask;
    uint32_t next_serial;
    SudekiMpLocalQuickMenuSession sessions[SUDEKIMP_LOCAL_QUICK_MENU_SEAT_COUNT];
} SudekiMpLocalQuickMenuState;

void SudekiMpLocalQuickMenuInitialize(SudekiMpLocalQuickMenuState *state);
/* The renderer must set this only after every category adapter has passed its
 * exact-image preflight.  A partial backend set never exposes the panel. */
void SudekiMpLocalQuickMenuSetActionCapableCategories(
    SudekiMpLocalQuickMenuState *state,
    uint32_t category_mask
);
int SudekiMpLocalQuickMenuActionCapable(const SudekiMpLocalQuickMenuState *state);
int SudekiMpLocalQuickMenuLeaseExact(
    const SudekiMpLocalQuickMenuLease *captured,
    const SudekiMpLocalQuickMenuLease *current
);
SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuOpen(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    const SudekiMpLocalQuickMenuLease *lease
);
SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuClose(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
);
SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuInvalidateIfLeaseChanged(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    const SudekiMpLocalQuickMenuLease *current
);
SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuHandleAction(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    SudekiMpLocalQuickMenuAction action,
    uint32_t row_count
);
/* Item adapters own the target identities; the core only keeps an immutable
 * selection list.  The caller must still reprove the target at execution. */
SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuBeginTargetSelection(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    const void *const *targets,
    uint32_t target_count
);
int SudekiMpLocalQuickMenuTargetSelectionActive(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
);
const void *SudekiMpLocalQuickMenuSelectedTarget(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
);
int SudekiMpLocalQuickMenuSetCategorySnapshot(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    SudekiMpLocalQuickMenuCategory category,
    const SudekiMpLocalQuickMenuRow *rows,
    uint32_t row_count,
    uint32_t revision
);
/* Returns a pointer-free row from the captured category snapshot.  Callers
 * must still reprove the session lease before using native_identifier. */
const SudekiMpLocalQuickMenuRow *SudekiMpLocalQuickMenuSelectedRow(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
);
/* Native adapters report the terminal result of an EXECUTE_REQUESTED edge.
 * Busy/rejection keep the panel open; only a verified start may close it. */
SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuRecordActionResult(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    SudekiMpLocalQuickMenuResult result
);
const SudekiMpLocalQuickMenuSession *SudekiMpLocalQuickMenuSessionForSeat(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
);
int SudekiMpLocalQuickMenuSeatActive(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
);
int SudekiMpLocalQuickMenuAnyActive(const SudekiMpLocalQuickMenuState *state);

#endif
