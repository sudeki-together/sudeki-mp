#include "network/lan_arena_tal_combo_graph.h"

#include <stddef.h>

typedef struct TalComboTransition {
    uint8_t action_variant;
    int selector;
    int replay_state;
    uint8_t combat_state;
} TalComboTransition;

/* Live-verified against the supported executable. The first two levels share
 * selectors by depth/input; the third level branches on the complete W/S
 * history. Final clips use state 65 on replica entry, matching the proven WWW
 * replay policy, while non-terminal stage clips enter through state 1. */
static const TalComboTransition tal_combo_transitions[] = {
    { SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 50, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_STRONG, 52, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO, 51, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO, 53, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE, 62, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS, 54, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW, 60, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS, 61, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS, 63, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW, 65, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW, 68, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS, 69, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE, 70, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_SWEEP, 71, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_BLOCK, 20, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK }
};

static const TalComboTransition *transition_for_variant(uint8_t variant) {
    size_t index;
    for (index = 0u;
         index < sizeof(tal_combo_transitions) / sizeof(tal_combo_transitions[0]);
         ++index) {
        if (tal_combo_transitions[index].action_variant == variant) {
            return &tal_combo_transitions[index];
        }
    }
    return NULL;
}

BOOL SudekiMpLanArenaTalActionFromNativePresentation(
    int selector,
    uint8_t state,
    uint8_t *action_variant
) {
    size_t index;
    if (action_variant == NULL || state == 192u) return FALSE;
    if (selector == 21) {
        *action_variant = SUDEKIMP_LAN_ARENA_ACTION_BLOCK;
        return TRUE;
    }
    if (state != 1u && state != 65u && selector != 20) return FALSE;
    for (index = 0u;
         index < sizeof(tal_combo_transitions) / sizeof(tal_combo_transitions[0]);
         ++index) {
        if (tal_combo_transitions[index].selector == selector) {
            *action_variant = tal_combo_transitions[index].action_variant;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL SudekiMpLanArenaTalActionToNativePresentation(
    uint8_t action_variant,
    int *selector,
    int *state
) {
    const TalComboTransition *transition;
    if (selector == NULL || state == NULL) return FALSE;
    transition = transition_for_variant(action_variant);
    if (transition == NULL) return FALSE;
    *selector = transition->selector;
    *state = transition->replay_state;
    return TRUE;
}

BOOL SudekiMpLanArenaTalActionCombatState(
    uint8_t action_variant,
    uint8_t *combat_state
) {
    const TalComboTransition *transition;
    if (combat_state == NULL) return FALSE;
    transition = transition_for_variant(action_variant);
    if (transition == NULL) return FALSE;
    *combat_state = transition->combat_state;
    return TRUE;
}
