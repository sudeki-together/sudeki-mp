#ifndef SUDEKIMP_LAN_STORY_CLIENT_H
#define SUDEKIMP_LAN_STORY_CLIENT_H

#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_task_trace.h"
#include "hooks/lan_story_recruit.h"
#include "hooks/lan_story_local_control.h"

/* The story runtime owns MenuNative. Acquire/Service/PrepareExit run from its
 * native render callback; Drain/ReacquireExit run on the terminal UI stack.
 * This module installs no competing render hook.
 * The callback must positively prove the runtime's retained local input fence;
 * merely being a client, connected, or in spectator policy is insufficient. */
typedef BOOL (*SudekiMpLanStoryClientInputClosed)(void *controller);
typedef enum SudekiMpLanStoryClientPhase {
    SUDEKIMP_STORY_CLIENT_IDLE=0,
    SUDEKIMP_STORY_CLIENT_OBSERVED,
    SUDEKIMP_STORY_CLIENT_ACQUIRING,
    SUDEKIMP_STORY_CLIENT_PAUSED,
    SUDEKIMP_STORY_CLIENT_RECRUITING,
    SUDEKIMP_STORY_CLIENT_RELEASING,
    SUDEKIMP_STORY_CLIENT_UNKNOWN
} SudekiMpLanStoryClientPhase;
typedef struct SudekiMpLanStoryClientReport {
    uint32_t load_generation, epoch, revision, entity_count;
    uint64_t observation;
    unsigned phase;
    BOOL pause_owned, native_pause_exact, roster_exact, input_closed;
    BOOL scheduler_unchanged, entities_unchanged;
} SudekiMpLanStoryClientReport;

/* This is a native containment adapter, not a campaign replica. PAUSED means
 * this exact pause transaction, roster, task-clock/counter and entity registry
 * were re-observed; it does not prove all native story mutation sources closed.
 * It never submits input, advances tasks, spawns actors or writes pose/camera.
 * Install owns two exact TriggerManager calls. Their void native phases are
 * suspended only for this retained pause/manager; queued events must be empty
 * before acquire and before drain. Two cluster-script query calls retain host
 * script authority; two spawn-group activity calls journal native per-entity
 * reference changes without consuming the client-owned pause. Registry and
 * task checks remain exact. No task/event is synthesized or discarded. */
BOOL SudekiMpLanStoryClientInstall(HMODULE image,
    SudekiMpLanStoryClientInputClosed input_closed);
/* Seed only from the existing exact post-controller observer. Both native
 * saved-load ROOT tasks must have positively completed and retired. The live
 * task status is obtained here, not trusted from a caller-provided boolean. */
