#include "engine/player_combat_context.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void) {
    uint8_t character_one[0x100];
    uint8_t character_two[0x100];
    uint8_t skill_one[0x80];
    uint8_t targeter[0x90];
    void *target_node = (void *)(uintptr_t)0x12345678u;
    const char *reason = NULL;
    SudekiMpPlayerCombatSnapshot snapshot;

    ZeroMemory(character_one, sizeof(character_one));
    ZeroMemory(character_two, sizeof(character_two));
    ZeroMemory(skill_one, sizeof(skill_one));
    ZeroMemory(targeter, sizeof(targeter));
    *(void **)(character_one + 0xacu) = targeter;
    skill_one[0x6cu] = 1u;

    SudekiMpCombatContextsReset();
    SudekiMpCombatContextSetCharacter(0u, character_one);
    SudekiMpCombatContextSetCharacter(1u, character_two);
    SudekiMpCombatContextSetInputSource(
        0u,
        SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER,
        (void *)(uintptr_t)0x3000u
    );
    SudekiMpCombatContextSetView(
        0u,
        (void *)(uintptr_t)0x1000u,
        (void *)(uintptr_t)0x2000u
    );
    SudekiMpCombatContextsPollState(FALSE, NULL, 0, 0, FALSE);
    check(
        SudekiMpCombatContextCanStartSkill(0u, &reason),
        "assigned player can start in shared real time"
    );

    SudekiMpCombatContextSkillStarted(character_one, skill_one);
    check(
        !SudekiMpCombatContextCanStartSkill(0u, &reason) &&
            strcmp(reason, "player_skill_already_active") == 0,
        "same player cannot re-enter an active skill"
    );
    check(
        SudekiMpCombatContextCanStartSkill(1u, &reason),
        "other player may overlap an executing skill"
    );

    SudekiMpCombatContextsPollState(TRUE, target_node, 0, 0, FALSE);
    check(SudekiMpCombatContextGlobalTargetingActive(),
        "native global targeting state is observed");
    check(
        !SudekiMpCombatContextCanStartSkill(1u, &reason) &&
            strcmp(reason, "native_global_targeting_busy") == 0,
        "other activation is rejected during global target selection"
    );
    check(
        SudekiMpCombatContextGetSnapshot(0u, &snapshot) &&
            snapshot.phase == SUDEKIMP_COMBAT_PHASE_TARGETING &&
            snapshot.target_node == target_node &&
            snapshot.viewport_camera == (void *)(uintptr_t)0x1000u &&
            snapshot.input_source_kind ==
                SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER &&
            snapshot.input_source == (void *)(uintptr_t)0x3000u,
        "targeting owner retains its target and viewport context"
    );

    SudekiMpCombatContextsPollState(FALSE, NULL, 0, 0, FALSE);
    check(
        SudekiMpCombatContextGetSnapshot(0u, &snapshot) &&
            snapshot.phase == SUDEKIMP_COMBAT_PHASE_EXECUTING,
        "target confirmation returns the caster to executing"
    );
    check(
        SudekiMpCombatContextCanStartSkill(1u, &reason),
        "other player is accepted after target selection ends"
    );

    skill_one[0x6cu] = 0u;
    SudekiMpCombatContextsPollState(FALSE, NULL, 0, 0, FALSE);
    check(
        SudekiMpCombatContextGetSnapshot(0u, &snapshot) &&
            snapshot.phase == SUDEKIMP_COMBAT_PHASE_IDLE &&
            snapshot.active_skill == NULL,
        "native active-byte cleanup clears the combat context"
    );

    SudekiMpCombatContextsPollState(FALSE, NULL, 1, 0, FALSE);
    check(
        !SudekiMpCombatContextRealtimeInvariantHolds() &&
        !SudekiMpCombatContextCanStartSkill(1u, &reason) &&
            strcmp(reason, "shared_world_not_realtime") == 0,
        "mod skill input is rejected if world time leaves 1.0x mode"
    );

    if (failures != 0) {
        fprintf(stderr, "%d player combat context test(s) failed\n", failures);
        return 1;
    }
    puts("player combat context tests passed");
    return 0;
}
