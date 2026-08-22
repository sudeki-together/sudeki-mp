#ifndef SUDEKIMP_CLEANROOM_ENGINE_H
#define SUDEKIMP_CLEANROOM_ENGINE_H

#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpResourceName {
    uint32_t encoded_kind;
    uint32_t identifier;
    uint32_t *text_reference;
} SudekiMpResourceName;
typedef enum SudekiMpCleanroomActor {
    SUDEKIMP_CLEANROOM_TAL = 0,
    SUDEKIMP_CLEANROOM_BUKI = 1,
    SUDEKIMP_CLEANROOM_ELCO = 2,
    SUDEKIMP_CLEANROOM_AILISH = 3,
    /* Authored developer-test PC. Keep outside the retail four-card UI. */
    SUDEKIMP_CLEANROOM_CAFU = 4,
    SUDEKIMP_CLEANROOM_ACTOR_COUNT = 5
} SudekiMpCleanroomActor;

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor);
const char *SudekiMpCleanroomActorResource(SudekiMpCleanroomActor actor);

BOOL SudekiMpCleanroomEngineInitialize(HMODULE game_module);
BOOL SudekiMpCleanroomEngineResourceNameFromText(
    SudekiMpResourceName *resource_name,
    const char *text
);
void SudekiMpCleanroomEngineReleaseResourceName(
    SudekiMpResourceName *resource_name
);
BOOL SudekiMpCleanroomEngineWorldReady(void);
BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor);
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor);
void *SudekiMpCleanroomEngineGenericEntity(const char *resource_name);
BOOL SudekiMpCleanroomEngineActorTargetsAllies(
    SudekiMpCleanroomActor actor,
    BOOL *enabled
);
BOOL SudekiMpCleanroomEngineSetActorTargetsAllies(
    SudekiMpCleanroomActor actor,
    BOOL enabled
);
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
