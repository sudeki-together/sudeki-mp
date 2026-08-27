#include "engine/blacksmith_ui_presenter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* RGB channels are taken from the exact native blacksmith palette at
 * 0x0073542c..0x00735448.  The mod supplies an opaque alpha because these
 * commands are first rasterized into an A8R8G8B8 viewport texture instead of
 * going through Sudeki's queued-font color combiner. */
static const uint32_t COLOR_NATIVE_VIOLET = UINT32_C(0xffdc96ff);
static const uint32_t COLOR_NATIVE_WHITE = UINT32_C(0xffffffff);
static const uint32_t COLOR_NATIVE_GREY = UINT32_C(0xffa0a0a0);
static const uint32_t COLOR_NATIVE_GREEN = UINT32_C(0xff80ff80);
static const uint32_t COLOR_NATIVE_RED = UINT32_C(0xffff8080);
static const uint32_t COLOR_NATIVE_CYAN = UINT32_C(0xffc0ffff);
static const uint32_t COLOR_VIEWPORT_VEIL = UINT32_C(0x7003070c);
static const uint32_t COLOR_WINDOW_SHADOW = UINT32_C(0xb0000000);
static const uint32_t COLOR_WINDOW_PANEL = UINT32_C(0xf016121a);
static const uint32_t COLOR_HEADER_PANEL = UINT32_C(0xf0231828);
static const uint32_t COLOR_INSET_PANEL = UINT32_C(0xe80e1015);
static const uint32_t COLOR_ACTIVE_TAB = UINT32_C(0xff493052);
static const uint32_t COLOR_SELECTED_ROW = UINT32_C(0xff38263f);
static const uint32_t COLOR_SELECTED_ROW_EDGE = UINT32_C(0xffdc96ff);
static const uint32_t COLOR_INNER_BORDER = UINT32_C(0xff59485e);

static const char *character_label(uint32_t character_id) {
    switch (character_id) {
    case 0x23u: return "TAL";
    case 0x05u: return "BUKI";
    case 0x0eu: return "ELCO";
    case 0x01u: return "AILISH";
    default: return "UNKNOWN";
    }
}

static long round_stat(float value) {
    if (!isfinite(value)) return 0L;
    if (value > 999999.0f) return 999999L;
    if (value < -999999.0f) return -999999L;
    return value >= 0.0f ? (long)(value + 0.5f) :
        (long)(value - 0.5f);
}

