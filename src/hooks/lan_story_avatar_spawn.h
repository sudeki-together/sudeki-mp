#ifndef SUDEKIMP_LAN_STORY_AVATAR_SPAWN_H
#define SUDEKIMP_LAN_STORY_AVATAR_SPAWN_H
#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpLanStoryAvatarSpawnObservation {
    uint32_t epoch,generation,load_generation;
    unsigned seat;
    void *actor,*world; /* Borrowed only while this observation is exact. */
    BOOL construction_exact,completion_returned,job_storage_released;
    BOOL ready,unknown;
} SudekiMpLanStoryAvatarSpawnObservation;

/* Game thread only, outside native task callbacks. Begin, then exactly one
 * existing EngineSpawnEntityNamed("ALLY_TALOS", position), then End on every
 * path immediately after the export returns. No lookup by resource name.
 * Four independent native jobs may finish in any order. Each seat/generation
 * is unique until positively witnessed native world destruction. No retries,
 * cancellation, despawn, actor writes, or hero-group operation occurs here. */
BOOL SudekiMpLanStoryAvatarSpawnBegin(HMODULE image,unsigned seat,uint32_t epoch,uint32_t generation);
BOOL SudekiMpLanStoryAvatarSpawnEnd(unsigned seat,uint32_t epoch,uint32_t generation);
/* TRUE means observation is available, not necessarily ready. Ready proves
 * native completion identity, deleting job destructor return, current actor
 * layout and settled arbiter. It does not prove all descendant scripts have
 * finished or grant camera/input/world ownership. Revalidate those separately.
 * Unknown retains all callback dependencies until verified native world exit. */
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned seat,uint32_t epoch,uint32_t generation,
    SudekiMpLanStoryAvatarSpawnObservation *out);
/* Explicit receipt for one native party operation by the Dev Play party
 * adapter. Begin requires every observed spawn to be completed and ready.
 * Observe is suspended until End verifies an unchanged group (native veto) or
 * exactly the requested add/remove/rotation. This never edits native memory.
 * Only an observed avatar may be added/selected; only a non-avatar nonleader
 * may be removed. The owner token must remain stable across the operation. */
typedef enum SudekiMpLanStoryAvatarSpawnGroupOperation {
    SUDEKIMP_AVATAR_SPAWN_GROUP_ADD = 1,
    SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE,
    SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE
} SudekiMpLanStoryAvatarSpawnGroupOperation;
BOOL SudekiMpLanStoryAvatarSpawnGroupBegin(const void *owner,uint32_t epoch,
    SudekiMpLanStoryAvatarSpawnGroupOperation operation,void *actor);
BOOL SudekiMpLanStoryAvatarSpawnGroupEnd(const void *owner,BOOL *changed);
/* Receipt from this owner's existing shared WORLD_EXIT callback. This does
 * not register a second EntitySetup observer or infer exit from NULL globals.
 * Read before SpawnShutdown; a new spawn invalidates the previous receipt. */
BOOL SudekiMpLanStoryAvatarSpawnWorldExited(uint32_t epoch,uint32_t load_generation,void *world);
/* Refuses while records or a scope remain. Verified TaskTrace world exit
 * discards native-owned borrowed identities; call this BEFORE trace uninstall. */
BOOL SudekiMpLanStoryAvatarSpawnShutdown(void);
#endif
