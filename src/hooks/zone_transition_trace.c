#include "hooks/zone_transition_trace.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__cdecl *ZoneFunction)(const char *zone_name);
typedef void (__cdecl *SetPlayerPositionFunction)(
    float x,
    float y,
    float z
);
typedef void (__attribute__((fastcall)) *InternalPositionSetterFunction)(
    void *position,
    const float *coordinates
);
typedef BOOL (__attribute__((thiscall)) *SetRenderCameraFunction)(
    void *manager,
    const char *name
);
/* FUN_004352D0 ends with ret 4; this is stdcall, not cdecl. */
typedef unsigned char (__attribute__((stdcall)) *TemporaryCameraStateUpdateFunction)(
    void *camera_state
);
typedef void (__attribute__((thiscall)) *SwitchMainZoneFunction)(
    void *world,
    const char *zone_name
);
typedef void (__attribute__((thiscall)) *EnterTemporaryZoneFunction)(
    void *world,
    const char *zone_name,
    const void *resource_name
);
typedef void (__attribute__((thiscall)) *ExitTemporaryZoneFunction)(
    void *world
);
typedef unsigned char (__attribute__((thiscall)) *DoorActivateFunction)(
    void *door,
    BOOL requested,
    BOOL forced
);

enum {
    RVA_SET_ZONE_NOW = 0x00007910u,
    RVA_ENTER_ZONE = 0x00007970u,
    RVA_SWITCH_ZONE_NOW = 0x00007990u,
    RVA_LOAD_ZONE = 0x00007b80u,
    RVA_SWITCH_MAIN_ZONE = 0x00006380u,
    RVA_DOOR_ACTIVATE_FROM_SCRIPT = 0x000ce3a0u,
    RVA_ENTER_TEMPORARY_ZONE = 0x000064b0u,
    RVA_EXIT_TEMPORARY_ZONE = 0x00006710u,
    RVA_SET_PLAYER_POSITION = 0x00104ed0u,
    RVA_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_TEMPORARY_CAMERA_STATE_UPDATE = 0x000352d0u,
};

static const uint8_t set_zone_now_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x51u
};
static const uint8_t enter_zone_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u
};
static const uint8_t switch_zone_now_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u
};
static const uint8_t load_zone_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x8bu, 0x0du, 0x10u, 0x8du,
    0x80u, 0x00u
};
static const uint8_t switch_main_zone_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u, 0x8bu, 0xf1u
};
static const uint8_t door_activate_entry[] = {
    0x80u, 0x7cu, 0x24u, 0x04u, 0x00u, 0x56u, 0x8bu, 0xf1u
};
static const uint8_t enter_temporary_zone_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu, 0x24u
};
static const uint8_t exit_temporary_zone_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf0u, 0x81u, 0xecu, 0x84u,
    0x00u, 0x00u, 0x00u
};
static const uint8_t set_player_position_entry[] = {
    /* The following absolute global is relocated at runtime; gate only the
     * stable prologue and leave the relocated operand untouched. */
    0x83u, 0xecu, 0x18u, 0x8bu, 0x0du
};
static const uint8_t internal_position_setter_entry[] = {
    0xd9u, 0x41u, 0x18u, 0xd9u, 0x02u
};
static const uint8_t set_render_camera_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t temporary_camera_state_update_entry[] = {
    /* Include the complete `and esp, -8` instruction; cutting it at
     * five bytes makes the inline trampoline jump into its final immediate. */
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};

static SudekiMpInlineHook set_zone_now_hook;
static SudekiMpInlineHook enter_zone_hook;
static SudekiMpInlineHook switch_zone_now_hook;
static SudekiMpInlineHook load_zone_hook;
static SudekiMpInlineHook switch_main_zone_hook;
static SudekiMpInlineHook door_activate_hook;
static SudekiMpInlineHook enter_temporary_zone_hook;
static SudekiMpInlineHook exit_temporary_zone_hook;
static SudekiMpInlineHook set_player_position_hook;
static SudekiMpInlineHook internal_position_setter_hook;
static SudekiMpInlineHook set_render_camera_hook;
static SudekiMpInlineHook temporary_camera_state_update_hook;
static ZoneFunction original_set_zone_now;
static ZoneFunction original_enter_zone;
static ZoneFunction original_switch_zone_now;
static ZoneFunction original_load_zone;
static SwitchMainZoneFunction original_switch_main_zone;
static DoorActivateFunction original_door_activate;
static EnterTemporaryZoneFunction original_enter_temporary_zone;
static ExitTemporaryZoneFunction original_exit_temporary_zone;
static SetPlayerPositionFunction original_set_player_position;
static InternalPositionSetterFunction original_internal_position_setter;
static SetRenderCameraFunction original_set_render_camera;
static TemporaryCameraStateUpdateFunction original_temporary_camera_state_update;
static HMODULE trace_module;
static void *last_world;
static char current_world_name[64];
static BOOL current_world_confirmed;
static char current_temporary_name[64];
static SudekiMpResourceName active_temporary_resource;
static BOOL active_temporary_resource_valid;
static unsigned int temporary_position_samples;
static unsigned int temporary_camera_samples;

#define ZONE_ARRIVAL_CONTEXT_CAPACITY 32u

