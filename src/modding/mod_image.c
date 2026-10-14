#include "modding/mod_image.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum { MAX_DIMENSION = 16384, MAX_IMAGE_BYTES = 256u * 1024u * 1024u };
enum { FMT_ARGB = 21, FMT_XRGB = 22, FMT_RGB = 20 };
#define FMT_DXT1 0x31545844u
#define FMT_DXT3 0x33545844u
#define FMT_DXT5 0x35545844u

typedef struct ImageSource {
    SudekiMpModImageInfo info;
    const uint8_t *pixels;
    const uint8_t *palette; /* SQX P8: 256 BGRA entries after the index mip chain */
    size_t available, level_size, pitch;
    unsigned bytes, descriptor, rle;
} ImageSource;

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int fail(char *error, size_t capacity, const char *message) {
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return 0;
}
static int dimensions(uint32_t width, uint32_t height) {
    return width && height && width <= MAX_DIMENSION && height <= MAX_DIMENSION &&
        (uint64_t)width * height * 4 <= MAX_IMAGE_BYTES;
}
static int compressed(uint32_t format) { return format == FMT_DXT1 || format == FMT_DXT3 || format == FMT_DXT5; }
static size_t block_size(uint32_t format, uint32_t width, uint32_t height) {
    return (size_t)((width + 3u) / 4u) * ((height + 3u) / 4u) * (format == FMT_DXT1 ? 8u : 16u);
}
static int raw_tga32(const uint8_t *data, size_t size) {
    size_t start;
    uint32_t width, height;
    if (size < 18 || data[1] || data[2] != 2 || data[16] != 32 || (data[17] & 0xc0u)) return 0;
    start = 18u + data[0]; width = u16(data + 12); height = u16(data + 14);
    return dimensions(width, height) && start <= size && (uint64_t)width * height * 4 <= size - start;
}

