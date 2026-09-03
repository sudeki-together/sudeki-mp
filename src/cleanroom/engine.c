#include "cleanroom/engine.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct SudekiMpResourceLookup {
    uint8_t storage[12];
    uint32_t *reference;
    void *resource_proxy;
} SudekiMpResourceLookup;

typedef struct SudekiMpCafuMissileModelPatch {
    SudekiMpResourceName *target;
    SudekiMpResourceName saved;
    SudekiMpResourceName applied;
} SudekiMpCafuMissileModelPatch;

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
typedef float (__attribute__((thiscall)) *ElcoGetFuelFunction)(
    const void *elco_ability
);
typedef void (__attribute__((thiscall)) *ElcoSetFuelFunction)(
    void *elco_ability,
    float value
);
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
typedef void (__attribute__((thiscall)) *TargeterFlagFunction)(
    void *targeter
);
typedef unsigned char (__attribute__((thiscall)) *TargeterPredicateFunction)(
    const void *targeter
);
typedef void (__attribute__((thiscall)) *ArbiterSetInvulnerableFunction)(
    void *arbiter,
    BOOL enabled
);
typedef void (__cdecl *SetMasterGameSpeedFunction)(float multiplier);
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
typedef unsigned int (__attribute__((thiscall)) *ResourceProxyTypeFunction)(
    void *resource_proxy
);
typedef void *(__attribute__((thiscall)) *LoadedResourceGetFunction)(
    void *loaded_resource
);
typedef void (__attribute__((thiscall)) *MissileLaunchThunkFunction)(
    void *missile_interface,
    void *launch_context
);
typedef unsigned char (__attribute__((regparm(2))) *MissileSelectFunction)(
    void *missile_manager,
    void *missile_data
);
typedef void (__attribute__((stdcall)) *PositionTransformUpdateFunction)(
    void *position
);
typedef const float *(__attribute__((thiscall))
    *CafuRenderLocatorMatrixFunction)(void *provider, int locator_index);
typedef unsigned int (__attribute__((thiscall))
    *CafuAnimationCountFunction)(void *renderer);
typedef int (__attribute__((thiscall)) *CafuAnimationSelectorGetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel
);
typedef float (__attribute__((thiscall)) *CafuAnimationFloatGetFunction)(
    void *renderer,
    int channel,
    unsigned int submodel
);
typedef unsigned int (__attribute__((thiscall))
    *CafuAnimationStateGetFunction)(
        void *renderer,
        int channel,
        unsigned int submodel
    );
typedef float (__attribute__((thiscall)) *AnimationBlendGetFunction)(
    void *renderer,
    int channel
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
    RVA_RESOURCE_LOOKUP = 0x00011730u,
    RVA_ENTITY_POINTER_CLEANUP = 0x000015e0u,
    RVA_RESOURCE_NAME_FROM_TEXT = 0x001b9440u,
    RVA_RESOURCE_NAME_RELEASE_REFERENCE = 0x001b9760u,
    RVA_GET_GROUP_PLAYERS = 0x00025100u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
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
    RVA_ELCO_GET_FUEL = 0x000cdfe0u,
    RVA_ELCO_SET_FUEL = 0x000cdf30u,
    RVA_GET_CHARACTER_NUMBER_STAT = 0x000c1270u,
    RVA_SET_CHARACTER_NUMBER_STAT = 0x000c1350u,
    RVA_SET_WEAPON = 0x000d8790u,
    RVA_TARGETER_INCLUDE_ALLIES = 0x0000f520u,
    RVA_TARGETER_REMOVE_ALLIES = 0x0000f560u,
    RVA_TARGETER_IS_TARGETING_ALLIES = 0x0000f5a0u,
    RVA_ARBITER_SET_INVULNERABLE = 0x000dca10u,
    RVA_SET_MASTER_GAME_SPEED = 0x0028be90u,
    RVA_MASTER_GAME_SPEED = 0x00325810u,
    RVA_MISSILE_LAUNCH_VTABLE_SLOT = 0x002d4cdcu,
    RVA_MISSILE_LAUNCH_THUNK = 0x000c7140u,
    RVA_MISSILE_SELECT = 0x000c6de0u,
    RVA_MISSILE_MANAGER_VTABLE = 0x002d4c8cu,
    RVA_MISSILE_PRESENTATION_TRANSFORM_CALL = 0x00136b36u,
    RVA_POSITION_TRANSFORM_UPDATE = 0x00110d40u,
    RVA_RENDER_LOCATOR_INDEX = 0x000c5ff0u,
    RVA_ANIMATION_RENDERER_VTABLE = 0x002df8ecu,
    RVA_ANIMATION_RENDERER_COUNT = 0x0021bb10u,
    RVA_ANIMATION_RENDERER_SELECTOR_GET = 0x002230b0u,
    RVA_ANIMATION_RENDERER_RATE_GET = 0x00223160u,
    RVA_ANIMATION_RENDERER_TIME_GET = 0x00223220u,
    RVA_ANIMATION_RENDERER_STATE_GET = 0x00223290u,
    RVA_ANIMATION_RENDERER_BLEND_GET = 0x002234e0u,
    RVA_NO_SP_NEEDED_FLAG = 0x003c2fccu,
    RVA_NO_SSP_NEEDED_FLAG = 0x003c2f23u,
    RVA_SPIRIT_STRIKE_MANAGER_GLOBAL = 0x00408d30u,
    RVA_ITEM_DATABASE_GLOBAL = 0x00408d80u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    CHARACTER_WEAPON_OFFSET = 0x00c0u,
    CHARACTER_WEAPON_CURRENT_ITEM_OFFSET = 0x0268u,
    ELCO_STARTER_WEAPON_SLOT = 12,
    ELCO_STARTER_WEAPON_ITEM_ID = 24,
    CAFU_WEAPON_ITEM_ID = 48,
    CAFU_WEAPON_BROKEN_MODEL_IDENTIFIER = 0xd4e32e13u,
    CAFU_WEAPON_ARCHIVE_MODEL_IDENTIFIER = 0xa4fe4833u,
    ELCO_WEAPON_MODEL_IDENTIFIER = 0xdf85ece7u,
    CAFU_WEAPON_PRELOAD_WAIT_MS = 2000u,
    CAFU_MISSILE_MANAGER_OFFSET = 0x0fd4u,
    CAFU_MISSILE_MANAGER_SELECTED_OFFSET = 0x0058u,
    CAFU_MISSILE_MANAGER_SELECTED_COMBO_OFFSET = 0x005cu,
    CAFU_MISSILE_COMBO_COUNT_OFFSET = 0x0044u,
    CAFU_MISSILE_COMBO_ARRAY_OFFSET = 0x004cu,
    CAFU_MISSILE_MODEL_OFFSET = 0x0030u,
    CAFU_MISSILE_MODEL_RESOURCE_KIND = 0x00000fa9u,
    CAFU_MISSILE_ARCHIVE_MODEL_IDENTIFIER = 0x4969a228u,
    CAFU_MISSILE_RUNTIME_MODEL_IDENTIFIER = 0xd7a3ae33u,
    CAFU_MISSILE_REPLACEMENT_MODEL_IDENTIFIER = 0x890597cdu,
    CAFU_MISSILE_PATCH_CAPACITY = 10u,
    CAFU_WEAPON_PRESENTATION_SAMPLE_LIMIT = 96u,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_ZERO_OFFSET = 0x0090u,
    PARTY_SLOT_STRIDE = 0x000cu,
    PARTY_COUNT_OFFSET = 0x00ccu,
    FORMATION_MEMBERS_OFFSET = 0x00f4u,
    FORMATION_COUNT_OFFSET = 0x0124u,
    CHARACTER_AI_COMPONENT_OFFSET = 0x0094u,
    AI_FORMATION_BACKPOINTER_OFFSET = 0x0040u,
    AI_MODE_STATE_OFFSET = 0x003cu,
    AI_MODE_VALUE_OFFSET = 0x000bu,
    AI_CONTROL_OVERRIDE_REF_OFFSET = 0x016au,
    CHARACTER_POSITION_OFFSET = 0x0044u,
    CHARACTER_COMBAT_DATA_OFFSET = 0x004cu,
    CHARACTER_ARBITER_OFFSET = 0x0090u,
    ARBITER_OWNER_OFFSET = 0x0010u,
    ARBITER_FLAGS_OFFSET = 0x0050u,
    ARBITER_INVULNERABILITY_REF_OFFSET = 0x0054u,
    ARBITER_INVULNERABILITY_FLAG = 0x00000800u,
    CONTROLLER_NEXT_CHARACTER_OFFSET = 0x00f4u,
    CONTROLLER_PREVIOUS_CHARACTER_OFFSET = 0x00fcu,
    CONTROLLER_TARGET_OFFSET = 0x0248u,
    CAFU_PLAYER_ONE_MAX_ROTATIONS = 8u,
    SPIRIT_STRIKE_UNLOCKS_OFFSET = 0x00acu
};

static const uint8_t elco_get_fuel_entry[] = {
    0xd9u, 0x41u, 0x6cu, 0xc3u
};
static const uint8_t elco_set_fuel_entry[] = {
    0xd9u, 0x44u, 0x24u, 0x04u, 0x56u, 0x8bu, 0xf1u,
    0xd9u, 0x56u, 0x68u, 0xd9u, 0x5eu, 0x6cu
};
static const uint8_t arbiter_set_invulnerable_entry[] = {
    0x80u, 0x7cu, 0x24u, 0x04u, 0x00u,
    0x74u, 0x05u,
    0xfeu, 0x41u, 0x54u,
    0xebu, 0x03u,
    0xfeu, 0x49u, 0x54u,
    0x80u, 0x79u, 0x54u, 0x00u,
    0x7eu, 0x0au,
    0x81u, 0x49u, 0x50u, 0x00u, 0x08u, 0x00u, 0x00u,
    0xc2u, 0x04u, 0x00u,
    0x81u, 0x61u, 0x50u, 0xffu, 0xf7u, 0xffu, 0xffu,
    0xc2u, 0x04u, 0x00u
};
static const uint8_t set_master_game_speed_entry_prefix[] = {
    0xd9u, 0x44u, 0x24u, 0x04u, 0xd9u, 0x1du
};

static const uint8_t resource_name_from_text_entry[] = {
    0x57u, 0x8bu, 0xf8u, 0x8bu, 0x06u, 0x25u, 0x80u, 0xefu
};
static const uint8_t resource_lookup_entry[] = {
    0x51u, 0xc6u, 0x04u, 0x24u, 0x00u, 0x8bu, 0x04u, 0x24u
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
static const uint8_t missile_select_entry[] = {
    0x51u, 0x32u, 0xc9u, 0x56u, 0x8bu, 0xf0u
};
static const uint8_t position_transform_update_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf0u,
    0x81u, 0xecu, 0xe4u, 0x00u, 0x00u, 0x00u
};

_Static_assert(
    sizeof(SudekiMpResourceName) == 12u,
    "Sudeki ResourceName ABI must remain 12 bytes"
);
_Static_assert(
    sizeof(SudekiMpResourceLookup) == 20u,
    "Sudeki retained resource lookup ABI must remain 20 bytes"
);

static const char *const actor_labels[SUDEKIMP_CLEANROOM_ACTOR_COUNT] = {
    "Tal", "Buki", "Elco", "Ailish", "Cafu"
};
static const char *const actor_resources[SUDEKIMP_CLEANROOM_ACTOR_COUNT] = {
    "PC_Tal", "PC_Buki", "PC_Elco", "PC_Ailish", "PC_Cafu"
};
/*
 * CCharacterWeapon::SetWeapon takes an index in inventory category 5, not
 * the global SOLData item ID. FillInventory orders that category by the
 * Ailish, Elco, Tal, then Buki weapon families. The matching starter slots
 * are therefore different from global item IDs 12, 24, 0, and 36.
 */
static const int actor_starter_weapon_slots[SUDEKIMP_CLEANROOM_ACTOR_COUNT] = {
    24, 36, 12, 0, -1
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
static ElcoGetFuelFunction elco_get_fuel;
static ElcoSetFuelFunction elco_set_fuel;
static GetCharacterNumberStatFunction get_character_number_stat;
static SetCharacterNumberStatFunction set_character_number_stat;
static SetWeaponFunction set_weapon;
static TargeterFlagFunction targeter_include_allies;
static TargeterFlagFunction targeter_remove_allies;
static TargeterPredicateFunction targeter_is_targeting_allies;
static ArbiterSetInvulnerableFunction arbiter_set_invulnerable;
static SetMasterGameSpeedFunction set_master_game_speed;
static volatile float *master_game_speed;
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
static BOOL infinite_jetpack_fuel;
static void *last_elco_ability;
static BOOL elco_fuel_refill_logged;
static BOOL spirit_strike_unlocks_captured;
static uint8_t saved_spirit_strike_unlocks;
static void *saved_spirit_strike_manager;
static void *initialized_actor_entities[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
static BOOL cafu_probe_requested;
static BOOL cafu_probe_spawn_attempted;
static BOOL cafu_player_one_confirmed;
static unsigned int cafu_player_one_rotation_attempts;
static BOOL cafu_weapon_inventory_logged;
static unsigned int cafu_weapon_gate_log;
static BOOL cafu_weapon_model_patched;
static BOOL cafu_weapon_model_uses_fallback;
static uint8_t *cafu_weapon_model_item;
static SudekiMpResourceName saved_cafu_weapon_model;
static SudekiMpResourceName applied_cafu_weapon_model;
static SudekiMpResourceLookup cafu_weapon_preload;
static BOOL cafu_weapon_preload_requested;
static BOOL cafu_weapon_model_ready;
static BOOL cafu_weapon_force_elco_model;
static DWORD cafu_weapon_preload_started_at;
static SudekiMpResourceName cafu_missile_model_name;
static SudekiMpResourceLookup cafu_missile_model_preload;
static BOOL cafu_missile_model_name_initialized;
static BOOL cafu_missile_model_preload_requested;
static BOOL cafu_missile_model_ready;
static SudekiMpCafuMissileModelPatch
    cafu_missile_model_patches[CAFU_MISSILE_PATCH_CAPACITY];
static unsigned int cafu_missile_model_patch_count;
static SudekiMpPointerHook cafu_missile_launch_hook;
static MissileLaunchThunkFunction original_missile_launch;
static SudekiMpInlineHook cafu_missile_select_hook;
static MissileSelectFunction original_missile_select;
static SudekiMpRelativeCallHook cafu_position_transform_hook;
static SudekiMpInlineHook cafu_position_transform_entry_hook;
static PositionTransformUpdateFunction original_position_transform_update;
static unsigned int cafu_missile_launch_sequence;
static void *last_cafu_missile_selected;
static void *last_cafu_missile_weapon;
static void *last_cafu_guarded_position;
static unsigned int cafu_guard_skip_count;
static PVOID cafu_exception_handler;
static BOOL cafu_autofire_requested;
static BOOL cafu_autofire_sent;
static DWORD cafu_autofire_ready_at;
static DWORD cafu_weapon_presentation_last_tick;
static unsigned int cafu_weapon_presentation_sample_count;

typedef struct SudekiMpPartyInvulnerabilityLease {
    void *character;
    void *arbiter;
    void *position;
    void *combat_data;
    void *world_manager;
    void *world_directory;
    int actor;
    BOOL owned;
} SudekiMpPartyInvulnerabilityLease;

static BOOL party_invulnerability_enabled;
static SudekiMpPartyInvulnerabilityLease
    party_invulnerability_leases[PARTY_SLOT_COUNT];
static unsigned int party_invulnerability_lease_count;
static BOOL story_test_speed_owned;
static BOOL story_test_speed_conflicted;
static uint32_t story_test_speed_saved_bits;
static uint32_t story_test_speed_applied_bits;

static BOOL readable_memory(const void *pointer, size_t size);
static BOOL writable_memory(const void *pointer, size_t size);
static void *actor_pointer(SudekiMpCleanroomActor actor);

static BOOL matches_set_master_game_speed_entry(
    const uint8_t *entry,
    const uint8_t *module_base
) {
    uint32_t relocated_target;

    if (!readable_memory(entry, 11u) || module_base == NULL ||
        memcmp(entry, set_master_game_speed_entry_prefix,
            sizeof(set_master_game_speed_entry_prefix)) != 0 ||
        entry[10] != 0xc3u) {
        return FALSE;
    }
    memcpy(&relocated_target, entry + 6u, sizeof(relocated_target));
    return relocated_target == (uint32_t)(uintptr_t)(
        module_base + RVA_MASTER_GAME_SPEED
    );
}

static void *elco_ability_pointer(void) {
    uint8_t *elco;
    void *ability;
    float maximum;
    float current;

    elco = (uint8_t *)actor_pointer(SUDEKIMP_CLEANROOM_ELCO);
    if (!readable_memory(elco, 0x108u)) {
        return NULL;
    }
    ability = *(void **)(elco + 0x104u);
    if (!readable_memory(ability, 0x80u) ||
        !writable_memory(ability, 0x80u)) {
        return NULL;
    }
    maximum = *(const float *)((const uint8_t *)ability + 0x68u);
    current = *(const float *)((const uint8_t *)ability + 0x6cu);
    if (!isfinite(maximum) || !isfinite(current) || maximum <= 0.0f ||
        maximum > 1000000.0f || current < -maximum ||
        current > maximum * 2.0f) {
        return NULL;
    }
    return ability;
}
static BOOL patch_cafu_missile_model_record(
    SudekiMpResourceName *model,
    uint8_t *combo,
    unsigned int combo_index,
    const char *source
);

__attribute__((naked, noinline, used))
static int call_cafu_render_locator_index(
    void *render_object __attribute__((unused)),
    const char *locator_name __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "movl 12(%esp), %edx\n\t"
        "pushl 8(%esp)\n\t"
        "call *%edx\n\t"
        "ret\n\t"
    );
}

static void maintain_cafu_autofire(void) {
    INPUT inputs[2];
    DWORD now;

    if (!cafu_autofire_requested || cafu_autofire_sent ||
        !cafu_player_one_confirmed) {
        return;
    }
    now = GetTickCount();
    if (cafu_autofire_ready_at == 0u) {
        cafu_autofire_ready_at = now;
        return;
    }
    if ((DWORD)(now - cafu_autofire_ready_at) < 1500u) {
        return;
    }
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    cafu_autofire_sent = TRUE;
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_autofire action=send_input "
        "sent=%u policy=diagnostic_environment_gate\r\n",
        (unsigned int)SendInput(2u, inputs, sizeof(INPUT))
    );
}

