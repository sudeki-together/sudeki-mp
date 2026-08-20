#include "cleanroom/engine.h"

#include "engine/log.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct SudekiMpResourceName {
    uint32_t encoded_kind;
    uint32_t identifier;
    uint32_t *text_reference;
} SudekiMpResourceName;

typedef void (__cdecl *InternalSpawnPcFunction)(
    const SudekiMpResourceName *resource_name,
    float x,
    float y,
    float z
);
typedef void (__cdecl *RemovePcFunction)(
    const SudekiMpResourceName *resource_name
);
typedef void (__cdecl *SpawnEntityFunction)(
    const char *resource_name,
    float x,
    float y,
    float z
);
typedef void (__cdecl *DespawnEntityFunction)(void *entity_pointer);
typedef void *(__cdecl *GetEntityFunction)(const char *resource_name);
typedef void *(__cdecl *GetGroupPlayersFunction)(void);
typedef unsigned char (__attribute__((thiscall)) *GroupPlayersInCombatFunction)(
    const void *group_players
);
typedef void (__attribute__((thiscall)) *GroupPlayersCombatTransitionFunction)(
    void *combat_event_sink,
    uintptr_t unused_event,
    uintptr_t unused_sender,
    BOOL enabled
);
typedef void (__cdecl *SetFirstPersonCameraModeFunction)(BOOL enabled);
typedef void (__cdecl *EnableResourceBypassFunction)(void);
typedef float (__cdecl *GetSspFunction)(void);
typedef void (__cdecl *SetSspFunction)(float value);
typedef void (__cdecl *FillInventoryFunction)(void);
typedef void (__cdecl *SpiritStrikeEnableFunction)(int identifier);
typedef float (__cdecl *GetCharacterNumberStatFunction)(
    const void *gel_pointer,
    const char *name
);
typedef unsigned char (__cdecl *SetCharacterNumberStatFunction)(
    const void *gel_pointer,
    const char *name,
    float value
);
typedef void (__attribute__((thiscall)) *SetWeaponFunction)(
    void *character_weapon,
    int item_identifier
);
typedef BOOL (__cdecl *GelPointerToEntityFunction)(
    void *gel_pointer,
    void *entity_pointer
);
typedef void (__attribute__((thiscall)) *EntityPointerCleanupFunction)(
    void *entity_pointer
);
typedef void *(__attribute__((thiscall)) *GelPointerDeletingDestructor)(
    void *gel_pointer,
    unsigned int flags
);

enum {
    RVA_INTERNAL_SPAWN_PC = 0x000b1b00u,
    RVA_REMOVE_PC = 0x000b23a0u,
    RVA_SPAWN_ENTITY = 0x000b20d0u,
    RVA_DESPAWN_ENTITY = 0x000b2300u,
    RVA_GET_PC_TEXT = 0x00104480u,
    RVA_GET_GENERIC_ENTITY_TEXT = 0x00104400u,
    RVA_ENTITY_MANAGER_GLOBAL = 0x00409d8cu,
    RVA_ENTITY_DIRECTORY_GLOBAL = 0x00409de4u,
    RVA_GEL_POINTER_VTABLE = 0x002c0098u,
    RVA_GEL_POINTER_TO_ENTITY = 0x001bf4e0u,
    RVA_ENTITY_POINTER_CLEANUP = 0x000015e0u,
    RVA_RESOURCE_NAME_FROM_TEXT = 0x001b9440u,
    RVA_RESOURCE_NAME_RELEASE_REFERENCE = 0x001b9760u,
    RVA_GET_GROUP_PLAYERS = 0x00025100u,
    RVA_GROUP_PLAYERS_IN_COMBAT = 0x00004fa0u,
    RVA_GROUP_PLAYERS_COMBAT_TRANSITION = 0x00024480u,
    RVA_GROUP_PLAYERS_COMBAT_SINK_VTABLE = 0x002c6d68u,
    RVA_GAMEPAD_CONTROL_GLOBAL = 0x00408da8u,
    RVA_SET_FIRST_PERSON_CAMERA_MODE = 0x0002a880u,
    RVA_SET_UI_ACTIVE = 0x0000afd0u,
    RVA_UI_MANAGER_GLOBAL = 0x00408d1cu,
    RVA_NO_SP_NEEDED = 0x000b5320u,
    RVA_NO_SSP_NEEDED = 0x0000f5b0u,
    RVA_GET_SSP = 0x0000f5e0u,
    RVA_SET_SSP = 0x0000f5c0u,
    RVA_FILL_INVENTORY = 0x000204d0u,
    RVA_SPIRIT_STRIKE_ENABLE = 0x000113a0u,
    RVA_GET_CHARACTER_NUMBER_STAT = 0x000c1270u,
    RVA_SET_CHARACTER_NUMBER_STAT = 0x000c1350u,
    RVA_SET_WEAPON = 0x000d8790u,
    RVA_NO_SP_NEEDED_FLAG = 0x003c2fccu,
    RVA_NO_SSP_NEEDED_FLAG = 0x003c2f23u,
    RVA_SPIRIT_STRIKE_MANAGER_GLOBAL = 0x00408d30u,
    RVA_ITEM_DATABASE_GLOBAL = 0x00408d80u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    CHARACTER_WEAPON_OFFSET = 0x00c0u,
    CHARACTER_WEAPON_CURRENT_ITEM_OFFSET = 0x0268u,
    SPIRIT_STRIKE_UNLOCKS_OFFSET = 0x00acu
};

static const uint8_t resource_name_from_text_entry[] = {
    0x57u, 0x8bu, 0xf8u, 0x8bu, 0x06u, 0x25u, 0x80u, 0xefu
};
static const uint8_t resource_name_release_entry[] = {
    0x85u, 0xc0u, 0x74u, 0x2fu, 0x53u, 0x8du, 0x58u, 0xfcu
};
static const uint8_t group_players_combat_transition_entry[] = {
    0x53u, 0x8bu, 0x5cu, 0x24u, 0x10u, 0x55u, 0x8bu, 0xe9u
};
static const uint8_t set_ui_active_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0xa1u, 0x94u
};

_Static_assert(
    sizeof(SudekiMpResourceName) == 12u,
    "Sudeki ResourceName ABI must remain 12 bytes"
);

