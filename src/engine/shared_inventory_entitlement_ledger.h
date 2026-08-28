#ifndef SUDEKIMP_SHARED_INVENTORY_ENTITLEMENT_LEDGER_H
#define SUDEKIMP_SHARED_INVENTORY_ENTITLEMENT_LEDGER_H

#include <stdint.h>

#include "engine/personal_wallet.h"

/* The ledger is deliberately bounded and pointer-free so it can be persisted
 * and audited without copying native inventory or UI objects. An item is only
 * tracked after the integration layer explicitly allowlists it. */
enum {
    SUDEKIMP_ENTITLEMENT_LEDGER_SCHEMA_VERSION = 1u,
    SUDEKIMP_ENTITLEMENT_LEDGER_MAX_ITEMS = 128u
};

typedef enum SudekiMpEntitlementLedgerState {
    SUDEKIMP_ENTITLEMENT_LEDGER_READY = 0,
    SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED
} SudekiMpEntitlementLedgerState;

typedef enum SudekiMpEntitlementResult {
    SUDEKIMP_ENTITLEMENT_NO_CHANGE = 0,
    SUDEKIMP_ENTITLEMENT_REGISTERED,
    SUDEKIMP_ENTITLEMENT_APPLIED,
    SUDEKIMP_ENTITLEMENT_REJECTED_INVALID,
    SUDEKIMP_ENTITLEMENT_REJECTED_INELIGIBLE,
    SUDEKIMP_ENTITLEMENT_REJECTED_STALE,
    SUDEKIMP_ENTITLEMENT_REJECTED_OWNERSHIP,
    SUDEKIMP_ENTITLEMENT_REJECTED_CAPACITY,
    SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED,
    SUDEKIMP_ENTITLEMENT_ENTERED_QUARANTINE
} SudekiMpEntitlementResult;

typedef struct SudekiMpEntitlementEntry {
    /* item_id 0 is valid; active distinguishes an unused table slot. */
    int active;
    uint32_t item_id;
    uint32_t owner_quantity[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t unallocated_quantity;
} SudekiMpEntitlementEntry;

typedef struct SudekiMpEntitlementLedgerSnapshot {
    uint32_t schema_version;
    uint32_t generation;
    uint32_t entry_count;
    SudekiMpEntitlementEntry entries[SUDEKIMP_ENTITLEMENT_LEDGER_MAX_ITEMS];
} SudekiMpEntitlementLedgerSnapshot;

typedef struct SudekiMpSharedInventoryEntitlementLedger {
    SudekiMpEntitlementLedgerState state;
    uint32_t generation;
    uint32_t entry_count;
    SudekiMpEntitlementEntry entries[SUDEKIMP_ENTITLEMENT_LEDGER_MAX_ITEMS];
} SudekiMpSharedInventoryEntitlementLedger;

void SudekiMpSharedInventoryEntitlementLedgerInitialize(
    SudekiMpSharedInventoryEntitlementLedger *ledger
);

/* Registers a consumable/material as eligible and seeds all existing native
 * copies as unallocated. Registering keys, equipment, or an unproven item is
 * intentionally the responsibility of a stricter integration allowlist. */
SudekiMpEntitlementResult SudekiMpEntitlementLedgerRegisterEligible(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    uint32_t native_quantity
);

/* Reconciliation is the invariant gate: native_quantity must exactly equal
 * Tal + Ailish + Buki + Elco + unallocated. A mismatch quarantines rather
 * than assigning unowned shared items by guesswork. */
SudekiMpEntitlementResult SudekiMpEntitlementLedgerVerifyNativeQuantity(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    uint32_t native_quantity
);

SudekiMpEntitlementResult SudekiMpEntitlementLedgerClaimUnallocated(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t quantity
);

/* These are called only after a native add/remove has been independently
 * verified. They preserve the invariant at every completed operation. */
SudekiMpEntitlementResult SudekiMpEntitlementLedgerApplyNativeAdd(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t quantity,
    uint32_t native_before,
    uint32_t native_after
);
SudekiMpEntitlementResult SudekiMpEntitlementLedgerApplyNativeRemove(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t quantity,
    uint32_t native_before,
    uint32_t native_after
);
SudekiMpEntitlementResult SudekiMpEntitlementLedgerTransfer(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId from,
    SudekiMpWalletCharacterId to,
    uint32_t quantity
);

int SudekiMpEntitlementLedgerGetQuantity(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t *quantity
);
int SudekiMpEntitlementLedgerExport(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    SudekiMpEntitlementLedgerSnapshot *snapshot
);
int SudekiMpEntitlementLedgerRestore(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    const SudekiMpEntitlementLedgerSnapshot *snapshot
);
int SudekiMpEntitlementLedgerRecover(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    const SudekiMpEntitlementLedgerSnapshot *trusted_snapshot
);

#endif
