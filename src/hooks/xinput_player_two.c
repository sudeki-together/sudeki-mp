#include "hooks/xinput_player_two.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>
#include <xinput.h>

typedef DWORD(WINAPI *SudekiMpXInputGetStateFunction)(DWORD, XINPUT_STATE *);

static SudekiMpPointerHook xinput_get_state_hook;
static SudekiMpXInputGetStateFunction original_xinput_get_state;
static unsigned int reserved_slot = XUSER_MAX_COUNT;

static DWORD WINAPI reserved_xinput_get_state(DWORD user_index,
                                              XINPUT_STATE *state) {
    if (user_index == (DWORD)reserved_slot) {
        if (state != NULL) ZeroMemory(state, sizeof(*state));
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return original_xinput_get_state == NULL ? ERROR_DEVICE_NOT_CONNECTED :
        original_xinput_get_state(user_index, state);
}

BOOL SudekiMpInstallXInputPlayerTwoReservation(HMODULE game_module,
                                                unsigned int slot) {
    union {
        FARPROC generic;
        SudekiMpXInputGetStateFunction typed;
    } resolver;
    HMODULE xinput_module;
    void **iat_slot;

    if (game_module == NULL || xinput_get_state_hook.installed ||
        original_xinput_get_state != NULL || slot >= XUSER_MAX_COUNT) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    xinput_module = GetModuleHandleW(L"xinput1_2.dll");
    if (xinput_module == NULL) {
        SetLastError(ERROR_MOD_NOT_FOUND);
        return FALSE;
    }
    resolver.generic = GetProcAddress(xinput_module, "XInputGetState");
    original_xinput_get_state = resolver.typed;
    iat_slot = (void **)((uint8_t *)game_module +
                         SUDEKIMP_XINPUT_GET_STATE_IAT_RVA);
    if (original_xinput_get_state == NULL ||
        *iat_slot != (void *)(uintptr_t)original_xinput_get_state) {
        original_xinput_get_state = NULL;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    reserved_slot = slot;
    if (!SudekiMpInstallPointerHook(
            &xinput_get_state_hook,
            iat_slot,
            (const void *)(uintptr_t)original_xinput_get_state,
            (const void *)(uintptr_t)reserved_xinput_get_state)) {
        reserved_slot = XUSER_MAX_COUNT;
        original_xinput_get_state = NULL;
        return FALSE;
    }
    SudekiMpLogFormat(
        "xinput_player_two event=reservation_install status=success "
        "iat_rva=0x%08lx slot=%u policy=hide_reserved_slot_from_native_game_only\r\n",
        (unsigned long)SUDEKIMP_XINPUT_GET_STATE_IAT_RVA, slot
    );
    return TRUE;
}

void SudekiMpUninstallXInputPlayerTwoReservation(void) {
    (void)SudekiMpRestorePointerHook(&xinput_get_state_hook);
    original_xinput_get_state = NULL;
    reserved_slot = XUSER_MAX_COUNT;
}