static const char *const actor_labels[SUDEKIMP_CLEANROOM_ACTOR_COUNT] = {
    "Tal", "Buki", "Elco", "Ailish"
};
static const char *const actor_resources[SUDEKIMP_CLEANROOM_ACTOR_COUNT] = {
    "PC_Tal", "PC_Buki", "PC_Elco", "PC_Ailish"
};
/*
 * CCharacterWeapon::SetWeapon takes an index in inventory category 5, not
 * the global SOLData item ID. FillInventory orders that category by the
 * Ailish, Elco, Tal, then Buki weapon families. The matching starter slots
 * are therefore different from global item IDs 12, 24, 0, and 36.
 */
static const int actor_starter_weapon_slots[SUDEKIMP_CLEANROOM_ACTOR_COUNT] = {
    24, 36, 12, 0
};
/* SpawnEntity appends .SOL; this is the monster definition, not its group. */
static const char dummy_resource[] = "MON_TrainingDummy";

static uint8_t *game_base;
static InternalSpawnPcFunction internal_spawn_pc;
static RemovePcFunction remove_pc;
static SpawnEntityFunction spawn_entity;
static DespawnEntityFunction despawn_entity;
static GetEntityFunction get_pc;
static GetEntityFunction get_generic_entity;
static GetGroupPlayersFunction get_group_players;
static GroupPlayersInCombatFunction group_players_in_combat;
static GroupPlayersCombatTransitionFunction group_players_combat_transition;
static SetFirstPersonCameraModeFunction set_first_person_camera_mode;
static EnableResourceBypassFunction no_sp_needed;
static EnableResourceBypassFunction no_ssp_needed;
static GetSspFunction get_ssp;
static SetSspFunction set_ssp;
static FillInventoryFunction fill_inventory;
static SpiritStrikeEnableFunction spirit_strike_enable;
static GetCharacterNumberStatFunction get_character_number_stat;
static SetCharacterNumberStatFunction set_character_number_stat;
static SetWeaponFunction set_weapon;
static GelPointerToEntityFunction gel_pointer_to_entity;
static EntityPointerCleanupFunction entity_pointer_cleanup;
static void *resource_name_from_text;
static void *resource_name_release_reference;
static BOOL ranged_prime_pending;
static BOOL ranged_prime_ui_active;
static UINT_PTR ranged_prime_timer;
static uint8_t *no_sp_needed_flag;
static uint8_t *no_ssp_needed_flag;
static uint8_t saved_no_sp_needed;
static uint8_t saved_no_ssp_needed;
static BOOL resource_flags_captured;
static BOOL inventory_filled;
static BOOL spirit_strikes_unlocked;
static BOOL spirit_strike_unlocks_captured;
static uint8_t saved_spirit_strike_unlocks;
static void *saved_spirit_strike_manager;
static void *initialized_actor_entities[SUDEKIMP_CLEANROOM_ACTOR_COUNT];

