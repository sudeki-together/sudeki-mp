#include "engine/skill_activation_abi.h"
#include "engine/spirit_activation_abi.h"
#include "engine/item_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "engine/player_combat_context.h"
#include "engine/player_statehood.h"
#include "hooks/accelerator_cache.h"
#include "hooks/blacksmith_ui_adapter.h"
#include "hooks/skill_trace.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/lan_arena_client_input.h"
#include "network/lan_arena_operator.h"
#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "hooks/lan_arena_host_input.h"
#include "hooks/lan_arena_pause_panel.h"
#include "hooks/lan_arena_runtime.h"
#include "hooks/lan_arena_window_policy.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_skill_input.h"
#include "hooks/save_book_intercept.h"
#include "hooks/spirit_strike_input.h"
#include "hooks/split_screen_render.h"
#include "hooks/talos_defense_trace.h"
#include "hooks/talos_native_lifecycle_trace.h"
#include "hooks/xinput_player_two.h"
#include "hooks/zone_transition_trace.h"
#include "input/bridge_receiver.h"

#include <windows.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static BOOL split_runtime_authorization_result;

static BOOL split_runtime_authorization_query(void) {
    return split_runtime_authorization_result;
}

enum {
    RVA_USE = 0x000b4810u,
    RVA_USE_CALL = 0x000998a1u,
    RVA_DIRECT_USE_CALL = 0x00027cb1u,
    RVA_QUICK_SKILL_ACTION = 0x00027bf0u,
    RVA_QUICK_SKILL_ACTION_CALL = 0x00027acfu,
    RVA_SKILL_VALIDATE = 0x000b4bc0u,
    RVA_QUICK_SKILL_VALIDATE_CALL = 0x00027c8cu,
    RVA_QUICK_MENU_VALIDATE_CALL = 0x00099867u,
    RVA_USE_INTERNAL_VALIDATE_CALL = 0x000b4828u,
    RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL = 0x000998b9u,
    RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL = 0x000998dcu,
    RVA_SPIRIT_STRIKE_VALIDATE = 0x00010940u,
    RVA_SPIRIT_STRIKE_ACTIVATE = 0x0000fba0u,
    RVA_SPIRIT_STRIKE_VALIDATION_FLAG = 0x00349570u,
    RVA_CHARACTER_INPUT_HANDLER = 0x000277b0u,
    RVA_CHARACTER_INPUT_VTABLE_SLOT = 0x002c9f84u,
    RVA_APPLY_DAMAGE = 0x000d21d0u,
    RVA_COLLISION_DAMAGE = 0x00138870u,
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    RVA_ARBITER_COMBAT_INPUT = 0x000db0e0u,
    RVA_CONTROLLER_AIM_UPDATE = 0x00028b00u,
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER = 0x00023f60u,
    RVA_GROUP_PLAYERS_NEXT_CHARACTER = 0x00024060u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_ARBITER_SET_SPEED = 0x000db070u,
    RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE = 0x000c3870u,
    RVA_MISSILE_MANAGER_IS_FIRING = 0x000c7990u,
    RVA_MISSILE_MANAGER_CAN_FIRE = 0x000c79a0u,
    RVA_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_POSITION_UPDATE = 0x00110d40u,
    RVA_POSITION_WORLD_MATRIX = 0x00111cc0u,
    RVA_POSITION_WORLD_MATRIX_UPDATE_CALL = 0x00111cdau,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_MAIN_FRAME_UPDATE_CALL = 0x0028ddbau,
    RVA_MAIN_FRAME_UPDATE = 0x0028d3f0u,
    RVA_RENDER_FIRST_PHASE = 0x001d48c0u,
    RVA_RENDER_FIRST_PHASE_CALL_MAIN = 0x0028d45bu,
    RVA_RENDER_START = 0x001dce30u,
    RVA_RENDER_START_CALL_MAIN = 0x0028d443u,
    RVA_RENDER_PHASE = 0x001d4750u,
    RVA_RENDER_PHASE_CALL_MAIN = 0x0028d473u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL_MAIN = 0x0028d58cu,
    RVA_PC_QUIT_SCREEN_GLOBAL = 0x00408d68u,
    RVA_PC_QUIT_SCREEN_SHOW = 0x0001dbe0u,
    RVA_PC_QUIT_SCREEN_RENDER = 0x0001d690u,
    RVA_PC_QUIT_SCREEN_RENDER_CALL = 0x0028d572u,
    RVA_PC_QUIT_SCREEN_SELECT = 0x0001d780u,
    RVA_PC_QUIT_SCREEN_SELECT_CALL = 0x0001db71u,
    RVA_PC_QUIT_SCREEN_BACK = 0x0001d860u,
    RVA_PC_QUIT_SCREEN_BACK_CALL = 0x0001db64u,
    RVA_PC_QUIT_SCREEN_NAVIGATE = 0x0001d9f0u,
    RVA_PC_QUIT_SCREEN_ANALOG_NAVIGATE_CALL = 0x0001d9dfu,
    RVA_PC_QUIT_SCREEN_NAVIGATE_CALL = 0x0001dba4u,
    RVA_QUICK_MENU_IS_ACTIVE = 0x0009c330u,
    RVA_QUICK_MENU_CLOSE = 0x0009c360u,
    RVA_QUICK_MENU_START = 0x0009c3a0u,
    RVA_QUICK_MENU_GLOBAL = 0x003c2f84u,
    RVA_QUICK_MENU_VTABLE = 0x002caf1cu,
    RVA_QUICK_MENU_RENDER_SUBMIT = 0x0009bba0u,
    RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT = 0x002caf28u,
    RVA_QUICK_MENU_INPUT = 0x00098b40u,
    RVA_QUICK_MENU_INPUT_VTABLE_SLOT = 0x002caf48u,
    RVA_QUICK_MENU_NATIVE_TOGGLE = 0x0000a080u,
    RVA_QUICK_MENU_NATIVE_TOGGLE_CALL = 0x00028228u,
    RVA_KILL_FOCUS_SHOW_COMMAND = 0x0028d784u,
    RVA_WINDOW_ACTIVE_COMPARE_IMMEDIATE = 0x0028d6a7u,
    RVA_FOCUS_LOSS_BOOL_OPCODE = 0x0028d74au,
    RVA_FOCUS_LOSS_BOOL_VALUE = 0x0028d74bu,
    RVA_FOCUS_LOSS_STATE_VALUE = 0x0028d752u,
    RVA_FOCUS_DEVICE_STATE_GLOBAL = 0x003c3110u,
    RVA_WINDOW_ACTIVATE_APP_COMPARE_IMMEDIATE = 0x0028d7d1u,
    RVA_SHOW_WINDOW_IAT = 0x0029a224u,
    RVA_QUICK_MENU_OWNER_COPY_UI_ACTIVE = 0x0000aff1u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION = 0x00099341u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_WEAPON = 0x000995d6u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_ITEM = 0x000996e9u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_SKILL = 0x0009984bu,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_HEADER = 0x00099afcu,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_SKILL = 0x00099e80u,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_WEAPON = 0x00099f48u,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_ITEM = 0x00099f8bu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_HEADER = 0x0009a33eu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_RESOURCE = 0x0009a960u,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_SKILL = 0x0009b40cu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_WEAPON = 0x0009b7cbu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_ITEM = 0x0009cc15u,
    RVA_QUICK_MENU_OWNER_COPY_DEFAULT_RECIPIENT = 0x0009c153u,
    QUICK_MENU_ACTIVE_OFFSET = 0x29u,
    RVA_INGAME_UI_CONTROLLER_GLOBAL = 0x003c2f88u,
    RVA_SHOP_LAYER_GLOBAL = 0x003c2f70u,
    RVA_BLACKSMITH_LAYER_GLOBAL = 0x003c2f74u,
    RVA_WORLD_SCENE_GLOBAL = 0x00408d1cu,
    RVA_SAVE_MENU_SHOW = 0x00084f10u,
    RVA_LOAD_GAME_SAVE = 0x00101690u,
    RVA_LOADING_FLAG = 0x00409df8u,
    RVA_SHOP_IS_ACTIVE = 0x0008d1c0u,
    RVA_BLACKSMITH_START = 0x00092c40u,
    RVA_BLACKSMITH_IS_ACTIVE = 0x00092c60u,
    RVA_BLACKSMITH_ACTIVE_EXPORT_ENTRY = 0x0030d414u,
    RVA_BLACKSMITH_START_EXPORT_ENTRY = 0x0030d418u,
    RVA_INGAME_UI_CONTROLLER_VTABLE = 0x002caf9cu,
    RVA_SHOP_LAYER_VTABLE = 0x002cabb4u,
    RVA_BLACKSMITH_LAYER_VTABLE = 0x002cacfcu,
    RVA_INGAME_UI_CONTROLLER_UPDATE = 0x0009d1d0u,
    RVA_INGAME_UI_CONTROLLER_RENDER = 0x0009d8d0u,
    RVA_INGAME_UI_CONTROLLER_INPUT = 0x0009c930u,
    RVA_SHOP_LAYER_UPDATE = 0x00089660u,
    RVA_SHOP_LAYER_RENDER = 0x0008a210u,
    RVA_SHOP_LAYER_INPUT = 0x000898a0u,
    RVA_SHOP_LAYER_RESOURCE_CREATE = 0x0008c850u,
    RVA_SHOP_LAYER_RESOURCE_DESTROY = 0x0008d030u,
    RVA_BLACKSMITH_LAYER_UPDATE = 0x0008d6f0u,
    RVA_BLACKSMITH_LAYER_RENDER = 0x0008e910u,
    RVA_BLACKSMITH_LAYER_INPUT = 0x0008d970u,
    RVA_BLACKSMITH_LAYER_RESOURCE_CREATE = 0x00090c20u,
    RVA_BLACKSMITH_LAYER_RESOURCE_DESTROY = 0x00091b40u,
    RVA_UI_SCENE_RENDER = 0x0000a820u,
    RVA_UI_SCENE_RENDER_CALL = 0x0000a760u,
    RVA_HUD_PARTY_POINTER_COPY = 0x000015b0u,
    RVA_HUD_GROUP_VALUES_POINTER_CALL = 0x00181517u,
    RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL = 0x000aab3au,
    RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT = 0x0015c0e0u,
    RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT_CALL = 0x000aac08u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM = 0x0003f430u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP = 0x0003f498u,
    RVA_HUD_PORTRAIT_RESOURCE_SELECT = 0x0015c070u,
    RVA_UI_RESOURCE_TABLE_INITIALIZED = 0x003c2fefu,
    RVA_HUD_GIZMO_VALUES_POINTER_CALL = 0x000a9d5bu,
    RVA_HUD_GIZMO_NAME_POINTER_CALL = 0x000a9e15u,
    RVA_HUD_GIZMO_STATUS_POINTER_CALL = 0x000aacabu,
    RVA_MINIMAP_UPDATE_POINTER_CALL = 0x00087760u,
    RVA_MINIMAP_SNAPSHOT_POINTER_CALL = 0x00087a27u,
    RVA_MINIMAP_RENDER_POINTER_CALL = 0x00087af7u,
    RVA_RENDER_PHASE_CALL_WORLD_PREPASS = 0x0000a62du,
    RVA_RENDER_PHASE_CALL_WORLD = 0x0000a689u,
    RVA_RENDER_PHASE_CALL_WORLD_OFFSET = 0x0000a738u,
    RVA_STOP_RUMBLE = 0x000b50d0u,
    RVA_STOP_RUMBLE_CALL = 0x000b4f23u,
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_METHOD_OPCODE = 0x001c4b10u,
    RVA_SCRIPT_METHOD_OPCODE_SLOT = 0x00323fa4u,
    RVA_SCRIPT_SCENE_OPCODE = 0x001c4d30u,
    RVA_SCRIPT_SCENE_OPCODE_SLOT = 0x00323fa8u,
    RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL = 0x001c4db8u,
    RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR = 0x001c3170u,
    RVA_KAZEL_GROUP_ADD_CALL = 0x000b15dbu,
    RVA_RAW_GROUP_ADD = 0x00023280u,
    RVA_AI_LISTENER_VTABLE = 0x002ca244u,
    RVA_AI_LISTENER_ADD = 0x000f2b00u,
    RVA_AI_LISTENER_FORMATION_ADD_CALL = 0x000f2b14u,
    RVA_RAW_FORMATION_ADD = 0x000b2cb0u,
    RVA_DELETE_PC = 0x000b2520u,
    RVA_REMOVE_ALL_PLAYERS = 0x000252d0u,
    RVA_FORMATION_POP_MEMBERS = 0x000f6260u,
    RVA_TSA_IS_PLAYING = 0x0001a230u,
    RVA_TSA_SET_PLAYING = 0x0001a240u,
    RVA_TSA_DISPATCH = 0x0003f3b0u,
    RVA_LIFECYCLE_TSA_PLAYING_GLOBAL = 0x00408d4cu,
    RVA_LIFECYCLE_TSA_SHADOW_GLOBAL = 0x003c2f3cu,
    RVA_LIFECYCLE_TSA_SCRIPT_MANAGER_GLOBAL = 0x00409d8cu,
    RVA_LIFECYCLE_TSA_EVENT_NAME = 0x003c3a64u,
    RVA_LIFECYCLE_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_LIFECYCLE_AI_MANAGER_GLOBAL = 0x00409de4u,
    RVA_SCRIPT_METHOD_BINDING_CALL = 0x001c4c2fu,
    RVA_SCRIPT_BINDING_INVOKE = 0x002351c0u,
    RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_GROUP_PLAYERS_GET_PLAYER_GROUP = 0x000246d0u,
    RVA_GEL_POINTER_RESOLVE_ENTITY = 0x001bf4e0u,
    RVA_TRACKED_ENTITY_CLEANUP = 0x000015e0u,
    RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR = 0x00001b30u,
    RVA_GEL_GROUP_PTR_VTABLE = 0x002c0098u,
    RVA_GEL_GROUP_PTR_GET_RAW_ENTITY = 0x000017b0u,
    RVA_GEL_GROUP_PTR_TYPE_NAME = 0x00001820u,
    RVA_GEL_POINTER_RESOLVER_HANDLER = 0x002947f8u,
    RVA_CAMERA_INPUT_EVENT = 0x000e85f0u,
    RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT = 0x002cce5cu,
    RVA_POSITION_SET_FORWARD = 0x001114d0u,
    RVA_MOTION_BLUR_POST_RENDER = 0x001de0b0u,
    RVA_SCREENSHOT_POST_RENDER = 0x001de7b0u,
    RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT = 0x002dd930u,
    RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT = 0x002dd910u,
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
    RVA_SKILL_DATA_AVAILABLE = 0x000da2a0u,
    RVA_SKILL_AVAILABILITY_FLAG = 0x003c2fd9u,
    RVA_SKILL_VALIDATE_FLAG = 0x0034a8b0u,
    RVA_QUICK_ITEM_APPLY_TO_PARTY_SLOT = 0x000dc110u,
    RVA_ZONE_SET_NOW = 0x00007910u,
    RVA_ZONE_ENTER = 0x00007970u,
    RVA_ZONE_SWITCH_NOW = 0x00007990u,
    RVA_ZONE_LOAD = 0x00007b80u,
    RVA_ZONE_SWITCH_MAIN = 0x00006380u,
    RVA_ZONE_DOOR_ACTIVATE = 0x000ce3a0u,
    RVA_ZONE_ENTER_TEMPORARY = 0x000064b0u,
    RVA_ZONE_EXIT_TEMPORARY = 0x00006710u,
    RVA_ZONE_SET_PLAYER_POSITION = 0x00104ed0u,
    RVA_ZONE_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_ZONE_ENTER_LEAD_POP_CALL = 0x00005c59u,
    RVA_ZONE_EXIT_LEAD_MOVE_CALL = 0x000068d3u,
    RVA_ZONE_POP_TO_NAMED_LOCATION = 0x000f63d0u,
    RVA_ZONE_EXIT_LEAD_MOVE = 0x000f30a0u,
    RVA_ZONE_FORMATION_POP_MEMBERS = 0x000f6260u,
    RVA_ZONE_SET_MODE_LEAD_ONLY = 0x00024720u,
    RVA_ZONE_SET_MODE_FULL_PARTY = 0x00024850u,
    RVA_ZONE_SHOW_PARTY_MEMBERS = 0x00024950u,
    RVA_ZONE_HIDE_PARTY_MEMBERS = 0x00024a70u,
    RVA_ZONE_AI_MANAGER_GLOBAL = 0x00409de4u
};

/* The exact-image harness links the pause adapter without the live arena
 * runtime. Render callbacks are never invoked here; these stubs isolate hook
 * preflight/install/restore from network or native gameplay state. */
BOOL SudekiMpLanArenaRuntimeEndSession(void) { return FALSE; }
BOOL SudekiMpLanArenaRuntimeJoinAddress(const char *remote_ipv4) {
    (void)remote_ipv4;
    return FALSE;
}
BOOL SudekiMpLanArenaRuntimeJoinEndpoint(const char *endpoint) {
    (void)endpoint;
    return FALSE;
}
BOOL SudekiMpLanArenaRuntimeHostArena(void) { return FALSE; }
BOOL SudekiMpLanArenaRuntimeGetStatus(SudekiMpLanArenaSessionStatus *status) {
    if (status != NULL) memset(status, 0, sizeof(*status));
    return FALSE;
}

static const uint32_t quick_menu_owner_copy_call_rvas[] = {
    RVA_QUICK_MENU_OWNER_COPY_UI_ACTIVE,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_WEAPON,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_ITEM,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_SKILL,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_HEADER,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_SKILL,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_WEAPON,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_ITEM,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_HEADER,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_RESOURCE,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_SKILL,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_WEAPON,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_ITEM,
    RVA_QUICK_MENU_OWNER_COPY_DEFAULT_RECIPIENT
};

typedef struct ExpectedExport {
    uint32_t slot_rva;
    uint32_t function_rva;
} ExpectedExport;

typedef struct ExpectedEntry {
    uint32_t function_rva;
    uint8_t bytes[16];
    size_t byte_count;
    const char *name;
} ExpectedEntry;

typedef void (__attribute__((thiscall)) *TestControllerUpdateFunction)(
    void *controller,
    void *update_data
);

static unsigned int service_update_original_calls;
static unsigned int service_update_observer_one_calls;
static unsigned int service_update_observer_two_calls;
static unsigned int service_update_sequence;
static BOOL service_update_order_failed;
static BOOL service_update_context_failed;
static BOOL service_update_expect_observer_one;
static const void *service_update_expected_controller;
static const void *service_update_expected_data;
static const void *service_update_self_unregister_owner;
static TestControllerUpdateFunction service_update_reentrant_target;
static unsigned int service_update_reentrant_depth;
static SudekiMpControlUpdateDispatchWitness service_update_witnesses[8];
static DWORD service_update_observer_entry_errors[8];
static BOOL service_update_witness_revalidated[8];
static BOOL service_update_witness_revalidated_after_mutation;
static unsigned int service_update_witness_count;
static BOOL service_update_request_uninstall;
static DWORD service_update_uninstall_error;
static BOOL service_update_revalidated_after_uninstall_attempt;
static SudekiMpControlUpdateObserverGate stale_snapshot_observer_gate;
static const void *stale_snapshot_observer_owner;
static unsigned int stale_snapshot_disabler_calls;
static unsigned int stale_snapshot_callback_calls;
static unsigned int stale_snapshot_backing_calls;

static void reset_service_update_witnesses(void) {
    ZeroMemory(service_update_witnesses, sizeof(service_update_witnesses));
    ZeroMemory(
        service_update_observer_entry_errors,
        sizeof(service_update_observer_entry_errors)
    );
    ZeroMemory(
        service_update_witness_revalidated,
        sizeof(service_update_witness_revalidated)
    );
    service_update_witness_revalidated_after_mutation = FALSE;
    service_update_uninstall_error = ERROR_SUCCESS;
    service_update_revalidated_after_uninstall_attempt = FALSE;
    service_update_witness_count = 0u;
}

static void capture_service_update_witness(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD entry_last_error = GetLastError();

    if (witness == NULL || service_update_witness_count >=
            sizeof(service_update_witnesses) /
                sizeof(service_update_witnesses[0])) {
        service_update_context_failed = TRUE;
        return;
    }
    service_update_observer_entry_errors[service_update_witness_count] =
        entry_last_error;
    service_update_witness_revalidated[service_update_witness_count] =
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness);
    if (GetLastError() != entry_last_error) {
        service_update_context_failed = TRUE;
    }
    service_update_witnesses[service_update_witness_count++] = *witness;
}

static BOOL service_update_witness_matches(
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpControlUpdateDispatchSource source,
    uint32_t original_call_count,
    uint32_t observer_count,
    BOOL service_only,
    BOOL post_original,
    BOOL sole_observer,
    BOOL registry_stable,
    BOOL hook_owned,
    BOOL slot_owned,
    BOOL source_exact,
    BOOL service_post_original_exact
) {
    return witness != NULL && witness->dispatch_serial != 0u &&
        witness->native_thread_id == GetCurrentThreadId() &&
        witness->outer_update_depth == 1u &&
        witness->active_dispatch_count == 1u &&
        witness->original_call_count == original_call_count &&
        witness->observer_snapshot_count == observer_count &&
        witness->observer_registry_generation != 0u &&
        witness->hook_owned_exact == (uint8_t)(hook_owned ? 1u : 0u) &&
        witness->slot_owned_exact == (uint8_t)(slot_owned ? 1u : 0u) &&
        witness->service_only == (uint8_t)(service_only ? 1u : 0u) &&
        witness->post_original == (uint8_t)(post_original ? 1u : 0u) &&
        witness->source == (uint8_t)source &&
        witness->source_exact == (uint8_t)(source_exact ? 1u : 0u) &&
        witness->service_post_original_exact ==
            (uint8_t)(service_post_original_exact ? 1u : 0u) &&
        witness->sole_observer == (uint8_t)(sole_observer ? 1u : 0u) &&
        witness->registry_generation_stable ==
            (uint8_t)(registry_stable ? 1u : 0u) &&
        witness->reserved[0] == 0u && witness->reserved[1] == 0u &&
        witness->reserved[2] == 0u;
}

static void __attribute__((thiscall)) service_update_original_stub(
    void *controller,
    void *update_data
) {
    (void)controller;
    (void)update_data;
    ++service_update_original_calls;
    if (service_update_sequence != 0u) {
        service_update_order_failed = TRUE;
    }
    service_update_sequence = 1u;
    SetLastError(0x1234u);
}

static void __attribute__((thiscall)) service_update_reentrant_original_stub(
    void *controller,
    void *update_data
) {
    ++service_update_original_calls;
    if (service_update_reentrant_depth == 0u &&
        service_update_reentrant_target != NULL) {
        service_update_reentrant_depth = 1u;
        service_update_reentrant_target(controller, update_data);
        service_update_reentrant_depth = 0u;
    }
    SetLastError(0x1234u);
}

static void service_update_observer_one(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    capture_service_update_witness(witness);
    ++service_update_observer_one_calls;
    if (!service_update_expect_observer_one ||
        service_update_sequence != 1u) {
        service_update_order_failed = TRUE;
    }
    if (controller != service_update_expected_controller ||
        update_data != service_update_expected_data) {
        service_update_context_failed = TRUE;
    }
    service_update_sequence = 2u;
    if (service_update_self_unregister_owner != NULL &&
        !SudekiMpControlSeparationUnregisterUpdateObserver(
            service_update_self_unregister_owner)) {
        service_update_order_failed = TRUE;
    }
    if (service_update_self_unregister_owner != NULL) {
        service_update_witness_revalidated_after_mutation =
            SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness);
    }
    SetLastError(0x5678u);
}

static void service_update_observer_two(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    capture_service_update_witness(witness);
    ++service_update_observer_two_calls;
    if (service_update_sequence !=
            (service_update_expect_observer_one ? 2u : 1u)) {
        service_update_order_failed = TRUE;
    }
    if (controller != service_update_expected_controller ||
        update_data != service_update_expected_data) {
        service_update_context_failed = TRUE;
    }
    service_update_sequence = service_update_expect_observer_one ? 3u : 2u;
}

static void service_update_observer_replacement(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)controller;
    (void)update_data;
    (void)witness;
    service_update_order_failed = TRUE;
}

static void service_update_witness_capture_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    if (controller != service_update_expected_controller ||
        update_data != service_update_expected_data) {
        service_update_context_failed = TRUE;
    }
    capture_service_update_witness(witness);
    if (service_update_request_uninstall) {
        SetLastError(ERROR_SUCCESS);
        SudekiMpUninstallControlSeparation();
        service_update_uninstall_error = GetLastError();
        service_update_revalidated_after_uninstall_attempt =
            SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness);
    }
}

static void stale_snapshot_disabler_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)controller;
    (void)update_data;
    (void)witness;
    ++stale_snapshot_disabler_calls;
    SudekiMpControlUpdateObserverGateDisable(
        &stale_snapshot_observer_gate);
    if (!SudekiMpControlSeparationUnregisterUpdateObserver(
            stale_snapshot_observer_owner)) {
        service_update_context_failed = TRUE;
    }
    SudekiMpControlUpdateObserverGateDrain(
        &stale_snapshot_observer_gate);
}

static void stale_snapshot_gated_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)controller;
    (void)update_data;
    (void)witness;
    ++stale_snapshot_callback_calls;
    if (!SudekiMpControlUpdateObserverGateTryEnter(
            &stale_snapshot_observer_gate)) {
        return;
    }
    ++stale_snapshot_backing_calls;
    SudekiMpControlUpdateObserverGateLeave(
        &stale_snapshot_observer_gate);
}

static BOOL install_control_separation_profile(
    uint8_t *image,
    UINT toggle_virtual_key,
    unsigned int enabled_feature
) {
    static const UINT skill_keys[4] = {'I', 'O', 'P', 'K'};

    return SudekiMpInstallControlSeparation(
        (HMODULE)image,
        toggle_virtual_key,
        enabled_feature == 1u,
        enabled_feature == 2u,
        enabled_feature == 3u,
        enabled_feature == 3u ? 10.0f : 0.0f,
        enabled_feature == 4u,
        enabled_feature == 4u ? 'U' : 0u,
        enabled_feature == 5u,
        enabled_feature == 5u ? skill_keys : NULL,
        enabled_feature == 6u,
        enabled_feature == 7u,
        enabled_feature == 8u,
        enabled_feature == 8u ? 0.20f : 0.0f
    );
}

typedef struct LifecycleHeroIdentityFixture {
    uint32_t main_vtable_rva;
    uint32_t secondary_vtable_rva;
    uint32_t resource_vtable_rva;
    uint32_t main_col_rva;
    uint32_t secondary_col_rva;
    uint32_t resource_col_rva;
    uint32_t type_descriptor_rva;
    uint32_t type_method_rva;
    uint32_t type_value;
    const char *type_name;
} LifecycleHeroIdentityFixture;

typedef struct LifecycleHeroRelocationBackup {
    uint32_t vtable_col_pointer[3];
    uint32_t col_type_pointer[3];
    uint32_t resource_method_pointer;
} LifecycleHeroRelocationBackup;

static const LifecycleHeroIdentityFixture lifecycle_hero_fixtures[] = {
    {0x002d5010u, 0x002d5034u, 0x002d5054u,
     0x002fd4e4u, 0x002fd4d0u, 0x002fd4bcu,
     0x0035a8fcu, 0x00139ad0u, 0x23u, ".?AVTalEntity@@"},
    {0x002d555cu, 0x002d5580u, 0x002d55a0u,
     0x002fe500u, 0x002fe4ecu, 0x002fe4d8u,
     0x0035ad34u, 0x001e8240u, 0x01u, ".?AVAilishEntity@@"},
    {0x002d5a88u, 0x002d5aacu, 0x002d5accu,
     0x002fec4cu, 0x002fec38u, 0x002fec24u,
     0x0035af80u, 0x0022c0e0u, 0x05u, ".?AVBukiEntity@@"},
    {0x002d66fcu, 0x002d6720u, 0x002d6740u,
     0x002ff718u, 0x002ff704u, 0x002ff6f0u,
     0x0035b1d4u, 0x0014d730u, 0x0eu, ".?AVElcoEntity@@"},
    {0x002d6884u, 0x002d68a8u, 0x002d68c8u,
     0x002ff864u, 0x002ff850u, 0x002ff83cu,
     0x0035b238u, 0x00151230u, 0x0bu, ".?AVDarkTalEntity@@"}
};

static void relocate_lifecycle_hero_identity(
    uint8_t *image,
    LifecycleHeroRelocationBackup backups[5]
) {
    size_t hero;

    for (hero = 0u; hero < 5u; ++hero) {
        const LifecycleHeroIdentityFixture *fixture =
            &lifecycle_hero_fixtures[hero];
        const uint32_t vtables[] = {
            fixture->main_vtable_rva,
            fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva
        };
        const uint32_t locators[] = {
            fixture->main_col_rva,
            fixture->secondary_col_rva,
            fixture->resource_col_rva
        };
        size_t subobject;

        for (subobject = 0u; subobject < 3u; ++subobject) {
            uint32_t relocated = (uint32_t)(uintptr_t)(
                image + locators[subobject]);

            memcpy(&backups[hero].vtable_col_pointer[subobject],
                image + vtables[subobject] - 4u, sizeof(uint32_t));
            memcpy(image + vtables[subobject] - 4u,
                &relocated, sizeof(relocated));
            memcpy(&backups[hero].col_type_pointer[subobject],
                image + locators[subobject] + 12u, sizeof(uint32_t));
            relocated = (uint32_t)(uintptr_t)(
                image + fixture->type_descriptor_rva);
            memcpy(image + locators[subobject] + 12u,
                &relocated, sizeof(relocated));
        }
        memcpy(&backups[hero].resource_method_pointer,
            image + fixture->resource_vtable_rva + 0x10u,
            sizeof(uint32_t));
        {
            uint32_t relocated = (uint32_t)(uintptr_t)(
                image + fixture->type_method_rva);

            memcpy(image + fixture->resource_vtable_rva + 0x10u,
                &relocated, sizeof(relocated));
        }
    }
}

static void restore_lifecycle_hero_identity(
    uint8_t *image,
    const LifecycleHeroRelocationBackup backups[5]
) {
    size_t hero;

    for (hero = 0u; hero < 5u; ++hero) {
        const LifecycleHeroIdentityFixture *fixture =
            &lifecycle_hero_fixtures[hero];
        const uint32_t vtables[] = {
            fixture->main_vtable_rva,
            fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva
        };
        const uint32_t locators[] = {
            fixture->main_col_rva,
            fixture->secondary_col_rva,
            fixture->resource_col_rva
        };
        size_t subobject;

        for (subobject = 0u; subobject < 3u; ++subobject) {
            memcpy(image + vtables[subobject] - 4u,
                &backups[hero].vtable_col_pointer[subobject],
                sizeof(uint32_t));
            memcpy(image + locators[subobject] + 12u,
                &backups[hero].col_type_pointer[subobject],
                sizeof(uint32_t));
        }
        memcpy(image + fixture->resource_vtable_rva + 0x10u,
            &backups[hero].resource_method_pointer, sizeof(uint32_t));
    }
}

