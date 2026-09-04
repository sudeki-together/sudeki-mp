#include "loader/lan_arena_profile.h"

#include <wchar.h>

static int role_key(const wchar_t *key) {
    if (key == NULL) return 0;
    if (_wcsicmp(key, L"EnableLanArenaHostPrototype") == 0) return 1;
    if (_wcsicmp(key, L"EnableLanArenaClientPrototype") == 0) return 2;
    if (_wcsicmp(key, L"EnableControlSeparationPrototype") == 0) return 3;
    if (_wcsicmp(key, L"EnableCleanroomMenu") == 0) return 4;
    return 0;
}

void SudekiMpLanArenaProfileInitialize(SudekiMpLanArenaProfileState *state) {
    if (state == NULL) return;
    state->host_seen = 0u;
    state->client_seen = 0u;
    state->control_seen = 0u;
    state->cleanroom_seen = 0u;
    state->host_enabled = 0u;
    state->client_enabled = 0u;
    state->control_enabled = 0u;
    state->cleanroom_enabled = 0u;
    state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_NONE;
}

BOOL SudekiMpLanArenaProfileObserve(
    SudekiMpLanArenaProfileState *state,
    const wchar_t *key,
    BOOL enabled
) {
    int index;
    if (state == NULL || key == NULL) {
        if (state != NULL) {
            state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_INVALID_ARGUMENT;
        }
        return FALSE;
    }
    if (state->failure != SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_NONE) return FALSE;
    if (_wcsnicmp(key, L"Enable", 6u) != 0) return TRUE;
    index = role_key(key);
    if (index == 0) {
        if (!enabled) return TRUE;
        state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED;
        return FALSE;
    }
    if ((index == 1 && state->host_seen) || (index == 2 && state->client_seen) ||
        (index == 3 && state->control_seen) ||
        (index == 4 && state->cleanroom_seen)) {
        state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_DUPLICATE_KEY;
        return FALSE;
    }
    if (index == 1) {
        state->host_seen = 1u;
        state->host_enabled = enabled ? 1u : 0u;
    } else if (index == 2) {
        state->client_seen = 1u;
        state->client_enabled = enabled ? 1u : 0u;
    } else if (index == 3) {
        state->control_seen = 1u;
        state->control_enabled = enabled ? 1u : 0u;
    } else {
        state->cleanroom_seen = 1u;
        state->cleanroom_enabled = enabled ? 1u : 0u;
    }
    return TRUE;
}

BOOL SudekiMpLanArenaProfileComplete(
    SudekiMpLanArenaProfileState *state,
    SudekiMpLanArenaProfileRole *role
) {
    if (role != NULL) *role = SUDEKIMP_LAN_ARENA_PROFILE_ROLE_NONE;
    if (state == NULL) return FALSE;
    if (state->failure != SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_NONE) return FALSE;
    if (!state->host_seen || !state->client_seen || !state->control_seen ||
        !state->cleanroom_seen ||
        (!state->host_enabled && !state->client_enabled)) {
        state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_ROLE_MISSING;
        return FALSE;
    }
    if (state->host_enabled && state->client_enabled) {
        state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_BOTH_ROLES_ENABLED;
        return FALSE;
    }
    if (!state->control_enabled) {
        state->failure = SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED;
        return FALSE;
    }
    if ((state->host_enabled && !state->cleanroom_enabled) ||
        (state->client_enabled && state->cleanroom_enabled)) {
        state->failure =
            SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_HOST_TOOLS_MISMATCH;
        return FALSE;
    }
    if (role != NULL) {
        *role = state->host_enabled ? SUDEKIMP_LAN_ARENA_PROFILE_ROLE_HOST :
            SUDEKIMP_LAN_ARENA_PROFILE_ROLE_CLIENT;
    }
    return TRUE;
}
