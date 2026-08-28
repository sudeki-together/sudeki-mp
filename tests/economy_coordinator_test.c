#include "engine/economy_coordinator.h"

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

static SudekiMpEconomyRequest request_for(
    const SudekiMpEconomyCoordinator *coordinator,
    uint64_t serial,
    SudekiMpEconomyOperationKind kind
) {
    SudekiMpEconomyRequest request;

    memset(&request, 0, sizeof(request));
    request.operation_serial = serial;
    request.expected_coordinator_generation = coordinator->generation;
    request.expected_wallet_generation = coordinator->wallet.generation;
    request.expected_inventory_generation = coordinator->entitlements.generation;
    request.source_provenance = 1u;
    request.merchant_provenance = 2u;
    request.kind = kind;
    request.initiator = SUDEKIMP_WALLET_CHARACTER_AILISH;
    request.recipient = SUDEKIMP_WALLET_CHARACTER_TAL;
    request.item_id = 77u;
    request.quantity = 1u;
    request.amount = 100u;
    return request;
}

static uint32_t balance_of(
    const SudekiMpEconomyCoordinator *coordinator,
    SudekiMpWalletCharacterId character
) {
    uint32_t balance = UINT32_MAX;
    CHECK(SudekiMpPersonalWalletGetBalance(&coordinator->wallet, character,
        &balance));
    return balance;
}

static void test_dividend_and_reserve_distribution(void) {
    SudekiMpEconomyCoordinator coordinator;
    SudekiMpEconomyRequest request;
    SudekiMpEconomyObservation observation;
    uint8_t identity[32] = {1u};

    SudekiMpEconomyCoordinatorInitialize(&coordinator, identity);
    request = request_for(&coordinator, 1u,
        SUDEKIMP_ECONOMY_OPERATION_LITERAL_FLORIN);
    memset(&observation, 0, sizeof(observation));
    observation.outcome = SUDEKIMP_ECONOMY_EXTERNAL_VERIFIED;
    observation.observed_wallet_source_generation = 2u;
    CHECK(SudekiMpEconomyCoordinatorPlan(&coordinator, &request) ==
        SUDEKIMP_ECONOMY_PLAN_CREATED);
    CHECK(SudekiMpEconomyCoordinatorBegin(&coordinator, 1u) ==
        SUDEKIMP_ECONOMY_APPLICATION_BEGUN);
    CHECK(SudekiMpEconomyCoordinatorResolve(&coordinator, 1u, &observation) ==
        SUDEKIMP_ECONOMY_APPLIED);
    CHECK(balance_of(&coordinator, SUDEKIMP_WALLET_CHARACTER_TAL) == 100u);
    CHECK(balance_of(&coordinator, SUDEKIMP_WALLET_CHARACTER_ELCO) == 100u);

    coordinator.wallet.party_reserve = 100u;
    ++coordinator.wallet.generation;
    request = request_for(&coordinator, 2u,
        SUDEKIMP_ECONOMY_OPERATION_DISTRIBUTE_RESERVE);
    request.source_provenance = 0u;
    request.merchant_provenance = 0u;
    request.expected_inventory_generation = 0u;
    request.recipient = SUDEKIMP_WALLET_CHARACTER_BUKI;
    request.amount = 75u;
    memset(&observation, 0, sizeof(observation));
    observation.outcome = SUDEKIMP_ECONOMY_EXTERNAL_VERIFIED;
    CHECK(SudekiMpEconomyCoordinatorPlan(&coordinator, &request) ==
        SUDEKIMP_ECONOMY_PLAN_CREATED);
    CHECK(SudekiMpEconomyCoordinatorBegin(&coordinator, 2u) ==
        SUDEKIMP_ECONOMY_APPLICATION_BEGUN);
    CHECK(SudekiMpEconomyCoordinatorResolve(&coordinator, 2u, &observation) ==
        SUDEKIMP_ECONOMY_APPLIED);
    CHECK(coordinator.wallet.party_reserve == 25u);
    CHECK(balance_of(&coordinator, SUDEKIMP_WALLET_CHARACTER_BUKI) == 175u);
}