static uint32_t float_bits(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
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
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL writable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    DWORD writable;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    writable = information.Protect & 0xffu;
    if (writable != PAGE_READWRITE && writable != PAGE_WRITECOPY &&
        writable != PAGE_EXECUTE_READWRITE &&
        writable != PAGE_EXECUTE_WRITECOPY) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL resolve_exact_export(
    HMODULE game_module,
    const char *name,
    uintptr_t expected_rva,
    void **result
) {
    FARPROC address;

    address = GetProcAddress(game_module, name);
    if (address == NULL ||
        (uintptr_t)((uint8_t *)(uintptr_t)address -
            (uint8_t *)game_module) != expected_rva) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    *result = (void *)(uintptr_t)address;
    return TRUE;
}

static BOOL matches_entry(
    const uint8_t *address,
    const uint8_t *expected,
    size_t size
) {
    return readable_memory(address, size) &&
        memcmp(address, expected, size) == 0;
}

/*
 * RVA 0x0000AFD0 is not a normal cdecl function. Sudeki expects its UI
 * manager in ESI and the active flag on the stack. This is the same native
 * transition used by the ranged-readiness experiment; keeping it here lets
 * the cleanroom arm ranged actors without opening the menu or writing
 * arbiter/animation state directly.
 */
static BOOL set_native_ui_active(BOOL active) {
    void *ui_manager;
    void *function;

    if (game_base == NULL ||
        !matches_entry(
            game_base + RVA_SET_UI_ACTIVE,
            set_ui_active_entry,
            sizeof(set_ui_active_entry)
        ) ||
        !readable_memory(
            game_base + RVA_UI_MANAGER_GLOBAL,
            sizeof(void *)
        )) {
        return FALSE;
    }
    ui_manager = *(void **)(game_base + RVA_UI_MANAGER_GLOBAL);
    if (!readable_memory(ui_manager, 1u)) {
        return FALSE;
    }
    function = game_base + RVA_SET_UI_ACTIVE;
    __asm__ volatile(
        "movl %0, %%esi\n\t"
        "pushl %1\n\t"
        "call *%2"
        :
        : "r"(ui_manager), "r"(active), "r"(function)
        : "eax", "ecx", "edx", "esi", "memory", "cc"
    );
    return TRUE;
}

static void cancel_ranged_prime(void) {
    if (ranged_prime_timer != 0u) {
        KillTimer(NULL, ranged_prime_timer);
        ranged_prime_timer = 0u;
    }
    if (ranged_prime_ui_active) {
        (void)set_native_ui_active(FALSE);
        ranged_prime_ui_active = FALSE;
    }
    ranged_prime_pending = FALSE;
}

static void CALLBACK complete_ranged_prime(
    HWND window,
    UINT message,
    UINT_PTR timer_id,
    DWORD time
) {
    BOOL combat_enabled = FALSE;

    (void)window;
    (void)message;
    (void)time;
    KillTimer(NULL, timer_id);
    if (ranged_prime_timer == timer_id) {
        ranged_prime_timer = 0u;
    }
    ranged_prime_pending = FALSE;
    if (!SudekiMpCleanroomEngineCombatMode(&combat_enabled) ||
        !combat_enabled) {
        (void)set_native_ui_active(FALSE);
        ranged_prime_ui_active = FALSE;
        SudekiMpLogWrite(
            "cleanroom_engine event=ranged_combat_prime phase=exit "
            "status=cancelled reason=combat_disabled\r\n"
        );
        return;
    }
    if (!set_native_ui_active(FALSE)) {
        ranged_prime_ui_active = FALSE;
        SudekiMpLogWrite(
            "cleanroom_engine event=ranged_combat_prime phase=exit "
            "status=rejected reason=native_ui_transition_unavailable\r\n"
        );
        return;
    }
    ranged_prime_ui_active = FALSE;
    SudekiMpLogWrite(
        "cleanroom_engine event=ranged_combat_prime phase=exit "
        "status=confirmed state=idle_armed policy=native_ui_cycle\r\n"
    );
}

/*
 * ResourceName uses an internal 12-byte, reference-backed representation.
 * The supported build's constructor uses its original register convention:
 * ESI=destination, EAX=text, ECX=resource kind. Kind 0x7f asks Sudeki to
 * resolve the textual resource name when the public API consumes it.
 */
static BOOL initialize_resource_name(
    SudekiMpResourceName *resource_name,
    const char *text
) {
    SudekiMpResourceName *result;
    uintptr_t kind = 0x7fu;

    if (resource_name == NULL || text == NULL || text[0] == '\0' ||
        resource_name_from_text == NULL) {
        return FALSE;
    }
    ZeroMemory(resource_name, sizeof(*resource_name));
    __asm__ volatile(
        "call *%[function]"
        : "=a"(result), "+c"(kind)
        : "0"(text), "S"(resource_name),
          [function] "r"(resource_name_from_text)
        : "edx", "memory", "cc"
    );
    if (result != resource_name || resource_name->text_reference == NULL ||
        !readable_memory(resource_name->text_reference, 2u * sizeof(uint32_t)) ||
        resource_name->text_reference[0] == 0u ||
        resource_name->text_reference[1] == 0u) {
        return FALSE;
    }
    return TRUE;
}

static void release_resource_name(SudekiMpResourceName *resource_name) {
    uint32_t *reference;

    if (resource_name == NULL) {
        return;
    }
    reference = resource_name->text_reference;
    resource_name->text_reference = NULL;
    if (reference != NULL &&
        readable_memory(reference, 2u * sizeof(uint32_t)) &&
        reference[0] > 0u) {
        --reference[0];
        if (reference[0] == 0u && resource_name_release_reference != NULL) {
            __asm__ volatile(
                "call *%[function]"
                : "+a"(reference)
                : [function] "r"(resource_name_release_reference)
                : "ecx", "edx", "memory", "cc"
            );
        }
    }
    ZeroMemory(resource_name, sizeof(*resource_name));
}

static void *entity_from_gel_pointer(void *gel_pointer) {
    void **vtable;
    void *tracked_entity[3] = {NULL, NULL, NULL};
    void *entity = NULL;
    GelPointerDeletingDestructor destroy;

    if (!readable_memory(gel_pointer, 0x10u)) {
        return NULL;
    }
    vtable = *(void ***)gel_pointer;
    if (vtable != (void **)(game_base + RVA_GEL_POINTER_VTABLE) ||
        !readable_memory(vtable, sizeof(void *)) || vtable[0] == NULL) {
        return NULL;
    }
    if (gel_pointer_to_entity != NULL && entity_pointer_cleanup != NULL &&
        gel_pointer_to_entity(gel_pointer, tracked_entity)) {
        entity = tracked_entity[0];
    }
    entity_pointer_cleanup(tracked_entity);
    destroy = (GelPointerDeletingDestructor)vtable[0];
    destroy(gel_pointer, 1u);
    return entity;
}

static void *lookup_entity(
    GetEntityFunction lookup,
    const char *resource_name
) {
    if (lookup == NULL || resource_name == NULL ||
        !SudekiMpCleanroomEngineWorldReady()) {
        return NULL;
    }
    return entity_from_gel_pointer(lookup(resource_name));
}

static void *actor_pointer(SudekiMpCleanroomActor actor) {
    if (get_pc == NULL || !SudekiMpCleanroomEngineWorldReady() || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) {
        return NULL;
    }
    return lookup_entity(get_pc, actor_resources[actor]);
}

void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    return actor_pointer(actor);
}

static BOOL prepare_training_inventory(void) {
    void **inventory_global;
    void **item_database_global;

    if (inventory_filled) {
        return TRUE;
    }
    if (game_base == NULL || fill_inventory == NULL) {
        return FALSE;
    }
    inventory_global = (void **)(game_base + RVA_INVENTORY_GLOBAL);
    item_database_global = (void **)(game_base + RVA_ITEM_DATABASE_GLOBAL);
    if (!readable_memory(inventory_global, sizeof(*inventory_global)) ||
        !readable_memory(item_database_global, sizeof(*item_database_global)) ||
        !readable_memory(*inventory_global, 0x131u) ||
        !readable_memory(*item_database_global, 0x1000u)) {
        return FALSE;
    }
    fill_inventory();
    inventory_filled = TRUE;
    SudekiMpLogWrite(
        "cleanroom_engine event=inventory_fill status=complete "
        "method=native_developer_function scope=all_items\r\n"
    );
    return TRUE;
}