static LONG CALLBACK trace_cafu_exception(
    EXCEPTION_POINTERS *exception_pointers
) {
    EXCEPTION_RECORD *record;
    CONTEXT *context;
    uintptr_t address;
    uintptr_t base;

    if (!cafu_probe_requested || exception_pointers == NULL ||
        exception_pointers->ExceptionRecord == NULL ||
        exception_pointers->ContextRecord == NULL || game_base == NULL) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    record = exception_pointers->ExceptionRecord;
    context = exception_pointers->ContextRecord;
    address = (uintptr_t)record->ExceptionAddress;
    base = (uintptr_t)game_base;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        address < base || address >= base + 0x00460000u) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_exception code=0x%08lx "
        "address=%p rva=0x%08lx operation=%lu target=%p "
        "eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx "
        "edi=%08lx esp=%08lx ebp=%08lx "
        "policy=diagnostic_continue_search\r\n",
        (unsigned long)record->ExceptionCode,
        record->ExceptionAddress,
        (unsigned long)(address - base),
        record->NumberParameters > 0u ?
            (unsigned long)record->ExceptionInformation[0] : 0ul,
        record->NumberParameters > 1u ?
            (void *)(uintptr_t)record->ExceptionInformation[1] : NULL,
        (unsigned long)context->Eax,
        (unsigned long)context->Ebx,
        (unsigned long)context->Ecx,
        (unsigned long)context->Edx,
        (unsigned long)context->Esi,
        (unsigned long)context->Edi,
        (unsigned long)context->Esp,
        (unsigned long)context->Ebp
    );
    return EXCEPTION_CONTINUE_SEARCH;
}

static BOOL consume_invalid_cafu_position_transform(
    void *position,
    BOOL establish_ownership,
    const char *source
) {
    uint8_t *wrapper;
    void *render_object;
    uint8_t dirty_transform;
    uint8_t attached_update;
    uint8_t direct_render;

    dirty_transform = readable_memory(position, 0xb9u) ?
        *((uint8_t *)position + 0xb8u) : 0u;
    attached_update = readable_memory(position, 0x103u) ?
        *((uint8_t *)position + 0x102u) : 0u;
    direct_render = readable_memory(position, 0x102u) ?
        *((uint8_t *)position + 0x101u) : 0u;
    wrapper = readable_memory(position, 0xb8u) ?
        *(uint8_t **)((uint8_t *)position + 0xb4u) : NULL;
    render_object = readable_memory(wrapper, 0x0cu) ?
        *(void **)(wrapper + 0x08u) : NULL;
    if (!establish_ownership && position != last_cafu_guarded_position) {
        return FALSE;
    }
    if (dirty_transform == 1u && wrapper != NULL &&
        render_object == NULL && writable_memory(position, 0xb9u)) {
        /*
         * CPosition::UpdateTransform normally consumes this dirty byte at
         * +0xB8 after publishing its matrix.  Cafu's missing projectile
         * presentation has an attachment wrapper but no render object, so
         * both UpdateTransform and the following attachment resolver would
         * dereference it.  Detach only that unusable presentation wrapper
         * and consume the impossible update; missile gameplay remains owned
         * by Sudeki.
         */
        *((uint8_t *)position + 0xb8u) = 0u;
        *(void **)((uint8_t *)position + 0xb4u) = NULL;
        last_cafu_guarded_position = position;
        cafu_guard_skip_count += 1u;
        if (cafu_guard_skip_count <= 3u) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_missile_presentation_guard "
                "action=skip_invalid_transform source=%s "
                "position=%p wrapper_before=%p wrapper_after=%p "
                "render_object=%p dirty_before=%u dirty_after=%u "
                "attached_update=%u direct_render=%u skips=%u "
                "policy=consume_impossible_presentation_update\r\n",
                source == NULL ? "unknown" : source,
                position,
                wrapper,
                *(void **)((uint8_t *)position + 0xb4u),
                render_object,
                (unsigned int)dirty_transform,
                (unsigned int)*((uint8_t *)position + 0xb8u),
                (unsigned int)attached_update,
                (unsigned int)direct_render,
                cafu_guard_skip_count
            );
        }
        return TRUE;
    }
    return FALSE;
}

static void __attribute__((stdcall)) guard_cafu_position_transform(
    void *position
) {
    if (consume_invalid_cafu_position_transform(
            position,
            TRUE,
            "missile_presentation_call")) {
        return;
    }
    if (original_position_transform_update != NULL) {
        original_position_transform_update(position);
    }
}

static void __attribute__((stdcall)) guard_cafu_position_transform_entry(
    void *position
) {
    if (consume_invalid_cafu_position_transform(
            position,
            FALSE,
            "global_transform_retry")) {
        return;
    }
    if (original_position_transform_update != NULL) {
        original_position_transform_update(position);
    }
}

static void inspect_cafu_missile_manager_state(void) {
    uint8_t *cafu;
    uint8_t *manager;
    uint8_t *selected;
    void *weapon;
    const SudekiMpResourceName *model;
    const SudekiMpResourceName *fire_effect;
    const SudekiMpResourceName *environment_effect;

    cafu = (uint8_t *)actor_pointer(SUDEKIMP_CLEANROOM_CAFU);
    if (!readable_memory(
            cafu,
            CAFU_MISSILE_MANAGER_OFFSET + 0xe4u)) {
        return;
    }
    manager = cafu + CAFU_MISSILE_MANAGER_OFFSET;
    if (*(void **)manager != game_base + RVA_MISSILE_MANAGER_VTABLE) {
        return;
    }
    selected = *(uint8_t **)(
        manager + CAFU_MISSILE_MANAGER_SELECTED_OFFSET
    );
    weapon = *(void **)(
        manager + CAFU_MISSILE_MANAGER_SELECTED_COMBO_OFFSET
    );
    if (selected == last_cafu_missile_selected &&
        weapon == last_cafu_missile_weapon) {
        return;
    }
    last_cafu_missile_selected = selected;
    last_cafu_missile_weapon = weapon;
    if (!readable_memory(selected, 0x48u)) {
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_missile_manager_state "
            "manager=%p selected=%p weapon=%p mode=%u "
            "status=selected_record_unavailable policy=read_only\r\n",
            manager,
            selected,
            weapon,
            (unsigned int)*(manager + 0xe0u)
        );
        return;
    }
    model = (const SudekiMpResourceName *)(selected + 0x24u);
    fire_effect = (const SudekiMpResourceName *)(selected + 0x30u);
    environment_effect = (const SudekiMpResourceName *)(selected + 0x3cu);
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_missile_manager_state "
        "manager=%p selected=%p weapon=%p mode=%u "
        "model=%08lx,%08lx,%p fire_effect=%08lx,%08lx,%p "
        "environment_effect=%08lx,%08lx,%p "
        "policy=read_only_persistent_selection_inventory\r\n",
        manager,
        selected,
        weapon,
        (unsigned int)*(manager + 0xe0u),
        (unsigned long)model->encoded_kind,
        (unsigned long)model->identifier,
        model->text_reference,
        (unsigned long)fire_effect->encoded_kind,
        (unsigned long)fire_effect->identifier,
        fire_effect->text_reference,
        (unsigned long)environment_effect->encoded_kind,
        (unsigned long)environment_effect->identifier,
        environment_effect->text_reference
    );
}

static unsigned char __attribute__((regparm(2))) trace_cafu_missile_select(
    void *missile_manager,
    void *missile_data
) {
    uint8_t *cafu;
    SudekiMpResourceName *model;
    const SudekiMpResourceName *fire_effect;
    const SudekiMpResourceName *environment_effect;
    BOOL is_cafu;

    cafu = (uint8_t *)actor_pointer(SUDEKIMP_CLEANROOM_CAFU);
    is_cafu = readable_memory(cafu, sizeof(void *)) &&
        readable_memory(missile_manager, 0x14u) &&
        *(void **)((uint8_t *)missile_manager + 0x10u) == cafu;
    if (is_cafu && readable_memory(missile_data, 0xc0u)) {
        model = (SudekiMpResourceName *)(
            (const uint8_t *)missile_data + 0x30u
        );
        fire_effect = (const SudekiMpResourceName *)(
            (const uint8_t *)missile_data + 0x3cu
        );
        environment_effect = (const SudekiMpResourceName *)(
            (const uint8_t *)missile_data + 0x48u
        );
        (void)patch_cafu_missile_model_record(
            model,
            (uint8_t *)missile_data,
            UINT_MAX,
            "first_use_selection"
        );
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_missile_select phase=pre_native "
            "manager=%p data=%p model=%08lx,%08lx,%p "
            "fire_effect=%08lx,%08lx,%p "
            "environment_effect=%08lx,%08lx,%p "
            "policy=read_only_resource_inventory\r\n",
            missile_manager,
            missile_data,
            (unsigned long)model->encoded_kind,
            (unsigned long)model->identifier,
            model->text_reference,
            (unsigned long)fire_effect->encoded_kind,
            (unsigned long)fire_effect->identifier,
            fire_effect->text_reference,
            (unsigned long)environment_effect->encoded_kind,
            (unsigned long)environment_effect->identifier,
            environment_effect->text_reference
        );
    }
    return original_missile_select != NULL ?
        original_missile_select(missile_manager, missile_data) : 0u;
}

static void __attribute__((thiscall)) trace_cafu_missile_launch(
    void *missile_interface,
    void *launch_context
) {
    uint8_t *missile_manager;
    uint8_t *cafu;
    uint8_t *selected;
    const SudekiMpResourceName *model;
    const SudekiMpResourceName *fire_effect;
    const SudekiMpResourceName *environment_effect;
    BOOL is_cafu;

    missile_manager = readable_memory(missile_interface, 0xc9u) &&
        *((uint8_t *)missile_interface + 0xc8u) == 2u ?
        (uint8_t *)missile_interface - 0x18u : NULL;
    cafu = (uint8_t *)actor_pointer(SUDEKIMP_CLEANROOM_CAFU);
    is_cafu = readable_memory(
            cafu,
            CAFU_MISSILE_MANAGER_OFFSET + sizeof(void *)) &&
        missile_manager == cafu + CAFU_MISSILE_MANAGER_OFFSET &&
        readable_memory(missile_manager, sizeof(void *)) &&
        *(void **)missile_manager ==
            game_base + RVA_MISSILE_MANAGER_VTABLE;
    selected = readable_memory(missile_manager, 0x5cu) ?
        *(uint8_t **)((uint8_t *)missile_manager + 0x58u) : NULL;
    if (is_cafu && readable_memory(selected, 0x48u)) {
        model = (const SudekiMpResourceName *)(selected + 0x24u);
        fire_effect = (const SudekiMpResourceName *)(selected + 0x30u);
        environment_effect =
            (const SudekiMpResourceName *)(selected + 0x3cu);
        cafu_missile_launch_sequence += 1u;
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_missile_launch phase=pre_native "
            "sequence=%u manager=%p selected=%p "
            "model=%08lx,%08lx,%p fire_effect=%08lx,%08lx,%p "
            "environment_effect=%08lx,%08lx,%p weapon=%p "
            "policy=read_only_resource_inventory\r\n",
            cafu_missile_launch_sequence,
            missile_manager,
            selected,
            (unsigned long)model->encoded_kind,
            (unsigned long)model->identifier,
            model->text_reference,
            (unsigned long)fire_effect->encoded_kind,
            (unsigned long)fire_effect->identifier,
            fire_effect->text_reference,
            (unsigned long)environment_effect->encoded_kind,
            (unsigned long)environment_effect->identifier,
            environment_effect->text_reference,
            *(void **)((uint8_t *)missile_manager + 0x5cu)
        );
    }
    if (original_missile_launch != NULL) {
        original_missile_launch(missile_interface, launch_context);
    }
}

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