static int supported_glyph(unsigned char value) {
    return (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == ' ' ||
        value == '-' || value == ':' || value == '?' || value == '>';
}

static void normalize_label(
    char *destination,
    size_t capacity,
    const char *source,
    size_t limit
) {
    size_t written = 0u;
    int previous_space = 1;

    if (destination == NULL || capacity == 0u) return;
    if (limit >= capacity) limit = capacity - 1u;
    if (source != NULL) {
        while (*source != '\0' && written < limit) {
            unsigned char value = (unsigned char)*source++;

            if (value >= 'a' && value <= 'z') {
                value = (unsigned char)(value - 'a' + 'A');
            }
            if (!supported_glyph(value)) value = ' ';
            if (value == ' ' && previous_space) continue;
            destination[written++] = (char)value;
            previous_space = value == ' ';
        }
    }
    while (written != 0u && destination[written - 1u] == ' ') --written;
    destination[written] = '\0';
}

static SudekiMpBlacksmithUiDrawCommand *next_command(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    SudekiMpBlacksmithUiDrawKind kind,
    SudekiMpBlacksmithUiDrawRole role
) {
    SudekiMpBlacksmithUiDrawCommand *command;

    if (viewport == NULL || viewport->command_count >=
            SUDEKIMP_BLACKSMITH_UI_PRESENTATION_MAX_COMMANDS) {
        return NULL;
    }
    command = &viewport->commands[viewport->command_count++];
    memset(command, 0, sizeof(*command));
    command->kind = kind;
    command->role = role;
    return command;
}

static int add_rectangle(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    SudekiMpBlacksmithUiDrawRole role,
    int left,
    int top,
    int right,
    int bottom,
    uint32_t color
) {
    SudekiMpBlacksmithUiDrawCommand *command;

    if (left < 0 || top < 0 || right <= left || bottom <= top ||
        right > (int)SUDEKIMP_BLACKSMITH_UI_VIEWPORT_WIDTH ||
        bottom > (int)SUDEKIMP_BLACKSMITH_UI_VIEWPORT_HEIGHT) {
        return 0;
    }
    command = next_command(
        viewport, SUDEKIMP_BLACKSMITH_UI_DRAW_RECTANGLE, role);
    if (command == NULL) return 0;
    command->left = left;
    command->top = top;
    command->right = right;
    command->bottom = bottom;
    command->color = color;
    return 1;
}

static int add_text(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    SudekiMpBlacksmithUiDrawRole role,
    int left,
    int top,
    int right_limit,
    const char *text,
    uint32_t color,
    uint32_t scale
) {
    SudekiMpBlacksmithUiDrawCommand *command;
    size_t character_limit;

    if (left < 0 || top < 0 || right_limit <= left ||
        right_limit > (int)SUDEKIMP_BLACKSMITH_UI_VIEWPORT_WIDTH ||
        scale == 0u ||
        top + (int)(7u * scale) >
            (int)SUDEKIMP_BLACKSMITH_UI_VIEWPORT_HEIGHT) {
        return 0;
    }
    character_limit = (size_t)(right_limit - left) / (6u * scale);
    if (character_limit == 0u) return 0;
    command = next_command(viewport, SUDEKIMP_BLACKSMITH_UI_DRAW_TEXT, role);
    if (command == NULL) return 0;
    normalize_label(command->text, sizeof(command->text), text,
        character_limit);
    if (command->text[0] == '\0') {
        --viewport->command_count;
        return 1;
    }
    command->left = left;
    command->top = top;
    command->right = left +
        (int)(strlen(command->text) * 6u * scale);
    command->bottom = top + (int)(7u * scale);
    command->color = color;
    command->text_scale = scale;
    return 1;
}

static int add_centered_text(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    SudekiMpBlacksmithUiDrawRole role,
    int center,
    int top,
    int left_limit,
    int right_limit,
    const char *text,
    uint32_t color,
    uint32_t scale
) {
    char normalized[SUDEKIMP_BLACKSMITH_UI_PRESENTATION_TEXT_CAPACITY];
    size_t maximum = (size_t)(right_limit - left_limit) / (6u * scale);
    int left;

    normalize_label(normalized, sizeof(normalized), text, maximum);
    left = center - (int)(strlen(normalized) * 6u * scale) / 2;
    if (left < left_limit) left = left_limit;
    return add_text(viewport, role, left, top, right_limit,
        normalized, color, scale);
}

static int add_right_aligned_text(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    SudekiMpBlacksmithUiDrawRole role,
    int left_limit,
    int right,
    int top,
    const char *text,
    uint32_t color
) {
    char normalized[SUDEKIMP_BLACKSMITH_UI_PRESENTATION_TEXT_CAPACITY];
    size_t maximum = (size_t)(right - left_limit) / 6u;
    int left;

    normalize_label(normalized, sizeof(normalized), text, maximum);
    left = right - (int)(strlen(normalized) * 6u);
    if (left < left_limit) left = left_limit;
    return add_text(viewport, role, left, top, right,
        normalized, color, 1u);
}

static int add_frame(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    SudekiMpBlacksmithUiDrawRole role,
    int left,
    int top,
    int right,
    int bottom,
    uint32_t color
) {
    return add_rectangle(viewport, role, left, top, right, top + 1, color) &&
        add_rectangle(viewport, role, left, bottom - 1, right, bottom, color) &&
        add_rectangle(viewport, role, left, top, left + 1, bottom, color) &&
        add_rectangle(viewport, role, right - 1, top, right, bottom, color);
}

static const SudekiMpBlacksmithReadEquipment *selected_equipment(
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index
) {
    const SudekiMpBlacksmithReadSeat *read_seat =
        &snapshot->read_model.seats[player_index];
    uint32_t selected =
        snapshot->seats[player_index].selected_equipment_index;

    return read_seat->valid && selected < read_seat->equipment_count ?
        &read_seat->equipment[selected] : NULL;
}

static const SudekiMpBlacksmithReadComponent *selected_component(
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index
) {
    uint32_t selected =
        snapshot->seats[player_index].selected_component_index;

    return selected < snapshot->read_model.component_count ?
        &snapshot->read_model.components[selected] : NULL;
}

static int add_window_chrome(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index
) {
    const SudekiMpBlacksmithUiSeatSnapshot *seat =
        &snapshot->seats[player_index];
    char text[56];

    if (!add_rectangle(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_VIEWPORT_VEIL,
            0, 0, 320, 480, COLOR_VIEWPORT_VEIL) ||
        !add_rectangle(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_SHADOW,
            9, 22, 313, 460, COLOR_WINDOW_SHADOW) ||
        !add_rectangle(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_PANEL,
            12, 18, 308, 456, COLOR_WINDOW_PANEL) ||
        !add_frame(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_BORDER,
            12, 18, 308, 456, COLOR_NATIVE_VIOLET) ||
        !add_frame(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_BORDER,
            15, 21, 305, 453, COLOR_INNER_BORDER) ||
        !add_rectangle(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_HEADER,
            16, 22, 304, 70, COLOR_HEADER_PANEL) ||
        !add_rectangle(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_HEADER,
            16, 69, 304, 71, COLOR_NATIVE_VIOLET) ||
        !add_centered_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
            160, 28, 22, 298, "BLACKSMITH",
            COLOR_NATIVE_WHITE, 2u)) {
        return 0;
    }
    snprintf(text, sizeof(text), "P%lu  %s",
        (unsigned long)(player_index + 1u),
        character_label(seat->character_id));
    if (!add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_SEAT_BADGE,
            24, 54, 170, text,
            player_index == 0u ? COLOR_NATIVE_VIOLET : COLOR_NATIVE_CYAN,
            1u)) {
        return 0;
    }
    snprintf(text, sizeof(text), "%lu GOLD",
        (unsigned long)snapshot->shared_money);
    return add_right_aligned_text(viewport,
        SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
        170, 296, 54, text, COLOR_NATIVE_WHITE);
}

