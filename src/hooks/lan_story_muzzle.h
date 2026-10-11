#ifndef SUDEKIMP_LAN_STORY_MUZZLE_H
#define SUDEKIMP_LAN_STORY_MUZZLE_H
#include "network/lan_story_frame.h"
#include "network/lan_story_vfx.h"

typedef struct SudekiMpStoryMuzzle {
    BOOL valid;
    uint8_t item;
    float position[3];
} SudekiMpStoryMuzzle;
/* Read-only geometry, on the native thread inside a fresh client roster
 * presentation proof. No native update, allocation or borrowed pointer escapes.
 * The caller has already checked the supported executable identity. */
BOOL SudekiMpStoryMuzzleImageMatches(HMODULE image);
BOOL SudekiMpStoryMuzzleObserve(HMODULE image,void *actor,uint8_t actor_type,
    SudekiMpStoryMuzzle *out);

typedef struct SudekiMpStoryMuzzleEntry {
    uint32_t instance,tick,generation;
    uint8_t kind,character,item;
    float origin[3],offset[3],distance;
} SudekiMpStoryMuzzleEntry;
typedef struct SudekiMpStoryMuzzleProjection {
    uint64_t stream;
    uint32_t epoch,revision;
    SudekiMpStoryMuzzleEntry entries[SUDEKIMP_STORY_VFX_CAPACITY];
} SudekiMpStoryMuzzleProjection;
/* Cosmetic copy only. Caller proves connection, scene and fresh frame/tips.
 * Native host hits, phases, identities, impacts and lifetimes stay unchanged.
 * Projectile launch displacement tapers over two world units, never a timer.
 * Returns the local ready mask. Unprepared ordinary entries stay in the host
 * roster, but must not spawn/synchronize at their uncorrected coordinates. */
uint64_t SudekiMpStoryMuzzleProject(SudekiMpStoryMuzzleProjection *cache,
    SudekiMpStoryVfx *copy,const SudekiMpLanStoryFrame *frame,
    const SudekiMpStoryMuzzle tips[4]);
/* Unaffected skills, spirits and impacts need no local muzzle geometry. */
uint64_t SudekiMpStoryMuzzleUnchangedMask(const SudekiMpStoryVfx *state);
#endif
