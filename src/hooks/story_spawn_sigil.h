#ifndef SUDEKIMP_STORY_SPAWN_SIGIL_H
#define SUDEKIMP_STORY_SPAWN_SIGIL_H

#include <stdint.h>

/* Supported archive: SFXKK001_SPAWN_EFFECT.HOM, also observed on both
 * sides of the castle-road ambush. ResourceName's low seven bits are kind;
 * its other bits are flags, not part of the resource type. A SpecialEffect
 * class or a name containing "spawn" is not enough. Keep unknown assets out.
 * The caller must separately prove the current native entity and resource. */
static inline int SudekiMpStoryIsSpawnSigilResource(uint32_t kind,uint32_t identifier) {
    return (kind&0x7fu)==0x29u && identifier==0x7e8a6f3fu;
}

#endif
