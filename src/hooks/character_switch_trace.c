#include "hooks/character_switch_trace.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#else
#error "Character-switch tracing requires 32-bit GCC thiscall support"
#endif

typedef void (SUDEKIMP_THISCALL *CharacterInputHandler)(
    void *listener,
    void *event
);
enum {
    RVA_CHARACTER_INPUT_HANDLER = 0x000277b0u,
    RVA_CHARACTER_INPUT_VTABLE_SLOT = 0x002c9f84u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    ACTION_PREVIOUS_CHARACTER = 0x32u,
    ACTION_NEXT_CHARACTER = 0x33u,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_FIRST_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu
};

typedef struct CharacterSnapshot {
    void *character;
    uint8_t control_2a;
    void *actor_state;
    void *ai_component;
    void *ai_owner;
    void *ai_mode_state;
    uint8_t ai_enabled_3c_0b;
    int16_t ai_control_ref_16a;
    uint32_t ai_flags_44;
    uint32_t actor_flags_50;
    uint32_t actor_state_58;
    uint32_t actor_flags_60;
} CharacterSnapshot;

typedef struct SwitchSnapshot {
    void *group;
    void *controller;
    void *controller_target;
    uint32_t controller_mode_80;
    uint32_t controller_mode_84;
    uint8_t group_switching_d6;
    uint8_t group_state_d7;
    CharacterSnapshot party[PARTY_SLOT_COUNT];
} SwitchSnapshot;

static SudekiMpPointerHook character_input_vtable_hook;
static CharacterInputHandler original_character_input_handler;
static uint8_t *game_base;
static UINT_PTR snapshot_timer;
static unsigned int snapshot_timer_stage;
static DWORD snapshot_timer_started;
static uint32_t snapshot_action;

static void capture_character(
    void *character_pointer,
    CharacterSnapshot *snapshot
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *actor_state;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->character = character;
    if (character == NULL) {
        return;
    }
    snapshot->control_2a = *(uint8_t *)(character + 0x2a);
    snapshot->actor_state = *(void **)(character + 0x90);
    snapshot->ai_component = *(void **)(character + 0x94);
    if (snapshot->ai_component != NULL) {
        uint8_t *ai_component = (uint8_t *)snapshot->ai_component;
        snapshot->ai_owner = *(void **)(ai_component + 0x10);
        snapshot->ai_mode_state = *(void **)(ai_component + 0x3c);
        snapshot->ai_control_ref_16a = *(int16_t *)(ai_component + 0x16a);
        snapshot->ai_flags_44 = *(uint32_t *)(ai_component + 0x44);
        if (snapshot->ai_mode_state != NULL) {
            snapshot->ai_enabled_3c_0b = *(
                (uint8_t *)snapshot->ai_mode_state + 0x0b
            );
        }
    }
    actor_state = (uint8_t *)snapshot->actor_state;
    if (actor_state == NULL) {
        return;
    }
    snapshot->actor_flags_50 = *(uint32_t *)(actor_state + 0x50);
    snapshot->actor_state_58 = *(uint32_t *)(actor_state + 0x58);
    snapshot->actor_flags_60 = *(uint32_t *)(actor_state + 0x60);
}

static void capture_switch_snapshot(SwitchSnapshot *snapshot) {
    uint8_t *group;
    uint8_t *controller;
    unsigned int index;

    memset(snapshot, 0, sizeof(*snapshot));
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    snapshot->group = group;
    snapshot->controller = controller;
    if (controller != NULL) {
        snapshot->controller_target = *(void **)(controller + 0x248);
        snapshot->controller_mode_80 = *(uint32_t *)(controller + 0x80);
        snapshot->controller_mode_84 = *(uint32_t *)(controller + 0x84);
    }
    if (group == NULL) {
        return;
    }
    snapshot->group_switching_d6 = *(uint8_t *)(group + 0xd6);
    snapshot->group_state_d7 = *(uint8_t *)(group + 0xd7);
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        void *character = *(void **)(
            group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE
        );
        capture_character(character, &snapshot->party[index]);
    }
}

