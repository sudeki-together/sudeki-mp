#include "engine/skill_activation_abi.h"

#include "engine/player_combat_context.h"

#include <stddef.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Skill activation ABI requires 32-bit GCC assembly support"
#endif

enum {
    RVA_SKILL_DATA_AVAILABLE = 0x000da2a0u,
    RVA_SKILL_VALIDATE = 0x000b4bc0u,
    RVA_SKILL_USE = 0x000b4810u,
    RVA_INCLUDE_UNAVAILABLE_SKILLS = 0x003c2fccu,
    RVA_SKILL_AVAILABILITY_FLAG = 0x003c2fd9u,
    RVA_SKILL_VALIDATION_FLAG = 0x0034a8b0u,
    CHARACTER_SKILL_CONTEXT_OFFSET = 0xd4u,
    CHARACTER_SKILL_OFFSET = 0xd8u,
    CSKILL_DATA_ARRAY_OFFSET = 0x3cu,
    CSKILL_ORDER_ARRAY_OFFSET = 0x54u,
    CSKILL_ACTIVE_OFFSET = 0x6cu,
    CSKILL_ACTIVE_SLOT_OFFSET = 0x70u,
    SKILL_DATA_ENABLED_OFFSET = 0x08u,
    SKILL_DATA_SLOT_OFFSET = 0x0cu,
    SKILL_DATA_COST_OFFSET = 0x94u,
    NATIVE_QUICK_SKILL_COUNT = 6u
};

static const uint8_t expected_availability_prefix[] = {
    0x80, 0x3d
};
static const uint8_t expected_availability_suffix[] = {
    0x00, 0x53, 0x8b, 0x58, 0x0c
};
static const uint8_t expected_validate_prefix[] = {
    0x80, 0x3d
};
static const uint8_t expected_validate_suffix[] = {
    0x00, 0x75, 0x06, 0xb8, 0x05
};
static const uint8_t expected_use_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x81, 0xec,
    0xcc, 0x00, 0x00, 0x00, 0x53, 0x56, 0x8b, 0x75
};

static SudekiMpSkillActivationApi native_api;
static HMODULE native_module;

static BOOL relocated_entry_matches(
    const uint8_t *entry,
    const uint8_t *prefix,
    size_t prefix_size,
    uintptr_t expected_address,
    const uint8_t *suffix,
    size_t suffix_size
) {
    uint32_t encoded_address;

    if (memcmp(entry, prefix, prefix_size) != 0) {
        return FALSE;
    }
    memcpy(&encoded_address, entry + prefix_size, sizeof(encoded_address));
    return encoded_address == (uint32_t)expected_address &&
        memcmp(
            entry + prefix_size + sizeof(encoded_address),
            suffix,
            suffix_size
        ) == 0;
}

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

