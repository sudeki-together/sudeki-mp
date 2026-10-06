#ifndef SUDEKIMP_LAN_STORY_AREA_FINALISE_H
#define SUDEKIMP_LAN_STORY_AREA_FINALISE_H
#include "hooks/control_separation.h"
#include <stdint.h>

enum { SUDEKIMP_AREA_FINALISERS=128 };
typedef struct SudekiMpLanStoryAreaFinaliseReceipt {
    uint64_t ticket;
    uint64_t resource_generation;
    uintptr_t task,resource,manager; /* Opaque comparison identities, NEVER leases. */
    uint32_t runs;
    BOOL submitting,running,destroying,destroyed,terminal_result;
    uintptr_t pvs_descriptor,pvs_node;
    BOOL pvs_submitting,pvs_retiring,pvs_retired;
} SudekiMpLanStoryAreaFinaliseReceipt;
typedef struct SudekiMpLanStoryAreaRetirementReceipt {
    uint64_t ticket;
    uint64_t resource_generation;
    uintptr_t resource,remover,manager; /* Copied addresses; generation is separate, never a lease. */
    BOOL copying,copied,submitting,submitted,destroying,destroyed,drained;
} SudekiMpLanStoryAreaRetirementReceipt;
enum { SUDEKIMP_AREA_RESOURCE_CONSTRUCTING=1,SUDEKIMP_AREA_RESOURCE_LIVE,
    SUDEKIMP_AREA_RESOURCE_DESTROYING,SUDEKIMP_AREA_RESOURCE_BODY_RETURNED };
typedef struct SudekiMpLanStoryAreaResourceReceipt {
    uint64_t generation;
    uintptr_t resource;
    unsigned phase;
    DWORD callback_thread;
} SudekiMpLanStoryAreaResourceReceipt;
/* Experimental passive journal, not installed by runtime. Startup-only install
 * owns the ZoneFinalise factory enqueue, class Run/destructor and resource
 * manager UpdateNode dispatch. Native originals always run once. Events copy
 * register identities only; workers never dereference game objects or policy.
 * It does not load, retain, initialize, activate or authorize an area.
 * Run returning true is terminal SETUP, not successful/playable loading.
 * An initial-load visibility node is retained through its native queue-free
 * return, independently of finaliser destruction. The node identity, not just
 * its descriptor/callback address, distinguishes unrelated reload callbacks.
 * Scripts, outer GPU/resource pumps and other area work remain separate.
 * Unknown/overflow retains the journal and hooks; no live reset API exists. */
BOOL SudekiMpLanStoryAreaFinaliseInstall(HMODULE);
/* Only pre-world/pre-worker startup restoration is currently implemented.
 * Live removal deliberately refuses and retains dependencies, even when this
 * journal is empty: native producers and owner teardown need a separate
 * admission/drain contract before integration. Even exited workers can leave
 * late submissions stranded. This is not a DLL-unload policy. */
BOOL SudekiMpLanStoryAreaFinaliseUninstall(void);
/* One future coordinator attaches during suspended startup and owns journal
 * consumption. Its native world/resource ownership must be retained separately. */
BOOL SudekiMpLanStoryAreaFinaliseAttach(HMODULE,const void *consumer);
/* Snapshot only at a fresh post-controller boundary outside all observed
 * callbacks/resource updates. Output/count stay unchanged on refusal.
 * A resource address alone cannot establish its generation or area identity. */
BOOL SudekiMpLanStoryAreaFinaliseSnapshot(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,SudekiMpLanStoryAreaFinaliseReceipt *,
    unsigned capacity,unsigned *count);
/* Acknowledgement retires only this journal record after BOTH enqueue and
 * destructor returns AND any associated initial-load visibility node's free
 * return, outside resource dispatch. This is NOT area readiness: a callback
 * may skip initialization on cancellation, or enqueue longer-lived work.
 * It releases NO native object
 * or area policy pin. Cancellation without Run is recorded distinctly.
 * The coordinator must first consume the receipt into its owned area lifetime. */
BOOL SudekiMpLanStoryAreaFinaliseAcknowledge(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t ticket);
/* Same adapter/consumer/update boundary owns deferred ZoneRemover history.
 * Native copy associates source resource with the distinct cleanup job. A
 * receipt drains only after copy/enqueue return, exact native manager deletion
 * and the enclosing update's return (which follows deferred-list erasure).
 * This does NOT prove native keepalive, every descendant task's completion,
 * or world destruction. Unknown deletion paths retain the journal.
 * Never call a native readiness predicate to obtain this snapshot: it mutates
 * cleanup state. These APIs observe only already-executed native work. */
BOOL SudekiMpLanStoryAreaRetirementSnapshot(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,SudekiMpLanStoryAreaRetirementReceipt *,
    unsigned capacity,unsigned *count);
BOOL SudekiMpLanStoryAreaRetirementAcknowledge(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t ticket);
/* Continuous class construction/destruction-body history, installed before
 * world/worker startup. Finalise/retirement records bind this generation at
 * their native publication, not by matching addresses in a later snapshot.
 * This is identity history, NOT native keepalive. BODY_RETURNED is not the
 * outer deleting-wrapper return or complete descendant/world teardown.
 * Acknowledge only retires a body-returned history record after all associated
 * finalise/retirement receipts have been consumed; it frees no native object. */
BOOL SudekiMpLanStoryAreaResourceSnapshot(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,SudekiMpLanStoryAreaResourceReceipt *,
    unsigned capacity,unsigned *count);
BOOL SudekiMpLanStoryAreaResourceAcknowledge(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *,uint64_t generation);
/* Sibling request observer only: Ready gates new request watches. Capture is
 * called synchronously at its verified worker publication, not on a transport
 * worker or to identify an old pointer later. It copies the currently LIVE
 * incarnation under this journal's lock without dereferencing the resource.
 * Existing admitted callbacks may still capture after admission closes.
 * Output stays unchanged on refusal. This is immutable identity, NOT a native
 * lease, readiness or a journal pin. The coordinator must consume root history
 * before acknowledging it; copied request generations remain valid identities
 * after that acknowledgement. Lock order is request owner -> resource owner;
 * this owner never calls back into the request owner. */
BOOL SudekiMpLanStoryAreaResourceJournalReady(HMODULE);
BOOL SudekiMpLanStoryAreaResourceCapture(HMODULE,uintptr_t resource,uint64_t *generation);
BOOL SudekiMpLanStoryAreaFinaliseDetach(HMODULE,const void *consumer,
    const SudekiMpControlUpdateDispatchWitness *);
#endif
