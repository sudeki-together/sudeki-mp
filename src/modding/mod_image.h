#ifndef SUDEKIMP_MOD_IMAGE_H
#define SUDEKIMP_MOD_IMAGE_H
#include <stddef.h>
#include <stdint.h>

typedef enum SudekiMpModImageKind {
    SUDEKIMP_MOD_IMAGE_UNKNOWN = 0,
    SUDEKIMP_MOD_IMAGE_TGA,
    SUDEKIMP_MOD_IMAGE_SQX,
    SUDEKIMP_MOD_IMAGE_DDS
} SudekiMpModImageKind;

typedef struct SudekiMpModImageInfo {
    uint32_t width, height, d3d_format, mip_levels;
    uint32_t texture_key; /* TexMod CRC of original upload bytes, when key_known. */
    int key_known;
    SudekiMpModImageKind kind;
} SudekiMpModImageInfo;

typedef struct SudekiMpModImage {
    SudekiMpModImageInfo info;
    uint8_t *rgba; /* Owned, top-down, tightly packed RGBA8. */
} SudekiMpModImage;

/* Output must be zero-initialized. Unsupported or malformed images return 0;
 * error is optional. All input memory is borrowed and remains unchanged.
 * TGA: true-color 24/32-bit, raw/RLE; SQX: DXT1/3/5; DDS: DXT1/3/5,
 * uncompressed A8R8G8B8/X8R8G8B8. PNG/JPEG/BMP need a platform decoder. */
int SudekiMpModImageInspect(const void *data, size_t size, SudekiMpModImageInfo *info,
    char *error, size_t error_capacity);
int SudekiMpModImageDecode(const void *data, size_t size, SudekiMpModImage *image,
    char *error, size_t error_capacity);
void SudekiMpModImageFree(SudekiMpModImage *image);
const char *SudekiMpModImageFormatName(uint32_t d3d_format);
#endif
