#ifndef SUDEKIMP_MOD_MANIFEST_H
#define SUDEKIMP_MOD_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

/* Pure in-memory mod.ini editing. Text is valid UTF-8 internally. Load accepts
 * the documented ASCII or UTF-16LE+BOM; Encode retains UTF-16 input and uses
 * UTF-16LE+BOM whenever an edit introduces non-ASCII characters. Unknown
 * sections, metadata, comments and unedited lines retain their original text.
 * Initialize to zero, Free before reusing Load/Create; successful edits replace
 * text atomically in memory. The caller owns Unicode disk I/O and atomic save. */
typedef struct SudekiMpModManifest {
    char *text;
    size_t length;
    int utf16;
} SudekiMpModManifest;

int SudekiMpModManifestLoad(const void *bytes, size_t size, SudekiMpModManifest *manifest);
int SudekiMpModManifestCreate(const char *name, SudekiMpModManifest *manifest);
int SudekiMpModManifestEncode(const SudekiMpModManifest *manifest, uint8_t **bytes, size_t *size);
void SudekiMpModManifestFree(SudekiMpModManifest *manifest);

int SudekiMpModManifestSetEnabled(SudekiMpModManifest *manifest, int enabled);
/* [Mod] metadata (Author, Description, Source, Version...): one UTF-8 line;
 * NULL removes it. Format and Enabled have their own rules and are refused. */
int SudekiMpModManifestSetMetadata(SudekiMpModManifest *manifest, const char *name, const char *value);
/* NULL relative removes every alias of the identity in that section. Texture
 * aliases include differently padded/cased hex keys; archive file aliases also
 * include NAME.EXT and its checksum key. Loose names match case-insensitively
 * with either separator. Paths must pass the existing runtime path parser. */
int SudekiMpModManifestSetTexture(SudekiMpModManifest *manifest, uint32_t key, const char *relative);
int SudekiMpModManifestSetFile(SudekiMpModManifest *manifest, const char *name, const char *relative);
/* Return 1 for a found, valid entry fitting capacity; 0 otherwise. */
int SudekiMpModManifestGetTexture(const SudekiMpModManifest *manifest, uint32_t key, char *relative, size_t capacity);
int SudekiMpModManifestGetFile(const SudekiMpModManifest *manifest, const char *name, char *relative, size_t capacity);
int SudekiMpModManifestGetValue(const SudekiMpModManifest *manifest, const char *section,
    const char *name, char *value, size_t capacity);

enum SudekiMpModFileInspection {
    SUDEKIMP_MOD_FILE_OK = 1,
    SUDEKIMP_MOD_FILE_MISSING = 0,
    SUDEKIMP_MOD_FILE_EMPTY = -1,
    SUDEKIMP_MOD_FILE_BAD_IMAGE = -2
};
/* Optional inspector receives a validated relative UTF-8 path. For textures,
 * check DDS/PNG/TGA/BMP/JPG readability and return dimensions when available;
 * for [Files], check existence and nonzero size. No game data is required. */
typedef int (*SudekiMpModManifestInspect)(void *context, const char *relative,
    int texture, uint32_t *width, uint32_t *height);
typedef void (*SudekiMpModManifestDiagnostic)(void *context, size_t line,
    int warning, const char *message);
typedef struct SudekiMpModManifestValidation {
    size_t textures, files, errors, warnings;
} SudekiMpModManifestValidation;
/* NULL inspect checks format, syntax, paths and duplicate identities only.
 * Returns 1 iff no errors; diagnostics are UTF-8 and valid during the call. */
int SudekiMpModManifestValidate(const SudekiMpModManifest *manifest,
    SudekiMpModManifestInspect inspect, SudekiMpModManifestDiagnostic diagnostic,
    void *context, SudekiMpModManifestValidation *result);

#endif
