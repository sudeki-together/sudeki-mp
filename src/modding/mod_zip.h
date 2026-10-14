#ifndef SUDEKIMP_MOD_ZIP_H
#define SUDEKIMP_MOD_ZIP_H

#include <stddef.h>
#include <stdint.h>

/* Pure zip support for mod packages: read (stored/deflate, optional
 * ZipCrypto password, central directory with a local-header fallback) and
 * write (stored only, streamed through a sink). No file I/O, no platform API.
 * Names are UTF-8 ('/' separators as stored); callers validate paths before
 * writing anything to disk. */

enum {
    SUDEKIMP_MOD_ZIP_NAME_MAX = 259,
    SUDEKIMP_MOD_ZIP_MAX_ENTRIES = 8192
};

typedef struct SudekiMpModZipEntry {
    char name[SUDEKIMP_MOD_ZIP_NAME_MAX + 1];
    uint8_t *data;
    size_t size;
    int directory;
} SudekiMpModZipEntry;

typedef struct SudekiMpModZip {
    SudekiMpModZipEntry *entries;
    size_t count;
    char comment[512]; /* UTF-8 (cp1252 converted) */
} SudekiMpModZip;

/* max_total caps the sum of uncompressed sizes (0 = 512 MiB). Returns 1 on
 * success; on failure the zip is empty and error holds a short reason. */
int SudekiMpModZipRead(const uint8_t *bytes, size_t size, const uint8_t *password,
    size_t password_length, size_t max_total, SudekiMpModZip *zip,
    char *error, size_t error_capacity);
void SudekiMpModZipFree(SudekiMpModZip *zip);
/* Exact (case-insensitive) name, else unique base-name match; -1 if none. */
long SudekiMpModZipFind(const SudekiMpModZip *zip, const char *name);

/* Raw DEFLATE (RFC 1951). Returns 1 and a malloc'd buffer, or 0. */
int SudekiMpModInflate(const uint8_t *in, size_t in_size, size_t expected_size,
    size_t max_size, uint8_t **out, size_t *out_size);
/* Standard zip CRC-32. */
uint32_t SudekiMpModZipCrc32(uint32_t crc, const void *data, size_t size);

typedef int (*SudekiMpModZipSink)(void *context, const void *bytes, size_t size);
typedef struct SudekiMpModZipWriter {
    SudekiMpModZipSink sink;
    void *context;
    uint64_t offset;
    uint8_t *central;
    size_t central_size, central_capacity;
    size_t count;
    int failed;
} SudekiMpModZipWriter;
void SudekiMpModZipWriterBegin(SudekiMpModZipWriter *writer, SudekiMpModZipSink sink, void *context);
/* Stores one file (no compression). name: UTF-8, '/' separators. */
int SudekiMpModZipWriterAdd(SudekiMpModZipWriter *writer, const char *name,
    const void *data, size_t size);
/* Writes the central directory; frees the writer's buffers either way. */
int SudekiMpModZipWriterFinish(SudekiMpModZipWriter *writer, const char *comment);

/* TexMod .tpf: the whole file is XORed with 0x3FA43FA4, then it is a
 * ZipCrypto zip with TexMod's fixed password holding texmod.def
 * ("0xKEY|member" lines) and the images. */
typedef struct SudekiMpModTpfTexture {
    uint32_t key;
    long member; /* index in the zip, -1 when the def names a missing file */
    char member_name[SUDEKIMP_MOD_ZIP_NAME_MAX + 1];
} SudekiMpModTpfTexture;
typedef struct SudekiMpModTpf {
    SudekiMpModZip zip; /* texmod.def is left out */
    SudekiMpModTpfTexture *textures;
    size_t count, missing;
    char author[256], description[512]; /* from the zip comment, UTF-8 */
} SudekiMpModTpf;
int SudekiMpModTpfRead(const uint8_t *bytes, size_t size, SudekiMpModTpf *tpf,
    char *error, size_t error_capacity);
void SudekiMpModTpfFree(SudekiMpModTpf *tpf);

/* "dds", "png", "tga", "bmp" or "jpg" from the leading bytes (tga as the
 * fallback for anything else). */
const char *SudekiMpModImageExtension(const uint8_t *bytes, size_t size);

#endif
