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

/* Buki shares selectors 53/54 between weak/strong first/second swings.
 * The semantic model channel distinguishes 0x72/73 from 0x74/75; require
 * both observations for capture. The positional table still validates the
 * destination clip during replay. Confirmed in the 2026-09-12 live capture. */
static const TalComboTransition buki_combo_transitions[] = {
    { SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 53, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_STRONG, 53, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO, 54, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO, 54, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE, 64, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS, 60, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW, 68, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS, 65, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS, 67, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW, 56, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW, 55, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS, 66, 65,
      SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_SWEEP, 71, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK, 69, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK },
    { SUDEKIMP_LAN_ARENA_ACTION_BLOCK, 44, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK },
    { SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD, 45, 0,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK },
    { SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE, 46, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK },
    { SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP, 49, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK },
    { SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT, 47, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK },
    { SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT, 48, 1,
      SUDEKIMP_LAN_ARENA_COMBAT_BLOCK }
};

static const TalComboTransition *buki_transition_for_variant(uint8_t variant) {
    size_t index;
    for (index = 0u;
         index < sizeof(buki_combo_transitions) /
             sizeof(buki_combo_transitions[0]);
         ++index) {
        if (buki_combo_transitions[index].action_variant == variant) {
            return &buki_combo_transitions[index];
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

BOOL SudekiMpLanArenaBukiActionFromNativePresentation(
    int selector,
    uint8_t state,
    uint8_t *action_variant
) {
    size_t index;
    if (action_variant == NULL || state == 192u) return FALSE;
    if (state != 1u && state != 65u) return FALSE;
    /* These selectors alone are ambiguous; never assume weak. */
    if (selector == 53 || selector == 54) return FALSE;
    for (index = 0u;
         index < sizeof(buki_combo_transitions) /
             sizeof(buki_combo_transitions[0]);
         ++index) {
        if (buki_combo_transitions[index].selector == selector &&
            !SudekiMpLanArenaBukiBodyAction(
                buki_combo_transitions[index].action_variant)) {
            *action_variant = buki_combo_transitions[index].action_variant;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL SudekiMpLanArenaBukiActionFromNativeAnimation(
    uint8_t animation_id, int selector, uint8_t state, uint8_t *action_variant
) {
    uint8_t variant;
    const TalComboTransition *transition;
    /* Positive semantic AND bank witnesses. The hold loop legitimately uses
     * state0/128; inactive192 is never an action. Keep the combo rules below
     * unchanged, especially ambiguous weak/strong selectors53/54. */
    if (action_variant != NULL &&
        (state == 0u || state == 1u || state == 64u || state == 65u ||
         state == 128u)) {
        switch (animation_id) {
        case 0x6au: variant = SUDEKIMP_LAN_ARENA_ACTION_BLOCK; break;
        case 0x6bu: variant = SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD; break;
        case 0x6cu: variant = SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE; break;
        case 0x82u: variant = SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK; break;
        case 0x6fu: variant = SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP; break;
        case 0x6du: variant = SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT; break;
        case 0x6eu: variant = SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT; break;
        default: variant = SUDEKIMP_LAN_ARENA_ACTION_NONE; break;
        }
        if (variant != SUDEKIMP_LAN_ARENA_ACTION_NONE) {
            transition = buki_transition_for_variant(variant);
            if (transition == NULL || transition->selector != selector)
                return FALSE;
            *action_variant = variant;
            return TRUE;
        }
    }
    if (action_variant == NULL || (state != 1u && state != 65u)) return FALSE;
    switch (animation_id) {
        case 0x72u: variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE; break;
        case 0x73u: variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO; break;
        case 0x74u: variant = SUDEKIMP_LAN_ARENA_ACTION_STRONG; break;
        case 0x75u: variant = SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO; break;
        default:
            return SudekiMpLanArenaBukiActionFromNativePresentation(
                selector, state, action_variant);
    }
    transition = buki_transition_for_variant(variant);
    if (transition == NULL || transition->selector != selector) return FALSE;
    *action_variant = variant;
    return TRUE;
}

BOOL SudekiMpLanArenaBukiBodyAction(uint8_t variant) {
    return variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT;
}

BOOL SudekiMpLanArenaActionCombatState(uint8_t variant, uint8_t *state) {
    if (state == NULL) return FALSE;
    if (SudekiMpLanArenaTalActionCombatState(variant, state)) return TRUE;
    if (variant == SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK) {
        *state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
        return TRUE;
    }
    if (variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT ||
        variant == SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT) {
        *state = SUDEKIMP_LAN_ARENA_COMBAT_BLOCK;
        return TRUE;
    }
    return FALSE;
}

BOOL SudekiMpLanArenaBukiActionToNativePresentation(
    uint8_t action_variant,
    int *selector,
    int *state
) {
    const TalComboTransition *transition;
    if (selector == NULL || state == NULL) return FALSE;
    transition = buki_transition_for_variant(action_variant);
    if (transition == NULL) return FALSE;
    *selector = transition->selector;
    *state = transition->replay_state;
    return TRUE;
}

BOOL SudekiMpLanArenaBukiActionCombatState(
    uint8_t action_variant,
    uint8_t *combat_state
) {
    const TalComboTransition *transition;
    if (combat_state == NULL) return FALSE;
    transition = buki_transition_for_variant(action_variant);
    if (transition == NULL) return FALSE;
    *combat_state = transition->combat_state;
    return TRUE;
}

/* Decompose a semantic action variant into the ordered melee attack kinds that
 * produce it. Kind codes: 1 = weak, 2 = strong, 3 = sweep, 4 = block. This is
 * the CHARACTER-INDEPENDENT meaning of the variant name (COMBO_SSS literally
 * means strong,strong,strong), so it is shared by every character's replay --
 * useful for inspecting combo composition, NOT for dispatching a whole combo
 * again per journal event. Playback submits one input per host action edge. */
BOOL SudekiMpLanArenaActionInputSequence(
    uint8_t action_variant,
    uint8_t kinds[3],
    size_t *count
) {
    if (kinds == NULL || count == NULL) return FALSE;
    *count = 0u;
    switch (action_variant) {
    case SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE:
        kinds[0] = 1u; *count = 1u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_STRONG:
        kinds[0] = 2u; *count = 1u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_SWEEP:
        kinds[0] = 3u; *count = 1u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_BLOCK:
        kinds[0] = 4u; *count = 1u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO:
        kinds[0] = 1u; kinds[1] = 1u; *count = 2u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO:
        kinds[0] = 2u; kinds[1] = 2u; *count = 2u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE:
        kinds[0] = 1u; kinds[1] = 1u; kinds[2] = 1u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS:
        kinds[0] = 1u; kinds[1] = 1u; kinds[2] = 2u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW:
        kinds[0] = 2u; kinds[1] = 1u; kinds[2] = 1u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS:
        kinds[0] = 2u; kinds[1] = 2u; kinds[2] = 2u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS:
        kinds[0] = 2u; kinds[1] = 1u; kinds[2] = 2u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW:
        kinds[0] = 2u; kinds[1] = 2u; kinds[2] = 1u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW:
        kinds[0] = 1u; kinds[1] = 2u; kinds[2] = 1u; *count = 3u; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE:
        kinds[0] = 1u; kinds[1] = 2u; kinds[2] = 2u; *count = 3u; return TRUE;
    default:
        return FALSE;
    }
}
