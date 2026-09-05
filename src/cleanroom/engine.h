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
/* Opt-in single-player testroom comparison. Protects both party and Woluf
 * with native invulnerability; no AI, movement, animation or speed override. */
BOOL SudekiMpCleanroomEngineEnableNativeAiProbe(void);
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
/* Hold Sudeki's native UI/control transition for at least 75 ms so ranged
 * actors observe the retail arm cycle. Completion is serviced synchronously
 * on the game thread; no TimerProc remains callable after teardown. */
BOOL SudekiMpCleanroomEnginePrimeRangedCombat(void);
/* Pure witness for the native UI lease. MaintainResources retires due work on
 * its owner game thread; teardown callers must defer while this returns TRUE. */
BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void);
BOOL SudekiMpCleanroomEngineFirstPersonMode(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetFirstPersonMode(BOOL enabled);
BOOL SudekiMpCleanroomEngineInfiniteSp(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetInfiniteSp(BOOL enabled);
/* Cleanroom-only reversible lease over each present retail hero's six native
 * SkillData availability bytes. Native validators and CSkill::Use still own
 * every activation. */
BOOL SudekiMpCleanroomEngineTrainingSkills(BOOL *enabled);
BOOL SudekiMpCleanroomEngineSetTrainingSkills(BOOL enabled);
/* Read-only witness for Sudeki's retail-global Spirit presentation
 * transaction. Zero is inactive; nonzero values are native internal stages
 * and must not be interpreted as actor-local CSkill slots. */
BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *state);
BOOL SudekiMpCleanroomEngineSpiritStrikeId(int *strike_id);
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

#if defined(SUDEKIMP_CLEANROOM_ENGINE_TESTING)
BOOL SudekiMpCleanroomEngineTrainingSkillLeaseForTesting(
    unsigned int actor_index, void *actor, BOOL enabled);
typedef struct SudekiMpCleanroomEngineRangedPrimeTestBackend {
    BOOL (*set_native_ui_active)(BOOL active);
    BOOL (*combat_mode)(BOOL *enabled);
    DWORD (*tick_count)(void);
    DWORD (*thread_id)(void);
} SudekiMpCleanroomEngineRangedPrimeTestBackend;

BOOL SudekiMpCleanroomEngineSetRangedPrimeTestBackend(
    const SudekiMpCleanroomEngineRangedPrimeTestBackend *backend
);
void SudekiMpCleanroomEngineResetRangedPrimeForTesting(void);
uint32_t SudekiMpCleanroomEngineRangedPrimeGenerationForTesting(void);
BOOL SudekiMpCleanroomEngineServiceRangedPrimeForTesting(
    uint32_t generation
);
BOOL SudekiMpCleanroomEngineCancelRangedPrimeForTesting(void);
/* Exact cached CSkill allocation/owner/readability gate used by the live
 * training-skill lease before any SkillData array dereference. */
BOOL SudekiMpCleanroomEngineTrainingSkillAllocationValidForTesting(
    const void *actor,
    const void *skill
);
#endif

#endif
