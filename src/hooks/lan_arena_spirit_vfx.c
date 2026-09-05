#include "hooks/lan_arena_spirit_vfx.h"
#include "engine/log.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    RVA_SFX_PLAY = 0x00018de0u,
    RVA_SFX_PRE_CACHE = 0x00019540u,
    RVA_SFX_UN_CACHE = 0x00019650u,
    RVA_SFX_GET_MANAGER = 0x00019770u,
    RVA_SFX_MANAGER_VTABLE = 0x002c62a0u,
    RVA_SFX_MANAGER_SECONDARY_VTABLE = 0x002c62a8u,
    RVA_SFX_MANAGER_GLOBAL = 0x00408d48u,
    SFX_CACHE_FIRST_PENDING_OFFSET = 0x410u,
    SFX_CACHE_FIRST_LOADED_OFFSET = 0x414u,
    SFX_CACHE_FIRST_IDENTIFIER_OFFSET = 0x41cu,
    SFX_CACHE_FIRST_REF_COUNT_OFFSET = 0x424u,
    SFX_CACHE_ENTRY_STRIDE = 0x18u,
    SFX_CACHE_ENTRY_COUNT = 0x40u,
    SFX_MANAGER_REQUIRED_SIZE = 0x0a10u,
    TAL_POSITION_OFFSET = 0x44u,
    TAL_EFFECT_PARENT_OFFSET = 0x58u,
    TAL_ANIMATION_SOURCE_OFFSET = 0x5cu,
    POSITION_PARENT_OFFSET = 0x8cu,
    POSITION_MODE_OFFSET = 0x94u,
    POSITION_RENDER_WRAPPER_OFFSET = 0xb4u,
    POSITION_DIRECT_MATRIX_OFFSET = 0xc0u,
    POSITION_PARENT_MATRIX_OFFSET = 0xa0u,
    POSITION_REQUIRED_SIZE = 0x104u,
    POSITION_PARENT_REQUIRED_SIZE = 0x0f6u,
    RENDER_WRAPPER_OBJECT_OFFSET = 0x08u,
    RENDER_OBJECT_CALLBACK_OFFSET = 0x14u,
    RENDER_OBJECT_REQUIRED_SIZE = 0x38u,
    CALLBACK_COUNT_VTABLE_OFFSET = 0x18u,
    ANIMATION_SOURCE_FLAGS_OFFSET = 0x60u,
    ANIMATION_SOURCE_TEXT_OFFSET = 0x64u,
    ANIMATION_SOURCE_REQUIRED_SIZE = 0x68u,
    MATRIX_FLOAT_COUNT = 16u,
    TAL_INITIATE_HOM_IDENTIFIER = 0x3cef3b8fu,
    RESOURCE_IDENTIFIER_INVALID = 0x0007ffffu,
    MAX_NATIVE_TEXT_SCAN = 4096u
};

/* PreCache consumes the identifier directly, unlike the typed resource path
 * which appends .hom. Use the backing filename for cache, replay, and release
 * so all three operations address the same authored opening effect. */
static const char initiate_resource[] = "SFXSS250_Initiate.HOM";

typedef struct SpiritVisualResource {
    const char *name;
    uint32_t identifier;
} SpiritVisualResource;

static const SpiritVisualResource visual_resources[SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST] = {
    {"SFXSS250_Initiate.HOM", 0x3cef3b8fu},
    {"SFXSS251_Initiate_Loop_Wait.HOM", 0xb5a0cf01u},
    {"SFXSS112_Small_Floor_Pattern.HOM", 0x03439ed3u},
    {"SFXSS800_Spirit_A2T.HOM", 0xb5661565u},
    {"SFXSS252_Morph_into_Spirit.HOM", 0x903afa53u},
    {"SFXSS801_Spirit_Link.HOM", 0x2e5a867bu},
    {"SFXSS802_Spirit_End.HOM", 0x4d727a05u},
    {"SFXSS300_Tal_Spirit_Strike.HOM", 0x449d201bu},
    {"SFXSS350_Tal_Spirit_Strike.HOM", 0xf007401bu},
    {"SFXSS110_Loop_Invulnerable.HOM", 0xc24c6a03u},
    {"SFXSS111_End_Invulnerable.HOM", 0xa8171ecfu},
    {"SFXSS900_generic_initate.HOM", 0x62dcc5a3u},
    {"SFXSS351_Tal_Hit_Character.HOM", 0xaeec0c83u}
};

static const uint8_t expected_sfx_play_body[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x83u, 0x38u, 0x00u, 0x74u,
    0x39u, 0xd9u, 0x44u, 0x24u, 0x1cu, 0x8bu, 0x54u, 0x24u,
    0x10u, 0x6au, 0x00u, 0x83u, 0xecu, 0x10u, 0xd9u, 0x5cu,
    0x24u, 0x0cu, 0xd9u, 0x44u, 0x24u, 0x2cu, 0xd9u, 0x5cu,
    0x24u, 0x08u, 0xd9u, 0x44u, 0x24u, 0x28u, 0xd9u, 0x5cu,
    0x24u, 0x04u, 0xd9u, 0xe8u, 0xd9u, 0x1cu, 0x24u, 0x6au,
    0x00u, 0x52u, 0x8bu, 0x54u, 0x24u, 0x28u, 0x52u, 0x8bu,
    0x54u, 0x24u, 0x28u, 0x52u, 0x50u, 0xe8u, 0x4eu, 0x00u,
    0x00u, 0x00u, 0xc2u, 0x1cu, 0x00u
};

static const uint8_t expected_pre_cache_prefix[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x0cu, 0x8bu, 0x45u, 0x08u, 0x81u, 0x78u, 0x04u, 0xffu,
    0xffu, 0x07u, 0x00u, 0x53u, 0x56u, 0x57u, 0x89u, 0x4cu,
    0x24u, 0x10u, 0x0fu, 0x84u, 0xc5u, 0x00u, 0x00u, 0x00u
};

static const uint8_t expected_pre_cache_new_tail[] = {
    0x24u, 0x10u, 0x89u, 0x4cu, 0x24u, 0x14u, 0xe8u, 0xf5u,
    0x1du, 0x1cu, 0x00u, 0x33u, 0xffu, 0x6au, 0x00u, 0x56u,
    0x8du, 0x57u, 0x04u, 0x8du, 0x44u, 0x24u, 0x18u, 0xe8u,
    0xb4u, 0x60u, 0x1cu, 0x00u, 0x83u, 0xc4u, 0x08u, 0xffu,
    0x83u, 0x24u, 0x04u, 0x00u, 0x00u, 0x32u, 0xc0u, 0x5fu,
    0x5eu, 0x5bu, 0x8bu, 0xe5u, 0x5du, 0xc2u, 0x04u, 0x00u
};

static const uint8_t expected_pre_cache_existing_tail[] = {
    0x8bu, 0x44u, 0x24u, 0x10u, 0x8du, 0x14u, 0x5bu, 0xffu,
    0x84u, 0xd0u, 0x24u, 0x04u, 0x00u, 0x00u, 0x5fu, 0x8du,
    0x84u, 0xd0u, 0x24u, 0x04u, 0x00u, 0x00u, 0x5eu, 0xb0u,
    0x01u, 0x5bu, 0x8bu, 0xe5u, 0x5du, 0xc2u, 0x04u, 0x00u
};

static const uint8_t expected_un_cache_prefix[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u, 0x8bu, 0x70u, 0x04u,
    0x81u, 0xfeu, 0xffu, 0xffu, 0x07u, 0x00u, 0x74u, 0x15u,
    0x33u, 0xc0u, 0x8du, 0x91u, 0x1cu, 0x04u, 0x00u, 0x00u,
    0x39u, 0x32u, 0x74u, 0x0fu, 0x40u, 0x83u, 0xc2u, 0x18u,
    0x83u, 0xf8u, 0x40u, 0x7cu, 0xf3u, 0x32u, 0xc0u, 0x5eu,
    0xc2u, 0x04u, 0x00u, 0x8du, 0x14u, 0x40u, 0xffu, 0x8cu,
    0xd1u, 0x24u, 0x04u, 0x00u, 0x00u
};

static const uint8_t expected_un_cache_suffix[] = {
    0xc7u, 0x87u, 0x24u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x5fu, 0xb0u, 0x01u, 0x5eu, 0xc2u, 0x04u,
    0x00u
};

static volatile LONG operation_active;
static SudekiMpLanArenaSpiritVfxCacheLease production_lease;
static SudekiMpLanArenaSpiritVfxVisualState production_visuals;
static HMODULE visual_module_pin;
static BOOL visual_quarantined;
static DWORD visual_prewarm_poll_at;
static unsigned int visual_prewarm_index;

_Static_assert(sizeof(SudekiMpResourceName) == 12u,
    "supported ResourceName ABI must remain 12 bytes");
_Static_assert(sizeof(SudekiMpLanArenaSpiritVfxTransientTPtr) == 12u,
    "supported TPtr ABI must remain 12 bytes");
_Static_assert(sizeof(expected_sfx_play_body) == 0x45u,
    "supported CSFXManager::PlaySfx body length changed");

