#ifndef SUDEKIMP_QUICK_SKILL_INPUT_H
#define SUDEKIMP_QUICK_SKILL_INPUT_H

#include <windows.h>

BOOL SudekiMpInstallQuickSkillInputTrace(
    HMODULE game_module,
    BOOL enable_ranged_prototype,
    BOOL enable_realtime_targeting_guard
);
void SudekiMpUninstallQuickSkillInputTrace(void);

#endif
