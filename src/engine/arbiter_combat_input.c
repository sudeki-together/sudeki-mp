#include "engine/arbiter_combat_input.h"
#include <stdint.h>
#include <string.h>

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

BOOL SudekiMpArbiterDodgeImageMatches(HMODULE module) {
    const uint8_t *base=(const uint8_t *)module;
    static const uint8_t entry[]={0x56,0x57,0x8b,0xf8,0x8b,0x4f,0x50,
        0xf6,0xc1,0x02,0x76,0x5e,0x8b,0x47,0x58,0x83,0xe0,0x0f,
        0x3c,0x03,0x74,0x54,0xf6,0x47,0x60,0x02,0x74,0x4e};
    static const uint8_t gate[]={0x83,0xb8,0x34,0x01,0,0,0,0x75,0x42,
        0xf7,0xc1,0x74,0xf6,0xdb,0x02,0x77,0x3a};
    static const uint8_t tail[]={0xb0,0x01,0x5f,0x5e,0xc2,0x04,0,
        0x5f,0x32,0xc0,0x5e,0xc2,0x04,0};
    return base && !memcmp(base+0xdae00u,entry,sizeof(entry)) &&
        !memcmp(base+0xdae1fu,gate,sizeof(gate)) &&
        !memcmp(base+0xdae63u,tail,sizeof(tail)) &&
        base[0xdae40u]==0xe8 && *(const int32_t *)(base+0xdae41u)==0xaf38b &&
        base[0xdae5eu]==0xe8 && *(const int32_t *)(base+0xdae5fu)==0xaf75d;
}

__attribute__((naked, noinline))
unsigned char SudekiMpSubmitArbiterDodgeInput(
    void *target, void *arbiter, const float direction[3]) {
    __asm__ volatile(
        "movl 4(%esp), %edx\n\t"
        "movl 8(%esp), %eax\n\t"
        "pushl 12(%esp)\n\t"
        "call *%edx\n\t"
        "ret\n\t");
}

BOOL SudekiMpBlockResetImageMatches(HMODULE module) {
    const uint8_t *base=(const uint8_t *)module;
    static const uint8_t reset[]={0x57,0x33,0xd2,0x8b,0xf9,
        0xe8,0xe6,0,0,0,0x5f,0xc3};
    static const uint8_t state[]={0x51,0x53,0x56,0x39,0x57,0x18,
        0x0f,0x84,0xc8,0,0,0,0x8b,0x4f,0x10,
        0x8b,0x81,0x90,0,0,0,0x8b,0x59,0x5c,0x89,0x57,0x18,
        0x85,0xd2,0x75,0x68};
    static const uint8_t clear[]={0x83,0x60,0x50,0xf7};
    return base && SudekiMpArbiterDodgeImageMatches(module) &&
        !memcmp(base+0x18a4d0u,reset,sizeof(reset)) &&
        !memcmp(base+0x18a5c0u,state,sizeof(state)) &&
        !memcmp(base+0x18a5e7u,clear,sizeof(clear)) &&
        *(void *const *)(base+0x2d4b94u)==base+0x18a4d0u &&
        base[0x18a600u]==0xe8 && *(const int32_t *)(base+0x18a601u)==-0xc365 &&
        base[0x18a637u]==0xe8 && *(const int32_t *)(base+0x18a638u)==-0x8e2ec;
}

void SudekiMpResetNativeBlock(void *target, void *block) {
    ((void (__attribute__((thiscall)) *)(void *))target)(block);
}
