#include "engine/local_viewport_layout.h"

#include <stdint.h>
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

static uint8_t count_bits(uint8_t value) {
    uint8_t count = 0u;

    while (value != 0u) {
        count = (uint8_t)(count + (uint8_t)(value & 1u));
        value = (uint8_t)(value >> 1u);
    }
    return count;
}

static void controller_slots_for_mask(
    uint8_t mask,
    uint8_t slots[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS]
) {
    static const uint8_t physical_by_seat[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS] = {
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, 2u, 0u, 3u
    };
    unsigned int seat;

    for (seat = 0u; seat < SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS; ++seat) {
        slots[seat] = (mask & (uint8_t)(1u << seat)) != 0u ?
            physical_by_seat[seat] : SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    }
}

static uint64_t area(const SudekiMpLocalViewportRectangle *rectangle) {
    return (uint64_t)rectangle->width * (uint64_t)rectangle->height;
}

static int overlaps(
    const SudekiMpLocalViewportRectangle *left,
    const SudekiMpLocalViewportRectangle *right
) {
    return left->x < right->x + right->width &&
        right->x < left->x + left->width &&
        left->y < right->y + right->height &&
        right->y < left->y + left->height;
}

static void check_rectangle(
    const SudekiMpLocalViewportRectangle *rectangle,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height
) {
    CHECK(rectangle->x == x);
    CHECK(rectangle->y == y);
    CHECK(rectangle->width == width);
    CHECK(rectangle->height == height);
}

static void test_input_contract(void) {
    SudekiMpLocalSeatInputBinding binding;
    unsigned int seat_index;

    memset(&binding, 0, sizeof(binding));
    CHECK(SudekiMpLocalSeatResolveInput(
        0u, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &binding));
    CHECK(binding.source == SUDEKIMP_LOCAL_INPUT_KEYBOARD_MOUSE);
    CHECK(binding.controller_index ==
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER);
    for (seat_index = 1u;
         seat_index < SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS;
         ++seat_index) {
        CHECK(SudekiMpLocalSeatResolveInput(
            seat_index, (uint8_t)(3u - seat_index), &binding));
        CHECK(binding.source == SUDEKIMP_LOCAL_INPUT_CONTROLLER);
        CHECK(binding.controller_index == 3u - seat_index);
    }
    binding.source = SUDEKIMP_LOCAL_INPUT_CONTROLLER;
    binding.controller_index = 0u;
    CHECK(!SudekiMpLocalSeatResolveInput(4u, 0u, &binding));
    CHECK(binding.source == SUDEKIMP_LOCAL_INPUT_NONE);
    CHECK(binding.controller_index ==
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER);
    CHECK(!SudekiMpLocalSeatResolveInput(
        0u, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, NULL));
    CHECK(!SudekiMpLocalSeatResolveInput(0u, 0u, &binding));
    CHECK(!SudekiMpLocalSeatResolveInput(
        1u, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &binding));
}

static void test_exact_layouts(void) {
    SudekiMpLocalViewportLayout layout;
    uint8_t slots[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS];

    controller_slots_for_mask(0x01u, slots);
    CHECK(SudekiMpLocalViewportLayoutBuild(
        0x01u, slots, 1281u, 721u, &layout));
    CHECK(layout.viewport_count == 1u);
    check_rectangle(&layout.viewports[0].rectangle,
        0u, 0u, 1281u, 721u);

    controller_slots_for_mask(0x09u, slots);
    CHECK(SudekiMpLocalViewportLayoutBuild(
        0x09u, slots, 1281u, 721u, &layout));
    CHECK(layout.viewport_count == 2u);
    CHECK(layout.viewports[0].seat_index == 0u);
    CHECK(layout.viewports[1].seat_index == 3u);
    check_rectangle(&layout.viewports[0].rectangle,
        0u, 0u, 640u, 721u);
    check_rectangle(&layout.viewports[1].rectangle,
        640u, 0u, 641u, 721u);

    controller_slots_for_mask(0x0bu, slots);
    CHECK(SudekiMpLocalViewportLayoutBuild(
        0x0bu, slots, 1281u, 721u, &layout));
    CHECK(layout.viewport_count == 3u);
    CHECK(layout.viewports[0].seat_index == 0u);
    CHECK(layout.viewports[1].seat_index == 1u);
    CHECK(layout.viewports[2].seat_index == 3u);
    check_rectangle(&layout.viewports[0].rectangle,
        0u, 0u, 1281u, 360u);
    check_rectangle(&layout.viewports[1].rectangle,
        0u, 360u, 640u, 361u);
    check_rectangle(&layout.viewports[2].rectangle,
        640u, 360u, 641u, 361u);

    controller_slots_for_mask(0x0fu, slots);
    CHECK(SudekiMpLocalViewportLayoutBuild(
        0x0fu, slots, 1281u, 721u, &layout));
    CHECK(layout.viewport_count == 4u);
    check_rectangle(&layout.viewports[0].rectangle,
        0u, 0u, 640u, 360u);
    check_rectangle(&layout.viewports[1].rectangle,
        640u, 0u, 641u, 360u);
    check_rectangle(&layout.viewports[2].rectangle,
        0u, 360u, 640u, 361u);
    check_rectangle(&layout.viewports[3].rectangle,
        640u, 360u, 641u, 361u);
}

