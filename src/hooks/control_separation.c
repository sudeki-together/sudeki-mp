#include "hooks/control_separation.h"

#include "cleanroom/engine.h"
#include "engine/arbiter_combat_input.h"
#include "engine/camera_target_abi.h"
#include "engine/controller_action_router.h"
#include "engine/log.h"
#include "engine/player_combat_context.h"
#include "engine/player_statehood.h"
#include "engine/roaming_boundary.h"
#include "engine/skill_activation_abi.h"
#include "hooks/blacksmith_ui_adapter.h"
#include "hooks/call_hook.h"
#include "hooks/split_screen_render.h"
#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#else
#error "Control separation requires 32-bit GCC thiscall support"
#endif

typedef void (SUDEKIMP_THISCALL *ControllerUpdateFunction)(
    void *controller,
    void *update_data
);
typedef void (*AiControlFunction)(void *character_pointer);
typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
);
typedef void (SUDEKIMP_THISCALL *ArbiterSetSpeedFunction)(
    void *arbiter,
    float speed,
    float turn_rate
);
typedef int (SUDEKIMP_THISCALL *CameraManagerGetCameraModeFunction)(
    void *manager,
    const char *name
);
typedef uint8_t (SUDEKIMP_THISCALL *GroupPlayersInCombatFunction)(
    void *group
);
typedef void (__stdcall *MovementCameraTransformFunction)(
    void *controller,
    float *output_direction,
    const float *local_direction
);
typedef void (__attribute__((regparm(1), stdcall))
    *MovementControllerUpdateFunction)(void *movement_controller,
                                       void *update_data);
typedef void (SUDEKIMP_THISCALL *MovementControllerSetAbsoluteDeltaFunction)(
    void *movement_controller,
    float delta_x,
    float delta_y,
    float delta_z
);
typedef void (SUDEKIMP_THISCALL *TalCharacterUpdateFunction)(
    void *character_update,
    void *update_data
);
typedef void (__stdcall *MatrixTargetCreateFunction)(
    void *target_list,
    void **output_target,
    const float *matrix
);
typedef const float *(SUDEKIMP_THISCALL *CameraTargetMatrixFunction)(
    void *target
);
enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_CAMERA_MANAGER_GLOBAL = 0x00409d7cu,
    RVA_ENTITY_MANAGER_GLOBAL = 0x00409d8cu,
    RVA_ENTITY_DIRECTORY_GLOBAL = 0x00409de4u,
    RVA_CAMERA_MANAGER_GET_CAMERA_MODE = 0x000374b0u,
    RVA_CAMERA_TARGET_LIST_OWNER_GLOBAL = 0x003c2f30u,
    RVA_AI_OVERRIDE_CONTROL = 0x000f60d0u,
    RVA_AI_DEFAULT_CONTROL = 0x000f6100u,
    RVA_MOVEMENT_CAMERA_TRANSFORM = 0x000291a0u,
    RVA_MOVEMENT_CONTROLLER_SET_ABSOLUTE_DELTA = 0x000030a0u,
    RVA_MOVEMENT_CONTROLLER_UPDATE = 0x000c3200u,
    RVA_TAL_CHARACTER_UPDATE = 0x00153240u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_ARBITER_SET_SPEED = 0x000db070u,
    RVA_GROUP_PLAYERS_IN_COMBAT = 0x00004fa0u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_ARBITER_COMBAT_INPUT = 0x000db0e0u,
    RVA_CAMERA_TARGET_INSTALL = 0x000e84c0u,
    RVA_MATRIX_TARGET_CREATE = 0x00134fb0u,
    RVA_CAMERA_TARGET_RELEASE = 0x00135340u,
    RVA_GAME_OBJECT_TARGET_VTABLE = 0x002d42ccu,
    RVA_OFFSET_TARGET_VTABLE = 0x002d436cu,
    RVA_MATRIX_TARGET_VTABLE = 0x002d43bcu,
    SUPPORTED_IMAGE_SIZE = 0x0045f000u,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_FIRST_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    CONTROLLER_MODE_80_OFFSET = 0x80u,
    CONTROLLER_MODE_84_OFFSET = 0x84u,
    CONTROLLER_NEXT_CHARACTER_OFFSET = 0xf4u,
    CONTROLLER_PREVIOUS_CHARACTER_OFFSET = 0xfcu,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    PARTY_STATE_D0_OFFSET = 0xd0u,
    PARTY_SWITCHING_D6_OFFSET = 0xd6u,
    PARTY_STATE_D7_OFFSET = 0xd7u,
    CAMERA_MODE_EXPLORATION = 0,
    ROAMING_BOUNDARY_SETTLE_MS = 250u
};

/*
 * Absolute-delta mode treats +0x1D4 as one world unit per second.  Sudeki's
 * normal melee locomotion is root-motion driven and the cleanroom Tal trace
 * measured roughly 6.4 world units per second at full input.  Match that
 * native gameplay pace only for the
 * Spirit-locked non-caster fallback; ordinary movement remains untouched.
 */
static const float spirit_noncaster_direct_movement_pace = 6.4f;

static SudekiMpPointerHook controller_update_vtable_hook;
static SudekiMpRelativeCallHook player_one_alternate_movement_call_hook;
static SudekiMpRelativeCallHook player_one_normal_movement_call_hook;
static SudekiMpInlineHook movement_controller_update_hook;
static SudekiMpInlineHook tal_character_update_hook;
static ControllerUpdateFunction original_controller_update;
static AiControlFunction ai_override_control;
static AiControlFunction ai_default_control;
static ArbiterMovementFunction arbiter_movement;
static ArbiterSetSpeedFunction arbiter_set_speed;
static CameraManagerGetCameraModeFunction camera_manager_get_camera_mode;
static GroupPlayersInCombatFunction group_players_in_combat;
static MovementCameraTransformFunction movement_camera_transform;
static MovementControllerSetAbsoluteDeltaFunction
    movement_controller_set_absolute_delta;
static uint8_t *game_base;
static void *overridden_character;
static UINT selected_virtual_key;
static BOOL hotkey_was_down;
static BOOL second_player_movement_enabled;
static BOOL camera_relative_movement_enabled;
static BOOL separation_guard_enabled;
static float maximum_separation_distance;
static SudekiMpRoamingBoundaryEvaluation roaming_boundary_snapshot;
static DWORD roaming_boundary_candidate_since;
static unsigned int roaming_boundary_last_gate;
static BOOL roaming_boundary_player_blocked[2];
static BOOL roaming_boundary_overlay_ready;
static BOOL second_player_weak_attack_enabled;
static UINT weak_attack_virtual_key;
static BOOL weak_attack_was_down;
static BOOL second_player_skills_enabled;
static UINT second_player_skill_virtual_keys[4];
static BOOL second_player_skill_keys_were_down[4];
static BOOL target_trace_enabled;
static DWORD target_trace_last_sample_tick;
static void *target_trace_last_node;
static int target_trace_last_auto_enabled;
static BOOL second_player_movement_active;
static float second_player_movement_magnitude;
static int last_movement_x;
static int last_movement_z;
static int last_native_movement_acceptance = -1;
static unsigned int last_native_movement_gate = 0xffffffffu;
static DWORD last_movement_pipeline_sample_tick;
static BOOL last_movement_pipeline_position_valid;
static float last_movement_pipeline_position[3];
static BOOL controller_update_spirit_virtualization_logged;
static BOOL spirit_direct_movement_active;
static DWORD spirit_direct_movement_last_trace_tick;
static DWORD movement_controller_update_last_trace_tick;
static DWORD tal_character_update_last_trace_tick;
static BOOL second_player_facing_valid;
static float second_player_last_facing[3];
static BOOL input_bridge_enabled;
static float input_bridge_deadzone;
static SudekiMpInputBridgeState input_bridge_state;
static BOOL input_bridge_connected;
static BOOL interaction_requests_enabled;
static SudekiMpControllerActionRouter controller_action_router;
static BOOL shared_interaction_modal_quiesce_logged;
static void *published_player_actors[2];
static uint32_t published_player_actor_generations[2];
static BOOL published_player_human_present[2];
static BOOL roster_join_start_was_down;
static DWORD roster_leave_chord_since;
static BOOL roster_leave_chord_consumed;
static DWORD input_bridge_last_right_stick_log_tick;
static int16_t input_bridge_last_right_x;
static int16_t input_bridge_last_right_y;
static BOOL transition_vote_input_freeze_logged;
static BOOL transition_vote_escape_release_pending;
static BOOL shared_group_camera_enabled;
static MatrixTargetCreateFunction matrix_target_create;
static void *camera_target_install;
static void *camera_target_release;
static void *group_camera_camera;
static void *group_camera_target;
static void *group_camera_original_targets[2];
static void *group_camera_target_list;
static unsigned int group_camera_last_rejection;
static BOOL player_two_requested;
static void *requested_player_two_character;
static BOOL role_lock_active;
static DWORD player_two_request_last_attempt;
static SudekiMpControlUpdateObserver update_observer;

static const uint8_t expected_arbiter_combat_input_entry[] = {
    0x55, 0x8b, 0x6c, 0x24, 0x08, 0x56, 0x57, 0x8b, 0xf8, 0x8b, 0xf1
};
static const uint8_t expected_movement_camera_transform_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x8b, 0x55, 0x08, 0xd9, 0xee
};
static const uint8_t expected_movement_controller_set_absolute_delta_entry[] = {
    0x83, 0xec, 0x0c, 0xf6, 0x81, 0xbe, 0x00, 0x00,
    0x00, 0x08, 0x74, 0x20
};
static const uint8_t expected_movement_controller_update_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
};
static const uint8_t expected_tal_character_update_entry[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56
};
static const uint8_t expected_camera_manager_get_camera_mode_entry[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57,
    0x8b, 0xe9, 0x85, 0xdb
};
static const uint8_t expected_group_players_in_combat_entry[] = {
    0x8a, 0x81, 0xd4, 0x00, 0x00, 0x00, 0xc3
};
static const uint8_t expected_camera_target_install_entry[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x8b, 0x94, 0x9e,
    0xb4, 0x00, 0x00, 0x00
};
static const uint8_t expected_matrix_target_create_entry[] = {
    0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x68, 0x80,
    0x00, 0x00, 0x00
};
static const uint8_t expected_camera_target_release_entry[] = {
    0x53, 0x56, 0x8b, 0x77, 0x04, 0x33, 0xdb, 0x32,
    0xc0
};
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

static BOOL game_code_pointer(const void *pointer) {
    return game_base != NULL && pointer != NULL &&
        (const uint8_t *)pointer >= game_base &&
        (const uint8_t *)pointer < game_base + SUPPORTED_IMAGE_SIZE;
}

static BOOL second_player_character_is_ranged(void *character_pointer) {
    unsigned int player_one_type;
    unsigned int player_two_type;

    if (character_pointer == NULL ||
        !SudekiMpSplitScreenGetRosterTypes(
            &player_one_type, &player_two_type) ||
        (player_two_type != 0x01u && player_two_type != 0x0eu)) {
        return FALSE;
    }
    return SudekiMpSplitScreenRosterActorIdentityMatches(
        1u, character_pointer, player_two_type);
}

static void log_group_camera_rejection(
    unsigned int rejection,
    const char *reason,
    void *camera,
    void *controller_target,
    void *second_character,
    void *slot_zero,
    void *slot_one,
    void *target_vtable
) {
    void *slot_one_vtable;

    if (group_camera_last_rejection == rejection) {
        return;
    }
    slot_one_vtable = readable_memory(slot_one, sizeof(void *)) ?
        *(void **)slot_one : NULL;
    group_camera_last_rejection = rejection;
    SudekiMpLogFormat(
        "control_separation event=shared_group_camera phase=reject reason=%s camera=0x%08lx controller_target=0x%08lx second_character=0x%08lx slot_zero=0x%08lx slot_one=0x%08lx slot_zero_vtable=0x%08lx slot_one_vtable=0x%08lx\r\n",
        reason,
        (unsigned long)(uintptr_t)camera,
        (unsigned long)(uintptr_t)controller_target,
        (unsigned long)(uintptr_t)second_character,
        (unsigned long)(uintptr_t)slot_zero,
        (unsigned long)(uintptr_t)slot_one,
        (unsigned long)(uintptr_t)target_vtable,
        (unsigned long)(uintptr_t)slot_one_vtable
    );
}

static void retain_camera_target(void *target) {
    if (readable_memory(target, 8u)) {
        ++*(uint32_t *)((uint8_t *)target + 4u);
    }
}

static void release_camera_target(void *target) {
    uint32_t *references;

    if (target == NULL || group_camera_target_list == NULL ||
        camera_target_release == NULL || !readable_memory(target, 8u)) {
        return;
    }
    references = (uint32_t *)((uint8_t *)target + 4u);
    if (*references == 0u) {
        return;
    }
    --*references;
    if (*references == 0u) {
        SudekiMpCallCameraTargetRelease(
            group_camera_target_list,
            target,
            camera_target_release
        );
    }
}

static void reset_target_trace_state(void) {
    target_trace_last_sample_tick = 0;
    target_trace_last_node = NULL;
    target_trace_last_auto_enabled = -1;
}

static BOOL character_is_in_active_group(void *wanted_character) {
    uint8_t *group;
    unsigned int index;

    if (game_base == NULL || wanted_character == NULL) {
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group == NULL) {
        return FALSE;
    }
    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)slot == wanted_character) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL overridden_character_is_in_active_group(void) {
    return character_is_in_active_group(overridden_character);
}

static uint32_t advance_actor_generation(uint32_t generation) {
    ++generation;
    if (generation == 0u) {
        ++generation;
    }
    return generation;
}

static void publish_runtime_player_lease(
    unsigned int player_index,
    void *actor,
    BOOL human_present
) {
    SudekiMpPlayerStatehood *statehood = SudekiMpPlayerStatehoodRuntime();

    if (player_index >= 2u) {
        return;
    }
    human_present = human_present != FALSE;
    if (!human_present) {
        actor = NULL;
    }
    if (published_player_actors[player_index] != actor ||
        published_player_human_present[player_index] != human_present) {
        published_player_actor_generations[player_index] =
            advance_actor_generation(
                published_player_actor_generations[player_index]);
        published_player_actors[player_index] = actor;
        published_player_human_present[player_index] = human_present;
        SudekiMpLogFormat(
            "control_separation event=player_actor_lease player=%u "
            "actor=0x%08lx actor_generation=%lu human_present=%s "
            "policy=runtime_only_never_save\r\n",
            player_index + 1u,
            (unsigned long)(uintptr_t)actor,
            (unsigned long)published_player_actor_generations[player_index],
            human_present ? "true" : "false");
    }
    SudekiMpPlayerStatehoodPublishPlayer(
        statehood,
        player_index,
        (uintptr_t)actor,
        published_player_actor_generations[player_index],
        human_present);
}

static void publish_runtime_player_leases(void *controller) {
    void *player_one = NULL;
    void *player_two = NULL;
    BOOL player_one_present = FALSE;
    BOOL player_two_present = FALSE;

    if (readable_memory(controller, CONTROLLER_TARGET_OFFSET +
            sizeof(player_one))) {
        player_one = *(void **)((uint8_t *)controller +
            CONTROLLER_TARGET_OFFSET);
        player_one_present = player_one != NULL &&
            character_is_in_active_group(player_one);
    }
    if (player_two_requested && input_bridge_enabled &&
        input_bridge_connected && SudekiMpSplitScreenRuntimeEnabled() &&
        (!SudekiMpSplitScreenRosterParticipationAvailable() ||
         SudekiMpSplitScreenRosterParticipationRequested()) &&
        overridden_character != NULL &&
        overridden_character != player_one &&
        overridden_character_is_in_active_group()) {
        player_two = overridden_character;
        player_two_present = TRUE;
    }
    publish_runtime_player_lease(0u, player_one, player_one_present);
    publish_runtime_player_lease(1u, player_two, player_two_present);
}

