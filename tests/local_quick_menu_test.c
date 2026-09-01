#include "engine/local_quick_menu.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expression, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static SudekiMpLocalQuickMenuLease lease_for(unsigned int seat,
                                              unsigned int revision) {
    SudekiMpLocalQuickMenuLease lease;
    memset(&lease, 0, sizeof(lease));
    lease.actor = (const void *)(uintptr_t)(0x1000u + seat * 0x100u);
    lease.input_identity = (const void *)(uintptr_t)(0x2000u + seat * 0x100u);
    lease.actor_generation = seat + 1u;
    lease.input_generation = seat + 10u;
    lease.view_revision = revision;
    return lease;
}

int main(void) {
    SudekiMpLocalQuickMenuState state;
    SudekiMpLocalQuickMenuLease p1 = lease_for(0u, 1u);
    SudekiMpLocalQuickMenuLease p2 = lease_for(1u, 1u);
    SudekiMpLocalQuickMenuLease p3 = lease_for(2u, 1u);
    const SudekiMpLocalQuickMenuSession *session;
    SudekiMpLocalQuickMenuRow rows[2];
    const void *targets[3] = {
        (const void *)(uintptr_t)0x1000u,
        (const void *)(uintptr_t)0x1100u,
        (const void *)(uintptr_t)0x1200u
    };

    SudekiMpLocalQuickMenuInitialize(&state);
    CHECK(!SudekiMpLocalQuickMenuActionCapable(&state));
    CHECK(SudekiMpLocalQuickMenuOpen(&state, 0u, &p1) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_NOT_READY);
    SudekiMpLocalQuickMenuSetActionCapableCategories(&state,
        SUDEKIMP_LOCAL_QUICK_MENU_ALL_CATEGORIES);
    CHECK(SudekiMpLocalQuickMenuOpen(&state, 0u, &p1) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_OPENED);
    CHECK(SudekiMpLocalQuickMenuOpen(&state, 1u, &p2) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_OPENED);
    CHECK(SudekiMpLocalQuickMenuOpen(&state, 2u, &p3) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_OPENED);
    memset(rows, 0, sizeof(rows));
    rows[0].native_identifier = 12u;
    rows[0].available = 1u;
    memcpy(rows[0].label, "Ailish row", sizeof("Ailish row"));
    rows[1].native_identifier = 13u;
    rows[1].cost = 40u;
    rows[1].available = 1u;
    memcpy(rows[1].label, "Ailish second", sizeof("Ailish second"));
    CHECK(SudekiMpLocalQuickMenuSetCategorySnapshot(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_SKILLS, rows, 2u, 91u));
    CHECK(SudekiMpLocalQuickMenuSessionForSeat(&state, 0u)
        ->snapshot_by_category[SUDEKIMP_LOCAL_QUICK_MENU_SKILLS].row_count == 0u);
    CHECK(SudekiMpLocalQuickMenuSessionForSeat(&state, 1u)
        ->snapshot_by_category[SUDEKIMP_LOCAL_QUICK_MENU_SKILLS].revision == 91u);
    CHECK(SudekiMpLocalQuickMenuAnyActive(&state));
    CHECK(SudekiMpLocalQuickMenuBeginTargetSelection(
        &state, 0u, targets, 3u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_TARGET_SELECTING);
    CHECK(SudekiMpLocalQuickMenuTargetSelectionActive(&state, 0u));
    CHECK(SudekiMpLocalQuickMenuSelectedTarget(&state, 0u) == targets[0]);
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 0u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_UP, 0u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED);
    CHECK(SudekiMpLocalQuickMenuSelectedTarget(&state, 0u) == targets[2]);
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 0u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CANCEL, 0u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED);
    CHECK(!SudekiMpLocalQuickMenuTargetSelectionActive(&state, 0u));
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_NEXT_CATEGORY, 0u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CATEGORY_CHANGED);
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_DOWN, 3u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED);
    session = SudekiMpLocalQuickMenuSessionForSeat(&state, 1u);
    CHECK(session != NULL && session->category == SUDEKIMP_LOCAL_QUICK_MENU_WEAPONS);
    CHECK(session->cursor_by_category[SUDEKIMP_LOCAL_QUICK_MENU_WEAPONS] == 1u);
    CHECK(SudekiMpLocalQuickMenuSessionForSeat(&state, 0u)->category ==
        SUDEKIMP_LOCAL_QUICK_MENU_SKILLS);
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CONFIRM, 3u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_EXECUTE_REQUESTED);
    CHECK(SudekiMpLocalQuickMenuSelectedRow(&state, 1u) == NULL);
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_PREVIOUS_CATEGORY, 0u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CATEGORY_CHANGED);
    CHECK(SudekiMpLocalQuickMenuHandleAction(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_ACTION_DOWN, 2u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED);
    CHECK(SudekiMpLocalQuickMenuSelectedRow(&state, 1u) != NULL);
    CHECK(SudekiMpLocalQuickMenuSelectedRow(&state, 1u)->native_identifier ==
        rows[1].native_identifier);
    CHECK(SudekiMpLocalQuickMenuRecordActionResult(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_BUSY) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_BUSY);
    CHECK(SudekiMpLocalQuickMenuSeatActive(&state, 1u));
    CHECK(SudekiMpLocalQuickMenuRecordActionResult(&state, 1u,
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_STARTED) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_STARTED);
    CHECK(!SudekiMpLocalQuickMenuSeatActive(&state, 1u));
    CHECK(SudekiMpLocalQuickMenuOpen(&state, 1u, &p2) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_OPENED);
    CHECK(SudekiMpLocalQuickMenuClose(&state, 1u) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CLOSED);
    CHECK(!SudekiMpLocalQuickMenuSeatActive(&state, 1u));
    p3.input_generation++;
    CHECK(SudekiMpLocalQuickMenuInvalidateIfLeaseChanged(&state, 2u, &p3) ==
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_LEASE);
    CHECK(SudekiMpLocalQuickMenuSeatActive(&state, 0u));
    SudekiMpLocalQuickMenuSetActionCapableCategories(&state,
        SUDEKIMP_LOCAL_QUICK_MENU_ALL_CATEGORIES - 1u);
    CHECK(!SudekiMpLocalQuickMenuAnyActive(&state));
    puts("local quick menu session checks passed");
    return 0;
}