static BOOL executable_memory(const void *pointer) {
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;

    if (pointer == NULL ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL cafu_locator_matrix(
    void *render_object_pointer,
    const char *locator_name,
    int *locator_index_result,
    const float **matrix_result
) {
    uint8_t *render_object = (uint8_t *)render_object_pointer;
    uint8_t *provider;
    void **vtable;
    CafuRenderLocatorMatrixFunction get_matrix;
    const float *matrix;
    int locator_index;
    unsigned int value_index;

    *locator_index_result = -1;
    *matrix_result = NULL;
    if (game_base == NULL || locator_name == NULL ||
        !readable_memory(render_object, 0x18u) ||
        !executable_memory(game_base + RVA_RENDER_LOCATOR_INDEX)) {
        return FALSE;
    }
    provider = *(uint8_t **)(render_object + 0x14u);
    if (!readable_memory(provider, sizeof(void *))) {
        return FALSE;
    }
    vtable = *(void ***)provider;
    if (!readable_memory(vtable, 0x2cu) ||
        !executable_memory(vtable[0x24u / sizeof(void *)]) ||
        !executable_memory(vtable[0x28u / sizeof(void *)])) {
        return FALSE;
    }
    locator_index = call_cafu_render_locator_index(
        render_object,
        locator_name,
        game_base + RVA_RENDER_LOCATOR_INDEX
    );
    *locator_index_result = locator_index;
    if (locator_index < 0) {
        return FALSE;
    }
    get_matrix = (CafuRenderLocatorMatrixFunction)(
        vtable[0x24u / sizeof(void *)]
    );
    matrix = get_matrix(provider, locator_index);
    if (!readable_memory(matrix, sizeof(float) * 16u)) {
        return FALSE;
    }
    for (value_index = 0u; value_index < 16u; ++value_index) {
        if (!isfinite(matrix[value_index])) {
            return FALSE;
        }
    }
    *matrix_result = matrix;
    return TRUE;
}

static void trace_cafu_weapon_presentation(void) {
    static const char *const locator_names[] = {
        "WeaponLoc_Rhand",
        "WeaponLoc_leg",
        "SFX",
        "WeaponFollow"
    };
    uint8_t *character;
    uint8_t *position;
    uint8_t *wrapper;
    uint8_t *render_object;
    uint8_t *weapon;
    uint8_t *weapon_slot0_wrapper;
    uint8_t *weapon_slot0_render_object;
    uint8_t *renderer;
    void **renderer_vtable;
    CafuAnimationCountFunction get_count;
    CafuAnimationSelectorGetFunction get_selector;
    CafuAnimationFloatGetFunction get_rate;
    CafuAnimationFloatGetFunction get_time;
    CafuAnimationStateGetFunction get_state;
    const float *locator_matrix;
    DWORD now;
    unsigned int submodels;
    unsigned int channel;
    unsigned int locator_name_index;
    int locator_index;

    if (!cafu_probe_requested || !cafu_player_one_confirmed ||
        cafu_weapon_presentation_sample_count >=
            CAFU_WEAPON_PRESENTATION_SAMPLE_LIMIT) {
        return;
    }
    now = GetTickCount();
    if (cafu_weapon_presentation_last_tick != 0u &&
        (DWORD)(now - cafu_weapon_presentation_last_tick) < 125u) {
        return;
    }
    character = (uint8_t *)actor_pointer(SUDEKIMP_CLEANROOM_CAFU);
    if (!readable_memory(character, 0x138u)) {
        return;
    }
    position = *(uint8_t **)(character + 0x44u);
    weapon = *(uint8_t **)(character + CHARACTER_WEAPON_OFFSET);
    if (!readable_memory(position, 0xb8u) ||
        !readable_memory(weapon, 0x26cu)) {
        return;
    }
    wrapper = *(uint8_t **)(position + 0xb4u);
    if (!readable_memory(wrapper, 0x14u)) {
        return;
    }
    render_object = *(uint8_t **)(wrapper + 0x08u);
    renderer = *(uint8_t **)(wrapper + 0x10u);
    if (!readable_memory(render_object, 0x18u) ||
        !readable_memory(renderer, sizeof(void *))) {
        return;
    }
    renderer_vtable = *(void ***)renderer;
    if (!readable_memory(renderer_vtable, 0x11cu) ||
        renderer_vtable[0xf8u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_COUNT ||
        renderer_vtable[0x100u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_SELECTOR_GET ||
        renderer_vtable[0x108u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_RATE_GET ||
        renderer_vtable[0x110u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_TIME_GET ||
        renderer_vtable[0x118u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_STATE_GET ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        return;
    }
    get_count = (CafuAnimationCountFunction)
        renderer_vtable[0xf8u / sizeof(void *)];
    get_selector = (CafuAnimationSelectorGetFunction)
        renderer_vtable[0x100u / sizeof(void *)];
    get_rate = (CafuAnimationFloatGetFunction)
        renderer_vtable[0x108u / sizeof(void *)];
    get_time = (CafuAnimationFloatGetFunction)
        renderer_vtable[0x110u / sizeof(void *)];
    get_state = (CafuAnimationStateGetFunction)
        renderer_vtable[0x118u / sizeof(void *)];
    submodels = get_count(renderer);
    if (submodels == 0u || submodels > 32u) {
        return;
    }

    cafu_weapon_presentation_last_tick = now;
    ++cafu_weapon_presentation_sample_count;
    weapon_slot0_wrapper = *(uint8_t **)(weapon + 0xf4u);
    weapon_slot0_render_object =
        readable_memory(weapon_slot0_wrapper, 0x0cu) ?
            *(uint8_t **)(weapon_slot0_wrapper + 0x08u) : NULL;
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_weapon_presentation sequence=%u "
        "phase=attachment character=%p position=%p wrapper=%p "
        "render_object=%p renderer=%p submodels=%u weapon=%p "
        "slot0_parent=%p slot0_locator=%ld slot0_wrapper=%p "
        "slot0_local_translation=%.5f,%.5f,%.5f "
        "slot0_world_translation=%.5f,%.5f,%.5f "
        "slot0_render=%p slot0_render_translation=%.5f,%.5f,%.5f "
        "slot1_parent=%p slot1_locator=%ld slot1_wrapper=%p "
        "slot1_local_translation=%.5f,%.5f,%.5f "
        "policy=read_only_no_attachment_or_animation_write\r\n",
        cafu_weapon_presentation_sample_count,
        character,
        position,
        wrapper,
        render_object,
        renderer,
        submodels,
        weapon,
        *(void **)(weapon + 0xd4u),
        (long)*(int *)(weapon + 0xecu),
        *(void **)(weapon + 0xf4u),
        *(float *)(weapon + 0xa0u),
        *(float *)(weapon + 0xa4u),
        *(float *)(weapon + 0xa8u),
        *(float *)(weapon + 0x130u),
        *(float *)(weapon + 0x134u),
        *(float *)(weapon + 0x138u),
        weapon_slot0_render_object,
        readable_memory(weapon_slot0_render_object, 0xd0u) ?
            *(float *)(weapon_slot0_render_object + 0xc0u) : NAN,
        readable_memory(weapon_slot0_render_object, 0xd0u) ?
            *(float *)(weapon_slot0_render_object + 0xc4u) : NAN,
        readable_memory(weapon_slot0_render_object, 0xd0u) ?
            *(float *)(weapon_slot0_render_object + 0xc8u) : NAN,
        *(void **)(weapon + 0x1e4u),
        (long)*(int *)(weapon + 0x1fcu),
        *(void **)(weapon + 0x204u),
        *(float *)(weapon + 0x1b0u),
        *(float *)(weapon + 0x1b4u),
        *(float *)(weapon + 0x1b8u)
    );
    for (locator_name_index = 0u;
         locator_name_index <
             sizeof(locator_names) / sizeof(locator_names[0]);
         ++locator_name_index) {
        locator_index = -1;
        locator_matrix = NULL;
        if (cafu_locator_matrix(
                render_object,
                locator_names[locator_name_index],
                &locator_index,
                &locator_matrix)) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_presentation "
                "sequence=%u phase=locator name=%s index=%d "
                "right=%.5f,%.5f,%.5f up=%.5f,%.5f,%.5f "
                "forward=%.5f,%.5f,%.5f translation=%.5f,%.5f,%.5f\r\n",
                cafu_weapon_presentation_sample_count,
                locator_names[locator_name_index],
                locator_index,
                locator_matrix[0], locator_matrix[1], locator_matrix[2],
                locator_matrix[4], locator_matrix[5], locator_matrix[6],
                locator_matrix[8], locator_matrix[9], locator_matrix[10],
                locator_matrix[12], locator_matrix[13], locator_matrix[14]
            );
        } else {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_presentation "
                "sequence=%u phase=locator name=%s index=%d "
                "status=unresolved\r\n",
                cafu_weapon_presentation_sample_count,
                locator_names[locator_name_index],
                locator_index
            );
        }
    }
    for (channel = 0u; channel < 5u; ++channel) {
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_presentation sequence=%u "
            "phase=animation channel=%u selector=%d state=%u "
            "rate=%.5f time=%.5f\r\n",
            cafu_weapon_presentation_sample_count,
            channel,
            get_selector(renderer, (int)channel, 0u),
            get_state(renderer, (int)channel, 0u),
            get_rate(renderer, (int)channel, 0u),
            get_time(renderer, (int)channel, 0u)
        );
    }
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
        !writable_memory(resource_name->text_reference, sizeof(uint32_t)) ||
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
        writable_memory(reference, sizeof(uint32_t)) &&
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

BOOL SudekiMpCleanroomEngineResourceNameFromText(
    SudekiMpResourceName *resource_name,
    const char *text
) {
    return initialize_resource_name(resource_name, text);
}

BOOL SudekiMpCleanroomEngineRetainResourceNameExact(
    SudekiMpResourceName *destination,
    const SudekiMpResourceName *source
) {
    SudekiMpResourceName copy;
    uint32_t *reference;

    if (destination == NULL || source == NULL || destination == source ||
        !readable_memory(source, sizeof(*source))) {
        return FALSE;
    }
    copy = *source;
    reference = copy.text_reference;
    if (reference == NULL ||
        !readable_memory(reference, 2u * sizeof(uint32_t)) ||
        !writable_memory(reference, sizeof(uint32_t)) ||
        reference[0] == 0u || reference[0] == UINT32_MAX ||
        reference[1] == 0u) {
        ZeroMemory(destination, sizeof(*destination));
        return FALSE;
    }
    ++reference[0];
    *destination = copy;
    return TRUE;
}

void SudekiMpCleanroomEngineReleaseResourceName(
    SudekiMpResourceName *resource_name
) {
    release_resource_name(resource_name);
}

static void release_retained_reference(uint32_t **reference_slot) {
    uint32_t *reference;

    if (reference_slot == NULL) {
        return;
    }
    reference = *reference_slot;
    *reference_slot = NULL;
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

void *SudekiMpCleanroomEngineGenericEntity(const char *resource_name) {
    return lookup_entity(get_generic_entity, resource_name);
}

static BOOL exact_retail_hero_set(
    void *const members[PARTY_SLOT_COUNT],
    void *const heroes[PARTY_SLOT_COUNT],
    BOOL require_tal_lead
) {
    uint8_t seen = 0u;
    unsigned int member_index;

    if (require_tal_lead && members[0] != heroes[SUDEKIMP_CLEANROOM_TAL]) {
        return FALSE;
    }
    for (member_index = 0u; member_index < PARTY_SLOT_COUNT;
            ++member_index) {
        unsigned int hero_index;

        if (members[member_index] == NULL) {
            return FALSE;
        }
        for (hero_index = 0u; hero_index < PARTY_SLOT_COUNT; ++hero_index) {
            if (members[member_index] == heroes[hero_index]) {
                break;
            }
        }
        if (hero_index == PARTY_SLOT_COUNT ||
            (seen & (uint8_t)(1u << hero_index)) != 0u) {
            return FALSE;
        }
        seen = (uint8_t)(seen | (uint8_t)(1u << hero_index));
    }
    return seen == 0x0fu;
}

BOOL SudekiMpCleanroomEngineExactRetailPartyReady(void) {
    uint8_t *group;
    uint8_t *ai_manager;
    uint8_t *formation;
    void **ai_manager_global;
    void *heroes[PARTY_SLOT_COUNT];
    void *group_members[PARTY_SLOT_COUNT];
    void *formation_members[PARTY_SLOT_COUNT];
    int group_count_before;
    int group_count_after;
    int formation_count_before;
    int formation_count_after;
    unsigned int index;

    if (game_base == NULL || get_group_players == NULL) {
        return FALSE;
    }
    for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
        heroes[index] = actor_pointer((SudekiMpCleanroomActor)index);
        if (heroes[index] == NULL) {
            return FALSE;
        }
    }
    group = (uint8_t *)get_group_players();
    ai_manager_global = (void **)(game_base + RVA_ENTITY_DIRECTORY_GLOBAL);
    if (!readable_memory(group, PARTY_COUNT_OFFSET + sizeof(int)) ||
        !readable_memory(ai_manager_global, sizeof(*ai_manager_global))) {
        return FALSE;
    }
    ai_manager = (uint8_t *)*ai_manager_global;
    if (!readable_memory(ai_manager,
            FORMATION_COUNT_OFFSET + sizeof(int))) {
        return FALSE;
    }
    formation = ai_manager + FORMATION_MEMBERS_OFFSET;
    group_count_before = *(int *)(group + PARTY_COUNT_OFFSET);
    formation_count_before = *(int *)(ai_manager + FORMATION_COUNT_OFFSET);
    if (group_count_before != (int)PARTY_SLOT_COUNT ||
        formation_count_before != (int)PARTY_SLOT_COUNT) {
        return FALSE;
    }
    for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *character = (uint8_t *)heroes[index];
        uint8_t *ai_component;

        group_members[index] = *(void **)(
            group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE);
        formation_members[index] = *(void **)(
            formation + index * PARTY_SLOT_STRIDE);
        if (!readable_memory(character,
                CHARACTER_AI_COMPONENT_OFFSET + sizeof(ai_component))) {
            return FALSE;
        }
        ai_component = *(uint8_t **)(
            character + CHARACTER_AI_COMPONENT_OFFSET);
        if (!readable_memory(ai_component,
                AI_FORMATION_BACKPOINTER_OFFSET + sizeof(void *)) ||
            *(void **)(ai_component + AI_FORMATION_BACKPOINTER_OFFSET) !=
                formation) {
            return FALSE;
        }
    }
    group_count_after = *(int *)(group + PARTY_COUNT_OFFSET);
    formation_count_after = *(int *)(ai_manager + FORMATION_COUNT_OFFSET);
    return group_count_before == group_count_after &&
        formation_count_before == formation_count_after &&
        exact_retail_hero_set(group_members, heroes, TRUE) &&
        exact_retail_hero_set(formation_members, heroes, FALSE);
}

static BOOL actor_control_state(
    SudekiMpCleanroomActor actor,
    int16_t *override_ref,
    uint8_t *mode
);

static BOOL actor_control_state_exact(
    SudekiMpCleanroomActor actor,
    int16_t expected_override_ref,
    uint8_t expected_mode
) {
    int16_t override_ref;
    uint8_t mode;

    if (!actor_control_state(actor, &override_ref, &mode)) {
        return FALSE;
    }
    return override_ref == expected_override_ref && mode == expected_mode;
}

static BOOL actor_control_state(
    SudekiMpCleanroomActor actor,
    int16_t *override_ref,
    uint8_t *mode
) {
    uint8_t *character = (uint8_t *)actor_pointer(actor);
    uint8_t *component;
    uint8_t *mode_state;

    if (override_ref == NULL || mode == NULL || !readable_memory(character,
            CHARACTER_AI_COMPONENT_OFFSET + sizeof(component))) {
        return FALSE;
    }
    component = *(uint8_t **)(
        character + CHARACTER_AI_COMPONENT_OFFSET);
    if (!readable_memory(component,
            AI_CONTROL_OVERRIDE_REF_OFFSET + sizeof(int16_t))) {
        return FALSE;
    }
    mode_state = *(uint8_t **)(component + AI_MODE_STATE_OFFSET);
    if (!readable_memory(mode_state,
            AI_MODE_VALUE_OFFSET + sizeof(uint8_t))) {
        return FALSE;
    }
    *override_ref = *(int16_t *)(component + AI_CONTROL_OVERRIDE_REF_OFFSET);
    *mode = *(uint8_t *)(mode_state + AI_MODE_VALUE_OFFSET);
    return TRUE;
}

BOOL SudekiMpCleanroomEngineExactPostRestoreControlsReady(void) {
    uint8_t *controller;
    void *tal;

    if (!SudekiMpCleanroomEngineExactRetailPartyReady() ||
        !readable_memory(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return FALSE;
    }
    controller = *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    tal = actor_pointer(SUDEKIMP_CLEANROOM_TAL);
    if (tal == NULL || !readable_memory(controller,
            CONTROLLER_TARGET_OFFSET + sizeof(void *)) ||
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) != tal) {
        return FALSE;
    }
    return actor_control_state_exact(SUDEKIMP_CLEANROOM_TAL, 0, 0u) &&
        actor_control_state_exact(SUDEKIMP_CLEANROOM_AILISH, 1, 0u) &&
        actor_control_state_exact(SUDEKIMP_CLEANROOM_BUKI, 0, 1u) &&
        actor_control_state_exact(SUDEKIMP_CLEANROOM_ELCO, 0, 1u) &&
        SudekiMpCleanroomEngineExactRetailPartyReady();
}

BOOL SudekiMpCleanroomEnginePostRestoreControlTupleActive(
    int16_t tal_ref,
    uint8_t tal_mode,
    int16_t ailish_ref,
    uint8_t ailish_mode,
    int16_t buki_ref,
    uint8_t buki_mode,
    int16_t elco_ref,
    uint8_t elco_mode
) {
    return tal_ref >= 0 && tal_mode == 0u &&
        ailish_ref >= 1 && ailish_mode == 0u &&
        buki_ref >= 0 &&
        ((buki_ref == 0 && buki_mode == 1u) ||
         (buki_ref > 0 && buki_mode == 0u)) &&
        elco_ref >= 0 &&
        ((elco_ref == 0 && elco_mode == 1u) ||
         (elco_ref > 0 && elco_mode == 0u));
}

BOOL SudekiMpCleanroomEnginePostRestoreControlsActive(void) {
    uint8_t *controller;
    void *tal;
    int16_t tal_override_ref;
    int16_t ailish_override_ref;
    int16_t buki_override_ref;
    int16_t elco_override_ref;
    uint8_t tal_mode;
    uint8_t ailish_mode;
    uint8_t buki_mode;
    uint8_t elco_mode;

    if (!SudekiMpCleanroomEngineExactRetailPartyReady() ||
        !readable_memory(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return FALSE;
    }
    controller = *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    tal = actor_pointer(SUDEKIMP_CLEANROOM_TAL);
    if (tal == NULL || !readable_memory(controller,
            CONTROLLER_TARGET_OFFSET + sizeof(void *)) ||
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) != tal ||
        !actor_control_state(SUDEKIMP_CLEANROOM_TAL,
            &tal_override_ref, &tal_mode) ||
        !actor_control_state(SUDEKIMP_CLEANROOM_AILISH,
            &ailish_override_ref, &ailish_mode) ||
        !actor_control_state(SUDEKIMP_CLEANROOM_BUKI,
            &buki_override_ref, &buki_mode) ||
        !actor_control_state(SUDEKIMP_CLEANROOM_ELCO,
            &elco_override_ref, &elco_mode)) {
        return FALSE;
    }
    /* +0x16A is a shared native lease count. Our Ailish lease was already
     * proven as the exact 0->1 transition at admission. Skill cameras acquire
     * every eligible party AI component: Tal/Buki/Elco transition 0->1->0,
     * while Ailish transitions 1->2->1. Those balanced native leases do not
     * revoke ours. Input consumers remain stricter and pause unless Ailish's
     * count is exactly one. */
    return SudekiMpCleanroomEnginePostRestoreControlTupleActive(
            tal_override_ref, tal_mode,
            ailish_override_ref, ailish_mode,
            buki_override_ref, buki_mode,
            elco_override_ref, elco_mode) &&
        SudekiMpCleanroomEngineExactRetailPartyReady();
}

static BOOL party_invulnerability_world(
    void **world_manager,
    void **world_directory
) {
    void **manager_global;
    void **directory_global;

    if (world_manager == NULL || world_directory == NULL ||
        game_base == NULL || !SudekiMpCleanroomEngineWorldReady()) {
        return FALSE;
    }
    manager_global = (void **)(game_base + RVA_ENTITY_MANAGER_GLOBAL);
    directory_global = (void **)(game_base + RVA_ENTITY_DIRECTORY_GLOBAL);
    if (!readable_memory(manager_global, sizeof(*manager_global)) ||
        !readable_memory(directory_global, sizeof(*directory_global))) {
        return FALSE;
    }
    *world_manager = *manager_global;
    *world_directory = *directory_global;
    return *world_manager != NULL && *world_directory != NULL;
}

static BOOL capture_party_invulnerability_identity(
    void *character_pointer,
    void *world_manager,
    void *world_directory,
    SudekiMpPartyInvulnerabilityLease *identity
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *arbiter;

    if (identity == NULL ||
        !readable_memory(
            character,
            CHARACTER_ARBITER_OFFSET + sizeof(arbiter))) {
        return FALSE;
    }
    arbiter = *(uint8_t **)(character + CHARACTER_ARBITER_OFFSET);
    if (!readable_memory(
            arbiter,
            ARBITER_INVULNERABILITY_REF_OFFSET + 1u) ||
        !writable_memory(
            arbiter + ARBITER_FLAGS_OFFSET,
            ARBITER_INVULNERABILITY_REF_OFFSET + 1u -
                ARBITER_FLAGS_OFFSET) ||
        *(void **)(arbiter + ARBITER_OWNER_OFFSET) != character) {
        return FALSE;
    }
    ZeroMemory(identity, sizeof(*identity));
    identity->character = character;
    identity->arbiter = arbiter;
    identity->position = *(void **)(character + CHARACTER_POSITION_OFFSET);
    identity->combat_data = *(
        void **)(character + CHARACTER_COMBAT_DATA_OFFSET);
    identity->world_manager = world_manager;
    identity->world_directory = world_directory;
    identity->actor = -1;
    return TRUE;
}

static BOOL same_party_invulnerability_identity(
    const SudekiMpPartyInvulnerabilityLease *left,
    const SudekiMpPartyInvulnerabilityLease *right
) {
    return left != NULL && right != NULL &&
        left->character == right->character &&
        left->arbiter == right->arbiter &&
        left->position == right->position &&
        left->combat_data == right->combat_data &&
        left->world_manager == right->world_manager &&
        left->world_directory == right->world_directory;
}

static BOOL capture_current_party_invulnerability_identities(
    SudekiMpPartyInvulnerabilityLease identities[PARTY_SLOT_COUNT],
    unsigned int *identity_count
) {
    uint8_t *group;
    void *world_manager;
    void *world_directory;
    int party_count;
    unsigned int index;

    if (identities == NULL || identity_count == NULL ||
        get_group_players == NULL ||
        !party_invulnerability_world(&world_manager, &world_directory)) {
        return FALSE;
    }
    group = (uint8_t *)get_group_players();
    if (!readable_memory(
            group,
            PARTY_COUNT_OFFSET + sizeof(party_count))) {
        return FALSE;
    }
    party_count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (party_count < 0 || party_count > (int)PARTY_SLOT_COUNT) {
        return FALSE;
    }
    *identity_count = 0u;
    for (index = 0u; index < (unsigned int)party_count; ++index) {
        void *character = *(void **)(
            group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE);
        SudekiMpPartyInvulnerabilityLease candidate;
        unsigned int prior;

        if (!capture_party_invulnerability_identity(
                character,
                world_manager,
                world_directory,
                &candidate)) {
            return FALSE;
        }
        for (prior = 0u; prior < *identity_count; ++prior) {
            if (same_party_invulnerability_identity(
                    &candidate,
                    &identities[prior])) {
                break;
            }
        }
        if (prior != *identity_count) {
            return FALSE;
        }
        identities[*identity_count] = candidate;
        ++*identity_count;
    }
    return TRUE;
}

static int party_invulnerability_actor(void *character) {
    int actor;

    for (actor = 0; actor < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++actor) {
        if (actor_pointer((SudekiMpCleanroomActor)actor) == character) {
            return actor;
        }
    }
    return -1;
}

static int find_party_invulnerability_identity(
    const SudekiMpPartyInvulnerabilityLease *identity,
    const SudekiMpPartyInvulnerabilityLease identities[PARTY_SLOT_COUNT],
    unsigned int identity_count
) {
    unsigned int index;

    for (index = 0u; index < identity_count; ++index) {
        if (same_party_invulnerability_identity(identity, &identities[index])) {
            return (int)index;
        }
    }
    return -1;
}

static BOOL party_invulnerability_lease_resolves(
    const SudekiMpPartyInvulnerabilityLease *lease,
    const SudekiMpPartyInvulnerabilityLease identities[PARTY_SLOT_COUNT],
    unsigned int identity_count
) {
    SudekiMpPartyInvulnerabilityLease resolved;
    void *world_manager;
    void *world_directory;

    if (find_party_invulnerability_identity(
            lease, identities, identity_count) >= 0) {
        return TRUE;
    }
    if (lease == NULL || lease->actor < 0 ||
        lease->actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT ||
        !party_invulnerability_world(&world_manager, &world_directory) ||
        lease->world_manager != world_manager ||
        lease->world_directory != world_directory ||
        actor_pointer((SudekiMpCleanroomActor)lease->actor) !=
            lease->character ||
        !capture_party_invulnerability_identity(
            lease->character,
            world_manager,
            world_directory,
            &resolved)) {
        return FALSE;
    }
    return same_party_invulnerability_identity(lease, &resolved);
}

static BOOL party_invulnerability_native_state_present(
    const SudekiMpPartyInvulnerabilityLease *lease
) {
    SudekiMpPartyInvulnerabilityLease resolved;
    const uint8_t *arbiter;
    int reference_count;
    uint32_t flags;

    if (lease == NULL || !lease->owned ||
        !capture_party_invulnerability_identity(
            lease->character,
            lease->world_manager,
            lease->world_directory,
            &resolved) ||
        !same_party_invulnerability_identity(lease, &resolved)) {
        return FALSE;
    }
    arbiter = (const uint8_t *)lease->arbiter;
    reference_count = (int)*(
        const int8_t *)(arbiter + ARBITER_INVULNERABILITY_REF_OFFSET);
    flags = *(const uint32_t *)(arbiter + ARBITER_FLAGS_OFFSET);
    return reference_count > 0 &&
        (flags & ARBITER_INVULNERABILITY_FLAG) != 0u;
}

static BOOL acquire_party_invulnerability_lease(
    const SudekiMpPartyInvulnerabilityLease *identity,
    SudekiMpPartyInvulnerabilityLease *lease
) {
    uint8_t *arbiter;
    int before;
    int after;
    int rollback_after;
    uint32_t flags_before;
    uint32_t flags_after;
    uint32_t rollback_flags;
    BOOL increment_proven;
    BOOL rollback_confirmed;

    if (identity == NULL || lease == NULL ||
        arbiter_set_invulnerable == NULL) {
        return FALSE;
    }
    arbiter = (uint8_t *)identity->arbiter;
    before = (int)*(
        int8_t *)(arbiter + ARBITER_INVULNERABILITY_REF_OFFSET);
    flags_before = *(uint32_t *)(arbiter + ARBITER_FLAGS_OFFSET);
    if (before < 0 || before == INT8_MAX ||
        ((before > 0) !=
            ((flags_before & ARBITER_INVULNERABILITY_FLAG) != 0u))) {
        SudekiMpLogFormat(
            "cleanroom_engine event=party_invulnerability "
            "action=acquire status=rejected character=%p arbiter=%p "
            "ref_before=%d flags_before=0x%08lx "
            "reason=invalid_native_state\r\n",
            identity->character,
            identity->arbiter,
            before,
            (unsigned long)flags_before
        );
        return FALSE;
    }
    ZeroMemory(lease, sizeof(*lease));
    arbiter_set_invulnerable(arbiter, TRUE);
    after = (int)*(
        int8_t *)(arbiter + ARBITER_INVULNERABILITY_REF_OFFSET);
    flags_after = *(uint32_t *)(arbiter + ARBITER_FLAGS_OFFSET);
    increment_proven = after == before + 1;
    if (increment_proven &&
        (flags_after & ARBITER_INVULNERABILITY_FLAG) != 0u) {
        *lease = *identity;
        lease->actor = party_invulnerability_actor(identity->character);
        lease->owned = TRUE;
        SudekiMpLogFormat(
            "cleanroom_engine event=party_invulnerability action=acquire "
            "status=confirmed actor=%s character=%p arbiter=%p "
            "ref_before=%d ref_after=%d flags_after=0x%08lx "
            "policy=native_refcount_lease\r\n",
            lease->actor >= 0 ? actor_labels[lease->actor] : "Unknown",
            lease->character,
            lease->arbiter,
            before,
            after,
            (unsigned long)flags_after
        );
        return TRUE;
    }
    if (!increment_proven) {
        SudekiMpLogFormat(
            "cleanroom_engine event=party_invulnerability action=acquire "
            "status=verification_failed character=%p arbiter=%p "
            "ref_before=%d ref_after=%d flags_after=0x%08lx "
            "rollback=not_attempted reason=increment_not_proven\r\n",
            identity->character,
            identity->arbiter,
            before,
            after,
            (unsigned long)flags_after
        );
        return FALSE;
    }
    arbiter_set_invulnerable(arbiter, FALSE);
    rollback_after = (int)*(
        int8_t *)(arbiter + ARBITER_INVULNERABILITY_REF_OFFSET);
    rollback_flags = *(uint32_t *)(arbiter + ARBITER_FLAGS_OFFSET);
    rollback_confirmed = rollback_after == before &&
        ((before > 0) ==
            ((rollback_flags & ARBITER_INVULNERABILITY_FLAG) != 0u));
    SudekiMpLogFormat(
        "cleanroom_engine event=party_invulnerability action=acquire "
        "status=verification_failed character=%p arbiter=%p "
        "ref_before=%d ref_after=%d flags_after=0x%08lx "
        "rollback=%s rollback_ref=%d rollback_flags=0x%08lx "
        "reason=flag_not_set_after_proven_increment\r\n",
        identity->character,
        identity->arbiter,
        before,
        after,
        (unsigned long)flags_after,
        rollback_confirmed ? "confirmed" : "verification_failed",
        rollback_after,
        (unsigned long)rollback_flags
    );
    return FALSE;
}

static BOOL release_party_invulnerability_lease(
    SudekiMpPartyInvulnerabilityLease *lease,
    const char *reason
) {
    uint8_t *arbiter;
    int before;
    int after;
    uint32_t flags_before;
    uint32_t flags_after;
    BOOL decrement_proven;
    BOOL state_consistent;

    if (lease == NULL || !lease->owned ||
        arbiter_set_invulnerable == NULL) {
        return lease != NULL && !lease->owned;
    }
    arbiter = (uint8_t *)lease->arbiter;
    before = (int)*(
        int8_t *)(arbiter + ARBITER_INVULNERABILITY_REF_OFFSET);
    flags_before = *(uint32_t *)(arbiter + ARBITER_FLAGS_OFFSET);
    if (before == 0 &&
        (flags_before & ARBITER_INVULNERABILITY_FLAG) == 0u) {
        SudekiMpLogFormat(
            "cleanroom_engine event=party_invulnerability action=release "
            "status=skipped actor=%s character=%p arbiter=%p "
            "ref_before=%d flags_before=0x%08lx "
            "reason=native_lease_no_longer_present\r\n",
            lease->actor >= 0 ? actor_labels[lease->actor] : "Unknown",
            lease->character,
            lease->arbiter,
            before,
            (unsigned long)flags_before
        );
        lease->owned = FALSE;
        return TRUE;
    }
    if (before <= 0 ||
        (flags_before & ARBITER_INVULNERABILITY_FLAG) == 0u) {
        SudekiMpLogFormat(
            "cleanroom_engine event=party_invulnerability action=release "
            "status=verification_failed actor=%s character=%p arbiter=%p "
            "ref_before=%d flags_before=0x%08lx "
            "reason=inconsistent_native_state\r\n",
            lease->actor >= 0 ? actor_labels[lease->actor] : "Unknown",
            lease->character,
            lease->arbiter,
            before,
            (unsigned long)flags_before
        );
        return FALSE;
    }
    arbiter_set_invulnerable(arbiter, FALSE);
    after = (int)*(
        int8_t *)(arbiter + ARBITER_INVULNERABILITY_REF_OFFSET);
    flags_after = *(uint32_t *)(arbiter + ARBITER_FLAGS_OFFSET);
    decrement_proven = after == before - 1;
    state_consistent = (after > 0) ==
        ((flags_after & ARBITER_INVULNERABILITY_FLAG) != 0u);
    SudekiMpLogFormat(
        "cleanroom_engine event=party_invulnerability action=release "
        "status=%s actor=%s character=%p arbiter=%p "
        "ref_before=%d ref_after=%d flags_after=0x%08lx reason=%s\r\n",
        decrement_proven && state_consistent ?
                    "confirmed" : "verification_failed",
        lease->actor >= 0 ? actor_labels[lease->actor] : "Unknown",
        lease->character,
        lease->arbiter,
        before,
        after,
        (unsigned long)flags_after,
        reason == NULL ? "unspecified" : reason
    );
    if (decrement_proven) {
        lease->owned = FALSE;
    }
    return decrement_proven && state_consistent;
}

static BOOL maintain_party_invulnerability_leases(void) {
    SudekiMpPartyInvulnerabilityLease current[PARTY_SLOT_COUNT];
    SudekiMpPartyInvulnerabilityLease next[PARTY_SLOT_COUNT];
    BOOL covered[PARTY_SLOT_COUNT] = {FALSE, FALSE, FALSE, FALSE};
    unsigned int current_count;
    unsigned int next_count = 0u;
    unsigned int index;
    BOOL reconciled = TRUE;

    if (game_base == NULL || arbiter_set_invulnerable == NULL ||
        !capture_current_party_invulnerability_identities(
            current, &current_count)) {
        return FALSE;
    }
    if (party_invulnerability_enabled && current_count == 0u) {
        return FALSE;
    }
    ZeroMemory(next, sizeof(next));
    for (index = 0u; index < party_invulnerability_lease_count; ++index) {
        SudekiMpPartyInvulnerabilityLease lease =
            party_invulnerability_leases[index];
        int current_index = find_party_invulnerability_identity(
            &lease, current, current_count);

        if (party_invulnerability_enabled && current_index >= 0 &&
            party_invulnerability_native_state_present(&lease)) {
            covered[current_index] = TRUE;
            next[next_count++] = lease;
            continue;
        }
        if (party_invulnerability_lease_resolves(
                &lease, current, current_count)) {
            if (!release_party_invulnerability_lease(
                    &lease,
                    party_invulnerability_enabled ?
                        "party_member_removed_or_rebuilt" :
                        "feature_disabled")) {
                reconciled = FALSE;
            }
            if (lease.owned) {
                if (next_count < PARTY_SLOT_COUNT) {
                    next[next_count++] = lease;
                    if (current_index >= 0) {
                        covered[current_index] = TRUE;
                    }
                } else {
                    reconciled = FALSE;
                }
            }
        } else if (lease.owned) {
            SudekiMpLogFormat(
                "cleanroom_engine event=party_invulnerability "
                "action=release status=dropped actor=%s character=%p "
                "arbiter=%p reason=object_identity_unavailable\r\n",
                lease.actor >= 0 ? actor_labels[lease.actor] : "Unknown",
                lease.character,
                lease.arbiter
            );
        }
    }
    if (party_invulnerability_enabled) {
        for (index = 0u; index < current_count; ++index) {
            SudekiMpPartyInvulnerabilityLease lease;

            if (covered[index] || next_count >= PARTY_SLOT_COUNT) {
                if (!covered[index]) {
                    reconciled = FALSE;
                }
                continue;
            }
            if (acquire_party_invulnerability_lease(
                    &current[index], &lease)) {
                next[next_count++] = lease;
                covered[index] = TRUE;
            } else {
                reconciled = FALSE;
            }
        }
    }
    ZeroMemory(
        party_invulnerability_leases,
        sizeof(party_invulnerability_leases)
    );
    memcpy(
        party_invulnerability_leases,
        next,
        next_count * sizeof(next[0])
    );
    party_invulnerability_lease_count = next_count;
    if (party_invulnerability_enabled) {
        for (index = 0u; index < current_count; ++index) {
            int lease_index = find_party_invulnerability_identity(
                &current[index], next, next_count);

            if (lease_index < 0 ||
                !party_invulnerability_native_state_present(
                    &next[lease_index])) {
                reconciled = FALSE;
            }
        }
        return reconciled;
    }
    return reconciled && next_count == 0u;
}

BOOL SudekiMpCleanroomEnginePartyInvulnerable(BOOL *enabled) {
    if (enabled == NULL || game_base == NULL ||
        arbiter_set_invulnerable == NULL) {
        return FALSE;
    }
    *enabled = party_invulnerability_enabled;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineMaintainPartyInvulnerability(BOOL enabled) {
    if (game_base == NULL || arbiter_set_invulnerable == NULL ||
        get_group_players == NULL) {
        return FALSE;
    }
    party_invulnerability_enabled = enabled != FALSE;
    return maintain_party_invulnerability_leases();
}

BOOL SudekiMpCleanroomEngineSetPartyInvulnerable(BOOL enabled) {
    BOOL requested = enabled != FALSE;
    BOOL reconciled;

    if (game_base == NULL || arbiter_set_invulnerable == NULL ||
        get_group_players == NULL) {
        return FALSE;
    }
    party_invulnerability_enabled = requested;
    reconciled = maintain_party_invulnerability_leases();
    SudekiMpLogFormat(
        "cleanroom_engine event=party_invulnerability action=request "
        "state=%s status=%s active_leases=%u "
        "policy=native_refcount_reconciled_current_party\r\n",
        requested ? "enabled" : "disabled",
        reconciled ? "confirmed" : "pending_world",
        party_invulnerability_lease_count
    );
    return reconciled;
}

BOOL SudekiMpCleanroomEngineActorTargetsAllies(
    SudekiMpCleanroomActor actor,
    BOOL *enabled
) {
    uint8_t *character;
    void *targeter;

    if (enabled == NULL || targeter_is_targeting_allies == NULL) {
        return FALSE;
    }
    character = (uint8_t *)actor_pointer(actor);
    if (!readable_memory(character, 0xb0u)) {
        return FALSE;
    }
    targeter = *(void **)(character + 0xacu);
    if (!readable_memory(targeter, 0x80u)) {
        return FALSE;
    }
    *enabled = targeter_is_targeting_allies(targeter) != 0u;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetActorTargetsAllies(
    SudekiMpCleanroomActor actor,
    BOOL enabled
) {
    uint8_t *character;
    void *targeter;
    BOOL actual;

    if (targeter_include_allies == NULL || targeter_remove_allies == NULL) {
        return FALSE;
    }
    character = (uint8_t *)actor_pointer(actor);
    if (!readable_memory(character, 0xb0u)) {
        return FALSE;
    }
    targeter = *(void **)(character + 0xacu);
    if (!readable_memory(targeter, 0x80u)) {
        return FALSE;
    }
    if (enabled) {
        targeter_include_allies(targeter);
    } else {
        targeter_remove_allies(targeter);
    }
    return SudekiMpCleanroomEngineActorTargetsAllies(actor, &actual) &&
        actual == enabled;
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

    if (!readable_memory(
            character,
            CHARACTER_WEAPON_OFFSET + sizeof(void *))) {
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
    /* A native story spawn normally arrives with its authored weapon already
     * equipped. Buki's proven post-Void respawn is the exception: use the
     * existing native per-character setter for only this actor. This does not
     * call FillInventory or grant/unlock any unrelated item. */
    if (set_weapon == NULL) {
        return FALSE;
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

static void log_weapon_item_resources(
    const char *source,
    const uint8_t *item
) {
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_weapon_resource_item "
        "source=%s item=%p item_id=%lu "
        "primary=%08lx,%08lx,%p equipped=%08lx,%08lx,%p\r\n",
        source,
        item,
        (unsigned long)*(const uint32_t *)(item + 0x14u),
        (unsigned long)*(const uint32_t *)(item + 0x44u),
        (unsigned long)*(const uint32_t *)(item + 0x48u),
        *(void *const *)(item + 0x4cu),
        (unsigned long)*(const uint32_t *)(item + 0x104u),
        (unsigned long)*(const uint32_t *)(item + 0x108u),
        *(void *const *)(item + 0x10cu)
    );
}

static uint8_t *global_item(unsigned int item_identifier) {
    void **item_database_global;
    uint8_t *item_database;

    if (game_base == NULL || item_identifier > 0x3e6u) {
        return NULL;
    }
    item_database_global = (void **)(game_base + RVA_ITEM_DATABASE_GLOBAL);
    if (!readable_memory(item_database_global, sizeof(*item_database_global)) ||
        !readable_memory(*item_database_global, 0x1000u)) {
        return NULL;
    }
    item_database = (uint8_t *)*item_database_global;
    return *(uint8_t **)(
        item_database + 0x0cu + item_identifier * sizeof(void *)
    );
}

static BOOL inspect_cafu_inventory_weapon(void) {
    uint8_t *cafu_item;
    uint8_t *elco_item;

    if (cafu_weapon_inventory_logged) {
        return TRUE;
    }
    if (!inventory_filled) {
        if ((cafu_weapon_gate_log & 0x01u) == 0u) {
            cafu_weapon_gate_log |= 0x01u;
            SudekiMpLogWrite(
                "cleanroom_engine event=cafu_weapon_resource_inventory "
                "status=pending reason=inventory_not_ready\r\n"
            );
        }
        return FALSE;
    }
    cafu_item = global_item(CAFU_WEAPON_ITEM_ID);
    elco_item = global_item(ELCO_STARTER_WEAPON_ITEM_ID);
    if (cafu_item == NULL && elco_item == NULL) {
        if ((cafu_weapon_gate_log & 0x02u) == 0u) {
            cafu_weapon_gate_log |= 0x02u;
            SudekiMpLogWrite(
                "cleanroom_engine event=cafu_weapon_resource_inventory "
                "status=pending reason=item_database_unreadable\r\n"
            );
        }
        return FALSE;
    }
    if (!readable_memory(cafu_item, 0x110u)) {
        if ((cafu_weapon_gate_log & 0x04u) == 0u) {
            cafu_weapon_gate_log |= 0x04u;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_resource_inventory "
                "status=rejected reason=cafu_database_item_unreadable "
                "global_item_id=%u item=%p\r\n",
                CAFU_WEAPON_ITEM_ID,
                cafu_item
            );
        }
        return FALSE;
    }
    if (!readable_memory(elco_item, 0x110u)) {
        if ((cafu_weapon_gate_log & 0x10u) == 0u) {
            cafu_weapon_gate_log |= 0x10u;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_resource_inventory "
                "status=rejected reason=control_database_item_unreadable "
                "global_item_id=%u item=%p\r\n",
                ELCO_STARTER_WEAPON_ITEM_ID,
                elco_item
            );
        }
        return FALSE;
    }
    log_weapon_item_resources("cafu_global_item_48", cafu_item);
    log_weapon_item_resources("elco_global_item_24", elco_item);
    cafu_weapon_inventory_logged = TRUE;
    SudekiMpLogWrite(
        "cleanroom_engine event=cafu_weapon_resource_inventory "
        "status=observed policy=no_resource_or_weapon_mutation\r\n"
    );
    return TRUE;
}

static BOOL __attribute__((noinline)) request_cafu_weapon_model(
    const SudekiMpResourceName *resource_name
) {
    void **manager_global;
    void *manager;
    void *function;
    void *output;
    void *result;
    uint32_t name_kind;
    uint32_t name_identifier;
    uint32_t *name_text;
    BOOL entry_matches;
    BOOL global_readable;
    BOOL manager_readable;
    void *type_manager;
    void **type_vtable;
    void **proxy_vtable;

    if (cafu_weapon_preload_requested) {
        return cafu_weapon_preload.resource_proxy != NULL;
    }
    manager_global = (void **)(game_base + RVA_ENTITY_MANAGER_GLOBAL);
    entry_matches = matches_entry(
        game_base + RVA_RESOURCE_LOOKUP,
        resource_lookup_entry,
        sizeof(resource_lookup_entry)
    );
    global_readable = readable_memory(
        manager_global,
        sizeof(*manager_global)
    );
    manager = global_readable ? *manager_global : NULL;
    /* Type 41 indexes manager+0x40+41*4, ending at manager+0xE8. */
    manager_readable = readable_memory(manager, 0xe8u);
    if (resource_name == NULL || !entry_matches || !global_readable ||
        !manager_readable) {
        if ((cafu_weapon_gate_log & 0x80u) == 0u) {
            cafu_weapon_gate_log |= 0x80u;
            SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_model_preload "
                "status=pending reason=lookup_gate_failed "
                "resource_name=%p entry_match=%s manager_global=%p "
                "global_readable=%s manager=%p manager_readable=%s\r\n",
                resource_name,
                entry_matches ? "true" : "false",
                manager_global,
                global_readable ? "true" : "false",
                manager,
                manager_readable ? "true" : "false"
            );
        }
        return FALSE;
    }
    function = game_base + RVA_RESOURCE_LOOKUP;
    output = &cafu_weapon_preload;
    name_kind = resource_name->encoded_kind;
    name_identifier = resource_name->identifier;
    name_text = resource_name->text_reference;
    ZeroMemory(&cafu_weapon_preload, sizeof(cafu_weapon_preload));
    __asm__ volatile(
        "pushl %[name_text]\n\t"
        "pushl %[name_identifier]\n\t"
        "pushl %[name_kind]\n\t"
        "pushl %[manager]\n\t"
        "movl %[output], %%esi\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "xorl %%edx, %%edx\n\t"
        "movl %[function], %%eax\n\t"
        "call *%%eax"
        : "=a"(result)
        : [name_text] "m"(name_text),
          [name_identifier] "m"(name_identifier),
          [name_kind] "m"(name_kind),
          [manager] "m"(manager),
          [output] "m"(output),
          [function] "m"(function)
        : "ecx", "edx", "esi", "memory", "cc"
    );
    cafu_weapon_preload_requested = TRUE;
    cafu_weapon_preload_started_at = GetTickCount();
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_weapon_model_preload phase=request "
        "status=%s encoded_kind=%08lx identifier=%08lx "
        "result=%p lookup=%p reference=%p proxy=%p\r\n",
        result == &cafu_weapon_preload &&
            cafu_weapon_preload.resource_proxy != NULL ?
                "retained" : "unavailable",
        (unsigned long)resource_name->encoded_kind,
        (unsigned long)resource_name->identifier,
        result,
        &cafu_weapon_preload,
        cafu_weapon_preload.reference,
        cafu_weapon_preload.resource_proxy
    );
    type_manager = *(void **)((uint8_t *)manager + 0xe4u);
    type_vtable = readable_memory(type_manager, sizeof(void *)) ?
        *(void ***)type_manager : NULL;
    proxy_vtable = readable_memory(
        cafu_weapon_preload.resource_proxy,
        sizeof(void *)
    ) ? *(void ***)cafu_weapon_preload.resource_proxy : NULL;
    if (readable_memory(type_vtable, 16u * sizeof(void *))) {
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_model_preload "
            "phase=type_manager manager=%p object=%p vtable=%p "
            "slots_00_1c=%p,%p,%p,%p,%p,%p,%p,%p\r\n",
            manager,
            type_manager,
            type_vtable,
            type_vtable[0], type_vtable[1], type_vtable[2],
            type_vtable[3], type_vtable[4], type_vtable[5],
            type_vtable[6], type_vtable[7]
        );
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_model_preload "
            "phase=type_manager slots_20_3c=%p,%p,%p,%p,%p,%p,%p,%p\r\n",
            type_vtable[8], type_vtable[9], type_vtable[10],
            type_vtable[11], type_vtable[12], type_vtable[13],
            type_vtable[14], type_vtable[15]
        );
    }
    if (readable_memory(proxy_vtable, 8u * sizeof(void *))) {
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_model_preload "
            "phase=proxy object=%p vtable=%p nested=%p "
            "slots_00_1c=%p,%p,%p,%p,%p,%p,%p,%p\r\n",
            cafu_weapon_preload.resource_proxy,
            proxy_vtable,
            *(void **)((uint8_t *)cafu_weapon_preload.resource_proxy + 8u),
            proxy_vtable[0], proxy_vtable[1], proxy_vtable[2],
            proxy_vtable[3], proxy_vtable[4], proxy_vtable[5],
            proxy_vtable[6], proxy_vtable[7]
        );
    }
    return result == &cafu_weapon_preload &&
        cafu_weapon_preload.resource_proxy != NULL;
}

