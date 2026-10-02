#include "ui/title_button.h"
#include <math.h>
#include <stddef.h>

typedef struct ButtonColor {
    float r, g, b, a; /* Premultiplied while composing; exported straight. */
} ButtonColor;

typedef struct ButtonKey {
    float seconds, x, y, offset, opacity;
} ButtonKey;

static float clamp01(float value) {
    return value < 0 ? 0 : value > 1 ? 1 : value;
}

static float mix(float a, float b, float t) { return a + (b - a) * t; }

static float bell(float value, float spread) {
    float t = value / spread;
    return expf(-t * t);
}

/* Deterministic, original cloud artwork. This is evaluated only while creating
 * cached textures. Broad warped noise produces translucent marbling across the
 * face; narrow edge glints alone cannot represent that material. */
static float noise_corner(int x, int y) {
    uint32_t hash = (uint32_t)x * UINT32_C(0x9e3779b1) ^
        (uint32_t)y * UINT32_C(0x85ebca77) ^ UINT32_C(0xc2b2ae35);
    hash ^= hash >> 16;
    hash *= UINT32_C(0x7feb352d);
    hash ^= hash >> 15;
    return (float)(hash & 65535u) / 65535.0f;
}

static float cloud_noise(float x, float y, int period_x, int period_y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    int x0 = (ix % period_x + period_x) % period_x;
    int y0 = (iy % period_y + period_y) % period_y;
    int x1 = (x0 + 1) % period_x, y1 = (y0 + 1) % period_y;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    return mix(mix(noise_corner(x0, y0), noise_corner(x1, y0), fx),
        mix(noise_corner(x0, y1), noise_corner(x1, y1), fx), fy);
}

static float cloud_gleam(float u, float v) {
    float x = u * 6.0f;
    float y = (v + 1) * 1.5f;
    float warp = cloud_noise(x + 3.7f, y + 8.1f, 6, 3) - .5f;
    float ribbon = y + .62f * sinf(u * 12.5663706f) + warp * 1.5f;
    float broad = cloud_noise(x + warp * .8f, ribbon, 6, 3);
    float detail = cloud_noise(x * 2 + 11.4f, ribbon * 2 + 5.2f, 12, 6);
    float grain = cloud_noise(x * 4 + 27, ribbon * 4 + 19, 24, 12);
    float value = .62f * broad + .28f * detail + .10f * grain;
    float ridge = bell(value - .52f, .11f);
    float density = clamp01((value - .25f) * 1.7f);
    return clamp01(.50f * density + .50f * ridge);
}

/* The native highlighted row keeps moving after its initial color switch.
 * About two percent scale travel, roughly 0.83 s per pulse in the reference.
 * Use an original smooth fit; preserve phase for arbitrarily long focus. */
static float focus_scale(double seconds, double phase) {
    double cycle = fmod(seconds + phase, 5.0 / 6.0) / (5.0 / 6.0);
    return .99f - .01f * (float)cos(cycle * 6.283185307179586);
}

static void focus_pose(double seconds, SudekiMpTitleButtonPose *pose) {
    pose->scale_x = pose->scale_y = focus_scale(seconds, 0);
    pose->label_scale = focus_scale(seconds, .06);
    pose->gleam_scale = focus_scale(seconds, -.02);
    pose->gleam_opacity = pose->highlight = 1;
    /* A full diagonal texture repeat over five small scale pulses. Native
     * material tracks establish U/V travel; seconds remain a capture fit. */
    pose->gleam_phase = (float)(fmod(seconds, 25.0 / 6.0) / (25.0 / 6.0));
    pose->settled = 0; /* Focus is an ongoing animation, not a static endpoint. */
}

static void over(ButtonColor *dst, float r, float g, float b, float alpha) {
    float a = clamp01(alpha), keep = 1 - a;
    dst->r = r * a + dst->r * keep;
    dst->g = g * a + dst->g * keep;
    dst->b = b * a + dst->b * keep;
    dst->a = a + dst->a * keep;
}

static unsigned byte(float value) {
    return (unsigned)(clamp01(value) * 255 + .5f);
}

static uint32_t pack(ButtonColor color) {
    if (color.a < .5f / 255) return 0;
    return (byte(color.a) << 24) | (byte(color.r / color.a) << 16) |
        (byte(color.g / color.a) << 8) | byte(color.b / color.a);
}

static void sample_keys(const ButtonKey *keys, size_t count, float seconds,
    SudekiMpTitleButtonPose *pose) {
    size_t next = 1;
    float t;
    const ButtonKey *a, *b;
    while (next < count - 1 && seconds > keys[next].seconds) ++next;
    a = &keys[next - 1];
    b = &keys[next];
    t = clamp01((seconds - a->seconds) / (b->seconds - a->seconds));
    pose->scale_x = mix(a->x, b->x, t);
    pose->scale_y = mix(a->y, b->y, t);
    pose->offset_y = mix(a->offset, b->offset, t);
    pose->opacity = mix(a->opacity, b->opacity, t);
    pose->settled = seconds >= keys[count - 1].seconds;
}

