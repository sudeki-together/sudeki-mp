#include "engine/arbiter_combat_input.h"

#if !defined(__GNUC__) || !defined(__i386__)
#error "Arbiter combat input requires 32-bit GCC assembly support"
#endif

__attribute__((naked, noinline))
void SudekiMpSubmitArbiterCombatInput(
    void *target,
    void *arbiter,
    int weak,
    int strong,
    int sweep,
    int block,
    int weapon_next,
    int weapon_previous
) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "movl 8(%ebp), %edx\n\t"
        "pushl 36(%ebp)\n\t"
        "pushl 32(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "pushl 16(%ebp)\n\t"
        "movl 12(%ebp), %ecx\n\t"
        "movl 28(%ebp), %eax\n\t"
        "call *%edx\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}
