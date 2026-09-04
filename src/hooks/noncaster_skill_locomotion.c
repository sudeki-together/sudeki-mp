#include "hooks/noncaster_skill_locomotion.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Non-caster skill locomotion requires 32-bit GCC thiscall support"
#endif

typedef unsigned int (__attribute__((thiscall)) *ResourceTypeFunction)(
    void *resource
);
typedef unsigned int (__attribute__((thiscall)) *AnimationCountFunction)(
    void *renderer
);
typedef int (__attribute__((thiscall)) *AnimationLookupFunction)(
    void *renderer,
    int handle
);
typedef void (__attribute__((thiscall)) *AnimationSelectorSetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel,
    int selector
);
typedef int (__attribute__((thiscall)) *AnimationSelectorGetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel
);
typedef void (__attribute__((thiscall)) *AnimationValueSetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel,
    float value
);
typedef float (__attribute__((thiscall)) *AnimationValueGetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel
);
typedef void (__attribute__((thiscall)) *AnimationTimeSetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel,
    float value,
    int mode
);
typedef void (__attribute__((thiscall)) *AnimationStateSetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel,
    int state
);
typedef int (__attribute__((thiscall)) *AnimationStateGetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel
);
typedef void (__attribute__((thiscall)) *AnimationBlendSetFunction)(
    void *renderer,
    int blend,
    float value
);
typedef float (__attribute__((thiscall)) *AnimationBlendGetFunction)(
    void *renderer,
    int blend
);

typedef struct AnimationMethods {
    AnimationCountFunction count;
    AnimationLookupFunction lookup;
    AnimationSelectorSetFunction set_selector;
    AnimationSelectorGetFunction get_selector;
    AnimationValueSetFunction set_rate;
    AnimationValueGetFunction get_rate;
    AnimationTimeSetFunction set_time;
    AnimationValueGetFunction get_time;
    AnimationStateSetFunction set_state;
    AnimationStateGetFunction get_state;
    AnimationBlendSetFunction set_blend;
    AnimationBlendGetFunction get_blend;
} AnimationMethods;

typedef struct LocomotionProfile {
    unsigned int resource_type;
    int idle_selector;
    int moving_primary_selector;
    int moving_secondary_selector;
    float idle_rate;
    float moving_primary_rate;
    float moving_secondary_rate;
} LocomotionProfile;

enum {
    RVA_ANIMATION_RENDERER_VTABLE = 0x002df8ecu,
    RVA_ANIMATION_RENDERER_LOOKUP = 0x0021bac0u,
    RVA_ANIMATION_RENDERER_COUNT = 0x0021bb10u,
    RVA_ANIMATION_RENDERER_SELECTOR_SET = 0x00223000u,
    RVA_ANIMATION_RENDERER_SELECTOR_GET = 0x002230b0u,
    RVA_ANIMATION_RENDERER_RATE_SET = 0x002230d0u,
    RVA_ANIMATION_RENDERER_RATE_GET = 0x00223160u,
    RVA_ANIMATION_RENDERER_TIME_SET = 0x00223180u,
    RVA_ANIMATION_RENDERER_TIME_GET = 0x00223220u,
    RVA_ANIMATION_RENDERER_STATE_SET = 0x00223240u,
    RVA_ANIMATION_RENDERER_STATE_GET = 0x00223290u,
    RVA_ANIMATION_RENDERER_BLEND_SET = 0x002234c0u,
    RVA_ANIMATION_RENDERER_BLEND_GET = 0x002234e0u,
    CHARACTER_RESOURCE_OBJECT_OFFSET = 0x2cu,
    CHARACTER_POSITION_OFFSET = 0x44u,
    CHARACTER_AILISH_COMPONENT_OFFSET = 0x134u,
    POSITION_ATTACHED_WRAPPER_OFFSET = 0xb4u,
    AILISH_ANIMATION_TABLE_OFFSET = 0xdcu,
    AILISH_ANIMATION_BANK_OFFSET = 0x133u,
    AILISH_FIRST_PERSON_WRAPPER_OFFSET = 0x160u,
    AILISH_WORLD_WRAPPER_OFFSET = 0x164u,
    WRAPPER_RENDERER_OFFSET = 0x10u,
    RENDERER_CHANNEL_COUNT_OFFSET = 0xa0u,
    DORMANT_CHANNEL_STATE = 192
};

