#include "hooks/character_switch_trace.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#else
#error "Character-switch tracing requires 32-bit GCC thiscall support"
#endif

typedef void (SUDEKIMP_THISCALL *CharacterInputHandler)(
    void *listener,
    void *event
);
typedef void (__cdecl *InternalSpawnPcFunction)(
    const void *resource_name,
    float x,
    float y,
    float z
);
typedef void (__cdecl *RemovePcFunction)(const void *resource_name);
typedef uint8_t (__stdcall *AiCandidateFilterFunction)(
    const float *query,
    void *source,
    float range,
    void *candidate
);
enum {
    RVA_CHARACTER_INPUT_HANDLER = 0x000277b0u,
    RVA_CHARACTER_INPUT_VTABLE_SLOT = 0x002c9f84u,
    RVA_INTERNAL_SPAWN_PC = 0x000b1b00u,
    RVA_REMOVE_PC = 0x000b23a0u,
    RVA_AI_CANDIDATE_FILTER = 0x001b6ec0u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    ACTION_PREVIOUS_CHARACTER = 0x32u,
    ACTION_NEXT_CHARACTER = 0x33u,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_FIRST_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu
};

static const uint8_t internal_spawn_pc_entry[] = {
    0x83u, 0xecu, 0x3cu, 0x56u, 0x57u,
    0x8bu, 0x7cu, 0x24u, 0x48u
};
static const uint8_t remove_pc_entry[] = {
    0x81u, 0xecu, 0x14u, 0x01u, 0x00u, 0x00u, 0x56u, 0x57u
};
static const uint8_t ai_candidate_filter_entry[] = {
    0x8bu, 0x54u, 0x24u, 0x08u,
    0x8bu, 0x4cu, 0x24u, 0x10u
};

typedef struct CharacterSnapshot {
    void *character;
    uint8_t control_2a;
    void *actor_state;
    void *targeter;
    void *ordinary_target;
    uint32_t target_mask_7c;
    uint32_t target_flags_84;
    void *ai_component;
    void *ai_owner;
    void *ai_mode_state;
    uint8_t ai_enabled_3c_0b;
    int16_t ai_control_ref_16a;
    uint32_t ai_flags_44;
    uint32_t actor_flags_50;
    uint32_t actor_state_58;
    uint32_t actor_flags_60;
} CharacterSnapshot;

typedef struct SwitchSnapshot {
    void *group;
    void *controller;
    void *controller_target;
    uint32_t controller_mode_80;
    uint32_t controller_mode_84;
    uint8_t group_switching_d6;
    uint8_t group_state_d7;
    int32_t party_count;
    CharacterSnapshot party[PARTY_SLOT_COUNT];
} SwitchSnapshot;

static SudekiMpPointerHook character_input_vtable_hook;
static SudekiMpInlineHook internal_spawn_pc_hook;
static SudekiMpInlineHook remove_pc_hook;
static SudekiMpInlineHook ai_candidate_filter_hook;
static CharacterInputHandler original_character_input_handler;
static InternalSpawnPcFunction original_internal_spawn_pc;
static RemovePcFunction original_remove_pc;
static AiCandidateFilterFunction original_ai_candidate_filter;
static uint8_t *game_base;
static UINT_PTR snapshot_timer;
static UINT_PTR lifecycle_timer;
static unsigned int snapshot_timer_stage;
static DWORD snapshot_timer_started;
static uint32_t snapshot_action;
static SwitchSnapshot last_lifecycle_snapshot;
static BOOL last_lifecycle_snapshot_valid;
static DWORD last_lifecycle_heartbeat;
static BOOL talos_party_prototype_enabled;
static BOOL talos_party_collapse_observed;
static DWORD talos_party_collapse_observed_at;
static BOOL talos_party_restore_armed;
static DWORD talos_party_restore_armed_at;
static BOOL talos_party_postspawn_pending;
static DWORD talos_party_postspawn_started_at;
static BOOL talos_party_session_active;
static BOOL talos_party_boss_seen;
static void *talos_party_real_boss_ai;
static void *talos_party_filter_logged_sources[PARTY_SLOT_COUNT];
static BOOL talos_party_allies_targeting_active;
static BOOL talos_party_targeting_state_captured[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
static BOOL talos_party_original_allies_targeting[SUDEKIMP_CLEANROOM_ACTOR_COUNT];

static void log_switch_snapshot(
    const char *phase,
    uint32_t action,
    DWORD elapsed_ms
);

static void poll_lifecycle_snapshot(const char *phase);

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void capture_character(
    void *character_pointer,
    CharacterSnapshot *snapshot
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *actor_state;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->character = character;
    if (!readable_memory(character, 0xb0u)) {
        return;
    }
    snapshot->control_2a = *(uint8_t *)(character + 0x2a);
    snapshot->actor_state = *(void **)(character + 0x90);
    snapshot->ai_component = *(void **)(character + 0x94);
    snapshot->targeter = *(void **)(character + 0xacu);
    if (readable_memory(snapshot->targeter, 0x88u)) {
        uint8_t *targeter = (uint8_t *)snapshot->targeter;
        snapshot->ordinary_target = *(void **)(targeter + 0x54u);
        snapshot->target_mask_7c = *(uint32_t *)(targeter + 0x7cu);
        snapshot->target_flags_84 = *(uint32_t *)(targeter + 0x84u);
    }
    if (readable_memory(snapshot->ai_component, 0x16cu)) {
        uint8_t *ai_component = (uint8_t *)snapshot->ai_component;
        snapshot->ai_owner = *(void **)(ai_component + 0x10);
        snapshot->ai_mode_state = *(void **)(ai_component + 0x3c);
        snapshot->ai_control_ref_16a = *(int16_t *)(ai_component + 0x16a);
        snapshot->ai_flags_44 = *(uint32_t *)(ai_component + 0x44);
        if (readable_memory(snapshot->ai_mode_state, 0x0cu)) {
            snapshot->ai_enabled_3c_0b = *(
                (uint8_t *)snapshot->ai_mode_state + 0x0b
            );
        }
    }
    actor_state = (uint8_t *)snapshot->actor_state;
    if (!readable_memory(actor_state, 0x64u)) {
        return;
    }
    snapshot->actor_flags_50 = *(uint32_t *)(actor_state + 0x50);
    snapshot->actor_state_58 = *(uint32_t *)(actor_state + 0x58);
    snapshot->actor_flags_60 = *(uint32_t *)(actor_state + 0x60);
}

static void capture_switch_snapshot(SwitchSnapshot *snapshot) {
    uint8_t *group;
    uint8_t *controller;
    unsigned int index;

    memset(snapshot, 0, sizeof(*snapshot));
    if (game_base == NULL ||
        !readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group)) ||
        !readable_memory(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    snapshot->group = group;
    snapshot->controller = controller;
    if (readable_memory(controller, 0x24cu)) {
        snapshot->controller_target = *(void **)(controller + 0x248);
        snapshot->controller_mode_80 = *(uint32_t *)(controller + 0x80);
        snapshot->controller_mode_84 = *(uint32_t *)(controller + 0x84);
    }
    if (!readable_memory(group, 0xd8u)) {
        return;
    }
    snapshot->group_switching_d6 = *(uint8_t *)(group + 0xd6);
    snapshot->group_state_d7 = *(uint8_t *)(group + 0xd7);
    snapshot->party_count = *(int32_t *)(group + 0xcc);
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        void *character = *(void **)(
            group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE
        );
        capture_character(character, &snapshot->party[index]);
    }
}

