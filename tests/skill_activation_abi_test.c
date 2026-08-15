#include "engine/player_combat_context.h"
#include "engine/skill_activation_abi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
void *seen_availability_data;
void *seen_availability_context;
uint8_t availability_result = 1u;
static void *seen_validate_skill;
static int seen_validate_slot;
static int validate_result;
static void *seen_use_skill;
static int seen_use_slot;
static uint8_t use_result = 1u;

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

__attribute__((naked, noinline))
static uint8_t availability_mock(void) {
    __asm__ volatile(
        "movl %eax, _seen_availability_data\n\t"
        "movl %esi, _seen_availability_context\n\t"
        "movzbl _availability_result, %eax\n\t"
        "ret\n\t"
    );
}

static int __attribute__((regparm(2))) validate_mock(
    void *skill,
    int slot
) {
    seen_validate_skill = skill;
    seen_validate_slot = slot;
    return validate_result;
}

static uint8_t __attribute__((fastcall)) use_mock(
    void *skill,
    void *ignored_edx,
    int slot
) {
    (void)ignored_edx;
    seen_use_skill = skill;
    seen_use_slot = slot;
    if (use_result != 0u) {
        *((uint8_t *)skill + 0x6cu) = 1u;
    }
    return use_result;
}

int main(void) {
    uint8_t character[0x100];
    uint8_t skill[0x80];
    uint8_t skill_data[6][0x20];
    uint8_t skill_context[4];
    uint8_t include_unavailable = 0u;
    SudekiMpSkillActivationApi api;
    SudekiMpSkillActivationResult result;
    unsigned int index;

    ZeroMemory(character, sizeof(character));
    ZeroMemory(skill, sizeof(skill));
    ZeroMemory(skill_data, sizeof(skill_data));
    ZeroMemory(skill_context, sizeof(skill_context));
    ZeroMemory(&api, sizeof(api));
    *(void **)(character + 0xd4u) = skill_context;
    *(void **)(character + 0xd8u) = skill;
    *(void **)(skill + 0x10u) = character;
    for (index = 0u; index < 6u; ++index) {
        *(void **)(skill + 0x3cu + index * sizeof(void *)) =
            skill_data[index];
        *(unsigned int *)(skill + 0x54u + index * sizeof(unsigned int)) =
            index;
        skill_data[index][0x08u] = 1u;
        *(int *)(skill_data[index] + 0x0cu) = (int)(10u + index);
    }
    api.availability_target = availability_mock;
    api.validate = validate_mock;
    api.use = use_mock;
    api.include_unavailable_skills = &include_unavailable;

    SudekiMpCombatContextsReset();
    SudekiMpCombatContextSetCharacter(1u, character);
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 2u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED,
        "third available skill starts through native ABI surface");
    check(result.skill == skill && result.skill_data == skill_data[2] &&
        result.slot == 12, "ordered skill data maps to its native slot");
    check(seen_availability_data == skill_data[2] &&
        seen_availability_context == skill_context,
        "availability bridge supplies EAX skill data and ESI context");
    check(seen_validate_skill == skill && seen_validate_slot == 12,
        "native validator receives CSkill and selected slot");
    check(seen_use_skill == skill && seen_use_slot == 12,
        "native Use receives CSkill and selected slot");

    skill[0x6cu] = 0u;
    validate_result = 3;
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 0u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED &&
        result.validation_result == 3,
        "validation rejection is returned without bypass");

    validate_result = 0;
    use_result = 0u;
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 0u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED,
        "native Use rejection is returned without patching state");

    use_result = 1u;
    availability_result = 0u;
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 0u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE,
        "unavailable authored skills remain unavailable");

    if (failures != 0) {
        fprintf(stderr, "%d skill activation ABI test(s) failed\n", failures);
        return 1;
    }
    puts("skill activation ABI tests passed");
    return 0;
}