/* This is deliberately a small authored registry, not a guessed list of
 * arbitrary strings.  These names are the same destinations exposed by the
 * cleanroom traversal page and confirmed in the archive/door research.  A
 * first request is allowed to enter the native pipeline; the position hook
 * then records the real actor anchors without requiring a manual visit. */
static const char *known_traversal_worlds[] = {
    "NewBrightwater",
    "Illumina_Countryside_Hub",
    "Illumina_Countryside_NE",
    "Illumina_Countryside_SE",
    "Illumina_Countryside_SW",
    "Illumina_Countryside_NW"
};

static const char *known_traversal_interiors[][2] = {
    {"NewBrightwater", "LNBr_Church"},
    {"NewBrightwater", "LNBr_Kamo_shop"},
    {"NewBrightwater", "LNBr_Kilks_house"},
    {"NewBrightwater", "LNBr_Lighthouse"},
    {"NewBrightwater", "LNBr_Salty_dog_Inn"},
    {"NewBrightwater", "LNBr_ShortTent"},
    {"NewBrightwater", "LNBr_TallTent01"},
    {"NewBrightwater", "LNBr_TallTent02"},
    {"Illumina_Countryside_SE", "LICo_Athlos_Shack"},
    {"Illumina_Countryside_SE", "LICo_Frappe_Farm"},
    {"Illumina_Countryside_SE", "LICo_Porkins"},
    {"Illumina_Countryside_SE", "LICo_SW_Trader_Cave"}
};