static uint8_t *current_gameplay_camera(void) {
    uint8_t *mode;
    uint8_t *camera_member;
    uint8_t *camera;

    if (game_base == NULL) {
        return NULL;
    }
    mode = *(uint8_t **)(game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    if (!readable_memory(mode, 0x10u)) {
        return NULL;
    }
    camera_member = *(uint8_t **)(mode + 0x0cu);
    if ((uintptr_t)camera_member < 0x2cu) {
        return NULL;
    }
    camera = camera_member - 0x2cu;
    if (!readable_memory(camera, 0xbcu)) {
        return NULL;
    }
    return camera;
}

static BOOL character_position(void *character, float output[3]) {
    uint8_t *transform;

    if (!readable_memory(character, 0x48u)) {
        return FALSE;
    }
    transform = *(uint8_t **)((uint8_t *)character + 0x44u);
    if (!readable_memory(transform, 0x24u)) {
        return FALSE;
    }
    output[0] = *(float *)(transform + 0x18u);
    output[1] = *(float *)(transform + 0x1cu);
    output[2] = *(float *)(transform + 0x20u);
    return isfinite(output[0]) && isfinite(output[1]) &&
        isfinite(output[2]);
}

static BOOL camera_target_matrix(void *target, float output[16]) {
    void **vtable;
    CameraTargetMatrixFunction get_matrix;
    const float *matrix;

    if (!readable_memory(target, 0x24u)) {
        return FALSE;
    }
    vtable = *(void ***)target;
    if (!readable_memory(vtable, 0x24u)) {
        return FALSE;
    }
    get_matrix = (CameraTargetMatrixFunction)vtable[8];
    if (!game_code_pointer((const void *)get_matrix)) {
        return FALSE;
    }
    matrix = get_matrix(target);
    if (!readable_memory(matrix, sizeof(float) * 16u)) {
        return FALSE;
    }
    memcpy(output, matrix, sizeof(float) * 16u);
    return TRUE;
}

static BOOL update_group_camera_target(
    void *controller_target,
    void *second_character
) {
    float first_position[3];
    float second_position[3];
    float matrix[16];
    uint8_t *target = (uint8_t *)group_camera_target;

    if (!readable_memory(target, 0x80u) ||
        *(void **)target != game_base + RVA_MATRIX_TARGET_VTABLE ||
        !camera_target_matrix(group_camera_original_targets[0], matrix) ||
        !character_position(controller_target, first_position) ||
        !character_position(second_character, second_position)) {
        return FALSE;
    }
    matrix[12] +=
        (first_position[0] + second_position[0]) * 0.5f -
        first_position[0];
    matrix[13] +=
        (first_position[1] + second_position[1]) * 0.5f -
        first_position[1];
    matrix[14] +=
        (first_position[2] + second_position[2]) * 0.5f -
        first_position[2];
    matrix[15] = 1.0f;
    memcpy(target + 0x20u, matrix, sizeof(matrix));
    memcpy(target + 0x14u, matrix + 12, sizeof(float) * 3u);
    return TRUE;
}

static void restore_group_camera(const char *reason) {
    uint8_t *camera = (uint8_t *)group_camera_camera;
    void *target = group_camera_target;
    unsigned int slot;

    if (target == NULL) {
        return;
    }
    if (readable_memory(camera, 0xbcu) && camera_target_install != NULL) {
        for (slot = 0; slot < 2u; ++slot) {
            void **camera_slot = (void **)(
                camera + 0xb4u + slot * sizeof(void *)
            );
            if (*camera_slot == target) {
                retain_camera_target(group_camera_original_targets[slot]);
                SudekiMpCallCameraTargetInstall(
                    camera,
                    group_camera_original_targets[slot],
                    slot,
                    camera_target_install
                );
            }
        }
    }
    release_camera_target(group_camera_original_targets[0]);
    release_camera_target(group_camera_original_targets[1]);
    release_camera_target(target);
    SudekiMpLogFormat(
        "control_separation event=shared_group_camera phase=restore reason=%s\r\n",
        reason == NULL ? "unspecified" : reason
    );
    group_camera_camera = NULL;
    group_camera_target = NULL;
    group_camera_original_targets[0] = NULL;
    group_camera_original_targets[1] = NULL;
    group_camera_target_list = NULL;
}

static BOOL acquire_group_camera(
    uint8_t *camera,
    void *controller_target,
    void *second_character
) {
    uint8_t *target_list_owner;
    void *created_target = NULL;
    float matrix[16];
    float first_position[3];
    float second_position[3];
    void *slot_zero = *(void **)(camera + 0xb4u);
    void *slot_one = *(void **)(camera + 0xb8u);
    void *target_vtable = readable_memory(slot_zero, sizeof(void *)) ?
        *(void **)slot_zero : NULL;
    void *slot_one_vtable = readable_memory(slot_one, sizeof(void *)) ?
        *(void **)slot_one : NULL;
    BOOL same_game_object_target =
        slot_zero == slot_one &&
        target_vtable == game_base + RVA_GAME_OBJECT_TARGET_VTABLE;
    BOOL offset_and_game_object_targets =
        slot_zero != slot_one &&
        target_vtable == game_base + RVA_OFFSET_TARGET_VTABLE &&
        slot_one_vtable == game_base + RVA_GAME_OBJECT_TARGET_VTABLE;

    if (!same_game_object_target && !offset_and_game_object_targets) {
        log_group_camera_rejection(
            5u,
            "unsupported_target_pair",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    if (!readable_memory(slot_zero, 0x34u) ||
        !readable_memory(slot_one, 0x34u)) {
        log_group_camera_rejection(
            6u,
            "target_unreadable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    if (!camera_target_matrix(slot_zero, matrix)) {
        log_group_camera_rejection(
            8u,
            "target_matrix_unavailable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    if (!character_position(controller_target, first_position) ||
        !character_position(second_character, second_position)) {
        log_group_camera_rejection(
            9u,
            "character_position_unavailable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    target_list_owner = *(uint8_t **)(
        game_base + RVA_CAMERA_TARGET_LIST_OWNER_GLOBAL
    );
    if (!readable_memory(target_list_owner, 0x58u)) {
        log_group_camera_rejection(
            10u,
            "target_list_owner_unavailable",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        return FALSE;
    }
    matrix[12] +=
        (first_position[0] + second_position[0]) * 0.5f -
        first_position[0];
    matrix[13] +=
        (first_position[1] + second_position[1]) * 0.5f -
        first_position[1];
    matrix[14] +=
        (first_position[2] + second_position[2]) * 0.5f -
        first_position[2];
    matrix[15] = 1.0f;
    group_camera_target_list = target_list_owner + 0x4cu;
    matrix_target_create(
        group_camera_target_list,
        &created_target,
        matrix
    );
    if (!readable_memory(created_target, 0x80u) ||
        *(void **)created_target != game_base + RVA_MATRIX_TARGET_VTABLE) {
        log_group_camera_rejection(
            11u,
            "matrix_target_creation_failed",
            camera,
            controller_target,
            second_character,
            slot_zero,
            slot_one,
            target_vtable
        );
        release_camera_target(created_target);
        group_camera_target_list = NULL;
        return FALSE;
    }

    group_camera_camera = camera;
    group_camera_target = created_target;
    group_camera_original_targets[0] = slot_zero;
    group_camera_original_targets[1] = slot_one;
    retain_camera_target(slot_zero);
    retain_camera_target(slot_one);
    retain_camera_target(created_target);
    SudekiMpCallCameraTargetInstall(
        camera,
        created_target,
        0u,
        camera_target_install
    );
    retain_camera_target(created_target);
    SudekiMpCallCameraTargetInstall(
        camera,
        created_target,
        1u,
        camera_target_install
    );
    if (*(void **)(camera + 0xb4u) != created_target ||
        *(void **)(camera + 0xb8u) != created_target) {
        restore_group_camera("install_verification_failed");
        return FALSE;
    }
    group_camera_last_rejection = 0u;
    SudekiMpLogFormat(
        "control_separation event=shared_group_camera phase=acquire camera=0x%08lx original_targets=0x%08lx,0x%08lx matrix_target=0x%08lx midpoint_bits=%08lx,%08lx,%08lx policy=two_player_centroid_preserve_native_offset_no_zoom\r\n",
        (unsigned long)(uintptr_t)camera,
        (unsigned long)(uintptr_t)slot_zero,
        (unsigned long)(uintptr_t)slot_one,
        (unsigned long)(uintptr_t)created_target,
        (unsigned long)float_bits(matrix[12]),
        (unsigned long)float_bits(matrix[13]),
        (unsigned long)float_bits(matrix[14])
    );
    return TRUE;
}

static void poll_shared_group_camera(void *controller) {
    uint8_t *camera;
    void *controller_target;

    if (!shared_group_camera_enabled) {
        return;
    }
    if (!player_two_requested) {
        restore_group_camera("player_two_not_requested");
        return;
    }
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248u);
    camera = current_gameplay_camera();
    if (overridden_character == NULL) {
        group_camera_last_rejection = 0u;
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (!overridden_character_is_in_active_group()) {
        log_group_camera_rejection(
            1u, "second_character_not_in_active_group", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (!character_is_in_active_group(controller_target)) {
        log_group_camera_rejection(
            2u, "controller_target_not_in_active_group", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (controller_target == overridden_character) {
        log_group_camera_rejection(
            3u, "controller_target_is_second_character", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (camera == NULL) {
        log_group_camera_rejection(
            4u, "gameplay_camera_unavailable", camera,
            controller_target, overridden_character, NULL, NULL, NULL
        );
        restore_group_camera("inactive_or_incomplete_state");
        return;
    }
    if (group_camera_target != NULL) {
        if (camera != group_camera_camera) {
            restore_group_camera("camera_changed");
            return;
        }
        if (*(void **)(camera + 0xb4u) != group_camera_target ||
            *(void **)(camera + 0xb8u) != group_camera_target) {
            restore_group_camera("native_target_changed");
            return;
        }
        if (!update_group_camera_target(
                controller_target,
                overridden_character)) {
            restore_group_camera("position_or_target_invalid");
        }
        return;
    }
    acquire_group_camera(
        camera,
        controller_target,
        overridden_character
    );
}

static void stop_second_player_movement(void) {
    uint8_t *character = (uint8_t *)overridden_character;
    void *arbiter;

    if (!second_player_movement_active) {
        return;
    }
    if (character == NULL) {
        second_player_movement_active = FALSE;
        second_player_movement_magnitude = 0.0f;
        last_movement_x = 0;
        last_movement_z = 0;
        return;
    }
    if (!overridden_character_is_in_active_group()) {
        SudekiMpLogWrite(
            "control_separation event=second_player_movement phase=abort reason=character_not_in_active_group\r\n"
        );
        second_player_movement_active = FALSE;
        second_player_movement_magnitude = 0.0f;
        last_movement_x = 0;
        last_movement_z = 0;
        return;
    }
    arbiter = *(void **)(character + 0x90);
    if (arbiter != NULL) {
        arbiter_set_speed(arbiter, 0.0f, 1.0f);
    }
    SudekiMpLogFormat(
        "control_separation event=second_player_movement phase=stop character=0x%08lx\r\n",
        (unsigned long)(uintptr_t)character
    );
    second_player_movement_active = FALSE;
    second_player_movement_magnitude = 0.0f;
    last_movement_x = 0;
    last_movement_z = 0;
}

static void quiesce_second_player_input(void) {
    unsigned int ordinal;

    stop_second_player_movement();
    second_player_facing_valid = FALSE;
    spirit_direct_movement_active = FALSE;
    weak_attack_was_down = !input_bridge_enabled ?
        ((GetAsyncKeyState((int)weak_attack_virtual_key) & 0x8000) != 0)
        : FALSE;
    for (ordinal = 0u; ordinal < 4u; ++ordinal) {
        second_player_skill_keys_were_down[ordinal] =
            (GetAsyncKeyState(
                (int)second_player_skill_virtual_keys[ordinal]) &
             0x8000) != 0;
    }
    restore_group_camera("player_two_not_requested");
    reset_target_trace_state();
}

enum {
    ROAMING_BOUNDARY_GATE_READY = 0u,
    ROAMING_BOUNDARY_GATE_DISABLED = 1u,
    ROAMING_BOUNDARY_GATE_WORLD_UNAVAILABLE = 2u,
    ROAMING_BOUNDARY_GATE_SPLIT_INACTIVE = 3u,
    ROAMING_BOUNDARY_GATE_TRANSITION = 4u,
    ROAMING_BOUNDARY_GATE_HUMANS_INACTIVE = 5u,
    ROAMING_BOUNDARY_GATE_NATIVE_STATE = 6u,
    ROAMING_BOUNDARY_GATE_COMBAT = 7u,
    ROAMING_BOUNDARY_GATE_CAMERA_MODE = 8u,
    ROAMING_BOUNDARY_GATE_POSITION = 9u,
    ROAMING_BOUNDARY_GATE_SHARED_INTERACTION_MODAL = 10u
};

static const char *roaming_boundary_gate_name(unsigned int gate) {
    switch (gate) {
    case ROAMING_BOUNDARY_GATE_READY: return "stable_exploration";
    case ROAMING_BOUNDARY_GATE_DISABLED: return "config_disabled";
    case ROAMING_BOUNDARY_GATE_WORLD_UNAVAILABLE: return "loading_or_world_unavailable";
    case ROAMING_BOUNDARY_GATE_SPLIT_INACTIVE: return "split_runtime_inactive";
    case ROAMING_BOUNDARY_GATE_TRANSITION: return "transition_or_vote";
    case ROAMING_BOUNDARY_GATE_HUMANS_INACTIVE: return "two_active_humans_required";
    case ROAMING_BOUNDARY_GATE_NATIVE_STATE: return "native_party_not_settled";
    case ROAMING_BOUNDARY_GATE_COMBAT: return "combat";
    case ROAMING_BOUNDARY_GATE_CAMERA_MODE: return "non_exploration_camera";
    case ROAMING_BOUNDARY_GATE_POSITION: return "position_unavailable";
    case ROAMING_BOUNDARY_GATE_SHARED_INTERACTION_MODAL: return "shared_interaction_modal";
    default: return "unknown";
    }
}

static BOOL roaming_boundary_world_ready(void) {
    void *entity_manager;
    void *entity_directory;

    if (game_base == NULL ||
        !readable_memory(
            game_base + RVA_ENTITY_MANAGER_GLOBAL,
            sizeof(entity_manager)) ||
        !readable_memory(
            game_base + RVA_ENTITY_DIRECTORY_GLOBAL,
            sizeof(entity_directory))) {
        return FALSE;
    }
    entity_manager = *(void **)(game_base + RVA_ENTITY_MANAGER_GLOBAL);
    entity_directory = *(void **)(game_base + RVA_ENTITY_DIRECTORY_GLOBAL);
    return readable_memory(entity_manager, 0x38u) &&
        readable_memory(entity_directory, 0xf0u);
}

static unsigned int classify_roaming_boundary_context(
    void *controller_pointer,
    float player_one_position[3],
    float player_two_position[3]
) {
    uint8_t *group;
    uint8_t *controller;
    uint8_t *player_one;
    uint8_t *player_two;
    uint8_t *component;
    uint8_t *mode_state;
    void *camera_manager;

    if (!separation_guard_enabled) {
        return ROAMING_BOUNDARY_GATE_DISABLED;
    }
    if (!roaming_boundary_world_ready()) {
        return ROAMING_BOUNDARY_GATE_WORLD_UNAVAILABLE;
    }
    if (!SudekiMpSplitScreenRuntimeEnabled()) {
        return ROAMING_BOUNDARY_GATE_SPLIT_INACTIVE;
    }
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        return ROAMING_BOUNDARY_GATE_SHARED_INTERACTION_MODAL;
    }
    if (SudekiMpControlSeparationGameplayInputFrozen()) {
        return ROAMING_BOUNDARY_GATE_TRANSITION;
    }
    if (!player_two_requested || overridden_character == NULL ||
        !second_player_movement_enabled ||
        (input_bridge_enabled && !input_bridge_connected)) {
        return ROAMING_BOUNDARY_GATE_HUMANS_INACTIVE;
    }
    if (!readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group)) ||
        !readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return ROAMING_BOUNDARY_GATE_NATIVE_STATE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    if (controller_pointer != controller ||
        !readable_memory(group, PARTY_STATE_D7_OFFSET + 1u) ||
        !readable_memory(controller, CONTROLLER_TARGET_OFFSET +
            sizeof(player_one))) {
        return ROAMING_BOUNDARY_GATE_NATIVE_STATE;
    }
    player_one = *(uint8_t **)(controller + CONTROLLER_TARGET_OFFSET);
    player_two = (uint8_t *)overridden_character;
    if (!SudekiMpSplitScreenRosterLeadReady(
            player_one,
            *(void **)(group + PARTY_SLOT_FIRST_OFFSET),
            player_one,
            *(uint32_t *)(controller + CONTROLLER_MODE_80_OFFSET),
            *(uint32_t *)(controller + CONTROLLER_MODE_84_OFFSET),
            *(uint32_t *)(group + PARTY_STATE_D0_OFFSET),
            *(uint8_t *)(group + PARTY_SWITCHING_D6_OFFSET),
            *(uint8_t *)(group + PARTY_STATE_D7_OFFSET),
            *(uint32_t *)(controller + CONTROLLER_NEXT_CHARACTER_OFFSET),
            *(uint32_t *)(controller + CONTROLLER_PREVIOUS_CHARACTER_OFFSET)) ||
        player_two == player_one ||
        !overridden_character_is_in_active_group() ||
        !readable_memory(player_two, 0x98u)) {
        return ROAMING_BOUNDARY_GATE_NATIVE_STATE;
    }
    component = *(uint8_t **)(player_two + 0x94u);
    mode_state = readable_memory(component, 0x170u) ?
        *(uint8_t **)(component + 0x3cu) : NULL;
    if (!readable_memory(mode_state, 0x0cu) ||
        *(int16_t *)(component + 0x16au) != 1 ||
        *(uint8_t *)(mode_state + 0x0bu) != 0u) {
        return ROAMING_BOUNDARY_GATE_HUMANS_INACTIVE;
    }
    if (group_players_in_combat == NULL) {
        return ROAMING_BOUNDARY_GATE_NATIVE_STATE;
    }
    if (group_players_in_combat(group) != 0u) {
        return ROAMING_BOUNDARY_GATE_COMBAT;
    }
    if (camera_manager_get_camera_mode == NULL ||
        !readable_memory(
            game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(camera_manager))) {
        return ROAMING_BOUNDARY_GATE_CAMERA_MODE;
    }
    camera_manager = *(void **)(game_base + RVA_CAMERA_MANAGER_GLOBAL);
    if (!readable_memory(camera_manager, sizeof(void *)) ||
        camera_manager_get_camera_mode(camera_manager, NULL) !=
            CAMERA_MODE_EXPLORATION) {
        return ROAMING_BOUNDARY_GATE_CAMERA_MODE;
    }
    if (!character_position(player_one, player_one_position) ||
        !character_position(player_two, player_two_position)) {
        return ROAMING_BOUNDARY_GATE_POSITION;
    }
    return ROAMING_BOUNDARY_GATE_READY;
}

static void update_roaming_boundary(void *controller) {
    SudekiMpRoamingBoundaryEvaluation next;
    float player_one_position[3];
    float player_two_position[3];
    unsigned int previous_phase = roaming_boundary_snapshot.phase;
    unsigned int gate = classify_roaming_boundary_context(
        controller, player_one_position, player_two_position);
    DWORD now = GetTickCount();

    ZeroMemory(&next, sizeof(next));
    if (gate == ROAMING_BOUNDARY_GATE_READY) {
        if (roaming_boundary_last_gate != ROAMING_BOUNDARY_GATE_READY) {
            roaming_boundary_candidate_since = now;
        }
        if ((DWORD)(now - roaming_boundary_candidate_since) >=
            ROAMING_BOUNDARY_SETTLE_MS) {
            SudekiMpRoamingBoundaryEvaluate(
                TRUE,
                player_one_position[0], player_one_position[2],
                player_two_position[0], player_two_position[2],
                maximum_separation_distance,
                0.8f,
                &next);
        }
    } else {
        roaming_boundary_candidate_since = 0u;
    }
    roaming_boundary_snapshot = next;
    if (gate != roaming_boundary_last_gate) {
        SudekiMpLogFormat(
            "control_separation event=roaming_boundary gate=%s active=%s policy=stable_exploration_two_active_humans_only\r\n",
            roaming_boundary_gate_name(gate),
            next.phase != SUDEKIMP_ROAMING_BOUNDARY_INACTIVE ?
                "true" : "false");
        roaming_boundary_last_gate = gate;
    }
    if (next.phase != previous_phase) {
        SudekiMpLogFormat(
            "control_separation event=roaming_boundary phase=%u distance_bits=0x%08lx warning_bits=0x%08lx maximum_bits=0x%08lx policy=warn_at_80_percent_require_clear_inward_at_limit\r\n",
            next.phase,
            (unsigned long)float_bits(next.distance),
            (unsigned long)float_bits(next.warning_distance),
            (unsigned long)float_bits(next.maximum_distance));
    }
    if (next.phase != SUDEKIMP_ROAMING_BOUNDARY_LIMIT) {
        roaming_boundary_player_blocked[0] = FALSE;
        roaming_boundary_player_blocked[1] = FALSE;
    }
    if (next.phase < SUDEKIMP_ROAMING_BOUNDARY_WARNING) {
        roaming_boundary_overlay_ready = FALSE;
    }
}

static BOOL constrain_roaming_boundary_movement(
    unsigned int player_index,
    float direction[3]
) {
    float constrained_x;
    float constrained_z;
    BOOL movement_remains;
    BOOL changed;

    if (!separation_guard_enabled || direction == NULL ||
        player_index > 1u) {
        return TRUE;
    }
    movement_remains = SudekiMpRoamingBoundaryConstrainMovement(
        &roaming_boundary_snapshot,
        roaming_boundary_overlay_ready,
        player_index,
        direction[0],
        direction[2],
        &constrained_x,
        &constrained_z) != 0;
    changed = fabsf(constrained_x - direction[0]) > 0.00001f ||
        fabsf(constrained_z - direction[2]) > 0.00001f;
    direction[0] = constrained_x;
    direction[2] = constrained_z;
    if (changed && !roaming_boundary_player_blocked[player_index]) {
        SudekiMpLogFormat(
            "control_separation event=roaming_boundary player=%u action=%s distance_bits=0x%08lx policy=require_clear_inward_radial_component\r\n",
            player_index + 1u,
            movement_remains ? "clamp" : "block",
            (unsigned long)float_bits(roaming_boundary_snapshot.distance));
    } else if (!changed && roaming_boundary_player_blocked[player_index]) {
        SudekiMpLogFormat(
            "control_separation event=roaming_boundary player=%u action=release reason=clear_inward_or_within_limit\r\n",
            player_index + 1u);
    }
    roaming_boundary_player_blocked[player_index] = changed;
    return movement_remains;
}

static void __stdcall enforce_player_one_roaming_boundary(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
) {
    float constrained[3];
    uint8_t *controller;
    void *character;

    if (direction == NULL || !readable_memory(arbiter, 0x14u)) {
        arbiter_movement(
            arbiter, direction, speed, turn_rate, movement_mode);
        return;
    }
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    character = *(void **)((uint8_t *)arbiter + 0x10u);
    if (!readable_memory(controller, CONTROLLER_TARGET_OFFSET +
            sizeof(character)) ||
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) != character) {
        arbiter_movement(
            arbiter, direction, speed, turn_rate, movement_mode);
        return;
    }
    memcpy(constrained, direction, sizeof(constrained));
    if (!constrain_roaming_boundary_movement(0u, constrained)) {
        arbiter_set_speed(arbiter, 0.0f, turn_rate);
        return;
    }
    arbiter_movement(
        arbiter, constrained, speed, turn_rate, movement_mode);
}

static void poll_input_bridge(void) {
    BOOL connected;
    DWORD now;
    int delta_x;
    int delta_y;

    if (!input_bridge_enabled) {
        return;
    }
    connected = SudekiMpInputBridgePoll(&input_bridge_state);
    if (!connected) {
        if (input_bridge_connected) {
            weak_attack_was_down = FALSE;
            stop_second_player_movement();
        }
        input_bridge_connected = FALSE;
        return;
    }
    input_bridge_connected = TRUE;
    now = GetTickCount();
    delta_x = (int)input_bridge_state.right_x -
        (int)input_bridge_last_right_x;
    delta_y = (int)input_bridge_state.right_y -
        (int)input_bridge_last_right_y;
    if ((delta_x > 4096 || delta_x < -4096 ||
         delta_y > 4096 || delta_y < -4096) &&
        (DWORD)(now - input_bridge_last_right_stick_log_tick) >= 250u) {
        SudekiMpLogFormat(
            "input_bridge event=right_stick_observed right_x=%d right_y=%d policy=available_to_player_two_render_camera\r\n",
            (int)input_bridge_state.right_x,
            (int)input_bridge_state.right_y
        );
        input_bridge_last_right_stick_log_tick = now;
        input_bridge_last_right_x = input_bridge_state.right_x;
        input_bridge_last_right_y = input_bridge_state.right_y;
    }
}

static BOOL second_player_combat_active(void) {
    uint8_t *group;

    if (game_base == NULL || group_players_in_combat == NULL ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group))) {
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    return readable_memory(group, 0xd5u) &&
        group_players_in_combat(group) != 0u;
}

static BOOL second_player_exact_interaction_target_known(void) {
    SudekiMpPlayerStatehood *statehood;
    SudekiMpPlayerStatehoodSnapshot snapshot;
    const SudekiMpPlayerLease *lease;

    if (!interaction_requests_enabled || overridden_character == NULL) {
        return FALSE;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    if (!SudekiMpPlayerStatehoodGetSnapshot(
            statehood, GetTickCount(), &snapshot) ||
        snapshot.state != SUDEKIMP_INTERACTION_SESSION_REQUESTED ||
        snapshot.provenance.player_index != 1u ||
        !snapshot.provenance.target_known ||
        snapshot.provenance.target == 0u ||
        snapshot.provenance.source_generation == 0u ||
        snapshot.provenance.kind == SUDEKIMP_INTERACTION_NONE) {
        return FALSE;
    }
    lease = &statehood->players[1];
    return lease->human_present &&
        lease->actor == (uintptr_t)overridden_character &&
        lease->actor == snapshot.provenance.actor &&
        lease->actor_generation != 0u &&
        lease->actor_generation == snapshot.provenance.actor_generation;
}

static BOOL submit_second_player_controller_combat_action(
    void *controller,
    BOOL owns_foreground,
    SudekiMpControllerActionIntent intent,
    const char **reason,
    void **submitted_arbiter,
    uint32_t *flags_50,
    uint32_t *state_58,
    uint8_t *flags_60
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *arbiter;
    void *controller_target;
    SudekiMpControllerCombatFlags combat_flags;

    *reason = "invalid_action_intent";
    *submitted_arbiter = NULL;
    *flags_50 = 0u;
    *state_58 = 0u;
    *flags_60 = 0u;
    if (!SudekiMpControllerActionCombatFlags(intent, &combat_flags)) {
        return FALSE;
    }
    if (!second_player_weak_attack_enabled || game_base == NULL) {
        *reason = "combat_input_consumer_disabled";
        return FALSE;
    }
    if (!owns_foreground) {
        *reason = "game_window_not_foreground";
        return FALSE;
    }
    if (!player_two_requested || character == NULL ||
        !overridden_character_is_in_active_group()) {
        *reason = "no_live_player_two_character";
        return FALSE;
    }

    component = *(uint8_t **)(character + 0x94u);
    mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3cu);
    arbiter = *(uint8_t **)(character + 0x90u);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + CONTROLLER_TARGET_OFFSET);
    if (component == NULL || mode_state == NULL || arbiter == NULL ||
        *(void **)(character + 0xacu) == NULL ||
        *(int16_t *)(component + 0x16au) != 1 ||
        *(mode_state + 0x0bu) != 0 || character == controller_target) {
        *reason = "native_player_two_combat_state_unavailable";
        return FALSE;
    }

    *submitted_arbiter = arbiter;
    *flags_50 = *(uint32_t *)(arbiter + 0x50u);
    *state_58 = *(uint32_t *)(arbiter + 0x58u);
    *flags_60 = *(uint8_t *)(arbiter + 0x60u);
    SudekiMpSubmitArbiterCombatInput(
        game_base + RVA_ARBITER_COMBAT_INPUT,
        arbiter,
        combat_flags.weak,
        combat_flags.strong,
        combat_flags.sweep,
        0,
        0,
        0
    );
    *reason = "native_arbiter_combat_input_submitted";
    return TRUE;
}

static BOOL controller_intent_is_combat(
    SudekiMpControllerActionIntent intent
) {
    return intent == SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK ||
        intent == SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG ||
        intent == SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP;
}

static const char *controller_action_layer_name(
    const SudekiMpControllerActionContext *context
) {
    if (!context->seat_active) return "inactive";
    if (context->modal_active) return "modal";
    if (context->transition_vote_active) return "transition_vote";
    if (context->interaction_target_known) return "known_interaction";
    return "gameplay";
}

static void service_second_player_controller_actions(
    void *controller,
    BOOL owns_foreground,
    BOOL modal_active,
    BOOL transition_vote_active
) {
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution
        results[SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS];
    size_t result_count;
    size_t result_index;

    ZeroMemory(&context, sizeof(context));
    context.seat_active = player_two_requested &&
        overridden_character != NULL &&
        overridden_character_is_in_active_group();
    context.modal_active = modal_active != FALSE;
    context.transition_vote_active = transition_vote_active != FALSE;
    context.interaction_target_known =
        second_player_exact_interaction_target_known();
    context.combat_active = second_player_combat_active();
    context.ranged_character = context.seat_active &&
        second_player_character_is_ranged(overridden_character);
    context.perspective_toggle_available = context.ranged_character &&
        SudekiMpSplitScreenPlayerTwoPerspectiveAvailable(
            overridden_character);
    result_count = SudekiMpControllerActionRouterAdvance(
        &controller_action_router,
        1u,
        input_bridge_enabled && input_bridge_connected,
        input_bridge_state.buttons,
        &context,
        results,
        SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS
    );
    if (result_count > SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS) {
        result_count = SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS;
    }
    for (result_index = 0u; result_index < result_count; ++result_index) {
        SudekiMpControllerActionResolution *resolution =
            &results[result_index];
        const char *delivery = "intent_only";
        const char *reason = "context_owner_must_consume";
        void *arbiter = NULL;
        uint32_t flags_50 = 0u;
        uint32_t state_58 = 0u;
        uint8_t flags_60 = 0u;
        BOOL perspective_first_person = FALSE;
        BOOL perspective_mode_known = FALSE;

        if (resolution->intent == SUDEKIMP_CONTROLLER_INTENT_NONE) {
            delivery = "blocked";
            reason = context.seat_active ?
                "no_action_in_current_context" : "seat_inactive";
        } else if (controller_intent_is_combat(resolution->intent)) {
            if (submit_second_player_controller_combat_action(
                    controller,
                    owns_foreground,
                    resolution->intent,
                    &reason,
                    &arbiter,
                    &flags_50,
                    &state_58,
                    &flags_60)) {
                delivery = "submitted";
            } else {
                delivery = "rejected";
            }
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_INTERACT) {
            reason = "exact_target_consumer_not_connected";
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_PERSPECTIVE_TOGGLE) {
            if (!owns_foreground) {
                delivery = "rejected";
                reason = "game_window_not_foreground";
            } else if (SudekiMpSplitScreenTogglePlayerTwoPerspective(
                           overridden_character,
                           &perspective_first_person)) {
                delivery = "applied";
                reason = "player_two_camera_perspective_toggled";
                perspective_mode_known = TRUE;
            } else {
                delivery = "rejected";
                reason = "player_two_camera_perspective_unavailable";
            }
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU) {
            reason = "per_seat_quick_menu_consumer_not_connected";
        } else if (resolution->intent >=
                SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_UP &&
            resolution->intent <=
                SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_LEFT) {
            reason = "per_seat_quickshot_consumer_not_connected";
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_VOTE_ACCEPT ||
            resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_VOTE_CANCEL) {
            reason = "transition_vote_owner_observes_bridge_independently";
        }
        SudekiMpLogFormat(
            "control_separation event=controller_action_edge phase=rising "
            "player=2 protocol_button=%s intent=%s layer=%s "
            "combat=%s exact_target=%s ranged=%s "
            "perspective_available=%s perspective_mode=%s "
            "delivery=%s reason=%s "
            "character=0x%08lx arbiter=0x%08lx flags_50=0x%08lx "
            "state_58=0x%08lx flags_60=0x%02x\r\n",
            SudekiMpControllerProtocolButtonName(
                resolution->protocol_button),
            SudekiMpControllerActionIntentName(resolution->intent),
            controller_action_layer_name(&context),
            context.combat_active ? "true" : "false",
            context.interaction_target_known ? "true" : "false",
            context.ranged_character ? "true" : "false",
            context.perspective_toggle_available ? "true" : "false",
            perspective_mode_known ?
                (perspective_first_person ?
                    "first_person" : "third_person") : "unchanged",
            delivery,
            reason,
            (unsigned long)(uintptr_t)overridden_character,
            (unsigned long)(uintptr_t)arbiter,
            (unsigned long)flags_50,
            (unsigned long)state_58,
            (unsigned int)flags_60
        );
    }
}

static void quiesce_for_shared_interaction_modal(void) {
    quiesce_second_player_input();
    SudekiMpCombatContextSetInputSource(
        1u, SUDEKIMP_COMBAT_INPUT_NONE, NULL);
    if (!shared_interaction_modal_quiesce_logged) {
        shared_interaction_modal_quiesce_logged = TRUE;
        SudekiMpLogWrite(
            "control_separation event=shared_interaction_modal player=2 "
            "input=quiesced "
            "policy=no_movement_attack_skill_camera_or_request_injection\r\n");
    }
}

static void report_shared_interaction_modal_released(void) {
    if (!shared_interaction_modal_quiesce_logged) {
        return;
    }
    shared_interaction_modal_quiesce_logged = FALSE;
    SudekiMpLogWrite(
        "control_separation event=shared_interaction_modal player=2 "
        "input=released policy=require_new_edges_after_fresh_camera_caches\r\n");
}

static void stop_first_player_movement(void *controller) {
    uint8_t *character;
    void *arbiter;

    if (controller == NULL || arbiter_set_speed == NULL ||
        !readable_memory(controller, 0x24cu)) {
        return;
    }
    character = *(uint8_t **)((uint8_t *)controller + 0x248u);
    if (character == NULL || !readable_memory(character, 0x94u)) {
        return;
    }
    arbiter = *(void **)(character + 0x90u);
    if (arbiter != NULL) {
        arbiter_set_speed(arbiter, 0.0f, 1.0f);
    }
}

BOOL SudekiMpControlSeparationGameplayInputFrozen(void) {
    return SudekiMpInputBridgeGameplaySuppressed() ||
        transition_vote_escape_release_pending ||
        SudekiMpSplitScreenSharedInteractionModalActive();
}

static BOOL service_transition_vote_input_freeze(void *controller) {
    unsigned int ordinal;
    BOOL blacksmith_modal;
    BOOL escape_down =
        (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    BOOL vote_suppressed = SudekiMpInputBridgeGameplaySuppressed();

    if (!vote_suppressed && transition_vote_input_freeze_logged &&
        escape_down) {
        transition_vote_escape_release_pending = TRUE;
    }
    if (!vote_suppressed && transition_vote_escape_release_pending &&
        !escape_down) {
        transition_vote_escape_release_pending = FALSE;
    }
    if (!vote_suppressed && !transition_vote_escape_release_pending) {
        if (transition_vote_input_freeze_logged) {
            transition_vote_input_freeze_logged = FALSE;
            SudekiMpLogWrite(
                "transition_vote event=input_freeze state=released "
                "policy=native_controller_update_resumes_after_escape_release\r\n");
        }
        return FALSE;
    }
    poll_input_bridge();
    blacksmith_modal = SudekiMpBlacksmithUiAdapterActive();
    service_second_player_controller_actions(
        controller,
        FALSE,
        blacksmith_modal,
        vote_suppressed && !blacksmith_modal
    );
    stop_first_player_movement(controller);
    stop_second_player_movement();
    second_player_facing_valid = FALSE;
    weak_attack_was_down = FALSE;
    for (ordinal = 0u; ordinal < 4u; ++ordinal) {
        second_player_skill_keys_were_down[ordinal] =
            (GetAsyncKeyState(
                (int)second_player_skill_virtual_keys[ordinal]) &
             0x8000) != 0;
    }
    if (!transition_vote_input_freeze_logged) {
        transition_vote_input_freeze_logged = TRUE;
        SudekiMpLogWrite(
            "transition_vote event=input_freeze state=active "
            "policy=skip_p1_native_controller_update_and_all_p2_submissions_keep_vote_observer_running_until_vote_and_escape_release\r\n");
    }
    if (update_observer != NULL) {
        update_observer();
    }
    return TRUE;
}

static void poll_roster_participation_input(BOOL owns_foreground) {
    BOOL start_down;
    BOOL back_down;
    DWORD now;

    if (!owns_foreground || !input_bridge_enabled ||
        !SudekiMpSplitScreenRosterParticipationAvailable()) {
        roster_join_start_was_down = FALSE;
        roster_leave_chord_since = 0u;
        roster_leave_chord_consumed = FALSE;
        return;
    }
    start_down = input_bridge_connected &&
        (input_bridge_state.buttons & SUDEKIMP_BRIDGE_BUTTON_START) != 0u;
    back_down = input_bridge_connected &&
        (input_bridge_state.buttons & SUDEKIMP_BRIDGE_BUTTON_BACK) != 0u;
    now = GetTickCount();

    if (!SudekiMpSplitScreenRosterParticipationRequested() &&
        start_down && !back_down && !roster_join_start_was_down) {
        if (SudekiMpSplitScreenRequestRosterParticipation(TRUE)) {
            SudekiMpLogWrite(
                "control_separation event=player_two_participation "
                "source=controller_start state=drop_in_requested "
                "policy=reclaim_locked_roster_character\r\n");
        }
    }
    if (SudekiMpSplitScreenRosterParticipationRequested() &&
        start_down && back_down) {
        if (roster_leave_chord_since == 0u) {
            roster_leave_chord_since = now;
        } else if (!roster_leave_chord_consumed &&
            (DWORD)(now - roster_leave_chord_since) >= 1000u) {
            if (SudekiMpSplitScreenRequestRosterParticipation(FALSE)) {
                roster_leave_chord_consumed = TRUE;
                SudekiMpLogWrite(
                    "control_separation event=player_two_participation "
                    "source=controller_back_start_hold state=dropped_out "
                    "policy=restore_native_ai_retain_roster_character\r\n");
            }
        }
    } else {
        roster_leave_chord_since = 0u;
        roster_leave_chord_consumed = FALSE;
    }
    roster_join_start_was_down = start_down;
}

static BOOL bridge_movement(float *x, float *z, float *speed) {
    float raw_x;
    float raw_z;
    float magnitude;
    float direction_magnitude;
    float scaled_magnitude;

    if (!input_bridge_enabled || !input_bridge_connected) {
        return FALSE;
    }
    raw_x = (float)input_bridge_state.left_x / 32768.0f;
    raw_z = -(float)input_bridge_state.left_y / 32768.0f;
    magnitude = sqrtf(raw_x * raw_x + raw_z * raw_z);
    if (magnitude <= input_bridge_deadzone) {
        *x = 0.0f;
        *z = 0.0f;
        *speed = 0.0f;
        return TRUE;
    }
    direction_magnitude = magnitude;
    if (magnitude > 1.0f) {
        magnitude = 1.0f;
    }
    scaled_magnitude = (magnitude - input_bridge_deadzone) /
        (1.0f - input_bridge_deadzone);
    *x = raw_x / direction_magnitude;
    *z = raw_z / direction_magnitude;
    *speed = scaled_magnitude;
    return TRUE;
}

enum {
    NATIVE_MOVEMENT_GATE_ALLOWED = 0u,
    NATIVE_MOVEMENT_GATE_UNREADABLE = 1u,
    NATIVE_MOVEMENT_GATE_ARBITER_FLAGS = 2u,
    NATIVE_MOVEMENT_GATE_ATTACHMENT_STATE = 3u,
    NATIVE_MOVEMENT_GATE_ARBITER_MODE = 4u,
    NATIVE_MOVEMENT_GATE_GLOBAL_STATE = 5u,
    NATIVE_MOVEMENT_GATE_CONTROL_STATE = 6u,
    NATIVE_MOVEMENT_GATE_CONTROLLER_DISABLED = 7u,
    NATIVE_MOVEMENT_GATE_SPIRIT_NONCASTER_VIRTUALIZED = 8u
};

static const char *native_movement_gate_name(unsigned int gate) {
    switch (gate) {
    case NATIVE_MOVEMENT_GATE_ALLOWED:
        return "allowed";
    case NATIVE_MOVEMENT_GATE_UNREADABLE:
        return "unreadable";
    case NATIVE_MOVEMENT_GATE_ARBITER_FLAGS:
        return "arbiter_flags";
    case NATIVE_MOVEMENT_GATE_ATTACHMENT_STATE:
        return "attachment_state";
    case NATIVE_MOVEMENT_GATE_ARBITER_MODE:
        return "arbiter_mode";
    case NATIVE_MOVEMENT_GATE_GLOBAL_STATE:
        return "global_state";
    case NATIVE_MOVEMENT_GATE_CONTROL_STATE:
        return "control_state";
    case NATIVE_MOVEMENT_GATE_CONTROLLER_DISABLED:
        return "movement_controller_disabled";
    case NATIVE_MOVEMENT_GATE_SPIRIT_NONCASTER_VIRTUALIZED:
        return "spirit_noncaster_virtualized";
    default:
        return "unknown";
    }
}

static void reset_native_movement_acceptance_trace(void) {
    last_native_movement_acceptance = -1;
    last_native_movement_gate = 0xffffffffu;
    last_movement_pipeline_sample_tick = 0u;
    last_movement_pipeline_position_valid = FALSE;
    ZeroMemory(
        last_movement_pipeline_position,
        sizeof(last_movement_pipeline_position)
    );
    controller_update_spirit_virtualization_logged = FALSE;
    movement_controller_update_last_trace_tick = 0u;
    tal_character_update_last_trace_tick = 0u;
}

static uint32_t begin_spirit_noncaster_arbiter_virtualization(
    uint8_t *character,
    uint32_t **arbiter_flags_result
) {
    void *arbiter;
    uint32_t *arbiter_flags;
    uint32_t saved_flags;

    *arbiter_flags_result = NULL;
    if (!readable_memory(character, 0x94u) ||
        !SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(character)) {
        return 0u;
    }
    arbiter = *(void **)(character + 0x90u);
    arbiter_flags = readable_memory(arbiter, 0x54u) ?
        (uint32_t *)((uint8_t *)arbiter + 0x50u) : NULL;
    if (!readable_memory(arbiter_flags, sizeof(*arbiter_flags)) ||
        (*arbiter_flags & 0x0289e568u) != 0x00080000u) {
        return 0u;
    }
    saved_flags = *arbiter_flags;
    *arbiter_flags = saved_flags & ~0x00080000u;
    *arbiter_flags_result = arbiter_flags;
    return saved_flags;
}

static void end_spirit_noncaster_arbiter_virtualization(
    uint8_t *character,
    uint32_t *arbiter_flags,
    uint32_t saved_flags
) {
    if (arbiter_flags == NULL || saved_flags == 0u ||
        !readable_memory(arbiter_flags, sizeof(*arbiter_flags))) {
        return;
    }
    (void)character;
    *arbiter_flags = (*arbiter_flags & ~0x00080000u) |
        (saved_flags & 0x00080000u);
}

static unsigned int classify_native_movement_gate(
    void *arbiter,
    uint32_t ignored_arbiter_flags,
    uint32_t *arbiter_flags,
    int *arbiter_mode,
    unsigned int *arbiter_state_60,
    int *control_state,
    unsigned int *movement_flags
) {
    uint8_t *arbiter_bytes = (uint8_t *)arbiter;
    uint8_t *character;
    uint8_t *attachment_state;
    uint8_t *attachment_owner;
    uint8_t *movement_controller;
    uint8_t *control;
    uint32_t raw_mode;
    unsigned int nibble;

    *arbiter_flags = 0u;
    *arbiter_mode = -1;
    *arbiter_state_60 = 0u;
    *control_state = -1;
    *movement_flags = 0u;
    if (!readable_memory(arbiter_bytes, 0x64u)) {
        return NATIVE_MOVEMENT_GATE_UNREADABLE;
    }
    *arbiter_flags = *(uint32_t *)(arbiter_bytes + 0x50u);
    *arbiter_state_60 = *(uint32_t *)(arbiter_bytes + 0x60u);
    raw_mode = *(uint32_t *)(arbiter_bytes + 0x58u);
    nibble = raw_mode & 0x0fu;
    *arbiter_mode = (int)(nibble >= 8u ? nibble - 16u : nibble);
    character = *(uint8_t **)(arbiter_bytes + 0x10u);
    if (!readable_memory(character, 0xd4u)) {
        return NATIVE_MOVEMENT_GATE_UNREADABLE;
    }
    movement_controller = *(uint8_t **)(character + 0x80u);
    if (!readable_memory(movement_controller, 0xbfu)) {
        return NATIVE_MOVEMENT_GATE_UNREADABLE;
    }
    *movement_flags = movement_controller[0xbeu];
    if (((*arbiter_flags & ~ignored_arbiter_flags) & 0x0289e568u) != 0u) {
        return NATIVE_MOVEMENT_GATE_ARBITER_FLAGS;
    }
    attachment_state = *(uint8_t **)(character + 0xa4u);
    attachment_owner = *(uint8_t **)(character + 0xb8u);
    if (readable_memory(attachment_state, 0x73u) &&
        readable_memory(attachment_owner, 0xb2u) &&
        (attachment_state[0x72u] & 0x10u) != 0u &&
        (attachment_owner[0xb1u] & 0x04u) == 0u) {
        return NATIVE_MOVEMENT_GATE_ATTACHMENT_STATE;
    }
    if (*arbiter_mode == 1 || *arbiter_mode == 3) {
        return NATIVE_MOVEMENT_GATE_ARBITER_MODE;
    }
    if (game_base != NULL &&
        readable_memory(game_base + 0x00409ddcu, 1u) &&
        game_base[0x00409ddcu] != 0u &&
        (*arbiter_state_60 & 0x01u) != 0u) {
        return NATIVE_MOVEMENT_GATE_GLOBAL_STATE;
    }
    control = *(uint8_t **)(character + 0xd0u);
    if (readable_memory(control, 0x20u)) {
        *control_state = *(int *)(control + 0x1cu);
        if (*control_state == 1 || *control_state == 2) {
            return NATIVE_MOVEMENT_GATE_CONTROL_STATE;
        }
    }
    if ((*movement_flags & 0x08u) == 0u) {
        return NATIVE_MOVEMENT_GATE_CONTROLLER_DISABLED;
    }
    return NATIVE_MOVEMENT_GATE_ALLOWED;
}

static void trace_native_movement_acceptance(
    uint8_t *character,
    void *arbiter,
    float requested_speed,
    uint32_t virtualized_arbiter_flags
) {
    uint8_t *movement_controller;
    float accepted_speed = 0.0f;
    uint32_t requested_speed_bits;
    uint32_t accepted_speed_bits = 0u;
    uint32_t arbiter_flags;
    int arbiter_mode;
    unsigned int arbiter_state_60;
    int control_state;
    unsigned int movement_flags;
    unsigned int gate;
    int accepted;

    if (!readable_memory(character, 0x84u)) {
        return;
    }
    movement_controller = *(uint8_t **)(character + 0x80u);
    if (readable_memory(movement_controller, 0x28u)) {
        accepted_speed = *(float *)(movement_controller + 0x24u);
        memcpy(&accepted_speed_bits, &accepted_speed,
            sizeof(accepted_speed_bits));
    }
    memcpy(&requested_speed_bits, &requested_speed,
        sizeof(requested_speed_bits));
    gate = classify_native_movement_gate(
        arbiter,
        virtualized_arbiter_flags,
        &arbiter_flags,
        &arbiter_mode,
        &arbiter_state_60,
        &control_state,
        &movement_flags
    );
    accepted = gate == NATIVE_MOVEMENT_GATE_ALLOWED &&
        isfinite(accepted_speed) && accepted_speed > 0.0001f;
    if (accepted && virtualized_arbiter_flags != 0u) {
        gate = NATIVE_MOVEMENT_GATE_SPIRIT_NONCASTER_VIRTUALIZED;
    }
    if (accepted == last_native_movement_acceptance &&
        gate == last_native_movement_gate) {
        return;
    }
    last_native_movement_acceptance = accepted;
    last_native_movement_gate = gate;
    SudekiMpLogFormat(
        "control_separation event=native_movement_acceptance character=0x%08lx arbiter=0x%08lx accepted=%u gate=%s gate_id=%u requested_speed_bits=0x%08lx accepted_speed_bits=0x%08lx arbiter_flags=0x%08lx blocked_flag_mask=0x%08lx virtualized_arbiter_flags=0x%08lx arbiter_mode=%d arbiter_state_60=0x%08lx control_state=%d movement_flags=0x%02x policy=exact_scoped_native_arbiter_submission_with_spirit_noncaster_flag_virtualization\r\n",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)arbiter,
        accepted ? 1u : 0u,
        native_movement_gate_name(gate),
        gate,
        (unsigned long)requested_speed_bits,
        (unsigned long)accepted_speed_bits,
        (unsigned long)arbiter_flags,
        (unsigned long)(arbiter_flags & 0x0289e568u),
        (unsigned long)virtualized_arbiter_flags,
        arbiter_mode,
        (unsigned long)arbiter_state_60,
        control_state,
        movement_flags
    );
}

static void trace_second_player_movement_pipeline(
    uint8_t *character,
    void *arbiter,
    uint32_t virtualized_arbiter_flags
) {
    DWORD now;
    uint8_t *movement_controller;
    uint8_t *transform;
    float target_speed;
    float smoothed_speed;
    float previous_speed;
    float current_speed;
    float run_blend;
    float position[3];
    float delta[3] = {0.0f, 0.0f, 0.0f};
    int direction_family;

    if (virtualized_arbiter_flags == 0u ||
        !readable_memory(character, 0x84u)) {
        return;
    }
    now = GetTickCount();
    if (last_movement_pipeline_sample_tick != 0u &&
        (DWORD)(now - last_movement_pipeline_sample_tick) < 200u) {
        return;
    }
    movement_controller = *(uint8_t **)(character + 0x80u);
    transform = *(uint8_t **)(character + 0x44u);
    if (!readable_memory(movement_controller, 0x64u) ||
        !readable_memory(transform, 0x24u)) {
        SudekiMpLogFormat(
            "control_separation event=second_player_movement_pipeline phase=unavailable character=0x%08lx arbiter=0x%08lx movement_controller=0x%08lx transform=0x%08lx policy=read_only_post_native_submission_pipeline_trace\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            (unsigned long)(uintptr_t)movement_controller,
            (unsigned long)(uintptr_t)transform
        );
        last_movement_pipeline_sample_tick = now;
        return;
    }
    target_speed = *(float *)(movement_controller + 0x24u);
    smoothed_speed = *(float *)(movement_controller + 0x28u);
    direction_family = *(int *)(movement_controller + 0x54u);
    previous_speed = *(float *)(movement_controller + 0x58u);
    current_speed = *(float *)(movement_controller + 0x5cu);
    run_blend = *(float *)(movement_controller + 0x60u);
    position[0] = *(float *)(transform + 0x18u);
    position[1] = *(float *)(transform + 0x1cu);
    position[2] = *(float *)(transform + 0x20u);
    if (last_movement_pipeline_position_valid) {
        delta[0] = position[0] - last_movement_pipeline_position[0];
        delta[1] = position[1] - last_movement_pipeline_position[1];
        delta[2] = position[2] - last_movement_pipeline_position[2];
    }
    SudekiMpLogFormat(
        "control_separation event=second_player_movement_pipeline phase=sample character=0x%08lx arbiter=0x%08lx movement_controller=0x%08lx transform=0x%08lx elapsed_ms=%lu target_speed_bits=0x%08lx smoothed_speed_bits=0x%08lx direction_family=%d previous_speed_bits=0x%08lx current_speed_bits=0x%08lx run_blend_bits=0x%08lx position_bits=%08lx,%08lx,%08lx delta_bits=%08lx,%08lx,%08lx virtualized_arbiter_flags=0x%08lx policy=read_only_post_native_submission_pipeline_trace\r\n",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)arbiter,
        (unsigned long)(uintptr_t)movement_controller,
        (unsigned long)(uintptr_t)transform,
        last_movement_pipeline_sample_tick == 0u ? 0ul :
            (unsigned long)(DWORD)(now - last_movement_pipeline_sample_tick),
        (unsigned long)float_bits(target_speed),
        (unsigned long)float_bits(smoothed_speed),
        direction_family,
        (unsigned long)float_bits(previous_speed),
        (unsigned long)float_bits(current_speed),
        (unsigned long)float_bits(run_blend),
        (unsigned long)float_bits(position[0]),
        (unsigned long)float_bits(position[1]),
        (unsigned long)float_bits(position[2]),
        (unsigned long)float_bits(delta[0]),
        (unsigned long)float_bits(delta[1]),
        (unsigned long)float_bits(delta[2]),
        (unsigned long)virtualized_arbiter_flags
    );
    memcpy(
        last_movement_pipeline_position,
        position,
        sizeof(last_movement_pipeline_position)
    );
    last_movement_pipeline_position_valid = TRUE;
    last_movement_pipeline_sample_tick = now;
}

static void __attribute__((regparm(1), stdcall))
trace_movement_controller_update(
    void *movement_controller,
    void *update_data
) {
    MovementControllerUpdateFunction original =
        (MovementControllerUpdateFunction)
            movement_controller_update_hook.trampoline;
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *expected_controller;
    uint8_t *transform;
    DWORD now;
    void *caller;
    unsigned long caller_rva;
    uint32_t update_words[4] = {0u, 0u, 0u, 0u};
    uint32_t position_before[3] = {0u, 0u, 0u};
    uint32_t position_after[3] = {0u, 0u, 0u};
    BOOL trace_this_update = FALSE;

    expected_controller = readable_memory(character, 0x84u) ?
        *(uint8_t **)(character + 0x80u) : NULL;
    transform = readable_memory(character, 0x48u) ?
        *(uint8_t **)(character + 0x44u) : NULL;
    now = GetTickCount();
    if (movement_controller == expected_controller &&
        SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(character) &&
        readable_memory(update_data, 0x10u) &&
        (movement_controller_update_last_trace_tick == 0u ||
         (DWORD)(now - movement_controller_update_last_trace_tick) >= 200u)) {
        memcpy(update_words, update_data, sizeof(update_words));
        if (readable_memory(transform, 0x24u)) {
            memcpy(&position_before[0], transform + 0x18u, sizeof(uint32_t));
            memcpy(&position_before[1], transform + 0x1cu, sizeof(uint32_t));
            memcpy(&position_before[2], transform + 0x20u, sizeof(uint32_t));
        }
        trace_this_update = TRUE;
    }
    original(movement_controller, update_data);
    if (!trace_this_update) {
        return;
    }
    if (readable_memory(transform, 0x24u)) {
        memcpy(&position_after[0], transform + 0x18u, sizeof(uint32_t));
        memcpy(&position_after[1], transform + 0x1cu, sizeof(uint32_t));
        memcpy(&position_after[2], transform + 0x20u, sizeof(uint32_t));
    }
    caller = __builtin_return_address(0);
    caller_rva = game_base != NULL &&
        (uint8_t *)caller >= game_base &&
        (uint8_t *)caller < game_base + SUPPORTED_IMAGE_SIZE ?
        (unsigned long)((uint8_t *)caller - game_base) : 0xfffffffful;
    movement_controller_update_last_trace_tick = now;
    SudekiMpLogFormat(
        "control_separation event=movement_controller_update phase=sample character=0x%08lx movement_controller=0x%08lx update_data=0x%08lx caller=0x%08lx caller_rva=0x%08lx update_words=%08lx,%08lx,%08lx,%08lx position_before=%08lx,%08lx,%08lx position_after=%08lx,%08lx,%08lx policy=read_only_exact_native_movement_controller_update_dt_and_transform_trace\r\n",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)movement_controller,
        (unsigned long)(uintptr_t)update_data,
        (unsigned long)(uintptr_t)caller,
        caller_rva,
        (unsigned long)update_words[0],
        (unsigned long)update_words[1],
        (unsigned long)update_words[2],
        (unsigned long)update_words[3],
        (unsigned long)position_before[0],
        (unsigned long)position_before[1],
        (unsigned long)position_before[2],
        (unsigned long)position_after[0],
        (unsigned long)position_after[1],
        (unsigned long)position_after[2]
    );
}

static unsigned long virtual_update_target_rva(
    uint8_t *owner,
    size_t component_offset,
    size_t slot_offset
) {
    uint8_t *component;
    uint8_t *vtable;
    void *target;

    component = owner + component_offset;
    if (!readable_memory(component, sizeof(void *))) {
        return 0xfffffffful;
    }
    vtable = *(uint8_t **)component;
    if (!readable_memory(vtable + slot_offset, sizeof(void *))) {
        return 0xfffffffful;
    }
    target = *(void **)(vtable + slot_offset);
    if (!game_code_pointer(target)) {
        return 0xfffffffful;
    }
    return (unsigned long)((uint8_t *)target - game_base);
}

static void SUDEKIMP_THISCALL trace_tal_character_update(
    void *character_update,
    void *update_data
) {
    TalCharacterUpdateFunction original =
        (TalCharacterUpdateFunction)tal_character_update_hook.trampoline;
    uint8_t *owner = (uint8_t *)character_update;
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *transform = NULL;
    DWORD now = GetTickCount();
    uint32_t update_words[4] = {0u, 0u, 0u, 0u};
    uint32_t position_before[3] = {0u, 0u, 0u};
    uint32_t position_after[3] = {0u, 0u, 0u};
    unsigned long phase_targets[4] = {
        0xfffffffful, 0xfffffffful, 0xfffffffful, 0xfffffffful
    };
    BOOL trace_this_update = FALSE;

    if (character != NULL && owner == character + 0x08u &&
        SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(character) &&
        readable_memory(update_data, sizeof(update_words)) &&
        (tal_character_update_last_trace_tick == 0u ||
         (DWORD)(now - tal_character_update_last_trace_tick) >= 200u)) {
        transform = readable_memory(character, 0x48u) ?
            *(uint8_t **)(character + 0x44u) : NULL;
        memcpy(update_words, update_data, sizeof(update_words));
        if (readable_memory(transform, 0x24u)) {
            memcpy(position_before, transform + 0x18u, sizeof(position_before));
        }
        phase_targets[0] = virtual_update_target_rva(owner, 0x878u, 0x3cu);
        phase_targets[1] = virtual_update_target_rva(owner, 0x39cu, 0x40u);
        phase_targets[2] = virtual_update_target_rva(owner, 0xaa0u, 0x3cu);
        phase_targets[3] = virtual_update_target_rva(owner, 0x1608u, 0x3cu);
        trace_this_update = TRUE;
    }

    original(character_update, update_data);
    if (!trace_this_update) {
        return;
    }
    if (readable_memory(transform, 0x24u)) {
        memcpy(position_after, transform + 0x18u, sizeof(position_after));
    }
    tal_character_update_last_trace_tick = now;
    SudekiMpLogFormat(
        "control_separation event=tal_character_update phase=sample character=0x%08lx owner=0x%08lx update_data=0x%08lx update_words=%08lx,%08lx,%08lx,%08lx virtual_targets_rva=%08lx,%08lx,%08lx,%08lx position_before=%08lx,%08lx,%08lx position_after=%08lx,%08lx,%08lx policy=read_only_exact_tal_post_controller_phase_trace\r\n",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)owner,
        (unsigned long)(uintptr_t)update_data,
        (unsigned long)update_words[0],
        (unsigned long)update_words[1],
        (unsigned long)update_words[2],
        (unsigned long)update_words[3],
        phase_targets[0],
        phase_targets[1],
        phase_targets[2],
        phase_targets[3],
        (unsigned long)position_before[0],
        (unsigned long)position_before[1],
        (unsigned long)position_before[2],
        (unsigned long)position_after[0],
        (unsigned long)position_after[1],
        (unsigned long)position_after[2]
    );
}

static void poll_second_player_movement(
    void *controller,
    void *update_data,
    BOOL owns_foreground
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    void *arbiter;
    void *controller_target;
    int x = 0;
    int z = 0;
    float input_x;
    float input_z;
    float movement_speed;
    BOOL bridge_source;
    BOOL player_two_camera_basis = FALSE;
    float direction[3];
    uint32_t direction_x_bits;
    uint32_t direction_z_bits;
    uint32_t *arbiter_flags;
    uint32_t saved_arbiter_flags = 0u;
    uint32_t virtualized_arbiter_flags = 0u;
    uint8_t *movement_controller;
    float frame_delta;
    float direct_move_speed;

    if (!player_two_requested) {
        stop_second_player_movement();
        return;
    }
    if (!second_player_movement_enabled || character == NULL) {
        return;
    }
    if (!overridden_character_is_in_active_group()) {
        stop_second_player_movement();
        return;
    }
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    arbiter = *(void **)(character + 0x90);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248);
    if (!owns_foreground || component == NULL || mode_state == NULL ||
        arbiter == NULL || *(void **)(character + 0xac) == NULL ||
        *(int16_t *)(component + 0x16a) != 1 ||
        *(mode_state + 0x0b) != 0 || character == controller_target) {
        stop_second_player_movement();
        return;
    }

    bridge_source = input_bridge_enabled;
    if (bridge_source) {
        if (!bridge_movement(&input_x, &input_z, &movement_speed)) {
            stop_second_player_movement();
            return;
        }
    } else {
        x = ((GetAsyncKeyState('L') & 0x8000) != 0) -
            ((GetAsyncKeyState('J') & 0x8000) != 0);
        z = ((GetAsyncKeyState('I') & 0x8000) != 0) -
            ((GetAsyncKeyState('K') & 0x8000) != 0);
        input_x = (float)x;
        input_z = (float)z;
        movement_speed = 1.0f;
        if (x != 0 && z != 0) {
            input_x *= 0.70710678f;
            input_z *= 0.70710678f;
        }
    }
    if (movement_speed <= 0.0001f) {
        stop_second_player_movement();
        return;
    }

    direction[0] = input_x;
    direction[1] = 0.0f;
    direction[2] = input_z;
    if (camera_relative_movement_enabled) {
        float transformed[3] = {0.0f, 0.0f, 0.0f};
        float horizontal_length;

        if (bridge_source) {
            player_two_camera_basis = SudekiMpTransformPlayerTwoMovement(
                direction,
                transformed
            );
        }
        if (!player_two_camera_basis) {
            movement_camera_transform(controller, transformed, direction);
        }
        transformed[1] = 0.0f;
        horizontal_length = sqrtf(
            transformed[0] * transformed[0] +
            transformed[2] * transformed[2]
        );
        if (horizontal_length <= 0.0001f) {
            stop_second_player_movement();
            return;
        }
        direction[0] = transformed[0] / horizontal_length;
        direction[1] = 0.0f;
        direction[2] = transformed[2] / horizontal_length;
    }
    if (!constrain_roaming_boundary_movement(1u, direction)) {
        stop_second_player_movement();
        return;
    }
    memcpy(&direction_x_bits, &direction[0], sizeof(direction_x_bits));
    memcpy(&direction_z_bits, &direction[2], sizeof(direction_z_bits));
    arbiter_flags = (uint32_t *)((uint8_t *)arbiter + 0x50u);
    if (readable_memory(arbiter_flags, sizeof(*arbiter_flags)) &&
        SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(character) &&
        (*arbiter_flags & 0x0289e568u) == 0x00080000u) {
        saved_arbiter_flags = *arbiter_flags;
        virtualized_arbiter_flags = 0x00080000u;
        *arbiter_flags = saved_arbiter_flags & ~virtualized_arbiter_flags;
    }
    arbiter_movement(arbiter, direction, movement_speed, 1.0f, 0u);
    movement_controller = readable_memory(character, 0x84u) ?
        *(uint8_t **)(character + 0x80u) : NULL;
    frame_delta = readable_memory(update_data, 0x10u) ?
        *(float *)((uint8_t *)update_data + 0x0cu) : 0.0f;
    direct_move_speed = readable_memory(controller, 0x1d8u) ?
        *(float *)((uint8_t *)controller + 0x1d4u) : 0.0f;
    if (virtualized_arbiter_flags != 0u &&
        readable_memory(movement_controller, 0xbfu) &&
        movement_controller_set_absolute_delta != NULL &&
        isfinite(frame_delta) && frame_delta > 0.0f &&
        frame_delta <= 0.25f &&
        isfinite(direct_move_speed) && direct_move_speed > 0.0f &&
        direct_move_speed <= 100.0f) {
        float direct_scale =
            movement_speed *
            direct_move_speed *
            spirit_noncaster_direct_movement_pace *
            frame_delta;
        DWORD now = GetTickCount();

        movement_controller_set_absolute_delta(
            movement_controller,
            direction[0] * direct_scale,
            0.0f,
            direction[2] * direct_scale
        );
        if (!spirit_direct_movement_active ||
            spirit_direct_movement_last_trace_tick == 0u ||
            (DWORD)(now - spirit_direct_movement_last_trace_tick) >= 500u) {
            SudekiMpLogFormat(
                "control_separation event=spirit_noncaster_direct_movement phase=submit character=0x%08lx movement_controller=0x%08lx direction_bits=%08lx,%08lx,%08lx movement_speed_bits=0x%08lx direct_move_speed_bits=0x%08lx pace_bits=0x%08lx frame_delta_bits=0x%08lx direct_scale_bits=0x%08lx policy=exact_native_set_absolute_delta_only_while_spirit_arbiter_lock_is_virtualized\r\n",
                (unsigned long)(uintptr_t)character,
                (unsigned long)(uintptr_t)movement_controller,
                (unsigned long)float_bits(direction[0]),
                (unsigned long)float_bits(direction[1]),
                (unsigned long)float_bits(direction[2]),
                (unsigned long)float_bits(movement_speed),
                (unsigned long)float_bits(direct_move_speed),
                (unsigned long)float_bits(spirit_noncaster_direct_movement_pace),
                (unsigned long)float_bits(frame_delta),
                (unsigned long)float_bits(direct_scale)
            );
            spirit_direct_movement_last_trace_tick = now;
        }
        spirit_direct_movement_active = TRUE;
    } else if (spirit_direct_movement_active) {
        spirit_direct_movement_active = FALSE;
        spirit_direct_movement_last_trace_tick = 0u;
        SudekiMpLogWrite(
            "control_separation event=spirit_noncaster_direct_movement phase=inactive policy=native_root_motion_only\r\n"
        );
    }
    if (virtualized_arbiter_flags != 0u &&
        readable_memory(arbiter_flags, sizeof(*arbiter_flags))) {
        *arbiter_flags = (*arbiter_flags & ~virtualized_arbiter_flags) |
            (saved_arbiter_flags & virtualized_arbiter_flags);
    }
    trace_native_movement_acceptance(
        character,
        arbiter,
        movement_speed,
        virtualized_arbiter_flags
    );
    trace_second_player_movement_pipeline(
        character,
        arbiter,
        virtualized_arbiter_flags
    );
    if (!second_player_movement_active ||
        (bridge_source &&
         (((int)input_bridge_state.left_x - last_movement_x > 4096 ||
           (int)input_bridge_state.left_x - last_movement_x < -4096) ||
          ((int)input_bridge_state.left_y - last_movement_z > 4096 ||
           (int)input_bridge_state.left_y - last_movement_z < -4096))) ||
        (!bridge_source && (x != last_movement_x || z != last_movement_z))) {
        SudekiMpLogFormat(
            "control_separation event=second_player_movement phase=submit source=%s character=0x%08lx arbiter=0x%08lx input_x=%d input_z=%d direction_bits=%08lx,00000000,%08lx camera_relative=%s camera_basis=%s speed_bits=0x%08lx turn_rate_bits=0x3f800000 movement_mode=0\r\n",
            bridge_source ? "external_bridge" : "keyboard",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            bridge_source ? (int)input_bridge_state.left_x : x,
            bridge_source ? (int)input_bridge_state.left_y : z,
            (unsigned long)direction_x_bits,
            (unsigned long)direction_z_bits,
            camera_relative_movement_enabled ? "true" : "false",
            player_two_camera_basis ? "player_two_render" : "native_player_one",
            (unsigned long)float_bits(movement_speed)
        );
    }
    second_player_movement_active = TRUE;
    second_player_movement_magnitude = movement_speed;
    last_movement_x = bridge_source ? input_bridge_state.left_x : x;
    last_movement_z = bridge_source ? input_bridge_state.left_y : z;
}

static void poll_second_player_camera_facing(
    void *controller,
    BOOL owns_foreground
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *arbiter;
    void *controller_target;
    float right_x;
    float right_y;
    static const float local_forward[3] = {0.0f, 0.0f, 1.0f};
    float world_forward[3];
    float dot;

    if (!player_two_requested || !owns_foreground ||
        !second_player_movement_enabled ||
        !camera_relative_movement_enabled || !input_bridge_enabled ||
        !input_bridge_connected || character == NULL ||
        !overridden_character_is_in_active_group()) {
        second_player_facing_valid = FALSE;
        return;
    }
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248u);
    arbiter = *(uint8_t **)(character + 0x90u);
    if (character == controller_target || !readable_memory(arbiter, 0x54u) ||
        (*(uint32_t *)(arbiter + 0x50u) & 0x00000002u) == 0u) {
        second_player_facing_valid = FALSE;
        return;
    }
    right_x = (float)input_bridge_state.right_x / 32768.0f;
    right_y = (float)input_bridge_state.right_y / 32768.0f;
    if (sqrtf(right_x * right_x + right_y * right_y) <=
        input_bridge_deadzone && second_player_movement_active) {
        return;
    }
    if (!SudekiMpTransformPlayerTwoMovement(
            local_forward,
            world_forward)) {
        return;
    }
    /* Position::SetForward is a native orientation commit, not a cheap
     * render-only assignment. Repeating it every controller tick while the
     * stick is at rest can make the observer model visibly micro-oscillate.
     * Keep the first commit and only send another one after a meaningful
     * camera rotation (about 0.5 degrees). */
    dot = world_forward[0] * second_player_last_facing[0] +
        world_forward[1] * second_player_last_facing[1] +
        world_forward[2] * second_player_last_facing[2];
    if (second_player_facing_valid && dot > 0.99996f) {
        return;
    }
    if (SudekiMpAlignPlayerTwoFacingToCamera(character)) {
        memcpy(second_player_last_facing, world_forward,
            sizeof(second_player_last_facing));
        second_player_facing_valid = TRUE;
    }
}

static void poll_second_player_weak_attack(
    void *controller,
    BOOL owns_foreground
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *arbiter;
    void *controller_target;
    BOOL key_is_down;

    if (!second_player_weak_attack_enabled || input_bridge_enabled) {
        /* External-controller face buttons are owned exclusively by the
         * seat-neutral action router. Keep this polling path only for the
         * legacy keyboard prototype so A can never submit twice. */
        weak_attack_was_down = FALSE;
        return;
    }
    key_is_down =
        (GetAsyncKeyState((int)weak_attack_virtual_key) & 0x8000) != 0;
    if (!player_two_requested) {
        weak_attack_was_down = key_is_down;
        return;
    }
    if (character == NULL || !overridden_character_is_in_active_group()) {
        weak_attack_was_down = key_is_down;
        return;
    }

    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    arbiter = *(uint8_t **)(character + 0x90);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248);
    if (!owns_foreground || component == NULL || mode_state == NULL ||
        arbiter == NULL || *(void **)(character + 0xac) == NULL ||
        *(int16_t *)(component + 0x16a) != 1 ||
        *(mode_state + 0x0b) != 0 || character == controller_target) {
        weak_attack_was_down = key_is_down;
        return;
    }

    if (key_is_down && !weak_attack_was_down) {
        uint32_t flags_50 = *(uint32_t *)(arbiter + 0x50);
        uint32_t state_58 = *(uint32_t *)(arbiter + 0x58);
        uint8_t flags_60 = *(uint8_t *)(arbiter + 0x60);

        SudekiMpLogFormat(
            "control_separation event=second_player_weak_attack phase=submit character=0x%08lx arbiter=0x%08lx flags_50=0x%08lx state_58=0x%08lx flags_60=0x%02x\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            (unsigned long)flags_50,
            (unsigned long)state_58,
            (unsigned int)flags_60
        );
        SudekiMpSubmitArbiterCombatInput(
            game_base + RVA_ARBITER_COMBAT_INPUT,
            arbiter,
            1,
            0,
            0,
            0,
            0,
            0
        );
    }
    weak_attack_was_down = key_is_down;
}

static void poll_second_player_skills(
    void *controller,
    BOOL owns_foreground
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    void *controller_target;
    unsigned int ordinal;

    if (!second_player_skills_enabled) {
        return;
    }
    if (!player_two_requested) {
        for (ordinal = 0u; ordinal < 4u; ++ordinal) {
            second_player_skill_keys_were_down[ordinal] =
                (GetAsyncKeyState(
                    (int)second_player_skill_virtual_keys[ordinal]) &
                 0x8000) != 0;
        }
        return;
    }
    component = character == NULL ? NULL :
        *(uint8_t **)(character + 0x94u);
    mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3cu);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248u);
    for (ordinal = 0u; ordinal < 4u; ++ordinal) {
        BOOL key_is_down = (GetAsyncKeyState(
            (int)second_player_skill_virtual_keys[ordinal]
        ) & 0x8000) != 0;

        if (owns_foreground && key_is_down &&
            !second_player_skill_keys_were_down[ordinal]) {
            const char *reason = "invalid_character_state";

            SudekiMpCombatContextsPollGame((HMODULE)game_base);
            if (character != NULL &&
                overridden_character_is_in_active_group() &&
                component != NULL && mode_state != NULL &&
                *(void **)(character + 0x90u) != NULL &&
                *(void **)(character + 0xacu) != NULL &&
                *(int16_t *)(component + 0x16au) == 1 &&
                *(mode_state + 0x0bu) == 0 &&
                character != controller_target &&
                SudekiMpCombatContextCanStartSkill(1u, &reason)) {
                SudekiMpSkillActivationResult result =
                    SudekiMpActivateCharacterQuickSkill(character, ordinal);
                SudekiMpCombatContextsPollGame((HMODULE)game_base);
                SudekiMpLogFormat(
                    "realtime_skill_combat event=player_skill_input player=2 ordinal=%u virtual_key=0x%02lx character=0x%08lx status=%s skill=0x%08lx skill_data=0x%08lx slot=%d validation=%d use=%u\r\n",
                    ordinal,
                    (unsigned long)second_player_skill_virtual_keys[ordinal],
                    (unsigned long)(uintptr_t)character,
                    SudekiMpSkillActivationStatusName(result.status),
                    (unsigned long)(uintptr_t)result.skill,
                    (unsigned long)(uintptr_t)result.skill_data,
                    result.slot,
                    result.validation_result,
                    (unsigned int)result.use_result
                );
            } else {
                SudekiMpLogFormat(
                    "realtime_skill_combat event=player_skill_rejected player=2 ordinal=%u virtual_key=0x%02lx reason=%s policy=fail_safe_native_targeting_serialization\r\n",
                    ordinal,
                    (unsigned long)second_player_skill_virtual_keys[ordinal],
                    reason
                );
            }
        }
        second_player_skill_keys_were_down[ordinal] = key_is_down;
    }
}

