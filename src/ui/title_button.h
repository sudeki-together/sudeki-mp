#ifndef SUDEKIMP_TITLE_BUTTON_H
#define SUDEKIMP_TITLE_BUTTON_H

#include <stdint.h>

/* Original procedural artwork. No native pointers, game assets, device or
 * clock are owned here. Keep the older roster's menu_button renderer separate.
 * Rasterize these four layers once; animate cached textures with Sample(). */
typedef enum SudekiMpTitleButtonLayer {
    SUDEKIMP_TITLE_BUTTON_IDLE,
    SUDEKIMP_TITLE_BUTTON_HIGHLIGHT,
    SUDEKIMP_TITLE_BUTTON_CYAN,
    SUDEKIMP_TITLE_BUTTON_GLEAM
} SudekiMpTitleButtonLayer;

typedef enum SudekiMpTitleButtonState {
    SUDEKIMP_TITLE_BUTTON_REST,
    SUDEKIMP_TITLE_BUTTON_FOCUS,
    SUDEKIMP_TITLE_BUTTON_CONFIRM,
    SUDEKIMP_TITLE_BUTTON_RETURN
} SudekiMpTitleButtonState;

typedef struct SudekiMpTitleButtonPose {
    float scale_x, scale_y;
    /* Vertical displacement in units of the unscaled button height. */
    float offset_y;
    float opacity;
    float highlight;
    float confirmation;
    /* Label follows return motion, but does NOT stretch with the cyan pulse. */
    float label_scale;
    /* Separate translucent sheen over the highlighted capsule. Render at
     * these scales, multiplied by opacity, after the base and before text. */
    float gleam_scale, gleam_opacity;
    /* Wrapped diagonal texture offset, 0..1, independent of the scale pulse. */
    float gleam_phase;
    int settled;
} SudekiMpTitleButtonPose;

/* Elapsed monotonic seconds since entry into state, never frame counts.
 * Confirmation is fitted to recorded bounds, not a recovered native constant.
 * Focus repeats a small scale pulse; its timing and layer phase are visual
 * fits, not a copy of native authored animation curves.
 * Return is a visual approximation of one row's scale/overshoot; the full
 * title scene and its staggered row animation remain the caller's concern.
 * Returns zero on invalid input without modifying the output. */
int SudekiMpTitleButtonSample(SudekiMpTitleButtonState state, double seconds,
    SudekiMpTitleButtonPose *pose);

/* Write one transparent, straight-alpha ARGB8 layer over the entire surface.
 * This is an image generator, NOT an alpha-over operation on existing pixels.
 * Pitch is bytes, 4-byte aligned. Bounds describe the capsule excluding glow.
 * Nominal aspect ratio is 13.3:1. Reserve at least 0.3 * height outside it for
 * the soft rim/shadow. Rasterization is for texture creation, not every frame.
 * Returns zero on invalid input without modifying the surface. */
int SudekiMpTitleButtonRaster(uint32_t *pixels, int pitch, int width, int height,
    float left, float top, float right, float bottom,
    SudekiMpTitleButtonLayer layer);

/* Gleam at a wrapped texture offset (0..1 inclusive). Other layers ignore it.
 * For cached animation frames or offline previews; do not regenerate this
 * software texture per render tick. Phase 0 and 1 join continuously. */
int SudekiMpTitleButtonRasterPhase(uint32_t *pixels, int pitch, int width, int height,
    float left, float top, float right, float bottom,
    SudekiMpTitleButtonLayer layer, float gleam_phase);

#endif
