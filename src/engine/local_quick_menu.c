#include "engine/local_quick_menu.h"

#include <string.h>

static int seat_valid(unsigned int seat_index) {
    return seat_index < SUDEKIMP_LOCAL_QUICK_MENU_SEAT_COUNT;
}

static int lease_valid(const SudekiMpLocalQuickMenuLease *lease) {
    return lease != NULL && lease->actor != NULL &&
        lease->input_identity != NULL && lease->actor_generation != 0u &&
        lease->input_generation != 0u && lease->view_revision != 0u;
}

static void close_session(SudekiMpLocalQuickMenuSession *session,
                          SudekiMpLocalQuickMenuResult result) {
    if (session == NULL) {
        return;
    }
    session->open = 0u;
    session->presentation_revision++;
    if (session->presentation_revision == 0u) {
        session->presentation_revision = 1u;
    }
    session->last_result = result;
    memset(&session->lease, 0, sizeof(session->lease));
}

static void clear_target_selection(SudekiMpLocalQuickMenuSession *session) {
    if (session != NULL) {
        memset(session->targets, 0, sizeof(session->targets));
        session->target_count = 0u;
        session->target_cursor = 0u;
    }
}

void SudekiMpLocalQuickMenuInitialize(SudekiMpLocalQuickMenuState *state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

void SudekiMpLocalQuickMenuSetActionCapableCategories(
    SudekiMpLocalQuickMenuState *state,
    uint32_t category_mask
) {
    unsigned int seat_index;

    if (state == NULL) {
        return;
    }
    state->action_capable_category_mask = category_mask &
        SUDEKIMP_LOCAL_QUICK_MENU_ALL_CATEGORIES;
    if (SudekiMpLocalQuickMenuActionCapable(state)) {
        return;
    }
    for (seat_index = 0u;
         seat_index < SUDEKIMP_LOCAL_QUICK_MENU_SEAT_COUNT;
         ++seat_index) {
        if (state->sessions[seat_index].open != 0u) {
            close_session(&state->sessions[seat_index],
                SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_NOT_READY);
        }
    }
}

int SudekiMpLocalQuickMenuActionCapable(const SudekiMpLocalQuickMenuState *state) {
    return state != NULL && state->action_capable_category_mask ==
        SUDEKIMP_LOCAL_QUICK_MENU_ALL_CATEGORIES;
}

int SudekiMpLocalQuickMenuLeaseExact(
    const SudekiMpLocalQuickMenuLease *captured,
    const SudekiMpLocalQuickMenuLease *current
) {
    return lease_valid(captured) && lease_valid(current) &&
        captured->actor == current->actor &&
        captured->input_identity == current->input_identity &&
        captured->actor_generation == current->actor_generation &&
        captured->input_generation == current->input_generation &&
        captured->view_revision == current->view_revision;
}

SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuOpen(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    const SudekiMpLocalQuickMenuLease *lease
) {
    SudekiMpLocalQuickMenuSession *session;

    if (state == NULL || !seat_valid(seat_index)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    session = &state->sessions[seat_index];
    if (!SudekiMpLocalQuickMenuActionCapable(state)) {
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_NOT_READY;
        return session->last_result;
    }
    if (!lease_valid(lease)) {
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_LEASE;
        return session->last_result;
    }
    if (session->open != 0u &&
        SudekiMpLocalQuickMenuLeaseExact(&session->lease, lease)) {
        return session->last_result;
    }
    memset(session, 0, sizeof(*session));
    session->open = 1u;
    session->lease = *lease;
    session->serial = ++state->next_serial;
    if (session->serial == 0u) {
        session->serial = ++state->next_serial;
    }
    session->presentation_revision = 1u;
    session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_OPENED;
    return session->last_result;
}

SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuClose(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
) {
    if (state == NULL || !seat_valid(seat_index)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    if (state->sessions[seat_index].open == 0u) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_NONE;
    }
    close_session(&state->sessions[seat_index],
        SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CLOSED);
    return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CLOSED;
}

SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuInvalidateIfLeaseChanged(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    const SudekiMpLocalQuickMenuLease *current
) {
    SudekiMpLocalQuickMenuSession *session;

    if (state == NULL || !seat_valid(seat_index)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    session = &state->sessions[seat_index];
    if (session->open == 0u ||
        SudekiMpLocalQuickMenuLeaseExact(&session->lease, current)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_NONE;
    }
    close_session(session, SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_LEASE);
    return session->last_result;
}

SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuHandleAction(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    SudekiMpLocalQuickMenuAction action,
    uint32_t row_count
) {
    SudekiMpLocalQuickMenuSession *session;
    uint32_t *cursor;

    if (state == NULL || !seat_valid(seat_index)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    session = &state->sessions[seat_index];
    if (session->open == 0u) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CANCEL) {
        if (session->target_count != 0u) {
            clear_target_selection(session);
            session->presentation_revision++;
            session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED;
            return session->last_result;
        }
        return SudekiMpLocalQuickMenuClose(state, seat_index);
    }
    if (session->target_count != 0u) {
        if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_UP) {
            session->target_cursor = session->target_cursor == 0u ?
                session->target_count - 1u : session->target_cursor - 1u;
            session->presentation_revision++;
            session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED;
            return session->last_result;
        }
        if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_DOWN) {
            session->target_cursor = (uint8_t)((session->target_cursor + 1u) %
                session->target_count);
            session->presentation_revision++;
            session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED;
            return session->last_result;
        }
        if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CONFIRM) {
            session->last_result =
                SUDEKIMP_LOCAL_QUICK_MENU_RESULT_EXECUTE_REQUESTED;
            return session->last_result;
        }
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
        return session->last_result;
    }
    if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_PREVIOUS_CATEGORY) {
        session->category = session->category == 0u ?
            SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT - 1u :
            session->category - 1u;
        session->presentation_revision++;
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CATEGORY_CHANGED;
        return session->last_result;
    }
    if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_NEXT_CATEGORY) {
        session->category = (session->category + 1u) %
            SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT;
        session->presentation_revision++;
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_CATEGORY_CHANGED;
        return session->last_result;
    }
    cursor = &session->cursor_by_category[session->category];
    if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_UP && row_count != 0u) {
        *cursor = *cursor == 0u ? row_count - 1u : *cursor - 1u;
        session->presentation_revision++;
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED;
        return session->last_result;
    }
    if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_DOWN && row_count != 0u) {
        *cursor = (*cursor + 1u) % row_count;
        session->presentation_revision++;
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_MOVED;
        return session->last_result;
    }
    if (action == SUDEKIMP_LOCAL_QUICK_MENU_ACTION_CONFIRM && row_count != 0u) {
        if (*cursor >= row_count) {
            *cursor = row_count - 1u;
        }
        session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_EXECUTE_REQUESTED;
        return session->last_result;
    }
    session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    return session->last_result;
}

SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuBeginTargetSelection(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    const void *const *targets,
    uint32_t target_count
) {
    SudekiMpLocalQuickMenuSession *session;
    uint32_t index;

    if (state == NULL || !seat_valid(seat_index) || targets == NULL ||
        target_count == 0u || target_count > SUDEKIMP_LOCAL_QUICK_MENU_MAX_TARGETS) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    session = &state->sessions[seat_index];
    if (session->open == 0u || session->target_count != 0u) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    for (index = 0u; index < target_count; ++index) {
        if (targets[index] == NULL) {
            return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_LEASE;
        }
    }
    clear_target_selection(session);
    memcpy(session->targets, targets, target_count * sizeof(targets[0]));
    session->target_count = (uint8_t)target_count;
    session->target_cursor = 0u;
    session->presentation_revision++;
    session->last_result = SUDEKIMP_LOCAL_QUICK_MENU_RESULT_TARGET_SELECTING;
    return session->last_result;
}

int SudekiMpLocalQuickMenuTargetSelectionActive(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
) {
    return state != NULL && seat_valid(seat_index) &&
        state->sessions[seat_index].open != 0u &&
        state->sessions[seat_index].target_count != 0u;
}

const void *SudekiMpLocalQuickMenuSelectedTarget(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
) {
    const SudekiMpLocalQuickMenuSession *session;

    if (state == NULL || !seat_valid(seat_index)) {
        return NULL;
    }
    session = &state->sessions[seat_index];
    return session->open != 0u && session->target_count != 0u &&
        session->target_cursor < session->target_count ?
        session->targets[session->target_cursor] : NULL;
}

