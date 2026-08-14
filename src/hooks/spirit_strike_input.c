#include "hooks/spirit_strike_input.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>

typedef void (__stdcall *MainFrameUpdateFunction)(
    void *game,
    float simulation_delta,
    float real_delta
);
typedef int (__stdcall *SpiritStrikeFunction)(void *manager, int strike_id);
typedef int (__attribute__((thiscall)) *CharacterResourceTypeFunction)(
    void *component
);

enum {
    RVA_MAIN_FRAME_UPDATE_CALL = 0x0028ddbau,
    RVA_MAIN_FRAME_UPDATE = 0x0028d3f0u,
    RVA_SPIRIT_STRIKE_VALIDATE = 0x00010940u,
    RVA_SPIRIT_STRIKE_ACTIVATE = 0x0000fba0u,
    RVA_SPIRIT_STRIKE_MANAGER_GLOBAL = 0x00408d30u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_UI_MANAGER_GLOBAL = 0x00408d1cu,
    RVA_SET_UI_ACTIVE = 0x0000afd0u,
    SUPPORTED_IMAGE_SIZE = 0x0045f000u
};

static SudekiMpRelativeCallHook main_frame_update_call_hook;
static MainFrameUpdateFunction original_main_frame_update;
static SpiritStrikeFunction spirit_strike_validate;
static SpiritStrikeFunction spirit_strike_activate;
static uint8_t *game_base;
static int configured_strike_id;
static unsigned int configured_variant;
static int pending_strike_id;
static UINT selected_virtual_key;
static BOOL hotkey_was_down;
static BOOL transition_pending;
static UINT_PTR transition_timer;

static int pair_start_for_resource_type(int resource_type) {
    switch (resource_type) {
    case 0x23:
        return 0;
    case 0x01:
        return 2;
    case 0x05:
        return 4;
    case 0x0e:
        return 6;
    default:
        return -1;
    }
}

static int resolve_strike_id(void) {
    uint8_t *group;
    uint8_t *character;
    void *component;
    void **vtable;
    CharacterResourceTypeFunction get_resource_type;
    int resource_type;
    int pair_start;
    int strike_id;

    if (configured_strike_id >= 0) {
        SudekiMpLogFormat(
            "spirit_strike_input event=resolve mode=fixed strike_id=%d\r\n",
            configured_strike_id
        );
        return configured_strike_id;
    }

    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group == NULL) {
        SudekiMpLogWrite(
            "spirit_strike_input event=resolve_abort reason=no_active_group\r\n"
        );
        return -1;
    }
    character = *(uint8_t **)(group + 0x90);
    if (character == NULL) {
        SudekiMpLogWrite(
            "spirit_strike_input event=resolve_abort reason=no_front_character\r\n"
        );
        return -1;
    }
    component = character + 0x2c;
    vtable = *(void ***)component;
    if (vtable == NULL) {
        SudekiMpLogWrite(
            "spirit_strike_input event=resolve_abort reason=no_type_vtable\r\n"
        );
        return -1;
    }
    get_resource_type = (CharacterResourceTypeFunction)vtable[4];
    if ((uint8_t *)get_resource_type < game_base ||
        (uint8_t *)get_resource_type >= game_base + SUPPORTED_IMAGE_SIZE) {
        SudekiMpLogWrite(
            "spirit_strike_input event=resolve_abort reason=unexpected_type_function\r\n"
        );
        return -1;
    }

    resource_type = get_resource_type(component);
    pair_start = pair_start_for_resource_type(resource_type);
    if (pair_start < 0) {
        SudekiMpLogFormat(
            "spirit_strike_input event=resolve_abort reason=unsupported_resource_type resource_type=0x%02x\r\n",
            resource_type
        );
        return -1;
    }
    strike_id = pair_start + (int)configured_variant - 1;
    SudekiMpLogFormat(
        "spirit_strike_input event=resolve mode=front_character group=0x%08lx character=0x%08lx resource_type=0x%02x variant=%u strike_id=%d\r\n",
        (unsigned long)(uintptr_t)group,
        (unsigned long)(uintptr_t)character,
        resource_type,
        configured_variant,
        strike_id
    );
    return strike_id;
}

static void set_native_ui_active(BOOL active) {
    void *ui_manager = *(void **)(game_base + RVA_UI_MANAGER_GLOBAL);
    void *function = game_base + RVA_SET_UI_ACTIVE;

    __asm__ volatile(
        "movl %0, %%esi\n\t"
        "pushl %1\n\t"
        "call *%2"
        :
        : "r"(ui_manager), "r"(active), "r"(function)
        : "eax", "ecx", "edx", "esi", "memory", "cc"
    );
}

