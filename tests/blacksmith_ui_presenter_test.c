#include "engine/blacksmith_ui_presenter.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpBlacksmithUiSnapshot make_snapshot(void) {
    SudekiMpBlacksmithUiSnapshot snapshot;
    uint32_t player;
    uint32_t item;
    uint32_t component;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.active = 1;
    snapshot.shared_money = 1234u;
    snapshot.shared_money_valid = 1;
    snapshot.read_model.valid = 1;
    snapshot.read_model.player_count = 2u;
    snapshot.read_model.component_count = 8u;
    for (component = 0u; component < 8u; ++component) {
        SudekiMpBlacksmithReadComponent *entry =
            &snapshot.read_model.components[component];
        entry->component_id = component;
        entry->price = (int32_t)(100u + component * 25u);
        entry->kind = 1u;
        entry->bank = 1u;
        entry->effect_class = 1u;
        entry->effect = 2.0f + (float)component;
        entry->definition_valid = 1;
        entry->effect_valid = 1;
        snprintf(entry->name, sizeof(entry->name),
            "Rune %lu", (unsigned long)component);
        strcpy(entry->effect_name, "Power");
    }
    for (player = 0u; player < 2u; ++player) {
        SudekiMpBlacksmithUiSeatSnapshot *seat = &snapshot.seats[player];
        SudekiMpBlacksmithReadSeat *read_seat =
            &snapshot.read_model.seats[player];

        seat->open = 1;
        seat->character_id = player == 0u ? 0x23u : 0x01u;
        seat->actor_generation = 10u + player;
        seat->page = player == 0u ? 0u : 2u;
        seat->cursor = player == 0u ? 1u : 2u;
        seat->selected_equipment_index = player;
        seat->selected_socket_index = 0u;
        seat->selected_component_index = player == 0u ? 0u : 2u;
        read_seat->valid = 1;
        read_seat->character_id = seat->character_id;
        read_seat->actor_generation = seat->actor_generation;
        read_seat->equipment_count = 6u;
        for (item = 0u; item < 6u; ++item) {
            SudekiMpBlacksmithReadEquipment *equipment =
                &read_seat->equipment[item];
            uint32_t socket;

            equipment->item_id = player * 100u + item;
            equipment->socket_count = 2u;
            equipment->primary_stat = 20.0f + (float)item;
            equipment->secondary_percent = 5.0f;
            equipment->stats_valid = 1;
            equipment->equipped = item == 0u;
            snprintf(equipment->name, sizeof(equipment->name),
                "Hero's Blade %lu", (unsigned long)item);
            for (socket = 0u; socket < 2u; ++socket) {
                equipment->sockets[socket].authored_component_id = -1;
                equipment->sockets[socket].occupant_component_id = -1;
                equipment->sockets[socket].bank = 1u;
                strcpy(equipment->sockets[socket].occupant_name, "Empty");
            }
        }
    }
    return snapshot;
}

