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
    uint32_t *export_slot;

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

    VirtualFree(memory, 0, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d hook test(s) failed\n", failures);
        return 1;
    }
    puts("call_hook_test: PASS");
    return 0;
}
