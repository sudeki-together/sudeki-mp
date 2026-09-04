#ifndef SUDEKIMP_CALL_HOOK_H
#define SUDEKIMP_CALL_HOOK_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SudekiMpRelativeCallHook {
    uint8_t *instruction;
    int32_t original_displacement;
    int32_t replacement_displacement;
    BOOL installed;
} SudekiMpRelativeCallHook;

typedef struct SudekiMpExportHook {
    uint32_t *slot;
    uint32_t original_rva;
    BOOL installed;
} SudekiMpExportHook;

typedef struct SudekiMpPointerHook {
    void **slot;
    void *original_value;
    void *replacement_value;
    BOOL installed;
} SudekiMpPointerHook;

typedef struct SudekiMpBytePatch {
    uint8_t *target;
    uint8_t original_value;
    uint8_t replacement_value;
    BOOL installed;
} SudekiMpBytePatch;

#define SUDEKIMP_INLINE_HOOK_MAX_BYTES 16u

typedef struct SudekiMpInlineHook {
    uint8_t *target;
    uint8_t original[SUDEKIMP_INLINE_HOOK_MAX_BYTES];
    size_t length;
    void *trampoline;
    BOOL installed;
} SudekiMpInlineHook;

BOOL SudekiMpInstallRelativeCallHook(
    SudekiMpRelativeCallHook *hook,
    uint8_t *instruction,
    const void *expected_target,
    const void *replacement
);

BOOL SudekiMpRestoreRelativeCallHook(SudekiMpRelativeCallHook *hook);

BOOL SudekiMpInstallExportHook(
    SudekiMpExportHook *hook,
    HMODULE game_module,
    uint32_t slot_rva,
    uint32_t expected_function_rva,
    const void *replacement
);

BOOL SudekiMpRestoreExportHook(SudekiMpExportHook *hook);

BOOL SudekiMpInstallPointerHook(
    SudekiMpPointerHook *hook,
    void **slot,
    const void *expected_value,
    const void *replacement
);

BOOL SudekiMpRestorePointerHook(SudekiMpPointerHook *hook);

BOOL SudekiMpInstallBytePatch(
    SudekiMpBytePatch *patch,
    uint8_t *target,
    uint8_t expected_value,
    uint8_t replacement_value
);

BOOL SudekiMpRestoreBytePatch(SudekiMpBytePatch *patch);

BOOL SudekiMpInstallInlineHook(
    SudekiMpInlineHook *hook,
    uint8_t *target,
    const uint8_t *expected,
    size_t length,
    const void *replacement
);

BOOL SudekiMpRestoreInlineHook(SudekiMpInlineHook *hook);

#endif
