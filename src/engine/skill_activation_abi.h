#ifndef SUDEKIMP_SKILL_ACTIVATION_ABI_H
#define SUDEKIMP_SKILL_ACTIVATION_ABI_H

#include <windows.h>
#include <stdint.h>

typedef int (__attribute__((regparm(2))) *SudekiMpSkillValidateFunction)(
    void *skill,
    int slot
);
typedef uint8_t (__attribute__((fastcall)) *SudekiMpSkillUseFunction)(
    void *skill,
    void *ignored_edx,
    int slot
);

typedef struct SudekiMpSkillActivationApi {
    void *availability_target;
    SudekiMpSkillValidateFunction validate;
    SudekiMpSkillUseFunction use;
    const uint8_t *include_unavailable_skills;
} SudekiMpSkillActivationApi;

typedef enum SudekiMpSkillActivationStatus {
    SUDEKIMP_SKILL_ACTIVATION_STARTED = 0,
    SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT = 1,
    SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE = 2,
    SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED = 3,
    SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED = 4
} SudekiMpSkillActivationStatus;

typedef struct SudekiMpSkillActivationResult {
    SudekiMpSkillActivationStatus status;
    void *skill;
    void *skill_data;
    int slot;
    int validation_result;
    uint8_t use_result;
} SudekiMpSkillActivationResult;

uint8_t SudekiMpCallSkillAvailability(
    void *target,
    void *skill_data,
    void *character_skill_context
);
BOOL SudekiMpInitializeSkillActivationAbi(HMODULE game_module);
void SudekiMpResetSkillActivationAbi(void);
SudekiMpSkillActivationResult SudekiMpActivateCharacterQuickSkill(
    void *character,
    unsigned int ordinal
);
SudekiMpSkillActivationResult SudekiMpActivateCharacterQuickSkillWithApi(
    void *character,
    unsigned int ordinal,
    const SudekiMpSkillActivationApi *api
);
const char *SudekiMpSkillActivationStatusName(
    SudekiMpSkillActivationStatus status
);

#endif
