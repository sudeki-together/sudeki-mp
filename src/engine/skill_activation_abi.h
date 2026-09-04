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

enum {
    SUDEKIMP_SKILL_ACTIVATION_MAX_QUICK_SKILLS = 6u
};

/* Pointer-free enough to cross into the local Quick Menu model.  `ordinal`
 * is the same filtered ordinal accepted by ActivateCharacterQuickSkill; it
 * is never a guessed UI row or a raw CSkill pointer. */
typedef struct SudekiMpSkillQuickSkillRow {
    uint32_t ordinal;
    int slot;
    uint32_t cost;
    uint8_t available;
    uint8_t reserved[3];
} SudekiMpSkillQuickSkillRow;

typedef struct SudekiMpSkillQuickSkillList {
    uint32_t row_count;
    SudekiMpSkillQuickSkillRow
        rows[SUDEKIMP_SKILL_ACTIVATION_MAX_QUICK_SKILLS];
} SudekiMpSkillQuickSkillList;

typedef struct SudekiMpCharacterSkillState {
    void *skill;
    int slot;
    uint32_t cost;
    uint8_t active;
    uint8_t reserved[3];
} SudekiMpCharacterSkillState;

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
/* Starts one exact actor-local CSkill slot. This is the stable semantic used
 * by the exact-hash LAN profile after the host has observed native admission;
 * it is never a pointer received from the network. */
SudekiMpSkillActivationResult SudekiMpActivateCharacterSkillSlot(
    void *character,
    int slot
);
SudekiMpSkillActivationResult SudekiMpActivateCharacterSkillSlotWithApi(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api
);
/* Replays a skill whose exact actor-local slot has already been admitted by
 * the authoritative LAN host.  The client may not have the host's learned
 * skill flags, so this presentation-only path temporarily enables the exact
 * resolved SkillData record and the native unavailable/no-SP admission byte
 * while retaining the native validator and Use entry.  Both authored bytes
 * are restored before this call returns. */
SudekiMpSkillActivationResult
SudekiMpReplayHostApprovedCharacterSkillSlot(
    void *character,
    int slot
);
SudekiMpSkillActivationResult
SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api
);
BOOL SudekiMpDescribeCharacterQuickSkills(
    void *character,
    SudekiMpSkillQuickSkillList *list
);
BOOL SudekiMpDescribeCharacterQuickSkillsWithApi(
    void *character,
    const SudekiMpSkillActivationApi *api,
    SudekiMpSkillQuickSkillList *list
);
BOOL SudekiMpDescribeCharacterSkillSlot(
    void *character,
    int slot,
    SudekiMpSkillQuickSkillRow *row
);
BOOL SudekiMpDescribeCharacterSkillSlotWithApi(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api,
    SudekiMpSkillQuickSkillRow *row
);
/* Reads only the exact actor-owned CSkill transaction state. Native pointers
 * remain process-local; callers serialize only slot/cost/active semantics. */
BOOL SudekiMpObserveCharacterSkill(
    void *character,
    SudekiMpCharacterSkillState *state
);
BOOL SudekiMpObserveCharacterSkillWithApi(
    void *character,
    const SudekiMpSkillActivationApi *api,
    SudekiMpCharacterSkillState *state
);
const char *SudekiMpSkillActivationStatusName(
    SudekiMpSkillActivationStatus status
);

#endif
