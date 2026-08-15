#include "engine/player_combat_context.h"

#include "engine/log.h"

#include <stdint.h>
#include <string.h>

enum {
    RVA_GAME_SPEED_GLOBAL = 0x00408da0u,
    RVA_SKILL_TARGET_NODE_GLOBAL = 0x003c3b44u,
    RVA_SKILL_TARGET_ACTIVE_GLOBAL = 0x003c2fcdu,
    CHARACTER_TARGETER_OFFSET = 0xacu,
    CSKILL_ACTIVE_OFFSET = 0x6cu
};

static SudekiMpPlayerCombatSnapshot contexts[SUDEKIMP_MAX_LOCAL_PLAYERS];
static BOOL global_targeting_active;
static int targeting_owner = -1;
static BOOL realtime_invariant = TRUE;
static int last_speed_state = -1;

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

static void clear_skill(unsigned int player, const char *reason) {
    SudekiMpPlayerCombatSnapshot *context = &contexts[player];

    if (context->active_skill != NULL) {
        SudekiMpLogFormat(
            "combat_context event=skill_end player=%u character=0x%08lx skill=0x%08lx reason=%s phase=%u\r\n",
            player + 1u,
            (unsigned long)(uintptr_t)context->character,
            (unsigned long)(uintptr_t)context->active_skill,
            reason,
            (unsigned int)context->phase
        );
    }
    context->active_skill = NULL;
    context->targeter = NULL;
    context->target_node = NULL;
    context->skill_started_tick = 0u;
    context->targeting_started_tick = 0u;
    context->phase = SUDEKIMP_COMBAT_PHASE_IDLE;
    if (targeting_owner == (int)player) {
        targeting_owner = -1;
    }
}

void SudekiMpCombatContextsReset(void) {
    ZeroMemory(contexts, sizeof(contexts));
    global_targeting_active = FALSE;
    targeting_owner = -1;
    realtime_invariant = TRUE;
    last_speed_state = -1;
}

void SudekiMpCombatContextSetCharacter(unsigned int player, void *character) {
    SudekiMpPlayerCombatSnapshot *context;

    if (player >= SUDEKIMP_MAX_LOCAL_PLAYERS) {
        return;
    }
    context = &contexts[player];
    if (context->character == character) {
        return;
    }
    clear_skill(player, "character_assignment_changed");
    context->character = character;
    SudekiMpLogFormat(
        "combat_context event=character_assignment player=%u character=0x%08lx\r\n",
        player + 1u,
        (unsigned long)(uintptr_t)character
    );
}

void SudekiMpCombatContextSetInputSource(
    unsigned int player,
    SudekiMpCombatInputSourceKind kind,
    void *input_source
) {
    SudekiMpPlayerCombatSnapshot *context;

    if (player >= SUDEKIMP_MAX_LOCAL_PLAYERS) {
        return;
    }
    context = &contexts[player];
    if (context->input_source == input_source &&
        context->input_source_kind == kind) {
        return;
    }
    context->input_source = input_source;
    context->input_source_kind = kind;
    SudekiMpLogFormat(
        "combat_context event=input_assignment player=%u kind=%u source=0x%08lx\r\n",
        player + 1u,
        (unsigned int)kind,
        (unsigned long)(uintptr_t)input_source
    );
}

void SudekiMpCombatContextSetView(
    unsigned int player,
    void *camera,
    void *render_state
) {
    SudekiMpPlayerCombatSnapshot *context;

    if (player >= SUDEKIMP_MAX_LOCAL_PLAYERS) {
        return;
    }
    context = &contexts[player];
    context->viewport_camera = camera;
    context->render_state = render_state;
}

int SudekiMpCombatContextFindPlayer(void *character) {
    unsigned int player;

    if (character == NULL) {
        return -1;
    }
    for (player = 0u; player < SUDEKIMP_MAX_LOCAL_PLAYERS; ++player) {
        if (contexts[player].character == character) {
            return (int)player;
        }
    }
    return -1;
}

void SudekiMpCombatContextSkillStarted(void *character, void *skill) {
    int player = SudekiMpCombatContextFindPlayer(character);
    SudekiMpPlayerCombatSnapshot *context;

    if (player < 0 || skill == NULL) {
        return;
    }
    context = &contexts[player];
    context->active_skill = skill;
    context->targeter = readable_memory(character, CHARACTER_TARGETER_OFFSET +
            sizeof(void *)) ?
        *(void **)((uint8_t *)character + CHARACTER_TARGETER_OFFSET) : NULL;
    context->target_node = NULL;
    context->skill_started_tick = GetTickCount();
    context->targeting_started_tick = 0u;
    context->phase = SUDEKIMP_COMBAT_PHASE_EXECUTING;
    SudekiMpLogFormat(
        "combat_context event=skill_start player=%u character=0x%08lx skill=0x%08lx targeter=0x%08lx world_time_policy=realtime_only protection_policy=native_task_lifetime\r\n",
        (unsigned int)player + 1u,
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)skill,
        (unsigned long)(uintptr_t)context->targeter
    );
}

void SudekiMpCombatContextSkillEnded(void *skill) {
    unsigned int player;

    if (skill == NULL) {
        return;
    }
    for (player = 0u; player < SUDEKIMP_MAX_LOCAL_PLAYERS; ++player) {
        if (contexts[player].active_skill == skill) {
            clear_skill(player, "native_skill_cleanup");
            return;
        }
    }
}

static int newest_active_player(void) {
    int selected = -1;
    DWORD selected_tick = 0u;
    unsigned int player;

    for (player = 0u; player < SUDEKIMP_MAX_LOCAL_PLAYERS; ++player) {
        SudekiMpPlayerCombatSnapshot *context = &contexts[player];
        if (context->active_skill != NULL &&
            (selected < 0 || (LONG)(context->skill_started_tick - selected_tick) > 0)) {
            selected = (int)player;
            selected_tick = context->skill_started_tick;
        }
    }
    return selected;
}

