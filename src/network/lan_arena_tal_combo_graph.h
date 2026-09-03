#ifndef SUDEKIMP_LAN_ARENA_TAL_COMBO_GRAPH_H
#define SUDEKIMP_LAN_ARENA_TAL_COMBO_GRAPH_H

#include "network/lan_arena_protocol.h"

#include <windows.h>
#include <stdint.h>

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

#endif
