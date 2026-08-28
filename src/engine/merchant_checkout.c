#include "engine/merchant_checkout.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) {
        ++value;
    }
    return value;
}

static SudekiMpMerchantCheckoutResult quarantine(
    SudekiMpMerchantCheckoutSession *session
) {
    session->state = SUDEKIMP_MERCHANT_CHECKOUT_QUARANTINED;
    return SUDEKIMP_MERCHANT_CHECKOUT_ENTERED_QUARANTINE;
}

static int catalog_valid(const SudekiMpMerchantCatalogSnapshot *catalog) {
    uint32_t entry_index;
    uint32_t prior_index;

    if (catalog == NULL || !catalog->valid ||
        catalog->merchant_provenance == 0u ||
        catalog->merchant_generation == 0u || catalog->catalog_generation == 0u ||
        catalog->entry_count == 0u ||
        catalog->entry_count > SUDEKIMP_MERCHANT_CHECKOUT_MAX_ENTRIES) {
        return 0;
    }
    for (entry_index = 0u; entry_index < catalog->entry_count; ++entry_index) {
        const SudekiMpMerchantCatalogEntry *entry = &catalog->entries[entry_index];
        if (entry->unit_price == 0u || entry->listed_quantity == 0u) {
            return 0;
        }
        for (prior_index = 0u; prior_index < entry_index; ++prior_index) {
            if (catalog->entries[prior_index].item_id == entry->item_id) {
                return 0;
            }
        }
    }
    return 1;
}

static int catalog_equal(
    const SudekiMpMerchantCatalogSnapshot *left,
    const SudekiMpMerchantCatalogSnapshot *right
) {
    uint32_t index;

    if (left->valid != right->valid ||
        left->merchant_provenance != right->merchant_provenance ||
        left->merchant_generation != right->merchant_generation ||
        left->catalog_generation != right->catalog_generation ||
        left->entry_count != right->entry_count) {
        return 0;
    }
    for (index = 0u; index < left->entry_count; ++index) {
        if (left->entries[index].item_id != right->entries[index].item_id ||
            left->entries[index].unit_price != right->entries[index].unit_price ||
            left->entries[index].listed_quantity !=
                right->entries[index].listed_quantity) {
            return 0;
        }
    }
    return 1;
}

static int seat_open(const SudekiMpMerchantCheckoutSession *session,
    uint32_t seat) {
    return seat < SUDEKIMP_MERCHANT_CHECKOUT_MAX_SEATS &&
        session->seats[seat].open;
}