typedef struct SudekiMpZoneArrivalContext {
    BOOL valid;
    BOOL temporary;
    char world_name[64];
    char destination_name[64];
    float actor_positions[SUDEKIMP_CLEANROOM_ACTOR_COUNT][3];
    BOOL actor_position_valid[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
    float fallback_position[3];
    BOOL fallback_position_valid;
    unsigned short camera_index;
} SudekiMpZoneArrivalContext;

static SudekiMpZoneArrivalContext arrival_contexts[
    ZONE_ARRIVAL_CONTEXT_CAPACITY];
static SudekiMpZoneArrivalContext *arrival_capture_context;
static BOOL arrival_capture_armed;
static BOOL arrival_reapply_pending;
static DWORD arrival_reapply_after;
static DWORD arrival_reapply_deadline;
static unsigned int arrival_reapply_attempts;

static uint32_t zone_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void release_active_temporary_resource(void) {
    if (active_temporary_resource_valid) {
        SudekiMpCleanroomEngineReleaseResourceName(
            &active_temporary_resource);
        active_temporary_resource_valid = FALSE;
    }
}

static BOOL readable_zone_bytes(const char *source, size_t size) {
    MEMORY_BASIC_INFORMATION information;

    if (source == NULL || size == 0u ||
        VirtualQuery(source, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    return (uintptr_t)source + size <=
        (uintptr_t)information.BaseAddress + information.RegionSize;
}

static void copy_zone_name(
    const char *source,
    char destination[64]
) {
    size_t index;

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }
    for (index = 0u; index + 1u < 64u; ++index) {
        if (!readable_zone_bytes(source + index, 1u)) {
            break;
        }
        destination[index] = source[index];
        if (destination[index] == '\0') {
            return;
        }
    }
    destination[index < 63u ? index : 63u] = '\0';
}

static BOOL finite_zone_position(const float *position) {
    return position != NULL &&
        isfinite(position[0]) && isfinite(position[1]) &&
        isfinite(position[2]) && fabsf(position[0]) < 1000000.0f &&
        fabsf(position[1]) < 1000000.0f &&
        fabsf(position[2]) < 1000000.0f;
}

static BOOL nonzero_zone_position(const float *position) {
    return finite_zone_position(position) &&
        (fabsf(position[0]) > 0.0001f ||
         fabsf(position[1]) > 0.0001f ||
         fabsf(position[2]) > 0.0001f);
}

static SudekiMpZoneArrivalContext *find_arrival_context(
    BOOL temporary,
    const char *world_name,
    const char *destination_name,
    BOOL create
) {
    unsigned int index;
    SudekiMpZoneArrivalContext *free_context = NULL;

    if (world_name == NULL || destination_name == NULL ||
        world_name[0] == '\0' || destination_name[0] == '\0') {
        return NULL;
    }
    for (index = 0u; index < ZONE_ARRIVAL_CONTEXT_CAPACITY; ++index) {
        SudekiMpZoneArrivalContext *context = &arrival_contexts[index];
        if (!context->valid) {
            if (free_context == NULL) {
                free_context = context;
            }
            continue;
        }
        if (context->temporary == temporary &&
            _stricmp(context->world_name, world_name) == 0 &&
            _stricmp(context->destination_name, destination_name) == 0) {
            return context;
        }
    }
    if (!create && free_context == NULL) {
        return NULL;
    }
    if (free_context == NULL) {
        /* Replace the oldest/last slot only when the cache is full.  The
         * context is a research cache, never a gameplay save format. */
        free_context = &arrival_contexts[ZONE_ARRIVAL_CONTEXT_CAPACITY - 1u];
    }
    ZeroMemory(free_context, sizeof(*free_context));
    free_context->temporary = temporary;
    lstrcpynA(free_context->world_name, world_name,
        sizeof(free_context->world_name));
    lstrcpynA(free_context->destination_name, destination_name,
        sizeof(free_context->destination_name));
    return free_context;
}

static void arm_arrival_capture(
    BOOL temporary,
    const char *world_name,
    const char *destination_name
) {
    arrival_capture_context = find_arrival_context(
        temporary, world_name, destination_name, TRUE);
    arrival_capture_armed = arrival_capture_context != NULL;
    if (arrival_capture_armed) {
        SudekiMpLogFormat(
            "zone_transition event=arrival_context_capture armed=true "
            "temporary=%d world=%s destination=%s policy=cache_native_anchor\r\n",
            temporary ? 1 : 0,
            world_name,
            destination_name);
    }
}

static BOOL context_has_actor_anchor(
    const SudekiMpZoneArrivalContext *context
) {
    unsigned int index;

    if (context == NULL || !context->valid) {
        return FALSE;
    }
    for (index = 0u; index < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++index) {
        if (context->actor_position_valid[index]) {
            return TRUE;
        }
    }
    return FALSE;
}

static void capture_arrival_position(
    void *position,
    const float coordinates[3]
) {
    unsigned int actor_index;
    BOOL matched = FALSE;

    if (!arrival_capture_armed || arrival_capture_context == NULL ||
        coordinates == NULL || !nonzero_zone_position(coordinates)) {
        return;
    }
    for (actor_index = 0u;
         actor_index < SUDEKIMP_CLEANROOM_ACTOR_COUNT;
         ++actor_index) {
        uint8_t *actor = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            (SudekiMpCleanroomActor)actor_index);
        void *actor_position;

        if (!readable_zone_bytes((const char *)actor, 0x48u)) {
            continue;
        }
        actor_position = *(void **)(actor + 0x44u);
        if (actor_position != position ||
            arrival_capture_context->actor_position_valid[actor_index]) {
            continue;
        }
        memcpy(
            arrival_capture_context->actor_positions[actor_index],
            coordinates,
            sizeof(float) * 3u);
        arrival_capture_context->actor_position_valid[actor_index] = TRUE;
        matched = TRUE;
        SudekiMpLogFormat(
            "zone_transition event=arrival_context_actor actor=%s "
            "x_bits=%08lx y_bits=%08lx z_bits=%08lx\r\n",
            SudekiMpCleanroomActorLabel(
                (SudekiMpCleanroomActor)actor_index),
            (unsigned long)zone_float_bits(coordinates[0]),
            (unsigned long)zone_float_bits(coordinates[1]),
            (unsigned long)zone_float_bits(coordinates[2]));
    }
    if (!matched && !arrival_capture_context->fallback_position_valid) {
        memcpy(
            arrival_capture_context->fallback_position,
            coordinates,
            sizeof(float) * 3u);
        arrival_capture_context->fallback_position_valid = TRUE;
    }
    arrival_capture_context->valid = TRUE;
}

static void log_zone_phase(
    const char *event,
    const char *phase,
    const char *zone_name,
    const void *world
) {
    char safe_name[64];

    copy_zone_name(zone_name, safe_name);
    SudekiMpLogFormat(
        "zone_transition event=%s phase=%s zone=%s zone_ptr=%p world=%p "
        "policy=observation_only\r\n",
        event,
        phase,
        safe_name[0] == '\0' ? "<empty-or-unreadable>" : safe_name,
        zone_name,
        world
    );
}

static void log_transition_callsite(const char *event) {
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    uintptr_t base = (uintptr_t)trace_module;
    unsigned long rva = 0ul;

    if (base != 0u && caller >= base && caller - base < 0x1000000u) {
        rva = (unsigned long)(caller - base);
    }
    SudekiMpLogFormat(
        "zone_transition event=%s callsite=%p callsite_rva=0x%08lx "
        "policy=observation_only\r\n",
        event,
        (void *)caller,
        rva
    );
}

static void __cdecl trace_set_zone_now(const char *zone_name) {
    log_transition_callsite("set_zone_now");
    log_zone_phase("set_zone_now", "before", zone_name, NULL);
    current_world_confirmed = FALSE;
    copy_zone_name(zone_name, current_world_name);
    arm_arrival_capture(FALSE, current_world_name, current_world_name);
    original_set_zone_now(zone_name);
    log_zone_phase("set_zone_now", "after", zone_name, NULL);
}

static void __cdecl trace_enter_zone(const char *zone_name) {
    log_transition_callsite("enter_zone");
    log_zone_phase("enter_zone", "before", zone_name, NULL);
    copy_zone_name(zone_name, current_world_name);
    if (!arrival_capture_armed) {
        arm_arrival_capture(FALSE, current_world_name, current_world_name);
    }
    original_enter_zone(zone_name);
    current_world_confirmed = TRUE;
    log_zone_phase("enter_zone", "after", zone_name, NULL);
}

static void __cdecl trace_switch_zone_now(const char *zone_name) {
    log_transition_callsite("switch_zone_now");
    log_zone_phase("switch_zone_now", "before", zone_name, NULL);
    current_world_confirmed = FALSE;
    original_switch_zone_now(zone_name);
    copy_zone_name(zone_name, current_world_name);
    log_zone_phase("switch_zone_now", "after", zone_name, NULL);
}

static void __cdecl trace_load_zone(const char *zone_name) {
    log_zone_phase("load_zone", "before", zone_name, NULL);
    original_load_zone(zone_name);
    log_zone_phase("load_zone", "after", zone_name, NULL);
}

static void __attribute__((thiscall)) trace_switch_main_zone(
    void *world,
    const char *zone_name
) {
    log_transition_callsite("switch_main_zone");
    log_zone_phase("switch_main_zone", "before", zone_name, world);
    copy_zone_name(zone_name, current_world_name);
    if (!arrival_capture_armed) {
        arm_arrival_capture(FALSE, current_world_name, current_world_name);
    }
    original_switch_main_zone(world, zone_name);
    last_world = world;
    current_world_confirmed = TRUE;
    log_zone_phase("switch_main_zone", "after", zone_name, world);
}

static unsigned char __attribute__((thiscall)) trace_door_activate(
    void *door,
    BOOL requested,
    BOOL forced
) {
    SudekiMpLogFormat(
        "zone_transition event=door_activate phase=before door=%p "
        "requested=%d forced=%d policy=observation_only\r\n",
        door,
        requested ? 1 : 0,
        forced ? 1 : 0
    );
    {
        unsigned char result = original_door_activate(door, requested, forced);
        SudekiMpLogFormat(
            "zone_transition event=door_activate phase=after door=%p "
            "requested=%d forced=%d result=%u policy=observation_only\r\n",
            door,
            requested ? 1 : 0,
            forced ? 1 : 0,
            (unsigned int)result
        );
        return result;
    }
}

static void __attribute__((thiscall)) trace_enter_temporary_zone(
    void *world,
    const char *zone_name,
    const void *resource_name
) {
    last_world = world;
    copy_zone_name(zone_name, current_temporary_name);
    temporary_position_samples = 0u;
    temporary_camera_samples = 0u;
    log_zone_phase("enter_temporary_zone", "before", zone_name, world);
    arm_arrival_capture(TRUE, current_world_name, current_temporary_name);
    SudekiMpLogFormat(
        "zone_transition event=enter_temporary_zone_resource resource=%p "
        "policy=observation_only\r\n",
        resource_name
    );
    original_enter_temporary_zone(world, zone_name, resource_name);
    copy_zone_name(zone_name, current_temporary_name);
    if (readable_zone_bytes((const char *)world, 0x42u)) {
        if (arrival_capture_context != NULL) {
            arrival_capture_context->camera_index =
                *(uint16_t *)((uint8_t *)world + 0x40u);
        }
        SudekiMpLogFormat(
            "zone_transition event=temporary_world_state phase=after "
            "world=%p start_x_bits=%08lx start_y_bits=%08lx "
            "start_z_bits=%08lx orient_x_bits=%08lx orient_y_bits=%08lx "
            "orient_z_bits=%08lx camera_index=%u policy=observation_only\r\n",
            world,
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x28u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x2cu)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x30u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x34u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x38u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x3cu)),
            (unsigned int)*(uint16_t *)((uint8_t *)world + 0x40u));
    }
    log_zone_phase("enter_temporary_zone", "after", zone_name, world);
}

