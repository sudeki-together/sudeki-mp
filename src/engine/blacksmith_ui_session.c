#include "engine/blacksmith_ui_session.h"

#include <string.h>

enum {
    RVA_BLACKSMITH_ACTIVE_EXPORT_ENTRY = 0x0030d414u,
    RVA_BLACKSMITH_START_EXPORT_ENTRY = 0x0030d418u,
    RVA_BLACKSMITH_START = 0x00092c40u,
    RVA_BLACKSMITH_ACTIVE = 0x00092c60u,
    RVA_WORLD_SCENE_GLOBAL = 0x00408d1cu,
    RVA_BLACKSMITH_LAYER_GLOBAL = 0x003c2f74u,
    PREFERRED_IMAGE_BASE = 0x00400000u
};

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) {
        ++value;
    }
    return value;
}

static int is_applied(SudekiMpBlacksmithShadowResult result) {
    return result == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED ||
        result == SUDEKIMP_BLACKSMITH_SHADOW_NO_CHANGE;
}

static int any_open(const SudekiMpBlacksmithUiSession *session) {
    uint32_t player_index;

    if (session == NULL || !session->active) {
        return 0;
    }
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        if (session->coordinator.players[player_index].state !=
            SUDEKIMP_BLACKSMITH_SHADOW_CLOSED) {
            return 1;
        }
    }
    return 0;
}

static int valid_read_model(
    const SudekiMpBlacksmithReadSnapshot *read_model,
    const uint32_t character_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    const uint32_t actor_generations[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT]
) {
    uint32_t player_index;

    if (read_model == NULL || !read_model->valid ||
        read_model->player_count < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT) {
        return 0;
    }
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        if (!read_model->seats[player_index].valid ||
            read_model->seats[player_index].character_id !=
                character_ids[player_index] ||
            read_model->seats[player_index].actor_generation !=
                actor_generations[player_index]) {
            return 0;
        }
    }
    return 1;
}

static uint32_t selected_equipment_socket_count(
    const SudekiMpBlacksmithUiSession *session,
    uint32_t player_index
) {
    const SudekiMpBlacksmithReadSeat *seat;
    uint32_t selected;

    if (session == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT) {
        return 0u;
    }
    seat = &session->read_model.seats[player_index];
    selected = session->selected_equipment_indices[player_index];
    return seat->valid && selected < seat->equipment_count ?
        seat->equipment[selected].socket_count : 0u;
}

static uint32_t page_entry_count(
    const SudekiMpBlacksmithUiSession *session,
    uint32_t player_index,
    uint32_t page
) {
    if (page == 0u) {
        return session->read_model.seats[player_index].equipment_count;
    }
    if (page == 1u) {
        return selected_equipment_socket_count(session, player_index);
    }
    return session->read_model.component_count;
}

static uint32_t list_page_count(
    const SudekiMpBlacksmithUiSession *session,
    uint32_t player_index,
    uint32_t page
) {
    uint32_t entries = page_entry_count(session, player_index, page);

    if (page == 1u || entries == 0u) {
        return 1u;
    }
    entries = (entries + SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT - 1u) /
        SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
    return entries < SUDEKIMP_BLACKSMITH_UI_CATEGORY_COUNT ? entries :
        SUDEKIMP_BLACKSMITH_UI_CATEGORY_COUNT;
}

static uint32_t visible_row_count(
    const SudekiMpBlacksmithUiSession *session,
    uint32_t player_index,
    uint32_t page,
    uint32_t list_page
) {
    uint32_t entries = page_entry_count(session, player_index, page);
    uint32_t offset;

    if (page == 1u) {
        return entries < SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT ? entries :
            SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
    }
    offset = list_page * SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
    if (offset >= entries) return 0u;
    entries -= offset;
    return entries < SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT ? entries :
        SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
}

