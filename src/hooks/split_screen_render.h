#ifndef SUDEKIMP_SPLIT_SCREEN_RENDER_H
#define SUDEKIMP_SPLIT_SCREEN_RENDER_H

#include <windows.h>

typedef void (*SudekiMpSplitScreenOverlayRenderer)(void);

BOOL SudekiMpInstallSplitScreenRender(
    HMODULE game_module,
    BOOL enable_second_player_camera,
    BOOL enable_dual_camera_frame_cache,
    UINT toggle_second_player_camera_virtual_key,
    BOOL enable_skill_camera_routing,
    BOOL enable_second_player_controller_camera,
    BOOL enable_split_screen_ranged_model_isolation,
    BOOL enable_spirit_strike_viewport_effect_isolation,
    float controller_camera_deadzone,
    float controller_camera_yaw_speed,
    float controller_camera_pitch_speed,
    float controller_camera_maximum_pitch
);
BOOL SudekiMpSplitScreenSetRuntimeEnabled(BOOL enabled);
BOOL SudekiMpSplitScreenRuntimeEnabled(void);
void SudekiMpSplitScreenSetOverlayRenderer(
    SudekiMpSplitScreenOverlayRenderer renderer
);
BOOL SudekiMpTransformPlayerTwoMovement(
    const float local_direction[3],
    float world_direction[3]
);
BOOL SudekiMpAlignPlayerTwoFacingToCamera(void *character);
void SudekiMpSplitScreenBeginSkillCameraCall(void *caster);
void SudekiMpSplitScreenEndSkillCameraCall(void);
void SudekiMpSplitScreenClearSkillCamera(void *caster, const char *reason);
void SudekiMpUninstallSplitScreenRender(void);

#endif
