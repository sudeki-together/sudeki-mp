#ifndef SUDEKIMP_LOADER_FIXED_THREE_PROFILE_H
#define SUDEKIMP_LOADER_FIXED_THREE_PROFILE_H

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

enum {
    SUDEKIMP_FIXED_THREE_REQUIRED_ENABLE_COUNT = 11u
};

typedef enum SudekiMpFixedThreeProfileFailure {
    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_NONE = 0,
    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_INVALID_ARGUMENT,
    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_DUPLICATE_REQUIRED_KEY,
    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED,
    SUDEKIMP_FIXED_THREE_PROFILE_FAILURE_REQUIRED_KEY_NOT_ENABLED
} SudekiMpFixedThreeProfileFailure;

typedef struct SudekiMpFixedThreeProfileState {
    uint32_t seen_required_mask;
    uint32_t enabled_required_mask;
    SudekiMpFixedThreeProfileFailure failure;
} SudekiMpFixedThreeProfileState;

BOOL SudekiMpConfigBooleanTextIsTrue(const wchar_t *value);

void SudekiMpFixedThreeProfileInitialize(
    SudekiMpFixedThreeProfileState *state);

BOOL SudekiMpFixedThreeProfileObserve(
    SudekiMpFixedThreeProfileState *state,
    const wchar_t *key,
    BOOL enabled);

BOOL SudekiMpFixedThreeProfileComplete(
    SudekiMpFixedThreeProfileState *state);

const wchar_t *SudekiMpFixedThreeProfileRequiredKey(unsigned int index);

const wchar_t *SudekiMpFixedThreeProfileFirstMissingKey(
    const SudekiMpFixedThreeProfileState *state);

#endif