static void poll_second_player_target_trace(BOOL owns_foreground) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *targeter;
    uint8_t *transform;
    void *target_node;
    uint32_t targeter_flags;
    uint32_t forward_bits[3] = {0u, 0u, 0u};
    int auto_enabled;
    DWORD now;

    if (!player_two_requested || !target_trace_enabled ||
        !owns_foreground || character == NULL ||
        !overridden_character_is_in_active_group()) {
        return;
    }
    now = GetTickCount();
    if ((DWORD)(now - target_trace_last_sample_tick) < 100u) {
        return;
    }
    target_trace_last_sample_tick = now;
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    if (component == NULL || mode_state == NULL ||
        *(int16_t *)(component + 0x16a) != 1 ||
        *(mode_state + 0x0b) != 0) {
        return;
    }
    targeter = *(uint8_t **)(character + 0xac);
    transform = *(uint8_t **)(character + 0x44);
    if (targeter == NULL) {
        return;
    }
    target_node = *(void **)(targeter + 0x54);
    targeter_flags = *(uint32_t *)(targeter + 0x84);
    auto_enabled = (targeter_flags & 2u) != 0;
    if (transform != NULL) {
        memcpy(forward_bits, transform + 0x50, sizeof(forward_bits));
    }
    if (target_node != target_trace_last_node ||
        auto_enabled != target_trace_last_auto_enabled) {
        SudekiMpLogFormat(
            "control_separation event=second_player_target_trace character=0x%08lx targeter=0x%08lx target_node=0x%08lx auto_target_enabled=%d forward_bits=%08lx,%08lx,%08lx\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)targeter,
            (unsigned long)(uintptr_t)target_node,
            auto_enabled,
            (unsigned long)forward_bits[0],
            (unsigned long)forward_bits[1],
            (unsigned long)forward_bits[2]
        );
        target_trace_last_node = target_node;
        target_trace_last_auto_enabled = auto_enabled;
    }
}