static void log_resource_name_words(
    const char *event,
    const char *phase,
    const void *resource_name
) {
    uint32_t words[3] = {0u, 0u, 0u};

    if (readable_memory(resource_name, sizeof(words))) {
        memcpy(words, resource_name, sizeof(words));
    }
    SudekiMpLogFormat(
        "party_lifecycle event=%s phase=%s resource=0x%08lx words=%08lx,%08lx,%08lx\r\n",
        event,
        phase,
        (unsigned long)(uintptr_t)resource_name,
        (unsigned long)words[0],
        (unsigned long)words[1],
        (unsigned long)words[2]
    );
}

static void __cdecl trace_internal_spawn_pc(
    const void *resource_name,
    float x,
    float y,
    float z
) {
    enum { TALOS_VOID_KAZEL_RESOURCE_IDENTIFIER = 0xa6d349ccu };
    SwitchSnapshot before;
    SwitchSnapshot after;
    uint32_t resource_words[3] = {0u, 0u, 0u};
    uint32_t x_bits;
    uint32_t y_bits;
    uint32_t z_bits;
    BOOL resource_readable;
    BOOL kazel_resource;
    BOOL zero_position;
    BOOL four_to_one;
    BOOL companions_removed;
    BOOL companions_removed_since_snapshot;
    BOOL recent_observed_collapse;

    memcpy(&x_bits, &x, sizeof(x_bits));
    memcpy(&y_bits, &y, sizeof(y_bits));
    memcpy(&z_bits, &z, sizeof(z_bits));
    resource_readable = readable_memory(resource_name, sizeof(resource_words));
    if (resource_readable) {
        memcpy(resource_words, resource_name, sizeof(resource_words));
    }
    kazel_resource = resource_readable &&
        resource_words[1] == TALOS_VOID_KAZEL_RESOURCE_IDENTIFIER;
    zero_position = x_bits == 0u && y_bits == 0u && z_bits == 0u;
    capture_switch_snapshot(&before);
    log_resource_name_words("spawn_pc", "before", resource_name);
    SudekiMpLogFormat(
        "party_lifecycle event=spawn_pc phase=position x_bits=%08lx y_bits=%08lx z_bits=%08lx\r\n",
        (unsigned long)x_bits,
        (unsigned long)y_bits,
        (unsigned long)z_bits
    );
    original_internal_spawn_pc(resource_name, x, y, z);
    capture_switch_snapshot(&after);
    log_resource_name_words("spawn_pc", "after", resource_name);
    log_switch_snapshot("after_spawn_pc", 0u, 0u);
    four_to_one = before.party_count == 4 && after.party_count == 1;
    companions_removed =
        before.party[0].character != NULL &&
        before.party[1].character != NULL &&
        before.party[2].character != NULL &&
        before.party[3].character != NULL &&
        after.party[0].character != NULL &&
        after.party[1].character == NULL &&
        after.party[2].character == NULL &&
        after.party[3].character == NULL;
    companions_removed_since_snapshot =
        last_lifecycle_snapshot_valid &&
        (DWORD)(GetTickCount() - last_lifecycle_heartbeat) <= 5000u &&
        last_lifecycle_snapshot.party_count == 4 &&
        last_lifecycle_snapshot.party[0].character != NULL &&
        last_lifecycle_snapshot.party[1].character != NULL &&
        last_lifecycle_snapshot.party[2].character != NULL &&
        last_lifecycle_snapshot.party[3].character != NULL &&
        before.party_count == 1 &&
        after.party_count == 1 &&
        after.party[0].character ==
            last_lifecycle_snapshot.party[0].character &&
        after.party[1].character == NULL &&
        after.party[2].character == NULL &&
        after.party[3].character == NULL;
    recent_observed_collapse = talos_party_collapse_observed &&
        (DWORD)(GetTickCount() - talos_party_collapse_observed_at) <= 5000u;
    if (talos_party_prototype_enabled && kazel_resource && zero_position) {
        SudekiMpLogFormat(
            "talos_party event=collapse_gate resource_snapshot=%08lx before_count=%u after_count=%u lead_before=0x%08lx lead_after=0x%08lx lead_same=%u companions_removed=%u companions_removed_since_snapshot=%u recent_observed_collapse=%u armed=%u\r\n",
            (unsigned long)resource_words[1],
            (unsigned int)before.party_count,
            (unsigned int)after.party_count,
            (unsigned long)(uintptr_t)before.party[0].character,
            (unsigned long)(uintptr_t)after.party[0].character,
            before.party[0].character == after.party[0].character ? 1u : 0u,
            companions_removed ? 1u : 0u,
            companions_removed_since_snapshot ? 1u : 0u,
            recent_observed_collapse ? 1u : 0u,
            talos_party_restore_armed ? 1u : 0u
        );
    }
    if (talos_party_prototype_enabled && !talos_party_restore_armed &&
        kazel_resource && zero_position &&
        ((four_to_one && companions_removed) ||
         companions_removed_since_snapshot ||
         (recent_observed_collapse && after.party_count == 1 &&
          after.party[0].character != NULL))) {
        talos_party_collapse_observed = FALSE;
        talos_party_restore_armed = TRUE;
        talos_party_restore_armed_at = GetTickCount();
        SudekiMpLogWrite(
            "talos_party event=void_party_collapse status=confirmed before_count=4 after_count=1 resource=PC_KAZEL resource_identifier=a6d349cc coordinates=zero action=arm_post_movie_native_party_restore\r\n"
        );
    }
}

