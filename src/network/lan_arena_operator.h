#ifndef SUDEKIMP_LAN_ARENA_OPERATOR_H
#define SUDEKIMP_LAN_ARENA_OPERATOR_H

/* Local operator commands are deliberately outside the LAN wire protocol.
 * Each isolated Wine prefix has its own NT object namespace, so opening this
 * auto-reset event can target the client process without desktop-global key
 * state or a remotely reachable control socket. */
#define SUDEKIMP_LAN_ARENA_HOST_COMBAT_TOGGLE_EVENT \
    L"Local\\SudekiMpLanArenaHostCombatToggleV1"
#define SUDEKIMP_LAN_ARENA_HOST_COMBAT_ON_EVENT \
    L"Local\\SudekiMpLanArenaHostCombatOnV1"
#define SUDEKIMP_LAN_ARENA_HOST_COMBAT_OFF_EVENT \
    L"Local\\SudekiMpLanArenaHostCombatOffV1"
#define SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT \
    L"Local\\SudekiMpLanArenaWeakAttackV1"
#define SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT \
    L"Local\\SudekiMpLanArenaClientWeakHoldV1"
/* Manual-reset test rails for deterministic held movement.  The two names
 * remain distinct even though host/client normally live in isolated Wine
 * prefixes, so an exact-image harness can install both adapters safely. */
#define SUDEKIMP_LAN_ARENA_HOST_FORWARD_HOLD_EVENT \
    L"Local\\SudekiMpLanArenaHostForwardHoldV1"
#define SUDEKIMP_LAN_ARENA_CLIENT_FORWARD_HOLD_EVENT \
    L"Local\\SudekiMpLanArenaClientForwardHoldV1"
#define SUDEKIMP_LAN_ARENA_HOST_STRONG_ATTACK_EVENT \
    L"Local\\SudekiMpLanArenaHostStrongAttackV1"
#define SUDEKIMP_LAN_ARENA_HOST_SWEEP_ATTACK_EVENT \
    L"Local\\SudekiMpLanArenaHostSweepAttackV1"
#define SUDEKIMP_LAN_ARENA_HOST_BLOCK_EVENT \
    L"Local\\SudekiMpLanArenaHostBlockV1"
/* Host-only, auto-reset Spirit test rails. They request one of Tal's two
 * retail QuickMenu variants; native validation and execution remain owned by
 * the authoritative game-thread runtime. */
#define SUDEKIMP_LAN_ARENA_HOST_SPIRIT_VARIANT_ONE_EVENT \
    L"Local\\SudekiMpLanArenaHostSpiritVariant1V1"
#define SUDEKIMP_LAN_ARENA_HOST_SPIRIT_VARIANT_TWO_EVENT \
    L"Local\\SudekiMpLanArenaHostSpiritVariant2V1"
/* Auto-reset acknowledgement for an observed native Tal selector edge. */
#define SUDEKIMP_LAN_ARENA_HOST_ACTION_ACK_EVENT \
    L"Local\\SudekiMpLanArenaHostActionAckV1"
#define SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_LEFT_EVENT \
    L"Local\\SudekiMpLanArenaClientCameraLeftV1"
#define SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_RIGHT_EVENT \
    L"Local\\SudekiMpLanArenaClientCameraRightV1"
#define SUDEKIMP_LAN_ARENA_SKILL_ZERO_EVENT \
    L"Local\\SudekiMpLanArenaSkill0V1"
#define SUDEKIMP_LAN_ARENA_SKILL_ONE_EVENT \
    L"Local\\SudekiMpLanArenaSkill1V1"
#define SUDEKIMP_LAN_ARENA_SKILL_TWO_EVENT \
    L"Local\\SudekiMpLanArenaSkill2V1"
#define SUDEKIMP_LAN_ARENA_SKILL_THREE_EVENT \
    L"Local\\SudekiMpLanArenaSkill3V1"
#define SUDEKIMP_LAN_ARENA_SKILL_FOUR_EVENT \
    L"Local\\SudekiMpLanArenaSkill4V1"
#define SUDEKIMP_LAN_ARENA_SKILL_FIVE_EVENT \
    L"Local\\SudekiMpLanArenaSkill5V1"

#endif