static int source(const void *input, size_t size, ImageSource *out, char *error, size_t capacity) {
    const uint8_t *data = (const uint8_t *)input;
    uint32_t width, height, format;
    size_t start;
    memset(out, 0, sizeof(*out));
    if (!data) return fail(error, capacity, "No image bytes");
    if (size >= 4 && !memcmp(data, "DDS ", 4)) {
        uint32_t flags, pf_flags, fourcc, pitch, caps2;
        if (size < 128 || u32(data + 4) != 124 || u32(data + 76) != 32)
            return fail(error, capacity, "Invalid DDS header");
        flags = u32(data + 8); height = u32(data + 12); width = u32(data + 16);
        pitch = u32(data + 20); pf_flags = u32(data + 80); fourcc = u32(data + 84);
        caps2 = u32(data + 112);
        if ((flags & 0x1007u) != 0x1007u || !dimensions(width, height) ||
            (caps2 & (0x200u | 0x200000u)) || u32(data + 24) > 1)
            return fail(error, capacity, "Unsupported DDS dimensions or surface type");
        if (pf_flags & 4u) {
            format = fourcc;
            if (!compressed(format)) return fail(error, capacity, "Unsupported DDS compression");
            out->level_size = block_size(format, width, height);
        } else {
            if (!(pf_flags & 0x40u) || u32(data + 88) != 32 ||
                u32(data + 92) != 0x00ff0000u || u32(data + 96) != 0x0000ff00u ||
                u32(data + 100) != 0x000000ffu ||
                ((pf_flags & 1u) && u32(data + 104) != 0xff000000u))
                return fail(error, capacity, "Unsupported DDS pixel layout");
            format = (pf_flags & 1u) ? FMT_ARGB : FMT_XRGB;
            out->bytes = 4; out->pitch = (flags & 8u) ? pitch : (size_t)width * 4;
            if (out->pitch < (size_t)width * 4 || out->pitch > MAX_IMAGE_BYTES ||
                (uint64_t)out->pitch * height > MAX_IMAGE_BYTES)
                return fail(error, capacity, "Invalid DDS row pitch");
            out->level_size = out->pitch * height;
        }
        start = 128; out->info.kind = SUDEKIMP_MOD_IMAGE_DDS;
        out->info.mip_levels = (flags & 0x20000u) ? u32(data + 28) : 1;
        if (!out->info.mip_levels || out->info.mip_levels > 15)
            return fail(error, capacity, "Invalid DDS mip count");
    } else if (!raw_tga32(data, size) && size >= 2048 && !(size % 2048) &&
               (u32(data + size - 4) & 255u) == 0x23u && !(u32(data + size - 4) >> 28)) {
        uint32_t word = u32(data + size - 4), sqx_format = (word >> 8) & 255u;
        format = sqx_format == 12 ? FMT_DXT1 : sqx_format == 14 ? FMT_DXT3 : sqx_format == 15 ? FMT_DXT5 :
            sqx_format == 11 ? FMT_ARGB : 0;
        width = 1u << ((word >> 20) & 15u); height = 1u << ((word >> 24) & 15u);
        if (!format || !dimensions(width, height)) return fail(error, capacity, "Unsupported SQX format or dimensions");
        out->info.kind = SUDEKIMP_MOD_IMAGE_SQX;
        out->info.mip_levels = (word >> 16) & 15u;
        if (!out->info.mip_levels) out->info.mip_levels = 1;
        start = 0;
        if (sqx_format == 11) {
            /* 8-bit palettized: one index byte per pixel for every mip, then
             * 256 BGRA entries. The game uploads it expanded to A8R8G8B8, so
             * that expansion is the TexMod identity. */
            size_t chain = 0;
            uint32_t mip_width = width, mip_height = height;
            for (uint32_t mip = 0; mip < out->info.mip_levels; ++mip) {
                chain += (size_t)mip_width * mip_height;
                mip_width = mip_width > 1 ? mip_width / 2 : 1;
                mip_height = mip_height > 1 ? mip_height / 2 : 1;
            }
            if (chain > size - 4 || 1024u > size - 4 - chain) return fail(error, capacity, "Truncated SQX palette");
            out->palette = data + chain; out->bytes = 1;
            out->level_size = (size_t)width * height; out->pitch = width;
        } else {
            out->level_size = block_size(format, width, height);
            if (out->level_size > size - 4) return fail(error, capacity, "Truncated SQX level zero");
        }
    } else {
        if (size < 18 || data[1] || (data[2] != 2 && data[2] != 10) ||
            (data[16] != 24 && data[16] != 32) || (data[17] & 0xc0u))
            return fail(error, capacity, "Unsupported image; expected TGA, SQX or DDS");
        width = u16(data + 12); height = u16(data + 14);
        if (!dimensions(width, height)) return fail(error, capacity, "Invalid TGA dimensions");
        start = 18u + data[0]; format = data[16] == 32 ? FMT_ARGB : FMT_RGB;
        out->bytes = data[16] / 8u; out->descriptor = data[17]; out->rle = data[2] == 10;
        out->pitch = (size_t)width * out->bytes; out->level_size = out->pitch * height;
        out->info.kind = SUDEKIMP_MOD_IMAGE_TGA; out->info.mip_levels = 1;
    }
    if (start > size || (!out->rle && out->level_size > size - start))
        return fail(error, capacity, "Truncated image level zero");
    if (out->info.kind == SUDEKIMP_MOD_IMAGE_DDS && out->info.mip_levels > 1) {
        size_t required = out->level_size;
        uint32_t mip_width = width, mip_height = height;
        for (uint32_t mip = 1; mip < out->info.mip_levels; ++mip) {
            size_t bytes;
            if (mip_width == 1 && mip_height == 1) return fail(error, capacity, "DDS mip count exceeds image dimensions");
            mip_width = mip_width > 1 ? mip_width / 2 : 1;
            mip_height = mip_height > 1 ? mip_height / 2 : 1;
            bytes = compressed(format) ? block_size(format, mip_width, mip_height) : (size_t)mip_width * mip_height * 4;
            if (bytes > size - start - required) return fail(error, capacity, "Truncated DDS mip chain");
            required += bytes;
        }
    }
    out->pixels = data + start; out->available = size - start;
    out->info.width = width; out->info.height = height; out->info.d3d_format = format;
    return 1;
}

/* TGA raw and RLE packets both use file row order, which is normalized here. */
static int tga_rgba(const ImageSource *src, uint8_t *rgba, char *error, size_t capacity) {
    size_t total = (size_t)src->info.width * src->info.height, cursor = 0, pixel = 0;
    while (pixel < total) {
        size_t count = 1;
        int repeat = 0;
        if (src->rle) {
            unsigned packet;
            if (cursor == src->available) return fail(error, capacity, "Truncated TGA RLE packet");
            packet = src->pixels[cursor++]; count = (packet & 127u) + 1u; repeat = !!(packet & 128u);
            if (count > total - pixel) return fail(error, capacity, "TGA RLE packet crosses image extent");
        }
        if ((repeat ? src->bytes : count * src->bytes) > src->available - cursor)
            return fail(error, capacity, "Truncated TGA pixels");
        if (!rgba) { cursor += repeat ? src->bytes : count * src->bytes; pixel += count; continue; }
        for (size_t n = 0; n < count; ++n) {
            const uint8_t *p = src->pixels + cursor + (repeat ? 0 : n * src->bytes);
            size_t ordinal = pixel + n;
            uint32_t x = (uint32_t)(ordinal % src->info.width), y = (uint32_t)(ordinal / src->info.width);
            uint8_t *d;
            if (!(src->descriptor & 0x20u)) y = src->info.height - 1u - y;
            if (src->descriptor & 0x10u) x = src->info.width - 1u - x;
            d = rgba + ((size_t)y * src->info.width + x) * 4;
            d[0] = p[2]; d[1] = p[1]; d[2] = p[0]; d[3] = src->bytes == 4 ? p[3] : 255;
        }
        cursor += repeat ? src->bytes : count * src->bytes; pixel += count;
    }
    return 1;
}

