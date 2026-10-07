#ifndef SUDEKIMP_MOD_ARCHIVE_H
#define SUDEKIMP_MOD_ARCHIVE_H
#include <stddef.h>
#include <stdint.h>

typedef struct SudekiMpModArchiveRecord { uint32_t offset, size, key; } SudekiMpModArchiveRecord;
typedef struct SudekiMpModArchive {
    const uint8_t *data;
    size_t size;
    SudekiMpModArchiveRecord *records;
    size_t count;
    uint8_t *owned_data; /* Only Open owns bytes; Parse borrows them. */
} SudekiMpModArchive;

/* Zero-initialize outputs. BAF validation checks index extents, counts, key
 * buckets and uniqueness; input bytes are immutable. Error is optional. */
int SudekiMpModArchiveParse(const void *data, size_t size, SudekiMpModArchive *archive,
    char *error, size_t error_capacity);
int SudekiMpModArchiveOpen(const char *path, SudekiMpModArchive *archive,
    char *error, size_t error_capacity);
const uint8_t *SudekiMpModArchiveResource(const SudekiMpModArchive *archive,
    size_t resource_index, size_t *size);
const uint8_t *SudekiMpModArchiveResourceByKey(const SudekiMpModArchive *archive,
    uint32_t key, size_t *size);
void SudekiMpModArchiveFree(SudekiMpModArchive *archive);

typedef struct SudekiMpModBlob { const void *data; size_t size; } SudekiMpModBlob;
typedef enum SudekiMpModResourceKind {
    SUDEKIMP_MOD_RESOURCE_TEXTURE = 1,
    SUDEKIMP_MOD_RESOURCE_MODEL = 2
} SudekiMpModResourceKind;

enum { SUDEKIMP_MOD_RESOURCE_NAME_MAX = 128 };
typedef struct SudekiMpModCatalogEntry {
    size_t archive_index, resource_index;
    uint32_t archive_key, texture_key, width, height, d3d_format;
    SudekiMpModResourceKind kind;
    unsigned name_candidates; /* Distinct case-insensitive names; 1 is resolved. */
    char name[SUDEKIMP_MOD_RESOURCE_NAME_MAX]; /* Empty for unknown/ambiguous. */
} SudekiMpModCatalogEntry;
typedef struct SudekiMpModCatalog {
    SudekiMpModCatalogEntry *entries;
    size_t count;
} SudekiMpModCatalog;
typedef int (*SudekiMpModCancelCheck)(void *context); /* Nonzero requests cancellation. */
typedef struct SudekiMpModNames { void *state; } SudekiMpModNames;
/* Bounded-memory scans: retain metadata only, harvest one mapped file at a
 * time, finish names, then build each mapped archive's catalog separately.
 * Begin accepts archives whose data pointers are NULL (records are required).
 * The finished names index owns its strings and has no borrowed blob pointers. */
int SudekiMpModNamesBegin(const SudekiMpModArchive *archives, size_t archive_count,
    SudekiMpModNames *names, char *error, size_t error_capacity);
int SudekiMpModNamesHarvestBlob(SudekiMpModNames *names, const void *data, size_t size,
    SudekiMpModCancelCheck cancel, void *cancel_context, char *error, size_t error_capacity);
void SudekiMpModNamesFinish(SudekiMpModNames *names);
void SudekiMpModNamesFree(SudekiMpModNames *names);
int SudekiMpModCatalogBuildNamed(const SudekiMpModArchive *archive,
    const SudekiMpModNames *names, SudekiMpModCatalog *catalog,
    SudekiMpModCancelCheck cancel, void *cancel_context, char *error, size_t error_capacity);

/* Harvest texture names, material stems and font pages from all archive bytes
 * plus optional executable/reference blobs, resolving against all archive keys.
 * Texture catalog keys match tools/sudekimod.py: original raw 32-bit TGA or
 * SQX DXT1/3/5. Models are included only when a unique known .HOM name resolves
 * to an archive record; this does not validate model contents or edit safety. */
int SudekiMpModCatalogBuild(const SudekiMpModArchive *archives, size_t archive_count,
    const SudekiMpModBlob *extra_blobs, size_t extra_count,
    SudekiMpModCatalog *catalog, char *error, size_t error_capacity);
int SudekiMpModCatalogBuildEx(const SudekiMpModArchive *archives, size_t archive_count,
    const SudekiMpModBlob *extra_blobs, size_t extra_count,
    SudekiMpModCatalog *catalog, SudekiMpModCancelCheck cancel, void *cancel_context,
    char *error, size_t error_capacity);
void SudekiMpModCatalogFree(SudekiMpModCatalog *catalog);
#endif
