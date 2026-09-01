#ifndef SUDEKIMP_SPIRIT_ACTIVATION_ABI_H
#define SUDEKIMP_SPIRIT_ACTIVATION_ABI_H

#include <windows.h>
#include <stdint.h>

typedef int (__stdcall *SudekiMpSpiritValidateFunction)(
    void *manager,
    int strike_id
);
typedef int (__stdcall *SudekiMpSpiritActivateFunction)(
    void *manager,
    int strike_id
);
typedef int (__attribute__((thiscall)) *SudekiMpSpiritResourceTypeFunction)(
    void *component
);

typedef struct SudekiMpSpiritActivationApi {
    void *manager;
    SudekiMpSpiritValidateFunction validate;
    SudekiMpSpiritActivateFunction activate;
    SudekiMpSpiritResourceTypeFunction resource_type;
} SudekiMpSpiritActivationApi;

typedef enum SudekiMpSpiritActivationStatus {
    SUDEKIMP_SPIRIT_ACTIVATION_STARTED = 0,
    SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT,
    SUDEKIMP_SPIRIT_ACTIVATION_INVALID_VARIANT,
    SUDEKIMP_SPIRIT_ACTIVATION_BUSY,
    SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED,
    SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED
} SudekiMpSpiritActivationStatus;

typedef struct SudekiMpSpiritActivationResult {
    SudekiMpSpiritActivationStatus status;
    int strike_id;
    int validation_result;
    int activation_result;
} SudekiMpSpiritActivationResult;

typedef struct SudekiMpSpiritQuickOption {
    unsigned int variant;
    int strike_id;
    int validation_result;
    uint8_t available;
    uint8_t reserved[3];
} SudekiMpSpiritQuickOption;

typedef struct SudekiMpSpiritQuickOptionList {
    unsigned int resource_type;
    unsigned int option_count;
    SudekiMpSpiritQuickOption options[2];
} SudekiMpSpiritQuickOptionList;

/* Resolves the retail two-strike pair for Tal/Ailish/Buki/Elco.  Variants are
 * one-based, matching the shipped QuickMenu ordering. */
BOOL SudekiMpResolveSpiritStrikeId(unsigned int resource_type,
    unsigned int variant, int *strike_id);
BOOL SudekiMpInitializeSpiritActivationAbi(HMODULE game_module);
void SudekiMpResetSpiritActivationAbi(void);
/* Read-only actor-specific snapshot.  It uses the same retail strike
 * validator as execution and never opens the global QuickMenu singleton. */
BOOL SudekiMpDescribeCharacterSpiritOptions(
    void *character,
    SudekiMpSpiritQuickOptionList *options
);
BOOL SudekiMpDescribeCharacterSpiritOptionsWithApi(
    void *character,
    const SudekiMpSpiritActivationApi *api,
    SudekiMpSpiritQuickOptionList *options
);
SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpirit(
    void *character,
    unsigned int variant
);
SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpiritWithApi(
    void *character,
    unsigned int variant,
    const SudekiMpSpiritActivationApi *api
);
const char *SudekiMpSpiritActivationStatusName(
    SudekiMpSpiritActivationStatus status
);

#endif
