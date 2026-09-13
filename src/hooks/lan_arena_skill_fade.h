#ifndef SUDEKIMP_LAN_ARENA_SKILL_FADE_H
#define SUDEKIMP_LAN_ARENA_SKILL_FADE_H
#include <windows.h>

/* Game/render thread only. FALSE means no proven cast-view override. */
typedef BOOL (*SudekiMpLanArenaSkillFadeWitness)(float rgb[3]);
BOOL SudekiMpInstallLanArenaSkillFade(HMODULE image, SudekiMpLanArenaSkillFadeWitness witness);
BOOL SudekiMpUninstallLanArenaSkillFade(void);
BOOL SudekiMpLanArenaReadSkillLight(float current[3], float baseline[3]);
#endif
