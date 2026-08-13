#include "hooks/quick_menu.h"

#include <string.h>

BOOL SudekiMpEnableQuickMenuNormalSpeed(
    const uint8_t *activation_signature
) {
    static const uint8_t expected_instruction[] = {
        0xc7, 0x40, 0x24, 0x01, 0x00, 0x00, 0x00
    };
    static const uint8_t patched_instruction[] = {
        0xc7, 0x40, 0x24, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t *instruction;
    DWORD old_protection;
    DWORD ignored_protection;

    if (activation_signature == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* The mode-write instruction begins 11 bytes into the full signature. */
    instruction = (uint8_t *)activation_signature + 11u;
    if (memcmp(instruction, expected_instruction,
            sizeof(expected_instruction)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    if (!VirtualProtect(
            instruction,
            sizeof(expected_instruction),
            PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        return FALSE;
    }

    CopyMemory(instruction, patched_instruction, sizeof(patched_instruction));
    FlushInstructionCache(
        GetCurrentProcess(),
        instruction,
        sizeof(patched_instruction)
    );

    if (memcmp(instruction, patched_instruction,
            sizeof(patched_instruction)) != 0) {
        CopyMemory(instruction, expected_instruction, sizeof(expected_instruction));
        FlushInstructionCache(
            GetCurrentProcess(),
            instruction,
            sizeof(expected_instruction)
        );
        VirtualProtect(
            instruction,
            sizeof(expected_instruction),
            old_protection,
            &ignored_protection
        );
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }

    if (!VirtualProtect(
            instruction,
            sizeof(patched_instruction),
            old_protection,
            &ignored_protection)) {
        return FALSE;
    }

    return TRUE;
}
