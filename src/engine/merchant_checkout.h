#ifndef SUDEKIMP_MERCHANT_CHECKOUT_H
#define SUDEKIMP_MERCHANT_CHECKOUT_H

#include <stdint.h>

#include "engine/economy_coordinator.h"

enum {
    SUDEKIMP_MERCHANT_CHECKOUT_MAX_SEATS = 4u,
    SUDEKIMP_MERCHANT_CHECKOUT_MAX_ENTRIES = 128u
};

typedef enum SudekiMpMerchantCheckoutState {
    SUDEKIMP_MERCHANT_CHECKOUT_IDLE = 0,
    SUDEKIMP_MERCHANT_CHECKOUT_BROWSING,
    SUDEKIMP_MERCHANT_CHECKOUT_APPLYING,
    SUDEKIMP_MERCHANT_CHECKOUT_QUARANTINED
} SudekiMpMerchantCheckoutState;

typedef enum SudekiMpMerchantCheckoutResult {
    SUDEKIMP_MERCHANT_CHECKOUT_NO_CHANGE = 0,
    SUDEKIMP_MERCHANT_CHECKOUT_OPENED,
    SUDEKIMP_MERCHANT_CHECKOUT_SELECTION_CHANGED,
    SUDEKIMP_MERCHANT_CHECKOUT_PLAN_CREATED,
    SUDEKIMP_MERCHANT_CHECKOUT_APPLICATION_BEGUN,
    SUDEKIMP_MERCHANT_CHECKOUT_APPLIED,
    SUDEKIMP_MERCHANT_CHECKOUT_CANCELLED,
    SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID,
    SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE,
    SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_FUNDS,
    SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_UNAVAILABLE,
    SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY,
    SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_QUARANTINED,
    SUDEKIMP_MERCHANT_CHECKOUT_ENTERED_QUARANTINE
} SudekiMpMerchantCheckoutResult;

typedef struct SudekiMpMerchantCatalogEntry {
    uint32_t item_id;
    uint32_t unit_price;
    uint32_t listed_quantity;
} SudekiMpMerchantCatalogEntry;

/* Pointer-free catalog copied from a validated CShopInventory traversal. Its
 * generation changes whenever the merchant/listing snapshot changes. */
typedef struct SudekiMpMerchantCatalogSnapshot {
    int valid;
    uint64_t merchant_provenance;
    uint32_t merchant_generation;
    uint32_t catalog_generation;
    uint32_t entry_count;
    SudekiMpMerchantCatalogEntry entries[SUDEKIMP_MERCHANT_CHECKOUT_MAX_ENTRIES];
} SudekiMpMerchantCatalogSnapshot;

typedef struct SudekiMpMerchantCheckoutSeat {
    int open;
    SudekiMpWalletCharacterId character_id;
    uint32_t actor_generation;
    uint32_t revision;
    uint32_t selected_entry;
} SudekiMpMerchantCheckoutSeat;

typedef struct SudekiMpMerchantCheckoutSession {
    SudekiMpMerchantCheckoutState state;
    uint32_t generation;
    SudekiMpMerchantCatalogSnapshot catalog;
    SudekiMpMerchantCheckoutSeat seats[SUDEKIMP_MERCHANT_CHECKOUT_MAX_SEATS];
    uint32_t applying_seat;
    uint64_t applying_serial;
    uint32_t applying_entry;
    uint32_t applying_quantity;
} SudekiMpMerchantCheckoutSession;

void SudekiMpMerchantCheckoutInitialize(
    SudekiMpMerchantCheckoutSession *session
);
/* A seat can browse only with its own stable character and current lease.
 * Multiple seats may browse the same shared catalog, but only one verified
 * checkout can apply at a time. */
SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutOpen(
    SudekiMpMerchantCheckoutSession *session,
    uint32_t seat,
    SudekiMpWalletCharacterId character_id,
    uint32_t actor_generation,
    const SudekiMpMerchantCatalogSnapshot *catalog
);
SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutSelect(
    SudekiMpMerchantCheckoutSession *session,
    uint32_t seat,
    uint32_t expected_revision,
    uint32_t entry_index
);
/* Plans and begins a personal-wallet purchase. The caller must subsequently
 * provide exact native item count and generation observations to Resolve. */
SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutPlanPurchase(
    SudekiMpMerchantCheckoutSession *session,
    SudekiMpEconomyCoordinator *economy,
    uint32_t seat,
    uint32_t expected_revision,
    uint32_t quantity,
    uint64_t operation_serial
);
SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutResolvePurchase(
    SudekiMpMerchantCheckoutSession *session,
    SudekiMpEconomyCoordinator *economy,
    uint64_t operation_serial,
    const SudekiMpEconomyObservation *observation
);
SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutClose(
    SudekiMpMerchantCheckoutSession *session,
    uint32_t seat,
    uint32_t actor_generation
);

#endif
