#include "engine/camera_target_abi.h"

#include <stdint.h>
#include <stdio.h>

typedef struct InstallCapture {
    void *target;
    uint32_t slot;
} InstallCapture;

typedef struct ReleaseCapture {
    void *target;
} ReleaseCapture;

__attribute__((naked, noinline))
static void capture_install(void) {
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "movl %eax, 0(%esi)\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl %eax, 4(%esi)\n\t"
        "ret $8\n\t"
    );
}

__attribute__((naked, noinline))
static void capture_release(void) {
    __asm__ volatile(
        "movl %edx, 0(%edi)\n\t"
        "ret\n\t"
    );
}

int main(void) {
    InstallCapture install = {0};
    ReleaseCapture release = {0};
    void *expected_target = (void *)(uintptr_t)0x12345678u;

    SudekiMpCallCameraTargetInstall(
        &install,
        expected_target,
        7u,
        capture_install
    );
    if (install.target != expected_target || install.slot != 7u) {
        fputs("FAIL: camera target install ABI mismatch\n", stderr);
        return 1;
    }

    SudekiMpCallCameraTargetRelease(
        &release,
        expected_target,
        capture_release
    );
    if (release.target != expected_target) {
        fputs("FAIL: camera target release ABI mismatch\n", stderr);
        return 1;
    }

    puts("camera_target_abi_test: PASS");
    return 0;
}
