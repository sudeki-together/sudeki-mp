#ifndef SUDEKIMP_LAN_ARENA_SKILL_FADE_H
#define SUDEKIMP_LAN_ARENA_SKILL_FADE_H
#include <windows.h>

/* Game/render thread only. FALSE means no proven cast-view override. */
typedef BOOL (*SudekiMpLanArenaSkillFadeWitness)(float rgb[3]);
/* Optional owner-validated camera scope at the SAME primary draw callsite.
 * End must tolerate a partially admitted Begin and retain any failed restore.
 * Both callbacks remain installed until End positively drains the lease. */
typedef BOOL (*SudekiMpLanArenaDrawViewBoundary)(void);
BOOL SudekiMpInstallLanArenaSkillFade(HMODULE image, SudekiMpLanArenaSkillFadeWitness witness);
BOOL SudekiMpInstallLanArenaSkillFadeWithDrawView(HMODULE image,
    SudekiMpLanArenaSkillFadeWitness witness,
    SudekiMpLanArenaDrawViewBoundary begin,
    SudekiMpLanArenaDrawViewBoundary end);
BOOL SudekiMpUninstallLanArenaSkillFade(void);
BOOL SudekiMpLanArenaReadSkillLight(float current[3], float baseline[3]);
#endif
