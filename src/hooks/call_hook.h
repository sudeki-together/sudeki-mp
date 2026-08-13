#ifndef SUDEKIMP_CALL_HOOK_H
#define SUDEKIMP_CALL_HOOK_H

#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpRelativeCallHook {
    uint8_t *instruction;
    int32_t original_displacement;
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
    BOOL installed;
} SudekiMpPointerHook;

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

#endif
