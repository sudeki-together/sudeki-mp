#include "loader/fixed_three_profile.h"

#include <wchar.h>

static const wchar_t *const required_enable_keys[] = {
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

static uint32_t required_mask(void) {
    return (UINT32_C(1) << SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT) - 1u;
}

static int required_key_index(const wchar_t *key) {
    unsigned int index;

    if (key == NULL) return -1;
    for (index = 0u; index < SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT;
            ++index) {
        if (_wcsicmp(key, required_enable_keys[index]) == 0) {
            return (int)index;
        }
    }
    return -1;
}

BOOL SudekiMpConfigBooleanTextIsTrue(const wchar_t *value) {
    return value != NULL &&
        (_wcsicmp(value, L"true") == 0 ||
         _wcsicmp(value, L"yes") == 0 ||
         _wcsicmp(value, L"on") == 0 ||
         wcscmp(value, L"1") == 0);
}

void SudekiMpFixedThreeProfileInitialize(
    SudekiMpFixedThreeProfileState *state
) {
    if (state == NULL) return;
    state->seen_required_mask = 0u;
    state->enabled_required_mask = 0u;
    state->failure = SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_NONE;
}

BOOL SudekiMpFixedThreeProfileObserve(
    SudekiMpFixedThreeProfileState *state,
    const wchar_t *key,
    BOOL enabled
) {
    int index;
    uint32_t bit;

    if (state == NULL || key == NULL) {
        if (state != NULL) {
            state->failure =
                SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_INVALID_ARGUMENT;
        }
        return FALSE;
    }
    if (state->failure != SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_NONE) {
        return FALSE;
    }
    if (_wcsnicmp(key, L"Enable", 6u) != 0) return TRUE;
    index = required_key_index(key);
    if (index < 0) {
        if (!enabled) return TRUE;
        state->failure =
            SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED;
        return FALSE;
    }
    bit = UINT32_C(1) << (unsigned int)index;
    if ((state->seen_required_mask & bit) != 0u) {
        state->failure =
            SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_DUPLICATE_REQUIRED_KEY;
        return FALSE;
    }
    state->seen_required_mask |= bit;
    if (enabled) state->enabled_required_mask |= bit;
    return TRUE;
}

BOOL SudekiMpFixedThreeProfileComplete(
    SudekiMpFixedThreeProfileState *state
) {
    uint32_t exact_mask = required_mask();

    if (state == NULL) return FALSE;
    if (state->failure != SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_NONE) {
        return FALSE;
    }
    if (state->seen_required_mask != exact_mask ||
        state->enabled_required_mask != exact_mask) {
        state->failure =
            SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_REQUIRED_KEY_NOT_ENABLED;
        return FALSE;
    }
    return TRUE;
}

const wchar_t *SudekiMpFixedThreeProfileRequiredKey(unsigned int index) {
    return index < SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT ?
        required_enable_keys[index] : NULL;
}

const wchar_t *SudekiMpFixedThreeProfileFirstMissingKey(
    const SudekiMpFixedThreeProfileState *state
) {
    unsigned int index;

    if (state == NULL) return NULL;
    for (index = 0u; index < SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT;
            ++index) {
        if ((state->enabled_required_mask &
                (UINT32_C(1) << index)) == 0u) {
            return required_enable_keys[index];
        }
    }
    return NULL;
}
