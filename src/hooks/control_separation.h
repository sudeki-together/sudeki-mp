#ifndef SUDEKIMP_CONTROL_SEPARATION_H
#define SUDEKIMP_CONTROL_SEPARATION_H

#include <windows.h>

typedef void (*SudekiMpControlUpdateObserver)(void);

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT toggle_virtual_key,
    BOOL enable_second_player_movement,
    BOOL enable_camera_relative_movement,
    BOOL enable_separation_guard,
    float maximum_separation,
    BOOL enable_second_player_weak_attack,
    UINT weak_attack_virtual_key,
    BOOL enable_second_player_skills,
    const UINT second_player_skill_virtual_keys[4],
    BOOL enable_target_trace,
    BOOL enable_shared_group_camera,
    BOOL enable_input_bridge,
    float input_bridge_deadzone
);
BOOL SudekiMpControlSeparationRequestPlayerTwo(BOOL enabled);
BOOL SudekiMpControlSeparationPlayerTwoRequested(void);
BOOL SudekiMpControlSeparationPlayerTwoActive(void);
BOOL SudekiMpControlSeparationInputReady(void);
void SudekiMpControlSeparationSetUpdateObserver(
    SudekiMpControlUpdateObserver observer
);
void SudekiMpUninstallControlSeparation(void);

#endif
