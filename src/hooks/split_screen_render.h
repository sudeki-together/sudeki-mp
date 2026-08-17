#ifndef SUDEKIMP_SPLIT_SCREEN_RENDER_H
#define SUDEKIMP_SPLIT_SCREEN_RENDER_H

#include <windows.h>

typedef void (*SudekiMpSplitScreenOverlayRenderer)(void);

enum {
    SUDEKIMP_QUICK_MENU_ISOLATION_IDLE = 0u,
    SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE = 1u,
    SUDEKIMP_QUICK_MENU_ISOLATION_FAILED = 2u,
    SUDEKIMP_QUICK_MENU_ISOLATION_TAIL = 3u
};

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
BOOL SudekiMpSplitScreenQuickMenuLiveViewAccepted(
    BOOL isolation_requested,
    BOOL resources_ready,
    BOOL player_two_requested_before_apply,
    BOOL player_two_rendered
);
unsigned int SudekiMpSplitScreenQuickMenuIsolationBeginState(
    unsigned int state,
    BOOL was_visible,
    BOOL visible,
    BOOL eligible_on_rising_edge
);
unsigned int SudekiMpSplitScreenQuickMenuIsolationEndState(
    unsigned int state,
    BOOL quick_menu_submit_seen
);
unsigned int SudekiMpSplitScreenQuickMenuIsolationCancelState(
    BOOL quick_menu_visible
);
BOOL SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
    BOOL isolation_in_progress,
    BOOL render_phase_confirmed,
    BOOL player_two_expected,
    BOOL player_two_rendered
);
void SudekiMpSplitScreenSetOverlayRenderer(
    SudekiMpSplitScreenOverlayRenderer renderer
);
BOOL SudekiMpTransformPlayerTwoMovement(
    const float local_direction[3],
    float world_direction[3]
);
BOOL SudekiMpAlignPlayerTwoFacingToCamera(void *character);
BOOL SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(void *character);
void SudekiMpSplitScreenBeginSkillCameraCall(void *caster);
void SudekiMpSplitScreenEndSkillCameraCall(void);
void SudekiMpSplitScreenClearSkillCamera(void *caster, const char *reason);
void SudekiMpUninstallSplitScreenRender(void);

#endif