BOOL SudekiMpLanStoryClientObserve(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryClientAcquire(SudekiMpLanStoryClientReport *report);
BOOL SudekiMpLanStoryClientService(SudekiMpLanStoryClientReport *report);
/* Fixed Lighthouse transition, while retaining the same full native pause.
 * The request uses this client's local load/scene identities; the runtime
 * separately proves the host's authenticated transition. Native operations
 * remain one-shot and only accounted removed/created owners are enrolled. */
BOOL SudekiMpLanStoryClientRecruitBegin(const SudekiMpLanStoryRecruitRequest *request,
    uint32_t remote_before_epoch);
BOOL SudekiMpLanStoryClientRecruitService(SudekiMpLanStoryRecruitReport *report);
BOOL SudekiMpLanStoryClientRecruitCommit(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryClientRecruiting(void);
/* Exact local physical selection after recruited-world presentation and old
 * spectator view retirement. Native pause/registry/task ownership is retained;
 * only the Tal/Ailish front permutation may change. A returned bound roster
 * is adopted even while camera readiness is pending, permitting safe exit.
 * This operation never acknowledges a network offer. */
BOOL SudekiMpLanStoryClientSelectCharacter(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene,unsigned character,SudekiMpLanStoryLocalControlReport *report);
/* Render-only borrowed identity observation for a future separate presentation
 * adapter. No expired controller witness is reused or fabricated. Returns only
 * while all retained native pause/registry/roster/input proofs still match. */
BOOL SudekiMpLanStoryClientPausedRoster(SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *roster);
typedef BOOL (*SudekiMpLanStoryClientPresentation)(
    const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,void *context);
/* Synchronous render transaction: recheck the complete registry before and
 * after callback. Only inside that callback may RosterExact use the smaller
 * fresh pause/roster/input/scheduler proof. No authorization survives return.
 * The callback must not pump messages, enter native world simulation or call
 * this adapter's Acquire/Observe/Service/Drain methods. */
BOOL SudekiMpLanStoryClientPresent(SudekiMpLanStoryClientPresentation callback,
    void *context);
/* Separate cosmetic render boundary. The witness must positively identify
 * the currently active, exact native effects hook on every check. A full
 * registry/pause/input/task proof brackets the callback, without granting
 * pause mutation or reusing authorization from a previous render frame. */
typedef BOOL (*SudekiMpLanStoryClientEffectsWitness)(void *context);
BOOL SudekiMpLanStoryClientEffectsPresent(SudekiMpLanStoryClientPresentation callback,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context);
/* Exact native UI-listener boundary, with the same full pre/post containment
 * checks as cosmetic rendering. A local menu may update its own widgets and
 * balanced UI locks, but must intercept script submission and gameplay
 * confirmations. This grants no world tick, skill execution or pause change. */
BOOL SudekiMpLanStoryClientUiPresent(SudekiMpLanStoryClientPresentation callback,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context);
/* Primary scene preparation only: a fresh native render witness permits the
 * dispatcher to select an authenticated frame and call ClientPresent. Full
 * registry/pause/input/task proofs bracket dispatch. It grants no pause
 * acquisition, exit preparation or simulation; all scope state clears before
 * native preparation resumes. The existing effects transaction cannot nest. */
typedef void (*SudekiMpLanStoryClientRenderDispatcher)(void *context);
BOOL SudekiMpLanStoryClientRenderDispatch(SudekiMpLanStoryClientRenderDispatcher dispatch,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context);
/* Shutdown-only view restoration. A changed task clock can revoke playback
 * without invalidating an exact camera/roster owner's restoration. These
 * retain every native pause/registry/input proof, but grant no new playback. */
BOOL SudekiMpLanStoryClientCleanupRoster(SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryClientCleanupRosterExact(const SudekiMpLanStoryNativeRoster *roster);
/* Render-only shutdown preparation, after retained view restoration. TRUE
 * keeps the full native pause held; Retains remains TRUE. */
BOOL SudekiMpLanStoryClientPrepareExit(SudekiMpLanStoryClientReport *report);
/* UI teardown only, outside all native menu callbacks, after PrepareExit.
 * Balance only this module's pause, on the same live native owners. Registry
 * mutation prevents native Unpause (which iterates CURRENT entities). Unknown
 * presentation/scheduler evidence alone may still drain if pause owners and
 * registry references positively match; no owner is discarded on failure. */
BOOL SudekiMpLanStoryClientDrain(SudekiMpLanStoryClientReport *report);
/* Same UI teardown transaction only. Reacquire the positively released pause
 * before yielding on another adapter's teardown failure. Input and MenuNative
 * must still be installed/exact, or be safely rearmed before this call. */
BOOL SudekiMpLanStoryClientReacquireExit(SudekiMpLanStoryClientReport *report);
/* Same terminal UI stack, only after LobbyGameplay's native quit bridge has
 * returned and positively verified its reset. Retires borrowed world objects
 * without dereferencing them: native quit may already have destroyed them.
 * It neither requests quit nor treats an entered/unknown outcome as success. */
BOOL SudekiMpLanStoryClientNativeExitReturned(void);
/* Reports native pause/presentation debt. The separate trigger exit hold
 * survives successful Drain until positively returned native quit. A failed
 * terminal attempt must reacquire the full pause before the next world tick. */
BOOL SudekiMpLanStoryClientRetains(void);
/* Final native adapter retirement after NativeExitReturned, or untouched
 * installation cleanup. Balanced pause alone is not a world-exit witness. */
BOOL SudekiMpLanStoryClientUninstall(void);

#endif
