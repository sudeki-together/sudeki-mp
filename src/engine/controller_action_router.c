#include "engine/controller_action_router.h"

#include "input/bridge_protocol.h"

#include <string.h>

static const uint32_t routed_buttons[
    SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS
] = {
    SUDEKIMP_BRIDGE_BUTTON_A,
    SUDEKIMP_BRIDGE_BUTTON_B,
    SUDEKIMP_BRIDGE_BUTTON_X,
    SUDEKIMP_BRIDGE_BUTTON_Y,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_UP,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN,
    SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT,
    SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER,
    SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER
};

static SudekiMpControllerActionIntent resolve_modal(uint32_t button) {
    switch (button) {
    case SUDEKIMP_BRIDGE_BUTTON_A:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_CONFIRM;
    case SUDEKIMP_BRIDGE_BUTTON_B:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_CANCEL;
    case SUDEKIMP_BRIDGE_BUTTON_X:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_SECONDARY;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_UP:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_UP;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_DOWN;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_LEFT;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_RIGHT;
    case SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_PREVIOUS_PAGE;
    case SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER:
        return SUDEKIMP_CONTROLLER_INTENT_MODAL_NEXT_PAGE;
    default:
        return SUDEKIMP_CONTROLLER_INTENT_NONE;
    }
}

static SudekiMpControllerActionIntent resolve_vote(uint32_t button) {
    switch (button) {
    case SUDEKIMP_BRIDGE_BUTTON_A:
        return SUDEKIMP_CONTROLLER_INTENT_VOTE_ACCEPT;
    case SUDEKIMP_BRIDGE_BUTTON_B:
        return SUDEKIMP_CONTROLLER_INTENT_VOTE_CANCEL;
    default:
        return SUDEKIMP_CONTROLLER_INTENT_NONE;
    }
}

static SudekiMpControllerActionIntent resolve_gameplay(
    uint32_t button,
    const SudekiMpControllerActionContext *context
) {
    switch (button) {
    case SUDEKIMP_BRIDGE_BUTTON_A:
        return context->interaction_target_known ?
            SUDEKIMP_CONTROLLER_INTENT_INTERACT :
            SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK;
    case SUDEKIMP_BRIDGE_BUTTON_X:
        if (context->ranged_character) {
            return context->perspective_toggle_available ?
                SUDEKIMP_CONTROLLER_INTENT_PERSPECTIVE_TOGGLE :
                SUDEKIMP_CONTROLLER_INTENT_NONE;
        }
        return SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG;
    case SUDEKIMP_BRIDGE_BUTTON_Y:
        return SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU;
    case SUDEKIMP_BRIDGE_BUTTON_B:
        return context->combat_active ?
            SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP :
            SUDEKIMP_CONTROLLER_INTENT_NONE;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_UP:
        return SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_UP;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT:
        return SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_RIGHT;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN:
        return SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_DOWN;
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT:
        return SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_LEFT;
    default:
        return SUDEKIMP_CONTROLLER_INTENT_NONE;
    }
}

static SudekiMpControllerActionIntent resolve_intent(
    uint32_t button,
    const SudekiMpControllerActionContext *context
) {
    if (!context->seat_active) {
        return SUDEKIMP_CONTROLLER_INTENT_NONE;
    }
    if (context->modal_active) {
        return resolve_modal(button);
    }
    if (context->transition_vote_active) {
        return resolve_vote(button);
    }
    return resolve_gameplay(button, context);
}

void SudekiMpControllerActionRouterInitialize(
    SudekiMpControllerActionRouter *router
) {
    unsigned int seat_index;

    if (router == NULL) {
        return;
    }
    memset(router, 0, sizeof(*router));
    for (seat_index = 0u;
         seat_index < SUDEKIMP_CONTROLLER_ACTION_MAX_SEATS;
         ++seat_index) {
        router->seats[seat_index].neutral_required = 1;
    }
}