static void test_owned_item_sale_transfer_and_replay(void) {
    SudekiMpEconomyCoordinator coordinator;
    SudekiMpEconomyRequest request;
    SudekiMpEconomyObservation observation;
    uint8_t identity[32] = {2u};
    uint32_t quantity = 0u;

    SudekiMpEconomyCoordinatorInitialize(&coordinator, identity);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(
        &coordinator.entitlements, 77u, 1u) == SUDEKIMP_ENTITLEMENT_REGISTERED);
    CHECK(SudekiMpEntitlementLedgerClaimUnallocated(
        &coordinator.entitlements, coordinator.entitlements.generation, 77u,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 1u) == SUDEKIMP_ENTITLEMENT_APPLIED);
    request = request_for(&coordinator, 1u,
        SUDEKIMP_ECONOMY_OPERATION_TRANSFER_ENTITLEMENT);
    request.source_provenance = 0u;
    request.merchant_provenance = 0u;
    request.amount = 0u;
    request.recipient = SUDEKIMP_WALLET_CHARACTER_TAL;
    memset(&observation, 0, sizeof(observation));
    observation.outcome = SUDEKIMP_ECONOMY_EXTERNAL_VERIFIED;
    CHECK(SudekiMpEconomyCoordinatorPlan(&coordinator, &request) ==
        SUDEKIMP_ECONOMY_PLAN_CREATED);
    CHECK(SudekiMpEconomyCoordinatorBegin(&coordinator, 1u) ==
        SUDEKIMP_ECONOMY_APPLICATION_BEGUN);
    CHECK(SudekiMpEconomyCoordinatorResolve(&coordinator, 1u, &observation) ==
        SUDEKIMP_ECONOMY_APPLIED);
    CHECK(SudekiMpEntitlementLedgerGetQuantity(&coordinator.entitlements, 77u,
        SUDEKIMP_WALLET_CHARACTER_TAL, &quantity));
    CHECK(quantity == 1u);

    request = request_for(&coordinator, 2u,
        SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM);
    request.initiator = SUDEKIMP_WALLET_CHARACTER_TAL;
    request.amount = 100u;
    memset(&observation, 0, sizeof(observation));
    observation.outcome = SUDEKIMP_ECONOMY_EXTERNAL_VERIFIED;
    observation.observed_inventory_generation = 999u;
    observation.native_quantity_before = 1u;
    observation.native_quantity_after = 0u;
    CHECK(SudekiMpEconomyCoordinatorPlan(&coordinator, &request) ==
        SUDEKIMP_ECONOMY_PLAN_CREATED);
    CHECK(SudekiMpEconomyCoordinatorBegin(&coordinator, 2u) ==
        SUDEKIMP_ECONOMY_APPLICATION_BEGUN);
    CHECK(SudekiMpEconomyCoordinatorResolve(&coordinator, 2u, &observation) ==
        SUDEKIMP_ECONOMY_APPLIED);
    CHECK(balance_of(&coordinator, SUDEKIMP_WALLET_CHARACTER_TAL) == 100u);
    CHECK(balance_of(&coordinator, SUDEKIMP_WALLET_CHARACTER_AILISH) == 100u);
    CHECK(SudekiMpEntitlementLedgerGetQuantity(&coordinator.entitlements, 77u,
        SUDEKIMP_WALLET_CHARACTER_TAL, &quantity));
    CHECK(quantity == 0u);
    CHECK(SudekiMpEconomyCoordinatorResolve(&coordinator, 2u, &observation) ==
        SUDEKIMP_ECONOMY_ALREADY_APPLIED);
}

static void test_save_identity_mismatch_quarantines(void) {
    SudekiMpEconomyCoordinator source;
    SudekiMpEconomyCoordinator target;
    SudekiMpEconomyCoordinatorSnapshot snapshot;
    uint8_t one[32] = {3u};
    uint8_t two[32] = {4u};

    SudekiMpEconomyCoordinatorInitialize(&source, one);
    CHECK(SudekiMpEconomyCoordinatorExport(&source, &snapshot));
    SudekiMpEconomyCoordinatorInitialize(&target, two);
    CHECK(!SudekiMpEconomyCoordinatorRestore(&target, &snapshot, two));
    CHECK(target.state == SUDEKIMP_ECONOMY_QUARANTINED);
}

int main(void) {
    test_dividend_and_reserve_distribution();
    test_owned_item_sale_transfer_and_replay();
    test_save_identity_mismatch_quarantines();
    if (failures != 0) {
        fprintf(stderr, "economy_coordinator_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("economy_coordinator_test: PASS");
    return 0;
}