static SudekiMpMerchantCheckoutResult map_economy_result(
    SudekiMpEconomyResult result
) {
    switch (result) {
    case SUDEKIMP_ECONOMY_PLAN_CREATED:
        return SUDEKIMP_MERCHANT_CHECKOUT_PLAN_CREATED;
    case SUDEKIMP_ECONOMY_APPLICATION_BEGUN:
        return SUDEKIMP_MERCHANT_CHECKOUT_APPLICATION_BEGUN;
    case SUDEKIMP_ECONOMY_APPLIED:
        return SUDEKIMP_MERCHANT_CHECKOUT_APPLIED;
    case SUDEKIMP_ECONOMY_CANCELLED:
        return SUDEKIMP_MERCHANT_CHECKOUT_CANCELLED;
    case SUDEKIMP_ECONOMY_REJECTED_FUNDS:
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_FUNDS;
    case SUDEKIMP_ECONOMY_REJECTED_STALE:
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE;
    case SUDEKIMP_ECONOMY_REJECTED_BUSY:
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY;
    case SUDEKIMP_ECONOMY_REJECTED_QUARANTINED:
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_QUARANTINED;
    case SUDEKIMP_ECONOMY_ENTERED_QUARANTINE:
        return SUDEKIMP_MERCHANT_CHECKOUT_ENTERED_QUARANTINE;
    default:
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
}

void SudekiMpMerchantCheckoutInitialize(
    SudekiMpMerchantCheckoutSession *session
) {
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->state = SUDEKIMP_MERCHANT_CHECKOUT_IDLE;
    session->generation = 1u;
    session->applying_seat = UINT32_MAX;
}

SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutOpen(
    SudekiMpMerchantCheckoutSession *session,
    uint32_t seat,
    SudekiMpWalletCharacterId character_id,
    uint32_t actor_generation,
    const SudekiMpMerchantCatalogSnapshot *catalog
) {
    SudekiMpMerchantCheckoutSeat *checkout_seat;

    if (session == NULL || seat >= SUDEKIMP_MERCHANT_CHECKOUT_MAX_SEATS ||
        actor_generation == 0u ||
        !SudekiMpPersonalWalletCharacterIndex(character_id, NULL) ||
        !catalog_valid(catalog)) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_QUARANTINED) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_QUARANTINED;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_APPLYING) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_IDLE) {
        session->catalog = *catalog;
        session->state = SUDEKIMP_MERCHANT_CHECKOUT_BROWSING;
    } else if (!catalog_equal(&session->catalog, catalog)) {
        return quarantine(session);
    }
    checkout_seat = &session->seats[seat];
    if (checkout_seat->open &&
        (checkout_seat->character_id != character_id ||
         checkout_seat->actor_generation != actor_generation)) {
        return quarantine(session);
    }
    checkout_seat->open = 1;
    checkout_seat->character_id = character_id;
    checkout_seat->actor_generation = actor_generation;
    checkout_seat->selected_entry = 0u;
    checkout_seat->revision = advance_nonzero(checkout_seat->revision);
    session->generation = advance_nonzero(session->generation);
    return SUDEKIMP_MERCHANT_CHECKOUT_OPENED;
}

SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutSelect(
    SudekiMpMerchantCheckoutSession *session,
    uint32_t seat,
    uint32_t expected_revision,
    uint32_t entry_index
) {
    SudekiMpMerchantCheckoutSeat *checkout_seat;

    if (session == NULL || !seat_open(session, seat)) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_QUARANTINED) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_QUARANTINED;
    }
    if (session->state != SUDEKIMP_MERCHANT_CHECKOUT_BROWSING) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY;
    }
    checkout_seat = &session->seats[seat];
    if (checkout_seat->revision != expected_revision) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE;
    }
    if (entry_index >= session->catalog.entry_count) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_UNAVAILABLE;
    }
    checkout_seat->selected_entry = entry_index;
    checkout_seat->revision = advance_nonzero(checkout_seat->revision);
    return SUDEKIMP_MERCHANT_CHECKOUT_SELECTION_CHANGED;
}

SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutPlanPurchase(
    SudekiMpMerchantCheckoutSession *session,
    SudekiMpEconomyCoordinator *economy,
    uint32_t seat,
    uint32_t expected_revision,
    uint32_t quantity,
    uint64_t operation_serial
) {
    SudekiMpMerchantCheckoutSeat *checkout_seat;
    const SudekiMpMerchantCatalogEntry *entry;
    SudekiMpEconomyRequest request;
    uint64_t amount;
    SudekiMpEconomyResult result;

    if (session == NULL || economy == NULL || !seat_open(session, seat) ||
        quantity == 0u || operation_serial == 0u) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_QUARANTINED ||
        economy->state == SUDEKIMP_ECONOMY_QUARANTINED) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_QUARANTINED;
    }
    if (session->state != SUDEKIMP_MERCHANT_CHECKOUT_BROWSING) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY;
    }
    checkout_seat = &session->seats[seat];
    if (checkout_seat->revision != expected_revision) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE;
    }
    entry = &session->catalog.entries[checkout_seat->selected_entry];
    if (quantity > entry->listed_quantity) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_UNAVAILABLE;
    }
    amount = (uint64_t)entry->unit_price * quantity;
    if (amount == 0u || amount > UINT32_MAX) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
    memset(&request, 0, sizeof(request));
    request.operation_serial = operation_serial;
    request.expected_coordinator_generation = economy->generation;
    request.expected_wallet_generation = economy->wallet.generation;
    request.expected_inventory_generation = economy->entitlements.generation;
    request.source_provenance = session->catalog.merchant_provenance;
    request.merchant_provenance = session->catalog.merchant_provenance;
    request.kind = SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM;
    request.initiator = checkout_seat->character_id;
    request.recipient = checkout_seat->character_id;
    request.item_id = entry->item_id;
    request.quantity = quantity;
    request.amount = (uint32_t)amount;
    result = SudekiMpEconomyCoordinatorPlan(economy, &request);
    if (result != SUDEKIMP_ECONOMY_PLAN_CREATED) {
        return map_economy_result(result);
    }
    result = SudekiMpEconomyCoordinatorBegin(economy, operation_serial);
    if (result != SUDEKIMP_ECONOMY_APPLICATION_BEGUN) {
        return quarantine(session);
    }
    session->applying_seat = seat;
    session->applying_serial = operation_serial;
    session->applying_entry = checkout_seat->selected_entry;
    session->applying_quantity = quantity;
    session->state = SUDEKIMP_MERCHANT_CHECKOUT_APPLYING;
    return SUDEKIMP_MERCHANT_CHECKOUT_APPLICATION_BEGUN;
}

SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutResolvePurchase(
    SudekiMpMerchantCheckoutSession *session,
    SudekiMpEconomyCoordinator *economy,
    uint64_t operation_serial,
    const SudekiMpEconomyObservation *observation
) {
    SudekiMpEconomyResult result;

    if (session == NULL || economy == NULL || observation == NULL ||
        operation_serial == 0u) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_QUARANTINED) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_QUARANTINED;
    }
    if (session->state != SUDEKIMP_MERCHANT_CHECKOUT_APPLYING ||
        session->applying_serial != operation_serial) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY;
    }
    result = SudekiMpEconomyCoordinatorResolve(economy, operation_serial,
        observation);
    if (result == SUDEKIMP_ECONOMY_APPLIED ||
        result == SUDEKIMP_ECONOMY_CANCELLED) {
        session->seats[session->applying_seat].revision = advance_nonzero(
            session->seats[session->applying_seat].revision);
        session->applying_seat = UINT32_MAX;
        session->applying_serial = 0u;
        session->applying_entry = 0u;
        session->applying_quantity = 0u;
        session->state = SUDEKIMP_MERCHANT_CHECKOUT_BROWSING;
        return result == SUDEKIMP_ECONOMY_APPLIED ?
            SUDEKIMP_MERCHANT_CHECKOUT_APPLIED :
            SUDEKIMP_MERCHANT_CHECKOUT_CANCELLED;
    }
    return quarantine(session);
}

SudekiMpMerchantCheckoutResult SudekiMpMerchantCheckoutClose(
    SudekiMpMerchantCheckoutSession *session,
    uint32_t seat,
    uint32_t actor_generation
) {
    SudekiMpMerchantCheckoutSeat *checkout_seat;
    uint32_t index;

    if (session == NULL || !seat_open(session, seat) || actor_generation == 0u) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_INVALID;
    }
    if (session->state == SUDEKIMP_MERCHANT_CHECKOUT_APPLYING) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_BUSY;
    }
    checkout_seat = &session->seats[seat];
    if (checkout_seat->actor_generation != actor_generation) {
        return SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE;
    }
    memset(checkout_seat, 0, sizeof(*checkout_seat));
    for (index = 0u; index < SUDEKIMP_MERCHANT_CHECKOUT_MAX_SEATS; ++index) {
        if (session->seats[index].open) {
            return SUDEKIMP_MERCHANT_CHECKOUT_NO_CHANGE;
        }
    }
    memset(&session->catalog, 0, sizeof(session->catalog));
    session->state = SUDEKIMP_MERCHANT_CHECKOUT_IDLE;
    session->generation = advance_nonzero(session->generation);
    return SUDEKIMP_MERCHANT_CHECKOUT_NO_CHANGE;
}
