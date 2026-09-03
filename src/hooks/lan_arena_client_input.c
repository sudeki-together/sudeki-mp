#include "hooks/lan_arena_client_input.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_pause_panel.h"
#include "network/lan_arena_operator.h"
#include "network/lan_arena_session.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__stdcall *ArbiterMovementFunction)(
    void *arbiter, const float *direction, float speed, float turn_rate,
    uint32_t movement_mode
);
typedef void (__stdcall *ControllerCombatFunction)(void *controller);
#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#endif
typedef uint8_t (SUDEKIMP_THISCALL *QuickMenuInputFunction)(
    void *quick_menu,
    unsigned int event_kind,
    unsigned int command,
    unsigned int value
);
typedef void (SUDEKIMP_THISCALL *CameraInputEventFunction)(
    void *camera,
    const void *event
);
typedef void (SUDEKIMP_THISCALL *CharacterInputHandler)(
    void *listener,
    void *event
);

typedef struct LanArenaNativeCameraInputEvent {
    uint32_t action;
    uint32_t reserved_04;
    uint32_t reserved_08;
    float magnitude;
    uint32_t reserved_10;
    uint16_t owner;
    uint16_t reserved_16;
} LanArenaNativeCameraInputEvent;

enum {
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_CHARACTER_INPUT_HANDLER = 0x000277b0u,
    RVA_CHARACTER_INPUT_VTABLE_SLOT = 0x002c9f84u,
    RVA_CAMERA_INPUT_EVENT = 0x000e85f0u,
    RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT = 0x002cce5cu,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_CONTROLLER_COMBAT = 0x000286c0u,
    RVA_CONTROLLER_AIM_UPDATE = 0x00028b00u,
    RVA_QUICK_MENU_NATIVE_TOGGLE = 0x0000a080u,
    RVA_QUICK_MENU_NATIVE_TOGGLE_CALL = 0x00028228u,
    RVA_QUICK_MENU_INPUT = 0x00098b40u,
    RVA_QUICK_MENU_INPUT_VTABLE_SLOT = 0x002caf48u,
    RVA_QUICK_MENU_GLOBAL = 0x003c2f84u,
    RVA_QUICK_MENU_VTABLE = 0x002caf1cu,
    CHARACTER_ARBITER_OWNER_OFFSET = 0x10u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    CONTROLLER_WEAK_OFFSET = 0x8cu,
    CONTROLLER_STRONG_OFFSET = 0x94u,
    CONTROLLER_SWEEP_OFFSET = 0x9cu,
    CONTROLLER_BLOCK_OFFSET = 0xa4u,
    CONTROLLER_WEAPON_NEXT_OFFSET = 0xacu,
    CONTROLLER_WEAPON_PREVIOUS_OFFSET = 0xb4u,
    CONTROLLER_MOVE_X_OFFSET = 0x1a0u,
    CONTROLLER_MOVE_Y_OFFSET = 0x1a4u,
    CONTROLLER_FIRST_PERSON_AIM_X_OFFSET = 0x1b0u,
    CONTROLLER_FIRST_PERSON_AIM_Y_OFFSET = 0x1b4u,
    QUICK_MENU_INPUT_EVENT_DOWN = 5u,
    QUICK_MENU_INPUT_EVENT_UP = 6u,
    QUICK_MENU_INPUT_EVENT_POINTER = 0x19u,
    QUICK_MENU_COMMAND_CONFIRM = 0u,
    QUICK_MENU_COMMAND_SECONDARY_CONFIRM = 2u,
    QUICK_MENU_ACTIVE_OFFSET = 0x29u,
    RANGED_FIRST_PERSON_ARBITER_FLAG = 0x00400000u,
    CAMERA_INPUT_ACTION_FIRST = 0x69u,
    CAMERA_INPUT_ACTION_LAST = 0x6cu
};

static const uint8_t expected_controller_combat_entry[] = {
    0x55u, 0x8bu, 0x6cu, 0x24u, 0x08u,
    0x83u, 0xbdu, 0x48u, 0x02u, 0x00u, 0x00u, 0x00u
};
static const uint8_t expected_controller_aim_update_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x34u, 0x53u, 0x8bu, 0x5du, 0x08u, 0x83u, 0xbbu, 0x48u,
    0x02u, 0x00u, 0x00u, 0x00u
};
static const uint8_t expected_quick_menu_native_toggle_entry[] = {
    0x80u, 0xb8u, 0x8cu, 0x00u, 0x00u,
    0x00u, 0x00u, 0x74u, 0x46u
};
static const uint8_t expected_quick_menu_input_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x55u, 0x56u, 0x57u, 0x8bu,
    0xe9u, 0x83u, 0xf8u, 0x19u
};
static const uint8_t expected_camera_input_event_entry[] = {
    0x51u, 0x56u, 0x8bu, 0xf1u, 0x8bu, 0x4eu, 0x38u, 0x33u,
    0xc0u, 0x85u, 0xc9u, 0x74u
};

