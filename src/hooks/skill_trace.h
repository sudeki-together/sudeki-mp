#ifndef SUDEKIMP_SKILL_TRACE_H
#define SUDEKIMP_SKILL_TRACE_H

#include <windows.h>

BOOL SudekiMpInstallSkillTrace(
    HMODULE game_module,
    float plasmatica_speed,
    float plasmatica_camera_speed
);
void SudekiMpUninstallSkillTrace(void);

#endif