static void *retained_resource_payload(
    const SudekiMpResourceLookup *lookup
) {
    uint8_t *proxy;
    void **proxy_vtable;
    ResourceProxyTypeFunction proxy_type;
    void *loaded_resource;
    void **loaded_vtable;
    LoadedResourceGetFunction get_resource;

    if (lookup == NULL) {
        return NULL;
    }
    proxy = (uint8_t *)lookup->resource_proxy;
    if (!readable_memory(proxy, 3u * sizeof(void *))) {
        return NULL;
    }
    proxy_vtable = *(void ***)proxy;
    if (!readable_memory(proxy_vtable, 5u * sizeof(void *))) {
        return NULL;
    }
    proxy_type = (ResourceProxyTypeFunction)proxy_vtable[4];
    if (!executable_memory((const void *)proxy_type) ||
        proxy_type(proxy) != 41u) {
        return NULL;
    }
    loaded_resource = *(void **)(proxy + 2u * sizeof(void *));
    if (!readable_memory(loaded_resource, sizeof(void *))) {
        return NULL;
    }
    loaded_vtable = *(void ***)loaded_resource;
    if (!readable_memory(loaded_vtable, 3u * sizeof(void *))) {
        return NULL;
    }
    get_resource = (LoadedResourceGetFunction)loaded_vtable[2];
    if (!executable_memory((const void *)get_resource)) {
        return NULL;
    }
    return get_resource(loaded_resource);
}