static void rgb565(uint16_t value, uint8_t *out) {
    unsigned r = (value >> 11) & 31u, g = (value >> 5) & 63u, b = value & 31u;
    out[0] = (uint8_t)((r << 3) | (r >> 2)); out[1] = (uint8_t)((g << 2) | (g >> 4));
    out[2] = (uint8_t)((b << 3) | (b >> 2)); out[3] = 255;
}
static void dxt_rgba(const ImageSource *src, uint8_t *rgba) {
    size_t cursor = 0;
    unsigned block_bytes = src->info.d3d_format == FMT_DXT1 ? 8u : 16u;
    for (uint32_t by = 0; by < src->info.height; by += 4) {
        for (uint32_t bx = 0; bx < src->info.width; bx += 4) {
            const uint8_t *block = src->pixels + cursor, *color = block + (block_bytes == 8 ? 0 : 8);
            uint8_t colors[4][4], alpha[8] = {0};
            uint16_t c0 = u16(color), c1 = u16(color + 2);
            uint32_t indices = u32(color + 4);
            uint64_t alpha_indices = 0;
            rgb565(c0, colors[0]); rgb565(c1, colors[1]);
            if (c0 > c1 || src->info.d3d_format != FMT_DXT1) {
                for (unsigned channel = 0; channel < 3; ++channel) {
                    colors[2][channel] = (uint8_t)((2u * colors[0][channel] + colors[1][channel]) / 3u);
                    colors[3][channel] = (uint8_t)((colors[0][channel] + 2u * colors[1][channel]) / 3u);
                }
                colors[2][3] = colors[3][3] = 255;
            } else {
                for (unsigned channel = 0; channel < 3; ++channel) colors[2][channel] = (uint8_t)((colors[0][channel] + colors[1][channel]) / 2u);
                colors[2][3] = 255; memset(colors[3], 0, 4);
            }
            if (src->info.d3d_format == FMT_DXT5) {
                alpha[0] = block[0]; alpha[1] = block[1];
                if (alpha[0] > alpha[1]) {
                    for (unsigned n = 1; n <= 6; ++n) alpha[n + 1] = (uint8_t)(((7u - n) * alpha[0] + n * alpha[1]) / 7u);
                } else {
                    for (unsigned n = 1; n <= 4; ++n) alpha[n + 1] = (uint8_t)(((5u - n) * alpha[0] + n * alpha[1]) / 5u);
                    alpha[6] = 0; alpha[7] = 255;
                }
                for (unsigned n = 0; n < 6; ++n) alpha_indices |= (uint64_t)block[2 + n] << (8u * n);
            }
            for (unsigned n = 0; n < 16; ++n) {
                uint32_t x = bx + n % 4u, y = by + n / 4u;
                uint8_t *pixel;
                if (x >= src->info.width || y >= src->info.height) continue;
                pixel = rgba + ((size_t)y * src->info.width + x) * 4;
                memcpy(pixel, colors[(indices >> (2u * n)) & 3u], 4);
                if (src->info.d3d_format == FMT_DXT3) pixel[3] = (uint8_t)(((block[n / 2u] >> (4u * (n & 1u))) & 15u) * 17u);
                if (src->info.d3d_format == FMT_DXT5) pixel[3] = alpha[(alpha_indices >> (3u * n)) & 7u];
            }
            cursor += block_bytes;
        }
    }
}