static int refresh_open_shadows(SudekiMpBlacksmithUiSession *session) {
    uint32_t player_index;

    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        SudekiMpBlacksmithPlayerShadow *shadow =
            &session->coordinator.players[player_index];
        if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
            continue;
        }
        if (!is_applied(SudekiMpBlacksmithShadowRefresh(
                &session->coordinator,
                player_index,
                shadow->session_serial,
                shadow->revision,
                shadow->merchant_id,
                shadow->merchant_generation))) {
            return 0;
        }
    }
    return 1;
}

static int align_open_navigation_to_selection(
    SudekiMpBlacksmithUiSession *session
) {
    uint32_t player_index;

    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        SudekiMpBlacksmithPlayerShadow *shadow =
            &session->coordinator.players[player_index];
        uint32_t absolute;
        uint32_t category;
        uint32_t cursor;

        if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
            continue;
        }
        if (shadow->selection.page == 0u) {
            absolute = session->selected_equipment_indices[player_index];
            category = absolute / SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
            cursor = absolute % SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
        } else if (shadow->selection.page == 1u) {
            category = 0u;
            cursor = session->selected_socket_indices[player_index];
        } else {
            absolute = session->selected_component_indices[player_index];
            category = absolute / SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
            cursor = absolute % SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
        }
        if (!is_applied(SudekiMpBlacksmithShadowSetNavigation(
                &session->coordinator,
                player_index,
                shadow->session_serial,
                shadow->revision,
                shadow->selection.page,
                category,
                cursor))) {
            return 0;
        }
    }
    return 1;
}

static void finish_if_closed(SudekiMpBlacksmithUiSession *session) {
    if (session != NULL && !any_open(session)) {
        session->active = 0;
    }
}

static int start_signatures_match(
    const uint8_t *image,
    size_t image_size,
    uint32_t expected_world_scene_global,
    uint32_t expected_blacksmith_layer_global
) {
    uint32_t start_export;
    uint32_t active_export;
    uint32_t start_operand;
    uint32_t active_operand;

    if (image == NULL ||
        image_size < RVA_BLACKSMITH_START_EXPORT_ENTRY + sizeof(uint32_t) ||
        image_size < RVA_BLACKSMITH_ACTIVE + 7u ||
        image_size < RVA_BLACKSMITH_START + 7u) {
        return 0;
    }
    memcpy(&active_export,
        image + RVA_BLACKSMITH_ACTIVE_EXPORT_ENTRY,
        sizeof(active_export));
    memcpy(&start_export,
        image + RVA_BLACKSMITH_START_EXPORT_ENTRY,
        sizeof(start_export));
    memcpy(&start_operand, image + RVA_BLACKSMITH_START + 1u,
        sizeof(start_operand));
    memcpy(&active_operand, image + RVA_BLACKSMITH_ACTIVE + 1u,
        sizeof(active_operand));
    return active_export == RVA_BLACKSMITH_ACTIVE &&
        start_export == RVA_BLACKSMITH_START &&
        image[RVA_BLACKSMITH_START] == 0xa1u &&
        start_operand == expected_world_scene_global &&
        image[RVA_BLACKSMITH_START + 5u] == 0x85u &&
        image[RVA_BLACKSMITH_START + 6u] == 0xc0u &&
        image[RVA_BLACKSMITH_ACTIVE] == 0xa1u &&
        active_operand == expected_blacksmith_layer_global &&
        image[RVA_BLACKSMITH_ACTIVE + 5u] == 0x85u &&
        image[RVA_BLACKSMITH_ACTIVE + 6u] == 0xc0u;
}

int SudekiMpBlacksmithUiStartSignaturesMatch(
    const uint8_t *image,
    size_t image_size
) {
    return start_signatures_match(
        image,
        image_size,
        PREFERRED_IMAGE_BASE + RVA_WORLD_SCENE_GLOBAL,
        PREFERRED_IMAGE_BASE + RVA_BLACKSMITH_LAYER_GLOBAL);
}

int SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
    const uint8_t *image,
    size_t image_size,
    uintptr_t loaded_image_base
) {
    if (loaded_image_base >
        (uintptr_t)(UINT32_MAX - RVA_WORLD_SCENE_GLOBAL)) {
        return 0;
    }
    return start_signatures_match(
        image,
        image_size,
        (uint32_t)(loaded_image_base + RVA_WORLD_SCENE_GLOBAL),
        (uint32_t)(loaded_image_base + RVA_BLACKSMITH_LAYER_GLOBAL));
}

void SudekiMpBlacksmithUiSessionInitialize(
    SudekiMpBlacksmithUiSession *session
) {
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    SudekiMpBlacksmithShadowInitialize(&session->coordinator);
}

int SudekiMpBlacksmithUiSessionBegin(
    SudekiMpBlacksmithUiSession *session,
    const uint32_t character_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    const uint32_t actor_generations[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT],
    uint32_t shared_money,
    const SudekiMpBlacksmithReadSnapshot *read_model,
    uint32_t now_ms
) {
    SudekiMpBlacksmithSharedSnapshot shared;
    uint32_t player_index;

    if (session == NULL || character_ids == NULL ||
        actor_generations == NULL || session->active ||
        !valid_read_model(read_model, character_ids, actor_generations)) {
        return 0;
    }
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        if (character_ids[player_index] == 0u ||
            actor_generations[player_index] == 0u) {
            return 0;
        }
    }

    session->presentation_serial = advance_nonzero(
        session->presentation_serial);
    SudekiMpBlacksmithShadowReset(&session->coordinator);
    shared.world_generation = session->presentation_serial;
    shared.catalog_generation = 1u;
    shared.inventory_generation = 1u;
    shared.economy_generation = 1u;
    if (!is_applied(SudekiMpBlacksmithShadowPublishSharedSnapshot(
            &session->coordinator, &shared))) {
        return 0;
    }
    /* UIBlackSmithStart has no merchant argument. This ID intentionally names
     * only the mod presentation session; it is never eligible for a native
     * commit ticket or persisted as a merchant identity. */
    session->presentation_merchant_id =
        UINT32_C(0x80000000) | session->presentation_serial;
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        if (!is_applied(SudekiMpBlacksmithShadowPublishPlayer(
                &session->coordinator,
                player_index,
                character_ids[player_index],
                actor_generations[player_index],
                1)) ||
            !is_applied(SudekiMpBlacksmithShadowOpen(
                &session->coordinator,
                player_index,
                session->presentation_merchant_id,
                session->presentation_serial))) {
            SudekiMpBlacksmithShadowReset(&session->coordinator);
            return 0;
        }
        session->notices[player_index] =
            SUDEKIMP_BLACKSMITH_UI_NOTICE_NAVIGATE;
    }
    session->shared_money = shared_money;
    session->shared_money_valid = 1;
    session->read_model = *read_model;
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        const SudekiMpBlacksmithReadSeat *seat =
            &session->read_model.seats[player_index];
        session->selected_equipment_indices[player_index] =
            seat->equipped_index < seat->equipment_count ?
                seat->equipped_index : 0u;
        session->selected_socket_indices[player_index] = 0u;
        session->selected_component_indices[player_index] = 0u;
    }
    session->overlay_acknowledged = 0;
    session->overlay_deadline_ms = now_ms +
        SUDEKIMP_BLACKSMITH_UI_OVERLAY_DEADLINE_MS;
    session->active = 1;
    return 1;
}