static void __attribute__((thiscall)) trace_exit_temporary_zone(void *world) {
    last_world = world;
    log_zone_phase("exit_temporary_zone", "before", NULL, world);
    original_exit_temporary_zone(world);
    current_temporary_name[0] = '\0';
    log_zone_phase("exit_temporary_zone", "after", NULL, world);
}

static void __cdecl trace_set_player_position(float x, float y, float z) {
    SudekiMpLogFormat(
        "zone_transition event=set_player_position phase=before "
        "x_bits=%08lx y_bits=%08lx z_bits=%08lx policy=observation_only\r\n",
        (unsigned long)zone_float_bits(x),
        (unsigned long)zone_float_bits(y),
        (unsigned long)zone_float_bits(z));
    original_set_player_position(x, y, z);
    SudekiMpLogFormat(
        "zone_transition event=set_player_position phase=after "
        "x_bits=%08lx y_bits=%08lx z_bits=%08lx policy=observation_only\r\n",
        (unsigned long)zone_float_bits(x),
        (unsigned long)zone_float_bits(y),
        (unsigned long)zone_float_bits(z));
}

static void __attribute__((fastcall)) trace_internal_position_setter(
    void *position,
    const float *coordinates
) {
    capture_arrival_position(position, coordinates);
    if (current_temporary_name[0] != '\0' &&
        coordinates != NULL && temporary_position_samples < 16u &&
        readable_zone_bytes((const char *)coordinates, sizeof(float) * 3u)) {
        SudekiMpLogFormat(
            "zone_transition event=internal_position_setter "
            "temporary=%s position=%p sample=%u "
            "x_bits=%08lx y_bits=%08lx z_bits=%08lx "
            "policy=observation_only\r\n",
            current_temporary_name,
            position,
            temporary_position_samples,
            (unsigned long)zone_float_bits(coordinates[0]),
            (unsigned long)zone_float_bits(coordinates[1]),
            (unsigned long)zone_float_bits(coordinates[2]));
        ++temporary_position_samples;
    }
    original_internal_position_setter(position, coordinates);
}

