#include "engine/merchant_checkout.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpMerchantCatalogSnapshot catalog_for(uint64_t merchant) {
    SudekiMpMerchantCatalogSnapshot catalog;

    memset(&catalog, 0, sizeof(catalog));
    catalog.valid = 1;
    catalog.merchant_provenance = merchant;
    catalog.merchant_generation = 1u;
    catalog.catalog_generation = 1u;
    catalog.entry_count = 1u;
    catalog.entries[0].item_id = 7u;
    catalog.entries[0].unit_price = 100u;
    catalog.entries[0].listed_quantity = 4u;
    return catalog;
}

static uint32_t balance_of(const SudekiMpEconomyCoordinator *economy,
    SudekiMpWalletCharacterId character) {
    uint32_t balance = UINT32_MAX;
    CHECK(SudekiMpPersonalWalletGetBalance(&economy->wallet, character, &balance));
    return balance;
}

static void test_authenticated_personal_purchase(void) {
    SudekiMpEconomyCoordinator economy;
    SudekiMpMerchantCheckoutSession session;
    SudekiMpMerchantCatalogSnapshot catalog = catalog_for(0x1111u);
    SudekiMpEconomyObservation observation;
    uint8_t identity[32] = {1u};
    uint32_t owned = 0u;

    SudekiMpEconomyCoordinatorInitialize(&economy, identity);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(
        &economy.entitlements, 7u, 0u) == SUDEKIMP_ENTITLEMENT_REGISTERED);
    economy.wallet.character_balance[1] = 250u;
    SudekiMpMerchantCheckoutInitialize(&session);
    CHECK(SudekiMpMerchantCheckoutOpen(&session, 1u,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 9u, &catalog) ==
        SUDEKIMP_MERCHANT_CHECKOUT_OPENED);
    CHECK(SudekiMpMerchantCheckoutOpen(&session, 0u,
        SUDEKIMP_WALLET_CHARACTER_TAL, 7u, &catalog) ==
        SUDEKIMP_MERCHANT_CHECKOUT_OPENED);
    CHECK(SudekiMpMerchantCheckoutPlanPurchase(&session, &economy, 1u,
        session.seats[1].revision, 1u, 1u) ==
        SUDEKIMP_MERCHANT_CHECKOUT_APPLICATION_BEGUN);
    memset(&observation, 0, sizeof(observation));
    observation.outcome = SUDEKIMP_ECONOMY_EXTERNAL_VERIFIED;
    observation.observed_inventory_generation = 99u;
    observation.native_quantity_before = 0u;
    observation.native_quantity_after = 1u;
    CHECK(SudekiMpMerchantCheckoutResolvePurchase(&session, &economy, 1u,
        &observation) == SUDEKIMP_MERCHANT_CHECKOUT_APPLIED);
    CHECK(balance_of(&economy, SUDEKIMP_WALLET_CHARACTER_AILISH) == 150u);
    CHECK(balance_of(&economy, SUDEKIMP_WALLET_CHARACTER_TAL) == 0u);
    CHECK(SudekiMpEntitlementLedgerGetQuantity(&economy.entitlements, 7u,
        SUDEKIMP_WALLET_CHARACTER_AILISH, &owned));
    CHECK(owned == 1u);
}

static void test_catalog_and_owner_fail_closed(void) {
    SudekiMpEconomyCoordinator economy;
    SudekiMpMerchantCheckoutSession session;
    SudekiMpMerchantCatalogSnapshot catalog = catalog_for(0x2222u);
    uint8_t identity[32] = {2u};

    SudekiMpEconomyCoordinatorInitialize(&economy, identity);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(
        &economy.entitlements, 7u, 0u) == SUDEKIMP_ENTITLEMENT_REGISTERED);
    SudekiMpMerchantCheckoutInitialize(&session);
    CHECK(SudekiMpMerchantCheckoutOpen(&session, 1u,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 9u, &catalog) ==
        SUDEKIMP_MERCHANT_CHECKOUT_OPENED);
    CHECK(SudekiMpMerchantCheckoutSelect(&session, 1u,
        session.seats[1].revision + 1u, 0u) ==
        SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE);
    CHECK(SudekiMpMerchantCheckoutPlanPurchase(&session, &economy, 1u,
        session.seats[1].revision, 5u, 1u) ==
        SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_UNAVAILABLE);
    CHECK(SudekiMpMerchantCheckoutClose(&session, 1u, 8u) ==
        SUDEKIMP_MERCHANT_CHECKOUT_REJECTED_STALE);
}

int main(void) {
    test_authenticated_personal_purchase();
    test_catalog_and_owner_fail_closed();
    if (failures != 0) {
        fprintf(stderr, "merchant_checkout_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("merchant_checkout_test: PASS");
    return 0;
}
