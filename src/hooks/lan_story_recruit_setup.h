#ifndef SUDEKIMP_LAN_STORY_RECRUIT_SETUP_H
#define SUDEKIMP_LAN_STORY_RECRUIT_SETUP_H

#include <windows.h>
#include <stdint.h>

typedef BOOL (*SudekiMpLanStoryRecruitSetupOwnerExact)(void *actor,void *context);
/* Plain transaction record owned by the recruitment coordinator. Initialize
 * to zero for each newly proved native creation. Borrowed pointers are only
 * identity comparisons until owner_exact freshly proves the paused actor.
 * attempted bits prevent replay after an ambiguous native return. */
typedef struct SudekiMpLanStoryRecruitSetup {
    void *actor,*experience,*stats;
    uint32_t thread,attempted,completed;
    float before_xp,expected_xp,level,ability_points,sp,max_sp,expected_sp,expected_max_sp;
    int16_t before_skill,before_bonus;
    uint8_t stats_before[0x64];
    int16_t abilities_before[41];
    BOOL initialized,active,failed;
    const char *reason; /* Static diagnostic text; never a native owner. */
} SudekiMpLanStoryRecruitSetup;

BOOL SudekiMpLanStoryRecruitSetupInitialize(HMODULE image);
/* Exact Lighthouse InitialSetup output on the authored PC_AILISH level3
 * resource: top XP up to201, GiveAbility(Skill2Alice,1), GiveAbility(SPAlice,1).
 * Actual level/XP table prove AddExperience cannot enter DoLevelUp. No GEL
 * task is run and no raw stat writes, level-up effects, or UI events occur.
 * A lower/different native default fails before mutation; it is not clamped.
 * owner_exact must prove the new actor's creation, pause enrollment and
 * no independent gameplay admission before and after every native call. */
BOOL SudekiMpLanStoryRecruitSetupApply(SudekiMpLanStoryRecruitSetup *,void *actor,
    SudekiMpLanStoryRecruitSetupOwnerExact,void *context);

#endif
