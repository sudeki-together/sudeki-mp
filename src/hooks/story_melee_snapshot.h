#ifndef SUDEKIMP_STORY_MELEE_SNAPSHOT_H
#define SUDEKIMP_STORY_MELEE_SNAPSHOT_H
#include "hooks/story_interaction_guard.h"
#include "hooks/story_block_input.h"

/* Names are part of the exact-image test report. No relocations occur in
 * these spans (DB1AF's 0x400000 is an arbiter FLAG, not an image pointer). */
static const struct {const char *name;uint32_t rva,size,hash;} story_melee_contracts[]={
    {"MeleeSnapshotSelection",0xdb189u,0xb2u,0x16a192d8u},
    {"MeleeSnapshotReturn",0xdb3f4u,7u,0xaf4f03ccu},
    {"MeleeSnapshotReject",0xdb436u,0x19u,0x180bc2b9u}
};
static inline BOOL SudekiMpStoryMeleeSnapshotImageExact(uint8_t *base,
    SudekiMpStoryInteractionReadable readable) {
    if(!SudekiMpStoryInteractionInputImageExact(base,readable) ||
        !SudekiMpStoryBlockImageExact(base,readable)) return FALSE;
    for(unsigned n=0;n<sizeof(story_melee_contracts)/sizeof(*story_melee_contracts);++n) {
        uint32_t hash=2166136261u;
        if(!readable(base+story_melee_contracts[n].rva,story_melee_contracts[n].size)) return FALSE;
        for(unsigned i=0;i<story_melee_contracts[n].size;++i)
            hash=(hash^base[story_melee_contracts[n].rva+i])*16777619u;
        if(hash!=story_melee_contracts[n].hash) return FALSE;
    }
    return TRUE;
}
/* Active interactions remain unavailable. For idle interactions the new
 * combat entry never reads/writes the host trigger singleton at all. */
static inline BOOL SudekiMpStoryMeleeSnapshotInteractionClear(uint8_t *base,void *actor,
    SudekiMpStoryInteractionReadable readable) {
    return SudekiMpStoryMeleeInteractionClear(base,actor,2u,readable);
}
#endif