static BOOL retained_resource_is_type(
    const SudekiMpResourceLookup *lookup,
    unsigned int expected_type
) {
    uint8_t *proxy;
    void **proxy_vtable;
    ResourceProxyTypeFunction proxy_type;

    if (lookup == NULL) {
        return FALSE;
    }
    proxy = (uint8_t *)lookup->resource_proxy;
    if (!readable_memory(proxy, sizeof(void *))) {
        return FALSE;
    }
    proxy_vtable = *(void ***)proxy;
    if (!readable_memory(proxy_vtable, 5u * sizeof(void *))) {
        return FALSE;
    }
    proxy_type = (ResourceProxyTypeFunction)proxy_vtable[4];
    return executable_memory((const void *)proxy_type) &&
        proxy_type(proxy) == expected_type;
}

static void *cafu_weapon_model_payload(void) {
    return retained_resource_payload(&cafu_weapon_preload);
}

static BOOL patch_cafu_missile_model_record(
    SudekiMpResourceName *model,
    uint8_t *combo,
    unsigned int combo_index,
    const char *source
) {
    SudekiMpCafuMissileModelPatch *patch;
    unsigned int index;

    if (!cafu_missile_model_ready || model == NULL ||
        model->encoded_kind != CAFU_MISSILE_MODEL_RESOURCE_KIND ||
        (model->identifier != CAFU_MISSILE_ARCHIVE_MODEL_IDENTIFIER &&
         model->identifier != CAFU_MISSILE_RUNTIME_MODEL_IDENTIFIER) ||
        model->text_reference != NULL ||
        !writable_memory(model, sizeof(*model))) {
        return FALSE;
    }
    for (index = 0u; index < cafu_missile_model_patch_count; ++index) {
        if (cafu_missile_model_patches[index].target == model) {
            return TRUE;
        }
    }
    if (cafu_missile_model_patch_count >= CAFU_MISSILE_PATCH_CAPACITY) {
        return FALSE;
    }
    patch = &cafu_missile_model_patches[cafu_missile_model_patch_count];
    patch->target = model;
    patch->saved = *model;
    patch->applied = *model;
    patch->applied.identifier =
        CAFU_MISSILE_REPLACEMENT_MODEL_IDENTIFIER;
    *model = patch->applied;
    if (memcmp(model, &patch->applied, sizeof(*model)) != 0) {
        *model = patch->saved;
        ZeroMemory(patch, sizeof(*patch));
        return FALSE;
    }
    ++cafu_missile_model_patch_count;
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_missile_model_correction "
        "status=applied source=%s combo_index=%u combo=%p model=%p "
        "original_identifier=%08lx replacement_identifier=%08lx "
        "policy=projectile_presentation_only\r\n",
        source == NULL ? "unknown" : source,
        combo_index,
        combo,
        model,
        (unsigned long)patch->saved.identifier,
        (unsigned long)patch->applied.identifier
    );
    return TRUE;
}

static BOOL __attribute__((noinline)) request_cafu_missile_model(void) {
    void **manager_global;
    void *manager;
    void *function;
    void *output;
    void *result;

    if (cafu_missile_model_preload_requested) {
        return cafu_missile_model_preload.resource_proxy != NULL;
    }
    manager_global = game_base == NULL ? NULL :
        (void **)(game_base + RVA_ENTITY_MANAGER_GLOBAL);
    manager = readable_memory(manager_global, sizeof(*manager_global)) ?
        *manager_global : NULL;
    if (!cafu_missile_model_name_initialized ||
        !matches_entry(
            game_base + RVA_RESOURCE_LOOKUP,
            resource_lookup_entry,
            sizeof(resource_lookup_entry)) ||
        !readable_memory(manager, 0xe8u)) {
        return FALSE;
    }
    function = game_base + RVA_RESOURCE_LOOKUP;
    ZeroMemory(
        &cafu_missile_model_preload,
        sizeof(cafu_missile_model_preload)
    );
    output = &cafu_missile_model_preload;
    __asm__ volatile(
        "pushl %[name_text]\n\t"
        "pushl %[name_identifier]\n\t"
        "pushl %[name_kind]\n\t"
        "pushl %[manager]\n\t"
        "movl %[output], %%esi\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "xorl %%edx, %%edx\n\t"
        "movl %[function], %%eax\n\t"
        "call *%%eax"
        : "=a"(result)
        : [name_text] "m"(cafu_missile_model_name.text_reference),
          [name_identifier] "m"(cafu_missile_model_name.identifier),
          [name_kind] "m"(cafu_missile_model_name.encoded_kind),
          [manager] "rm"(manager),
          [output] "m"(output),
          [function] "m"(function)
        : "ecx", "edx", "esi", "memory", "cc"
    );
    cafu_missile_model_preload_requested = TRUE;
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_missile_model_preload phase=request "
        "status=%s encoded_kind=%08lx identifier=%08lx "
        "result=%p reference=%p proxy=%p "
        "replacement=SFXEP001_MK1_PISTOL.HOM:41\r\n",
        result == &cafu_missile_model_preload &&
            cafu_missile_model_preload.resource_proxy != NULL ?
                "retained" : "unavailable",
        (unsigned long)cafu_missile_model_name.encoded_kind,
        (unsigned long)cafu_missile_model_name.identifier,
        result,
        cafu_missile_model_preload.reference,
        cafu_missile_model_preload.resource_proxy
    );
    return result == &cafu_missile_model_preload &&
        cafu_missile_model_preload.resource_proxy != NULL;
}