static void __cdecl trace_remove_pc(const void *resource_name) {
    log_resource_name_words("remove_pc", "before", resource_name);
    log_switch_snapshot("before_remove_pc", 0u, 0u);
    original_remove_pc(resource_name);
    log_resource_name_words("remove_pc", "after", resource_name);
    log_switch_snapshot("after_remove_pc", 0u, 0u);
}

static void log_switch_snapshot(
    const char *phase,
    uint32_t action,
    DWORD elapsed_ms
) {
    SwitchSnapshot snapshot;
    unsigned int index;

    capture_switch_snapshot(&snapshot);
    SudekiMpLogFormat(
        "character_switch event=snapshot phase=%s action=0x%02lx elapsed_ms=%lu group=0x%08lx party_count=%ld switching_d6=%u state_d7=%u controller=0x%08lx target=0x%08lx mode_80=%lu mode_84=%lu\r\n",
        phase,
        (unsigned long)action,
        (unsigned long)elapsed_ms,
        (unsigned long)(uintptr_t)snapshot.group,
        (long)snapshot.party_count,
        (unsigned int)snapshot.group_switching_d6,
        (unsigned int)snapshot.group_state_d7,
        (unsigned long)(uintptr_t)snapshot.controller,
        (unsigned long)(uintptr_t)snapshot.controller_target,
        (unsigned long)snapshot.controller_mode_80,
        (unsigned long)snapshot.controller_mode_84
    );
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        const CharacterSnapshot *character = &snapshot.party[index];
        SudekiMpLogFormat(
            "character_switch event=party phase=%s action=0x%02lx elapsed_ms=%lu slot=%u character=0x%08lx control_2a=%u actor_state=0x%08lx targeter=0x%08lx ordinary_target=0x%08lx target_mask_7c=0x%08lx target_flags_84=0x%08lx ai=0x%08lx ai_owner=0x%08lx ai_mode_state=0x%08lx ai_enabled_3c_0b=%u ai_control_ref_16a=%d ai_flags_44=0x%08lx flags_50=0x%08lx state_58=0x%08lx flags_60=0x%08lx\r\n",
            phase,
            (unsigned long)action,
            (unsigned long)elapsed_ms,
            index,
            (unsigned long)(uintptr_t)character->character,
            (unsigned int)character->control_2a,
            (unsigned long)(uintptr_t)character->actor_state,
            (unsigned long)(uintptr_t)character->targeter,
            (unsigned long)(uintptr_t)character->ordinary_target,
            (unsigned long)character->target_mask_7c,
            (unsigned long)character->target_flags_84,
            (unsigned long)(uintptr_t)character->ai_component,
            (unsigned long)(uintptr_t)character->ai_owner,
            (unsigned long)(uintptr_t)character->ai_mode_state,
            (unsigned int)character->ai_enabled_3c_0b,
            (int)character->ai_control_ref_16a,
            (unsigned long)character->ai_flags_44,
            (unsigned long)character->actor_flags_50,
            (unsigned long)character->actor_state_58,
            (unsigned long)character->actor_flags_60
        );
    }
}

