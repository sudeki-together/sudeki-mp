#ifndef SUDEKIMP_CONTROL_SEPARATION_H
#define SUDEKIMP_CONTROL_SEPARATION_H

#include <windows.h>

BOOL SudekiMpInstallControlSeparation(
    HMODULE game_module,
    UINT toggle_virtual_key,
    BOOL enable_second_player_movement,
    BOOL enable_camera_relative_movement,
    BOOL enable_separation_guard,
    float maximum_separation,
    BOOL enable_second_player_weak_attack,
    UINT weak_attack_virtual_key,
    BOOL enable_target_trace,
    BOOL enable_shared_group_camera
);
void SudekiMpUninstallControlSeparation(void);

#endif
