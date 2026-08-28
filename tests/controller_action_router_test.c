#include "engine/controller_action_router.h"

#include "input/bridge_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void prime_seat(
    SudekiMpControllerActionRouter *router,
    unsigned int seat_index
) {
    SudekiMpControllerActionContext context;

    memset(&context, 0, sizeof(context));
    context.seat_active = 1;
    CHECK(SudekiMpControllerActionRouterAdvance(
        router, seat_index, 1, 0u, &context, NULL, 0u) == 0u);
}

static SudekiMpControllerActionResolution press(
    SudekiMpControllerActionRouter *router,
    unsigned int seat_index,
    uint32_t button,
    const SudekiMpControllerActionContext *context
) {
    SudekiMpControllerActionResolution result;

    memset(&result, 0, sizeof(result));
    CHECK(SudekiMpControllerActionRouterAdvance(
        router, seat_index, 1, button, context, &result, 1u) == 1u);
    return result;
}

static void release(
    SudekiMpControllerActionRouter *router,
    unsigned int seat_index,
    const SudekiMpControllerActionContext *context
) {
    CHECK(SudekiMpControllerActionRouterAdvance(
        router, seat_index, 1, 0u, context, NULL, 0u) == 0u);
}

static void test_shipped_gameplay_face_contract(void) {
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution result;

    SudekiMpControllerActionRouterInitialize(&router);
    prime_seat(&router, 0u);
    memset(&context, 0, sizeof(context));
    context.seat_active = 1;

    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_INTERACT);
    release(&router, 0u, &context);
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_X, &context);
    CHECK(result.intent ==
        SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG);
    release(&router, 0u, &context);
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_Y, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU);
    release(&router, 0u, &context);
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_B, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_NONE);
    release(&router, 0u, &context);

    context.combat_active = 1;
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK);
    release(&router, 0u, &context);
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_B, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP);
}

static void test_sudekimp_ranged_x_policy(void) {
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution result;

    SudekiMpControllerActionRouterInitialize(&router);
    prime_seat(&router, 1u);
    memset(&context, 0, sizeof(context));
    context.seat_active = 1;
    context.ranged_character = 1;
    context.perspective_toggle_available = 1;

    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_X, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_PERSPECTIVE_TOGGLE);
    release(&router, 1u, &context);

    context.perspective_toggle_available = 0;
    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_X, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_NONE);
    release(&router, 1u, &context);

    context.ranged_character = 0;
    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_X, &context);
    CHECK(result.intent ==
        SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG);
}

static void test_context_priority(void) {
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution result;

    SudekiMpControllerActionRouterInitialize(&router);
    prime_seat(&router, 1u);
    memset(&context, 0, sizeof(context));
    context.seat_active = 1;
    context.combat_active = 1;
    context.interaction_target_known = 1;
    context.transition_vote_active = 1;
    context.modal_active = 1;

    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_MODAL_CONFIRM);
    release(&router, 1u, &context);
    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_B, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_MODAL_CANCEL);
    release(&router, 1u, &context);

    context.modal_active = 0;
    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_VOTE_ACCEPT);
    release(&router, 1u, &context);
    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_B, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_VOTE_CANCEL);
    release(&router, 1u, &context);

    context.transition_vote_active = 0;
    context.combat_active = 0;
    result = press(&router, 1u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_INTERACT);
}

static void test_modal_navigation_and_quickshots(void) {
    static const uint32_t buttons[6] = {
        SUDEKIMP_BRIDGE_BUTTON_DPAD_UP,
        SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT,
        SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN,
        SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT,
        SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER,
        SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER
    };
    static const SudekiMpControllerActionIntent modal_intents[6] = {
        SUDEKIMP_CONTROLLER_INTENT_MODAL_UP,
        SUDEKIMP_CONTROLLER_INTENT_MODAL_RIGHT,
        SUDEKIMP_CONTROLLER_INTENT_MODAL_DOWN,
        SUDEKIMP_CONTROLLER_INTENT_MODAL_LEFT,
        SUDEKIMP_CONTROLLER_INTENT_MODAL_PREVIOUS_PAGE,
        SUDEKIMP_CONTROLLER_INTENT_MODAL_NEXT_PAGE
    };
    static const SudekiMpControllerActionIntent gameplay_intents[4] = {
        SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_UP,
        SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_RIGHT,
        SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_DOWN,
        SUDEKIMP_CONTROLLER_INTENT_QUICKSHOT_LEFT
    };
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    unsigned int index;

    SudekiMpControllerActionRouterInitialize(&router);
    prime_seat(&router, 2u);
    memset(&context, 0, sizeof(context));
    context.seat_active = 1;
    context.modal_active = 1;
    for (index = 0u; index < 6u; ++index) {
        SudekiMpControllerActionResolution result =
            press(&router, 2u, buttons[index], &context);
        CHECK(result.intent == modal_intents[index]);
        release(&router, 2u, &context);
    }

    context.modal_active = 0;
    for (index = 0u; index < 4u; ++index) {
        SudekiMpControllerActionResolution result =
            press(&router, 2u, buttons[index], &context);
        CHECK(result.intent == gameplay_intents[index]);
        release(&router, 2u, &context);
    }
}

