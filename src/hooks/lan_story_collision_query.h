#ifndef SUDEKIMP_LAN_STORY_COLLISION_QUERY_H
#define SUDEKIMP_LAN_STORY_COLLISION_QUERY_H

#include <windows.h>
#include "engine/story_area.h"
#include "hooks/lan_story_collision_lifetime.h"

enum { SUDEKIMP_STORY_COLLISION_QUERY_SOURCES=8192 };
typedef struct SudekiMpLanStoryCollisionSourceArea {
    void *source;
    uint64_t incarnation;
    uint32_t area_slot;
} SudekiMpLanStoryCollisionSourceArea;
typedef struct SudekiMpLanStoryCollisionQuerySet {
    void *grid;
    uint64_t lifetime_revision;
    SudekiMpStoryAreaRef areas[SUDEKIMP_STORY_AREAS];
    /* Strictly sorted by unsigned source address; covers the ENTIRE grid,
     * including disabled sources. Unknown ownership cannot be omitted. */
    const SudekiMpLanStoryCollisionSourceArea *sources;
    uint32_t count;
} SudekiMpLanStoryCollisionQuerySet;
typedef enum SudekiMpLanStoryCollisionQueryResult {
    SUDEKIMP_STORY_COLLISION_NOT_RUN=0,
    SUDEKIMP_STORY_COLLISION_COMPLETE,
    SUDEKIMP_STORY_COLLISION_INCOMPLETE
} SudekiMpLanStoryCollisionQueryResult;

/* Exact-build low-level adapter; NOT installed by story runtime yet. The
 * coordinator must hold native source/grid/position/hit-buffer lifetimes for
 * the whole synchronous call, and prove source-to-area enrollment afresh.
 * Policy pins below prevent policy retirement; they do NOT retain native
 * objects or turn copied observations into leases. No loading-phase authority
 * is granted by this interface. Never construct the set from coordinates or
 * global current room. The passive lifetime observer must already be installed
 * from suspended startup. Every source incarnation and the common revision
 * must match it; replacement at the same address invalidates the query. This
 * is detection, not native retention. Install query hooks during that same
 * suspended startup. Queries bind to the lifetime observer's native owner thread.
 *
 * Only this explicit movement query is filtered. Other native callers of the
 * broad phase retain original behavior. No grid links, masks or enabled bits
 * are edited. The callback performs bounded lookup in a private copied set.
 *
 * COMPLETE publishes the native hit count. NOT_RUN leaves native output alone.
 * INCOMPLETE means native collection ran but its hit buffer MUST NOT be consumed
 * as valid collision (and MUST NOT be interpreted as no floor). All failures
 * leave *hits unchanged. No fallback to an unscoped query is performed. */
BOOL SudekiMpLanStoryCollisionQueryInstall(HMODULE);
BOOL SudekiMpLanStoryCollisionQueryUninstall(void);
SudekiMpLanStoryCollisionQueryResult SudekiMpLanStoryCollisionQueryRun(
    SudekiMpStoryAreas *,const SudekiMpLanStoryCollisionQuerySet *,
    SudekiMpStoryAreaRef query_area,void *query_source,
    const float position[3],float radius,void *hit_buffer,uint32_t *hits);

#endif
