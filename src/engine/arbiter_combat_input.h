#ifndef SUDEKIMP_ARBITER_COMBAT_INPUT_H
#define SUDEKIMP_ARBITER_COMBAT_INPUT_H

#include <windows.h>

/*
 * Sudeki's combat-input function uses a nonstandard 32-bit ABI:
 * ECX carries the CCharacterArbiter, EAX carries Block state, and the native
 * callee removes five stack arguments for Weak, Strong, Sweep, Weapon Next,
 * and Weapon Previous. Keep that detail isolated behind this adapter.
 */
void SudekiMpSubmitArbiterCombatInput(
    void *target,
    void *arbiter,
    int weak,
    int strong,
    int sweep,
    int block,
    int weapon_next,
    int weapon_previous
);

/* Retail dodge: EAX is the arbiter, one callee-popped vector pointer.
 * Unlike locomotion, this enters the actor's own dodge component. */
BOOL SudekiMpArbiterDodgeImageMatches(HMODULE module);
unsigned char SudekiMpSubmitArbiterDodgeInput(
    void *target, void *arbiter, const float direction[3]);

/* CBlock's native reset calls the same synchronous state-zero cleanup used
 * after an admitted dodge. The caller must prove the exact component owner.
 * This is not a dodge submission and grants no protection or damage rights. */
BOOL SudekiMpBlockResetImageMatches(HMODULE module);
void SudekiMpResetNativeBlock(void *target, void *block);

#endif