static void test_edges_reconnect_and_seat_independence(void) {
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution result;

    SudekiMpControllerActionRouterInitialize(&router);
    memset(&context, 0, sizeof(context));
    context.seat_active = 1;
    prime_seat(&router, 0u);
    prime_seat(&router, 3u);

    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.seat_index == 0u);
    CHECK(SudekiMpControllerActionRouterAdvance(
        &router, 0u, 1, SUDEKIMP_BRIDGE_BUTTON_A,
        &context, &result, 1u) == 0u);

    result = press(&router, 3u, SUDEKIMP_BRIDGE_BUTTON_A, &context);
    CHECK(result.seat_index == 3u);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_INTERACT);

    CHECK(SudekiMpControllerActionRouterAdvance(
        &router, 0u, 0, 0u, &context, NULL, 0u) == 0u);
    CHECK(SudekiMpControllerActionRouterAdvance(
        &router, 0u, 1, SUDEKIMP_BRIDGE_BUTTON_X,
        &context, &result, 1u) == 0u);
    CHECK(SudekiMpControllerActionRouterAdvance(
        &router, 0u, 1, 0u, &context, &result, 1u) == 0u);
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_X, &context);
    CHECK(result.intent ==
        SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG);

    CHECK(SudekiMpControllerActionRouterAdvance(
        &router, 4u, 1, 0u, &context, NULL, 0u) == 0u);
}

static void test_simultaneous_edges_and_capacity(void) {
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution results[2];
    size_t count;

    SudekiMpControllerActionRouterInitialize(&router);
    prime_seat(&router, 0u);
    memset(&context, 0, sizeof(context));
    context.seat_active = 1;
    context.combat_active = 1;
    count = SudekiMpControllerActionRouterAdvance(
        &router,
        0u,
        1,
        SUDEKIMP_BRIDGE_BUTTON_A |
            SUDEKIMP_BRIDGE_BUTTON_B |
            SUDEKIMP_BRIDGE_BUTTON_X,
        &context,
        results,
        2u
    );
    CHECK(count == 3u);
    CHECK(results[0].protocol_button == SUDEKIMP_BRIDGE_BUTTON_A);
    CHECK(results[0].intent ==
        SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK);
    CHECK(results[1].protocol_button == SUDEKIMP_BRIDGE_BUTTON_B);
    CHECK(results[1].intent ==
        SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP);
}

static void test_inactive_and_names(void) {
    SudekiMpControllerActionRouter router;
    SudekiMpControllerActionContext context;
    SudekiMpControllerActionResolution result;

    SudekiMpControllerActionRouterInitialize(&router);
    prime_seat(&router, 0u);
    memset(&context, 0, sizeof(context));
    result = press(&router, 0u, SUDEKIMP_BRIDGE_BUTTON_Y, &context);
    CHECK(result.intent == SUDEKIMP_CONTROLLER_INTENT_NONE);
    CHECK(strcmp(SudekiMpControllerProtocolButtonName(
        SUDEKIMP_BRIDGE_BUTTON_A), "A") == 0);
    CHECK(strcmp(SudekiMpControllerProtocolButtonName(
        SUDEKIMP_BRIDGE_BUTTON_X), "X") == 0);
    CHECK(strcmp(SudekiMpControllerProtocolButtonName(
        SUDEKIMP_BRIDGE_BUTTON_Y), "Y") == 0);
    CHECK(strcmp(SudekiMpControllerActionIntentName(
        SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU), "quick_menu") == 0);
    CHECK(strcmp(SudekiMpControllerActionIntentName(
        SUDEKIMP_CONTROLLER_INTENT_PERSPECTIVE_TOGGLE),
        "perspective_toggle") == 0);
    CHECK(strcmp(SudekiMpControllerActionIntentName(
        (SudekiMpControllerActionIntent)999), "unknown") == 0);
}

static void test_exact_native_combat_flags(void) {
    SudekiMpControllerCombatFlags flags;

    CHECK(SudekiMpControllerActionCombatFlags(
        SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK, &flags));
    CHECK(flags.weak == 1 && flags.strong == 0 && flags.sweep == 0);
    CHECK(SudekiMpControllerActionCombatFlags(
        SUDEKIMP_CONTROLLER_INTENT_SECONDARY_ATTACK_STRONG, &flags));
    CHECK(flags.weak == 0 && flags.strong == 1 && flags.sweep == 0);
    CHECK(SudekiMpControllerActionCombatFlags(
        SUDEKIMP_CONTROLLER_INTENT_CROWD_CLEAR_SWEEP, &flags));
    CHECK(flags.weak == 0 && flags.strong == 0 && flags.sweep == 1);
    CHECK(!SudekiMpControllerActionCombatFlags(
        SUDEKIMP_CONTROLLER_INTENT_QUICK_MENU, &flags));
    CHECK(flags.weak == 0 && flags.strong == 0 && flags.sweep == 0);
    CHECK(!SudekiMpControllerActionCombatFlags(
        SUDEKIMP_CONTROLLER_INTENT_PERSPECTIVE_TOGGLE, &flags));
    CHECK(flags.weak == 0 && flags.strong == 0 && flags.sweep == 0);
    CHECK(!SudekiMpControllerActionCombatFlags(
        SUDEKIMP_CONTROLLER_INTENT_PRIMARY_ATTACK_WEAK, NULL));
}

int main(void) {
    test_shipped_gameplay_face_contract();
    test_sudekimp_ranged_x_policy();
    test_context_priority();
    test_modal_navigation_and_quickshots();
    test_edges_reconnect_and_seat_independence();
    test_simultaneous_edges_and_capacity();
    test_inactive_and_names();
    test_exact_native_combat_flags();

    if (failures != 0) {
        fprintf(stderr, "%d controller-action-router checks failed\n",
            failures);
        return 1;
    }
    puts("controller-action-router checks passed");
    return 0;
}
