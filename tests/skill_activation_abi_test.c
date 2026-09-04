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
static const uint8_t *validate_host_approval_flag;
static uint8_t seen_validate_host_approval;
static void *seen_use_skill;
static int seen_use_slot;
static uint8_t seen_use_host_approval;
static uint8_t use_result = 1u;
static unsigned int use_calls;

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
    seen_validate_host_approval = validate_host_approval_flag != NULL ?
        *validate_host_approval_flag : 0u;
    return validate_result;
}

static uint8_t __attribute__((fastcall)) use_mock(
    void *skill,
    void *ignored_edx,
    int slot
) {
    (void)ignored_edx;
    ++use_calls;
    seen_use_skill = skill;
    seen_use_slot = slot;
    seen_use_host_approval = validate_host_approval_flag != NULL ?
        *validate_host_approval_flag : 0u;
    if (use_result != 0u) {
        *((uint8_t *)skill + 0x6cu) = 1u;
        *(int *)((uint8_t *)skill + 0x70u) = slot;
    }
    return use_result;
}

int main(void) {
    uint8_t character[0x100];
    uint8_t skill[0x80];
    uint8_t skill_data[6][0xa0];
    uint8_t skill_context[4];
    uint8_t include_unavailable = 0u;
    SudekiMpSkillActivationApi api;
    SudekiMpSkillActivationResult result;
    SudekiMpSkillQuickSkillList list;
    SudekiMpSkillQuickSkillRow row;
    SudekiMpCharacterSkillState state;
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
        *(uint32_t *)(skill_data[index] + 0x94u) = 20u + index;
    }
    api.availability_target = availability_mock;
    api.validate = validate_mock;
    api.use = use_mock;
    api.include_unavailable_skills = &include_unavailable;
    validate_host_approval_flag = &include_unavailable;

    check(SudekiMpDescribeCharacterQuickSkillsWithApi(
              character, &api, &list) && list.row_count == 6u,
        "custom-menu Skills snapshot uses the native filtered CSkill order");
    check(list.rows[2].ordinal == 2u && list.rows[2].slot == 12 &&
            list.rows[2].cost == 22u && list.rows[2].available != 0u,
        "custom-menu Skills row retains native execute ordinal, slot, cost, and availability");

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
    ZeroMemory(&row, sizeof(row));
    check(SudekiMpDescribeCharacterSkillSlotWithApi(
              character, 14, &api, &row),
        "exact actor-local skill slot can be described for LAN transport");
    check(row.slot == 14 && row.cost == 24u && row.available != 0u,
        "exact slot description retains native cost and availability");
    result = SudekiMpActivateCharacterSkillSlotWithApi(
        character, 14, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED &&
            result.skill_data == skill_data[4] && result.slot == 14,
        "exact actor-local slot starts through the native validator and Use");
    check(seen_validate_slot == 14 && seen_use_slot == 14,
        "slot activation does not reinterpret the wire value as a UI ordinal");
    check(SudekiMpObserveCharacterSkillWithApi(character, &api, &state) &&
            state.active != 0u && state.skill == skill &&
            state.slot == 14 && state.cost == 24u,
        "host observation exposes only the exact active slot and authored cost");
    skill[0x6cu] = 0u;
    check(SudekiMpObserveCharacterSkillWithApi(character, &api, &state) &&
            state.active == 0u && state.slot == -1,
        "inactive actor skill state is explicit and pointer-local");

    *(int *)(skill_data[5] + 0x0cu) = 14;
    check(!SudekiMpDescribeCharacterSkillSlotWithApi(
              character, 14, &api, &row),
        "duplicate native slot identities are rejected instead of guessed");
    result = SudekiMpActivateCharacterSkillSlotWithApi(
        character, 14, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT,
        "duplicate slot activation fails closed");
    *(int *)(skill_data[5] + 0x0cu) = 15;

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
    result = SudekiMpActivateCharacterSkillSlotWithApi(character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE,
        "exact slot activation also preserves native availability rejection");

    skill[0x6cu] = 0u;
    availability_result = 0u;
    skill_data[0][0x08u] = 0u;
    validate_result = 0;
    use_result = 1u;
    result = SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
        character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED &&
            result.skill_data == skill_data[0] && result.slot == 10,
        "host-approved replay resolves the stable native slot despite a different client unlock state");
    check(skill_data[0][0x08u] == 0u,
        "host-approved replay restores the client's authored enable byte");
    check(seen_validate_host_approval == 1u &&
            seen_use_host_approval == 1u && include_unavailable == 0u,
        "host-approved replay scopes and restores the native unavailable/no-SP admission byte");
    check(seen_validate_slot == 10 && seen_use_slot == 10,
        "host-approved replay retains the native validator and Use path");

    skill[0x6cu] = 0u;
    validate_result = 3;
    use_calls = 0u;
    result = SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
        character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED &&
            result.validation_result == 3 && use_calls == 0u,
        "host-approved replay never bypasses an invalid local actor state");
    check(skill_data[0][0x08u] == 0u && include_unavailable == 0u,
        "rejected host-approved replay restores the client's authored skill bytes");

    skill[0x6cu] = 0u;
    validate_result = 4;
    result = SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
        character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED &&
            skill_data[0][0x08u] == 0u && include_unavailable == 0u,
        "host-approved replay fails closed and restores local state when the native task validator rejects");

    if (failures != 0) {
        fprintf(stderr, "%d skill activation ABI test(s) failed\n", failures);
        return 1;
    }
    puts("skill activation ABI tests passed");
    return 0;
}
