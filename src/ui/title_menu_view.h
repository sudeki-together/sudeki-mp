#ifndef SUDEKIMP_TITLE_MENU_VIEW_H
#define SUDEKIMP_TITLE_MENU_VIEW_H
#include <windows.h>
#include <stdint.h>
#include "ui/title_button.h"

enum {
    SUDEKIMP_TITLE_MAX_ROWS = 6,
    SUDEKIMP_TITLE_CANVAS_WIDTH = 960,
    SUDEKIMP_TITLE_CANVAS_HEIGHT = 720,
    SUDEKIMP_TITLE_FIRST_ROW = 284,
    SUDEKIMP_TITLE_ROW_PITCH = 46,
    SUDEKIMP_TITLE_ROW_HEIGHT = 39,
    SUDEKIMP_TITLE_LEFT = 220,
    SUDEKIMP_TITLE_RIGHT = 740
};
typedef enum SudekiMpTitleLabel {
    SUDEKIMP_TITLE_CONTINUE, SUDEKIMP_TITLE_NEW_GAME, SUDEKIMP_TITLE_MULTIPLAYER,
    SUDEKIMP_TITLE_OPTIONS, SUDEKIMP_TITLE_CREDITS, SUDEKIMP_TITLE_QUIT,
    SUDEKIMP_TITLE_CREATE, SUDEKIMP_TITLE_JOIN, SUDEKIMP_TITLE_BACK,
    SUDEKIMP_TITLE_LABEL_COUNT
} SudekiMpTitleLabel;
enum { SUDEKIMP_PANEL_CONTROLS = 24, SUDEKIMP_PANEL_TEXTS = 24 };
typedef enum SudekiMpPanelKind {
    SUDEKIMP_PANEL_NAV, SUDEKIMP_PANEL_FIELD, SUDEKIMP_PANEL_BUTTON,
    SUDEKIMP_PANEL_SERVER, SUDEKIMP_PANEL_MEMBER, SUDEKIMP_PANEL_SAVE,
    SUDEKIMP_PANEL_PORTRAIT, SUDEKIMP_PANEL_CHARACTER
} SudekiMpPanelKind;
typedef struct SudekiMpPanelControl {
    float x, y, width, height;
    SudekiMpPanelKind kind;
    BOOL active;
    /* Optional borrowed IDirect3DTexture9. The native owner must hold a
     * verified lease for this draw; the view never retains or releases it.
     * NULL draws a styled name tile while native residency is unavailable. */
    void *texture;
    /* PORTRAIT only: lobby character ID, used by the native draw-time
     * provider. Labels and tile order do not identify native resources. */
    unsigned portrait_character;
    char label[80], value[96], detail[96];
} SudekiMpPanelControl;
typedef struct SudekiMpPanelText {
    float x, y, size, width;
    char text[128];
} SudekiMpPanelText;
enum { SUDEKIMP_OVERLAY_AVATAR_CARDS=4 };
typedef struct SudekiMpOverlayAvatarCard {
    /* right/y anchor in full-viewport 960x720 units; card dimensions and
     * lettering scale uniformly, keeping portrait tiles square. */
    float right,y,width,height,hp_fraction,sp_fraction;
    unsigned player;
    BOOL local;
    /* Optional native texture borrowed for this draw only. */
    void *portrait;
    char name[32],avatar[16],hp[96],sp[96];
} SudekiMpOverlayAvatarCard;
typedef struct SudekiMpTitleExtras {
    unsigned informational_rows;
    char rows[SUDEKIMP_TITLE_MAX_ROWS][80];
    char heading[80], hint[128], status[128];
    BOOL panel;
    /* Gameplay overlay: text uses full-viewport 960x720 coordinates; lettering
     * scales uniformly. Optional avatar cards are separate from native party
     * HUD slots. No interactive buttons or pointer are drawn in this mode. */
    BOOL overlay;
    float overlay_letterbox,overlay_letterbox_bottom; /* Independent normalized bar heights. */
    unsigned avatar_card_count;
    SudekiMpOverlayAvatarCard avatar_cards[SUDEKIMP_OVERLAY_AVATAR_CARDS];
    BOOL full_width; /* Panel content uses the sidebar space as well. */
    BOOL save_details; /* Save list/details layout with a central divider. */
    unsigned text_count;
    SudekiMpPanelControl controls[SUDEKIMP_PANEL_CONTROLS];
    SudekiMpPanelText texts[SUDEKIMP_PANEL_TEXTS];
} SudekiMpTitleExtras;
/* Render-thread only. Warm during the native intro, before its root entrance.
 * TRUE means all required button states and lettering are ready; the optional
 * gleam cache warms incrementally. FALSE leaves the stock UI visible/usable.
 * Call once before the native draw, then Draw after its matching flush. */
BOOL SudekiMpTitleViewPrepare(void *device);
BOOL SudekiMpTitleViewDraw(void *device, unsigned count, unsigned selected,
    unsigned enabled_mask, const SudekiMpTitleLabel *labels,
    SudekiMpTitleButtonState state, double seconds, float opacity, HWND *window,
    const SudekiMpTitleExtras *extras);
BOOL SudekiMpTitleViewRelease(void);
/* Retry a failed state restoration even after leaving the custom page. */
BOOL SudekiMpTitleViewRestore(void);
BOOL SudekiMpTitleViewHit(HWND window, unsigned count, unsigned *row,
    POINT *client_point);
BOOL SudekiMpTitlePanelHit(HWND window, unsigned count,
    const SudekiMpTitleExtras *panel, unsigned *row, POINT *client_point);
BOOL SudekiMpTitlePanelHitPoint(HWND window, unsigned count,
    const SudekiMpTitleExtras *panel, POINT client_point, unsigned *row);
/* Drawn only at the verified title flush; retains/restores the same D3D lease. */
BOOL SudekiMpTitleViewFade(void *device, float opacity);
/* Existing atlas only; returns -1 for unsupported glyphs or missing NUL. */
float SudekiMpTitleViewTextWidth(const char *text,unsigned capacity,float size);
#endif