static BOOL lifecycle_hero_identity_relocations_match(
    const uint8_t *image
) {
    size_t hero;

    for (hero = 0u; hero < 5u; ++hero) {
        const LifecycleHeroIdentityFixture *fixture =
            &lifecycle_hero_fixtures[hero];
        const uint32_t vtables[] = {
            fixture->main_vtable_rva,
            fixture->secondary_vtable_rva,
            fixture->resource_vtable_rva
        };
        const uint32_t locators[] = {
            fixture->main_col_rva,
            fixture->secondary_col_rva,
            fixture->resource_col_rva
        };
        size_t subobject;

        for (subobject = 0u; subobject < 3u; ++subobject) {
            if (*(const uint32_t *)(image + vtables[subobject] - 4u) !=
                    (uint32_t)(uintptr_t)(image + locators[subobject]) ||
                *(const uint32_t *)(image + locators[subobject] + 12u) !=
                    (uint32_t)(uintptr_t)(
                        image + fixture->type_descriptor_rva)) return FALSE;
        }
        if (*(const uint32_t *)(image + fixture->resource_vtable_rva +
                0x10u) != (uint32_t)(uintptr_t)(
                    image + fixture->type_method_rva)) return FALSE;
    }
    return TRUE;
}

static const ExpectedExport expected_exports[] = {
    {0x0030c570u, 0x000d3ae0u},
    {0x0030c698u, 0x000c89c0u},
    {0x0030c69cu, 0x000c8a00u},
    {0x0030ceb4u, 0x000e0460u},
    {0x0030d3d0u, 0x0000f380u},
    {0x0030d3d4u, 0x0000f310u},
    {0x0030d3d8u, 0x0003af80u},
    {0x0030d3dcu, 0x0000f2e0u},
    {0x0030d3e0u, 0x0000f2e0u},
    {0x0030d3e4u, 0x0000f480u},
    {0x0030d3e8u, 0x0000f420u},
    {0x0030d3ecu, 0x0003aff0u},
    {0x0030d3f0u, 0x0000f3f0u},
    {0x0030d3f4u, 0x0000f3f0u}
};

static const ExpectedEntry expected_cleanroom_entries[] = {
    {0x000b1b00u, {0x83,0xec,0x3c,0x56,0x57}, 5u,
        "InternalSpawnPC(ResourceName, xyz)"},
    {0x000b23a0u, {0x81,0xec,0x14,0x01,0x00,0x00,0x56,0x57}, 8u,
        "RemovePC(ResourceName)"},
    {0x000b20d0u, {0x55,0x8b,0xec,0x83,0xe4,0xf8}, 6u,
        "SpawnEntity(name, xyz)"},
    {0x000b2300u, {0x55,0x8b,0xec,0x83,0xe4,0xf8}, 6u,
        "DespawnEntity"},
    {0x00104480u, {0x51,0x8b,0x44,0x24,0x08,0x50}, 6u,
        "GetPC(text)"},
    {0x00104400u, {0x83,0xec,0x0c,0xa1,0x8c,0x9d,0x80,0x00}, 8u,
        "GetGenericEntity(text)"},
    {0x001b9440u, {0x57,0x8b,0xf8,0x8b,0x06,0x25,0x80,0xef}, 8u,
        "ResourceName from text"},
    {0x001b9760u, {0x85,0xc0,0x74,0x2f,0x53,0x8d,0x58,0xfc}, 8u,
        "ResourceName reference release"},
    {0x00025100u, {0xa1,0x94,0x8d,0x80,0x00,0xc3}, 6u,
        "GetGroupPlayers"},
    {RVA_GROUP_PLAYERS_GET_PLAYER_GROUP,
        {0x83,0xec,0x0c,0x8b,0x44,0x24,0x10,0x8d,
         0x44,0x40,0x24,0x8d,0x0c,0x81,0x8d,0x04}, 16u,
        "CGroupPlayers::GetPlayerGroupByPosition"},
    {RVA_TRACKED_ENTITY_CLEANUP,
        {0x8b,0x01,0x33,0xd2,0x3b,0xc2,0x74,0x2d,
         0x56,0x39,0x48,0x04}, 12u,
        "tracked-entity cleanup"},
    {RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR,
        {0x56,0x8b,0xf1,0x56,0xe8,0x17,0x00,0x00,
         0x00,0xf6,0x44,0x24,0x08,0x01}, 14u,
        "GELGroupPtr scalar deleting destructor"},
    {RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER,
        {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,
         0x18,0x83,0xbe,0xcc,0x00,0x00,0x00,0x01}, 16u,
        "CGroupPlayers::PreviousCharacter consumer"},
    {RVA_GROUP_PLAYERS_NEXT_CHARACTER,
        {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,
         0x18,0x83,0xbe,0xcc,0x00,0x00,0x00,0x01}, 16u,
        "CGroupPlayers::NextCharacter consumer"},
    {0x00004fa0u, {0x8a,0x81,0xd4,0x00,0x00,0x00,0xc3}, 7u,
        "CGroupPlayers::InCombat"},
    {0x00024480u, {0x53,0x8b,0x5c,0x24,0x10,0x55,0x8b,0xe9}, 8u,
        "CGroupPlayers combat event transition"},
    {0x0002a880u, {0xa1,0xa8,0x8d,0x80,0x00,0x85,0xc0,0x74}, 8u,
        "SetFirstPersonCameraMode"},
    {0x000b5320u, {0xc6,0x05,0xcc,0x2f,0x7c,0x00,0x01,0xc3}, 8u,
        "NoSpNeeded"},
    {0x0000f5b0u, {0xc6,0x05,0x23,0x2f,0x7c,0x00,0x01,0xc3}, 8u,
        "NoSspNeeded"},
    {0x0000f5e0u, {0x51,0xa1,0x30,0x8d,0x80,0x00,0x85,0xc0}, 8u,
        "GetSsp"},
    {0x0000f5c0u, {0xd9,0x44,0x24,0x04,0x51,0x8b,0x0d,0x30}, 8u,
        "SetSsp"},
    {0x000204d0u, {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec}, 8u,
        "FillInventory"},
    {0x000113a0u, {0xa1,0x30,0x8d,0x80,0x00,0x56,0x8b,0x74}, 8u,
        "SpiritStrikeEnable"},
    {0x000c1270u, {0x83,0xec,0x0c,0x56,0x8b,0x74,0x24,0x14}, 8u,
        "GetCharacterNumberStat"},
    {0x000c1350u, {0x83,0xec,0x0c,0x53,0x56,0x8b,0x74,0x24}, 8u,
        "SetCharacterNumberStat"},
    {0x000d8790u, {0x8b,0x44,0x24,0x04,0x56,0x8b,0xf1,0x83}, 8u,
        "CCharacterWeapon::SetWeapon"},
    {0x000dca10u, {0x80,0x7c,0x24,0x04,0x00,0x74,0x05,0xfe}, 8u,
        "CCharacterArbiter::GELSetInvulnerable"},
    {0x0028be90u, {0xd9,0x44,0x24,0x04,0xd9,0x1d,0x10,0x58}, 8u,
        "SetMasterGameSpeed"},
    {0x000d8280u, {0x53,0x55,0x8b,0x6c,0x24,0x0c,0x56,0x57}, 8u,
        "ranged weapon reattach after model switch"}
};

static uint8_t *read_file(const wchar_t *path, DWORD *file_size) {
    HANDLE file;
    uint8_t *data;
    DWORD bytes_read;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    *file_size = GetFileSize(file, NULL);
    if (*file_size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return NULL;
    }
    data = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, *file_size);
    if (data == NULL || !ReadFile(file, data, *file_size, &bytes_read, NULL) ||
        bytes_read != *file_size) {
        HeapFree(GetProcessHeap(), 0, data);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    return data;
}

static uint8_t *map_pe_image(const uint8_t *file, DWORD file_size) {
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *sections;
    uint8_t *image;
    WORD index;

    if (file == NULL || file_size < sizeof(IMAGE_DOS_HEADER)) {
        return NULL;
    }
    dos = (const IMAGE_DOS_HEADER *)file;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (DWORD)dos->e_lfanew > file_size - sizeof(IMAGE_NT_HEADERS32)) {
        return NULL;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfHeaders > file_size) {
        return NULL;
    }

    image = (uint8_t *)VirtualAlloc(NULL, nt->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (image == NULL) {
        return NULL;
    }
    memcpy(image, file, nt->OptionalHeader.SizeOfHeaders);
    sections = IMAGE_FIRST_SECTION(nt);
    for (index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        DWORD source = sections[index].PointerToRawData;
        DWORD size = sections[index].SizeOfRawData;
        DWORD destination = sections[index].VirtualAddress;
        if (source > file_size || size > file_size - source ||
            destination > nt->OptionalHeader.SizeOfImage ||
            size > nt->OptionalHeader.SizeOfImage - destination) {
            VirtualFree(image, 0, MEM_RELEASE);
            return NULL;
        }
        if (size != 0) {
            memcpy(image + destination, file + source, size);
        }
    }
    return image;
}

static const uint8_t *relative_call_target(const uint8_t *instruction) {
    int32_t displacement;
    if (instruction[0] != 0xe8) {
        return NULL;
    }
    memcpy(&displacement, instruction + 1, sizeof(displacement));
    return instruction + 5 + displacement;
}

static const uint8_t *relative_jump_target(const uint8_t *instruction) {
    int32_t displacement;

    if (instruction[0] != 0xe9) {
        return NULL;
    }
    memcpy(&displacement, instruction + 1, sizeof(displacement));
    return instruction + 5 + displacement;
}

static void check_save_book_intercept_exact_image(
    uint8_t *image,
    int *failures
) {
    static const uint8_t expected_body[] = {
        0xa1, 0x1c, 0x8d, 0x80, 0x00,
        0x85, 0xc0, 0x74, 0x17,
        0x8b, 0x88, 0x70, 0x01, 0x00, 0x00,
        0x85, 0xc9, 0x74, 0x0d,
        0x8b, 0x01, 0x8b, 0x50, 0x2c,
        0x6a, 0x00, 0x6a, 0x00, 0x6a, 0x1b,
        0xff, 0xd2, 0xc3
    };
    static const uint8_t expected_load_entry[] = {
        0x80, 0x3d, 0xf8, 0x9d, 0x80, 0x00, 0x00
    };
    uint8_t original_entry[5];
    uint8_t original_load_entry[7];
    uint32_t relocated_scene_global;
    uint32_t relocated_load_flag;
    const uint8_t *trampoline;
    const uint8_t *load_trampoline;

    if (memcmp(
            image + RVA_SAVE_MENU_SHOW,
            expected_body,
            sizeof(expected_body)) != 0) {
        fputs("FAIL: exact SaveMenuShow cdecl body mismatch\n", stderr);
        ++*failures;
        return;
    }
    if (memcmp(image + RVA_LOAD_GAME_SAVE,
            expected_load_entry, sizeof(expected_load_entry)) != 0) {
        fputs("FAIL: exact LoadGameSave cdecl body mismatch\n", stderr);
        ++*failures;
        return;
    }
    memcpy(original_entry, image + RVA_SAVE_MENU_SHOW, sizeof(original_entry));
    if (!SudekiMpInstallSaveBookIntercept((HMODULE)image, FALSE) ||
        memcmp(image + RVA_SAVE_MENU_SHOW,
            original_entry, sizeof(original_entry)) != 0 ||
        SudekiMpSaveBookInterceptOriginalForTesting() != NULL ||
        SudekiMpSaveBookInterceptLoadGameSaveOriginalForTesting() != NULL) {
        fputs("FAIL: disabled save-book hook was not an inert no-op\n", stderr);
        ++*failures;
        SudekiMpUninstallSaveBookIntercept();
    }

    relocated_scene_global = (uint32_t)(uintptr_t)(
        image + RVA_WORLD_SCENE_GLOBAL);
    memcpy(
        image + RVA_SAVE_MENU_SHOW + 1u,
        &relocated_scene_global,
        sizeof(relocated_scene_global));
    relocated_load_flag = (uint32_t)(uintptr_t)(
        image + 0x00409df8u);
    memcpy(
        image + RVA_LOAD_GAME_SAVE + 2u,
        &relocated_load_flag,
        sizeof(relocated_load_flag));
    memcpy(original_entry, image + RVA_SAVE_MENU_SHOW, sizeof(original_entry));
    memcpy(original_load_entry, image + RVA_LOAD_GAME_SAVE,
        sizeof(original_load_entry));

    *(uint32_t *)(image + RVA_SAVE_MENU_SHOW + 1u) ^=
        UINT32_C(0x00000004);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallSaveBookIntercept((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT) {
        fputs("FAIL: save-book hook accepted a mismatched relocated operand\n",
            stderr);
        ++*failures;
        SudekiMpUninstallSaveBookIntercept();
    }
    memcpy(image + RVA_SAVE_MENU_SHOW, original_entry, sizeof(original_entry));

    *(uint32_t *)(image + RVA_LOAD_GAME_SAVE + 2u) ^=
        UINT32_C(0x00000004);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallSaveBookIntercept((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT) {
        fputs("FAIL: save-book hook accepted a mismatched LoadGameSave operand\n",
            stderr);
        ++*failures;
        SudekiMpUninstallSaveBookIntercept();
    }
    memcpy(image + RVA_LOAD_GAME_SAVE, original_load_entry,
        sizeof(original_load_entry));

    image[RVA_SAVE_MENU_SHOW + 5u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallSaveBookIntercept((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT) {
        fputs("FAIL: save-book hook accepted a mismatched stable tail\n",
            stderr);
        ++*failures;
        SudekiMpUninstallSaveBookIntercept();
    }
    image[RVA_SAVE_MENU_SHOW + 5u] ^= 0xffu;

    if (!SudekiMpInstallSaveBookIntercept((HMODULE)image, TRUE)) {
        fprintf(stderr,
            "FAIL: save-book hook rejected relocated exact image error=%lu\n",
            (unsigned long)GetLastError());
        ++*failures;
        return;
    }
    trampoline = (const uint8_t *)
        SudekiMpSaveBookInterceptOriginalForTesting();
    load_trampoline = (const uint8_t *)
        SudekiMpSaveBookInterceptLoadGameSaveOriginalForTesting();
    if (image[RVA_SAVE_MENU_SHOW] != 0xe9u || trampoline == NULL ||
        memcmp(trampoline, original_entry, sizeof(original_entry)) != 0 ||
        relative_jump_target(trampoline + sizeof(original_entry)) !=
            image + RVA_SAVE_MENU_SHOW + sizeof(original_entry)) {
        fputs("FAIL: save-book entry detour/trampoline shape mismatch\n",
            stderr);
        ++*failures;
    }
    if (image[RVA_LOAD_GAME_SAVE] != 0xe9u || load_trampoline == NULL ||
        memcmp(load_trampoline, original_load_entry,
            sizeof(original_load_entry)) != 0 ||
        relative_jump_target(load_trampoline + sizeof(original_load_entry)) !=
            image + RVA_LOAD_GAME_SAVE + sizeof(original_load_entry)) {
        fputs("FAIL: final LoadGameSave detour/trampoline shape mismatch\n",
            stderr);
        ++*failures;
    }
    SudekiMpUninstallSaveBookIntercept();
    if (memcmp(image + RVA_SAVE_MENU_SHOW,
            original_entry, sizeof(original_entry)) != 0 ||
        memcmp(image + RVA_LOAD_GAME_SAVE, original_load_entry,
            sizeof(original_load_entry)) != 0 ||
        SudekiMpSaveBookInterceptOriginalForTesting() != NULL ||
        SudekiMpSaveBookInterceptLoadGameSaveOriginalForTesting() != NULL) {
        fputs("FAIL: save-book uninstall did not restore exact entry\n",
            stderr);
        ++*failures;
    }
}

typedef struct ZonePatchProbe {
    uint32_t rva;
    size_t size;
    uint8_t original[16];
    const char *name;
} ZonePatchProbe;

static void point_relative_call(uint8_t *instruction, const uint8_t *target) {
    int32_t displacement = (int32_t)(target - (instruction + 5u));

    instruction[0] = 0xe8u;
    memcpy(instruction + 1u, &displacement, sizeof(displacement));
}

static void point_relative_jump(uint8_t *instruction, const uint8_t *target) {
    int32_t displacement = (int32_t)(target - (instruction + 5u));

    instruction[0] = 0xe9u;
    memcpy(instruction + 1u, &displacement, sizeof(displacement));
}

static void check_blacksmith_ui_adapter_exact_image(
    uint8_t *image,
    int *failures
) {
    uint8_t start_original[7];
    uint8_t active_original[7];
    uint8_t mismatched_byte;

    memcpy(start_original, image + RVA_BLACKSMITH_START,
        sizeof(start_original));
    memcpy(active_original, image + RVA_BLACKSMITH_IS_ACTIVE,
        sizeof(active_original));

    if (*(const uint32_t *)(
            image + RVA_BLACKSMITH_START_EXPORT_ENTRY) !=
            RVA_BLACKSMITH_START ||
        *(const uint32_t *)(
            image + RVA_BLACKSMITH_ACTIVE_EXPORT_ENTRY) !=
            RVA_BLACKSMITH_IS_ACTIVE ||
        !SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
            image,
            RVA_BLACKSMITH_START_EXPORT_ENTRY + sizeof(uint32_t),
            (uintptr_t)image)) {
        fputs("FAIL: relocated Blacksmith Start/Active exact gate mismatch\n",
            stderr);
        ++*failures;
        return;
    }

    if (!SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, FALSE) ||
        memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0 ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0) {
        fputs("FAIL: disabled Blacksmith adapter changed the exact image\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }

    SudekiMpBlacksmithUiAdapterInjectSecondHookFailureForTest(TRUE);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE)) {
        fputs("FAIL: injected second Blacksmith hook failure was ignored\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    } else if (GetLastError() != ERROR_GEN_FAILURE ||
        memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0 ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0 ||
        SudekiMpBlacksmithUiAdapterActive()) {
        fputs("FAIL: partial paired-hook failure did not roll back Start\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }

    mismatched_byte = image[RVA_BLACKSMITH_IS_ACTIVE + 5u];
    image[RVA_BLACKSMITH_IS_ACTIVE + 5u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT ||
        memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0) {
        fputs("FAIL: Blacksmith Active mismatch did not reject atomically\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }
    image[RVA_BLACKSMITH_IS_ACTIVE + 5u] = mismatched_byte;

    mismatched_byte = image[RVA_BLACKSMITH_START + 5u];
    image[RVA_BLACKSMITH_START + 5u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0) {
        fputs("FAIL: Blacksmith Start mismatch did not reject atomically\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }
    image[RVA_BLACKSMITH_START + 5u] = mismatched_byte;

    SetLastError(ERROR_SUCCESS);
    if (!SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE)) {
        fprintf(stderr,
            "FAIL: paired Blacksmith adapter rejected exact image (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
    } else {
        if (image[RVA_BLACKSMITH_START] != 0xe9u ||
            image[RVA_BLACKSMITH_IS_ACTIVE] != 0xe9u ||
            memcmp(image + RVA_BLACKSMITH_START + 5u,
                start_original + 5u, 2u) != 0 ||
            memcmp(image + RVA_BLACKSMITH_IS_ACTIVE + 5u,
                active_original + 5u, 2u) != 0) {
            fputs("FAIL: paired Blacksmith entry detours were not exact\n",
                stderr);
            ++*failures;
        }
        SudekiMpUninstallBlacksmithUiAdapter();
    }
    if (memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0 ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0 ||
        SudekiMpBlacksmithUiAdapterActive() ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, FALSE, FALSE, 0u, 0u)) {
        fputs("FAIL: Blacksmith adapter uninstall did not restore native policy\n",
            stderr);
        ++*failures;
    }
}

static void check_blacksmith_roster_actor_identity_policy(int *failures) {
    int actor_marker;
    int other_marker;
    const void *actor = &actor_marker;
    const void *other = &other_marker;

#define CHECK_ROSTER_IDENTITY(expected, expression, label) do { \
    BOOL actual = (expression); \
    if ((actual != FALSE) != (expected)) { \
        fprintf(stderr, "FAIL: Blacksmith roster identity policy %s\n", \
            label); \
        ++*failures; \
    } \
} while (0)
    CHECK_ROSTER_IDENTITY(1,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "rejected exact unique active lease");
    CHECK_ROSTER_IDENTITY(1,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            2u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "rejected exact Player 3 lease");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            3u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted unsupported Player 4 seat");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x01u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted wrong stable type");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 2u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted duplicate active-party type");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, other, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted different resolved actor");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, other,
            TRUE, TRUE, TRUE, FALSE),
        "accepted stale locked actor");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            FALSE, TRUE, TRUE, FALSE),
        "accepted released runtime");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, FALSE, TRUE, FALSE),
        "accepted released roles");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, FALSE, FALSE),
        "accepted dropped participation");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, TRUE),
        "accepted transition quarantine");
#undef CHECK_ROSTER_IDENTITY
}

static void check_adaptive_seat_activation_policy(int *failures) {
    unsigned int active_mask;

#define CHECK_ADAPTIVE_SEATS(expected, expression, label) do { \
    BOOL actual = (expression); \
    if ((actual != FALSE) != (expected)) { \
        fprintf(stderr, "FAIL: adaptive-seat activation policy %s\n", \
            label); \
        ++*failures; \
    } \
} while (0)
    for (active_mask = 0u; active_mask <= 0xffu; ++active_mask) {
        const int valid = active_mask <= 0x0fu &&
            (active_mask & 1u) != 0u;

        CHECK_ADAPTIVE_SEATS(valid,
            SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                TRUE, active_mask, TRUE,
                active_mask, active_mask, active_mask,
                active_mask, active_mask, active_mask, TRUE),
            "did not enforce host-present four-bit active mask");
        CHECK_ADAPTIVE_SEATS(0,
            SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                FALSE, active_mask, TRUE,
                active_mask, active_mask, active_mask,
                active_mask, active_mask, active_mask, TRUE),
            "accepted a disabled feature");
    }
    for (active_mask = 1u; active_mask <= 0x0fu; active_mask += 2u) {
        unsigned int missing_bit;
        unsigned int extra_bit = 0u;

        for (missing_bit = 1u; missing_bit <= 8u; missing_bit <<= 1u) {
            if ((active_mask & missing_bit) == 0u) {
                if (extra_bit == 0u) {
                    extra_bit = missing_bit;
                }
                continue;
            }
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask & ~missing_bit,
                    active_mask, active_mask, active_mask,
                    active_mask, active_mask, TRUE),
                "accepted missing actor lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask & ~missing_bit,
                    active_mask, active_mask,
                    active_mask, active_mask, TRUE),
                "accepted missing camera lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask,
                    active_mask & ~missing_bit, active_mask,
                    active_mask, active_mask, TRUE),
                "accepted missing render-state lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask, active_mask,
                    active_mask & ~missing_bit,
                    active_mask, active_mask, TRUE),
                "accepted missing HUD lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask, active_mask,
                    active_mask, active_mask & ~missing_bit,
                    active_mask, TRUE),
                "accepted missing input lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask, active_mask,
                    active_mask, active_mask,
                    active_mask & ~missing_bit, TRUE),
                "accepted missing frame cache");
        }
        if (extra_bit != 0u) {
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask | extra_bit,
                    active_mask, active_mask, active_mask,
                    active_mask, active_mask, TRUE),
                "accepted an inactive seat actor lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask | extra_bit,
                    active_mask, active_mask,
                    active_mask, active_mask, TRUE),
                "accepted an inactive seat camera lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask,
                    active_mask | extra_bit, active_mask,
                    active_mask, active_mask, TRUE),
                "accepted an inactive seat render-state lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask, active_mask,
                    active_mask | extra_bit,
                    active_mask, active_mask, TRUE),
                "accepted an inactive seat HUD lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask, active_mask,
                    active_mask, active_mask | extra_bit,
                    active_mask, TRUE),
                "accepted an inactive seat input lease");
            CHECK_ADAPTIVE_SEATS(0,
                SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                    TRUE, active_mask, TRUE,
                    active_mask, active_mask, active_mask,
                    active_mask, active_mask,
                    active_mask | extra_bit, TRUE),
                "accepted an inactive seat frame cache");
        }
        CHECK_ADAPTIVE_SEATS(0,
            SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                TRUE, active_mask, FALSE,
                active_mask, active_mask, active_mask,
                active_mask, active_mask, active_mask, TRUE),
            "accepted an unproven viewport layout");
        CHECK_ADAPTIVE_SEATS(0,
            SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
                TRUE, active_mask, TRUE,
                active_mask, active_mask, active_mask,
                active_mask, active_mask, active_mask, FALSE),
            "accepted global presentation ownership");
    }
#undef CHECK_ADAPTIVE_SEATS
}

static void check_fixed_three_owner_evidence_and_orbit_policies(
    int *failures
) {
#define CHECK_FIXED_THREE_POLICY(expected, expression, label) do { \
    BOOL actual = (expression); \
    if ((actual != FALSE) != (expected)) { \
        fprintf(stderr, "FAIL: fixed-three policy %s\n", label); \
        ++*failures; \
    } \
} while (0)
    CHECK_FIXED_THREE_POLICY(1,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            0u, 0u, 0x01u, 0u, 0x01u,
            9u, TRUE, 0u, 9u, TRUE, 0u, FALSE),
        "rejected exact P1 frame-owner evidence");
    CHECK_FIXED_THREE_POLICY(1,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 1u, 0x03u, 1u, 0x03u,
            10u, TRUE, 1u, 10u, TRUE, 1u, FALSE),
        "rejected exact P2 frame-owner evidence");
    CHECK_FIXED_THREE_POLICY(1,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            2u, 2u, 0x03u, 2u, 0x03u,
            11u, TRUE, 2u, 11u, TRUE, 2u, FALSE),
        "rejected exact P3 frame-owner evidence");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            3u, 3u, 0x03u, 3u, 0x03u,
            12u, TRUE, 3u, 12u, TRUE, 3u, FALSE),
        "accepted out-of-range rendered seat");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 0u, 0x03u, 1u, 0x03u,
            10u, TRUE, 1u, 10u, TRUE, 1u, FALSE),
        "accepted wrong-seat HUD evidence");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 1u, 0x01u, 1u, 0x03u,
            10u, TRUE, 1u, 10u, TRUE, 1u, FALSE),
        "accepted incomplete companion HUD roles");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 1u, 0x03u, 0u, 0x03u,
            10u, TRUE, 1u, 10u, TRUE, 1u, FALSE),
        "accepted wrong-seat portrait evidence");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            2u, 2u, 0x03u, 2u, 0x01u,
            11u, TRUE, 2u, 11u, TRUE, 2u, FALSE),
        "accepted incomplete companion portrait roles");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            0u, 0u, 0x01u, 0u, 0x01u,
            0u, TRUE, 0u, 0u, TRUE, 0u, FALSE),
        "accepted an unarmed minimap update epoch");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 1u, 0x03u, 1u, 0x03u,
            10u, FALSE, 1u, 10u, TRUE, 1u, FALSE),
        "accepted a missing minimap update");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 1u, 0x03u, 1u, 0x03u,
            10u, TRUE, 2u, 10u, TRUE, 1u, FALSE),
        "accepted a wrong-seat minimap update");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            1u, 1u, 0x03u, 1u, 0x03u,
            10u, TRUE, 1u, 9u, TRUE, 1u, FALSE),
        "accepted a stale minimap update epoch");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            2u, 2u, 0x03u, 2u, 0x03u,
            11u, TRUE, 2u, 11u, FALSE, 2u, FALSE),
        "accepted a missing minimap render");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            2u, 2u, 0x03u, 2u, 0x03u,
            11u, TRUE, 2u, 11u, TRUE, 1u, FALSE),
        "accepted a wrong-seat minimap render");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
            0u, 0u, 0x01u, 0u, 0x01u,
            9u, TRUE, 0u, 9u, TRUE, 0u, TRUE),
        "accepted sticky UI source failure");

    CHECK_FIXED_THREE_POLICY(1,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x07u, 0x07u, FALSE),
        "rejected exact ready orbit state");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            FALSE, TRUE, TRUE, 0x07u, 0x07u, FALSE),
        "accepted unclear presentation for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, FALSE, TRUE, 0x07u, 0x07u, FALSE),
        "accepted inexact base leases for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, FALSE, 0x07u, 0x07u, FALSE),
        "accepted inexact layout for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x00u, 0x07u, FALSE),
        "accepted empty cache warmup for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x01u, 0x07u, FALSE),
        "accepted one-seat cache warmup for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x03u, 0x07u, FALSE),
        "accepted two-seat cache warmup for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x0fu, 0x07u, FALSE),
        "accepted non-exact cache mask for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x07u, 0x03u, FALSE),
        "accepted incomplete owner evidence for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x07u, 0x0fu, FALSE),
        "accepted non-exact owner evidence for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x07u, 0x07u, TRUE),
        "accepted frozen gameplay input for orbit");
    CHECK_FIXED_THREE_POLICY(0,
        SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
            TRUE, TRUE, TRUE, 0x00u, 0x00u, FALSE),
        "accepted orbit immediately after cache invalidation");
#undef CHECK_FIXED_THREE_POLICY
}