static SudekiMpRelativeCallHook alternate_movement_hook;
static SudekiMpRelativeCallHook normal_movement_hook;
static SudekiMpInlineHook controller_combat_hook;
static SudekiMpPointerHook quick_menu_input_hook;
static SudekiMpPointerHook camera_input_event_hook;
static SudekiMpPointerHook character_input_hook;
static ArbiterMovementFunction original_arbiter_movement;
static ControllerCombatFunction original_controller_combat;
static QuickMenuInputFunction original_quick_menu_input;
static CameraInputEventFunction original_camera_input_event;
static CharacterInputHandler original_character_input_handler;
static int16_t last_direction_x;
static int16_t last_direction_z;
static int16_t last_aim_x;
static int16_t last_aim_y;
static int16_t last_aim_z;
static BOOL weak_was_down;
static BOOL movement_send_logged;
static BOOL quick_menu_action_block_logged;
static int quick_menu_visible_state;
static DWORD last_input_send_at;
static int16_t last_transmitted_direction_x;
static int16_t last_transmitted_direction_z;
static int16_t last_transmitted_aim_x;
static int16_t last_transmitted_aim_y;
static int16_t last_transmitted_aim_z;
static BOOL last_transmitted_weak_held;
static BOOL last_transmitted_first_person_active;
static BOOL native_weak_held;
static DWORD native_weak_sample_at_ms;
static BOOL combat_toggle_pending;
static BOOL combat_toggle_was_down;
static uint8_t *client_game_base;
static HANDLE operator_combat_toggle_event;
static HANDLE operator_weak_attack_event;
static HANDLE operator_weak_hold_event;
static HANDLE operator_camera_left_event;
static HANDLE operator_camera_right_event;
static DWORD operator_weak_attack_until_ms;
static DWORD operator_camera_until_ms;
static int operator_camera_direction;
static BOOL operator_camera_release_pending;
static BOOL operator_camera_submission_logged;
static DWORD last_native_camera_input_trace_at_ms;
static LanArenaNativeCameraInputEvent pending_character_camera_event;
static BOOL pending_character_camera_event_valid;
static DWORD pending_character_camera_event_at_ms;
static int client_camera_route_trace_state = -1;
static int client_first_person_aim_bridge_trace_state = -1;

enum {
    CLIENT_INPUT_SEND_INTERVAL_MS = 50u,
    CLIENT_NATIVE_INPUT_FRESH_MS = 125u
};

BOOL SudekiMpLanArenaClientNativeWeakHeld(int transition_state) {
    return transition_state == 1 || transition_state == 2;
}

int SudekiMpLanArenaClientSuppressedWeakNextState(int transition_state) {
    if (transition_state == 1 || transition_state == 2) return 2;
    return 0;
}

BOOL SudekiMpLanArenaClientRangedWeakHeld(
    BOOL first_person_active,
    BOOL raw_weak_held
) {
    return first_person_active && raw_weak_held;
}

static BOOL authenticated_client(void);
static BOOL readable_memory(const void *pointer, size_t length);
static BOOL client_ailish_first_person_active(void);

