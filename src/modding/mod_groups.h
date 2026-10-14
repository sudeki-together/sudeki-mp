#ifndef SUDEKIMP_MOD_GROUPS_H
#define SUDEKIMP_MOD_GROUPS_H
#include <stddef.h>

/* Name-based grouping is a configurable heuristic, not an item/model link. */
enum SudekiMpModCharacter {
    SUDEKIMP_MOD_TAL, SUDEKIMP_MOD_AILISH, SUDEKIMP_MOD_BUKI,
    SUDEKIMP_MOD_ELCO, SUDEKIMP_MOD_WORLD
};
enum SudekiMpModCategory {
    SUDEKIMP_MOD_WEAPONS, SUDEKIMP_MOD_ARMOUR, SUDEKIMP_MOD_BODY_FACE,
    SUDEKIMP_MOD_OTHER_TEXTURES, SUDEKIMP_MOD_MODELS
};
typedef struct SudekiMpModGroupRule {
    unsigned character, category;
    char pattern[128];
} SudekiMpModGroupRule;
typedef struct SudekiMpModGroups {
    SudekiMpModGroupRule *rules;
    size_t count;
} SudekiMpModGroups;
/* ASCII INI: [Tal.Weapons] Pattern1=CL002_Tal_*Sword*, etc.
 * Characters: Tal, Ailish, Buki, Elco, World.
 * Categories: Weapons, Armour, BodyFace, OtherTextures, Models.
 * Unknown sections/keys are ignored; malformed recognized rules fail.
 * First matching rule wins, so broad fallback patterns should come last. */
int SudekiMpModGroupsLoad(SudekiMpModGroups *groups, const void *data, size_t size);
void SudekiMpModGroupsFree(SudekiMpModGroups *groups);
int SudekiMpModGroupPatternMatch(const char *pattern, const char *name);
/* Unmatched names (including unnamed resources) always remain visible in
 * World/OtherTextures, or World/Models for a model. A model is never grouped
 * as a texture; matching character texture patterns still identify its hero. */
void SudekiMpModGroupsClassify(const SudekiMpModGroups *groups, const char *name,
                              int model, unsigned *character, unsigned *category);
#endif
