#include "hooks/lan_arena_host_input.h"

#include "cleanroom/engine.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

typedef void (__stdcall *ControllerCombatFunction)(void *controller);

enum {
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    CONTROLLER_WEAK_OFFSET = 0x8cu
};

static const uint8_t expected_controller_combat_entry[] = {
    0x55u, 0x8bu, 0x6cu, 0x24u, 0x08u,
    0x83u, 0xbdu, 0x48u, 0x02u, 0x00u, 0x00u, 0x00u
};

static SudekiMpInlineHook controller_combat_hook;
static ControllerCombatFunction original_controller_combat;
static BOOL tal_weak_was_down;
static BOOL tal_weak_pending;

static void __stdcall observe_host_combat(void *controller) {
    uint8_t *state = (uint8_t *)controller;
    void *tal = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
    BOOL owns_tal = state != NULL && tal != NULL &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == tal;
    BOOL weak_down = owns_tal &&
        *(int *)(state + CONTROLLER_WEAK_OFFSET) == 1;
    if (weak_down && !tal_weak_was_down) tal_weak_pending = TRUE;
    tal_weak_was_down = weak_down;
    original_controller_combat(controller);
}

BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    if (base == NULL || original_controller_combat != NULL ||
        memcmp(base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry)) != 0) {
        SetLastError(base == NULL || original_controller_combat != NULL ?
            ERROR_INVALID_PARAMETER : ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(
            &controller_combat_hook,
            base + RVA_CONTROLLER_COMBAT,
            expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry),
            observe_host_combat)) {
        return FALSE;
    }
    original_controller_combat =
        (ControllerCombatFunction)controller_combat_hook.trampoline;
    tal_weak_was_down = FALSE;
    tal_weak_pending = FALSE;
    return TRUE;
}

void SudekiMpUninstallLanArenaHostInput(void) {
    SudekiMpRestoreInlineHook(&controller_combat_hook);
    original_controller_combat = NULL;
    tal_weak_was_down = FALSE;
    tal_weak_pending = FALSE;
}

BOOL SudekiMpLanArenaHostInputTakeTalWeakAttack(void) {
    BOOL pending = tal_weak_pending;
    tal_weak_pending = FALSE;
    return pending;
}
