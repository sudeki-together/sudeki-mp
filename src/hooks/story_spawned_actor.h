#ifndef SUDEKIMP_STORY_SPAWNED_ACTOR_H
#define SUDEKIMP_STORY_SPAWNED_ACTOR_H
#include <stdint.h>

/* Group-spawned actors the host replicates by spawn serial (#54 D2, E2).
 * MonsterEntity and AllyEntity (the castle-road gate guards) construct the
 * same component classes at the same offsets (CONFIRMED_STATIC: constructors
 * 0x5460A0 / 0x53EF20) and share the update node 0x553240. Only the primary,
 * update-node and resource tables, the destructor and the spawn entry's
 * resource type (0x19 monster, 0x02 ally) differ. Pure. */
enum { SUDEKIMP_STORY_MONSTER_VTABLE=0x2d5fa0u, SUDEKIMP_STORY_ALLY_VTABLE=0x2d55d4u };
/* The exact primary table a spawn entry's resource type creates, or 0. */
static inline unsigned SudekiMpStorySpawnedPrimary(uint32_t entry_type) {
    entry_type&=0x7fu;
    return entry_type==0x19u?SUDEKIMP_STORY_MONSTER_VTABLE:entry_type==0x02u?SUDEKIMP_STORY_ALLY_VTABLE:0u;
}
static inline int SudekiMpStorySpawnedClass(const uint8_t *image,const void *vtable) {
    return image && vtable &&
        (vtable==image+SUDEKIMP_STORY_MONSTER_VTABLE || vtable==image+SUDEKIMP_STORY_ALLY_VTABLE);
}
static inline const char *SudekiMpStorySpawnedName(const uint8_t *image,const void *vtable) {
    return image && vtable==image+SUDEKIMP_STORY_ALLY_VTABLE?"ally":"monster";
}
#endif