static uint8_t *active_native_camera(void) {
    uint8_t *mode;
    uint8_t *camera_member;
    if (client_game_base == NULL || !readable_memory(
            client_game_base + RVA_GAME_CAMERA_MODE_GLOBAL,
            sizeof(mode))) return NULL;
    mode = *(uint8_t **)(client_game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    if (!readable_memory(mode, 0x10u)) return NULL;
    camera_member = *(uint8_t **)(mode + 0x0cu);
    if ((uintptr_t)camera_member < 0x2cu ||
        !readable_memory(camera_member - 0x2cu, 0x108u)) return NULL;
    return camera_member - 0x2cu;
}

void SudekiMpLanArenaClientRequestCombatToggle(void) {
    if (authenticated_client()) combat_toggle_pending = TRUE;
}

static BOOL readable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    if (pointer == NULL || length == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u ||
        address + length < address ||
        address + length >
            (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    return TRUE;
}

/* The client may browse Ailish's own native full-screen QuickMenu, but it is
 * a presentation terminal rather than an action authority.  Consume both
 * native confirm commands until each category has a verified host-routed
 * adapter. Navigation, category changes, Q toggle, and close remain retail. */
static uint8_t SUDEKIMP_THISCALL route_client_quick_menu_input(
    void *quick_menu,
    unsigned int event_kind,
    unsigned int command,
    unsigned int value
) {
    BOOL action_event = event_kind == QUICK_MENU_INPUT_EVENT_DOWN ||
        event_kind == QUICK_MENU_INPUT_EVENT_UP ||
        event_kind == QUICK_MENU_INPUT_EVENT_POINTER;
    if (action_event &&
        (command == QUICK_MENU_COMMAND_CONFIRM ||
         command == QUICK_MENU_COMMAND_SECONDARY_CONFIRM)) {
        if (!quick_menu_action_block_logged) {
            quick_menu_action_block_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_client_input event=quick_menu_action "
                "phase=rejected reason=host_adapter_not_implemented "
                "policy=native_browse_and_close_allowed_execution_blocked\r\n");
        }
        return 1u;
    }
    return original_quick_menu_input == NULL ? 0u :
        original_quick_menu_input(
            quick_menu, event_kind, command, value);
}

static void SUDEKIMP_THISCALL route_client_camera_input_event(
    void *camera,
    const void *event_pointer
) {
    const LanArenaNativeCameraInputEvent *event =
        (const LanArenaNativeCameraInputEvent *)event_pointer;
    DWORD now = GetTickCount();
    if (authenticated_client() && readable_memory(event, sizeof(*event)) &&
        event->action >= 0x3fu && event->action <= 0x72u &&
        isfinite(event->magnitude) && event->magnitude != 0.0f &&
        (last_native_camera_input_trace_at_ms == 0u ||
         (DWORD)(now - last_native_camera_input_trace_at_ms) >= 50u)) {
        last_native_camera_input_trace_at_ms = now;
        SudekiMpLogFormat(
            "lan_arena_client_input event=native_camera_input "
            "action=0x%02lx magnitude=%.5f owner=%u camera=0x%08lx "
            "source=native_window_input\r\n",
            (unsigned long)event->action,
            event->magnitude,
            (unsigned int)event->owner,
            (unsigned long)(uintptr_t)camera);
    }
    if (pending_character_camera_event_valid &&
        readable_memory(event, sizeof(*event)) &&
        event->action == pending_character_camera_event.action) {
        pending_character_camera_event_valid = FALSE;
        if (client_camera_route_trace_state != 1) {
            client_camera_route_trace_state = 1;
            SudekiMpLogWrite(
                "lan_arena_client_input event=client_camera_route "
                "state=native action_source=character_input "
                "policy=no_duplicate_camera_submission\r\n");
        }
    }
    if (original_camera_input_event != NULL) {
        original_camera_input_event(camera, event_pointer);
    }
}

static void SUDEKIMP_THISCALL route_client_character_input(
    void *listener,
    void *event_pointer
) {
    const LanArenaNativeCameraInputEvent *event =
        (const LanArenaNativeCameraInputEvent *)event_pointer;
    uint8_t *controller = NULL;
    BOOL capture;
    if (client_game_base != NULL && readable_memory(
            client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        controller = *(uint8_t **)(
            client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    }
    /* The retail input dispatcher broadcasts the same mouse event through
     * several character-listener instances.  Only the global active
     * CCharacterController owns the local Ailish camera.  Arming the deferred
     * fallback for every listener let a later, unrelated listener overwrite
     * the valid event and produced the observed active/rejected route flap. */
    capture = authenticated_client() && listener == controller &&
        readable_memory(controller, 0x1b8u) &&
        client_ailish_first_person_active() &&
        readable_memory(event, sizeof(*event)) &&
        event->action >= CAMERA_INPUT_ACTION_FIRST &&
        event->action <= CAMERA_INPUT_ACTION_LAST &&
        isfinite(event->magnitude);
    if (capture) {
        pending_character_camera_event = *event;
        /* The active camera accepts owner zero as its native wildcard. The
         * client process has one local view, so normalize only the deferred
         * fallback copy; retail receives the unchanged event below. */
        pending_character_camera_event.owner = 0u;
        pending_character_camera_event_valid = TRUE;
        pending_character_camera_event_at_ms = GetTickCount();
    }
    /* Arm the fallback before entering retail. CCharacterController may route
     * the same event synchronously to CCamera; the camera hook can then clear
     * this exact pending copy and prove that no duplicate is needed. Arming
     * after retail returned left a one-event-behind duplicate alive. */
    if (original_character_input_handler != NULL) {
        original_character_input_handler(listener, event_pointer);
    }
    if (capture && (event->action == 0x69u || event->action == 0x6au)) {
        size_t aim_offset;
        aim_offset = event->action == 0x69u ?
            CONTROLLER_FIRST_PERSON_AIM_X_OFFSET :
            CONTROLLER_FIRST_PERSON_AIM_Y_OFFSET;
        /* Retail records mouse axes at +0x184/+0x188, while its verified
         * first-person aim update consumes +0x1b0/+0x1b4. The LAN client
         * keeps native local UI/camera but suppresses local arbiter
         * execution, so carry the same finite native event into the
         * proven aim-only input field. The ordinary controller update
         * performs all yaw/pitch limits and ranged-component writes. */
        *(float *)(controller + aim_offset) = event->magnitude;
        if (client_first_person_aim_bridge_trace_state != 1) {
            client_first_person_aim_bridge_trace_state = 1;
            SudekiMpLogFormat(
                "lan_arena_client_input event=first_person_aim_bridge "
                "state=active action=0x%02lx field=0x%03lx "
                "listener=active_character_controller "
                "policy=native_mouse_event_to_verified_controller_aim_axis\r\n",
                (unsigned long)event->action,
                (unsigned long)aim_offset);
        }
    }
}

static void service_pending_character_camera_input(void) {
    uint8_t *camera;
    DWORD now;
    LanArenaNativeCameraInputEvent event;
    if (!pending_character_camera_event_valid) return;
    now = GetTickCount();
    /* Give Sudeki the rest of the controller tick to route the event to
     * CCamera itself.  The next observer pass supplies a fallback only when
     * that native route never arrived, which is the LAN-client failure seen
     * with first-person mouse input. */
    if ((DWORD)(now - pending_character_camera_event_at_ms) < 8u) return;
    if (!authenticated_client() ||
        !client_ailish_first_person_active()) {
        pending_character_camera_event_valid = FALSE;
        return;
    }
    camera = active_native_camera();
    if (camera == NULL || original_camera_input_event == NULL) return;
    event = pending_character_camera_event;
    pending_character_camera_event_valid = FALSE;
    original_camera_input_event(camera, &event);
    if (client_camera_route_trace_state != 2) {
        client_camera_route_trace_state = 2;
        SudekiMpLogFormat(
            "lan_arena_client_input event=client_camera_route "
            "state=fallback action=0x%02lx magnitude=%.5f camera=0x%08lx "
            "policy=deferred_only_when_native_camera_route_missing\r\n",
            (unsigned long)event.action, event.magnitude,
            (unsigned long)(uintptr_t)camera);
    }
}

static BOOL authenticated_client(void) {
    SudekiMpLanArenaSessionStatus status;
    return SudekiMpLanArenaSessionGetStatus(&status) && status.peer_connected &&
        status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
}

static void service_operator_camera_input(void) {
    BOOL left_requested = operator_camera_left_event != NULL &&
        WaitForSingleObject(operator_camera_left_event, 0u) == WAIT_OBJECT_0;
    BOOL right_requested = operator_camera_right_event != NULL &&
        WaitForSingleObject(operator_camera_right_event, 0u) == WAIT_OBJECT_0;
    uint8_t *camera;
    uint8_t *controller;
    LanArenaNativeCameraInputEvent event;
    DWORD now = GetTickCount();
    float before_64;
    float before_70;
    float before_74;
    float before_9c;
    float before_a0;
    float before_a4;
    float before_ac;
    float after_64;
    float after_70;
    float after_74;
    float after_9c;
    float after_a0;
    float after_a4;
    float after_ac;
    float before_controller_mouse_x;
    float before_controller_mouse_y;
    float before_controller_aim_x;
    float before_controller_aim_y;
    float after_controller_mouse_x;
    float after_controller_mouse_y;
    float after_controller_aim_x;
    float after_controller_aim_y;
    if (left_requested || right_requested) {
        if (left_requested == right_requested) {
            operator_camera_direction = 0;
            operator_camera_until_ms = 0u;
            SudekiMpLogWrite(
                "lan_arena_client_input event=operator_camera "
                "phase=rejected reason=conflicting_command\r\n");
            return;
        }
        operator_camera_direction = right_requested ? 1 : -1;
        operator_camera_until_ms = now + 500u;
        operator_camera_release_pending = TRUE;
        operator_camera_submission_logged = FALSE;
        SudekiMpLogFormat(
            "lan_arena_client_input event=operator_camera phase=requested "
            "direction=%s duration_ms=500 policy=bounded_native_input_pulse\r\n",
            operator_camera_direction > 0 ? "right" : "left");
    }
    if (operator_camera_direction == 0 ||
        (LONG)(operator_camera_until_ms - now) <= 0) {
        if (operator_camera_release_pending && client_game_base != NULL &&
            original_character_input_handler != NULL && readable_memory(
                client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
                sizeof(controller))) {
            controller = *(uint8_t **)(
                client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
            if (readable_memory(controller, 0x1b8u)) {
                ZeroMemory(&event, sizeof(event));
                event.action = 0x69u;
                route_client_character_input(controller, &event);
                SudekiMpLogWrite(
                    "lan_arena_client_input event=operator_camera "
                    "phase=released action=0x69 magnitude=0 "
                    "policy=native_character_input_release_edge\r\n");
            }
        }
        operator_camera_release_pending = FALSE;
        operator_camera_direction = 0;
        operator_camera_until_ms = 0u;
        return;
    }
    if (client_game_base == NULL || original_camera_input_event == NULL) {
        SudekiMpLogWrite(
            "lan_arena_client_input event=operator_camera "
            "phase=rejected reason=camera_not_ready_or_conflicting_command\r\n");
        return;
    }
    camera = active_native_camera();
    if (camera == NULL) {
        SudekiMpLogWrite(
            "lan_arena_client_input event=operator_camera "
            "phase=rejected reason=active_camera_unavailable\r\n");
        return;
    }
    if (!readable_memory(
            client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) return;
    controller = *(uint8_t **)(
        client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    if (!readable_memory(controller, 0x1b8u) ||
        original_character_input_handler == NULL) return;
    before_64 = *(float *)(camera + 0x64u);
    before_70 = *(float *)(camera + 0x70u);
    before_74 = *(float *)(camera + 0x74u);
    before_9c = *(float *)(camera + 0x9cu);
    before_a0 = *(float *)(camera + 0xa0u);
    before_a4 = *(float *)(camera + 0xa4u);
    before_ac = *(float *)(camera + 0xacu);
    before_controller_mouse_x = *(float *)(controller + 0x184u);
    before_controller_mouse_y = *(float *)(controller + 0x188u);
    before_controller_aim_x = *(float *)(controller + 0x1b0u);
    before_controller_aim_y = *(float *)(controller + 0x1b4u);
    ZeroMemory(&event, sizeof(event));
    /* The supported PC build normalizes horizontal mouse motion to action
     * 0x69 before CCamera::InputEvent.  This is also the exact action used by
     * the already-proven split-screen controller-camera adapter. */
    event.action = 0x69u;
    event.magnitude = operator_camera_direction > 0 ? 4.0f : -4.0f;
    /* Enter through the native character listener, exactly like real mouse
     * input. In first-person, the controller consumes its own aim fields;
     * calling CCamera directly only exercises third-person orbit. The guarded
     * character hook supplies the camera fallback when retail omits it. */
    route_client_character_input(controller, &event);
    after_64 = *(float *)(camera + 0x64u);
    after_70 = *(float *)(camera + 0x70u);
    after_74 = *(float *)(camera + 0x74u);
    after_9c = *(float *)(camera + 0x9cu);
    after_a0 = *(float *)(camera + 0xa0u);
    after_a4 = *(float *)(camera + 0xa4u);
    after_ac = *(float *)(camera + 0xacu);
    after_controller_mouse_x = *(float *)(controller + 0x184u);
    after_controller_mouse_y = *(float *)(controller + 0x188u);
    after_controller_aim_x = *(float *)(controller + 0x1b0u);
    after_controller_aim_y = *(float *)(controller + 0x1b4u);
    if (!operator_camera_submission_logged) {
        operator_camera_submission_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_input event=operator_camera phase=submitted "
            "action=0x%02lx magnitude=%.5f owner=0 camera=0x%08lx "
            "enabled=%u,%u mode=%lu "
            "fields_64_70_74_before=%.5f,%.5f,%.5f "
            "fields_64_70_74_after=%.5f,%.5f,%.5f "
            "owner_fields_before=%.5f,%.5f,%.5f,%.5f "
            "owner_fields_after=%.5f,%.5f,%.5f,%.5f "
            "controller_mouse_before=%.5f,%.5f after=%.5f,%.5f "
            "controller_aim_before=%.5f,%.5f after=%.5f,%.5f "
            "policy=repeated_native_character_input_event_for_500ms\r\n",
            (unsigned long)event.action, event.magnitude,
            (unsigned long)(uintptr_t)camera,
            (unsigned int)camera[0x106u],
            (unsigned int)camera[0x107u],
            (unsigned long)*(uint32_t *)(camera + 0x80u),
            before_64, before_70, before_74,
            after_64, after_70, after_74,
            before_9c, before_a0, before_a4, before_ac,
            after_9c, after_a0, after_a4, after_ac,
            before_controller_mouse_x, before_controller_mouse_y,
            after_controller_mouse_x, after_controller_mouse_y,
            before_controller_aim_x, before_controller_aim_y,
            after_controller_aim_x, after_controller_aim_y);
    }
}

static BOOL client_quick_menu_visible(void) {
    uint8_t *menu;
    if (client_game_base == NULL || !readable_memory(
            client_game_base + RVA_QUICK_MENU_GLOBAL, sizeof(menu))) {
        return FALSE;
    }
    menu = *(uint8_t **)(client_game_base + RVA_QUICK_MENU_GLOBAL);
    return readable_memory(menu, QUICK_MENU_ACTIVE_OFFSET + 1u) &&
        *(void **)menu == client_game_base + RVA_QUICK_MENU_VTABLE &&
        menu[QUICK_MENU_ACTIVE_OFFSET] != 0u;
}

static BOOL client_local_modal_active(void) {
    return SudekiMpLanArenaPausePanelActive() || client_quick_menu_visible();
}

static int16_t normalized_axis(float value) {
    if (!isfinite(value)) return 0;
    if (value >= 1.0f) return 32767;
    if (value <= -1.0f) return -32767;
    return (int16_t)(value * 32767.0f);
}

static void refresh_client_camera_aim(void) {
    uint8_t *mode;
    uint8_t *camera_member;
    uint8_t *camera;
    uint8_t *render_state;
    const float *matrix;
    float x;
    float y;
    float z;
    float length;
    last_aim_x = 0;
    last_aim_y = 0;
    last_aim_z = 0;
    if (client_game_base == NULL || !readable_memory(
            client_game_base + RVA_GAME_CAMERA_MODE_GLOBAL,
            sizeof(mode))) return;
    mode = *(uint8_t **)(client_game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    if (!readable_memory(mode, 0x10u)) return;
    camera_member = *(uint8_t **)(mode + 0x0cu);
    if ((uintptr_t)camera_member < 0x2cu) return;
    camera = camera_member - 0x2cu;
    if (!readable_memory(camera, 0x38u)) return;
    render_state = *(uint8_t **)(camera + 0x34u);
    if (!readable_memory(render_state, 0xd0u)) return;
    matrix = (const float *)(render_state + 0x90u);
    x = matrix[8];
    y = matrix[9];
    z = matrix[10];
    length = sqrtf(x * x + y * y + z * z);
    if (!isfinite(length) || length < 0.0001f) return;
    last_aim_x = normalized_axis(x / length);
    last_aim_y = normalized_axis(y / length);
    last_aim_z = normalized_axis(z / length);
}

static BOOL client_ailish_first_person_active(void) {
    uint8_t *character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
        SUDEKIMP_CLEANROOM_AILISH);
    uint8_t *arbiter;
    if (!readable_memory(character, 0x94u)) return FALSE;
    arbiter = *(uint8_t **)(character + 0x90u);
    return readable_memory(arbiter, 0x54u) &&
        *(void **)(arbiter + CHARACTER_ARBITER_OWNER_OFFSET) == character &&
        (*(uint32_t *)(arbiter + 0x50u) &
         RANGED_FIRST_PERSON_ARBITER_FLAG) != 0u;
}

static BOOL send_client_input(
    int16_t direction_x,
    int16_t direction_z,
    BOOL weak_pressed,
    BOOL weak_held
) {
    SudekiMpLanArenaInput input;
    refresh_client_camera_aim();
    ZeroMemory(&input, sizeof(input));
    input.sequence = 0u;
    input.acknowledged_snapshot = 0u;
    input.client_tick = GetTickCount();
    input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    input.world_direction_x = direction_x;
    input.world_direction_z = direction_z;
    input.aim_direction_x = last_aim_x;
    input.aim_direction_y = last_aim_y;
    input.aim_direction_z = last_aim_z;
    input.weak_attack_pressed = weak_pressed ? 1u : 0u;
    input.weak_attack_held = weak_held ? 1u : 0u;
    input.ranged_first_person_active =
        client_ailish_first_person_active() ? 1u : 0u;
    input.cleanroom_combat_test_pressed =
        combat_toggle_pending ? 1u : 0u;
    if (!SudekiMpLanArenaSessionSendInput(&input)) return FALSE;
    combat_toggle_pending = FALSE;
    return TRUE;
}

static BOOL send_client_input_at(
    int16_t direction_x,
    int16_t direction_z,
    BOOL weak_pressed,
    BOOL weak_held,
    DWORD now
) {
    if (!send_client_input(
            direction_x, direction_z, weak_pressed, weak_held)) return FALSE;
    last_input_send_at = now;
    last_transmitted_direction_x = direction_x;
    last_transmitted_direction_z = direction_z;
    last_transmitted_aim_x = last_aim_x;
    last_transmitted_aim_y = last_aim_y;
    last_transmitted_aim_z = last_aim_z;
    last_transmitted_weak_held = weak_held != FALSE;
    last_transmitted_first_person_active =
        client_ailish_first_person_active();
    return TRUE;
}

static void __stdcall capture_client_movement(
    void *arbiter,
    const float *direction,
    float speed,
    float turn_rate,
    uint32_t movement_mode
) {
    DWORD now;
    int16_t direction_x;
    int16_t direction_z;
    BOOL changed;
    void *character = arbiter == NULL ? NULL :
        *(void **)((uint8_t *)arbiter + CHARACTER_ARBITER_OWNER_OFFSET);
    void *ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    (void)turn_rate;
    (void)movement_mode;
    if (!authenticated_client() || direction == NULL || character == NULL ||
        character != ailish) {
        original_arbiter_movement(arbiter, direction, speed, turn_rate, movement_mode);
        return;
    }
    if (client_local_modal_active()) {
        now = GetTickCount();
        last_direction_x = 0;
        last_direction_z = 0;
        if (last_transmitted_direction_x != 0 ||
            last_transmitted_direction_z != 0 || last_input_send_at == 0u) {
            (void)send_client_input_at(0, 0, FALSE, FALSE, now);
        }
        return;
    }
    if (!isfinite(speed) || speed <= 0.0f) speed = 0.0f;
    if (speed > 1.0f) speed = 1.0f;
    now = GetTickCount();
    /* This direction is already the retail controller's live camera-relative
     * world vector. Preserve it exactly: recomputing or latching it in the LAN
     * layer breaks native mouse-steered arcs around actors and scenery. */
    direction_x = normalized_axis(direction[0] * speed);
    direction_z = normalized_axis(direction[2] * speed);
    changed = direction_x != last_direction_x || direction_z != last_direction_z;
    last_direction_x = direction_x;
    last_direction_z = direction_z;
    if ((changed || last_input_send_at == 0u ||
         (DWORD)(now - last_input_send_at) >= CLIENT_INPUT_SEND_INTERVAL_MS) &&
        send_client_input_at(
            last_direction_x, last_direction_z,
            FALSE, weak_was_down, now) &&
        !movement_send_logged &&
        (last_direction_x != 0 || last_direction_z != 0)) {
        movement_send_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_input event=movement_send phase=confirmed "
            "world_direction=%d,%d policy=native_live_camera_relative_vector\r\n",
            (int)last_direction_x, (int)last_direction_z);
    }
    /* Do not call the local native arbiter. The shared simulation owns actor
     * movement/action outcomes; this process only keeps Ailish's camera/UI.
     * Sudeki's native world trigger remains the source of combat-mode state. */
}

void SudekiMpLanArenaClientInputService(void) {
    DWORD now;
    uint8_t *controller;
    void *ailish;
    float raw_x;
    float raw_y;
    BOOL local_direction_held;
    BOOL local_weak_held;
    BOOL weak_pressed;
    BOOL weak_changed;
    int16_t desired_x;
    int16_t desired_z;
    BOOL aim_changed;
    BOOL first_person_active;
    BOOL combat_toggle_down =
        (GetKeyState(VK_F8) & (SHORT)0x8000) != 0;
    BOOL cleanroom_combat_test_pressed =
        combat_toggle_down && !combat_toggle_was_down;
    BOOL operator_toggle_requested = operator_combat_toggle_event != NULL &&
        WaitForSingleObject(operator_combat_toggle_event, 0u) == WAIT_OBJECT_0;
    BOOL operator_weak_requested = operator_weak_attack_event != NULL &&
        WaitForSingleObject(operator_weak_attack_event, 0u) == WAIT_OBJECT_0;
    BOOL operator_weak_held = operator_weak_hold_event != NULL &&
        WaitForSingleObject(operator_weak_hold_event, 0u) == WAIT_OBJECT_0;
    combat_toggle_was_down = combat_toggle_down;
    if (!authenticated_client()) {
        if (operator_toggle_requested || operator_weak_requested) {
            SudekiMpLogWrite(
                "lan_arena_client_input event=operator_action phase=rejected "
                "reason=session_not_authenticated\r\n");
        }
        return;
    }
    if (cleanroom_combat_test_pressed || operator_toggle_requested) {
        combat_toggle_pending = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_input event=cleanroom_combat_test phase=requested "
            "source=%s policy=test_edge_only_native_world_flag_replicated\r\n",
            operator_toggle_requested ? "local_operator_api" : "native_F8");
    }
    now = GetTickCount();
    {
        BOOL visible = client_quick_menu_visible();
        if ((int)visible != quick_menu_visible_state) {
            quick_menu_visible_state = (int)visible;
            if (!visible) quick_menu_action_block_logged = FALSE;
            SudekiMpLogFormat(
                "lan_arena_client_input event=native_quick_menu state=%s "
                "authority=browse_only local_gameplay=quiesced\r\n",
                visible ? "open" : "closed");
        }
    }
    if (client_local_modal_active()) {
        pending_character_camera_event_valid = FALSE;
        last_direction_x = 0;
        last_direction_z = 0;
        weak_was_down = FALSE;
        refresh_client_camera_aim();
        if (last_transmitted_direction_x != 0 ||
            last_transmitted_direction_z != 0 ||
            last_transmitted_weak_held || combat_toggle_pending ||
            last_input_send_at == 0u) {
            (void)send_client_input_at(0, 0, FALSE, FALSE, now);
        }
        return;
    }
    service_pending_character_camera_input();
    service_operator_camera_input();
    if (operator_weak_requested) {
        operator_weak_attack_until_ms = now + 125u;
        SudekiMpLogWrite(
            "lan_arena_client_input event=weak_attack phase=requested "
            "source=local_operator_api policy=bounded_test_pulse\r\n");
    }
    controller = client_game_base != NULL && readable_memory(
            client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller)) ?
        *(uint8_t **)(client_game_base + RVA_CHARACTER_CONTROLLER_GLOBAL) : NULL;
    ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    if (readable_memory(
            controller, CONTROLLER_TARGET_OFFSET + sizeof(void *)) &&
        *(void **)(controller + CONTROLLER_TARGET_OFFSET) == ailish) {
        raw_x = *(float *)(controller + CONTROLLER_MOVE_X_OFFSET);
        raw_y = *(float *)(controller + CONTROLLER_MOVE_Y_OFFSET);
    } else {
        raw_x = 0.0f;
        raw_y = 0.0f;
    }
    /* The native controller's axes and transition states are scoped to this
     * actual Sudeki window.  GetAsyncKeyState is not: under two isolated Wine
     * servers it can report a Tal-host key/click while the client prefix still
     * considers its own window foreground.  That cross-fired Ailish and could
     * also cross-drive movement. */
    local_direction_held = isfinite(raw_x) && isfinite(raw_y) &&
        (fabsf(raw_x) > 0.0001f || fabsf(raw_y) > 0.0001f);
    first_person_active = client_ailish_first_person_active();
    local_weak_held = SudekiMpLanArenaClientRangedWeakHeld(
        first_person_active,
        (native_weak_sample_at_ms != 0u &&
         (DWORD)(now - native_weak_sample_at_ms) <=
             CLIENT_NATIVE_INPUT_FRESH_MS && native_weak_held) ||
        (operator_weak_attack_until_ms != 0u &&
         (LONG)(operator_weak_attack_until_ms - now) > 0) ||
        operator_weak_held);
    weak_pressed = local_weak_held && !weak_was_down;
    weak_changed = local_weak_held != weak_was_down;
    weak_was_down = local_weak_held;
    desired_x = local_direction_held ? last_direction_x : 0;
    desired_z = local_direction_held ? last_direction_z : 0;
    refresh_client_camera_aim();
    if (first_person_active != last_transmitted_first_person_active) {
        SudekiMpLogFormat(
            "lan_arena_client_input event=ranged_first_person "
            "state=%s arbiter_offset=0x50 flag=0x00400000 "
            "policy=exact_native_aim_mode_witness\r\n",
            first_person_active ? "active" : "inactive");
    }
    aim_changed = last_aim_x != last_transmitted_aim_x ||
        last_aim_y != last_transmitted_aim_y ||
        last_aim_z != last_transmitted_aim_z;
    if (desired_x != last_transmitted_direction_x ||
        desired_z != last_transmitted_direction_z ||
        weak_changed || weak_was_down != last_transmitted_weak_held ||
        aim_changed ||
        first_person_active != last_transmitted_first_person_active ||
        combat_toggle_pending ||
        ((desired_x != 0 || desired_z != 0) &&
            (DWORD)(now - last_input_send_at) >=
                CLIENT_INPUT_SEND_INTERVAL_MS) ||
        (weak_was_down && (DWORD)(now - last_input_send_at) >=
            CLIENT_INPUT_SEND_INTERVAL_MS) ||
        last_input_send_at == 0u) {
        if (send_client_input_at(
                desired_x, desired_z, weak_pressed, weak_was_down, now) &&
            weak_changed) {
            SudekiMpLogFormat(
                "lan_arena_client_input event=weak_attack_send phase=%s "
                "policy=foreground_physical_button_edge_plus_held_state\r\n",
                weak_was_down ? "pressed" : "released");
        }
    }
}

static void __stdcall capture_client_combat(void *controller) {
    uint8_t *state = (uint8_t *)controller;
    void *ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    BOOL owns_ailish = state != NULL && ailish != NULL &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == ailish;
    int native_weak_state;
    if (!authenticated_client() || !owns_ailish) {
        original_controller_combat(controller);
        return;
    }
    native_weak_state = *(int *)(state + CONTROLLER_WEAK_OFFSET);
    native_weak_held =
        SudekiMpLanArenaClientNativeWeakHeld(native_weak_state);
    native_weak_sample_at_ms = GetTickCount();
    /* Consume local execution while still advancing Sudeki's 0/1/2/3 input
     * transition. Restoring state 1 verbatim caused every later frame to
     * manufacture another press edge; state 3 likewise had to retire to 0.
     * The host receives the sampled held bit above and remains authoritative. */
    *(int *)(state + CONTROLLER_WEAK_OFFSET) = 0;
    *(int *)(state + CONTROLLER_STRONG_OFFSET) = 0;
    *(int *)(state + CONTROLLER_SWEEP_OFFSET) = 0;
    *(int *)(state + CONTROLLER_BLOCK_OFFSET) = 0;
    *(int *)(state + CONTROLLER_WEAPON_NEXT_OFFSET) = 0;
    *(int *)(state + CONTROLLER_WEAPON_PREVIOUS_OFFSET) = 0;
    original_controller_combat(controller);
    if (authenticated_client() &&
        *(void **)(state + CONTROLLER_TARGET_OFFSET) == ailish) {
        *(int *)(state + CONTROLLER_WEAK_OFFSET) =
            SudekiMpLanArenaClientSuppressedWeakNextState(native_weak_state);
    }
}

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module) {
    uint8_t *base;
    if (game_module == NULL || original_arbiter_movement != NULL ||
        original_controller_combat != NULL ||
        original_quick_menu_input != NULL ||
        original_camera_input_event != NULL ||
        original_character_input_handler != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (memcmp(base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry)) != 0 ||
        memcmp(base + RVA_CONTROLLER_AIM_UPDATE,
            expected_controller_aim_update_entry,
            sizeof(expected_controller_aim_update_entry)) != 0 ||
        memcmp(base + RVA_QUICK_MENU_NATIVE_TOGGLE,
            expected_quick_menu_native_toggle_entry,
            sizeof(expected_quick_menu_native_toggle_entry)) != 0 ||
        memcmp(base + RVA_QUICK_MENU_INPUT,
            expected_quick_menu_input_entry,
            sizeof(expected_quick_menu_input_entry)) != 0 ||
        memcmp(base + RVA_CAMERA_INPUT_EVENT,
            expected_camera_input_event_entry,
            sizeof(expected_camera_input_event_entry)) != 0 ||
        *(void **)(base + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
            base + RVA_CHARACTER_INPUT_HANDLER ||
        *(void **)(base + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
            base + RVA_QUICK_MENU_INPUT ||
        *(void **)(base + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
            base + RVA_CAMERA_INPUT_EVENT) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    original_arbiter_movement = (ArbiterMovementFunction)(base + RVA_ARBITER_MOVEMENT);
    original_quick_menu_input =
        (QuickMenuInputFunction)(base + RVA_QUICK_MENU_INPUT);
    original_camera_input_event =
        (CameraInputEventFunction)(base + RVA_CAMERA_INPUT_EVENT);
    original_character_input_handler =
        (CharacterInputHandler)(base + RVA_CHARACTER_INPUT_HANDLER);
    if (!SudekiMpInstallRelativeCallHook(&alternate_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_ALTERNATE, original_arbiter_movement,
            capture_client_movement) ||
        !SudekiMpInstallRelativeCallHook(&normal_movement_hook,
            base + RVA_PLAYER_MOVE_CALL_NORMAL, original_arbiter_movement,
            capture_client_movement) ||
        !SudekiMpInstallInlineHook(&controller_combat_hook,
            base + RVA_CONTROLLER_COMBAT, expected_controller_combat_entry,
            sizeof(expected_controller_combat_entry), capture_client_combat) ||
        !SudekiMpInstallPointerHook(&quick_menu_input_hook,
            (void **)(base + RVA_QUICK_MENU_INPUT_VTABLE_SLOT),
            original_quick_menu_input,
            route_client_quick_menu_input) ||
        !SudekiMpInstallPointerHook(&camera_input_event_hook,
            (void **)(base + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT),
            original_camera_input_event,
            route_client_camera_input_event) ||
        !SudekiMpInstallPointerHook(&character_input_hook,
            (void **)(base + RVA_CHARACTER_INPUT_VTABLE_SLOT),
            original_character_input_handler,
            route_client_character_input)) {
        SudekiMpUninstallLanArenaClientInput();
        return FALSE;
    }
    original_controller_combat = (ControllerCombatFunction)controller_combat_hook.trampoline;
    operator_combat_toggle_event = CreateEventW(
        NULL, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_CLIENT_COMBAT_TOGGLE_EVENT);
    operator_weak_attack_event = CreateEventW(
        NULL, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT);
    operator_weak_hold_event = CreateEventW(
        NULL, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT);
    operator_camera_left_event = CreateEventW(
        NULL, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_LEFT_EVENT);
    operator_camera_right_event = CreateEventW(
        NULL, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_RIGHT_EVENT);
    if (operator_combat_toggle_event == NULL ||
        operator_weak_attack_event == NULL ||
        operator_weak_hold_event == NULL ||
        operator_camera_left_event == NULL ||
        operator_camera_right_event == NULL) {
        SudekiMpUninstallLanArenaClientInput();
        return FALSE;
    }
    ResetEvent(operator_weak_hold_event);
    last_direction_x = 0;
    last_direction_z = 0;
    last_aim_x = 0;
    last_aim_y = 0;
    last_aim_z = 0;
    weak_was_down = FALSE;
    movement_send_logged = FALSE;
    quick_menu_action_block_logged = FALSE;
    quick_menu_visible_state = -1;
    last_input_send_at = 0u;
    last_transmitted_direction_x = 0;
    last_transmitted_direction_z = 0;
    last_transmitted_aim_x = 0;
    last_transmitted_aim_y = 0;
    last_transmitted_aim_z = 0;
    last_transmitted_weak_held = FALSE;
    last_transmitted_first_person_active = FALSE;
    native_weak_held = FALSE;
    native_weak_sample_at_ms = 0u;
    combat_toggle_pending = FALSE;
    operator_weak_attack_until_ms = 0u;
    operator_camera_until_ms = 0u;
    operator_camera_direction = 0;
    operator_camera_release_pending = FALSE;
    operator_camera_submission_logged = FALSE;
    last_native_camera_input_trace_at_ms = 0u;
    ZeroMemory(&pending_character_camera_event,
        sizeof(pending_character_camera_event));
    pending_character_camera_event_valid = FALSE;
    pending_character_camera_event_at_ms = 0u;
    client_camera_route_trace_state = -1;
    client_first_person_aim_bridge_trace_state = -1;
    combat_toggle_was_down =
        (GetKeyState(VK_F8) & (SHORT)0x8000) != 0;
    client_game_base = base;
    return TRUE;
}

void SudekiMpUninstallLanArenaClientInput(void) {
    SudekiMpRestorePointerHook(&character_input_hook);
    SudekiMpRestorePointerHook(&camera_input_event_hook);
    SudekiMpRestorePointerHook(&quick_menu_input_hook);
    SudekiMpRestoreInlineHook(&controller_combat_hook);
    SudekiMpRestoreRelativeCallHook(&normal_movement_hook);
    SudekiMpRestoreRelativeCallHook(&alternate_movement_hook);
    original_controller_combat = NULL;
    original_arbiter_movement = NULL;
    original_quick_menu_input = NULL;
    original_camera_input_event = NULL;
    original_character_input_handler = NULL;
    last_direction_x = 0;
    last_direction_z = 0;
    last_aim_x = 0;
    last_aim_y = 0;
    last_aim_z = 0;
    weak_was_down = FALSE;
    movement_send_logged = FALSE;
    quick_menu_action_block_logged = FALSE;
    quick_menu_visible_state = -1;
    last_input_send_at = 0u;
    last_transmitted_direction_x = 0;
    last_transmitted_direction_z = 0;
    last_transmitted_aim_x = 0;
    last_transmitted_aim_y = 0;
    last_transmitted_aim_z = 0;
    last_transmitted_weak_held = FALSE;
    last_transmitted_first_person_active = FALSE;
    native_weak_held = FALSE;
    native_weak_sample_at_ms = 0u;
    combat_toggle_pending = FALSE;
    operator_weak_attack_until_ms = 0u;
    operator_camera_until_ms = 0u;
    operator_camera_direction = 0;
    operator_camera_release_pending = FALSE;
    operator_camera_submission_logged = FALSE;
    last_native_camera_input_trace_at_ms = 0u;
    ZeroMemory(&pending_character_camera_event,
        sizeof(pending_character_camera_event));
    pending_character_camera_event_valid = FALSE;
    pending_character_camera_event_at_ms = 0u;
    client_camera_route_trace_state = -1;
    client_first_person_aim_bridge_trace_state = -1;
    combat_toggle_was_down = FALSE;
    if (operator_combat_toggle_event != NULL) {
        CloseHandle(operator_combat_toggle_event);
        operator_combat_toggle_event = NULL;
    }
    if (operator_weak_attack_event != NULL) {
        CloseHandle(operator_weak_attack_event);
        operator_weak_attack_event = NULL;
    }
    if (operator_weak_hold_event != NULL) {
        ResetEvent(operator_weak_hold_event);
        CloseHandle(operator_weak_hold_event);
        operator_weak_hold_event = NULL;
    }
    if (operator_camera_right_event != NULL) {
        CloseHandle(operator_camera_right_event);
        operator_camera_right_event = NULL;
    }
    if (operator_camera_left_event != NULL) {
        CloseHandle(operator_camera_left_event);
        operator_camera_left_event = NULL;
    }
    client_game_base = NULL;
}
