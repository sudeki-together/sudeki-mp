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
#include "hooks/interaction_provenance.h"
#include "hooks/split_screen_render.h"
#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"
#include "input/local_input_hub.h"
#include "network/lan_arena_authority.h"
#include "network/lan_arena_tal_combo_graph.h"

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
typedef void (SUDEKIMP_THISCALL *MovementControllerSetSpeedImmediateFunction)(
    void *movement_controller,
    float speed,
    float turn_rate
);
typedef void (SUDEKIMP_THISCALL *GameSpeedPlayerInputEnableFunction)(
    void *seat_index
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
typedef void (__attribute__((regparm(1), stdcall))
    *MovementRelativeDeltaFunction)(const float *delta, void *controller);
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
typedef uint8_t (SUDEKIMP_THISCALL *MissileManagerPredicateFunction)(
    const void *missile_manager
);
enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_GAME_SPEED_GLOBAL = 0x00408da0u,
    RVA_GAME_SPEED_PLAYER_INPUT_ENABLE = 0x00027120u,
    RVA_CONTROLLER_FILTER_ALL = 0x00008ae0u,
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
    RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE = 0x000c3870u,
    RVA_TAL_CHARACTER_UPDATE = 0x00153240u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_ARBITER_SET_SPEED = 0x000db070u,
    RVA_POSITION_SET_FORWARD = 0x001114d0u,
    RVA_ANIMATION_ROOT_MOVEMENT_CALL = 0x000e1a98u,
    RVA_MOVEMENT_RELATIVE_DELTA = 0x000c3650u,
    RVA_GROUP_PLAYERS_IN_COMBAT = 0x00004fa0u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_ARBITER_COMBAT_INPUT = 0x000db0e0u,
    RVA_FIRST_PERSON_HELD_FIRE = 0x00134410u,
    RVA_MISSILE_MANAGER_CAN_FIRE = 0x000c79a0u,
    RVA_MISSILE_MANAGER_IS_FIRING = 0x000c7990u,
    RVA_MISSILE_MANAGER_VTABLE = 0x002d4c8cu,
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
    ROAMING_BOUNDARY_SETTLE_MS = 250u,
    UPDATE_OBSERVER_CAPACITY = 4u,
    CONTROL_COMPANION_FIRST_SEAT = 1u,
    CONTROL_COMPANION_LAST_SEAT = 2u,
    CONTROL_COMPANION_COUNT = 2u,
    CONTROL_PUBLISHED_PLAYER_COUNT = 3u
};

typedef struct SudekiMpCompanionControlRuntime {
    BOOL requested;
    void *requested_character;
    DWORD request_last_attempt;
    void *character;
    void *ai_component;
    BOOL lease_exact;
    SudekiMpInputBridgeState input_state;
    BOOL input_connected;
    const void *input_identity;
    uint32_t input_generation;
    const void *leased_input_identity;
    uint32_t leased_input_generation;
    BOOL movement_active;
    float movement_magnitude;
    int movement_input_x;
    int movement_input_z;
    BOOL keyboard_weak_was_down;
} SudekiMpCompanionControlRuntime;

typedef struct ControlUpdateObserverEntry {
    const void *owner;
    SudekiMpControlUpdateObserver observer;
} ControlUpdateObserverEntry;

typedef struct ControlUpdateDispatchFrame {
    struct ControlUpdateDispatchFrame *previous;
    const SudekiMpControlUpdateDispatchWitness *active_witness;
    uint64_t dispatch_serial;
    DWORD native_thread_id;
    uint32_t update_depth;
    uint32_t overlap_generation;
    uint32_t original_call_count;
    uint8_t tls_exact;
    uint8_t service_only;
    uint8_t reentrancy_seen;
    uint8_t reserved;
} ControlUpdateDispatchFrame;

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
static SudekiMpRelativeCallHook lan_spirit_root_movement_call_hook;
static MovementRelativeDeltaFunction original_animation_root_movement;
static void *lan_spirit_direct_actor;
static DWORD lan_spirit_direct_at_ms;
static void *lan_tal_skill_direct_actor;
static DWORD lan_tal_skill_direct_at_ms;
static SudekiMpInlineHook movement_controller_update_hook;
static SudekiMpInlineHook tal_character_update_hook;
static ControllerUpdateFunction original_controller_update;
static AiControlFunction ai_override_control;
static AiControlFunction ai_default_control;
static ArbiterMovementFunction arbiter_movement;
static void *position_set_forward;
static void *lan_arena_first_person_held_fire;
static MissileManagerPredicateFunction missile_manager_can_fire;
static MissileManagerPredicateFunction missile_manager_is_firing;
static void *lan_arena_cached_missile_manager;
static void *lan_arena_cached_missile_owner;
static int lan_arena_missile_ready_trace_state = -1;
static int lan_arena_ranged_fire_validation_state = -1;
static ArbiterSetSpeedFunction arbiter_set_speed;
static MovementControllerSetSpeedImmediateFunction
    movement_controller_set_speed_immediate;
static GameSpeedPlayerInputEnableFunction game_speed_player_input_enable;
static CameraManagerGetCameraModeFunction camera_manager_get_camera_mode;
static GroupPlayersInCombatFunction group_players_in_combat;
static MovementCameraTransformFunction movement_camera_transform;
static MovementControllerSetAbsoluteDeltaFunction
    movement_controller_set_absolute_delta;
static uint8_t *game_base;
static SudekiMpCompanionControlRuntime
    companion_controls[CONTROL_COMPANION_COUNT];
#define overridden_character (companion_controls[0].character)
#define overridden_ai_component (companion_controls[0].ai_component)
#define player_two_requested (companion_controls[0].requested)
#define requested_player_two_character \
    (companion_controls[0].requested_character)
#define player_two_request_last_attempt \
    (companion_controls[0].request_last_attempt)
#define input_bridge_state (companion_controls[0].input_state)
#define input_bridge_connected (companion_controls[0].input_connected)
#define second_player_movement_active \
    (companion_controls[0].movement_active)
#define second_player_movement_magnitude \
    (companion_controls[0].movement_magnitude)
#define last_movement_x (companion_controls[0].movement_input_x)
#define last_movement_z (companion_controls[0].movement_input_z)
#define weak_attack_was_down (companion_controls[0].keyboard_weak_was_down)
static UINT selected_virtual_key;
static BOOL hotkey_was_down;
static BOOL manual_toggle_enabled;
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
static BOOL second_player_skills_enabled;
static UINT second_player_skill_virtual_keys[4];
static BOOL second_player_skill_keys_were_down[4];
static BOOL target_trace_enabled;
static DWORD target_trace_last_sample_tick;
static void *target_trace_last_node;
static int target_trace_last_auto_enabled;
static int last_native_movement_acceptance = -1;
static unsigned int last_native_movement_gate = 0xffffffffu;
static DWORD last_movement_pipeline_sample_tick;
static BOOL last_movement_pipeline_position_valid;
static float last_movement_pipeline_position[3];
static BOOL controller_update_spirit_virtualization_logged;
static BOOL player_one_skill_input_isolation_enabled;
static BOOL player_one_skill_native_input_restored;
static int player_one_skill_input_isolation_trace_state = -1;
static BOOL player_one_skill_arbiter_virtualization_logged;
static BOOL player_one_skill_direct_movement_scope_active;
static BOOL player_one_skill_direct_movement_submitted;
static BOOL player_one_skill_direct_movement_operator_override;
static float player_one_skill_frame_delta;
static DWORD player_one_skill_direct_movement_last_trace_tick;
static SudekiMpLanArenaPlayerOneSkillDirectionOverride
    player_one_skill_direction_override;
static BOOL lan_arena_player_two_skill_input_isolation_enabled;
static BOOL lan_arena_player_two_skill_virtualization_logged;
static DWORD lan_arena_player_two_skill_direct_movement_last_trace_tick;
static BOOL spirit_direct_movement_active;
static DWORD spirit_direct_movement_last_trace_tick;
static DWORD movement_controller_update_last_trace_tick;
static DWORD tal_character_update_last_trace_tick;
static BOOL second_player_facing_valid;
static float second_player_last_facing[3];
static BOOL input_bridge_enabled;
static BOOL lan_arena_remote_input_enabled;
static float input_bridge_deadzone;
static BOOL interaction_requests_enabled;
static SudekiMpControllerActionRouter controller_action_router;
static BOOL shared_interaction_modal_quiesce_logged;
static void *published_player_actors[CONTROL_PUBLISHED_PLAYER_COUNT];
static uint32_t
    published_player_actor_generations[CONTROL_PUBLISHED_PLAYER_COUNT];
static BOOL published_player_human_present[CONTROL_PUBLISHED_PLAYER_COUNT];
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
static BOOL role_lock_active;
static BOOL fixed_three_release_deferred_logged;
static BOOL service_only_mode;
static volatile LONG control_update_lifecycle_lock;
static BOOL control_update_wrapper_enabled;
static ControllerUpdateFunction retained_original_controller_update;
static volatile LONG update_observer_registry_lock;
static ControlUpdateObserverEntry update_observers[UPDATE_OBSERVER_CAPACITY];
static uint32_t update_observer_registry_generation;
static DWORD control_update_dispatch_tls = TLS_OUT_OF_INDEXES;
static volatile LONG control_update_dispatch_serial;
static volatile LONG active_control_update_dispatches;
static volatile LONG control_update_overlap_generation;
static BOOL control_separation_containment_pinned;
static BOOL control_separation_restore_failure_logged;

static void SUDEKIMP_THISCALL service_control_update_observers(
    void *controller,
    void *update_data
);
static void SUDEKIMP_THISCALL poll_control_separation_hotkey(
    void *controller,
    void *update_data
);

static const uint8_t expected_controller_update_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec,
    0x24, 0x53, 0x8b, 0xd9, 0x80, 0xbb, 0xc7, 0x01
};
static const uint8_t expected_game_speed_player_input_enable_entry[] = {
    0x83, 0xec, 0x0c, 0x85, 0xc0, 0x74, 0x52, 0x8d,
    0x4c, 0x49, 0x24
};
static const uint8_t expected_controller_filter_all[] = {
    0x56,0x8b,0xf1,0xc7,0x81,0x84,0x00,0x00,0x00,0x01,
    0x00,0x00,0x00,0xe8,0xde,0x05,0x02,0x00,0x5e,0xc3
};
static BOOL tal_skill_direct_actor_exact(uint8_t *character);
static const uint8_t expected_arbiter_combat_input_entry[] = {
    0x55, 0x8b, 0x6c, 0x24, 0x08, 0x56, 0x57, 0x8b, 0xf8, 0x8b, 0xf1
};
static const uint8_t expected_arbiter_set_speed_entry[] = {
    0x8b, 0x41, 0x10, 0x56, 0x8b, 0xb0, 0x80, 0x00,
    0x00, 0x00, 0x85, 0xf6, 0x74, 0x5e, 0x8b, 0xd1
};
static const uint8_t expected_position_set_forward_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x83, 0xec,
    0x60, 0xd9, 0xee, 0xd9, 0x54, 0x24, 0x14
};
static const uint8_t expected_first_person_held_fire_entry[] = {
    0x83, 0x78, 0x2c, 0x00, 0x56, 0x74, 0x29, 0x8b,
    0x40, 0x2c, 0x8b, 0x70, 0x70, 0x85, 0xf6, 0x74,
    0x1f, 0x8b, 0x46, 0x3c, 0x85, 0xc0, 0x74, 0x18,
    0x83, 0x78, 0x50, 0x08, 0x0f, 0x94, 0xc0, 0x84,
    0xc0, 0x74, 0x0d, 0xe8, 0x48, 0x59, 0xf9, 0xff,
    0x84, 0xc0, 0x74, 0x04, 0xb0, 0x01, 0x5e, 0xc3
};
static const uint8_t expected_missile_manager_can_fire_entry[] = {
    0x8au, 0x81u, 0xe0u, 0x00u, 0x00u, 0x00u, 0x84u, 0xc0u,
    0x74u, 0x07u, 0x3cu, 0x06u, 0x74u, 0x03u, 0x33u, 0xc0u,
    0xc3u, 0xb8u, 0x01u, 0x00u, 0x00u, 0x00u, 0xc3u
};
static const uint8_t expected_missile_manager_is_firing_entry[] = {
    0x33u, 0xc0u, 0x80u, 0xb9u, 0xe0u, 0x00u, 0x00u, 0x00u,
    0x02u, 0x0fu, 0x94u, 0xc0u, 0xc3u
};
static const uint8_t expected_movement_controller_set_speed_immediate_entry[] = {
    0x83, 0x79, 0x68, 0x01, 0x75, 0x15, 0xd9, 0xe8,
    0xd8, 0x54, 0x24, 0x04, 0xdf, 0xe0, 0xf6, 0xc4,
    0x41, 0x74, 0x06, 0xd9, 0x5c, 0x24, 0x04, 0xeb,
    0x02, 0xdd, 0xd8, 0xf6, 0x81, 0xbe, 0x00, 0x00,
    0x00, 0x08, 0x74, 0x11, 0xd9, 0x44, 0x24, 0x04,
    0xd9, 0x51, 0x24, 0xd9, 0x59, 0x28, 0xd9, 0x44,
    0x24, 0x08, 0xd9, 0x59, 0x30, 0xc2, 0x08, 0x00
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

static SudekiMpCompanionControlRuntime *companion_control_for_seat(
    unsigned int seat_index
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT) {
        return NULL;
    }
    return &companion_controls[seat_index - CONTROL_COMPANION_FIRST_SEAT];
}

BOOL SudekiMpControlSeparationSeatSubmissionReadyPolicy(
    unsigned int seat_index,
    BOOL requested,
    BOOL active,
    BOOL input_ready,
    BOOL seat_view_ready
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT ||
        !requested || !active || !input_ready) {
        return FALSE;
    }
    /* The established P2 path may still use its exact native P1-camera
     * fallback.  P3 has no such proven fallback and must wait for a distinct
     * camera/render-state lease from the compositor. */
    return seat_index == CONTROL_COMPANION_FIRST_SEAT || seat_view_ready;
}

BOOL SudekiMpControlSeparationSeatRequestTransitionPolicy(
    unsigned int seat_index,
    BOOL enabling,
    BOOL actor_changed,
    BOOL player_two_exact_requested,
    BOOL player_three_present
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT) {
        return FALSE;
    }
    if (seat_index == 1u) {
        if (!enabling && player_three_present) {
            return FALSE;
        }
        return !actor_changed || !player_three_present;
    }
    return !enabling || player_two_exact_requested;
}

BOOL SudekiMpControlSeparationSeatAcquireOrderPolicy(
    unsigned int seat_index,
    BOOL player_two_exact_active
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT) {
        return FALSE;
    }
    return seat_index == 1u || player_two_exact_active;
}

BOOL SudekiMpControlSeparationSeatReleaseOrderPolicy(
    unsigned int seat_index,
    BOOL player_three_owned
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT) {
        return FALSE;
    }
    return seat_index == 2u || !player_three_owned;
}

BOOL SudekiMpControlSeparationDeferReleaseToRosterPolicy(
    BOOL fixed_three_contract,
    BOOL role_lock_active_value,
    BOOL release_required
) {
    return fixed_three_contract != FALSE &&
        role_lock_active_value != FALSE && release_required != FALSE;
}

BOOL SudekiMpControlSeparationSeatInputLeaseExactPolicy(
    const void *leased_identity,
    uint32_t leased_generation,
    const void *current_identity,
    uint32_t current_generation
) {
    return leased_identity != NULL && current_identity != NULL &&
        leased_generation != 0u && current_generation != 0u &&
        leased_identity == current_identity &&
        leased_generation == current_generation;
}

BOOL SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
    unsigned int seat_index,
    BOOL fixed_three_contract,
    BOOL current_input_ready
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT) {
        return FALSE;
    }
    return seat_index == CONTROL_COMPANION_FIRST_SEAT &&
            !fixed_three_contract ?
        TRUE : current_input_ready != FALSE;
}

BOOL SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
    BOOL player_two_input_ready,
    BOOL player_three_input_ready
) {
    return player_two_input_ready != FALSE &&
        player_three_input_ready != FALSE;
}

BOOL SudekiMpControlSeparationSeatActiveInputLeasePolicy(
    unsigned int seat_index,
    BOOL fixed_three_contract,
    BOOL current_input_ready,
    const void *leased_identity,
    uint32_t leased_generation,
    const void *current_identity,
    uint32_t current_generation
) {
    if (seat_index < CONTROL_COMPANION_FIRST_SEAT ||
        seat_index > CONTROL_COMPANION_LAST_SEAT ||
        !current_input_ready) {
        return FALSE;
    }
    if (seat_index == CONTROL_COMPANION_FIRST_SEAT &&
        !fixed_three_contract) {
        return TRUE;
    }
    return SudekiMpControlSeparationSeatInputLeaseExactPolicy(
        leased_identity,
        leased_generation,
        current_identity,
        current_generation);
}

BOOL SudekiMpControlSeparationPlayerOneSkillInputIsolationPolicy(
    BOOL enabled,
    int current_mode,
    int requested_mode,
    BOOL paused
) {
    (void)requested_mode;
    return enabled != FALSE && paused == FALSE && current_mode == 2;
}

BOOL SudekiMpControlSeparationPlayerOneSkillDirectionOverridePolicy(
    BOOL physical_direction_held,
    BOOL operator_direction_held
) {
    return physical_direction_held == FALSE &&
        operator_direction_held != FALSE;
}

void SudekiMpControlSeparationSetLanArenaPlayerOneSkillDirectionOverride(
    SudekiMpLanArenaPlayerOneSkillDirectionOverride source
) {
    player_one_skill_direction_override = source;
}

static void acquire_update_observer_registry(void) {
    while (InterlockedCompareExchange(
            &update_observer_registry_lock, 1, 0) != 0) {
        SwitchToThread();
    }
}

static void acquire_control_update_lifecycle(void) {
    while (InterlockedCompareExchange(
            &control_update_lifecycle_lock, 1, 0) != 0) {
        SwitchToThread();
    }
}

static void release_control_update_lifecycle(void) {
    InterlockedExchange(&control_update_lifecycle_lock, 0);
}

static void release_update_observer_registry(void) {
    InterlockedExchange(&update_observer_registry_lock, 0);
}

