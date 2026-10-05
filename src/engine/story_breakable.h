#ifndef SUDEKIMP_STORY_BREAKABLE_H
#define SUDEKIMP_STORY_BREAKABLE_H

#include "engine/story_loot.h"

/* Host game-thread policy. An intent is NOT a reservation of an object.
 * Separate objects are independent; the first CONFIRMED native break consumes
 * the source and immediately invalidates every competing intent on it.
 * This module neither dispatches native actions nor deletes native objects. */
typedef enum SudekiMpStoryBreakablePhase {
    SUDEKIMP_STORY_BREAKABLE_EMPTY=0, SUDEKIMP_STORY_BREAKABLE_REQUESTED,
    SUDEKIMP_STORY_BREAKABLE_ACTIVE, SUDEKIMP_STORY_BREAKABLE_WON,
    SUDEKIMP_STORY_BREAKABLE_TARGET_GONE, SUDEKIMP_STORY_BREAKABLE_REVOKED
} SudekiMpStoryBreakablePhase;
typedef struct SudekiMpStoryBreakableIntent {
    uint64_t ticket, source, actor_generation, connection_generation, native_task;
    SudekiMpWalletCharacterId actor;
    uint32_t phase;
    /* Losing a target revokes interaction authority immediately. A native
     * task may still hold object/actor references: retain that obligation
     * until its EXACT completion/cancellation has positively returned. */
    uint8_t native_pending;
} SudekiMpStoryBreakableIntent;
typedef struct SudekiMpStoryBreakables {
    uint8_t save_identity[32];
    uint64_t visit, revision_floor, next_ticket;
    SudekiMpStoryBreakableIntent actors[4];
} SudekiMpStoryBreakables;

/* One module lifetime per save identity. A different visit is admitted only
 * after all previously started tasks positively drain. Same visit is a no-op,
 * not an opportunity to forget a target or replay a reward. */
int SudekiMpStoryBreakablesBind(SudekiMpStoryBreakables *,const SudekiMpStoryLootState *);
int SudekiMpStoryBreakableConsumed(const SudekiMpStoryLootState *,uint64_t source);
/* Caller must resolve a unique native INSTANCE in this visit (not a template
 * ResourceName, current list index, nearest-position guess, or native pointer)
 * and validate the actor's current generation, reach and native eligibility. */
int SudekiMpStoryBreakableRequest(SudekiMpStoryBreakables *,const SudekiMpStoryLootState *,
    SudekiMpWalletCharacterId actor,uint64_t source,uint64_t actor_generation,
    uint64_t connection_generation,SudekiMpStoryBreakableIntent *out);
int SudekiMpStoryBreakableCurrent(const SudekiMpStoryBreakables *,const SudekiMpStoryLootState *,
    const SudekiMpStoryBreakableIntent *);
/* Retain the returned native start identity BEFORE any other native callback
 * or yield. A queued request alone cannot enter ACTIVE. */
int SudekiMpStoryBreakableStarted(SudekiMpStoryBreakables *,const SudekiMpStoryLootState *,
    const SudekiMpStoryBreakableIntent *,uint64_t native_task);
/* Only the verified native effect owner may confirm, AFTER actual break proof.
 * winner_ticket zero denotes a proven native action outside the request path.
 * No request arrival, animation start or HUD notification is break proof.
 * Returns a bit mask in WALLET CHARACTER order of all invalidated intents,
 * including the winner. Rejection changes neither state nor output. */
SudekiMpStoryLootResult SudekiMpStoryBreakableConfirm(SudekiMpStoryBreakables *,
    SudekiMpStoryLootState *,uint64_t source,SudekiMpWalletCharacterId winner,
    uint64_t winner_ticket,uint8_t *invalidated);
/* Apply an authenticated account tombstone snapshot to pending intentions.
 * This invalidates authority, not native tasks. It never changes inventory. */
int SudekiMpStoryBreakablesReconcile(SudekiMpStoryBreakables *,const SudekiMpStoryLootState *);
/* Revoke only the departing actor/connection lease. A delayed disconnect from
 * an old connection must not revoke a new owner's intent. Neither this nor
 * RevokeAll pretends to cancel a native task or closes the caller's admission. */
int SudekiMpStoryBreakableRevokeActor(SudekiMpStoryBreakables *,SudekiMpWalletCharacterId,
    uint64_t actor_generation,uint64_t connection_generation);
void SudekiMpStoryBreakablesRevokeAll(SudekiMpStoryBreakables *);
/* Positive native terminal observation only; stale ticket/task callbacks
 * cannot retire a later action. Returns FALSE for unknown or mismatched work. */
int SudekiMpStoryBreakableReturned(SudekiMpStoryBreakables *,uint64_t ticket,uint64_t native_task);

#endif
