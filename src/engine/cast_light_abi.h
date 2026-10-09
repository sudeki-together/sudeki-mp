#ifndef SUDEKIMP_CAST_LIGHT_ABI_H
#define SUDEKIMP_CAST_LIGHT_ABI_H
#include <windows.h>
#include <stdint.h>

/* Game-thread only, exact-image native CLightManager lifetimes. The caller
 * retains the actor/session through natural task AND lighting drain. Keys are
 * non-reused Spirit-instance generations, never seat indices or wire IDs. */
typedef BOOL (*SudekiMpCastLightWitness)(void *actor,uint64_t session);
BOOL SudekiMpInitializeCastLightAbi(HMODULE image);
BOOL SudekiMpCreateCastLight(uint32_t key,void *actor,uint64_t session,
    SudekiMpCastLightWitness retained);
BOOL SudekiMpDestroyCastLight(uint32_t key);
BOOL SudekiMpResetCastLightAbi(void);
BOOL SudekiMpCastLightDrained(uint32_t key);
BOOL SudekiMpReadCastLight(uint32_t key,float current[3],float baseline[3]);
/* Integrated into the existing strict-LIFO Spirit scope transaction. Ready
 * validates both outgoing and incoming managers before ANY scope mutation.
 * Commit cannot fail and is called immediately after all preflights. Key 0
 * means world lighting. Disabled adapter is a no-op, not partial admission. */
BOOL SudekiMpCastLightTransitionReady(uint32_t key);
unsigned SudekiMpCastLightWorldAdoptions(void);
unsigned SudekiMpCastLightForeignSkips(void);
unsigned SudekiMpCastLightReadyFailure(void);
unsigned SudekiMpCastLightFaultSite(void);
unsigned SudekiMpCastLightFaultLine(void);
void SudekiMpCastLightTransitionCommit(uint32_t key);
/* Optional predicate: TRUE while retained-actor identity is temporarily
 * unknowable (native world load). Unknown is skipped, never faulted. */
void SudekiMpCastLightSetUnknownWitness(BOOL (*unknown)(void));
BOOL SudekiMpCastLightTransient(void);
unsigned SudekiMpCastLightTransientSkips(void);
#endif
