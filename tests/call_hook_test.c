#include "hooks/call_hook.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error=%lu)\n", message,
            (unsigned long)GetLastError());
        ++failures;
    }
}

int main(void) {
    uint8_t *memory = (uint8_t *)VirtualAlloc(
        NULL, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
    );
    uint8_t *call_instruction;
    uint8_t *original_target;
    uint8_t *replacement_target;
    int32_t displacement;
    SudekiMpRelativeCallHook call_hook = {0};
    SudekiMpRelativeCallHook call_install_failure_hook = {0};
    SudekiMpExportHook export_hook = {0};
    SudekiMpPointerHook pointer_hook = {0};
    SudekiMpPointerHook restore_failure_hook = {0};
    SudekiMpInlineHook inline_hook = {0};
    uint32_t *export_slot;
    void **pointer_slot;
    void *foreign_target;
    HANDLE mapping;
    HANDLE call_mapping;
    void **writable_view;
    void **read_only_view;
    uint8_t *writable_call_view;
    uint8_t *read_only_call_view;
    uint8_t *inline_target;
    uint8_t *inline_trampoline;
    static const uint8_t inline_expected[] = {
        0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
    };

    check(memory != NULL, "allocate synthetic image");
    if (memory == NULL) {
        return 1;
    }

    call_instruction = memory + 0x100;
    original_target = memory + 0x300;
    replacement_target = memory + 0x500;
    call_instruction[0] = 0xe8;
    displacement = (int32_t)(original_target - (call_instruction + 5));
    memcpy(call_instruction + 1, &displacement, sizeof(displacement));

    check(SudekiMpInstallRelativeCallHook(
        &call_hook, call_instruction, original_target, replacement_target
    ), "install relative call hook");
    memcpy(&displacement, call_instruction + 1, sizeof(displacement));
    check(call_instruction + 5 + displacement == replacement_target,
        "relative call targets replacement");
    check(SudekiMpRestoreRelativeCallHook(&call_hook),
        "restore relative call hook");
    memcpy(&displacement, call_instruction + 1, sizeof(displacement));
    check(call_instruction + 5 + displacement == original_target,
        "relative call targets original after restore");

    check(SudekiMpInstallRelativeCallHook(
        &call_hook, call_instruction, original_target, replacement_target
    ), "reinstall relative call hook for ownership loss");
    displacement = (int32_t)((memory + 0x700) - (call_instruction + 5));
    memcpy(call_instruction + 1, &displacement, sizeof(displacement));
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpRestoreRelativeCallHook(&call_hook) &&
        GetLastError() == ERROR_BUSY && call_hook.installed,
        "relative call teardown retains foreign ownership for retry");
    memcpy(call_instruction + 1, &call_hook.replacement_displacement,
        sizeof(call_hook.replacement_displacement));
    check(SudekiMpRestoreRelativeCallHook(&call_hook) &&
        !call_hook.installed && call_hook.instruction == NULL &&
        call_hook.original_displacement == 0 &&
        call_hook.replacement_displacement == 0,
        "relative call teardown retries after replacement ownership returns");
    memcpy(&displacement, call_instruction + 1, sizeof(displacement));
    check(call_instruction + 5 + displacement == original_target,
        "relative call retry restores the original target");

    call_instruction[0] = 0x90;
    check(!SudekiMpInstallRelativeCallHook(
        &call_hook, call_instruction, original_target, replacement_target
    ), "reject non-CALL instruction");

    /* A read-only mapped CALL lets validation succeed while its displacement
     * remains impossible for write_protected_memory to patch. */
    call_mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0u, 0x1000u, NULL
    );
    writable_call_view = call_mapping != NULL ? (uint8_t *)MapViewOfFile(
        call_mapping, FILE_MAP_WRITE, 0u, 0u, 0x1000u) : NULL;
    check(call_mapping != NULL && writable_call_view != NULL,
        "create mapped CALL for install-write failure");
    if (writable_call_view != NULL) {
        writable_call_view[0x100u] = 0xe8u;
        displacement = 0;
        memcpy(
            writable_call_view + 0x101u,
            &displacement,
            sizeof(displacement)
        );
        UnmapViewOfFile(writable_call_view);
    }
    read_only_call_view = call_mapping != NULL ? (uint8_t *)MapViewOfFile(
        call_mapping, FILE_MAP_READ, 0u, 0u, 0x1000u) : NULL;
    check(read_only_call_view != NULL,
        "map read-only CALL for install-write failure");
    if (read_only_call_view != NULL) {
        SetLastError(ERROR_SUCCESS);
        check(!SudekiMpInstallRelativeCallHook(
            &call_install_failure_hook,
            read_only_call_view + 0x100u,
            read_only_call_view + 0x105u,
            read_only_call_view + 0x180u
        ), "relative-call install reports a protected-write failure");
        memcpy(
            &displacement,
            read_only_call_view + 0x101u,
            sizeof(displacement)
        );
        check(displacement == 0,
            "failed relative-call install preserves the original target");
        check(!call_install_failure_hook.installed &&
            call_install_failure_hook.instruction == NULL &&
            call_install_failure_hook.original_displacement == 0 &&
            call_install_failure_hook.replacement_displacement == 0,
            "failed relative-call install clears ownership bookkeeping");
        UnmapViewOfFile(read_only_call_view);
    }
    if (call_mapping != NULL) {
        CloseHandle(call_mapping);
    }

    call_instruction[0] = 0xe8u;
    displacement = (int32_t)(original_target - (call_instruction + 5));
    memcpy(call_instruction + 1, &displacement, sizeof(displacement));
    check(SudekiMpInstallRelativeCallHook(
        &call_install_failure_hook,
        call_instruction,
        original_target,
        replacement_target
    ), "retry relative-call install after a protected-write failure");
    check(SudekiMpRestoreRelativeCallHook(&call_install_failure_hook),
        "restore retried relative-call hook");
    memcpy(&displacement, call_instruction + 1, sizeof(displacement));
    check(call_instruction + 5 + displacement == original_target,
        "retried relative-call hook restores its original target");

    export_slot = (uint32_t *)(memory + 0x800);
    *export_slot = 0x300u;
    check(SudekiMpInstallExportHook(
        &export_hook, (HMODULE)memory, 0x800u, 0x300u, replacement_target
    ), "install export hook");
    check(*export_slot == 0x500u, "export slot targets replacement RVA");
    check(SudekiMpRestoreExportHook(&export_hook), "restore export hook");
    check(*export_slot == 0x300u, "export slot restored");

    *export_slot = 0x301u;
    check(!SudekiMpInstallExportHook(
        &export_hook, (HMODULE)memory, 0x800u, 0x300u, replacement_target
    ), "reject unexpected export RVA");

    pointer_slot = (void **)(memory + 0x900);
    foreign_target = memory + 0x700;
    *pointer_slot = original_target;
    check(SudekiMpInstallPointerHook(
        &pointer_hook, pointer_slot, original_target, replacement_target
    ), "install pointer hook");
    check(*pointer_slot == replacement_target,
        "pointer slot targets replacement");
    check(pointer_hook.installed && pointer_hook.slot == pointer_slot &&
        pointer_hook.original_value == original_target &&
        pointer_hook.replacement_value == replacement_target,
        "pointer hook records complete ownership");

    *pointer_slot = foreign_target;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpRestorePointerHook(&pointer_hook) &&
        GetLastError() == ERROR_BUSY,
        "pointer restore rejects a foreign slot owner");
    check(*pointer_slot == foreign_target && pointer_hook.installed &&
        pointer_hook.slot == pointer_slot &&
        pointer_hook.original_value == original_target &&
        pointer_hook.replacement_value == replacement_target,
        "foreign ownership retains pointer-hook bookkeeping");

    *pointer_slot = replacement_target;
    check(SudekiMpRestorePointerHook(&pointer_hook),
        "restore pointer hook after ownership returns");
    check(*pointer_slot == original_target,
        "pointer slot restored after ownership returns");
    check(!pointer_hook.installed && pointer_hook.slot == NULL &&
        pointer_hook.original_value == NULL &&
        pointer_hook.replacement_value == NULL,
        "successful pointer restore clears ownership bookkeeping");

    /* A read-only mapped view lets the ownership comparison succeed while
     * making write_protected_memory fail its protection upgrade. */
    mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0u, 0x1000u, NULL
    );
    writable_view = mapping != NULL ? (void **)MapViewOfFile(
        mapping, FILE_MAP_WRITE, 0u, 0u, 0x1000u) : NULL;
    check(mapping != NULL && writable_view != NULL,
        "create mapped pointer slot for restore-write failure");
    if (writable_view != NULL) {
        *writable_view = replacement_target;
        UnmapViewOfFile(writable_view);
    }
    read_only_view = mapping != NULL ? (void **)MapViewOfFile(
        mapping, FILE_MAP_READ, 0u, 0u, 0x1000u) : NULL;
    check(read_only_view != NULL,
        "map read-only pointer slot for restore-write failure");
    if (read_only_view != NULL) {
        restore_failure_hook.slot = read_only_view;
        restore_failure_hook.original_value = original_target;
        restore_failure_hook.replacement_value = replacement_target;
        restore_failure_hook.installed = TRUE;
        SetLastError(ERROR_SUCCESS);
        check(!SudekiMpRestorePointerHook(&restore_failure_hook),
            "pointer restore reports a protected-write failure");
        check(*read_only_view == replacement_target &&
            restore_failure_hook.installed &&
            restore_failure_hook.slot == read_only_view &&
            restore_failure_hook.original_value == original_target &&
            restore_failure_hook.replacement_value == replacement_target,
            "protected-write failure retains pointer-hook bookkeeping");
        UnmapViewOfFile(read_only_view);
    }
    if (mapping != NULL) {
        CloseHandle(mapping);
    }

    inline_target = memory + 0xa00;
    memcpy(inline_target, inline_expected, sizeof(inline_expected));
    check(SudekiMpInstallInlineHook(
        &inline_hook,
        inline_target,
        inline_expected,
        sizeof(inline_expected),
        replacement_target
    ), "install inline hook");
    memcpy(&displacement, inline_target + 1u, sizeof(displacement));
    check(inline_target[0] == 0xe9 &&
        inline_target + 5u + displacement == replacement_target,
        "inline hook targets replacement");
    check(inline_target[5] == 0x90, "inline hook pads stolen bytes");
    inline_trampoline = (uint8_t *)inline_hook.trampoline;
    check(inline_trampoline != NULL && memcmp(
            inline_trampoline,
            inline_expected,
            sizeof(inline_expected)) == 0,
        "inline trampoline preserves stolen instructions");
    if (inline_trampoline != NULL) {
        memcpy(
            &displacement,
            inline_trampoline + sizeof(inline_expected) + 1u,
            sizeof(displacement)
        );
        check(
            inline_trampoline[sizeof(inline_expected)] == 0xe9 &&
            inline_trampoline + sizeof(inline_expected) + 5u + displacement ==
                inline_target + sizeof(inline_expected),
            "inline trampoline returns after stolen instructions"
        );
    }
    check(SudekiMpRestoreInlineHook(&inline_hook), "restore inline hook");
    check(memcmp(
            inline_target,
            inline_expected,
            sizeof(inline_expected)) == 0,
        "inline hook restores original instructions");

    inline_target[0] = 0x90;
    check(!SudekiMpInstallInlineHook(
        &inline_hook,
        inline_target,
        inline_expected,
        sizeof(inline_expected),
        replacement_target
    ), "reject unexpected inline-hook signature");

    VirtualFree(memory, 0, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d hook test(s) failed\n", failures);
        return 1;
    }
    puts("call_hook_test: PASS");
    return 0;
}
