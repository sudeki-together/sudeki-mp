#include "engine/arbiter_combat_input.h"

#include <stdint.h>
#include <stdio.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Arbiter combat input test requires 32-bit GCC assembly support"
#endif

volatile uintptr_t test_arbiter;
volatile int test_weak;
volatile int test_strong;
volatile int test_sweep;
volatile int test_block;
volatile int test_weapon_next;
volatile int test_weapon_previous;
volatile uintptr_t test_dodge_direction;

static void __attribute__((thiscall)) mock_native_block_reset(void *block) {
    test_arbiter=(uintptr_t)block;
    ++test_block;
}

__attribute__((naked, noinline, used))
static void mock_native_dodge_input(void) {
    __asm__ volatile(
        "movl %eax, _test_arbiter\n\t"
        "movl 4(%esp), %edx\n\t"
        "movl %edx, _test_dodge_direction\n\t"
        "movl $1, %eax\n\t"
        "ret $4\n\t");
}

__attribute__((naked, noinline, used))
static void mock_native_combat_input(void) {
    __asm__ volatile(
        "movl %ecx, _test_arbiter\n\t"
        "movl %eax, _test_block\n\t"
        "movl 4(%esp), %eax\n\t"
        "movl %eax, _test_weak\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl %eax, _test_strong\n\t"
        "movl 12(%esp), %eax\n\t"
        "movl %eax, _test_sweep\n\t"
        "movl 16(%esp), %eax\n\t"
        "movl %eax, _test_weapon_next\n\t"
        "movl 20(%esp), %eax\n\t"
        "movl %eax, _test_weapon_previous\n\t"
        "ret $20\n\t"
    );
}

int main(void) {
    void *expected_arbiter = (void *)(uintptr_t)0x12345678u;

    SudekiMpSubmitArbiterCombatInput(
        (void *)mock_native_combat_input,
        expected_arbiter,
        11,
        22,
        33,
        44,
        55,
        66
    );

    if (test_arbiter != (uintptr_t)expected_arbiter ||
        test_weak != 11 || test_strong != 22 || test_sweep != 33 ||
        test_block != 44 || test_weapon_next != 55 ||
        test_weapon_previous != 66) {
        fprintf(stderr,
            "FAIL: arbiter=%08lx weak=%d strong=%d sweep=%d block=%d next=%d previous=%d\n",
            (unsigned long)test_arbiter,
            test_weak,
            test_strong,
            test_sweep,
            test_block,
            test_weapon_next,
            test_weapon_previous
        );
        return 1;
    }

    {
        const float direction[3]={0.6f,0.0f,0.8f};
        if(SudekiMpSubmitArbiterDodgeInput((void *)mock_native_dodge_input,
                expected_arbiter,direction)!=1u ||
            test_arbiter!=(uintptr_t)expected_arbiter ||
            test_dodge_direction!=(uintptr_t)direction) {
            fputs("FAIL: native dodge register/stack/result ABI\n",stderr);
            return 1;
        }
    }
    test_arbiter=0;
    test_block=0;
    SudekiMpResetNativeBlock((void *)mock_native_block_reset,expected_arbiter);
    if(test_arbiter!=(uintptr_t)expected_arbiter || test_block!=1) {
        fputs("FAIL: native block reset thiscall ABI\n",stderr);
        return 1;
    }
    puts("arbiter_combat_input_test: PASS");
    return 0;
}