static void check_shared_interaction_modal_runtime(
    uint8_t *image,
    int *failures
) {
    union {
        void *alignment;
        uint8_t bytes[0xc0u];
    } controller_storage;
    union {
        void *alignment;
        uint8_t bytes[0x8cu];
    } shop_storage;
    union {
        void *alignment;
        uint8_t bytes[0x318u];
    } blacksmith_storage;
    uint8_t *controller = controller_storage.bytes;
    uint8_t *shop = shop_storage.bytes;
    uint8_t *blacksmith = blacksmith_storage.bytes;
    SudekiMpPlayerStatehoodSnapshot snapshot;

    ZeroMemory(&controller_storage, sizeof(controller_storage));
    ZeroMemory(&shop_storage, sizeof(shop_storage));
    ZeroMemory(&blacksmith_storage, sizeof(blacksmith_storage));
    *(void **)controller = image + RVA_INGAME_UI_CONTROLLER_VTABLE;
    *(void **)shop = image + RVA_SHOP_LAYER_VTABLE;
    *(void **)blacksmith = image + RVA_BLACKSMITH_LAYER_VTABLE;
    *(void **)(controller + 0x74u) = shop;
    *(void **)(controller + 0x78u) = blacksmith;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL) = controller;
    *(void **)(image + RVA_SHOP_LAYER_GLOBAL) = shop;
    *(void **)(image + RVA_BLACKSMITH_LAYER_GLOBAL) = blacksmith;
    SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehoodRuntime());

    if (!SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            FALSE,
            FALSE,
            0u,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fprintf(stderr,
            "FAIL: shared modal detector install rejected exact image (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
        goto restore_globals;
    }
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: inactive native Shop/Blacksmith reported active\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpSplitScreenNativeMovieOpening() ||
        !SudekiMpSplitScreenNativeMovieActive() ||
        !SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: native movie opening did not synchronously quiesce presentation\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpSplitScreenNativeMovieOpening()) {
        fputs("FAIL: nested native movie opening was rejected\n", stderr);
        ++*failures;
    }
    SudekiMpSplitScreenNativeMovieClosed();
    if (!SudekiMpSplitScreenNativeMovieActive()) {
        fputs("FAIL: nested native movie close released the outer gate\n",
            stderr);
        ++*failures;
    }
    SudekiMpSplitScreenNativeMovieClosed();
    if (SudekiMpSplitScreenNativeMovieActive() ||
        SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: native movie close retained presentation quiescence\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpSplitScreenNativeSaveModalOpening()) {
        fputs("FAIL: save-book pre-native opening was not accepted\n",
            stderr);
        ++*failures;
    }
    /* This assertion is deliberately the first operation after Opening:
     * the save hook's next operation is allowed to be the native continuation. */
    if (!SudekiMpSplitScreenNativeSaveModalActive() ||
        SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
            SudekiMpSplitScreenNativeSaveModalActive(),
            1u,
            2u) ||
        !SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: save-book opening returned before portrait assignment/compositor quiescence\n",
            stderr);
        ++*failures;
    }
    SudekiMpSplitScreenNativeSaveModalClosed();
    if (SudekiMpSplitScreenNativeSaveModalActive() ||
        SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: no-cache save-book close retained a modal/recovery barrier\n",
            stderr);
        ++*failures;
    }
    *(unsigned int *)(controller + 0xb8u) = 9u;
    if (!SudekiMpSplitScreenSharedInteractionModalActive() ||
        !SudekiMpPlayerStatehoodGetSnapshot(
            SudekiMpPlayerStatehoodRuntime(),
            GetTickCount(),
            &snapshot) ||
        snapshot.state != SUDEKIMP_INTERACTION_SESSION_ACTIVE ||
        snapshot.provenance.kind != SUDEKIMP_INTERACTION_SHOP ||
        snapshot.provenance.player_index != 0u) {
        fputs("FAIL: Shop closing-mode full-width/statehood observation mismatch\n",
            stderr);
        ++*failures;
    }
    *(unsigned int *)(controller + 0xb8u) = 0u;
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: no-cache Shop close retained a recovery barrier\n",
            stderr);
        ++*failures;
    }
    blacksmith[0x29u] = 0u;
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: zero AL Blacksmith getter was widened as active\n",
            stderr);
        ++*failures;
    }
    blacksmith[0x29u] = 1u;
    if (!SudekiMpSplitScreenSharedInteractionModalActive() ||
        !SudekiMpPlayerStatehoodGetSnapshot(
            SudekiMpPlayerStatehoodRuntime(),
            GetTickCount(),
            &snapshot) ||
        snapshot.provenance.kind != SUDEKIMP_INTERACTION_BLACKSMITH ||
        snapshot.provenance.player_index != 0u) {
        fputs("FAIL: Blacksmith full-width/statehood observation mismatch\n",
            stderr);
        ++*failures;
    }
    blacksmith[0x29u] = 0u;
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: no-cache Blacksmith close retained a recovery barrier\n",
            stderr);
        ++*failures;
    }
    split_runtime_authorization_result = FALSE;
    SudekiMpSplitScreenSetRuntimeAuthorizationQuery(
        split_runtime_authorization_query);
    if (SudekiMpSplitScreenRuntimeAuthorized()) {
        fputs("FAIL: split runtime query was not active before uninstall\n",
            stderr);
        ++*failures;
    }
    SudekiMpUninstallSplitScreenRender();
    if (!SudekiMpSplitScreenRuntimeAuthorized()) {
        fputs("FAIL: split-screen uninstall retained runtime authorization query\n",
            stderr);
        ++*failures;
    }
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: uninstalled modal inspector retained quiescence\n",
            stderr);
        ++*failures;
    }
    if (SudekiMpSplitScreenNativeSaveModalOpening() ||
        SudekiMpSplitScreenNativeSaveModalActive()) {
        fputs("FAIL: uninstalled save-book lifecycle accepted opening\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpSplitScreenNativeMovieOpening() ||
        SudekiMpSplitScreenNativeMovieActive()) {
        fputs("FAIL: uninstalled native movie lifecycle was not inert\n",
            stderr);
        ++*failures;
    }

restore_globals:
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL) = NULL;
    *(void **)(image + RVA_SHOP_LAYER_GLOBAL) = NULL;
    *(void **)(image + RVA_BLACKSMITH_LAYER_GLOBAL) = NULL;
}

static void capture_zone_patch_probes(
    ZonePatchProbe *probes,
    size_t count,
    const uint8_t *image
) {
    size_t index;

    for (index = 0u; index < count; ++index) {
        memcpy(probes[index].original, image + probes[index].rva,
            probes[index].size);
    }
}

static void check_zone_patch_probes_restored(
    const ZonePatchProbe *probes,
    size_t count,
    const uint8_t *image,
    int *failures
) {
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (memcmp(image + probes[index].rva, probes[index].original,
                probes[index].size) != 0) {
            fprintf(stderr,
                "FAIL: zone transition hook did not restore %s\n",
                probes[index].name);
            ++*failures;
        }
    }
}

static void test_zone_transition_exact_image(
    uint8_t *image,
    int *failures
) {
    static const uint8_t formation_tail[] = {
        0x85u,0xc0u,0x74u,0x10u,0x05u,0xf4u,0x00u,0x00u,
        0x00u,0x74u,0x09u,0x6au,0x00u,0x6au,0x00u
    };
    static const uint8_t set_mode_lead_only_entry[] = {
        0x83u,0xecu,0x14u,0x83u,0xb9u,0xd0u,0x00u,
        0x00u,0x00u,0x00u,0x89u,0x4cu,0x24u,0x04u
    };
    static const uint8_t set_mode_full_party_entry[] = {
        0x83u,0xecu,0x10u,0x53u,0x8bu,0xd9u,0x83u,
        0xbbu,0xd0u,0x00u,0x00u,0x00u,0x01u
    };
    static const uint8_t party_visibility_entry[] = {
        0x83u,0xecu,0x10u,0x53u,0x55u,0x56u,0x57u,
        0x8du,0xa9u,0x9cu,0x00u,0x00u,0x00u
    };
    ZonePatchProbe probes[] = {
        {RVA_ZONE_SET_NOW, 7u, {0}, "SetZoneNow"},
        {RVA_ZONE_ENTER, 5u, {0}, "EnterZone"},
        {RVA_ZONE_SWITCH_NOW, 5u, {0}, "SwitchZoneNow"},
        {RVA_ZONE_LOAD, 10u, {0}, "LoadZone"},
        {RVA_ZONE_SWITCH_MAIN, 7u, {0}, "SwitchMainZone"},
        {RVA_ZONE_DOOR_ACTIVATE, 8u, {0}, "DoorActivateFromScript"},
        {RVA_ZONE_ENTER_TEMPORARY, 9u, {0}, "EnterTemporaryZone"},
        {RVA_ZONE_EXIT_TEMPORARY, 12u, {0}, "ExitTemporaryZone"},
        {RVA_ZONE_SET_PLAYER_POSITION, 5u, {0}, "SetPlayerPosition"},
        {RVA_ZONE_INTERNAL_POSITION_SETTER, 5u, {0},
            "InternalPositionSetter"},
        {RVA_ZONE_HIDE_PARTY_MEMBERS, sizeof(party_visibility_entry), {0},
            "HidePartyMembers"},
        {RVA_ZONE_ENTER_LEAD_POP_CALL, 5u, {0},
            "enter lead placement call"},
        {RVA_ZONE_EXIT_LEAD_MOVE_CALL, 5u, {0},
            "exit lead placement call"}
    };
    const size_t probe_count = sizeof(probes) / sizeof(probes[0]);
    uint8_t saved_call[5];
    uint8_t saved_set_mode_lead_only_byte;
    uint8_t saved_set_mode_full_party_byte;
    uint8_t saved_show_party_members_byte;
    uint8_t saved_hide_party_members_byte;
    uint32_t relocated_ai_manager;
    size_t index;

    if (!SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 0u, 4u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(FALSE, 0u, 4u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 1u, 4u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 0u, 3u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 0u, 0u)) {
        fputs("FAIL: temporary-exit policy confused save-load cleanup with a real co-op exit\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpZoneTransitionShouldDeferExitLeadPlacement(TRUE, TRUE) ||
        SudekiMpZoneTransitionShouldDeferExitLeadPlacement(FALSE, TRUE) ||
        SudekiMpZoneTransitionShouldDeferExitLeadPlacement(TRUE, FALSE) ||
        SudekiMpZoneTransitionShouldDeferExitLeadPlacement(FALSE, FALSE)) {
        fputs("FAIL: temporary-exit lead placement was not always deferred from the inline callsite\n",
            stderr);
        ++*failures;
    }
    if (SudekiMpZoneTransitionExitPresentationAction(
            FALSE, FALSE, FALSE, FALSE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_READY ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, TRUE, FALSE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_READY ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, FALSE, TRUE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_SHOW_OWNED ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, FALSE, TRUE, TRUE) !=
                SUDEKIMP_EXIT_PRESENTATION_WAIT ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, FALSE, FALSE, TRUE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_WAIT ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, FALSE, FALSE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_WAIT) {
        fputs("FAIL: temporary-exit owned presentation lease policy mismatch\n",
            stderr);
        ++*failures;
    }
    if (SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 1.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_WAIT ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_REPOP_NOW ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 2.0f, 0.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST) {
        fputs("FAIL: doorway-staging delayed native-pop policy mismatch\n",
            stderr);
        ++*failures;
    }

    if (image[RVA_ZONE_FORMATION_POP_MEMBERS] != 0xa1u ||
        memcmp(image + RVA_ZONE_FORMATION_POP_MEMBERS + 5u,
            formation_tail, sizeof(formation_tail)) != 0 ||
        memcmp(image + RVA_ZONE_SET_MODE_LEAD_ONLY,
            set_mode_lead_only_entry,
            sizeof(set_mode_lead_only_entry)) != 0 ||
        memcmp(image + RVA_ZONE_SET_MODE_FULL_PARTY,
            set_mode_full_party_entry,
            sizeof(set_mode_full_party_entry)) != 0 ||
        memcmp(image + RVA_ZONE_SHOW_PARTY_MEMBERS,
            party_visibility_entry,
            sizeof(party_visibility_entry)) != 0 ||
        memcmp(image + RVA_ZONE_HIDE_PARTY_MEMBERS,
            party_visibility_entry,
            sizeof(party_visibility_entry)) != 0 ||
        relative_call_target(image + RVA_ZONE_ENTER_LEAD_POP_CALL) !=
            image + RVA_ZONE_POP_TO_NAMED_LOCATION ||
        relative_call_target(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL) !=
            image + RVA_ZONE_EXIT_LEAD_MOVE) {
        fputs("FAIL: exact party-transition formation/callsite seam mismatch\n",
            stderr);
        ++*failures;
        return;
    }
    capture_zone_patch_probes(probes, probe_count, image);

    relocated_ai_manager = (uint32_t)(uintptr_t)(
        image + RVA_ZONE_AI_MANAGER_GLOBAL + 4u);
    memcpy(image + RVA_ZONE_FORMATION_POP_MEMBERS + 1u,
        &relocated_ai_manager, sizeof(relocated_ai_manager));
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched relocated formation global\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    relocated_ai_manager = (uint32_t)(uintptr_t)(
        image + RVA_ZONE_AI_MANAGER_GLOBAL);
    memcpy(image + RVA_ZONE_FORMATION_POP_MEMBERS + 1u,
        &relocated_ai_manager, sizeof(relocated_ai_manager));

    memcpy(saved_call, image + RVA_ZONE_ENTER_LEAD_POP_CALL,
        sizeof(saved_call));
    point_relative_call(image + RVA_ZONE_ENTER_LEAD_POP_CALL,
        image + RVA_ZONE_POP_TO_NAMED_LOCATION + 1u);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched enter callsite\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    memcpy(image + RVA_ZONE_ENTER_LEAD_POP_CALL, saved_call,
        sizeof(saved_call));
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_set_mode_lead_only_byte =
        image[RVA_ZONE_SET_MODE_LEAD_ONLY +
            sizeof(set_mode_lead_only_entry) - 1u];
    image[RVA_ZONE_SET_MODE_LEAD_ONLY +
        sizeof(set_mode_lead_only_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched SetModeLeadOnly entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_SET_MODE_LEAD_ONLY +
        sizeof(set_mode_lead_only_entry) - 1u] =
            saved_set_mode_lead_only_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_set_mode_full_party_byte =
        image[RVA_ZONE_SET_MODE_FULL_PARTY +
            sizeof(set_mode_full_party_entry) - 1u];
    image[RVA_ZONE_SET_MODE_FULL_PARTY +
        sizeof(set_mode_full_party_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched SetModeFullParty entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_SET_MODE_FULL_PARTY +
        sizeof(set_mode_full_party_entry) - 1u] =
            saved_set_mode_full_party_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_show_party_members_byte =
        image[RVA_ZONE_SHOW_PARTY_MEMBERS +
            sizeof(party_visibility_entry) - 1u];
    image[RVA_ZONE_SHOW_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched ShowPartyMembers entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_SHOW_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] =
            saved_show_party_members_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_hide_party_members_byte =
        image[RVA_ZONE_HIDE_PARTY_MEMBERS +
            sizeof(party_visibility_entry) - 1u];
    image[RVA_ZONE_HIDE_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched HidePartyMembers entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_HIDE_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] =
            saved_hide_party_members_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    memcpy(saved_call, image + RVA_ZONE_EXIT_LEAD_MOVE_CALL,
        sizeof(saved_call));
    point_relative_call(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL,
        image + RVA_ZONE_EXIT_LEAD_MOVE + 1u);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched exit callsite\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    memcpy(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL, saved_call,
        sizeof(saved_call));
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    if (!SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE)) {
        fprintf(stderr,
            "FAIL: party-transition exact-image install failed (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
        return;
    }
    for (index = 0u; index + 2u < probe_count; ++index) {
        if (image[probes[index].rva] != 0xe9u) {
            fprintf(stderr, "FAIL: party-transition hook not installed: %s\n",
                probes[index].name);
            ++*failures;
        }
    }
    for (index = 5u; index < sizeof(party_visibility_entry); ++index) {
        if (image[RVA_ZONE_HIDE_PARTY_MEMBERS + index] != 0x90u) {
            fputs("FAIL: HidePartyMembers inline hook did not cover its exact entry\n",
                stderr);
            ++*failures;
            break;
        }
    }
    if (relative_call_target(image + RVA_ZONE_ENTER_LEAD_POP_CALL) ==
            image + RVA_ZONE_POP_TO_NAMED_LOCATION ||
        relative_call_target(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL) ==
            image + RVA_ZONE_EXIT_LEAD_MOVE) {
        fputs("FAIL: party-transition placement callsites were not redirected\n",
            stderr);
        ++*failures;
    }
    SudekiMpUninstallZoneTransitionTrace();
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);
}

static BOOL lifecycle_kazel_seams_match(
    const uint8_t *image,
    BOOL native_group_add_call_expected
) {
    static const uint8_t call_prefix[] = {
        0x8bu, 0x4cu, 0x24u, 0x18u, 0xa1u
    };
    static const uint8_t raw_group_entry[] = {
        0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x8bu, 0x55u,
        0x08u, 0x83u, 0xecu, 0x14u, 0x53u, 0x56u, 0x57u, 0x8bu,
        0xf0u
    };
    static const uint8_t listener_prefix[] = {
        0x8bu, 0x54u, 0x24u, 0x04u, 0x56u, 0x85u, 0xd2u, 0x74u,
        0x10u, 0x51u, 0x8bu, 0xc4u, 0x8du, 0xb1u, 0xb0u, 0x00u,
        0x00u, 0x00u, 0x89u, 0x10u
    };
    static const uint8_t raw_formation_entry[] = {
        0x51u, 0x8bu, 0x4eu, 0x30u, 0x8bu, 0x54u, 0x24u, 0x08u,
        0x33u, 0xc0u, 0x57u, 0x85u, 0xc9u, 0x7eu, 0x0eu
    };
    const uint8_t *window = image + RVA_KAZEL_GROUP_ADD_CALL - 10u;
    uint32_t operand;
    uint32_t listener;
    BOOL call_is_native;

    memcpy(&operand, window + 5u, sizeof(operand));
    memcpy(&listener, image + RVA_AI_LISTENER_VTABLE + 0x18u,
        sizeof(listener));
    call_is_native = relative_call_target(
        (uint8_t *)image + RVA_KAZEL_GROUP_ADD_CALL) ==
        image + RVA_RAW_GROUP_ADD;
    return memcmp(window, call_prefix, sizeof(call_prefix)) == 0 &&
        operand == (uint32_t)(uintptr_t)(
            image + RVA_LIFECYCLE_ACTIVE_GROUP_GLOBAL) &&
        window[9u] == 0x51u && window[10u] == 0xe8u &&
        window[15u] == 0xebu && window[16u] == 0x1bu &&
        call_is_native == native_group_add_call_expected &&
        memcmp(image + RVA_RAW_GROUP_ADD, raw_group_entry,
            sizeof(raw_group_entry)) == 0 &&
        memcmp(image + RVA_RAW_GROUP_ADD + 0x108u,
            "\xc2\x04\x00", 3u) == 0 &&
        listener == (uint32_t)(uintptr_t)(image + RVA_AI_LISTENER_ADD) &&
        memcmp(image + RVA_AI_LISTENER_ADD, listener_prefix,
            sizeof(listener_prefix)) == 0 &&
        relative_call_target(
            (uint8_t *)image + RVA_AI_LISTENER_FORMATION_ADD_CALL) ==
            image + RVA_RAW_FORMATION_ADD &&
        memcmp(image + RVA_AI_LISTENER_ADD + 0x23u,
            "\xc2\x0c\x00", 3u) == 0 &&
        memcmp(image + RVA_RAW_FORMATION_ADD, raw_formation_entry,
            sizeof(raw_formation_entry)) == 0 &&
        memcmp(image + RVA_RAW_FORMATION_ADD + 0x87u,
            "\xc2\x04\x00", 3u) == 0 &&
        memcmp(image + RVA_RAW_FORMATION_ADD + 0x8eu,
            "\xc2\x04\x00", 3u) == 0;
}

static void test_talos_native_lifecycle_exact_image(
    uint8_t *image,
    int *failures
) {
    static const size_t corrupt_rvas[] = {
        RVA_SCRIPT_CALL_OPCODE + 17u,
        RVA_SCRIPT_SCENE_OPCODE + 17u,
        RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL + 5u,
        RVA_DELETE_PC + 7u,
        RVA_REMOVE_ALL_PLAYERS + 1u,
        RVA_REMOVE_ALL_PLAYERS + 15u,
        RVA_FORMATION_POP_MEMBERS + 1u,
        RVA_FORMATION_POP_MEMBERS + 19u,
        RVA_TSA_IS_PLAYING + 1u,
        RVA_TSA_IS_PLAYING + 15u,
        RVA_TSA_SET_PLAYING + 0x01u,
        RVA_TSA_SET_PLAYING + 0x1eu,
        RVA_TSA_SET_PLAYING + 0x56u,
        RVA_TSA_SET_PLAYING + 0x31u,
        RVA_TSA_SET_PLAYING + 0x36u,
        RVA_TSA_SET_PLAYING + 0x5fu,
        RVA_TSA_SET_PLAYING + 0x50u,
        RVA_KAZEL_GROUP_ADD_CALL - 10u,
        RVA_KAZEL_GROUP_ADD_CALL - 5u,
        RVA_KAZEL_GROUP_ADD_CALL + 1u,
        RVA_KAZEL_GROUP_ADD_CALL + 6u,
        RVA_RAW_GROUP_ADD + 16u,
        RVA_RAW_GROUP_ADD + 0x108u,
        RVA_AI_LISTENER_VTABLE + 0x18u,
        RVA_AI_LISTENER_ADD + 19u,
        RVA_AI_LISTENER_FORMATION_ADD_CALL + 1u,
        RVA_AI_LISTENER_ADD + 0x23u,
        RVA_RAW_FORMATION_ADD + 14u,
        RVA_RAW_FORMATION_ADD + 0x87u,
        RVA_RAW_FORMATION_ADD + 0x8eu,
        0x002d5010u - 4u,
        0x002fe4ecu + 4u,
        0x002fec24u + 12u,
        0x0035b1d4u + 8u,
        0x002d5054u + 0x10u,
        0x0014d730u + 5u,
        0x002d6884u - 4u,
        0x002ff850u + 4u,
        0x002ff83cu + 12u,
        0x0035b238u + 8u,
        0x002d68c8u + 0x10u,
        0x00151230u + 5u
    };
    static const char *const corrupt_names[] = {
        "opcode-27 signature",
        "opcode-29 signature",
        "task-constructor call window",
        "DeletePC signature",
        "RemoveAllPlayers relocated global",
        "RemoveAllPlayers signature",
        "AiPCFormationPopMembers relocated global",
        "AiPCFormationPopMembers signature",
        "TSAIsPlaying relocated global",
        "TSAIsPlaying signature",
        "TSASetPlaying playing-state relocation",
        "TSASetPlaying shadow relocation (read)",
        "TSASetPlaying shadow relocation (write)",
        "TSASetPlaying script-manager relocation",
        "TSASetPlaying event-name relocation",
        "TSASetPlaying stable suffix",
        "TSASetPlaying dispatch call target",
        "Kazel completion call prefix",
        "Kazel completion active-group relocation",
        "Kazel completion raw-group call target",
        "Kazel completion call suffix",
        "raw group-add entry",
        "raw group-add ret-4 site",
        "AI listener vtable add slot",
        "AI listener add entry",
        "AI listener formation-add call target",
        "AI listener add ret-12 site",
        "raw formation-add entry",
        "raw formation-add primary ret-4 site",
        "raw formation-add duplicate ret-4 site",
        "hero vtable complete-object-locator pointer",
        "hero complete-object-locator subobject offset",
        "hero complete-object-locator type pointer",
        "hero RTTI type name",
        "hero resource-vtable type-method slot",
        "hero type-method body",
        "DarkTal vtable complete-object-locator pointer",
        "DarkTal complete-object-locator subobject offset",
        "DarkTal complete-object-locator type pointer",
        "DarkTal RTTI type name",
        "DarkTal resource-vtable type-method slot",
        "DarkTal type-method body"
    };
    void **call_slot = (void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT);
    void **scene_slot = (void **)(image + RVA_SCRIPT_SCENE_OPCODE_SLOT);
    void *const native_call = image + RVA_SCRIPT_CALL_OPCODE;
    void *const native_scene = image + RVA_SCRIPT_SCENE_OPCODE;
    SudekiMpTalosNativeLifecycleSnapshot snapshot;
    uint8_t saved_constructor_call[5];
    uint8_t saved_kazel_group_add_window[17];
    uint8_t saved_delete_pc[8];
    uint8_t saved_remove_all_players[16];
    uint8_t saved_formation_pop_members[20];
    uint8_t saved_tsa_is_playing[16];
    uint8_t original_tsa_set_playing[0x60];
    uint8_t saved_tsa_set_playing[0x60];
    LifecycleHeroRelocationBackup hero_relocation_backups[5];
    uint32_t original_kazel_active_group_global;
    uint32_t original_ai_listener_add;
    uint32_t original_remove_all_players_global;
    uint32_t original_formation_pop_members_global;
    uint32_t original_tsa_is_playing_global;
    uint32_t relocated_global;
    size_t index;

    if (relative_call_target(
            image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL) !=
            image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR) {
        fputs("FAIL: exact task-constructor call target mismatch\n", stderr);
        ++*failures;
        return;
    }
    if (relative_call_target(image + RVA_KAZEL_GROUP_ADD_CALL) !=
            image + RVA_RAW_GROUP_ADD ||
        relative_call_target(
            image + RVA_AI_LISTENER_FORMATION_ADD_CALL) !=
            image + RVA_RAW_FORMATION_ADD) {
        fputs("FAIL: exact Kazel native-add call target mismatch\n", stderr);
        ++*failures;
        return;
    }
    memcpy(&original_kazel_active_group_global,
        image + RVA_KAZEL_GROUP_ADD_CALL - 5u,
        sizeof(original_kazel_active_group_global));
    memcpy(&original_ai_listener_add,
        image + RVA_AI_LISTENER_VTABLE + 0x18u,
        sizeof(original_ai_listener_add));
    memcpy(&original_remove_all_players_global,
        image + RVA_REMOVE_ALL_PLAYERS + 1u,
        sizeof(original_remove_all_players_global));
    memcpy(&original_formation_pop_members_global,
        image + RVA_FORMATION_POP_MEMBERS + 1u,
        sizeof(original_formation_pop_members_global));
    memcpy(&original_tsa_is_playing_global,
        image + RVA_TSA_IS_PLAYING + 1u,
        sizeof(original_tsa_is_playing_global));
    memcpy(original_tsa_set_playing, image + RVA_TSA_SET_PLAYING,
        sizeof(original_tsa_set_playing));
    relocated_global = (uint32_t)(uintptr_t)(
        image + RVA_LIFECYCLE_ACTIVE_GROUP_GLOBAL);
    memcpy(image + RVA_REMOVE_ALL_PLAYERS + 1u,
        &relocated_global, sizeof(relocated_global));
    memcpy(image + RVA_KAZEL_GROUP_ADD_CALL - 5u,
        &relocated_global, sizeof(relocated_global));
    relocated_global = (uint32_t)(uintptr_t)(image + RVA_AI_LISTENER_ADD);
    memcpy(image + RVA_AI_LISTENER_VTABLE + 0x18u,
        &relocated_global, sizeof(relocated_global));
    relocated_global = (uint32_t)(uintptr_t)(
        image + RVA_LIFECYCLE_AI_MANAGER_GLOBAL);
    memcpy(image + RVA_FORMATION_POP_MEMBERS + 1u,
        &relocated_global, sizeof(relocated_global));
    relocated_global = (uint32_t)(uintptr_t)(
        image + RVA_LIFECYCLE_TSA_PLAYING_GLOBAL);
    memcpy(image + RVA_TSA_IS_PLAYING + 1u,
        &relocated_global, sizeof(relocated_global));
    memcpy(image + RVA_TSA_SET_PLAYING + 0x01u,
        &relocated_global, sizeof(relocated_global));
    relocated_global = (uint32_t)(uintptr_t)(
        image + RVA_LIFECYCLE_TSA_SHADOW_GLOBAL);
    memcpy(image + RVA_TSA_SET_PLAYING + 0x1eu,
        &relocated_global, sizeof(relocated_global));
    memcpy(image + RVA_TSA_SET_PLAYING + 0x56u,
        &relocated_global, sizeof(relocated_global));
    relocated_global = (uint32_t)(uintptr_t)(
        image + RVA_LIFECYCLE_TSA_SCRIPT_MANAGER_GLOBAL);
    memcpy(image + RVA_TSA_SET_PLAYING + 0x31u,
        &relocated_global, sizeof(relocated_global));
    relocated_global = (uint32_t)(uintptr_t)(
        image + RVA_LIFECYCLE_TSA_EVENT_NAME);
    memcpy(image + RVA_TSA_SET_PLAYING + 0x36u,
        &relocated_global, sizeof(relocated_global));
    point_relative_call(image + RVA_TSA_SET_PLAYING + 0x4fu,
        image + RVA_TSA_DISPATCH);
    if (relative_call_target(image + RVA_TSA_SET_PLAYING + 0x4fu) !=
            image + RVA_TSA_DISPATCH) {
        fputs("FAIL: exact TSASetPlaying dispatch call target mismatch\n",
            stderr);
        ++*failures;
        memcpy(image + RVA_REMOVE_ALL_PLAYERS + 1u,
            &original_remove_all_players_global,
            sizeof(original_remove_all_players_global));
        memcpy(image + RVA_FORMATION_POP_MEMBERS + 1u,
            &original_formation_pop_members_global,
            sizeof(original_formation_pop_members_global));
        memcpy(image + RVA_TSA_IS_PLAYING + 1u,
            &original_tsa_is_playing_global,
            sizeof(original_tsa_is_playing_global));
        memcpy(image + RVA_TSA_SET_PLAYING, original_tsa_set_playing,
            sizeof(original_tsa_set_playing));
        memcpy(image + RVA_KAZEL_GROUP_ADD_CALL - 5u,
            &original_kazel_active_group_global,
            sizeof(original_kazel_active_group_global));
        memcpy(image + RVA_AI_LISTENER_VTABLE + 0x18u,
            &original_ai_listener_add, sizeof(original_ai_listener_add));
        return;
    }
    relocate_lifecycle_hero_identity(image, hero_relocation_backups);
    memcpy(saved_constructor_call,
        image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
        sizeof(saved_constructor_call));
    memcpy(saved_kazel_group_add_window,
        image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
        sizeof(saved_kazel_group_add_window));
    memcpy(saved_delete_pc, image + RVA_DELETE_PC,
        sizeof(saved_delete_pc));
    memcpy(saved_remove_all_players, image + RVA_REMOVE_ALL_PLAYERS,
        sizeof(saved_remove_all_players));
    memcpy(saved_formation_pop_members,
        image + RVA_FORMATION_POP_MEMBERS,
        sizeof(saved_formation_pop_members));
    memcpy(saved_tsa_is_playing, image + RVA_TSA_IS_PLAYING,
        sizeof(saved_tsa_is_playing));
    memcpy(saved_tsa_set_playing, image + RVA_TSA_SET_PLAYING,
        sizeof(saved_tsa_set_playing));

    SetLastError(ERROR_SUCCESS);
    if (!SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE)) {
        fprintf(stderr,
            "FAIL: Talos lifecycle exact-image install failed (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
        memcpy(image + RVA_REMOVE_ALL_PLAYERS + 1u,
            &original_remove_all_players_global,
            sizeof(original_remove_all_players_global));
        memcpy(image + RVA_FORMATION_POP_MEMBERS + 1u,
            &original_formation_pop_members_global,
            sizeof(original_formation_pop_members_global));
        memcpy(image + RVA_TSA_IS_PLAYING + 1u,
            &original_tsa_is_playing_global,
            sizeof(original_tsa_is_playing_global));
        memcpy(image + RVA_TSA_SET_PLAYING, original_tsa_set_playing,
            sizeof(original_tsa_set_playing));
        memcpy(image + RVA_KAZEL_GROUP_ADD_CALL - 5u,
            &original_kazel_active_group_global,
            sizeof(original_kazel_active_group_global));
        memcpy(image + RVA_AI_LISTENER_VTABLE + 0x18u,
            &original_ai_listener_add, sizeof(original_ai_listener_add));
        restore_lifecycle_hero_identity(image, hero_relocation_backups);
        return;
    }
    if (*call_slot == native_call || *scene_slot == native_scene) {
        fputs("FAIL: Talos lifecycle install did not own both opcode slots\n",
            stderr);
        ++*failures;
    }
    if (relative_call_target(
            image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL) ==
            image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR ||
        !lifecycle_kazel_seams_match(image, FALSE) ||
        memcmp(image + RVA_DELETE_PC, saved_delete_pc,
            sizeof(saved_delete_pc)) == 0 ||
        memcmp(image + RVA_REMOVE_ALL_PLAYERS, saved_remove_all_players,
            5u) == 0 ||
        memcmp(image + RVA_FORMATION_POP_MEMBERS,
            saved_formation_pop_members, 5u) == 0 ||
        memcmp(image + RVA_TSA_IS_PLAYING, saved_tsa_is_playing,
            sizeof(saved_tsa_is_playing)) != 0 ||
        memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing,
            5u) == 0 ||
        memcmp(image + RVA_TSA_SET_PLAYING + 5u,
            saved_tsa_set_playing + 5u,
            sizeof(saved_tsa_set_playing) - 5u) != 0 ||
        !lifecycle_hero_identity_relocations_match(image)) {
        fputs("FAIL: Talos lifecycle install did not own every native edge\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpTalosNativeLifecycleGetSnapshot(&snapshot) ||
        snapshot.installed == 0u ||
        snapshot.native_passthrough_required == 0u ||
        snapshot.mutation_supported != 0u) {
        fputs("FAIL: Talos lifecycle installed snapshot is not inert passthrough\n",
            stderr);
        ++*failures;
    }
    SudekiMpUninstallTalosNativeLifecycleTrace();
    if (*call_slot != native_call || *scene_slot != native_scene ||
        memcmp(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
            saved_constructor_call, sizeof(saved_constructor_call)) != 0 ||
        memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
            saved_kazel_group_add_window,
            sizeof(saved_kazel_group_add_window)) != 0 ||
        !lifecycle_kazel_seams_match(image, TRUE) ||
        memcmp(image + RVA_DELETE_PC, saved_delete_pc,
            sizeof(saved_delete_pc)) != 0 ||
        memcmp(image + RVA_REMOVE_ALL_PLAYERS, saved_remove_all_players,
            sizeof(saved_remove_all_players)) != 0 ||
        memcmp(image + RVA_FORMATION_POP_MEMBERS,
            saved_formation_pop_members,
            sizeof(saved_formation_pop_members)) != 0 ||
        memcmp(image + RVA_TSA_IS_PLAYING, saved_tsa_is_playing,
            sizeof(saved_tsa_is_playing)) != 0 ||
        memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing,
            sizeof(saved_tsa_set_playing)) != 0 ||
        !lifecycle_hero_identity_relocations_match(image)) {
        fputs("FAIL: Talos lifecycle uninstall did not exactly restore every seam\n",
            stderr);
        ++*failures;
    }

    for (index = 0u;
            index < sizeof(corrupt_rvas) / sizeof(corrupt_rvas[0]);
            ++index) {
        uint8_t saved_byte = image[corrupt_rvas[index]];

        image[corrupt_rvas[index]] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE) ||
            GetLastError() != ERROR_BAD_EXE_FORMAT) {
            fprintf(stderr,
                "FAIL: Talos lifecycle accepted a mismatched %s\n",
                corrupt_names[index]);
            ++*failures;
            SudekiMpUninstallTalosNativeLifecycleTrace();
        }
        image[corrupt_rvas[index]] = saved_byte;
        if (*call_slot != native_call || *scene_slot != native_scene ||
            memcmp(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
                saved_constructor_call,
                sizeof(saved_constructor_call)) != 0 ||
            memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
                saved_kazel_group_add_window,
                sizeof(saved_kazel_group_add_window)) != 0 ||
            !lifecycle_kazel_seams_match(image, TRUE) ||
            memcmp(image + RVA_DELETE_PC, saved_delete_pc,
                sizeof(saved_delete_pc)) != 0 ||
            memcmp(image + RVA_REMOVE_ALL_PLAYERS,
                saved_remove_all_players,
                sizeof(saved_remove_all_players)) != 0 ||
            memcmp(image + RVA_FORMATION_POP_MEMBERS,
                saved_formation_pop_members,
                sizeof(saved_formation_pop_members)) != 0 ||
            memcmp(image + RVA_TSA_IS_PLAYING, saved_tsa_is_playing,
                sizeof(saved_tsa_is_playing)) != 0 ||
            memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing,
                sizeof(saved_tsa_set_playing)) != 0 ||
            !lifecycle_hero_identity_relocations_match(image)) {
            fprintf(stderr,
                "FAIL: Talos lifecycle %s rejection changed a native seam\n",
                corrupt_names[index]);
            ++*failures;
        }
    }

    *call_slot = image + RVA_SCRIPT_METHOD_OPCODE;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallTalosNativeLifecycleTrace((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BUSY) {
        fputs("FAIL: Talos lifecycle did not reject a pre-owned opcode-27 slot\n",
            stderr);
        ++*failures;
        SudekiMpUninstallTalosNativeLifecycleTrace();
    }
    if (*call_slot != image + RVA_SCRIPT_METHOD_OPCODE ||
        *scene_slot != native_scene ||
        memcmp(image + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
            saved_constructor_call, sizeof(saved_constructor_call)) != 0 ||
        memcmp(image + RVA_KAZEL_GROUP_ADD_CALL - 10u,
            saved_kazel_group_add_window,
            sizeof(saved_kazel_group_add_window)) != 0 ||
        !lifecycle_kazel_seams_match(image, TRUE) ||
        memcmp(image + RVA_DELETE_PC, saved_delete_pc,
            sizeof(saved_delete_pc)) != 0 ||
        memcmp(image + RVA_REMOVE_ALL_PLAYERS, saved_remove_all_players,
            sizeof(saved_remove_all_players)) != 0 ||
        memcmp(image + RVA_FORMATION_POP_MEMBERS,
            saved_formation_pop_members,
            sizeof(saved_formation_pop_members)) != 0 ||
        memcmp(image + RVA_TSA_IS_PLAYING, saved_tsa_is_playing,
            sizeof(saved_tsa_is_playing)) != 0 ||
        memcmp(image + RVA_TSA_SET_PLAYING, saved_tsa_set_playing,
            sizeof(saved_tsa_set_playing)) != 0 ||
        !lifecycle_hero_identity_relocations_match(image)) {
        fputs("FAIL: Talos lifecycle busy rejection changed a native seam\n",
            stderr);
        ++*failures;
    }
    *call_slot = native_call;
    memcpy(image + RVA_REMOVE_ALL_PLAYERS + 1u,
        &original_remove_all_players_global,
        sizeof(original_remove_all_players_global));
    memcpy(image + RVA_FORMATION_POP_MEMBERS + 1u,
        &original_formation_pop_members_global,
        sizeof(original_formation_pop_members_global));
    memcpy(image + RVA_TSA_IS_PLAYING + 1u,
        &original_tsa_is_playing_global,
        sizeof(original_tsa_is_playing_global));
    memcpy(image + RVA_TSA_SET_PLAYING, original_tsa_set_playing,
        sizeof(original_tsa_set_playing));
    memcpy(image + RVA_KAZEL_GROUP_ADD_CALL - 5u,
        &original_kazel_active_group_global,
        sizeof(original_kazel_active_group_global));
    memcpy(image + RVA_AI_LISTENER_VTABLE + 0x18u,
        &original_ai_listener_add, sizeof(original_ai_listener_add));
    restore_lifecycle_hero_identity(image, hero_relocation_backups);
}

