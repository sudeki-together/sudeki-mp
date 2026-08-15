#include "engine/render_phase_abi.h"

#include <stdint.h>
#include <stdio.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Render-phase ABI test requires 32-bit GCC assembly support"
#endif

void *captured_renderer __attribute__((used));
void *captured_world_context __attribute__((used));
uint32_t captured_float_bits __attribute__((used));

__attribute__((naked, noinline, used))
static void capture_render_phase(void) {
    __asm__ volatile(
        "movl %eax, _captured_renderer\n\t"
        "movl %edi, _captured_world_context\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static void capture_render_phase_with_float(void) {
    __asm__ volatile(
        "movl %eax, _captured_renderer\n\t"
        "movl %edi, _captured_world_context\n\t"
        "movl 4(%esp), %eax\n\t"
        "movl %eax, _captured_float_bits\n\t"
        "ret $4\n\t"
    );
}

int main(void) {
    void *renderer = (void *)(uintptr_t)0x12345678u;
    void *world_context = (void *)(uintptr_t)0x76543210u;

    SudekiMpCallRenderPhase(
        renderer,
        world_context,
        capture_render_phase
    );
    if (captured_renderer != renderer) {
        fputs("FAIL: renderer was not supplied in EAX\n", stderr);
        return 1;
    }
    if (captured_world_context != world_context) {
        fputs("FAIL: world context was not supplied in EDI\n", stderr);
        return 1;
    }
    captured_renderer = NULL;
    captured_world_context = NULL;
    captured_float_bits = 0u;
    SudekiMpCallRenderPhaseWithFloat(
        renderer,
        world_context,
        0x3f123456u,
        capture_render_phase_with_float
    );
    if (captured_renderer != renderer) {
        fputs("FAIL: float-phase renderer was not supplied in EAX\n", stderr);
        return 1;
    }
    if (captured_world_context != world_context) {
        fputs("FAIL: float-phase world context was not supplied in EDI\n",
            stderr);
        return 1;
    }
    if (captured_float_bits != 0x3f123456u) {
        fputs("FAIL: float-phase stack argument or cleanup was incorrect\n",
            stderr);
        return 1;
    }
    puts("render_phase_abi_test: PASS");
    return 0;
}