static int add_tabs(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    uint32_t active_page
) {
    static const char *const labels[3] = {
        "EQUIPMENT", "SOCKETS", "RUNES"
    };
    static const int lefts[3] = {20, 115, 211};
    static const int rights[3] = {108, 204, 300};
    uint32_t page;

    for (page = 0u; page < 3u; ++page) {
        SudekiMpBlacksmithUiDrawRole role = page == active_page ?
            SUDEKIMP_BLACKSMITH_UI_ROLE_ACTIVE_TAB :
            SUDEKIMP_BLACKSMITH_UI_ROLE_TAB;
        uint32_t color = page == active_page ?
            COLOR_NATIVE_WHITE : COLOR_NATIVE_GREY;

        if (!add_rectangle(viewport, role,
                lefts[page], 79, rights[page], 98,
                page == active_page ? COLOR_ACTIVE_TAB :
                    COLOR_INSET_PANEL) ||
            !add_centered_text(viewport, role,
                (lefts[page] + rights[page]) / 2,
                85, lefts[page] + 2, rights[page] - 2,
                labels[page], color, 1u)) {
            return 0;
        }
        if (page == active_page &&
            !add_rectangle(viewport, role,
                lefts[page], 97, rights[page], 99,
                COLOR_NATIVE_VIOLET)) {
            return 0;
        }
    }
    return 1;
}