int SudekiMpTitleButtonSample(SudekiMpTitleButtonState state, double seconds,
    SudekiMpTitleButtonPose *pose) {
    /* Sampled width/height ratios, with linear interpolation between observed
     * bounds. Confirmation is separate from the much smaller focus pulse. */
    static const ButtonKey confirm[] = {
        { .000f, 1.026f, 1.024f, 0, 1 },
        { .017f, 1.079f, 1.073f, 0, 1 },
        { .050f, 1.129f, 1.122f, 0, 1 },
        { .067f, 1.159f, 1.146f, 0, 1 },
        { .084f, 1.185f, 1.195f, 0, 1 },
        { .100f, 1.197f, 1.195f, 0, 1 },
        { .134f, 1.191f, 1.195f, 0, 1 },
        { .150f, 1.184f, 1.195f, 0, 1 },
        { .167f, 1.155f, 1.146f, 0, 1 },
        { .184f, 1.122f, 1.122f, 0, 1 },
        { .217f, 1.103f, 1.098f, 0, 1 },
        { .234f, 1.066f, 1.073f, 0, 1 },
        { .250f, 1.052f, 1.024f, 0, 1 },
        { .267f, 1.021f, 1.024f, 0, 1 },
        { .300f, 1.015f, 1.024f, 0, 1 },
        { .317f, 1.000f, 1.000f, 0, 1 },
        { .417f, 1.000f, 1.000f, 0, 1 }
    };
    /* Approximation for the review component, not the native scene contract.
     * Final title integration must align with the scene's own row transforms. */
    static const ButtonKey returning[] = {
        { .000f, .440f, .440f, 5.00f, 0 },
        { .100f, .580f, .580f, 3.00f, .80f },
        { .200f, .710f, .710f, 1.00f, 1 },
        { .300f, .810f, .810f, -.70f, 1 },
        { .400f, .860f, .860f, -.45f, 1 },
        { .500f, .940f, .940f, -.15f, 1 },
        { .600f, 1.00f, 1.00f, 0, 1 },
        { .800f, 1.00f, 1.00f, 0, 1 }
    };
    SudekiMpTitleButtonPose result = {1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1};
    float elapsed;
    if (!pose || !isfinite(seconds) || seconds < 0 ||
        state < SUDEKIMP_TITLE_BUTTON_REST ||
        state > SUDEKIMP_TITLE_BUTTON_RETURN) return 0;
    /* Clamp before converting double to float. Stable states may last days. */
    elapsed = (float)(seconds > 60 ? 60 : seconds);
    switch (state) {
    case SUDEKIMP_TITLE_BUTTON_REST: break;
    case SUDEKIMP_TITLE_BUTTON_FOCUS: focus_pose(seconds, &result); break;
    case SUDEKIMP_TITLE_BUTTON_CONFIRM:
        sample_keys(confirm, sizeof(confirm) / sizeof(confirm[0]), elapsed, &result);
        result.confirmation = 1;
        break;
    case SUDEKIMP_TITLE_BUTTON_RETURN:
        sample_keys(returning, sizeof(returning) / sizeof(returning[0]), elapsed, &result);
        result.label_scale = result.scale_y;
        if (seconds >= .8) focus_pose(seconds - .8, &result);
        break;
    }
    *pose = result;
    return 1;
}

int SudekiMpTitleButtonRaster(uint32_t *pixels, int pitch, int width, int height,
    float left, float top, float right, float bottom,
    SudekiMpTitleButtonLayer layer) {
    return SudekiMpTitleButtonRasterPhase(pixels, pitch, width, height,
        left, top, right, bottom, layer, 0);
}

