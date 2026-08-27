#ifndef SUDEKIMP_CONTROLLER_ACTION_ROUTER_H
#define SUDEKIMP_CONTROLLER_ACTION_ROUTER_H

#include <stddef.h>
#include <stdint.h>

enum {
    SUDEKIMP_CONTROLLER_ACTION_MAX_SEATS = 4u,
    SUDEKIMP_CONTROLLER_ACTION_MAX_RESULTS = 10u
};

typedef enum SudekiMpControllerActionIntent {
    SUDEKIMP_CONTROLLER_INTENT_NONE = 0,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_CONFIRM,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_CANCEL,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_SECONDARY,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_UP,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_DOWN,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_LEFT,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_RIGHT,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_PREVIOUS_PAGE,
    SUDEKIMP_CONTROLLER_INTENT_MODAL_NEXT_PAGE,
    SUDEKIMP_CONTROLLER_INTENT_VOTE_ACCEPT,
    SUDEKIMP_CONTROLLER_INTENT_VOTE_CANCEL,
    SUDEKIMP_CONTROLLER_INTENT_INTERACT,
    SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK,
    SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG,
    SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU,
    SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP,
    SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_UP,
    SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_RIGHT,
    SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_DOWN,
    SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_LEFT
} SudekiMpControllerActionIntent;

/* Context is authoritative and supplied independently for each local seat.
 * A true interaction_target_known means an upstream resolver has already
 * validated the exact actor/target/source-generation tuple. The router never
 * promotes a targetless attention request into an interaction. */
typedef struct SudekiMpControllerActionContext {
    int seat_active;
    int modal_active;
    int transition_vote_active;
    int interaction_target_known;
    int combat_active;
} SudekiMpControllerActionContext;

typedef struct SudekiMpControllerActionResolution {
    unsigned int seat_index;
    uint32_t protocol_button;
    SudekiMpControllerActionIntent intent;
} SudekiMpControllerActionResolution;

typedef struct SudekiMpControllerCombatFlags {
    int weak;
    int strong;
    int sweep;
} SudekiMpControllerCombatFlags;

typedef struct SudekiMpControllerActionSeatState {
    uint32_t previous_buttons;
    int connected;
    int neutral_required;
} SudekiMpControllerActionSeatState;

typedef struct SudekiMpControllerActionRouter {
    SudekiMpControllerActionSeatState
        seats[SUDEKIMP_CONTROLLER_ACTION_MAX_SEATS];
} SudekiMpControllerActionRouter;

void SudekiMpControllerActionRouterInitialize(
    SudekiMpControllerActionRouter *router
);

/* Advances exactly one seat. Results are rising-edge intents in deterministic
 * protocol-button order. The return value is the number of results resolved;
 * it may exceed output_capacity. Disconnect/reconnect requires a fully neutral
 * packet before new edges are accepted, preventing held-button leakage. */
size_t SudekiMpControllerActionRouterAdvance(
    SudekiMpControllerActionRouter *router,
    unsigned int seat_index,
    int connected,
    uint32_t buttons,
    const SudekiMpControllerActionContext *context,
    SudekiMpControllerActionResolution *output,
    size_t output_capacity
);

const char *SudekiMpControllerProtocolButtonName(uint32_t protocol_button);
const char *SudekiMpControllerActionIntentName(
    SudekiMpControllerActionIntent intent
);
int SudekiMpControllerActionCombatFlags(
    SudekiMpControllerActionIntent intent,
    SudekiMpControllerCombatFlags *flags
);

#endif