static BOOL complete_talos_party_restore(void) {
    SwitchSnapshot snapshot;
    float lead_position[3];
    float spawn_position[3];
    static const SudekiMpCleanroomActor missing_party[] = {
        SUDEKIMP_CLEANROOM_AILISH,
        SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO
    };
    unsigned int index;
    unsigned int requested = 0u;

    capture_switch_snapshot(&snapshot);
    if (snapshot.party_count != 1 || snapshot.party[0].character == NULL ||
        snapshot.controller_target != snapshot.party[0].character ||
        snapshot.controller_mode_80 != 1u || snapshot.controller_mode_84 != 1u ||
        !SudekiMpCleanroomEngineWorldReady() ||
        !SudekiMpCleanroomEngineActorPosition(
            SUDEKIMP_CLEANROOM_TAL,
            lead_position)) {
        return FALSE;
    }

    talos_party_restore_armed = FALSE;
    for (index = 0u; index < sizeof(missing_party) / sizeof(missing_party[0]);
            ++index) {
        SudekiMpCleanroomActor actor = missing_party[index];
        if (SudekiMpCleanroomEngineActorPresent(actor)) {
            SudekiMpLogFormat(
                "talos_party event=restore actor=%s status=already_present\r\n",
                SudekiMpCleanroomActorLabel(actor)
            );
            continue;
        }
        spawn_position[0] = lead_position[0] + 1.5f * (float)(index + 1u);
        spawn_position[1] = lead_position[1];
        spawn_position[2] = lead_position[2] - 1.5f;
        if (SudekiMpCleanroomEngineSpawnActor(actor, spawn_position)) {
            ++requested;
            SudekiMpLogFormat(
                "talos_party event=restore actor=%s status=requested x_bits=%08lx y_bits=%08lx z_bits=%08lx\r\n",
                SudekiMpCleanroomActorLabel(actor),
                (unsigned long)float_bits(spawn_position[0]),
                (unsigned long)float_bits(spawn_position[1]),
                (unsigned long)float_bits(spawn_position[2])
            );
        } else {
            SudekiMpLogFormat(
                "talos_party event=restore actor=%s status=rejected\r\n",
                SudekiMpCleanroomActorLabel(actor)
            );
        }
    }
    SudekiMpCleanroomEngineMaintainResources();
    talos_party_postspawn_pending = requested != 0u;
    talos_party_postspawn_started_at = GetTickCount();
    talos_party_session_active = TRUE;
    talos_party_boss_seen = FALSE;
    talos_party_real_boss_ai = NULL;
    ZeroMemory(
        talos_party_filter_logged_sources,
        sizeof(talos_party_filter_logged_sources)
    );
    SudekiMpLogFormat(
        "talos_party event=restore status=complete requested=%u postspawn_pending=%u policy=native_spawn_after_exact_void_party_collapse\r\n",
        requested,
        talos_party_postspawn_pending ? 1u : 0u
    );
    poll_lifecycle_snapshot("after_talos_party_restore");
    return TRUE;
}

static void *find_talos_fight_entity(const char **resource_name) {
    static const char *const candidates[] = {
        "BOSS_Talos",
        "BOSS_Talos_Fake",
        "BOSS_Talos_Fake2",
        "ALLY_Talos",
        "CC_Ally_Talos"
    };
    unsigned int index;

    for (index = 0u; index < sizeof(candidates) / sizeof(candidates[0]);
            ++index) {
        void *entity = SudekiMpCleanroomEngineGenericEntity(candidates[index]);
        if (entity != NULL) {
            if (resource_name != NULL) {
                *resource_name = candidates[index];
            }
            return entity;
        }
    }
    if (resource_name != NULL) {
        *resource_name = NULL;
    }
    return NULL;
}

static int talos_companion_slot_for_ai(void *source_ai) {
    uint8_t *group;
    unsigned int slot;

    if (!talos_party_prototype_enabled || !talos_party_session_active ||
        source_ai == NULL || game_base == NULL ||
        !readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return -1;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_memory(group, 0xc0u)) {
        return -1;
    }
    for (slot = 1u; slot < PARTY_SLOT_COUNT; ++slot) {
        uint8_t *character = *(uint8_t **)(
            group + PARTY_SLOT_FIRST_OFFSET + slot * PARTY_SLOT_STRIDE
        );
        if (readable_memory(character, 0x98u) &&
            *(void **)(character + 0x94u) == source_ai) {
            return (int)slot;
        }
    }
    return -1;
}