void SudekiMpCombatContextsPollState(
    BOOL targeting_active,
    void *target_node,
    int current_speed_mode,
    int requested_speed_mode,
    BOOL paused
) {
    BOOL was_targeting = global_targeting_active;
    int speed_state;
    unsigned int player;

    realtime_invariant = current_speed_mode == 0 &&
        requested_speed_mode == 0 && !paused;
    speed_state = realtime_invariant ? 1 : 0;
    if (speed_state != last_speed_state) {
        last_speed_state = speed_state;
        SudekiMpLogFormat(
            "combat_context event=world_time state=%s current_mode=%d requested_mode=%d paused=%u policy=shared_simulation_1x\r\n",
            realtime_invariant ? "realtime" : "reject_mod_skill_input",
            current_speed_mode,
            requested_speed_mode,
            (unsigned int)paused
        );
    }

    for (player = 0u; player < SUDEKIMP_MAX_LOCAL_PLAYERS; ++player) {
        SudekiMpPlayerCombatSnapshot *context = &contexts[player];
        if (context->active_skill != NULL && readable_memory(
                (uint8_t *)context->active_skill + CSKILL_ACTIVE_OFFSET,
                1u) &&
            *(uint8_t *)((uint8_t *)context->active_skill +
                CSKILL_ACTIVE_OFFSET) == 0u) {
            clear_skill(player, "native_active_byte_cleared");
        }
    }

    global_targeting_active = targeting_active;
    if (targeting_active && !was_targeting) {
        targeting_owner = newest_active_player();
        if (targeting_owner >= 0) {
            SudekiMpPlayerCombatSnapshot *context = &contexts[targeting_owner];
            context->phase = SUDEKIMP_COMBAT_PHASE_TARGETING;
            context->target_node = target_node;
            context->targeting_started_tick = GetTickCount();
        }
        SudekiMpLogFormat(
            "combat_context event=targeting_begin owner_player=%d target_node=0x%08lx policy=single_native_global_context_fail_safe\r\n",
            targeting_owner < 0 ? 0 : targeting_owner + 1,
            (unsigned long)(uintptr_t)target_node
        );
    } else if (targeting_active && targeting_owner >= 0) {
        contexts[targeting_owner].target_node = target_node;
    } else if (!targeting_active && was_targeting) {
        DWORD duration = 0u;
        if (targeting_owner >= 0) {
            SudekiMpPlayerCombatSnapshot *context = &contexts[targeting_owner];
            duration = GetTickCount() - context->targeting_started_tick;
            context->targeting_started_tick = 0u;
            context->target_node = NULL;
            context->phase = context->active_skill == NULL ?
                SUDEKIMP_COMBAT_PHASE_IDLE :
                SUDEKIMP_COMBAT_PHASE_EXECUTING;
        }
        SudekiMpLogFormat(
            "combat_context event=targeting_end owner_player=%d duration_ms=%lu policy=measure_native_protection_window\r\n",
            targeting_owner < 0 ? 0 : targeting_owner + 1,
            (unsigned long)duration
        );
        targeting_owner = -1;
    }
}

void SudekiMpCombatContextsPollGame(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    uint8_t *game_speed;
    BOOL target_active;
    void *target_node;

    if (base == NULL || !readable_memory(
            base + RVA_GAME_SPEED_GLOBAL,
            sizeof(game_speed)) ||
        !readable_memory(
            base + RVA_SKILL_TARGET_ACTIVE_GLOBAL,
            sizeof(uint8_t)) ||
        !readable_memory(
            base + RVA_SKILL_TARGET_NODE_GLOBAL,
            sizeof(target_node))) {
        return;
    }
    game_speed = *(uint8_t **)(base + RVA_GAME_SPEED_GLOBAL);
    if (!readable_memory(game_speed, 0x29u)) {
        return;
    }
    target_active = *(uint8_t *)(base + RVA_SKILL_TARGET_ACTIVE_GLOBAL) != 0u;
    target_node = *(void **)(base + RVA_SKILL_TARGET_NODE_GLOBAL);
    SudekiMpCombatContextsPollState(
        target_active,
        target_node,
        *(int *)(game_speed + 0x20u),
        *(int *)(game_speed + 0x24u),
        *(uint8_t *)(game_speed + 0x28u) != 0u
    );
}

BOOL SudekiMpCombatContextCanStartSkill(
    unsigned int player,
    const char **reason
) {
    if (reason == NULL) {
        return FALSE;
    }
    if (player >= SUDEKIMP_MAX_LOCAL_PLAYERS ||
        contexts[player].character == NULL) {
        *reason = "player_context_unassigned";
        return FALSE;
    }
    if (!realtime_invariant) {
        *reason = "shared_world_not_realtime";
        return FALSE;
    }
    if (global_targeting_active) {
        *reason = "native_global_targeting_busy";
        return FALSE;
    }
    if (contexts[player].active_skill != NULL) {
        *reason = "player_skill_already_active";
        return FALSE;
    }
    *reason = "accepted";
    return TRUE;
}

BOOL SudekiMpCombatContextGlobalTargetingActive(void) {
    return global_targeting_active;
}

BOOL SudekiMpCombatContextRealtimeInvariantHolds(void) {
    return realtime_invariant;
}

BOOL SudekiMpCombatContextGetSnapshot(
    unsigned int player,
    SudekiMpPlayerCombatSnapshot *snapshot
) {
    if (player >= SUDEKIMP_MAX_LOCAL_PLAYERS || snapshot == NULL) {
        return FALSE;
    }
    *snapshot = contexts[player];
    return TRUE;
}