static int contains_text(
    const SudekiMpBlacksmithUiViewportPresentation *viewport,
    const char *needle
) {
    uint32_t index;

    for (index = 0u; index < viewport->command_count; ++index) {
        const SudekiMpBlacksmithUiDrawCommand *command =
            &viewport->commands[index];
        if (command->kind == SUDEKIMP_BLACKSMITH_UI_DRAW_TEXT &&
            strstr(command->text, needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int unsupported_text(const char *text) {
    while (*text != '\0') {
        unsigned char value = (unsigned char)*text++;
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == ' ' ||
              value == '-' || value == ':' || value == '?' ||
              value == '>')) {
            return 1;
        }
    }
    return 0;
}

static void test_viewport_local_native_style(void) {
    SudekiMpBlacksmithUiSnapshot snapshot = make_snapshot();
    SudekiMpBlacksmithUiPresentation presentation;
    uint32_t player;

    CHECK(SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
    for (player = 0u; player < 2u; ++player) {
        const SudekiMpBlacksmithUiViewportPresentation *viewport =
            &presentation.viewports[player];
        uint32_t index;
        uint32_t selected_rows = 0u;
        int selected_top = -1;

        CHECK(viewport->command_count != 0u);
        CHECK(contains_text(viewport, "BLACKSMITH"));
        CHECK(contains_text(viewport, "1234 GOLD"));
        CHECK(contains_text(viewport, "PREVIEW ONLY - FORGING DISABLED"));
        CHECK(!contains_text(viewport, "BLACKSMITH LAB"));
        CHECK(!contains_text(viewport, "SEAT REV"));
        CHECK(!contains_text(viewport, "FILTER"));
        CHECK(!contains_text(viewport, "CAT 1"));
        CHECK(!contains_text(viewport, "INV 1"));
        CHECK(!contains_text(viewport, "ECO 1"));
        for (index = 0u; index < viewport->command_count; ++index) {
            const SudekiMpBlacksmithUiDrawCommand *command =
                &viewport->commands[index];

            CHECK(command->left >= 0);
            CHECK(command->top >= 0);
            CHECK(command->right <=
                (int)SUDEKIMP_BLACKSMITH_UI_VIEWPORT_WIDTH);
            CHECK(command->bottom <=
                (int)SUDEKIMP_BLACKSMITH_UI_VIEWPORT_HEIGHT);
            CHECK(command->right > command->left);
            CHECK(command->bottom > command->top);
            if (command->kind == SUDEKIMP_BLACKSMITH_UI_DRAW_TEXT) {
                CHECK(!unsupported_text(command->text));
            }
            if (command->role ==
                    SUDEKIMP_BLACKSMITH_UI_ROLE_SELECTED_ROW &&
                command->kind == SUDEKIMP_BLACKSMITH_UI_DRAW_RECTANGLE &&
                command->right == 296) {
                ++selected_rows;
                selected_top = command->top;
            }
        }
        CHECK(selected_rows == 1u);
        CHECK(selected_top == 166 + (int)snapshot.seats[player].cursor * 26);
    }
    CHECK(contains_text(&presentation.viewports[0], "P1 TAL"));
    CHECK(contains_text(&presentation.viewports[1], "P2 AILISH"));
    CHECK(contains_text(&presentation.viewports[0], "ENTER PREVIEW"));
    CHECK(contains_text(&presentation.viewports[1], "A PREVIEW"));
    CHECK(contains_text(&presentation.viewports[1], "B BACK"));
    CHECK(contains_text(&presentation.viewports[1], "LB RB PAGE"));
    CHECK(!contains_text(&presentation.viewports[0], "RUNE 2"));
    CHECK(contains_text(&presentation.viewports[1], "RUNE 2"));
}

static void test_closed_and_fail_closed_contract(void) {
    SudekiMpBlacksmithUiSnapshot snapshot = make_snapshot();
    SudekiMpBlacksmithUiPresentation presentation;

    snapshot.seats[1].open = 0;
    snapshot.seats[1].notice = SUDEKIMP_BLACKSMITH_UI_NOTICE_DROPPED_OUT;
    CHECK(SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
    CHECK(contains_text(&presentation.viewports[1], "PLAYER DROPPED OUT"));
    CHECK(contains_text(&presentation.viewports[1],
        "WAITING FOR OTHER PLAYER"));
    CHECK(!contains_text(&presentation.viewports[1], "A PREVIEW"));

    snapshot.native_commit_enabled = 1;
    CHECK(!SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
    snapshot.native_commit_enabled = 0;
    snapshot.shared_money_valid = 0;
    CHECK(!SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
    snapshot.shared_money_valid = 1;
    snapshot.read_model.seats[0].actor_generation++;
    CHECK(!SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));

    snapshot = make_snapshot();
    snapshot.seats[0].page = SUDEKIMP_BLACKSMITH_UI_PAGE_COUNT;
    CHECK(!SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
    snapshot = make_snapshot();
    snapshot.read_model.seats[0].equipment[0].socket_count =
        SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS + 1u;
    snapshot.seats[0].selected_equipment_index = 0u;
    CHECK(!SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
}

static void test_partial_catalog_notice(void) {
    SudekiMpBlacksmithUiSnapshot snapshot = make_snapshot();
    SudekiMpBlacksmithUiPresentation presentation;

    snapshot.read_model.catalog_truncated = 1;
    CHECK(SudekiMpBlacksmithUiBuildPresentation(
        &snapshot, &presentation));
    CHECK(contains_text(&presentation.viewports[0], "CATALOG INCOMPLETE"));
    CHECK(contains_text(&presentation.viewports[1], "CATALOG INCOMPLETE"));
}

int main(void) {
    test_viewport_local_native_style();
    test_closed_and_fail_closed_contract();
    test_partial_catalog_notice();
    if (failures != 0) {
        fprintf(stderr, "blacksmith_ui_presenter_test: %d failure(s)\n",
            failures);
        return 1;
    }
    puts("blacksmith_ui_presenter_test: PASS");
    return 0;
}