int SudekiMpBlacksmithUiSessionApplyInput(
    SudekiMpBlacksmithUiSession *session,
    uint32_t player_index,
    SudekiMpBlacksmithUiInput input
) {
    SudekiMpBlacksmithPlayerShadow *shadow;
    uint32_t page;
    uint32_t category;
    uint32_t cursor;
    uint32_t pages;
    uint32_t rows;
    SudekiMpBlacksmithShadowResult result;

    if (session == NULL || !session->active ||
        player_index >= SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT ||
        input <= SUDEKIMP_BLACKSMITH_UI_INPUT_NONE ||
        input > SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE) {
        return 0;
    }
    shadow = &session->coordinator.players[player_index];
    if (shadow->state != SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
        return 0;
    }
    if (input == SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE) {
        result = SudekiMpBlacksmithShadowClose(
            &session->coordinator,
            player_index,
            shadow->session_serial,
            shadow->revision);
        finish_if_closed(session);
        return result == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED;
    }
    if (input == SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIEW) {
        session->notices[player_index] =
            SUDEKIMP_BLACKSMITH_UI_NOTICE_COMMIT_DISABLED;
        return 1;
    }

    page = shadow->selection.page;
    category = shadow->selection.category;
    cursor = shadow->selection.cursor;
    switch (input) {
    case SUDEKIMP_BLACKSMITH_UI_INPUT_UP:
        rows = visible_row_count(
            session, player_index, page, category);
        if (rows == 0u) return 0;
        cursor = cursor == 0u ? rows - 1u : cursor - 1u;
        break;
    case SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN:
        rows = visible_row_count(
            session, player_index, page, category);
        if (rows == 0u) return 0;
        cursor = (cursor + 1u) % rows;
        break;
    case SUDEKIMP_BLACKSMITH_UI_INPUT_LEFT:
        pages = list_page_count(session, player_index, page);
        category = category == 0u ? pages - 1u : category - 1u;
        rows = visible_row_count(session, player_index, page, category);
        if (rows == 0u) cursor = 0u;
        else if (cursor >= rows) cursor = rows - 1u;
        break;
    case SUDEKIMP_BLACKSMITH_UI_INPUT_RIGHT:
        pages = list_page_count(session, player_index, page);
        category = (category + 1u) % pages;
        rows = visible_row_count(session, player_index, page, category);
        if (rows == 0u) cursor = 0u;
        else if (cursor >= rows) cursor = rows - 1u;
        break;
    case SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIOUS_PAGE:
        page = page == 0u ?
            SUDEKIMP_BLACKSMITH_UI_PAGE_COUNT - 1u : page - 1u;
        category = 0u;
        cursor = 0u;
        break;
    case SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE:
        page = (page + 1u) % SUDEKIMP_BLACKSMITH_UI_PAGE_COUNT;
        category = 0u;
        cursor = 0u;
        break;
    default:
        return 0;
    }
    result = SudekiMpBlacksmithShadowSetNavigation(
        &session->coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        page,
        category,
        cursor);
    if (is_applied(result)) {
        uint32_t absolute = category *
            SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT + cursor;
        if (page == 0u && absolute <
                session->read_model.seats[player_index].equipment_count) {
            session->selected_equipment_indices[player_index] = absolute;
            if (session->selected_socket_indices[player_index] >=
                    selected_equipment_socket_count(
                        session, player_index)) {
                session->selected_socket_indices[player_index] = 0u;
            }
        } else if (page == 1u && cursor <
                selected_equipment_socket_count(session, player_index)) {
            session->selected_socket_indices[player_index] = cursor;
        } else if (page == 2u && absolute <
                session->read_model.component_count) {
            session->selected_component_indices[player_index] = absolute;
        }
        session->notices[player_index] =
            SUDEKIMP_BLACKSMITH_UI_NOTICE_NAVIGATE;
        return 1;
    }
    return 0;
}

int SudekiMpBlacksmithUiSessionDropPlayer(
    SudekiMpBlacksmithUiSession *session,
    uint32_t player_index
) {
    if (session == NULL ||
        player_index >= SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT) {
        return 0;
    }
    session->notices[player_index] =
        SUDEKIMP_BLACKSMITH_UI_NOTICE_DROPPED_OUT;
    return SudekiMpBlacksmithUiSessionApplyInput(
        session, player_index, SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE);
}