static int add_equipment_banner(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithReadEquipment *equipment
) {
    char text[56];

    if (!add_rectangle(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_DETAIL_FRAME,
            20, 106, 300, 154, COLOR_INSET_PANEL) ||
        !add_frame(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_DETAIL_FRAME,
            20, 106, 300, 154, COLOR_INNER_BORDER) ||
        !add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
            28, 112, 294, "CURRENT EQUIPMENT", COLOR_NATIVE_GREY, 1u)) {
        return 0;
    }
    if (equipment == NULL) {
        return add_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
            28, 129, 294, "EQUIPMENT UNAVAILABLE",
            COLOR_NATIVE_RED, 1u);
    }
    if (!add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
            28, 125, 294, equipment->name,
            COLOR_NATIVE_WHITE, 1u)) {
        return 0;
    }
    if (equipment->stats_valid) {
        snprintf(text, sizeof(text), "POWER %ld  RESIST %ld PCT",
            round_stat(equipment->primary_stat),
            round_stat(equipment->secondary_percent));
    } else {
        snprintf(text, sizeof(text), "STATS UNAVAILABLE");
    }
    return add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
        28, 139, 294, text, COLOR_NATIVE_GREY, 1u);
}

static int add_equipment_row(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithReadEquipment *equipment,
    int top
) {
    char detail[56];

    if (!add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
            42, top + 3, 292, equipment->name,
            COLOR_NATIVE_WHITE, 1u)) {
        return 0;
    }
    if (equipment->equipped) {
        snprintf(detail, sizeof(detail), "EQUIPPED");
    } else if (equipment->stats_valid) {
        snprintf(detail, sizeof(detail), "POWER %ld  RESIST %ld PCT",
            round_stat(equipment->primary_stat),
            round_stat(equipment->secondary_percent));
    } else {
        snprintf(detail, sizeof(detail), "STATS UNAVAILABLE");
    }
    return add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
        42, top + 14, 292, detail, COLOR_NATIVE_GREY, 1u);
}

static int add_socket_row(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithReadSocket *socket,
    uint32_t socket_index,
    int top
) {
    char text[56];
    char detail[56];

    snprintf(text, sizeof(text), "SLOT %lu  %s",
        (unsigned long)(socket_index + 1u), socket->occupant_name);
    if (socket->locked) {
        snprintf(detail, sizeof(detail), "SEALED");
    } else if (socket->authored_component_id >= 0) {
        snprintf(detail, sizeof(detail), "FIXED AUGMENT");
    } else {
        snprintf(detail, sizeof(detail), "BANK %lu  OPEN",
            (unsigned long)socket->bank);
    }
    return add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
            42, top + 3, 292, text,
            socket->locked ? COLOR_NATIVE_GREY : COLOR_NATIVE_WHITE, 1u) &&
        add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
            42, top + 14, 292, detail, COLOR_NATIVE_GREY, 1u);
}

static int add_component_row(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithReadEquipment *equipment,
    uint32_t socket_index,
    const SudekiMpBlacksmithReadComponent *component,
    int top
) {
    char detail[56];
    int compatible = component->definition_valid &&
        component->effect_valid && equipment != NULL &&
        SudekiMpBlacksmithReadModelComponentCompatible(
            equipment, socket_index, component);

    if (!component->definition_valid || !component->effect_valid) {
        snprintf(detail, sizeof(detail), "UNAVAILABLE");
    } else if (!compatible) {
        snprintf(detail, sizeof(detail), "INCOMPATIBLE");
    } else if (component->effect_class == 2u) {
        snprintf(detail, sizeof(detail), "%ld GOLD  %s %ld PCT",
            (long)component->price, component->effect_name,
            round_stat(component->effect * 100.0f));
    } else {
        snprintf(detail, sizeof(detail), "%ld GOLD  %s %ld",
            (long)component->price, component->effect_name,
            round_stat(component->effect));
    }
    return add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
            42, top + 3, 292, component->name,
            compatible ? COLOR_NATIVE_WHITE : COLOR_NATIVE_GREY, 1u) &&
        add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
            42, top + 14, 292, detail,
            compatible ? COLOR_NATIVE_GREY : COLOR_NATIVE_RED, 1u);
}

