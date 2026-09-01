#include "hooks/call_hook.h"

#include <stddef.h>
#include <string.h>

static BOOL write_protected_memory(void *destination, const void *source, size_t size) {
    uint8_t original[SUDEKIMP_INLINE_HOOK_MAX_BYTES];
    DWORD old_protection;
    DWORD ignored_protection;
    DWORD restore_error;
    DWORD rollback_protection;

    if (destination == NULL || source == NULL || size == 0u ||
        size > sizeof(original)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &old_protection)) {
        return FALSE;
    }
    CopyMemory(original, destination, size);
    CopyMemory(destination, source, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);
    if (!VirtualProtect(destination, size, old_protection, &ignored_protection)) {
        /* The bytes are already live.  Never report a failed install and let
         * the caller free its trampoline while a detour still targets it.
         * Reacquire write access and roll the bytes back first.  If even that
         * fails, keep the hook transaction installed/owned; this is safer
         * than creating a dangling jump. */
        restore_error = GetLastError();
        if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE,
                &rollback_protection)) {
            SetLastError(restore_error);
            return TRUE;
        }
        CopyMemory(destination, original, size);
        FlushInstructionCache(GetCurrentProcess(), destination, size);
        (void)VirtualProtect(destination, size, old_protection,
            &ignored_protection);
        SetLastError(restore_error);
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
    replacement_value = (void *)replacement;
    hook->replacement_value = replacement_value;
    hook->installed = TRUE;
    if (!write_protected_memory(slot, &replacement_value, sizeof(replacement_value))) {
        /* A FALSE result guarantees that write_protected_memory either never
         * wrote the slot or rolled it back.  Do not leave an inert failed
         * install looking owned. */
        ZeroMemory(hook, sizeof(*hook));
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpRestorePointerHook(SudekiMpPointerHook *hook) {
    if (hook == NULL || !hook->installed || hook->slot == NULL) {
        return TRUE;
    }
    if (*hook->slot != hook->replacement_value) {
        /* Another hook owns the slot now.  Retain the complete record so its
         * owner can put our replacement back and retry teardown safely. */
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (!write_protected_memory(
            hook->slot,
            &hook->original_value,
            sizeof(hook->original_value))) {
        return FALSE;
    }
    ZeroMemory(hook, sizeof(*hook));
    return TRUE;
}

BOOL SudekiMpInstallBytePatch(
    SudekiMpBytePatch *patch,
    uint8_t *target,
    uint8_t expected_value,
    uint8_t replacement_value
) {
    if (patch == NULL || target == NULL || patch->installed ||
        expected_value == replacement_value) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (*target != expected_value) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    patch->target = target;
    patch->original_value = expected_value;
    patch->replacement_value = replacement_value;
    patch->installed = TRUE;
    if (!write_protected_memory(
            target, &replacement_value, sizeof(replacement_value))) {
        ZeroMemory(patch, sizeof(*patch));
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpRestoreBytePatch(SudekiMpBytePatch *patch) {
    if (patch == NULL || !patch->installed || patch->target == NULL) {
        return TRUE;
    }
    if (*patch->target != patch->replacement_value) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (!write_protected_memory(
            patch->target, &patch->original_value,
            sizeof(patch->original_value))) {
        return FALSE;
    }
    ZeroMemory(patch, sizeof(*patch));
    return TRUE;
}

BOOL SudekiMpInstallInlineHook(
    SudekiMpInlineHook *hook,
    uint8_t *target,
    const uint8_t *expected,
    size_t length,
    const void *replacement
) {
    uint8_t patch[SUDEKIMP_INLINE_HOOK_MAX_BYTES];
    uint8_t *trampoline;
    int32_t displacement;
    DWORD old_protection;

    if (hook == NULL || target == NULL || expected == NULL ||
        replacement == NULL || hook->installed || length < 5u ||
        length > SUDEKIMP_INLINE_HOOK_MAX_BYTES) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (memcmp(target, expected, length) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    trampoline = (uint8_t *)VirtualAlloc(
        NULL,
        length + 5u,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (trampoline == NULL) {
        return FALSE;
    }
    memcpy(trampoline, target, length);
    trampoline[length] = 0xe9;
    displacement = (int32_t)((target + length) - (trampoline + length + 5u));
    memcpy(trampoline + length + 1u, &displacement, sizeof(displacement));
    if (!VirtualProtect(
            trampoline,
            length + 5u,
            PAGE_EXECUTE_READ,
            &old_protection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, length + 5u);

    memset(patch, 0x90, length);
    patch[0] = 0xe9;
    displacement = (int32_t)((const uint8_t *)replacement - (target + 5u));
    memcpy(patch + 1u, &displacement, sizeof(displacement));

    hook->target = target;
    memcpy(hook->original, target, length);
    hook->length = length;
    hook->trampoline = trampoline;
    if (!write_protected_memory(target, patch, length)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        ZeroMemory(hook, sizeof(*hook));
        return FALSE;
    }
    hook->installed = TRUE;
    return TRUE;
}

BOOL SudekiMpRestoreInlineHook(SudekiMpInlineHook *hook) {
    BOOL restored;

    if (hook == NULL || !hook->installed || hook->target == NULL) {
        return TRUE;
    }
    restored = write_protected_memory(
        hook->target,
        hook->original,
        hook->length
    );
    if (!restored) {
        return FALSE;
    }
    if (hook->trampoline != NULL) {
        VirtualFree(hook->trampoline, 0, MEM_RELEASE);
    }
    ZeroMemory(hook, sizeof(*hook));
    return TRUE;
}