static BOOL __attribute__((thiscall)) trace_set_render_camera(
    void *manager,
    const char *name
) {
    char safe_name[64];
    BOOL result;

    copy_zone_name(name, safe_name);
    SudekiMpLogFormat(
        "zone_transition event=set_render_camera phase=before "
        "temporary=%s manager=%p name=%s policy=observation_only\r\n",
        current_temporary_name[0] == '\0' ? "<none>" : current_temporary_name,
        manager,
        safe_name[0] == '\0' ? "<empty-or-unreadable>" : safe_name);
    result = original_set_render_camera(manager, name);
    SudekiMpLogFormat(
        "zone_transition event=set_render_camera phase=after "
        "temporary=%s manager=%p name=%s result=%d policy=observation_only\r\n",
        current_temporary_name[0] == '\0' ? "<none>" : current_temporary_name,
        manager,
        safe_name[0] == '\0' ? "<empty-or-unreadable>" : safe_name,
        result ? 1 : 0);
    return result;
}

static unsigned char __attribute__((stdcall)) trace_temporary_camera_state_update(
    void *camera_state
) {
    unsigned short before = 0xffffu;
    unsigned char result;

    if (current_temporary_name[0] != '\0' && camera_state != NULL &&
        temporary_camera_samples < 8u &&
        readable_zone_bytes((const char *)camera_state, 0x30u)) {
        before = *(unsigned short *)((uint8_t *)camera_state + 0x2eu);
        SudekiMpLogFormat(
            "zone_transition event=temporary_camera_state_update "
            "phase=before temporary=%s state=%p camera_index=%u "
            "policy=observation_only\r\n",
            current_temporary_name,
            camera_state,
            (unsigned int)before);
    }
    result = original_temporary_camera_state_update(camera_state);
    if (current_temporary_name[0] != '\0' && camera_state != NULL &&
        temporary_camera_samples < 8u &&
        readable_zone_bytes((const char *)camera_state, 0x30u)) {
        SudekiMpLogFormat(
            "zone_transition event=temporary_camera_state_update "
            "phase=after temporary=%s state=%p camera_index_before=%u "
            "camera_index_after=%u result=%u policy=observation_only\r\n",
            current_temporary_name,
            camera_state,
            (unsigned int)before,
            (unsigned int)*(unsigned short *)((uint8_t *)camera_state + 0x2eu),
            (unsigned int)result);
        ++temporary_camera_samples;
    }
    return result;
}

static BOOL install_zone_hook(
    SudekiMpInlineHook *hook,
    uint8_t *target,
    const uint8_t *entry,
    size_t entry_size,
    const void *replacement
) {
    return SudekiMpInstallInlineHook(
        hook,
        target,
        entry,
        entry_size,
        replacement
    );
}

static BOOL install_named_zone_hook(
    const char *name,
    SudekiMpInlineHook *hook,
    uint8_t *target,
    const uint8_t *entry,
    size_t entry_size,
    const void *replacement
) {
    if (install_zone_hook(hook, target, entry, entry_size, replacement)) {
        return TRUE;
    }
    SudekiMpLogFormat(
        "zone_transition_trace_install status=rejected hook=%s "
        "target=%p expected_size=%lu win32_error=%lu\r\n",
        name,
        target,
        (unsigned long)entry_size,
        (unsigned long)GetLastError()
    );
    return FALSE;
}

