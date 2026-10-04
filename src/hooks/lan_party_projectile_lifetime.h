#ifndef SUDEKIMP_LAN_PARTY_PROJECTILE_LIFETIME_H
#define SUDEKIMP_LAN_PARTY_PROJECTILE_LIFETIME_H

#include <windows.h>
#include <stdint.h>
#include "hooks/lan_arena_spirit_visual_host.h"

typedef struct SudekiMpLanPartyProjectileTag {
    uint64_t session;
    uint32_t generation;
    uint16_t sequence;
    uint8_t character, item;
    void *actor;
} SudekiMpLanPartyProjectileTag;

/* Caller first proves the supported executable file identity. Install checks
 * its loaded PE and the exact TPtr/CMissile lifetime ABI. It owns the narrow
 * constructor/update/termination call observers for descendant lineage.
 * Before Capture, the existing AIM observer must prove its native shot lease,
 * actor, item, session and fresh native generation. This module does not grant
 * gameplay authority. Its independent weak observers never delay destruction.
 */
BOOL SudekiMpLanPartyProjectileLifetimeInstall(HMODULE module);
BOOL SudekiMpLanPartyProjectileLifetimeCapture(
    const SudekiMpLanPartyProjectileTag *tag, void *manager);
/* Production ordinary emissions additionally retain their exact native actor
 * namespace so later impact SFX inherit it after the shot/task itself ends. */
BOOL SudekiMpLanPartyProjectileLifetimeCaptureOwned(
    const SudekiMpLanPartyProjectileTag *tag,
    const SudekiMpLanPartyEffectOwner *owner, void *manager);
/* Capture binds the verified calling game thread. Poll must run on that same
 * thread, including after disconnect. TRUE means observation succeeded; use
 * Retains to distinguish live objects from complete drain. No capture yet is
 * an empty successful poll and does not bind an arbitrary caller's thread. */
BOOL SudekiMpLanPartyProjectileLifetimePoll(void);
/* Shutdown only: caller has stopped emission and positively drained all cast,
 * task and body namespaces. Requests the verified native retirement once per
 * recorded missile, under its retained effect namespace. TRUE means requests
 * were issued/already observed, never that destruction has completed. Keep
 * polling; independent TPtr observers and descendant effects must still drain.
 * Never use this for a character transfer or ordinary body-idle transition. */
BOOL SudekiMpLanPartyProjectileLifetimeRequestRetire(void);
/* Atomic, no native reads: includes live cells, operations and sticky unknown
 * capture/lifetime failures. Such failures cannot be cleared by reinstall. */
BOOL SudekiMpLanPartyProjectileLifetimeRetains(void);
/* Native scope only, never calls back into effect/cast attribution. TRUE
 * means a missile callback scope is present; a zero owner explicitly masks
 * an unrelated enclosing cast. Callers must not fall through in that case. */
BOOL SudekiMpLanPartyProjectileLifetimeCurrent(SudekiMpLanPartyEffectOwner *owner);
/* Synchronous, native-thread-only ownership observation for paused story
 * containment. Neither missile nor entity may be borrowed beyond this call's
 * verified native transaction. Incarnation distinguishes allocator address
 * reuse even within one actor namespace. Entry/return are deliberately
 * separate: a queued retirement is NOT a destructor-cleared weak observer.
 * A successful empty list proves no live missile, never a failed observation;
 * Poll/Retains must still prove module drain before uninstall.
 * Snapshot does not poll, discard observers, tick objects or grant admission.
 * No partial output on failure (including insufficient caller capacity). */
typedef struct SudekiMpLanPartyProjectileObservation {
    SudekiMpLanPartyEffectOwner owner;
    uint32_t incarnation;
    void *missile,*entity;
    BOOL termination_entered,termination_returned;
} SudekiMpLanPartyProjectileObservation;
BOOL SudekiMpLanPartyProjectileLifetimeObserve(
    SudekiMpLanPartyProjectileObservation *entries,unsigned capacity,unsigned *count);
/* Caller has stopped/drained AIM notifications before uninstall. Fails and
 * retains all state if any observer or unknown obligation remains. */
BOOL SudekiMpLanPartyProjectileLifetimeUninstall(void);

#endif
