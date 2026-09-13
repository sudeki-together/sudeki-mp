#ifndef SUDEKIMP_LAN_ARENA_TAL_COMBO_GRAPH_H
#define SUDEKIMP_LAN_ARENA_TAL_COMBO_GRAPH_H

#include "network/lan_arena_protocol.h"

#include <windows.h>
#include <stdint.h>
#include <stddef.h>

/* Exact supported-image adapter between Tal's native presentation tree and
 * the process-independent LAN action journal. It is deliberately the single
 * source of truth used in both host capture and client replay. */
BOOL SudekiMpLanArenaTalActionFromNativePresentation(
    int selector,
    uint8_t state,
    uint8_t *action_variant
);

BOOL SudekiMpLanArenaTalActionToNativePresentation(
    uint8_t action_variant,
    int *selector,
    int *state
);

BOOL SudekiMpLanArenaTalActionCombatState(
    uint8_t action_variant,
    uint8_t *combat_state
);

/* Buki's positional-selector adapter (see lan_arena_tal_combo_graph.c for the
 * mapping rationale). Same action-variant space as Tal; only the native
 * selector values differ. */
BOOL SudekiMpLanArenaBukiActionFromNativePresentation(
    int selector,
    uint8_t state,
    uint8_t *action_variant
);

/* Semantic channel plus matching renderer observation disambiguates Buki's
 * shared first/second-swing selectors. No native playback is performed. */
BOOL SudekiMpLanArenaBukiActionFromNativeAnimation(
    uint8_t animation_id, int selector, uint8_t state, uint8_t *action_variant
);

BOOL SudekiMpLanArenaBukiActionToNativePresentation(
    uint8_t action_variant,
    int *selector,
    int *state
);

BOOL SudekiMpLanArenaBukiActionCombatState(
    uint8_t action_variant,
    uint8_t *combat_state
);

/* Host-authored Buki body phases use the bounded channel frame, not another
 * native combat-input submission. Ordinary combo admission remains separate. */
BOOL SudekiMpLanArenaBukiBodyAction(uint8_t action_variant);
BOOL SudekiMpLanArenaActionCombatState(
    uint8_t action_variant, uint8_t *combat_state
);

/* Character-independent: decompose a variant into the ordered melee attack
 * kinds (1 weak / 2 strong / 3 sweep / 4 block) that produce it. */
BOOL SudekiMpLanArenaActionInputSequence(
    uint8_t action_variant,
    uint8_t kinds[3],
    size_t *count
);

#endif
