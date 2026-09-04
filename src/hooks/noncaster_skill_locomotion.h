#ifndef SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_H
#define SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_H

#include <windows.h>

typedef enum SudekiMpNoncasterSkillLocomotionActor {
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TAL = 0,
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_AILISH = 1
} SudekiMpNoncasterSkillLocomotionActor;

typedef enum SudekiMpNoncasterSkillLocomotionResult {
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED = 0,
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_INACTIVE = 1,
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_STABLE = 2,
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_APPLIED = 3
} SudekiMpNoncasterSkillLocomotionResult;

/*
 * Presentation-only ownership for one actor while another actor owns a native
 * CSkill/Spirit transaction.  Callers own the exact non-caster/session/modal
 * predicate and pass it as ownership_active.  A false predicate only releases
 * this lease; it never writes a restorative animation.
 *
 * Service this after Sudeki's final RenderStart and before world submission.
 * The adapter owns only animation channels 0/1 and blend 0.  It deliberately
 * never invokes a gameplay animation controller or task/global tick; a drift
 * repair reapplies only the lease's independently tracked real-time phase.
 */
typedef struct SudekiMpNoncasterSkillLocomotionLease {
    void *character;
    void *component;
    void *wrapper;
    void *renderer;
    unsigned int submodel_count;
    SudekiMpNoncasterSkillLocomotionActor actor;
    BOOL moving;
    /*
     * Sudeki can rewrite the non-caster's base channels every frame while a
     * native skill owns the process-global animation state.  Keep an
     * independent real-time phase for each channel so a repair does not
     * restart both clips at frame zero.  DWORD subtraction is intentionally
     * used by the implementation so GetTickCount wrap remains well-defined.
     */
    DWORD channel_phase_tick_ms[2];
    double channel_phase_time[2];
    BOOL channel_phase_valid[2];
    BOOL valid;
} SudekiMpNoncasterSkillLocomotionLease;

SudekiMpNoncasterSkillLocomotionResult
SudekiMpNoncasterSkillLocomotionService(
    HMODULE game_module,
    void *character,
    SudekiMpNoncasterSkillLocomotionActor actor,
    BOOL ownership_active,
    BOOL moving,
    SudekiMpNoncasterSkillLocomotionLease *lease,
    const char **reason_result
);

void SudekiMpNoncasterSkillLocomotionRelease(
    SudekiMpNoncasterSkillLocomotionLease *lease
);

#ifdef SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TESTING
/* Pure topology seam for the exact LAN-cleanroom Ailish model shape. */
BOOL SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
    void *attached_wrapper,
    BOOL attached_wrapper_readable,
    void *first_person_wrapper,
    BOOL first_person_wrapper_readable,
    void *saved_world_wrapper,
    BOOL saved_world_wrapper_readable,
    void **wrapper_result
);
/* Deterministic seam for the wall-clock phase policy; never exported by the
 * production DLL. */
BOOL SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
    SudekiMpNoncasterSkillLocomotionLease *lease,
    BOOL reset,
    DWORD now_ms,
    float channel_zero_rate,
    float channel_one_rate,
    float phase_result[2]
);
/* The production STABLE decision includes these same three predicates.  This
 * seam proves that matching selectors/state/rates/blend cannot hide a native
 * channel-time reset or stale frame. */
BOOL SudekiMpNoncasterSkillLocomotionTestStablePredicate(
    BOOL presentation_matches_value,
    BOOL transition_states_match_value,
    const float expected_phases[2],
    const float current_phases[2]
);
#endif

#endif