static void log_switch_snapshot(
    const char *phase,
    uint32_t action,
    DWORD elapsed_ms
) {
    SwitchSnapshot snapshot;
    unsigned int index;

    capture_switch_snapshot(&snapshot);
    SudekiMpLogFormat(
        "character_switch event=snapshot phase=%s action=0x%02lx elapsed_ms=%lu group=0x%08lx switching_d6=%u state_d7=%u controller=0x%08lx target=0x%08lx mode_80=%lu mode_84=%lu\r\n",
        phase,
        (unsigned long)action,
        (unsigned long)elapsed_ms,
        (unsigned long)(uintptr_t)snapshot.group,
        (unsigned int)snapshot.group_switching_d6,
        (unsigned int)snapshot.group_state_d7,
        (unsigned long)(uintptr_t)snapshot.controller,
        (unsigned long)(uintptr_t)snapshot.controller_target,
        (unsigned long)snapshot.controller_mode_80,
        (unsigned long)snapshot.controller_mode_84
    );
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        const CharacterSnapshot *character = &snapshot.party[index];
        SudekiMpLogFormat(
            "character_switch event=party phase=%s action=0x%02lx elapsed_ms=%lu slot=%u character=0x%08lx control_2a=%u actor_state=0x%08lx ai=0x%08lx ai_owner=0x%08lx ai_mode_state=0x%08lx ai_enabled_3c_0b=%u ai_control_ref_16a=%d ai_flags_44=0x%08lx flags_50=0x%08lx state_58=0x%08lx flags_60=0x%08lx\r\n",
            phase,
            (unsigned long)action,
            (unsigned long)elapsed_ms,
            index,
            (unsigned long)(uintptr_t)character->character,
            (unsigned int)character->control_2a,
            (unsigned long)(uintptr_t)character->actor_state,
            (unsigned long)(uintptr_t)character->ai_component,
            (unsigned long)(uintptr_t)character->ai_owner,
            (unsigned long)(uintptr_t)character->ai_mode_state,
            (unsigned int)character->ai_enabled_3c_0b,
            (int)character->ai_control_ref_16a,
            (unsigned long)character->ai_flags_44,
            (unsigned long)character->actor_flags_50,
            (unsigned long)character->actor_state_58,
            (unsigned long)character->actor_flags_60
        );
    }
}

static void CALLBACK snapshot_timer_callback(
    HWND window,
    UINT message,
    UINT_PTR timer_id,
    DWORD time
) {
    static const unsigned int next_delays[] = {200u, 750u};
    static const char *phases[] = {
        "after_50ms",
        "after_250ms",
        "after_1000ms"
    };
    DWORD elapsed;

    (void)window;
    (void)message;
    (void)time;
    KillTimer(NULL, timer_id);
    snapshot_timer = 0;
    elapsed = GetTickCount() - snapshot_timer_started;
    log_switch_snapshot(
        phases[snapshot_timer_stage],
        snapshot_action,
        elapsed
    );
    if (snapshot_timer_stage < 2u) {
        unsigned int delay = next_delays[snapshot_timer_stage];
        ++snapshot_timer_stage;
        snapshot_timer = SetTimer(NULL, 0, delay, snapshot_timer_callback);
        if (snapshot_timer == 0) {
            SudekiMpLogWrite(
                "character_switch event=timer_error phase=reschedule\r\n"
            );
        }
    }
}

static void begin_delayed_snapshots(uint32_t action) {
    if (snapshot_timer != 0) {
        KillTimer(NULL, snapshot_timer);
        snapshot_timer = 0;
    }
    snapshot_action = action;
    snapshot_timer_stage = 0;
    snapshot_timer_started = GetTickCount();
    snapshot_timer = SetTimer(NULL, 0, 50, snapshot_timer_callback);
    if (snapshot_timer == 0) {
        SudekiMpLogWrite("character_switch event=timer_error phase=initial\r\n");
    }
}

static void SUDEKIMP_THISCALL trace_character_input(
    void *listener,
    void *event_pointer
) {
    uint32_t *event = (uint32_t *)event_pointer;
    uint32_t action;
    uint32_t pressed;
    uint32_t analog_bits;

    if (event == NULL) {
        original_character_input_handler(listener, event_pointer);
        return;
    }
    action = event[0];
    if (action != ACTION_PREVIOUS_CHARACTER &&
        action != ACTION_NEXT_CHARACTER) {
        original_character_input_handler(listener, event_pointer);
        return;
    }
    pressed = event[2];
    analog_bits = event[3];
    SudekiMpLogFormat(
        "character_switch event=input phase=before action=0x%02lx pressed=%lu analog_bits=0x%08lx listener=0x%08lx\r\n",
        (unsigned long)action,
        (unsigned long)pressed,
        (unsigned long)analog_bits,
        (unsigned long)(uintptr_t)listener
    );
    log_switch_snapshot("before_handler", action, 0);
    original_character_input_handler(listener, event_pointer);
    log_switch_snapshot("after_handler", action, 0);
    if (pressed != 0) {
        begin_delayed_snapshots(action);
    }
}

BOOL SudekiMpInstallCharacterSwitchTrace(HMODULE game_module) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    slot = (void **)(base + RVA_CHARACTER_INPUT_VTABLE_SLOT);
    game_base = base;
    original_character_input_handler = (CharacterInputHandler)(
        base + RVA_CHARACTER_INPUT_HANDLER
    );
    if (!SudekiMpInstallPointerHook(
            &character_input_vtable_hook,
            slot,
            original_character_input_handler,
            trace_character_input)) {
        game_base = NULL;
        original_character_input_handler = NULL;
        return FALSE;
    }
    SudekiMpLogWrite("character_switch_trace_install=success\r\n");
    return TRUE;
}

void SudekiMpUninstallCharacterSwitchTrace(void) {
    if (snapshot_timer != 0) {
        KillTimer(NULL, snapshot_timer);
    }
    SudekiMpRestorePointerHook(&character_input_vtable_hook);
    original_character_input_handler = NULL;
    game_base = NULL;
    snapshot_timer = 0;
    snapshot_timer_stage = 0;
    snapshot_timer_started = 0;
    snapshot_action = 0;
}