static void image_key(ImageSource *src) {
    uint32_t crc = 0xffffffffu;
    uint32_t table[256];
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t value = i;
        for (unsigned n = 0; n < 8; ++n) value = (value & 1u) ? (value >> 1) ^ 0xedb88320u : value >> 1;
        table[i] = value;
    }
    if (src->info.kind == SUDEKIMP_MOD_IMAGE_TGA) {
        /* Catalog identity is confirmed for raw 32-bit TGA. RLE/24-bit are
         * useful replacements, but their native upload contract is unknown. */
        if (src->rle || src->bytes != 4) return;
        for (uint32_t y = 0; y < src->info.height; ++y) {
            uint32_t file_y = (src->descriptor & 0x20u) ? y : src->info.height - 1u - y;
            const uint8_t *p = src->pixels + file_y * src->pitch;
            for (size_t n = 0; n < src->pitch; ++n) crc = table[(crc ^ p[n]) & 255u] ^ (crc >> 8);
        }
    } else if (src->palette) {
        for (size_t n = 0; n < src->level_size; ++n) {
            const uint8_t *entry = src->palette + src->pixels[n] * 4u;
            for (unsigned c = 0; c < 4; ++c) crc = table[(crc ^ entry[c]) & 255u] ^ (crc >> 8);
        }
    } else if (compressed(src->info.d3d_format)) {
        /* Use this call's table: previews and scans can hash concurrently,
         * while the old engine helper initializes a shared mutable table. */
        for (size_t n = 0; n < src->level_size; ++n) crc = table[(crc ^ src->pixels[n]) & 255u] ^ (crc >> 8);
    } else {
        for (uint32_t y = 0; y < src->info.height; ++y) {
            const uint8_t *p = src->pixels + y * src->pitch;
            for (size_t n = 0; n < (size_t)src->info.width * 4; ++n) crc = table[(crc ^ p[n]) & 255u] ^ (crc >> 8);
        }
    }
    src->info.texture_key = crc; src->info.key_known = 1;
}

int SudekiMpModImageDecode(const void *data, size_t size, SudekiMpModImage *image, char *error, size_t capacity) {
    ImageSource src;
    uint8_t *rgba;
    if (!image) return fail(error, capacity, "No image output");
    if (!source(data, size, &src, error, capacity)) return 0;
    rgba = (uint8_t *)malloc((size_t)src.info.width * src.info.height * 4);
    if (!rgba) return fail(error, capacity, "Not enough memory for image");
    if (src.info.kind == SUDEKIMP_MOD_IMAGE_TGA) {
        if (!tga_rgba(&src, rgba, error, capacity)) { free(rgba); return 0; }
    } else if (compressed(src.info.d3d_format)) dxt_rgba(&src, rgba);
    else if (src.palette) {
        for (size_t n = 0; n < src.level_size; ++n) {
            const uint8_t *p = src.palette + src.pixels[n] * 4u;
            uint8_t *d = rgba + n * 4;
            d[0] = p[2]; d[1] = p[1]; d[2] = p[0]; d[3] = p[3];
        }
    } else {
        for (uint32_t y = 0; y < src.info.height; ++y) {
            const uint8_t *row = src.pixels + y * src.pitch;
            for (uint32_t x = 0; x < src.info.width; ++x) {
                const uint8_t *p = row + x * 4;
                uint8_t *d = rgba + ((size_t)y * src.info.width + x) * 4;
                d[0] = p[2]; d[1] = p[1]; d[2] = p[0]; d[3] = src.info.d3d_format == FMT_ARGB ? p[3] : 255;
            }
        }
    }
    image_key(&src); image->info = src.info; image->rgba = rgba;
    if (error && capacity) *error = 0;
    return 1;
}

int SudekiMpModImageInspect(const void *data, size_t size, SudekiMpModImageInfo *info, char *error, size_t capacity) {
    ImageSource src;
    if (!info) return fail(error, capacity, "No image metadata output");
    if (!source(data, size, &src, error, capacity)) return 0;
    if (src.info.kind == SUDEKIMP_MOD_IMAGE_TGA && src.rle) {
        if (!tga_rgba(&src, NULL, error, capacity)) return 0;
        *info = src.info;
    } else { image_key(&src); *info = src.info; }
    if (error && capacity) *error = 0;
    return 1;
}
void SudekiMpModImageFree(SudekiMpModImage *image) {
    if (!image) return;
    free(image->rgba); memset(image, 0, sizeof(*image));
}
const char *SudekiMpModImageFormatName(uint32_t format) {
    switch (format) {
    case FMT_ARGB: return "A8R8G8B8";
    case FMT_XRGB: return "X8R8G8B8";
    case FMT_RGB: return "R8G8B8";
    case FMT_DXT1: return "DXT1";
    case FMT_DXT3: return "DXT3";
    case FMT_DXT5: return "DXT5";
    default: return "Unknown";
    }
}
