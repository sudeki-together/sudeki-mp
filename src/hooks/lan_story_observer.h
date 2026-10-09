#ifndef SUDEKIMP_LAN_STORY_OBSERVER_H
#define SUDEKIMP_LAN_STORY_OBSERVER_H

#include "hooks/control_separation.h"
#include "network/lan_party_story.h"

/* Observation-only owner of the seven exact zone entry hooks. Mutually
 * exclusive with zone_transition_trace: both verify unmodified entry bytes.
 * No spawn, native task, actor lease, input or camera mutation. Temporary
 * entry/exit also emit a bounded local diagnostic journal of copied authored
 * resource text and exterior return context. This is not a replay/transport
 * API and does not establish asynchronous transition completion. */
BOOL SudekiMpLanStoryObserverInstall(HMODULE module);
BOOL SudekiMpLanStoryObserverSample(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpLanStoryScene *scene);
/* Borrowed read-only roster proof for one exact observer callback. This is
 * neither an actor/input lease nor dialogue/script completion evidence. */
typedef struct SudekiMpLanStoryNativeRoster {
    uint64_t dispatch_serial;
    uint32_t epoch, revision;
    uint8_t available_mask, leader_character;
    void *world, *descriptor, *group, *controller;
    void *actors[4], *ai[4];
} SudekiMpLanStoryNativeRoster;
/* Sample(scene) must already have published this READY scene on this thread.
 * Every call freshly verifies the live sparse native group; no cached pointer
 * grants access. Actor/scene replacement fails instead of spawning a member. */
BOOL SudekiMpLanStoryObserverRoster(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryNativeRoster *roster);
BOOL SudekiMpLanStoryObserverRosterStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryNativeRoster *roster);
/* Retained native callback identity only. Re-observes the sparse roster on
 * the established native thread, outside zone entry/exit. No controller
 * dispatch, input, movement, acquisition or new network authority is granted. */
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *roster);
/* A whole-world zone change is pending (identity is unknown, not changed). */
BOOL SudekiMpLanStoryObserverWorldLoading(void);
/* After a native callback has occurred, remove on that same native thread,
 * outside the synchronous zone calls. Failed restoration retains all state. */
BOOL SudekiMpLanStoryObserverUninstall(void);
/* Optional split-area owner. begin_temp is asked on the native thread at a
 * TEMP entry from a READY exterior scene; TRUE keeps the scene epoch, marks
 * only the lead character inside and holds the last READY scene while the
 * native transition settles. begin_exit likewise for the matching exit.
 * settled reports the transition reached native exactness; ended reports the
 * split is over (normal exit or vanilla zone change). Callbacks must not call
 * back into the observer. NULL removes the owner (refused while split). */
typedef struct SudekiMpLanStoryObserverSplit {
    BOOL (*begin_temp)(const char *temporary,unsigned leader_character);
    BOOL (*begin_exit)(void);
    void (*settled)(BOOL inside);
    void (*ended)(BOOL vanilla_change);
} SudekiMpLanStoryObserverSplit;
BOOL SudekiMpLanStoryObserverSetSplit(const SudekiMpLanStoryObserverSplit *split);
/* Native thread, before a whole-world zone call (SET/ENTER/SWITCH/MAIN) runs
 * its original; NULL clears. */
BOOL SudekiMpLanStoryObserverSetWorldChangeHook(void (*hook)(void));

#endif
