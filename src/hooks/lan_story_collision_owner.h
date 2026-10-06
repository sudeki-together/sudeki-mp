#ifndef SUDEKIMP_LAN_STORY_COLLISION_OWNER_H
#define SUDEKIMP_LAN_STORY_COLLISION_OWNER_H

#include "hooks/lan_story_observer.h"

enum { SUDEKIMP_STORY_COLLISION_TERRAIN=1, SUDEKIMP_STORY_COLLISION_ENTITY=2 };
/* A synchronous identity observation, NOT an area/lifetime lease or wire data.
 * Pointer values are comparison identities only; this result does not retain
 * their targets. Entity ownership is deliberately separate from area assignment:
 * the area coordinator must resolve that entity through its retained area lease,
 * never through global current room, coordinates or a stale movement sector. */
typedef struct SudekiMpLanStoryCollisionOwner {
    uint64_t dispatch_serial;
    uint32_t epoch,revision,kind,region,resource_identifier;
    void *source,*owner;
    uint8_t registration,enabled,parent_depth;
} SudekiMpLanStoryCollisionOwner;
/* Read-only preparation on the existing READY/post-controller witness. Requires
 * unique collision-system AND spatial-grid enrollment; follows bounded native
 * CPosition parent links without calling the reference-mutating native getter.
 * Terrain uses reciprocal resident Zone ownership. Other sources must resolve
 * to a uniquely registered entity's own root position. No new hook, native
 * query, reference mutation or filter is installed. Not callable as the future
 * per-candidate callback: that seam needs a separate retained query contract.
 * Failure is UNKNOWN, never permission to discard the source; output unchanged.
 * Loading-phase and cross-frame use remain unsupported. */
BOOL SudekiMpLanStoryCollisionOwnerResolve(HMODULE,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *,
    const void *source,SudekiMpLanStoryCollisionOwner *);

enum { SUDEKIMP_STORY_ORIGIN_TEXT=128 };
typedef struct SudekiMpLanStoryCollisionAuthoredOrigin {
    SudekiMpLanStoryCollisionOwner collision;
    void *descriptor,*data,*spawn;
    uint32_t region,spawn_index,resource_kind,anchor_identifier;
    char zone[SUDEKIMP_LAN_STORY_NAME_SIZE];
    char anchor[SUDEKIMP_STORY_ORIGIN_TEXT],resource[SUDEKIMP_STORY_ORIGIN_TEXT];
} SudekiMpLanStoryCollisionAuthoredOrigin;
/* Resolve a UNIQUE resident authored spawn -> resource -> registered entity
 * binding for supported scenery, NPC and breakable classes, including a source
 * attached through that entity's position children. A noncurrent area is valid.
 * This is authored ORIGIN, not current area membership, a policy assignment or
 * a native lifetime lease. Never use it alone to build a collision query set:
 * the coordinator must own the area's native lifetime and journal subsequent
 * transfers/reparenting. Party actors/projectiles are deliberately unsupported;
 * their explicit assignment/birth area must not come from a spawn or creator's
 * later location. Two full captures also detect same-allocation reassignment.
 * No native calls/refcount changes/hooks/writes. Missing, ambiguous, loading or
 * changing catalogs are UNKNOWN, with output unchanged. READY witness only;
 * cross-frame use and per-candidate/hot-query use remain unsupported. */
BOOL SudekiMpLanStoryCollisionAuthoredOriginResolve(HMODULE,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *,
    const void *source,SudekiMpLanStoryCollisionAuthoredOrigin *);

#endif