int SudekiMpBlacksmithUiSessionObserveMoney(
    SudekiMpBlacksmithUiSession *session,
    uint32_t shared_money
) {
    SudekiMpBlacksmithSharedSnapshot shared;

    if (session == NULL || !session->active) {
        return 0;
    }
    if (session->shared_money_valid &&
        session->shared_money == shared_money) {
        return 1;
    }
    session->shared_money = shared_money;
    session->shared_money_valid = 1;
    shared = session->coordinator.shared_snapshot;
    shared.economy_generation = advance_nonzero(
        shared.economy_generation);
    if (!is_applied(SudekiMpBlacksmithShadowPublishSharedSnapshot(
            &session->coordinator, &shared))) {
        return 0;
    }
    return refresh_open_shadows(session);
}

int SudekiMpBlacksmithUiSessionObserveReadModel(
    SudekiMpBlacksmithUiSession *session,
    const SudekiMpBlacksmithReadSnapshot *read_model
) {
    SudekiMpBlacksmithSharedSnapshot shared;
    uint32_t characters[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t generations[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t selected_equipment_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t selected_component_ids[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    int selected_equipment_valid[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    int selected_component_valid[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
    uint32_t player_index;
    int catalog_changed;
    int inventory_changed;

    if (session == NULL || !session->active || read_model == NULL) {
        return 0;
    }
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        characters[player_index] =
            session->coordinator.actors[player_index].character_id;
        generations[player_index] =
            session->coordinator.actors[player_index].actor_generation;
        selected_equipment_valid[player_index] =
            session->selected_equipment_indices[player_index] <
                session->read_model.seats[player_index].equipment_count;
        selected_equipment_ids[player_index] =
            selected_equipment_valid[player_index] ?
                session->read_model.seats[player_index].equipment[
                    session->selected_equipment_indices[player_index]].item_id :
                0u;
        selected_component_valid[player_index] =
            session->selected_component_indices[player_index] <
                session->read_model.component_count;
        selected_component_ids[player_index] =
            selected_component_valid[player_index] ?
                session->read_model.components[
                    session->selected_component_indices[player_index]].component_id :
                0u;
    }
    if (!valid_read_model(read_model, characters, generations)) {
        return 0;
    }
    catalog_changed = session->read_model.catalog_fingerprint !=
        read_model->catalog_fingerprint;
    inventory_changed = session->read_model.inventory_fingerprint !=
        read_model->inventory_fingerprint;
    session->read_model = *read_model;
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        const SudekiMpBlacksmithReadSeat *seat =
            &session->read_model.seats[player_index];
        uint32_t index;
        int equipment_found = 0;
        int component_found = 0;

        if (selected_equipment_valid[player_index]) {
            for (index = 0u; index < seat->equipment_count; ++index) {
                if (seat->equipment[index].item_id ==
                        selected_equipment_ids[player_index]) {
                    session->selected_equipment_indices[player_index] = index;
                    equipment_found = 1;
                    break;
                }
            }
        }
        if (!equipment_found) {
            session->selected_equipment_indices[player_index] =
                seat->equipped_index < seat->equipment_count ?
                    seat->equipped_index : 0u;
            session->selected_socket_indices[player_index] = 0u;
            session->selected_component_indices[player_index] = 0u;
        }
        if (session->selected_socket_indices[player_index] >=
                selected_equipment_socket_count(session, player_index)) {
            session->selected_socket_indices[player_index] = 0u;
        }
        if (equipment_found && selected_component_valid[player_index]) {
            for (index = 0u;
                 index < session->read_model.component_count; ++index) {
                if (session->read_model.components[index].component_id ==
                        selected_component_ids[player_index]) {
                    session->selected_component_indices[player_index] = index;
                    component_found = 1;
                    break;
                }
            }
        }
        if (!component_found) {
            session->selected_component_indices[player_index] = 0u;
        }
    }
    if (!catalog_changed && !inventory_changed) {
        return 1;
    }
    shared = session->coordinator.shared_snapshot;
    if (catalog_changed) {
        shared.catalog_generation = advance_nonzero(
            shared.catalog_generation);
    }
    if (inventory_changed) {
        shared.inventory_generation = advance_nonzero(
            shared.inventory_generation);
    }
    if (!is_applied(SudekiMpBlacksmithShadowPublishSharedSnapshot(
            &session->coordinator, &shared))) {
        return 0;
    }
    return refresh_open_shadows(session) &&
        align_open_navigation_to_selection(session);
}

void SudekiMpBlacksmithUiSessionReportOverlay(
    SudekiMpBlacksmithUiSession *session,
    int visible
) {
    if (session != NULL && session->active && visible) {
        session->overlay_acknowledged = 1;
    }
}

int SudekiMpBlacksmithUiSessionService(
    SudekiMpBlacksmithUiSession *session,
    uint32_t now_ms
) {
    uint32_t player_index;

    if (session == NULL || !session->active) {
        return 0;
    }
    if (!session->overlay_acknowledged &&
        (int32_t)(now_ms - session->overlay_deadline_ms) >= 0) {
        for (player_index = 0u;
             player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
             ++player_index) {
            SudekiMpBlacksmithPlayerShadow *shadow =
                &session->coordinator.players[player_index];
            if (shadow->state == SUDEKIMP_BLACKSMITH_SHADOW_BROWSING) {
                (void)SudekiMpBlacksmithShadowClose(
                    &session->coordinator,
                    player_index,
                    shadow->session_serial,
                    shadow->revision);
            }
        }
        session->active = 0;
        return 0;
    }
    finish_if_closed(session);
    return session->active;
}

int SudekiMpBlacksmithUiSessionGetSnapshot(
    const SudekiMpBlacksmithUiSession *session,
    SudekiMpBlacksmithUiSnapshot *snapshot
) {
    uint32_t player_index;

    if (session == NULL || snapshot == NULL) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->active = session->active;
    snapshot->presentation_serial = session->presentation_serial;
    snapshot->shared_money = session->shared_money;
    snapshot->shared_money_valid = session->shared_money_valid;
    snapshot->catalog_generation =
        session->coordinator.shared_snapshot.catalog_generation;
    snapshot->inventory_generation =
        session->coordinator.shared_snapshot.inventory_generation;
    snapshot->economy_generation =
        session->coordinator.shared_snapshot.economy_generation;
    snapshot->overlay_acknowledged = session->overlay_acknowledged;
    /* These remain false until a world-target resolver and verified native
     * transaction adapter exist. The UI must present that limitation. */
    snapshot->merchant_target_resolved = 0;
    snapshot->native_commit_enabled = 0;
    snapshot->read_model = session->read_model;
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        const SudekiMpBlacksmithPlayerShadow *shadow =
            &session->coordinator.players[player_index];
        SudekiMpBlacksmithUiSeatSnapshot *seat =
            &snapshot->seats[player_index];
        seat->open = shadow->state ==
            SUDEKIMP_BLACKSMITH_SHADOW_BROWSING;
        seat->character_id = shadow->character_id;
        seat->actor_generation = shadow->actor_generation;
        seat->session_serial = shadow->session_serial;
        seat->revision = shadow->revision;
        seat->page = shadow->selection.page;
        seat->category = shadow->selection.category;
        seat->cursor = shadow->selection.cursor;
        seat->selected_equipment_index =
            session->selected_equipment_indices[player_index];
        seat->selected_socket_index =
            session->selected_socket_indices[player_index];
        seat->selected_component_index =
            session->selected_component_indices[player_index];
        seat->notice = session->notices[player_index];
    }
    return session->active;
}
