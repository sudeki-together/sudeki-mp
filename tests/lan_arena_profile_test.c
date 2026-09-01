#include "loader/lan_arena_profile.h"

#include <stdio.h>

static int failures;
#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); ++failures; \
} } while (0)

static void observe_pair(
    SudekiMpLanArenaProfileState *state,
    BOOL host,
    BOOL client,
    BOOL control
) {
    CHECK(SudekiMpLanArenaProfileObserve(state, L"EnableLanArenaHostPrototype", host));
    CHECK(SudekiMpLanArenaProfileObserve(state, L"EnableLanArenaClientPrototype", client));
    CHECK(SudekiMpLanArenaProfileObserve(
        state, L"EnableControlSeparationPrototype", control));
}

int main(void) {
    SudekiMpLanArenaProfileState state;
    SudekiMpLanArenaProfileRole role;
    SudekiMpLanArenaProfileInitialize(&state);
    observe_pair(&state, TRUE, FALSE, TRUE);
    CHECK(SudekiMpLanArenaProfileComplete(&state, &role));
    CHECK(role == SUDEKIMP_LAN_ARENA_PROFILE_ROLE_HOST);

    SudekiMpLanArenaProfileInitialize(&state);
    observe_pair(&state, FALSE, TRUE, TRUE);
    CHECK(SudekiMpLanArenaProfileComplete(&state, &role));
    CHECK(role == SUDEKIMP_LAN_ARENA_PROFILE_ROLE_CLIENT);

    SudekiMpLanArenaProfileInitialize(&state);
    observe_pair(&state, TRUE, TRUE, TRUE);
    CHECK(!SudekiMpLanArenaProfileComplete(&state, &role));
    CHECK(state.failure == SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_BOTH_ROLES_ENABLED);

    SudekiMpLanArenaProfileInitialize(&state);
    observe_pair(&state, FALSE, FALSE, FALSE);
    CHECK(!SudekiMpLanArenaProfileComplete(&state, &role));
    CHECK(state.failure == SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_ROLE_MISSING);

    SudekiMpLanArenaProfileInitialize(&state);
    observe_pair(&state, TRUE, FALSE, FALSE);
    CHECK(!SudekiMpLanArenaProfileComplete(&state, &role));
    CHECK(state.failure == SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED);

    SudekiMpLanArenaProfileInitialize(&state);
    observe_pair(&state, FALSE, TRUE, FALSE);
    CHECK(!SudekiMpLanArenaProfileComplete(&state, &role));
    CHECK(state.failure == SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED);

    SudekiMpLanArenaProfileInitialize(&state);
    CHECK(!SudekiMpLanArenaProfileObserve(&state, L"EnableSplitScreenRenderPrototype", TRUE));
    CHECK(state.failure == SUDEKIMP_LAN_ARENA_PROFILE_FAILURE_FORBIDDEN_KEY_ENABLED);

    if (failures != 0) return 1;
    puts("lan arena profile tests passed");
    return 0;
}
