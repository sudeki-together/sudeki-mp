#include <windows.h>
#include <xinput.h>

#include <stdio.h>

typedef DWORD(WINAPI *xinput_get_state_fn)(DWORD, XINPUT_STATE *);
typedef DWORD(WINAPI *xinput_get_capabilities_fn)(DWORD, DWORD,
                                                   XINPUT_CAPABILITIES *);

int main(void) {
    union {
        FARPROC generic;
        xinput_get_state_fn typed;
    } state_resolver;
    union {
        FARPROC generic;
        xinput_get_capabilities_fn typed;
    } capabilities_resolver;
    HMODULE module = LoadLibraryW(L"xinput1_2.dll");
    xinput_get_state_fn get_state;
    xinput_get_capabilities_fn get_capabilities;
    DWORD index;
    unsigned int connected = 0;

    if (module == NULL) {
        printf("xinput_probe: xinput1_2.dll unavailable error=%lu\n",
               (unsigned long)GetLastError());
        return 2;
    }

    state_resolver.generic = GetProcAddress(module, "XInputGetState");
    capabilities_resolver.generic =
        GetProcAddress(module, "XInputGetCapabilities");
    get_state = state_resolver.typed;
    get_capabilities = capabilities_resolver.typed;
    if (get_state == NULL || get_capabilities == NULL) {
        printf("xinput_probe: required exports unavailable\n");
        FreeLibrary(module);
        return 3;
    }

    for (index = 0; index < XUSER_MAX_COUNT; ++index) {
        XINPUT_STATE state;
        XINPUT_CAPABILITIES capabilities;
        DWORD state_result;
        DWORD capabilities_result;

        ZeroMemory(&state, sizeof(state));
        ZeroMemory(&capabilities, sizeof(capabilities));
        state_result = get_state(index, &state);
        capabilities_result =
            get_capabilities(index, XINPUT_FLAG_GAMEPAD, &capabilities);
        if (state_result != ERROR_SUCCESS) {
            printf("slot=%lu connected=false state_result=%lu capabilities_result=%lu\n",
                   (unsigned long)index, (unsigned long)state_result,
                   (unsigned long)capabilities_result);
            continue;
        }

        ++connected;
        printf("slot=%lu connected=true packet=%lu type=%u subtype=%u flags=0x%04x "
               "buttons=0x%04x left_trigger=%u right_trigger=%u "
               "left_stick=%d,%d right_stick=%d,%d\n",
               (unsigned long)index, (unsigned long)state.dwPacketNumber,
               (unsigned int)capabilities.Type,
               (unsigned int)capabilities.SubType,
               (unsigned int)capabilities.Flags,
               (unsigned int)state.Gamepad.wButtons,
               (unsigned int)state.Gamepad.bLeftTrigger,
               (unsigned int)state.Gamepad.bRightTrigger,
               (int)state.Gamepad.sThumbLX, (int)state.Gamepad.sThumbLY,
               (int)state.Gamepad.sThumbRX, (int)state.Gamepad.sThumbRY);
    }

    FreeLibrary(module);
    printf("xinput_probe: connected=%u\n", connected);
    return connected != 0 ? 0 : 1;
}
