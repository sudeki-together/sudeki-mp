#include "hooks/accelerator_cache.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

static SudekiMpPointerHook load_accelerators_hook;
static SudekiMpAcceleratorCacheState accelerator_cache_state;
static SudekiMpLoadAcceleratorsAFunction original_load_accelerators;
static volatile LONG first_load_logged;
static volatile LONG first_reuse_logged;

static BOOL is_sudeki_accelerator_request(
    const SudekiMpAcceleratorCacheState *state,
    HINSTANCE instance,
    LPCSTR table_name
) {
    return state != NULL && state->game_module != NULL &&
        instance == state->game_module &&
        (uintptr_t)table_name == SUDEKIMP_ACCELERATOR_RESOURCE_ID;
}

HACCEL SudekiMpAcceleratorCacheLoad(
    SudekiMpAcceleratorCacheState *state,
    SudekiMpLoadAcceleratorsAFunction original,
    HINSTANCE instance,
    LPCSTR table_name
) {
    void *cached;
    HACCEL loaded;
    void *winner;

    if (state == NULL || original == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (!is_sudeki_accelerator_request(state, instance, table_name)) {
        return original(instance, table_name);
    }

    cached = InterlockedCompareExchangePointer(
        &state->cached_handle, NULL, NULL);
    if (cached != NULL) {
        return (HACCEL)cached;
    }

    loaded = original(instance, table_name);
    if (loaded == NULL) {
        return NULL;
    }
    winner = InterlockedCompareExchangePointer(
        &state->cached_handle, (void *)loaded, NULL);
    return winner == NULL ? loaded : (HACCEL)winner;
}

static HACCEL WINAPI cached_load_accelerators(
    HINSTANCE instance,
    LPCSTR table_name
) {
    BOOL matching;
    BOOL already_cached;
    HACCEL result;

    matching = is_sudeki_accelerator_request(
        &accelerator_cache_state, instance, table_name);
    already_cached = matching && InterlockedCompareExchangePointer(
        &accelerator_cache_state.cached_handle, NULL, NULL) != NULL;
    result = SudekiMpAcceleratorCacheLoad(
        &accelerator_cache_state,
        original_load_accelerators,
        instance,
        table_name
    );
    if (matching && result != NULL && !already_cached &&
        InterlockedCompareExchange(&first_load_logged, 1, 0) == 0) {
        SudekiMpLogFormat(
            "accelerator_cache event=resource_loaded resource_id=%lu "
            "handle=0x%08lx policy=process_lifetime_cache\r\n",
            (unsigned long)SUDEKIMP_ACCELERATOR_RESOURCE_ID,
            (unsigned long)(uintptr_t)result
        );
    } else if (matching && result != NULL && already_cached &&
        InterlockedCompareExchange(&first_reuse_logged, 1, 0) == 0) {
        SudekiMpLogFormat(
            "accelerator_cache event=resource_reused resource_id=%lu "
            "handle=0x%08lx policy=no_new_user_handle\r\n",
            (unsigned long)SUDEKIMP_ACCELERATOR_RESOURCE_ID,
            (unsigned long)(uintptr_t)result
        );
    }
    return result;
}

BOOL SudekiMpInstallAcceleratorCache(HMODULE game_module) {
    static const uint8_t message_pump_opcodes[] = {
        0x6a, 0x65,
        0x6a, 0x00,
        0xff, 0x15
    };
    uint8_t *base;
    uint8_t *callsite;
    uint32_t get_module_handle_slot;
    uint32_t load_accelerators_slot;
    HMODULE user32;
    FARPROC expected;

    if (game_module == NULL || load_accelerators_hook.installed ||
        original_load_accelerators != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    callsite = base + SUDEKIMP_MESSAGE_PUMP_ACCELERATOR_CALL_RVA;
    memcpy(&get_module_handle_slot, callsite + 6,
        sizeof(get_module_handle_slot));
    memcpy(&load_accelerators_slot, callsite + 13,
        sizeof(load_accelerators_slot));
    if (memcmp(callsite, message_pump_opcodes,
            sizeof(message_pump_opcodes)) != 0 ||
        callsite[10] != 0x50 || callsite[11] != 0xff ||
        callsite[12] != 0x15 ||
        get_module_handle_slot !=
            (uint32_t)(uintptr_t)(base + 0x0029a0ecu) ||
        load_accelerators_slot !=
            (uint32_t)(uintptr_t)(base + SUDEKIMP_LOAD_ACCELERATORS_IAT_RVA)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    user32 = GetModuleHandleW(L"user32.dll");
    expected = user32 == NULL ? NULL :
        GetProcAddress(user32, "LoadAcceleratorsA");
    if (expected == NULL) {
        return FALSE;
    }

    ZeroMemory(&accelerator_cache_state, sizeof(accelerator_cache_state));
    accelerator_cache_state.game_module = game_module;
    original_load_accelerators =
        (SudekiMpLoadAcceleratorsAFunction)(uintptr_t)expected;
    InterlockedExchange(&first_load_logged, 0);
    InterlockedExchange(&first_reuse_logged, 0);
    if (!SudekiMpInstallPointerHook(
            &load_accelerators_hook,
            (void **)(base + SUDEKIMP_LOAD_ACCELERATORS_IAT_RVA),
            (const void *)(uintptr_t)expected,
            (const void *)(uintptr_t)cached_load_accelerators)) {
        original_load_accelerators = NULL;
        ZeroMemory(&accelerator_cache_state, sizeof(accelerator_cache_state));
        return FALSE;
    }
    SudekiMpLogFormat(
        "accelerator_cache event=install status=success iat_rva=0x%08lx "
        "callsite_rva=0x%08lx resource_id=%lu scope=exact_supported_build\r\n",
        (unsigned long)SUDEKIMP_LOAD_ACCELERATORS_IAT_RVA,
        (unsigned long)SUDEKIMP_MESSAGE_PUMP_ACCELERATOR_CALL_RVA,
        (unsigned long)SUDEKIMP_ACCELERATOR_RESOURCE_ID
    );
    return TRUE;
}

BOOL SudekiMpUninstallAcceleratorCache(void) {
    if (!SudekiMpRestorePointerHook(&load_accelerators_hook)) {
        return FALSE;
    }
    original_load_accelerators = NULL;
    ZeroMemory(&accelerator_cache_state, sizeof(accelerator_cache_state));
    InterlockedExchange(&first_load_logged, 0);
    InterlockedExchange(&first_reuse_logged, 0);
    return TRUE;
}