int SudekiMpTitleButtonRasterPhase(uint32_t *pixels, int pitch, int width, int height,
    float left, float top, float right, float bottom,
    SudekiMpTitleButtonLayer layer, float gleam_phase) {
    float cx, cy, radius, segment, unit;
    int x, y;
    if (!isfinite(gleam_phase) || gleam_phase < 0 || gleam_phase > 1 ||
        !pixels || (uintptr_t)pixels % 4 || width < 1 || width > 4096 ||
        height < 1 || height > 4096 || pitch < width * 4 || pitch > 65536 ||
        pitch % 4 || !isfinite(left) || !isfinite(top) || !isfinite(right) ||
        !isfinite(bottom) || left < 0 || top < 0 || right > width ||
        bottom > height || bottom - top < 8 || right - left < bottom - top ||
        layer < SUDEKIMP_TITLE_BUTTON_IDLE || layer > SUDEKIMP_TITLE_BUTTON_GLEAM)
        return 0;
    cx = (left + right) * .5f;
    cy = (top + bottom) * .5f;
    radius = (bottom - top) * .5f;
    segment = (right - left) * .5f - radius;
    unit = radius / 20;
    for (y = 0; y < height; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (size_t)y * pitch);
        for (x = 0; x < width; ++x) {
            float dx = fmaxf(fabsf(x + .5f - cx) - segment, 0);
            float dy = y + .5f - cy;
            float d = (sqrtf(dx * dx + dy * dy) - radius) / unit;
            float sd = (sqrtf(dx * dx + (dy - 2 * unit) *
                (dy - 2 * unit)) - radius) / unit;
            float fill = clamp01(.5f - d * unit);
            float u = clamp01((x + .5f - left) / (right - left));
            float v = dy / radius;
            ButtonColor color = {0, 0, 0, 0};
            if (d > 12 || d < -40) { row[x] = 0; continue; }
            if (layer == SUDEKIMP_TITLE_BUTTON_GLEAM) {
                /* Full-face cloudy marbling, plus paired curved end glints.
                 * Kept below the label and clipped inside the capsule. The
                 * material and its phase are a recreation, not native pixels. */
                float lx = (x + .5f - left) / unit;
                float rx = (right - x - .5f) / unit;
                float left_arc = sqrtf((lx - 28) * (lx - 28) / (25 * 25) +
                    (v + .03f) * (v + .03f) / (.63f * .63f));
                float right_arc = sqrtf((rx - 28) * (rx - 28) / (25 * 25) +
                    (v - .03f) * (v - .03f) / (.63f * .63f));
                float curl = bell(left_arc - 1, .18f) * bell(lx - 23, 38) *
                    (.55f - .30f * v) +
                    bell(right_arc - 1, .18f) * bell(rx - 23, 38) *
                    (.55f + .30f * v);
                float cloud = cloud_gleam(u + gleam_phase, v + 2 * gleam_phase);
                float mask = clamp01((-d - 1.5f) / 2);
                over(&color, .49f + .18f * cloud, .56f + .17f * cloud,
                    .65f + .16f * cloud, mask * .334f * cloud);
                over(&color, .72f, .76f, .72f, mask * .13f * curl);
                row[x] = pack(color);
                continue;
            }
            over(&color, .025f, .023f, .028f,
                .35f * bell(fmaxf(sd, 0), 3.0f));
            /* Translucent smoky glass, with a soft dark rim. */
            over(&color, .13f, .125f, .135f,
                fill * (.44f + .10f * clamp01(v)));
            if (layer == SUDEKIMP_TITLE_BUTTON_HIGHLIGHT) {
                float cyan = expf(-u * 3.3f);
                float gold = expf(-(1 - u) * 3.5f);
                float inset = clamp01((-d - 1.3f) / 2);
                float sheen = bell(v + .36f, .40f);
                /* Bend each reflection into the cap; broad Gaussian blobs
                 * alone lose the curved blue/gold detail at the two ends. */
                float cap = bell(u - .035f, .036f);
                float other_cap = bell(u - .965f, .040f);
                float end = cap * bell(v + .34f - .28f * cap, .45f);
                float gold_end = other_cap * bell(v - .41f +
                    .20f * other_cap, .36f);
                over(&color, .065f, .072f, .065f, inset * .30f);
                over(&color, .22f, .48f, .50f,
                    inset * cyan * .24f * bell(v + .22f, .65f));
                over(&color, .50f, .76f, .77f,
                    inset * (.75f * cyan * sheen + .43f * end));
                over(&color, .59f, .49f, .27f,
                    inset * gold * .62f * bell(v - .38f, .38f));
                over(&color, .92f, .82f, .52f, inset * gold_end * .73f);
                /* Warm reflected light along the lower inside edge. */
                over(&color, .61f, .50f, .28f,
                    inset * bell(v - .80f, .19f) * (.30f + .38f * cyan));
                over(&color, .59f, .51f, .32f,
                    inset * bell(v + .82f, .15f) * (.18f + gold * .70f));
            }
            over(&color, .025f, .025f, .031f,
                .50f * bell(d + .5f, 1.55f));
            if (layer == SUDEKIMP_TITLE_BUTTON_CYAN) {
                /* Bright cyan through the bevel with a dark, translucent
                 * center. Both the cap and long edges share the same falloff. */
                over(&color, 0, .47f, .49f, fill * .27f);
                over(&color, 0, .86f, .88f, .65f * bell(d + 2.0f, 4.1f));
                over(&color, .11f, 1, 1, .95f * bell(d + .9f, 1.25f));
                over(&color, .18f, .86f, .87f, .16f * bell(d, 4.5f));
            }
            row[x] = pack(color);
        }
    }
    return 1;
}
