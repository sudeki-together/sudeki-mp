#include "hooks/noncaster_skill_locomotion.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static BOOL float_near(float actual, float expected) {
    return isfinite(actual) && fabsf(actual - expected) <= 0.0001f;
}

static void test_ailish_world_wrapper_topology_is_exact(void) {
    void *attached = (void *)(uintptr_t)0x1000u;
    void *first_person = (void *)(uintptr_t)0x2000u;
    void *saved_world = (void *)(uintptr_t)0x3000u;
    void *selected = (void *)(uintptr_t)0xdeadbeefu;

    /* Preserve the retail path whenever the saved-world lease exists. */
    CHECK(SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
        attached, TRUE, first_person, TRUE, saved_world, TRUE, &selected));
    CHECK(selected == saved_world);

    /* The LAN cleanroom has an exact NULL +0x164 slot.  Its attached wrapper
     * is accepted only when the distinct first-person half proves topology. */
    selected = NULL;
    CHECK(SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
        attached, TRUE, first_person, TRUE, NULL, FALSE, &selected));
    CHECK(selected == attached);

    selected = (void *)(uintptr_t)0xdeadbeefu;
    CHECK(!SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
        first_person, TRUE, first_person, TRUE, NULL, FALSE, &selected));
    CHECK(selected == NULL);
    CHECK(!SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
        attached, FALSE, first_person, TRUE, NULL, FALSE, &selected));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
        attached, TRUE, first_person, FALSE, NULL, FALSE, &selected));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
        attached, TRUE, first_person, TRUE, saved_world, FALSE, &selected));
}

static void test_repeated_renderer_drift_preserves_realtime_phase(void) {
    SudekiMpNoncasterSkillLocomotionLease lease;
    float phase[2];

    memset(&lease, 0, sizeof(lease));
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, TRUE, 1000u, 37.17093f, 30.97577f, phase));
    CHECK(float_near(phase[0], 0.0f));
    CHECK(float_near(phase[1], 0.0f));

    /* Each call represents Sudeki rewriting selector/state immediately before
     * the post-RenderStart repair.  Drift is not an ownership transition, so
     * neither channel may restart at zero. */
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 1016u, 37.17093f, 30.97577f, phase));
    CHECK(float_near(phase[0], 37.17093f * 0.016f));
    CHECK(float_near(phase[1], 30.97577f * 0.016f));
    CHECK(phase[0] > 0.0f && phase[1] > 0.0f);

    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 1032u, 37.17093f, 30.97577f, phase));
    CHECK(float_near(phase[0], 37.17093f * 0.032f));
    CHECK(float_near(phase[1], 30.97577f * 0.032f));

    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 1049u, 37.17093f, 30.97577f, phase));
    CHECK(float_near(phase[0], 37.17093f * 0.049f));
    CHECK(float_near(phase[1], 30.97577f * 0.049f));
}

static void test_only_ownership_or_profile_transition_resets_phase(void) {
    SudekiMpNoncasterSkillLocomotionLease lease;
    float phase[2];

    memset(&lease, 0, sizeof(lease));
    /* Ownership acquisition. */
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, TRUE, 2000u, 41.22882f, 30.92161f, phase));
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 2100u, 41.22882f, 30.92161f, phase));
    CHECK(float_near(phase[0], 4.122882f));
    CHECK(float_near(phase[1], 3.092161f));

    /* Moving -> idle is a semantic profile transition and intentionally
     * starts the idle clip once. */
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, TRUE, 2110u, 12.0f, 0.0f, phase));
    CHECK(float_near(phase[0], 0.0f));
    CHECK(float_near(phase[1], 0.0f));
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 2160u, 12.0f, 0.0f, phase));
    CHECK(float_near(phase[0], 0.6f));
    CHECK(float_near(phase[1], 0.0f));

    /* A new owner/profile lease starts independently. */
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, TRUE, 3000u, 37.17093f, 30.97577f, phase));
    CHECK(float_near(phase[0], 0.0f));
    CHECK(float_near(phase[1], 0.0f));
}

static void test_tick_wrap_and_invalid_clock_fail_closed(void) {
    SudekiMpNoncasterSkillLocomotionLease lease;
    float phase[2];

    memset(&lease, 0, sizeof(lease));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 10u, 24.0f, 12.0f, phase));

    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, TRUE, 0xfffffff0u, 24.0f, 12.0f, phase));
    CHECK(SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 0x00000010u, 24.0f, 12.0f, phase));
    CHECK(float_near(phase[0], 24.0f * 0.032f));
    CHECK(float_near(phase[1], 12.0f * 0.032f));

    CHECK(!SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
        &lease, FALSE, 0x00000020u, NAN, 12.0f, phase));
    SudekiMpNoncasterSkillLocomotionRelease(&lease);
    CHECK(!lease.valid);
    CHECK(!lease.channel_phase_valid[0]);
    CHECK(!lease.channel_phase_valid[1]);
    CHECK(lease.channel_phase_time[0] == 0.0);
    CHECK(lease.channel_phase_time[1] == 0.0);
}

static void test_stale_native_time_is_drift_not_stable(void) {
    const float expected_phase[2] = {0.5947349f, 0.4956123f};
    const float exact_native_phase[2] = {0.5947349f, 0.4956123f};
    const float reset_native_phase[2] = {0.0f, 0.0f};
    const float stale_secondary_phase[2] = {0.5947349f, 0.25f};

    /* The first two TRUE values stand for an otherwise exact renderer:
     * selectors, states, rates and blend all match the moving profile. */
    CHECK(SudekiMpNoncasterSkillLocomotionTestStablePredicate(
        TRUE, TRUE, expected_phase, exact_native_phase));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestStablePredicate(
        TRUE, TRUE, expected_phase, reset_native_phase));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestStablePredicate(
        TRUE, TRUE, expected_phase, stale_secondary_phase));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestStablePredicate(
        FALSE, TRUE, expected_phase, exact_native_phase));
    CHECK(!SudekiMpNoncasterSkillLocomotionTestStablePredicate(
        TRUE, FALSE, expected_phase, exact_native_phase));
}

int main(void) {
    test_ailish_world_wrapper_topology_is_exact();
    test_repeated_renderer_drift_preserves_realtime_phase();
    test_only_ownership_or_profile_transition_resets_phase();
    test_tick_wrap_and_invalid_clock_fail_closed();
    test_stale_native_time_is_drift_not_stable();
    if (failures != 0) {
        fprintf(stderr, "%d non-caster skill locomotion test(s) failed\n",
            failures);
        return 1;
    }
    puts("Non-caster skill locomotion tests passed");
    return 0;
}