static uint8_t *find_character_party_slot(
    uint8_t *group,
    void *wanted_character,
    unsigned int *slot_index
) {
    unsigned int index;

    for (index = 0; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        uint8_t *character = *(uint8_t **)slot;
        if (character == wanted_character) {
            if (slot_index != NULL) {
                *slot_index = index;
            }
            return slot;
        }
    }
    return NULL;
}

static uint8_t *find_second_player_party_slot(
    uint8_t *group,
    void *controller_target,
    unsigned int *slot_index
) {
    unsigned int index;

    if (group == NULL || controller_target == NULL ||
        *(void **)(group + PARTY_SLOT_FIRST_OFFSET) != controller_target) {
        return NULL;
    }
    for (index = 1u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        void *candidate = *(void **)slot;

        if (candidate != NULL && candidate != controller_target &&
            (requested_player_two_character == NULL ||
             candidate == requested_player_two_character)) {
            if (slot_index != NULL) {
                *slot_index = index;
            }
            return slot;
        }
    }
    return NULL;
}

static void log_control_state(
    const char *event,
    const char *result,
    uint8_t *slot,
    unsigned int slot_index
) {
    uint8_t *character = slot == NULL ? NULL : *(uint8_t **)slot;
    uint8_t *component = character == NULL ? NULL :
        *(uint8_t **)(character + 0x94);
    uint8_t *mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3c);
    int control_ref = component == NULL ? -1 :
        (int)*(int16_t *)(component + 0x16a);
    int ai_enabled = mode_state == NULL ? -1 : (int)*(mode_state + 0x0b);

    SudekiMpLogFormat(
        "control_separation event=%s result=%s slot=%u character=0x%08lx component=0x%08lx control_ref_16a=%d ai_enabled_3c_0b=%d\r\n",
        event,
        result,
        slot_index,
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)component,
        control_ref,
        ai_enabled
    );
}

