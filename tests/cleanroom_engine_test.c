#include "cleanroom/engine.h"

#include <stdio.h>

static int require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "cleanroom_engine_test: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    unsigned int actor;
    BOOL mode = FALSE;

    for (actor = 0u; actor < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++actor) {
        if (!require_true(
                SudekiMpCleanroomActorLabel(
                    (SudekiMpCleanroomActor)actor) != NULL,
                "actor label is missing") ||
            !require_true(
                SudekiMpCleanroomActorResource(
                    (SudekiMpCleanroomActor)actor) != NULL,
                "actor resource is missing")) {
            return 1;
        }
    }
    if (!require_true(
            !SudekiMpCleanroomEngineCombatMode(&mode),
            "combat state should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineFirstPersonMode(&mode),
            "camera state should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineInfiniteSp(&mode),
            "infinite SP should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineInfiniteSpirit(&mode),
            "infinite spirit should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineInfiniteJetpackFuel(&mode),
            "infinite jetpack should be unavailable before initialization")) {
        return 1;
    }
    puts("cleanroom_engine_test: pass");
    return 0;
}