BOOL SudekiMpInstallZoneTransitionTrace(HMODULE game_module) {
    uint8_t *base;

    if (game_module == NULL || trace_module != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (!install_named_zone_hook(
            "set_zone_now",
            &set_zone_now_hook,
            base + RVA_SET_ZONE_NOW,
            set_zone_now_entry,
            sizeof(set_zone_now_entry),
            trace_set_zone_now) ||
        !install_named_zone_hook(
            "enter_zone",
            &enter_zone_hook,
            base + RVA_ENTER_ZONE,
            enter_zone_entry,
            sizeof(enter_zone_entry),
            trace_enter_zone) ||
        !install_named_zone_hook(
            "switch_zone_now",
            &switch_zone_now_hook,
            base + RVA_SWITCH_ZONE_NOW,
            switch_zone_now_entry,
            sizeof(switch_zone_now_entry),
            trace_switch_zone_now) ||
        !install_named_zone_hook(
            "switch_main_zone",
            &switch_main_zone_hook,
            base + RVA_SWITCH_MAIN_ZONE,
            switch_main_zone_entry,
            sizeof(switch_main_zone_entry),
            trace_switch_main_zone)) {
        SudekiMpUninstallZoneTransitionTrace();
        return FALSE;
    }
    if (!install_named_zone_hook(
            "load_zone",
            &load_zone_hook,
            base + RVA_LOAD_ZONE,
            load_zone_entry,
            sizeof(load_zone_entry),
            trace_load_zone)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=load_zone status=unavailable "
            "policy=continue_without_loadzone_hook\r\n"
        );
    }
    if (!install_named_zone_hook(
            "door_activate_from_script",
            &door_activate_hook,
            base + RVA_DOOR_ACTIVATE_FROM_SCRIPT,
            door_activate_entry,
            sizeof(door_activate_entry),
            trace_door_activate)) {
        SudekiMpUninstallZoneTransitionTrace();
        return FALSE;
    }
    if (!install_named_zone_hook(
            "enter_temporary_zone",
            &enter_temporary_zone_hook,
            base + RVA_ENTER_TEMPORARY_ZONE,
            enter_temporary_zone_entry,
            sizeof(enter_temporary_zone_entry),
            trace_enter_temporary_zone) ||
        !install_named_zone_hook(
            "exit_temporary_zone",
            &exit_temporary_zone_hook,
            base + RVA_EXIT_TEMPORARY_ZONE,
            exit_temporary_zone_entry,
            sizeof(exit_temporary_zone_entry),
            trace_exit_temporary_zone)) {
        SudekiMpUninstallZoneTransitionTrace();
        return FALSE;
    }
    if (!install_named_zone_hook(
            "set_player_position",
            &set_player_position_hook,
            base + RVA_SET_PLAYER_POSITION,
            set_player_position_entry,
            sizeof(set_player_position_entry),
            trace_set_player_position)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=set_player_position "
            "status=unavailable policy=continue_without_position_hook\r\n");
    }
    if (!install_named_zone_hook(
            "internal_position_setter",
            &internal_position_setter_hook,
            base + RVA_INTERNAL_POSITION_SETTER,
            internal_position_setter_entry,
            sizeof(internal_position_setter_entry),
            trace_internal_position_setter)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=internal_position_setter "
            "status=unavailable policy=continue_without_internal_position_hook\r\n");
    }
    if (!install_named_zone_hook(
            "set_render_camera",
            &set_render_camera_hook,
            base + RVA_SET_RENDER_CAMERA,
            set_render_camera_entry,
            sizeof(set_render_camera_entry),
            trace_set_render_camera)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=set_render_camera "
            "status=unavailable policy=continue_without_camera_hook\r\n");
    }
    if (!install_named_zone_hook(
            "temporary_camera_state_update",
            &temporary_camera_state_update_hook,
            base + RVA_TEMPORARY_CAMERA_STATE_UPDATE,
            temporary_camera_state_update_entry,
            sizeof(temporary_camera_state_update_entry),
            trace_temporary_camera_state_update)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=temporary_camera_state_update "
            "status=unavailable policy=continue_without_camera_state_hook\r\n");
    }
    original_set_zone_now = (ZoneFunction)set_zone_now_hook.trampoline;
    original_enter_zone = (ZoneFunction)enter_zone_hook.trampoline;
    original_switch_zone_now = (ZoneFunction)switch_zone_now_hook.trampoline;
    original_load_zone = (ZoneFunction)load_zone_hook.trampoline;
    original_switch_main_zone =
        (SwitchMainZoneFunction)switch_main_zone_hook.trampoline;
    original_door_activate =
        (DoorActivateFunction)door_activate_hook.trampoline;
    original_enter_temporary_zone =
        (EnterTemporaryZoneFunction)enter_temporary_zone_hook.trampoline;
    original_exit_temporary_zone =
        (ExitTemporaryZoneFunction)exit_temporary_zone_hook.trampoline;
    original_set_player_position =
        (SetPlayerPositionFunction)set_player_position_hook.trampoline;
    original_internal_position_setter =
        (InternalPositionSetterFunction)internal_position_setter_hook.trampoline;
    original_set_render_camera =
        (SetRenderCameraFunction)set_render_camera_hook.trampoline;
    original_temporary_camera_state_update =
        (TemporaryCameraStateUpdateFunction)
            temporary_camera_state_update_hook.trampoline;
    trace_module = game_module;
    last_world = NULL;
    current_world_confirmed = FALSE;
    current_world_name[0] = '\0';
    current_temporary_name[0] = '\0';
    ZeroMemory(&active_temporary_resource,
        sizeof(active_temporary_resource));
    active_temporary_resource_valid = FALSE;
    ZeroMemory(arrival_contexts, sizeof(arrival_contexts));
    arrival_capture_context = NULL;
    arrival_capture_armed = FALSE;
    arrival_reapply_pending = FALSE;
    arrival_reapply_after = 0u;
    arrival_reapply_deadline = 0u;
    arrival_reapply_attempts = 0u;
    SudekiMpLogWrite(
        "zone_transition_trace_install=success "
        "enter_rva=0x00007970 switch_now_rva=0x00007990 "
        "load_rva=0x00007b80 switch_main_rva=0x00006380 "
        "door_activate_rva=0x000ce3a0 enter_temp_rva=0x000064b0 "
        "exit_temp_rva=0x00006710 set_player_position_rva=0x00104ed0 "
        "internal_position_setter_rva=0x00003050 set_render_camera_rva=0x00036fb0 "
        "temporary_camera_state_update_rva=0x000352d0\r\n"
    );
    return TRUE;
}

BOOL SudekiMpZoneTraversalSwitchWorld(const char *zone_name) {
    if (zone_name == NULL || zone_name[0] == '\0' ||
        original_set_zone_now == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!SudekiMpZoneTraversalArrivalContextReady(zone_name, NULL)) {
        SetLastError(ERROR_NOT_FOUND);
        SudekiMpLogFormat(
            "zone_traversal action=set_world status=rejected "
            "reason=no_cached_native_arrival_context zone=%s\r\n",
            zone_name);
        return FALSE;
    }
    if (!context_has_actor_anchor(find_arrival_context(
            FALSE, zone_name, zone_name, FALSE))) {
        SudekiMpLogFormat(
            "zone_traversal action=set_world discovery=automatic "
            "world=%s policy=native_transition_capture\r\n",
            zone_name);
    }
    SudekiMpLogFormat(
        "zone_traversal action=set_world zone=%s policy=authored_full_transition\r\n",
        zone_name);
    /* SwitchZoneNOW only marks a zone for switching. Authored transitions
     * use SetZoneNOW, which performs the complete teardown/load pipeline. */
    trace_set_zone_now(zone_name);
    current_temporary_name[0] = '\0';
    arrival_reapply_pending = TRUE;
    arrival_reapply_after = GetTickCount() + 750u;
    arrival_reapply_deadline = GetTickCount() + 15000u;
    arrival_reapply_attempts = 0u;
    return TRUE;
}