static void toggle_second_player_ai(void) {
    uint8_t *group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    uint8_t *controller = *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL
    );
    void *controller_target = controller == NULL ? NULL :
        *(void **)(controller + 0x248);
    uint8_t *slot;
    uint8_t *character;
    uint8_t *component;
    uint8_t *mode_state;
    unsigned int slot_index = 0;
    int before_ref;
    int before_mode;
    int after_ref;
    int after_mode;

    if (group == NULL) {
        SudekiMpLogWrite(
            "control_separation event=toggle_abort reason=no_active_group\r\n"
        );
        return;
    }

    if (overridden_character == NULL) {
        slot = find_second_player_party_slot(
            group,
            controller_target,
            &slot_index
        );
        if (slot == NULL) {
            SudekiMpLogFormat(
                "control_separation event=toggle_abort reason=%s controller_target=0x%08lx front_character=0x%08lx policy=first_non_front_active_party_member\r\n",
                controller_target == NULL ?
                    "no_controller_target" :
                    (*(void **)(group + PARTY_SLOT_FIRST_OFFSET) !=
                        controller_target ?
                        "front_character_not_controller_owned" :
                        "second_player_not_in_party"),
                (unsigned long)(uintptr_t)controller_target,
                (unsigned long)(uintptr_t)*(void **)(
                    group + PARTY_SLOT_FIRST_OFFSET
                )
            );
            return;
        }
        character = *(uint8_t **)slot;
        component = *(uint8_t **)(character + 0x94);
        mode_state = component == NULL ? NULL :
            *(uint8_t **)(component + 0x3c);
        if (component == NULL || mode_state == NULL) {
            SudekiMpLogWrite(
                "control_separation event=toggle_abort reason=incomplete_ai_component\r\n"
            );
            return;
        }
        before_ref = (int)*(int16_t *)(component + 0x16a);
        before_mode = (int)*(mode_state + 0x0b);
        if (before_ref != 0 || before_mode != 1) {
            log_control_state("override_abort", "unexpected_initial_state",
                slot, slot_index);
            return;
        }

        ai_override_control(slot);
        after_ref = (int)*(int16_t *)(component + 0x16a);
        after_mode = (int)*(mode_state + 0x0b);
        if (after_ref == before_ref + 1 && after_mode == 0) {
            overridden_character = character;
            reset_native_movement_acceptance_trace();
            reset_target_trace_state();
            log_control_state("override", "success", slot, slot_index);
            if (SudekiMpCleanroomEngineRefreshCombatMode()) {
                SudekiMpLogFormat(
                    "control_separation event=combat_arm_refresh status=confirmed character=0x%08lx reason=second_player_control_override policy=native_group_transition\r\n",
                    (unsigned long)(uintptr_t)character
                );
            } else {
                SudekiMpLogFormat(
                    "control_separation event=combat_arm_refresh status=skipped character=0x%08lx reason=combat_mode_unavailable_or_disabled policy=native_group_transition\r\n",
                    (unsigned long)(uintptr_t)character
                );
            }
        } else {
            log_control_state("override", "verification_failed", slot,
                slot_index);
            if (after_ref > before_ref) {
                ai_default_control(slot);
                log_control_state("override_rollback", "requested", slot,
                    slot_index);
            }
        }
        return;
    }

    slot = find_character_party_slot(
        group,
        overridden_character,
        &slot_index
    );
    if (slot == NULL) {
        SudekiMpLogWrite(
            "control_separation event=restore status=released_without_native_default "
            "reason=owned_character_no_longer_in_active_party "
            "policy=drop_stale_runtime_identity_after_party_rebuild\r\n"
        );
        overridden_character = NULL;
        stop_second_player_movement();
        reset_native_movement_acceptance_trace();
        reset_target_trace_state();
        return;
    }
    character = *(uint8_t **)slot;
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    if (component == NULL || mode_state == NULL) {
        SudekiMpLogWrite(
            "control_separation event=restore_abort reason=incomplete_ai_component\r\n"
        );
        return;
    }
    before_ref = (int)*(int16_t *)(component + 0x16a);
    before_mode = (int)*(mode_state + 0x0b);
    if (before_ref != 1) {
        log_control_state("restore_abort", "unexpected_override_count", slot,
            slot_index);
        return;
    }

    stop_second_player_movement();
    ai_default_control(slot);
    after_ref = (int)*(int16_t *)(component + 0x16a);
    after_mode = (int)*(mode_state + 0x0b);
    if (after_ref == 0 &&
        ((character == controller_target && after_mode == 0) ||
         (character != controller_target && after_mode == 1))) {
        log_control_state("restore", "success", slot, slot_index);
        overridden_character = NULL;
        reset_native_movement_acceptance_trace();
        reset_target_trace_state();
    } else {
        log_control_state("restore", "verification_failed", slot, slot_index);
        if (after_ref == 0) {
            overridden_character = NULL;
            reset_native_movement_acceptance_trace();
        }
    }
}

