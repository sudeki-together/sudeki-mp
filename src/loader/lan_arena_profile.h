#ifndef SUDEKIMP_LOADER_LAN_ARENA_PROFILE_H
#define SUDEKIMP_LOADER_LAN_ARENA_PROFILE_H

#include <windows.h>

#include <stdint.h>

typedef enum SudekiMpLanArenaProfileRole {
    SUDEKIMP_LAN_ARENA_PROFILE_ROLE_NONE = 0,
    SUDEKIMP_LAN_ARENA_PROFILE_ROLE_HOST,
    SUDEKIMP_LAN_ARENA_PROFILE_ROLE_CLIENT
} SudekiMpLanArenaProfileRole;

typedef enum SudekiMpLanArenaProfileFailure {
    SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_NONE = 0,
    SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_INVALID_ARGUMENT,
    SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_DUPLICATE_KEY,
    SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED,
    SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_ROLE_MISSING,
    SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_BOTH_ROLES_ENABLED
} SudekiMpLanArenaProfileFailure;

typedef struct SudekiMpLanArenaProfileState {
    uint8_t host_seen;
    uint8_t client_seen;
    uint8_t control_seen;
    uint8_t host_enabled;
    uint8_t client_enabled;
    uint8_t control_enabled;
    SudekiMpLanArenaProfileFailure failure;
} SudekiMpLanArenaProfileState;

void SudekiMpLanArenaProfileInitialize(SudekiMpLanArenaProfileState *state);
BOOL SudekiMpLanArenaProfileObserve(
    SudekiMpLanArenaProfileState *state,
    const wchar_t *key,
    BOOL enabled
);
BOOL SudekiMpLanArenaProfileComplete(
    SudekiMpLanArenaProfileState *state,
    SudekiMpLanArenaProfileRole *role
);

#endif
