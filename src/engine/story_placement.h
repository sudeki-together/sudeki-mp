#ifndef SUDEKIMP_STORY_PLACEMENT_H
#define SUDEKIMP_STORY_PLACEMENT_H

#include <stddef.h>
#include <stdint.h>

enum { SUDEKIMP_STORY_PLACEMENT_MAX=512, SUDEKIMP_STORY_PLACEMENT_NAME=96 };
/* Copied from the native AUTHORED spawn record, which outlives its entity.
 * The region and named spawn point identify a placement; the template and
 * initial transform verify it. Never infer identity from a current moving
 * pose, a registry index or nearest-object search. Full text fences hash aliases. */
typedef struct SudekiMpStoryPlacement {
    uint32_t zone,anchor,resource;
    char anchor_name[SUDEKIMP_STORY_PLACEMENT_NAME],name[SUDEKIMP_STORY_PLACEMENT_NAME];
    uint32_t position[3],forward[3];
} SudekiMpStoryPlacement;
typedef struct SudekiMpStoryPlacementScope {
    uint8_t save_identity[32];
    uint64_t visit;
    char world[64],temporary[64];
} SudekiMpStoryPlacementScope;
typedef struct SudekiMpStoryPlacementCatalog {
    SudekiMpStoryPlacementScope scope;
    uint32_t bound,count;
    /* Strictly sorted unique keys. Source ID is index+1, scoped by save/visit.
     * It is NOT the changing registry index or a hash of a native pointer. */
    SudekiMpStoryPlacement placements[SUDEKIMP_STORY_PLACEMENT_MAX];
} SudekiMpStoryPlacementCatalog;

int SudekiMpStoryPlacementMake(SudekiMpStoryPlacement *,uint32_t zone,
    uint32_t anchor,const char *anchor_name,uint32_t resource,const char *name,
    const float position[3],const float forward[3]);
int SudekiMpStoryPlacementValid(const SudekiMpStoryPlacement *);
int SudekiMpStoryPlacementCatalogValid(const SudekiMpStoryPlacementCatalog *);
/* Destination starts zeroed. Once bound, only an identical complete INITIAL
 * catalog is accepted. Removing an entry or advancing a visit never silently
 * rebuilds/reindexes it. The native world owner retires it only after a proved
 * world teardown; reconnect/party changes are not a new catalog lifetime. */
int SudekiMpStoryPlacementCatalogBind(SudekiMpStoryPlacementCatalog *,
    const SudekiMpStoryPlacementScope *,const SudekiMpStoryPlacement *,size_t count);
int SudekiMpStoryPlacementSource(const SudekiMpStoryPlacementCatalog *,
    const SudekiMpStoryPlacement *,uint64_t *source);
/* Match a complete locally captured INITIAL set to the host's immutable set.
 * map[host source-1] = local initial row. No partial/nearest fallback. Missing,
 * extra, duplicate, shifted, wrong-world/save/visit sets all fail unchanged.
 * A late join matches the original catalog, then consumes the break history;
 * it must NOT rebuild the host catalog from surviving objects. */
int SudekiMpStoryPlacementMatch(const SudekiMpStoryPlacementCatalog *,
    const SudekiMpStoryPlacementScope *,const SudekiMpStoryPlacement *,size_t count,
    uint16_t *map,size_t capacity);

#endif