BOOL SudekiMpControlUpdateObserverGateEnable(
    SudekiMpControlUpdateObserverGate *gate
) {
    DWORD saved_error = GetLastError();

    if (gate == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (InterlockedCompareExchange(&gate->active_entries, 0, 0) != 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (InterlockedCompareExchange(&gate->enabled, 1, 0) != 0) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    SetLastError(saved_error);
    return TRUE;
}

BOOL SudekiMpControlUpdateObserverGateTryEnter(
    SudekiMpControlUpdateObserverGate *gate
) {
    DWORD saved_error = GetLastError();
    BOOL entered = FALSE;

    if (gate != NULL &&
        InterlockedCompareExchange(&gate->enabled, 0, 0) != 0) {
        (void)InterlockedIncrement(&gate->active_entries);
        if (InterlockedCompareExchange(&gate->enabled, 0, 0) != 0) {
            entered = TRUE;
        } else {
            (void)InterlockedDecrement(&gate->active_entries);
        }
    }
    SetLastError(saved_error);
    return entered;
}

void SudekiMpControlUpdateObserverGateLeave(
    SudekiMpControlUpdateObserverGate *gate
) {
    DWORD saved_error = GetLastError();

    if (gate != NULL) {
        (void)InterlockedDecrement(&gate->active_entries);
    }
    SetLastError(saved_error);
}

void SudekiMpControlUpdateObserverGateDisable(
    SudekiMpControlUpdateObserverGate *gate
) {
    DWORD saved_error = GetLastError();

    if (gate != NULL) InterlockedExchange(&gate->enabled, 0);
    SetLastError(saved_error);
}

void SudekiMpControlUpdateObserverGateDrain(
    SudekiMpControlUpdateObserverGate *gate
) {
    DWORD saved_error = GetLastError();

    if (gate != NULL) {
        while (InterlockedCompareExchange(
                &gate->active_entries, 0, 0) != 0) {
            SwitchToThread();
        }
    }
    SetLastError(saved_error);
}

static void advance_update_observer_registry_generation(void) {
    ++update_observer_registry_generation;
    if (update_observer_registry_generation == 0u) {
        ++update_observer_registry_generation;
    }
}

static BOOL ensure_control_update_dispatch_tls(void) {
    DWORD tls_index;

    if (control_update_dispatch_tls != TLS_OUT_OF_INDEXES) return TRUE;
    tls_index = TlsAlloc();
    if (tls_index == TLS_OUT_OF_INDEXES) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    control_update_dispatch_tls = tls_index;
    return TRUE;
}

static uint64_t next_control_update_dispatch_serial(void) {
    uint32_t serial = (uint32_t)InterlockedIncrement(
        &control_update_dispatch_serial);

    if (serial == 0u) {
        serial = (uint32_t)InterlockedIncrement(
            &control_update_dispatch_serial);
    }
    return (uint64_t)serial;
}

static void begin_control_update_dispatch(
    ControlUpdateDispatchFrame *frame,
    BOOL service_only
) {
    DWORD saved_error = GetLastError();
    LONG active_count;
    LONG overlap_before;
    DWORD tls_error = ERROR_SUCCESS;
    ControlUpdateDispatchFrame *previous = NULL;

    ZeroMemory(frame, sizeof(*frame));
    frame->dispatch_serial = next_control_update_dispatch_serial();
    frame->native_thread_id = GetCurrentThreadId();
    frame->service_only = service_only != FALSE ? 1u : 0u;
    overlap_before = InterlockedCompareExchange(
        &control_update_overlap_generation, 0, 0);
    active_count = InterlockedIncrement(&active_control_update_dispatches);
    if (active_count != 1) {
        (void)InterlockedIncrement(&control_update_overlap_generation);
        frame->reentrancy_seen = 1u;
    }
    frame->overlap_generation = (uint32_t)overlap_before;

    if (control_update_dispatch_tls != TLS_OUT_OF_INDEXES) {
        SetLastError(ERROR_SUCCESS);
        previous = (ControlUpdateDispatchFrame *)TlsGetValue(
            control_update_dispatch_tls);
        tls_error = GetLastError();
        if (previous != NULL) {
            previous->reentrancy_seen = 1u;
            frame->reentrancy_seen = 1u;
        }
        if (tls_error == ERROR_SUCCESS) {
            frame->previous = previous;
            frame->update_depth = previous == NULL ? 1u :
                previous->update_depth + 1u;
            if (TlsSetValue(control_update_dispatch_tls, frame)) {
                frame->tls_exact = 1u;
            }
        }
    }
    SetLastError(saved_error);
}

static void end_control_update_dispatch(ControlUpdateDispatchFrame *frame) {
    DWORD saved_error = GetLastError();

    if (frame != NULL && frame->tls_exact != 0u &&
        control_update_dispatch_tls != TLS_OUT_OF_INDEXES) {
        (void)TlsSetValue(control_update_dispatch_tls, frame->previous);
    }
    (void)InterlockedDecrement(&active_control_update_dispatches);
    SetLastError(saved_error);
}

static BOOL begin_owned_control_update_dispatch(
    ControlUpdateDispatchFrame *frame,
    BOOL service_only,
    ControllerUpdateFunction *fallback_original
) {
    DWORD saved_error = GetLastError();
    void *expected_replacement = service_only ?
        (void *)service_control_update_observers :
        (void *)poll_control_separation_hotkey;
    BOOL admitted;

    acquire_control_update_lifecycle();
    *fallback_original = retained_original_controller_update;
    admitted = control_update_wrapper_enabled &&
        controller_update_vtable_hook.installed != FALSE &&
        controller_update_vtable_hook.replacement_value ==
            expected_replacement &&
        original_controller_update != NULL &&
        service_only_mode == (service_only != FALSE);
    if (admitted) begin_control_update_dispatch(frame, service_only);
    release_control_update_lifecycle();
    SetLastError(saved_error);
    return admitted;
}

static BOOL control_update_dispatch_frame_current(
    const ControlUpdateDispatchFrame *frame
) {
    DWORD saved_error;
    DWORD tls_error;
    const ControlUpdateDispatchFrame *current;

    if (frame == NULL || frame->tls_exact == 0u ||
        control_update_dispatch_tls == TLS_OUT_OF_INDEXES) return FALSE;
    saved_error = GetLastError();
    SetLastError(ERROR_SUCCESS);
    current = (const ControlUpdateDispatchFrame *)TlsGetValue(
        control_update_dispatch_tls);
    tls_error = GetLastError();
    SetLastError(saved_error);
    return tls_error == ERROR_SUCCESS && current == frame;
}

static void control_update_hook_ownership(
    const ControlUpdateDispatchFrame *frame,
    BOOL *hook_owned,
    BOOL *slot_owned
) {
    void *expected_replacement;
    void **expected_slot;
    BOOL hook_exact;

    *hook_owned = FALSE;
    *slot_owned = FALSE;
    if (frame == NULL || game_base == NULL) return;
    expected_replacement = frame->service_only != 0u ?
        (void *)service_control_update_observers :
        (void *)poll_control_separation_hotkey;
    expected_slot = (void **)(game_base +
        RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
    hook_exact = controller_update_vtable_hook.installed != FALSE &&
        controller_update_vtable_hook.slot == expected_slot &&
        controller_update_vtable_hook.original_value ==
            game_base + RVA_CONTROLLER_UPDATE &&
        controller_update_vtable_hook.replacement_value ==
            expected_replacement &&
        original_controller_update == (ControllerUpdateFunction)(
            game_base + RVA_CONTROLLER_UPDATE) &&
        service_only_mode == (frame->service_only != 0u);
    *hook_owned = hook_exact;
    *slot_owned = hook_exact && *expected_slot == expected_replacement;
}

static BOOL update_observer_registry_generation_is(uint32_t generation) {
    BOOL equal;

    acquire_update_observer_registry();
    equal = update_observer_registry_generation == generation;
    release_update_observer_registry();
    return equal;
}

static BOOL control_update_source_shape_exact(
    const ControlUpdateDispatchFrame *frame,
    SudekiMpControlUpdateDispatchSource source
) {
    if (frame == NULL) return FALSE;
    if (source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL) {
        return frame->service_only != 0u &&
            frame->original_call_count == 1u;
    }
    if (source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_PRE_ORIGINAL) {
        return frame->service_only == 0u &&
            frame->original_call_count == 0u;
    }
    if (source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL) {
        return frame->service_only == 0u &&
            frame->original_call_count == 1u;
    }
    return FALSE;
}

static void build_control_update_dispatch_witness(
    const ControlUpdateDispatchFrame *frame,
    SudekiMpControlUpdateDispatchSource source,
    uint32_t observer_count,
    uint32_t registry_generation,
    SudekiMpControlUpdateDispatchWitness *witness
) {
    LONG active_count_start;
    LONG active_count_end;
    LONG overlap_generation_start;
    LONG overlap_generation_end;
    BOOL hook_owned_start;
    BOOL hook_owned_end;
    BOOL slot_owned_start;
    BOOL slot_owned_end;
    BOOL registry_stable_start;
    BOOL registry_stable_end;
    BOOL source_shape_start;
    BOOL source_shape_end;
    BOOL frame_current_start;
    BOOL frame_current_end;
    BOOL frame_exact;

    ZeroMemory(witness, sizeof(*witness));
    overlap_generation_start = InterlockedCompareExchange(
        &control_update_overlap_generation, 0, 0);
    active_count_start = InterlockedCompareExchange(
        &active_control_update_dispatches, 0, 0);
    control_update_hook_ownership(
        frame, &hook_owned_start, &slot_owned_start);
    registry_stable_start =
        update_observer_registry_generation_is(registry_generation);
    source_shape_start = control_update_source_shape_exact(frame, source);
    frame_current_start = control_update_dispatch_frame_current(frame);

    control_update_hook_ownership(
        frame, &hook_owned_end, &slot_owned_end);
    registry_stable_end =
        update_observer_registry_generation_is(registry_generation);
    source_shape_end = control_update_source_shape_exact(frame, source);
    frame_current_end = control_update_dispatch_frame_current(frame);
    active_count_end = InterlockedCompareExchange(
        &active_control_update_dispatches, 0, 0);
    overlap_generation_end = InterlockedCompareExchange(
        &control_update_overlap_generation, 0, 0);

    witness->dispatch_serial = frame == NULL ? 0u :
        frame->dispatch_serial;
    witness->native_thread_id = frame == NULL ? 0u :
        (uint32_t)frame->native_thread_id;
    witness->outer_update_depth = frame == NULL ||
        frame->tls_exact == 0u ? 0u : frame->update_depth;
    witness->active_dispatch_count = active_count_end <= 0 ? 0u :
        (uint32_t)active_count_end;
    witness->original_call_count = frame == NULL ? 0u :
        frame->original_call_count;
    witness->observer_snapshot_count = observer_count;
    witness->observer_registry_generation = registry_generation;
    witness->dispatch_overlap_generation =
        (uint32_t)overlap_generation_end;
    witness->hook_owned_exact =
        hook_owned_start && hook_owned_end ? 1u : 0u;
    witness->slot_owned_exact =
        slot_owned_start && slot_owned_end ? 1u : 0u;
    witness->service_only = frame != NULL && frame->service_only != 0u ?
        1u : 0u;
    witness->source = (uint8_t)source;
    witness->post_original =
        source == SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL ||
        source == SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL ?
            1u : 0u;
    witness->sole_observer = observer_count == 1u ? 1u : 0u;
    witness->registry_generation_stable =
        registry_stable_start && registry_stable_end ? 1u : 0u;

    frame_exact = frame != NULL && frame->dispatch_serial != 0u &&
        frame->native_thread_id == GetCurrentThreadId() &&
        frame->update_depth == 1u && frame->reentrancy_seen == 0u &&
        active_count_start == 1 && active_count_end == 1 &&
        overlap_generation_start == overlap_generation_end &&
        (uint32_t)overlap_generation_start == frame->overlap_generation &&
        frame_current_start && frame_current_end &&
        hook_owned_start && hook_owned_end &&
        slot_owned_start && slot_owned_end;
    witness->source_exact = frame_exact &&
        source_shape_start && source_shape_end ? 1u : 0u;
    witness->service_post_original_exact =
        witness->source_exact != 0u &&
        source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL ?
                1u : 0u;
}

static void notify_update_observers(
    void *controller,
    void *update_data,
    ControlUpdateDispatchFrame *frame,
    SudekiMpControlUpdateDispatchSource source
) {
    SudekiMpControlUpdateObserver snapshot[UPDATE_OBSERVER_CAPACITY];
    DWORD incoming_last_error = GetLastError();
    uint32_t registry_generation;
    uint32_t observer_count = 0u;
    unsigned int index;

    /* Snapshot outside callback execution so an observer may unregister
     * itself without changing which callbacks belong to this native update. */
    acquire_update_observer_registry();
    for (index = 0u; index < UPDATE_OBSERVER_CAPACITY; ++index) {
        snapshot[index] = update_observers[index].observer;
        if (snapshot[index] != NULL) ++observer_count;
    }
    registry_generation = update_observer_registry_generation;
    release_update_observer_registry();
    for (index = 0u; index < UPDATE_OBSERVER_CAPACITY; ++index) {
        if (snapshot[index] != NULL) {
            SudekiMpControlUpdateDispatchWitness witness;

            build_control_update_dispatch_witness(
                frame, source, observer_count, registry_generation, &witness);
            if (frame != NULL) frame->active_witness = &witness;
            SetLastError(incoming_last_error);
            snapshot[index](controller, update_data, &witness);
            if (frame != NULL && frame->active_witness == &witness) {
                frame->active_witness = NULL;
            }
            SetLastError(incoming_last_error);
        }
    }
    SetLastError(incoming_last_error);
}

static void clear_update_observers(void) {
    BOOL changed = FALSE;
    unsigned int index;

    acquire_update_observer_registry();
    for (index = 0u; index < UPDATE_OBSERVER_CAPACITY; ++index) {
        if (update_observers[index].owner != NULL ||
            update_observers[index].observer != NULL) {
            changed = TRUE;
            break;
        }
    }
    ZeroMemory(update_observers, sizeof(update_observers));
    if (changed) advance_update_observer_registry_generation();
    release_update_observer_registry();
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

static BOOL writable_memory(void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end >= start && end <= region_end;
}

BOOL SudekiMpControlSeparationDirectionalGait(
    float travel_x, float travel_z, float aim_x, float aim_z,
    unsigned int *mode, float *body_x, float *body_z
) {
    float length, aim_length, forward, lateral;
    if (mode == NULL || body_x == NULL || body_z == NULL) return FALSE;
    *mode = 0u;
    *body_x = 0.0f;
    *body_z = 0.0f;
    if (!isfinite(travel_x) || !isfinite(travel_z) ||
        !isfinite(aim_x) || !isfinite(aim_z)) return FALSE;
    length = sqrtf(travel_x * travel_x + travel_z * travel_z);
    aim_length = sqrtf(aim_x * aim_x + aim_z * aim_z);
    if (!isfinite(length) || !isfinite(aim_length) ||
        length < 0.0001f || aim_length < 0.0001f) return FALSE;
    travel_x /= length;
    travel_z /= length;
    aim_x /= aim_length;
    aim_z /= aim_length;
    forward = travel_x * aim_x + travel_z * aim_z;
    lateral = travel_x * aim_z - travel_z * aim_x;
    if (fabsf(lateral) > fabsf(forward)) {
        *mode = lateral > 0.0f ? 2u : 3u;
        *body_x = lateral > 0.0f ? -travel_z : travel_z;
        *body_z = lateral > 0.0f ? travel_x : -travel_x;
    } else {
        *mode = forward < 0.0f ? 1u : 0u;
        *body_x = forward < 0.0f ? -travel_x : travel_x;
        *body_z = forward < 0.0f ? -travel_z : travel_z;
    }
    return TRUE;
}

static void service_player_one_skill_direct_movement(void *controller) {
    uint8_t *character;
    void *arbiter;
    float local_direction[3];
    float world_direction[3];
    float magnitude;
    float horizontal_length;
    BOOL physical_direction_held;
    BOOL operator_direction_held = FALSE;

    if (!player_one_skill_direct_movement_scope_active ||
        player_one_skill_direct_movement_submitted ||
        movement_camera_transform == NULL ||
        !readable_memory(controller, 0x1a8u)) {
        return;
    }
    character = *(uint8_t **)(
        (uint8_t *)controller + CONTROLLER_TARGET_OFFSET);
    arbiter = readable_memory(character, 0x94u) ?
        *(void **)(character + 0x90u) : NULL;
    if (!readable_memory(arbiter, 0x54u) ||
        *(void **)((uint8_t *)arbiter + 0x10u) != character) {
        return;
    }
    local_direction[0] = *(float *)((uint8_t *)controller + 0x1a0u);
    local_direction[1] = 0.0f;
    local_direction[2] = *(float *)((uint8_t *)controller + 0x1a4u);
    if (!isfinite(local_direction[0]) || !isfinite(local_direction[2])) {
        return;
    }
    magnitude = sqrtf(
        local_direction[0] * local_direction[0] +
        local_direction[2] * local_direction[2]);
    if (!isfinite(magnitude)) return;
    physical_direction_held = magnitude > 0.0001f;
    if (!physical_direction_held &&
        player_one_skill_direction_override != NULL) {
        float override_direction[3] = {0.0f, 0.0f, 0.0f};
        operator_direction_held = player_one_skill_direction_override(
            override_direction);
        if (SudekiMpControlSeparationPlayerOneSkillDirectionOverridePolicy(
                physical_direction_held, operator_direction_held)) {
            if (!isfinite(override_direction[0]) ||
                !isfinite(override_direction[2])) return;
            local_direction[0] = override_direction[0];
            local_direction[2] = override_direction[2];
            magnitude = sqrtf(
                local_direction[0] * local_direction[0] +
                local_direction[2] * local_direction[2]);
            if (!isfinite(magnitude) || magnitude <= 0.0001f) return;
            player_one_skill_direct_movement_operator_override = TRUE;
        }
    }
    if (!physical_direction_held &&
        !player_one_skill_direct_movement_operator_override) {
        static const float neutral[3] = {0.0f, 0.0f, 0.0f};
        (void)SudekiMpControlSeparationApplyLanArenaPlayerOneSkillMovement(
            arbiter, neutral, 0.0f);
        return;
    }
    movement_camera_transform(controller, world_direction, local_direction);
    world_direction[1] = 0.0f;
    horizontal_length = sqrtf(
        world_direction[0] * world_direction[0] +
        world_direction[2] * world_direction[2]);
    if (!isfinite(horizontal_length) || horizontal_length <= 0.0001f) {
        return;
    }
    world_direction[0] /= horizontal_length;
    world_direction[2] /= horizontal_length;
    if (magnitude > 1.0f) magnitude = 1.0f;
    (void)SudekiMpControlSeparationApplyLanArenaPlayerOneSkillMovement(
        arbiter, world_direction, magnitude);
}

static void call_original_controller_update_with_skill_input_isolation(
    void *controller,
    void *update_data
) {
    uint8_t *game_speed = NULL;
    int current_mode = 0;
    int requested_mode = 0;
    BOOL paused = FALSE;
    BOOL restore_player_input = FALSE;
    BOOL isolate_controller_mode = FALSE;
    uint8_t *player_one_character = NULL;
    uint8_t *player_one_arbiter = NULL;
    uint32_t *player_one_arbiter_flags = NULL;
    uint32_t saved_player_one_arbiter_flags = 0u;
    DWORD native_last_error;
    DWORD incoming_last_error = GetLastError();

    player_one_skill_direct_movement_scope_active = FALSE;
    player_one_skill_direct_movement_submitted = FALSE;
    player_one_skill_direct_movement_operator_override = FALSE;
    player_one_skill_frame_delta = 0.0f;

    lan_tal_skill_direct_actor = NULL;
    lan_tal_skill_direct_at_ms = 0u;

    if (player_one_skill_input_isolation_enabled && game_base != NULL &&
        readable_memory(game_base + RVA_GAME_SPEED_GLOBAL,
            sizeof(game_speed))) {
        game_speed = *(uint8_t **)(game_base + RVA_GAME_SPEED_GLOBAL);
    }
    if (readable_memory(game_speed, 0x29u)) {
        current_mode = *(int *)(game_speed + 0x20u);
        requested_mode = *(int *)(game_speed + 0x24u);
        paused = *(game_speed + 0x28u) != 0u;
        restore_player_input =
            SudekiMpControlSeparationPlayerOneSkillInputIsolationPolicy(
            player_one_skill_input_isolation_enabled,
            current_mode,
            requested_mode,
            paused);
    }
    if (restore_player_input && !player_one_skill_native_input_restored &&
        game_speed_player_input_enable != NULL) {
        /* Native mode 2 disables active-party seat 0 at RVA 0x270a0.  Use
         * Sudeki's exact inverse transition once, rather than spoofing the
         * process-global skill mode that owns the CSkill task. */
        game_speed_player_input_enable(NULL);
        player_one_skill_native_input_restored = TRUE;
    }
    isolate_controller_mode = restore_player_input &&
        writable_memory(game_speed + 0x20u, 8u);
    if (isolate_controller_mode) {
        *(int *)(game_speed + 0x20u) = 0;
        *(int *)(game_speed + 0x24u) = 0;
        if (player_one_skill_input_isolation_trace_state != 1) {
            player_one_skill_input_isolation_trace_state = 1;
            SudekiMpLogFormat(
                "control_separation event=player_one_skill_input_isolation "
                "state=active current=%d requested=%d "
                "policy=native_seat0_restore_plus_controller_local_mode0_"
                "preserve_skill_task\r\n",
                current_mode, requested_mode);
        }
    } else if (player_one_skill_input_isolation_enabled &&
               !player_one_skill_native_input_restored &&
               player_one_skill_input_isolation_trace_state != 0) {
        player_one_skill_input_isolation_trace_state = 0;
        SudekiMpLogFormat(
            "control_separation event=player_one_skill_input_isolation "
            "state=waiting reason=%s policy=native_modes_untouched\r\n",
            paused ? "native_pause" :
                (game_speed == NULL ? "game_speed_unavailable" :
                    "skill_mode_inactive"));
    }
    if (restore_player_input && readable_memory(
            controller, CONTROLLER_TARGET_OFFSET + sizeof(void *))) {
        player_one_character = *(uint8_t **)(
            (uint8_t *)controller + CONTROLLER_TARGET_OFFSET);
    }
    if (readable_memory(player_one_character, 0x94u)) {
        player_one_arbiter = *(uint8_t **)(player_one_character + 0x90u);
    }
    if (readable_memory(player_one_arbiter, 0x54u) &&
        *(void **)(player_one_arbiter + 0x10u) == player_one_character) {
        player_one_arbiter_flags = (uint32_t *)(player_one_arbiter + 0x50u);
    }
    if (restore_player_input && isolate_controller_mode &&
        readable_memory(
            player_one_arbiter_flags, sizeof(*player_one_arbiter_flags))) {
        float frame_delta = readable_memory(update_data, 0x10u) ?
            *(float *)((uint8_t *)update_data + 0x0cu) : 0.0f;

        player_one_skill_direct_movement_scope_active = TRUE;
        if (isfinite(frame_delta) && frame_delta > 0.0f &&
            frame_delta <= 0.25f) {
            player_one_skill_frame_delta = frame_delta;
        }
    }
    if (restore_player_input && isolate_controller_mode &&
        tal_skill_direct_actor_exact(player_one_character) &&
        SudekiMpControlSeparationTalSkillFilterRestorePolicy(
            TRUE, *(int *)((uint8_t *)controller + 0x80u),
            *(int *)((uint8_t *)controller + 0x84u))) {
        typedef void (SUDEKIMP_THISCALL *SetFilterAll)(void *);
        /* The remote task also requests filter-none on the local controller.
         * Seat-0 restore alone does not undo it. Use the native inverse once
         * per disabled edge: the ordinary controller transition enables its
         * camera and combat route. Never replace a UI-only filter (2/3). */
        ((SetFilterAll)(game_base + RVA_CONTROLLER_FILTER_ALL))(controller);
        SudekiMpLogWrite("control_separation event=tal_noncaster_filter "
            "state=restore_requested policy=exact_remote_skill_native_filter_all\r\n");
    }
    if (player_one_skill_direct_movement_scope_active &&
        (*player_one_arbiter_flags & 0x0289e568u) == 0x00080000u) {
        saved_player_one_arbiter_flags = *player_one_arbiter_flags;
        *player_one_arbiter_flags =
            saved_player_one_arbiter_flags & ~0x00080000u;
        if (!player_one_skill_arbiter_virtualization_logged) {
            player_one_skill_arbiter_virtualization_logged = TRUE;
            SudekiMpLogFormat(
                "control_separation event=player_one_skill_input_isolation "
                "state=arbiter_lock_virtualized character=0x%08lx "
                "flags_before=0x%08lx virtualized_flag=0x00080000 "
                "policy=remote_Ailish_skill_non_caster_Tal_only\r\n",
                (unsigned long)(uintptr_t)player_one_character,
                (unsigned long)saved_player_one_arbiter_flags);
        }
    }
    SetLastError(incoming_last_error);
    original_controller_update(controller, update_data);
    native_last_error = GetLastError();
    /* A native skill removes the ordinary controller-to-arbiter callsite
     * entirely. If that callsite did not already submit through the LAN
     * wrapper, consume the same current controller axes here and use the
     * proven camera transform plus absolute-delta movement boundary. */
    service_player_one_skill_direct_movement(controller);
    player_one_skill_direct_movement_scope_active = FALSE;
    player_one_skill_direct_movement_operator_override = FALSE;
    player_one_skill_frame_delta = 0.0f;
    if (saved_player_one_arbiter_flags != 0u && readable_memory(
            player_one_arbiter_flags, sizeof(*player_one_arbiter_flags))) {
        *player_one_arbiter_flags =
            (*player_one_arbiter_flags & ~0x00080000u) |
            (saved_player_one_arbiter_flags & 0x00080000u);
    }
    if (isolate_controller_mode) {
        *(int *)(game_speed + 0x20u) = current_mode;
        *(int *)(game_speed + 0x24u) = requested_mode;
    }
    SetLastError(native_last_error);
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

static BOOL companion_character_is_in_active_group(
    const SudekiMpCompanionControlRuntime *companion
) {
    return companion != NULL &&
        character_is_in_active_group(companion->character);
}

static BOOL companion_seat_view_ready(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion
) {
    SudekiMpPlayerCombatSnapshot snapshot;

    return companion != NULL && companion->character != NULL &&
        SudekiMpCombatContextGetSnapshot(seat_index, &snapshot) &&
        snapshot.character == companion->character &&
        snapshot.viewport_camera != NULL && snapshot.render_state != NULL &&
        SudekiMpSplitScreenSeatViewReady(
            seat_index, companion->character);
}

static BOOL fixed_three_control_contract_active(void) {
    DWORD saved_error = GetLastError();
    BOOL active = SudekiMpSplitScreenFixedThreeSeatEnabled() &&
        SudekiMpLocalInputHubRequestedMask() ==
            SUDEKIMP_LOCAL_INPUT_FIXED_THREE_SEAT_MASK;

    SetLastError(saved_error);
    return active;
}

static BOOL companion_input_ready(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion
) {
    if (companion == NULL || !input_bridge_enabled ||
        !companion->input_connected || companion->input_identity == NULL) {
        return FALSE;
    }
    if ((SudekiMpLocalInputHubRequestedMask() &
            (uint8_t)(1u << seat_index)) != 0u) {
        return companion->input_generation != 0u &&
            companion->input_identity ==
                SudekiMpLocalInputHubSeatIdentity(seat_index) &&
            companion->input_generation ==
                SudekiMpLocalInputHubSeatIdentityGeneration(seat_index);
    }
    return seat_index == CONTROL_COMPANION_FIRST_SEAT &&
        companion->input_identity == SudekiMpInputBridgeIdentity();
}

static BOOL companion_active_input_lease(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion
) {
    return companion != NULL &&
        SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            seat_index,
            fixed_three_control_contract_active(),
            companion_input_ready(seat_index, companion),
            companion->leased_input_identity,
            companion->leased_input_generation,
            companion->input_identity,
            companion->input_generation);
}

static BOOL companion_actor_publication_ready(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion
) {
    if (companion == NULL || !companion->requested ||
        !companion->lease_exact || companion->character == NULL) {
        return FALSE;
    }
    return seat_index == CONTROL_COMPANION_FIRST_SEAT &&
            !fixed_three_control_contract_active() ?
        TRUE : companion_active_input_lease(seat_index, companion);
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

    if (player_index >= CONTROL_PUBLISHED_PLAYER_COUNT) {
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
    void *player_three = NULL;
    BOOL player_one_present = FALSE;
    BOOL player_two_present = FALSE;
    BOOL player_three_present = FALSE;
    SudekiMpCompanionControlRuntime *player_three_control =
        companion_control_for_seat(2u);

    if (readable_memory(controller, CONTROLLER_TARGET_OFFSET +
            sizeof(player_one))) {
        player_one = *(void **)((uint8_t *)controller +
            CONTROLLER_TARGET_OFFSET);
        player_one_present = player_one != NULL &&
            character_is_in_active_group(player_one);
    }
    if (player_two_requested && input_bridge_enabled &&
        companion_active_input_lease(1u, &companion_controls[0]) &&
        SudekiMpSplitScreenRuntimeEnabled() &&
        (!SudekiMpSplitScreenRosterParticipationAvailable() ||
         SudekiMpSplitScreenRosterParticipationRequested()) &&
        companion_controls[0].lease_exact && overridden_character != NULL &&
        overridden_character != player_one &&
        overridden_character_is_in_active_group()) {
        player_two = overridden_character;
        player_two_present = TRUE;
    }
    if (player_three_control != NULL && player_three_control->requested &&
        companion_active_input_lease(2u, player_three_control) &&
        SudekiMpSplitScreenRuntimeEnabled() &&
        player_three_control->lease_exact &&
        player_three_control->character != NULL &&
        player_three_control->character != player_one &&
        player_three_control->character != player_two &&
        companion_character_is_in_active_group(player_three_control)) {
        player_three = player_three_control->character;
        player_three_present = TRUE;
    }
    publish_runtime_player_lease(0u, player_one, player_one_present);
    publish_runtime_player_lease(1u, player_two, player_two_present);
    publish_runtime_player_lease(2u, player_three, player_three_present);
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

static void stop_companion_movement(
    unsigned int seat_index,
    SudekiMpCompanionControlRuntime *companion
) {
    uint8_t *character = companion == NULL ? NULL :
        (uint8_t *)companion->character;
    void *arbiter;

    if (companion == NULL || !companion->movement_active) {
        return;
    }
    if (character == NULL) {
        companion->movement_active = FALSE;
        companion->movement_magnitude = 0.0f;
        companion->movement_input_x = 0;
        companion->movement_input_z = 0;
        return;
    }
    if (!companion_character_is_in_active_group(companion)) {
        SudekiMpLogFormat(
            "control_separation event=companion_movement player=%u "
            "phase=abort reason=character_not_in_active_group\r\n",
            seat_index + 1u);
        companion->movement_active = FALSE;
        companion->movement_magnitude = 0.0f;
        companion->movement_input_x = 0;
        companion->movement_input_z = 0;
        return;
    }
    arbiter = *(void **)(character + 0x90);
    if (arbiter != NULL && arbiter_set_speed != NULL) {
        arbiter_set_speed(arbiter, 0.0f, 1.0f);
    }
    SudekiMpLogFormat(
        "control_separation event=companion_movement player=%u "
        "phase=stop character=0x%08lx\r\n",
        seat_index + 1u,
        (unsigned long)(uintptr_t)character
    );
    companion->movement_active = FALSE;
    companion->movement_magnitude = 0.0f;
    companion->movement_input_x = 0;
    companion->movement_input_z = 0;
}

static void stop_second_player_movement(void) {
    stop_companion_movement(1u, &companion_controls[0]);
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

static void poll_companion_input(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);
    const uint8_t hub_mask = SudekiMpLocalInputHubRequestedMask();
    BOOL hub_seat_requested =
        (hub_mask & (uint8_t)(1u << seat_index)) != 0u;
    BOOL connected = FALSE;

    if (companion == NULL) {
        return;
    }
    if (hub_seat_requested) {
        connected = SudekiMpLocalInputHubPoll(
            seat_index, &companion->input_state);
        companion->input_identity = connected ?
            SudekiMpLocalInputHubSeatIdentity(seat_index) : NULL;
        companion->input_generation = connected ?
            SudekiMpLocalInputHubSeatIdentityGeneration(seat_index) : 0u;
    } else if (seat_index == CONTROL_COMPANION_FIRST_SEAT) {
        connected = SudekiMpInputBridgePoll(&companion->input_state);
        companion->input_identity = connected ?
            SudekiMpInputBridgeIdentity() : NULL;
        /* The legacy bridge has no public connection generation. */
        companion->input_generation = 0u;
    } else {
        ZeroMemory(&companion->input_state, sizeof(companion->input_state));
        companion->input_identity = NULL;
        companion->input_generation = 0u;
    }
    if (!connected) {
        if (companion->input_connected) {
            companion->keyboard_weak_was_down = FALSE;
            stop_companion_movement(seat_index, companion);
        }
        companion->input_connected = FALSE;
        return;
    }
    companion->input_connected = TRUE;
}

static void poll_input_bridge(void) {
    DWORD now;
    int delta_x;
    int delta_y;

    if (!input_bridge_enabled) {
        return;
    }
    poll_companion_input(1u);
    poll_companion_input(2u);
    if (!input_bridge_connected) {
        return;
    }
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

static BOOL submit_companion_controller_combat_action(
    unsigned int seat_index,
    SudekiMpCompanionControlRuntime *companion,
    void *controller,
    BOOL owns_foreground,
    SudekiMpControllerActionIntent intent,
    const char **reason,
    void **submitted_arbiter,
    uint32_t *flags_50,
    uint32_t *state_58,
    uint8_t *flags_60
) {
    uint8_t *character = companion == NULL ? NULL :
        (uint8_t *)companion->character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *arbiter;
    void *controller_target;
    SudekiMpControllerCombatFlags combat_flags;
    BOOL seat_view_ready;
    BOOL fixed_three_contract;

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
    seat_view_ready = companion_seat_view_ready(seat_index, companion);
    fixed_three_contract = seat_index == 1u &&
        fixed_three_control_contract_active();
    if (companion == NULL ||
        !SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            seat_index,
            companion->requested,
            companion->lease_exact && character != NULL,
            companion_active_input_lease(seat_index, companion),
            seat_view_ready) ||
        (fixed_three_contract && !seat_view_ready) ||
        !companion_character_is_in_active_group(companion)) {
        *reason = (seat_index == 2u || fixed_three_contract) &&
                !seat_view_ready ?
                "seat_view_unavailable" : "no_live_companion_character";
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
        component != companion->ai_component ||
        *(int16_t *)(component + 0x16au) != 1 ||
        *(mode_state + 0x0bu) != 0 || character == controller_target) {
        *reason = "native_companion_combat_state_unavailable";
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

static BOOL controller_intent_quick_menu_action(
    SudekiMpControllerActionIntent intent,
    SudekiMpSplitScreenQuickMenuAction *action
) {
    if (action == NULL) {
        return FALSE;
    }
    switch (intent) {
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_CONFIRM:
        *action = SUDEKIMP_QUICK_MENU_ACTION_CONFIRM;
        return TRUE;
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_CANCEL:
        *action = SUDEKIMP_QUICK_MENU_ACTION_CANCEL;
        return TRUE;
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_SECONDARY:
        *action = SUDEKIMP_QUICK_MENU_ACTION_SECONDARY;
        return TRUE;
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_UP:
        *action = SUDEKIMP_QUICK_MENU_ACTION_UP;
        return TRUE;
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_DOWN:
        *action = SUDEKIMP_QUICK_MENU_ACTION_DOWN;
        return TRUE;
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_LEFT:
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_PREVIOUS_PAGE:
        *action = SUDEKIMP_QUICK_MENU_ACTION_PREVIOUS_CATEGORY;
        return TRUE;
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_RIGHT:
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_NEXT_PAGE:
        *action = SUDEKIMP_QUICK_MENU_ACTION_NEXT_CATEGORY;
        return TRUE;
    default:
        return FALSE;
    }
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
    size_t quick_menu_result_index = SIZE_MAX;
    BOOL seat_quick_menu_active;
    BOOL any_quick_menu_active;
    BOOL other_seat_quick_menu_active;
    BOOL seat_quick_menu_controls_modal;
    BOOL input_lease_active;
    BOOL quick_menu_opened = FALSE;
    BOOL custom_three_seat_menu;
    const char *quick_menu_open_reason = "seat_quick_menu_open_rejected";

    ZeroMemory(&context, sizeof(context));
    custom_three_seat_menu =
        SudekiMpSplitScreenFixedThreeCustomQuickMenuEnabled();
    any_quick_menu_active = SudekiMpSplitScreenQuickMenuAnyActive();
    seat_quick_menu_active =
        SudekiMpSplitScreenQuickMenuActive(1u);
    other_seat_quick_menu_active = !custom_three_seat_menu &&
        any_quick_menu_active &&
        !seat_quick_menu_active;
    seat_quick_menu_controls_modal = seat_quick_menu_active &&
        !modal_active && !transition_vote_active;
    input_lease_active = companion_active_input_lease(
        1u, &companion_controls[0]);
    context.seat_active = player_two_requested &&
        companion_controls[0].lease_exact && overridden_character != NULL &&
        input_lease_active &&
        overridden_character_is_in_active_group();
    context.modal_active = modal_active != FALSE ||
        (custom_three_seat_menu ? seat_quick_menu_active :
            any_quick_menu_active);
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
        input_bridge_enabled && input_lease_active,
        input_bridge_state.buttons,
        &context,
        results,
        SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS
    );
    if (result_count > SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS) {
        result_count = SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS;
    }
    if (!seat_quick_menu_active &&
        (custom_three_seat_menu || !any_quick_menu_active)) {
        for (result_index = 0u;
             result_index < result_count;
             ++result_index) {
            if (results[result_index].intent ==
                    SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU) {
                quick_menu_result_index = result_index;
                break;
            }
        }
    }
    if (quick_menu_result_index != SIZE_MAX) {
        if (!owns_foreground) {
            quick_menu_open_reason = "game_window_not_foreground";
        } else if (!context.seat_active) {
            quick_menu_open_reason = "seat_input_lease_inactive";
        } else if (SudekiMpSplitScreenQuickMenuRequest(1u)) {
            quick_menu_opened = TRUE;
            quick_menu_open_reason = "seat_quick_menu_opened";
        }
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

        if (other_seat_quick_menu_active) {
            delivery = "blocked";
            reason = "other_seat_quick_menu_active";
        } else if (quick_menu_result_index != SIZE_MAX &&
            result_index != quick_menu_result_index) {
            delivery = "blocked";
            reason = "menu_open_edge_consumed";
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU) {
            delivery = quick_menu_opened ? "submitted" : "rejected";
            reason = quick_menu_open_reason;
        } else if (resolution->intent == SUDEKIMP_CONTROLLER_INTENT_NONE) {
            delivery = "blocked";
            reason = context.seat_active ?
                "no_action_in_current_context" : "seat_inactive";
        } else if (!context.seat_active) {
            delivery = "blocked";
            reason = "seat_input_lease_inactive";
        } else if (context.modal_active &&
                seat_quick_menu_controls_modal) {
            SudekiMpSplitScreenQuickMenuAction action;

            if (!owns_foreground) {
                delivery = "rejected";
                reason = "game_window_not_foreground";
            } else if (!controller_intent_quick_menu_action(
                           resolution->intent, &action)) {
                delivery = "blocked";
                reason = "seat_quick_menu_action_not_supported";
            } else if (SudekiMpSplitScreenQuickMenuSubmit(1u, action)) {
                delivery = "submitted";
                reason = "seat_quick_menu_action_submitted";
            } else {
                delivery = "rejected";
                reason = "seat_quick_menu_action_rejected";
            }
        } else if (controller_intent_is_combat(resolution->intent)) {
            if (submit_companion_controller_combat_action(
                    1u,
                    &companion_controls[0],
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
            if (!owns_foreground) {
                delivery = "rejected";
                reason = "game_window_not_foreground";
            } else if (context.interaction_target_known) {
                reason = "exact_target_consumer_not_connected";
            } else if (SudekiMpInteractionProvenanceProbeActorLocalNearby(1u)) {
                reason = "actor_local_nearby_entities_observed_no_target_mapping";
            } else {
                reason = "awaiting_actor_local_interaction_target";
            }
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

static void service_player_three_controller_actions(
    void *controller,
    BOOL owns_foreground,
    BOOL modal_active,
    BOOL transition_vote_active
) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[1];
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution
        results[SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS];
    size_t result_count;
    size_t result_index;
    size_t quick_menu_result_index = SIZE_MAX;
    BOOL view_ready = companion_seat_view_ready(2u, companion);
    BOOL input_ready = companion_active_input_lease(2u, companion);
    BOOL any_quick_menu_active;
    BOOL seat_quick_menu_active;
    BOOL other_seat_quick_menu_active;
    BOOL seat_quick_menu_controls_modal;
    BOOL quick_menu_opened = FALSE;
    BOOL custom_three_seat_menu;
    const char *quick_menu_open_reason = "seat_quick_menu_open_rejected";

    ZeroMemory(&context, sizeof(context));
    custom_three_seat_menu =
        SudekiMpSplitScreenFixedThreeCustomQuickMenuEnabled();
    any_quick_menu_active = SudekiMpSplitScreenQuickMenuAnyActive();
    seat_quick_menu_active = SudekiMpSplitScreenQuickMenuActive(2u);
    other_seat_quick_menu_active = !custom_three_seat_menu &&
        any_quick_menu_active &&
        !seat_quick_menu_active;
    seat_quick_menu_controls_modal = seat_quick_menu_active &&
        !modal_active && !transition_vote_active;
    context.seat_active =
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            2u,
            companion->requested,
            companion->lease_exact && companion->character != NULL,
            input_ready,
            view_ready) &&
        companion_character_is_in_active_group(companion);
    context.modal_active = modal_active != FALSE ||
        (custom_three_seat_menu ? seat_quick_menu_active :
            any_quick_menu_active);
    context.transition_vote_active = transition_vote_active != FALSE;
    context.combat_active = second_player_combat_active();
    result_count = SudekiMpControllerActionRouterAdvance(
        &controller_action_router,
        2u,
        input_ready,
        companion->input_state.buttons,
        &context,
        results,
        SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS);
    if (result_count > SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS) {
        result_count = SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS;
    }
    if (!seat_quick_menu_active &&
        (custom_three_seat_menu || !any_quick_menu_active)) {
        for (result_index = 0u;
             result_index < result_count;
             ++result_index) {
            if (results[result_index].intent ==
                    SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU) {
                quick_menu_result_index = result_index;
                break;
            }
        }
    }
    if (quick_menu_result_index != SIZE_MAX) {
        if (!owns_foreground) {
            quick_menu_open_reason = "game_window_not_foreground";
        } else if (!context.seat_active) {
            quick_menu_open_reason = "seat_input_lease_inactive";
        } else if (SudekiMpSplitScreenQuickMenuRequest(2u)) {
            quick_menu_opened = TRUE;
            quick_menu_open_reason = "seat_quick_menu_opened";
        }
    }
    for (result_index = 0u; result_index < result_count; ++result_index) {
        SudekiMpControllerActionResolution *resolution =
            &results[result_index];
        const char *delivery = "blocked";
        const char *reason = context.seat_active ?
            "consumer_not_in_first_p3_slice" :
            (view_ready ? "seat_inactive" : "seat_view_unavailable");
        void *arbiter = NULL;
        uint32_t flags_50 = 0u;
        uint32_t state_58 = 0u;
        uint8_t flags_60 = 0u;

        if (other_seat_quick_menu_active) {
            delivery = "blocked";
            reason = "other_seat_quick_menu_active";
        } else if (quick_menu_result_index != SIZE_MAX &&
                   result_index != quick_menu_result_index) {
            delivery = "blocked";
            reason = "menu_open_edge_consumed";
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU) {
            delivery = quick_menu_opened ? "submitted" : "rejected";
            reason = quick_menu_open_reason;
        } else if (context.modal_active &&
                   seat_quick_menu_controls_modal) {
            SudekiMpSplitScreenQuickMenuAction action;

            if (!owns_foreground) {
                delivery = "rejected";
                reason = "game_window_not_foreground";
            } else if (!controller_intent_quick_menu_action(
                           resolution->intent, &action)) {
                delivery = "blocked";
                reason = "seat_quick_menu_action_not_supported";
            } else if (SudekiMpSplitScreenQuickMenuSubmit(2u, action)) {
                delivery = "submitted";
                reason = "seat_quick_menu_action_submitted";
            } else {
                delivery = "rejected";
                reason = "seat_quick_menu_action_rejected";
            }
        } else if (resolution->intent ==
                SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK) {
            if (submit_companion_controller_combat_action(
                    2u,
                    companion,
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
                SUDEKIMP_CONTROLLER_INTENT_NONE) {
            reason = context.seat_active ?
                "no_action_in_current_context" : reason;
        }
        SudekiMpLogFormat(
            "control_separation event=controller_action_edge phase=rising "
            "player=3 protocol_button=%s intent=%s layer=%s combat=%s "
            "view_ready=%s delivery=%s reason=%s character=0x%08lx "
            "arbiter=0x%08lx flags_50=0x%08lx state_58=0x%08lx "
            "flags_60=0x%02x policy=seat_owned_local_quick_menu\r\n",
            SudekiMpControllerProtocolButtonName(
                resolution->protocol_button),
            SudekiMpControllerActionIntentName(resolution->intent),
            controller_action_layer_name(&context),
            context.combat_active ? "true" : "false",
            view_ready ? "true" : "false",
            delivery,
            reason,
            (unsigned long)(uintptr_t)companion->character,
            (unsigned long)(uintptr_t)arbiter,
            (unsigned long)flags_50,
            (unsigned long)state_58,
            (unsigned int)flags_60);
    }
}

static void quiesce_for_shared_interaction_modal(void) {
    quiesce_second_player_input();
    stop_companion_movement(2u, &companion_controls[1]);
    SudekiMpCombatContextSetInputSource(
        1u, SUDEKIMP_COMBAT_INPUT_NONE, NULL);
    SudekiMpCombatContextSetInputSource(
        2u, SUDEKIMP_COMBAT_INPUT_NONE, NULL);
    if (!shared_interaction_modal_quiesce_logged) {
        shared_interaction_modal_quiesce_logged = TRUE;
        SudekiMpLogWrite(
            "control_separation event=shared_interaction_modal players=2,3 "
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
        "control_separation event=shared_interaction_modal players=2,3 "
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
        SudekiMpSplitScreenSharedInteractionModalActive() ||
        (!SudekiMpSplitScreenFixedThreeCustomQuickMenuEnabled() &&
         SudekiMpSplitScreenQuickMenuAnyActive());
}

static BOOL service_transition_vote_input_freeze(
    void *controller,
    void *update_data,
    ControlUpdateDispatchFrame *dispatch_frame
) {
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
    service_player_three_controller_actions(
        controller,
        FALSE,
        blacksmith_modal,
        vote_suppressed && !blacksmith_modal
    );
    stop_first_player_movement(controller);
    stop_second_player_movement();
    stop_companion_movement(2u, &companion_controls[1]);
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
            "policy=skip_p1_native_controller_update_and_all_companion_submissions_keep_vote_observer_running_until_vote_and_escape_release\r\n");
    }
    notify_update_observers(
        controller,
        update_data,
        dispatch_frame,
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_PRE_ORIGINAL
    );
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

static BOOL companion_bridge_movement(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion,
    float *x,
    float *z,
    float *speed
) {
    float raw_x;
    float raw_z;
    float magnitude;
    float direction_magnitude;
    float scaled_magnitude;

    if (!companion_active_input_lease(seat_index, companion)) {
        return FALSE;
    }
    raw_x = (float)companion->input_state.left_x / 32768.0f;
    raw_z = -(float)companion->input_state.left_y / 32768.0f;
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

static BOOL bridge_movement(float *x, float *z, float *speed) {
    return companion_bridge_movement(
        1u, &companion_controls[0], x, z, speed);
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

    /* Authenticated LAN input is submitted by the post-controller LAN
     * observer.  The legacy local-P2 poll must not stop that native arbiter
     * lease first merely because its own keyboard/bridge movement feature is
     * disabled; doing so alternates stop/submit every controller frame and
     * presents as Ailish stumbling.  Session timeout and teardown remain the
     * sole LAN stop authorities through Submit(...0,0...) and
     * SetLanArenaRemoteInputEnabled(FALSE). */
    if (lan_arena_remote_input_enabled) {
        return;
    }
    if (!player_two_requested) {
        stop_second_player_movement();
        return;
    }
    if (!second_player_movement_enabled ||
        !companion_controls[0].lease_exact || character == NULL) {
        stop_second_player_movement();
        return;
    }
    if (fixed_three_control_contract_active() &&
        (!companion_active_input_lease(1u, &companion_controls[0]) ||
         !companion_seat_view_ready(1u, &companion_controls[0]))) {
        stop_second_player_movement();
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
    if (!owns_foreground || component == NULL ||
        component != companion_controls[0].ai_component ||
        mode_state == NULL ||
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
            player_two_camera_basis = SudekiMpTransformSeatMovement(
                1u,
                character,
                direction,
                transformed
            );
        }
        if (!player_two_camera_basis) {
            if (fixed_three_control_contract_active()) {
                stop_second_player_movement();
                return;
            }
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

static void poll_player_three_movement(
    void *controller,
    BOOL owns_foreground
) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[1];
    uint8_t *character = (uint8_t *)companion->character;
    uint8_t *component;
    uint8_t *mode_state;
    void *arbiter;
    void *controller_target;
    float input_x;
    float input_z;
    float movement_speed;
    float direction[3];
    uint32_t arbiter_flags;
    int arbiter_mode;
    unsigned int arbiter_state_60;
    int control_state;
    unsigned int movement_flags;
    unsigned int native_gate;

    if (!second_player_movement_enabled || !owns_foreground ||
        !SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            2u,
            companion->requested,
            companion->lease_exact && character != NULL,
            companion_active_input_lease(2u, companion),
            companion_seat_view_ready(2u, companion)) ||
        !companion_character_is_in_active_group(companion) ||
        !companion_bridge_movement(
            2u, companion, &input_x, &input_z, &movement_speed)) {
        stop_companion_movement(2u, companion);
        return;
    }
    if (movement_speed <= 0.0001f) {
        stop_companion_movement(2u, companion);
        return;
    }
    component = *(uint8_t **)(character + 0x94u);
    mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3cu);
    arbiter = *(void **)(character + 0x90u);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + CONTROLLER_TARGET_OFFSET);
    if (component == NULL || component != companion->ai_component ||
        mode_state == NULL || arbiter == NULL ||
        *(void **)(character + 0xacu) == NULL ||
        *(int16_t *)(component + 0x16au) != 1 ||
        *(mode_state + 0x0bu) != 0u || character == controller_target ||
        arbiter_movement == NULL) {
        stop_companion_movement(2u, companion);
        return;
    }

    direction[0] = input_x;
    direction[1] = 0.0f;
    direction[2] = input_z;
    if (camera_relative_movement_enabled) {
        float transformed[3] = {0.0f, 0.0f, 0.0f};
        float horizontal_length;

        /* The fixed-three renderer validates actor, view, render state, and
         * cache ownership again. Never fall back to P1's global basis. */
        if (!SudekiMpTransformSeatMovement(
                2u, character, direction, transformed)) {
            stop_companion_movement(2u, companion);
            return;
        }
        transformed[1] = 0.0f;
        horizontal_length = sqrtf(
            transformed[0] * transformed[0] +
            transformed[2] * transformed[2]);
        if (horizontal_length <= 0.0001f) {
            stop_companion_movement(2u, companion);
            return;
        }
        direction[0] = transformed[0] / horizontal_length;
        direction[2] = transformed[2] / horizontal_length;
    }
    native_gate = classify_native_movement_gate(
        arbiter,
        0u,
        &arbiter_flags,
        &arbiter_mode,
        &arbiter_state_60,
        &control_state,
        &movement_flags);
    if (native_gate != NATIVE_MOVEMENT_GATE_ALLOWED) {
        stop_companion_movement(2u, companion);
        return;
    }
    arbiter_movement(arbiter, direction, movement_speed, 1.0f, 0u);
    if (!companion->movement_active ||
        (int)companion->input_state.left_x -
                companion->movement_input_x > 4096 ||
        (int)companion->input_state.left_x -
                companion->movement_input_x < -4096 ||
        (int)companion->input_state.left_y -
                companion->movement_input_z > 4096 ||
        (int)companion->input_state.left_y -
                companion->movement_input_z < -4096) {
        SudekiMpLogFormat(
            "control_separation event=companion_movement player=3 "
            "phase=submit source=local_input_hub character=0x%08lx "
            "arbiter=0x%08lx input_x=%d input_z=%d "
            "camera_relative=%s camera_basis=%s seat_view=exact "
            "speed_bits=0x%08lx native_gate=%s\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)arbiter,
            (int)companion->input_state.left_x,
            (int)companion->input_state.left_y,
            camera_relative_movement_enabled ? "true" : "false",
            camera_relative_movement_enabled ?
                "player_three_render" : "world_axes",
            (unsigned long)float_bits(movement_speed),
            native_movement_gate_name(native_gate));
    }
    companion->movement_active = TRUE;
    companion->movement_magnitude = movement_speed;
    companion->movement_input_x = companion->input_state.left_x;
    companion->movement_input_z = companion->input_state.left_y;
}

static void poll_second_player_camera_facing(
    void *controller,
    BOOL owns_foreground
) {
    uint8_t *character = (uint8_t *)overridden_character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *arbiter;
    void *controller_target;
    float right_x;
    float right_y;
    static const float local_forward[3] = {0.0f, 0.0f, 1.0f};
    float world_forward[3];
    float dot;

    if (!player_two_requested || !owns_foreground ||
        !companion_controls[0].lease_exact ||
        !second_player_movement_enabled ||
        !camera_relative_movement_enabled || !input_bridge_enabled ||
        !input_bridge_connected || character == NULL ||
        (fixed_three_control_contract_active() &&
         (!companion_active_input_lease(1u, &companion_controls[0]) ||
          !companion_seat_view_ready(1u, &companion_controls[0]))) ||
        !overridden_character_is_in_active_group()) {
        second_player_facing_valid = FALSE;
        return;
    }
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248u);
    component = *(uint8_t **)(character + 0x94u);
    mode_state = component == NULL ? NULL :
        *(uint8_t **)(component + 0x3cu);
    arbiter = *(uint8_t **)(character + 0x90u);
    if (component == NULL ||
        component != companion_controls[0].ai_component ||
        mode_state == NULL ||
        *(int16_t *)(component + 0x16au) != 1 ||
        *(uint8_t *)(mode_state + 0x0bu) != 0u ||
        character == controller_target || !readable_memory(arbiter, 0x54u) ||
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
    if (!SudekiMpTransformSeatMovement(
            1u,
            character,
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
    if (SudekiMpAlignSeatFacingToCamera(1u, character)) {
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
    if (!companion_controls[0].lease_exact || character == NULL ||
        !overridden_character_is_in_active_group()) {
        weak_attack_was_down = key_is_down;
        return;
    }

    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    arbiter = *(uint8_t **)(character + 0x90);
    controller_target = controller == NULL ? NULL :
        *(void **)((uint8_t *)controller + 0x248);
    if (!owns_foreground || component == NULL ||
        component != companion_controls[0].ai_component ||
        mode_state == NULL ||
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
                companion_controls[0].lease_exact &&
                overridden_character_is_in_active_group() &&
                component != NULL &&
                component == companion_controls[0].ai_component &&
                mode_state != NULL &&
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

static BOOL companion_character_reserved_by_other_seat(
    unsigned int seat_index,
    void *character
) {
    unsigned int other_seat;

    for (other_seat = CONTROL_COMPANION_FIRST_SEAT;
         other_seat <= CONTROL_COMPANION_LAST_SEAT;
         ++other_seat) {
        SudekiMpCompanionControlRuntime *other;

        if (other_seat == seat_index) {
            continue;
        }
        other = companion_control_for_seat(other_seat);
        if (other != NULL &&
            (other->character == character ||
             other->requested_character == character)) {
            return TRUE;
        }
    }
    return FALSE;
}

static uint8_t *find_companion_party_slot(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion,
    uint8_t *group,
    void *controller_target,
    unsigned int *slot_index
) {
    unsigned int index;

    if (companion == NULL || group == NULL || controller_target == NULL ||
        *(void **)(group + PARTY_SLOT_FIRST_OFFSET) != controller_target) {
        return NULL;
    }
    /* P3 never infers identity from party order. */
    if (seat_index == 2u && companion->requested_character == NULL) {
        return NULL;
    }
    for (index = 1u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *slot = group + PARTY_SLOT_FIRST_OFFSET +
            index * PARTY_SLOT_STRIDE;
        void *candidate = *(void **)slot;

        if (candidate != NULL && candidate != controller_target &&
            (companion->requested_character == NULL ||
             candidate == companion->requested_character) &&
            !companion_character_reserved_by_other_seat(
                seat_index, candidate)) {
            if (slot_index != NULL) {
                *slot_index = index;
            }
            return slot;
        }
    }
    return NULL;
}

static void log_control_state(
    unsigned int seat_index,
    const char *event,
    const char *result,
    uint8_t *slot,
    unsigned int party_slot_index
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
        "control_separation event=%s player=%u result=%s slot=%u "
        "character=0x%08lx component=0x%08lx control_ref_16a=%d "
        "ai_enabled_3c_0b=%d\r\n",
        event,
        seat_index + 1u,
        result,
        party_slot_index,
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)component,
        control_ref,
        ai_enabled
    );
}

BOOL SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
    int16_t before_ref,
    uint8_t before_mode,
    int16_t after_ref,
    uint8_t after_mode,
    BOOL controller_target
) {
    if (before_ref <= 0 || before_mode != 0u ||
        (int)after_ref != (int)before_ref - 1) {
        return FALSE;
    }
    if (after_ref > 0) {
        return after_mode == 0u;
    }
    return after_mode == (controller_target ? 0u : 1u);
}

BOOL SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
    int16_t before_ref,
    uint8_t before_mode,
    int16_t after_ref,
    uint8_t after_mode
) {
    return before_ref == 0 && before_mode == 1u &&
        after_ref == 1 && after_mode == 0u;
}

static void clear_companion_owned_identity(
    unsigned int seat_index,
    SudekiMpCompanionControlRuntime *companion
) {
    if (companion == NULL) {
        return;
    }
    stop_companion_movement(seat_index, companion);
    companion->character = NULL;
    companion->ai_component = NULL;
    companion->lease_exact = FALSE;
    companion->leased_input_identity = NULL;
    companion->leased_input_generation = 0u;
    if (seat_index == 1u) {
        reset_native_movement_acceptance_trace();
        reset_target_trace_state();
    } else {
        SudekiMpCombatContextSetCharacter(seat_index, NULL);
        SudekiMpCombatContextSetInputSource(
            seat_index, SUDEKIMP_COMBAT_INPUT_NONE, NULL);
        SudekiMpCombatContextSetView(seat_index, NULL, NULL);
    }
}

static BOOL transition_companion_ai(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    uint8_t *slot;
    uint8_t *character;
    uint8_t *component;
    uint8_t *mode_state;
    unsigned int slot_index = 0;
    int before_ref;
    int before_mode;
    int after_ref;
    int after_mode;

    if (companion == NULL || game_base == NULL || ai_override_control == NULL ||
        ai_default_control == NULL ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group)) ||
        !readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    controller_target = readable_memory(
            controller, CONTROLLER_TARGET_OFFSET + sizeof(controller_target)) ?
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) : NULL;
    if (!readable_memory(
            group,
            PARTY_SLOT_FIRST_OFFSET + PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
        SudekiMpLogFormat(
            "control_separation event=toggle_abort player=%u "
            "reason=no_active_group\r\n",
            seat_index + 1u);
        return FALSE;
    }

    if (companion->character == NULL) {
        slot = find_companion_party_slot(
            seat_index,
            companion,
            group,
            controller_target,
            &slot_index
        );
        if (slot == NULL) {
            SudekiMpLogFormat(
                "control_separation event=toggle_abort player=%u reason=%s "
                "controller_target=0x%08lx front_character=0x%08lx "
                "policy=exact_distinct_companion_p3_never_inferred\r\n",
                seat_index + 1u,
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
            return FALSE;
        }
        character = *(uint8_t **)slot;
        component = *(uint8_t **)(character + 0x94);
        mode_state = component == NULL ? NULL :
            *(uint8_t **)(component + 0x3c);
        if (component == NULL || mode_state == NULL) {
            SudekiMpLogFormat(
                "control_separation event=toggle_abort player=%u "
                "reason=incomplete_ai_component\r\n",
                seat_index + 1u);
            return FALSE;
        }
        before_ref = (int)*(int16_t *)(component + 0x16a);
        before_mode = (int)*(mode_state + 0x0b);
        if (before_ref != 0 || before_mode != 1) {
            log_control_state(seat_index, "override_abort",
                "unexpected_initial_state", slot, slot_index);
            return FALSE;
        }

        if (!SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
                seat_index,
                fixed_three_control_contract_active(),
                companion_input_ready(seat_index, companion))) {
            SudekiMpLogFormat(
                "control_separation event=override_abort player=%u "
                "reason=current_input_not_ready_before_native_acquire "
                "policy=fixed_0x07_requires_both_transport_leases\r\n",
                seat_index + 1u);
            return FALSE;
        }

        ai_override_control(slot);
        after_ref = (int)*(int16_t *)(component + 0x16a);
        after_mode = (int)*(mode_state + 0x0b);
        if (SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
                (int16_t)before_ref,
                (uint8_t)before_mode,
                (int16_t)after_ref,
                (uint8_t)after_mode)) {
            companion->character = character;
            companion->ai_component = component;
            companion->lease_exact = TRUE;
            companion->leased_input_identity = companion->input_identity;
            companion->leased_input_generation = companion->input_generation;
            if (seat_index == 1u) {
                reset_native_movement_acceptance_trace();
                reset_target_trace_state();
            } else {
                /* A prior actor's view can never authorize this new lease. */
                SudekiMpCombatContextSetView(seat_index, NULL, NULL);
                /* Publish actor and transport identity before the compositor
                 * is allowed to acquire a view for this exact lease. */
                SudekiMpCombatContextSetCharacter(seat_index, character);
                SudekiMpCombatContextSetInputSource(
                    seat_index,
                    SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE,
                    (void *)companion->input_identity);
            }
            log_control_state(
                seat_index, "override", "success", slot, slot_index);
            if (SudekiMpCleanroomEngineRefreshCombatMode()) {
                SudekiMpLogFormat(
                    "control_separation event=combat_arm_refresh player=%u "
                    "status=confirmed character=0x%08lx "
                    "reason=companion_control_override "
                    "policy=native_group_transition\r\n",
                    seat_index + 1u,
                    (unsigned long)(uintptr_t)character
                );
            } else {
                SudekiMpLogFormat(
                    "control_separation event=combat_arm_refresh player=%u "
                    "status=skipped character=0x%08lx "
                    "reason=combat_mode_unavailable_or_disabled "
                    "policy=native_group_transition\r\n",
                    seat_index + 1u,
                    (unsigned long)(uintptr_t)character
                );
            }
            return TRUE;
        } else {
            log_control_state(seat_index, "override",
                "verification_failed", slot, slot_index);
            if (after_ref > before_ref) {
                ai_default_control(slot);
                after_ref = (int)*(int16_t *)(component + 0x16a);
                after_mode = (int)*(mode_state + 0x0b);
                log_control_state(seat_index, "override_rollback",
                    after_ref == before_ref && after_mode == before_mode ?
                        "confirmed" : "verification_failed",
                    slot, slot_index);
                if (after_ref > before_ref) {
                    /* Preserve the exact identity so a later teardown can
                     * retry; this quarantined lease is never reported active. */
                    companion->character = character;
                    companion->ai_component = component;
                    companion->lease_exact = FALSE;
                }
            }
        }
        return FALSE;
    }

    slot = find_character_party_slot(
        group,
        companion->character,
        &slot_index
    );
    if (slot == NULL) {
        SudekiMpLogFormat(
            "control_separation event=restore player=%u "
            "status=released_without_native_default "
            "reason=owned_character_no_longer_in_active_party "
            "policy=drop_stale_runtime_identity_after_party_rebuild\r\n",
            seat_index + 1u);
        clear_companion_owned_identity(seat_index, companion);
        return TRUE;
    }
    character = *(uint8_t **)slot;
    component = *(uint8_t **)(character + 0x94);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3c);
    if (component == NULL || mode_state == NULL) {
        SudekiMpLogFormat(
            "control_separation event=restore_abort player=%u "
            "reason=incomplete_ai_component\r\n",
            seat_index + 1u);
        return FALSE;
    }
    if (component != companion->ai_component) {
        SudekiMpLogFormat(
            "control_separation event=restore_abort player=%u "
            "reason=owned_ai_component_identity_changed\r\n",
            seat_index + 1u);
        return FALSE;
    }
    before_ref = (int)*(int16_t *)(component + 0x16a);
    before_mode = (int)*(mode_state + 0x0b);
    if (before_ref < 1 || before_mode != 0) {
        log_control_state(seat_index, "restore_abort",
            "unexpected_owned_lease_state", slot, slot_index);
        return FALSE;
    }

    stop_companion_movement(seat_index, companion);
    ai_default_control(slot);
    after_ref = (int)*(int16_t *)(component + 0x16a);
    after_mode = (int)*(mode_state + 0x0b);
    if (SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            (int16_t)before_ref,
            (uint8_t)before_mode,
            (int16_t)after_ref,
            (uint8_t)after_mode,
            character == controller_target)) {
        log_control_state(
            seat_index, "restore", "success", slot, slot_index);
        clear_companion_owned_identity(seat_index, companion);
        return TRUE;
    } else {
        log_control_state(seat_index, "restore",
            "verification_failed", slot, slot_index);
        if (after_ref == before_ref - 1) {
            /* Our one decrement occurred. Never retry and double-release it,
             * even if the remaining native state is surprising. */
            clear_companion_owned_identity(seat_index, companion);
        }
        return FALSE;
    }
}

static BOOL companion_requires_release(
    unsigned int seat_index,
    const SudekiMpCompanionControlRuntime *companion
) {
    if (companion == NULL || companion->character == NULL) {
        return FALSE;
    }
    if (!companion->requested || !companion->lease_exact ||
        (companion->requested_character != NULL &&
         companion->character != companion->requested_character)) {
        return TRUE;
    }
    if ((seat_index == 2u ||
         (seat_index == 1u && fixed_three_control_contract_active())) &&
        !companion_active_input_lease(seat_index, companion)) {
        return TRUE;
    }
    if (seat_index == 2u &&
        (!companion_controls[0].requested ||
         !companion_controls[0].lease_exact ||
         companion_controls[0].character == NULL ||
         companion_controls[0].requested_character == NULL ||
         companion_controls[0].character !=
            companion_controls[0].requested_character ||
         !companion_active_input_lease(
             1u, &companion_controls[0]))) {
        return TRUE;
    }
    return FALSE;
}

static void reconcile_companion_request(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);
    BOOL release_required;
    BOOL acquire_required;
    DWORD now;

    if (companion == NULL) {
        return;
    }
    release_required = companion_requires_release(seat_index, companion);
    acquire_required = companion->character == NULL && companion->requested &&
        SudekiMpControlSeparationSeatAcquireOrderPolicy(
            seat_index,
            companion_controls[0].requested &&
                companion_controls[0].lease_exact &&
                companion_controls[0].character != NULL &&
                companion_controls[0].requested_character != NULL &&
                companion_controls[0].character ==
                    companion_controls[0].requested_character) &&
        SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            seat_index,
            fixed_three_control_contract_active(),
            companion_input_ready(seat_index, companion));
    if (!release_required && !acquire_required) {
        return;
    }
    now = GetTickCount();
    if (companion->request_last_attempt != 0u &&
        (DWORD)(now - companion->request_last_attempt) < 250u) {
        return;
    }
    companion->request_last_attempt = now;
    (void)transition_companion_ai(seat_index);
}

static void reconcile_companion_requests(void) {
    BOOL player_three_release_required =
        companion_requires_release(2u, &companion_controls[1]);
    BOOL player_two_release_required =
        companion_requires_release(1u, &companion_controls[0]);

    if (SudekiMpControlSeparationDeferReleaseToRosterPolicy(
            fixed_three_control_contract_active(),
            role_lock_active,
            player_two_release_required ||
                player_three_release_required)) {
        if (!fixed_three_release_deferred_logged) {
            fixed_three_release_deferred_logged = TRUE;
            SudekiMpLogWrite(
                "control_separation event=companion_release phase=deferred "
                "reason=fixed_three_committed_input_lease_lost "
                "policy=roster_observer_releases_cameras_then_p3_then_p2\r\n");
        }
        return;
    }
    fixed_three_release_deferred_logged = FALSE;
    /* Native ownership unwinds in reverse acquisition order. */
    if (player_three_release_required) {
        reconcile_companion_request(2u);
    }
    if (SudekiMpControlSeparationSeatReleaseOrderPolicy(
            1u, companion_controls[1].character != NULL) &&
        player_two_release_required) {
        reconcile_companion_request(1u);
    }
    if (companion_controls[0].character == NULL) {
        reconcile_companion_request(1u);
    }
    if (companion_controls[1].character == NULL) {
        reconcile_companion_request(2u);
    }
}

static void SUDEKIMP_THISCALL service_control_update_observers(
    void *controller,
    void *update_data
) {
    ControlUpdateDispatchFrame dispatch_frame;
    ControllerUpdateFunction fallback_original;
    DWORD native_last_error;

    if (!begin_owned_control_update_dispatch(
            &dispatch_frame, TRUE, &fallback_original)) {
        if (fallback_original != NULL) {
            fallback_original(controller, update_data);
        }
        return;
    }
    original_controller_update(controller, update_data);
    ++dispatch_frame.original_call_count;
    native_last_error = GetLastError();
    notify_update_observers(
        controller,
        update_data,
        &dispatch_frame,
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL
    );
    end_control_update_dispatch(&dispatch_frame);
    SetLastError(native_last_error);
}

static void poll_control_separation_hotkey_body(
    void *controller,
    void *update_data,
    ControlUpdateDispatchFrame *dispatch_frame
) {
    HWND foreground;
    DWORD foreground_process_id = 0;
    BOOL hotkey_is_down;
    BOOL owns_foreground;
    BOOL gameplay_input_allowed;
    BOOL player_two_gameplay_allowed;
    BOOL player_three_gameplay_allowed;
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
    if (service_transition_vote_input_freeze(
            controller, update_data, dispatch_frame)) {
        return;
    }
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        service_second_player_controller_actions(
            controller, FALSE, TRUE, FALSE);
        service_player_three_controller_actions(
            controller, FALSE, TRUE, FALSE);
        quiesce_for_shared_interaction_modal();
        call_original_controller_update_with_skill_input_isolation(
            controller, update_data);
        ++dispatch_frame->original_call_count;
        publish_runtime_player_leases(controller);
        SudekiMpPlayerStatehoodService(
            SudekiMpPlayerStatehoodRuntime(), GetTickCount());
        update_roaming_boundary(controller);
        notify_update_observers(
            controller,
            update_data,
            dispatch_frame,
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL
        );
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
    call_original_controller_update_with_skill_input_isolation(
        controller, update_data);
    ++dispatch_frame->original_call_count;
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
        service_player_three_controller_actions(
            controller, FALSE, TRUE, FALSE);
        quiesce_for_shared_interaction_modal();
        SudekiMpPlayerStatehoodService(
            SudekiMpPlayerStatehoodRuntime(), GetTickCount());
        update_roaming_boundary(controller);
        notify_update_observers(
            controller,
            update_data,
            dispatch_frame,
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL
        );
        return;
    }
    if (service_transition_vote_input_freeze(
            controller, update_data, dispatch_frame)) {
        return;
    }
    poll_input_bridge();
    SudekiMpCombatContextSetCharacter(
        0u,
        controller == NULL ? NULL :
            *(void **)((uint8_t *)controller + 0x248u)
    );
    SudekiMpCombatContextSetCharacter(
        1u, companion_actor_publication_ready(
                1u, &companion_controls[0]) ?
            overridden_character : NULL);
    SudekiMpCombatContextSetCharacter(
        2u,
        companion_actor_publication_ready(
                2u, &companion_controls[1]) ?
            companion_controls[1].character : NULL);
    SudekiMpCombatContextSetInputSource(
        0u,
        controller == NULL ? SUDEKIMP_COMBAT_INPUT_NONE :
            SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER,
        controller
    );
    SudekiMpCombatContextSetInputSource(
        1u,
        !player_two_requested || !companion_controls[0].lease_exact ||
                overridden_character == NULL ||
                (input_bridge_enabled &&
                 !companion_active_input_lease(
                     1u, &companion_controls[0])) ?
            SUDEKIMP_COMBAT_INPUT_NONE :
            (input_bridge_enabled ?
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE :
                SUDEKIMP_COMBAT_INPUT_KEYBOARD_PROTOTYPE),
        !player_two_requested || !companion_controls[0].lease_exact ||
                overridden_character == NULL ||
                (input_bridge_enabled &&
                 !companion_active_input_lease(
                     1u, &companion_controls[0])) ? NULL :
            (input_bridge_enabled ?
                (void *)companion_controls[0].input_identity :
                (void *)second_player_skill_virtual_keys)
    );
    SudekiMpCombatContextSetInputSource(
        2u,
        !companion_controls[1].requested ||
                !companion_controls[1].lease_exact ||
                !companion_active_input_lease(
                    2u, &companion_controls[1]) ?
            SUDEKIMP_COMBAT_INPUT_NONE :
            SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE,
        !companion_controls[1].requested ||
                !companion_controls[1].lease_exact ||
                !companion_active_input_lease(
                    2u, &companion_controls[1]) ? NULL :
            (void *)companion_controls[1].input_identity
    );
    SudekiMpCombatContextsPollGame((HMODULE)game_base);
    hotkey_is_down = manual_toggle_enabled &&
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
        } else if (player_two_requested &&
            (companion_controls[1].requested ||
             companion_controls[1].character != NULL)) {
            SudekiMpLogWrite(
                "control_separation event=player_two_request source=hotkey "
                "status=rejected reason=release_player_three_first\r\n"
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
    reconcile_companion_requests();
    publish_runtime_player_leases(controller);
    SudekiMpCombatContextSetCharacter(
        1u, companion_actor_publication_ready(
                1u, &companion_controls[0]) ?
            overridden_character : NULL);
    SudekiMpCombatContextSetCharacter(
        2u,
        companion_actor_publication_ready(
                2u, &companion_controls[1]) ?
            companion_controls[1].character : NULL);
    SudekiMpCombatContextSetInputSource(
        1u,
        !player_two_requested || !companion_controls[0].lease_exact ||
                (input_bridge_enabled &&
                 !companion_active_input_lease(
                     1u, &companion_controls[0])) ?
            SUDEKIMP_COMBAT_INPUT_NONE :
            (input_bridge_enabled ?
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE :
                SUDEKIMP_COMBAT_INPUT_KEYBOARD_PROTOTYPE),
        !player_two_requested || !companion_controls[0].lease_exact ||
                (input_bridge_enabled &&
                 !companion_active_input_lease(
                     1u, &companion_controls[0])) ? NULL :
            (input_bridge_enabled ?
                (void *)companion_controls[0].input_identity :
                (void *)second_player_skill_virtual_keys));
    SudekiMpCombatContextSetInputSource(
        2u,
        !companion_controls[1].requested ||
                !companion_controls[1].lease_exact ||
                !companion_active_input_lease(
                    2u, &companion_controls[1]) ?
            SUDEKIMP_COMBAT_INPUT_NONE :
            SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE,
        !companion_controls[1].requested ||
                !companion_controls[1].lease_exact ||
                !companion_active_input_lease(
                    2u, &companion_controls[1]) ? NULL :
            (void *)companion_controls[1].input_identity);
    service_second_player_controller_actions(
        controller, owns_foreground, FALSE, FALSE);
    service_player_three_controller_actions(
        controller, owns_foreground, FALSE, FALSE);
    gameplay_input_allowed =
        !SudekiMpControlSeparationGameplayInputFrozen();
    /* The custom fixed-three panel is deliberately per-seat.  Legacy native
     * QuickMenu retains its singleton/global freeze; a P2 or P3 panel only
     * stops that same companion's movement, combat, and orbit submissions. */
    player_two_gameplay_allowed = gameplay_input_allowed &&
        !SudekiMpSplitScreenQuickMenuActive(1u);
    player_three_gameplay_allowed = gameplay_input_allowed &&
        !SudekiMpSplitScreenQuickMenuActive(2u);
    update_roaming_boundary(controller);
    poll_second_player_movement(
        controller,
        update_data,
        owns_foreground && player_two_gameplay_allowed
    );
    poll_player_three_movement(
        controller,
        owns_foreground && player_three_gameplay_allowed
    );
    poll_second_player_camera_facing(
        controller,
        owns_foreground && player_two_gameplay_allowed
    );
    poll_second_player_weak_attack(
        controller,
        owns_foreground && player_two_gameplay_allowed
    );
    poll_second_player_skills(
        controller,
        owns_foreground && player_two_gameplay_allowed
    );
    poll_second_player_target_trace(
        owns_foreground && player_two_gameplay_allowed
    );
    poll_shared_group_camera(controller);
    notify_update_observers(
        controller,
        update_data,
        dispatch_frame,
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL
    );
}

static void SUDEKIMP_THISCALL poll_control_separation_hotkey(
    void *controller,
    void *update_data
) {
    ControlUpdateDispatchFrame dispatch_frame;
    ControllerUpdateFunction fallback_original;
    DWORD entry_last_error = GetLastError();
    DWORD body_last_error;

    if (!begin_owned_control_update_dispatch(
            &dispatch_frame, FALSE, &fallback_original)) {
        SetLastError(entry_last_error);
        if (fallback_original != NULL) {
            fallback_original(controller, update_data);
        }
        return;
    }
    SetLastError(entry_last_error);
    poll_control_separation_hotkey_body(
        controller, update_data, &dispatch_frame);
    body_last_error = GetLastError();
    end_control_update_dispatch(&dispatch_frame);
    SetLastError(body_last_error);
}

static void quiesce_companion_input(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    if (companion == NULL) {
        return;
    }
    if (seat_index == 1u) {
        quiesce_second_player_input();
    } else {
        stop_companion_movement(seat_index, companion);
        companion->keyboard_weak_was_down = FALSE;
    }
    SudekiMpCombatContextSetInputSource(
        seat_index, SUDEKIMP_COMBAT_INPUT_NONE, NULL);
}

BOOL SudekiMpControlSeparationRequestSeat(
    unsigned int seat_index,
    BOOL enabled
) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    if (original_controller_update == NULL || service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (companion == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (role_lock_active && !enabled) {
        SetLastError(ERROR_LOCK_VIOLATION);
        SudekiMpLogFormat(
            "control_separation event=companion_request player=%u "
            "status=rejected reason=co_op_roles_locked\r\n",
            seat_index + 1u);
        return FALSE;
    }
    if (!SudekiMpControlSeparationSeatRequestTransitionPolicy(
            seat_index,
            enabled,
            companion->requested != (enabled != FALSE),
            companion_controls[0].requested &&
                companion_controls[0].requested_character != NULL,
            companion_controls[1].requested ||
                companion_controls[1].character != NULL)) {
        SetLastError(seat_index == 1u ? ERROR_BUSY : ERROR_NOT_READY);
        SudekiMpLogFormat(
            "control_separation event=companion_request player=%u "
            "status=rejected reason=%s\r\n",
            seat_index + 1u,
            seat_index == 1u ? "release_player_three_first" :
                "exact_player_two_dependency_required");
        return FALSE;
    }
    if (seat_index == 2u && enabled &&
        companion->requested_character == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        SudekiMpLogWrite(
            "control_separation event=companion_request player=3 "
            "status=rejected reason=exact_actor_required\r\n");
        return FALSE;
    }
    if (seat_index == 1u && enabled && companion->requested) {
        /* Idempotent legacy enable must not downgrade a fixed exact P2 actor
         * while P3 depends on it. */
        return TRUE;
    }
    if (seat_index == 1u && !(role_lock_active && enabled)) {
        companion->requested_character = NULL;
    }
    companion->requested = enabled != FALSE;
    companion->request_last_attempt = 0u;
    if (!companion->requested) {
        companion->requested_character = NULL;
        quiesce_companion_input(seat_index);
    }
    SudekiMpLogFormat(
        "control_separation event=companion_request player=%u source=api "
        "state=%s policy=%s\r\n",
        seat_index + 1u,
        companion->requested ? "enabled" : "disabled",
        seat_index == 1u ? "legacy_first_non_front" : "exact_actor_only");
    return TRUE;
}

BOOL SudekiMpControlSeparationRequestPlayerTwo(BOOL enabled) {
    return SudekiMpControlSeparationRequestSeat(1u, enabled);
}

BOOL SudekiMpControlSeparationRequestSeatCharacter(
    unsigned int seat_index,
    void *character
) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    unsigned int slot_index;

    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (companion == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (role_lock_active &&
        (character == NULL || character != companion->requested_character)) {
        SetLastError(ERROR_LOCK_VIOLATION);
        SudekiMpLogFormat(
            "control_separation event=companion_request player=%u "
            "source=roster status=rejected reason=co_op_roles_locked\r\n",
            seat_index + 1u);
        return FALSE;
    }
    if (!SudekiMpControlSeparationSeatRequestTransitionPolicy(
            seat_index,
            character != NULL,
            character != companion->requested_character,
            companion_controls[0].requested &&
                companion_controls[0].requested_character != NULL,
            companion_controls[1].requested ||
                companion_controls[1].character != NULL)) {
        SetLastError(seat_index == 1u ? ERROR_BUSY : ERROR_NOT_READY);
        SudekiMpLogFormat(
            "control_separation event=companion_request player=%u "
            "source=roster status=rejected reason=%s\r\n",
            seat_index + 1u,
            seat_index == 1u ? "release_player_three_first" :
                "exact_player_two_dependency_required");
        return FALSE;
    }
    if ((character != NULL && companion->requested &&
         companion->requested_character == character) ||
        (character == NULL && !companion->requested &&
         companion->requested_character == NULL)) {
        if (character == NULL) {
            quiesce_companion_input(seat_index);
        }
        return TRUE;
    }
    if (character != NULL) {
        if (seat_index == 2u &&
            (SudekiMpLocalInputHubRequestedMask() & 0x04u) == 0u) {
            SetLastError(ERROR_NOT_READY);
            return FALSE;
        }
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
            slot_index == 0u || character == controller_target ||
            companion_character_reserved_by_other_seat(
                seat_index, character)) {
            SetLastError(ERROR_NOT_FOUND);
            return FALSE;
        }
    }
    companion->requested_character = character;
    companion->requested = character != NULL;
    companion->request_last_attempt = 0u;
    if (!companion->requested) {
        quiesce_companion_input(seat_index);
    }
    SudekiMpLogFormat(
        "control_separation event=companion_request player=%u source=roster "
        "state=%s character=0x%08lx policy=exact_selected_party_member\r\n",
        seat_index + 1u,
        character != NULL ? "enabled" : "disabled",
        (unsigned long)(uintptr_t)character
    );
    return TRUE;
}

BOOL SudekiMpControlSeparationRequestPlayerTwoCharacter(void *character) {
    return SudekiMpControlSeparationRequestSeatCharacter(1u, character);
}

BOOL SudekiMpControlSeparationReleaseSeatNow(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);
    BOOL transition_exact;

    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (companion == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (role_lock_active) {
        SetLastError(ERROR_LOCK_VIOLATION);
        return FALSE;
    }
    if (!SudekiMpControlSeparationSeatReleaseOrderPolicy(
            seat_index, companion_controls[1].character != NULL) ||
        (seat_index == 1u && companion_controls[1].requested)) {
        SetLastError(ERROR_BUSY);
        SudekiMpLogWrite(
            "control_separation event=companion_transition_release player=2 "
            "status=rejected reason=release_player_three_first\r\n");
        return FALSE;
    }
    companion->requested_character = NULL;
    companion->requested = FALSE;
    companion->request_last_attempt = 0u;
    quiesce_companion_input(seat_index);
    if (companion->character == NULL) {
        return TRUE;
    }
    transition_exact = transition_companion_ai(seat_index);
    if (!transition_exact || companion->character != NULL) {
        SetLastError(ERROR_BUSY);
        SudekiMpLogFormat(
            "control_separation event=companion_transition_release player=%u "
            "status=pending reason=native_ai_restore_not_verified\r\n",
            seat_index + 1u);
        return FALSE;
    }
    SudekiMpLogFormat(
        "control_separation event=companion_transition_release player=%u "
        "status=confirmed "
        "policy=synchronous_game_thread_native_ai_restore\r\n",
        seat_index + 1u);
    return TRUE;
}

BOOL SudekiMpControlSeparationReleasePlayerTwoNow(void) {
    return SudekiMpControlSeparationReleaseSeatNow(1u);
}

BOOL SudekiMpControlSeparationSetRoleLock(BOOL enabled) {
    unsigned int seat_index;
    BOOL fixed_three_contract;

    if (service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    fixed_three_contract = enabled && fixed_three_control_contract_active();
    if (enabled) {
        for (seat_index = CONTROL_COMPANION_FIRST_SEAT;
             seat_index <= CONTROL_COMPANION_LAST_SEAT;
             ++seat_index) {
            SudekiMpCompanionControlRuntime *companion =
                companion_control_for_seat(seat_index);

            if (companion->requested_character != NULL &&
                (!companion->lease_exact ||
                 companion->character != companion->requested_character ||
                 (fixed_three_contract &&
                  !companion_active_input_lease(
                      seat_index, companion)))) {
                SetLastError(ERROR_INVALID_STATE);
                SudekiMpLogFormat(
                    "control_separation event=co_op_roles state=rejected "
                    "player=%u reason=selected_companion_control_or_input_"
                    "lease_not_exact\r\n",
                    seat_index + 1u);
                return FALSE;
            }
        }
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

    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
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

BOOL SudekiMpControlSeparationSeatRequested(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    return companion != NULL && companion->requested;
}

BOOL SudekiMpControlSeparationSeatActive(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    return companion != NULL && companion->lease_exact &&
        companion->character != NULL;
}

void *SudekiMpControlSeparationSeatCharacter(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    return companion != NULL && companion->lease_exact ?
        companion->character : NULL;
}

BOOL SudekiMpControlSeparationSeatInputReady(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    return companion_input_ready(seat_index, companion);
}

BOOL SudekiMpControlSeparationSeatInputLeaseActive(unsigned int seat_index) {
    SudekiMpCompanionControlRuntime *companion =
        companion_control_for_seat(seat_index);

    return companion != NULL && companion->requested &&
        companion->lease_exact && companion->character != NULL &&
        companion_active_input_lease(seat_index, companion);
}

BOOL SudekiMpControlSeparationFilterSpiritRootDelta(
    BOOL direct_movement_owned,
    const float input[3],
    float output[3]
) {
    if (input == NULL || output == NULL || !isfinite(input[0]) ||
        !isfinite(input[1]) || !isfinite(input[2])) return FALSE;
    output[0] = direct_movement_owned ? 0.0f : input[0];
    output[1] = input[1];
    output[2] = direct_movement_owned ? 0.0f : input[2];
    return TRUE;
}

BOOL SudekiMpControlSeparationTalSkillDirectMovementPolicy(
    BOOL scope_exact,
    BOOL skills_known,
    BOOL own_skill_active,
    BOOL remote_skill_active,
    BOOL spirit_known,
    BOOL spirit_active,
    uint32_t arbiter_flags
) {
    return scope_exact && skills_known && !own_skill_active &&
        remote_skill_active && spirit_known && !spirit_active &&
        (arbiter_flags & (0x0289e568u & ~0x00080000u)) == 0u;
}

BOOL SudekiMpControlSeparationTalSkillFilterRestorePolicy(
    BOOL scope_exact, int current_filter, int requested_filter
) {
    return scope_exact && current_filter == 0 && requested_filter == 0;
}

float SudekiMpControlSeparationTalSkillMovementMagnitude(float native_speed) {
    if (!isfinite(native_speed) || native_speed < 0.0f || native_speed > 4.0f)
        return 0.0f;
    return native_speed > 1.0f ? 1.0f : native_speed;
}

static BOOL tal_native_locomotion_owns_movement(void) {
    SudekiMpCleanroomActorPresentation presentation;
    uint8_t action;
    if (!SudekiMpCleanroomEngineActorPresentation(
            SUDEKIMP_CLEANROOM_TAL, &presentation)) return FALSE;
    /* Never suppress an attack's authored movement, including its terminal
     * selector before native idle retirement. Reuse the wire classifier. */
    return !SudekiMpLanArenaTalActionFromNativePresentation(
        presentation.selector[0], 1u, &action);
}

static BOOL tal_skill_direct_actor_exact(uint8_t *character) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[0];
    SudekiMpCharacterSkillState own_skill;
    SudekiMpCharacterSkillState remote_skill;
    uint8_t *arbiter;
    uint8_t *controller;
    int spirit_state = 0;
    if (!lan_arena_remote_input_enabled ||
        !player_one_skill_input_isolation_enabled ||
        character == NULL || character != SudekiMpCleanroomEngineActorEntity(
            SUDEKIMP_CLEANROOM_TAL) ||
        !character_is_in_active_group(character) ||
        !companion->requested || !companion->lease_exact ||
        !companion_character_is_in_active_group(companion) ||
        companion->character != SudekiMpCleanroomEngineActorEntity(
            SUDEKIMP_CLEANROOM_AILISH) ||
        !readable_memory(character, 0x94u)) return FALSE;
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    arbiter = *(uint8_t **)(character + 0x90u);
    if (!readable_memory(controller, CONTROLLER_TARGET_OFFSET + sizeof(void *)) ||
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) != character ||
        !readable_memory(arbiter, 0x54u) ||
        *(void **)(arbiter + 0x10u) != character ||
        !SudekiMpObserveCharacterSkill(character, &own_skill) ||
        !SudekiMpObserveCharacterSkill(companion->character, &remote_skill) ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state)) {
        return FALSE;
    }
    return SudekiMpControlSeparationTalSkillDirectMovementPolicy(
        TRUE, TRUE, own_skill.active != 0u, remote_skill.active != 0u,
        TRUE, spirit_state != 0, *(uint32_t *)(arbiter + 0x50u));
}

/* Only the animation-root callsite is adapted. Direct player deltas, ranged
 * movement, collision response and every other caller retain the native path.
 * EAX carries the vector; the controller is the one callee-cleaned argument. */
static void __attribute__((regparm(1), stdcall))
filter_lan_spirit_animation_root(const float *delta, void *movement) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[0];
    uint8_t *character = companion->character;
    uint8_t *arbiter = NULL;
    float filtered[3];
    int spirit_state = 0;
    SudekiMpCharacterSkillState own_skill;
    BOOL exact = lan_arena_remote_input_enabled &&
        lan_arena_player_two_skill_input_isolation_enabled &&
        companion->requested && companion->lease_exact &&
        character != NULL && character == lan_spirit_direct_actor &&
        lan_spirit_direct_at_ms != 0u &&
        (DWORD)(GetTickCount() - lan_spirit_direct_at_ms) <= 125u &&
        companion_character_is_in_active_group(companion) &&
        character == SudekiMpCleanroomEngineActorEntity(
            SUDEKIMP_CLEANROOM_AILISH) &&
        readable_memory(character, 0x94u) &&
        *(void **)(character + 0x80u) == movement &&
        readable_memory(movement, 0x14u) &&
        *(void **)((uint8_t *)movement + 0x10u) == character;
    if (exact) {
        arbiter = *(uint8_t **)(character + 0x90u);
        exact = readable_memory(arbiter, 0x54u) &&
            *(void **)(arbiter + 0x10u) == character &&
            SudekiMpObserveCharacterSkill(character, &own_skill) &&
            own_skill.active == 0u &&
            SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state) &&
            SudekiMpControlSeparationSpiritDirectMovementPolicy(
                TRUE, spirit_state != 0, *(uint32_t *)(arbiter + 0x50u));
    }
    if (!exact && lan_tal_skill_direct_actor != NULL &&
        lan_tal_skill_direct_at_ms != 0u &&
        (DWORD)(GetTickCount() - lan_tal_skill_direct_at_ms) <= 125u &&
        tal_skill_direct_actor_exact(lan_tal_skill_direct_actor) &&
        tal_native_locomotion_owns_movement()) {
        character = lan_tal_skill_direct_actor;
        exact = *(void **)(character + 0x80u) == movement &&
            readable_memory(movement, 0x14u) &&
            *(void **)((uint8_t *)movement + 0x10u) == character;
    }
    if (exact && readable_memory(delta, sizeof(filtered)) &&
        SudekiMpControlSeparationFilterSpiritRootDelta(TRUE, delta, filtered)) {
        original_animation_root_movement(filtered, movement);
    } else {
        original_animation_root_movement(delta, movement);
    }
}

BOOL SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(BOOL enabled) {
    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (enabled && !lan_spirit_root_movement_call_hook.installed) {
        static const uint8_t entry[] = {
            0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x81, 0xec,
            0x94, 0x00, 0x00, 0x00
        };
        if (memcmp(game_base + RVA_MOVEMENT_RELATIVE_DELTA,
                entry, sizeof(entry)) != 0) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        original_animation_root_movement = (MovementRelativeDeltaFunction)(
            game_base + RVA_MOVEMENT_RELATIVE_DELTA);
        if (!SudekiMpInstallRelativeCallHook(
                &lan_spirit_root_movement_call_hook,
                game_base + RVA_ANIMATION_ROOT_MOVEMENT_CALL,
                original_animation_root_movement,
                filter_lan_spirit_animation_root)) return FALSE;
    }
    lan_arena_remote_input_enabled = enabled != FALSE;
    if (!lan_arena_remote_input_enabled) {
        lan_tal_skill_direct_actor = NULL;
        lan_tal_skill_direct_at_ms = 0u;
        lan_spirit_direct_actor = NULL;
        lan_spirit_direct_at_ms = 0u;
        stop_companion_movement(1u, &companion_controls[0]);
        if (!SudekiMpRestoreRelativeCallHook(
                &lan_spirit_root_movement_call_hook)) return FALSE;
        original_animation_root_movement = NULL;
    }
    SudekiMpLogFormat(
        "control_separation event=lan_arena_remote_input state=%s "
        "policy=authenticated_host_only_native_ailish_arbiter\r\n",
        lan_arena_remote_input_enabled ? "enabled" : "disabled"
    );
    return TRUE;
}

BOOL SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(BOOL enabled) {
    BOOL next_enabled;

    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    next_enabled = enabled != FALSE;
    if (player_one_skill_input_isolation_enabled == next_enabled) {
        return TRUE;
    }
    player_one_skill_input_isolation_enabled = next_enabled;
    player_one_skill_native_input_restored = FALSE;
    player_one_skill_input_isolation_trace_state = -1;
    if (!next_enabled) {
        lan_tal_skill_direct_actor = NULL;
        lan_tal_skill_direct_at_ms = 0u;
        player_one_skill_arbiter_virtualization_logged = FALSE;
        player_one_skill_direct_movement_scope_active = FALSE;
        player_one_skill_direct_movement_submitted = FALSE;
        player_one_skill_direct_movement_operator_override = FALSE;
        player_one_skill_frame_delta = 0.0f;
        player_one_skill_direct_movement_last_trace_tick = 0u;
    }
    SudekiMpLogFormat(
        "control_separation event=player_one_skill_input_isolation "
        "state=%s policy=remote_Ailish_skill_only_Tal_controller_boundary\r\n",
        next_enabled ? "enabled" : "disabled");
    return TRUE;
}

static void call_position_set_forward(void *position, const float direction[3]);

BOOL SudekiMpControlSeparationApplyLanArenaPlayerOneSkillMovement(
    void *arbiter,
    const float direction[3],
    float speed
) {
    uint8_t *controller;
    uint8_t *character;
    uint8_t *movement_controller;
    void *position;
    float facing[3];
    float direct_move_speed;
    float horizontal_length;
    float normalized_x;
    float normalized_z;
    float direct_scale;
    DWORD now;

    if (!player_one_skill_input_isolation_enabled ||
        !player_one_skill_direct_movement_scope_active ||
        player_one_skill_direct_movement_submitted ||
        game_base == NULL || movement_controller_set_absolute_delta == NULL ||
        direction == NULL || !readable_memory(arbiter, 0x54u) ||
        !isfinite(direction[0]) || !isfinite(direction[2]) ||
        !isfinite(speed) || speed < 0.0f || speed > 4.0f ||
        player_one_skill_frame_delta <= 0.0f) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    character = *(uint8_t **)((uint8_t *)arbiter + 0x10u);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    movement_controller = readable_memory(character, 0x84u) ?
        *(uint8_t **)(character + 0x80u) : NULL;
    if (!readable_memory(controller,
            CONTROLLER_TARGET_OFFSET + sizeof(void *)) ||
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) != character ||
        !readable_memory(movement_controller, 0xbfu) ||
        *(void **)(movement_controller + 0x10u) != character ||
        !tal_skill_direct_actor_exact(character) ||
        !tal_native_locomotion_owns_movement()) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    direct_move_speed = *(float *)(controller + 0x1d4u);
    horizontal_length = sqrtf(
        direction[0] * direction[0] + direction[2] * direction[2]);
    if (!isfinite(direct_move_speed) || direct_move_speed <= 0.0f ||
        direct_move_speed > 100.0f || !isfinite(horizontal_length) ||
        (speed > 0.0f && horizontal_length <= 0.0001f)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    position = *(void **)(character + 0x44u);
    if (!readable_memory(position, 0xbcu) ||
        *(void **)position != game_base + 0x002cdefcu ||
        *(void **)((uint8_t *)position + 0x10u) != character ||
        position_set_forward == NULL ||
        !SudekiMpControlSeparationForceStopCharacter(character)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    normalized_x = speed > 0.0f ? direction[0] / horizontal_length : 0.0f;
    normalized_z = speed > 0.0f ? direction[2] / horizontal_length : 0.0f;
    /* The task lock is restored before native turning/root movement runs.
     * Commit the accepted world direction now, then let the shared root seam
     * remove only duplicate horizontal animation movement for this lease. */
    if (speed > 0.0f) {
        facing[0] = normalized_x;
        facing[1] = 0.0f;
        facing[2] = normalized_z;
        call_position_set_forward(position, facing);
    }
    /* The native movement callsite supplies keyboard magnitudes near 1.5
     * (and 1.8 diagonally), not a world-speed multiplier. Both this callsite
     * and the controller-tail fallback must saturate at the same run pace. */
    direct_scale = SudekiMpControlSeparationTalSkillMovementMagnitude(speed) *
        direct_move_speed *
        spirit_noncaster_direct_movement_pace *
        player_one_skill_frame_delta;
    movement_controller_set_absolute_delta(
        movement_controller,
        normalized_x * direct_scale,
        0.0f,
        normalized_z * direct_scale);
    player_one_skill_direct_movement_submitted = TRUE;
    now = GetTickCount();
    lan_tal_skill_direct_actor = character;
    lan_tal_skill_direct_at_ms = now;
    if (player_one_skill_direct_movement_last_trace_tick == 0u ||
        (DWORD)(now -
            player_one_skill_direct_movement_last_trace_tick) >= 500u) {
        player_one_skill_direct_movement_last_trace_tick = now;
        SudekiMpLogFormat(
            "control_separation event=player_one_skill_direct_movement "
            "phase=submit character=0x%08lx direction_bits=%08lx,%08lx "
            "frame_delta_bits=0x%08lx direct_scale_bits=0x%08lx source=%s "
            "policy=LAN_host_Tal_absolute_delta_only_during_authenticated_remote_skill_scope\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)float_bits(normalized_x),
            (unsigned long)float_bits(normalized_z),
            (unsigned long)float_bits(player_one_skill_frame_delta),
            (unsigned long)float_bits(direct_scale),
            player_one_skill_direct_movement_operator_override ?
                "local_operator_forward" : "native_physical_axes");
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
    BOOL enabled
) {
    BOOL next_enabled;

    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    next_enabled = enabled != FALSE;
    if (lan_arena_player_two_skill_input_isolation_enabled == next_enabled) {
        return TRUE;
    }
    lan_arena_player_two_skill_input_isolation_enabled = next_enabled;
    if (!next_enabled) {
        lan_arena_player_two_skill_virtualization_logged = FALSE;
        lan_arena_player_two_skill_direct_movement_last_trace_tick = 0u;
        lan_spirit_direct_actor = NULL;
        lan_spirit_direct_at_ms = 0u;
    }
    SudekiMpLogFormat(
        "control_separation event=lan_arena_player_two_skill_input_isolation "
        "state=%s policy=Tal_skill_non_caster_Ailish_remote_input_boundary\r\n",
        next_enabled ? "enabled" : "disabled");
    return TRUE;
}

BOOL SudekiMpControlSeparationSetManualToggleEnabled(BOOL enabled) {
    if (original_controller_update == NULL || game_base == NULL ||
        service_only_mode) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    manual_toggle_enabled = enabled != FALSE;
    hotkey_was_down = FALSE;
    SudekiMpLogFormat(
        "control_separation event=manual_toggle state=%s "
        "policy=lan_named_actor_lease_cannot_be_retargeted_by_local_hotkey\r\n",
        manual_toggle_enabled ? "enabled" : "disabled");
    return TRUE;
}

BOOL SudekiMpControlSeparationLanArenaRemoteSubmissionPolicy(
    BOOL remote_session_authenticated,
    BOOL player_two_requested_value,
    BOOL player_two_lease_exact,
    BOOL character_in_active_group,
    BOOL native_control_state_exact,
    BOOL direction_finite,
    BOOL weak_attack_edge
) {
    (void)weak_attack_edge;
    return SudekiMpLanArenaHostRemoteInputAllowed(
        remote_session_authenticated,
        player_two_requested_value,
        player_two_lease_exact,
        character_in_active_group,
        native_control_state_exact,
        direction_finite);
}

__attribute__((naked, noinline, used))
static void call_position_set_forward(
    void *position __attribute__((unused)),
    const float direction[3] __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "movl 12(%esp), %ecx\n\t"
        "call *_position_set_forward\n\t"
        "popl %esi\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static uint8_t call_first_person_held_fire(
    void *ranged_state __attribute__((unused))
) {
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "call *_lan_arena_first_person_held_fire\n\t"
        "ret\n\t"
    );
}

static uint8_t *lan_arena_player_two_missile_manager(void) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[0];
    uint8_t *character = (uint8_t *)companion->character;
    uint8_t *candidate;
    uint8_t *resolved = NULL;
    size_t offset;
    unsigned int matches = 0u;
    if (game_base == NULL || character == NULL || !companion->requested ||
        !companion->lease_exact ||
        !companion_character_is_in_active_group(companion)) {
        lan_arena_cached_missile_manager = NULL;
        lan_arena_cached_missile_owner = NULL;
        return NULL;
    }
    candidate = (uint8_t *)lan_arena_cached_missile_manager;
    if (lan_arena_cached_missile_owner == character &&
        readable_memory(candidate, 0xe1u) &&
        *(void **)candidate == game_base + RVA_MISSILE_MANAGER_VTABLE &&
        *(void **)(candidate + 0x10u) == character) {
        return candidate;
    }
    /* Player entity subclasses place CMissileManager at different offsets.
     * Resolve the exact subobject structurally: its supported-build vtable and
     * native owner backpointer must both name this immutable actor lease. */
    for (offset = 0u; offset < 0x2000u; offset += sizeof(void *)) {
        candidate = character + offset;
        if (!readable_memory(candidate, 0xe1u)) continue;
        if (*(void **)candidate == game_base + RVA_MISSILE_MANAGER_VTABLE &&
            *(void **)(candidate + 0x10u) == character) {
            resolved = candidate;
            ++matches;
        }
    }
    if (matches != 1u) {
        lan_arena_cached_missile_manager = NULL;
        lan_arena_cached_missile_owner = NULL;
        return NULL;
    }
    lan_arena_cached_missile_manager = resolved;
    lan_arena_cached_missile_owner = character;
    SudekiMpLogFormat(
        "control_separation event=lan_arena_missile_manager state=resolved "
        "character=0x%08lx manager=0x%08lx actor_offset=0x%04lx "
        "policy=unique_vtable_and_owner_backpointer_read_only_gate\r\n",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)resolved,
        (unsigned long)(resolved - character));
    return resolved;
}

BOOL SudekiMpControlSeparationLanArenaPlayerTwoRangedReady(
    BOOL *ready,
    uint16_t *authored_delay_half_result
) {
    uint8_t *manager;
    uint8_t *selected_weapon;
    BOOL can_fire;
    BOOL is_firing;
    uint16_t authored_delay_half = 0u;
    uint16_t authored_cycle_half = 0u;
    float native_cooldown = NAN;
    int trace_state;
    if (ready == NULL || authored_delay_half_result == NULL ||
        missile_manager_can_fire == NULL ||
        missile_manager_is_firing == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    manager = lan_arena_player_two_missile_manager();
    if (manager == NULL) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    can_fire = missile_manager_can_fire(manager) != 0u;
    is_firing = missile_manager_is_firing(manager) != 0u;
    selected_weapon = readable_memory(manager, 0x64u) ?
        *(uint8_t **)(manager + 0x60u) : NULL;
    if (readable_memory(selected_weapon, 0xc4u)) {
        authored_delay_half = *(uint16_t *)(selected_weapon + 0xb8u);
        authored_cycle_half = *(uint16_t *)(selected_weapon + 0xbau);
        native_cooldown = *(float *)(selected_weapon + 0xc0u);
    }
    *ready = can_fire && !is_firing;
    *authored_delay_half_result = authored_delay_half;
    trace_state = *ready ? 1 : 0;
    if (trace_state != lan_arena_missile_ready_trace_state) {
        lan_arena_missile_ready_trace_state = trace_state;
        SudekiMpLogFormat(
            "control_separation event=lan_arena_missile_cadence "
            "state=%s native_state=%u can_fire=%s is_firing=%s "
            "selected=0x%08lx authored_delay_half=0x%04x "
            "authored_cycle_half=0x%04x cooldown=%.6f "
            "policy=retail_CMissileManager_predicates_and_read_only_weapon_timing\r\n",
            *ready ? "ready" : "busy",
            (unsigned int)manager[0xe0u],
            can_fire ? "true" : "false",
            is_firing ? "true" : "false",
            (unsigned long)(uintptr_t)selected_weapon,
            (unsigned int)authored_delay_half,
            (unsigned int)authored_cycle_half,
            native_cooldown);
    }
    return TRUE;
}

BOOL SudekiMpControlSeparationSpiritDirectMovementPolicy(
    BOOL skill_scope_exact,
    BOOL spirit_active,
    uint32_t arbiter_flags
) {
    return skill_scope_exact && spirit_active &&
        (arbiter_flags & 0x0289e568u) == 0x00080000u;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
    float world_direction_x,
    float world_direction_z,
    float aim_direction_x,
    float aim_direction_z,
    BOOL aim_direction_valid,
    BOOL weak_attack_active,
    float frame_delta_seconds
) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[0];
    uint8_t *character = (uint8_t *)companion->character;
    uint8_t *component;
    uint8_t *mode_state;
    uint8_t *controller;
    void *arbiter;
    void *controller_target;
    void *position;
    float direction[3];
    float aim_direction[3];
    float magnitude;
    float aim_magnitude;
    BOOL native_control_state_exact;
    uint32_t *arbiter_flags = NULL;
    uint32_t saved_arbiter_flags = 0u;
    uint8_t *movement_controller = NULL;
    float direct_move_speed = 0.0f;
    BOOL skill_input_scope_exact = FALSE;
    int spirit_state = 0;
    BOOL spirit_active = FALSE;
    BOOL spirit_direct_movement = FALSE;
    BOOL submission_ok = TRUE;
    BOOL spirit_direct_serviced = FALSE;
    BOOL directional_gait = FALSE;
    unsigned int gait_mode = 0u;
    float gait_heading[3] = {0.0f, 0.0f, 0.0f};
    SudekiMpCharacterSkillState own_skill;

    if (!isfinite(world_direction_x) || !isfinite(world_direction_z) ||
        fabsf(world_direction_x) > 1.0f || fabsf(world_direction_z) > 1.0f ||
        (aim_direction_valid &&
         (!isfinite(aim_direction_x) || !isfinite(aim_direction_z) ||
          fabsf(aim_direction_x) > 1.0f || fabsf(aim_direction_z) > 1.0f))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!isfinite(frame_delta_seconds) || frame_delta_seconds < 0.0f ||
        frame_delta_seconds > 0.25f) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    component = character == NULL ? NULL : *(uint8_t **)(character + 0x94u);
    mode_state = component == NULL ? NULL : *(uint8_t **)(component + 0x3cu);
    arbiter = character == NULL ? NULL : *(void **)(character + 0x90u);
    controller = game_base == NULL ? NULL : *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    controller_target = controller == NULL ? NULL :
        *(void **)(controller + CONTROLLER_TARGET_OFFSET);
    native_control_state_exact = component != NULL &&
        component == companion->ai_component && mode_state != NULL &&
        readable_memory(arbiter, 0x64u) &&
        *(void **)(character + 0xacu) != NULL &&
        *(int16_t *)(component + 0x16au) == 1 && *(mode_state + 0x0bu) == 0u &&
        character != controller_target && arbiter_movement != NULL;
    if (!SudekiMpControlSeparationLanArenaRemoteSubmissionPolicy(
            lan_arena_remote_input_enabled,
            companion->requested,
            companion->lease_exact && character != NULL,
            companion_character_is_in_active_group(companion),
            native_control_state_exact,
            TRUE,
            weak_attack_active)) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    direction[0] = world_direction_x;
    direction[1] = 0.0f;
    direction[2] = world_direction_z;
    magnitude = sqrtf(direction[0] * direction[0] + direction[2] * direction[2]);
    if (lan_arena_player_two_skill_input_isolation_enabled &&
        readable_memory(arbiter, 0x54u) &&
        *(void **)((uint8_t *)arbiter + 0x10u) == character) {
        skill_input_scope_exact = TRUE;
        arbiter_flags = (uint32_t *)((uint8_t *)arbiter + 0x50u);
        if (readable_memory(arbiter_flags, sizeof(*arbiter_flags)) &&
            (*arbiter_flags & 0x0289e568u) == 0x00080000u) {
            saved_arbiter_flags = *arbiter_flags;
            *arbiter_flags = saved_arbiter_flags & ~0x00080000u;
            if (!lan_arena_player_two_skill_virtualization_logged) {
                lan_arena_player_two_skill_virtualization_logged = TRUE;
                SudekiMpLogFormat(
                    "control_separation event=lan_arena_player_two_skill_input_isolation "
                    "state=arbiter_lock_virtualized character=0x%08lx "
                    "flags_before=0x%08lx virtualized_flag=0x00080000 "
                    "policy=Tal_skill_non_caster_Ailish_only\r\n",
                    (unsigned long)(uintptr_t)character,
                    (unsigned long)saved_arbiter_flags);
            }
        }
    }
    spirit_active = skill_input_scope_exact &&
        SudekiMpCleanroomEngineSpiritPresentationState(&spirit_state) &&
        spirit_state != 0;
    spirit_direct_movement =
        SudekiMpControlSeparationSpiritDirectMovementPolicy(
            skill_input_scope_exact, spirit_active, saved_arbiter_flags) &&
        SudekiMpObserveCharacterSkill(character, &own_skill) &&
        own_skill.active == 0u;
    if (magnitude > 0.0001f) {
        direction[0] /= magnitude;
        direction[2] /= magnitude;
        if (magnitude > 1.0f) magnitude = 1.0f;
        directional_gait = aim_direction_valid && !skill_input_scope_exact &&
            SudekiMpControlSeparationDirectionalGait(
                direction[0], direction[2], aim_direction_x, aim_direction_z,
                &gait_mode, &gait_heading[0], &gait_heading[2]);
        arbiter_movement(arbiter, directional_gait ? gait_heading : direction,
            magnitude, 1.0f, directional_gait ? gait_mode : 0u);
        movement_controller = readable_memory(character, 0x84u) ?
            *(uint8_t **)(character + 0x80u) : NULL;
        direct_move_speed = readable_memory(controller, 0x1d8u) ?
            *(float *)(controller + 0x1d4u) : 0.0f;
        if (skill_input_scope_exact &&
            (!spirit_active || spirit_direct_movement) &&
            readable_memory(movement_controller, 0xbfu) &&
            movement_controller_set_absolute_delta != NULL &&
            frame_delta_seconds > 0.0f &&
            isfinite(direct_move_speed) && direct_move_speed > 0.0f &&
            direct_move_speed <= 100.0f) {
            float direct_scale = magnitude * direct_move_speed *
                spirit_noncaster_direct_movement_pace * frame_delta_seconds;
            DWORD now = GetTickCount();
            /* Spirit restores the arbiter lock before native world update:
             * the normal turning update then rejects the direction just
             * accepted above. Animation root displacement is independently
             * additive (filtered at its own callsite for this lease). Quiesce
             * the native speed request, not the animation compositor, and
             * commit the same authenticated world direction to CPosition.
             * Both native APIs are already exact-image validated owners. */
            if (spirit_direct_movement) {
                position = *(void **)(character + 0x44u);
                if (!readable_memory(position, 0xbcu) ||
                    *(void **)position != game_base + 0x002cdefcu ||
                    *(void **)((uint8_t *)position + 0x10u) != character ||
                    *(void **)(movement_controller + 0x10u) != character ||
                    position_set_forward == NULL) {
                    submission_ok = FALSE;
                    SetLastError(ERROR_INVALID_DATA);
                    goto restore_remote_arbiter_flags;
                }
                if (!SudekiMpControlSeparationForceStopCharacter(character)) {
                    submission_ok = FALSE;
                    goto restore_remote_arbiter_flags;
                }
                if (!aim_direction_valid && position_set_forward != NULL) {
                    call_position_set_forward(position, direction);
                }
            }
            movement_controller_set_absolute_delta(
                movement_controller,
                direction[0] * direct_scale,
                0.0f,
                direction[2] * direct_scale);
            spirit_direct_serviced = spirit_direct_movement;
            if (lan_arena_player_two_skill_direct_movement_last_trace_tick ==
                    0u ||
                (DWORD)(now -
                    lan_arena_player_two_skill_direct_movement_last_trace_tick) >=
                    500u) {
                lan_arena_player_two_skill_direct_movement_last_trace_tick = now;
                SudekiMpLogFormat(
                    "control_separation event=lan_arena_player_two_skill_direct_movement "
                    "phase=submit character=0x%08lx direction_bits=%08lx,%08lx "
                    "frame_delta_bits=0x%08lx direct_scale_bits=0x%08lx "
                    "policy=native_absolute_delta_only_during_authenticated_Tal_skill_scope\r\n",
                    (unsigned long)(uintptr_t)character,
                    (unsigned long)float_bits(direction[0]),
                    (unsigned long)float_bits(direction[2]),
                    (unsigned long)float_bits(frame_delta_seconds),
                    (unsigned long)float_bits(direct_scale));
            }
        }
        companion->movement_active = TRUE;
        companion->movement_magnitude = magnitude;
    } else {
        stop_companion_movement(1u, companion);
        if (spirit_direct_movement) {
            submission_ok = SudekiMpControlSeparationForceStopCharacter(
                character);
            spirit_direct_serviced = submission_ok;
        }
    }
    /* The authenticated remote first-person bit is folded into
     * aim_direction_valid by the LAN runtime. Preserve ordinary third-person
     * turning and replace body forward only while that remote camera owns
     * Ailish's aim; the host need not own the same local camera mode. */
    if (aim_direction_valid && !directional_gait && position_set_forward != NULL) {
        position = *(void **)(character + 0x44u);
        aim_direction[0] = aim_direction_x;
        aim_direction[1] = 0.0f;
        aim_direction[2] = aim_direction_z;
        aim_magnitude = sqrtf(
            aim_direction[0] * aim_direction[0] +
            aim_direction[2] * aim_direction[2]);
        if (readable_memory(position, 0x5cu) &&
            isfinite(aim_magnitude) && aim_magnitude > 0.0001f) {
            aim_direction[0] /= aim_magnitude;
            aim_direction[2] /= aim_magnitude;
            /* At rest, enter through the same native arbiter direction path
             * used by an AI-controlled companion. This updates the actor's
             * accepted facing state as well as CPosition, so the next native
             * update cannot immediately restore the old cardinal direction.
             * Moving remains owned by the preceding world-direction submit;
             * forcing a zero-speed aim submit there would cancel translation. */
            if (magnitude <= 0.0001f) {
                arbiter_movement(
                    arbiter, aim_direction, 0.0f, 1.0f, 0u);
            }
            call_position_set_forward(position, aim_direction);
        }
    }
    if (weak_attack_active) {
        SudekiMpSubmitArbiterCombatInput(
            game_base + RVA_ARBITER_COMBAT_INPUT, arbiter,
            1, 0, 0, 0, 0, 0);
    }
restore_remote_arbiter_flags:
    if (spirit_direct_serviced && submission_ok) {
        lan_spirit_direct_actor = character;
        lan_spirit_direct_at_ms = GetTickCount();
    } else {
        lan_spirit_direct_actor = NULL;
        lan_spirit_direct_at_ms = 0u;
    }
    if (saved_arbiter_flags != 0u &&
        readable_memory(arbiter_flags, sizeof(*arbiter_flags))) {
        *arbiter_flags = (*arbiter_flags & ~0x00080000u) |
            (saved_arbiter_flags & 0x00080000u);
    }
    return submission_ok;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoRangedFire(void) {
    SudekiMpCompanionControlRuntime *companion = &companion_controls[0];
    uint8_t *character = (uint8_t *)companion->character;
    uint8_t *component = NULL;
    uint8_t *mode_state = NULL;
    uint8_t *ranged_state = NULL;
    uint8_t *weapon_owner = NULL;
    uint8_t *weapon = NULL;
    uint8_t *weapon_state = NULL;
    int validation_state;
    const char *validation_reason;

    if (lan_arena_first_person_held_fire == NULL) {
        validation_state = 0;
        validation_reason = "native_entry_unavailable";
    } else if (!lan_arena_remote_input_enabled ||
        !companion->requested || !companion->lease_exact ||
        !companion_character_is_in_active_group(companion) ||
        !readable_memory(character, 0xf4u)) {
        validation_state = 1;
        validation_reason = "actor_lease_not_exact";
    } else {
        component = *(uint8_t **)(character + 0x94u);
        mode_state = readable_memory(component, 0x40u) ?
            *(uint8_t **)(component + 0x3cu) : NULL;
        ranged_state = *(uint8_t **)(character + 0xf0u);
        weapon_owner = readable_memory(ranged_state, 0x30u) ?
            *(uint8_t **)(ranged_state + 0x2cu) : NULL;
        weapon = readable_memory(weapon_owner, 0x74u) ?
            *(uint8_t **)(weapon_owner + 0x70u) : NULL;
        weapon_state = readable_memory(weapon, 0x40u) ?
            *(uint8_t **)(weapon + 0x3cu) : NULL;
        if (component == NULL || component != companion->ai_component ||
            !readable_memory(component, 0x16cu) ||
            !readable_memory(mode_state, 0x0cu)) {
            validation_state = 2;
            validation_reason = "ai_component_graph_not_exact";
        } else if (*(int16_t *)(component + 0x16au) != 1 ||
            *(mode_state + 0x0bu) != 0u) {
            validation_state = 3;
            validation_reason = "ai_control_lease_not_active";
        } else if (!readable_memory(weapon_state, 0x54u)) {
            validation_state = 4;
            validation_reason = "ranged_weapon_graph_not_ready";
        } else if (*(uint32_t *)(weapon_state + 0x50u) != 8u) {
            validation_state = 5;
            validation_reason = "ranged_weapon_type_not_firearm";
        } else {
            validation_state = 6;
            validation_reason = "exact_native_weapon_gate_ready";
        }
    }
    if (validation_state != lan_arena_ranged_fire_validation_state) {
        lan_arena_ranged_fire_validation_state = validation_state;
        SudekiMpLogFormat(
            "control_separation event=lan_arena_ranged_fire_validation "
            "state=%s reason=%s character=0x%08lx component=0x%08lx "
            "ranged_state=0x%08lx weapon_owner=0x%08lx "
            "weapon=0x%08lx weapon_state=0x%08lx "
            "policy=exact_Ailish_first_person_native_weapon_gate\r\n",
            validation_state == 6 ? "ready" : "rejected",
            validation_reason,
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)component,
            (unsigned long)(uintptr_t)ranged_state,
            (unsigned long)(uintptr_t)weapon_owner,
            (unsigned long)(uintptr_t)weapon,
            (unsigned long)(uintptr_t)weapon_state);
    }
    if (validation_state != 6) {
        SetLastError(validation_state <= 1 ?
            ERROR_NOT_READY : ERROR_INVALID_DATA);
        return FALSE;
    }
    if (call_first_person_held_fire(ranged_state) == 0u) {
        /* Native weapon eligibility/cooldown rejected this control tick.  A
         * held LAN input may retry; no action edge is published until the
         * exact retail routine accepts and begins the shot. */
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpControlSeparationForceStopCharacter(void *character) {
    uint8_t *bytes = (uint8_t *)character;
    uint8_t *arbiter;
    uint8_t *movement_controller;
    float target_speed;
    float current_speed;
    if (movement_controller_set_speed_immediate == NULL ||
        !readable_memory(bytes, 0x94u)) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    arbiter = *(uint8_t **)(bytes + 0x90u);
    movement_controller = *(uint8_t **)(bytes + 0x80u);
    if (!readable_memory(arbiter, 0x14u) ||
        *(void **)(arbiter + 0x10u) != character ||
        !readable_memory(movement_controller, 0xc0u) ||
        (movement_controller[0xbeu] & 0x08u) == 0u) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    movement_controller_set_speed_immediate(
        movement_controller, 0.0f, 1.0f);
    target_speed = *(float *)(movement_controller + 0x24u);
    current_speed = *(float *)(movement_controller + 0x28u);
    if (!isfinite(target_speed) || !isfinite(current_speed) ||
        fabsf(target_speed) > 0.0001f || fabsf(current_speed) > 0.0001f) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpControlSeparationPlayerTwoRequested(void) {
    return SudekiMpControlSeparationSeatRequested(1u);
}

BOOL SudekiMpControlSeparationPlayerTwoActive(void) {
    return SudekiMpControlSeparationSeatActive(1u);
}

void *SudekiMpControlSeparationPlayerTwoCharacter(void) {
    return SudekiMpControlSeparationSeatCharacter(1u);
}

BOOL SudekiMpControlSeparationInputReady(void) {
    return SudekiMpControlSeparationSeatInputReady(1u);
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

BOOL SudekiMpControlSeparationRegisterUpdateObserver(
    const void *owner,
    SudekiMpControlUpdateObserver observer
) {
    unsigned int index;
    unsigned int free_index = UPDATE_OBSERVER_CAPACITY;

    if (owner == NULL || observer == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    acquire_update_observer_registry();
    for (index = 0u; index < UPDATE_OBSERVER_CAPACITY; ++index) {
        if (update_observers[index].owner == owner) {
            if (update_observers[index].observer == observer) {
                release_update_observer_registry();
                return TRUE;
            }
            release_update_observer_registry();
            SetLastError(ERROR_ALREADY_EXISTS);
            return FALSE;
        }
        if (free_index == UPDATE_OBSERVER_CAPACITY &&
            update_observers[index].owner == NULL) {
            free_index = index;
        }
    }
    if (free_index == UPDATE_OBSERVER_CAPACITY) {
        release_update_observer_registry();
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    update_observers[free_index].owner = owner;
    update_observers[free_index].observer = observer;
    advance_update_observer_registry_generation();
    release_update_observer_registry();
    return TRUE;
}

BOOL SudekiMpControlSeparationUnregisterUpdateObserver(
    const void *owner
) {
    unsigned int index;

    if (owner == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    acquire_update_observer_registry();
    for (index = 0u; index < UPDATE_OBSERVER_CAPACITY; ++index) {
        if (update_observers[index].owner == owner) {
            update_observers[index].observer = NULL;
            update_observers[index].owner = NULL;
            advance_update_observer_registry_generation();
            release_update_observer_registry();
            return TRUE;
        }
    }
    release_update_observer_registry();
    return TRUE;
}

BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD saved_error = GetLastError();
    DWORD tls_error = ERROR_SUCCESS;
    ControlUpdateDispatchFrame *frame = NULL;
    SudekiMpControlUpdateDispatchSource source;
    LONG active_count_start;
    LONG active_count_end;
    LONG overlap_generation_start;
    LONG overlap_generation_end;
    BOOL hook_owned_start;
    BOOL hook_owned_end;
    BOOL slot_owned_start;
    BOOL slot_owned_end;
    BOOL registry_stable_start;
    BOOL registry_stable_end;
    BOOL source_shape_start;
    BOOL source_shape_end;
    BOOL frame_current_end;
    BOOL service_source;
    BOOL post_original;
    BOOL exact = FALSE;

    if (witness == NULL ||
        control_update_dispatch_tls == TLS_OUT_OF_INDEXES) {
        goto done;
    }
    SetLastError(ERROR_SUCCESS);
    frame = (ControlUpdateDispatchFrame *)TlsGetValue(
        control_update_dispatch_tls);
    tls_error = GetLastError();
    if (tls_error != ERROR_SUCCESS || frame == NULL ||
        frame->active_witness != witness) {
        goto done;
    }
    overlap_generation_start = InterlockedCompareExchange(
        &control_update_overlap_generation, 0, 0);
    active_count_start = InterlockedCompareExchange(
        &active_control_update_dispatches, 0, 0);
    control_update_hook_ownership(
        frame, &hook_owned_start, &slot_owned_start);
    registry_stable_start = update_observer_registry_generation_is(
        witness->observer_registry_generation);
    source = (SudekiMpControlUpdateDispatchSource)witness->source;
    service_source = source ==
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL;
    post_original = service_source || source ==
        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL;
    source_shape_start = control_update_source_shape_exact(frame, source);

    control_update_hook_ownership(
        frame, &hook_owned_end, &slot_owned_end);
    registry_stable_end = update_observer_registry_generation_is(
        witness->observer_registry_generation);
    source_shape_end = control_update_source_shape_exact(frame, source);
    frame_current_end = control_update_dispatch_frame_current(frame);
    active_count_end = InterlockedCompareExchange(
        &active_control_update_dispatches, 0, 0);
    overlap_generation_end = InterlockedCompareExchange(
        &control_update_overlap_generation, 0, 0);

    exact = witness->dispatch_serial != 0u &&
        witness->dispatch_serial == frame->dispatch_serial &&
        witness->native_thread_id == (uint32_t)GetCurrentThreadId() &&
        witness->native_thread_id == (uint32_t)frame->native_thread_id &&
        witness->outer_update_depth == 1u && frame->update_depth == 1u &&
        witness->active_dispatch_count == 1u &&
        active_count_start == 1 && active_count_end == 1 &&
        witness->original_call_count == frame->original_call_count &&
        witness->observer_snapshot_count != 0u &&
        witness->observer_registry_generation != 0u &&
        witness->dispatch_overlap_generation ==
            frame->overlap_generation &&
        witness->dispatch_overlap_generation ==
            (uint32_t)overlap_generation_start &&
        overlap_generation_start == overlap_generation_end &&
        frame->tls_exact != 0u && frame->reentrancy_seen == 0u &&
        frame->active_witness == witness && frame_current_end &&
        witness->hook_owned_exact == 1u &&
        hook_owned_start && hook_owned_end &&
        witness->slot_owned_exact == 1u &&
        slot_owned_start && slot_owned_end &&
        witness->service_only == (uint8_t)(service_source ? 1u : 0u) &&
        witness->service_only == frame->service_only &&
        witness->post_original == (uint8_t)(post_original ? 1u : 0u) &&
        witness->source_exact == 1u &&
        witness->service_post_original_exact ==
            (uint8_t)(service_source ? 1u : 0u) &&
        witness->sole_observer ==
            (uint8_t)(witness->observer_snapshot_count == 1u ? 1u : 0u) &&
        witness->registry_generation_stable == 1u &&
        registry_stable_start && registry_stable_end &&
        witness->reserved[0] == 0u && witness->reserved[1] == 0u &&
        witness->reserved[2] == 0u &&
        source_shape_start && source_shape_end;

done:
    SetLastError(saved_error);
    return exact;
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
    BOOL gameplay_features_enabled;
    BOOL service_only;
    BOOL already_installed;

    acquire_control_update_lifecycle();
    already_installed = control_update_wrapper_enabled ||
        controller_update_vtable_hook.installed != FALSE ||
        game_base != NULL || original_controller_update != NULL;
    release_control_update_lifecycle();
    if (already_installed) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }

    gameplay_features_enabled = enable_second_player_movement ||
        enable_camera_relative_movement || enable_separation_guard ||
        enable_second_player_weak_attack || enable_second_player_skills ||
        enable_target_trace || enable_shared_group_camera ||
        enable_input_bridge;
    service_only = toggle_virtual_key == 0u;
    if (game_module == NULL || toggle_virtual_key > 0xffu ||
        (service_only && gameplay_features_enabled) ||
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
         (SudekiMpInputBridgeIdentity() == NULL &&
          SudekiMpLocalInputHubRequestedMask() == 0u))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (enable_separation_guard &&
        (!enable_second_player_movement || maximum_separation <= 0.0f ||
         maximum_separation > 1000.0f)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (memcmp(
            base + RVA_CONTROLLER_UPDATE,
            expected_controller_update_entry,
            sizeof(expected_controller_update_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(
            base + RVA_GAME_SPEED_PLAYER_INPUT_ENABLE + 5u,
            expected_game_speed_player_input_enable_entry,
            sizeof(expected_game_speed_player_input_enable_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(base + RVA_CONTROLLER_FILTER_ALL,
            expected_controller_filter_all,
            sizeof(expected_controller_filter_all)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(
            base + RVA_ARBITER_SET_SPEED,
            expected_arbiter_set_speed_entry,
            sizeof(expected_arbiter_set_speed_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(
            base + RVA_POSITION_SET_FORWARD,
            expected_position_set_forward_entry,
            sizeof(expected_position_set_forward_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(
            base + RVA_FIRST_PERSON_HELD_FIRE,
            expected_first_person_held_fire_entry,
            sizeof(expected_first_person_held_fire_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(
            base + RVA_MISSILE_MANAGER_CAN_FIRE,
            expected_missile_manager_can_fire_entry,
            sizeof(expected_missile_manager_can_fire_entry)) != 0 ||
        memcmp(
            base + RVA_MISSILE_MANAGER_IS_FIRING,
            expected_missile_manager_is_firing_entry,
            sizeof(expected_missile_manager_is_firing_entry)) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (memcmp(
            base + RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE,
            expected_movement_controller_set_speed_immediate_entry,
            sizeof(expected_movement_controller_set_speed_immediate_entry)) !=
            0) {
        SetLastError(ERROR_INVALID_DATA);
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
    if (memcmp(
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
    if (!ensure_control_update_dispatch_tls()) return FALSE;
    if (service_only) {
        game_base = base;
        selected_virtual_key = 0u;
        original_controller_update = (ControllerUpdateFunction)(
            base + RVA_CONTROLLER_UPDATE
        );
        retained_original_controller_update = original_controller_update;
        service_only_mode = TRUE;
        if (!SudekiMpInstallPointerHook(
                &controller_update_vtable_hook,
                slot,
                original_controller_update,
                service_control_update_observers)) {
            DWORD install_error = GetLastError();
            if (SudekiMpUninstallControlSeparation()) {
                SetLastError(install_error == ERROR_SUCCESS ?
                    ERROR_GEN_FAILURE : install_error);
            }
            return FALSE;
        }
        acquire_control_update_lifecycle();
        control_update_wrapper_enabled = TRUE;
        release_control_update_lifecycle();
        SudekiMpLogWrite(
            "control_separation_install=success profile=service_only "
            "policy=exact_native_controller_update_once_then_owned_observers "
            "gameplay_and_coop_services=disabled\r\n"
        );
        return TRUE;
    }
    game_base = base;
    service_only_mode = FALSE;
    selected_virtual_key = toggle_virtual_key;
    hotkey_was_down = FALSE;
    manual_toggle_enabled = TRUE;
    ZeroMemory(companion_controls, sizeof(companion_controls));
    overridden_character = NULL;
    overridden_ai_component = NULL;
    role_lock_active = FALSE;
    fixed_three_release_deferred_logged = FALSE;
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
    lan_arena_remote_input_enabled = FALSE;
    player_one_skill_input_isolation_enabled = FALSE;
    player_one_skill_native_input_restored = FALSE;
    player_one_skill_input_isolation_trace_state = -1;
    player_one_skill_arbiter_virtualization_logged = FALSE;
    player_one_skill_direct_movement_scope_active = FALSE;
    player_one_skill_direct_movement_submitted = FALSE;
    player_one_skill_direct_movement_operator_override = FALSE;
    player_one_skill_frame_delta = 0.0f;
    player_one_skill_direct_movement_last_trace_tick = 0u;
    player_one_skill_direction_override = NULL;
    lan_arena_player_two_skill_input_isolation_enabled = FALSE;
    lan_arena_player_two_skill_virtualization_logged = FALSE;
    lan_arena_player_two_skill_direct_movement_last_trace_tick = 0u;
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
    retained_original_controller_update = original_controller_update;
    ai_override_control = (AiControlFunction)(base + RVA_AI_OVERRIDE_CONTROL);
    ai_default_control = (AiControlFunction)(base + RVA_AI_DEFAULT_CONTROL);
    arbiter_movement = (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    position_set_forward = base + RVA_POSITION_SET_FORWARD;
    lan_arena_first_person_held_fire =
        base + RVA_FIRST_PERSON_HELD_FIRE;
    missile_manager_can_fire = (MissileManagerPredicateFunction)(
        base + RVA_MISSILE_MANAGER_CAN_FIRE);
    missile_manager_is_firing = (MissileManagerPredicateFunction)(
        base + RVA_MISSILE_MANAGER_IS_FIRING);
    lan_arena_cached_missile_manager = NULL;
    lan_arena_cached_missile_owner = NULL;
    lan_arena_missile_ready_trace_state = -1;
    lan_arena_ranged_fire_validation_state = -1;
    arbiter_set_speed = (ArbiterSetSpeedFunction)(
        base + RVA_ARBITER_SET_SPEED
    );
    movement_controller_set_speed_immediate =
        (MovementControllerSetSpeedImmediateFunction)(
            base + RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE);
    game_speed_player_input_enable =
        (GameSpeedPlayerInputEnableFunction)(
            base + RVA_GAME_SPEED_PLAYER_INPUT_ENABLE);
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
        DWORD install_error = GetLastError();
        if (SudekiMpUninstallControlSeparation()) {
            SetLastError(install_error == ERROR_SUCCESS ?
                ERROR_GEN_FAILURE : install_error);
        }
        return FALSE;
    }
    if (enable_second_player_movement &&
        !SudekiMpInstallInlineHook(
            &tal_character_update_hook,
            base + RVA_TAL_CHARACTER_UPDATE,
            expected_tal_character_update_entry,
            sizeof(expected_tal_character_update_entry),
            trace_tal_character_update)) {
        DWORD install_error = GetLastError();
        if (SudekiMpUninstallControlSeparation()) {
            SetLastError(install_error == ERROR_SUCCESS ?
                ERROR_GEN_FAILURE : install_error);
        }
        return FALSE;
    }

    if (enable_separation_guard &&
        !SudekiMpInstallRelativeCallHook(
            &player_one_alternate_movement_call_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE,
            arbiter_movement,
            enforce_player_one_roaming_boundary)) {
        DWORD install_error = GetLastError();
        if (SudekiMpUninstallControlSeparation()) {
            SetLastError(install_error == ERROR_SUCCESS ?
                ERROR_GEN_FAILURE : install_error);
        }
        return FALSE;
    }
    if (enable_separation_guard &&
        !SudekiMpInstallRelativeCallHook(
            &player_one_normal_movement_call_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL,
            arbiter_movement,
            enforce_player_one_roaming_boundary)) {
        DWORD install_error = GetLastError();
        if (SudekiMpUninstallControlSeparation()) {
            SetLastError(install_error == ERROR_SUCCESS ?
                ERROR_GEN_FAILURE : install_error);
        }
        return FALSE;
    }

    if (!SudekiMpInstallPointerHook(
            &controller_update_vtable_hook,
            slot,
            original_controller_update,
            poll_control_separation_hotkey)) {
        DWORD install_error = GetLastError();
        if (SudekiMpUninstallControlSeparation()) {
            SetLastError(install_error == ERROR_SUCCESS ?
                ERROR_GEN_FAILURE : install_error);
        }
        return FALSE;
    }
    acquire_control_update_lifecycle();
    control_update_wrapper_enabled = TRUE;
    release_control_update_lifecycle();
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

static BOOL release_companion_leases_for_uninstall(void) {
    unsigned int seat_index;
    BOOL released = TRUE;

    role_lock_active = FALSE;
    for (seat_index = CONTROL_COMPANION_FIRST_SEAT;
         seat_index <= CONTROL_COMPANION_LAST_SEAT;
         ++seat_index) {
        SudekiMpCompanionControlRuntime *companion =
            companion_control_for_seat(seat_index);

        companion->requested = FALSE;
        companion->requested_character = NULL;
        companion->request_last_attempt = 0u;
        quiesce_companion_input(seat_index);
    }
    for (seat_index = CONTROL_COMPANION_LAST_SEAT;; --seat_index) {
        SudekiMpCompanionControlRuntime *companion =
            companion_control_for_seat(seat_index);

        if (companion->character != NULL &&
            (!transition_companion_ai(seat_index) ||
             companion->character != NULL)) {
            SudekiMpLogFormat(
                "control_separation event=module_uninstall player=%u "
                "status=pending reason=owned_ai_lease_restore_not_exact "
                "policy=preserve_live_hook_and_retry_reverse_order\r\n",
                seat_index + 1u);
            released = FALSE;
        }
        if (seat_index == CONTROL_COMPANION_FIRST_SEAT) {
            break;
        }
    }
    return released;
}

static void retain_control_separation_dependencies(
    const char *reason,
    DWORD error
) {
    HMODULE pinned_module = NULL;
    BOOL pinned = control_separation_containment_pinned;

    if (!pinned) {
        pinned = GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)&SudekiMpUninstallControlSeparation,
            &pinned_module);
        if (pinned) control_separation_containment_pinned = TRUE;
    }
    if (!control_separation_restore_failure_logged) {
        control_separation_restore_failure_logged = TRUE;
        SudekiMpLogFormat(
            "control_separation event=module_uninstall status=quarantined "
            "reason=%s error=%lu module_pinned=%s "
            "policy=retain_callbacks_observers_and_dependencies_for_retry\r\n",
            reason == NULL ? "unspecified" : reason,
            (unsigned long)error,
            pinned ? "true" : "false");
    }
    SetLastError(error);
}

BOOL SudekiMpUninstallControlSeparation(void) {
    BOOL restored = TRUE;
    DWORD teardown_error = ERROR_SUCCESS;
#define RECORD_RESTORE_RESULT(expression) do { \
        if (!(expression)) { \
            DWORD current_error = GetLastError(); \
            if (restored) { \
                teardown_error = current_error == ERROR_SUCCESS ? \
                    ERROR_WRITE_FAULT : current_error; \
            } \
            restored = FALSE; \
        } \
    } while (0)

    acquire_control_update_lifecycle();
    if (InterlockedCompareExchange(
            &active_control_update_dispatches, 0, 0) != 0) {
        release_control_update_lifecycle();
        retain_control_separation_dependencies(
            "controller_update_dispatch_in_flight", ERROR_BUSY);
        return FALSE;
    }
    if (!release_companion_leases_for_uninstall()) {
        release_control_update_lifecycle();
        retain_control_separation_dependencies(
            "owned_ai_lease_restore_not_exact", ERROR_BUSY);
        return FALSE;
    }
    RECORD_RESTORE_RESULT(SudekiMpRestorePointerHook(
        &controller_update_vtable_hook));
    control_update_wrapper_enabled =
        controller_update_vtable_hook.installed != FALSE;
    release_control_update_lifecycle();
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &player_one_normal_movement_call_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &lan_spirit_root_movement_call_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreRelativeCallHook(
        &player_one_alternate_movement_call_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreInlineHook(
        &tal_character_update_hook));
    RECORD_RESTORE_RESULT(SudekiMpRestoreInlineHook(
        &movement_controller_update_hook));
#undef RECORD_RESTORE_RESULT
    if (!restored) {
        retain_control_separation_dependencies(
            "native_hook_restore_failed", teardown_error);
        return FALSE;
    }

    clear_update_observers();
    restore_group_camera("module_uninstall");
    original_controller_update = NULL;
    original_animation_root_movement = NULL;
    lan_tal_skill_direct_actor = NULL;
    lan_tal_skill_direct_at_ms = 0u;
    lan_spirit_direct_actor = NULL;
    lan_spirit_direct_at_ms = 0u;
    ai_override_control = NULL;
    ai_default_control = NULL;
    arbiter_movement = NULL;
    position_set_forward = NULL;
    lan_arena_first_person_held_fire = NULL;
    missile_manager_can_fire = NULL;
    missile_manager_is_firing = NULL;
    lan_arena_cached_missile_manager = NULL;
    lan_arena_cached_missile_owner = NULL;
    lan_arena_missile_ready_trace_state = -1;
    lan_arena_ranged_fire_validation_state = -1;
    arbiter_set_speed = NULL;
    movement_controller_set_speed_immediate = NULL;
    game_speed_player_input_enable = NULL;
    camera_manager_get_camera_mode = NULL;
    group_players_in_combat = NULL;
    movement_camera_transform = NULL;
    movement_controller_set_absolute_delta = NULL;
    spirit_direct_movement_active = FALSE;
    spirit_direct_movement_last_trace_tick = 0u;
    game_base = NULL;
    ZeroMemory(companion_controls, sizeof(companion_controls));
    role_lock_active = FALSE;
    fixed_three_release_deferred_logged = FALSE;
    service_only_mode = FALSE;
    selected_virtual_key = 0;
    hotkey_was_down = FALSE;
    manual_toggle_enabled = TRUE;
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
    input_bridge_enabled = FALSE;
    lan_arena_remote_input_enabled = FALSE;
    player_one_skill_input_isolation_enabled = FALSE;
    player_one_skill_native_input_restored = FALSE;
    player_one_skill_input_isolation_trace_state = -1;
    player_one_skill_arbiter_virtualization_logged = FALSE;
    player_one_skill_direct_movement_scope_active = FALSE;
    player_one_skill_direct_movement_submitted = FALSE;
    player_one_skill_direct_movement_operator_override = FALSE;
    player_one_skill_frame_delta = 0.0f;
    player_one_skill_direct_movement_last_trace_tick = 0u;
    player_one_skill_direction_override = NULL;
    lan_arena_player_two_skill_input_isolation_enabled = FALSE;
    lan_arena_player_two_skill_virtualization_logged = FALSE;
    lan_arena_player_two_skill_direct_movement_last_trace_tick = 0u;
    input_bridge_deadzone = 0.0f;
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
    reset_native_movement_acceptance_trace();
    SudekiMpCombatContextsReset();
    if (control_update_dispatch_tls != TLS_OUT_OF_INDEXES &&
        InterlockedCompareExchange(
            &active_control_update_dispatches, 0, 0) == 0) {
        (void)TlsFree(control_update_dispatch_tls);
        control_update_dispatch_tls = TLS_OUT_OF_INDEXES;
    }
    return TRUE;
}
