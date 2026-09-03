#include "network/lan_arena_tal_combo_graph.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

int main(void) {
    static const struct {
        uint8_t variant;
        int selector;
        int replay_state;
        uint8_t combat_state;
    } expected[] = {
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
    unsigned int index;
    for (index = 0u; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        uint8_t variant = 0u;
        uint8_t combat_state = 0u;
        int selector = 0;
        int state = 0;
        CHECK(SudekiMpLanArenaTalActionFromNativePresentation(
            expected[index].selector, 1u, &variant));
        CHECK(variant == expected[index].variant);
        CHECK(SudekiMpLanArenaTalActionToNativePresentation(
            expected[index].variant, &selector, &state));
        CHECK(selector == expected[index].selector);
        CHECK(state == expected[index].replay_state);
        CHECK(SudekiMpLanArenaTalActionCombatState(
            expected[index].variant, &combat_state));
        CHECK(combat_state == expected[index].combat_state);
    }
    {
        uint8_t variant = 0u;
        CHECK(SudekiMpLanArenaTalActionFromNativePresentation(21, 128u,
            &variant));
        CHECK(variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK);
        CHECK(!SudekiMpLanArenaTalActionFromNativePresentation(54, 192u,
            &variant));
        CHECK(!SudekiMpLanArenaTalActionFromNativePresentation(3, 1u,
            &variant));
        CHECK(!SudekiMpLanArenaTalActionToNativePresentation(
            SUDEKIMP_LAN_ARENA_ACTION_NONE, NULL, NULL));
    }
    if (failures != 0) return 1;
    puts("LAN Tal combo graph checks passed");
    return 0;
}