static BOOL prepare_spirit_strikes(void) {
    void **manager_global;
    uint8_t *manager;

    if (spirit_strikes_unlocked) {
        return TRUE;
    }
    if (game_base == NULL || spirit_strike_enable == NULL) {
        return FALSE;
    }
    manager_global =
        (void **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
    if (!readable_memory(manager_global, sizeof(*manager_global))) {
        return FALSE;
    }
    manager = (uint8_t *)*manager_global;
    if (manager == NULL || !writable_memory(
            manager + SPIRIT_STRIKE_UNLOCKS_OFFSET,
            sizeof(manager[SPIRIT_STRIKE_UNLOCKS_OFFSET]))) {
        return FALSE;
    }
    if (!spirit_strike_unlocks_captured) {
        saved_spirit_strike_manager = manager;
        saved_spirit_strike_unlocks =
            manager[SPIRIT_STRIKE_UNLOCKS_OFFSET];
        spirit_strike_unlocks_captured = TRUE;
    }
    spirit_strike_enable(-1);
    if (manager[SPIRIT_STRIKE_UNLOCKS_OFFSET] != 0xffu) {
        return FALSE;
    }
    spirit_strikes_unlocked = TRUE;
    SudekiMpLogFormat(
        "cleanroom_engine event=spirit_strikes status=unlocked_all "
        "previous_mask=0x%02lx current_mask=0xff\r\n",
        (unsigned long)saved_spirit_strike_unlocks
    );
    return TRUE;
}

static BOOL repair_actor_stat_maxima(SudekiMpCleanroomActor actor) {
    void *gel_pointer;
    float hit_points;
    float maximum_hit_points;
    float skill_points;
    float maximum_skill_points;
    BOOL repaired_hit_points = FALSE;
    BOOL repaired_skill_points = FALSE;
    BOOL ready;

    if (get_pc == NULL || get_character_number_stat == NULL ||
        set_character_number_stat == NULL || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT ||
        !SudekiMpCleanroomEngineWorldReady()) {
        return FALSE;
    }
    gel_pointer = get_pc(actor_resources[actor]);
    if (!readable_memory(gel_pointer, 0x10u)) {
        return FALSE;
    }
    hit_points = get_character_number_stat(gel_pointer, "HitPoints");
    maximum_hit_points = get_character_number_stat(
        gel_pointer,
        "Maximum HitPoints"
    );
    skill_points = get_character_number_stat(gel_pointer, "SkillPoints");
    maximum_skill_points = get_character_number_stat(
        gel_pointer,
        "Maximum SkillPoints"
    );
    if (isfinite(hit_points) && hit_points > 0.0f &&
        (!isfinite(maximum_hit_points) || maximum_hit_points <= 0.0f)) {
        repaired_hit_points = set_character_number_stat(
            gel_pointer,
            "Maximum HitPoints",
            hit_points
        ) != 0u;
        if (repaired_hit_points) {
            maximum_hit_points = hit_points;
        }
    }
    if (isfinite(skill_points) && skill_points > 0.0f &&
        (!isfinite(maximum_skill_points) || maximum_skill_points <= 0.0f)) {
        repaired_skill_points = set_character_number_stat(
            gel_pointer,
            "Maximum SkillPoints",
            skill_points
        ) != 0u;
        if (repaired_skill_points) {
            maximum_skill_points = skill_points;
        }
    }
    ready = isfinite(maximum_hit_points) && maximum_hit_points > 0.0f &&
        isfinite(maximum_skill_points) && maximum_skill_points > 0.0f;
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_stats actor=%s status=%s "
        "hp_bits=%08lx max_hp_bits=%08lx sp_bits=%08lx "
        "max_sp_bits=%08lx repaired_hp=%s repaired_sp=%s\r\n",
        actor_labels[actor],
        ready ? "ready" : "pending",
        (unsigned long)float_bits(hit_points),
        (unsigned long)float_bits(maximum_hit_points),
        (unsigned long)float_bits(skill_points),
        (unsigned long)float_bits(maximum_skill_points),
        repaired_hit_points ? "true" : "false",
        repaired_skill_points ? "true" : "false"
    );
    (void)entity_from_gel_pointer(gel_pointer);
    return ready;
}