size_t SudekiMpControllerActionRouterAdvance(
    SudekiMpControllerActionRouter *router,
    unsigned int seat_index,
    int connected,
    uint32_t buttons,
    const SudekiMpControllerActionContext *context,
    SudekiMpControllerActionResolution *output,
    size_t output_capacity
) {
    SudekiMpControllerActionSeatState *seat;
    uint32_t rising;
    size_t result_count = 0u;
    size_t button_index;

    if (router == NULL ||
        seat_index >= SUDEKIMP_CONTROLLER_ACTION_MAX_SEATS) {
        return 0u;
    }
    seat = &router->seats[seat_index];
    if (!connected) {
        seat->connected = 0;
        seat->neutral_required = 1;
        seat->previous_buttons = 0u;
        return 0u;
    }
    if (!seat->connected) {
        seat->connected = 1;
        seat->neutral_required = 1;
        seat->previous_buttons = buttons;
        if (buttons == 0u) {
            seat->neutral_required = 0;
        }
        return 0u;
    }
    if (seat->neutral_required) {
        seat->previous_buttons = buttons;
        if (buttons == 0u) {
            seat->neutral_required = 0;
        }
        return 0u;
    }

    rising = buttons & ~seat->previous_buttons;
    seat->previous_buttons = buttons;
    if (context == NULL || rising == 0u) {
        return 0u;
    }
    for (button_index = 0u;
         button_index < SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS;
         ++button_index) {
        uint32_t button = routed_buttons[button_index];
        SudekiMpControllerActionIntent intent;

        if ((rising & button) == 0u) {
            continue;
        }
        intent = resolve_intent(button, context);
        if (output != NULL && result_count < output_capacity) {
            output[result_count].seat_index = seat_index;
            output[result_count].protocol_button = button;
            output[result_count].intent = intent;
        }
        ++result_count;
    }
    return result_count;
}

const char *SudekiMpControllerProtocolButtonName(uint32_t protocol_button) {
    switch (protocol_button) {
    case SUDEKIMP_BRIDGE_BUTTON_A: return "A";
    case SUDEKIMP_BRIDGE_BUTTON_B: return "B";
    case SUDEKIMP_BRIDGE_BUTTON_X: return "X";
    case SUDEKIMP_BRIDGE_BUTTON_Y: return "Y";
    case SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER: return "LB";
    case SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER: return "RB";
    case SUDEKIMP_BRIDGE_BUTTON_BACK: return "BACK";
    case SUDEKIMP_BRIDGE_BUTTON_START: return "START";
    case SUDEKIMP_BRIDGE_BUTTON_LEFT_STICK: return "LS";
    case SUDEKIMP_BRIDGE_BUTTON_RIGHT_STICK: return "RS";
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_UP: return "DPAD_UP";
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN: return "DPAD_DOWN";
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT: return "DPAD_LEFT";
    case SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT: return "DPAD_RIGHT";
    default: return "UNKNOWN";
    }
}

const char *SudekiMpControllerActionIntentName(
    SudekiMpControllerActionIntent intent
) {
    switch (intent) {
    case SUDEKIMP_CONTROLLER_INTENT_NONE: return "none";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_CONFIRM: return "modal_confirm";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_CANCEL: return "modal_cancel";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_SECONDARY:
        return "modal_secondary";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_UP: return "modal_up";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_DOWN: return "modal_down";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_LEFT: return "modal_left";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_RIGHT: return "modal_right";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_PREVIOUS_PAGE:
        return "modal_previous_page";
    case SUDEKIMP_CONTROLLER_INTENT_MODAL_NEXT_PAGE:
        return "modal_next_page";
    case SUDEKIMP_CONTROLLER_INTENT_VOTE_ACCEPT: return "vote_accept";
    case SUDEKIMP_CONTROLLER_INTENT_VOTE_CANCEL: return "vote_cancel";
    case SUDEKIMP_CONTROLLER_INTENT_INTERACT: return "interact";
    case SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK:
        return "primary_attack_weak";
    case SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG:
        return "secondary_attack_strong";
    case SUDEKIMP_CONTROLLER_INTENT_PERSPECTIVE_TOGGLE:
        return "perspective_toggle";
    case SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU: return "quick_menu";
    case SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP:
        return "crowd_clear_sweep";
    case SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_UP: return "quickshot_up";
    case SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_RIGHT:
        return "quickshot_right";
    case SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_DOWN:
        return "quickshot_down";
    case SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_LEFT:
        return "quickshot_left";
    default: return "unknown";
    }
}

int SudekiMpControllerActionCombatFlags(
    SudekiMpControllerActionIntent intent,
    SudekiMpControllerCombatFlags *flags
) {
    if (flags == NULL) {
        return 0;
    }
    memset(flags, 0, sizeof(*flags));
    switch (intent) {
    case SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK:
        flags->weak = 1;
        return 1;
    case SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG:
        flags->strong = 1;
        return 1;
    case SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP:
        flags->sweep = 1;
        return 1;
    default:
        return 0;
    }
}