static const LocomotionProfile locomotion_profiles[] = {
    {
        0x23u,
        17,
        36,
        32,
        12.0f,
        37.17093f,
        30.97577f
    },
    {
        0x01u,
        20,
        22,
        23,
        12.0f,
        41.22882f,
        30.92161f
    }
};

static BOOL memory_has_access(
    const void *pointer,
    size_t size,
    BOOL executable
) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;
    BOOL allowed;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    if (executable) {
        allowed = protection == PAGE_EXECUTE ||
            protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
    } else {
        allowed = protection == PAGE_READONLY ||
            protection == PAGE_READWRITE ||
            protection == PAGE_WRITECOPY ||
            protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
    }
    if (!allowed) return FALSE;
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL readable_memory(const void *pointer, size_t size) {
    return memory_has_access(pointer, size, FALSE);
}

static BOOL executable_memory(const void *pointer) {
    return memory_has_access(pointer, 1u, TRUE);
}

static void set_reason(const char **reason_result, const char *reason) {
    if (reason_result != NULL) *reason_result = reason;
}

void SudekiMpNoncasterSkillLocomotionRelease(
    SudekiMpNoncasterSkillLocomotionLease *lease
) {
    if (lease != NULL) memset(lease, 0, sizeof(*lease));
}

static BOOL actor_resource_type_matches(
    uint8_t *character,
    unsigned int expected_type
) {
    uint8_t *resource;
    void **vtable;
    ResourceTypeFunction get_type;

    if (!readable_memory(
            character, CHARACTER_RESOURCE_OBJECT_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    resource = character + CHARACTER_RESOURCE_OBJECT_OFFSET;
    vtable = *(void ***)resource;
    if (!readable_memory(vtable, 5u * sizeof(void *))) return FALSE;
    get_type = (ResourceTypeFunction)vtable[4];
    return executable_memory((const void *)get_type) &&
        get_type(resource) == expected_type;
}

static BOOL animation_methods(
    uint8_t *base,
    void *renderer,
    AnimationMethods *methods
) {
    void **vtable;

    if (base == NULL || methods == NULL ||
        !readable_memory(renderer, RENDERER_CHANNEL_COUNT_OFFSET + sizeof(int)) ||
        *(void **)renderer != base + RVA_ANIMATION_RENDERER_VTABLE ||
        !readable_memory(
            base + RVA_ANIMATION_RENDERER_VTABLE, 0x14cu)) {
        return FALSE;
    }
    vtable = *(void ***)renderer;
    methods->lookup =
        (AnimationLookupFunction)vtable[0x40u / sizeof(void *)];
    methods->count =
        (AnimationCountFunction)vtable[0xf8u / sizeof(void *)];
    methods->set_selector =
        (AnimationSelectorSetFunction)vtable[0xfcu / sizeof(void *)];
    methods->get_selector =
        (AnimationSelectorGetFunction)vtable[0x100u / sizeof(void *)];
    methods->set_rate =
        (AnimationValueSetFunction)vtable[0x104u / sizeof(void *)];
    methods->get_rate =
        (AnimationValueGetFunction)vtable[0x108u / sizeof(void *)];
    methods->set_time =
        (AnimationTimeSetFunction)vtable[0x10cu / sizeof(void *)];
    methods->get_time =
        (AnimationValueGetFunction)vtable[0x110u / sizeof(void *)];
    methods->set_state =
        (AnimationStateSetFunction)vtable[0x114u / sizeof(void *)];
    methods->get_state =
        (AnimationStateGetFunction)vtable[0x118u / sizeof(void *)];
    methods->set_blend =
        (AnimationBlendSetFunction)vtable[0x144u / sizeof(void *)];
    methods->get_blend =
        (AnimationBlendGetFunction)vtable[0x148u / sizeof(void *)];
    return (void *)methods->lookup == base + RVA_ANIMATION_RENDERER_LOOKUP &&
        (void *)methods->count == base + RVA_ANIMATION_RENDERER_COUNT &&
        (void *)methods->set_selector ==
            base + RVA_ANIMATION_RENDERER_SELECTOR_SET &&
        (void *)methods->get_selector ==
            base + RVA_ANIMATION_RENDERER_SELECTOR_GET &&
        (void *)methods->set_rate == base + RVA_ANIMATION_RENDERER_RATE_SET &&
        (void *)methods->get_rate == base + RVA_ANIMATION_RENDERER_RATE_GET &&
        (void *)methods->set_time == base + RVA_ANIMATION_RENDERER_TIME_SET &&
        (void *)methods->get_time == base + RVA_ANIMATION_RENDERER_TIME_GET &&
        (void *)methods->set_state == base + RVA_ANIMATION_RENDERER_STATE_SET &&
        (void *)methods->get_state == base + RVA_ANIMATION_RENDERER_STATE_GET &&
        (void *)methods->set_blend == base + RVA_ANIMATION_RENDERER_BLEND_SET &&
        (void *)methods->get_blend == base + RVA_ANIMATION_RENDERER_BLEND_GET;
}

/*
 * The retail ranged component normally retains its detached world wrapper at
 * +0x164.  The LAN cleanroom's native Player-2 construction is a narrower
 * topology: +0x164 is exactly NULL while CPosition+0xB4 owns the live world
 * wrapper and +0x160 still retains the distinct first-person wrapper.
 *
 * Preserve the retail path whenever +0x164 is non-NULL.  The fallback is
 * intentionally admitted only for that exact NULL shape and only when the
 * attached and first-person wrappers are both readable and distinct.  The
 * caller still proves the selected renderer vtable, method table, submodel
 * count, and all three authored world selectors before performing a write.
 */
static BOOL select_ailish_world_wrapper(
    void *attached_wrapper,
    BOOL attached_wrapper_readable,
    void *first_person_wrapper,
    BOOL first_person_wrapper_readable,
    void *saved_world_wrapper,
    BOOL saved_world_wrapper_readable,
    void **wrapper_result
) {
    if (wrapper_result == NULL) return FALSE;
    *wrapper_result = NULL;
    if (saved_world_wrapper != NULL) {
        if (!saved_world_wrapper_readable) return FALSE;
        *wrapper_result = saved_world_wrapper;
        return TRUE;
    }
    if (attached_wrapper == NULL || !attached_wrapper_readable ||
        first_person_wrapper == NULL || !first_person_wrapper_readable ||
        attached_wrapper == first_person_wrapper) {
        return FALSE;
    }
    *wrapper_result = attached_wrapper;
    return TRUE;
}

static BOOL resolve_actor_renderer(
    uint8_t *character,
    SudekiMpNoncasterSkillLocomotionActor actor,
    void **component_result,
    void **wrapper_result,
    void **renderer_result,
    const char **failure_result
) {
    uint8_t *position;
    uint8_t *component = NULL;
    uint8_t *wrapper = NULL;

    set_reason(failure_result, "character_position_slot_unavailable");
    if (component_result == NULL || wrapper_result == NULL ||
        renderer_result == NULL || !readable_memory(
            character, CHARACTER_POSITION_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    *component_result = NULL;
    *wrapper_result = NULL;
    *renderer_result = NULL;
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    if (actor == SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_AILISH) {
        uint8_t *attached_wrapper;
        uint8_t *first_person_wrapper;
        uint8_t *saved_world_wrapper;
        void *selected_wrapper;

        set_reason(failure_result, "ailish_component_slot_unavailable");
        if (!readable_memory(
                character,
                CHARACTER_AILISH_COMPONENT_OFFSET + sizeof(void *))) {
            return FALSE;
        }
        component = *(uint8_t **)(
            character + CHARACTER_AILISH_COMPONENT_OFFSET);
        set_reason(failure_result, "ailish_component_lease_mismatch");
        if (!readable_memory(component, 0x168u) ||
            *(void **)(component + 0x10u) != character) return FALSE;
        attached_wrapper = readable_memory(
                position,
                POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *)) ?
            *(uint8_t **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) : NULL;
        first_person_wrapper = *(uint8_t **)(
            component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
        saved_world_wrapper = *(uint8_t **)(
            component + AILISH_WORLD_WRAPPER_OFFSET);
        set_reason(failure_result, "ailish_world_wrapper_topology_mismatch");
        if (!select_ailish_world_wrapper(
                attached_wrapper,
                readable_memory(
                    attached_wrapper, WRAPPER_RENDERER_OFFSET + sizeof(void *)),
                first_person_wrapper,
                readable_memory(
                    first_person_wrapper,
                    WRAPPER_RENDERER_OFFSET + sizeof(void *)),
                saved_world_wrapper,
                readable_memory(
                    saved_world_wrapper,
                    WRAPPER_RENDERER_OFFSET + sizeof(void *)),
                &selected_wrapper)) {
            return FALSE;
        }
        wrapper = (uint8_t *)selected_wrapper;
    } else {
        set_reason(failure_result, "position_attached_wrapper_unavailable");
        wrapper = readable_memory(
                position,
                POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *)) ?
            *(uint8_t **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) : NULL;
    }
    set_reason(failure_result, "wrapper_lease_unavailable");
    if (!readable_memory(
            wrapper, WRAPPER_RENDERER_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    *component_result = component;
    *wrapper_result = wrapper;
    *renderer_result = *(void **)(wrapper + WRAPPER_RENDERER_OFFSET);
    set_reason(failure_result, "renderer_pointer_unavailable");
    return *renderer_result != NULL;
}

static BOOL ailish_selector_resolves(
    void *component_pointer,
    void *renderer,
    const AnimationMethods *methods,
    unsigned int animation_id,
    int expected_selector
) {
    uint8_t *component = (uint8_t *)component_pointer;
    uint8_t *animation_table;
    uint8_t *details;
    uint32_t handle;
    uint32_t alternate_handle;
    BOOL first_person_bank_active;

    if (methods == NULL || methods->lookup == NULL ||
        !readable_memory(component, 0x168u)) {
        return FALSE;
    }
    animation_table = *(uint8_t **)(component + AILISH_ANIMATION_TABLE_OFFSET);
    if (!readable_memory(
            animation_table,
            0x14u + (animation_id + 1u) * sizeof(void *))) {
        return FALSE;
    }
    details = *(uint8_t **)(
        animation_table + 0x14u + animation_id * sizeof(void *));
    if (!readable_memory(details, 0x28u)) return FALSE;
    first_person_bank_active =
        (component[AILISH_ANIMATION_BANK_OFFSET] & 2u) != 0u;
    memcpy(&handle, details + (first_person_bank_active ? 0x20u : 0x14u),
        sizeof(handle));
    if (handle != 0u && handle != 0x0007ffffu &&
        methods->lookup(renderer, (int)(int32_t)handle) == expected_selector) {
        return TRUE;
    }
    memcpy(&alternate_handle,
        details + (first_person_bank_active ? 0x14u : 0x20u),
        sizeof(alternate_handle));
    return alternate_handle != 0u &&
        alternate_handle != 0x0007ffffu && alternate_handle != handle &&
        methods->lookup(renderer, (int)(int32_t)alternate_handle) ==
            expected_selector;
}

static BOOL presentation_matches(
    void *renderer,
    const AnimationMethods *methods,
    unsigned int submodels,
    const LocomotionProfile *profile,
    BOOL moving
) {
    int selector_zero = moving ? profile->moving_primary_selector :
        profile->idle_selector;
    int selector_one = moving ? profile->moving_secondary_selector : 0;
    float rate_zero = moving ? profile->moving_primary_rate :
        profile->idle_rate;
    float rate_one = moving ? profile->moving_secondary_rate : 0.0f;
    float blend;
    unsigned int submodel;

    for (submodel = 0u; submodel < submodels; ++submodel) {
        float time_zero = methods->get_time(renderer, 0, submodel);
        float time_one = methods->get_time(renderer, 1, submodel);
        float actual_rate_zero = methods->get_rate(renderer, 0, submodel);
        float actual_rate_one = methods->get_rate(renderer, 1, submodel);
        if (methods->get_selector(renderer, 0, submodel) != selector_zero ||
            methods->get_selector(renderer, 1, submodel) != selector_one ||
            !isfinite(time_zero) || !isfinite(time_one) ||
            !isfinite(actual_rate_zero) || !isfinite(actual_rate_one) ||
            fabsf(actual_rate_zero - rate_zero) > 0.001f ||
            fabsf(actual_rate_one - rate_one) > 0.001f) {
            return FALSE;
        }
    }
    blend = methods->get_blend(renderer, 0);
    return isfinite(blend) &&
        fabsf(blend - (moving ? 0.99f : 0.0f)) <= 0.001f;
}

static BOOL transition_states_match(
    void *renderer,
    const AnimationMethods *methods,
    unsigned int submodels,
    BOOL moving
) {
    unsigned int submodel;
    int expected_secondary = moving ? 0 : DORMANT_CHANNEL_STATE;

    for (submodel = 0u; submodel < submodels; ++submodel) {
        if (methods->get_state(renderer, 0, submodel) != 0 ||
            methods->get_state(renderer, 1, submodel) != expected_secondary) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL step_channel_phase_clock(
    SudekiMpNoncasterSkillLocomotionLease *lease,
    unsigned int channel,
    BOOL reset,
    DWORD now_ms,
    float rate
) {
    DWORD elapsed_ms;
    double phase;

    if (lease == NULL || channel >= 2u || !isfinite(rate) || rate < 0.0f) {
        return FALSE;
    }
    if (reset) {
        lease->channel_phase_tick_ms[channel] = now_ms;
        lease->channel_phase_time[channel] = 0.0;
        lease->channel_phase_valid[channel] = TRUE;
        return TRUE;
    }
    if (!lease->channel_phase_valid[channel] ||
        !isfinite(lease->channel_phase_time[channel]) ||
        lease->channel_phase_time[channel] < 0.0) {
        return FALSE;
    }
    /* Unsigned DWORD subtraction is defined modulo 2^32, matching the
     * documented GetTickCount wrap interval. */
    elapsed_ms = (DWORD)(
        now_ms - lease->channel_phase_tick_ms[channel]);
    phase = lease->channel_phase_time[channel] +
        (double)rate * ((double)elapsed_ms / 1000.0);
    if (!isfinite(phase) || phase < 0.0 || phase > (double)FLT_MAX) {
        return FALSE;
    }
    lease->channel_phase_tick_ms[channel] = now_ms;
    lease->channel_phase_time[channel] = phase;
    return TRUE;
}

static BOOL step_phase_clocks(
    SudekiMpNoncasterSkillLocomotionLease *lease,
    BOOL reset,
    DWORD now_ms,
    float channel_zero_rate,
    float channel_one_rate
) {
    return step_channel_phase_clock(
            lease, 0u, reset, now_ms, channel_zero_rate) &&
        step_channel_phase_clock(
            lease, 1u, reset, now_ms, channel_one_rate);
}

static BOOL channel_phase_value(
    const SudekiMpNoncasterSkillLocomotionLease *lease,
    unsigned int channel,
    float *phase_result
) {
    double phase;

    if (lease == NULL || phase_result == NULL || channel >= 2u ||
        !lease->channel_phase_valid[channel]) {
        return FALSE;
    }
    phase = lease->channel_phase_time[channel];
    if (!isfinite(phase) || phase < 0.0 || phase > (double)FLT_MAX) {
        return FALSE;
    }
    *phase_result = (float)phase;
    return isfinite(*phase_result) && *phase_result >= 0.0f;
}

#ifdef SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TESTING
BOOL SudekiMpNoncasterSkillLocomotionTestSelectAilishWorldWrapper(
    void *attached_wrapper,
    BOOL attached_wrapper_readable,
    void *first_person_wrapper,
    BOOL first_person_wrapper_readable,
    void *saved_world_wrapper,
    BOOL saved_world_wrapper_readable,
    void **wrapper_result
) {
    return select_ailish_world_wrapper(
        attached_wrapper, attached_wrapper_readable,
        first_person_wrapper, first_person_wrapper_readable,
        saved_world_wrapper, saved_world_wrapper_readable,
        wrapper_result);
}

BOOL SudekiMpNoncasterSkillLocomotionTestStepPhaseClock(
    SudekiMpNoncasterSkillLocomotionLease *lease,
    BOOL reset,
    DWORD now_ms,
    float channel_zero_rate,
    float channel_one_rate,
    float phase_result[2]
) {
    if (phase_result == NULL || !step_phase_clocks(
            lease, reset, now_ms,
            channel_zero_rate, channel_one_rate) ||
        !channel_phase_value(lease, 0u, &phase_result[0]) ||
        !channel_phase_value(lease, 1u, &phase_result[1])) {
        return FALSE;
    }
    return TRUE;
}
#endif

static void set_channel(
    void *renderer,
    const AnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    int state,
    float rate,
    float phase
) {
    unsigned int submodel;
    for (submodel = 0u; submodel < submodels; ++submodel) {
        methods->set_selector(renderer, channel, submodel, selector);
        methods->set_state(renderer, channel, submodel, state);
        methods->set_time(renderer, channel, submodel, phase, 0);
        methods->set_rate(renderer, channel, submodel, rate);
    }
}

static BOOL channel_phases_match(
    void *renderer,
    const AnimationMethods *methods,
    unsigned int submodels,
    const float phases[2]
) {
    unsigned int channel;
    unsigned int submodel;

    if (phases == NULL) return FALSE;
    for (channel = 0u; channel < 2u; ++channel) {
        if (!isfinite(phases[channel]) || phases[channel] < 0.0f) {
            return FALSE;
        }
        for (submodel = 0u; submodel < submodels; ++submodel) {
            float actual = methods->get_time(
                renderer, (int)channel, submodel);
            if (!isfinite(actual) ||
                fabsf(actual - phases[channel]) > 0.001f) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static BOOL stable_presentation_predicate(
    BOOL presentation_matches_value,
    BOOL transition_states_match_value,
    BOOL channel_phases_match_value
) {
    return presentation_matches_value &&
        transition_states_match_value &&
        channel_phases_match_value;
}

#ifdef SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TESTING
static float __attribute__((thiscall)) test_current_channel_phase(
    void *renderer,
    int channel,
    unsigned int submodel
) {
    const float *current_phases = (const float *)renderer;

    (void)submodel;
    return current_phases[channel];
}

BOOL SudekiMpNoncasterSkillLocomotionTestStablePredicate(
    BOOL presentation_matches_value,
    BOOL transition_states_match_value,
    const float expected_phases[2],
    const float current_phases[2]
) {
    AnimationMethods methods;

    if (expected_phases == NULL || current_phases == NULL) return FALSE;
    memset(&methods, 0, sizeof(methods));
    methods.get_time = test_current_channel_phase;
    return stable_presentation_predicate(
        presentation_matches_value,
        transition_states_match_value,
        channel_phases_match(
            (void *)current_phases, &methods, 1u, expected_phases));
}
#endif

SudekiMpNoncasterSkillLocomotionResult
SudekiMpNoncasterSkillLocomotionService(
    HMODULE game_module,
    void *character_pointer,
    SudekiMpNoncasterSkillLocomotionActor actor,
    BOOL ownership_active,
    BOOL moving,
    SudekiMpNoncasterSkillLocomotionLease *lease,
    const char **reason_result
) {
    uint8_t *base = (uint8_t *)game_module;
    uint8_t *character = (uint8_t *)character_pointer;
    const LocomotionProfile *profile;
    AnimationMethods methods;
    void *component;
    void *wrapper;
    void *renderer;
    unsigned int submodels;
    int channel_count;
    BOOL ownership_changed;
    BOOL profile_changed;
    BOOL reset_phase;
    BOOL moving_value;
    DWORD now_ms;
    float rates[2];
    float phases[2];
    const char *renderer_failure = "renderer_lease_unavailable";

    set_reason(reason_result, "invalid_parameter");
    if (lease == NULL || actor < SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TAL ||
        actor > SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_AILISH) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    if (!ownership_active) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "ownership_inactive");
        SetLastError(ERROR_SUCCESS);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_INACTIVE;
    }
    if (base == NULL || character == NULL) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        SetLastError(ERROR_INVALID_PARAMETER);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    profile = &locomotion_profiles[(unsigned int)actor];
    if (!actor_resource_type_matches(character, profile->resource_type)) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "actor_identity_mismatch");
        SetLastError(ERROR_INVALID_DATA);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    if (!resolve_actor_renderer(
            character, actor, &component, &wrapper, &renderer,
            &renderer_failure)) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, renderer_failure);
        SetLastError(ERROR_INVALID_DATA);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    memset(&methods, 0, sizeof(methods));
    if (!animation_methods(base, renderer, &methods)) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "animation_method_identity_mismatch");
        SetLastError(ERROR_INVALID_DATA);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    channel_count = *(int *)((uint8_t *)renderer +
        RENDERER_CHANNEL_COUNT_OFFSET);
    submodels = methods.count(renderer);
    if (channel_count < 2 || channel_count > 32 ||
        submodels == 0u || submodels > 32u) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "renderer_count_gate_failed");
        SetLastError(ERROR_INVALID_DATA);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    if (actor == SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_AILISH &&
        (!ailish_selector_resolves(
             component, renderer, &methods, 0x02u,
             profile->idle_selector) ||
         !ailish_selector_resolves(
             component, renderer, &methods, 0x06u,
             profile->moving_primary_selector) ||
         !ailish_selector_resolves(
             component, renderer, &methods, 0x07u,
             profile->moving_secondary_selector))) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "ailish_world_selector_mismatch");
        SetLastError(ERROR_INVALID_DATA);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    moving_value = moving != FALSE;
    rates[0] = moving_value ? profile->moving_primary_rate :
        profile->idle_rate;
    rates[1] = moving_value ? profile->moving_secondary_rate : 0.0f;
    ownership_changed = !lease->valid || lease->character != character ||
        lease->component != component || lease->wrapper != wrapper ||
        lease->renderer != renderer || lease->submodel_count != submodels ||
        lease->actor != actor;
    profile_changed = !ownership_changed &&
        lease->moving != moving_value;
    reset_phase = ownership_changed || profile_changed;
    now_ms = GetTickCount();
    if (!step_phase_clocks(
            lease, reset_phase, now_ms, rates[0], rates[1]) ||
        !channel_phase_value(lease, 0u, &phases[0]) ||
        !channel_phase_value(lease, 1u, &phases[1])) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "phase_clock_invalid");
        SetLastError(ERROR_INVALID_DATA);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    if (!ownership_changed && !profile_changed &&
        stable_presentation_predicate(
            presentation_matches(
                renderer, &methods, submodels, profile, moving_value),
            transition_states_match(
                renderer, &methods, submodels, moving_value),
            channel_phases_match(
                renderer, &methods, submodels, phases))) {
        set_reason(reason_result, "stable");
        SetLastError(ERROR_SUCCESS);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_STABLE;
    }
    set_channel(
        renderer, &methods, submodels, 0,
        moving ? profile->moving_primary_selector : profile->idle_selector,
        0,
        rates[0], phases[0]);
    set_channel(
        renderer, &methods, submodels, 1,
        moving ? profile->moving_secondary_selector : 0,
        moving ? 0 : DORMANT_CHANNEL_STATE,
        rates[1], phases[1]);
    methods.set_blend(renderer, 0, moving ? 0.99f : 0.0f);
    if (!presentation_matches(
            renderer, &methods, submodels, profile, moving_value) ||
        !transition_states_match(
            renderer, &methods, submodels, moving_value) ||
        !channel_phases_match(renderer, &methods, submodels, phases)) {
        SudekiMpNoncasterSkillLocomotionRelease(lease);
        set_reason(reason_result, "setter_verification_failed");
        SetLastError(ERROR_WRITE_FAULT);
        return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_REJECTED;
    }
    lease->character = character;
    lease->component = component;
    lease->wrapper = wrapper;
    lease->renderer = renderer;
    lease->submodel_count = submodels;
    lease->actor = actor;
    lease->moving = moving_value;
    lease->valid = TRUE;
    set_reason(reason_result, ownership_changed ? "ownership_changed" :
        "semantic_transition_or_renderer_drift");
    SetLastError(ERROR_SUCCESS);
    return SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_APPLIED;
}