static void test_all_masks(void) {
    const uint32_t surface_width = 1279u;
    const uint32_t surface_height = 719u;
    unsigned int mask;

    for (mask = 0u; mask <= 0xffu; ++mask) {
        SudekiMpLocalViewportLayout layout;
        uint8_t slots[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS];
        const uint8_t active_human_mask = (uint8_t)mask;
        uint64_t total_area = 0u;
        unsigned int ordinal;
        unsigned int previous_seat = 0u;

        memset(&layout, 0xa5, sizeof(layout));
        controller_slots_for_mask(active_human_mask, slots);
        if (active_human_mask > SUDEKIMP_LOCAL_VIEWPORT_VALID_MASK ||
            (active_human_mask & 1u) == 0u) {
            SudekiMpLocalViewportLayout zero_layout;

            memset(&zero_layout, 0, sizeof(zero_layout));
            CHECK(!SudekiMpLocalViewportLayoutBuild(
                active_human_mask,
                slots,
                surface_width,
                surface_height,
                &layout));
            CHECK(memcmp(&layout, &zero_layout, sizeof(layout)) == 0);
            continue;
        }
        CHECK(SudekiMpLocalViewportLayoutBuild(
            active_human_mask,
            slots,
            surface_width,
            surface_height,
            &layout));
        CHECK(layout.active_human_mask == active_human_mask);
        CHECK(layout.viewport_count == count_bits(active_human_mask));
        CHECK(layout.surface_width == surface_width);
        CHECK(layout.surface_height == surface_height);
        for (ordinal = 0u; ordinal < layout.viewport_count; ++ordinal) {
            const SudekiMpLocalSeatViewport *viewport =
                &layout.viewports[ordinal];
            unsigned int other;

            CHECK(viewport->viewport_ordinal == ordinal);
            CHECK((active_human_mask &
                (uint8_t)(1u << viewport->seat_index)) != 0u);
            CHECK(ordinal == 0u || viewport->seat_index > previous_seat);
            CHECK(viewport->rectangle.width > 0u);
            CHECK(viewport->rectangle.height > 0u);
            CHECK(viewport->rectangle.x + viewport->rectangle.width <=
                surface_width);
            CHECK(viewport->rectangle.y + viewport->rectangle.height <=
                surface_height);
            if (viewport->seat_index == 0u) {
                CHECK(viewport->input.source ==
                    SUDEKIMP_LOCAL_INPUT_KEYBOARD_MOUSE);
            } else {
                CHECK(viewport->input.source ==
                    SUDEKIMP_LOCAL_INPUT_CONTROLLER);
                CHECK(viewport->input.controller_index ==
                    slots[viewport->seat_index]);
            }
            for (other = 0u; other < ordinal; ++other) {
                CHECK(!overlaps(&viewport->rectangle,
                    &layout.viewports[other].rectangle));
            }
            total_area += area(&viewport->rectangle);
            previous_seat = viewport->seat_index;
        }
        CHECK(total_area ==
            (uint64_t)surface_width * (uint64_t)surface_height);
    }
}

static void test_invalid_inputs(void) {
    SudekiMpLocalViewportLayout layout;
    SudekiMpLocalViewportLayout zero_layout;
    uint8_t slots[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS];

    memset(&zero_layout, 0, sizeof(zero_layout));
    memset(&layout, 0xa5, sizeof(layout));
    controller_slots_for_mask(0x01u, slots);
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x00u, slots, 640u, 480u, &layout));
    CHECK(memcmp(&layout, &zero_layout, sizeof(layout)) == 0);
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x02u, slots, 640u, 480u, &layout));
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x11u, slots, 640u, 480u, &layout));
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x01u, slots, 0u, 480u, &layout));
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x01u, slots, 640u, 0u, &layout));
    controller_slots_for_mask(0x03u, slots);
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x03u, slots, 1u, 480u, &layout));
    controller_slots_for_mask(0x07u, slots);
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x07u, slots, 640u, 1u, &layout));
    controller_slots_for_mask(0x0fu, slots);
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x0fu, slots, 1u, 1u, &layout));
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x01u, NULL, 640u, 480u, &layout));
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x01u, slots, 640u, 480u, NULL));

    controller_slots_for_mask(0x03u, slots);
    slots[1] = SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x03u, slots, 640u, 480u, &layout));
    controller_slots_for_mask(0x07u, slots);
    slots[2] = slots[1];
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x07u, slots, 640u, 480u, &layout));
    controller_slots_for_mask(0x01u, slots);
    slots[3] = 0u;
    CHECK(!SudekiMpLocalViewportLayoutBuild(
        0x01u, slots, 640u, 480u, &layout));
}

