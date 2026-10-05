#ifndef SUDEKIMP_STORY_LOOT_H
#define SUDEKIMP_STORY_LOOT_H

#include "engine/personal_wallet.h"
#include <stddef.h>
#include <stdint.h>

/* Saved-story policy only. NOT a native inventory adapter. Four independent
 * character stocks must never be summed into the retail singleton inventory.
 * Keys, quest items and equipment are not eligible for this first adapter. */
enum { SUDEKIMP_STORY_LOOT_MAX_ITEMS=128, SUDEKIMP_STORY_LOOT_MAX_SOURCES=512,
    SUDEKIMP_STORY_LOOT_MAX_LINES=8, SUDEKIMP_STORY_LOOT_SCHEMA=1 };
typedef enum SudekiMpStoryLootItemKind {
    SUDEKIMP_STORY_LOOT_CONSUMABLE=1, SUDEKIMP_STORY_LOOT_MATERIAL
} SudekiMpStoryLootItemKind;
typedef enum SudekiMpStoryLootOperation {
    SUDEKIMP_STORY_LOOT_BREAK=1, SUDEKIMP_STORY_LOOT_WORLD_REWARD,
    SUDEKIMP_STORY_LOOT_BUY, SUDEKIMP_STORY_LOOT_SELL,
    SUDEKIMP_STORY_LOOT_USE, SUDEKIMP_STORY_LOOT_FORGE
} SudekiMpStoryLootOperation;
typedef enum SudekiMpStoryLootResult {
    SUDEKIMP_STORY_LOOT_INVALID=0, SUDEKIMP_STORY_LOOT_APPLIED,
    SUDEKIMP_STORY_LOOT_ALREADY_APPLIED, SUDEKIMP_STORY_LOOT_STALE,
    SUDEKIMP_STORY_LOOT_CAPACITY, SUDEKIMP_STORY_LOOT_FUNDS,
    SUDEKIMP_STORY_LOOT_OWNERSHIP, SUDEKIMP_STORY_LOOT_INELIGIBLE
} SudekiMpStoryLootResult;
typedef struct SudekiMpStoryLootItem {
    uint32_t id, cap, kind;
    uint32_t quantity[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
} SudekiMpStoryLootItem;
typedef struct SudekiMpStoryLootSource {
    uint64_t id, operation;
    /* BREAK (container identity) or WORLD_REWARD (consumed drop identity).
     * Different drops from one barrel have distinct canonical source IDs. */
    uint32_t kind;
} SudekiMpStoryLootSource;
typedef struct SudekiMpStoryLootState {
    uint8_t save_identity[32];
    uint64_t revision, visit, operation;
    uint32_t reserve, money[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t item_count, source_count;
    SudekiMpStoryLootItem items[SUDEKIMP_STORY_LOOT_MAX_ITEMS];
    SudekiMpStoryLootSource sources[SUDEKIMP_STORY_LOOT_MAX_SOURCES];
} SudekiMpStoryLootState;
typedef struct SudekiMpStoryLootLine { uint32_t item, quantity; } SudekiMpStoryLootLine;
typedef struct SudekiMpStoryLootEvent {
    /* Operation numbers order confirmed account commits on the game thread.
     * They are NOT interaction reservations: different objects may be breaking
     * concurrently and their drops may finish in either order. Notification
     * playback must not block world actions or repeat an account mutation. */
    uint64_t visit, operation, source;
    uint32_t kind;
    SudekiMpWalletCharacterId actor;
    /* Total price/proceeds/florins, NOT unit price. */
    uint32_t amount, line_count;
    SudekiMpStoryLootLine lines[SUDEKIMP_STORY_LOOT_MAX_LINES];
} SudekiMpStoryLootEvent;
typedef struct SudekiMpStoryLootReceipt {
    uint64_t operation;
    uint32_t money_credit[4], money_debit[4], money_overflow[4];
    uint32_t item_credit[SUDEKIMP_STORY_LOOT_MAX_LINES][4];
    uint32_t item_debit[SUDEKIMP_STORY_LOOT_MAX_LINES][4];
    uint32_t item_overflow[SUDEKIMP_STORY_LOOT_MAX_LINES][4];
} SudekiMpStoryLootReceipt;

int SudekiMpStoryLootValid(const SudekiMpStoryLootState *state);
/* Monotonic account/catalog/source history for persistence and replicas.
 * A newer revision cannot forget a paid source in the same world visit. */
int SudekiMpStoryLootAdvances(const SudekiMpStoryLootState *before,const SudekiMpStoryLootState *after);
int SudekiMpStoryLootInitialize(SudekiMpStoryLootState *state,
    const uint8_t save_identity[32], uint32_t legacy_reserve);
/* Native adapter must prove a newly loaded world incarnation. Reconnection,
 * a revised roster, or seeing the same area name is NOT a new visit. Equal
 * visits are idempotent and never clear the consumed-source tombstones. */
SudekiMpStoryLootResult SudekiMpStoryLootBeginVisit(SudekiMpStoryLootState *,uint64_t visit);
/* Exact item-definition adapter supplies the classification and native cap.
 * This call does not classify an arbitrary remote item ID. */
SudekiMpStoryLootResult SudekiMpStoryLootRegisterItem(SudekiMpStoryLootState *,
    uint32_t id,uint32_t cap,SudekiMpStoryLootItemKind kind);
/* Pure preflight: no native actions, no state mutation. The returned state is
 * a proposal, not a receipt of native execution. */
SudekiMpStoryLootResult SudekiMpStoryLootPlan(const SudekiMpStoryLootState *,
    const SudekiMpStoryLootEvent *,SudekiMpStoryLootState *after,SudekiMpStoryLootReceipt *);
/* Only a canonical native-thread adapter may call this AFTER exact source,
 * actor, effect, and before/after proofs. It is not a client request handler.
 * No HUD popup or raw AddItem/AddMoney call alone is sufficient provenance.
 * Unknown/partial native outcomes must be quarantined by that adapter. */
SudekiMpStoryLootResult SudekiMpStoryLootApplyConfirmed(SudekiMpStoryLootState *,
    const SudekiMpStoryLootEvent *,SudekiMpStoryLootReceipt *);

#endif
