#include "ui/menu_button.h"

/* Original SudekiMP raster artwork, shared by the roster and title menu. */
static unsigned int roster_color_channel(
    unsigned int base,
    unsigned int cyan,
    unsigned int gold,
    unsigned int cyan_weight,
    unsigned int gold_weight
) {
    unsigned int value = base;
    value += cyan * cyan_weight / 255u;
    value += gold * gold_weight / 255u;
    return value > 255u ? 255u : value;
}

/* Return 0..4 covered quarter-pixel samples for a rounded rectangle.  The
 * stock title row is layered: a soft capsule-shaped shadow/rim surrounds a
 * tighter, squarer inset bar.  A shared coverage routine lets both contours
 * remain smooth without baking any original game artwork into the mod. */
unsigned int SudekiMpButtonCoverage(
    int pixel_x,
    int pixel_y,
    int left,
    int top,
    int right,
    int bottom,
    int radius
) {
    static const int sample_offsets[2] = {1, 3};
    const int center_y_x4 = (top + bottom) * 2;
    int radius_x4;
    int max_radius;
    int cap_left_x4;
    int cap_right_x4;
    int radius_squared;
    unsigned int coverage = 0u;
    int sample_y;

    if (right <= left || bottom <= top || radius <= 0) {
        return 0u;
    }
    max_radius = (bottom - top) / 2;
    if (radius > max_radius) {
        radius = max_radius;
    }
    if (radius > (right - left) / 2) {
        radius = (right - left) / 2;
    }
    radius_x4 = radius * 4;
    cap_left_x4 = left * 4 + radius_x4;
    cap_right_x4 = right * 4 - radius_x4;
    radius_squared = radius_x4 * radius_x4;
    for (sample_y = 0; sample_y < 2; ++sample_y) {
        const int y_x4 = pixel_y * 4 + sample_offsets[sample_y];
        const int delta_y = y_x4 - center_y_x4;
        int sample_x;
        for (sample_x = 0; sample_x < 2; ++sample_x) {
            const int x_x4 = pixel_x * 4 + sample_offsets[sample_x];
            int delta_x = 0;

            if (x_x4 < cap_left_x4) {
                delta_x = x_x4 - cap_left_x4;
            }
            else if (x_x4 > cap_right_x4) {
                delta_x = x_x4 - cap_right_x4;
            }
            if (delta_x * delta_x + delta_y * delta_y <= radius_squared) {
                ++coverage;
            }
        }
    }
    return coverage;
}

void SudekiMpDrawMenuButton(
    uint32_t *pixels, int pitch, int width, int height,
    int left, int top, int right, int bottom, int highlighted
) {
    const int border_inset = 2;
    const int inner_radius = 8;
    int y;
    int first_x;
    int last_x_exclusive;
    int first_y;
    int last_y_exclusive;

    if (!pixels || width < 1 || width > 4096 || height < 1 || height > 4096 ||
        pitch < width * 4 || left < 0 || top < 0 || right > width ||
        bottom > height || right - left < 32 || bottom - top != 30) return;

    first_x = left - 3;
    last_x_exclusive = right + 3;
    first_y = top - 1;
    last_y_exclusive = bottom + 4;
    if (first_x < 0) first_x = 0;
    if (last_x_exclusive > width) {
        last_x_exclusive = width;
    }
    if (first_y < 0) first_y = 0;
    if (last_y_exclusive > height) {
        last_y_exclusive = height;
    }

    for (y = first_y; y < last_y_exclusive; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
        int x;
        for (x = first_x; x < last_x_exclusive; ++x) {
            int local_x = x - left;
            unsigned int shadow_coverage = SudekiMpButtonCoverage(
                x, y, left - 2, top + 1, right + 2, bottom + 4, 16);
            unsigned int outer_coverage = SudekiMpButtonCoverage(
                x, y, left, top, right, bottom, 15);
            unsigned int inner_coverage;
            unsigned int vertical;
            unsigned int cyan_weight = 0u;
            unsigned int gold_weight = 0u;
            unsigned int fill_red;
            unsigned int fill_green;
            unsigned int fill_blue;
            unsigned int red;
            unsigned int green;
            unsigned int blue;
            unsigned int alpha;

            if (outer_coverage == 0u) {
                if (shadow_coverage != 0u) {
                    row[x] = ((82u * shadow_coverage / 4u) << 24) |
                        UINT32_C(0x00050406);
                }
                continue;
            }
            inner_coverage = SudekiMpButtonCoverage(
                x,
                y,
                left + border_inset,
                top + border_inset,
                right - border_inset,
                bottom - border_inset,
                inner_radius);
            vertical = (unsigned int)(bottom - y) * 24u /
                (unsigned int)(bottom - top);
            if (highlighted) {
                if (local_x < 120) {
                    cyan_weight = (unsigned int)(120 - local_x) * 190u / 120u;
                }
                if (local_x > right - left - 121) {
                    gold_weight = (unsigned int)(local_x -
                        (right - left - 121)) * 175u / 120u;
                }
            }
            fill_red = roster_color_channel(29u + vertical, 0u, 65u,
                cyan_weight, gold_weight);
            fill_green = roster_color_channel(27u + vertical, 105u, 49u,
                cyan_weight, gold_weight);
            fill_blue = roster_color_channel(30u + vertical, 118u, 0u,
                cyan_weight, gold_weight);
            if (y < top + 5) {
                fill_red = fill_red + 15u > 255u ? 255u : fill_red + 15u;
                fill_green = fill_green + 15u > 255u ? 255u :
                    fill_green + 15u;
                fill_blue = fill_blue + 15u > 255u ? 255u :
                    fill_blue + 15u;
            }
            else if (y >= bottom - 5) {
                fill_red = fill_red > 8u ? fill_red - 8u : 0u;
                fill_green = fill_green > 8u ? fill_green - 8u : 0u;
                fill_blue = fill_blue > 8u ? fill_blue - 8u : 0u;
            }

            /* Blend the antialiased inner contour against the dark outer
             * shell.  Fully covered inner pixels get the gradient; edge
             * samples keep a softly rounded, two-pixel frame. */
            red = (14u * (4u - inner_coverage) +
                fill_red * inner_coverage) / 4u;
            green = (13u * (4u - inner_coverage) +
                fill_green * inner_coverage) / 4u;
            blue = (15u * (4u - inner_coverage) +
                fill_blue * inner_coverage) / 4u;
            alpha = 244u * outer_coverage / 4u;
            row[x] = (alpha << 24) |
                (red << 16) | (green << 8) | blue;
        }
    }
}
