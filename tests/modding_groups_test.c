#include "modding/mod_groups.h"
#include <assert.h>
#include <string.h>
int SudekiMpModGroupsTests(void) {
    const char ini[] = "[Tal.BodyFace]\nPattern1=CL002_Tal_*Face*.SQX\n"
        "[Tal.OtherTextures]\nPattern1=*Tal*\n[future]\nIgnored=true\n";
    SudekiMpModGroups groups = {0};
    unsigned hero, cat;
    assert(SudekiMpModGroupsLoad(&groups, ini, strlen(ini)));
    SudekiMpModGroupsClassify(&groups, "CL002_Tal_New_FaceLR.SQX", 0, &hero, &cat);
    assert(hero == SUDEKIMP_MOD_TAL && cat == SUDEKIMP_MOD_BODY_FACE);
    SudekiMpModGroupsClassify(&groups, "tal.hom", 1, &hero, &cat);
    assert(hero == SUDEKIMP_MOD_TAL && cat == SUDEKIMP_MOD_MODELS);
    SudekiMpModGroupsClassify(&groups, "", 0, &hero, &cat);
    assert(hero == SUDEKIMP_MOD_WORLD && cat == SUDEKIMP_MOD_OTHER_TEXTURES);
    SudekiMpModGroupsClassify(&groups, "unknown.hom", 1, &hero, &cat);
    assert(hero == SUDEKIMP_MOD_WORLD && cat == SUDEKIMP_MOD_MODELS);
    assert(SudekiMpModGroupPatternMatch("a*b?d", "AxxBcd"));
    assert(!SudekiMpModGroupPatternMatch("a*b?d", "AxxBd"));
    assert(!SudekiMpModGroupsLoad(&groups, "[Tal.BodyFace]\nPattern1=\n", strlen("[Tal.BodyFace]\nPattern1=\n")));
    assert(groups.count == 2); /* Failed refresh retains the previous rules. */
    SudekiMpModGroupsFree(&groups);
    return 0;
}
