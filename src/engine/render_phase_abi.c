#include "engine/render_phase_abi.h"

#if !defined(__GNUC__) || !defined(__i386__)
#error "Render-phase ABI adapter requires 32-bit GCC assembly support"
#endif

__attribute__((naked, noinline))
void SudekiMpCallRenderPhase(
    void *renderer __attribute__((unused)),
    void *world_context __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %edi\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %edi\n\t"
        "movl 16(%esp), %edx\n\t"
        "call *%edx\n\t"
        "popl %edi\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline))
void SudekiMpCallRenderPhaseWithFloat(
    void *renderer __attribute__((unused)),
    void *world_context __attribute__((unused)),
    unsigned int value_bits __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %edi\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %edi\n\t"
        "movl 20(%esp), %edx\n\t"
        "pushl 16(%esp)\n\t"
        "call *%edx\n\t"
        "popl %edi\n\t"
        "ret\n\t"
    );
}
