#include "engine/spirit_activation_abi.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct TestCharacter {
    uint8_t padding[0x2c];
    void **vtable;
} TestCharacter;

static int validation_result;
static int activation_result;
static int observed_strike;

static int __attribute__((thiscall)) test_resource_type(void *component) {
    (void)component;
    return 0x05;
}

static int __stdcall test_validate(void *manager, int strike_id) {
    CHECK(manager == (void *)(uintptr_t)0x4567u);
    observed_strike = strike_id;
    return validation_result;
}

static int __stdcall test_activate(void *manager, int strike_id) {
    CHECK(manager == (void *)(uintptr_t)0x4567u);
    observed_strike = strike_id;
    return activation_result;
}

int main(void) {
    void *vtable[5];
    TestCharacter character;
    SudekiMpSpiritActivationApi api;
    SudekiMpSpiritActivationResult result;
    SudekiMpSpiritQuickOptionList options;
    int strike_id;

    CHECK(SudekiMpResolveSpiritStrikeId(0x23u, 1u, &strike_id) &&
        strike_id == 0);
    CHECK(SudekiMpResolveSpiritStrikeId(0x01u, 2u, &strike_id) &&
        strike_id == 3);
    CHECK(SudekiMpResolveSpiritStrikeId(0x05u, 1u, &strike_id) &&
        strike_id == 4);
    CHECK(SudekiMpResolveSpiritStrikeId(0x0eu, 2u, &strike_id) &&
        strike_id == 7);
    CHECK(!SudekiMpResolveSpiritStrikeId(0x99u, 1u, &strike_id));
    CHECK(!SudekiMpResolveSpiritStrikeId(0x23u, 0u, &strike_id));

    memset(&character, 0, sizeof(character));
    memset(vtable, 0, sizeof(vtable));
    vtable[4] = (void *)test_resource_type;
    character.vtable = vtable;
    memset(&api, 0, sizeof(api));
    api.manager = (void *)(uintptr_t)0x4567u;
    api.validate = test_validate;
    api.activate = test_activate;
    api.resource_type = test_resource_type;

    validation_result = 0;
    CHECK(SudekiMpDescribeCharacterSpiritOptionsWithApi(
        &character, &api, &options));
    CHECK(options.resource_type == 0x05u && options.option_count == 2u);
    CHECK(options.options[0].variant == 1u &&
        options.options[0].strike_id == 4 && options.options[0].available);
    CHECK(options.options[1].variant == 2u &&
        options.options[1].strike_id == 5 && options.options[1].available);

    validation_result = 9;
    CHECK(SudekiMpDescribeCharacterSpiritOptionsWithApi(
        &character, &api, &options));
    CHECK(!options.options[0].available &&
        options.options[0].validation_result == 9);

    validation_result = 0;
    activation_result = 1;
    observed_strike = -1;
    result = SudekiMpActivateCharacterSpiritWithApi(&character, 2u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_STARTED);
    CHECK(result.strike_id == 5 && observed_strike == 5);

    validation_result = 6;
    result = SudekiMpActivateCharacterSpiritWithApi(&character, 1u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED);
    CHECK(result.validation_result == 6);

    validation_result = 0;
    activation_result = 0;
    result = SudekiMpActivateCharacterSpiritWithApi(&character, 1u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED);
    CHECK(result.activation_result == 0);

    result = SudekiMpActivateCharacterSpiritWithApi(&character, 3u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_INVALID_VARIANT);
    puts("spirit activation ABI tests passed");
    return 0;
}