static uint8_t __stdcall talos_party_ai_candidate_filter(
    const float *query,
    void *source,
    float range,
    void *candidate
) {
    enum {
        QUERY_COPY_SIZE = 0x28u,
        QUERY_FLAGS_OFFSET = 0x25u,
        QUERY_REJECT_BOSS_FLAG = 0x04u,
        AI_UNIT_TYPE_OFFSET = 0x148u,
        AI_UNIT_TYPE_BOSS = 3u
    };
    uint8_t query_copy[QUERY_COPY_SIZE];
    uint8_t flags;
    uint8_t result;
    int companion_slot;

    if (original_ai_candidate_filter == NULL) {
        return 0u;
    }
    companion_slot = talos_companion_slot_for_ai(source);
    if (companion_slot < 1 ||
        candidate == NULL || candidate != talos_party_real_boss_ai ||
        !readable_memory(candidate, AI_UNIT_TYPE_OFFSET + sizeof(uint32_t)) ||
        *(uint32_t *)((uint8_t *)candidate + AI_UNIT_TYPE_OFFSET) !=
            AI_UNIT_TYPE_BOSS ||
        !readable_memory(query, sizeof(query_copy))) {
        return original_ai_candidate_filter(query, source, range, candidate);
    }
    flags = *((const uint8_t *)query + QUERY_FLAGS_OFFSET);
    if ((flags & QUERY_REJECT_BOSS_FLAG) == 0u) {
        return original_ai_candidate_filter(query, source, range, candidate);
    }
    memcpy(query_copy, query, sizeof(query_copy));
    query_copy[QUERY_FLAGS_OFFSET] = (uint8_t)(
        query_copy[QUERY_FLAGS_OFFSET] & ~QUERY_REJECT_BOSS_FLAG
    );
    result = original_ai_candidate_filter(
        (const float *)query_copy,
        source,
        range,
        candidate
    );
    if (talos_party_filter_logged_sources[companion_slot] != source) {
        talos_party_filter_logged_sources[companion_slot] = source;
        SudekiMpLogFormat(
            "talos_party event=boss_filter status=scoped_bypass slot=%d source_ai=0x%08lx candidate_ai=0x%08lx flags_before=0x%02x flags_after=0x%02x result=%u policy=temporary_query_copy_only\r\n",
            companion_slot,
            (unsigned long)(uintptr_t)source,
            (unsigned long)(uintptr_t)candidate,
            (unsigned int)flags,
            (unsigned int)query_copy[QUERY_FLAGS_OFFSET],
            (unsigned int)result
        );
    }
    return result;
}

