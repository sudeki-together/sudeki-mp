#ifndef SUDEKIMP_TEXTURE_MOD_INDEX_H
#define SUDEKIMP_TEXTURE_MOD_INDEX_H
#include <stddef.h>
#include <stdint.h>

/* Pure core of the texture mod format (docs/mod-packages.md). No Windows or
 * native dependency, so it is host-testable.
 *
 * A texture is identified by the TexMod-compatible key: CRC-32 (reflected
 * polynomial 0xEDB88320, initial value 0xFFFFFFFF, NO final inversion) of the
 * level-0 pixel bytes the game uploads, size = bits(format)*width*height/8.
 * Offline cross-check: the SymphonyUI TPF key 0xAC57BC5D equals this hash of
 * Fonts.baf Verdana_16-0.tga in top-down row order. */
uint32_t SudekiMpTexModCrc32(const void *data, size_t size);
/* Bits per pixel TexMod uses for a D3DFORMAT, or 0 when not supported. */
unsigned SudekiMpTexModFormatBits(uint32_t d3d_format);
/* Level-0 byte count for the key, or 0 for an unsupported format/size. */
size_t SudekiMpTexModLevelBytes(uint32_t d3d_format, uint32_t width, uint32_t height);

/* "0x1A2B3C4D" (case-insensitive, 1..8 hex digits). */
int SudekiMpTextureModParseKey(const char *text, uint32_t *key);
/* Relative path inside a mod folder: no drive, root, '..', empty or '.'
 * component, control character or ':'; '/' and '\\' both separate. */
int SudekiMpTextureModSafePath(const char *path);

enum { SUDEKIMP_TEXTURE_MOD_PATH_MAX = 240 };
/* One [Textures] line ("0xKEY = relative/file.dds", trimmed; ';' or '#'
 * starts a comment line). Returns 1 for an entry, 0 for blank/comment, -1 for
 * a malformed line. */
int SudekiMpTextureModParseLine(const char *line, uint32_t *key, char *path, size_t path_capacity);

typedef struct SudekiMpTextureModEntry {
    uint32_t key;
    uint32_t mod;      /* index of the owning mod in load order */
    uint32_t sequence; /* insertion order; the last entry for a key wins */
    uint32_t path;     /* offset into the string pool */
} SudekiMpTextureModEntry;

typedef struct SudekiMpTextureModIndex {
    SudekiMpTextureModEntry *entries;
    size_t count, capacity;
    char *pool;
    size_t pool_used, pool_capacity;
    size_t overridden; /* entries dropped because a later mod used the key */
    int finished;
} SudekiMpTextureModIndex;

int SudekiMpTextureModIndexAdd(SudekiMpTextureModIndex *index, uint32_t key, uint32_t mod, const char *path);
/* Sorts and keeps the last entry for each key. Required before Find. */
void SudekiMpTextureModIndexFinish(SudekiMpTextureModIndex *index);
const SudekiMpTextureModEntry *SudekiMpTextureModIndexFind(const SudekiMpTextureModIndex *index, uint32_t key);
const char *SudekiMpTextureModIndexPath(const SudekiMpTextureModIndex *index, const SudekiMpTextureModEntry *entry);
void SudekiMpTextureModIndexFree(SudekiMpTextureModIndex *index);

/* ---- Whole-file archive resources ([Files], docs/mod-packages.md) ---- */
/* Archive key of NAME.EXT: upper-cased ASCII, add on even and multiply on odd
 * positions, modulo 2^32 (the native checksum at RVA 0x3A4A0). */
uint32_t SudekiMpModResourceKey(const char *name);
enum { SUDEKIMP_MOD_NAME_MAX = 120 };
/* One [Files] line: "NAME.EXT = relative/file" or "0xKEY = relative/file".
 * name receives NAME.EXT (or the key text). 1 entry, 0 blank/comment, -1 bad. */
int SudekiMpModFileParseLine(const char *line, uint32_t *key, char *name, size_t name_capacity,
    char *path, size_t path_capacity);
/* Archive index bucket rows are 12 bytes {offset, size, key}, sorted by key. */
long SudekiMpArchiveBucketFind(const uint8_t *rows, uint32_t count, uint32_t key);
/* dst (count+1 rows) = src with {offset,size,key} inserted in key order. */
void SudekiMpArchiveBucketInsert(uint8_t *dst, const uint8_t *src, uint32_t count,
    uint32_t offset, uint32_t size, uint32_t key);

/* Loose game-folder files a mod may override ([Files] names with a folder):
 * "sound/<file>" (wave/sound banks, speech) and "movies/<file>". The name is
 * normalised to backslashes; 1 when valid. */
int SudekiMpModLooseNameValid(const char *name);
/* The "sound\..." or "movies\..." tail of a path the game opens (after the
 * last separator-bounded sound\ or movies\ component), copied into rel with
 * backslashes; 0 when the path is not under either folder. */
int SudekiMpModLooseRelative(const char *path, char *rel, size_t capacity);
#endif