static void reconcile_player_two_request(void) {
    BOOL active = overridden_character != NULL;
    BOOL target_mismatch = active && player_two_requested &&
        requested_player_two_character != NULL &&
        overridden_character != requested_player_two_character;
    DWORD now;

    if (!target_mismatch && active == player_two_requested) {
        return;
    }
    now = GetTickCount();
    if (player_two_request_last_attempt != 0u &&
        (DWORD)(now - player_two_request_last_attempt) < 250u) {
        return;
    }
    player_two_request_last_attempt = now;
    toggle_second_player_ai();
}

static void SUDEKIMP_THISCALL poll_control_separation_hotkey(
    void *controller,
    void *update_data
) {
    HWND foreground;
    DWORD foreground_process_id = 0;
    BOOL hotkey_is_down;
    BOOL owns_foreground;
    uint8_t *player_two_character = (uint8_t *)overridden_character;
    uint32_t *player_two_arbiter_flags = NULL;
    uint32_t saved_player_two_arbiter_flags;

    /* Refresh bridge liveness before the native Player 1 movement callsites
     * run, so a disconnected Player 2 can never leave Player 1 tethered for
     * an extra controller frame. */
    poll_input_bridge();
    publish_runtime_player_leases(controller);
    SudekiMpPlayerStatehoodService(
        SudekiMpPlayerStatehoodRuntime(), GetTickCount());
    update_roaming_boundary(controller);
    if (service_transition_vote_input_freeze(controller)) {
        return;
    }
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        service_second_player_controller_actions(
            controller, FALSE, TRUE, FALSE);
        quiesce_for_shared_interaction_modal();
        original_controller_update(controller, update_data);
        publish_runtime_player_leases(controller);
        SudekiMpPlayerStatehoodService(
            SudekiMpPlayerStatehoodRuntime(), GetTickCount());
        update_roaming_boundary(controller);
        if (update_observer != NULL) {
            update_observer();
        }
        return;
    }
    report_shared_interaction_modal_released();
    saved_player_two_arbiter_flags =
        begin_spirit_noncaster_arbiter_virtualization(
            player_two_requested ? player_two_character : NULL,
            &player_two_arbiter_flags
        );
    if (saved_player_two_arbiter_flags != 0u &&
        !controller_update_spirit_virtualization_logged) {
        controller_update_spirit_virtualization_logged = TRUE;
        SudekiMpLogFormat(
            "control_separation event=spirit_noncaster_movement_virtualization phase=controller_update_active character=0x%08lx arbiter_flags_before=0x%08lx virtualized_flag=0x00080000 policy=exact_player_two_native_controller_update_scope_restore_while_spirit_remains_active\r\n",
            (unsigned long)(uintptr_t)player_two_character,
            (unsigned long)saved_player_two_arbiter_flags
        );
    }
    original_controller_update(controller, update_data);
    end_spirit_noncaster_arbiter_virtualization(
        player_two_character,
        player_two_arbiter_flags,
        saved_player_two_arbiter_flags
    );
    if (saved_player_two_arbiter_flags == 0u &&
        !SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(
            player_two_character) &&
        controller_update_spirit_virtualization_logged) {
        controller_update_spirit_virtualization_logged = FALSE;
        SudekiMpLogWrite(
            "control_separation event=spirit_noncaster_movement_virtualization phase=controller_update_inactive policy=native_arbiter_state_unmodified\r\n"
        );
    }
    publish_runtime_player_leases(controller);
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        service_second_player_controller_actions(
            controller, FALSE, TRUE, FALSE);
        quiesce_for_shared_interaction_modal();
        SudekiMpPlayerStatehoodService(
            SudekiMpPlayerStatehoodRuntime(), GetTickCount());
        update_roaming_boundary(controller);
        if (update_observer != NULL) {
            update_observer();
        }
        return;
    }
    if (service_transition_vote_input_freeze(controller)) {
        return;
    }
    poll_input_bridge();
    SudekiMpCombatContextSetCharacter(
        0u,
        controller == NULL ? NULL :
            *(void **)((uint8_t *)controller + 0x248u)
    );
    SudekiMpCombatContextSetCharacter(
        1u, player_two_requested ? overridden_character : NULL);
    SudekiMpCombatContextSetInputSource(
        0u,
        controller == NULL ? SUDEKIMP_COMBAT_INPUT_NONE :
            SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER,
        controller
    );
    SudekiMpCombatContextSetInputSource(
        1u,
        !player_two_requested || overridden_character == NULL ?
            SUDEKIMP_COMBAT_INPUT_NONE :
            (input_bridge_enabled ?
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE :
                SUDEKIMP_COMBAT_INPUT_KEYBOARD_PROTOTYPE),
        !player_two_requested || overridden_character == NULL ? NULL :
            (input_bridge_enabled ?
                (void *)SudekiMpInputBridgeIdentity() :
                (void *)second_player_skill_virtual_keys)
    );
    SudekiMpCombatContextsPollGame((HMODULE)game_base);
    hotkey_is_down =
        (GetAsyncKeyState((int)selected_virtual_key) & 0x8000) != 0;
    foreground = GetForegroundWindow();
    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &foreground_process_id);
    }
    owns_foreground = foreground_process_id == GetCurrentProcessId();
    poll_roster_participation_input(owns_foreground);
    if (owns_foreground &&
        hotkey_is_down && !hotkey_was_down) {
        if (SudekiMpSplitScreenRosterParticipationAvailable()) {
            BOOL participation_requested =
                SudekiMpSplitScreenRosterParticipationRequested();
            if (!SudekiMpSplitScreenRequestRosterParticipation(
                    !participation_requested)) {
                SudekiMpLogFormat(
                    "control_separation event=player_two_request "
                    "source=hotkey status=rejected reason=roster_transition_busy "
                    "error=%lu\r\n",
                    (unsigned long)GetLastError());
            } else {
                SudekiMpLogFormat(
                    "control_separation event=player_two_request "
                    "source=hotkey state=%s "
                    "policy=retain_locked_character_identity\r\n",
                    participation_requested ? "dropped_out" :
                        "drop_in_requested");
            }
        } else if (role_lock_active) {
            SudekiMpLogWrite(
                "control_separation event=player_two_request source=hotkey "
                "status=rejected reason=co_op_roles_locked\r\n"
            );
        } else {
            requested_player_two_character = NULL;
            player_two_requested = !player_two_requested;
            player_two_request_last_attempt = 0u;
            SudekiMpLogFormat(
                "control_separation event=player_two_request source=hotkey "
                "state=%s policy=first_non_front_active_party_member\r\n",
                player_two_requested ? "enabled" : "disabled"
            );
        }
    }
    hotkey_was_down = hotkey_is_down;
    reconcile_player_two_request();
    publish_runtime_player_leases(controller);
    service_second_player_controller_actions(
        controller, owns_foreground, FALSE, FALSE);
    update_roaming_boundary(controller);
    poll_second_player_movement(controller, update_data, owns_foreground);
    poll_second_player_camera_facing(controller, owns_foreground);
    poll_second_player_weak_attack(controller, owns_foreground);
    poll_second_player_skills(controller, owns_foreground);
    poll_second_player_target_trace(owns_foreground);
    poll_shared_group_camera(controller);
    if (update_observer != NULL) {
        update_observer();
    }
}