static int add_rows(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index,
    const SudekiMpBlacksmithReadEquipment *equipment
) {
    const SudekiMpBlacksmithUiSeatSnapshot *seat =
        &snapshot->seats[player_index];
    const SudekiMpBlacksmithReadSeat *read_seat =
        &snapshot->read_model.seats[player_index];
    uint32_t count;
    uint32_t offset;
    uint32_t row;

    if (!add_rectangle(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_LIST_FRAME,
            20, 162, 300, 326, COLOR_INSET_PANEL) ||
        !add_frame(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_LIST_FRAME,
            20, 162, 300, 326, COLOR_INNER_BORDER)) {
        return 0;
    }
    if (seat->page == 0u) {
        count = read_seat->equipment_count;
        offset = seat->category * SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
    } else if (seat->page == 1u) {
        count = equipment == NULL ? 0u : equipment->socket_count;
        offset = 0u;
    } else {
        count = snapshot->read_model.component_count;
        offset = seat->category * SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT;
    }
    for (row = 0u; row < SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT; ++row) {
        uint32_t absolute = offset + row;
        int top = 166 + (int)row * 26;

        if (absolute >= count) continue;
        if (row == seat->cursor) {
            if (!add_rectangle(viewport,
                    SUDEKIMP_BLACKSMITH_UI_ROLE_SELECTED_ROW,
                    24, top, 296, top + 24, COLOR_SELECTED_ROW) ||
                !add_rectangle(viewport,
                    SUDEKIMP_BLACKSMITH_UI_ROLE_SELECTED_ROW,
                    24, top, 28, top + 24, COLOR_SELECTED_ROW_EDGE) ||
                !add_text(viewport,
                    SUDEKIMP_BLACKSMITH_UI_ROLE_SELECTED_ROW,
                    31, top + 8, 41, ">", COLOR_NATIVE_VIOLET, 1u)) {
                return 0;
            }
        } else if (!add_rectangle(viewport,
                SUDEKIMP_BLACKSMITH_UI_ROLE_ROW,
                24, top + 23, 296, top + 24, COLOR_INNER_BORDER)) {
            return 0;
        }
        if (seat->page == 0u) {
            if (!add_equipment_row(viewport,
                    &read_seat->equipment[absolute], top)) return 0;
        } else if (seat->page == 1u) {
            if (!add_socket_row(viewport,
                    &equipment->sockets[absolute], absolute, top)) return 0;
        } else if (!add_component_row(viewport, equipment,
                seat->selected_socket_index,
                &snapshot->read_model.components[absolute], top)) {
            return 0;
        }
    }
    if (count == 0u) {
        return add_centered_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
            160, 235, 30, 290, "NOTHING AVAILABLE",
            COLOR_NATIVE_GREY, 1u);
    }
    return 1;
}

