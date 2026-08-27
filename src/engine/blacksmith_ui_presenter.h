#ifndef SUDEKIMP_BLACKSMITH_UI_PRESENTER_H
#define SUDEKIMP_BLACKSMITH_UI_PRESENTER_H

#include "engine/blacksmith_ui_session.h"

#include <stdint.h>

enum {
    SUDEKIMP_BLACKSMITH_UI_VIEWPORT_WIDTH = 320u,
    SUDEKIMP_BLACKSMITH_UI_VIEWPORT_HEIGHT = 480u,
    SUDEKIMP_BLACKSMITH_UI_PRESENTATION_MAX_COMMANDS = 96u,
    SUDEKIMP_BLACKSMITH_UI_PRESENTATION_TEXT_CAPACITY = 56u
};

typedef enum SudekiMpBlacksmithUiDrawKind {
    SUDEKIMP_BLACKSMITH_UI_DRAW_RECTANGLE = 0,
    SUDEKIMP_BLACKSMITH_UI_DRAW_TEXT
} SudekiMpBlacksmithUiDrawKind;

typedef enum SudekiMpBlacksmithUiDrawRole {
    SUDEKIMP_BLACKSMITH_UI_ROLE_VIEWPORT_VEIL = 0,
    SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_SHADOW,
    SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_PANEL,
    SUDEKIMP_BLACKSMITH_UI_ROLE_WINDOW_BORDER,
    SUDEKIMP_BLACKSMITH_UI_ROLE_HEADER,
    SUDEKIMP_BLACKSMITH_UI_ROLE_SEAT_BADGE,
    SUDEKIMP_BLACKSMITH_UI_ROLE_TAB,
    SUDEKIMP_BLACKSMITH_UI_ROLE_ACTIVE_TAB,
    SUDEKIMP_BLACKSMITH_UI_ROLE_LIST_FRAME,
    SUDEKIMP_BLACKSMITH_UI_ROLE_ROW,
    SUDEKIMP_BLACKSMITH_UI_ROLE_SELECTED_ROW,
    SUDEKIMP_BLACKSMITH_UI_ROLE_DETAIL_FRAME,
    SUDEKIMP_BLACKSMITH_UI_ROLE_PRIMARY_TEXT,
    SUDEKIMP_BLACKSMITH_UI_ROLE_MUTED_TEXT,
    SUDEKIMP_BLACKSMITH_UI_ROLE_WARNING_TEXT,
    SUDEKIMP_BLACKSMITH_UI_ROLE_POSITIVE_TEXT
} SudekiMpBlacksmithUiDrawRole;

typedef struct SudekiMpBlacksmithUiDrawCommand {
    SudekiMpBlacksmithUiDrawKind kind;
    SudekiMpBlacksmithUiDrawRole role;
    int left;
    int top;
    int right;
    int bottom;
    uint32_t color;
    uint32_t text_scale;
    char text[SUDEKIMP_BLACKSMITH_UI_PRESENTATION_TEXT_CAPACITY];
} SudekiMpBlacksmithUiDrawCommand;

typedef struct SudekiMpBlacksmithUiViewportPresentation {
    uint32_t command_count;
    SudekiMpBlacksmithUiDrawCommand
        commands[SUDEKIMP_BLACKSMITH_UI_PRESENTATION_MAX_COMMANDS];
} SudekiMpBlacksmithUiViewportPresentation;

typedef struct SudekiMpBlacksmithUiPresentation {
    SudekiMpBlacksmithUiViewportPresentation
        viewports[SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT];
} SudekiMpBlacksmithUiPresentation;

/* Build two pointer-free, viewport-local draw lists.  This presenter is
 * deliberately preview-only: it rejects a snapshot that advertises a native
 * commit lane rather than accidentally presenting a mutable forge. */
int SudekiMpBlacksmithUiBuildPresentation(
    const SudekiMpBlacksmithUiSnapshot *snapshot,
    SudekiMpBlacksmithUiPresentation *presentation
);

#endif
