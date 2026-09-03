#ifndef SUDEKIMP_LAN_ARENA_OPERATOR_H
#define SUDEKIMP_LAN_ARENA_OPERATOR_H

/* Local operator commands are deliberately outside the LAN wire protocol.
 * Each isolated Wine prefix has its own NT object namespace, so opening this
 * auto-reset event can target the client process without desktop-global key
 * state or a remotely reachable control socket. */
#define SUDEKIMP_LAN_ARENA_CLIENT_COMBAT_TOGGLE_EVENT \
    L"Local\\SudekiMpLanArenaClientCombatToggleV1"
#define SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT \
    L"Local\\SudekiMpLanArenaWeakAttackV1"
#define SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT \
    L"Local\\SudekiMpLanArenaClientWeakHoldV1"
#define SUDEKIMP_LAN_ARENA_HOST_STRONG_ATTACK_EVENT \
    L"Local\\SudekiMpLanArenaHostStrongAttackV1"
#define SUDEKIMP_LAN_ARENA_HOST_SWEEP_ATTACK_EVENT \
    L"Local\\SudekiMpLanArenaHostSweepAttackV1"
#define SUDEKIMP_LAN_ARENA_HOST_BLOCK_EVENT \
    L"Local\\SudekiMpLanArenaHostBlockV1"
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
