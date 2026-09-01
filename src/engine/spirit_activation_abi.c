#include "engine/spirit_activation_abi.h"

#include <stddef.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Spirit activation ABI requires 32-bit GCC assembly support"
#endif

enum {
    RVA_SPIRIT_VALIDATE = 0x00010940u,
    RVA_SPIRIT_ACTIVATE = 0x0000fba0u,
    RVA_SPIRIT_MANAGER_GLOBAL = 0x00408d30u,
    CHARACTER_COMPONENT_OFFSET = 0x2cu,
    RESOURCE_TYPE_VTABLE_SLOT = 4u,
    SUPPORTED_IMAGE_SIZE = 0x0045f000u
};

static const uint8_t expected_validate_prefix[] = {
    0x83, 0xec, 0x2c, 0x80, 0x3d
};
static const uint8_t expected_validate_suffix[] = {
    0x00, 0x55, 0x8b, 0x6c, 0x24, 0x38
};
static const uint8_t expected_activate_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x81, 0xec,
    0xc4, 0x00, 0x00, 0x00, 0x53, 0x56, 0x8b, 0x75
};

static HMODULE native_module;
static SudekiMpSpiritActivationApi native_api;
static volatile LONG activation_in_progress;

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL validate_entry_matches(uint8_t *base) {
    uint32_t address;

    if (base == NULL || memcmp(base + RVA_SPIRIT_VALIDATE,
            expected_validate_prefix, sizeof(expected_validate_prefix)) != 0) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    if (memcmp(base + RVA_SPIRIT_ACTIVATE, expected_activate_entry,
            sizeof(expected_activate_entry)) != 0) {
        SetLastError(ERROR_BAD_LENGTH);
        return FALSE;
    }
    memcpy(&address, base + RVA_SPIRIT_VALIDATE +
        sizeof(expected_validate_prefix), sizeof(address));
    if (address != (uint32_t)(uintptr_t)(base + 0x00349570u)) {
        SetLastError(ERROR_INVALID_ADDRESS);
        return FALSE;
    }
    if (memcmp(base + RVA_SPIRIT_VALIDATE + sizeof(expected_validate_prefix) +
            sizeof(address), expected_validate_suffix,
            sizeof(expected_validate_suffix)) != 0) {
        SetLastError(ERROR_BAD_FORMAT);
        return FALSE;
    }
    return TRUE;
}

static SudekiMpSpiritActivationResult empty_result(
    SudekiMpSpiritActivationStatus status
) {
    SudekiMpSpiritActivationResult result;

    ZeroMemory(&result, sizeof(result));
    result.status = status;
    result.strike_id = -1;
    result.validation_result = -1;
    result.activation_result = -1;
    return result;
}

BOOL SudekiMpResolveSpiritStrikeId(unsigned int resource_type,
    unsigned int variant, int *strike_id) {
    int first;

    if (strike_id == NULL || variant < 1u || variant > 2u) {
        return FALSE;
    }
    switch (resource_type) {
    case 0x23u: first = 0; break;
    case 0x01u: first = 2; break;
    case 0x05u: first = 4; break;
    case 0x0eu: first = 6; break;
    default: return FALSE;
    }
    *strike_id = first + (int)variant - 1;
    return TRUE;
}

BOOL SudekiMpInitializeSpiritActivationAbi(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;

    if (native_module != NULL) {
        SetLastError(ERROR_ALREADY_INITIALIZED);
        return FALSE;
    }
    if (!validate_entry_matches(base)) {
        return FALSE;
    }
    if (!readable_memory(base + RVA_SPIRIT_MANAGER_GLOBAL,
            sizeof(void *))) {
        SetLastError(ERROR_NOACCESS);
        return FALSE;
    }
    native_module = game_module;
    native_api.manager = *(void **)(base + RVA_SPIRIT_MANAGER_GLOBAL);
    native_api.validate = (SudekiMpSpiritValidateFunction)(
        base + RVA_SPIRIT_VALIDATE);
    native_api.activate = (SudekiMpSpiritActivateFunction)(
        base + RVA_SPIRIT_ACTIVATE);
    InterlockedExchange(&activation_in_progress, 0);
    return TRUE;
}

void SudekiMpResetSpiritActivationAbi(void) {
    ZeroMemory(&native_api, sizeof(native_api));
    native_module = NULL;
    InterlockedExchange(&activation_in_progress, 0);
}

BOOL SudekiMpDescribeCharacterSpiritOptionsWithApi(
    void *character,
    const SudekiMpSpiritActivationApi *api,
    SudekiMpSpiritQuickOptionList *options
) {
    uint8_t *component;
    void **vtable;
    unsigned int resource_type;
    unsigned int variant;

    if (character == NULL || options == NULL || api == NULL ||
        api->manager == NULL || api->validate == NULL ||
        api->resource_type == NULL ||
        !readable_memory((uint8_t *)character + CHARACTER_COMPONENT_OFFSET,
            sizeof(void *))) {
        return FALSE;
    }
    component = (uint8_t *)character + CHARACTER_COMPONENT_OFFSET;
    vtable = *(void ***)component;
    if (!readable_memory(vtable,
            (RESOURCE_TYPE_VTABLE_SLOT + 1u) * sizeof(void *))) {
        return FALSE;
    }
    if (vtable[RESOURCE_TYPE_VTABLE_SLOT] != (void *)api->resource_type) {
        return FALSE;
    }
    resource_type = (unsigned int)api->resource_type(component);
    ZeroMemory(options, sizeof(*options));
    options->resource_type = resource_type;
    for (variant = 1u; variant <= 2u; ++variant) {
        SudekiMpSpiritQuickOption *option =
            &options->options[variant - 1u];

        option->variant = variant;
        if (!SudekiMpResolveSpiritStrikeId(resource_type, variant,
                &option->strike_id)) {
            ZeroMemory(options, sizeof(*options));
            return FALSE;
        }
        option->validation_result = api->validate(api->manager,
            option->strike_id);
        option->available = option->validation_result == 0 ? 1u : 0u;
    }
    options->option_count = 2u;
    return TRUE;
}