static BOOL initialize_actor_weapon(
    SudekiMpCleanroomActor actor,
    uint8_t *character
) {
    uint8_t *character_weapon;
    void *current_item;

    if (!inventory_filled || set_weapon == NULL ||
        !readable_memory(character, CHARACTER_WEAPON_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    character_weapon = *(uint8_t **)(character + CHARACTER_WEAPON_OFFSET);
    if (character_weapon == NULL || !readable_memory(
            character_weapon,
            CHARACTER_WEAPON_CURRENT_ITEM_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    current_item = *(void **)(
        character_weapon + CHARACTER_WEAPON_CURRENT_ITEM_OFFSET
    );
    if (current_item != NULL) {
        return TRUE;
    }
    set_weapon(character_weapon, actor_starter_weapon_slots[actor]);
    current_item = *(void **)(
        character_weapon + CHARACTER_WEAPON_CURRENT_ITEM_OFFSET
    );
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_weapon actor=%s inventory_slot=%d status=%s\r\n",
        actor_labels[actor],
        actor_starter_weapon_slots[actor],
        current_item != NULL ? "equipped" : "pending"
    );
    return current_item != NULL;
}

static BOOL invoke_combat_transition(
    BOOL enabled,
    BOOL force,
    const char *reason,
    const char *actor_label
) {
    uint8_t *group_players;
    void **combat_event_sink;
    BOOL current;

    enabled = enabled != FALSE;
    if (group_players_combat_transition == NULL ||
        !SudekiMpCleanroomEngineCombatMode(&current)) {
        return FALSE;
    }
    if (!force && current == enabled) {
        return TRUE;
    }
    group_players = (uint8_t *)get_group_players();
    combat_event_sink = (void **)(group_players + 0x44u);
    if (!readable_memory(combat_event_sink, sizeof(*combat_event_sink)) ||
        *combat_event_sink !=
            (void *)(game_base + RVA_GROUP_PLAYERS_COMBAT_SINK_VTABLE)) {
        SudekiMpLogWrite(
            "cleanroom_engine event=combat_mode status=rejected "
            "reason=event_sink_mismatch\r\n"
        );
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=combat_mode phase=%s state=%s actor=%s "
        "reason=%s\r\n",
        force ? "refresh" : "begin",
        enabled ? "enabled" : "disabled",
        actor_label == NULL ? "none" : actor_label,
        reason == NULL ? "unspecified" : reason
    );
    group_players_combat_transition(
        combat_event_sink,
        0u,
        0u,
        enabled
    );
    if (!SudekiMpCleanroomEngineCombatMode(&current) || current != enabled) {
        SudekiMpLogWrite(
            "cleanroom_engine event=combat_mode status=rejected "
            "reason=transition_not_confirmed\r\n"
        );
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=combat_mode status=confirmed state=%s "
        "phase=%s actor=%s\r\n",
        enabled ? "enabled" : "disabled",
        force ? "refresh" : "begin",
        actor_label == NULL ? "none" : actor_label
    );
    return TRUE;
}

static void initialize_present_actors(void) {
    unsigned int index;
    uint8_t *character;
    BOOL stats_ready;
    BOOL weapon_ready;
    BOOL combat_enabled;

    for (index = 0u; index < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++index) {
        character = (uint8_t *)actor_pointer((SudekiMpCleanroomActor)index);
        if (character == NULL) {
            initialized_actor_entities[index] = NULL;
            continue;
        }
        if (initialized_actor_entities[index] == character) {
            continue;
        }
        stats_ready = repair_actor_stat_maxima(
            (SudekiMpCleanroomActor)index
        );
        weapon_ready = initialize_actor_weapon(
            (SudekiMpCleanroomActor)index,
            character
        );
        if (stats_ready && weapon_ready) {
            initialized_actor_entities[index] = character;
            SudekiMpLogFormat(
                "cleanroom_engine event=actor_training_setup actor=%s "
                "status=complete entity=%p\r\n",
                actor_labels[index],
                character
            );
            /*
             * A PC spawned after the group entered combat is not included in
             * the already-completed native arm pass.  Re-run that native pass
             * once for this newly initialized actor; never toggle combat or
             * synthesize the actor's combat state locally.
             */
            if (SudekiMpCleanroomEngineCombatMode(&combat_enabled) &&
                combat_enabled) {
                (void)invoke_combat_transition(
                    TRUE,
                    TRUE,
                    "new_actor_initialized_during_combat",
                    actor_labels[index]
                );
            }
        }
    }
}

BOOL SudekiMpCleanroomEngineWorldReady(void) {
    void **entity_manager;
    void **entity_directory;

    if (game_base == NULL) {
        return FALSE;
    }
    entity_manager = (void **)(game_base + RVA_ENTITY_MANAGER_GLOBAL);
    entity_directory = (void **)(game_base + RVA_ENTITY_DIRECTORY_GLOBAL);
    return readable_memory(entity_manager, sizeof(*entity_manager)) &&
        readable_memory(entity_directory, sizeof(*entity_directory)) &&
        readable_memory(*entity_manager, 0x38u) &&
        readable_memory(*entity_directory, 0xf0u);
}

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor) {
    if (actor < 0 || actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) {
        return "Unknown";
    }
    return actor_labels[actor];
}

const char *SudekiMpCleanroomActorResource(SudekiMpCleanroomActor actor) {
    if (actor < 0 || actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) {
        return NULL;
    }
    return actor_resources[actor];
}

BOOL SudekiMpCleanroomEngineInitialize(HMODULE game_module) {
    void *resolved_internal_spawn_pc = NULL;
    void *resolved_remove_pc = NULL;
    void *resolved_spawn_entity = NULL;
    void *resolved_despawn_entity = NULL;
    void *resolved_get_pc = NULL;
    void *resolved_get_generic_entity = NULL;
    void *resolved_get_group_players = NULL;
    void *resolved_group_players_in_combat = NULL;
    void *resolved_set_first_person_camera_mode = NULL;
    void *resolved_no_sp_needed = NULL;
    void *resolved_no_ssp_needed = NULL;
    void *resolved_get_ssp = NULL;
    void *resolved_set_ssp = NULL;
    void *resolved_fill_inventory = NULL;
    void *resolved_spirit_strike_enable = NULL;
    void *resolved_get_character_number_stat = NULL;
    void *resolved_set_character_number_stat = NULL;
    void *resolved_set_weapon = NULL;
    uint8_t *base;

    if (game_module == NULL || game_base != NULL ||
        !resolve_exact_export(
            game_module,
            "?InternalSpawnPC@@YAXABVResourceName@@MMM@Z",
            RVA_INTERNAL_SPAWN_PC,
            &resolved_internal_spawn_pc) ||
        !resolve_exact_export(
            game_module,
            "?RemovePC@@YAXABVResourceName@@@Z",
            RVA_REMOVE_PC,
            &resolved_remove_pc) ||
        !resolve_exact_export(
            game_module,
            "?SpawnEntity@@YAXPBDMMM@Z",
            RVA_SPAWN_ENTITY,
            &resolved_spawn_entity) ||
        !resolve_exact_export(
            game_module,
            "?DespawnEntity@@YAXPAV?$TPtr@VEntity@@@@@Z",
            RVA_DESPAWN_ENTITY,
            &resolved_despawn_entity) ||
        !resolve_exact_export(
            game_module,
            "?GetPC@@YAPAVGELPointer@@QBD@Z",
            RVA_GET_PC_TEXT,
            &resolved_get_pc) ||
        !resolve_exact_export(
            game_module,
            "?GetGenericEntity@@YAPAVGELPointer@@QBD@Z",
            RVA_GET_GENERIC_ENTITY_TEXT,
            &resolved_get_generic_entity) ||
        !resolve_exact_export(
            game_module,
            "?GetGroupPlayers@@YAPAVCGroupPlayers@@XZ",
            RVA_GET_GROUP_PLAYERS,
            &resolved_get_group_players) ||
        !resolve_exact_export(
            game_module,
            "?InCombat@CGroupPlayers@@QBE_NXZ",
            RVA_GROUP_PLAYERS_IN_COMBAT,
            &resolved_group_players_in_combat) ||
        !resolve_exact_export(
            game_module,
            "?SetFirstPersonCameraMode@@YAX_N@Z",
            RVA_SET_FIRST_PERSON_CAMERA_MODE,
            &resolved_set_first_person_camera_mode) ||
        !resolve_exact_export(
            game_module,
            "?NoSpNeeded@@YAXXZ",
            RVA_NO_SP_NEEDED,
            &resolved_no_sp_needed) ||
        !resolve_exact_export(
            game_module,
            "?NoSspNeeded@@YAXXZ",
            RVA_NO_SSP_NEEDED,
            &resolved_no_ssp_needed) ||
        !resolve_exact_export(
            game_module,
            "?GetSsp@@YAMXZ",
            RVA_GET_SSP,
            &resolved_get_ssp) ||
        !resolve_exact_export(
            game_module,
            "?SetSsp@@YAXM@Z",
            RVA_SET_SSP,
            &resolved_set_ssp) ||
        !resolve_exact_export(
            game_module,
            "?FillInventory@@YAXXZ",
            RVA_FILL_INVENTORY,
            &resolved_fill_inventory) ||
        !resolve_exact_export(
            game_module,
            "?SpiritStrikeEnable@@YAXH@Z",
            RVA_SPIRIT_STRIKE_ENABLE,
            &resolved_spirit_strike_enable) ||
        !resolve_exact_export(
            game_module,
            "?GetCharacterNumberStat@@YAMPBVGELPointer@@QBD@Z",
            RVA_GET_CHARACTER_NUMBER_STAT,
            &resolved_get_character_number_stat) ||
        !resolve_exact_export(
            game_module,
            "?SetCharacterNumberStat@@YA_NPBVGELPointer@@QBDM@Z",
            RVA_SET_CHARACTER_NUMBER_STAT,
            &resolved_set_character_number_stat) ||
        !resolve_exact_export(
            game_module,
            "?SetWeapon@CCharacterWeapon@@QAEXH@Z",
            RVA_SET_WEAPON,
            &resolved_set_weapon) ||
        !matches_entry(
            (const uint8_t *)game_module + RVA_RESOURCE_NAME_FROM_TEXT,
            resource_name_from_text_entry,
            sizeof(resource_name_from_text_entry)) ||
        !matches_entry(
            (const uint8_t *)game_module +
                RVA_RESOURCE_NAME_RELEASE_REFERENCE,
            resource_name_release_entry,
            sizeof(resource_name_release_entry)) ||
        !matches_entry(
            (const uint8_t *)game_module +
                RVA_GROUP_PLAYERS_COMBAT_TRANSITION,
            group_players_combat_transition_entry,
            sizeof(group_players_combat_transition_entry)) ||
        !matches_entry(
            (const uint8_t *)game_module + RVA_SET_UI_ACTIVE,
            set_ui_active_entry,
            sizeof(set_ui_active_entry))) {
        SudekiMpCleanroomEngineReset();
        return FALSE;
    }

    base = (uint8_t *)game_module;
    if (!writable_memory(base + RVA_NO_SP_NEEDED_FLAG, 1u) ||
        !writable_memory(base + RVA_NO_SSP_NEEDED_FLAG, 1u)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    game_base = base;
    internal_spawn_pc = (InternalSpawnPcFunction)resolved_internal_spawn_pc;
    remove_pc = (RemovePcFunction)resolved_remove_pc;
    spawn_entity = (SpawnEntityFunction)resolved_spawn_entity;
    despawn_entity = (DespawnEntityFunction)resolved_despawn_entity;
    get_pc = (GetEntityFunction)resolved_get_pc;
    get_generic_entity = (GetEntityFunction)resolved_get_generic_entity;
    get_group_players =
        (GetGroupPlayersFunction)resolved_get_group_players;
    group_players_in_combat =
        (GroupPlayersInCombatFunction)resolved_group_players_in_combat;
    group_players_combat_transition =
        (GroupPlayersCombatTransitionFunction)(
            game_base + RVA_GROUP_PLAYERS_COMBAT_TRANSITION
        );
    set_first_person_camera_mode =
        (SetFirstPersonCameraModeFunction)
            resolved_set_first_person_camera_mode;
    no_sp_needed = (EnableResourceBypassFunction)resolved_no_sp_needed;
    no_ssp_needed = (EnableResourceBypassFunction)resolved_no_ssp_needed;
    get_ssp = (GetSspFunction)resolved_get_ssp;
    set_ssp = (SetSspFunction)resolved_set_ssp;
    fill_inventory = (FillInventoryFunction)resolved_fill_inventory;
    spirit_strike_enable =
        (SpiritStrikeEnableFunction)resolved_spirit_strike_enable;
    get_character_number_stat =
        (GetCharacterNumberStatFunction)resolved_get_character_number_stat;
    set_character_number_stat =
        (SetCharacterNumberStatFunction)resolved_set_character_number_stat;
    set_weapon = (SetWeaponFunction)resolved_set_weapon;
    gel_pointer_to_entity = (GelPointerToEntityFunction)(
        game_base + RVA_GEL_POINTER_TO_ENTITY
    );
    entity_pointer_cleanup = (EntityPointerCleanupFunction)(
        game_base + RVA_ENTITY_POINTER_CLEANUP
    );
    resource_name_from_text = game_base + RVA_RESOURCE_NAME_FROM_TEXT;
    resource_name_release_reference =
        game_base + RVA_RESOURCE_NAME_RELEASE_REFERENCE;
    no_sp_needed_flag = game_base + RVA_NO_SP_NEEDED_FLAG;
    no_ssp_needed_flag = game_base + RVA_NO_SSP_NEEDED_FLAG;
    saved_no_sp_needed = *no_sp_needed_flag;
    saved_no_ssp_needed = *no_ssp_needed_flag;
    resource_flags_captured = TRUE;
    SudekiMpLogWrite(
        "cleanroom_engine event=initialize status=success "
        "player_spawn_rva=0x000b1b00 player_remove_rva=0x000b23a0 "
        "entity_spawn_rva=0x000b20d0 entity_despawn_rva=0x000b2300 "
        "resource_name_rva=0x001b9440 resource_name_size=12 "
        "combat_transition_rva=0x00024480 "
        "ranged_ui_transition_rva=0x0000afd0 "
        "first_person_camera_rva=0x0002a880 "
        "no_sp_needed_rva=0x000b5320 no_ssp_needed_rva=0x0000f5b0 "
        "fill_inventory_rva=0x000204d0 "
        "spirit_strike_enable_rva=0x000113a0 "
        "character_stats_rvas=0x000c1270,0x000c1350 "
        "set_weapon_rva=0x000d8790\r\n"
    );
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor) {
    return actor_pointer(actor) != NULL;
}

BOOL SudekiMpCleanroomEngineActorPosition(
    SudekiMpCleanroomActor actor,
    float position[3]
) {
    uint8_t *character;
    uint8_t *transform;

    if (position == NULL) {
        return FALSE;
    }
    character = (uint8_t *)actor_pointer(actor);
    if (!readable_memory(character, 0x48u)) {
        return FALSE;
    }
    transform = *(uint8_t **)(character + 0x44u);
    if (!readable_memory(transform, 0x24u)) {
        return FALSE;
    }
    position[0] = *(float *)(transform + 0x18u);
    position[1] = *(float *)(transform + 0x1cu);
    position[2] = *(float *)(transform + 0x20u);
    return isfinite(position[0]) && isfinite(position[1]) &&
        isfinite(position[2]) && fabsf(position[0]) < 1000000.0f &&
        fabsf(position[1]) < 1000000.0f &&
        fabsf(position[2]) < 1000000.0f;
}

BOOL SudekiMpCleanroomEngineSpawnActor(
    SudekiMpCleanroomActor actor,
    const float position[3]
) {
    SudekiMpResourceName resource_name;

    if (internal_spawn_pc == NULL || position == NULL || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT ||
        SudekiMpCleanroomEngineActorPresent(actor) ||
        !initialize_resource_name(&resource_name, actor_resources[actor])) {
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_spawn phase=begin actor=%s "
        "encoded_kind=0x%08lx identifier=0x%08lx refcount=%lu\r\n",
        actor_labels[actor],
        (unsigned long)resource_name.encoded_kind,
        (unsigned long)resource_name.identifier,
        (unsigned long)resource_name.text_reference[0]
    );
    internal_spawn_pc(
        &resource_name,
        position[0],
        position[1],
        position[2]
    );
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_spawn phase=returned actor=%s\r\n",
        actor_labels[actor]
    );
    release_resource_name(&resource_name);
    return TRUE;
}

BOOL SudekiMpCleanroomEngineRemoveActor(SudekiMpCleanroomActor actor) {
    SudekiMpResourceName resource_name;

    if (remove_pc == NULL || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT ||
        !SudekiMpCleanroomEngineActorPresent(actor) ||
        !initialize_resource_name(&resource_name, actor_resources[actor])) {
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_remove phase=begin actor=%s\r\n",
        actor_labels[actor]
    );
    remove_pc(&resource_name);
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_remove phase=returned actor=%s\r\n",
        actor_labels[actor]
    );
    release_resource_name(&resource_name);
    return TRUE;
}

BOOL SudekiMpCleanroomEngineDummyPresent(void) {
    return lookup_entity(get_generic_entity, dummy_resource) != NULL;
}

BOOL SudekiMpCleanroomEngineSpawnDummy(const float position[3]) {
    if (spawn_entity == NULL || position == NULL ||
        SudekiMpCleanroomEngineDummyPresent()) {
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=dummy_spawn phase=begin resource=%s "
        "position_bits=%08lx,%08lx,%08lx\r\n",
        dummy_resource,
        (unsigned long)float_bits(position[0]),
        (unsigned long)float_bits(position[1]),
        (unsigned long)float_bits(position[2])
    );
    spawn_entity(
        dummy_resource,
        position[0],
        position[1],
        position[2]
    );
    SudekiMpLogFormat(
        "cleanroom_engine event=dummy_spawn phase=returned resource=%s\r\n",
        dummy_resource
    );
    return TRUE;
}

BOOL SudekiMpCleanroomEngineRemoveDummy(void) {
    void *entity;

    if (despawn_entity == NULL || get_generic_entity == NULL ||
        !SudekiMpCleanroomEngineWorldReady()) {
        return FALSE;
    }
    entity = lookup_entity(get_generic_entity, dummy_resource);
    if (entity == NULL) {
        return FALSE;
    }
    despawn_entity(&entity);
    return TRUE;
}

BOOL SudekiMpCleanroomEngineCombatMode(BOOL *enabled) {
    void *group_players;

    if (enabled == NULL || get_group_players == NULL ||
        group_players_in_combat == NULL ||
        !SudekiMpCleanroomEngineWorldReady()) {
        return FALSE;
    }
    group_players = get_group_players();
    if (!readable_memory(group_players, 0xd5u)) {
        return FALSE;
    }
    *enabled = group_players_in_combat(group_players) != 0u;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetCombatMode(BOOL enabled) {
    BOOL accepted = invoke_combat_transition(
        enabled,
        FALSE,
        NULL,
        NULL
    );
    if (!enabled) {
        cancel_ranged_prime();
    } else if (accepted) {
        (void)SudekiMpCleanroomEnginePrimeRangedCombat();
    }
    return accepted;
}

BOOL SudekiMpCleanroomEngineRefreshCombatMode(void) {
    BOOL enabled;

    if (!SudekiMpCleanroomEngineCombatMode(&enabled) || !enabled) {
        return FALSE;
    }
    if (!invoke_combat_transition(
        TRUE,
        TRUE,
        "second_player_control_override",
        NULL
    )) {
        return FALSE;
    }
    (void)SudekiMpCleanroomEnginePrimeRangedCombat();
    return TRUE;
}

BOOL SudekiMpCleanroomEnginePrimeRangedCombat(void) {
    if (game_base == NULL || ranged_prime_pending ||
        ranged_prime_ui_active) {
        return ranged_prime_pending || ranged_prime_ui_active;
    }
    if (!set_native_ui_active(TRUE)) {
        SudekiMpLogWrite(
            "cleanroom_engine event=ranged_combat_prime phase=enter "
            "status=rejected reason=native_ui_transition_unavailable\r\n"
        );
        return FALSE;
    }
    ranged_prime_ui_active = TRUE;
    ranged_prime_pending = TRUE;
    SudekiMpLogWrite(
        "cleanroom_engine event=ranged_combat_prime phase=enter "
        "status=confirmed state=ui_active delay_ms=75 "
        "policy=native_ui_cycle\r\n"
    );
    ranged_prime_timer = SetTimer(NULL, 0u, 75u, complete_ranged_prime);
    if (ranged_prime_timer == 0u) {
        cancel_ranged_prime();
        SudekiMpLogWrite(
            "cleanroom_engine event=ranged_combat_prime phase=schedule "
            "status=rejected reason=timer_error\r\n"
        );
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpCleanroomEngineFirstPersonMode(BOOL *enabled) {
    uint8_t **controller_global;
    uint8_t *controller;

    if (enabled == NULL || game_base == NULL) {
        return FALSE;
    }
    controller_global =
        (uint8_t **)(game_base + RVA_GAMEPAD_CONTROL_GLOBAL);
    if (!readable_memory(controller_global, sizeof(*controller_global))) {
        return FALSE;
    }
    controller = *controller_global;
    if (!readable_memory(controller, 0xa1u)) {
        return FALSE;
    }
    *enabled = (controller[0xa0u] & 0x01u) != 0u;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetFirstPersonMode(BOOL enabled) {
    BOOL current;

    enabled = enabled != FALSE;
    if (set_first_person_camera_mode == NULL ||
        !SudekiMpCleanroomEngineFirstPersonMode(&current)) {
        return FALSE;
    }
    if (current == enabled) {
        return TRUE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=camera_mode phase=begin state=%s\r\n",
        enabled ? "first_person" : "third_person"
    );
    set_first_person_camera_mode(enabled);
    if (!SudekiMpCleanroomEngineFirstPersonMode(&current) ||
        current != enabled) {
        SudekiMpLogWrite(
            "cleanroom_engine event=camera_mode status=rejected "
            "reason=transition_not_confirmed\r\n"
        );
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=camera_mode status=confirmed state=%s\r\n",
        enabled ? "first_person" : "third_person"
    );
    return TRUE;
}

BOOL SudekiMpCleanroomEngineInfiniteSp(BOOL *enabled) {
    if (enabled == NULL || !resource_flags_captured ||
        !readable_memory(no_sp_needed_flag, 1u)) {
        return FALSE;
    }
    *enabled = *no_sp_needed_flag != 0u;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetInfiniteSp(BOOL enabled) {
    BOOL current;

    enabled = enabled != FALSE;
    if (no_sp_needed == NULL || !resource_flags_captured ||
        !writable_memory(no_sp_needed_flag, 1u)) {
        return FALSE;
    }
    if (enabled) {
        no_sp_needed();
    } else {
        *no_sp_needed_flag = 0u;
    }
    if (!SudekiMpCleanroomEngineInfiniteSp(&current) || current != enabled) {
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=infinite_sp status=confirmed state=%s\r\n",
        enabled ? "enabled" : "disabled"
    );
    return TRUE;
}

BOOL SudekiMpCleanroomEngineInfiniteSpirit(BOOL *enabled) {
    if (enabled == NULL || !resource_flags_captured ||
        !readable_memory(no_ssp_needed_flag, 1u)) {
        return FALSE;
    }
    *enabled = *no_ssp_needed_flag != 0u;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetInfiniteSpirit(BOOL enabled) {
    BOOL current;

    enabled = enabled != FALSE;
    if (no_ssp_needed == NULL || !resource_flags_captured ||
        !writable_memory(no_ssp_needed_flag, 1u)) {
        return FALSE;
    }
    if (enabled) {
        no_ssp_needed();
    } else {
        *no_ssp_needed_flag = 0u;
    }
    if (!SudekiMpCleanroomEngineInfiniteSpirit(&current) ||
        current != enabled) {
        return FALSE;
    }
    SudekiMpCleanroomEngineMaintainResources();
    SudekiMpLogFormat(
        "cleanroom_engine event=infinite_spirit status=confirmed state=%s\r\n",
        enabled ? "enabled" : "disabled"
    );
    return TRUE;
}

void SudekiMpCleanroomEngineMaintainResources(void) {
    void **manager_global;
    void *manager;
    BOOL enabled;
    float current;

    if (game_base == NULL || !SudekiMpCleanroomEngineWorldReady()) {
        return;
    }
    (void)prepare_training_inventory();
    (void)prepare_spirit_strikes();
    initialize_present_actors();

    if (get_ssp == NULL || set_ssp == NULL ||
        !SudekiMpCleanroomEngineInfiniteSpirit(&enabled) || !enabled) {
        return;
    }
    manager_global =
        (void **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
    if (!readable_memory(manager_global, sizeof(*manager_global))) {
        return;
    }
    manager = *manager_global;
    if (!readable_memory(manager, 0xacu)) {
        return;
    }
    current = get_ssp();
    if (isfinite(current) && current < 200.0f) {
        set_ssp(200.0f);
        SudekiMpLogFormat(
            "cleanroom_engine event=infinite_spirit action=refill "
            "previous_bits=0x%08lx value=200\r\n",
            (unsigned long)float_bits(current)
        );
    }
}

void SudekiMpCleanroomEngineReset(void) {
    void **manager_global;

    cancel_ranged_prime();
    if (game_base != NULL && spirit_strike_unlocks_captured &&
        saved_spirit_strike_manager != NULL) {
        manager_global =
            (void **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
        if (readable_memory(manager_global, sizeof(*manager_global)) &&
            *manager_global == saved_spirit_strike_manager &&
            writable_memory(
                (uint8_t *)saved_spirit_strike_manager +
                    SPIRIT_STRIKE_UNLOCKS_OFFSET,
                1u)) {
            *((uint8_t *)saved_spirit_strike_manager +
                SPIRIT_STRIKE_UNLOCKS_OFFSET) =
                    saved_spirit_strike_unlocks;
        }
    }
    if (resource_flags_captured) {
        if (writable_memory(no_sp_needed_flag, 1u)) {
            *no_sp_needed_flag = saved_no_sp_needed;
        }
        if (writable_memory(no_ssp_needed_flag, 1u)) {
            *no_ssp_needed_flag = saved_no_ssp_needed;
        }
    }
    game_base = NULL;
    internal_spawn_pc = NULL;
    remove_pc = NULL;
    spawn_entity = NULL;
    despawn_entity = NULL;
    get_pc = NULL;
    get_generic_entity = NULL;
    get_group_players = NULL;
    group_players_in_combat = NULL;
    group_players_combat_transition = NULL;
    set_first_person_camera_mode = NULL;
    no_sp_needed = NULL;
    no_ssp_needed = NULL;
    get_ssp = NULL;
    set_ssp = NULL;
    fill_inventory = NULL;
    spirit_strike_enable = NULL;
    get_character_number_stat = NULL;
    set_character_number_stat = NULL;
    set_weapon = NULL;
    gel_pointer_to_entity = NULL;
    entity_pointer_cleanup = NULL;
    resource_name_from_text = NULL;
    resource_name_release_reference = NULL;
    ranged_prime_pending = FALSE;
    ranged_prime_ui_active = FALSE;
    ranged_prime_timer = 0u;
    no_sp_needed_flag = NULL;
    no_ssp_needed_flag = NULL;
    saved_no_sp_needed = 0u;
    saved_no_ssp_needed = 0u;
    resource_flags_captured = FALSE;
    inventory_filled = FALSE;
    spirit_strikes_unlocked = FALSE;
    spirit_strike_unlocks_captured = FALSE;
    saved_spirit_strike_unlocks = 0u;
    saved_spirit_strike_manager = NULL;
    ZeroMemory(
        initialized_actor_entities,
        sizeof(initialized_actor_entities)
    );
}