int SudekiMpLocalQuickMenuSetCategorySnapshot(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    SudekiMpLocalQuickMenuCategory category,
    const SudekiMpLocalQuickMenuRow *rows,
    uint32_t row_count,
    uint32_t revision
) {
    SudekiMpLocalQuickMenuSession *session;
    SudekiMpLocalQuickMenuCategorySnapshot *snapshot;

    if (state == NULL || !seat_valid(seat_index) ||
        (unsigned int)category >= SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT ||
        row_count > SUDEKIMP_LOCAL_QUICK_MENU_MAX_ROWS || revision == 0u ||
        (row_count != 0u && rows == NULL)) {
        return 0;
    }
    session = &state->sessions[seat_index];
    if (session->open == 0u) {
        return 0;
    }
    snapshot = &session->snapshot_by_category[(unsigned int)category];
    memset(snapshot, 0, sizeof(*snapshot));
    if (row_count != 0u) {
        memcpy(snapshot->rows, rows, row_count * sizeof(rows[0]));
    }
    snapshot->row_count = row_count;
    snapshot->revision = revision;
    if (session->cursor_by_category[(unsigned int)category] >= row_count &&
        row_count != 0u) {
        session->cursor_by_category[(unsigned int)category] = row_count - 1u;
    }
    session->presentation_revision++;
    if (session->presentation_revision == 0u) {
        session->presentation_revision = 1u;
    }
    return 1;
}

const SudekiMpLocalQuickMenuRow *SudekiMpLocalQuickMenuSelectedRow(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
) {
    const SudekiMpLocalQuickMenuSession *session;
    const SudekiMpLocalQuickMenuCategorySnapshot *snapshot;
    uint32_t cursor;

    if (state == NULL || !seat_valid(seat_index)) {
        return NULL;
    }
    session = &state->sessions[seat_index];
    if (session->open == 0u ||
        session->category >= SUDEKIMP_LOCAL_QUICK_MENU_CATEGORY_COUNT) {
        return NULL;
    }
    snapshot = &session->snapshot_by_category[session->category];
    cursor = session->cursor_by_category[session->category];
    return snapshot->row_count != 0u && cursor < snapshot->row_count ?
        &snapshot->rows[cursor] : NULL;
}

SudekiMpLocalQuickMenuResult SudekiMpLocalQuickMenuRecordActionResult(
    SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index,
    SudekiMpLocalQuickMenuResult result
) {
    SudekiMpLocalQuickMenuSession *session;

    if (state == NULL || !seat_valid(seat_index)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    session = &state->sessions[seat_index];
    if (session->open == 0u ||
        (result != SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_STARTED &&
         result != SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_REJECTED &&
         result != SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_BUSY)) {
        return SUDEKIMP_LOCAL_QUICK_MENU_RESULT_REJECTED_ACTION;
    }
    session->last_result = result;
    session->presentation_revision++;
    if (session->presentation_revision == 0u) {
        session->presentation_revision = 1u;
    }
    if (result == SUDEKIMP_LOCAL_QUICK_MENU_RESULT_ACTION_STARTED) {
        close_session(session, result);
    }
    return result;
}

const SudekiMpLocalQuickMenuSession *SudekiMpLocalQuickMenuSessionForSeat(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
) {
    return state != NULL && seat_valid(seat_index) ?
        &state->sessions[seat_index] : NULL;
}

int SudekiMpLocalQuickMenuSeatActive(
    const SudekiMpLocalQuickMenuState *state,
    unsigned int seat_index
) {
    return state != NULL && seat_valid(seat_index) &&
        state->sessions[seat_index].open != 0u;
}

int SudekiMpLocalQuickMenuAnyActive(const SudekiMpLocalQuickMenuState *state) {
    unsigned int seat_index;

    if (state == NULL) {
        return 0;
    }
    for (seat_index = 0u;
         seat_index < SUDEKIMP_LOCAL_QUICK_MENU_SEAT_COUNT;
         ++seat_index) {
        if (state->sessions[seat_index].open != 0u) {
            return 1;
        }
    }
    return 0;
}
