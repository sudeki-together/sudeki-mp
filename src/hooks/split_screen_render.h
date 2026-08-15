#ifndef SUDEKIMP_SPLIT_SCREEN_RENDER_H
#define SUDEKIMP_SPLIT_SCREEN_RENDER_H

#include <windows.h>

BOOL SudekiMpInstallSplitScreenRender(
    HMODULE game_module,
    BOOL enable_second_player_camera,
    BOOL enable_dual_camera_frame_cache,
    UINT toggle_second_player_camera_virtual_key,
    BOOL enable_skill_camera_routing
);
void SudekiMpSplitScreenBeginSkillCameraCall(void *caster);
void SudekiMpSplitScreenEndSkillCameraCall(void);
void SudekiMpSplitScreenClearSkillCamera(void *caster, const char *reason);
void SudekiMpUninstallSplitScreenRender(void);

#endif
