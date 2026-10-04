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
/* Exact startup binding for a host who selected any canonical character.
 * This does not switch a live native controller or change network authority. */
BOOL SudekiMpLanPartyControlBeginHostSession(
    const SudekiMpControlUpdateDispatchWitness *witness, unsigned local_character);
/* Native actor leases have a separate generation domain from connection
 * leases. Allocate only after the previous exact actor lease has drained.
 * The returned key is for native adapters, never transport authorization. */
BOOL SudekiMpLanPartyControlNextLease(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *connection, unsigned character,
    SudekiMpLanPartyLease *native_key);
/* Read-only actor-domain key lookup. The caller separately proves connection
 * ownership; this verifies the retained native actor and generation. */
BOOL SudekiMpLanPartyControlActorLease(
    const SudekiMpControlUpdateDispatchWitness *witness, unsigned character,
    SudekiMpLanPartyLease *native_key);
BOOL SudekiMpLanPartyControlActorLeaseOnNativeThread(
    unsigned character, SudekiMpLanPartyLease *native_key);
/* Client startup chooses its one local controller ONCE, before any native
 * lease. It does not swap the global legacy pair or retarget a live camera.
 * The process must have been launched as this fixed local character. Remote
 * presentation AI leases use the client's session token/generation and the
 * represented actor seat (including Buki/zero), never transport authority. */
BOOL SudekiMpLanPartyControlBeginClientSession(
    const SudekiMpControlUpdateDispatchWitness *witness, unsigned int local_seat);
/* Read-only guard for local operator commands: exact local actor/controller,
 * native UsingUI clear and an unpaused normal-speed world. */
BOOL SudekiMpLanPartyControlGameplayReady(
    const SudekiMpControlUpdateDispatchWitness *witness);
/* Persistent owned native None filter: service until Update commits 0/0.
 * Acquire only from neutral 1/1; an existing skill-owned 0/0 is not ours.
 * Enable AI only after TRUE; restore only after local AI has been reclaimed.
 * Native UI/puzzle movement and switching locks remain with their owner. */
BOOL SudekiMpLanPartyControlMenuInputBlocked(
    const SudekiMpControlUpdateDispatchWitness *witness, BOOL blocked);
BOOL SudekiMpLanPartyControlMenuInputExact(void *actor);
/* Deliver released button states to the current human actor while our exact
 * None input fence is held. Never cancels native tasks or an AI action;
 * callers still wait for ordinary animation/action completion afterwards. */
BOOL SudekiMpLanPartyControlLocalReleaseInput(
    const SudekiMpControlUpdateDispatchWitness *witness);
/* Physical native controller owner on its verified game thread; independent
 * of a network player reservation (including an observing player). */
unsigned SudekiMpLanPartyControlLocalCharacter(void);

BOOL SudekiMpLanPartyControlAcquire(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    SudekiMpLanPartyControlDrainProbe ready);
BOOL SudekiMpLanPartyControlExact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor);
/* Host-only observation for an unoccupied companion: exact roster/actor,
 * no retained multiplayer lease, and native default AI positively enabled.
 * Does not acquire control or confer a player lease. */
BOOL SudekiMpLanPartyControlHostAiExact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    unsigned int seat, void *actor);
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
/* Cleanup-only lifetime proof for HELD or DRAINING on the verified native
 * thread. Requires the same exact actor/key/roster and owned AI override
 * (ref1/mode0); excludes RELEASE_VERIFY and never admits fresh input/tasks. */
BOOL SudekiMpLanPartyControlRetainedCleanupNativeThreadExact(
    const SudekiMpLanPartyLease *lease, void *actor);
/* Ordinary native locomotion only. This does not route attacks, skills,
 * Spirit Strikes, vertical aim, cameras or animation presentation. */
BOOL SudekiMpLanPartyControlMove(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor,
    float world_x, float world_z, float aim_x, float aim_z, BOOL aiming);
/* Host-only facing during the exact actor's admitted native targeting phase.
 * No speed, position, task, resource, or combat flag mutation. */
BOOL SudekiMpLanPartyControlSkillFacing(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor, float aim_x, float aim_z);
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
BOOL SudekiMpLanPartyControlSubmitDodge(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor, float x, float z);
/* Read-only Ailish ranged cadence gate. Resolves that exact leased actor's
 * native CMissileManager and requires retail CanFire && !IsFiring. FALSE
 * means the manager or lease could not be proved on this host callback. */
BOOL SudekiMpLanPartyControlAilishRangedReady(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyLease *lease, void *actor, BOOL *ready);
/* Host-only read of Ailish's selected native weapon record under her exact
 * party actor lease. Does not mutate ammunition or reload state. */
