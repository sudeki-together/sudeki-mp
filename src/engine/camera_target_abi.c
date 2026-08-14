#include "engine/camera_target_abi.h"

#if !defined(__GNUC__) || !defined(__i386__)
#error "Camera target ABI adapters require 32-bit GCC assembly support"
#endif

__attribute__((naked, noinline))
void SudekiMpCallCameraTargetInstall(
    void *camera __attribute__((unused)),
    void *target __attribute__((unused)),
    unsigned int slot __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "movl 20(%esp), %eax\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "call *%eax\n\t"
        "popl %esi\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline))
void SudekiMpCallCameraTargetRelease(
    void *target_list __attribute__((unused)),
    void *target __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %edi\n\t"
        "movl 8(%esp), %edi\n\t"
        "movl 12(%esp), %edx\n\t"
        "movl 16(%esp), %eax\n\t"
        "call *%eax\n\t"
        "popl %edi\n\t"
        "ret\n\t"
    );
}