BOOL SudekiMpControlSeparationRequestPlayerTwo(BOOL enabled) {
    if (original_controller_update == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (role_lock_active && !enabled) {
        SetLastError(ERROR_LOCK_VIOLATION);
        SudekiMpLogWrite(
            "control_separation event=player_two_request status=rejected "
            "reason=co_op_roles_locked\r\n"
        );
        return FALSE;
    }
    requested_player_two_character = NULL;
    player_two_requested = enabled != FALSE;
    player_two_request_last_attempt = 0u;
    if (!player_two_requested) {
        quiesce_second_player_input();
    }
    SudekiMpLogFormat(
        "control_separation event=player_two_request source=api state=%s\r\n",
        player_two_requested ? "enabled" : "disabled"
    );
    return TRUE;
}

BOOL SudekiMpControlSeparationRequestPlayerTwoCharacter(void *character) {
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    unsigned int slot_index;

    if (original_controller_update == NULL || game_base == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (role_lock_active &&
        (character == NULL || character != requested_player_two_character)) {
        SetLastError(ERROR_LOCK_VIOLATION);
        SudekiMpLogWrite(
            "control_separation event=player_two_request source=roster "
            "status=rejected reason=co_op_roles_locked\r\n"
        );
        return FALSE;
    }
    if ((character != NULL && player_two_requested &&
         requested_player_two_character == character) ||
        (character == NULL && !player_two_requested &&
         requested_player_two_character == NULL)) {
        if (character == NULL) {
            quiesce_second_player_input();
        }
        return TRUE;
    }
    if (character != NULL) {
        if (!readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
                sizeof(group)) ||
            !readable_memory(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
                sizeof(controller))) {
            SetLastError(ERROR_INVALID_STATE);
            return FALSE;
        }
        group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
        controller = *(uint8_t **)(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
        if (!readable_memory(group,
                PARTY_SLOT_FIRST_OFFSET +
                    PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE) ||
            !readable_memory(controller, 0x24cu)) {
            SetLastError(ERROR_NOT_FOUND);
            return FALSE;
        }
        controller_target = controller == NULL ? NULL :
            *(void **)(controller + 0x248u);
        if (group == NULL || controller_target == NULL ||
            *(void **)(group + PARTY_SLOT_FIRST_OFFSET) != controller_target ||
            find_character_party_slot(group, character, &slot_index) == NULL ||
            slot_index == 0u || character == controller_target) {
            SetLastError(ERROR_NOT_FOUND);
            return FALSE;
        }
    }
    requested_player_two_character = character;
    player_two_requested = character != NULL;
    player_two_request_last_attempt = 0u;
    if (!player_two_requested) {
        quiesce_second_player_input();
    }
    SudekiMpLogFormat(
        "control_separation event=player_two_request source=roster "
        "state=%s character=0x%08lx policy=exact_selected_party_member\r\n",
        character != NULL ? "enabled" : "disabled",
        (unsigned long)(uintptr_t)character
    );
    return TRUE;
}

BOOL SudekiMpControlSeparationReleasePlayerTwoNow(void) {
    if (original_controller_update == NULL || game_base == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (role_lock_active) {
        SetLastError(ERROR_LOCK_VIOLATION);
        return FALSE;
    }
    requested_player_two_character = NULL;
    player_two_requested = FALSE;
    player_two_request_last_attempt = 0u;
    quiesce_second_player_input();
    if (overridden_character == NULL) {
        return TRUE;
    }
    toggle_second_player_ai();
    if (overridden_character != NULL) {
        SetLastError(ERROR_BUSY);
        SudekiMpLogWrite(
            "control_separation event=player_two_transition_release "
            "status=pending reason=native_ai_restore_not_verified\r\n");
        return FALSE;
    }
    SudekiMpLogWrite(
        "control_separation event=player_two_transition_release "
        "status=confirmed policy=synchronous_game_thread_native_ai_restore\r\n");
    return TRUE;
}

BOOL SudekiMpControlSeparationSetRoleLock(BOOL enabled) {
    if (enabled && requested_player_two_character != NULL &&
        overridden_character != requested_player_two_character) {
        SetLastError(ERROR_INVALID_STATE);
        SudekiMpLogWrite(
            "control_separation event=co_op_roles state=rejected "
            "reason=selected_player_two_not_control_owned\r\n"
        );
        return FALSE;
    }
    role_lock_active = enabled != FALSE;
    SudekiMpLogFormat(
        "control_separation event=co_op_roles state=%s\r\n",
        role_lock_active ? "locked" : "unlocked"
    );
    return TRUE;
}

BOOL SudekiMpControlSeparationSetInteractionRequestsEnabled(BOOL enabled) {
    SudekiMpPlayerStatehood *statehood;
    SudekiMpPlayerStatehoodSnapshot snapshot;
    BOOL next_enabled;

    if (original_controller_update == NULL || game_base == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    next_enabled = enabled != FALSE;
    if (!next_enabled &&
        SudekiMpPlayerStatehoodGetSnapshot(
            statehood, GetTickCount(), &snapshot) &&
        snapshot.state == SUDEKIMP_INTERACTION_SESSION_REQUESTED &&
        snapshot.provenance.player_index == 1u &&
        snapshot.provenance.kind == SUDEKIMP_INTERACTION_GENERIC_REQUEST &&
        !snapshot.provenance.target_known &&
        snapshot.provenance.target == 0u) {
        SudekiMpPlayerStatehoodCancelRequest(statehood, 1u);
    }
    interaction_requests_enabled = next_enabled;
    SudekiMpLogFormat(
        "control_separation event=interaction_requests state=%s "
        "button=controller_a "
        "policy=exact_actor_target_source_generation_required_intent_only_"
        "no_targetless_controller_request_generation\r\n",
        interaction_requests_enabled ? "enabled" : "disabled");
    return TRUE;
}

BOOL SudekiMpControlSeparationPlayerTwoRequested(void) {
    return player_two_requested;
}

BOOL SudekiMpControlSeparationPlayerTwoActive(void) {
    return overridden_character != NULL;
}

void *SudekiMpControlSeparationPlayerTwoCharacter(void) {
    return overridden_character;
}

BOOL SudekiMpControlSeparationInputReady(void) {
    return input_bridge_enabled && input_bridge_connected;
}

BOOL SudekiMpControlSeparationSecondPlayerMovementActive(void) {
    return second_player_movement_active;
}

float SudekiMpControlSeparationSecondPlayerMovementMagnitude(void) {
    return second_player_movement_active ?
        second_player_movement_magnitude : 0.0f;
}

BOOL SudekiMpControlSeparationGetRoamingBoundarySnapshot(
    SudekiMpRoamingBoundaryEvaluation *snapshot
) {
    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!separation_guard_enabled) {
        ZeroMemory(snapshot, sizeof(*snapshot));
        return FALSE;
    }
    *snapshot = roaming_boundary_snapshot;
    if (!roaming_boundary_world_ready() ||
        !SudekiMpSplitScreenRuntimeEnabled() ||
        SudekiMpControlSeparationGameplayInputFrozen()) {
        ZeroMemory(snapshot, sizeof(*snapshot));
    }
    return TRUE;
}

void SudekiMpControlSeparationReportRoamingBoundaryOverlay(BOOL visible) {
    BOOL ready = separation_guard_enabled && visible &&
        roaming_boundary_snapshot.phase >=
            SUDEKIMP_ROAMING_BOUNDARY_WARNING;

    if (ready != roaming_boundary_overlay_ready) {
        roaming_boundary_overlay_ready = ready;
        SudekiMpLogFormat(
            "control_separation event=roaming_boundary_overlay state=%s policy=hard_limit_requires_visible_warning\r\n",
            ready ? "ready" : "unavailable");
    }
}

void SudekiMpControlSeparationSetUpdateObserver(
    SudekiMpControlUpdateObserver observer
) {
    update_observer = observer;
}

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT toggle_virtual_key,
    BOOL enable_second_player_movement,
    BOOL enable_camera_relative_movement,
    BOOL enable_separation_guard,
    float maximum_separation,
    BOOL enable_second_player_weak_attack,
    UINT attack_virtual_key,
    BOOL enable_second_player_skills,
    const UINT skill_virtual_keys[4],
    BOOL enable_target_trace,
    BOOL enable_shared_group_camera,
    BOOL enable_input_bridge,
    float bridge_deadzone
) {
    uint8_t *base;
    void **slot;

    if (game_module == NULL || toggle_virtual_key == 0u ||
        toggle_virtual_key > 0xffu ||
        (enable_second_player_weak_attack &&
         (attack_virtual_key == 0u || attack_virtual_key > 0xffu)) ||
        (enable_second_player_skills && skill_virtual_keys == NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_second_player_skills) {
        unsigned int index;
        for (index = 0u; index < 4u; ++index) {
            if (skill_virtual_keys[index] == 0u ||
                skill_virtual_keys[index] > 0xffu) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
        }
    }
    base = (uint8_t *)game_module;
    if (enable_camera_relative_movement && !enable_second_player_movement) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_input_bridge &&
        (!enable_second_player_movement || bridge_deadzone < 0.0f ||
         bridge_deadzone >= 0.95f ||
         SudekiMpInputBridgeIdentity() == NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_separation_guard &&
        (!enable_second_player_movement || maximum_separation <= 0.0f ||
         maximum_separation > 1000.0f)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if ((enable_separation_guard &&
         memcmp(
             base + RVA_CAMERA_MANAGER_GET_CAMERA_MODE,
             expected_camera_manager_get_camera_mode_entry,
             sizeof(expected_camera_manager_get_camera_mode_entry)) != 0) ||
        ((enable_separation_guard || enable_input_bridge) &&
         memcmp(
             base + RVA_GROUP_PLAYERS_IN_COMBAT,
             expected_group_players_in_combat_entry,
             sizeof(expected_group_players_in_combat_entry)) != 0)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_camera_relative_movement &&
        memcmp(
            base + RVA_MOVEMENT_CAMERA_TRANSFORM,
            expected_movement_camera_transform_entry,
            sizeof(expected_movement_camera_transform_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_second_player_movement &&
        memcmp(
            base + RVA_MOVEMENT_CONTROLLER_SET_ABSOLUTE_DELTA,
            expected_movement_controller_set_absolute_delta_entry,
            sizeof(expected_movement_controller_set_absolute_delta_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_second_player_movement &&
        memcmp(
            base + RVA_MOVEMENT_CONTROLLER_UPDATE,
            expected_movement_controller_update_entry,
            sizeof(expected_movement_controller_update_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_second_player_movement &&
        memcmp(
            base + RVA_TAL_CHARACTER_UPDATE,
            expected_tal_character_update_entry,
            sizeof(expected_tal_character_update_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_shared_group_camera &&
        (memcmp(
            base + RVA_CAMERA_TARGET_INSTALL,
            expected_camera_target_install_entry,
            sizeof(expected_camera_target_install_entry)) != 0 ||
         memcmp(
            base + RVA_MATRIX_TARGET_CREATE,
            expected_matrix_target_create_entry,
            sizeof(expected_matrix_target_create_entry)) != 0 ||
         memcmp(
            base + RVA_CAMERA_TARGET_RELEASE,
            expected_camera_target_release_entry,
            sizeof(expected_camera_target_release_entry)) != 0)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_second_player_weak_attack &&
        memcmp(
            base + RVA_ARBITER_COMBAT_INPUT,
            expected_arbiter_combat_input_entry,
            sizeof(expected_arbiter_combat_input_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    slot = (void **)(base + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
    game_base = base;
    selected_virtual_key = toggle_virtual_key;
    hotkey_was_down = FALSE;
    overridden_character = NULL;
    role_lock_active = FALSE;
    player_two_requested = FALSE;
    requested_player_two_character = NULL;
    player_two_request_last_attempt = 0u;
    second_player_movement_enabled = enable_second_player_movement;
    camera_relative_movement_enabled = enable_camera_relative_movement;
    separation_guard_enabled = enable_separation_guard;
    maximum_separation_distance = maximum_separation;
    ZeroMemory(&roaming_boundary_snapshot,
        sizeof(roaming_boundary_snapshot));
    roaming_boundary_candidate_since = 0u;
    roaming_boundary_last_gate = 0xffffffffu;
    ZeroMemory(roaming_boundary_player_blocked,
        sizeof(roaming_boundary_player_blocked));
    roaming_boundary_overlay_ready = FALSE;
    second_player_weak_attack_enabled = enable_second_player_weak_attack;
    weak_attack_virtual_key = attack_virtual_key;
    weak_attack_was_down = FALSE;
    input_bridge_enabled = enable_input_bridge;
    input_bridge_deadzone = bridge_deadzone;
    ZeroMemory(&input_bridge_state, sizeof(input_bridge_state));
    input_bridge_connected = FALSE;
    interaction_requests_enabled = FALSE;
    SudekiMpControllerActionRouterInitialize(&controller_action_router);
    shared_interaction_modal_quiesce_logged = FALSE;
    ZeroMemory(published_player_actors,
        sizeof(published_player_actors));
    ZeroMemory(published_player_actor_generations,
        sizeof(published_player_actor_generations));
    ZeroMemory(published_player_human_present,
        sizeof(published_player_human_present));
    SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehoodRuntime());
    input_bridge_last_right_stick_log_tick = 0u;
    input_bridge_last_right_x = 0;
    input_bridge_last_right_y = 0;
    transition_vote_input_freeze_logged = FALSE;
    transition_vote_escape_release_pending = FALSE;
    roster_join_start_was_down = FALSE;
    roster_leave_chord_since = 0u;
    roster_leave_chord_consumed = FALSE;
    second_player_skills_enabled = enable_second_player_skills;
    ZeroMemory(
        second_player_skill_virtual_keys,
        sizeof(second_player_skill_virtual_keys)
    );
    ZeroMemory(
        second_player_skill_keys_were_down,
        sizeof(second_player_skill_keys_were_down)
    );
    if (enable_second_player_skills) {
        memcpy(
            second_player_skill_virtual_keys,
            skill_virtual_keys,
            sizeof(second_player_skill_virtual_keys)
        );
    }
    target_trace_enabled = enable_target_trace;
    shared_group_camera_enabled = enable_shared_group_camera;
    matrix_target_create = (MatrixTargetCreateFunction)(
        base + RVA_MATRIX_TARGET_CREATE
    );
    camera_target_install = base + RVA_CAMERA_TARGET_INSTALL;
    camera_target_release = base + RVA_CAMERA_TARGET_RELEASE;
    group_camera_camera = NULL;
    group_camera_target = NULL;
    group_camera_original_targets[0] = NULL;
    group_camera_original_targets[1] = NULL;
    group_camera_target_list = NULL;
    reset_target_trace_state();
    second_player_movement_active = FALSE;
    second_player_movement_magnitude = 0.0f;
    last_movement_x = 0;
    last_movement_z = 0;
    reset_native_movement_acceptance_trace();
    original_controller_update = (ControllerUpdateFunction)(
        base + RVA_CONTROLLER_UPDATE
    );
    ai_override_control = (AiControlFunction)(base + RVA_AI_OVERRIDE_CONTROL);
    ai_default_control = (AiControlFunction)(base + RVA_AI_DEFAULT_CONTROL);
    arbiter_movement = (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    arbiter_set_speed = (ArbiterSetSpeedFunction)(
        base + RVA_ARBITER_SET_SPEED
    );
    camera_manager_get_camera_mode = enable_separation_guard ?
        (CameraManagerGetCameraModeFunction)(
            base + RVA_CAMERA_MANAGER_GET_CAMERA_MODE) : NULL;
    group_players_in_combat =
        (enable_separation_guard || enable_input_bridge) ?
        (GroupPlayersInCombatFunction)(
            base + RVA_GROUP_PLAYERS_IN_COMBAT) : NULL;
    movement_camera_transform = (MovementCameraTransformFunction)(
        base + RVA_MOVEMENT_CAMERA_TRANSFORM
    );
    movement_controller_set_absolute_delta =
        (MovementControllerSetAbsoluteDeltaFunction)(
            base + RVA_MOVEMENT_CONTROLLER_SET_ABSOLUTE_DELTA
        );
    spirit_direct_movement_active = FALSE;
    spirit_direct_movement_last_trace_tick = 0u;

    if (enable_second_player_movement &&
        !SudekiMpInstallInlineHook(
            &movement_controller_update_hook,
            base + RVA_MOVEMENT_CONTROLLER_UPDATE,
            expected_movement_controller_update_entry,
            sizeof(expected_movement_controller_update_entry),
            trace_movement_controller_update)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }
    if (enable_second_player_movement &&
        !SudekiMpInstallInlineHook(
            &tal_character_update_hook,
            base + RVA_TAL_CHARACTER_UPDATE,
            expected_tal_character_update_entry,
            sizeof(expected_tal_character_update_entry),
            trace_tal_character_update)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }

    if (enable_separation_guard &&
        !SudekiMpInstallRelativeCallHook(
            &player_one_alternate_movement_call_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE,
            arbiter_movement,
            enforce_player_one_roaming_boundary)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }
    if (enable_separation_guard &&
        !SudekiMpInstallRelativeCallHook(
            &player_one_normal_movement_call_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL,
            arbiter_movement,
            enforce_player_one_roaming_boundary)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }

    if (!SudekiMpInstallPointerHook(
            &controller_update_vtable_hook,
            slot,
            original_controller_update,
            poll_control_separation_hotkey)) {
        SudekiMpUninstallControlSeparation();
        return FALSE;
    }
    SudekiMpLogFormat(
        "control_separation_install=success target_policy=first_non_front_active_party_member virtual_key=0x%02lx second_player_movement=%s camera_relative_movement=%s roaming_boundary=%s roaming_boundary_policy=symmetric_p1_p2_stable_exploration_only warning_fraction_bits=0x3f4ccccd maximum_separation_bits=0x%08lx second_player_weak_attack=%s weak_attack_virtual_key=0x%02lx second_player_skills=%s skill_keys=0x%02lx,0x%02lx,0x%02lx,0x%02lx target_trace=%s shared_group_camera=%s external_input_bridge=%s bridge_deadzone_bits=0x%08lx combat_input_rva=0x000db0e0 controller_router_seats=4 controller_contract=xbox_a_context_or_weak_x_strong_y_quick_menu_intent_b_modal_cancel_or_combat_sweep_dpad_quickshot_intent\r\n",
        (unsigned long)selected_virtual_key,
        second_player_movement_enabled ? "true" : "false",
        camera_relative_movement_enabled ? "true" : "false",
        separation_guard_enabled ? "true" : "false",
        (unsigned long)float_bits(maximum_separation_distance),
        second_player_weak_attack_enabled ? "true" : "false",
        (unsigned long)weak_attack_virtual_key,
        second_player_skills_enabled ? "true" : "false",
        (unsigned long)second_player_skill_virtual_keys[0],
        (unsigned long)second_player_skill_virtual_keys[1],
        (unsigned long)second_player_skill_virtual_keys[2],
        (unsigned long)second_player_skill_virtual_keys[3],
        target_trace_enabled ? "true" : "false",
        shared_group_camera_enabled ? "true" : "false",
        input_bridge_enabled ? "true" : "false",
        (unsigned long)float_bits(input_bridge_deadzone)
    );
    return TRUE;
}

void SudekiMpUninstallControlSeparation(void) {
    SudekiMpRestorePointerHook(&controller_update_vtable_hook);
    SudekiMpRestoreRelativeCallHook(
        &player_one_normal_movement_call_hook);
    SudekiMpRestoreRelativeCallHook(
        &player_one_alternate_movement_call_hook);
    SudekiMpRestoreInlineHook(&tal_character_update_hook);
    SudekiMpRestoreInlineHook(&movement_controller_update_hook);
    restore_group_camera("module_uninstall");
    original_controller_update = NULL;
    ai_override_control = NULL;
    ai_default_control = NULL;
    arbiter_movement = NULL;
    arbiter_set_speed = NULL;
    camera_manager_get_camera_mode = NULL;
    group_players_in_combat = NULL;
    movement_camera_transform = NULL;
    movement_controller_set_absolute_delta = NULL;
    spirit_direct_movement_active = FALSE;
    spirit_direct_movement_last_trace_tick = 0u;
    game_base = NULL;
    overridden_character = NULL;
    role_lock_active = FALSE;
    player_two_requested = FALSE;
    requested_player_two_character = NULL;
    player_two_request_last_attempt = 0u;
    selected_virtual_key = 0;
    hotkey_was_down = FALSE;
    second_player_movement_enabled = FALSE;
    camera_relative_movement_enabled = FALSE;
    separation_guard_enabled = FALSE;
    maximum_separation_distance = 0.0f;
    ZeroMemory(&roaming_boundary_snapshot,
        sizeof(roaming_boundary_snapshot));
    roaming_boundary_candidate_since = 0u;
    roaming_boundary_last_gate = 0xffffffffu;
    ZeroMemory(roaming_boundary_player_blocked,
        sizeof(roaming_boundary_player_blocked));
    roaming_boundary_overlay_ready = FALSE;
    second_player_weak_attack_enabled = FALSE;
    weak_attack_virtual_key = 0;
    weak_attack_was_down = FALSE;
    input_bridge_enabled = FALSE;
    input_bridge_deadzone = 0.0f;
    ZeroMemory(&input_bridge_state, sizeof(input_bridge_state));
    input_bridge_connected = FALSE;
    interaction_requests_enabled = FALSE;
    SudekiMpControllerActionRouterInitialize(&controller_action_router);
    shared_interaction_modal_quiesce_logged = FALSE;
    ZeroMemory(published_player_actors,
        sizeof(published_player_actors));
    ZeroMemory(published_player_actor_generations,
        sizeof(published_player_actor_generations));
    ZeroMemory(published_player_human_present,
        sizeof(published_player_human_present));
    SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehoodRuntime());
    input_bridge_last_right_stick_log_tick = 0u;
    input_bridge_last_right_x = 0;
    input_bridge_last_right_y = 0;
    transition_vote_input_freeze_logged = FALSE;
    transition_vote_escape_release_pending = FALSE;
    roster_join_start_was_down = FALSE;
    roster_leave_chord_since = 0u;
    roster_leave_chord_consumed = FALSE;
    second_player_skills_enabled = FALSE;
    ZeroMemory(
        second_player_skill_virtual_keys,
        sizeof(second_player_skill_virtual_keys)
    );
    ZeroMemory(
        second_player_skill_keys_were_down,
        sizeof(second_player_skill_keys_were_down)
    );
    target_trace_enabled = FALSE;
    shared_group_camera_enabled = FALSE;
    matrix_target_create = NULL;
    camera_target_install = NULL;
    camera_target_release = NULL;
    group_camera_camera = NULL;
    group_camera_target = NULL;
    group_camera_original_targets[0] = NULL;
    group_camera_original_targets[1] = NULL;
    group_camera_target_list = NULL;
    reset_target_trace_state();
    second_player_movement_active = FALSE;
    second_player_movement_magnitude = 0.0f;
    last_movement_x = 0;
    last_movement_z = 0;
    reset_native_movement_acceptance_trace();
    update_observer = NULL;
    SudekiMpCombatContextsReset();
}
