#ifndef SUDEKIMP_LAN_STORY_OBSERVER_H
#define SUDEKIMP_LAN_STORY_OBSERVER_H

#include "hooks/control_separation.h"
#include "network/lan_party_story.h"

/* Observation-only owner of the seven exact zone entry hooks. Mutually
 * exclusive with zone_transition_trace: both verify unmodified entry bytes.
 * No spawn, native task, actor lease, input or camera mutation. Temporary
 * entry/exit also emit a bounded local diagnostic journal of copied authored
 * resource text and exterior return context. This is not a replay/transport
 * API and does not establish asynchronous transition completion. The explicit
 * test-start API below is the only opt-in transition initiator here. */
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
    /* Optional read-only test-start preflight. Must not call the observer. */
    BOOL (*test_ready)(const char *temporary,unsigned leader_character);
} SudekiMpLanStoryObserverSplit;
BOOL SudekiMpLanStoryObserverSetSplit(const SudekiMpLanStoryObserverSplit *split);
/* Native thread, before a whole-world zone call (SET/ENTER/SWITCH/MAIN) runs
 * its original; NULL clears. */
BOOL SudekiMpLanStoryObserverSetWorldChangeHook(void (*hook)(void));

/* Opt-in host test-start, on the established native thread. Requires a fresh
 * exact READY exterior roster and the installed split owner's admission.
 * -1: terminal refusal, native entry not called; 0: not ready, not called;
 * 1: native entry dispatched exactly once (completion remains unproven).
 * An uncertain post-call result is still 1 and must NEVER be retried. One
 * request is permitted per installation. marker is the authored arrival
 * ResourceName text, not the destination zone name. No field teleport. */
int SudekiMpLanStoryObserverTestEnterTemporary(const char *zone,const char *marker);
/* Positive matching TEMP completion only: native state 4, exact current
 * roster, matching split name, no pending transition. FALSE means unknown
 * or incomplete; it does not authorize retrying entry. */
BOOL SudekiMpLanStoryObserverTestTemporarySettled(const char *zone);

/* H1b remote interior (host, native thread): non-lead characters in `mask`
 * stand in `zone`, a house the host keeps background-resident (state 2)
 * while the lead stays in the current exterior. The exterior epoch is kept;
 * the scene names `zone` as temporary and marks only `mask` inside. Refused
 * outside a READY exterior scene, during a split or a zone call, or when
 * `mask` holds the lead or a missing character. Clear ends it (revision
 * only). The evict callback runs on the native thread before any zone call
 * other than a background load of `zone` itself, and must move the members
 * out and call Clear; a zone call that still finds the mask set clears it
 * and follows the vanilla (new epoch) path. */
BOOL SudekiMpLanStoryObserverSetRemoteInterior(const char *zone,uint8_t mask);
void SudekiMpLanStoryObserverClearRemoteInterior(void);
/* Remove only `mask` from the remote interior (others stay inside). */
void SudekiMpLanStoryObserverClearRemoteMembers(uint8_t mask);
void SudekiMpLanStoryObserverSetRemoteInteriorEvict(void (*evict)(void));
/* The house the remote-interior owner keeps background-resident (NULL
 * clears). The native entry refuses a loaded target, and entering one
 * anyway hangs the transition (CONFIRMED_LIVE 2026-10-17), so a native TEMP
 * to exactly that house first runs the lead-entry callback (native thread,
 * before the entry; it moves its members' bodies out and unloads the house,
 * keeping them in the remote mask so the split begins with them inside, or
 * changes nothing, in which case the native entry is a no-op). */
void SudekiMpLanStoryObserverNoteResidentHouse(const char *zone);
void SudekiMpLanStoryObserverSetRemoteInteriorLeadEntry(void (*release)(const char *zone));
/* The house a member visit (loading or inside) uses, NULL when none. Any
 * native zone call other than a load of / entry into that house runs the
 * evict callback first, also while no member is marked inside yet. While
 * set (or while members/resident house remain) Uninstall refuses. */
void SudekiMpLanStoryObserverSetRemoteVisit(const char *zone);
/* TRUE when `zone` is the member visit's house or the resident house. */
BOOL SudekiMpLanStoryObserverRemoteHouseIs(const char *zone);
/* TRUE while the sampler is holding the last READY scene through a short
 * native party miss during a background zone load (no epoch change), and
 * for 1.5 s after the hold ends. */
BOOL SudekiMpLanStoryObserverHolding(void);
/* Identity is unknown, not changed: a whole-world load is pending, the
 * sampler holds through a background-load miss, or a background zone load
 * was requested within the last 5 s (its first native frame can fail an
 * identity proof before any sample observes it). */
BOOL SudekiMpLanStoryObserverIdentityUnknown(void);
/* The last published READY scene (no native proof; callers re-prove). */
BOOL SudekiMpLanStoryObserverPublished(SudekiMpLanStoryScene *out);
/* The zone and authored arrival marker (ResourceName text) of the last
 * native TEMP entry on this machine; FALSE when none or too long. */
BOOL SudekiMpLanStoryObserverLastTemporary(char zone[40],char marker[40]);

#endif
