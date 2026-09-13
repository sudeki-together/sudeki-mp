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
    {
        static const uint8_t ids[] = { 0x72u, 0x73u, 0x74u, 0x75u };
        static const int selectors[] = { 53, 54, 53, 54 };
        static const uint8_t variants[] = {
            SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
            SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO,
            SUDEKIMP_LAN_ARENA_ACTION_STRONG,
            SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO
        };
        uint8_t variant = 0u;
        for (index = 0u; index < sizeof(ids); ++index) {
            int selector = -1, state = -1;
            CHECK(SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[index], selectors[index], 1u, &variant));
            CHECK(variant == variants[index]);
            CHECK(SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[index], selectors[index], 65u, &variant));
            CHECK(variant == variants[index]);
            CHECK(SudekiMpLanArenaBukiActionToNativePresentation(
                variant, &selector, &state));
            CHECK(selector == selectors[index] && state == 1);
            CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[index], selectors[index], 192u, &variant));
            CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[index], selectors[index] == 53 ? 54 : 53, 1u, &variant));
            CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[index], selectors[index], 1u, NULL));
            CHECK(!SudekiMpLanArenaBukiActionFromNativePresentation(
                selectors[index], 1u, &variant));
        }
        CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(0u, 53, 1u, &variant));
        CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(2u, 20, 0u, &variant));
        CHECK(SudekiMpLanArenaBukiActionFromNativeAnimation(0x7fu, 64, 65u, &variant));
        CHECK(variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE);
    }
    {
        const uint8_t ids[] = {0x6a,0x6b,0x6c,0x82,0x6f,0x6d,0x6e};
        const int selectors[] = {44,45,46,69,49,47,48};
        const uint8_t variants[] = {SUDEKIMP_LAN_ARENA_ACTION_BLOCK,
            SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD,
            SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE,
            SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK,
            SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP,
            SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT,
            SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT};
        const uint8_t states[] = {0,1,64,65,128};
        unsigned int i,j;
        for (i=0; i<7; ++i) {
            uint8_t variant=0, combat=0;
            int selector=0,state=0;
            CHECK(SudekiMpLanArenaBukiBodyAction(variants[i]));
            CHECK(SudekiMpLanArenaBukiActionToNativePresentation(
                variants[i], &selector, &state));
            CHECK(selector == selectors[i]);
            CHECK(SudekiMpLanArenaActionCombatState(variants[i], &combat));
            CHECK(combat == (i==3 ? SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK :
                SUDEKIMP_LAN_ARENA_COMBAT_BLOCK));
            for (j=0; j<sizeof(states); ++j) {
                CHECK(SudekiMpLanArenaBukiActionFromNativeAnimation(
                    ids[i], selectors[i], states[j], &variant));
                CHECK(variant == variants[i]);
                CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                    ids[i], 20, states[j], &variant));
                CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                    0, selectors[i], states[j], &variant));
            }
            CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[i], selectors[i], 192, &variant));
            CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[i], selectors[i], 2, &variant));
            CHECK(!SudekiMpLanArenaBukiActionFromNativeAnimation(
                ids[i], selectors[i], 1, NULL));
            if (i != 0) CHECK(!SudekiMpLanArenaTalActionToNativePresentation(
                variants[i], &selector, &state));
        }
        CHECK(!SudekiMpLanArenaBukiBodyAction(SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE));
        CHECK(!SudekiMpLanArenaBukiBodyAction(SUDEKIMP_LAN_ARENA_ACTION_SWEEP));
    }
    if (failures != 0) return 1;
    puts("LAN Tal combo graph checks passed");
    return 0;
}
