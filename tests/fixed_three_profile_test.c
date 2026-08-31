#include "loader/fixed_three_profile.h"

#include <stdio.h>
#include <wchar.h>

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

static const wchar_t *const expected_required[] = {
    L"EnableCoopRosterMenu",
    L"EnableControlSeparationPrototype",
    L"EnableSecondPlayerMovementPrototype",
    L"EnableSecondPlayerCameraRelativeMovementPrototype",
    L"EnableSecondPlayerWeakAttackPrototype",
    L"EnableThreeSeatUdpTransportPrototype",
    L"EnableSplitScreenRenderPrototype",
    L"EnableSecondPlayerCameraPrototype",
    L"EnableDualCameraFrameCachePrototype",
    L"EnableFixedThreeSeatRendererPrototype",
    L"EnableSecondPlayerControllerCameraPrototype"
};

static const wchar_t *const forbidden_enable_keys[] = {
    L"EnableCleanroomMenu",
    L"EnableLoadedSaveCoopAutostartPrototype",
    L"EnableZoneTraversalMenu",
    L"EnablePartyAtomicTransitionsPrototype",
    L"EnableTransitionVotePrototype",
    L"EnableSaveBookVotePrototype",
    L"EnableStoryTestBoost",
    L"EnableQuickMenuNormalSpeed",
    L"EnablePlasmaticaTrace",
    L"EnableQuickSkillInputTrace",
    L"EnableCharacterSwitchTrace",
    L"EnableTalosPartyPrototype",
    L"EnableExpandedTalosEncounterPrototype",
    L"EnableExpandedTalosLifecycleTrace",
    L"EnableTalosPostMoviePartyRestorePrototype",
    L"EnableTalosCompanionStagingObservation",
    L"EnableTalosDefenseTrace",
    L"EnableFreeRoamCameraModifierPrototype",
    L"EnablePlayerMovementTrace",
    L"EnableSecondPlayerSeparationGuardPrototype",
    L"EnableExternalInputBridgePrototype",
    L"EnableNativeXInputPlayerTwoPrototype",
    L"EnablePlayerInteractionRequestsPrototype",
    L"EnableMerchantCheckoutTracePrototype",
    L"EnablePerPlayerBlacksmithUiExperiment",
    L"EnableSecondPlayerTargetTrace",
    L"EnableSharedGroupCameraPrototype",
    L"EnableNativeSecondPlayerCameraCollisionPrototype",
    L"EnableSplitScreenRangedModelIsolationPrototype",
    L"EnableSpiritStrikeViewportEffectIsolationPrototype",
    L"EnableRangedQuickSkillPrototype",
    L"EnableRealtimeMultiplayerSkillCombatPrototype",
    L"EnableDirectSpiritStrikePrototype",
    L"EnablePlasmaticaAnimationSpeed",
    L"EnablePlasmaticaCameraSpeed"
};

static int failures;

