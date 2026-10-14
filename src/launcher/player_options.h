#ifndef SUDEKIMP_PLAYER_OPTIONS_H
#define SUDEKIMP_PLAYER_OPTIONS_H

#include <stddef.h>

/*
 * Pure editor for Sudeki's launcher option document (PlayerOptions.xml and
 * launcherdata/DefaultOptions.xml). The vanilla SudekiLauncher.exe writes it
 * as UTF-16LE with a BOM; the shipped defaults are ASCII. The document is kept
 * as UTF-8 text and only the value attribute of an existing
 * <setting id='S'><Variant id='V' type='T' value='...' /> is ever replaced, so
 * every other byte (unknown settings, comments, line endings) round-trips.
 * Settings are never invented: setting an absent setting/variant fails.
 */

typedef enum SudekiMpPlayerOptionsEncoding {
    SUDEKIMP_PLAYER_OPTIONS_UTF8 = 0,
    SUDEKIMP_PLAYER_OPTIONS_UTF16LE = 1
} SudekiMpPlayerOptionsEncoding;

typedef enum SudekiMpPlayerOptionsType {
    SUDEKIMP_PLAYER_OPTION_UNKNOWN = 0,
    SUDEKIMP_PLAYER_OPTION_INTEGER,
    SUDEKIMP_PLAYER_OPTION_BOOL,
    SUDEKIMP_PLAYER_OPTION_FLOAT,
    SUDEKIMP_PLAYER_OPTION_STRING
} SudekiMpPlayerOptionsType;

typedef struct SudekiMpPlayerOptions {
    char *text;
    size_t length;
    size_t capacity;
    SudekiMpPlayerOptionsEncoding encoding;
    int has_bom;
} SudekiMpPlayerOptions;

#define SUDEKIMP_PLAYER_OPTION_ID_CAPACITY 64u
#define SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY 256u

typedef void (*SudekiMpPlayerOptionsSettingCallback)(const char *setting_id,
                                                     void *context);

/* Decodes UTF-16LE (BOM required) or UTF-8/ASCII bytes. Returns 0 on
   malformed encoding, an embedded NUL, or a missing <launcher_options> root. */
int SudekiMpPlayerOptionsLoad(SudekiMpPlayerOptions *options,
                              const unsigned char *bytes,
                              size_t count);
void SudekiMpPlayerOptionsFree(SudekiMpPlayerOptions *options);

int SudekiMpPlayerOptionsHas(const SudekiMpPlayerOptions *options,
                             const char *setting_id,
                             const char *variant_id);
int SudekiMpPlayerOptionsGet(const SudekiMpPlayerOptions *options,
                             const char *setting_id,
                             const char *variant_id,
                             char *value,
                             size_t value_count,
                             SudekiMpPlayerOptionsType *type);
/* Replaces one existing value after validating it against the variant's
   declared type. Strings may not contain XML markup or quote characters. */
int SudekiMpPlayerOptionsSet(SudekiMpPlayerOptions *options,
                             const char *setting_id,
                             const char *variant_id,
                             const char *value);
/* Visits each <setting> id in document order. Returns the number visited. */
size_t SudekiMpPlayerOptionsForEachSetting(const SudekiMpPlayerOptions *options,
                                           SudekiMpPlayerOptionsSettingCallback callback,
                                           void *context);
/* Encodes back to the loaded encoding; the caller frees *bytes with free(). */
int SudekiMpPlayerOptionsSerialize(const SudekiMpPlayerOptions *options,
                                   unsigned char **bytes,
                                   size_t *count);

int SudekiMpPlayerOptionsValueIsValid(SudekiMpPlayerOptionsType type,
                                      const char *value);

#endif
