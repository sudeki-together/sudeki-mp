#include "hooks/freeroam_camera_input.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#else
#error "Free-roam camera input requires 32-bit GCC thiscall support"
#endif

typedef void (SUDEKIMP_THISCALL *CharacterInputHandler)(
    void *listener,
    void *event
);
typedef uint8_t (SUDEKIMP_THISCALL *GroupInCombatFunction)(void *group);

enum {
    RVA_CHARACTER_INPUT_HANDLER = 0x000277b0u,
    RVA_CHARACTER_INPUT_VTABLE_SLOT = 0x002c9f84u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_GROUP_IN_COMBAT = 0x00004fa0u,
    ACTION_CAMERA_UP = 0x69u,
    ACTION_CAMERA_DOWN = 0x6au
};

static SudekiMpPointerHook character_input_vtable_hook;
static CharacterInputHandler original_character_input_handler;
static GroupInCombatFunction group_in_combat;
static uint8_t *game_base;
static UINT camera_modifier_key;
static BOOL modifier_was_down;

static void SUDEKIMP_THISCALL gate_freeroam_camera_input(
    void *listener,
    void *event_pointer
) {
    const uint32_t *event = (const uint32_t *)event_pointer;
    uint32_t action;
    void *group;
    BOOL modifier_down;

    if (event == NULL) {
        original_character_input_handler(listener, event_pointer);
        return;
    }
    action = event[0];
    if (action != ACTION_CAMERA_UP && action != ACTION_CAMERA_DOWN) {
        original_character_input_handler(listener, event_pointer);
        return;
    }

    group = *(void **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group == NULL || group_in_combat(group) != 0) {
        original_character_input_handler(listener, event_pointer);
        return;
    }

    modifier_down = (GetAsyncKeyState((int)camera_modifier_key) & 0x8000) != 0;
    if (modifier_down != modifier_was_down) {
        SudekiMpLogFormat(
            "freeroam_camera event=modifier state=%s virtual_key=0x%02lx\r\n",
            modifier_down ? "down" : "up",
            (unsigned long)camera_modifier_key
        );
        modifier_was_down = modifier_down;
    }
    if (modifier_down) {
        original_character_input_handler(listener, event_pointer);
    }
}

BOOL SudekiMpInstallFreeRoamCameraInput(
    HMODULE game_module,
    UINT modifier_key
) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL || modifier_key == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    slot = (void **)(base + RVA_CHARACTER_INPUT_VTABLE_SLOT);
    game_base = base;
    camera_modifier_key = modifier_key;
    group_in_combat = (GroupInCombatFunction)(base + RVA_GROUP_IN_COMBAT);
    original_character_input_handler = (CharacterInputHandler)(
        base + RVA_CHARACTER_INPUT_HANDLER
    );
    if (!SudekiMpInstallPointerHook(
            &character_input_vtable_hook,
            slot,
            original_character_input_handler,
            gate_freeroam_camera_input)) {
        game_base = NULL;
        camera_modifier_key = 0;
        group_in_combat = NULL;
        original_character_input_handler = NULL;
        return FALSE;
    }
    SudekiMpLogFormat(
        "freeroam_camera_install=success route=modifier_gate camera_up=0x69 camera_down=0x6a modifier_virtual_key=0x%02lx\r\n",
        (unsigned long)camera_modifier_key
    );
    return TRUE;
}

void SudekiMpUninstallFreeRoamCameraInput(void) {
    SudekiMpRestorePointerHook(&character_input_vtable_hook);
    original_character_input_handler = NULL;
    group_in_combat = NULL;
    game_base = NULL;
    camera_modifier_key = 0;
    modifier_was_down = FALSE;
}
