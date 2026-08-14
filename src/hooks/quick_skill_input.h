#ifndef SUDEKIMP_QUICK_SKILL_INPUT_H
#define SUDEKIMP_QUICK_SKILL_INPUT_H

#include <windows.h>

BOOL SudekiMpInstallQuickSkillInputTrace(
    HMODULE game_module,
    BOOL enable_ranged_prototype
);
void SudekiMpUninstallQuickSkillInputTrace(void);

#endif