BOOL SudekiMpDescribeCharacterSpiritOptions(
    void *character,
    SudekiMpSpiritQuickOptionList *options
) {
    uint8_t *component;
    void **vtable;
    BOOL described;

    if (native_module == NULL || character == NULL ||
        !readable_memory((uint8_t *)character + CHARACTER_COMPONENT_OFFSET,
            sizeof(void *))) {
        return FALSE;
    }
    component = (uint8_t *)character + CHARACTER_COMPONENT_OFFSET;
    vtable = *(void ***)component;
    if (!readable_memory(vtable,
            (RESOURCE_TYPE_VTABLE_SLOT + 1u) * sizeof(void *)) ||
        (uint8_t *)vtable[RESOURCE_TYPE_VTABLE_SLOT] <
            (uint8_t *)native_module ||
        (uint8_t *)vtable[RESOURCE_TYPE_VTABLE_SLOT] >=
            (uint8_t *)native_module + SUPPORTED_IMAGE_SIZE) {
        return FALSE;
    }
    native_api.manager = *(void **)((uint8_t *)native_module +
        RVA_SPIRIT_MANAGER_GLOBAL);
    native_api.resource_type = (SudekiMpSpiritResourceTypeFunction)
        vtable[RESOURCE_TYPE_VTABLE_SLOT];
    described = SudekiMpDescribeCharacterSpiritOptionsWithApi(
        character, &native_api, options);
    native_api.resource_type = NULL;
    return described;
}

SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpiritWithApi(
    void *character,
    unsigned int variant,
    const SudekiMpSpiritActivationApi *api
) {
    SudekiMpSpiritActivationResult result = empty_result(
        SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT);
    uint8_t *component;
    void **vtable;
    unsigned int resource_type;

    if (character == NULL || api == NULL || api->manager == NULL ||
        api->validate == NULL || api->activate == NULL ||
        api->resource_type == NULL || variant < 1u || variant > 2u) {
        if (variant < 1u || variant > 2u) {
            result.status = SUDEKIMP_SPIRIT_ACTIVATION_INVALID_VARIANT;
        }
        return result;
    }
    if (!readable_memory(character,
            CHARACTER_COMPONENT_OFFSET + sizeof(void *))) {
        return result;
    }
    component = (uint8_t *)character + CHARACTER_COMPONENT_OFFSET;
    vtable = *(void ***)component;
    if (!readable_memory(vtable,
            (RESOURCE_TYPE_VTABLE_SLOT + 1u) * sizeof(void *)) ||
        vtable[RESOURCE_TYPE_VTABLE_SLOT] != (void *)api->resource_type) {
        return result;
    }
    resource_type = (unsigned int)api->resource_type(component);
    if (!SudekiMpResolveSpiritStrikeId(resource_type, variant,
            &result.strike_id)) {
        return result;
    }
    if (InterlockedCompareExchange(&activation_in_progress, 1, 0) != 0) {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_BUSY;
        return result;
    }
    result.validation_result = api->validate(api->manager, result.strike_id);
    if (result.validation_result != 0) {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED;
    } else {
        result.activation_result = api->activate(api->manager,
            result.strike_id);
        result.status = result.activation_result != 0 ?
            SUDEKIMP_SPIRIT_ACTIVATION_STARTED :
            SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED;
    }
    InterlockedExchange(&activation_in_progress, 0);
    return result;
}

SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpirit(
    void *character,
    unsigned int variant
) {
    SudekiMpSpiritActivationResult result;
    uint8_t *component;
    void **vtable;

    if (native_module == NULL || character == NULL ||
        !readable_memory((uint8_t *)character + CHARACTER_COMPONENT_OFFSET,
            sizeof(void *))) {
        return empty_result(SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT);
    }
    component = (uint8_t *)character + CHARACTER_COMPONENT_OFFSET;
    vtable = *(void ***)component;
    if (!readable_memory(vtable,
            (RESOURCE_TYPE_VTABLE_SLOT + 1u) * sizeof(void *)) ||
        (uint8_t *)vtable[RESOURCE_TYPE_VTABLE_SLOT] <
            (uint8_t *)native_module ||
        (uint8_t *)vtable[RESOURCE_TYPE_VTABLE_SLOT] >=
            (uint8_t *)native_module + SUPPORTED_IMAGE_SIZE) {
        return empty_result(SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT);
    }
    native_api.manager = *(void **)((uint8_t *)native_module +
        RVA_SPIRIT_MANAGER_GLOBAL);
    native_api.resource_type = (SudekiMpSpiritResourceTypeFunction)
        vtable[RESOURCE_TYPE_VTABLE_SLOT];
    result = SudekiMpActivateCharacterSpiritWithApi(character, variant,
        &native_api);
    native_api.resource_type = NULL;
    return result;
}

const char *SudekiMpSpiritActivationStatusName(
    SudekiMpSpiritActivationStatus status
) {
    switch (status) {
    case SUDEKIMP_SPIRIT_ACTIVATION_STARTED: return "started";
    case SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT: return "invalid_context";
    case SUDEKIMP_SPIRIT_ACTIVATION_INVALID_VARIANT: return "invalid_variant";
    case SUDEKIMP_SPIRIT_ACTIVATION_BUSY: return "busy";
    case SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED:
        return "validation_rejected";
    case SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED:
        return "activation_rejected";
    default: return "unknown";
    }
}