static BOOL prepare_cafu_missile_models(void) {
    uint8_t *cafu;
    uint8_t *manager;
    uint8_t **combo_array;
    uint8_t *combo;
    SudekiMpResourceName *model;
    unsigned int combo_count;
    unsigned int index;

    if (!cafu_missile_model_name_initialized) {
        ZeroMemory(
            &cafu_missile_model_name,
            sizeof(cafu_missile_model_name)
        );
        cafu_missile_model_name.encoded_kind =
            CAFU_MISSILE_MODEL_RESOURCE_KIND;
        cafu_missile_model_name.identifier =
            CAFU_MISSILE_REPLACEMENT_MODEL_IDENTIFIER;
        cafu_missile_model_name_initialized = TRUE;
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_missile_model_correction "
            "status=prepared encoded_kind=%08lx "
            "original_identifier=%08lx replacement_identifier=%08lx "
            "replacement=SFXEP001_MK1_PISTOL.HOM:41\r\n",
            (unsigned long)cafu_missile_model_name.encoded_kind,
            (unsigned long)CAFU_MISSILE_RUNTIME_MODEL_IDENTIFIER,
            (unsigned long)cafu_missile_model_name.identifier
        );
    }
    if (!cafu_missile_model_preload_requested) {
        (void)request_cafu_missile_model();
        return FALSE;
    }
    if (!retained_resource_is_type(&cafu_missile_model_preload, 41u)) {
        return FALSE;
    }
    if (!cafu_missile_model_ready) {
        cafu_missile_model_ready = TRUE;
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_missile_model_correction "
            "status=preload_ready encoded_kind=%08lx identifier=%08lx "
            "policy=spawn_blocked_until_replacement_available\r\n",
            (unsigned long)cafu_missile_model_name.encoded_kind,
            (unsigned long)cafu_missile_model_name.identifier
        );
    }
    cafu = (uint8_t *)actor_pointer(SUDEKIMP_CLEANROOM_CAFU);
    if (!readable_memory(
            cafu,
            CAFU_MISSILE_MANAGER_OFFSET + 0xe4u)) {
        return TRUE;
    }
    manager = cafu + CAFU_MISSILE_MANAGER_OFFSET;
    if (*(void **)manager != game_base + RVA_MISSILE_MANAGER_VTABLE) {
        return TRUE;
    }
    combo_count = *(unsigned int *)(
        manager + CAFU_MISSILE_COMBO_COUNT_OFFSET
    );
    combo_array = *(uint8_t ***) (
        manager + CAFU_MISSILE_COMBO_ARRAY_OFFSET
    );
    if (combo_count == 0u || combo_count > CAFU_MISSILE_PATCH_CAPACITY ||
        !readable_memory(combo_array, combo_count * sizeof(*combo_array))) {
        return TRUE;
    }
    for (index = 0u; index < combo_count; ++index) {
        combo = combo_array[index];
        model = readable_memory(
            combo,
            CAFU_MISSILE_MODEL_OFFSET + sizeof(*model)) ?
                (SudekiMpResourceName *)(
                    combo + CAFU_MISSILE_MODEL_OFFSET
                ) : NULL;
        (void)patch_cafu_missile_model_record(
            model,
            combo,
            index,
            "manager_inventory"
        );
    }
    return TRUE;
}

static BOOL prepare_cafu_weapon_model(void) {
    uint8_t *cafu_item;
    uint8_t *elco_item;
    SudekiMpResourceName *cafu_model;
    const SudekiMpResourceName *elco_model;
    SudekiMpResourceName archive_model;
    void *payload;
    DWORD elapsed;

    if (cafu_weapon_model_ready) {
        return TRUE;
    }
    if (!inspect_cafu_inventory_weapon()) {
        return FALSE;
    }
    cafu_item = global_item(CAFU_WEAPON_ITEM_ID);
    elco_item = global_item(ELCO_STARTER_WEAPON_ITEM_ID);
    if (!readable_memory(cafu_item, 0x110u) ||
        !readable_memory(elco_item, 0x110u)) {
        return FALSE;
    }
    cafu_model = (SudekiMpResourceName *)(cafu_item + 0x44u);
    elco_model = (const SudekiMpResourceName *)(elco_item + 0x44u);
    if (!cafu_weapon_force_elco_model && !cafu_weapon_preload_requested) {
        if (cafu_model->identifier !=
                CAFU_WEAPON_BROKEN_MODEL_IDENTIFIER ||
            (cafu_model->encoded_kind & 0x7fu) != 41u ||
            cafu_model->text_reference != NULL) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_model_correction "
                "status=rejected reason=source_identity_mismatch "
                "kind=%08lx identifier=%08lx reference=%p\r\n",
                (unsigned long)cafu_model->encoded_kind,
                (unsigned long)cafu_model->identifier,
                cafu_model->text_reference
            );
            return FALSE;
        }
        archive_model = *cafu_model;
        archive_model.identifier = CAFU_WEAPON_ARCHIVE_MODEL_IDENTIFIER;
        (void)request_cafu_weapon_model(&archive_model);
        return FALSE;
    }
    payload = cafu_weapon_force_elco_model ? NULL :
        cafu_weapon_model_payload();
    if (!cafu_weapon_force_elco_model && payload != NULL) {
        if (!writable_memory(cafu_model, sizeof(*cafu_model)) ||
            cafu_model->identifier !=
                CAFU_WEAPON_BROKEN_MODEL_IDENTIFIER ||
            (cafu_model->encoded_kind & 0x7fu) != 41u ||
            cafu_model->text_reference != NULL) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_model_correction "
                "status=rejected reason=apply_identity_mismatch "
                "kind=%08lx identifier=%08lx reference=%p\r\n",
                (unsigned long)cafu_model->encoded_kind,
                (unsigned long)cafu_model->identifier,
                cafu_model->text_reference
            );
            return FALSE;
        }
        saved_cafu_weapon_model = *cafu_model;
        applied_cafu_weapon_model = *cafu_model;
        applied_cafu_weapon_model.identifier =
            CAFU_WEAPON_ARCHIVE_MODEL_IDENTIFIER;
        *cafu_model = applied_cafu_weapon_model;
        if (memcmp(cafu_model, &applied_cafu_weapon_model,
                sizeof(*cafu_model)) != 0) {
            *cafu_model = saved_cafu_weapon_model;
            ZeroMemory(
                &saved_cafu_weapon_model,
                sizeof(saved_cafu_weapon_model)
            );
            ZeroMemory(
                &applied_cafu_weapon_model,
                sizeof(applied_cafu_weapon_model)
            );
            SudekiMpLogWrite(
                "cleanroom_engine event=cafu_weapon_model_correction "
                "status=rejected reason=write_verification_failed\r\n"
            );
            return FALSE;
        }
        cafu_weapon_model_item = cafu_item;
        cafu_weapon_model_patched = TRUE;
        cafu_weapon_model_uses_fallback = FALSE;
        cafu_weapon_model_ready = TRUE;
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_model_preload phase=ready "
            "status=native_model_available proxy=%p payload=%p "
            "broken_identifier=%08lx archive_identifier=%08lx "
            "policy=actual_w033_archive_resource\r\n",
            cafu_weapon_preload.resource_proxy,
            payload,
            (unsigned long)saved_cafu_weapon_model.identifier,
            (unsigned long)applied_cafu_weapon_model.identifier
        );
        return TRUE;
    }
    elapsed = cafu_weapon_force_elco_model ?
        CAFU_WEAPON_PRELOAD_WAIT_MS :
        GetTickCount() - cafu_weapon_preload_started_at;
    if (!cafu_weapon_force_elco_model &&
        elapsed < CAFU_WEAPON_PRELOAD_WAIT_MS) {
        return FALSE;
    }
    if (!writable_memory(cafu_model, sizeof(*cafu_model)) ||
        cafu_model->identifier != CAFU_WEAPON_BROKEN_MODEL_IDENTIFIER ||
        elco_model->identifier != ELCO_WEAPON_MODEL_IDENTIFIER ||
        (cafu_model->encoded_kind & 0x7fu) != 41u ||
        (elco_model->encoded_kind & 0x7fu) != 41u ||
        cafu_model->text_reference != NULL ||
        elco_model->text_reference != NULL) {
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_weapon_model_alias status=rejected "
            "reason=resource_identity_mismatch cafu_item=%p elco_item=%p "
            "cafu_kind=%08lx cafu_identifier=%08lx cafu_ref=%p "
            "elco_kind=%08lx elco_identifier=%08lx elco_ref=%p\r\n",
            cafu_item,
            elco_item,
            (unsigned long)cafu_model->encoded_kind,
            (unsigned long)cafu_model->identifier,
            cafu_model->text_reference,
            (unsigned long)elco_model->encoded_kind,
            (unsigned long)elco_model->identifier,
            elco_model->text_reference
        );
        return FALSE;
    }
    saved_cafu_weapon_model = *cafu_model;
    applied_cafu_weapon_model = *elco_model;
    *cafu_model = applied_cafu_weapon_model;
    if (memcmp(cafu_model, &applied_cafu_weapon_model,
            sizeof(*cafu_model)) != 0) {
        *cafu_model = saved_cafu_weapon_model;
        ZeroMemory(&saved_cafu_weapon_model, sizeof(saved_cafu_weapon_model));
        ZeroMemory(&applied_cafu_weapon_model,
            sizeof(applied_cafu_weapon_model));
        SudekiMpLogWrite(
            "cleanroom_engine event=cafu_weapon_model_alias status=rejected "
            "reason=write_verification_failed\r\n"
        );
        return FALSE;
    }
    cafu_weapon_model_item = cafu_item;
    cafu_weapon_model_patched = TRUE;
    cafu_weapon_model_uses_fallback = TRUE;
    cafu_weapon_model_ready = TRUE;
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_weapon_model_alias status=applied "
        "item=%p preserved_item_id=%u original_identifier=%08lx "
        "replacement_identifier=%08lx preload_wait_ms=%lu "
        "policy=visual_resource_only\r\n",
        cafu_item,
        CAFU_WEAPON_ITEM_ID,
        (unsigned long)saved_cafu_weapon_model.identifier,
        (unsigned long)applied_cafu_weapon_model.identifier,
        (unsigned long)elapsed
    );
    return TRUE;
}

static BOOL inspect_cafu_weapon_resources(uint8_t *character) {
    uint8_t *character_weapon;
    uint8_t *current_item;

    if (!inspect_cafu_inventory_weapon()) {
        return FALSE;
    }
    if (!readable_memory(character, CHARACTER_WEAPON_OFFSET + sizeof(void *))) {
        if ((cafu_weapon_gate_log & 0x08u) == 0u) {
            cafu_weapon_gate_log |= 0x08u;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_runtime_item status=pending "
                "reason=character_unreadable character=%p\r\n",
                character
            );
        }
        return FALSE;
    }
    character_weapon = *(uint8_t **)(character + CHARACTER_WEAPON_OFFSET);
    if (character_weapon == NULL || !readable_memory(
            character_weapon,
            CHARACTER_WEAPON_CURRENT_ITEM_OFFSET + sizeof(void *))) {
        if ((cafu_weapon_gate_log & 0x20u) == 0u) {
            cafu_weapon_gate_log |= 0x20u;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_runtime_item status=pending "
                "reason=character_weapon_unreadable character=%p weapon=%p\r\n",
                character,
                character_weapon
            );
        }
        return FALSE;
    }
    current_item = *(uint8_t **)(
        character_weapon + CHARACTER_WEAPON_CURRENT_ITEM_OFFSET
    );
    if (!readable_memory(current_item, 0x110u)) {
        if ((cafu_weapon_gate_log & 0x40u) == 0u) {
            cafu_weapon_gate_log |= 0x40u;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_runtime_item status=pending "
                "reason=current_item_unreadable character=%p weapon=%p item=%p\r\n",
                character,
                character_weapon,
                current_item
            );
        }
        return FALSE;
    }
    log_weapon_item_resources("cafu_runtime_current", current_item);
    return TRUE;
}

static void maintain_cafu_player_one(void) {
    uint8_t *group;
    uint8_t **controller_global;
    uint8_t *controller;
    void *cafu;
    void *controller_target;
    uint8_t *next_action;
    uint8_t *previous_action;
    unsigned int index;
    unsigned int cafu_slot = PARTY_SLOT_COUNT;
    int party_count;

    if (!cafu_probe_requested || cafu_player_one_confirmed ||
        game_base == NULL) {
        return;
    }
    cafu = actor_pointer(SUDEKIMP_CLEANROOM_CAFU);
    group = (uint8_t *)get_group_players();
    controller_global = (uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL
    );
    if (cafu == NULL || !readable_memory(group,
            PARTY_COUNT_OFFSET + sizeof(party_count)) ||
        !readable_memory(controller_global, sizeof(*controller_global))) {
        return;
    }
    controller = *controller_global;
    if (!readable_memory(controller,
            CONTROLLER_TARGET_OFFSET + sizeof(controller_target))) {
        return;
    }
    controller_target = *(void **)(controller + CONTROLLER_TARGET_OFFSET);
    if (controller_target == cafu) {
        cafu_player_one_confirmed = TRUE;
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_player_one status=confirmed "
            "character=%p controller=%p rotations=%u "
            "policy=native_character_switch\r\n",
            cafu,
            controller,
            cafu_player_one_rotation_attempts
        );
        return;
    }
    party_count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (party_count < 1 || party_count > (int)PARTY_SLOT_COUNT ||
        !readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE
        )) {
        return;
    }
    for (index = 0u; index < (unsigned int)party_count; ++index) {
        if (*(void **)(group + PARTY_SLOT_ZERO_OFFSET +
                index * PARTY_SLOT_STRIDE) == cafu) {
            cafu_slot = index;
            break;
        }
    }
    if (cafu_slot == PARTY_SLOT_COUNT) {
        return;
    }
    if (cafu_player_one_rotation_attempts >=
            CAFU_PLAYER_ONE_MAX_ROTATIONS) {
        if (cafu_player_one_rotation_attempts ==
                CAFU_PLAYER_ONE_MAX_ROTATIONS) {
            ++cafu_player_one_rotation_attempts;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_player_one status=rejected "
                "reason=rotation_limit character=%p slot=%u target=%p\r\n",
                cafu,
                cafu_slot,
                controller_target
            );
        }
        return;
    }
    next_action = controller + CONTROLLER_NEXT_CHARACTER_OFFSET;
    previous_action = controller + CONTROLLER_PREVIOUS_CHARACTER_OFFSET;
    if (!writable_memory(next_action, sizeof(*next_action)) ||
        !writable_memory(previous_action, sizeof(*previous_action)) ||
        *next_action != 0u || *previous_action != 0u) {
        return;
    }
    *next_action = 1u;
    ++cafu_player_one_rotation_attempts;
    SudekiMpLogFormat(
        "cleanroom_engine event=cafu_player_one phase=rotate "
        "character=%p slot=%u target=%p attempt=%u "
        "policy=native_next_character_action\r\n",
        cafu,
        cafu_slot,
        controller_target,
        cafu_player_one_rotation_attempts
    );
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

