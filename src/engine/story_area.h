#ifndef SUDEKIMP_STORY_AREA_H
#define SUDEKIMP_STORY_AREA_H

#include <stdint.h>

/* Host game-thread policy, NOT a native loader or a wire format. Initial
 * integration boundary: one exterior and one authored temporary interior.
 * No call here activates a scene, grants script authority or proves native
 * completion. Native adapters must supply exact lifetime/terminal evidence.
 * Character identifiers use story order (Buki, Elco, Tal, Ailish), not wallet
 * order. A player slot is independent of its assigned character. */
enum { SUDEKIMP_STORY_AREAS=2, SUDEKIMP_STORY_AREA_PLAYERS=4,
    SUDEKIMP_STORY_AREA_PINS=32, SUDEKIMP_STORY_AREA_NAME=64 };
typedef struct SudekiMpStoryAreaRef {
    uint64_t session, lifetime;
    uint32_t slot;
} SudekiMpStoryAreaRef;
typedef struct SudekiMpStoryAreaOwner {
    uint64_t session, actor_generation, connection_generation;
    uint32_t player, character;
} SudekiMpStoryAreaOwner;
typedef enum SudekiMpStoryAreaPhase {
    SUDEKIMP_STORY_AREA_EMPTY=0, SUDEKIMP_STORY_AREA_LOADING,
    SUDEKIMP_STORY_AREA_READY, SUDEKIMP_STORY_AREA_RETIRING
} SudekiMpStoryAreaPhase;
typedef enum SudekiMpStoryTravelPhase {
    SUDEKIMP_STORY_TRAVEL_NONE=0, SUDEKIMP_STORY_TRAVEL_REQUESTED,
    SUDEKIMP_STORY_TRAVEL_ACTIVE, SUDEKIMP_STORY_TRAVEL_SETTLED
} SudekiMpStoryTravelPhase;
typedef struct SudekiMpStoryAreaRecord {
    SudekiMpStoryAreaRef ref, parent;
    uint32_t phase;
    char world[SUDEKIMP_STORY_AREA_NAME], temporary[SUDEKIMP_STORY_AREA_NAME];
} SudekiMpStoryAreaRecord;
typedef struct SudekiMpStoryTravel {
    uint64_t ticket, native_task;
    SudekiMpStoryAreaRef source, destination;
    uint32_t phase;
} SudekiMpStoryTravel;
typedef struct SudekiMpStoryAreaPlayer {
    SudekiMpStoryAreaOwner owner;
    SudekiMpStoryAreaRef area;
    SudekiMpStoryTravel travel;
    uint8_t bound, connected;
} SudekiMpStoryAreaPlayer;
typedef struct SudekiMpStoryAreaPin {
    uint64_t serial;
    SudekiMpStoryAreaRef area;
} SudekiMpStoryAreaPin;
typedef struct SudekiMpStoryAreas {
    uint64_t session, next_lifetime, next_ticket, next_pin;
    uint64_t connection_floor[SUDEKIMP_STORY_AREA_PLAYERS];
    SudekiMpStoryAreaRecord areas[SUDEKIMP_STORY_AREAS];
    SudekiMpStoryAreaPlayer players[SUDEKIMP_STORY_AREA_PLAYERS];
    SudekiMpStoryAreaPin pins[SUDEKIMP_STORY_AREA_PINS];
} SudekiMpStoryAreas;

/* Start once in zero-initialized storage with a nonzero session identity.
 * Reinitialization is never a way to drop retained native obligations. */
int SudekiMpStoryAreasInitialize(SudekiMpStoryAreas *,uint64_t session);
int SudekiMpStoryAreaRefSame(SudekiMpStoryAreaRef,SudekiMpStoryAreaRef);
/* Canonical lowercase names. Empty temporary selects the exterior. A second
 * request for a loading/ready identical area returns its EXISTING lifetime.
 * Load intent alone does not mean the native resource request has started. */
int SudekiMpStoryAreaLoad(SudekiMpStoryAreas *,const char *world,
    const char *temporary,SudekiMpStoryAreaRef *);
/* Call only after positively proven native ready state, including collision,
 * scene registration and required initialization work, for this exact load. */
int SudekiMpStoryAreaReady(SudekiMpStoryAreas *,SudekiMpStoryAreaRef);
int SudekiMpStoryAreaBind(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner,
    SudekiMpStoryAreaRef);
int SudekiMpStoryAreaInputAllowed(const SudekiMpStoryAreas *,SudekiMpStoryAreaOwner);
/* Request retains both areas, without moving anyone or blocking unrelated
 * players. Only the adapter may authorize the actual authored door/reach.
 * Repeated same-destination requests share the pending ticket. */
int SudekiMpStoryAreaRequest(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner,
    SudekiMpStoryAreaRef destination,uint64_t *ticket);
int SudekiMpStoryAreaCancelRequest(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner,uint64_t ticket);
/* Revalidate door, actor, source and destination natively before start. The
 * nonzero native_task is an adapter-issued task identity, not a wire pointer.
 * Record it synchronously when native work starts, before callbacks/yields. */
int SudekiMpStoryAreaStarted(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner,
    uint64_t ticket,uint64_t native_task);
/* Exact positive terminal + placement proof. Actual area must be source or
 * destination; source represents a VERIFIED failed/rolled-back transition,
 * never a timeout assumption. Also permitted after connection revocation so
 * already-started native work can drain. Only this player changes location. */
int SudekiMpStoryAreaNativeReturned(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner,
    uint64_t ticket,uint64_t native_task,SudekiMpStoryAreaRef actual);
/* After native return AND positive release of old view/presentation owners
 * (matching client completion or verified disconnect cleanup), retire ticket.
 * This is not a network ACK handler; the caller proves its native boundary. */
int SudekiMpStoryAreaTravelReleased(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner,uint64_t ticket);
/* Disconnect revokes input, not native tasks, occupancy or camera references.
 * Unbind requires caller proof that the actor/view no longer needs the area. */
int SudekiMpStoryAreaRevoke(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner);
int SudekiMpStoryAreaUnbind(SudekiMpStoryAreas *,SudekiMpStoryAreaOwner);
/* Every asynchronous native dependency not covered by an actor/transition
 * gets its own pin. Retain all areas it uses. Only exact completion releases
 * that pin; repeated/stale returns cannot decrement a different obligation. */
int SudekiMpStoryAreaRetain(SudekiMpStoryAreas *,SudekiMpStoryAreaRef,uint64_t *pin);
int SudekiMpStoryAreaRelease(SudekiMpStoryAreas *,SudekiMpStoryAreaRef,uint64_t pin);
int SudekiMpStoryAreaRetireBegin(SudekiMpStoryAreas *,SudekiMpStoryAreaRef);
/* Only after exact native retirement/destruction has positively completed.
 * Failure retains RETIRING; never pretend an elapsed timeout unloaded it. */
int SudekiMpStoryAreaRetireReturned(SudekiMpStoryAreas *,SudekiMpStoryAreaRef);

#endif