static int add_detail(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index,
    const SudekiMpBlacksmithReadEquipment *equipment,
    const SudekiMpBlacksmithReadComponent *component
) {
    const SudekiMpBlacksmithUiSeatSnapshot *seat =
        &snapshot->seats[player_index];
    char text[56];
    float primary;
    float secondary;

    if (!add_rectangle(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_DETAIL_FRAME,
            20, 334, 300, 413, COLOR_INSET_PANEL) ||
        !add_frame(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_DETAIL_FRAME,
            20, 334, 300, 413, COLOR_INNER_BORDER) ||
        !add_text(viewport, SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
            28, 341, 292, "DETAIL", COLOR_NATIVE_GREY, 1u)) {
        return 0;
    }
    if (seat->page == 2u && component != NULL) {
        int compatible = equipment != NULL &&
            component->definition_valid && component->effect_valid &&
            SudekiMpBlacksmithReadModelComponentCompatible(
                equipment, seat->selected_socket_index, component);

        if (!add_text(viewport,
                SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
                28, 355, 292, component->name,
                compatible ? COLOR_NATIVE_WHITE : COLOR_NATIVE_GREY, 1u)) {
            return 0;
        }
        if (compatible && SudekiMpBlacksmithReadModelProjectStats(
                equipment, seat->selected_socket_index, component,
                &primary, &secondary)) {
            snprintf(text, sizeof(text), "POWER %ld  RESIST %ld PCT",
                round_stat(primary), round_stat(secondary));
            if (!add_text(viewport,
                    SUDEKIMP_BLACKSMITH_UI_ROLE_POSITIVE_TEXT,
                    28, 369, 292, text, COLOR_NATIVE_GREEN, 1u)) {
                return 0;
            }
            snprintf(text, sizeof(text), "COST %ld GOLD",
                (long)component->price);
        } else {
            snprintf(text, sizeof(text), "CANNOT FIT SELECTED SLOT");
        }
        if (!add_text(viewport,
                compatible ? SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT :
                    SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
                28, 383, 292, text,
                compatible ? COLOR_NATIVE_WHITE : COLOR_NATIVE_RED, 1u)) {
            return 0;
        }
    } else if (equipment != NULL) {
        if (!add_text(viewport,
                SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
                28, 358, 292, equipment->name,
                COLOR_NATIVE_WHITE, 1u)) {
            return 0;
        }
        snprintf(text, sizeof(text), "%lu SOCKETS",
            (unsigned long)equipment->socket_count);
        if (!add_text(viewport,
                SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
                28, 373, 292, text, COLOR_NATIVE_GREY, 1u)) {
            return 0;
        }
    } else if (!add_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
            28, 365, 292, "NO EQUIPMENT SELECTED",
            COLOR_NATIVE_RED, 1u)) {
        return 0;
    }
    return add_centered_text(viewport,
        SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
        160, 400, 26, 294, "PREVIEW ONLY - FORGING DISABLED",
        COLOR_NATIVE_RED, 1u);
}

static int add_controls(
    SudekiMpBlacksmithUiViewportPresentation *viewport,
    uint32_t player_index,
    int catalog_truncated
) {
    const char *first = player_index == 0u ?
        "ARROWS MOVE  ENTER PREVIEW" : "DPAD MOVE  A PREVIEW";
    const char *second = player_index == 0u ?
        "PGUP PGDN PAGE  ESC BACK" : "LB RB PAGE  B BACK";

    if (catalog_truncated && !add_centered_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
            160, 416, 22, 298, "CATALOG INCOMPLETE",
            COLOR_NATIVE_RED, 1u)) {
        return 0;
    }
    return add_centered_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
            160, 429, 22, 298, first, COLOR_NATIVE_WHITE, 1u) &&
        add_centered_text(viewport,
            SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
            160, 442, 22, 298, second, COLOR_NATIVE_GREY, 1u);
}

