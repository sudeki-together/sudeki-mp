#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <stdio.h>

typedef struct probe_context {
    IDirectInput8A *direct_input;
    unsigned int connected;
} probe_context;

static BOOL CALLBACK enumerate_controller(const DIDEVICEINSTANCEA *instance,
                                          void *opaque) {
    probe_context *context = (probe_context *)opaque;
    IDirectInputDevice8A *device = NULL;
    DIJOYSTATE2 state;
    HRESULT result;

    ++context->connected;
    printf("device=%u instance_name=\"%s\" product_name=\"%s\" "
           "dev_type=0x%08lx\n",
           context->connected, instance->tszInstanceName,
           instance->tszProductName, (unsigned long)instance->dwDevType);

    result = IDirectInput8_CreateDevice(context->direct_input,
                                        &instance->guidInstance, &device, NULL);
    if (FAILED(result)) {
        printf("device=%u create_result=0x%08lx\n", context->connected,
               (unsigned long)result);
        return DIENUM_CONTINUE;
    }

    result = IDirectInputDevice8_SetDataFormat(device, &c_dfDIJoystick2);
    if (SUCCEEDED(result)) {
        result = IDirectInputDevice8_SetCooperativeLevel(
            device, GetDesktopWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    }
    if (SUCCEEDED(result)) {
        result = IDirectInputDevice8_Acquire(device);
    }
    if (SUCCEEDED(result) || result == S_FALSE) {
        ZeroMemory(&state, sizeof(state));
        IDirectInputDevice8_Poll(device);
        result = IDirectInputDevice8_GetDeviceState(device, sizeof(state),
                                                     &state);
        if (SUCCEEDED(result)) {
            printf("device=%u state=read x=%ld y=%ld z=%ld rx=%ld ry=%ld rz=%ld "
                   "slider=%ld,%ld pov=%lu,%lu,%lu,%lu "
                   "buttons=%02x,%02x,%02x,%02x\n",
                   context->connected, (long)state.lX, (long)state.lY,
                   (long)state.lZ, (long)state.lRx, (long)state.lRy,
                   (long)state.lRz, (long)state.rglSlider[0],
                   (long)state.rglSlider[1], (unsigned long)state.rgdwPOV[0],
                   (unsigned long)state.rgdwPOV[1],
                   (unsigned long)state.rgdwPOV[2],
                   (unsigned long)state.rgdwPOV[3], state.rgbButtons[0],
                   state.rgbButtons[1], state.rgbButtons[2],
                   state.rgbButtons[3]);
        } else {
            printf("device=%u state_result=0x%08lx\n", context->connected,
                   (unsigned long)result);
        }
    } else {
        printf("device=%u acquire_result=0x%08lx\n", context->connected,
               (unsigned long)result);
    }

    IDirectInputDevice8_Unacquire(device);
    IDirectInputDevice8_Release(device);
    return DIENUM_CONTINUE;
}

int main(void) {
    probe_context context;
    HRESULT result;

    ZeroMemory(&context, sizeof(context));
    result = DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION,
                                &IID_IDirectInput8A,
                                (void **)&context.direct_input, NULL);
    if (FAILED(result)) {
        printf("direct_input_probe: create_result=0x%08lx\n",
               (unsigned long)result);
        return 2;
    }

    result = IDirectInput8_EnumDevices(context.direct_input,
                                       DI8DEVCLASS_GAMECTRL,
                                       enumerate_controller, &context,
                                       DIEDFL_ATTACHEDONLY);
    IDirectInput8_Release(context.direct_input);

    if (FAILED(result)) {
        printf("direct_input_probe: enumerate_result=0x%08lx\n",
               (unsigned long)result);
        return 3;
    }

    printf("direct_input_probe: connected=%u\n", context.connected);
    return context.connected != 0 ? 0 : 1;
}