BOOL SudekiMpZoneTraversalEnterTemporary(const char *zone_name) {
    SudekiMpResourceName resource_name;

    if (zone_name == NULL || zone_name[0] == '\0' ||
        original_enter_temporary_zone == NULL || last_world == NULL ||
        current_world_name[0] == '\0') {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!SudekiMpZoneTraversalArrivalContextReady(
            current_world_name, zone_name)) {
        SetLastError(ERROR_NOT_FOUND);
        SudekiMpLogFormat(
            "zone_traversal action=enter_temporary status=rejected "
            "reason=no_cached_native_arrival_context world=%s zone=%s\r\n",
            current_world_name,
            zone_name);
        return FALSE;
    }
    if (!context_has_actor_anchor(find_arrival_context(
            TRUE, current_world_name, zone_name, FALSE))) {
        SudekiMpLogFormat(
            "zone_traversal action=enter_temporary discovery=automatic "
            "world=%s zone=%s policy=native_transition_capture\r\n",
            current_world_name,
            zone_name);
    }
    if (!SudekiMpCleanroomEngineResourceNameFromText(
            &resource_name, zone_name)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (current_temporary_name[0] != '\0' &&
        original_exit_temporary_zone != NULL) {
        original_exit_temporary_zone(last_world);
        current_temporary_name[0] = '\0';
        release_active_temporary_resource();
    }
    SudekiMpLogFormat(
        "zone_traversal action=enter_temporary world=%s zone=%s "
        "policy=active_world_only\r\n",
        current_world_name, zone_name);
    /*
     * Temporary-zone loads can outlive this call.  Keep the reference-backed
     * ResourceName alive until the corresponding exit instead of releasing
     * it immediately after the native call returns.
     */
    active_temporary_resource = resource_name;
    ZeroMemory(&resource_name, sizeof(resource_name));
    active_temporary_resource_valid = TRUE;
    original_enter_temporary_zone(
        last_world, zone_name, &active_temporary_resource);
    copy_zone_name(zone_name, current_temporary_name);
    arrival_reapply_pending = TRUE;
    arrival_reapply_after = GetTickCount() + 750u;
    arrival_reapply_deadline = GetTickCount() + 15000u;
    arrival_reapply_attempts = 0u;
    return TRUE;
}

BOOL SudekiMpZoneTraversalExitTemporary(void) {
    if (original_exit_temporary_zone == NULL || last_world == NULL ||
        current_temporary_name[0] == '\0') {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    original_exit_temporary_zone(last_world);
    current_temporary_name[0] = '\0';
    temporary_position_samples = 0u;
    temporary_camera_samples = 0u;
    release_active_temporary_resource();
    return TRUE;
}

const char *SudekiMpZoneTraversalCurrentWorld(void) {
    return current_world_name[0] == '\0' ? NULL : current_world_name;
}

const char *SudekiMpZoneTraversalCurrentTemporary(void) {
    return current_temporary_name[0] == '\0' ? NULL : current_temporary_name;
}

BOOL SudekiMpZoneTraversalWorldMatches(const char *zone_name) {
    return current_world_confirmed && zone_name != NULL &&
        current_world_name[0] != '\0' &&
        _stricmp(current_world_name, zone_name) == 0;
}

BOOL SudekiMpZoneTraversalKnownDestination(
    const char *world_name,
    const char *temporary_name
) {
    unsigned int index;

    if (world_name == NULL || world_name[0] == '\0') {
        return FALSE;
    }
    if (temporary_name == NULL || temporary_name[0] == '\0') {
        for (index = 0u; index < sizeof(known_traversal_worlds) /
                sizeof(known_traversal_worlds[0]); ++index) {
            if (_stricmp(world_name, known_traversal_worlds[index]) == 0) {
                return TRUE;
            }
        }
        return FALSE;
    }
    for (index = 0u; index < sizeof(known_traversal_interiors) /
            sizeof(known_traversal_interiors[0]); ++index) {
        if (_stricmp(world_name, known_traversal_interiors[index][0]) == 0 &&
            _stricmp(temporary_name, known_traversal_interiors[index][1]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static SudekiMpZoneArrivalContext *current_arrival_context(void) {
    if (current_temporary_name[0] != '\0') {
        return find_arrival_context(
            TRUE,
            current_world_name,
            current_temporary_name,
            FALSE);
    }
    if (!current_world_confirmed || current_world_name[0] == '\0') {
        return NULL;
    }
    return find_arrival_context(
        FALSE,
        current_world_name,
        current_world_name,
        FALSE);
}

BOOL SudekiMpZoneTraversalArrivalContextReady(
    const char *world_name,
    const char *temporary_name
) {
    SudekiMpZoneArrivalContext *context;

    if (world_name == NULL || world_name[0] == '\0') {
        return FALSE;
    }
    context = find_arrival_context(
        temporary_name != NULL && temporary_name[0] != '\0',
        world_name,
        temporary_name != NULL && temporary_name[0] != '\0' ?
            temporary_name : world_name,
        FALSE);
    if (context_has_actor_anchor(context)) {
        return TRUE;
    }
    /* The first request for an authored destination is intentionally allowed
     * to proceed.  The native door/world pipeline will populate the context
     * through trace_internal_position_setter; later requests use the cached
     * actor-specific anchors above. */
    return SudekiMpZoneTraversalKnownDestination(world_name, temporary_name);
}

BOOL SudekiMpZoneTraversalApplyArrivalContext(void) {
    SudekiMpZoneArrivalContext *context = current_arrival_context();
    unsigned int actor_index;
    unsigned int applied = 0u;

    if (context == NULL || original_internal_position_setter == NULL ||
        !context_has_actor_anchor(context)) {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    for (actor_index = 0u;
         actor_index < SUDEKIMP_CLEANROOM_ACTOR_COUNT;
         ++actor_index) {
        uint8_t *actor = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            (SudekiMpCleanroomActor)actor_index);
        void *position;

        if (!context->actor_position_valid[actor_index] ||
            !readable_zone_bytes((const char *)actor, 0x48u)) {
            continue;
        }
        position = *(void **)(actor + 0x44u);
        if (!readable_zone_bytes((const char *)position, 0x24u)) {
            continue;
        }
        original_internal_position_setter(
            position,
            context->actor_positions[actor_index]);
        ++applied;
    }
    if (applied == 0u) {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    SudekiMpLogFormat(
        "zone_traversal event=arrival_context_apply status=success "
        "temporary=%d world=%s destination=%s actors=%u camera_index=%u "
        "policy=native_cached_savepoint_anchor\r\n",
        context->temporary ? 1 : 0,
        context->world_name,
        context->destination_name,
        applied,
        (unsigned int)context->camera_index);
    return TRUE;
}

void SudekiMpZoneTraversalService(void) {
    DWORD now;

    if (!arrival_reapply_pending ||
        (LONG)(GetTickCount() - arrival_reapply_after) < 0) {
        return;
    }
    now = GetTickCount();
    if (SudekiMpZoneTraversalApplyArrivalContext()) {
        arrival_reapply_pending = FALSE;
        arrival_reapply_deadline = 0u;
        arrival_reapply_attempts = 0u;
        return;
    }
    /* Native actor creation/placement can lag the world load by several
     * seconds. Retry on the game thread while this initiated transition is
     * still settling; this makes first-use discovery automatic. */
    if (arrival_reapply_deadline != 0u &&
        (LONG)(now - arrival_reapply_deadline) < 0) {
        ++arrival_reapply_attempts;
        arrival_reapply_after = now + 250u;
        return;
    }
    arrival_reapply_pending = FALSE;
    SudekiMpLogFormat(
        "zone_traversal event=arrival_context_apply status=rejected "
        "reason=no_actor_anchor_or_destination_not_ready attempts=%u\r\n",
        arrival_reapply_attempts);
    arrival_reapply_deadline = 0u;
    arrival_reapply_attempts = 0u;
}

void SudekiMpUninstallZoneTransitionTrace(void) {
    SudekiMpRestoreInlineHook(&exit_temporary_zone_hook);
    SudekiMpRestoreInlineHook(&set_render_camera_hook);
    SudekiMpRestoreInlineHook(&temporary_camera_state_update_hook);
    SudekiMpRestoreInlineHook(&internal_position_setter_hook);
    SudekiMpRestoreInlineHook(&set_player_position_hook);
    SudekiMpRestoreInlineHook(&enter_temporary_zone_hook);
    SudekiMpRestoreInlineHook(&door_activate_hook);
    SudekiMpRestoreInlineHook(&switch_main_zone_hook);
    SudekiMpRestoreInlineHook(&load_zone_hook);
    SudekiMpRestoreInlineHook(&switch_zone_now_hook);
    SudekiMpRestoreInlineHook(&enter_zone_hook);
    SudekiMpRestoreInlineHook(&set_zone_now_hook);
    original_set_zone_now = NULL;
    original_enter_zone = NULL;
    original_switch_zone_now = NULL;
    original_load_zone = NULL;
    original_switch_main_zone = NULL;
    original_door_activate = NULL;
    original_enter_temporary_zone = NULL;
    original_exit_temporary_zone = NULL;
    original_set_player_position = NULL;
    original_internal_position_setter = NULL;
    original_set_render_camera = NULL;
    original_temporary_camera_state_update = NULL;
    trace_module = NULL;
    last_world = NULL;
    current_world_confirmed = FALSE;
    current_world_name[0] = '\0';
    current_temporary_name[0] = '\0';
    temporary_position_samples = 0u;
    temporary_camera_samples = 0u;
    ZeroMemory(arrival_contexts, sizeof(arrival_contexts));
    arrival_capture_context = NULL;
    arrival_capture_armed = FALSE;
    arrival_reapply_pending = FALSE;
    arrival_reapply_after = 0u;
    arrival_reapply_deadline = 0u;
    arrival_reapply_attempts = 0u;
    release_active_temporary_resource();
}
