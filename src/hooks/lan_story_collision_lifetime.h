#ifndef SUDEKIMP_LAN_STORY_COLLISION_LIFETIME_H
#define SUDEKIMP_LAN_STORY_COLLISION_LIFETIME_H
#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpLanStoryCollisionStamp {
    uint64_t revision,incarnation;
} SudekiMpLanStoryCollisionStamp;
/* Passive source-allocation journal, not a strong native reference or an area
 * assignment. Observe only exact heap-source factory returns and deletion
 * entries; stack query sources are deliberately not enrolled. Install during
 * suspended startup before world/collision singletons exist. No runtime caller
 * installs this experimental adapter yet. Native methods still execute once.
 * Address reuse gets a new incarnation; any lost/foreign event closes reads.
 * Output remains untouched on refusal. Native callbacks bind the owner thread.
 * No ownership may be inferred merely from a readable pointer or a stamp. */
BOOL SudekiMpLanStoryCollisionLifetimeInstall(HMODULE);
BOOL SudekiMpLanStoryCollisionLifetimeUninstall(void);
BOOL SudekiMpLanStoryCollisionLifetimeObserve(HMODULE,const void *,SudekiMpLanStoryCollisionStamp *);
BOOL SudekiMpLanStoryCollisionLifetimeRevision(HMODULE,uint64_t *);
/* Cleanup thread identity only; does not imply healthy history or authority. */
BOOL SudekiMpLanStoryCollisionLifetimeOwnerThread(HMODULE);
/* Bounded identity lookup, no native dereference. Requires current exact
 * revision obtained on this thread; caller retains native usage separately. */
BOOL SudekiMpLanStoryCollisionLifetimeMatches(const void *,SudekiMpLanStoryCollisionStamp);
/* One consumer owns collision-query admission. Attach during startup/owner
 * thread; detach only after that consumer's queries and hooks have drained.
 * Uninstall refuses while attached. Sticky unknown cannot be reset/reinstalled
 * to fabricate continuous history; a failed restore keeps all dependencies. */
BOOL SudekiMpLanStoryCollisionLifetimeAttach(HMODULE,const void *consumer);
BOOL SudekiMpLanStoryCollisionLifetimeDetach(HMODULE,const void *consumer);
#endif
