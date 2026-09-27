#ifndef SUDEKIMP_LAN_PARTY_CONTROL_H
#define SUDEKIMP_LAN_PARTY_CONTROL_H

#include "hooks/control_separation.h"
#include "network/lan_party_session.h"

/* Private movement-profile render observer. The exact adapter owns these
 * callsites; it cannot coexist with the legacy LAN runtime at those sites.
 * Phases 0/1 surround first RenderStart; phase 2 follows second RenderStart.
 * A NULL update witness is accepted by READ-ONLY roster/lease observations
 * only inside this callback on the previously verified controller thread.
 * Acquisition, movement, release and startup still require update witnesses. */
typedef void (*SudekiMpLanPartyPresentationObserver)(unsigned phase);
BOOL SudekiMpLanPartyInstallPresentationObserver(SudekiMpLanPartyPresentationObserver);
BOOL SudekiMpLanPartyRemovePresentationObserver(void);
BOOL SudekiMpLanPartyPresentationBoundary(void);

/* Fixed four-player HOST only, at the existing service-post-original seam.
 * These are independent actor leases, not local split-screen seats. The caller
 * must prove transport admission/freshness before submitting movement. No
 * socket thread may call these APIs. A borrowed dispatch witness is required
 * again at every native mutation; copying it does not grant authority.
 *
 * The coordinator's mandatory probe must positively observe that this actor's
 * native actions, camera/effect tasks and scoped global ownership have drained.
 * Unknown is FALSE. Never substitute a timeout or a disconnected socket for
 * that proof. It runs synchronously and must not mutate ownership itself. */
typedef BOOL (*SudekiMpLanPartyControlDrainProbe)(
    const SudekiMpLanPartyLease *lease, void *actor,
    const SudekiMpControlUpdateDispatchWitness *witness);

/* Once per NEW transport session, by its sole coordinator after the previous
 * coordinator/worker has drained. No native leases may remain. A new session
 * has a new token domain and may restart its per-seat generation counter. */
BOOL SudekiMpLanPartyControlBeginSession(
    const SudekiMpControlUpdateDispatchWitness *witness);
/* Client startup chooses its one local controller ONCE, before any native
 * lease. It does not swap the global legacy pair or retarget a live camera.
 * The process must have been launched as this fixed local character. Remote
 * presentation AI leases use the client's session token/generation and the
 * represented actor seat (including Buki/zero), never transport authority. */
BOOL SudekiMpLanPartyControlBeginClientSession(
    const SudekiMpControlUpdateDispatchWitness *witness, unsigned int local_seat);

BOOL SudekiMpLanPartyControlAcquire(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    SudekiMpLanPartyControlDrainProbe ready);
BOOL SudekiMpLanPartyControlExact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor);
/* Game-thread observations, not permission to mutate the returned actor. */
void *SudekiMpLanPartyControlObserveActor(
    const SudekiMpControlUpdateDispatchWitness *witness, unsigned int seat);
BOOL SudekiMpLanPartyControlRetains(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor);
/* Read-only revalidation for an already-installed native adapter callback on
 * the same verified game thread, including callbacks outside the observer
 * stack. This never admits input or mutates the lease. */
BOOL SudekiMpLanPartyControlRetainedNativeThreadExact(
    const SudekiMpLanPartyLease *lease, void *actor);
/* Ordinary native locomotion only. This does not route attacks, skills,
 * Spirit Strikes, vertical aim, cameras or animation presentation. */
BOOL SudekiMpLanPartyControlMove(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    float world_x, float world_z, float aim_x, float aim_z, BOOL aiming);
/* One weak-action edge on an exact remote actor lease, admitted only in the
 * host-observed native combat mode. Each actor's own native arbiter remains
 * the validator and damage remains host-owned. */
BOOL SudekiMpLanPartyControlSubmitWeakAttack(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor);
/* Actor-local Strong/Sweep rising edges and Block controller state. Only the
 * Buki host may submit these through the retained actor's native arbiter. */
BOOL SudekiMpLanPartyControlSubmitMelee(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    BOOL weak, BOOL strong, BOOL sweep);
BOOL SudekiMpLanPartyControlSubmitBlockState(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor, unsigned int state);
/* Read-only Ailish ranged cadence gate. Resolves that exact leased actor's
 * native CMissileManager and requires retail CanFire && !IsFiring. FALSE
 * means the manager or lease could not be proved on this host callback. */