static BOOL writable_memory(void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;

    if (!readable_memory(pointer, size) ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

__attribute__((naked, noinline))
uint8_t SudekiMpCallSkillAvailability(
    void *target,
    void *skill_data,
    void *character_skill_context
) {
    (void)target;
    (void)skill_data;
    (void)character_skill_context;
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %esi\n\t"
        "movl 8(%ebp), %edx\n\t"
        "movl 12(%ebp), %eax\n\t"
        "movl 16(%ebp), %esi\n\t"
        "call *%edx\n\t"
        "popl %esi\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

BOOL SudekiMpInitializeSkillActivationAbi(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;

    if (base == NULL || native_module != NULL ||
        !relocated_entry_matches(
            base + RVA_SKILL_DATA_AVAILABLE,
            expected_availability_prefix,
            sizeof(expected_availability_prefix),
            (uintptr_t)(base + RVA_SKILL_AVAILABILITY_FLAG),
            expected_availability_suffix,
            sizeof(expected_availability_suffix)) ||
        !relocated_entry_matches(
            base + RVA_SKILL_VALIDATE,
            expected_validate_prefix,
            sizeof(expected_validate_prefix),
            (uintptr_t)(base + RVA_SKILL_VALIDATION_FLAG),
            expected_validate_suffix,
            sizeof(expected_validate_suffix)) ||
        memcmp(
            base + RVA_SKILL_USE,
            expected_use_entry,
            sizeof(expected_use_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    native_module = game_module;
    native_api.availability_target = base + RVA_SKILL_DATA_AVAILABLE;
    native_api.validate = (SudekiMpSkillValidateFunction)(
        base + RVA_SKILL_VALIDATE
    );
    native_api.use = (SudekiMpSkillUseFunction)(base + RVA_SKILL_USE);
    native_api.include_unavailable_skills =
        base + RVA_INCLUDE_UNAVAILABLE_SKILLS;
    return TRUE;
}

void SudekiMpResetSkillActivationAbi(void) {
    ZeroMemory(&native_api, sizeof(native_api));
    native_module = NULL;
}

static SudekiMpSkillActivationResult empty_result(
    SudekiMpSkillActivationStatus status
) {
    SudekiMpSkillActivationResult result;
    ZeroMemory(&result, sizeof(result));
    result.status = status;
    result.slot = -1;
    result.validation_result = -1;
    return result;
}

SudekiMpSkillActivationResult SudekiMpActivateCharacterQuickSkillWithApi(
    void *character,
    unsigned int ordinal,
    const SudekiMpSkillActivationApi *api
) {
    SudekiMpSkillActivationResult result = empty_result(
        SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT
    );
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *skill;
    void *skill_context;
    unsigned int index;
    unsigned int available_ordinal = 0u;

    if (character_bytes == NULL || ordinal >= NATIVE_QUICK_SKILL_COUNT ||
        api == NULL || api->availability_target == NULL ||
        api->validate == NULL || api->use == NULL ||
        !readable_memory(
            api->include_unavailable_skills,
            sizeof(*api->include_unavailable_skills)) ||
        !readable_memory(
            character_bytes,
            CHARACTER_SKILL_OFFSET + sizeof(skill))) {
        return result;
    }
    skill_context = *(void **)(character_bytes +
        CHARACTER_SKILL_CONTEXT_OFFSET);
    skill = *(uint8_t **)(character_bytes + CHARACTER_SKILL_OFFSET);
    if (skill_context == NULL ||
        !readable_memory(skill, CSKILL_ORDER_ARRAY_OFFSET +
            NATIVE_QUICK_SKILL_COUNT * sizeof(unsigned int)) ||
        *(void **)(skill + 0x10u) != character) {
        return result;
    }
    result.skill = skill;

    for (index = 0u; index < NATIVE_QUICK_SKILL_COUNT; ++index) {
        unsigned int data_index = *(unsigned int *)(
            skill + CSKILL_ORDER_ARRAY_OFFSET + index * sizeof(unsigned int)
        );
        void *skill_data;
        BOOL available;

        if (data_index >= NATIVE_QUICK_SKILL_COUNT) {
            continue;
        }
        skill_data = *(void **)(
            skill + CSKILL_DATA_ARRAY_OFFSET +
                data_index * sizeof(void *)
        );
        available = readable_memory(
                skill_data,
                SKILL_DATA_SLOT_OFFSET + sizeof(int)) &&
            SudekiMpCallSkillAvailability(
                api->availability_target,
                skill_data,
                skill_context
            ) != 0u &&
            *((uint8_t *)skill_data + SKILL_DATA_ENABLED_OFFSET) != 0u;
        if (!available && *api->include_unavailable_skills == 0u) {
            continue;
        }
        if (available_ordinal++ != ordinal) {
            continue;
        }
        if (skill_data == NULL) {
            break;
        }
        result.skill_data = skill_data;
        result.slot = *(int *)((uint8_t *)skill_data +
            SKILL_DATA_SLOT_OFFSET);
        result.validation_result = api->validate(skill, result.slot);
        if (result.validation_result != 0) {
            result.status = SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED;
            return result;
        }
        result.use_result = api->use(skill, NULL, result.slot);
        if (result.use_result == 0u) {
            result.status = SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED;
            return result;
        }
        result.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
        SudekiMpCombatContextSkillStarted(character, skill);
        return result;
    }
    result.status = SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE;
    return result;
}

BOOL SudekiMpDescribeCharacterSkillSlotWithApi(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api,
    SudekiMpSkillQuickSkillRow *row
) {
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *skill;
    void *skill_context;
    unsigned int index;
    BOOL found = FALSE;

    if (row == NULL) return FALSE;
    ZeroMemory(row, sizeof(*row));
    row->slot = -1;
    if (character_bytes == NULL || slot < 0 || api == NULL ||
        api->availability_target == NULL ||
        !readable_memory(character_bytes,
            CHARACTER_SKILL_OFFSET + sizeof(skill))) return FALSE;
    skill_context = *(void **)(character_bytes +
        CHARACTER_SKILL_CONTEXT_OFFSET);
    skill = *(uint8_t **)(character_bytes + CHARACTER_SKILL_OFFSET);
    if (skill_context == NULL || !readable_memory(skill,
            CSKILL_ORDER_ARRAY_OFFSET +
            NATIVE_QUICK_SKILL_COUNT * sizeof(unsigned int)) ||
        *(void **)(skill + 0x10u) != character) return FALSE;
    for (index = 0u; index < NATIVE_QUICK_SKILL_COUNT; ++index) {
        void *skill_data = *(void **)(skill + CSKILL_DATA_ARRAY_OFFSET +
            index * sizeof(void *));
        if (!readable_memory(skill_data,
                SKILL_DATA_COST_OFFSET + sizeof(uint32_t)) ||
            *(int *)((uint8_t *)skill_data + SKILL_DATA_SLOT_OFFSET) !=
                slot) continue;
        if (found) return FALSE;
        found = TRUE;
        row->ordinal = index;
        row->slot = slot;
        row->cost = *(uint32_t *)((uint8_t *)skill_data +
            SKILL_DATA_COST_OFFSET);
        row->available =
            *((uint8_t *)skill_data + SKILL_DATA_ENABLED_OFFSET) != 0u &&
            SudekiMpCallSkillAvailability(
                api->availability_target, skill_data, skill_context) != 0u;
    }
    return found;
}

SudekiMpSkillActivationResult SudekiMpActivateCharacterSkillSlotWithApi(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api
) {
    SudekiMpSkillActivationResult result = empty_result(
        SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT);
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *skill;
    void *skill_context;
    unsigned int index;
    void *selected_data = NULL;

    if (character_bytes == NULL || slot < 0 || api == NULL ||
        api->availability_target == NULL || api->validate == NULL ||
        api->use == NULL || !readable_memory(character_bytes,
            CHARACTER_SKILL_OFFSET + sizeof(skill))) return result;
    skill_context = *(void **)(character_bytes +
        CHARACTER_SKILL_CONTEXT_OFFSET);
    skill = *(uint8_t **)(character_bytes + CHARACTER_SKILL_OFFSET);
    if (skill_context == NULL || !readable_memory(skill,
            CSKILL_ORDER_ARRAY_OFFSET +
            NATIVE_QUICK_SKILL_COUNT * sizeof(unsigned int)) ||
        *(void **)(skill + 0x10u) != character) return result;
    result.skill = skill;
    for (index = 0u; index < NATIVE_QUICK_SKILL_COUNT; ++index) {
        void *skill_data = *(void **)(skill + CSKILL_DATA_ARRAY_OFFSET +
            index * sizeof(void *));
        if (!readable_memory(skill_data,
                SKILL_DATA_SLOT_OFFSET + sizeof(int)) ||
            *(int *)((uint8_t *)skill_data + SKILL_DATA_SLOT_OFFSET) !=
                slot) continue;
        if (selected_data != NULL) return result;
        selected_data = skill_data;
    }
    if (selected_data == NULL || !readable_memory(selected_data,
            SKILL_DATA_ENABLED_OFFSET + 1u) ||
        *((uint8_t *)selected_data + SKILL_DATA_ENABLED_OFFSET) == 0u ||
        SudekiMpCallSkillAvailability(api->availability_target,
            selected_data, skill_context) == 0u) {
        result.status = SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE;
        return result;
    }
    result.skill_data = selected_data;
    result.slot = slot;
    result.validation_result = api->validate(skill, slot);
    if (result.validation_result != 0) {
        result.status = SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED;
        return result;
    }
    result.use_result = api->use(skill, NULL, slot);
    if (result.use_result == 0u) {
        result.status = SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED;
        return result;
    }
    result.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    SudekiMpCombatContextSkillStarted(character, skill);
    return result;
}

static SudekiMpSkillActivationResult
replay_host_approved_character_skill_slot(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api
) {
    SudekiMpSkillActivationResult result = empty_result(
        SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT);
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *skill;
    unsigned int index;
    uint8_t *selected_data = NULL;
    uint8_t saved_enabled;
    uint8_t *host_approval_flag;
    uint8_t saved_host_approval;

    if (character_bytes == NULL || slot < 0 || api == NULL ||
        api->validate == NULL || api->use == NULL ||
        api->include_unavailable_skills == NULL ||
        !readable_memory(character_bytes,
            CHARACTER_SKILL_OFFSET + sizeof(skill))) return result;
    skill = *(uint8_t **)(character_bytes + CHARACTER_SKILL_OFFSET);
    if (!readable_memory(skill, CSKILL_ORDER_ARRAY_OFFSET +
            NATIVE_QUICK_SKILL_COUNT * sizeof(unsigned int)) ||
        *(void **)(skill + 0x10u) != character) return result;
    result.skill = skill;
    for (index = 0u; index < NATIVE_QUICK_SKILL_COUNT; ++index) {
        uint8_t *skill_data = *(uint8_t **)(
            skill + CSKILL_DATA_ARRAY_OFFSET + index * sizeof(void *));
        if (!readable_memory(skill_data,
                SKILL_DATA_SLOT_OFFSET + sizeof(int)) ||
            *(int *)(skill_data + SKILL_DATA_SLOT_OFFSET) != slot) continue;
        if (selected_data != NULL) return result;
        selected_data = skill_data;
    }
    host_approval_flag = (uint8_t *)(uintptr_t)
        api->include_unavailable_skills;
    if (selected_data == NULL || !writable_memory(
            selected_data + SKILL_DATA_ENABLED_OFFSET, 1u) ||
        !writable_memory(host_approval_flag, 1u)) {
        result.status = SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE;
        return result;
    }
    result.skill_data = selected_data;
    result.slot = slot;
    saved_enabled = selected_data[SKILL_DATA_ENABLED_OFFSET];
    saved_host_approval = *host_approval_flag;
    selected_data[SKILL_DATA_ENABLED_OFFSET] = 1u;
    *host_approval_flag = 1u;
    result.validation_result = api->validate(skill, slot);
    if (result.validation_result != 0) {
        *host_approval_flag = saved_host_approval;
        selected_data[SKILL_DATA_ENABLED_OFFSET] = saved_enabled;
        result.status = SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED;
        return result;
    }
    result.use_result = api->use(skill, NULL, slot);
    *host_approval_flag = saved_host_approval;
    selected_data[SKILL_DATA_ENABLED_OFFSET] = saved_enabled;
    if (result.use_result == 0u) {
        result.status = SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED;
        return result;
    }
    result.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    SudekiMpCombatContextSkillStarted(character, skill);
    return result;
}

SudekiMpSkillActivationResult
SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
    void *character,
    int slot,
    const SudekiMpSkillActivationApi *api
) {
    return replay_host_approved_character_skill_slot(
        character, slot, api);
}

BOOL SudekiMpDescribeCharacterQuickSkillsWithApi(
    void *character,
    const SudekiMpSkillActivationApi *api,
    SudekiMpSkillQuickSkillList *list
) {
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *skill;
    void *skill_context;
    unsigned int index;

    if (list == NULL) {
        return FALSE;
    }
    ZeroMemory(list, sizeof(*list));
    if (character_bytes == NULL || api == NULL ||
        api->availability_target == NULL ||
        !readable_memory(api->include_unavailable_skills,
            sizeof(*api->include_unavailable_skills)) ||
        !readable_memory(character_bytes,
            CHARACTER_SKILL_OFFSET + sizeof(skill))) {
        return FALSE;
    }
    skill_context = *(void **)(character_bytes +
        CHARACTER_SKILL_CONTEXT_OFFSET);
    skill = *(uint8_t **)(character_bytes + CHARACTER_SKILL_OFFSET);
    if (skill_context == NULL || !readable_memory(skill,
            CSKILL_ORDER_ARRAY_OFFSET +
            NATIVE_QUICK_SKILL_COUNT * sizeof(unsigned int)) ||
        *(void **)(skill + 0x10u) != character) {
        return FALSE;
    }
    for (index = 0u; index < NATIVE_QUICK_SKILL_COUNT; ++index) {
        unsigned int data_index = *(unsigned int *)(
            skill + CSKILL_ORDER_ARRAY_OFFSET + index * sizeof(unsigned int));
        void *skill_data;
        BOOL available;
        SudekiMpSkillQuickSkillRow *row;

        if (data_index >= NATIVE_QUICK_SKILL_COUNT) {
            continue;
        }
        skill_data = *(void **)(skill + CSKILL_DATA_ARRAY_OFFSET +
            data_index * sizeof(void *));
        if (!readable_memory(skill_data,
                SKILL_DATA_COST_OFFSET + sizeof(uint32_t))) {
            continue;
        }
        available = *((uint8_t *)skill_data + SKILL_DATA_ENABLED_OFFSET) != 0u &&
            SudekiMpCallSkillAvailability(
                api->availability_target, skill_data, skill_context) != 0u;
        if (!available && *api->include_unavailable_skills == 0u) {
            continue;
        }
        if (list->row_count >= NATIVE_QUICK_SKILL_COUNT) {
            return FALSE;
        }
        row = &list->rows[list->row_count];
        row->ordinal = list->row_count;
        row->slot = *(int *)((uint8_t *)skill_data +
            SKILL_DATA_SLOT_OFFSET);
        row->cost = *(uint32_t *)((uint8_t *)skill_data +
            SKILL_DATA_COST_OFFSET);
        row->available = available != FALSE;
        ++list->row_count;
    }
    return TRUE;
}

BOOL SudekiMpDescribeCharacterQuickSkills(
    void *character,
    SudekiMpSkillQuickSkillList *list
) {
    return native_module != NULL &&
        SudekiMpDescribeCharacterQuickSkillsWithApi(character, &native_api, list);
}

BOOL SudekiMpDescribeCharacterSkillSlot(
    void *character,
    int slot,
    SudekiMpSkillQuickSkillRow *row
) {
    return native_module != NULL &&
        SudekiMpDescribeCharacterSkillSlotWithApi(
            character, slot, &native_api, row);
}

BOOL SudekiMpObserveCharacterSkillWithApi(
    void *character,
    const SudekiMpSkillActivationApi *api,
    SudekiMpCharacterSkillState *state
) {
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *skill;
    SudekiMpSkillQuickSkillRow row;

    if (state == NULL) return FALSE;
    ZeroMemory(state, sizeof(*state));
    state->slot = -1;
    if (api == NULL || character_bytes == NULL ||
        !readable_memory(character_bytes,
            CHARACTER_SKILL_OFFSET + sizeof(skill))) return FALSE;
    skill = *(uint8_t **)(character_bytes + CHARACTER_SKILL_OFFSET);
    if (!readable_memory(skill, CSKILL_ACTIVE_SLOT_OFFSET + sizeof(int)) ||
        *(void **)(skill + 0x10u) != character) return FALSE;
    state->skill = skill;
    state->active = *(uint8_t *)(skill + CSKILL_ACTIVE_OFFSET) != 0u;
    if (!state->active) return TRUE;
    state->slot = *(int *)(skill + CSKILL_ACTIVE_SLOT_OFFSET);
    if (!SudekiMpDescribeCharacterSkillSlotWithApi(
            character, state->slot, api, &row)) {
        ZeroMemory(state, sizeof(*state));
        state->slot = -1;
        return FALSE;
    }
    state->cost = row.cost;
    return TRUE;
}

BOOL SudekiMpObserveCharacterSkill(
    void *character,
    SudekiMpCharacterSkillState *state
) {
    return native_module != NULL && SudekiMpObserveCharacterSkillWithApi(
        character, &native_api, state);
}

SudekiMpSkillActivationResult SudekiMpActivateCharacterQuickSkill(
    void *character,
    unsigned int ordinal
) {
    if (native_module == NULL) {
        return empty_result(SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT);
    }
    return SudekiMpActivateCharacterQuickSkillWithApi(
        character,
        ordinal,
        &native_api
    );
}

SudekiMpSkillActivationResult SudekiMpActivateCharacterSkillSlot(
    void *character,
    int slot
) {
    if (native_module == NULL) {
        return empty_result(SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT);
    }
    return SudekiMpActivateCharacterSkillSlotWithApi(
        character, slot, &native_api);
}

SudekiMpSkillActivationResult
SudekiMpReplayHostApprovedCharacterSkillSlot(
    void *character,
    int slot
) {
    if (native_module == NULL) {
        return empty_result(SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT);
    }
    return replay_host_approved_character_skill_slot(
        character, slot, &native_api);
}

const char *SudekiMpSkillActivationStatusName(
    SudekiMpSkillActivationStatus status
) {
    switch (status) {
        case SUDEKIMP_SKILL_ACTIVATION_STARTED:
            return "started";
        case SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT:
            return "invalid_context";
        case SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE:
            return "ordinal_unavailable";
        case SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED:
            return "validation_rejected";
        case SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED:
            return "use_rejected";
        default:
            return "unknown";
    }
}