static BOOL talos_targeting_state_is_captured(void) {
    unsigned int index;

    for (index = 0u; index < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++index) {
        if (talos_party_targeting_state_captured[index]) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL set_talos_companion_target_policy(BOOL target_allies) {
    static const SudekiMpCleanroomActor companions[] = {
        SUDEKIMP_CLEANROOM_AILISH,
        SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO
    };
    BOOL all_confirmed = TRUE;
    unsigned int index;

    for (index = 0u; index < sizeof(companions) / sizeof(companions[0]);
            ++index) {
        SudekiMpCleanroomActor actor = companions[index];
        BOOL current;
        BOOL desired;

        if (!SudekiMpCleanroomEngineActorTargetsAllies(actor, &current)) {
            if (target_allies &&
                SudekiMpCleanroomEngineActorPresent(actor)) {
                all_confirmed = FALSE;
            }
            continue;
        }
        if (target_allies &&
            !talos_party_targeting_state_captured[actor]) {
            talos_party_targeting_state_captured[actor] = TRUE;
            talos_party_original_allies_targeting[actor] = current;
        }
        desired = target_allies ? TRUE :
            talos_party_original_allies_targeting[actor];
        if (current != desired) {
            if (!SudekiMpCleanroomEngineSetActorTargetsAllies(
                    actor,
                    desired)) {
                all_confirmed = FALSE;
                SudekiMpLogFormat(
                    "talos_party event=target_policy actor=%s status=rejected requested_allies=%u\r\n",
                    SudekiMpCleanroomActorLabel(actor),
                    desired ? 1u : 0u
                );
                continue;
            }
        }
        if (!target_allies && current == desired) {
            talos_party_targeting_state_captured[actor] = FALSE;
            talos_party_original_allies_targeting[actor] = FALSE;
        } else if (!target_allies) {
            BOOL restored;
            if (SudekiMpCleanroomEngineActorTargetsAllies(actor, &restored) &&
                restored == desired) {
                talos_party_targeting_state_captured[actor] = FALSE;
                talos_party_original_allies_targeting[actor] = FALSE;
            } else {
                all_confirmed = FALSE;
            }
        }
    }
    if (target_allies && all_confirmed &&
        !talos_party_allies_targeting_active) {
        SudekiMpLogWrite(
            "talos_party event=target_policy status=confirmed target_class=allies actors=Ailish,Buki,Elco policy=native_CTargeter_IncludeAlliesAsTargets\r\n"
        );
    } else if (!target_allies && talos_party_allies_targeting_active) {
        SudekiMpLogWrite(
            "talos_party event=target_policy status=restored policy=original_per_actor_target_masks\r\n"
        );
    }
    talos_party_allies_targeting_active = target_allies && all_confirmed;
    return all_confirmed;
}

static void maintain_talos_companion_target_policy(void) {
    SwitchSnapshot snapshot;
    const char *talos_resource = NULL;
    void *talos_entity;
    void *lead_target;

    if (!talos_party_session_active) {
        return;
    }
    capture_switch_snapshot(&snapshot);
    talos_entity = find_talos_fight_entity(&talos_resource);
    lead_target = snapshot.party[0].ordinary_target;
    if (talos_entity != NULL) {
        if (talos_resource != NULL &&
            strcmp(talos_resource, "BOSS_Talos") == 0 &&
            readable_memory(talos_entity, 0x98u)) {
            void *candidate_ai = *(void **)(
                (uint8_t *)talos_entity + 0x94u
            );
            if (readable_memory(candidate_ai, 0x14cu) &&
                *(uint32_t *)((uint8_t *)candidate_ai + 0x148u) == 3u) {
                talos_party_real_boss_ai = candidate_ai;
            }
        }
        if (!talos_party_boss_seen) {
            SudekiMpLogFormat(
                "talos_party event=target_entity status=resolved resource=%s entity=0x%08lx\r\n",
                talos_resource,
                (unsigned long)(uintptr_t)talos_entity
            );
        }
        talos_party_boss_seen = TRUE;
    } else if (lead_target != NULL && !talos_party_boss_seen) {
        /*
         * The final encounter's live entity is not registered under the
         * authored ALLY_Talos resource names accepted by GetGenericEntity.
         * Tal's native targeter nevertheless resolves the encounter target.
         * Keep this as confirmation only: the exact four-to-one void-party
         * lifecycle already owns the bounded targeting policy.
         */
        talos_party_boss_seen = TRUE;
        SudekiMpLogFormat(
            "talos_party event=target_entity status=inferred source=lead_native_target entity=0x%08lx resource_lookup=unavailable\r\n",
            (unsigned long)(uintptr_t)lead_target
        );
    }
    if (snapshot.party_count == 4 &&
        snapshot.party[0].character != NULL &&
        snapshot.party[1].character != NULL &&
        snapshot.party[2].character != NULL &&
        snapshot.party[3].character != NULL) {
        /*
         * Do not gate the native ally-target flag on a best-effort resource
         * lookup.  This session is armed only by the exact Talos void spawn
         * after the observed retail four-to-one party collapse.
         */
        (void)set_talos_companion_target_policy(TRUE);
        return;
    }
    if (talos_party_allies_targeting_active ||
        talos_targeting_state_is_captured()) {
        (void)set_talos_companion_target_policy(FALSE);
    }
    if (talos_party_boss_seen && talos_entity == NULL &&
        lead_target == NULL && snapshot.party_count != 4) {
        talos_party_session_active = FALSE;
        talos_party_boss_seen = FALSE;
        talos_party_real_boss_ai = NULL;
        ZeroMemory(
            talos_party_filter_logged_sources,
            sizeof(talos_party_filter_logged_sources)
        );
        SudekiMpLogWrite(
            "talos_party event=target_entity status=released reason=verified_party_or_target_lifecycle_ended\r\n"
        );
    }
}

static void poll_lifecycle_snapshot(const char *phase) {
    SwitchSnapshot snapshot;
    DWORD now;
    BOOL changed;

    capture_switch_snapshot(&snapshot);
    now = GetTickCount();
    if (talos_party_prototype_enabled && last_lifecycle_snapshot_valid &&
        last_lifecycle_snapshot.party_count == 4 &&
        snapshot.party_count == 1 &&
        last_lifecycle_snapshot.party[0].character != NULL &&
        snapshot.party[0].character != NULL &&
        last_lifecycle_snapshot.party[1].character != NULL &&
        last_lifecycle_snapshot.party[2].character != NULL &&
        last_lifecycle_snapshot.party[3].character != NULL &&
        snapshot.party[1].character == NULL &&
        snapshot.party[2].character == NULL &&
        snapshot.party[3].character == NULL) {
        talos_party_collapse_observed = TRUE;
        talos_party_collapse_observed_at = now;
        SudekiMpLogFormat(
            "talos_party event=party_collapse phase=poll status=observed before_count=4 after_count=1 lead_before=0x%08lx lead_after=0x%08lx confirmation_window_ms=5000\r\n",
            (unsigned long)(uintptr_t)last_lifecycle_snapshot.party[0].character,
            (unsigned long)(uintptr_t)snapshot.party[0].character
        );
    } else if (talos_party_collapse_observed &&
            (DWORD)(now - talos_party_collapse_observed_at) > 5000u) {
        talos_party_collapse_observed = FALSE;
        SudekiMpLogWrite(
            "talos_party event=party_collapse status=expired reason=no_matching_void_spawn timeout_ms=5000\r\n"
        );
    }
    changed = !last_lifecycle_snapshot_valid ||
        memcmp(&snapshot, &last_lifecycle_snapshot, sizeof(snapshot)) != 0;
    if (!changed && (DWORD)(now - last_lifecycle_heartbeat) < 2000u) {
        return;
    }
    last_lifecycle_snapshot = snapshot;
    last_lifecycle_snapshot_valid = TRUE;
    last_lifecycle_heartbeat = now;
    log_switch_snapshot(changed ? phase : "heartbeat", 0u, 0u);
}

static void CALLBACK lifecycle_timer_callback(
    HWND window,
    UINT message,
    UINT_PTR timer_id,
    DWORD time
) {
    (void)window;
    (void)message;
    (void)timer_id;
    (void)time;
    poll_lifecycle_snapshot("party_changed");
    if (talos_party_postspawn_pending) {
        SwitchSnapshot snapshot;
        DWORD elapsed = GetTickCount() - talos_party_postspawn_started_at;

        capture_switch_snapshot(&snapshot);
        if (elapsed > 10000u) {
            talos_party_postspawn_pending = FALSE;
            SudekiMpLogFormat(
                "talos_party event=postspawn_combat status=expired party_count=%u timeout_ms=10000\r\n",
                (unsigned int)snapshot.party_count
            );
        } else if (snapshot.party_count == 4 &&
                snapshot.party[0].character != NULL &&
                snapshot.party[1].character != NULL &&
                snapshot.party[2].character != NULL &&
                snapshot.party[3].character != NULL) {
            BOOL refreshed;

            /*
             * InternalSpawnPC completes asynchronously.  Keep the native
             * resource initializer alive until every restored actor exists;
             * it is entity-keyed and only initializes each actor once.
             */
            SudekiMpCleanroomEngineMaintainResources();
            if (elapsed >= 1000u) {
                refreshed = SudekiMpCleanroomEngineRefreshCombatMode();
                talos_party_postspawn_pending = FALSE;
                SudekiMpLogFormat(
                    "talos_party event=postspawn_combat status=%s party_count=4 elapsed_ms=%lu policy=initialize_all_then_native_group_combat_refresh\r\n",
                    refreshed ? "confirmed" : "rejected",
                    (unsigned long)elapsed
                );
            }
        }
    }
    maintain_talos_companion_target_policy();
    if (talos_party_restore_armed) {
        DWORD elapsed = GetTickCount() - talos_party_restore_armed_at;
        if (elapsed > 15000u) {
            talos_party_restore_armed = FALSE;
            SudekiMpLogWrite(
                "talos_party event=restore status=expired reason=arena_controller_not_ready timeout_ms=15000\r\n"
            );
        } else if (elapsed >= 500u) {
            (void)complete_talos_party_restore();
        }
    }
}

static void ensure_lifecycle_timer(void) {
    if (lifecycle_timer != 0u) {
        return;
    }
    lifecycle_timer = SetTimer(NULL, 0u, 250u, lifecycle_timer_callback);
    if (lifecycle_timer == 0u) {
        SudekiMpLogWrite(
            "party_lifecycle event=timer status=failed interval_ms=250\r\n"
        );
        return;
    }
    SudekiMpLogWrite(
        "party_lifecycle event=timer status=started interval_ms=250 policy=read_only_change_and_heartbeat_snapshots\r\n"
    );
    poll_lifecycle_snapshot("initial_gameplay_input");
}

static void CALLBACK snapshot_timer_callback(
    HWND window,
    UINT message,
    UINT_PTR timer_id,
    DWORD time
) {
    static const unsigned int next_delays[] = {200u, 750u};
    static const char *phases[] = {
        "after_50ms",
        "after_250ms",
        "after_1000ms"
    };
    DWORD elapsed;

    (void)window;
    (void)message;
    (void)time;
    KillTimer(NULL, timer_id);
    snapshot_timer = 0;
    elapsed = GetTickCount() - snapshot_timer_started;
    log_switch_snapshot(
        phases[snapshot_timer_stage],
        snapshot_action,
        elapsed
    );
    if (snapshot_timer_stage < 2u) {
        unsigned int delay = next_delays[snapshot_timer_stage];
        ++snapshot_timer_stage;
        snapshot_timer = SetTimer(NULL, 0, delay, snapshot_timer_callback);
        if (snapshot_timer == 0) {
            SudekiMpLogWrite(
                "character_switch event=timer_error phase=reschedule\r\n"
            );
        }
    }
}

static void begin_delayed_snapshots(uint32_t action) {
    if (snapshot_timer != 0) {
        KillTimer(NULL, snapshot_timer);
        snapshot_timer = 0;
    }
    snapshot_action = action;
    snapshot_timer_stage = 0;
    snapshot_timer_started = GetTickCount();
    snapshot_timer = SetTimer(NULL, 0, 50, snapshot_timer_callback);
    if (snapshot_timer == 0) {
        SudekiMpLogWrite("character_switch event=timer_error phase=initial\r\n");
    }
}

static void SUDEKIMP_THISCALL trace_character_input(
    void *listener,
    void *event_pointer
) {
    uint32_t *event = (uint32_t *)event_pointer;
    uint32_t action;
    uint32_t pressed;
    uint32_t analog_bits;

    if (event == NULL) {
        original_character_input_handler(listener, event_pointer);
        return;
    }
    ensure_lifecycle_timer();
    action = event[0];
    if (action != ACTION_PREVIOUS_CHARACTER &&
        action != ACTION_NEXT_CHARACTER) {
        original_character_input_handler(listener, event_pointer);
        return;
    }
    pressed = event[2];
    analog_bits = event[3];
    SudekiMpLogFormat(
        "character_switch event=input phase=before action=0x%02lx pressed=%lu analog_bits=0x%08lx listener=0x%08lx\r\n",
        (unsigned long)action,
        (unsigned long)pressed,
        (unsigned long)analog_bits,
        (unsigned long)(uintptr_t)listener
    );
    log_switch_snapshot("before_handler", action, 0);
    original_character_input_handler(listener, event_pointer);
    log_switch_snapshot("after_handler", action, 0);
    if (pressed != 0) {
        begin_delayed_snapshots(action);
    }
}

BOOL SudekiMpInstallCharacterSwitchTrace(
    HMODULE game_module,
    BOOL enable_talos_party_prototype
) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    slot = (void **)(base + RVA_CHARACTER_INPUT_VTABLE_SLOT);
    game_base = base;
    talos_party_prototype_enabled = enable_talos_party_prototype;
    original_character_input_handler = (CharacterInputHandler)(
        base + RVA_CHARACTER_INPUT_HANDLER
    );
    if (!SudekiMpInstallInlineHook(
            &internal_spawn_pc_hook,
            base + RVA_INTERNAL_SPAWN_PC,
            internal_spawn_pc_entry,
            sizeof(internal_spawn_pc_entry),
            trace_internal_spawn_pc)) {
        game_base = NULL;
        original_character_input_handler = NULL;
        return FALSE;
    }
    original_internal_spawn_pc = (InternalSpawnPcFunction)(
        internal_spawn_pc_hook.trampoline
    );
    if (!SudekiMpInstallInlineHook(
            &remove_pc_hook,
            base + RVA_REMOVE_PC,
            remove_pc_entry,
            sizeof(remove_pc_entry),
            trace_remove_pc)) {
        SudekiMpRestoreInlineHook(&internal_spawn_pc_hook);
        game_base = NULL;
        original_character_input_handler = NULL;
        original_internal_spawn_pc = NULL;
        return FALSE;
    }
    original_remove_pc = (RemovePcFunction)remove_pc_hook.trampoline;
    if (talos_party_prototype_enabled) {
        if (!SudekiMpInstallInlineHook(
                &ai_candidate_filter_hook,
                base + RVA_AI_CANDIDATE_FILTER,
                ai_candidate_filter_entry,
                sizeof(ai_candidate_filter_entry),
                talos_party_ai_candidate_filter)) {
            SudekiMpRestoreInlineHook(&remove_pc_hook);
            SudekiMpRestoreInlineHook(&internal_spawn_pc_hook);
            game_base = NULL;
            original_character_input_handler = NULL;
            original_internal_spawn_pc = NULL;
            original_remove_pc = NULL;
            return FALSE;
        }
        original_ai_candidate_filter = (AiCandidateFilterFunction)(
            ai_candidate_filter_hook.trampoline
        );
    }
    if (!SudekiMpInstallPointerHook(
            &character_input_vtable_hook,
            slot,
            original_character_input_handler,
            trace_character_input)) {
        SudekiMpRestoreInlineHook(&ai_candidate_filter_hook);
        SudekiMpRestoreInlineHook(&remove_pc_hook);
        SudekiMpRestoreInlineHook(&internal_spawn_pc_hook);
        game_base = NULL;
        original_character_input_handler = NULL;
        original_internal_spawn_pc = NULL;
        original_remove_pc = NULL;
        original_ai_candidate_filter = NULL;
        return FALSE;
    }
    SudekiMpLogWrite("character_switch_trace_install=success\r\n");
    SudekiMpLogFormat(
        "talos_party_prototype_requested=%s policy=exact_zero_position_tal_spawn_after_four_to_one_void_party_collapse\r\n",
        talos_party_prototype_enabled ? "true" : "false"
    );
    return TRUE;
}

void SudekiMpUninstallCharacterSwitchTrace(void) {
    if (snapshot_timer != 0) {
        KillTimer(NULL, snapshot_timer);
    }
    if (lifecycle_timer != 0) {
        KillTimer(NULL, lifecycle_timer);
    }
    if (talos_party_session_active ||
        talos_party_allies_targeting_active ||
        talos_targeting_state_is_captured()) {
        (void)set_talos_companion_target_policy(FALSE);
    }
    SudekiMpRestorePointerHook(&character_input_vtable_hook);
    SudekiMpRestoreInlineHook(&ai_candidate_filter_hook);
    SudekiMpRestoreInlineHook(&remove_pc_hook);
    SudekiMpRestoreInlineHook(&internal_spawn_pc_hook);
    original_character_input_handler = NULL;
    original_internal_spawn_pc = NULL;
    original_remove_pc = NULL;
    original_ai_candidate_filter = NULL;
    game_base = NULL;
    snapshot_timer = 0;
    lifecycle_timer = 0;
    snapshot_timer_stage = 0;
    snapshot_timer_started = 0;
    snapshot_action = 0;
    ZeroMemory(&last_lifecycle_snapshot, sizeof(last_lifecycle_snapshot));
    last_lifecycle_snapshot_valid = FALSE;
    last_lifecycle_heartbeat = 0u;
    talos_party_prototype_enabled = FALSE;
    talos_party_collapse_observed = FALSE;
    talos_party_collapse_observed_at = 0u;
    talos_party_restore_armed = FALSE;
    talos_party_restore_armed_at = 0u;
    talos_party_postspawn_pending = FALSE;
    talos_party_postspawn_started_at = 0u;
    talos_party_session_active = FALSE;
    talos_party_boss_seen = FALSE;
    talos_party_real_boss_ai = NULL;
    ZeroMemory(
        talos_party_filter_logged_sources,
        sizeof(talos_party_filter_logged_sources)
    );
    talos_party_allies_targeting_active = FALSE;
    ZeroMemory(
        talos_party_targeting_state_captured,
        sizeof(talos_party_targeting_state_captured)
    );
    ZeroMemory(
        talos_party_original_allies_targeting,
        sizeof(talos_party_original_allies_targeting)
    );
}
