#include "engine/shared_inventory_entitlement_ledger.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static uint32_t owner_quantity(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner
) {
    uint32_t value = UINT32_MAX;
    CHECK(SudekiMpEntitlementLedgerGetQuantity(
        ledger, item_id, owner, &value));
    return value;
}

static void test_invariant_claim_transfer_and_owner_only_remove(void) {
    SudekiMpSharedInventoryEntitlementLedger ledger;
    uint32_t generation;

    SudekiMpSharedInventoryEntitlementLedgerInitialize(&ledger);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(&ledger, 0u, 10u) ==
        SUDEKIMP_ENTITLEMENT_REGISTERED);
    generation = ledger.generation;
    CHECK(SudekiMpEntitlementLedgerClaimUnallocated(
        &ledger, generation, 0u, SUDEKIMP_WALLET_CHARACTER_AILISH, 4u) ==
        SUDEKIMP_ENTITLEMENT_APPLIED);
    CHECK(owner_quantity(&ledger, 0u, SUDEKIMP_WALLET_CHARACTER_AILISH) == 4u);
    CHECK(SudekiMpEntitlementLedgerVerifyNativeQuantity(&ledger, 0u, 10u) ==
        SUDEKIMP_ENTITLEMENT_NO_CHANGE);
    generation = ledger.generation;
    CHECK(SudekiMpEntitlementLedgerTransfer(
        &ledger, generation, 0u, SUDEKIMP_WALLET_CHARACTER_AILISH,
        SUDEKIMP_WALLET_CHARACTER_ELCO, 3u) == SUDEKIMP_ENTITLEMENT_APPLIED);
    CHECK(owner_quantity(&ledger, 0u, SUDEKIMP_WALLET_CHARACTER_AILISH) == 1u);
    CHECK(owner_quantity(&ledger, 0u, SUDEKIMP_WALLET_CHARACTER_ELCO) == 3u);
    generation = ledger.generation;
    CHECK(SudekiMpEntitlementLedgerApplyNativeRemove(
        &ledger, generation, 0u, SUDEKIMP_WALLET_CHARACTER_TAL, 1u, 10u, 9u) ==
        SUDEKIMP_ENTITLEMENT_ENTERED_QUARANTINE);
    CHECK(ledger.state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED);
}

static void test_native_add_remove_and_stale_generation(void) {
    SudekiMpSharedInventoryEntitlementLedger ledger;
    uint32_t generation;

    SudekiMpSharedInventoryEntitlementLedgerInitialize(&ledger);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(&ledger, 77u, 2u) ==
        SUDEKIMP_ENTITLEMENT_REGISTERED);
    generation = ledger.generation;
    CHECK(SudekiMpEntitlementLedgerApplyNativeAdd(
        &ledger, generation, 77u, SUDEKIMP_WALLET_CHARACTER_BUKI,
        2u, 2u, 4u) == SUDEKIMP_ENTITLEMENT_APPLIED);
    CHECK(owner_quantity(&ledger, 77u, SUDEKIMP_WALLET_CHARACTER_BUKI) == 2u);
    CHECK(SudekiMpEntitlementLedgerApplyNativeAdd(
        &ledger, generation, 77u, SUDEKIMP_WALLET_CHARACTER_BUKI,
        1u, 4u, 5u) == SUDEKIMP_ENTITLEMENT_REJECTED_STALE);
    generation = ledger.generation;
    CHECK(SudekiMpEntitlementLedgerApplyNativeRemove(
        &ledger, generation, 77u, SUDEKIMP_WALLET_CHARACTER_BUKI,
        1u, 4u, 3u) == SUDEKIMP_ENTITLEMENT_APPLIED);
    CHECK(owner_quantity(&ledger, 77u, SUDEKIMP_WALLET_CHARACTER_BUKI) == 1u);
    CHECK(SudekiMpEntitlementLedgerVerifyNativeQuantity(&ledger, 77u, 3u) ==
        SUDEKIMP_ENTITLEMENT_NO_CHANGE);
}

static void test_restore_rejects_duplicate_ids_and_recovers(void) {
    SudekiMpSharedInventoryEntitlementLedger ledger;
    SudekiMpEntitlementLedgerSnapshot snapshot;
    SudekiMpEntitlementLedgerSnapshot trusted;

    SudekiMpSharedInventoryEntitlementLedgerInitialize(&ledger);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(&ledger, 1u, 3u) ==
        SUDEKIMP_ENTITLEMENT_REGISTERED);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(&ledger, 2u, 4u) ==
        SUDEKIMP_ENTITLEMENT_REGISTERED);
    CHECK(SudekiMpEntitlementLedgerExport(&ledger, &trusted));
    snapshot = trusted;
    snapshot.entries[1].item_id = snapshot.entries[0].item_id;
    CHECK(!SudekiMpEntitlementLedgerRestore(&ledger, &snapshot));
    CHECK(ledger.state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED);
    CHECK(SudekiMpEntitlementLedgerRecover(&ledger, &trusted));
    CHECK(ledger.state == SUDEKIMP_ENTITLEMENT_LEDGER_READY);
    CHECK(SudekiMpEntitlementLedgerVerifyNativeQuantity(&ledger, 2u, 4u) ==
        SUDEKIMP_ENTITLEMENT_NO_CHANGE);
}

int main(void) {
    test_invariant_claim_transfer_and_owner_only_remove();
    test_native_add_remove_and_stale_generation();
    test_restore_rejects_duplicate_ids_and_recovers();
    if (failures != 0) {
        fprintf(stderr, "shared_inventory_entitlement_ledger_test: %d failure(s)\n",
            failures);
        return 1;
    }
    puts("shared_inventory_entitlement_ledger_test: PASS");
    return 0;
}