int wmain(int argc, wchar_t **argv) {
    uint8_t *file;
    uint8_t *image;
    DWORD file_size;
    size_t index;
    int failures = 0;
    unsigned int quick_menu_isolation_state;
    BOOL shared_modal_recovery_pending;
    unsigned int roster_player_one_type;
    unsigned int roster_player_two_type;
    SudekiMpCoopRosterAssignment three_seat_assignment;
    int player_two_character_marker = 0;
    int other_character_marker = 0;
    uint8_t minimap_snapshot_call_original[5];
    uint32_t raw_lan_client_quick_menu_input;
    uint32_t raw_lan_client_camera_input;
    uint32_t raw_lan_client_character_input;
    uint32_t raw_lan_pause_quit_show_global;
    const UINT second_player_skill_keys[4] = {
        VK_F1, VK_F2, VK_F3, VK_F4
    };
    static const uint8_t set_render_camera_original[] = {
        0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
    };

    if (!SudekiMpSplitScreenRuntimeAuthorizationPolicy(
            TRUE, FALSE, FALSE) ||
        !SudekiMpSplitScreenRuntimeAuthorizationPolicy(
            TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenRuntimeAuthorizationPolicy(
            FALSE, FALSE, TRUE) ||
        SudekiMpSplitScreenRuntimeAuthorizationPolicy(
            TRUE, TRUE, FALSE)) {
        fputs("FAIL: split runtime authorization policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
            FALSE, FALSE, FALSE, NULL, NULL) ||
        !SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
            TRUE, TRUE, TRUE,
            &player_two_character_marker,
            &player_two_character_marker) ||
        SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
            TRUE, FALSE, TRUE,
            &player_two_character_marker,
            &player_two_character_marker) ||
        SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
            TRUE, TRUE, FALSE,
            &player_two_character_marker,
            &player_two_character_marker) ||
        SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
            TRUE, TRUE, TRUE, NULL, NULL) ||
        SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
            TRUE, TRUE, TRUE,
            &player_two_character_marker,
            &other_character_marker)) {
        fputs("FAIL: external Player 2 lease identity policy\n", stderr);
        ++failures;
    }
    split_runtime_authorization_result = FALSE;
    SudekiMpSplitScreenSetRuntimeAuthorizationQuery(
        split_runtime_authorization_query);
    SetLastError(0x1234u);
    if (SudekiMpSplitScreenRuntimeAuthorized() ||
        GetLastError() != 0x1234u) {
        fputs("FAIL: false split runtime authorization query or LastError\n",
            stderr);
        ++failures;
    }
    split_runtime_authorization_result = TRUE;
    SetLastError(0x5678u);
    if (!SudekiMpSplitScreenRuntimeAuthorized() ||
        GetLastError() != 0x5678u) {
        fputs("FAIL: true split runtime authorization query or LastError\n",
            stderr);
        ++failures;
    }
    SudekiMpSplitScreenSetRuntimeAuthorizationQuery(NULL);

    if (!SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
            0, 1u, 1, 0u) ||
        SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
            1, 1u, 2, 0u) ||
        SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
            0, 0u, 1, 0u) ||
        SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
            0, 1u, 2, 0u) ||
        SudekiMpControlSeparationAiLeaseAcquireTransitionExact(
            0, 1u, 1, 1u)) {
        fputs("FAIL: AI lease acquire transition policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            1, 0u, 0, 1u, FALSE) ||
        !SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            1, 0u, 0, 0u, TRUE) ||
        !SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            2, 0u, 1, 0u, FALSE) ||
        !SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            3, 0u, 2, 0u, FALSE) ||
        !SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            INT16_MAX, 0u, INT16_MAX - 1, 0u, FALSE) ||
        SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            0, 0u, 0, 1u, FALSE) ||
        SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            1, 1u, 0, 1u, FALSE) ||
        SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            2, 0u, 0, 1u, FALSE) ||
        SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            2, 0u, 1, 1u, FALSE) ||
        SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            1, 0u, 0, 0u, FALSE) ||
        SudekiMpControlSeparationAiLeaseReleaseTransitionExact(
            1, 0u, 0, 1u, TRUE)) {
        fputs("FAIL: AI lease release transition policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            1u, TRUE, TRUE, TRUE, FALSE) ||
        !SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            2u, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            0u, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            3u, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            1u, FALSE, TRUE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            1u, TRUE, FALSE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            1u, TRUE, TRUE, FALSE, TRUE) ||
        SudekiMpControlSeparationSeatSubmissionReadyPolicy(
            2u, TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: companion seat submission readiness policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatRequestTransitionPolicy(
            1u, TRUE, FALSE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatRequestTransitionPolicy(
            1u, FALSE, TRUE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatRequestTransitionPolicy(
            1u, FALSE, FALSE, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatRequestTransitionPolicy(
            1u, TRUE, TRUE, TRUE, TRUE) ||
        !SudekiMpControlSeparationSeatRequestTransitionPolicy(
            1u, FALSE, TRUE, TRUE, FALSE) ||
        !SudekiMpControlSeparationSeatRequestTransitionPolicy(
            2u, TRUE, TRUE, TRUE, FALSE) ||
        SudekiMpControlSeparationSeatRequestTransitionPolicy(
            2u, TRUE, TRUE, FALSE, FALSE) ||
        !SudekiMpControlSeparationSeatRequestTransitionPolicy(
            2u, FALSE, TRUE, FALSE, TRUE) ||
        SudekiMpControlSeparationSeatRequestTransitionPolicy(
            0u, TRUE, FALSE, TRUE, FALSE)) {
        fputs("FAIL: companion seat request/dependency policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatAcquireOrderPolicy(
            1u, FALSE) ||
        !SudekiMpControlSeparationSeatAcquireOrderPolicy(
            2u, TRUE) ||
        SudekiMpControlSeparationSeatAcquireOrderPolicy(
            2u, FALSE) ||
        SudekiMpControlSeparationSeatAcquireOrderPolicy(
            3u, TRUE)) {
        fputs("FAIL: companion P2-to-P3 acquire order policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatReleaseOrderPolicy(
            2u, TRUE) ||
        !SudekiMpControlSeparationSeatReleaseOrderPolicy(
            1u, FALSE) ||
        SudekiMpControlSeparationSeatReleaseOrderPolicy(
            1u, TRUE) ||
        SudekiMpControlSeparationSeatReleaseOrderPolicy(
            0u, FALSE)) {
        fputs("FAIL: companion P3-to-P2 release order policy\n", stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationDeferReleaseToRosterPolicy(
            TRUE, TRUE, TRUE) ||
        SudekiMpControlSeparationDeferReleaseToRosterPolicy(
            TRUE, TRUE, FALSE) ||
        SudekiMpControlSeparationDeferReleaseToRosterPolicy(
            TRUE, FALSE, TRUE) ||
        SudekiMpControlSeparationDeferReleaseToRosterPolicy(
            FALSE, TRUE, TRUE)) {
        fputs("FAIL: fixed-three camera-first release deferral policy\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatInputLeaseExactPolicy(
            (const void *)(uintptr_t)0x11111111u, 7u,
            (const void *)(uintptr_t)0x11111111u, 7u) ||
        SudekiMpControlSeparationSeatInputLeaseExactPolicy(
            NULL, 7u, (const void *)(uintptr_t)0x11111111u, 7u) ||
        SudekiMpControlSeparationSeatInputLeaseExactPolicy(
            (const void *)(uintptr_t)0x11111111u, 0u,
            (const void *)(uintptr_t)0x11111111u, 0u) ||
        SudekiMpControlSeparationSeatInputLeaseExactPolicy(
            (const void *)(uintptr_t)0x11111111u, 7u,
            (const void *)(uintptr_t)0x22222222u, 7u) ||
        SudekiMpControlSeparationSeatInputLeaseExactPolicy(
            (const void *)(uintptr_t)0x11111111u, 7u,
            (const void *)(uintptr_t)0x11111111u, 8u)) {
        fputs("FAIL: companion input identity/generation lease policy\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            1u, FALSE, FALSE) ||
        !SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            1u, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            1u, TRUE, FALSE) ||
        !SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            2u, TRUE, TRUE) ||
        SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            2u, TRUE, FALSE) ||
        SudekiMpControlSeparationSeatAcquireInputReadyPolicy(
            0u, FALSE, TRUE)) {
        fputs("FAIL: fixed-three pre-acquire input readiness policy\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
            TRUE, TRUE) ||
        SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
            TRUE, FALSE) ||
        SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
            FALSE, TRUE) ||
        SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
            FALSE, FALSE)) {
        fputs("FAIL: fixed-three atomic two-seat input preflight policy\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            1u, FALSE, TRUE, NULL, 0u, NULL, 0u) ||
        SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            1u, FALSE, FALSE, NULL, 0u, NULL, 0u) ||
        !SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            1u, TRUE, TRUE,
            (const void *)(uintptr_t)0x11111111u, 7u,
            (const void *)(uintptr_t)0x11111111u, 7u) ||
        SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            1u, TRUE, TRUE,
            (const void *)(uintptr_t)0x11111111u, 7u,
            (const void *)(uintptr_t)0x11111111u, 8u) ||
        !SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            2u, TRUE, TRUE,
            (const void *)(uintptr_t)0x22222222u, 9u,
            (const void *)(uintptr_t)0x22222222u, 9u) ||
        SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            2u, TRUE, TRUE,
            (const void *)(uintptr_t)0x22222222u, 9u,
            (const void *)(uintptr_t)0x22222222u, 10u) ||
        SudekiMpControlSeparationSeatActiveInputLeasePolicy(
            2u, TRUE, FALSE,
            (const void *)(uintptr_t)0x22222222u, 9u,
            (const void *)(uintptr_t)0x22222222u, 9u)) {
        fputs("FAIL: fixed-three active input lease policy\n", stderr);
        ++failures;
    }

    if (argc != 2) {
        fwprintf(stderr, L"usage: SudekiMP.SkillTraceImageTest.exe SUDEKI.exe\n");
        return 2;
    }
    file = read_file(argv[1], &file_size);
    if (file == NULL) {
        fwprintf(stderr, L"failed to read PE image (error=%lu)\n",
            (unsigned long)GetLastError());
        return 1;
    }
    image = map_pe_image(file, file_size);
    HeapFree(GetProcessHeap(), 0, file);
    if (image == NULL) {
        fputs("failed to map PE image\n", stderr);
        return 1;
    }
    {
        int selector = -1;
        if (!SudekiMpLanArenaClientIdleVariantSelector(
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
                &selector) || selector != 4 ||
            !SudekiMpLanArenaClientIdleVariantSelector(
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO,
                &selector) || selector != 5 ||
            !SudekiMpLanArenaClientIdleVariantSelector(
                SUDEKIMP_LAN_ARENA_TAL_TYPE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
                &selector) || selector != 10 ||
            SudekiMpLanArenaClientIdleVariantSelector(
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                &selector)) {
            fputs("FAIL: LAN idle-variant semantic mapping mismatch\n", stderr);
            ++failures;
        }
        if (SudekiMpLanArenaClientAnimationShouldResetTime(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                FALSE) ||
            SudekiMpLanArenaClientAnimationShouldResetTime(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
                TRUE) ||
            !SudekiMpLanArenaClientAnimationShouldResetTime(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
                FALSE) ||
            !SudekiMpLanArenaClientAnimationShouldResetTime(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
                FALSE) ||
            !SudekiMpLanArenaClientAnimationShouldResetTime(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                FALSE) ||
            SudekiMpLanArenaClientAnimationShouldResetTime(
                1u,
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                FALSE) ||
            SudekiMpLanArenaClientAnimationShouldResetTime(
                2u,
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
                SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
                FALSE)) {
            fprintf(stderr,
                "LAN client animation time-reset policy mismatch\n");
            ++failures;
        }
        if (SudekiMpLanArenaClientAnimationTransitionState(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                128) != 0 ||
            SudekiMpLanArenaClientAnimationTransitionState(
                1u,
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                128) != 128 ||
            SudekiMpLanArenaClientAnimationTransitionState(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                128) != 128 ||
            SudekiMpLanArenaClientAnimationTransitionState(
                0u,
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,
                SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
                0) != 0) {
            fprintf(stderr,
                "LAN client animation transition-state policy mismatch\n");
            ++failures;
        }
        {
            float phase = -1.0f;
            if (!SudekiMpLanArenaClientRetirementPreUpdatePhase(
                    0.204f, 13u, FALSE, &phase) ||
                fabsf(phase - 0.048f) > 0.0001f ||
                !SudekiMpLanArenaClientRetirementPreUpdatePhase(
                    0.204f, 13u, TRUE, &phase) ||
                fabsf(phase - 0.204f) > 0.0001f ||
                !SudekiMpLanArenaClientRetirementPreUpdatePhase(
                    0.100f, 17u, FALSE, &phase) ||
                fabsf(phase) > 0.0001f ||
                SudekiMpLanArenaClientRetirementPreUpdatePhase(
                    -1.0f, 13u, FALSE, &phase) ||
                SudekiMpLanArenaClientRetirementPreUpdatePhase(
                    0.204f, 51u, FALSE, &phase) ||
                SudekiMpLanArenaClientRetirementPreUpdatePhase(
                    0.204f, 13u, FALSE, NULL)) {
                fprintf(stderr,
                    "LAN client retirement pre-update phase policy mismatch\n");
                ++failures;
            }
        }
        if (!SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
                0u, TRUE, FALSE, TRUE) ||
            !SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
                0u, TRUE, TRUE, FALSE) ||
            SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
                0u, FALSE, FALSE, FALSE) ||
            SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
                0u, TRUE, TRUE, TRUE) ||
            SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
                1u, TRUE, FALSE, FALSE) ||
            !SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
                1u, TRUE, TRUE, FALSE)) {
            fprintf(stderr,
                "LAN client secondary animation reset policy mismatch\n");
            ++failures;
        }
        if (!SudekiMpLanArenaClientPresentationOverrideAllowed(FALSE) ||
            !SudekiMpLanArenaClientPresentationOverrideAllowed(TRUE)) {
            fputs("FAIL: LAN client exact-mapped presentation policy mismatch\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpLanArenaClientActorPresentationAllowed(
                0u, TRUE, TRUE, FALSE) ||
            SudekiMpLanArenaClientActorPresentationAllowed(
                1u, TRUE, TRUE, FALSE) ||
            !SudekiMpLanArenaClientActorPresentationAllowed(
                1u, TRUE, FALSE, TRUE) ||
            !SudekiMpLanArenaClientActorPresentationAllowed(
                0u, FALSE, FALSE, FALSE) ||
            SudekiMpLanArenaClientActorPresentationAllowed(
                2u, FALSE, TRUE, TRUE)) {
            fputs("FAIL: LAN client actor-local presentation gate mismatch\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpLanArenaClientTalTransitionSelectorReady(TRUE, 17) ||
            !SudekiMpLanArenaClientTalTransitionSelectorReady(TRUE, 36) ||
            SudekiMpLanArenaClientTalTransitionSelectorReady(TRUE, 4) ||
            !SudekiMpLanArenaClientTalTransitionSelectorReady(FALSE, 4) ||
            !SudekiMpLanArenaClientTalTransitionSelectorReady(FALSE, 8) ||
            SudekiMpLanArenaClientTalTransitionSelectorReady(FALSE, 36)) {
            fputs("FAIL: LAN client Tal transition selector gate mismatch\n",
                stderr);
            ++failures;
        }
        if (SudekiMpLanArenaClientCombatTransitionRefreshDue(
                TRUE, FALSE, 99u) ||
            !SudekiMpLanArenaClientCombatTransitionRefreshDue(
                TRUE, FALSE, 100u) ||
            SudekiMpLanArenaClientCombatTransitionRefreshDue(
                TRUE, TRUE, 1000u) ||
            SudekiMpLanArenaClientCombatTransitionRefreshDue(
                FALSE, FALSE, 1000u)) {
            fputs("FAIL: LAN client combat transition refresh policy mismatch\n",
                stderr);
            ++failures;
        }
        if (SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
                12.0f, 12.009f) ||
            !SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
                12.0f, 12.011f) ||
            !SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
                NAN, 12.0f) ||
            !SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
                12.0f, -1.0f)) {
            fputs("FAIL: LAN client animation phase correction policy mismatch\n",
                stderr);
            ++failures;
        }
        {
            static const struct {
                uint8_t action;
                int selector;
                int state;
            } tal_actions[] = {
                { SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 50, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO, 51, 1 },
                { SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE, 62, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_STRONG, 52, 1 },
                { SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO, 53, 1 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS, 54, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW, 60, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS, 61, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS, 63, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW, 65, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW, 68, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS, 69, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE, 70, 65 },
                { SUDEKIMP_LAN_ARENA_ACTION_SWEEP, 71, 1 },
                { SUDEKIMP_LAN_ARENA_ACTION_BLOCK, 20, 65 }
            };
            unsigned int action_index;
            int action_selector = -1;
            int action_state = -1;
            int weak = -1;
            int strong = -1;
            int sweep = -1;
            int block = -1;
            for (action_index = 0u;
                 action_index < sizeof(tal_actions) / sizeof(tal_actions[0]);
                 ++action_index) {
                if (!SudekiMpLanArenaClientTalActionPresentation(
                        tal_actions[action_index].action,
                        &action_selector, &action_state) ||
                    action_selector != tal_actions[action_index].selector ||
                    action_state != tal_actions[action_index].state) {
                    fputs("FAIL: LAN client Tal semantic action mapping\n",
                        stderr);
                    ++failures;
                    break;
                }
                if (!SudekiMpLanArenaClientTalNativeCombatInput(
                        tal_actions[action_index].action,
                        &weak, &strong, &sweep, &block) ||
                    weak + strong + sweep + block != 1) {
                    fputs("FAIL: LAN client Tal native action input mapping\n",
                        stderr);
                    ++failures;
                    break;
                }
            }
            if (SudekiMpLanArenaClientTalActionPresentation(
                    SUDEKIMP_LAN_ARENA_ACTION_NONE,
                    &action_selector, &action_state) ||
                SudekiMpLanArenaClientTalActionPresentation(
                    SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
                    NULL, &action_state) ||
                SudekiMpLanArenaClientTalActionPresentation(
                    SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
                    &action_selector, NULL)) {
                fputs("FAIL: LAN client Tal semantic action rejection\n",
                    stderr);
                ++failures;
            }
            if (SudekiMpLanArenaClientTalNativeCombatInput(
                    SUDEKIMP_LAN_ARENA_ACTION_NONE,
                    &weak, &strong, &sweep, &block) ||
                SudekiMpLanArenaClientTalNativeCombatInput(
                    SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
                    NULL, &strong, &sweep, &block)) {
                fputs("FAIL: LAN client Tal native action input rejection\n",
                    stderr);
                ++failures;
            }
        }
        if (!SudekiMpLanArenaClientShouldApplyHostFacing(0u, FALSE) ||
            !SudekiMpLanArenaClientShouldApplyHostFacing(0u, TRUE) ||
            !SudekiMpLanArenaClientShouldApplyHostFacing(1u, FALSE) ||
            SudekiMpLanArenaClientShouldApplyHostFacing(1u, TRUE) ||
            SudekiMpLanArenaClientShouldApplyHostFacing(2u, FALSE)) {
            fputs("FAIL: LAN client first-person facing ownership policy mismatch\n",
                stderr);
            ++failures;
        }
        {
            SudekiMpLanArenaActorSnapshot phased_action;
            float phase_time = -1.0f;
            memset(&phased_action, 0, sizeof(phased_action));
            phased_action.animation_state =
                SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
            phased_action.action_phase_valid = 1u;
            phased_action.action_phase_q8 = 35u * 256u + 128u;
            if (!SudekiMpLanArenaClientActionPhaseTime(
                    &phased_action, &phase_time) ||
                fabsf(phase_time - 35.5f) > 0.0001f ||
                SudekiMpLanArenaClientActionPhaseTime(NULL, &phase_time) ||
                SudekiMpLanArenaClientActionPhaseTime(
                    &phased_action, NULL)) {
                fputs("FAIL: LAN client authoritative action phase policy\n",
                    stderr);
                ++failures;
            }
            phased_action.action_phase_valid = 0u;
            if (SudekiMpLanArenaClientActionPhaseTime(
                    &phased_action, &phase_time)) {
                fputs("FAIL: LAN client accepted missing action phase\n",
                    stderr);
                ++failures;
            }
            phased_action.action_phase_valid = 1u;
            phased_action.animation_state =
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
            if (SudekiMpLanArenaClientActionPhaseTime(
                    &phased_action, &phase_time)) {
                fputs("FAIL: LAN client accepted idle action phase\n",
                    stderr);
                ++failures;
            }
            phased_action.action_sequence = 9u;
            phased_action.action_phase_q8 = 0u;
            phased_action.action_retirement_valid = 1u;
            phased_action.action_terminal_phase_q8 = 49u * 256u;
            phased_action.idle_entry_phase_q8 = 2u * 256u + 64u;
            if (!SudekiMpLanArenaClientRetirementIdlePhaseTime(
                    &phased_action, &phase_time) ||
                fabsf(phase_time - 2.25f) > 0.0001f) {
                fputs("FAIL: LAN client action-retirement idle phase policy\n",
                    stderr);
                ++failures;
            }
            phased_action.action_retirement_valid = 0u;
            if (SudekiMpLanArenaClientRetirementIdlePhaseTime(
                    &phased_action, &phase_time)) {
                fputs("FAIL: LAN client accepted missing retirement phase\n",
                    stderr);
                ++failures;
            }
        }
        if (SudekiMpLanArenaClientNativeWeakHeld(0) ||
            !SudekiMpLanArenaClientNativeWeakHeld(1) ||
            !SudekiMpLanArenaClientNativeWeakHeld(2) ||
            SudekiMpLanArenaClientNativeWeakHeld(3) ||
            SudekiMpLanArenaClientNativeWeakHeld(-1) ||
            SudekiMpLanArenaClientNativeWeakHeld(4)) {
            fputs("FAIL: LAN client native weak transition policy mismatch\n",
                stderr);
            ++failures;
        }
        if (SudekiMpLanArenaClientSuppressedWeakNextState(0) != 0 ||
            SudekiMpLanArenaClientSuppressedWeakNextState(1) != 2 ||
            SudekiMpLanArenaClientSuppressedWeakNextState(2) != 2 ||
            SudekiMpLanArenaClientSuppressedWeakNextState(3) != 0 ||
            SudekiMpLanArenaClientSuppressedWeakNextState(-1) != 0 ||
            SudekiMpLanArenaClientSuppressedWeakNextState(4) != 0) {
            fputs("FAIL: LAN client suppressed weak state did not advance\n",
                stderr);
            ++failures;
        }
        if (SudekiMpLanArenaClientRangedWeakHeld(FALSE, FALSE) ||
            SudekiMpLanArenaClientRangedWeakHeld(FALSE, TRUE) ||
            SudekiMpLanArenaClientRangedWeakHeld(TRUE, FALSE) ||
            !SudekiMpLanArenaClientRangedWeakHeld(TRUE, TRUE)) {
            fputs("FAIL: LAN client ranged weak readiness gate mismatch\n",
                stderr);
            ++failures;
        }
    }
    /* The harness maps sections without applying PE base relocations.  Put the
     * exact animation methods used by the LAN replica at their mapped-image
     * addresses before asking its supported-image preflight to inspect them. */
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x40u) =
        image + RVA_ANIMATION_RENDERER_LOOKUP;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xf8u) =
        image + RVA_ANIMATION_RENDERER_COUNT;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xfcu) =
        image + RVA_ANIMATION_RENDERER_SELECTOR_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x100u) =
        image + RVA_ANIMATION_RENDERER_SELECTOR_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x104u) =
        image + RVA_ANIMATION_RENDERER_RATE_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x108u) =
        image + RVA_ANIMATION_RENDERER_RATE_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x10cu) =
        image + RVA_ANIMATION_RENDERER_TIME_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x110u) =
        image + RVA_ANIMATION_RENDERER_TIME_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x114u) =
        image + RVA_ANIMATION_RENDERER_STATE_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x118u) =
        image + RVA_ANIMATION_RENDERER_STATE_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x144u) =
        image + RVA_ANIMATION_RENDERER_BLEND_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x148u) =
        image + RVA_ANIMATION_RENDERER_BLEND_GET;
    if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
        fputs("FAIL: LAN client replica exact native seams rejected\n", stderr);
        ++failures;
    }
    SudekiMpResetLanArenaClientReplica();
    {
        void *saved_animation_lookup = *(void **)(
            image + RVA_ANIMATION_RENDERER_VTABLE + 0x40u);
        *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x40u) =
            image + RVA_INTERNAL_POSITION_SETTER;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched animation lookup\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x40u) =
            saved_animation_lookup;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client animation-lookup mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_position_setter_byte = image[RVA_INTERNAL_POSITION_SETTER];
        image[RVA_INTERNAL_POSITION_SETTER] ^= 0x01u;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched position setter\n", stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        image[RVA_INTERNAL_POSITION_SETTER] = saved_position_setter_byte;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica position-setter mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_world_matrix_byte = image[RVA_POSITION_WORLD_MATRIX];
        image[RVA_POSITION_WORLD_MATRIX] ^= 0x01u;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched position world-matrix getter\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        image[RVA_POSITION_WORLD_MATRIX] = saved_world_matrix_byte;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica world-matrix mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_call_displacement =
            image[RVA_POSITION_WORLD_MATRIX_UPDATE_CALL + 1u];
        image[RVA_POSITION_WORLD_MATRIX_UPDATE_CALL + 1u] ^= 0x01u;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched world-matrix updater call\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        image[RVA_POSITION_WORLD_MATRIX_UPDATE_CALL + 1u] =
            saved_call_displacement;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica updater-call mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_update_byte = image[RVA_POSITION_UPDATE];
        image[RVA_POSITION_UPDATE] ^= 0x01u;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched position updater\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        image[RVA_POSITION_UPDATE] = saved_update_byte;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica position-updater mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_combat_input_byte = image[RVA_ARBITER_COMBAT_INPUT];
        image[RVA_ARBITER_COMBAT_INPUT] ^= 0x01u;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched arbiter combat input\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        image[RVA_ARBITER_COMBAT_INPUT] = saved_combat_input_byte;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client arbiter-input mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_apply_damage_byte = image[RVA_APPLY_DAMAGE];
        image[RVA_APPLY_DAMAGE] ^= 0x01u;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted mismatched damage authority\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        image[RVA_APPLY_DAMAGE] = saved_apply_damage_byte;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client damage-authority mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        void *saved_animation_count = *(void **)(
            image + RVA_ANIMATION_RENDERER_VTABLE + 0xf8u);
        *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xf8u) =
            image + RVA_INTERNAL_POSITION_SETTER;
        if (SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica accepted a mismatched animation renderer\n",
                stderr);
            ++failures;
            SudekiMpResetLanArenaClientReplica();
        }
        *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xf8u) =
            saved_animation_count;
        if (!SudekiMpInitializeLanArenaClientReplica((HMODULE)image)) {
            fputs("FAIL: LAN client replica animation mismatch restore was sticky\n",
                stderr);
            ++failures;
        }
        SudekiMpResetLanArenaClientReplica();
    }
    {
        uint8_t saved_prefix = image[RVA_KILL_FOCUS_SHOW_COMMAND - 4u];
        uint32_t raw_focus_state_operand;
        uint32_t relocated_focus_state_operand = (uint32_t)(uintptr_t)(
            image + RVA_FOCUS_DEVICE_STATE_GLOBAL);
        uint32_t raw_show_window_operand;
        uint32_t relocated_show_window_operand = (uint32_t)(uintptr_t)(
            image + RVA_SHOW_WINDOW_IAT);
        memcpy(&raw_focus_state_operand,
            image + RVA_FOCUS_LOSS_BOOL_OPCODE + 4u,
            sizeof(raw_focus_state_operand));
        memcpy(image + RVA_FOCUS_LOSS_BOOL_OPCODE + 4u,
            &relocated_focus_state_operand,
            sizeof(relocated_focus_state_operand));
        memcpy(&raw_show_window_operand,
            image + RVA_KILL_FOCUS_SHOW_COMMAND + 4u,
            sizeof(raw_show_window_operand));
        memcpy(image + RVA_KILL_FOCUS_SHOW_COMMAND + 4u,
            &relocated_show_window_operand,
            sizeof(relocated_show_window_operand));
        if (!SudekiMpInstallLanArenaWindowPolicy((HMODULE)image)) {
            fputs("FAIL: LAN window policy exact focus-loss seam rejected\n",
                stderr);
            ++failures;
        } else {
            if (!SudekiMpLanArenaWindowPolicyInstalled() ||
                image[RVA_KILL_FOCUS_SHOW_COMMAND] != 8u ||
                image[RVA_WINDOW_ACTIVE_COMPARE_IMMEDIATE] != 0xffu ||
                image[RVA_WINDOW_ACTIVATE_APP_COMPARE_IMMEDIATE] != 0xffu ||
                image[RVA_FOCUS_LOSS_BOOL_OPCODE] != 0xb2u ||
                image[RVA_FOCUS_LOSS_BOOL_VALUE] != 0x01u ||
                image[RVA_FOCUS_LOSS_STATE_VALUE] != 0x01u) {
                fputs("FAIL: LAN window policy did not enable background presentation\n",
                    stderr);
                ++failures;
            }
            if (!SudekiMpUninstallLanArenaWindowPolicy() ||
                image[RVA_KILL_FOCUS_SHOW_COMMAND] != 6u ||
                image[RVA_WINDOW_ACTIVE_COMPARE_IMMEDIATE] != 0u ||
                image[RVA_WINDOW_ACTIVATE_APP_COMPARE_IMMEDIATE] != 0u ||
                image[RVA_FOCUS_LOSS_BOOL_OPCODE] != 0x32u ||
                image[RVA_FOCUS_LOSS_BOOL_VALUE] != 0xd2u ||
                image[RVA_FOCUS_LOSS_STATE_VALUE] != 0u) {
                fputs("FAIL: LAN window policy did not restore native focus policy\n",
                    stderr);
                ++failures;
            }
        }
        image[RVA_FOCUS_LOSS_STATE_VALUE] = 1u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaWindowPolicy((HMODULE)image)) {
            fputs("FAIL: LAN window policy accepted mismatched graphics focus context\n",
                stderr);
            ++failures;
            (void)SudekiMpUninstallLanArenaWindowPolicy();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   image[RVA_KILL_FOCUS_SHOW_COMMAND] != 6u) {
            fputs("FAIL: LAN graphics focus mismatch was not fail-closed\n",
                stderr);
            ++failures;
        }
        image[RVA_FOCUS_LOSS_STATE_VALUE] = 0u;
        image[RVA_WINDOW_ACTIVATE_APP_COMPARE_IMMEDIATE] = 1u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaWindowPolicy((HMODULE)image)) {
            fputs("FAIL: LAN window policy accepted mismatched WM_ACTIVATEAPP context\n",
                stderr);
            ++failures;
            (void)SudekiMpUninstallLanArenaWindowPolicy();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   image[RVA_KILL_FOCUS_SHOW_COMMAND] != 6u) {
            fputs("FAIL: LAN WM_ACTIVATEAPP mismatch was not fail-closed\n",
                stderr);
            ++failures;
        }
        image[RVA_WINDOW_ACTIVATE_APP_COMPARE_IMMEDIATE] = 0u;
        image[RVA_WINDOW_ACTIVE_COMPARE_IMMEDIATE] = 1u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaWindowPolicy((HMODULE)image)) {
            fputs("FAIL: LAN window policy accepted mismatched activation context\n",
                stderr);
            ++failures;
            (void)SudekiMpUninstallLanArenaWindowPolicy();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   image[RVA_KILL_FOCUS_SHOW_COMMAND] != 6u) {
            fputs("FAIL: LAN activation mismatch was not fail-closed\n", stderr);
            ++failures;
        }
        image[RVA_WINDOW_ACTIVE_COMPARE_IMMEDIATE] = 0u;
        image[RVA_KILL_FOCUS_SHOW_COMMAND - 4u] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaWindowPolicy((HMODULE)image)) {
            fputs("FAIL: LAN window policy accepted mismatched WndProc context\n",
                stderr);
            ++failures;
            (void)SudekiMpUninstallLanArenaWindowPolicy();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   image[RVA_KILL_FOCUS_SHOW_COMMAND] != 6u) {
            fputs("FAIL: LAN window mismatch was not fail-closed\n", stderr);
            ++failures;
        }
        image[RVA_KILL_FOCUS_SHOW_COMMAND - 4u] = saved_prefix;
        if (SudekiMpInstallLanArenaWindowPolicy((HMODULE)image)) {
            image[RVA_KILL_FOCUS_SHOW_COMMAND] = 9u;
            SetLastError(ERROR_SUCCESS);
            if (SudekiMpUninstallLanArenaWindowPolicy() ||
                GetLastError() != ERROR_BUSY ||
                !SudekiMpLanArenaWindowPolicyInstalled()) {
                fputs("FAIL: LAN window policy lost byte-patch ownership\n",
                    stderr);
                ++failures;
            }
            image[RVA_KILL_FOCUS_SHOW_COMMAND] = 8u;
            if (!SudekiMpUninstallLanArenaWindowPolicy()) {
                fputs("FAIL: LAN window policy could not retry exact restore\n",
                    stderr);
                ++failures;
            }
        } else {
            fputs("FAIL: LAN window policy retry install rejected\n", stderr);
            ++failures;
        }
        memcpy(image + RVA_KILL_FOCUS_SHOW_COMMAND + 4u,
            &raw_show_window_operand, sizeof(raw_show_window_operand));
        memcpy(image + RVA_FOCUS_LOSS_BOOL_OPCODE + 4u,
            &raw_focus_state_operand, sizeof(raw_focus_state_operand));
    }
    /* Simulate the loader relocation for the native QuickMenu input vtable
     * before exercising the LAN client's read-only browsing hook. */
    raw_lan_client_quick_menu_input =
        *(uint32_t *)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT);
    raw_lan_client_camera_input =
        *(uint32_t *)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT);
    raw_lan_client_character_input =
        *(uint32_t *)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT);
    *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_INPUT;
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_CAMERA_INPUT_EVENT;
    *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) =
        image + RVA_CHARACTER_INPUT_HANDLER;
    if (!SudekiMpInstallLanArenaClientInput((HMODULE)image)) {
        fputs("FAIL: LAN client input exact seams rejected\n", stderr);
        ++failures;
    } else {
        HANDLE operator_event = OpenEventW(
            SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_CLIENT_COMBAT_TOGGLE_EVENT);
        HANDLE operator_weak_event = OpenEventW(
            SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT);
        HANDLE operator_weak_hold_event = OpenEventW(
            SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT);
        HANDLE operator_camera_left_event = OpenEventW(
            SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_LEFT_EVENT);
        HANDLE operator_camera_right_event = OpenEventW(
            SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_RIGHT_EVENT);
        if (relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
            *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) ==
                image + RVA_QUICK_MENU_INPUT ||
            *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) ==
                image + RVA_CAMERA_INPUT_EVENT ||
            *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) ==
                image + RVA_CHARACTER_INPUT_HANDLER ||
            relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) ==
                image + RVA_ARBITER_MOVEMENT ||
            relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) ==
                image + RVA_ARBITER_MOVEMENT) {
            fputs("FAIL: LAN client input hooks were not installed\n", stderr);
            ++failures;
        }
        if (operator_event == NULL || !SetEvent(operator_event) ||
            operator_weak_event == NULL || !SetEvent(operator_weak_event) ||
            operator_weak_hold_event == NULL ||
                !SetEvent(operator_weak_hold_event) ||
                !ResetEvent(operator_weak_hold_event) ||
            operator_camera_left_event == NULL ||
                !SetEvent(operator_camera_left_event) ||
            operator_camera_right_event == NULL ||
                !SetEvent(operator_camera_right_event)) {
            fputs("FAIL: LAN client local operator endpoint is unavailable\n",
                stderr);
            ++failures;
        }
        if (operator_event != NULL) CloseHandle(operator_event);
        if (operator_weak_event != NULL) CloseHandle(operator_weak_event);
        if (operator_weak_hold_event != NULL) {
            CloseHandle(operator_weak_hold_event);
        }
        if (operator_camera_left_event != NULL) {
            CloseHandle(operator_camera_left_event);
        }
        if (operator_camera_right_event != NULL) {
            CloseHandle(operator_camera_right_event);
        }
        SudekiMpUninstallLanArenaClientInput();
        if (relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
            *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
                image + RVA_QUICK_MENU_INPUT ||
            *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
                image + RVA_CAMERA_INPUT_EVENT ||
            *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
                image + RVA_CHARACTER_INPUT_HANDLER ||
            relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
                image + RVA_ARBITER_MOVEMENT ||
            relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) !=
                image + RVA_ARBITER_MOVEMENT) {
            fputs("FAIL: LAN client input hooks were not restored\n", stderr);
            ++failures;
        }
    }
    {
        uint8_t saved_quick_menu_entry = image[RVA_QUICK_MENU_INPUT];
        image[RVA_QUICK_MENU_INPUT] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaClientInput((HMODULE)image)) {
            fputs("FAIL: LAN client input accepted a mismatched QuickMenu entry\n",
                stderr);
            ++failures;
            SudekiMpUninstallLanArenaClientInput();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: LAN QuickMenu mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
                image + RVA_ARBITER_MOVEMENT ||
            relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
            *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
                image + RVA_QUICK_MENU_INPUT ||
            *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
                image + RVA_CHARACTER_INPUT_HANDLER) {
            fputs("FAIL: LAN QuickMenu mismatch mutated input hooks\n", stderr);
            ++failures;
        }
        image[RVA_QUICK_MENU_INPUT] = saved_quick_menu_entry;
    }
    {
        uint8_t saved_aim_update_entry = image[RVA_CONTROLLER_AIM_UPDATE];
        image[RVA_CONTROLLER_AIM_UPDATE] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaClientInput((HMODULE)image)) {
            fputs("FAIL: LAN client input accepted a mismatched first-person aim updater\n",
                stderr);
            ++failures;
            SudekiMpUninstallLanArenaClientInput();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: LAN first-person aim mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
                image + RVA_ARBITER_MOVEMENT ||
            relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
            *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
                image + RVA_QUICK_MENU_INPUT ||
            *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
                image + RVA_CAMERA_INPUT_EVENT ||
            *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
                image + RVA_CHARACTER_INPUT_HANDLER) {
            fputs("FAIL: LAN first-person aim mismatch mutated input hooks\n",
                stderr);
            ++failures;
        }
        image[RVA_CONTROLLER_AIM_UPDATE] = saved_aim_update_entry;
        if (!SudekiMpInstallLanArenaClientInput((HMODULE)image)) {
            fputs("FAIL: LAN client input could not reinstall after first-person aim mismatch\n",
                stderr);
            ++failures;
        } else {
            SudekiMpUninstallLanArenaClientInput();
        }
    }
    *(uint32_t *)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) =
        raw_lan_client_quick_menu_input;
    *(uint32_t *)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        raw_lan_client_camera_input;
    *(uint32_t *)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) =
        raw_lan_client_character_input;
    {
        uint8_t controller_entry[12];
        memcpy(controller_entry, image + RVA_CONTROLLER_COMBAT,
            sizeof(controller_entry));
        if (!SudekiMpInstallLanArenaHostInput((HMODULE)image)) {
            fputs("FAIL: LAN host input exact combat seam rejected\n", stderr);
            ++failures;
        } else {
            HANDLE weak_event = OpenEventW(
                SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT);
            HANDLE strong_event = OpenEventW(
                SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                SUDEKIMP_LAN_ARENA_HOST_STRONG_ATTACK_EVENT);
            HANDLE sweep_event = OpenEventW(
                SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                SUDEKIMP_LAN_ARENA_HOST_SWEEP_ATTACK_EVENT);
            HANDLE block_event = OpenEventW(
                SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                SUDEKIMP_LAN_ARENA_HOST_BLOCK_EVENT);
            HANDLE action_ack_event = OpenEventW(
                SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                SUDEKIMP_LAN_ARENA_HOST_ACTION_ACK_EVENT);
            if (memcmp(image + RVA_CONTROLLER_COMBAT, controller_entry,
                    sizeof(controller_entry)) == 0 ||
                relative_call_target(
                    image + RVA_PLAYER_MOVE_CALL_ALTERNATE) ==
                    image + RVA_ARBITER_MOVEMENT ||
                relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) ==
                    image + RVA_ARBITER_MOVEMENT) {
                fputs("FAIL: LAN host input observer was not installed\n", stderr);
                ++failures;
            }
            if (weak_event == NULL || !SetEvent(weak_event) ||
                strong_event == NULL || !SetEvent(strong_event) ||
                sweep_event == NULL || !SetEvent(sweep_event) ||
                block_event == NULL || !SetEvent(block_event) ||
                action_ack_event == NULL) {
                fputs("FAIL: LAN host local operator endpoint is unavailable\n",
                    stderr);
                ++failures;
            }
            if (weak_event != NULL) CloseHandle(weak_event);
            if (strong_event != NULL) CloseHandle(strong_event);
            if (sweep_event != NULL) CloseHandle(sweep_event);
            if (block_event != NULL) CloseHandle(block_event);
            if (action_ack_event != NULL) CloseHandle(action_ack_event);
            SudekiMpUninstallLanArenaHostInput();
            if (memcmp(image + RVA_CONTROLLER_COMBAT, controller_entry,
                    sizeof(controller_entry)) != 0 ||
                relative_call_target(
                    image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
                    image + RVA_ARBITER_MOVEMENT ||
                relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) !=
                    image + RVA_ARBITER_MOVEMENT) {
                fputs("FAIL: LAN host input observer was not restored\n", stderr);
                ++failures;
            }
        }
    }
    {
        uint8_t raw_save_entry[5];
        uint8_t raw_slot_entry[7];
        uint8_t raw_previous_character =
            image[RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER];
        uint8_t raw_next_character =
            image[RVA_GROUP_PLAYERS_NEXT_CHARACTER];
        uint8_t relocated_save_entry[5];
        uint8_t relocated_slot_entry[7];
        memcpy(raw_save_entry, image + RVA_SAVE_MENU_SHOW,
            sizeof(raw_save_entry));
        memcpy(raw_slot_entry, image + RVA_LOAD_GAME_SAVE,
            sizeof(raw_slot_entry));
        *(uint32_t *)(image + RVA_SAVE_MENU_SHOW + 1u) =
            (uint32_t)(uintptr_t)(image + RVA_WORLD_SCENE_GLOBAL);
        *(uint32_t *)(image + RVA_LOAD_GAME_SAVE + 2u) =
            (uint32_t)(uintptr_t)(image + RVA_LOADING_FLAG);
        memcpy(relocated_save_entry, image + RVA_SAVE_MENU_SHOW,
            sizeof(relocated_save_entry));
        memcpy(relocated_slot_entry, image + RVA_LOAD_GAME_SAVE,
            sizeof(relocated_slot_entry));
        if (!SudekiMpInstallLanArenaCampaignGuard((HMODULE)image)) {
            fputs("FAIL: LAN campaign guard exact seams rejected\n", stderr);
            ++failures;
        } else {
            if (memcmp(image + RVA_SAVE_MENU_SHOW, relocated_save_entry,
                    sizeof(relocated_save_entry)) == 0 ||
                memcmp(image + RVA_LOAD_GAME_SAVE, relocated_slot_entry,
                    sizeof(relocated_slot_entry)) == 0 ||
                image[RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER] != 0xc3u ||
                image[RVA_GROUP_PLAYERS_NEXT_CHARACTER] != 0xc3u) {
                fputs("FAIL: LAN campaign guard hooks were not installed\n",
                    stderr);
                ++failures;
            }
            SudekiMpUninstallLanArenaCampaignGuard();
            if (memcmp(image + RVA_SAVE_MENU_SHOW, relocated_save_entry,
                    sizeof(relocated_save_entry)) != 0 ||
                memcmp(image + RVA_LOAD_GAME_SAVE, relocated_slot_entry,
                    sizeof(relocated_slot_entry)) != 0 ||
                image[RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER] !=
                    raw_previous_character ||
                image[RVA_GROUP_PLAYERS_NEXT_CHARACTER] !=
                    raw_next_character) {
                fputs("FAIL: LAN campaign guard hooks were not restored\n",
                    stderr);
                ++failures;
            }
        }
        image[RVA_LOAD_GAME_SAVE] ^= 0x01u;
        if (SudekiMpInstallLanArenaCampaignGuard((HMODULE)image)) {
            fputs("FAIL: LAN campaign guard accepted mismatched slot seam\n",
                stderr);
            ++failures;
            SudekiMpUninstallLanArenaCampaignGuard();
        }
        if (memcmp(image + RVA_SAVE_MENU_SHOW, relocated_save_entry,
                sizeof(relocated_save_entry)) != 0 ||
            image[RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER] !=
                raw_previous_character ||
            image[RVA_GROUP_PLAYERS_NEXT_CHARACTER] !=
                raw_next_character) {
            fputs("FAIL: LAN campaign mismatch mutated save-menu seam\n",
                stderr);
            ++failures;
        }
        memcpy(image + RVA_SAVE_MENU_SHOW, raw_save_entry,
            sizeof(raw_save_entry));
        memcpy(image + RVA_LOAD_GAME_SAVE, raw_slot_entry,
            sizeof(raw_slot_entry));
    }
    raw_lan_pause_quit_show_global =
        *(uint32_t *)(image + RVA_PC_QUIT_SCREEN_SHOW + 3u);
    *(uint32_t *)(image + RVA_PC_QUIT_SCREEN_SHOW + 3u) =
        (uint32_t)(uintptr_t)(image + RVA_PC_QUIT_SCREEN_GLOBAL);
    if (!SudekiMpInstallLanArenaPausePanel((HMODULE)image)) {
        fputs("FAIL: LAN pause panel exact seams rejected\n", stderr);
        ++failures;
    } else {
        if (SudekiMpLanArenaPausePanelActive()) {
            fputs("FAIL: LAN pause panel began active\n", stderr);
            ++failures;
        }
        if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) ==
                image + RVA_PC_QUIT_SCREEN_RENDER ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_SELECT_CALL) ==
                image + RVA_PC_QUIT_SCREEN_SELECT ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_BACK_CALL) ==
                image + RVA_PC_QUIT_SCREEN_BACK ||
            relative_call_target(
                image + RVA_PC_QUIT_SCREEN_ANALOG_NAVIGATE_CALL) ==
                image + RVA_PC_QUIT_SCREEN_NAVIGATE ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_NAVIGATE_CALL) ==
                image + RVA_PC_QUIT_SCREEN_NAVIGATE) {
            fputs("FAIL: LAN pause-panel interaction hooks were not installed\n",
                stderr);
            ++failures;
        }
        SudekiMpUninstallLanArenaPausePanel();
        if (SudekiMpLanArenaPausePanelActive()) {
            fputs("FAIL: LAN pause panel remained active after uninstall\n",
                stderr);
            ++failures;
        }
        if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) !=
                image + RVA_PC_QUIT_SCREEN_RENDER ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_SELECT_CALL) !=
                image + RVA_PC_QUIT_SCREEN_SELECT ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_BACK_CALL) !=
                image + RVA_PC_QUIT_SCREEN_BACK ||
            relative_call_target(
                image + RVA_PC_QUIT_SCREEN_ANALOG_NAVIGATE_CALL) !=
                image + RVA_PC_QUIT_SCREEN_NAVIGATE ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_NAVIGATE_CALL) !=
                image + RVA_PC_QUIT_SCREEN_NAVIGATE) {
            fputs("FAIL: LAN pause-panel interaction hooks were not restored\n",
                stderr);
            ++failures;
        }
    }
    {
        uint8_t saved_show_entry = image[RVA_PC_QUIT_SCREEN_SHOW];
        image[RVA_PC_QUIT_SCREEN_SHOW] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaPausePanel((HMODULE)image)) {
            fputs("FAIL: LAN pause panel accepted a mismatched show seam\n",
                stderr);
            ++failures;
            SudekiMpUninstallLanArenaPausePanel();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: LAN pause show mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        image[RVA_PC_QUIT_SCREEN_SHOW] = saved_show_entry;
        if (!SudekiMpInstallLanArenaPausePanel((HMODULE)image)) {
            fputs("FAIL: LAN pause panel could not reinstall after show mismatch\n",
                stderr);
            ++failures;
        } else {
            SudekiMpUninstallLanArenaPausePanel();
        }
    }
    {
        uint8_t saved_select_displacement =
            image[RVA_PC_QUIT_SCREEN_SELECT_CALL + 1u];
        image[RVA_PC_QUIT_SCREEN_SELECT_CALL + 1u] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallLanArenaPausePanel((HMODULE)image)) {
            fputs("FAIL: LAN pause panel accepted a mismatched select seam\n",
                stderr);
            ++failures;
            SudekiMpUninstallLanArenaPausePanel();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: LAN pause select mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) !=
                image + RVA_PC_QUIT_SCREEN_RENDER ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_BACK_CALL) !=
                image + RVA_PC_QUIT_SCREEN_BACK ||
            relative_call_target(
                image + RVA_PC_QUIT_SCREEN_ANALOG_NAVIGATE_CALL) !=
                image + RVA_PC_QUIT_SCREEN_NAVIGATE ||
            relative_call_target(image + RVA_PC_QUIT_SCREEN_NAVIGATE_CALL) !=
                image + RVA_PC_QUIT_SCREEN_NAVIGATE) {
            fputs("FAIL: LAN pause mismatch rollback left hooks installed\n",
                stderr);
            ++failures;
        }
        image[RVA_PC_QUIT_SCREEN_SELECT_CALL + 1u] =
            saved_select_displacement;
    }
    *(uint32_t *)(image + RVA_PC_QUIT_SCREEN_SHOW + 3u) =
        raw_lan_pause_quit_show_global;
    memcpy(
        minimap_snapshot_call_original,
        image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
        sizeof(minimap_snapshot_call_original)
    );
    check_save_book_intercept_exact_image(image, &failures);
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: uninstalled shared modal inspector quiesced gameplay\n",
            stderr);
        ++failures;
    }

    {
        static const uint8_t message_pump_accelerator_call[] = {
            0x6a, 0x65,
            0x6a, 0x00,
            0xff, 0x15, 0xec, 0xa0, 0x69, 0x00,
            0x50,
            0xff, 0x15, 0xf0, 0xa1, 0x69, 0x00
        };
        if (memcmp(
                image + SUDEKIMP_MESSAGE_PUMP_ACCELERATOR_CALL_RVA,
                message_pump_accelerator_call,
                sizeof(message_pump_accelerator_call)) != 0 ||
            *(const uint32_t *)(
                image + SUDEKIMP_LOAD_ACCELERATORS_IAT_RVA) !=
                SUDEKIMP_LOAD_ACCELERATORS_IMPORT_NAME_RVA) {
            fputs("FAIL: exact accelerator-cache import seam mismatch\n",
                stderr);
            ++failures;
        }
    }

    if (*(const uint32_t *)(image + SUDEKIMP_XINPUT_GET_STATE_IAT_RVA) !=
        SUDEKIMP_XINPUT_GET_STATE_IMPORT_NAME_RVA) {
        fputs("FAIL: exact XInputGetState import seam mismatch\n", stderr);
        ++failures;
    }

    if (SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            FALSE, TRUE, FALSE, FALSE) ||
        SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, FALSE, FALSE, FALSE) ||
        !SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, TRUE, FALSE, FALSE) ||
        !SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: Quick Menu live-view acceptance truth table mismatch\n",
            stderr);
        ++failures;
    }

    if (SudekiMpSplitScreenClassifySharedInteractionModal(
            FALSE, FALSE, FALSE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_NONE ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, TRUE, FALSE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, TRUE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 7u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 8u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 9u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 9u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 5u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 6u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, TRUE, TRUE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN) {
        fputs("FAIL: shared Shop/Blacksmith full-width policy mismatch\n",
            stderr);
        ++failures;
    }
    if (SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, FALSE, 0u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, FALSE, FALSE, 0u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, TRUE, 0u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, FALSE, 5u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, FALSE, 0u, 6u)) {
        fputs("FAIL: mod-owned Blacksmith native-layer exclusion policy mismatch\n",
            stderr);
        ++failures;
    }
    if (SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            FALSE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN,
            TRUE) ||
        !SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            TRUE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN,
            FALSE) ||
        SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            TRUE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_NONE,
            FALSE) ||
        !SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            TRUE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_NONE,
            TRUE)) {
        fputs("FAIL: installed/uninstalled shared modal quiesce policy mismatch\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
            FALSE, 1u, 2u) ||
        SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
            FALSE, 2u, 2u) ||
        SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
            TRUE, 1u, 2u) ||
        SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
            FALSE, 1u, 16u)) {
        fputs("FAIL: viewport portrait enum-edge/modal suspension policy mismatch\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: shared modal live split recovery eligibility mismatch\n",
            stderr);
        ++failures;
    }
    shared_modal_recovery_pending =
        SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            FALSE, TRUE, TRUE, TRUE, FALSE);
    if (!shared_modal_recovery_pending) {
        fputs("FAIL: eligible modal close did not arm cache recovery\n",
            stderr);
        ++failures;
    }
    shared_modal_recovery_pending =
        SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            shared_modal_recovery_pending,
            FALSE,
            FALSE,
            FALSE,
            FALSE
        );
    if (shared_modal_recovery_pending) {
        fputs("FAIL: Player 2 dropout did not cancel cache recovery\n",
            stderr);
        ++failures;
    }
    shared_modal_recovery_pending =
        SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            shared_modal_recovery_pending,
            FALSE,
            FALSE,
            TRUE,
            FALSE
        );
    if (shared_modal_recovery_pending) {
        fputs("FAIL: Player 2 rejoin rearmed a cancelled modal recovery\n",
            stderr);
        ++failures;
    }
    if (SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            TRUE, FALSE, FALSE, TRUE, TRUE)) {
        fputs("FAIL: fresh split cache pair did not complete modal recovery\n",
            stderr);
        ++failures;
    }

    if (SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            FALSE, TRUE, TRUE) ||
        SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            TRUE, FALSE, TRUE) ||
        SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            TRUE, TRUE, FALSE) ||
        !SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            TRUE, TRUE, TRUE)) {
        fputs("FAIL: minimap update-owner latch policy mismatch\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenMinimapCaptureAllowed(
            FALSE, FALSE, TRUE, FALSE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, FALSE, FALSE, FALSE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, FALSE, FALSE, TRUE) ||
        !SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, FALSE, FALSE) ||
        !SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, FALSE, TRUE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: minimap cache owner-match policy mismatch\n", stderr);
        ++failures;
    }

    {
        unsigned short native_generation_baseline = 7u;
        void *native_camera_entity = &native_generation_baseline;
        unsigned int native_actor_generation = 17u;
        int native_player_one_camera_marker = 0;
        int native_player_two_camera_marker = 0;
        int native_other_camera_marker = 0;
        int native_player_one_state_marker = 0;
        int native_player_two_state_marker = 0;
        int native_other_state_marker = 0;
        void *native_camera_party_slot[3] = {
            native_camera_entity,
            NULL,
            NULL
        };
        float native_matrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            4.0f, 5.0f, 6.0f, 1.0f
        };

        if (SudekiMpSplitScreenObserveNativeCameraGeneration(
                &native_generation_baseline, 8u, TRUE) ||
            native_generation_baseline != 8u ||
            SudekiMpSplitScreenObserveNativeCameraGeneration(
                &native_generation_baseline, 8u, FALSE) ||
            !SudekiMpSplitScreenObserveNativeCameraGeneration(
                &native_generation_baseline, 9u, FALSE)) {
            fputs("FAIL: manual P2 camera generation write falsely proved native readiness\n",
                stderr);
            ++failures;
        }
        if (SudekiMpSplitScreenNativeCameraActorFromPartySlot(NULL) != NULL ||
            SudekiMpSplitScreenNativeCameraActorFromPartySlot(
                native_camera_party_slot) != native_camera_entity ||
            SudekiMpSplitScreenNativeCameraActorFromPartySlot(
                native_camera_party_slot) == native_camera_party_slot) {
            fputs("FAIL: native P2 camera actor provenance did not resolve the party TPtr entity\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenNativeCameraWrapperPolicy(
                &native_player_two_camera_marker,
                native_camera_party_slot,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation,
                TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperPolicy(
                native_camera_party_slot,
                native_camera_party_slot,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation,
                TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperPolicy(
                native_camera_entity,
                native_camera_party_slot,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation,
                TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperPolicy(
                &native_player_two_camera_marker,
                native_camera_party_slot,
                native_camera_entity,
                native_camera_entity,
                &native_other_camera_marker,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation,
                TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperPolicy(
                &native_player_two_camera_marker,
                native_camera_party_slot,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation,
                FALSE)) {
            fputs("FAIL: native P2 camera wrapper gate accepted a party slot, raw actor, or unproven GELGroupPtr\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
                FALSE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
                TRUE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
                FALSE, FALSE, TRUE) ||
            SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
                FALSE, TRUE, FALSE)) {
            fputs("FAIL: native P2 camera wrapper one-shot gate allowed reuse or an unstable live precondition\n",
                stderr);
            ++failures;
        }
        if (SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_INACTIVE ||
            SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE, TRUE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE ||
            SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_PENDING_STATE ||
            SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED, TRUE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE ||
            SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED, TRUE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_CHECK_STATE ||
            SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE ||
            SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
                99u, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE) {
            fputs("FAIL: native P2 camera readiness stage gate revoked the intentional target-to-state delay or accepted an invalid stage/bound pair\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                TRUE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                FALSE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                TRUE, FALSE, TRUE, TRUE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                TRUE, TRUE, FALSE, TRUE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                TRUE, TRUE, TRUE, FALSE, TRUE, TRUE) ||
            SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                TRUE, TRUE, TRUE, TRUE, FALSE, TRUE) ||
            SudekiMpSplitScreenNativeCameraOwnershipPolicy(
                TRUE, TRUE, TRUE, TRUE, TRUE, FALSE)) {
            fputs("FAIL: native P2 camera ownership gate was not fail closed\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenNativeCameraIdentityPolicy(
                native_camera_party_slot,
                native_camera_party_slot,
                native_camera_entity,
                native_camera_entity,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation) ||
            SudekiMpSplitScreenNativeCameraIdentityPolicy(
                NULL, native_camera_party_slot,
                native_camera_entity, native_camera_entity,
                native_camera_entity,
                native_actor_generation, native_actor_generation) ||
            SudekiMpSplitScreenNativeCameraIdentityPolicy(
                native_camera_party_slot,
                (uint8_t *)native_camera_party_slot + sizeof(void *),
                native_camera_entity, native_camera_entity,
                native_camera_entity,
                native_actor_generation, native_actor_generation) ||
            SudekiMpSplitScreenNativeCameraIdentityPolicy(
                native_camera_party_slot, native_camera_party_slot,
                native_camera_entity, native_camera_entity,
                native_camera_party_slot,
                native_actor_generation, native_actor_generation) ||
            SudekiMpSplitScreenNativeCameraIdentityPolicy(
                native_camera_party_slot, native_camera_party_slot,
                native_camera_entity, native_camera_entity,
                native_camera_entity,
                0u, native_actor_generation) ||
            SudekiMpSplitScreenNativeCameraIdentityPolicy(
                native_camera_party_slot, native_camera_party_slot,
                native_camera_entity, native_camera_entity,
                native_camera_entity,
                native_actor_generation,
                native_actor_generation + 1u)) {
            fputs("FAIL: native P2 camera actor-generation identity gate mismatch\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenNativeCameraMatrixPolicy(native_matrix)) {
            fputs("FAIL: valid affine native P2 camera matrix was rejected\n",
                stderr);
            ++failures;
        }
        if (SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_REMOVE ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                FALSE, FALSE, NULL,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE, NULL,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_other_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, FALSE, NULL,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                FALSE, NULL, NULL,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                FALSE, NULL,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_two_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_two_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_two_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_other_camera_marker,
                &native_other_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_other_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                TRUE, TRUE,
                &native_player_two_camera_marker,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                FALSE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                FALSE, FALSE, NULL, NULL,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                &native_player_two_state_marker,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT ||
            SudekiMpSplitScreenNativeCameraReleasePolicy(
                FALSE, FALSE, NULL,
                &native_player_two_camera_marker,
                TRUE,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                &native_player_one_camera_marker,
                &native_player_one_state_marker,
                TRUE,
                &native_player_one_state_marker,
                NULL,
                TRUE) != SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT) {
            fputs("FAIL: native P2 camera release ownership policy mismatch\n",
                stderr);
            ++failures;
        }
        native_matrix[15] = 0.0f;
        if (SudekiMpSplitScreenNativeCameraMatrixPolicy(native_matrix)) {
            fputs("FAIL: non-affine native P2 camera matrix was accepted\n",
                stderr);
            ++failures;
        }
        native_matrix[15] = 1.0f;
        native_matrix[4] = 1.0f;
        native_matrix[5] = 0.0f;
        if (SudekiMpSplitScreenNativeCameraMatrixPolicy(native_matrix)) {
            fputs("FAIL: degenerate native P2 camera basis was accepted\n",
                stderr);
            ++failures;
        }
        native_matrix[0] = 0.0f;
        native_matrix[4] = 0.0f;
        if (SudekiMpSplitScreenNativeCameraMatrixPolicy(native_matrix)) {
            fputs("FAIL: zero native P2 camera basis was accepted\n",
                stderr);
            ++failures;
        }
        native_matrix[0] = FLT_MAX;
        native_matrix[1] = 0.0f;
        native_matrix[2] = 0.0f;
        native_matrix[4] = 0.0f;
        native_matrix[5] = 1.0f;
        native_matrix[6] = 0.0f;
        native_matrix[8] = 0.0f;
        native_matrix[9] = 0.0f;
        native_matrix[10] = 1.0f;
        if (SudekiMpSplitScreenNativeCameraMatrixPolicy(native_matrix)) {
            fputs("FAIL: finite native P2 matrix with derived overflow was accepted\n",
                stderr);
            ++failures;
        }
        if (SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                FALSE, SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE,
                TRUE, TRUE, TRUE, FALSE, FALSE, FALSE,
                TRUE, FALSE, FALSE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_FALLBACK ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE,
                TRUE, TRUE, FALSE, FALSE, FALSE, FALSE,
                TRUE, FALSE, FALSE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_FALLBACK ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE,
                TRUE, TRUE, TRUE, FALSE, FALSE, FALSE,
                TRUE, FALSE, FALSE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_SET_TARGET ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED,
                TRUE, TRUE, TRUE, TRUE, FALSE, FALSE,
                TRUE, FALSE, FALSE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED,
                TRUE, TRUE, TRUE, TRUE, TRUE, FALSE,
                TRUE, FALSE, FALSE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_SET_STATE ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED,
                TRUE, TRUE, TRUE, FALSE, TRUE, FALSE,
                TRUE, FALSE, FALSE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED,
                TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                TRUE, FALSE, TRUE, TRUE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED,
                TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                TRUE, TRUE, TRUE, TRUE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_READY ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED,
                TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                TRUE, TRUE, TRUE, TRUE, TRUE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED,
                TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                TRUE, TRUE, TRUE, FALSE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE ||
            SudekiMpSplitScreenNativeCameraBootstrapPolicy(
                TRUE, SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED,
                TRUE, TRUE, FALSE, TRUE, TRUE, TRUE,
                TRUE, TRUE, TRUE, TRUE, FALSE) !=
                    SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE) {
            fputs("FAIL: staged native P2 Exploration bootstrap policy mismatch\n",
                stderr);
            ++failures;
        }
    }

    if (SudekiMpSplitScreenTemporaryCameraPolicy(
            FALSE, -1, FALSE) != SUDEKIMP_TEMP_CAMERA_OUTSIDE ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, -1, FALSE) !=
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, 1, TRUE) !=
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, 0, FALSE) !=
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, 0, TRUE) !=
                SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO) {
        fputs("FAIL: settled TEMP camera policy did not fail closed outside proven native Exploration\n",
            stderr);
        ++failures;
    }

    {
        BOOL perspective_gates[8] = {
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE
        };
        unsigned int gate_index;

        if (!SudekiMpSplitScreenPlayerTwoPerspectivePolicy(
                perspective_gates[0], perspective_gates[1],
                perspective_gates[2], perspective_gates[3],
                perspective_gates[4], perspective_gates[5],
                perspective_gates[6], perspective_gates[7])) {
            fputs("FAIL: complete P2 perspective gate set was rejected\n",
                stderr);
            ++failures;
        }
        for (gate_index = 0u; gate_index < 8u; ++gate_index) {
            perspective_gates[gate_index] = FALSE;
            if (SudekiMpSplitScreenPlayerTwoPerspectivePolicy(
                    perspective_gates[0], perspective_gates[1],
                    perspective_gates[2], perspective_gates[3],
                    perspective_gates[4], perspective_gates[5],
                    perspective_gates[6], perspective_gates[7])) {
                fputs("FAIL: P2 perspective policy did not fail closed\n",
                    stderr);
                ++failures;
            }
            perspective_gates[gate_index] = TRUE;
        }
    }

    {
        const void *player_one = (const void *)(uintptr_t)0x11110000u;
        const void *player_two = (const void *)(uintptr_t)0x22220000u;
        const void *player_three = (const void *)(uintptr_t)0x33330000u;

        if (!SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_two, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_two, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                0u, 0u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 1u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 0u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 1u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 1u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 1u)) {
            fputs("FAIL: co-op roster lead-readiness truth table mismatch\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenRosterLockHealthy(
                player_one, player_one, player_one,
                player_two, player_two) ||
            SudekiMpSplitScreenRosterLockHealthy(
                player_two, player_one, player_one,
                player_two, player_two) ||
            SudekiMpSplitScreenRosterLockHealthy(
                player_one, player_two, player_one,
                player_two, player_two) ||
            SudekiMpSplitScreenRosterLockHealthy(
                player_one, player_one, player_one,
                player_one, player_two)) {
            fputs("FAIL: co-op roster lock-health truth table mismatch\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenRosterThreeSeatLockHealthy(
                player_one, player_one, player_one,
                player_two, player_two,
                player_three, player_three) ||
            SudekiMpSplitScreenRosterThreeSeatLockHealthy(
                player_one, player_one, player_one,
                player_two, player_two,
                player_two, player_three) ||
            SudekiMpSplitScreenRosterThreeSeatLockHealthy(
                player_one, player_one, player_one,
                player_two, player_two,
                player_three, player_two) ||
            SudekiMpSplitScreenRosterThreeSeatLockHealthy(
                player_one, player_one, player_one,
                player_two, player_two,
                player_three, player_one)) {
            fputs("FAIL: three-seat roster lock-health truth table mismatch\n",
                stderr);
            ++failures;
        }
    }

    {
        unsigned int isolation;
        unsigned int owner_valid;
        unsigned int owner_player_two;
        unsigned int fallback_player_two;
        unsigned int render_phase_confirmed;
        unsigned int rendered_player_two;
        unsigned int capture_allowed;
        unsigned int compose_succeeded;
        unsigned int state;
        unsigned int submit_seen;
        unsigned int owner_frame_captured;
        BOOL pin_policy_mismatch = FALSE;
        BOOL submit_policy_mismatch = FALSE;
        BOOL capture_policy_mismatch = FALSE;
        BOOL tail_policy_mismatch = FALSE;

        for (isolation = 0u; isolation <= 1u; ++isolation) {
            for (owner_valid = 0u; owner_valid <= 1u; ++owner_valid) {
                for (owner_player_two = 0u;
                     owner_player_two <= 1u;
                     ++owner_player_two) {
                    for (fallback_player_two = 0u;
                         fallback_player_two <= 1u;
                         ++fallback_player_two) {
                        BOOL expected = isolation && owner_valid ?
                            owner_player_two != 0u :
                            fallback_player_two != 0u;
                        BOOL actual =
                            SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
                                isolation != 0u,
                                owner_valid != 0u,
                                owner_player_two != 0u,
                                fallback_player_two != 0u
                            );
                        if ((actual != FALSE) != (expected != FALSE)) {
                            pin_policy_mismatch = TRUE;
                        }
                    }
                }
            }
        }
        if (pin_policy_mismatch) {
            fputs("FAIL: Quick Menu owner-pinned viewport truth table mismatch\n",
                stderr);
            ++failures;
        }

        for (isolation = 0u; isolation <= 1u; ++isolation) {
            for (render_phase_confirmed = 0u;
                 render_phase_confirmed <= 1u;
                 ++render_phase_confirmed) {
                for (owner_valid = 0u;
                     owner_valid <= 1u;
                     ++owner_valid) {
                    for (owner_player_two = 0u;
                         owner_player_two <= 1u;
                         ++owner_player_two) {
                        for (rendered_player_two = 0u;
                             rendered_player_two <= 1u;
                             ++rendered_player_two) {
                            BOOL expected = isolation &&
                                render_phase_confirmed && owner_valid &&
                                rendered_player_two != owner_player_two;
                            BOOL actual =
                                SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
                                    isolation != 0u,
                                    render_phase_confirmed != 0u,
                                    owner_valid != 0u,
                                    owner_player_two != 0u,
                                    rendered_player_two != 0u
                                );
                            if ((actual != FALSE) != (expected != FALSE)) {
                                submit_policy_mismatch = TRUE;
                            }
                        }
                    }
                }
            }
        }
        if (submit_policy_mismatch) {
            fputs("FAIL: Quick Menu owner-only submit truth table mismatch\n",
                stderr);
            ++failures;
        }

        for (isolation = 0u; isolation <= 1u; ++isolation) {
            for (owner_valid = 0u; owner_valid <= 1u; ++owner_valid) {
                for (owner_player_two = 0u;
                     owner_player_two <= 1u;
                     ++owner_player_two) {
                    for (rendered_player_two = 0u;
                         rendered_player_two <= 1u;
                         ++rendered_player_two) {
                        for (capture_allowed = 0u;
                             capture_allowed <= 1u;
                             ++capture_allowed) {
                            for (compose_succeeded = 0u;
                                 compose_succeeded <= 1u;
                                 ++compose_succeeded) {
                                BOOL expected = isolation && owner_valid &&
                                    capture_allowed && compose_succeeded &&
                                    rendered_player_two == owner_player_two;
                                BOOL actual =
                                    SudekiMpSplitScreenQuickMenuOwnerCaptureAdvanced(
                                        isolation != 0u,
                                        owner_valid != 0u,
                                        owner_player_two != 0u,
                                        rendered_player_two != 0u,
                                        capture_allowed != 0u,
                                        compose_succeeded != 0u
                                    );
                                if ((actual != FALSE) !=
                                    (expected != FALSE)) {
                                    capture_policy_mismatch = TRUE;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (capture_policy_mismatch) {
            fputs("FAIL: Quick Menu owner-cache capture truth table mismatch\n",
                stderr);
            ++failures;
        }

        for (state = SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
             state <= SUDEKIMP_QUICK_MENU_ISOLATION_TAIL + 1u;
             ++state) {
            for (submit_seen = 0u; submit_seen <= 1u; ++submit_seen) {
                for (owner_frame_captured = 0u;
                     owner_frame_captured <= 1u;
                     ++owner_frame_captured) {
                    unsigned int expected = state;
                    unsigned int actual;

                    if (state == SUDEKIMP_QUICK_MENU_ISOLATION_TAIL &&
                        !submit_seen && owner_frame_captured) {
                        expected = SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
                    }
                    actual = SudekiMpSplitScreenQuickMenuIsolationEndState(
                        state,
                        submit_seen != 0u,
                        owner_frame_captured != 0u
                    );
                    if (actual != expected) {
                        tail_policy_mismatch = TRUE;
                    }
                }
            }
        }
        if (tail_policy_mismatch) {
            fputs("FAIL: Quick Menu close-tail truth table mismatch\n", stderr);
            ++failures;
        }

        /* The native root has both same-frame drawing and next-flush queued
         * text.  Consecutive frames must therefore stay on the same owner.
         * A minimap owner mismatch holds capture=false and must not discharge
         * the tail just because composition retained a valid cached pair. */
        if (SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
                TRUE, TRUE, FALSE, TRUE) ||
            SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
                TRUE, TRUE, FALSE, FALSE) ||
            SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
                TRUE, TRUE, TRUE, FALSE, FALSE) ||
            !SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
                TRUE, TRUE, TRUE, FALSE, TRUE) ||
            SudekiMpSplitScreenQuickMenuOwnerCaptureAdvanced(
                TRUE, TRUE, FALSE, FALSE, FALSE, TRUE) ||
            SudekiMpSplitScreenQuickMenuIsolationEndState(
                SUDEKIMP_QUICK_MENU_ISOLATION_TAIL,
                FALSE,
                FALSE
            ) != SUDEKIMP_QUICK_MENU_ISOLATION_TAIL ||
            !SudekiMpSplitScreenQuickMenuOwnerCaptureAdvanced(
                TRUE, TRUE, FALSE, FALSE, TRUE, TRUE) ||
            SudekiMpSplitScreenQuickMenuIsolationEndState(
                SUDEKIMP_QUICK_MENU_ISOLATION_TAIL,
                FALSE,
                TRUE
            ) != SUDEKIMP_QUICK_MENU_ISOLATION_IDLE ||
            !SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
                TRUE, TRUE, TRUE, FALSE) ||
            SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
                TRUE, TRUE, TRUE, TRUE, TRUE) ||
            !SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
                TRUE, TRUE, TRUE, TRUE, FALSE) ||
            !SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
                FALSE, TRUE, FALSE, TRUE)) {
            fputs("FAIL: Quick Menu current/queued owner-cache sequence mismatch\n",
                stderr);
            ++failures;
        }
    }

    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE,
            FALSE,
            TRUE,
            TRUE
        );
    if (quick_menu_isolation_state !=
            SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE) {
        fputs("FAIL: Quick Menu eligible rising edge did not start isolation\n",
            stderr);
        ++failures;
    }
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE,
            FALSE,
            TRUE,
            FALSE
        );
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            quick_menu_isolation_state,
            TRUE,
            TRUE,
            TRUE
        );
    if (quick_menu_isolation_state !=
        SUDEKIMP_QUICK_MENU_ISOLATION_FAILED) {
        fputs("FAIL: Quick Menu isolation upgraded in the middle of an open menu\n",
            stderr);
        ++failures;
    }
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE,
            TRUE,
            FALSE,
            FALSE
        );
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationEndState(
            quick_menu_isolation_state,
            TRUE,
            TRUE
        );
    if (quick_menu_isolation_state !=
        SUDEKIMP_QUICK_MENU_ISOLATION_TAIL) {
        fputs("FAIL: Quick Menu close tail did not preserve a queued submit\n",
            stderr);
        ++failures;
    }
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationEndState(
            quick_menu_isolation_state,
            FALSE,
            TRUE
        );
    if (quick_menu_isolation_state !=
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE ||
        SudekiMpSplitScreenQuickMenuIsolationCancelState(TRUE) !=
            SUDEKIMP_QUICK_MENU_ISOLATION_FAILED ||
        SudekiMpSplitScreenQuickMenuIsolationCancelState(FALSE) !=
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE) {
        fputs("FAIL: Quick Menu close/failure/quit state sequence mismatch\n",
            stderr);
        ++failures;
    }

    if (*(const uint32_t *)(
            image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) !=
        0x0049bba0u) {
        fputs("FAIL: exact Quick Menu render-submit vtable slot mismatch\n",
            stderr);
        ++failures;
    }
    if (*(const uint32_t *)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
            0x00498b40u ||
        relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
            image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
        *(const uint32_t *)(image + RVA_QUICK_MENU_CLOSE + 3u) !=
            0x007c2f84u ||
        *(const uint32_t *)(image + RVA_QUICK_MENU_START + 1u) !=
            0x00808d1cu ||
        memcmp(image + RVA_QUICK_MENU_INPUT,
            "\x8b\x44\x24\x04\x55\x56\x57\x8b\xe9\x83\xf8\x19",
            12u) != 0 ||
        memcmp(image + RVA_HUD_PARTY_POINTER_COPY,
            "\x8b\x11\x8b\xca\x89\x10\xc7\x40\x04\x00\x00\x00",
            12u) != 0 ||
        memcmp(image + RVA_TRACKED_ENTITY_CLEANUP,
            "\x8b\x01\x33\xd2\x3b\xc2\x74\x2d",
            8u) != 0) {
        fputs("FAIL: exact seat-owned Quick Menu ABI seam mismatch\n", stderr);
        ++failures;
    }
    for (index = 0u;
         index < sizeof(quick_menu_owner_copy_call_rvas) /
            sizeof(quick_menu_owner_copy_call_rvas[0]);
         ++index) {
        if (relative_call_target(
                image + quick_menu_owner_copy_call_rvas[index]) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
            fprintf(stderr,
                "FAIL: exact Quick Menu owner-copy seam %lu mismatch\n",
                (unsigned long)index);
            ++failures;
        }
    }
    {
        static const uint8_t shop_active_tail[] = {
            0x85, 0xc0, 0x74, 0x1e, 0x83, 0xb8, 0xb8, 0x00,
            0x00, 0x00, 0x07, 0x74, 0x0f, 0x8b, 0x40, 0x74
        };
        static const uint8_t blacksmith_active_tail[] = {
            0x85, 0xc0, 0x74, 0x04, 0x8a, 0x40, 0x29, 0xc3,
            0x32, 0xc0, 0xc3
        };

        if (image[RVA_SHOP_IS_ACTIVE] != 0xa1u ||
            *(const uint32_t *)(image + RVA_SHOP_IS_ACTIVE + 1u) !=
                0x007c2f88u ||
            memcmp(
                image + RVA_SHOP_IS_ACTIVE + 5u,
                shop_active_tail,
                sizeof(shop_active_tail)) != 0 ||
            image[RVA_BLACKSMITH_IS_ACTIVE] != 0xa1u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_IS_ACTIVE + 1u) != 0x007c2f74u ||
            memcmp(
                image + RVA_BLACKSMITH_IS_ACTIVE + 5u,
                blacksmith_active_tail,
                sizeof(blacksmith_active_tail)) != 0 ||
            *(const uint32_t *)(
                image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x08u) !=
                    0x0049d1d0u ||
            *(const uint32_t *)(
                image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x0cu) !=
                    0x0049d8d0u ||
            *(const uint32_t *)(
                image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x2cu) !=
                    0x0049c930u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x08u) !=
                0x00489660u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x0cu) !=
                0x0048a210u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x2cu) !=
                0x004898a0u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x48u) !=
                0x0048c850u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x4cu) !=
                0x0048d030u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x08u) !=
                    0x0048d6f0u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x0cu) !=
                    0x0048e910u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x2cu) !=
                    0x0048d970u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x48u) !=
                    0x00490c20u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x4cu) !=
                    0x00491b40u) {
            fputs("FAIL: exact shared Shop/Blacksmith modal seams mismatch\n",
                stderr);
            ++failures;
        }
    }
    if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: exact minimap party-owner call seam mismatch\n", stderr);
        ++failures;
    }

    /* Simulate the loader relocation for the absolute jump-table pointer. */
    *(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) =
        image + RVA_SCRIPT_CALL_OPCODE;
    *(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) =
        image + RVA_SCRIPT_METHOD_OPCODE;
    *(void **)(image + RVA_SCRIPT_SCENE_OPCODE_SLOT) =
        image + RVA_SCRIPT_SCENE_OPCODE;
    *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) =
        image + RVA_CHARACTER_INPUT_HANDLER;
    *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) =
        image + RVA_CONTROLLER_UPDATE;
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_CAMERA_INPUT_EVENT;
    *(void **)(image + RVA_GEL_GROUP_PTR_VTABLE + 0x00u) =
        image + RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR;
    *(void **)(image + RVA_GEL_GROUP_PTR_VTABLE + 0x10u) =
        image + RVA_GEL_GROUP_PTR_GET_RAW_ENTITY;
    *(void **)(image + RVA_GEL_GROUP_PTR_VTABLE + 0x2cu) =
        image + RVA_GEL_GROUP_PTR_TYPE_NAME;
    *(uint32_t *)(image + RVA_GEL_POINTER_RESOLVE_ENTITY + 3u) =
        (uint32_t)(uintptr_t)(image + RVA_GEL_POINTER_RESOLVER_HANDLER);
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_RENDER_SUBMIT;
    *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_INPUT;
    *(uint32_t *)(image + RVA_QUICK_MENU_CLOSE + 3u) =
        (uint32_t)(uintptr_t)(image + RVA_QUICK_MENU_GLOBAL);
    *(uint32_t *)(image + RVA_QUICK_MENU_START + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_WORLD_SCENE_GLOBAL);
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x08u) =
        image + RVA_INGAME_UI_CONTROLLER_UPDATE;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x0cu) =
        image + RVA_INGAME_UI_CONTROLLER_RENDER;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x2cu) =
        image + RVA_INGAME_UI_CONTROLLER_INPUT;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x08u) =
        image + RVA_SHOP_LAYER_UPDATE;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x0cu) =
        image + RVA_SHOP_LAYER_RENDER;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x2cu) =
        image + RVA_SHOP_LAYER_INPUT;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x48u) =
        image + RVA_SHOP_LAYER_RESOURCE_CREATE;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x4cu) =
        image + RVA_SHOP_LAYER_RESOURCE_DESTROY;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x08u) =
        image + RVA_BLACKSMITH_LAYER_UPDATE;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x0cu) =
        image + RVA_BLACKSMITH_LAYER_RENDER;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x2cu) =
        image + RVA_BLACKSMITH_LAYER_INPUT;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x48u) =
        image + RVA_BLACKSMITH_LAYER_RESOURCE_CREATE;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x4cu) =
        image + RVA_BLACKSMITH_LAYER_RESOURCE_DESTROY;
    *(uint32_t *)(image + RVA_PC_QUIT_SCREEN_SHOW + 3u) =
        (uint32_t)(uintptr_t)(image + RVA_PC_QUIT_SCREEN_GLOBAL);
    *(uint32_t *)(image + RVA_QUICK_MENU_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_QUICK_MENU_GLOBAL);
    *(uint32_t *)(image + RVA_SHOP_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL);
    *(uint32_t *)(image + RVA_BLACKSMITH_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_BLACKSMITH_LAYER_GLOBAL);
    *(uint32_t *)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM + 9u) =
        (uint32_t)(uintptr_t)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP);
    *(uint32_t *)(image + RVA_HUD_PORTRAIT_RESOURCE_SELECT + 8u) =
        (uint32_t)(uintptr_t)(image + RVA_UI_RESOURCE_TABLE_INITIALIZED);
    *(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) =
        image + RVA_MOTION_BLUR_POST_RENDER;
    *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) =
        image + RVA_SCREENSHOT_POST_RENDER;
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_RENDER_SUBMIT;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x08u) =
        image + RVA_INGAME_UI_CONTROLLER_UPDATE;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x0cu) =
        image + RVA_INGAME_UI_CONTROLLER_RENDER;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x2cu) =
        image + RVA_INGAME_UI_CONTROLLER_INPUT;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x08u) =
        image + RVA_SHOP_LAYER_UPDATE;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x0cu) =
        image + RVA_SHOP_LAYER_RENDER;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x2cu) =
        image + RVA_SHOP_LAYER_INPUT;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x48u) =
        image + RVA_SHOP_LAYER_RESOURCE_CREATE;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x4cu) =
        image + RVA_SHOP_LAYER_RESOURCE_DESTROY;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x08u) =
        image + RVA_BLACKSMITH_LAYER_UPDATE;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x0cu) =
        image + RVA_BLACKSMITH_LAYER_RENDER;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x2cu) =
        image + RVA_BLACKSMITH_LAYER_INPUT;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x48u) =
        image + RVA_BLACKSMITH_LAYER_RESOURCE_CREATE;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x4cu) =
        image + RVA_BLACKSMITH_LAYER_RESOURCE_DESTROY;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x40u) =
        image + RVA_ANIMATION_RENDERER_LOOKUP;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xf8u) =
        image + RVA_ANIMATION_RENDERER_COUNT;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xfcu) =
        image + RVA_ANIMATION_RENDERER_SELECTOR_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x100u) =
        image + RVA_ANIMATION_RENDERER_SELECTOR_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x104u) =
        image + RVA_ANIMATION_RENDERER_RATE_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x108u) =
        image + RVA_ANIMATION_RENDERER_RATE_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x10cu) =
        image + RVA_ANIMATION_RENDERER_TIME_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x110u) =
        image + RVA_ANIMATION_RENDERER_TIME_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x114u) =
        image + RVA_ANIMATION_RENDERER_STATE_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x118u) =
        image + RVA_ANIMATION_RENDERER_STATE_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x144u) =
        image + RVA_ANIMATION_RENDERER_BLEND_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x148u) =
        image + RVA_ANIMATION_RENDERER_BLEND_GET;
    *(uint32_t *)(image + RVA_PC_QUIT_SCREEN_SHOW + 3u) =
        (uint32_t)(uintptr_t)(image + RVA_PC_QUIT_SCREEN_GLOBAL);
    *(uint32_t *)(image + RVA_QUICK_MENU_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_QUICK_MENU_GLOBAL);
    *(uint32_t *)(image + RVA_SHOP_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL);
    *(uint32_t *)(image + RVA_BLACKSMITH_START + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_WORLD_SCENE_GLOBAL);
    *(uint32_t *)(image + RVA_BLACKSMITH_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_BLACKSMITH_LAYER_GLOBAL);
    *(uint32_t *)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM + 9u) =
        (uint32_t)(uintptr_t)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP);
    *(uint32_t *)(image + RVA_HUD_PORTRAIT_RESOURCE_SELECT + 8u) =
        (uint32_t)(uintptr_t)(image + RVA_UI_RESOURCE_TABLE_INITIALIZED);
    *(uint32_t *)(image + RVA_SKILL_DATA_AVAILABLE + 2u) =
        (uint32_t)(uintptr_t)(image + RVA_SKILL_AVAILABILITY_FLAG);
    *(uint32_t *)(image + RVA_SKILL_VALIDATE + 2u) =
        (uint32_t)(uintptr_t)(image + RVA_SKILL_VALIDATE_FLAG);
    *(uint32_t *)(image + RVA_SPIRIT_STRIKE_VALIDATE + 5u) =
        (uint32_t)(uintptr_t)(image + RVA_SPIRIT_STRIKE_VALIDATION_FLAG);

    check_blacksmith_ui_adapter_exact_image(image, &failures);
    check_blacksmith_roster_actor_identity_policy(&failures);
    check_adaptive_seat_activation_policy(&failures);
    check_fixed_three_owner_evidence_and_orbit_policies(&failures);

    {
        uint8_t saved_blacksmith_signature =
            image[RVA_BLACKSMITH_IS_ACTIVE + 5u];

        image[RVA_BLACKSMITH_IS_ACTIVE + 5u] ^= 0xffu;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                FALSE,
                FALSE,
                0u,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: modal inspector accepted a mismatched exact signature\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: modal inspector signature mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        if (SudekiMpSplitScreenSharedInteractionModalActive()) {
            fputs("FAIL: failed modal inspector install quiesced gameplay\n",
                stderr);
            ++failures;
        }
        image[RVA_BLACKSMITH_IS_ACTIVE + 5u] =
            saved_blacksmith_signature;
    }
    check_shared_interaction_modal_runtime(image, &failures);

    for (index = 0u;
            index < sizeof(expected_cleanroom_entries) /
                sizeof(expected_cleanroom_entries[0]);
            ++index) {
        if (memcmp(
                image + expected_cleanroom_entries[index].function_rva,
                expected_cleanroom_entries[index].bytes,
                expected_cleanroom_entries[index].byte_count) != 0) {
            fprintf(
                stderr,
                "FAIL: cleanroom native entry mismatch: %s\n",
                expected_cleanroom_entries[index].name
            );
            ++failures;
        }
    }
    test_zone_transition_exact_image(image, &failures);
    test_talos_native_lifecycle_exact_image(image, &failures);
    if (failures != 0) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }

    if (!SudekiMpInstallSkillTrace((HMODULE)image, 1.0f, 1.0f)) {
        fprintf(stderr, "install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInitializeSkillActivationAbi((HMODULE)image)) {
        fprintf(stderr, "skill activation ABI rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInitializeSpiritActivationAbi((HMODULE)image)) {
        fprintf(stderr, "spirit activation ABI rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    {
        uint8_t saved_item_apply =
            image[RVA_QUICK_ITEM_APPLY_TO_PARTY_SLOT];

        image[RVA_QUICK_ITEM_APPLY_TO_PARTY_SLOT] ^= 0x01u;
        if (SudekiMpInitializeItemActivationAbi((HMODULE)image) ||
            GetLastError() != ERROR_BAD_EXE_FORMAT) {
            fputs("FAIL: item activation ABI accepted a changed native apply entry\n",
                stderr);
            ++failures;
            SudekiMpResetItemActivationAbi();
        }
        image[RVA_QUICK_ITEM_APPLY_TO_PARTY_SLOT] = saved_item_apply;
    }
    if (!SudekiMpInitializeWeaponActivationAbi((HMODULE)image) ||
        !SudekiMpInitializeItemActivationAbi((HMODULE)image)) {
        fprintf(stderr, "weapon activation ABI rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallQuickSkillInputTrace((HMODULE)image, TRUE, TRUE)) {
        fprintf(stderr, "quick-skill install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallSpiritStrikeInput((HMODULE)image, -1, 1u, 'G')) {
        fprintf(stderr, "Spirit Strike input install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallCharacterSwitchTrace((HMODULE)image, TRUE)) {
        fprintf(stderr, "character-switch trace install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallTalosDefenseTrace((HMODULE)image)) {
        fprintf(stderr, "Talos defense trace rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (image[RVA_APPLY_DAMAGE] != 0xe9u ||
        image[RVA_COLLISION_DAMAGE] != 0xe9u) {
        fputs("FAIL: Talos defense inline hooks were not installed\n", stderr);
        ++failures;
    }
    {
        uint8_t controller_update_original[16];
        char observer_owner_one;
        char observer_owner_two;
        unsigned int feature;
        BOOL service_installed;
        TestControllerUpdateFunction installed_update = NULL;
        void *installed_update_value = NULL;
        uint64_t first_dispatch_serial = 0u;
        uint32_t first_registry_generation = 0u;

        memcpy(
            controller_update_original,
            image + RVA_CONTROLLER_UPDATE,
            sizeof(controller_update_original)
        );
        image[RVA_CONTROLLER_UPDATE + 15u] ^= 0xffu;
        SetLastError(ERROR_SUCCESS);
        if (install_control_separation_profile(image, 0u, 0u) ||
            GetLastError() != ERROR_INVALID_DATA) {
            fputs("FAIL: service-only control hook accepted a mismatched exact controller entry\n",
                stderr);
            ++failures;
            SudekiMpUninstallControlSeparation();
        }
        memcpy(
            image + RVA_CONTROLLER_UPDATE,
            controller_update_original,
            sizeof(controller_update_original)
        );
        for (feature = 1u; feature <= 8u; ++feature) {
            SetLastError(ERROR_SUCCESS);
            if (install_control_separation_profile(image, 0u, feature) ||
                GetLastError() != ERROR_INVALID_PARAMETER) {
                fprintf(stderr,
                    "FAIL: zero-key control service accepted enabled feature %u (error=%lu)\n",
                    feature,
                    (unsigned long)GetLastError());
                ++failures;
                SudekiMpUninstallControlSeparation();
            }
            if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                    image + RVA_CONTROLLER_UPDATE) {
                fputs("FAIL: rejected zero-key control profile changed the controller slot\n",
                    stderr);
                ++failures;
                SudekiMpUninstallControlSeparation();
            }
        }
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpControlSeparationRegisterUpdateObserver(
                NULL, service_update_observer_one) ||
            GetLastError() != ERROR_INVALID_PARAMETER) {
            fputs("FAIL: control observer registry accepted a null owner\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpControlSeparationRegisterUpdateObserver(
                &observer_owner_one, service_update_observer_one) ||
            !SudekiMpControlSeparationRegisterUpdateObserver(
                &observer_owner_one, service_update_observer_one)) {
            fputs("FAIL: owned control observer registration was not idempotent\n",
                stderr);
            ++failures;
        }
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpControlSeparationRegisterUpdateObserver(
                &observer_owner_one, service_update_observer_replacement) ||
            GetLastError() != ERROR_ALREADY_EXISTS) {
            fputs("FAIL: control observer owner was silently replaced\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpControlSeparationRegisterUpdateObserver(
                &observer_owner_two, service_update_observer_two)) {
            fputs("FAIL: second owned control observer was rejected\n", stderr);
            ++failures;
        }
        service_installed = install_control_separation_profile(image, 0u, 0u);
        if (!service_installed) {
            fprintf(stderr,
                "FAIL: exact service-only control hook was rejected (error=%lu)\n",
                (unsigned long)GetLastError());
            ++failures;
        } else {
            TestControllerUpdateFunction update;

            installed_update_value = *(void **)(
                image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
            update = (TestControllerUpdateFunction)installed_update_value;
            installed_update = update;
            if (installed_update_value == image + RVA_CONTROLLER_UPDATE) {
                fputs("FAIL: service-only control hook did not redirect the controller slot\n",
                    stderr);
                ++failures;
            }
            SetLastError(ERROR_SUCCESS);
            if (install_control_separation_profile(image, 'J', 0u) ||
                GetLastError() != ERROR_ALREADY_EXISTS ||
                *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                    installed_update_value ||
                memcmp(
                    image + RVA_CONTROLLER_UPDATE,
                    controller_update_original,
                    sizeof(controller_update_original)) != 0) {
                fputs("FAIL: duplicate normal install mutated the live service hook or native entry\n",
                    stderr);
                ++failures;
            }
            service_update_original_calls = 0u;
            service_update_observer_one_calls = 0u;
            service_update_observer_two_calls = 0u;
            service_update_sequence = 0u;
            service_update_order_failed = FALSE;
            service_update_context_failed = FALSE;
            service_update_expect_observer_one = TRUE;
            reset_service_update_witnesses();
            service_update_expected_controller =
                (void *)(uintptr_t)0x11111111u;
            service_update_expected_data =
                (void *)(uintptr_t)0x22222222u;
            service_update_self_unregister_owner = &observer_owner_one;
            point_relative_jump(
                image + RVA_CONTROLLER_UPDATE,
                (const uint8_t *)service_update_original_stub
            );
            update(
                (void *)service_update_expected_controller,
                (void *)service_update_expected_data
            );
            if (service_update_original_calls != 1u ||
                service_update_observer_one_calls != 1u ||
                service_update_observer_two_calls != 1u ||
                service_update_sequence != 3u ||
                service_update_order_failed || service_update_context_failed ||
                service_update_witness_count != 2u ||
                !service_update_witness_matches(
                    &service_update_witnesses[0],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 2u, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE,
                    TRUE, TRUE) ||
                !service_update_witness_matches(
                    &service_update_witnesses[1],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 2u, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE,
                    TRUE, TRUE) ||
                service_update_witnesses[0].dispatch_serial !=
                    service_update_witnesses[1].dispatch_serial ||
                service_update_witnesses[0].observer_registry_generation !=
                    service_update_witnesses[1].observer_registry_generation ||
                service_update_observer_entry_errors[0] != 0x1234u ||
                service_update_observer_entry_errors[1] != 0x1234u ||
                !service_update_witness_revalidated[0] ||
                service_update_witness_revalidated_after_mutation ||
                service_update_witness_revalidated[1] ||
                GetLastError() != 0x1234u) {
                fputs("FAIL: service-only control dispatch did not preserve native context, exact witness, and original-once then owned-observers order\n",
                    stderr);
                ++failures;
            }
            if (service_update_witness_count == 2u) {
                first_dispatch_serial =
                    service_update_witnesses[0].dispatch_serial;
                first_registry_generation =
                    service_update_witnesses[0]
                        .observer_registry_generation;
            }
            service_update_sequence = 0u;
            service_update_order_failed = FALSE;
            service_update_context_failed = FALSE;
            service_update_expect_observer_one = FALSE;
            reset_service_update_witnesses();
            service_update_expected_controller =
                (void *)(uintptr_t)0x33333333u;
            service_update_expected_data =
                (void *)(uintptr_t)0x44444444u;
            service_update_self_unregister_owner = NULL;
            update(
                (void *)service_update_expected_controller,
                (void *)service_update_expected_data
            );
            if (service_update_original_calls != 2u ||
                service_update_observer_one_calls != 1u ||
                service_update_observer_two_calls != 2u ||
                service_update_sequence != 2u ||
                service_update_order_failed || service_update_context_failed ||
                service_update_witness_count != 1u ||
                !service_update_witness_matches(
                    &service_update_witnesses[0],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 1u, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                    TRUE, TRUE) ||
                service_update_witnesses[0].dispatch_serial ==
                    first_dispatch_serial ||
                service_update_witnesses[0].observer_registry_generation ==
                    first_registry_generation ||
                service_update_observer_entry_errors[0] != 0x1234u ||
                !service_update_witness_revalidated[0] ||
                GetLastError() != 0x1234u) {
                fputs("FAIL: self-unregistered control observer ran later or registry witness did not advance\n",
                    stderr);
                ++failures;
            }
            SetLastError(0x2468u);
            if (service_update_witness_count == 1u &&
                (SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
                    &service_update_witnesses[0]) ||
                 GetLastError() != 0x2468u)) {
                fputs("FAIL: retained control witness revalidated outside its borrowed callback lifetime\n",
                    stderr);
                ++failures;
            }
            memcpy(
                image + RVA_CONTROLLER_UPDATE,
                controller_update_original,
                sizeof(controller_update_original)
            );
            SetLastError(ERROR_SUCCESS);
            if (SudekiMpControlSeparationRequestPlayerTwo(TRUE) ||
                GetLastError() != ERROR_INVALID_STATE) {
                fputs("FAIL: service-only control profile accepted a Player 2 request\n",
                    stderr);
                ++failures;
            }
            SetLastError(ERROR_SUCCESS);
            if (SudekiMpControlSeparationRequestSeat(2u, TRUE) ||
                GetLastError() != ERROR_INVALID_STATE) {
                fputs("FAIL: service-only control profile accepted a Player 3 request\n",
                    stderr);
                ++failures;
            }
            SetLastError(ERROR_SUCCESS);
            if (SudekiMpControlSeparationRequestSeatCharacter(
                    2u, (void *)(uintptr_t)0x33333333u) ||
                GetLastError() != ERROR_INVALID_STATE) {
                fputs("FAIL: service-only control profile accepted a Player 3 actor request\n",
                    stderr);
                ++failures;
            }
        }
        if (!SudekiMpControlSeparationUnregisterUpdateObserver(
                &observer_owner_one) ||
            !SudekiMpControlSeparationUnregisterUpdateObserver(
                &observer_owner_one)) {
            fputs("FAIL: owned control observer removal was not idempotent\n",
                stderr);
            ++failures;
        }
        if (service_installed) {
            void **controller_slot = (void **)(
                image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
            void *foreign_controller_owner =
                image + RVA_CHARACTER_INPUT_HANDLER;

            if (!SudekiMpControlSeparationRegisterUpdateObserver(
                    &observer_owner_one, service_update_observer_one)) {
                fputs("FAIL: control observer could not be restored for teardown ownership test\n",
                    stderr);
                ++failures;
            }
            *controller_slot = foreign_controller_owner;
            SetLastError(ERROR_SUCCESS);
            SudekiMpUninstallControlSeparation();
            if (GetLastError() != ERROR_BUSY ||
                *controller_slot != foreign_controller_owner) {
                fputs("FAIL: control teardown overwrote a foreign controller-slot owner\n",
                    stderr);
                ++failures;
            }

            /* The still-live wrapper must expose that its pointer-hook record
             * remains owned while the vtable slot itself has been replaced.
             * A direct stale invocation is observable, but cannot claim exact
             * service-only dispatch authority. */
            point_relative_jump(
                image + RVA_CONTROLLER_UPDATE,
                (const uint8_t *)service_update_original_stub
            );
            service_update_original_calls = 0u;
            service_update_observer_one_calls = 0u;
            service_update_observer_two_calls = 0u;
            service_update_sequence = 0u;
            service_update_order_failed = FALSE;
            service_update_context_failed = FALSE;
            service_update_expect_observer_one = TRUE;
            service_update_expected_controller =
                (void *)(uintptr_t)0x55555555u;
            service_update_expected_data =
                (void *)(uintptr_t)0x66666666u;
            service_update_self_unregister_owner = NULL;
            reset_service_update_witnesses();
            installed_update(
                (void *)service_update_expected_controller,
                (void *)service_update_expected_data
            );
            if (service_update_original_calls != 1u ||
                service_update_observer_one_calls != 1u ||
                service_update_observer_two_calls != 1u ||
                service_update_sequence != 3u ||
                service_update_order_failed || service_update_context_failed ||
                service_update_witness_count != 2u ||
                !service_update_witness_matches(
                    &service_update_witnesses[0],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 2u, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE,
                    FALSE, FALSE) ||
                !service_update_witness_matches(
                    &service_update_witnesses[1],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 2u, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE,
                    FALSE, FALSE) ||
                service_update_witnesses[0].dispatch_serial !=
                    service_update_witnesses[1].dispatch_serial ||
                service_update_observer_entry_errors[0] != 0x1234u ||
                service_update_observer_entry_errors[1] != 0x1234u ||
                service_update_witness_revalidated[0] ||
                service_update_witness_revalidated[1] ||
                GetLastError() != 0x1234u) {
                fputs("FAIL: foreign controller slot retained or forged exact service dispatch authority\n",
                    stderr);
                ++failures;
            }

            /* Return slot ownership and call the still-installed wrapper.
             * Failed teardown must preserve both the native callback and its
             * observer registry until a later successful retry. */
            *controller_slot = installed_update_value;
            service_update_original_calls = 0u;
            service_update_observer_one_calls = 0u;
            service_update_observer_two_calls = 0u;
            service_update_sequence = 0u;
            service_update_order_failed = FALSE;
            service_update_context_failed = FALSE;
            service_update_expect_observer_one = TRUE;
            service_update_expected_controller =
                (void *)(uintptr_t)0x77777777u;
            service_update_expected_data =
                (void *)(uintptr_t)0x88888888u;
            reset_service_update_witnesses();
            installed_update(
                (void *)service_update_expected_controller,
                (void *)service_update_expected_data
            );
            if (service_update_original_calls != 1u ||
                service_update_observer_one_calls != 1u ||
                service_update_observer_two_calls != 1u ||
                service_update_sequence != 3u ||
                service_update_order_failed || service_update_context_failed ||
                service_update_witness_count != 2u ||
                !service_update_witness_matches(
                    &service_update_witnesses[0],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 2u, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE,
                    TRUE, TRUE) ||
                !service_update_witness_matches(
                    &service_update_witnesses[1],
                    SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                    1u, 2u, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE,
                    TRUE, TRUE) ||
                service_update_witnesses[0].dispatch_serial !=
                    service_update_witnesses[1].dispatch_serial ||
                service_update_observer_entry_errors[0] != 0x1234u ||
                service_update_observer_entry_errors[1] != 0x1234u ||
                !service_update_witness_revalidated[0] ||
                !service_update_witness_revalidated[1] ||
                GetLastError() != 0x1234u) {
                fputs("FAIL: failed control teardown cleared live wrapper state or exact witness\n",
                    stderr);
                ++failures;
            }
            memcpy(
                image + RVA_CONTROLLER_UPDATE,
                controller_update_original,
                sizeof(controller_update_original)
            );
        }
        {
            int player_three_character_marker;
            int player_three_input_marker;
            int player_three_camera_marker;
            int player_three_render_state_marker;
            SudekiMpPlayerCombatSnapshot player_three_snapshot;

            SudekiMpCombatContextSetCharacter(
                2u, &player_three_character_marker);
            SudekiMpCombatContextSetInputSource(
                2u,
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE,
                &player_three_input_marker);
            SudekiMpCombatContextSetView(
                2u,
                &player_three_camera_marker,
                &player_three_render_state_marker);
            SudekiMpUninstallControlSeparation();
            if (!SudekiMpCombatContextGetSnapshot(
                    2u, &player_three_snapshot) ||
                player_three_snapshot.character != NULL ||
                player_three_snapshot.input_source != NULL ||
                player_three_snapshot.input_source_kind !=
                    SUDEKIMP_COMBAT_INPUT_NONE ||
                player_three_snapshot.viewport_camera != NULL ||
                player_three_snapshot.render_state != NULL) {
                fputs("FAIL: control teardown retained Player 3 combat-context ownership\n",
                    stderr);
                ++failures;
            }
        }
        if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                image + RVA_CONTROLLER_UPDATE) {
            fputs("FAIL: service-only control hook did not restore the controller slot\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpControlSeparationRegisterUpdateObserver(
                &observer_owner_two, service_update_observer_replacement)) {
            fputs("FAIL: control teardown did not clear owned observers after slot restoration\n",
                stderr);
            ++failures;
        }
        (void)SudekiMpControlSeparationUnregisterUpdateObserver(
            &observer_owner_two);

        {
            char normal_observer_owner;
            uint8_t controller[0x24cu];
            uint32_t update_data = 0x13572468u;
            TestControllerUpdateFunction normal_update;

            ZeroMemory(controller, sizeof(controller));
            if (!SudekiMpControlSeparationRegisterUpdateObserver(
                    &normal_observer_owner,
                    service_update_witness_capture_observer) ||
                !install_control_separation_profile(image, 'J', 0u)) {
                fprintf(stderr,
                    "FAIL: normal control witness fixture could not install (error=%lu)\n",
                    (unsigned long)GetLastError());
                ++failures;
                SudekiMpUninstallControlSeparation();
            } else {
                normal_update = (TestControllerUpdateFunction)*(void **)(
                    image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
                SetLastError(ERROR_SUCCESS);
                if (install_control_separation_profile(image, 0u, 0u) ||
                    GetLastError() != ERROR_ALREADY_EXISTS ||
                    *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                        (void *)normal_update ||
                    memcmp(
                        image + RVA_CONTROLLER_UPDATE,
                        controller_update_original,
                        sizeof(controller_update_original)) != 0) {
                    fputs("FAIL: duplicate service install mutated the live normal hook or native entry\n",
                        stderr);
                    ++failures;
                }
                point_relative_jump(
                    image + RVA_CONTROLLER_UPDATE,
                    (const uint8_t *)service_update_original_stub
                );
                SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
                service_update_original_calls = 0u;
                service_update_context_failed = FALSE;
                service_update_expected_controller = controller;
                service_update_expected_data = &update_data;
                reset_service_update_witnesses();
                normal_update(controller, &update_data);
                if (service_update_original_calls != 1u ||
                    service_update_context_failed ||
                    service_update_witness_count != 1u ||
                    !service_update_witness_matches(
                        &service_update_witnesses[0],
                        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL,
                        1u, 1u, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE,
                        TRUE, FALSE) ||
                    service_update_observer_entry_errors[0] != 0x1234u ||
                    !service_update_witness_revalidated[0] ||
                    GetLastError() != 0x1234u) {
                    fputs("FAIL: normal post-original notify forged or lost its dispatch witness\n",
                        stderr);
                    ++failures;
                }

                SudekiMpInputBridgeSetGameplaySuppressed(TRUE);
                service_update_context_failed = FALSE;
                reset_service_update_witnesses();
                normal_update(controller, &update_data);
                if (service_update_original_calls != 1u ||
                    service_update_context_failed ||
                    service_update_witness_count != 1u ||
                    !service_update_witness_matches(
                        &service_update_witnesses[0],
                        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_PRE_ORIGINAL,
                        0u, 1u, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE,
                        TRUE, FALSE) ||
                    !service_update_witness_revalidated[0]) {
                    fputs("FAIL: normal pre-original notify forged service-only post-original authority\n",
                        stderr);
                    ++failures;
                }
                SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
                memcpy(
                    image + RVA_CONTROLLER_UPDATE,
                    controller_update_original,
                    sizeof(controller_update_original)
                );
                (void)SudekiMpControlSeparationUnregisterUpdateObserver(
                    &normal_observer_owner);
                SudekiMpUninstallControlSeparation();
                if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                        image + RVA_CONTROLLER_UPDATE) {
                    fputs("FAIL: normal control witness fixture did not restore the controller slot\n",
                        stderr);
                    ++failures;
                }
            }
        }

        {
            char reentrant_observer_owner;
            TestControllerUpdateFunction reentrant_update;

            if (!SudekiMpControlSeparationRegisterUpdateObserver(
                    &reentrant_observer_owner,
                    service_update_witness_capture_observer) ||
                !install_control_separation_profile(image, 0u, 0u)) {
                fprintf(stderr,
                    "FAIL: reentrant control witness fixture could not install (error=%lu)\n",
                    (unsigned long)GetLastError());
                ++failures;
                SudekiMpUninstallControlSeparation();
            } else {
                reentrant_update = (TestControllerUpdateFunction)*(void **)(
                    image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
                point_relative_jump(
                    image + RVA_CONTROLLER_UPDATE,
                    (const uint8_t *)service_update_reentrant_original_stub
                );
                service_update_reentrant_target = reentrant_update;
                service_update_reentrant_depth = 0u;
                service_update_original_calls = 0u;
                service_update_context_failed = FALSE;
                service_update_expected_controller =
                    (void *)(uintptr_t)0x99999999u;
                service_update_expected_data =
                    (void *)(uintptr_t)0xaaaaaaaau;
                reset_service_update_witnesses();
                reentrant_update(
                    (void *)service_update_expected_controller,
                    (void *)service_update_expected_data
                );
                if (service_update_original_calls != 2u ||
                    service_update_context_failed ||
                    service_update_witness_count != 2u ||
                    service_update_witnesses[0].dispatch_serial == 0u ||
                    service_update_witnesses[1].dispatch_serial == 0u ||
                    service_update_witnesses[0].dispatch_serial ==
                        service_update_witnesses[1].dispatch_serial ||
                    service_update_witnesses[0].native_thread_id !=
                        GetCurrentThreadId() ||
                    service_update_witnesses[1].native_thread_id !=
                        GetCurrentThreadId() ||
                    service_update_witnesses[0].outer_update_depth != 2u ||
                    service_update_witnesses[0].active_dispatch_count != 2u ||
                    service_update_witnesses[1].outer_update_depth != 1u ||
                    service_update_witnesses[1].active_dispatch_count != 1u ||
                    service_update_witnesses[0].original_call_count != 1u ||
                    service_update_witnesses[1].original_call_count != 1u ||
                    service_update_witnesses[0].observer_snapshot_count != 1u ||
                    service_update_witnesses[1].observer_snapshot_count != 1u ||
                    service_update_witnesses[0].observer_registry_generation !=
                        service_update_witnesses[1]
                            .observer_registry_generation ||
                    service_update_witnesses[0].hook_owned_exact != 1u ||
                    service_update_witnesses[1].hook_owned_exact != 1u ||
                    service_update_witnesses[0].slot_owned_exact != 1u ||
                    service_update_witnesses[1].slot_owned_exact != 1u ||
                    service_update_witnesses[0].service_only != 1u ||
                    service_update_witnesses[1].service_only != 1u ||
                    service_update_witnesses[0].post_original != 1u ||
                    service_update_witnesses[1].post_original != 1u ||
                    service_update_witnesses[0].source !=
                        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL ||
                    service_update_witnesses[1].source !=
                        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL ||
                    service_update_witnesses[0].source_exact != 0u ||
                    service_update_witnesses[1].source_exact != 0u ||
                    service_update_witnesses[0]
                        .service_post_original_exact != 0u ||
                    service_update_witnesses[1]
                        .service_post_original_exact != 0u ||
                    service_update_witnesses[0].sole_observer != 1u ||
                    service_update_witnesses[1].sole_observer != 1u ||
                    service_update_witnesses[0]
                        .registry_generation_stable != 1u ||
                    service_update_witnesses[1]
                        .registry_generation_stable != 1u ||
                    service_update_observer_entry_errors[0] != 0x1234u ||
                    service_update_observer_entry_errors[1] != 0x1234u ||
                    service_update_witness_revalidated[0] ||
                    service_update_witness_revalidated[1] ||
                    GetLastError() != 0x1234u) {
                    fputs("FAIL: reentrant controller updates retained exact dispatch authority\n",
                        stderr);
                    ++failures;
                }
                service_update_reentrant_target = NULL;
                memcpy(
                    image + RVA_CONTROLLER_UPDATE,
                    controller_update_original,
                    sizeof(controller_update_original)
                );
                (void)SudekiMpControlSeparationUnregisterUpdateObserver(
                    &reentrant_observer_owner);
                SudekiMpUninstallControlSeparation();
            }
        }

        {
            char teardown_observer_owner;
            TestControllerUpdateFunction fetched_update;

            if (!SudekiMpControlSeparationRegisterUpdateObserver(
                    &teardown_observer_owner,
                    service_update_witness_capture_observer) ||
                !install_control_separation_profile(image, 0u, 0u)) {
                fprintf(stderr,
                    "FAIL: in-flight control teardown fixture could not install (error=%lu)\n",
                    (unsigned long)GetLastError());
                ++failures;
                SudekiMpUninstallControlSeparation();
            } else {
                fetched_update = (TestControllerUpdateFunction)*(void **)(
                    image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
                point_relative_jump(
                    image + RVA_CONTROLLER_UPDATE,
                    (const uint8_t *)service_update_original_stub
                );
                service_update_original_calls = 0u;
                service_update_context_failed = FALSE;
                service_update_expected_controller =
                    (void *)(uintptr_t)0xbbbbbbbbu;
                service_update_expected_data =
                    (void *)(uintptr_t)0xccccccccu;
                reset_service_update_witnesses();
                service_update_request_uninstall = TRUE;
                fetched_update(
                    (void *)service_update_expected_controller,
                    (void *)service_update_expected_data
                );
                service_update_request_uninstall = FALSE;
                if (service_update_original_calls != 1u ||
                    service_update_context_failed ||
                    service_update_witness_count != 1u ||
                    service_update_uninstall_error != ERROR_BUSY ||
                    !service_update_revalidated_after_uninstall_attempt ||
                    *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                        (void *)fetched_update ||
                    !service_update_witness_matches(
                        &service_update_witnesses[0],
                        SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_SERVICE_POST_ORIGINAL,
                        1u, 1u, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                        TRUE, TRUE) ||
                    GetLastError() != 0x1234u) {
                    fputs("FAIL: in-flight control teardown cleared or weakened the live installation\n",
                        stderr);
                    ++failures;
                }

                reset_service_update_witnesses();
                fetched_update(
                    (void *)service_update_expected_controller,
                    (void *)service_update_expected_data
                );
                if (service_update_original_calls != 2u ||
                    service_update_context_failed ||
                    service_update_witness_count != 1u ||
                    !service_update_witness_revalidated[0]) {
                    fputs("FAIL: observer state did not survive rejected in-flight teardown\n",
                        stderr);
                    ++failures;
                }

                reset_service_update_witnesses();
                SudekiMpUninstallControlSeparation();
                if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                        image + RVA_CONTROLLER_UPDATE) {
                    fputs("FAIL: quiescent retry did not restore the controller slot\n",
                        stderr);
                    ++failures;
                }
                fetched_update(
                    (void *)service_update_expected_controller,
                    (void *)service_update_expected_data
                );
                if (service_update_original_calls != 3u ||
                    service_update_witness_count != 0u ||
                    GetLastError() != 0x1234u) {
                    fputs("FAIL: prefetched controller wrapper touched cleared state after teardown\n",
                        stderr);
                    ++failures;
                }
                memcpy(
                    image + RVA_CONTROLLER_UPDATE,
                    controller_update_original,
                    sizeof(controller_update_original)
                );
            }
        }

        {
            char disabler_owner;
            char gated_owner;
            TestControllerUpdateFunction stale_snapshot_update;

            stale_snapshot_observer_owner = &gated_owner;
            stale_snapshot_disabler_calls = 0u;
            stale_snapshot_callback_calls = 0u;
            stale_snapshot_backing_calls = 0u;
            service_update_context_failed = FALSE;
            if (!SudekiMpControlUpdateObserverGateEnable(
                    &stale_snapshot_observer_gate) ||
                !SudekiMpControlSeparationRegisterUpdateObserver(
                    &disabler_owner,
                    stale_snapshot_disabler_observer) ||
                !SudekiMpControlSeparationRegisterUpdateObserver(
                    &gated_owner,
                    stale_snapshot_gated_observer) ||
                !install_control_separation_profile(image, 0u, 0u)) {
                fprintf(stderr,
                    "FAIL: stale observer snapshot gate fixture could not install (error=%lu)\n",
                    (unsigned long)GetLastError());
                ++failures;
                SudekiMpControlUpdateObserverGateDisable(
                    &stale_snapshot_observer_gate);
                SudekiMpControlUpdateObserverGateDrain(
                    &stale_snapshot_observer_gate);
                SudekiMpUninstallControlSeparation();
            } else {
                stale_snapshot_update =
                    (TestControllerUpdateFunction)*(void **)(
                        image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
                point_relative_jump(
                    image + RVA_CONTROLLER_UPDATE,
                    (const uint8_t *)service_update_original_stub
                );
                service_update_original_calls = 0u;
                service_update_sequence = 0u;
                stale_snapshot_update(
                    (void *)(uintptr_t)0xddddddddu,
                    (void *)(uintptr_t)0xeeeeeeeeu
                );
                if (service_update_original_calls != 1u ||
                    stale_snapshot_disabler_calls != 1u ||
                    stale_snapshot_callback_calls != 1u ||
                    stale_snapshot_backing_calls != 0u ||
                    InterlockedCompareExchange(
                        &stale_snapshot_observer_gate.enabled, 0, 0) != 0 ||
                    InterlockedCompareExchange(
                        &stale_snapshot_observer_gate.active_entries,
                        0, 0) != 0 ||
                    service_update_context_failed ||
                    GetLastError() != 0x1234u) {
                    fputs("FAIL: stale snapshotted observer entered disabled backing state\n",
                        stderr);
                    ++failures;
                }
                (void)SudekiMpControlSeparationUnregisterUpdateObserver(
                    &disabler_owner);
                memcpy(
                    image + RVA_CONTROLLER_UPDATE,
                    controller_update_original,
                    sizeof(controller_update_original)
                );
                SudekiMpUninstallControlSeparation();
            }
        }
    }
    {
        uint8_t saved_set_speed = image[RVA_ARBITER_SET_SPEED];
        void *saved_update_slot = *(void **)(
            image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
        image[RVA_ARBITER_SET_SPEED] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (install_control_separation_profile(image, 'J', 0u)) {
            fputs("FAIL: control separation accepted mismatched native stop entry\n",
                stderr);
            ++failures;
            SudekiMpUninstallControlSeparation();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                       saved_update_slot) {
            fputs("FAIL: native stop mismatch did not fail closed before hook mutation\n",
                stderr);
            ++failures;
        }
        image[RVA_ARBITER_SET_SPEED] = saved_set_speed;
    }
    {
        uint8_t saved_set_speed =
            image[RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE];
        void *saved_update_slot = *(void **)(
            image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
        image[RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (install_control_separation_profile(image, 'J', 0u)) {
            fputs("FAIL: control separation accepted mismatched immediate native stop entry\n",
                stderr);
            ++failures;
            SudekiMpUninstallControlSeparation();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                       saved_update_slot) {
            fputs("FAIL: immediate native stop mismatch did not fail closed before hook mutation\n",
                stderr);
            ++failures;
        }
        image[RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE] = saved_set_speed;
    }
    {
        uint8_t saved_set_forward = image[RVA_POSITION_SET_FORWARD];
        void *saved_update_slot = *(void **)(
            image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
        image[RVA_POSITION_SET_FORWARD] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (install_control_separation_profile(image, 'J', 0u)) {
            fputs("FAIL: LAN control accepted mismatched native facing entry\n",
                stderr);
            ++failures;
            SudekiMpUninstallControlSeparation();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                       saved_update_slot) {
            fputs("FAIL: native facing mismatch did not fail closed before hook mutation\n",
                stderr);
            ++failures;
        }
        image[RVA_POSITION_SET_FORWARD] = saved_set_forward;
    }
    {
        uint8_t saved_is_firing = image[RVA_MISSILE_MANAGER_IS_FIRING];
        void *saved_update_slot = *(void **)(
            image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
        image[RVA_MISSILE_MANAGER_IS_FIRING] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (install_control_separation_profile(image, 'J', 0u)) {
            fputs("FAIL: LAN control accepted mismatched missile firing predicate\n",
                stderr);
            ++failures;
            SudekiMpUninstallControlSeparation();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                       saved_update_slot) {
            fputs("FAIL: missile firing mismatch did not fail closed before hook mutation\n",
                stderr);
            ++failures;
        }
        image[RVA_MISSILE_MANAGER_IS_FIRING] = saved_is_firing;
    }
    {
        uint8_t saved_can_fire = image[RVA_MISSILE_MANAGER_CAN_FIRE];
        void *saved_update_slot = *(void **)(
            image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
        image[RVA_MISSILE_MANAGER_CAN_FIRE] ^= 0x01u;
        SetLastError(ERROR_SUCCESS);
        if (install_control_separation_profile(image, 'J', 0u)) {
            fputs("FAIL: LAN control accepted mismatched missile readiness predicate\n",
                stderr);
            ++failures;
            SudekiMpUninstallControlSeparation();
        } else if (GetLastError() != ERROR_INVALID_DATA ||
                   *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
                       saved_update_slot) {
            fputs("FAIL: missile readiness mismatch did not fail closed before hook mutation\n",
                stderr);
            ++failures;
        }
        image[RVA_MISSILE_MANAGER_CAN_FIRE] = saved_can_fire;
    }
    if (!SudekiMpInstallControlSeparation(
            (HMODULE)image,
            'J',
            TRUE,
            TRUE,
            TRUE,
            10.0f,
            TRUE,
            'U',
            TRUE,
            second_player_skill_keys,
            TRUE,
            TRUE,
            FALSE,
            0.20f)) {
        fprintf(stderr, "control-separation install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) ==
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) ==
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: roaming boundary did not hook both native Player 1 movement submissions\n",
            stderr);
        ++failures;
    }
    SudekiMpUninstallControlSeparation();
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) !=
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: roaming boundary did not restore both native Player 1 movement submissions\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpInstallControlSeparation(
            (HMODULE)image,
            'J',
            TRUE,
            TRUE,
            FALSE,
            10.0f,
            TRUE,
            'U',
            TRUE,
            second_player_skill_keys,
            TRUE,
            TRUE,
            FALSE,
            0.20f)) {
        fprintf(stderr,
            "control-separation coexistence install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    {
        const uint32_t character_switch_rvas[] = {
            RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER,
            RVA_GROUP_PLAYERS_NEXT_CHARACTER
        };

        for (index = 0u;
                index < sizeof(character_switch_rvas) /
                    sizeof(character_switch_rvas[0]);
                ++index) {
            uint8_t saved_entry = image[character_switch_rvas[index]];

            image[character_switch_rvas[index]] ^= 0xffu;
            SetLastError(ERROR_SUCCESS);
            if (SudekiMpInstallSplitScreenRender(
                    (HMODULE)image,
                    FALSE,
                    FALSE,
                    0u,
                    FALSE,
                    FALSE,
                    FALSE,
                    FALSE,
                    FALSE,
                    0.20f,
                    2.25f,
                    1.50f,
                    0.65f)) {
                fprintf(stderr,
                    "FAIL: split-screen render accepted mismatched character-switch consumer %lu\n",
                    (unsigned long)index);
                ++failures;
                SudekiMpUninstallSplitScreenRender();
            } else if (GetLastError() != ERROR_INVALID_DATA) {
                fprintf(stderr,
                    "FAIL: character-switch consumer %lu mismatch returned error=%lu\n",
                    (unsigned long)index,
                    (unsigned long)GetLastError());
                ++failures;
            }
            image[character_switch_rvas[index]] = saved_entry;
        }
    }
    *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_IS_ACTIVE;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fputs("FAIL: split-screen render accepted a mismatched Quick Menu input slot\n",
            stderr);
        ++failures;
        SudekiMpUninstallSplitScreenRender();
    } else if (GetLastError() != ERROR_INVALID_DATA) {
        fprintf(stderr,
            "FAIL: Quick Menu input-slot mismatch returned error=%lu\n",
            (unsigned long)GetLastError());
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
            image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
        relative_call_target(
            image + RVA_QUICK_MENU_OWNER_COPY_UI_ACTIVE) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: Quick Menu input mismatch mutated later owner hooks\n",
            stderr);
        ++failures;
    }
    *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_INPUT;
    {
        uint8_t cleanup_entry = image[RVA_TRACKED_ENTITY_CLEANUP];

        image[RVA_TRACKED_ENTITY_CLEANUP] ^= 0xffu;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: split-screen render accepted mismatched Quick Menu TPtr cleanup\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: Quick Menu TPtr cleanup mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        if (*(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
                image + RVA_QUICK_MENU_INPUT ||
            relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE) {
            fputs("FAIL: Quick Menu cleanup mismatch retained an earlier hook\n",
                stderr);
            ++failures;
        }
        image[RVA_TRACKED_ENTITY_CLEANUP] = cleanup_entry;
    }
    for (index = 0u;
         index < sizeof(quick_menu_owner_copy_call_rvas) /
            sizeof(quick_menu_owner_copy_call_rvas[0]);
         ++index) {
        uint32_t call_rva = quick_menu_owner_copy_call_rvas[index];
        int32_t owner_copy_displacement;

        memcpy(
            &owner_copy_displacement,
            image + call_rva + 1u,
            sizeof(owner_copy_displacement));
        image[call_rva + 1u] ^= 0xffu;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fprintf(stderr,
                "FAIL: split-screen render accepted mismatched Quick Menu owner source %lu\n",
                (unsigned long)index);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: Quick Menu owner-source %lu mismatch returned error=%lu\n",
                (unsigned long)index,
                (unsigned long)GetLastError());
            ++failures;
        }
        if (*(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
                image + RVA_QUICK_MENU_INPUT ||
            relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
            relative_call_target(image + (index == 0u ?
                RVA_QUICK_MENU_OWNER_COPY_SELECTION :
                RVA_QUICK_MENU_OWNER_COPY_UI_ACTIVE)) !=
                image + RVA_HUD_PARTY_POINTER_COPY) {
            fprintf(stderr,
                "FAIL: Quick Menu source %lu mismatch retained an earlier hook\n",
                (unsigned long)index);
            ++failures;
        }
        memcpy(
            image + call_rva + 1u,
            &owner_copy_displacement,
            sizeof(owner_copy_displacement));
        if (relative_call_target(image + call_rva) !=
                image + RVA_HUD_PARTY_POINTER_COPY) {
            fprintf(stderr,
                "FAIL: Quick Menu owner source %lu did not restore after probe\n",
                (unsigned long)index);
            ++failures;
        }
    }
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_IS_ACTIVE;
    if (SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            TRUE,
            FALSE,
            FALSE,
            TRUE,
            TRUE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fputs("FAIL: split-screen render accepted a mismatched Quick Menu render-submit slot\n",
            stderr);
        ++failures;
        SudekiMpUninstallSplitScreenRender();
    }
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_RENDER_SUBMIT;
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_IS_ACTIVE;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            FALSE,
            FALSE,
            TRUE,
            FALSE,
            FALSE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fputs("FAIL: split-screen render accepted a mismatched native camera input slot\n",
            stderr);
        ++failures;
        SudekiMpUninstallSplitScreenRender();
    } else if (GetLastError() != ERROR_INVALID_DATA) {
        fprintf(stderr,
            "FAIL: native camera input slot mismatch returned error=%lu\n",
            (unsigned long)GetLastError());
        ++failures;
    }
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_CAMERA_INPUT_EVENT;
    if (!SudekiMpInputBridgeStart(0u, 100u)) {
        fprintf(stderr,
            "FAIL: controller-camera-only exact-image probe could not start input bridge (error=%lu)\n",
            (unsigned long)GetLastError());
        ++failures;
    } else {
        uint8_t saved_position_set_forward =
            image[RVA_POSITION_SET_FORWARD];

        *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
            image + RVA_QUICK_MENU_IS_ACTIVE;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                FALSE,
                TRUE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: manual controller camera accepted a mismatched input slot\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: manual controller-camera input-slot mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
            image + RVA_CAMERA_INPUT_EVENT;

        image[RVA_POSITION_SET_FORWARD] ^= 0xffu;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                FALSE,
                TRUE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: manual controller camera accepted a mismatched facing seam\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: manual controller-camera facing mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        image[RVA_POSITION_SET_FORWARD] = saved_position_set_forward;

        if (!SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                FALSE,
                TRUE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fprintf(stderr,
                "FAIL: manual controller-camera exact-image install rejected (error=%lu)\n",
                (unsigned long)GetLastError());
            ++failures;
        } else {
            if (*(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) ==
                    image + RVA_CAMERA_INPUT_EVENT) {
                fputs("FAIL: manual controller camera did not isolate P1 camera broadcasts\n",
                    stderr);
                ++failures;
            }
            SudekiMpUninstallSplitScreenRender();
            if (*(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
                    image + RVA_CAMERA_INPUT_EVENT) {
                fputs("FAIL: manual controller-camera input slot was not restored\n",
                    stderr);
                ++failures;
            }
        }
        SudekiMpInputBridgeStop();
    }
    {
        int32_t minimap_render_displacement;

        memcpy(
            &minimap_render_displacement,
            image + RVA_MINIMAP_RENDER_POINTER_CALL + 1u,
            sizeof(minimap_render_displacement)
        );
        image[RVA_MINIMAP_RENDER_POINTER_CALL + 1u] ^= 0xffu;
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                TRUE,
                FALSE,
                FALSE,
                TRUE,
                TRUE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: split-screen render accepted a mismatched minimap render seam\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        }
        if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) !=
                image + RVA_HUD_PARTY_POINTER_COPY ||
            *(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
                image + RVA_QUICK_MENU_INPUT ||
            relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
                image + RVA_QUICK_MENU_NATIVE_TOGGLE ||
            memcmp(
                image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
                minimap_snapshot_call_original,
                sizeof(minimap_snapshot_call_original)) != 0) {
            fputs("FAIL: minimap partial-install rollback retained an earlier hook\n",
                stderr);
            ++failures;
        }
        for (index = 0u;
             index < sizeof(quick_menu_owner_copy_call_rvas) /
                sizeof(quick_menu_owner_copy_call_rvas[0]);
             ++index) {
            if (relative_call_target(
                    image + quick_menu_owner_copy_call_rvas[index]) !=
                    image + RVA_HUD_PARTY_POINTER_COPY) {
                fprintf(stderr,
                    "FAIL: Quick Menu owner hook %lu survived minimap rollback\n",
                    (unsigned long)index);
                ++failures;
            }
        }
        memcpy(
            image + RVA_MINIMAP_RENDER_POINTER_CALL + 1u,
            &minimap_render_displacement,
            sizeof(minimap_render_displacement)
        );
        if (relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
            fputs("FAIL: minimap render seam was not restored after mismatch probe\n",
                stderr);
            ++failures;
        }
    }
    if (SudekiMpSplitScreenRosterSeatCapacity() != 0u ||
        !SudekiMpSplitScreenSetRosterTypes(
            SUDEKIMP_COOP_ROSTER_ACTOR_TAL,
            SUDEKIMP_COOP_ROSTER_ACTOR_AILISH) ||
        !SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type) ||
        roster_player_one_type != SUDEKIMP_COOP_ROSTER_ACTOR_TAL ||
        roster_player_two_type != SUDEKIMP_COOP_ROSTER_ACTOR_AILISH) {
        fputs("FAIL: preinstall co-op roster contract was not recorded\n",
            stderr);
            ++failures;
    }
    if (!SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            TRUE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fprintf(stderr, "split-screen render install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallControlSeparation();
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (SudekiMpSplitScreenRosterSeatCapacity() != 2u ||
        !SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type) ||
        roster_player_one_type != SUDEKIMP_COOP_ROSTER_ACTOR_TAL ||
        roster_player_two_type != SUDEKIMP_COOP_ROSTER_ACTOR_AILISH) {
        fputs("FAIL: split-screen install discarded the preinstalled co-op roster contract\n",
            stderr);
        ++failures;
    }
    ZeroMemory(&three_seat_assignment, sizeof(three_seat_assignment));
    three_seat_assignment.active_human_mask = 0x07u;
    three_seat_assignment.actor_type_by_seat[0] =
        SUDEKIMP_COOP_ROSTER_ACTOR_TAL;
    three_seat_assignment.actor_type_by_seat[1] =
        SUDEKIMP_COOP_ROSTER_ACTOR_AILISH;
    three_seat_assignment.actor_type_by_seat[2] =
        SUDEKIMP_COOP_ROSTER_ACTOR_BUKI;
    if (SudekiMpSplitScreenSetRosterAssignment(&three_seat_assignment) ||
        !SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type) ||
        roster_player_one_type != SUDEKIMP_COOP_ROSTER_ACTOR_TAL ||
        roster_player_two_type != SUDEKIMP_COOP_ROSTER_ACTOR_AILISH) {
        fputs("FAIL: capacity-two renderer accepted 0x07 roster or mutated prior assignment\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenRosterParticipationAvailable() ||
        !SudekiMpSplitScreenRosterParticipationRequested()) {
        fputs("FAIL: installed roster did not begin as available and joined\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenRequestRosterParticipation(FALSE) ||
        SudekiMpSplitScreenRosterParticipationRequested() ||
        SudekiMpSplitScreenRuntimeEnabled()) {
        fputs("FAIL: roster drop-out did not preserve a disabled participation/runtime state\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenRequestRosterParticipation(TRUE) ||
        !SudekiMpSplitScreenRosterParticipationRequested() ||
        !SudekiMpSplitScreenBeginPartyTransition()) {
        fputs("FAIL: roster drop-in or transition quarantine could not begin\n",
            stderr);
        ++failures;
    } else {
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpSplitScreenRequestRosterParticipation(FALSE) ||
            GetLastError() != ERROR_BUSY) {
            fputs("FAIL: roster participation changed inside a party transition\n",
                stderr);
            ++failures;
        }
        SudekiMpSplitScreenEndPartyTransition(TRUE);
        if (!SudekiMpSplitScreenRosterParticipationRequested()) {
            fputs("FAIL: successful party transition discarded the joined state\n",
                stderr);
            ++failures;
        }
    }
    if (!SudekiMpSplitScreenBeginPartyTransition()) {
        fputs("FAIL: second party transition quarantine could not begin\n",
            stderr);
        ++failures;
    } else {
        SudekiMpSplitScreenEndPartyTransition(FALSE);
        if (SudekiMpSplitScreenRosterParticipationRequested()) {
            fputs("FAIL: failed party placement did not leave Player 2 dropped out\n",
                stderr);
            ++failures;
        }
    }
    if (!SudekiMpSplitScreenRequestRosterParticipation(TRUE) ||
        !SudekiMpSplitScreenSetRuntimeEnabled(TRUE)) {
        fputs("FAIL: roster could not request drop-in after transition failure\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenClearRosterTypes() ||
        SudekiMpSplitScreenRosterParticipationAvailable() ||
        SudekiMpSplitScreenRosterParticipationRequested() ||
        SudekiMpSplitScreenRuntimeEnabled() ||
        SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type)) {
        fputs("FAIL: Single Player roster clear retained a multiplayer contract or runtime\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenSetRosterTypes(
            SUDEKIMP_COOP_ROSTER_ACTOR_TAL,
            SUDEKIMP_COOP_ROSTER_ACTOR_AILISH) ||
        !SudekiMpSplitScreenSetRuntimeEnabled(TRUE)) {
        fputs("FAIL: co-op roster could not be republished after Single Player clear\n",
            stderr);
        ++failures;
    }
    if (image[RVA_CAMERA_MANAGER_SET_RENDER_CAMERA] != 0xe9) {
        fputs("FAIL: SetRenderCamera inline hook was not installed\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) ==
            image + RVA_MOTION_BLUR_POST_RENDER ||
        *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) ==
            image + RVA_SCREENSHOT_POST_RENDER) {
        fputs("FAIL: Spirit viewport effect callbacks were not redirected\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) ==
            image + RVA_QUICK_MENU_RENDER_SUBMIT) {
        fputs("FAIL: Quick Menu render-submit vtable slot was not redirected\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) ==
            image + RVA_QUICK_MENU_INPUT ||
        relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) ==
            image + RVA_QUICK_MENU_NATIVE_TOGGLE) {
        fputs("FAIL: seat-owned Quick Menu input/toggle seams were not redirected\n",
            stderr);
        ++failures;
    }
    for (index = 0u;
         index < sizeof(quick_menu_owner_copy_call_rvas) /
            sizeof(quick_menu_owner_copy_call_rvas[0]);
         ++index) {
        if (relative_call_target(
                image + quick_menu_owner_copy_call_rvas[index]) ==
                image + RVA_HUD_PARTY_POINTER_COPY) {
            fprintf(stderr,
                "FAIL: Quick Menu owner-copy call %lu was not redirected\n",
                (unsigned long)index);
            ++failures;
        }
    }
    {
        union {
            void *alignment;
            uint8_t bytes[0x2au];
        } quick_menu_visible_edge_fake;
        void *saved_quick_menu =
            *(void **)(image + RVA_QUICK_MENU_GLOBAL);

        ZeroMemory(
            &quick_menu_visible_edge_fake,
            sizeof(quick_menu_visible_edge_fake)
        );
        *(void **)quick_menu_visible_edge_fake.bytes =
            image + RVA_QUICK_MENU_VTABLE;
        *(void **)(image + RVA_QUICK_MENU_GLOBAL) =
            quick_menu_visible_edge_fake.bytes;
        quick_menu_visible_edge_fake.bytes[QUICK_MENU_ACTIVE_OFFSET] = 1u;
        if (!SudekiMpSplitScreenQuickMenuAnyActive()) {
            fputs("FAIL: native Quick Menu visible edge was not modal before owner capture\n",
                stderr);
            ++failures;
        }
        quick_menu_visible_edge_fake.bytes[QUICK_MENU_ACTIVE_OFFSET] = 0u;
        if (SudekiMpSplitScreenQuickMenuAnyActive()) {
            fputs("FAIL: inactive native Quick Menu edge remained globally modal\n",
                stderr);
            ++failures;
        }
        *(void **)(image + RVA_QUICK_MENU_GLOBAL) = saved_quick_menu;
    }
    if (SudekiMpSplitScreenQuickMenuRequest(2u) ||
        SudekiMpSplitScreenQuickMenuRequest(3u) ||
        SudekiMpSplitScreenQuickMenuActive(2u) ||
        SudekiMpSplitScreenQuickMenuActive(3u) ||
        SudekiMpSplitScreenQuickMenuSubmit(
            2u, SUDEKIMP_QUICK_MENU_ACTION_CONFIRM) ||
        SudekiMpSplitScreenQuickMenuSubmit(
            3u, SUDEKIMP_QUICK_MENU_ACTION_CANCEL) ||
        SudekiMpSplitScreenQuickMenuSubmit(
            1u, SUDEKIMP_QUICK_MENU_ACTION_SECONDARY)) {
        fputs("FAIL: unsupported Quick Menu seat/action did not fail closed\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpInstallPlayerInputTrace((HMODULE)image)) {
        fprintf(stderr, "player-input trace install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSplitScreenRender();
        SudekiMpUninstallControlSeparation();
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) ==
        image + RVA_CHARACTER_INPUT_HANDLER) {
        fputs("FAIL: character input vtable slot was not redirected\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) ==
        image + RVA_CONTROLLER_UPDATE) {
        fputs("FAIL: controller update vtable slot was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) ==
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) ==
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: one or more player movement calls were not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_MAIN_FRAME_UPDATE_CALL) ==
        image + RVA_MAIN_FRAME_UPDATE) {
        fputs("FAIL: main-frame input poll call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_FRAME_END_CALL_MAIN) ==
            image + RVA_FRAME_END) {
        fputs("FAIL: gameplay-gated frame-end compositor was not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_START_CALL_MAIN) ==
            image + RVA_RENDER_START) {
        fputs("FAIL: render-only camera start call was not redirected\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) ==
            image + RVA_CAMERA_INPUT_EVENT) {
        fputs("FAIL: Player 2 native camera input gate was not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) ==
            image + RVA_PC_QUIT_SCREEN_RENDER) {
        fputs("FAIL: pre-Quit cached-backdrop call was not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_HUD_GROUP_VALUES_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_VALUES_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_NAME_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_STATUS_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: one or more viewport HUD ownership calls were not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        memcmp(
            image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
            minimap_snapshot_call_original,
            sizeof(minimap_snapshot_call_original)) != 0) {
        fputs("FAIL: viewport minimap hooks or native last-cluster snapshot ownership mismatch\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT_CALL) !=
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT) {
        fputs("FAIL: one or more native portrait-refresh calls were unexpectedly redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_PHASE_CALL_MAIN) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_FIRST_PHASE_CALL_MAIN) !=
            image + RVA_RENDER_FIRST_PHASE) {
        fputs("FAIL: native primary render sequence was redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_PREPASS) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_OFFSET) !=
            image + RVA_RENDER_PHASE) {
        fputs("FAIL: one or more world subpass calls were redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_UI_SCENE_RENDER_CALL) !=
            image + RVA_UI_SCENE_RENDER) {
        fputs("FAIL: native shared UI queue drain was redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_SKILL_ACTION_CALL) ==
        image + RVA_QUICK_SKILL_ACTION) {
        fputs("FAIL: QuickSkill action call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_SKILL_VALIDATE_CALL) ==
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_VALIDATE_CALL) ==
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_USE_INTERNAL_VALIDATE_CALL) ==
        image + RVA_SKILL_VALIDATE) {
        fputs("FAIL: one or more skill-validator calls were not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL) ==
        image + RVA_SPIRIT_STRIKE_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL) ==
        image + RVA_SPIRIT_STRIKE_ACTIVATE) {
        fputs("FAIL: one or more Spirit Strike calls were not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_USE_CALL) == image + RVA_USE) {
        fputs("FAIL: Use call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_DIRECT_USE_CALL) == image + RVA_USE) {
        fputs("FAIL: direct Use call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_STOP_RUMBLE_CALL) ==
        image + RVA_STOP_RUMBLE) {
        fputs("FAIL: StopRumble call was not redirected\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) ==
        image + RVA_SCRIPT_CALL_OPCODE) {
        fputs("FAIL: script-call opcode slot was not redirected\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) ==
        image + RVA_SCRIPT_METHOD_OPCODE) {
        fputs("FAIL: script-method opcode slot was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_SCRIPT_METHOD_BINDING_CALL) ==
        image + RVA_SCRIPT_BINDING_INVOKE) {
        fputs("FAIL: script binding invoke call was not redirected\n", stderr);
        ++failures;
    }
    for (index = 0; index < sizeof(expected_exports) / sizeof(expected_exports[0]);
            ++index) {
        if (*(const uint32_t *)(image + expected_exports[index].slot_rva) ==
            expected_exports[index].function_rva) {
            fprintf(stderr, "FAIL: export %lu was not redirected\n",
                (unsigned long)index);
            ++failures;
        }
    }

    SudekiMpUninstallPlayerInputTrace();
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) !=
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: one or more player movement calls were not restored\n",
            stderr);
        ++failures;
    }
    SudekiMpUninstallSplitScreenRender();
    if (SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type)) {
        fputs("FAIL: split-screen uninstall retained the co-op roster contract\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) !=
            image + RVA_QUICK_MENU_RENDER_SUBMIT) {
        fputs("FAIL: Quick Menu render-submit vtable slot was not restored\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
            image + RVA_QUICK_MENU_INPUT ||
        relative_call_target(image + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL) !=
            image + RVA_QUICK_MENU_NATIVE_TOGGLE) {
        fputs("FAIL: seat-owned Quick Menu input/toggle seams were not restored\n",
            stderr);
        ++failures;
    }
    for (index = 0u;
         index < sizeof(quick_menu_owner_copy_call_rvas) /
            sizeof(quick_menu_owner_copy_call_rvas[0]);
         ++index) {
        if (relative_call_target(
                image + quick_menu_owner_copy_call_rvas[index]) !=
                image + RVA_HUD_PARTY_POINTER_COPY) {
            fprintf(stderr,
                "FAIL: Quick Menu owner-copy call %lu was not restored\n",
                (unsigned long)index);
            ++failures;
        }
    }
    if (*(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) !=
            image + RVA_MOTION_BLUR_POST_RENDER ||
        *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) !=
            image + RVA_SCREENSHOT_POST_RENDER) {
        fputs("FAIL: Spirit viewport effect callbacks were not restored\n",
            stderr);
        ++failures;
    }
    if (memcmp(
            image + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            set_render_camera_original,
            sizeof(set_render_camera_original)) != 0) {
        fputs("FAIL: SetRenderCamera inline hook was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_FRAME_END_CALL_MAIN) !=
            image + RVA_FRAME_END) {
        fputs("FAIL: frame-end compositor call was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_START_CALL_MAIN) !=
            image + RVA_RENDER_START) {
        fputs("FAIL: render-only camera start call was not restored\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
            image + RVA_CAMERA_INPUT_EVENT) {
        fputs("FAIL: Player 2 native camera input gate was not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) !=
            image + RVA_PC_QUIT_SCREEN_RENDER) {
        fputs("FAIL: pre-Quit cached-backdrop call was not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_HUD_GROUP_VALUES_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT_CALL) !=
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT ||
        relative_call_target(image + RVA_HUD_GIZMO_VALUES_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_NAME_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_STATUS_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: one or more viewport HUD ownership calls were not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        memcmp(
            image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
            minimap_snapshot_call_original,
            sizeof(minimap_snapshot_call_original)) != 0) {
        fputs("FAIL: one or more viewport minimap ownership calls were not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_PREPASS) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_OFFSET) !=
            image + RVA_RENDER_PHASE) {
        fputs("FAIL: one or more render-phase calls were not restored\n",
            stderr);
        ++failures;
    }
    SudekiMpUninstallControlSeparation();
    if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
        image + RVA_CONTROLLER_UPDATE) {
        fputs("FAIL: controller update vtable slot was not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallTalosDefenseTrace();
    if (memcmp(image + RVA_APPLY_DAMAGE,
            "\x55\x8b\xec\x83\xe4\xf8", 6u) != 0 ||
        memcmp(image + RVA_COLLISION_DAMAGE,
            "\x83\xec\x78\x53\x55", 5u) != 0) {
        fputs("FAIL: Talos defense inline hooks were not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallCharacterSwitchTrace();
    if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
        image + RVA_CHARACTER_INPUT_HANDLER) {
        fputs("FAIL: character input vtable slot was not restored\n", stderr);
        ++failures;
    }
    if (!SudekiMpInstallFreeRoamCameraInput((HMODULE)image, VK_LCONTROL)) {
        fprintf(stderr, "free-roam camera install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        ++failures;
    } else {
        if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) ==
            image + RVA_CHARACTER_INPUT_HANDLER) {
            fputs("FAIL: free-roam camera input slot was not redirected\n",
                stderr);
            ++failures;
        }
        SudekiMpUninstallFreeRoamCameraInput();
        if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
            image + RVA_CHARACTER_INPUT_HANDLER) {
            fputs("FAIL: free-roam camera input slot was not restored\n",
                stderr);
            ++failures;
        }
    }
    SudekiMpUninstallSpiritStrikeInput();
    if (relative_call_target(image + RVA_MAIN_FRAME_UPDATE_CALL) !=
        image + RVA_MAIN_FRAME_UPDATE) {
        fputs("FAIL: main-frame input poll call was not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallQuickSkillInputTrace();
    if (relative_call_target(image + RVA_QUICK_SKILL_ACTION_CALL) !=
        image + RVA_QUICK_SKILL_ACTION ||
        relative_call_target(image + RVA_QUICK_SKILL_VALIDATE_CALL) !=
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_VALIDATE_CALL) !=
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_USE_INTERNAL_VALIDATE_CALL) !=
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL) !=
        image + RVA_SPIRIT_STRIKE_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL) !=
        image + RVA_SPIRIT_STRIKE_ACTIVATE) {
        fputs("FAIL: one or more QuickSkill hooks were not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallSkillTrace();
    SudekiMpResetSkillActivationAbi();
    SudekiMpResetSpiritActivationAbi();
    SudekiMpResetWeaponActivationAbi();
    SudekiMpResetItemActivationAbi();
    if (relative_call_target(image + RVA_USE_CALL) != image + RVA_USE) {
        fputs("FAIL: Use call was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_DIRECT_USE_CALL) != image + RVA_USE) {
        fputs("FAIL: direct Use call was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_STOP_RUMBLE_CALL) !=
        image + RVA_STOP_RUMBLE) {
        fputs("FAIL: StopRumble call was not restored\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) !=
        image + RVA_SCRIPT_CALL_OPCODE) {
        fputs("FAIL: script-call opcode slot was not restored\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) !=
        image + RVA_SCRIPT_METHOD_OPCODE) {
        fputs("FAIL: script-method opcode slot was not restored\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_SCENE_OPCODE_SLOT) !=
        image + RVA_SCRIPT_SCENE_OPCODE) {
        fputs("FAIL: script-scene opcode slot was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_SCRIPT_METHOD_BINDING_CALL) !=
        image + RVA_SCRIPT_BINDING_INVOKE) {
        fputs("FAIL: script binding invoke call was not restored\n", stderr);
        ++failures;
    }
    for (index = 0; index < sizeof(expected_exports) / sizeof(expected_exports[0]);
            ++index) {
        if (*(const uint32_t *)(image + expected_exports[index].slot_rva) !=
            expected_exports[index].function_rva) {
            fprintf(stderr, "FAIL: export %lu was not restored\n",
                (unsigned long)index);
            ++failures;
        }
    }

    VirtualFree(image, 0, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d image hook test(s) failed\n", failures);
        return 1;
    }
    puts("skill_trace_image_test: PASS");
    return 0;
}