BOOL SudekiMpLanPartyControlAilishRangedReady(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor, BOOL *ready);
/* Host-only read of Ailish's selected native weapon record under her exact
 * party actor lease. Does not mutate ammunition or reload state. */
BOOL SudekiMpLanPartyControlObserveAilishWeapon(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    SudekiMpLanPartyAilishWeaponState *state);
/* Client cosmetic facing only: zero speed, fresh four-member roster, no
 * combat/casts (proved by caller). Never invokes this for host simulation. */
BOOL SudekiMpLanPartyControlPresentationFacing(
    const SudekiMpControlUpdateDispatchWitness *, unsigned seat, void *actor,
    const float direction[3]);
/* Closes native movement admission even if stopping cannot yet be verified.
 * Retry is allowed; returning FALSE never drops the retained actor identity. */
BOOL SudekiMpLanPartyControlQuiesce(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor);
BOOL SudekiMpLanPartyControlRelease(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    SudekiMpLanPartyControlDrainProbe drained);
/* Pointer-free teardown barrier, safe to query outside the game callback.
 * The owning control hook refuses uninstall while ANY lease is retained. */
BOOL SudekiMpLanPartyControlHasLeases(void);

/* Read-only startup observation, on the same borrowed controller seam. Slots
 * may be missing while InternalSpawnPC completes. Every present slot must
 * match a unique retail actor and Buki must remain the controller/front owner.
 * These identities are comparison fences, never cross-frame dereference rights. */
typedef struct SudekiMpLanPartyRosterObservation {
    void *group, *controller, *actors[4];
    float anchor[3];
    uint8_t present_mask;
    BOOL combat;
} SudekiMpLanPartyRosterObservation;
BOOL SudekiMpLanPartyControlObserveRoster(
    const SudekiMpControlUpdateDispatchWitness *witness,
    SudekiMpLanPartyRosterObservation *observation);
/* Revalidate the complete observation immediately before each existing native
 * API call. Spawn TRUE means submitted, NOT present. Initialization preserves
 * an already-equipped weapon and valid resources; no global FillInventory. */
BOOL SudekiMpLanPartyControlSpawnActor(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyRosterObservation *expected, unsigned int seat,
    const float position[3]);
BOOL SudekiMpLanPartyControlInitializeActor(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyRosterObservation *expected, unsigned int seat);

#ifdef SUDEKIMP_LAN_PARTY_CONTROL_TESTING
/* Inert exact-image fixture only. Production never substitutes these calls. */
typedef void (*SudekiMpLanPartyTestAiCall)(void *party_slot);
typedef void (__attribute__((stdcall)) *SudekiMpLanPartyTestMoveCall)(
    void *, const float *, float, float, uint32_t);
typedef void *(*SudekiMpLanPartyTestActorLookup)(unsigned int seat);
typedef void (*SudekiMpLanPartyTestCombatCall)(void *actor, void *arbiter);
typedef BOOL (*SudekiMpLanPartyTestCombatMode)(BOOL *enabled);
void SudekiMpLanPartyControlTestCalls(SudekiMpLanPartyTestAiCall acquire,
    SudekiMpLanPartyTestAiCall release, SudekiMpLanPartyTestMoveCall movement,
    SudekiMpLanPartyTestActorLookup lookup);
void SudekiMpLanPartyControlTestCombat(SudekiMpLanPartyTestCombatCall combat);
void SudekiMpLanPartyControlTestCombatMode(SudekiMpLanPartyTestCombatMode mode);
typedef BOOL (*SudekiMpLanPartyTestWorld)(float anchor[3], BOOL *combat);
typedef BOOL (*SudekiMpLanPartyTestSpawn)(unsigned int seat, const float position[3]);
typedef BOOL (*SudekiMpLanPartyTestInitialize)(unsigned int seat);
void SudekiMpLanPartyControlTestRosterCalls(SudekiMpLanPartyTestWorld world,
    SudekiMpLanPartyTestSpawn spawn, SudekiMpLanPartyTestInitialize initialize);
#endif
#endif