BOOL SudekiMpCleanroomEngineInitializePartyActor(
    SudekiMpCleanroomActor actor
) {
    uint8_t *character;
    BOOL stats_ready;
    BOOL weapon_ready;
    BOOL combat_enabled;

    if (actor < SUDEKIMP_CLEANROOM_TAL ||
        actor > SUDEKIMP_CLEANROOM_AILISH) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    character = (uint8_t *)actor_pointer(actor);
    if (character == NULL) {
        initialized_actor_entities[actor] = NULL;
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    if (initialized_actor_entities[actor] == character) {
        return TRUE;
    }
    stats_ready = repair_actor_stat_maxima(actor);
    weapon_ready = initialize_actor_weapon(actor, character);
    if (!stats_ready || !weapon_ready) {
        return FALSE;
    }
    initialized_actor_entities[actor] = character;
    SudekiMpLogFormat(
        "cleanroom_engine event=actor_training_setup actor=%s "
        "status=complete entity=%p scope=single_party_actor\r\n",
        actor_labels[actor],
        character
    );
    /*
     * A PC spawned after the group entered combat is not included in the
     * already-completed native arm pass. Re-run that native pass once for
     * this newly initialized actor; never toggle combat or synthesize the
     * actor's combat state locally.
     */
    if (SudekiMpCleanroomEngineCombatMode(&combat_enabled) &&
        combat_enabled) {
        (void)invoke_combat_transition(
            TRUE,
            TRUE,
            "new_actor_initialized_during_combat",
            actor_labels[actor]
        );
    }
    return TRUE;
}

static void initialize_present_actors(void) {
    unsigned int index;
    uint8_t *character;

    for (index = 0u; index < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++index) {
        character = (uint8_t *)actor_pointer((SudekiMpCleanroomActor)index);
        if (character == NULL) {
            initialized_actor_entities[index] = NULL;
            continue;
        }
        if (initialized_actor_entities[index] == character) {
            continue;
        }
        if (index == SUDEKIMP_CLEANROOM_CAFU) {
            /*
             * Cafu is an authored developer PC, but he is not part of the
             * retail four-character inventory/HUD contract.  The first
             * isolation path avoids retail stat, combat, and controller
             * assumptions.  His hidden item 48 remains the equipped weapon;
             * only its stale visual-model resource hash is corrected.  The
             * compatible Elco visual remains a load-failure fallback.
             */
            if (inspect_cafu_weapon_resources(character)) {
                initialized_actor_entities[index] = character;
                SudekiMpLogFormat(
                    "cleanroom_engine event=actor_training_setup actor=Cafu "
                    "status=observed entity=%p "
                    "policy=%s\r\n",
                    character,
                    cafu_weapon_model_uses_fallback ?
                        "authored_item_with_visual_resource_alias" :
                        "authored_item_with_actual_w033_resource"
                );
            }
            continue;
        }
        (void)SudekiMpCleanroomEngineInitializePartyActor(
            (SudekiMpCleanroomActor)index
        );
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
    void *resolved_elco_get_fuel = NULL;
    void *resolved_elco_set_fuel = NULL;
    void *resolved_get_character_number_stat = NULL;
    void *resolved_set_character_number_stat = NULL;
    void *resolved_set_weapon = NULL;
    void *resolved_targeter_include_allies = NULL;
    void *resolved_targeter_remove_allies = NULL;
    void *resolved_targeter_is_targeting_allies = NULL;
    void *resolved_arbiter_set_invulnerable = NULL;
    void *resolved_set_master_game_speed = NULL;
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
            "?GetFuel@CElcoAbility@@QAEMXZ",
            RVA_ELCO_GET_FUEL,
            &resolved_elco_get_fuel) ||
        !resolve_exact_export(
            game_module,
            "?SetFuel@CElcoAbility@@QAEXM@Z",
            RVA_ELCO_SET_FUEL,
            &resolved_elco_set_fuel) ||
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
        !resolve_exact_export(
            game_module,
            "?IncludeAlliesAsTargets@CTargeter@@QAEXXZ",
            RVA_TARGETER_INCLUDE_ALLIES,
            &resolved_targeter_include_allies) ||
        !resolve_exact_export(
            game_module,
            "?RemoveAlliesAsTargets@CTargeter@@QAEXXZ",
            RVA_TARGETER_REMOVE_ALLIES,
            &resolved_targeter_remove_allies) ||
        !resolve_exact_export(
            game_module,
            "?IsTargettingAllies@CTargeter@@QBE_NXZ",
            RVA_TARGETER_IS_TARGETING_ALLIES,
            &resolved_targeter_is_targeting_allies) ||
        !resolve_exact_export(
            game_module,
            "?GELSetInvulnerable@CCharacterArbiter@@QAEX_N@Z",
            RVA_ARBITER_SET_INVULNERABLE,
            &resolved_arbiter_set_invulnerable) ||
        !resolve_exact_export(
            game_module,
            "?SetMasterGameSpeed@@YAXM@Z",
            RVA_SET_MASTER_GAME_SPEED,
            &resolved_set_master_game_speed) ||
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
            sizeof(set_ui_active_entry)) ||
        !matches_entry(
            (const uint8_t *)resolved_elco_get_fuel,
            elco_get_fuel_entry,
            sizeof(elco_get_fuel_entry)) ||
        !matches_entry(
            (const uint8_t *)resolved_elco_set_fuel,
            elco_set_fuel_entry,
            sizeof(elco_set_fuel_entry)) ||
        !matches_entry(
            (const uint8_t *)resolved_arbiter_set_invulnerable,
            arbiter_set_invulnerable_entry,
            sizeof(arbiter_set_invulnerable_entry)) ||
        !matches_set_master_game_speed_entry(
            (const uint8_t *)resolved_set_master_game_speed,
            (const uint8_t *)game_module)) {
        SudekiMpCleanroomEngineReset();
        return FALSE;
    }

    base = (uint8_t *)game_module;
    if (!writable_memory(base + RVA_NO_SP_NEEDED_FLAG, 1u) ||
        !writable_memory(base + RVA_NO_SSP_NEEDED_FLAG, 1u) ||
        !writable_memory(base + RVA_MASTER_GAME_SPEED, sizeof(float))) {
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
    elco_get_fuel = (ElcoGetFuelFunction)resolved_elco_get_fuel;
    elco_set_fuel = (ElcoSetFuelFunction)resolved_elco_set_fuel;
    get_character_number_stat =
        (GetCharacterNumberStatFunction)resolved_get_character_number_stat;
    set_character_number_stat =
        (SetCharacterNumberStatFunction)resolved_set_character_number_stat;
    set_weapon = (SetWeaponFunction)resolved_set_weapon;
    targeter_include_allies =
        (TargeterFlagFunction)resolved_targeter_include_allies;
    targeter_remove_allies =
        (TargeterFlagFunction)resolved_targeter_remove_allies;
    targeter_is_targeting_allies =
        (TargeterPredicateFunction)resolved_targeter_is_targeting_allies;
    arbiter_set_invulnerable =
        (ArbiterSetInvulnerableFunction)resolved_arbiter_set_invulnerable;
    set_master_game_speed =
        (SetMasterGameSpeedFunction)resolved_set_master_game_speed;
    master_game_speed = (float *)(game_base + RVA_MASTER_GAME_SPEED);
    party_invulnerability_enabled = FALSE;
    party_invulnerability_lease_count = 0u;
    ZeroMemory(
        party_invulnerability_leases,
        sizeof(party_invulnerability_leases)
    );
    story_test_speed_owned = FALSE;
    story_test_speed_conflicted = FALSE;
    story_test_speed_saved_bits = 0u;
    story_test_speed_applied_bits = 0u;
    gel_pointer_to_entity = (GelPointerToEntityFunction)(
        game_base + RVA_GEL_POINTER_TO_ENTITY
    );
    entity_pointer_cleanup = (EntityPointerCleanupFunction)(
        game_base + RVA_ENTITY_POINTER_CLEANUP
    );
    resource_name_from_text = game_base + RVA_RESOURCE_NAME_FROM_TEXT;
    resource_name_release_reference =
        game_base + RVA_RESOURCE_NAME_RELEASE_REFERENCE;
    cafu_probe_requested = strstr(
        GetCommandLineA(),
        "-SudekiMPCafuProbe 1"
    ) != NULL;
    cafu_autofire_requested = cafu_probe_requested &&
        GetEnvironmentVariableA(
            "SUDEKIMP_CAFU_AUTOFIRE",
            NULL,
            0u
        ) > 0u;
    cafu_weapon_force_elco_model = cafu_probe_requested &&
        GetEnvironmentVariableA(
            "SUDEKIMP_CAFU_ELCO_WEAPON",
            NULL,
            0u
        ) > 0u;
    cafu_autofire_sent = FALSE;
    cafu_autofire_ready_at = 0u;
    cafu_weapon_presentation_last_tick = 0u;
    cafu_weapon_presentation_sample_count = 0u;
    cafu_exception_handler = cafu_probe_requested ?
        AddVectoredExceptionHandler(1u, trace_cafu_exception) : NULL;
    if (cafu_probe_requested && cafu_exception_handler == NULL) {
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_exception_trace_install "
            "status=rejected win32_error=%lu\r\n",
            (unsigned long)GetLastError()
        );
    }
    original_missile_launch = NULL;
    original_missile_select = NULL;
    original_position_transform_update = NULL;
    cafu_missile_launch_sequence = 0u;
    if (cafu_probe_requested) {
        original_missile_launch = (MissileLaunchThunkFunction)(
            game_base + RVA_MISSILE_LAUNCH_THUNK
        );
        if (!SudekiMpInstallPointerHook(
                &cafu_missile_launch_hook,
                (void **)(game_base + RVA_MISSILE_LAUNCH_VTABLE_SLOT),
                original_missile_launch,
                trace_cafu_missile_launch)) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_missile_trace_install "
                "status=rejected win32_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpCleanroomEngineReset();
            return FALSE;
        }
        if (!SudekiMpInstallInlineHook(
                &cafu_missile_select_hook,
                game_base + RVA_MISSILE_SELECT,
                missile_select_entry,
                sizeof(missile_select_entry),
                trace_cafu_missile_select)) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_missile_select_trace_install "
                "status=rejected win32_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpCleanroomEngineReset();
            return FALSE;
        }
        original_missile_select = (MissileSelectFunction)(
            cafu_missile_select_hook.trampoline
        );
        original_position_transform_update =
            (PositionTransformUpdateFunction)(
                game_base + RVA_POSITION_TRANSFORM_UPDATE
            );
        if (!SudekiMpInstallRelativeCallHook(
                &cafu_position_transform_hook,
                game_base + RVA_MISSILE_PRESENTATION_TRANSFORM_CALL,
                original_position_transform_update,
                guard_cafu_position_transform)) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_missile_presentation_guard_install "
                "status=rejected win32_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpCleanroomEngineReset();
            return FALSE;
        }
        if (!SudekiMpInstallInlineHook(
                &cafu_position_transform_entry_hook,
                game_base + RVA_POSITION_TRANSFORM_UPDATE,
                position_transform_update_entry,
                sizeof(position_transform_update_entry),
                guard_cafu_position_transform_entry)) {
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_position_transform_entry_guard_install "
                "status=rejected win32_error=%lu\r\n",
                (unsigned long)GetLastError()
            );
            SudekiMpCleanroomEngineReset();
            return FALSE;
        }
        original_position_transform_update =
            (PositionTransformUpdateFunction)(
                cafu_position_transform_entry_hook.trampoline
            );
        SudekiMpLogWrite(
            "cleanroom_engine event=cafu_missile_trace_install "
            "status=success vtable_slot_rva=0x002d4cdc "
            "target_rva=0x000c7140 select_entry_rva=0x000c6de0 "
            "select_hook_kind=inline transform_call_rva=0x00136b36 "
            "transform_entry_rva=0x00110d40 "
            "transform_guard=probe_only\r\n"
        );
    }
    cafu_probe_spawn_attempted = FALSE;
    infinite_jetpack_fuel = FALSE;
    last_elco_ability = NULL;
    elco_fuel_refill_logged = FALSE;
    cafu_player_one_confirmed = FALSE;
    cafu_player_one_rotation_attempts = 0u;
    cafu_weapon_inventory_logged = FALSE;
    cafu_weapon_gate_log = 0u;
    cafu_weapon_model_patched = FALSE;
    cafu_weapon_model_uses_fallback = FALSE;
    cafu_weapon_model_item = NULL;
    cafu_weapon_preload_requested = FALSE;
    cafu_weapon_model_ready = FALSE;
    cafu_weapon_preload_started_at = 0u;
    cafu_missile_model_name_initialized = FALSE;
    cafu_missile_model_preload_requested = FALSE;
    cafu_missile_model_ready = FALSE;
    cafu_missile_model_patch_count = 0u;
    ZeroMemory(&saved_cafu_weapon_model, sizeof(saved_cafu_weapon_model));
    ZeroMemory(&applied_cafu_weapon_model, sizeof(applied_cafu_weapon_model));
    ZeroMemory(&cafu_weapon_preload, sizeof(cafu_weapon_preload));
    ZeroMemory(&cafu_missile_model_name, sizeof(cafu_missile_model_name));
    ZeroMemory(
        &cafu_missile_model_preload,
        sizeof(cafu_missile_model_preload)
    );
    ZeroMemory(
        cafu_missile_model_patches,
        sizeof(cafu_missile_model_patches)
    );
    no_sp_needed_flag = game_base + RVA_NO_SP_NEEDED_FLAG;
    no_ssp_needed_flag = game_base + RVA_NO_SSP_NEEDED_FLAG;
    saved_no_sp_needed = *no_sp_needed_flag;
    saved_no_ssp_needed = *no_ssp_needed_flag;
    resource_flags_captured = TRUE;
    SudekiMpLogFormat(
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
        "set_weapon_rva=0x000d8790 cafu_probe=%s\r\n",
        cafu_probe_requested ? "requested" : "disabled"
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

BOOL SudekiMpCleanroomEngineActorFacing(
    SudekiMpCleanroomActor actor,
    float facing[2]
) {
    uint8_t *character;
    uint8_t *transform;
    float length;

    if (facing == NULL) return FALSE;
    character = (uint8_t *)actor_pointer(actor);
    if (!readable_memory(character, 0x48u)) return FALSE;
    transform = *(uint8_t **)(character + 0x44u);
    /* Position::SetForward reads the current native forward at +0x50..+0x58.
     * Export only its horizontal unit direction; never a process pointer. */
    if (!readable_memory(transform, 0x5cu)) return FALSE;
    facing[0] = *(float *)(transform + 0x50u);
    facing[1] = *(float *)(transform + 0x58u);
    if (!isfinite(facing[0]) || !isfinite(facing[1])) return FALSE;
    length = sqrtf(facing[0] * facing[0] + facing[1] * facing[1]);
    if (!isfinite(length) || length < 0.0001f || length > 1000.0f) return FALSE;
    facing[0] /= length;
    facing[1] /= length;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorPresentation(
    SudekiMpCleanroomActor actor,
    SudekiMpCleanroomActorPresentation *presentation
) {
    uint8_t *character;
    uint8_t *position;
    uint8_t *wrapper;
    void *renderer;
    void **vtable;
    CafuAnimationCountFunction get_count;
    CafuAnimationSelectorGetFunction get_selector;
    CafuAnimationFloatGetFunction get_rate;
    CafuAnimationFloatGetFunction get_time;
    CafuAnimationStateGetFunction get_state;
    AnimationBlendGetFunction get_blend;
    unsigned int channel;
    unsigned int channel_limit;

    if (presentation == NULL || game_base == NULL || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) return FALSE;
    ZeroMemory(presentation, sizeof(*presentation));
    character = (uint8_t *)actor_pointer(actor);
    position = readable_memory(character, 0x48u) ?
        *(uint8_t **)(character + 0x44u) : NULL;
    wrapper = readable_memory(position, 0xb8u) ?
        *(uint8_t **)(position + 0xb4u) : NULL;
    renderer = readable_memory(wrapper, 0x14u) ?
        *(void **)((uint8_t *)wrapper + 0x10u) : NULL;
    if (!readable_memory(renderer, sizeof(void *)) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        return FALSE;
    }
    vtable = *(void ***)renderer;
    if (!readable_memory(vtable, 0x14cu) ||
        vtable[0xf8u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_COUNT ||
        vtable[0x100u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_SELECTOR_GET ||
        vtable[0x108u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_RATE_GET ||
        vtable[0x110u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_TIME_GET ||
        vtable[0x118u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_STATE_GET ||
        vtable[0x148u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_BLEND_GET) {
        return FALSE;
    }
    get_count = (CafuAnimationCountFunction)
        vtable[0xf8u / sizeof(void *)];
    get_selector = (CafuAnimationSelectorGetFunction)
        vtable[0x100u / sizeof(void *)];
    get_rate = (CafuAnimationFloatGetFunction)
        vtable[0x108u / sizeof(void *)];
    get_time = (CafuAnimationFloatGetFunction)
        vtable[0x110u / sizeof(void *)];
    get_state = (CafuAnimationStateGetFunction)
        vtable[0x118u / sizeof(void *)];
    get_blend = (AnimationBlendGetFunction)
        vtable[0x148u / sizeof(void *)];
    presentation->submodel_count = get_count(renderer);
    if (presentation->submodel_count == 0u ||
        presentation->submodel_count > 32u) return FALSE;
    /* The melee renderers share the vtable but do not expose valid backing
     * storage for the ranged auxiliary channels. Tal's proven surface is
     * channels 0-1 only; Ailish's world renderer owns all five. */
    channel_limit = actor == SUDEKIMP_CLEANROOM_AILISH ?
        SUDEKIMP_CLEANROOM_PRESENTATION_CHANNELS : 2u;
    for (channel = 0u;
         channel < channel_limit;
         ++channel) {
        presentation->selector[channel] =
            get_selector(renderer, (int)channel, 0u);
        presentation->state[channel] =
            (uint8_t)get_state(renderer, (int)channel, 0u);
        presentation->rate[channel] =
            get_rate(renderer, (int)channel, 0u);
        presentation->time[channel] =
            get_time(renderer, (int)channel, 0u);
        if (!isfinite(presentation->rate[channel]) ||
            !isfinite(presentation->time[channel])) return FALSE;
    }
    if (actor == SUDEKIMP_CLEANROOM_AILISH) {
        for (channel = 0u;
             channel < SUDEKIMP_CLEANROOM_PRESENTATION_BLENDS;
             ++channel) {
            presentation->blend[channel] = get_blend(renderer, (int)channel);
            if (!isfinite(presentation->blend[channel])) return FALSE;
        }
    } else {
        presentation->blend[0] = get_blend(renderer, 0);
        presentation->blend[3] = get_blend(renderer, 3);
        if (!isfinite(presentation->blend[0]) ||
            !isfinite(presentation->blend[3])) return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorResources(
    SudekiMpCleanroomActor actor,
    float *hit_points,
    float *skill_points
) {
    void *gel_pointer;
    float hp;
    float sp;

    if (hit_points == NULL || skill_points == NULL || get_pc == NULL ||
        get_character_number_stat == NULL || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT ||
        !SudekiMpCleanroomEngineWorldReady()) {
        return FALSE;
    }
    gel_pointer = get_pc(actor_resources[actor]);
    if (!readable_memory(gel_pointer, 0x10u)) return FALSE;
    hp = get_character_number_stat(gel_pointer, "HitPoints");
    sp = get_character_number_stat(gel_pointer, "SkillPoints");
    if (!isfinite(hp) || !isfinite(sp) || hp < 0.0f || sp < 0.0f ||
        hp > 100000000.0f || sp > 100000000.0f) {
        return FALSE;
    }
    *hit_points = hp;
    *skill_points = sp;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetActorResources(
    SudekiMpCleanroomActor actor,
    float hit_points,
    float skill_points
) {
    void *gel_pointer;
    if (get_pc == NULL || set_character_number_stat == NULL || actor < 0 ||
        actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT ||
        !isfinite(hit_points) || !isfinite(skill_points) ||
        hit_points < 0.0f || skill_points < 0.0f ||
        hit_points > 100000000.0f || skill_points > 100000000.0f ||
        !SudekiMpCleanroomEngineWorldReady()) {
        return FALSE;
    }
    gel_pointer = get_pc(actor_resources[actor]);
    if (!readable_memory(gel_pointer, 0x10u)) return FALSE;
    if (set_character_number_stat(
            gel_pointer, "HitPoints", hit_points) == 0u) return FALSE;
    return set_character_number_stat(
        gel_pointer, "SkillPoints", skill_points) != 0u;
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

BOOL SudekiMpCleanroomEngineDummySnapshot(
    float position[3],
    float *hit_points
) {
    uint8_t *entity;
    uint8_t *transform;
    void *gel_pointer;
    float hp;
    if (position == NULL || hit_points == NULL || get_generic_entity == NULL ||
        get_character_number_stat == NULL ||
        !SudekiMpCleanroomEngineWorldReady()) return FALSE;
    entity = (uint8_t *)lookup_entity(get_generic_entity, dummy_resource);
    if (!readable_memory(entity, 0x48u)) return FALSE;
    transform = *(uint8_t **)(entity + 0x44u);
    if (!readable_memory(transform, 0x24u)) return FALSE;
    gel_pointer = get_generic_entity(dummy_resource);
    if (!readable_memory(gel_pointer, 0x10u)) return FALSE;
    hp = get_character_number_stat(gel_pointer, "HitPoints");
    if (!isfinite(hp) || hp < 0.0f || hp > 100000000.0f) return FALSE;
    position[0] = *(float *)(transform + 0x18u);
    position[1] = *(float *)(transform + 0x1cu);
    position[2] = *(float *)(transform + 0x20u);
    if (!isfinite(position[0]) || !isfinite(position[1]) ||
        !isfinite(position[2])) return FALSE;
    *hit_points = hp;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetDummyHitPoints(float hit_points) {
    void *gel_pointer;
    if (get_generic_entity == NULL || set_character_number_stat == NULL ||
        !isfinite(hit_points) || hit_points < 0.0f ||
        hit_points > 100000000.0f ||
        !SudekiMpCleanroomEngineWorldReady()) return FALSE;
    gel_pointer = get_generic_entity(dummy_resource);
    return readable_memory(gel_pointer, 0x10u) &&
        set_character_number_stat(
            gel_pointer, "HitPoints", hit_points) != 0u;
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

BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) {
    return ranged_prime_pending || ranged_prime_ui_active;
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

BOOL SudekiMpCleanroomEngineInfiniteJetpackFuel(BOOL *enabled) {
    if (enabled == NULL || game_base == NULL || elco_get_fuel == NULL ||
        elco_set_fuel == NULL) {
        return FALSE;
    }
    *enabled = infinite_jetpack_fuel;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetInfiniteJetpackFuel(BOOL enabled) {
    BOOL current;

    enabled = enabled != FALSE;
    if (game_base == NULL || elco_get_fuel == NULL || elco_set_fuel == NULL) {
        return FALSE;
    }
    infinite_jetpack_fuel = enabled;
    last_elco_ability = NULL;
    elco_fuel_refill_logged = FALSE;
    if (!SudekiMpCleanroomEngineInfiniteJetpackFuel(&current) ||
        current != enabled) {
        return FALSE;
    }
    if (enabled) {
        SudekiMpCleanroomEngineMaintainResources();
    }
    SudekiMpLogFormat(
        "cleanroom_engine event=infinite_jetpack_fuel "
        "status=confirmed state=%s policy=refill_native_maximum\r\n",
        enabled ? "enabled" : "disabled"
    );
    return TRUE;
}

static BOOL story_test_speed_bits(uint32_t *bits) {
    float value;

    if (bits == NULL || master_game_speed == NULL ||
        !readable_memory((const void *)master_game_speed, sizeof(float))) {
        return FALSE;
    }
    value = *master_game_speed;
    memcpy(bits, &value, sizeof(*bits));
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSetStoryTestSpeed(
    BOOL enabled,
    float multiplier
) {
    uint32_t requested_bits;
    uint32_t current_bits;
    uint32_t result_bits;
    float current;
    float restore;
    BOOL restored = TRUE;
    BOOL result_readable;

    if (game_base == NULL || set_master_game_speed == NULL ||
        master_game_speed == NULL || !isfinite(multiplier) ||
        multiplier < 1.0f || multiplier > 4.0f ||
        !story_test_speed_bits(&current_bits)) {
        return FALSE;
    }
    requested_bits = float_bits(multiplier);
    if (enabled) {
        if (story_test_speed_conflicted) {
            return FALSE;
        }
        if (story_test_speed_owned &&
            current_bits != story_test_speed_applied_bits) {
            SudekiMpLogFormat(
                "cleanroom_engine event=story_test_speed action=apply "
                "status=rejected requested_bits=0x%08lx "
                "current_bits=0x%08lx owned_bits=0x%08lx "
                "reason=ownership_changed\r\n",
                (unsigned long)requested_bits,
                (unsigned long)current_bits,
                (unsigned long)story_test_speed_applied_bits
            );
            story_test_speed_owned = FALSE;
            story_test_speed_conflicted = TRUE;
            story_test_speed_saved_bits = 0u;
            story_test_speed_applied_bits = 0u;
            return FALSE;
        }
        if (story_test_speed_owned &&
            current_bits == story_test_speed_applied_bits &&
            requested_bits == story_test_speed_applied_bits) {
            return TRUE;
        }
        memcpy(&current, &current_bits, sizeof(current));
        if (!isfinite(current)) {
            return FALSE;
        }
        if (!story_test_speed_owned) {
            story_test_speed_saved_bits = current_bits;
        }
        set_master_game_speed(multiplier);
        story_test_speed_owned = TRUE;
        story_test_speed_applied_bits = requested_bits;
        result_readable = story_test_speed_bits(&result_bits);
        if (!result_readable || result_bits != requested_bits) {
            SudekiMpLogFormat(
                "cleanroom_engine event=story_test_speed action=apply "
                "status=verification_failed requested_bits=0x%08lx "
                "result_bits=0x%08lx\r\n",
                (unsigned long)requested_bits,
                (unsigned long)(result_readable ? result_bits : 0u)
            );
            return FALSE;
        }
        SudekiMpLogFormat(
            "cleanroom_engine event=story_test_speed action=apply "
            "status=confirmed previous_bits=0x%08lx "
            "multiplier_bits=0x%08lx "
            "policy=native_master_multiplier_owned_lease\r\n",
            (unsigned long)current_bits,
            (unsigned long)requested_bits
        );
        return TRUE;
    }
    if (!story_test_speed_owned) {
        story_test_speed_conflicted = FALSE;
        return TRUE;
    }
    if (current_bits == story_test_speed_applied_bits) {
        memcpy(&restore, &story_test_speed_saved_bits, sizeof(restore));
        set_master_game_speed(restore);
        result_readable = story_test_speed_bits(&result_bits);
        restored = result_readable &&
            result_bits == story_test_speed_saved_bits;
        SudekiMpLogFormat(
            "cleanroom_engine event=story_test_speed action=restore "
            "status=%s applied_bits=0x%08lx restored_bits=0x%08lx\r\n",
            restored ? "confirmed" : "verification_failed",
            (unsigned long)story_test_speed_applied_bits,
            (unsigned long)(result_readable ? result_bits : 0u)
        );
        if (!restored) {
            if (result_readable &&
                result_bits != story_test_speed_applied_bits) {
                SudekiMpLogFormat(
                    "cleanroom_engine event=story_test_speed "
                    "action=restore status=ownership_yielded "
                    "result_bits=0x%08lx "
                    "reason=different_external_value_observed\r\n",
                    (unsigned long)result_bits
                );
                story_test_speed_owned = FALSE;
                story_test_speed_conflicted = FALSE;
                story_test_speed_saved_bits = 0u;
                story_test_speed_applied_bits = 0u;
            }
            return FALSE;
        }
    } else {
        SudekiMpLogFormat(
            "cleanroom_engine event=story_test_speed action=restore "
            "status=skipped applied_bits=0x%08lx current_bits=0x%08lx "
            "reason=ownership_changed\r\n",
            (unsigned long)story_test_speed_applied_bits,
            (unsigned long)current_bits
        );
    }
    story_test_speed_owned = FALSE;
    story_test_speed_conflicted = FALSE;
    story_test_speed_saved_bits = 0u;
    story_test_speed_applied_bits = 0u;
    return TRUE;
}

void SudekiMpCleanroomEngineMaintainResources(void) {
    void **manager_global;
    void *manager;
    BOOL enabled;
    float current;
    float maximum;
    void *elco_ability;
    float cafu_position[3];

    if (game_base == NULL) {
        return;
    }
    (void)maintain_party_invulnerability_leases();
    if (!SudekiMpCleanroomEngineWorldReady()) {
        return;
    }
    (void)prepare_training_inventory();
    (void)prepare_spirit_strikes();
    if (cafu_probe_requested) {
        (void)prepare_cafu_weapon_model();
        (void)prepare_cafu_missile_models();
    }
    if (cafu_probe_requested && !cafu_probe_spawn_attempted &&
        cafu_weapon_model_ready && cafu_missile_model_ready &&
        !SudekiMpCleanroomEngineActorPresent(SUDEKIMP_CLEANROOM_CAFU) &&
        SudekiMpCleanroomEngineActorPosition(
            SUDEKIMP_CLEANROOM_AILISH,
            cafu_position)) {
        cafu_position[0] += 2.0f;
        cafu_probe_spawn_attempted = SudekiMpCleanroomEngineSpawnActor(
            SUDEKIMP_CLEANROOM_CAFU,
            cafu_position
        );
        SudekiMpLogFormat(
            "cleanroom_engine event=cafu_probe phase=runtime_spawn "
            "status=%s position_bits=%08lx,%08lx,%08lx\r\n",
            cafu_probe_spawn_attempted ? "returned" : "pending",
            (unsigned long)float_bits(cafu_position[0]),
            (unsigned long)float_bits(cafu_position[1]),
            (unsigned long)float_bits(cafu_position[2])
        );
    }
    initialize_present_actors();
    if (infinite_jetpack_fuel && elco_get_fuel != NULL &&
        elco_set_fuel != NULL) {
        elco_ability = elco_ability_pointer();
        if (elco_ability != last_elco_ability) {
            last_elco_ability = elco_ability;
            elco_fuel_refill_logged = FALSE;
        }
        if (elco_ability != NULL) {
            maximum = *(const float *)((const uint8_t *)elco_ability + 0x68u);
            current = elco_get_fuel(elco_ability);
            if (isfinite(maximum) && isfinite(current) && maximum > 0.0f &&
                current < maximum) {
                elco_set_fuel(elco_ability, maximum);
                if (!elco_fuel_refill_logged) {
                    SudekiMpLogFormat(
                        "cleanroom_engine event=infinite_jetpack_fuel "
                        "action=refill ability=%p previous_bits=0x%08lx "
                        "maximum_bits=0x%08lx\r\n",
                        elco_ability,
                        (unsigned long)float_bits(current),
                        (unsigned long)float_bits(maximum)
                    );
                    elco_fuel_refill_logged = TRUE;
                }
            }
        }
    }
    if (cafu_probe_requested) {
        (void)prepare_cafu_missile_models();
        inspect_cafu_missile_manager_state();
        if (cafu_missile_model_ready) {
            maintain_cafu_player_one();
            maintain_cafu_autofire();
            trace_cafu_weapon_presentation();
        }
    }

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
    SudekiMpResourceName *current_cafu_model;
    SudekiMpCafuMissileModelPatch *missile_patch;
    unsigned int missile_patch_index;

    party_invulnerability_enabled = FALSE;
    (void)maintain_party_invulnerability_leases();
    if (party_invulnerability_lease_count != 0u) {
        SudekiMpLogFormat(
            "cleanroom_engine event=party_invulnerability action=reset "
            "status=dropped_unresolved leases=%u "
            "policy=never_call_native_helper_through_stale_identity\r\n",
            party_invulnerability_lease_count
        );
    }
    party_invulnerability_lease_count = 0u;
    ZeroMemory(
        party_invulnerability_leases,
        sizeof(party_invulnerability_leases)
    );
    (void)SudekiMpCleanroomEngineSetStoryTestSpeed(FALSE, 1.0f);
    cancel_ranged_prime();
    if (cafu_exception_handler != NULL) {
        (void)RemoveVectoredExceptionHandler(cafu_exception_handler);
        cafu_exception_handler = NULL;
    }
    (void)SudekiMpRestoreInlineHook(&cafu_position_transform_entry_hook);
    (void)SudekiMpRestoreRelativeCallHook(&cafu_position_transform_hook);
    (void)SudekiMpRestoreInlineHook(&cafu_missile_select_hook);
    (void)SudekiMpRestorePointerHook(&cafu_missile_launch_hook);
    original_missile_launch = NULL;
    original_missile_select = NULL;
    original_position_transform_update = NULL;
    cafu_missile_launch_sequence = 0u;
    last_cafu_missile_selected = NULL;
    last_cafu_missile_weapon = NULL;
    last_cafu_guarded_position = NULL;
    cafu_guard_skip_count = 0u;
    cafu_autofire_requested = FALSE;
    cafu_autofire_sent = FALSE;
    cafu_autofire_ready_at = 0u;
    cafu_weapon_presentation_last_tick = 0u;
    cafu_weapon_presentation_sample_count = 0u;
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
    if (cafu_weapon_model_patched && cafu_weapon_model_item != NULL) {
        current_cafu_model = (SudekiMpResourceName *)(
            cafu_weapon_model_item + 0x44u
        );
        if (writable_memory(current_cafu_model, sizeof(*current_cafu_model)) &&
            memcmp(current_cafu_model, &applied_cafu_weapon_model,
                sizeof(*current_cafu_model)) == 0) {
            *current_cafu_model = saved_cafu_weapon_model;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_weapon_model_patch "
                "status=restored item=%p identifier=%08lx "
                "previous_policy=%s\r\n",
                cafu_weapon_model_item,
                (unsigned long)saved_cafu_weapon_model.identifier,
                cafu_weapon_model_uses_fallback ?
                    "visual_fallback" : "actual_w033_archive_resource"
            );
        } else {
            SudekiMpLogWrite(
                "cleanroom_engine event=cafu_weapon_model_patch "
                "status=restore_skipped reason=ownership_changed\r\n"
            );
        }
    }
    for (missile_patch_index = 0u;
         missile_patch_index < cafu_missile_model_patch_count;
         ++missile_patch_index) {
        missile_patch = &cafu_missile_model_patches[missile_patch_index];
        if (writable_memory(missile_patch->target, sizeof(*missile_patch->target)) &&
            memcmp(
                missile_patch->target,
                &missile_patch->applied,
                sizeof(missile_patch->applied)) == 0) {
            *missile_patch->target = missile_patch->saved;
            SudekiMpLogFormat(
                "cleanroom_engine event=cafu_missile_model_patch "
                "status=restored model=%p identifier=%08lx\r\n",
                missile_patch->target,
                (unsigned long)missile_patch->saved.identifier
            );
        }
    }
    release_retained_reference(&cafu_missile_model_preload.reference);
    if (cafu_missile_model_name_initialized) {
        release_resource_name(&cafu_missile_model_name);
    }
    release_retained_reference(&cafu_weapon_preload.reference);
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
    elco_get_fuel = NULL;
    elco_set_fuel = NULL;
    get_character_number_stat = NULL;
    set_character_number_stat = NULL;
    set_weapon = NULL;
    targeter_include_allies = NULL;
    targeter_remove_allies = NULL;
    targeter_is_targeting_allies = NULL;
    arbiter_set_invulnerable = NULL;
    set_master_game_speed = NULL;
    master_game_speed = NULL;
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
    infinite_jetpack_fuel = FALSE;
    party_invulnerability_enabled = FALSE;
    story_test_speed_owned = FALSE;
    story_test_speed_conflicted = FALSE;
    story_test_speed_saved_bits = 0u;
    story_test_speed_applied_bits = 0u;
    last_elco_ability = NULL;
    elco_fuel_refill_logged = FALSE;
    spirit_strike_unlocks_captured = FALSE;
    saved_spirit_strike_unlocks = 0u;
    saved_spirit_strike_manager = NULL;
    cafu_probe_requested = FALSE;
    cafu_probe_spawn_attempted = FALSE;
    cafu_player_one_confirmed = FALSE;
    cafu_player_one_rotation_attempts = 0u;
    cafu_weapon_inventory_logged = FALSE;
    cafu_weapon_gate_log = 0u;
    cafu_weapon_model_patched = FALSE;
    cafu_weapon_model_uses_fallback = FALSE;
    cafu_weapon_model_item = NULL;
    cafu_weapon_preload_requested = FALSE;
    cafu_weapon_model_ready = FALSE;
    cafu_weapon_force_elco_model = FALSE;
    cafu_weapon_preload_started_at = 0u;
    cafu_missile_model_name_initialized = FALSE;
    cafu_missile_model_preload_requested = FALSE;
    cafu_missile_model_ready = FALSE;
    cafu_missile_model_patch_count = 0u;
    ZeroMemory(&saved_cafu_weapon_model, sizeof(saved_cafu_weapon_model));
    ZeroMemory(&applied_cafu_weapon_model, sizeof(applied_cafu_weapon_model));
    ZeroMemory(&cafu_weapon_preload, sizeof(cafu_weapon_preload));
    ZeroMemory(&cafu_missile_model_name, sizeof(cafu_missile_model_name));
    ZeroMemory(
        &cafu_missile_model_preload,
        sizeof(cafu_missile_model_preload)
    );
    ZeroMemory(
        cafu_missile_model_patches,
        sizeof(cafu_missile_model_patches)
    );
    ZeroMemory(
        initialized_actor_entities,
        sizeof(initialized_actor_entities)
    );
}
