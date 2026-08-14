#include "hooks/player_input_trace.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
);

enum {
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    LOG_INTERVAL_MS = 100u
};

static SudekiMpRelativeCallHook alternate_movement_call_hook;
static SudekiMpRelativeCallHook normal_movement_call_hook;
static ArbiterMovementFunction original_arbiter_movement;
static DWORD last_log_tick;

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void __stdcall trace_arbiter_movement(
    void *arbiter_pointer,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
) {
    uint8_t *arbiter = (uint8_t *)arbiter_pointer;
    void *character = arbiter == NULL ? NULL : *(void **)(arbiter + 0x10);
    DWORD now = GetTickCount();

    if (direction != NULL &&
        (last_log_tick == 0 || now - last_log_tick >= LOG_INTERVAL_MS)) {
        SudekiMpLogFormat(
            "player_input event=movement character=0x%08lx arbiter=0x%08lx direction_bits=%08lx,%08lx,%08lx speed_bits=0x%08lx turn_rate_bits=0x%08lx movement_mode=%lu\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            (unsigned long)float_bits(direction[0]),
            (unsigned long)float_bits(direction[1]),
            (unsigned long)float_bits(direction[2]),
            (unsigned long)float_bits(speed),
            (unsigned long)float_bits(turn_rate),
            (unsigned long)movement_mode
        );
        last_log_tick = now;
    }
    original_arbiter_movement(
        arbiter_pointer,
        direction,
        speed,
        turn_rate,
        movement_mode
    );
}

BOOL SudekiMpInstallPlayerInputTrace(HMODULE game_module) {
    uint8_t *base;

    if (game_module == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    original_arbiter_movement = (ArbiterMovementFunction)(
        base + RVA_ARBITER_MOVEMENT
    );
    last_log_tick = 0;
    if (!SudekiMpInstallRelativeCallHook(
            &alternate_movement_call_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE,
            original_arbiter_movement,
            trace_arbiter_movement)) {
        SudekiMpUninstallPlayerInputTrace();
        return FALSE;
    }
    if (!SudekiMpInstallRelativeCallHook(
            &normal_movement_call_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL,
            original_arbiter_movement,
            trace_arbiter_movement)) {
        SudekiMpUninstallPlayerInputTrace();
        return FALSE;
    }
    SudekiMpLogWrite(
        "player_input_trace_install=success call_rvas=0x00028e3f,0x00028e5e target_rva=0x000dae80\r\n"
    );
    return TRUE;
}

void SudekiMpUninstallPlayerInputTrace(void) {
    SudekiMpRestoreRelativeCallHook(&normal_movement_call_hook);
    SudekiMpRestoreRelativeCallHook(&alternate_movement_call_hook);
    original_arbiter_movement = NULL;
    last_log_tick = 0;
}