/* Exact host-world read for local or AI Ailish; grants no control lease. */
BOOL SudekiMpLanPartyControlObserveAilishWorldWeapon(
    const SudekiMpControlUpdateDispatchWitness *,void *,
    SudekiMpLanPartyAilishWeaponState *);
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
/* Explicit saved-story native lease entrypoints. They use separate retained
 * records and a fresh sparse StoryObserver witness; they do not change any
 * four-member/TestRoom predicate above. The story coordinator additionally
 * owns Q/E containment, action readiness and network admission. */
struct SudekiMpLanStoryNativeRoster;
BOOL SudekiMpLanPartyControlStoryBegin(const SudekiMpControlUpdateDispatchWitness *,
    const struct SudekiMpLanStoryNativeRoster *);
BOOL SudekiMpLanPartyControlStoryNextLease(const SudekiMpControlUpdateDispatchWitness *,
    const SudekiMpLanPartyLease *,unsigned character,SudekiMpLanPartyLease *);
BOOL SudekiMpLanPartyControlStoryAcquire(const SudekiMpControlUpdateDispatchWitness *,
    const struct SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,
    SudekiMpLanPartyControlDrainProbe);
BOOL SudekiMpLanPartyControlStoryExact(const SudekiMpControlUpdateDispatchWitness *,
    const struct SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *);
BOOL SudekiMpLanPartyControlStoryMove(const SudekiMpControlUpdateDispatchWitness *,
    const struct SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,float,float);
BOOL SudekiMpLanPartyControlStoryDrain(const SudekiMpControlUpdateDispatchWitness *,
    const struct SudekiMpLanStoryNativeRoster *,const SudekiMpLanPartyLease *,
    SudekiMpLanPartyControlDrainProbe);
BOOL SudekiMpLanPartyControlStoryActorOwned(const SudekiMpControlUpdateDispatchWitness *,
    const struct SudekiMpLanStoryNativeRoster *,unsigned);
BOOL SudekiMpLanPartyControlStoryRetainsKey(const SudekiMpLanPartyLease *key);
BOOL SudekiMpLanPartyControlStoryRetains(void);
BOOL SudekiMpLanPartyControlStoryEnd(void);
/* Native-thread coordinator query: all actor leases and their retained mask
 * are empty. This deliberately excludes the separately owned menu input fence;
 * it is not permission to uninstall control callbacks or release that fence. */
BOOL SudekiMpLanPartyControlActorLeasesEmpty(void);
/* A same-process lobby may supply independently validated native Test Room
 * evidence. Installed only with no actor leases; never replaces roster checks. */
BOOL SudekiMpLanPartyControlSetTestroomProbe(BOOL (*probe)(unsigned seat));

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
/* Read-only retained identity check for scheduled native cast callbacks on
 * the previously established game thread. Does not grant a dispatch witness
 * or permit acquisition, movement, release or new actor mutation. */
BOOL SudekiMpLanPartyControlNativeRosterExact(
    const SudekiMpLanPartyRosterObservation *expected);
BOOL SudekiMpLanPartyControlNativeActorExact(
    const SudekiMpLanPartyRosterObservation *expected,unsigned seat);
/* Native local selection transaction. All actor leases must drain; the
 * existing owned None input fence remains 0/0 throughout. Rebind validates
 * the native rotation's same-roster result, then changes mod-owned physical
 * identity only. It preserves launch identity and generation tombstones. */
BOOL SudekiMpLanPartyControlLocalSwitchReady(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyRosterObservation *expected);
BOOL SudekiMpLanPartyControlRebindLocal(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanPartyRosterObservation *expected,unsigned character);
BOOL SudekiMpLanPartyControlEnableCastInputIsolation(
    const SudekiMpControlUpdateDispatchWitness *witness);
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
typedef void (*SudekiMpLanPartyTestMenuFilter)(void *controller,BOOL blocked);
void SudekiMpLanPartyControlTestMenuFilter(SudekiMpLanPartyTestMenuFilter call);
typedef void (*SudekiMpLanPartyTestMenuRelease)(void *actor,void *arbiter,
    const int states[6]);
void SudekiMpLanPartyControlTestMenuRelease(SudekiMpLanPartyTestMenuRelease call);
typedef BOOL (*SudekiMpLanPartyTestSpawn)(unsigned int seat, const float position[3]);
typedef BOOL (*SudekiMpLanPartyTestInitialize)(unsigned int seat);
void SudekiMpLanPartyControlTestRosterCalls(SudekiMpLanPartyTestWorld world,
    SudekiMpLanPartyTestSpawn spawn, SudekiMpLanPartyTestInitialize initialize);
#endif
#endif
