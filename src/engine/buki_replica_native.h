#ifndef SUDEKIMP_BUKI_REPLICA_NATIVE_H
#define SUDEKIMP_BUKI_REPLICA_NATIVE_H
#include <windows.h>

/* Game-thread only. The caller must hold the authenticated remote actor,
 * session/generation and damage-guard leases, and exclude a live CSkill.
 * These operations neither start attacks nor simulate world translation. */
BOOL SudekiMpBukiReplicaNativeImageMatches(HMODULE module);
/* Read-only positive task-drain witness before host body channels take over.
 * Unknown ownership or any active/queued native combo returns FALSE. */
BOOL SudekiMpBukiReplicaBodyAvailable(HMODULE module, void *character,
    void *expected_arbiter);
BOOL SudekiMpBukiReplicaSyncFacing(HMODULE module, void *character,
    void *expected_arbiter, const float direction[3]);
BOOL SudekiMpBukiReplicaInterruptAttack(HMODULE module, void *character,
    void *expected_arbiter);

#ifdef SUDEKIMP_BUKI_REPLICA_NATIVE_TESTING
typedef void (__attribute__((stdcall)) *SudekiMpBukiFacingCall)(
    void *, const float *, float, float, unsigned int);
typedef void (__attribute__((thiscall)) *SudekiMpBukiInterruptCall)(void *);
void SudekiMpBukiReplicaNativeTestCalls(SudekiMpBukiFacingCall facing,
    SudekiMpBukiInterruptCall interrupt);
#endif
#endif
