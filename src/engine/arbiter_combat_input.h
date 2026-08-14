#ifndef SUDEKIMP_ARBITER_COMBAT_INPUT_H
#define SUDEKIMP_ARBITER_COMBAT_INPUT_H

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

#endif