static void CALLBACK complete_spirit_transition(
    HWND window,
    UINT message,
    UINT_PTR timer_id,
    DWORD time
) {
    void *manager;
    int strike_id;
    int validation;
    int activation;

    (void)window;
    (void)message;
    (void)time;
    KillTimer(NULL, timer_id);
    transition_timer = 0;

    SudekiMpLogWrite("spirit_strike_input event=transition_wait_complete\r\n");
    set_native_ui_active(FALSE);
    SudekiMpLogWrite("spirit_strike_input event=transition_exit\r\n");
    transition_pending = FALSE;

    manager = *(void **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
    strike_id = pending_strike_id;
    pending_strike_id = -1;
    validation = manager == NULL ? -1 :
        spirit_strike_validate(manager, strike_id);
    SudekiMpLogFormat(
        "spirit_strike_input event=post_transition_validate strike_id=%d result=%d\r\n",
        strike_id,
        validation
    );
    if (validation != 0) {
        return;
    }

    activation = spirit_strike_activate(manager, strike_id);
    SudekiMpLogFormat(
        "spirit_strike_input event=direct_activate strike_id=%d result=%d\r\n",
        strike_id,
        activation
    );
}

static void try_direct_spirit_strike(void) {
    void *manager;
    int strike_id;
    int validation;

    manager = *(void **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
    strike_id = resolve_strike_id();
    SudekiMpLogFormat(
        "spirit_strike_input event=direct_pressed virtual_key=0x%02lx manager=0x%08lx strike_id=%d\r\n",
        (unsigned long)selected_virtual_key,
        (unsigned long)(uintptr_t)manager,
        strike_id
    );
    if (manager == NULL) {
        SudekiMpLogFormat(
            "spirit_strike_input event=direct_abort reason=no_manager\r\n"
        );
        return;
    }
    if (strike_id < 0) {
        return;
    }

    validation = spirit_strike_validate(manager, strike_id);
    SudekiMpLogFormat(
        "spirit_strike_input event=direct_validate virtual_key=0x%02lx strike_id=%d result=%d\r\n",
        (unsigned long)selected_virtual_key,
        strike_id,
        validation
    );
    if (validation != 0) {
        return;
    }
    if (transition_pending) {
        SudekiMpLogWrite(
            "spirit_strike_input event=direct_abort reason=transition_pending\r\n"
        );
        return;
    }

    SudekiMpLogWrite(
        "spirit_strike_input event=transition_begin method=native_ui_cycle\r\n"
    );
    set_native_ui_active(TRUE);
    SudekiMpLogWrite("spirit_strike_input event=transition_enter\r\n");
    pending_strike_id = strike_id;
    transition_pending = TRUE;
    transition_timer = SetTimer(NULL, 0, 75, complete_spirit_transition);
    if (transition_timer == 0) {
        transition_pending = FALSE;
        pending_strike_id = -1;
        set_native_ui_active(FALSE);
        SudekiMpLogWrite(
            "spirit_strike_input event=direct_abort reason=timer_error\r\n"
        );
    } else {
        SudekiMpLogFormat(
            "spirit_strike_input event=transition_scheduled timer=0x%08lx delay_ms=75\r\n",
            (unsigned long)transition_timer
        );
    }
}

static void __stdcall poll_spirit_strike_hotkey(
    void *game,
    float simulation_delta,
    float real_delta
) {
    HWND foreground = GetForegroundWindow();
    DWORD foreground_process_id = 0;
    BOOL hotkey_is_down =
        (GetAsyncKeyState((int)selected_virtual_key) & 0x8000) != 0;

    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &foreground_process_id);
    }
    if (foreground_process_id != GetCurrentProcessId()) {
        hotkey_was_down = hotkey_is_down;
        original_main_frame_update(game, simulation_delta, real_delta);
        return;
    }

    if (hotkey_is_down && !hotkey_was_down) {
        try_direct_spirit_strike();
    }
    hotkey_was_down = hotkey_is_down;
    original_main_frame_update(game, simulation_delta, real_delta);
}

BOOL SudekiMpInstallSpiritStrikeInput(
    HMODULE game_module,
    int strike_id,
    unsigned int variant,
    UINT virtual_key
) {
    uint8_t *base;

    if (game_module == NULL || strike_id < -1 || strike_id > 15 ||
        variant < 1u || variant > 2u ||
        virtual_key == 0u || virtual_key > 0xffu) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    base = (uint8_t *)game_module;
    game_base = base;
    configured_strike_id = strike_id;
    configured_variant = variant;
    pending_strike_id = -1;
    selected_virtual_key = virtual_key;
    hotkey_was_down = FALSE;
    transition_pending = FALSE;
    transition_timer = 0;
    original_main_frame_update = (MainFrameUpdateFunction)(
        base + RVA_MAIN_FRAME_UPDATE
    );
    spirit_strike_validate = (SpiritStrikeFunction)(
        base + RVA_SPIRIT_STRIKE_VALIDATE
    );
    spirit_strike_activate = (SpiritStrikeFunction)(
        base + RVA_SPIRIT_STRIKE_ACTIVATE
    );

    if (!SudekiMpInstallRelativeCallHook(
            &main_frame_update_call_hook,
            base + RVA_MAIN_FRAME_UPDATE_CALL,
            original_main_frame_update,
            poll_spirit_strike_hotkey)) {
        SudekiMpUninstallSpiritStrikeInput();
        return FALSE;
    }

    SudekiMpLogFormat(
        "spirit_strike_input_install=success virtual_key=0x%02lx strike_id=%d variant=%u mode=%s\r\n",
        (unsigned long)selected_virtual_key,
        configured_strike_id,
        configured_variant,
        configured_strike_id < 0 ? "front_character" : "fixed"
    );
    return TRUE;
}

void SudekiMpUninstallSpiritStrikeInput(void) {
    if (transition_timer != 0) {
        KillTimer(NULL, transition_timer);
    }
    SudekiMpRestoreRelativeCallHook(&main_frame_update_call_hook);
    original_main_frame_update = NULL;
    spirit_strike_validate = NULL;
    spirit_strike_activate = NULL;
    game_base = NULL;
    configured_strike_id = -1;
    configured_variant = 1;
    pending_strike_id = -1;
    selected_virtual_key = 0;
    hotkey_was_down = FALSE;
    transition_pending = FALSE;
    transition_timer = 0;
}
