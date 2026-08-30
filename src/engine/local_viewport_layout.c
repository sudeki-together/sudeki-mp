#include "engine/local_viewport_layout.h"

#include <stddef.h>
#include <string.h>

static uint8_t active_seat_count(uint8_t active_human_mask) {
    uint8_t count = 0u;
    unsigned int seat_index;

    for (seat_index = 0u;
         seat_index < SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS;
         ++seat_index) {
        if ((active_human_mask & (uint8_t)(1u << seat_index)) != 0u) {
            ++count;
        }
    }
    return count;
}

int SudekiMpLocalSeatResolveInput(
    unsigned int seat_index,
    uint8_t controller_slot,
    SudekiMpLocalSeatInputBinding *binding
) {
    if (binding == NULL) {
        return 0;
    }
    memset(binding, 0, sizeof(*binding));
    binding->controller_index = SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    if (seat_index >= SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS) {
        return 0;
    }
    if (seat_index == SUDEKIMP_LOCAL_VIEWPORT_HOST_SEAT) {
        if (controller_slot != SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER) {
            return 0;
        }
        binding->source = SUDEKIMP_LOCAL_INPUT_KEYBOARD_MOUSE;
        return 1;
    }
    if (controller_slot >= SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS) {
        return 0;
    }
    binding->source = SUDEKIMP_LOCAL_INPUT_CONTROLLER;
    binding->controller_index = controller_slot;
    return 1;
}

static int surface_supports_count(
    uint8_t viewport_count,
    uint32_t surface_width,
    uint32_t surface_height
) {
    if (surface_width == 0u || surface_height == 0u) {
        return 0;
    }
    if (viewport_count == 2u && surface_width < 2u) {
        return 0;
    }
    if (viewport_count >= 3u &&
        (surface_width < 2u || surface_height < 2u)) {
        return 0;
    }
    return viewport_count >= 1u &&
        viewport_count <= SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS;
}

static SudekiMpLocalViewportRectangle rectangle_for_ordinal(
    uint8_t viewport_count,
    uint8_t ordinal,
    uint32_t surface_width,
    uint32_t surface_height
) {
    const uint32_t left_width = surface_width / 2u;
    const uint32_t right_width = surface_width - left_width;
    const uint32_t top_height = surface_height / 2u;
    const uint32_t bottom_height = surface_height - top_height;
    SudekiMpLocalViewportRectangle rectangle;

    memset(&rectangle, 0, sizeof(rectangle));
    if (viewport_count == 1u) {
        rectangle.width = surface_width;
        rectangle.height = surface_height;
    } else if (viewport_count == 2u) {
        rectangle.x = ordinal == 0u ? 0u : left_width;
        rectangle.width = ordinal == 0u ? left_width : right_width;
        rectangle.height = surface_height;
    } else if (viewport_count == 3u && ordinal == 0u) {
        rectangle.width = surface_width;
        rectangle.height = top_height;
    } else if (viewport_count == 3u) {
        rectangle.x = ordinal == 1u ? 0u : left_width;
        rectangle.y = top_height;
        rectangle.width = ordinal == 1u ? left_width : right_width;
        rectangle.height = bottom_height;
    } else {
        const uint8_t column = (uint8_t)(ordinal % 2u);
        const uint8_t row = (uint8_t)(ordinal / 2u);

        rectangle.x = column == 0u ? 0u : left_width;
        rectangle.y = row == 0u ? 0u : top_height;
        rectangle.width = column == 0u ? left_width : right_width;
        rectangle.height = row == 0u ? top_height : bottom_height;
    }
    return rectangle;
}

int SudekiMpLocalViewportLayoutBuild(
    uint8_t active_human_mask,
    const uint8_t controller_slot_by_seat[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS],
    uint32_t surface_width,
    uint32_t surface_height,
    SudekiMpLocalViewportLayout *layout
) {
    uint8_t viewport_count;
    uint8_t ordinal = 0u;
    unsigned int seat_index;

    if (layout == NULL) {
        return 0;
    }
    memset(layout, 0, sizeof(*layout));
    if (controller_slot_by_seat == NULL ||
        (active_human_mask & 1u) == 0u ||
        (active_human_mask &
            (uint8_t)~SUDEKIMP_LOCAL_VIEWPORT_VALID_MASK) != 0u) {
        return 0;
    }
    viewport_count = active_seat_count(active_human_mask);
    if (!surface_supports_count(
            viewport_count, surface_width, surface_height)) {
        return 0;
    }
    layout->active_human_mask = active_human_mask;
    layout->viewport_count = viewport_count;
    layout->surface_width = surface_width;
    layout->surface_height = surface_height;
    for (seat_index = 0u;
         seat_index < SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS;
         ++seat_index) {
        SudekiMpLocalSeatViewport *viewport;

        if ((active_human_mask & (uint8_t)(1u << seat_index)) == 0u) {
            if (controller_slot_by_seat[seat_index] !=
                    SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER) {
                memset(layout, 0, sizeof(*layout));
                return 0;
            }
            continue;
        }
        viewport = &layout->viewports[ordinal];
        viewport->seat_index = (uint8_t)seat_index;
        viewport->viewport_ordinal = ordinal;
        if (!SudekiMpLocalSeatResolveInput(
                seat_index,
                controller_slot_by_seat[seat_index],
                &viewport->input)) {
            memset(layout, 0, sizeof(*layout));
            return 0;
        }
        if (viewport->input.source == SUDEKIMP_LOCAL_INPUT_CONTROLLER) {
            unsigned int prior;

            for (prior = 0u; prior < ordinal; ++prior) {
                if (layout->viewports[prior].input.source ==
                        SUDEKIMP_LOCAL_INPUT_CONTROLLER &&
                    layout->viewports[prior].input.controller_index ==
                        viewport->input.controller_index) {
                    memset(layout, 0, sizeof(*layout));
                    return 0;
                }
            }
        }
        viewport->rectangle = rectangle_for_ordinal(
            viewport_count,
            ordinal,
            surface_width,
            surface_height
        );
        ++ordinal;
    }
    return ordinal == viewport_count;
}

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
) {
    const unsigned int valid_seat_mask =
        SUDEKIMP_LOCAL_VIEWPORT_VALID_MASK;

    if (!feature_enabled || !layout_ready || !global_presentation_clear ||
        active_human_mask == 0u ||
        (active_human_mask & 1u) == 0u ||
        (active_human_mask & ~valid_seat_mask) != 0u) {
        return 0;
    }
    return actor_lease_mask == active_human_mask &&
        camera_lease_mask == active_human_mask &&
        render_state_lease_mask == active_human_mask &&
        hud_lease_mask == active_human_mask &&
        input_lease_mask == active_human_mask &&
        frame_cache_ready_mask == active_human_mask;
}
