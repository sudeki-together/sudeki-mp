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

#define SUDEKIMP_CLEANROOM_PRESENTATION_CHANNELS 5u
#define SUDEKIMP_CLEANROOM_PRESENTATION_BLENDS 4u
typedef struct SudekiMpCleanroomActorPresentation {
    uint32_t submodel_count;
    int32_t selector[SUDEKIMP_CLEANROOM_PRESENTATION_CHANNELS];
    uint8_t state[SUDEKIMP_CLEANROOM_PRESENTATION_CHANNELS];
    float rate[SUDEKIMP_CLEANROOM_PRESENTATION_CHANNELS];
    float time[SUDEKIMP_CLEANROOM_PRESENTATION_CHANNELS];
    float blend[SUDEKIMP_CLEANROOM_PRESENTATION_BLENDS];
} SudekiMpCleanroomActorPresentation;

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor);
const char *SudekiMpCleanroomActorResource(SudekiMpCleanroomActor actor);

BOOL SudekiMpCleanroomEngineInitialize(HMODULE game_module);
BOOL SudekiMpCleanroomEngineResourceNameFromText(
    SudekiMpResourceName *resource_name,
    const char *text
);
/* Copy the exact 12-byte engine value and acquire one reference to its shared
 * backing store. This is for deferred calls whose ResourceName may describe a
 * start marker distinct from the human-readable destination string. */
BOOL SudekiMpCleanroomEngineRetainResourceNameExact(
    SudekiMpResourceName *destination,
    const SudekiMpResourceName *source
);
void SudekiMpCleanroomEngineReleaseResourceName(
    SudekiMpResourceName *resource_name
);
BOOL SudekiMpCleanroomEngineWorldReady(void);
BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor);
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor);
void *SudekiMpCleanroomEngineGenericEntity(const char *resource_name);
/* Prove the live native group and AI formation each contain exactly the four
 * retail heroes once, with Tal still leading the group. No pointer escapes. */
BOOL SudekiMpCleanroomEngineExactRetailPartyReady(void);
/* Additionally prove the restored control split: Ailish owns the native
 * player-override lease while Buki and Elco remain in native AI mode. */
BOOL SudekiMpCleanroomEngineExactPostRestoreControlsReady(void);
/* Pointer-free policy behind the persistent live-combat control check. */
BOOL SudekiMpCleanroomEnginePostRestoreControlTupleActive(
    int16_t tal_ref,
    uint8_t tal_mode,
    int16_t ailish_ref,
    uint8_t ailish_mode,
    int16_t buki_ref,
    uint8_t buki_mode,
    int16_t elco_ref,
    uint8_t elco_mode
);
/* Revalidate an already-proven control split during live combat. Sudeki's
 * native skill-camera path temporarily acquires refcounted control leases on
 * all eligible party actors. This accepts their balanced native leases
 * without weakening the initial exact-one Ailish admission proof. */
BOOL SudekiMpCleanroomEnginePostRestoreControlsActive(void);
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
BOOL SudekiMpCleanroomEngineActorFacing(
    SudekiMpCleanroomActor actor,
    float facing[2]
);
/* Read the attached native world presentation without advancing or mutating
 * its animation clock. This remains process-local diagnostic state and must
 * never be copied directly into an unauthenticated network packet. */
BOOL SudekiMpCleanroomEngineActorPresentation(
    SudekiMpCleanroomActor actor,
    SudekiMpCleanroomActorPresentation *presentation
);
/* Read/write the same named current HP/SP values the native HUD/stat code
 * exposes. The setter is reserved for an authenticated, non-authoritative LAN
 * presentation replica; gameplay authority must never call it on the host. */
BOOL SudekiMpCleanroomEngineActorResources(
    SudekiMpCleanroomActor actor,
    float *hit_points,
    float *skill_points
);
BOOL SudekiMpCleanroomEngineSetActorResources(
    SudekiMpCleanroomActor actor,
    float hit_points,
    float skill_points
);
BOOL SudekiMpCleanroomEngineSpawnActor(
    SudekiMpCleanroomActor actor,
    const float position[3]
);
/* Initialize one already-present retail party actor without applying the
 * cleanroom-wide inventory, Spirit Strike, fuel, or training-room policies.
 * This is intended for native asynchronous party spawns whose entity became
 * visible after the world's original initialization pass. */
BOOL SudekiMpCleanroomEngineInitializePartyActor(
    SudekiMpCleanroomActor actor
);
BOOL SudekiMpCleanroomEngineRemoveActor(SudekiMpCleanroomActor actor);
BOOL SudekiMpCleanroomEngineDummyPresent(void);
BOOL SudekiMpCleanroomEngineDummySnapshot(
    float position[3],
    float *hit_points
);
BOOL SudekiMpCleanroomEngineSetDummyHitPoints(float hit_points);
BOOL SudekiMpCleanroomEngineSpawnDummy(const float position[3]);
BOOL SudekiMpCleanroomEngineRemoveDummy(void);
BOOL SudekiMpCleanroomEngineCombatMode(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetCombatMode(BOOL enabled);
/* Re-run the native party arm pass after a control-owner handoff. */
BOOL SudekiMpCleanroomEngineRefreshCombatMode(void);
/* Hold Sudeki's native UI/control transition long enough to arm ranged actors. */
BOOL SudekiMpCleanroomEnginePrimeRangedCombat(void);
/* True only while the native ranged-arm transition still owns its timer/UI
 * lease. Callers may retry unchanged native validation after it clears. */
BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void);
BOOL SudekiMpCleanroomEngineFirstPersonMode(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetFirstPersonMode(BOOL enabled);
BOOL SudekiMpCleanroomEngineInfiniteSp(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetInfiniteSp(BOOL enabled);
BOOL SudekiMpCleanroomEngineInfiniteSpirit(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetInfiniteSpirit(BOOL enabled);
BOOL SudekiMpCleanroomEngineInfiniteJetpackFuel(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetInfiniteJetpackFuel(BOOL enabled);
BOOL SudekiMpCleanroomEnginePartyInvulnerable(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetPartyInvulnerable(BOOL enabled);
/* Reconcile the native refcount lease after party or level changes. */
BOOL SudekiMpCleanroomEngineMaintainPartyInvulnerability(BOOL enabled);
BOOL SudekiMpCleanroomEngineSetStoryTestSpeed(
    BOOL enabled,
    float multiplier
);
void SudekiMpCleanroomEngineMaintainResources(void);
void SudekiMpCleanroomEngineReset(void);

#endif