static void test_activation_policy(void) {
    unsigned int mask;

    for (mask = 0u; mask <= 0xffu; ++mask) {
        const int valid = mask <= SUDEKIMP_LOCAL_VIEWPORT_VALID_MASK &&
            (mask & 1u) != 0u;

        CHECK(SudekiMpLocalViewportActivationPolicy(
            1, mask, 1, mask, mask, mask, mask, mask, mask, 1) == valid);
        CHECK(!SudekiMpLocalViewportActivationPolicy(
            0, mask, 1, mask, mask, mask, mask, mask, mask, 1));
    }
    for (mask = 1u; mask <= SUDEKIMP_LOCAL_VIEWPORT_VALID_MASK; mask += 2u) {
        unsigned int bit;

        CHECK(!SudekiMpLocalViewportActivationPolicy(
            1, mask, 0, mask, mask, mask, mask, mask, mask, 1));
        CHECK(!SudekiMpLocalViewportActivationPolicy(
            1, mask, 1, mask, mask, mask, mask, mask, mask, 0));
        for (bit = 1u; bit <= 8u; bit <<= 1u) {
            if ((mask & bit) == 0u) continue;
            CHECK(!SudekiMpLocalViewportActivationPolicy(
                1, mask, 1, mask & ~bit, mask, mask, mask, mask, mask, 1));
            CHECK(!SudekiMpLocalViewportActivationPolicy(
                1, mask, 1, mask, mask & ~bit, mask, mask, mask, mask, 1));
            CHECK(!SudekiMpLocalViewportActivationPolicy(
                1, mask, 1, mask, mask, mask & ~bit, mask, mask, mask, 1));
            CHECK(!SudekiMpLocalViewportActivationPolicy(
                1, mask, 1, mask, mask, mask, mask & ~bit, mask, mask, 1));
            CHECK(!SudekiMpLocalViewportActivationPolicy(
                1, mask, 1, mask, mask, mask, mask, mask & ~bit, mask, 1));
            CHECK(!SudekiMpLocalViewportActivationPolicy(
                1, mask, 1, mask, mask, mask, mask, mask, mask & ~bit, 1));
        }
    }
}

static void test_render_seat_scheduler(void) {
    uint8_t next = 0u;
    unsigned int current;

    CHECK(SudekiMpLocalViewportNextRenderSeat(
        0x01u, 0u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(next == 0u);
    CHECK(SudekiMpLocalViewportNextRenderSeat(
        0x07u, 0u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(next == 1u);
    CHECK(SudekiMpLocalViewportNextRenderSeat(
        0x07u, 1u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(next == 2u);
    CHECK(SudekiMpLocalViewportNextRenderSeat(
        0x07u, 2u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(next == 0u);
    CHECK(SudekiMpLocalViewportNextRenderSeat(
        0x0bu, 1u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(next == 3u);
    CHECK(SudekiMpLocalViewportNextRenderSeat(
        0x0bu, 3u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(next == 0u);
    for (current = 0u; current < 4u; ++current) {
        CHECK(SudekiMpLocalViewportNextRenderSeat(
            0x07u, (uint8_t)current, 1, 2u, &next));
        CHECK(next == 2u);
    }
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x07u, 0u, 1, 3u, &next));
    CHECK(next == SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER);
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x07u, 0u, 2, 2u, &next));
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x07u, 0u, 0, 0u, &next));
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x06u, 1u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x17u, 1u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x07u, 4u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, &next));
    CHECK(!SudekiMpLocalViewportNextRenderSeat(
        0x07u, 0u, 0, SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER, NULL));
}

int main(void) {
    test_input_contract();
    test_exact_layouts();
    test_all_masks();
    test_invalid_inputs();
    test_activation_policy();
    test_render_seat_scheduler();

    if (failures != 0) {
        fprintf(stderr, "local viewport layout tests failed: %d\n",
            failures);
        return 1;
    }
    puts("local viewport layout tests passed");
    return 0;
}
