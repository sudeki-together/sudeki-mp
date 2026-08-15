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
    SudekiMpExportHook export_hook = {0};
    SudekiMpInlineHook inline_hook = {0};
    uint32_t *export_slot;
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

    call_instruction[0] = 0x90;
    check(!SudekiMpInstallRelativeCallHook(
        &call_hook, call_instruction, original_target, replacement_target
    ), "reject non-CALL instruction");

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
