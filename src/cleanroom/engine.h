#ifndef SUDEKIMP_CLEANROOM_ENGINE_H
#define SUDEKIMP_CLEANROOM_ENGINE_H

#include <windows.h>
typedef enum SudekiMpCleanroomActor {
    SUDEKIMP_CLEANROOM_TAL = 0,
    SUDEKIMP_CLEANROOM_BUKI = 1,
    SUDEKIMP_CLEANROOM_ELCO = 2,
    SUDEKIMP_CLEANROOM_AILISH = 3,
    SUDEKIMP_CLEANROOM_ACTOR_COUNT = 4
} SudekiMpCleanroomActor;

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor);
const char *SudekiMpCleanroomActorResource(SudekiMpCleanroomActor actor);

BOOL SudekiMpCleanroomEngineInitialize(HMODULE game_module);
BOOL SudekiMpCleanroomEngineWorldReady(void);
BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor);
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor);
BOOL SudekiMpCleanroomEngineActorPosition(
    SudekiMpCleanroomActor actor,
    float position[3]
);
BOOL SudekiMpCleanroomEngineSpawnActor(
    SudekiMpCleanroomActor actor,
    const float position[3]
);
BOOL SudekiMpCleanroomEngineRemoveActor(SudekiMpCleanroomActor actor);
BOOL SudekiMpCleanroomEngineDummyPresent(void);
BOOL SudekiMpCleanroomEngineSpawnDummy(const float position[3]);
BOOL SudekiMpCleanroomEngineRemoveDummy(void);
BOOL SudekiMpCleanroomEngineCombatMode(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetCombatMode(BOOL enabled);
/* Re-run the native party arm pass after a control-owner handoff. */
BOOL SudekiMpCleanroomEngineRefreshCombatMode(void);
/* Hold Sudeki's native UI/control transition long enough to arm ranged actors. */
BOOL SudekiMpCleanroomEnginePrimeRangedCombat(void);
BOOL SudekiMpCleanroomEngineFirstPersonMode(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetFirstPersonMode(BOOL enabled);
BOOL SudekiMpCleanroomEngineInfiniteSp(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetInfiniteSp(BOOL enabled);
BOOL SudekiMpCleanroomEngineInfiniteSpirit(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetInfiniteSpirit(BOOL enabled);
void SudekiMpCleanroomEngineMaintainResources(void);
void SudekiMpCleanroomEngineReset(void);

#endif