static BOOL readable_region(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;

    if (pointer == NULL || length == 0u || address > UINTPTR_MAX - length) {
        return FALSE;
    }
    end = address + length;
    if (VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & PAGE_GUARD) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return FALSE;
    }
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return region_end >= (uintptr_t)information.BaseAddress && end <= region_end;
}

static BOOL writable_region(void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;

    if (pointer == NULL || length == 0u || address > UINTPTR_MAX - length) {
        return FALSE;
    }
    end = address + length;
    if (VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & PAGE_GUARD) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return FALSE;
    }
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return region_end >= (uintptr_t)information.BaseAddress && end <= region_end;
}

static BOOL executable_region(const void *pointer) {
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;

    if (pointer == NULL ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & PAGE_GUARD) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL readable_c_string(const char *text) {
    size_t index;

    if (text == NULL) return TRUE;
    for (index = 0u; index < MAX_NATIVE_TEXT_SCAN; ++index) {
        if (!readable_region(text + index, 1u)) return FALSE;
        if (text[index] == '\0') return TRUE;
    }
    return FALSE;
}

static BOOL finite_matrix(const float *matrix) {
    unsigned int index;

    if (!readable_region(matrix, MATRIX_FLOAT_COUNT * sizeof(*matrix))) {
        return FALSE;
    }
    for (index = 0u; index < MATRIX_FLOAT_COUNT; ++index) {
        if (!isfinite(matrix[index])) return FALSE;
    }
    return TRUE;
}

static BOOL native_tal_ready(void *context, void *tal) {
    uint8_t *actor = (uint8_t *)tal;
    uint8_t *position;
    uint8_t *matrix_owner;
    const float *matrix;
    uint8_t *effect_parent;
    uint8_t *observer;
    uint8_t *animation_source;
    const char *animation_text;
    uint8_t *render_wrapper;
    uint8_t *render_object;
    uint8_t *callback;
    uint8_t *callback_vtable;
    void *count_method;
    int position_mode;

    (void)context;
    if (!readable_region(actor, TAL_ANIMATION_SOURCE_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    position = *(uint8_t **)(actor + TAL_POSITION_OFFSET);
    if (!readable_region(position, POSITION_REQUIRED_SIZE) ||
        !writable_region(position, POSITION_REQUIRED_SIZE)) {
        return FALSE;
    }
    position_mode = *(int *)(position + POSITION_MODE_OFFSET);
    if (position_mode == 0 || position_mode == 4) {
        matrix = (const float *)(position + POSITION_DIRECT_MATRIX_OFFSET);
    } else {
        matrix_owner = *(uint8_t **)(position + POSITION_PARENT_OFFSET);
        /* 0x510f90 mutates both the child and this parent before copying
         * parent+0xa0 into SfxSetupMatrix. */
        if (!readable_region(matrix_owner, POSITION_PARENT_REQUIRED_SIZE) ||
            !writable_region(matrix_owner, POSITION_PARENT_REQUIRED_SIZE)) {
            return FALSE;
        }
        matrix = (const float *)(matrix_owner + POSITION_PARENT_MATRIX_OFFSET);
    }
    if (!finite_matrix(matrix)) return FALSE;

    /* 0x418c90 has no null guards after position+0xb4: selector zero must be
     * below the count returned through callback vtable slot +0x18. */
    render_wrapper = *(uint8_t **)(position + POSITION_RENDER_WRAPPER_OFFSET);
    if (!readable_region(render_wrapper, RENDER_WRAPPER_OBJECT_OFFSET +
            sizeof(void *))) {
        return FALSE;
    }
    render_object = *(uint8_t **)(render_wrapper + RENDER_WRAPPER_OBJECT_OFFSET);
    if (!readable_region(render_object, RENDER_OBJECT_REQUIRED_SIZE)) {
        return FALSE;
    }
    callback = *(uint8_t **)(render_object + RENDER_OBJECT_CALLBACK_OFFSET);
    if (!readable_region(callback, sizeof(void *))) return FALSE;
    callback_vtable = *(uint8_t **)callback;
    if (!readable_region(callback_vtable,
            CALLBACK_COUNT_VTABLE_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    count_method = *(void **)(callback_vtable + CALLBACK_COUNT_VTABLE_OFFSET);
    if (!executable_region(count_method)) return FALSE;

    /* The common tail weakly observes this actor-owned object. Its stack TPtr
     * is linked and unlinked before PlaySfx returns. */
    effect_parent = *(uint8_t **)(actor + TAL_EFFECT_PARENT_OFFSET);
    if (effect_parent != NULL) {
        if (!readable_region(effect_parent, 3u * sizeof(void *)) ||
            !writable_region(effect_parent, 3u * sizeof(void *))) {
            return FALSE;
        }
        observer = *(uint8_t **)(effect_parent + 2u * sizeof(void *));
        if (observer != NULL &&
            (!readable_region(observer, 2u * sizeof(void *)) ||
             !writable_region(observer, 2u * sizeof(void *)))) {
            return FALSE;
        }
    }

    /* 0x419229 selects either inline text at +0x64 or the pointer stored
     * there, then 0x4fc3b0 scans it through its terminating NUL. */
    animation_source = *(uint8_t **)(actor + TAL_ANIMATION_SOURCE_OFFSET);
    if (animation_source != NULL) {
        if (!readable_region(animation_source,
                ANIMATION_SOURCE_REQUIRED_SIZE)) {
            return FALSE;
        }
        if ((*(uint32_t *)(animation_source + ANIMATION_SOURCE_FLAGS_OFFSET) &
                0x80000000u) != 0u) {
            animation_text = (const char *)(animation_source +
                ANIMATION_SOURCE_TEXT_OFFSET);
        } else {
            animation_text = *(const char **)(animation_source +
                ANIMATION_SOURCE_TEXT_OFFSET);
        }
        if (!readable_c_string(animation_text)) return FALSE;
    }
    return TRUE;
}

static void *native_resolve_tal(void *context) {
    (void)context;
    return SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_TAL);
}

static void *native_get_sfx_manager(void *context) {
    uint8_t *base = (uint8_t *)context;
    void *entry;
    void *manager;

    if (base == NULL) return NULL;
    entry = base + RVA_SFX_GET_MANAGER;
    __asm__ volatile(
        "call *%[entry]"
        : "=a"(manager)
        : [entry] "r"(entry)
        : "ecx", "edx", "memory", "cc"
    );
    return manager;
}

static BOOL native_manager_matches(void *context, void *manager) {
    uint8_t *base = (uint8_t *)context;
    void *primary_vtable;
    void *secondary_vtable;

    if (base == NULL ||
        !readable_region(manager, SFX_MANAGER_REQUIRED_SIZE) ||
        !writable_region(manager, SFX_MANAGER_REQUIRED_SIZE)) {
        return FALSE;
    }
    primary_vtable = *(void **)manager;
    secondary_vtable = *(void **)((uint8_t *)manager + 8u);
    return primary_vtable == (void *)(base + RVA_SFX_MANAGER_VTABLE) &&
        secondary_vtable ==
            (void *)(base + RVA_SFX_MANAGER_SECONDARY_VTABLE);
}

static BOOL native_resource_name_from_text(
    void *context,
    SudekiMpResourceName *resource_name,
    const char *text
) {
    (void)context;
    return SudekiMpCleanroomEngineResourceNameFromText(resource_name, text);
}

static void native_release_resource_name(
    void *context,
    SudekiMpResourceName *resource_name
) {
    (void)context;
    SudekiMpCleanroomEngineReleaseResourceName(resource_name);
}

static BOOL native_cache_snapshot(
    void *context,
    void *manager,
    uint32_t resource_identifier,
    SudekiMpLanArenaSpiritVfxCacheSnapshot *snapshot
) {
    uint8_t *bytes = (uint8_t *)manager;
    unsigned int index;

    if (snapshot == NULL || resource_identifier == RESOURCE_IDENTIFIER_INVALID ||
        !native_manager_matches(context, manager)) {
        return FALSE;
    }
    ZeroMemory(snapshot, sizeof(*snapshot));
    for (index = 0u; index < SFX_CACHE_ENTRY_COUNT; ++index) {
        size_t delta = (size_t)index * SFX_CACHE_ENTRY_STRIDE;
        uint32_t identifier = *(uint32_t *)(bytes +
            SFX_CACHE_FIRST_IDENTIFIER_OFFSET + delta);

        if (identifier == resource_identifier) {
            ++snapshot->matching_slots;
            if (snapshot->matching_slots == 1u) {
                snapshot->slot_index = index;
                snapshot->ref_count = *(LONG *)(bytes +
                    SFX_CACHE_FIRST_REF_COUNT_OFFSET + delta);
                snapshot->pending = *(void **)(bytes +
                    SFX_CACHE_FIRST_PENDING_OFFSET + delta) != NULL;
                snapshot->loaded = *(void **)(bytes +
                    SFX_CACHE_FIRST_LOADED_OFFSET + delta) != NULL;
            }
        }
    }
    return TRUE;
}

static BOOL invoke_cache_call(
    void *manager,
    SudekiMpResourceName *resource_name,
    void *entry
) {
    uintptr_t this_register = (uintptr_t)manager;
    uintptr_t result;

    __asm__ volatile(
        "pushl %[resource_name]\n\t"
        "call *%[entry]"
        : "+c"(this_register), "=a"(result)
        : [resource_name] "r"(resource_name), [entry] "r"(entry)
        : "edx", "memory", "cc"
    );
    return (result & 0xffu) != 0u;
}

static BOOL native_pre_cache_effect(
    void *context,
    void *manager,
    SudekiMpResourceName *resource_name
) {
    return invoke_cache_call(manager, resource_name,
        (uint8_t *)context + RVA_SFX_PRE_CACHE);
}

static BOOL native_un_cache_effect(
    void *context,
    void *manager,
    SudekiMpResourceName *resource_name
) {
    return invoke_cache_call(manager, resource_name,
        (uint8_t *)context + RVA_SFX_UN_CACHE);
}

static void invoke_sfx_play(
    void *manager,
    SudekiMpLanArenaSpiritVfxTransientTPtr *actor,
    SudekiMpResourceName *resource_name,
    void *entry
) {
    uintptr_t this_register = (uintptr_t)manager;

    /* Exact x86 thiscall. Seven dword arguments are callee-cleaned. The five
     * immediate zeroes are z, y, x, realTime, and followCharacter. */
    __asm__ volatile(
        "pushl $0\n\t"
        "pushl $0\n\t"
        "pushl $0\n\t"
        "pushl $0\n\t"
        "pushl $0\n\t"
        "pushl %[resource_name]\n\t"
        "pushl %[actor]\n\t"
        "call *%[entry]"
        : "+c"(this_register)
        : [resource_name] "r"(resource_name), [actor] "r"(actor),
          [entry] "r"(entry)
        : "eax", "edx", "memory", "cc"
    );
}

static void native_play_sfx(
    void *context,
    void *manager,
    SudekiMpLanArenaSpiritVfxTransientTPtr *actor,
    SudekiMpResourceName *resource_name,
    BOOL follow_character,
    BOOL real_time,
    float x,
    float y,
    float z
) {
    (void)follow_character;
    (void)real_time;
    (void)x;
    (void)y;
    (void)z;
    invoke_sfx_play(manager, actor, resource_name,
        (uint8_t *)context + RVA_SFX_PLAY);
}

static void fill_native_api(
    HMODULE game_module,
    SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    ZeroMemory(api, sizeof(*api));
    api->context = (void *)game_module;
    api->resolve_tal = native_resolve_tal;
    api->tal_ready = native_tal_ready;
    api->get_sfx_manager = native_get_sfx_manager;
    api->resource_name_from_text = native_resource_name_from_text;
    api->release_resource_name = native_release_resource_name;
    api->cache_snapshot = native_cache_snapshot;
    api->pre_cache_effect = native_pre_cache_effect;
    api->un_cache_effect = native_un_cache_effect;
    api->play_sfx = native_play_sfx;
}

static BOOL begin_operation(void) {
    if (InterlockedCompareExchange(&operation_active, 1, 0) != 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return TRUE;
}

static void end_operation(DWORD error) {
    InterlockedExchange(&operation_active, 0);
    SetLastError(error);
}

static void clear_lease(SudekiMpLanArenaSpiritVfxCacheLease *lease) {
    ZeroMemory(lease, sizeof(*lease));
    lease->slot_index = -1;
}

static BOOL lease_shape_valid(
    const SudekiMpLanArenaSpiritVfxCacheLease *lease
) {
    if (lease == NULL || lease->state < SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY ||
        lease->state > SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING) {
        return FALSE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY) return TRUE;
    if (lease->manager == NULL ||
        lease->resource_identifier == RESOURCE_IDENTIFIER_INVALID) {
        return FALSE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_POISONED) {
        return lease->slot_index >= -1 &&
            lease->slot_index < (LONG)SFX_CACHE_ENTRY_COUNT;
    }
    return lease->slot_index >= 0 &&
        lease->slot_index < (LONG)SFX_CACHE_ENTRY_COUNT;
}

static BOOL api_has_cache_observation(
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    return api != NULL && api->get_sfx_manager != NULL &&
        api->cache_snapshot != NULL;
}

static BOOL api_has_resource_name(
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    return api != NULL && api->resource_name_from_text != NULL &&
        api->release_resource_name != NULL;
}

static BOOL snapshots_equal(
    const SudekiMpLanArenaSpiritVfxCacheSnapshot *left,
    const SudekiMpLanArenaSpiritVfxCacheSnapshot *right
) {
    return left->matching_slots == right->matching_slots &&
        (left->matching_slots == 0u ||
         (left->slot_index == right->slot_index &&
          left->ref_count == right->ref_count &&
          left->pending == right->pending &&
          left->loaded == right->loaded));
}

static BOOL snapshot_owns_lease(
    const SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxCacheSnapshot *snapshot
) {
    return snapshot->matching_slots == 1u &&
        snapshot->slot_index == (unsigned int)lease->slot_index &&
        snapshot->ref_count > 0;
}

static BOOL construct_named_resource(
    const SudekiMpLanArenaSpiritVfxReplayApi *api,
    SudekiMpResourceName *resource_name,
    const char *name,
    uint32_t identifier
) {
    ZeroMemory(resource_name, sizeof(*resource_name));
    if (!api->resource_name_from_text(api->context, resource_name,
            name)) {
        return FALSE;
    }
    if (resource_name->identifier != identifier ||
        resource_name->text_reference == NULL) {
        api->release_resource_name(api->context, resource_name);
        ZeroMemory(resource_name, sizeof(*resource_name));
        return FALSE;
    }
    return TRUE;
}

static BOOL construct_initiate_resource(
    const SudekiMpLanArenaSpiritVfxReplayApi *api,
    SudekiMpResourceName *resource_name
) {
    return construct_named_resource(api, resource_name, initiate_resource,
        TAL_INITIATE_HOM_IDENTIFIER);
}

static BOOL poll_lease_locked(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api,
    void *manager
) {
    SudekiMpLanArenaSpiritVfxCacheSnapshot snapshot;

    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_POISONED) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (!api->cache_snapshot(api->context, manager,
            lease->resource_identifier, &snapshot) ||
        !snapshot_owns_lease(lease, &snapshot)) {
        lease->state = SUDEKIMP_SPIRIT_VFX_CACHE_POISONED;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (snapshot.loaded && !snapshot.pending) {
        lease->state = SUDEKIMP_SPIRIT_VFX_CACHE_READY;
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    lease->state = SUDEKIMP_SPIRIT_VFX_CACHE_LOADING;
    SetLastError(ERROR_IO_PENDING);
    return FALSE;
}

static BOOL release_locked(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
);

static BOOL prepare_named_locked(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api,
    const char *name,
    uint32_t identifier
) {
    SudekiMpLanArenaSpiritVfxCacheSnapshot before;
    SudekiMpLanArenaSpiritVfxCacheSnapshot confirmed_before;
    SudekiMpLanArenaSpiritVfxCacheSnapshot after;
    SudekiMpResourceName resource_name;
    uint32_t resource_identifier;
    void *manager;
    BOOL native_existing;
    BOOL accepted;

    if (!lease_shape_valid(lease) || !api_has_cache_observation(api) ||
        !api_has_resource_name(api) || api->pre_cache_effect == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    manager = api->get_sfx_manager(api->context);
    if (lease->state != SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY &&
        lease->manager != manager) {
        /* The old manager is no longer callable. Its destruction owns any
         * stale slot; never transfer or UnCache through the new identity. */
        clear_lease(lease);
    }
    if (manager == NULL) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING) {
        if (!release_locked(lease, api)) return FALSE;
        /* Release may invoke native callbacks. Resolve the manager again
         * before constructing a fresh acquisition on this boundary. */
        manager = api->get_sfx_manager(api->context);
        if (manager == NULL) {
            SetLastError(ERROR_NOT_READY);
            return FALSE;
        }
    }
    if (lease->state != SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY) {
        return poll_lease_locked(lease, api, manager);
    }
    if (!construct_named_resource(api, &resource_name, name, identifier)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    resource_identifier = resource_name.identifier;
    if (!api->cache_snapshot(api->context, manager,
            resource_identifier, &before) ||
        before.matching_slots > 1u ||
        (before.matching_slots == 1u &&
         (before.ref_count <= 0 || before.ref_count == LONG_MAX))) {
        api->release_resource_name(api->context, &resource_name);
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (api->get_sfx_manager(api->context) != manager ||
        !api->cache_snapshot(api->context, manager,
            resource_identifier, &confirmed_before) ||
        !snapshots_equal(&before, &confirmed_before)) {
        api->release_resource_name(api->context, &resource_name);
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }

    /* Native admission edge. It is called exactly once for this generation;
     * post-call ambiguity poisons the generation instead of retrying. */
    ZeroMemory(&after, sizeof(after));
    native_existing = api->pre_cache_effect(api->context, manager,
        &resource_name);
    accepted = api->cache_snapshot(api->context, manager,
        resource_identifier, &after);
    if (resource_name.identifier != resource_identifier || !accepted ||
        after.matching_slots != 1u || after.ref_count <= 0 ||
        (before.matching_slots == 0u &&
         (native_existing || after.ref_count != 1)) ||
        (before.matching_slots == 1u &&
         (!native_existing || after.slot_index != before.slot_index ||
          after.ref_count != before.ref_count + 1))) {
        lease->manager = manager;
        lease->resource_identifier = resource_identifier;
        lease->slot_index = after.matching_slots == 1u ?
            (LONG)after.slot_index : -1;
        lease->acquired_ref_count = after.matching_slots == 1u ?
            after.ref_count : 0;
        lease->state = SUDEKIMP_SPIRIT_VFX_CACHE_POISONED;
        api->release_resource_name(api->context, &resource_name);
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    lease->manager = manager;
    lease->resource_identifier = resource_identifier;
    lease->slot_index = (LONG)after.slot_index;
    lease->acquired_ref_count = after.ref_count;
    lease->state = after.loaded && !after.pending ?
        SUDEKIMP_SPIRIT_VFX_CACHE_READY :
        SUDEKIMP_SPIRIT_VFX_CACHE_LOADING;
    api->release_resource_name(api->context, &resource_name);
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_READY) {
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    SetLastError(ERROR_IO_PENDING);
    return FALSE;
}

static BOOL prepare_locked(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    return prepare_named_locked(lease, api, initiate_resource,
        TAL_INITIATE_HOM_IDENTIFIER);
}

static BOOL ready_locked(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    void *manager;

    if (!lease_shape_valid(lease) || !api_has_cache_observation(api)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    manager = api->get_sfx_manager(api->context);
    if (manager != lease->manager) {
        clear_lease(lease);
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    return poll_lease_locked(lease, api, manager);
}

static BOOL release_locked(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    SudekiMpLanArenaSpiritVfxCacheSnapshot snapshot;
    SudekiMpResourceName resource_name;
    void *manager;
    const char *name = NULL;
    unsigned int resource_index;

    if (!lease_shape_valid(lease) || !api_has_cache_observation(api) ||
        !api_has_resource_name(api) || api->un_cache_effect == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_EMPTY) {
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    manager = api->get_sfx_manager(api->context);
    if (manager != lease->manager) {
        clear_lease(lease);
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    if (lease->state == SUDEKIMP_SPIRIT_VFX_CACHE_POISONED ||
        !api->cache_snapshot(api->context, manager,
            lease->resource_identifier, &snapshot) ||
        !snapshot_owns_lease(lease, &snapshot)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    for (resource_index = 0u; resource_index < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST;
            ++resource_index) {
        if (visual_resources[resource_index].identifier ==
                lease->resource_identifier) {
            name = visual_resources[resource_index].name;
            break;
        }
    }
    if (name == NULL || !construct_named_resource(api, &resource_name, name,
            lease->resource_identifier)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (resource_name.identifier != lease->resource_identifier ||
        api->get_sfx_manager(api->context) != manager ||
        !api->cache_snapshot(api->context, manager,
            lease->resource_identifier, &snapshot) ||
        !snapshot_owns_lease(lease, &snapshot)) {
        api->release_resource_name(api->context, &resource_name);
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }

    /* Once entered, never retry: native UnCache itself owns the one decrement.
     * Its return is deliberately not used as permission for a second call. */
    (void)api->un_cache_effect(api->context, manager, &resource_name);
    clear_lease(lease);
    api->release_resource_name(api->context, &resource_name);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

static BOOL replay_locked(
    void *expected_tal,
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    SudekiMpLanArenaSpiritVfxTransientTPtr actor;
    SudekiMpLanArenaSpiritVfxCacheSnapshot snapshot;
    SudekiMpResourceName resource_name;
    void *manager;
    void *current_manager;

    if (expected_tal == NULL || !lease_shape_valid(lease) ||
        !api_has_cache_observation(api) || !api_has_resource_name(api) ||
        api->resolve_tal == NULL || api->tal_ready == NULL ||
        api->play_sfx == NULL || api->un_cache_effect == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lease->state != SUDEKIMP_SPIRIT_VFX_CACHE_READY) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    manager = api->get_sfx_manager(api->context);
    if (manager == NULL || manager != lease->manager ||
        !api->cache_snapshot(api->context, manager,
            lease->resource_identifier, &snapshot) ||
        !snapshot_owns_lease(lease, &snapshot) ||
        !snapshot.loaded || snapshot.pending ||
        api->resolve_tal(api->context) != expected_tal ||
        !api->tal_ready(api->context, expected_tal)) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (api->resolve_tal(api->context) != expected_tal ||
        api->get_sfx_manager(api->context) != manager) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!construct_initiate_resource(api, &resource_name)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (resource_name.identifier != lease->resource_identifier ||
        api->get_sfx_manager(api->context) != manager ||
        !api->cache_snapshot(api->context, manager,
            lease->resource_identifier, &snapshot) ||
        !snapshot_owns_lease(lease, &snapshot) ||
        !snapshot.loaded || snapshot.pending ||
        api->resolve_tal(api->context) != expected_tal ||
        !api->tal_ready(api->context, expected_tal)) {
        api->release_resource_name(api->context, &resource_name);
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }

    ZeroMemory(&actor, sizeof(actor));
    actor.object = expected_tal;
    /* Irreversible admission edge. Every return below is TRUE. */
    api->play_sfx(api->context, manager, &actor, &resource_name,
        FALSE, FALSE, 0.0f, 0.0f, 0.0f);

    current_manager = api->get_sfx_manager(api->context);
    if (current_manager == manager && lease->manager == manager &&
        api->cache_snapshot(api->context, manager,
            lease->resource_identifier, &snapshot) &&
        snapshot_owns_lease(lease, &snapshot)) {
        (void)api->un_cache_effect(api->context, manager, &resource_name);
        clear_lease(lease);
    } else if (current_manager == manager) {
        /* PlaySfx has consumed this START, but no UnCache was entered. Keep
         * the exact acquisition so a later cleanup can retry without either
         * replaying the effect or accumulating another cache reference. */
        lease->state = SUDEKIMP_SPIRIT_VFX_CACHE_RELEASE_PENDING;
    } else {
        /* The old manager is no longer callable; its destruction owns the
         * stale slot. Never transfer that obligation to the new manager. */
        clear_lease(lease);
    }
    api->release_resource_name(api->context, &resource_name);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVfxReplayImageMatches(HMODULE game_module) {
    const uint8_t *base = (const uint8_t *)game_module;
    uint32_t manager_global_operand;
    uint32_t expected_manager_global;

    if (base == NULL ||
        !readable_region(base + RVA_SFX_GET_MANAGER, 6u) ||
        !readable_region(base + RVA_SFX_PLAY,
            sizeof(expected_sfx_play_body)) ||
        !readable_region(base + RVA_SFX_PRE_CACHE + 0xf0u,
            sizeof(expected_pre_cache_existing_tail)) ||
        !readable_region(base + RVA_SFX_UN_CACHE + 0x5eu,
            sizeof(expected_un_cache_suffix)) ||
        base[RVA_SFX_GET_MANAGER] != 0xa1u ||
        base[RVA_SFX_GET_MANAGER + 5u] != 0xc3u ||
        memcmp(base + RVA_SFX_PLAY, expected_sfx_play_body,
            sizeof(expected_sfx_play_body)) != 0 ||
        memcmp(base + RVA_SFX_PRE_CACHE, expected_pre_cache_prefix,
            sizeof(expected_pre_cache_prefix)) != 0 ||
        memcmp(base + RVA_SFX_PRE_CACHE + 0xc0u,
            expected_pre_cache_new_tail,
            sizeof(expected_pre_cache_new_tail)) != 0 ||
        memcmp(base + RVA_SFX_PRE_CACHE + 0xf0u,
            expected_pre_cache_existing_tail,
            sizeof(expected_pre_cache_existing_tail)) != 0 ||
        memcmp(base + RVA_SFX_UN_CACHE, expected_un_cache_prefix,
            sizeof(expected_un_cache_prefix)) != 0 ||
        memcmp(base + RVA_SFX_UN_CACHE + 0x5eu,
            expected_un_cache_suffix,
            sizeof(expected_un_cache_suffix)) != 0) {
        return FALSE;
    }
    memcpy(&manager_global_operand, base + RVA_SFX_GET_MANAGER + 1u,
        sizeof(manager_global_operand));
    expected_manager_global = (uint32_t)(uintptr_t)(
        base + RVA_SFX_MANAGER_GLOBAL);
    return manager_global_operand == expected_manager_global &&
        readable_region(base + RVA_SFX_MANAGER_VTABLE, sizeof(void *)) &&
        readable_region(base + RVA_SFX_MANAGER_SECONDARY_VTABLE,
            sizeof(void *));
}

BOOL SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    BOOL result;
    DWORD error;

    if (!begin_operation()) return FALSE;
    result = prepare_locked(lease, api);
    error = result ? ERROR_SUCCESS : GetLastError();
    end_operation(error);
    return result;
}

BOOL SudekiMpLanArenaSpiritVfxTalInitiateCacheReadyWithApi(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    BOOL result;
    DWORD error;

    if (!begin_operation()) return FALSE;
    result = ready_locked(lease, api);
    error = result ? ERROR_SUCCESS : GetLastError();
    end_operation(error);
    return result;
}

BOOL SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    BOOL result;
    DWORD error;

    if (!begin_operation()) return FALSE;
    result = release_locked(lease, api);
    error = result ? ERROR_SUCCESS : GetLastError();
    end_operation(error);
    return result;
}

BOOL SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
    void *expected_tal,
    SudekiMpLanArenaSpiritVfxCacheLease *lease,
    const SudekiMpLanArenaSpiritVfxReplayApi *api
) {
    BOOL result;
    DWORD error;

    if (!begin_operation()) return FALSE;
    result = replay_locked(expected_tal, lease, api);
    error = result ? ERROR_SUCCESS : GetLastError();
    end_operation(error);
    return result;
}

BOOL SudekiMpLanArenaSpiritVfxPrepareTalInitiate(HMODULE game_module) {
    SudekiMpLanArenaSpiritVfxReplayApi api;

    if (!SudekiMpLanArenaSpiritVfxReplayImageMatches(game_module)) {
        SetLastError(game_module == NULL ? ERROR_INVALID_PARAMETER :
            ERROR_INVALID_DATA);
        return FALSE;
    }
    fill_native_api(game_module, &api);
    return SudekiMpLanArenaSpiritVfxPrepareTalInitiateWithApi(
        &production_lease, &api);
}

BOOL SudekiMpLanArenaSpiritVfxTalInitiateCacheReady(HMODULE game_module) {
    SudekiMpLanArenaSpiritVfxReplayApi api;

    if (!SudekiMpLanArenaSpiritVfxReplayImageMatches(game_module)) {
        SetLastError(game_module == NULL ? ERROR_INVALID_PARAMETER :
            ERROR_INVALID_DATA);
        return FALSE;
    }
    fill_native_api(game_module, &api);
    return SudekiMpLanArenaSpiritVfxTalInitiateCacheReadyWithApi(
        &production_lease, &api);
}

BOOL SudekiMpLanArenaSpiritVfxReleaseTalInitiateCache(HMODULE game_module) {
    SudekiMpLanArenaSpiritVfxReplayApi api;

    if (!SudekiMpLanArenaSpiritVfxReplayImageMatches(game_module)) {
        SetLastError(game_module == NULL ? ERROR_INVALID_PARAMETER :
            ERROR_INVALID_DATA);
        return FALSE;
    }
    fill_native_api(game_module, &api);
    return SudekiMpLanArenaSpiritVfxReleaseTalInitiateCacheWithApi(
        &production_lease, &api);
}

BOOL SudekiMpLanArenaSpiritVfxReplayTalInitiate(
    HMODULE game_module,
    void *expected_tal
) {
    SudekiMpLanArenaSpiritVfxReplayApi api;

    if (expected_tal == NULL || game_module == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!SudekiMpLanArenaSpiritVfxReplayImageMatches(game_module)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    fill_native_api(game_module, &api);
    return SudekiMpLanArenaSpiritVfxReplayTalInitiateWithApi(
        expected_tal, &production_lease, &api);
}

LONG SudekiMpLanArenaSpiritVfxReplayActiveCalls(void) {
    return InterlockedCompareExchange(&operation_active, 0, 0);
}

static const SpiritVisualResource *visual_resource(unsigned int kind) {
    if (kind < 1u || kind > SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST ||
        visual_resources[kind - 1u].name == NULL)
        return NULL;
    return &visual_resources[kind - 1u];
}

BOOL SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(
    unsigned int kind, float native_phase, float host_phase, BOOL *apply
) {
    if (apply == NULL || visual_resource(kind) == NULL ||
        !isfinite(native_phase) || native_phase < 0.0f ||
        native_phase > 1000000.0f || !isfinite(host_phase) ||
        host_phase < 0.0f || host_phase > 1000000.0f) return FALSE;
    /* SFXSS900 is a finite, non-looping host charge effect. Client native
     * playback advances between publications; forcing an older interpolated
     * phase back onto it repeatedly rewinds its particle timeline. Preserve
     * that progress, but allow initial/late forward catch-up. The complete
     * host roster still owns retirement, never this phase decision. */
    *apply = kind != SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE ||
        host_phase > native_phase;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVfxVisualMatrix(
    const SudekiMpLanArenaSpiritVfxSnapshot *visual, float matrix[16]
) {
    float x, y, z, w, length;
    unsigned int index;
    if (visual == NULL || matrix == NULL || visual->instance_sequence == 0u ||
        visual->skill_sequence == 0u || visual_resource(visual->kind) == NULL ||
        visual->phase_valid > 1u || !isfinite(visual->phase) ||
        visual->phase < 0.0f || visual->phase > 1000000.0f) return FALSE;
    for (index = 0u; index < 3u; ++index) {
        if (!isfinite(visual->position[index]) ||
            fabsf(visual->position[index]) > 1000000.0f ||
            !isfinite(visual->scale[index]) || visual->scale[index] <= 0.0f ||
            visual->scale[index] > 1000.0f) return FALSE;
    }
    for (index = 0u; index < 4u; ++index)
        if (!isfinite(visual->rotation_xyzw[index])) return FALSE;
    x = visual->rotation_xyzw[0]; y = visual->rotation_xyzw[1];
    z = visual->rotation_xyzw[2]; w = visual->rotation_xyzw[3];
    length = sqrtf(x*x + y*y + z*z + w*w);
    if (!isfinite(length) || length < 0.99f || length > 1.01f) return FALSE;
    x /= length; y /= length; z /= length; w /= length;
    ZeroMemory(matrix, 16u * sizeof(*matrix));
    /* Native D3DX row-vector convention, scale applied to each basis row. */
    matrix[0] = 1.0f - 2.0f*(y*y + z*z);
    matrix[1] = 2.0f*(x*y + z*w); matrix[2] = 2.0f*(x*z - y*w);
    matrix[4] = 2.0f*(x*y - z*w);
    matrix[5] = 1.0f - 2.0f*(x*x + z*z); matrix[6] = 2.0f*(y*z + x*w);
    matrix[8] = 2.0f*(x*z + y*w); matrix[9] = 2.0f*(y*z - x*w);
    matrix[10] = 1.0f - 2.0f*(x*x + y*y);
    for (index = 0u; index < 3u; ++index) {
        matrix[index*4u] *= visual->scale[index];
        matrix[index*4u+1u] *= visual->scale[index];
        matrix[index*4u+2u] *= visual->scale[index];
        matrix[12u+index] = visual->position[index];
    }
    matrix[15] = 1.0f;
    return TRUE;
}

static BOOL visual_api_valid(const SudekiMpLanArenaSpiritVfxVisualApi *api) {
    return api != NULL && api_has_resource_name(&api->cache) &&
        api_has_cache_observation(&api->cache) &&
        api->cache.pre_cache_effect != NULL &&
        api->cache.un_cache_effect != NULL && api->spawn != NULL &&
        api->synchronize != NULL && api->retire != NULL && api->detach != NULL;
}

static BOOL retire_visual_slot(SudekiMpLanArenaSpiritVfxVisualSlot *slot,
    const SudekiMpLanArenaSpiritVfxVisualApi *api) {
    if (!slot->occupied) return TRUE;
    if (!slot->retire_entered) {
        if (!api->retire(api->cache.context, &slot->observer)) {
            if (!slot->cleanup_failure_reported) {
                SudekiMpLogFormat("lan_arena_spirit_vfx event=visual_retire "
                    "instance=%lu kind=%u state=pending\r\n",
                    (unsigned long)slot->identity.instance_sequence,
                    (unsigned int)slot->identity.kind);
                slot->cleanup_failure_reported = TRUE;
            }
            return FALSE;
        }
        slot->retire_entered = TRUE;
        SudekiMpLogFormat("lan_arena_spirit_vfx event=visual_retire "
            "instance=%lu kind=%u state=entered\r\n",
            (unsigned long)slot->identity.instance_sequence,
            (unsigned int)slot->identity.kind);
    }
    if (!api->detach(api->cache.context, &slot->observer)) return FALSE;
    ZeroMemory(slot, sizeof(*slot));
    return TRUE;
}

static BOOL reset_visuals_locked(SudekiMpLanArenaSpiritVfxVisualState *state,
    const SudekiMpLanArenaSpiritVfxVisualApi *api) {
    unsigned int index;
    BOOL result = TRUE;
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY; ++index)
        if (!retire_visual_slot(&state->slots[index], api)) result = FALSE;
    /* Effects and observer nodes must be quiescent before backing caches. */
    if (!result) { SetLastError(ERROR_BUSY); return FALSE; }
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST; ++index)
        if (!release_locked(&state->caches[index], &api->cache)) result = FALSE;
    if (result) {
        state->session_token = 0u;
        state->newest_instance_sequence = 0u;
    }
    return result;
}

static BOOL same_visual_identity(const SudekiMpLanArenaSpiritVfxSnapshot *a,
    const SudekiMpLanArenaSpiritVfxSnapshot *b) {
    return a->instance_sequence == b->instance_sequence &&
        a->skill_sequence == b->skill_sequence && a->kind == b->kind &&
        a->emitted_host_tick == b->emitted_host_tick;
}

static BOOL service_visuals_locked(SudekiMpLanArenaSpiritVfxVisualState *state,
    const SudekiMpLanArenaSnapshot *snapshot, uint64_t session_token,
    const SudekiMpLanArenaSpiritVfxVisualApi *api) {
    unsigned int index, other;
    uint32_t previous_newest;
    float matrix[16];
    BOOL result = TRUE;
    if (snapshot == NULL || session_token == 0u ||
        snapshot->spirit_vfx_observed > 1u ||
        snapshot->spirit_vfx_count > SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY ||
        (!snapshot->spirit_vfx_observed && snapshot->spirit_vfx_count != 0u)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    /* Validate the complete incoming roster before any native mutation. */
    for (index = 0u; index < snapshot->spirit_vfx_count; ++index) {
        if (!SudekiMpLanArenaSpiritVfxVisualMatrix(
                &snapshot->spirit_vfx[index], matrix)) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        for (other = 0u; other < index; ++other)
            if (snapshot->spirit_vfx[index].instance_sequence ==
                    snapshot->spirit_vfx[other].instance_sequence) {
                SetLastError(ERROR_INVALID_DATA); return FALSE;
            }
        if (state->session_token == session_token) {
            for (other = 0u; other < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY;
                    ++other) {
                const SudekiMpLanArenaSpiritVfxVisualSlot *existing =
                    &state->slots[other];
                if (existing->occupied &&
                    existing->identity.instance_sequence ==
                        snapshot->spirit_vfx[index].instance_sequence &&
                    !same_visual_identity(&existing->identity,
                        &snapshot->spirit_vfx[index])) {
                    SetLastError(ERROR_INVALID_DATA); return FALSE;
                }
            }
        }
    }
    if (state->session_token != session_token) {
        if (!reset_visuals_locked(state, api)) return FALSE;
        state->session_token = session_token;
    }
    if (!snapshot->spirit_vfx_observed) return TRUE;
    for (index = 0u; index < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY; ++index) {
        SudekiMpLanArenaSpiritVfxVisualSlot *slot = &state->slots[index];
        BOOL present = FALSE;
        if (!slot->occupied) continue;
        for (other = 0u; other < snapshot->spirit_vfx_count; ++other) {
            if (slot->identity.instance_sequence ==
                    snapshot->spirit_vfx[other].instance_sequence) {
                /* Immutable identity was validated for every occupied slot
                 * before the first retirement above can be entered. */
                present = TRUE;
            }
        }
        if (!present && !retire_visual_slot(slot, api)) result = FALSE;
    }
    previous_newest = state->newest_instance_sequence;
    for (index = 0u; index < snapshot->spirit_vfx_count; ++index) {
        const SudekiMpLanArenaSpiritVfxSnapshot *visual =
            &snapshot->spirit_vfx[index];
        const SpiritVisualResource *resource = visual_resource(visual->kind);
        SudekiMpLanArenaSpiritVfxVisualSlot *slot = NULL, *empty = NULL;
        SudekiMpLanArenaSpiritVfxCacheLease *cache =
            &state->caches[visual->kind - 1u];
        for (other = 0u; other < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY; ++other) {
            if (!state->slots[other].occupied && empty == NULL)
                empty = &state->slots[other];
            if (state->slots[other].occupied &&
                state->slots[other].identity.instance_sequence ==
                    visual->instance_sequence) slot = &state->slots[other];
        }
        if (slot == NULL) {
            /* A complete roster cannot resurrect an older retired identity. */
            if (previous_newest != 0u &&
                (int32_t)(visual->instance_sequence - previous_newest) <= 0)
                continue;
            if (empty == NULL) { result = FALSE; continue; }
            slot = empty;
            slot->identity = *visual;
            slot->occupied = TRUE;
            if (state->newest_instance_sequence == 0u ||
                (int32_t)(visual->instance_sequence -
                    state->newest_instance_sequence) > 0)
                state->newest_instance_sequence = visual->instance_sequence;
        }
        if (slot->retire_entered) { result = FALSE; continue; }
        if (!slot->spawn_entered) {
            if (!prepare_named_locked(cache, &api->cache, resource->name,
                    resource->identifier)) { result = FALSE; continue; }
            if (!api->spawn(api->cache.context, cache->manager, visual,
                    &slot->observer)) { result = FALSE; continue; }
            slot->spawn_entered = TRUE;
        }
        /* Native destruction nulls the weak node. It is still consumed. */
        if (slot->observer.object != NULL &&
            !api->synchronize(api->cache.context, visual, &slot->observer))
            result = FALSE;
    }
    if (!result && GetLastError() == ERROR_SUCCESS)
        SetLastError(ERROR_NOT_READY);
    return result;
}

BOOL SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(
    SudekiMpLanArenaSpiritVfxVisualState *state,
    const SudekiMpLanArenaSnapshot *snapshot, uint64_t session_token,
    const SudekiMpLanArenaSpiritVfxVisualApi *api
) {
    BOOL result; DWORD error;
    if (state == NULL || !visual_api_valid(api)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if (!begin_operation()) return FALSE;
    SetLastError(ERROR_SUCCESS);
    result = service_visuals_locked(state, snapshot, session_token, api);
    error = result ? ERROR_SUCCESS : GetLastError();
    end_operation(error); return result;
}

BOOL SudekiMpLanArenaSpiritVfxResetVisualsWithApi(
    SudekiMpLanArenaSpiritVfxVisualState *state,
    const SudekiMpLanArenaSpiritVfxVisualApi *api
) {
    BOOL result; DWORD error;
    if (state == NULL || !visual_api_valid(api)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if (!begin_operation()) return FALSE;
    result = reset_visuals_locked(state, api);
    error = result ? ERROR_SUCCESS : GetLastError();
    end_operation(error); return result;
}

enum {
    RVA_VISUAL_MATRIX_SPAWN = 0x17da0u,
    RVA_VISUAL_WEAK_ASSIGN = 0x1750u,
    RVA_VISUAL_RETIRE = 0x131df0u,
    RVA_VISUAL_VTABLE = 0x2d3c7cu,
    RVA_VISUAL_POSITION_VTABLE = 0x2cdefcu,
    RVA_VISUAL_COMPONENT_VTABLE = 0x2c83f4u,
    RVA_VISUAL_EVENT_SOURCE_VTABLE = 0x2c8464u,
    RVA_VISUAL_SOUND_VTABLE = 0x2cd0acu,
    RVA_VISUAL_SOUND_LISTENER_VTABLE = 0x2cd0f4u,
    RVA_VISUAL_RENDERER_VTABLE = 0x2df8ecu,
    RVA_VISUAL_MATRIX_SET = 0x110a40u,
    RVA_VISUAL_POSITION_UPDATE = 0x110f90u,
    RVA_VISUAL_TIME_SET = 0x223180u,
    RVA_VISUAL_TIME_GET = 0x223220u,
    RVA_VISUAL_SUBMODEL_COUNT = 0x21bb10u
};

typedef unsigned int (__attribute__((thiscall)) *VisualCount)(void *renderer);
typedef int (__attribute__((thiscall)) *VisualSelector)(void *, int, unsigned int);
typedef void (__attribute__((thiscall)) *VisualSetTime)(void *, int,
    unsigned int, float, int);
typedef void (__attribute__((stdcall)) *VisualRetire)(void *);

BOOL SudekiMpLanArenaSpiritVfxVisualImageMatches(HMODULE module) {
    const uint8_t *base = (const uint8_t *)module;
    unsigned int index;
    static const uint32_t sound_listener_callbacks[] = {
        0x3b870u, 0x1e8310u, 0x1e8310u, 0x1e8310u, 0xa2900u,
        0xfc1b0u, 0xfc100u, 0xfc1f0u, 0x1e8310u, 0x1e8310u,
        0x1e8310u, 0x1e8310u, 0x1e8310u, 0x1e8310u
    };
    static const uint8_t spawn[] = {0xd9,0x44,0x24,0x18,0x56,0x50,0x51,0xd9,0x1c,0x24};
    static const uint8_t weak[] = {0x8b,0x08,0x85,0xc9,0x74,0x35,0x57,0x39,0x41,0x04};
    static const uint8_t retire[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x8b,0x4d,0x08};
    static const uint8_t matrix[] = {0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0xc4,0,0,0};
    static const uint8_t update[] = {0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0x34,0x01,0,0};
    if (!SudekiMpLanArenaSpiritVfxReplayImageMatches(module) ||
        !readable_region(base + RVA_VISUAL_RENDERER_VTABLE, 0x114u) ||
        !readable_region(base + RVA_VISUAL_SOUND_LISTENER_VTABLE, 0x3cu)) return FALSE;
    for (index = 0u; index < sizeof(sound_listener_callbacks) /
            sizeof(sound_listener_callbacks[0]); ++index)
        if (*(void **)(base + RVA_VISUAL_SOUND_LISTENER_VTABLE +
                4u + index * 4u) != base + sound_listener_callbacks[index])
            return FALSE;
    return memcmp(base + RVA_VISUAL_MATRIX_SPAWN, spawn, sizeof(spawn)) == 0 &&
        memcmp(base + RVA_VISUAL_WEAK_ASSIGN, weak, sizeof(weak)) == 0 &&
        memcmp(base + RVA_VISUAL_RETIRE, retire, sizeof(retire)) == 0 &&
        memcmp(base + RVA_VISUAL_MATRIX_SET, matrix, sizeof(matrix)) == 0 &&
        memcmp(base + RVA_VISUAL_POSITION_UPDATE, update, sizeof(update)) == 0 &&
        base[RVA_VISUAL_MATRIX_SPAWN + 0xa9u] == 0xc2u &&
        base[RVA_VISUAL_MATRIX_SPAWN + 0xaau] == 0x18u &&
        *(void **)(base + RVA_VISUAL_RENDERER_VTABLE + 0xf8u) ==
            base + RVA_VISUAL_SUBMODEL_COUNT &&
        *(void **)(base + RVA_VISUAL_RENDERER_VTABLE + 0x10cu) ==
            base + RVA_VISUAL_TIME_SET &&
        *(void **)(base + RVA_VISUAL_RENDERER_VTABLE + 0x110u) ==
            base + RVA_VISUAL_TIME_GET;
}

/* This is the engine's intrusive weak assignment, not a retained Entity
 * reference. Base destructor0x404d30 nulls every linked node before free. */
static void visual_weak_assign(void *context,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer, void *entity) {
    uintptr_t accumulator = (uintptr_t)observer;
    uintptr_t data = (uintptr_t)entity;
    void *entry = (uint8_t *)context + RVA_VISUAL_WEAK_ASSIGN;
    __asm__ volatile("call *%[entry]"
        : "+a"(accumulator), "+d"(data)
        : [entry] "r"(entry) : "ecx", "memory", "cc");
}

static BOOL visual_weak_links_valid(
    const SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    uint8_t *entity = (uint8_t *)observer->object;
    SudekiMpLanArenaSpiritVfxTransientTPtr *previous =
        (SudekiMpLanArenaSpiritVfxTransientTPtr *)observer->previous_observer;
    SudekiMpLanArenaSpiritVfxTransientTPtr *next =
        (SudekiMpLanArenaSpiritVfxTransientTPtr *)observer->next_observer;
    if (entity == NULL) return previous == NULL && next == NULL;
    if (!writable_region(entity, 8u)) return FALSE;
    if (previous == NULL) {
        if (*(void **)(entity + 4u) != observer) return FALSE;
    } else if (!writable_region(previous, sizeof(*previous)) ||
        previous->object != entity || previous->next_observer != observer)
        return FALSE;
    return next == NULL || (writable_region(next, sizeof(*next)) &&
        next->object == entity && next->previous_observer == observer);
}

BOOL SudekiMpLanArenaSpiritVfxVisualIdentityMatches(HMODULE module, void *object) {
    uint8_t *base = (uint8_t *)module;
    uint8_t *entity = (uint8_t *)object;
    void **listeners, **sources;
    if (module == NULL || !writable_region(entity, 0x3e4u) ||
        *(void **)entity != base + RVA_VISUAL_VTABLE ||
        *(void **)(entity + 0x44u) != entity + 0x160u ||
        *(void **)(entity + 0x58u) != entity + 0x270u ||
        *(void **)(entity + 0x160u) != base + RVA_VISUAL_POSITION_VTABLE ||
        *(void **)(entity + 0x270u) != base + RVA_VISUAL_COMPONENT_VTABLE ||
        *(void **)(entity + 0x170u) != entity ||
        *(void **)(entity + 0x280u) != entity ||
        /* The actor-forwarding listener is unbound. The generic factory does
         * install exactly one embedded sound listener, not zero listeners. */
        *(void **)(entity + 0x3d4u) != NULL ||
        *(uint32_t *)(entity + 0x150u) != 0u ||
        *(void **)(entity + 0x5cu) != entity + 0x2e8u ||
        *(void **)(entity + 0x2e8u) != base + RVA_VISUAL_SOUND_VTABLE ||
        *(void **)(entity + 0x2f8u) != entity ||
        *(void **)(entity + 0x288u) != base + RVA_VISUAL_EVENT_SOURCE_VTABLE ||
        *(void **)(entity + 0x300u) != base + RVA_VISUAL_SOUND_LISTENER_VTABLE ||
        *(uint32_t *)(entity + 0x290u) != 1u ||
        *(uint32_t *)(entity + 0x308u) != 1u) return FALSE;
    listeners = *(void ***)(entity + 0x298u);
    sources = *(void ***)(entity + 0x310u);
    return readable_region(listeners, sizeof(*listeners)) &&
        readable_region(sources, sizeof(*sources)) &&
        listeners[0] == entity + 0x300u && sources[0] == entity + 0x288u;
}

static BOOL native_visual_identity(void *context, uint8_t *entity) {
    return SudekiMpLanArenaSpiritVfxVisualIdentityMatches((HMODULE)context, entity);
}

static BOOL native_visual_detach(void *context,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    if (!visual_weak_links_valid(observer)) return FALSE;
    visual_weak_assign(context, observer, NULL);
    return observer->object == NULL && observer->previous_observer == NULL &&
        observer->next_observer == NULL;
}

static BOOL native_visual_retire(void *context,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    uint8_t *entity = (uint8_t *)observer->object;
    if (entity == NULL) return visual_weak_links_valid(observer);
    if (!visual_weak_links_valid(observer) ||
        !native_visual_identity(context, entity)) return FALSE;
    if ((entity[0x3e0u] & 8u) == 0u)
        ((VisualRetire)((uint8_t *)context + RVA_VISUAL_RETIRE))(entity);
    return TRUE;
}

static void *invoke_visual_spawn(void *context, void *manager,
    const float *matrix, SudekiMpResourceName *resource) {
    void *arguments[4];
    uintptr_t result;
    arguments[0] = (uint8_t *)context + RVA_VISUAL_MATRIX_SPAWN;
    arguments[1] = manager; arguments[2] = (void *)matrix;
    arguments[3] = resource;
    /* Native by-value ResourceName transfer consumes this extra reference.
     * ECX=loop1 keeps the owned clone alive; EAX=0 uses native CacheReady,
     * never a caller-forced ready bypass. Six stack words, ret0x18. */
    ++*resource->text_reference;
    __asm__ volatile(
        "pushl $0x3f800000\n\t"
        "movl 12(%%edi), %%edx\n\t"
        "pushl 8(%%edx)\n\tpushl 4(%%edx)\n\tpushl (%%edx)\n\t"
        "pushl 8(%%edi)\n\tpushl 4(%%edi)\n\t"
        "xorl %%eax, %%eax\n\tmovl $1, %%ecx\n\tcall *(%%edi)"
        : "=a"(result) : "D"(arguments) : "ecx", "edx", "memory", "cc");
    return (void *)result;
}

static BOOL native_visual_spawn(void *context, void *manager,
    const SudekiMpLanArenaSpiritVfxSnapshot *visual,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    SudekiMpLanArenaSpiritVfxReplayApi api;
    SudekiMpResourceName name;
    SudekiMpLanArenaSpiritVfxCacheSnapshot cache;
    const SpiritVisualResource *resource = visual_resource(visual->kind);
    uint8_t *entity;
    SudekiMpLanArenaSpiritVfxTransientTPtr *head;
    float matrix[16];
    if (visual_quarantined || resource == NULL ||
        !SudekiMpLanArenaSpiritVfxVisualMatrix(visual, matrix) ||
        observer->object != NULL || observer->previous_observer != NULL ||
        observer->next_observer != NULL ||
        !native_manager_matches(context, manager)) return FALSE;
    if (visual_module_pin == NULL && !GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)&production_visuals, &visual_module_pin)) return FALSE;
    fill_native_api((HMODULE)context, &api);
    if (!construct_named_resource(&api, &name, resource->name,
            resource->identifier)) return FALSE;
    if (!native_cache_snapshot(context, manager, resource->identifier, &cache) ||
        cache.matching_slots != 1u || cache.ref_count <= 0 ||
        !cache.loaded || cache.pending ||
        !writable_region(name.text_reference, sizeof(*name.text_reference)) ||
        *name.text_reference >= (uint32_t)LONG_MAX) {
        native_release_resource_name(context, &name); return FALSE;
    }
    name.encoded_kind = (name.encoded_kind & ~0x7fu) | 0x29u;
    entity = (uint8_t *)invoke_visual_spawn(context, manager, matrix, &name);
    /* Creation was entered: never retry this instance even if native factory
     * returned NULL. Attach before any later operation can pump native work. */
    if (entity != NULL) {
        if (writable_region(entity, 8u) &&
            *(void **)entity == (uint8_t *)context + RVA_VISUAL_VTABLE) {
            head = *(SudekiMpLanArenaSpiritVfxTransientTPtr **)(entity + 4u);
            if (head == NULL || (writable_region(head, sizeof(*head)) &&
                    head->object == entity && head->previous_observer == NULL))
                visual_weak_assign(context, observer, entity);
        }
        if (observer->object != entity || !visual_weak_links_valid(observer)) {
            /* Native entry is consumed, but no safe ownership witness exists.
             * Do not guess a destructor target or release dependent resources. */
            visual_quarantined = TRUE;
        }
    }
    SudekiMpLogFormat("lan_arena_spirit_vfx event=visual_spawn "
        "instance=%lu skill=%u kind=%u state=%s\r\n",
        (unsigned long)visual->instance_sequence, (unsigned int)visual->skill_sequence,
        (unsigned int)visual->kind, visual_quarantined ? "quarantined" :
        (observer->object != NULL ? "observed" : "native_null_consumed"));
    native_release_resource_name(context, &name);
    return TRUE;
}

static BOOL native_visual_synchronize_proven(void *context,
    const SudekiMpLanArenaSpiritVfxSnapshot *visual,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    uint8_t *base = (uint8_t *)context;
    uint8_t *entity = (uint8_t *)observer->object;
    uint8_t *position, *wrapper, *renderer, *matrix_owner, *render_object;
    uint8_t *model, *model_header, *channels, *submodel_states, *clip_table, *clip;
    const SpiritVisualResource *resource = visual_resource(visual->kind);
    unsigned int submodels, index;
    BOOL apply_phase[256];
    uintptr_t accumulator;
    void *matrix_entry = base + RVA_VISUAL_MATRIX_SET;
    float matrix[16];
    if (!visual_weak_links_valid(observer) ||
        !native_visual_identity(context, entity) || resource == NULL ||
        *(uint32_t *)(entity + 0x2a0u) != resource->identifier ||
        !SudekiMpLanArenaSpiritVfxVisualMatrix(visual, matrix)) return FALSE;
    if ((entity[0x3e0u] & 8u) != 0u) return TRUE;
    position = entity + 0x160u;
    matrix_owner = *(uint8_t **)(position + 0x8cu);
    wrapper = *(uint8_t **)(position + 0xb4u);
    if (*(uint32_t *)(position + 0x94u) != 0u ||
        !writable_region(matrix_owner, 0x110u) ||
        !readable_region(wrapper, 0x18u)) return FALSE;
    render_object = *(uint8_t **)(wrapper + 8u);
    /* The root update writes the render object's inline world matrix and
     * transform version. A render-driven root with a native scene parent
     * would enter an additional hierarchy-update contract not owned here. */
    if (!writable_region(render_object, 0xd0u) ||
        (position[0x102u] == 1u &&
         *(void **)(render_object + 0x18u) != NULL)) return FALSE;
    renderer = *(uint8_t **)(wrapper + 0x10u);
    if (!writable_region(renderer, 0xa8u) ||
        *(void **)renderer != base + RVA_VISUAL_RENDERER_VTABLE ||
        /* Wrapper constructor 0x523420 queries the model interface for ANM.
         * Exact concrete renderer 0x61ba40 returns itself for that interface;
         * render-object binding 0x5d6460 stores the same interface at +14. */
        *(void **)(wrapper + 0xcu) != renderer ||
        *(void **)(render_object + 0x14u) != renderer) return FALSE;
    model = *(uint8_t **)(renderer + 8u);
    if (!readable_region(model, 0x24u)) return FALSE;
    model_header = *(uint8_t **)(model + 0x1cu);
    if (!readable_region(model_header, 0x10u)) return FALSE;
    submodels = ((VisualCount)(base + RVA_VISUAL_SUBMODEL_COUNT))(renderer);
    if (submodels == 0u || submodels > 256u) return FALSE;
    channels = *(uint8_t **)(renderer + 0x98u);
    if (!writable_region(channels, 0x24u)) return FALSE;
    submodel_states = *(uint8_t **)channels;
    if (!writable_region(submodel_states, (size_t)submodels * 0x18u)) return FALSE;
    clip_table = *(uint8_t **)(model + 0x20u);
    if (!readable_region(clip_table, sizeof(void *))) return FALSE;
    clip = *(uint8_t **)clip_table;
    if (!readable_region(clip, 8u) || !isfinite(*(float *)(clip + 4u)) ||
        *(float *)(clip + 4u) <= 0.0f) return FALSE;
    for (index = 0u; index < submodels; ++index) {
        if (*(uint16_t *)(submodel_states + index * 0x18u) != 0u) return FALSE;
        apply_phase[index] = FALSE;
        if (visual->phase_valid &&
            !SudekiMpLanArenaSpiritVfxVisualPhaseCorrection(visual->kind,
                *(float *)(submodel_states + index * 0x18u + 8u),
                visual->phase, &apply_phase[index])) return FALSE;
    }
    accumulator = (uintptr_t)matrix;
    __asm__ volatile("pushl %[position]\n\tcall *%[entry]"
        : "+a"(accumulator)
        : [position] "r"(position), [entry] "r"(matrix_entry)
        : "ecx", "edx", "memory", "cc");
    ((VisualRetire)(base + RVA_VISUAL_POSITION_UPDATE))(position);
    if (visual->phase_valid) {
        for (index = 0u; index < submodels; ++index) {
            /* The factory selected clip0 for channel0 on every submodel. */
            if (apply_phase[index])
                ((VisualSetTime)(base + RVA_VISUAL_TIME_SET))(
                    renderer, 0, index, visual->phase, 0);
        }
    }
    return TRUE;
}

static BOOL native_visual_synchronize(void *context,
    const SudekiMpLanArenaSpiritVfxSnapshot *visual,
    SudekiMpLanArenaSpiritVfxTransientTPtr *observer) {
    BOOL result = native_visual_synchronize_proven(context, visual, observer);
    /* Proof rejection is not ordinary asynchronous cache loading. The caller
     * records this error transition without per-frame diagnostic spam. */
    SetLastError(result ? ERROR_SUCCESS : ERROR_INVALID_DATA);
    return result;
}

static void fill_visual_api(HMODULE module,
    SudekiMpLanArenaSpiritVfxVisualApi *api) {
    ZeroMemory(api, sizeof(*api)); fill_native_api(module, &api->cache);
    api->spawn = native_visual_spawn;
    api->synchronize = native_visual_synchronize;
    api->retire = native_visual_retire; api->detach = native_visual_detach;
}

BOOL SudekiMpLanArenaSpiritVfxServiceVisuals(HMODULE module,
    const SudekiMpLanArenaSnapshot *snapshot, uint64_t session_token) {
    SudekiMpLanArenaSpiritVfxVisualApi api;
    BOOL result;
    DWORD service_error;
    if (visual_quarantined ||
        !SudekiMpLanArenaSpiritVfxVisualImageMatches(module)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    fill_visual_api(module, &api);
    result = SudekiMpLanArenaSpiritVfxServiceVisualsWithApi(
        &production_visuals, snapshot, session_token, &api);
    if (visual_quarantined) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    service_error = result ? ERROR_SUCCESS : GetLastError();
    /* Session-wide fixed caches warm at most one resource per boundary.
     * Poll loading rows at 100ms; active instances can demand readiness sooner. */
    if ((result || service_error == ERROR_IO_PENDING ||
         service_error == ERROR_NOT_READY) &&
        production_visuals.session_token == session_token && session_token != 0u &&
        begin_operation()) {
        unsigned int attempt;
        DWORD now = GetTickCount();
        for (attempt = 0u; attempt < SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST; ++attempt) {
            unsigned int index = visual_prewarm_index++ %
                SUDEKIMP_LAN_ARENA_SPIRIT_VFX_LAST;
            SudekiMpLanArenaSpiritVfxCacheLease *lease = &production_visuals.caches[index];
            SudekiMpLanArenaSpiritVfxCacheState before = lease->state;
            if (visual_resources[index].name == NULL ||
                before == SUDEKIMP_SPIRIT_VFX_CACHE_READY ||
                before == SUDEKIMP_SPIRIT_VFX_CACHE_POISONED ||
                (before == SUDEKIMP_SPIRIT_VFX_CACHE_LOADING &&
                 (DWORD)(now - visual_prewarm_poll_at) < 100u)) continue;
            (void)prepare_named_locked(lease, &api.cache,
                visual_resources[index].name, visual_resources[index].identifier);
            if (before != lease->state)
                SudekiMpLogFormat("lan_arena_spirit_vfx event=visual_cache "
                    "kind=%u state=%u\r\n", index + 1u, (unsigned int)lease->state);
            if (before == SUDEKIMP_SPIRIT_VFX_CACHE_LOADING)
                visual_prewarm_poll_at = now;
            break;
        }
        end_operation(service_error);
    }
    return result;
}

BOOL SudekiMpLanArenaSpiritVfxResetVisuals(HMODULE module) {
    SudekiMpLanArenaSpiritVfxVisualApi api;
    if (visual_quarantined ||
        !SudekiMpLanArenaSpiritVfxVisualImageMatches(module)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    fill_visual_api(module, &api);
    return SudekiMpLanArenaSpiritVfxResetVisualsWithApi(&production_visuals, &api);
}