static int build_viewport(
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index,
    SudekiMpBlacksmithUiViewportPresentation *viewport
) {
    const SudekiMpBlacksmithUiSeatSnapshot *seat =
        &snapshot->seats[player_index];
    const SudekiMpBlacksmithReadEquipment *equipment =
        selected_equipment(snapshot, player_index);
    const SudekiMpBlacksmithReadComponent *component =
        selected_component(snapshot, player_index);

    memset(viewport, 0, sizeof(*viewport));
    if (!add_window_chrome(viewport, snapshot, player_index)) return 0;
    if (!seat->open) {
        const char *status = seat->notice ==
                SUDEKIMP_BLACKSMITH_UI_NOTICE_DROPPED_OUT ?
            "PLAYER DROPPED OUT" : "BLACKSMITH CLOSED";

        return add_centered_text(viewport,
                SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
                160, 218, 28, 292, status,
                COLOR_NATIVE_RED, 2u) &&
            add_centered_text(viewport,
                SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
                160, 250, 28, 292, "WAITING FOR OTHER PLAYER",
                COLOR_NATIVE_GREY, 1u);
    }
    return add_tabs(viewport, seat->page %
            SUDEKIMP_BLACKSMITH_UI_PAGE_COUNT) &&
        add_equipment_banner(viewport, equipment) &&
        add_rows(viewport, snapshot, player_index, equipment) &&
        add_detail(viewport, snapshot, player_index, equipment, component) &&
        add_controls(viewport, player_index,
            snapshot->read_model.catalog_truncated);
}

static int valid_viewport_snapshot(
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    uint32_t player_index
) {
    const SudekiMpBlacksmithUiSeatSnapshot *seat =
        &snapshot->seats[player_index];
    const SudekiMpBlacksmithReadSeat *read_seat =
        &snapshot->read_model.seats[player_index];
    uint32_t visible_index;
    uint32_t entry_count;

    if (!read_seat->valid ||
        read_seat->character_id != seat->character_id ||
        read_seat->actor_generation != seat->actor_generation ||
        read_seat->equipment_count >
            SUDEKIMP_BLACKSMITH_READ_MAX_EQUIPMENT ||
        seat->page >= SUDEKIMP_BLACKSMITH_UI_PAGE_COUNT ||
        seat->category >= SUDEKIMP_BLACKSMITH_UI_CATEGORY_COUNT ||
        seat->cursor >= SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT ||
        (read_seat->equipment_count != 0u &&
         seat->selected_equipment_index >= read_seat->equipment_count) ||
        (snapshot->read_model.component_count != 0u &&
         seat->selected_component_index >=
            snapshot->read_model.component_count)) {
        return 0;
    }
    if (read_seat->equipment_count != 0u &&
        read_seat->equipment[seat->selected_equipment_index].socket_count >
            SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS) {
        return 0;
    }
    if (!seat->open) return 1;
    if (seat->page == 0u) {
        entry_count = read_seat->equipment_count;
        visible_index = seat->category *
            SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT + seat->cursor;
    } else if (seat->page == 1u) {
        entry_count = read_seat->equipment_count == 0u ? 0u :
            read_seat->equipment[
                seat->selected_equipment_index].socket_count;
        visible_index = seat->cursor;
    } else {
        entry_count = snapshot->read_model.component_count;
        visible_index = seat->category *
            SUDEKIMP_BLACKSMITH_UI_CURSOR_COUNT + seat->cursor;
    }
    return entry_count == 0u || visible_index < entry_count;
}

int SudekiMpBlacksmithUiBuildPresentation(
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    SudekiMpBlacksmithUiPresentation *presentation
) {
    uint32_t player_index;

    if (snapshot == NULL || presentation == NULL || !snapshot->active ||
        !snapshot->shared_money_valid || !snapshot->read_model.valid ||
        snapshot->native_commit_enabled ||
        snapshot->read_model.player_count <
            SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT ||
        snapshot->read_model.player_count >
            SUDEKIMP_BLACKSMITH_READ_MAX_PLAYERS ||
        snapshot->read_model.component_count >
            SUDEKIMP_BLACKSMITH_READ_MAX_COMPONENTS) {
        return 0;
    }
    memset(presentation, 0, sizeof(*presentation));
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        if (!valid_viewport_snapshot(snapshot, player_index) ||
            !build_viewport(snapshot, player_index,
                &presentation->viewports[player_index])) {
            memset(presentation, 0, sizeof(*presentation));
            return 0;
        }
    }
    return 1;
}