static void check(BOOL condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

static BOOL observe_required(
    SudekiMpFixedThreeProfileState *state,
    int omitted,
    int disabled
) {
    unsigned int index;

    for (index = 0u; index < ARRAY_COUNT(expected_required); ++index) {
        if ((int)index == omitted) continue;
        if (!SudekiMpFixedThreeProfileObserve(
                state,
                expected_required[index],
                (int)index == disabled ? FALSE : TRUE)) {
            return FALSE;
        }
    }
    return TRUE;
}

static void test_exact_profile(void) {
    SudekiMpFixedThreeProfileState state;
    unsigned int index;

    check(ARRAY_COUNT(expected_required) ==
            SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT,
        "canonical required key count changed");
    check(ARRAY_COUNT(expected_required) +
            ARRAY_COUNT(forbidden_enable_keys) == 46u,
        "canonical config Enable-key inventory changed");
    for (index = 0u; index < ARRAY_COUNT(expected_required); ++index) {
        check(SudekiMpFixedThreeProfileRequiredKey(index) != NULL &&
                _wcsicmp(
                    SudekiMpFixedThreeProfileRequiredKey(index),
                    expected_required[index]) == 0,
            "DLL allowlist key differs from launcher canonical list");
    }
    check(SudekiMpFixedThreeProfileRequiredKey(
            SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT) == NULL,
        "out-of-range required key lookup succeeded");

    SudekiMpFixedThreeProfileInitialize(&state);
    check(observe_required(&state, -1, -1),
        "exact required profile was rejected while observing");
    check(SudekiMpFixedThreeProfileComplete(&state),
        "exact required profile did not complete");
}

static void test_missing_disabled_and_duplicate_required(void) {
    unsigned int index;

    for (index = 0u; index < ARRAY_COUNT(expected_required); ++index) {
        SudekiMpFixedThreeProfileState state;

        SudekiMpFixedThreeProfileInitialize(&state);
        check(observe_required(&state, (int)index, -1),
            "missing-key fixture was rejected before completion");
        check(!SudekiMpFixedThreeProfileComplete(&state) &&
                state.failure ==
                    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_REQUIRED_KEY_NOT_ENABLED &&
                SudekiMpFixedThreeProfileFirstMissingKey(&state) != NULL &&
                _wcsicmp(
                    SudekiMpFixedThreeProfileFirstMissingKey(&state),
                    expected_required[index]) == 0,
            "missing required key was not identified");

        SudekiMpFixedThreeProfileInitialize(&state);
        check(observe_required(&state, -1, (int)index),
            "disabled-key fixture was rejected before completion");
        check(!SudekiMpFixedThreeProfileComplete(&state) &&
                state.failure ==
                    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_REQUIRED_KEY_NOT_ENABLED,
            "disabled required key completed");
    }

    {
        SudekiMpFixedThreeProfileState state;

        SudekiMpFixedThreeProfileInitialize(&state);
        check(SudekiMpFixedThreeProfileObserve(
                &state, expected_required[0], TRUE),
            "first required key observation failed");
        check(!SudekiMpFixedThreeProfileObserve(
                &state, expected_required[0], TRUE) &&
                state.failure ==
                    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_DUPLICATE_REQUIRED_KEY,
            "duplicate required key was accepted");
    }
}

static void test_forbidden_enable_keys(void) {
    unsigned int index;

    {
        SudekiMpFixedThreeProfileState state;

        SudekiMpFixedThreeProfileInitialize(&state);
        for (index = 0u; index < ARRAY_COUNT(forbidden_enable_keys); ++index) {
            check(SudekiMpFixedThreeProfileObserve(
                    &state, forbidden_enable_keys[index], FALSE),
                "disabled forbidden key was rejected");
        }
        check(observe_required(&state, -1, -1) &&
                SudekiMpFixedThreeProfileComplete(&state),
            "disabled optional keys changed exact admission");
    }

    for (index = 0u; index < ARRAY_COUNT(forbidden_enable_keys); ++index) {
        SudekiMpFixedThreeProfileState state;

        SudekiMpFixedThreeProfileInitialize(&state);
        check(!SudekiMpFixedThreeProfileObserve(
                &state, forbidden_enable_keys[index], TRUE) &&
                state.failure ==
                    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED,
            "enabled forbidden config key was accepted");
    }
}

static void test_unknown_case_and_non_enable_keys(void) {
    SudekiMpFixedThreeProfileState state;

    SudekiMpFixedThreeProfileInitialize(&state);
    check(SudekiMpFixedThreeProfileObserve(
            &state, L"EnableFutureUnsafePrototype", FALSE),
        "future disabled Enable key was rejected");
    check(SudekiMpFixedThreeProfileObserve(
            &state, L"SkipStartupMovies", TRUE),
        "non-Enable launcher policy key affected DLL profile");
    check(!SudekiMpFixedThreeProfileObserve(
            &state, L"EnableFutureUnsafePrototype", TRUE) &&
            state.failure ==
                SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED,
        "future enabled key was not fail-closed");

    SudekiMpFixedThreeProfileInitialize(&state);
    check(SudekiMpFixedThreeProfileObserve(
            &state, L"enablecooprostermenu", TRUE),
        "case-insensitive required key was rejected");
    {
        unsigned int index;
        for (index = 1u; index < ARRAY_COUNT(expected_required); ++index) {
            check(SudekiMpFixedThreeProfileObserve(
                    &state, expected_required[index], TRUE),
                "case-insensitive profile fixture failed");
        }
    }
    check(SudekiMpFixedThreeProfileComplete(&state),
        "case-insensitive exact profile did not complete");
}

static void test_boolean_text(void) {
    static const wchar_t *const true_values[] = {
        L"true", L"TRUE", L"yes", L"YES", L"on", L"ON", L"1"
    };
    static const wchar_t *const false_values[] = {
        L"false", L"no", L"off", L"0", L"", L"garbage"
    };
    unsigned int index;

    for (index = 0u; index < ARRAY_COUNT(true_values); ++index) {
        check(SudekiMpConfigBooleanTextIsTrue(true_values[index]),
            "supported true spelling parsed false");
    }
    for (index = 0u; index < ARRAY_COUNT(false_values); ++index) {
        check(!SudekiMpConfigBooleanTextIsTrue(false_values[index]),
            "false spelling parsed true");
    }
    check(!SudekiMpConfigBooleanTextIsTrue(NULL),
        "null boolean text parsed true");
}

int main(void) {
    test_exact_profile();
    test_missing_disabled_and_duplicate_required();
    test_forbidden_enable_keys();
    test_unknown_case_and_non_enable_keys();
    test_boolean_text();
    if (failures != 0) {
        fprintf(stderr, "fixed three profile tests failed: %d\n", failures);
        return 1;
    }
    puts("fixed three profile tests passed");
    return 0;
}
