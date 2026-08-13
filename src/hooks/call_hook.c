#include "hooks/call_hook.h"

#include <stddef.h>
#include <string.h>

static BOOL write_protected_memory(void *destination, const void *source, size_t size) {
    DWORD old_protection;
    DWORD ignored_protection;

    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &old_protection)) {
        return FALSE;
    }
    CopyMemory(destination, source, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);
    if (!VirtualProtect(destination, size, old_protection, &ignored_protection)) {
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpInstallRelativeCallHook(
    SudekiMpRelativeCallHook *hook,
    uint8_t *instruction,
    const void *expected_target,
    const void *replacement
) {
    int32_t current_displacement;
    int32_t replacement_displacement;
    const uint8_t *current_target;

    if (hook == NULL || instruction == NULL || expected_target == NULL ||
        replacement == NULL || instruction[0] != 0xe8 || hook->installed) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    memcpy(&current_displacement, instruction + 1, sizeof(current_displacement));
    current_target = instruction + 5 + current_displacement;
    if (current_target != (const uint8_t *)expected_target) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    replacement_displacement = (int32_t)(
        (const uint8_t *)replacement - (instruction + 5)
    );
    hook->instruction = instruction;
    hook->original_displacement = current_displacement;
    hook->installed = TRUE;
    if (!write_protected_memory(
            instruction + 1,
            &replacement_displacement,
            sizeof(replacement_displacement))) {
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpRestoreRelativeCallHook(SudekiMpRelativeCallHook *hook) {
    if (hook == NULL || !hook->installed || hook->instruction == NULL) {
        return TRUE;
    }
    if (!write_protected_memory(
            hook->instruction + 1,
            &hook->original_displacement,
            sizeof(hook->original_displacement))) {
        return FALSE;
    }
    hook->installed = FALSE;
    hook->instruction = NULL;
    return TRUE;
}

BOOL SudekiMpInstallExportHook(
    SudekiMpExportHook *hook,
    HMODULE game_module,
    uint32_t slot_rva,
    uint32_t expected_function_rva,
    const void *replacement
) {
    uint32_t replacement_rva;

    if (hook == NULL || game_module == NULL || replacement == NULL ||
        hook->installed) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    hook->slot = (uint32_t *)((uint8_t *)game_module + slot_rva);
    if (*hook->slot != expected_function_rva) {
        hook->slot = NULL;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    replacement_rva = (uint32_t)(
        (const uint8_t *)replacement - (const uint8_t *)game_module
    );
    hook->original_rva = expected_function_rva;
    hook->installed = TRUE;
    if (!write_protected_memory(hook->slot, &replacement_rva, sizeof(replacement_rva))) {
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpRestoreExportHook(SudekiMpExportHook *hook) {
    if (hook == NULL || !hook->installed || hook->slot == NULL) {
        return TRUE;
    }
    if (!write_protected_memory(
            hook->slot,
            &hook->original_rva,
            sizeof(hook->original_rva))) {
        return FALSE;
    }
    hook->installed = FALSE;
    hook->slot = NULL;
    return TRUE;
}

BOOL SudekiMpInstallPointerHook(
    SudekiMpPointerHook *hook,
    void **slot,
    const void *expected_value,
    const void *replacement
) {
    void *replacement_value;

    if (hook == NULL || slot == NULL || expected_value == NULL ||
        replacement == NULL || hook->installed) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (*slot != expected_value) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    hook->slot = slot;
    hook->original_value = *slot;
    hook->installed = TRUE;
    replacement_value = (void *)replacement;
    if (!write_protected_memory(slot, &replacement_value, sizeof(replacement_value))) {
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpRestorePointerHook(SudekiMpPointerHook *hook) {
    if (hook == NULL || !hook->installed || hook->slot == NULL) {
        return TRUE;
    }
    if (!write_protected_memory(
            hook->slot,
            &hook->original_value,
            sizeof(hook->original_value))) {
        return FALSE;
    }
    hook->installed = FALSE;
    hook->slot = NULL;
    hook->original_value = NULL;
    return TRUE;
}
