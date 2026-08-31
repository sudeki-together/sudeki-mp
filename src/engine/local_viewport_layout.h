#ifndef SUDEKIMP_LOCAL_VIEWPORT_LAYOUT_H
#define SUDEKIMP_LOCAL_VIEWPORT_LAYOUT_H

#include <stdint.h>

enum {
    SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS = 4u,
    SUDEKIMP_LOCAL_VIEWPORT_HOST_SEAT = 0u,
    SUDEKIMP_LOCAL_VIEWPORT_VALID_MASK = 0x0fu,
    SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER = 0xffu
};

typedef enum SudekiMpLocalInputSource {
    SUDEKIMP_LOCAL_INPUT_NONE = 0,
    SUDEKIMP_LOCAL_INPUT_KEYBOARD_MOUSE,
    SUDEKIMP_LOCAL_INPUT_CONTROLLER
} SudekiMpLocalInputSource;

typedef struct SudekiMpLocalSeatInputBinding {
    SudekiMpLocalInputSource source;
    uint8_t controller_index;
} SudekiMpLocalSeatInputBinding;

typedef struct SudekiMpLocalViewportRectangle {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} SudekiMpLocalViewportRectangle;

typedef struct SudekiMpLocalSeatViewport {
    uint8_t seat_index;
    uint8_t viewport_ordinal;
    SudekiMpLocalSeatInputBinding input;
    SudekiMpLocalViewportRectangle rectangle;
} SudekiMpLocalSeatViewport;

/* Camera, movement-transform, and orbit ownership follow seat_index, never
 * the compact viewport_ordinal. This keeps P1-P4 view/input bases stable when
 * a future local session uses a gapped human-seat mask. */

typedef struct SudekiMpLocalViewportLayout {
    uint8_t active_human_mask;
    uint8_t viewport_count;
    uint32_t surface_width;
    uint32_t surface_height;
    SudekiMpLocalSeatViewport
        viewports[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS];
} SudekiMpLocalViewportLayout;

/* The input source belongs to the stable seat, not its compact viewport
 * position. P1 requires NO_CONTROLLER and uses keyboard/mouse. Every active
 * companion seat receives the physical XInput/transport slot captured in the
 * immutable encounter assignment; it is never inferred from seat ordinal. */
int SudekiMpLocalSeatResolveInput(
    unsigned int seat_index,
    uint8_t controller_slot,
    SudekiMpLocalSeatInputBinding *binding
);

/* Build exact pixel rectangles for the active humans. controller_slot_by_seat
 * is copied from the immutable encounter assignment: P1 and inactive seats
 * must be NO_CONTROLLER, while active P2-P4 entries must be distinct slots
 * 0..3. Bit zero (the Tal host) is mandatory and bits above P4 are rejected.
 * Gapped masks are valid: occupied seats are emitted in ascending seat order
 * and packed into layout positions without changing their input bindings.
 *
 * One human receives the full surface.  Two humans receive left/right
 * halves.  With three humans P1 is top-wide and the other occupied seats are
 * bottom-left/bottom-right.  Four humans receive a row-major 2x2 grid.  Odd
 * dimensions are assigned to the right/bottom rectangles so every pixel is
 * covered exactly once.  On failure, a non-null output is cleared. */
int SudekiMpLocalViewportLayoutBuild(
    uint8_t active_human_mask,
    const uint8_t controller_slot_by_seat[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS],
    uint32_t surface_width,
    uint32_t surface_height,
    SudekiMpLocalViewportLayout *layout
);

/* Pure activation policy shared by the dormant adaptive compositor and its
 * standalone Windows regression. Every ownership/readiness mask must equal
 * the exact active seat mask; stale inactive-seat resources are rejected. */
int SudekiMpLocalViewportActivationPolicy(
    int feature_enabled,
    unsigned int active_human_mask,
    int layout_ready,
    unsigned int actor_lease_mask,
    unsigned int camera_lease_mask,
    unsigned int render_state_lease_mask,
    unsigned int hud_lease_mask,
    unsigned int input_lease_mask,
    unsigned int frame_cache_ready_mask,
    int global_presentation_clear
);

/* Select one stable seat for the next native full-frame render pass.  The
 * ordinary path advances through active seat indices in ascending order and
 * wraps.  A serialized owner-pinned presentation (for example QuickMenu)
 * keeps rendering the exact owner seat until the caller releases the pin.
 * The compact viewport ordinal is deliberately never used as ownership. */
int SudekiMpLocalViewportNextRenderSeat(
    uint8_t active_human_mask,
    uint8_t current_seat,
    int owner_pin_active,
    uint8_t owner_seat,
    uint8_t *next_seat
);

#endif
