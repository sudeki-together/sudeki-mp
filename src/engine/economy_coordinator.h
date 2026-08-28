#ifndef SUDEKIMP_ECONOMY_COORDINATOR_H
#define SUDEKIMP_ECONOMY_COORDINATOR_H

#include <stdint.h>

#include "engine/personal_wallet.h"
#include "engine/shared_inventory_entitlement_ledger.h"

/* This module owns ordering only. Native adapters must prove their own source,
 * merchant, and before/after inventory observations; this core never receives
 * a native pointer or performs a game mutation. */
typedef enum SudekiMpEconomyCoordinatorState {
    SUDEKIMP_ECONOMY_READY = 0,
    SUDEKIMP_ECONOMY_PLANNED,
    SUDEKIMP_ECONOMY_APPLYING,
    SUDEKIMP_ECONOMY_QUARANTINED
} SudekiMpEconomyCoordinatorState;

typedef enum SudekiMpEconomyOperationKind {
    SUDEKIMP_ECONOMY_OPERATION_LITERAL_FLORIN = 0,
    SUDEKIMP_ECONOMY_OPERATION_QUEST_DIVIDEND,
    SUDEKIMP_ECONOMY_OPERATION_PICKUP_ELIGIBLE_ITEM,
    SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM,
    SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM,
    SUDEKIMP_ECONOMY_OPERATION_FORGE,
    SUDEKIMP_ECONOMY_OPERATION_TRANSFER_ENTITLEMENT,
    SUDEKIMP_ECONOMY_OPERATION_DISTRIBUTE_RESERVE
} SudekiMpEconomyOperationKind;

typedef enum SudekiMpEconomyExternalOutcome {
    SUDEKIMP_ECONOMY_EXTERNAL_NOT_APPLIED = 0,
    SUDEKIMP_ECONOMY_EXTERNAL_VERIFIED,
    SUDEKIMP_ECONOMY_EXTERNAL_AMBIGUOUS
} SudekiMpEconomyExternalOutcome;

typedef enum SudekiMpEconomyResult {
    SUDEKIMP_ECONOMY_NO_CHANGE = 0,
    SUDEKIMP_ECONOMY_PLAN_CREATED,
    SUDEKIMP_ECONOMY_PLAN_REPLAYED,
    SUDEKIMP_ECONOMY_APPLICATION_BEGUN,
    SUDEKIMP_ECONOMY_APPLIED,
    SUDEKIMP_ECONOMY_ALREADY_APPLIED,
    SUDEKIMP_ECONOMY_CANCELLED,
    SUDEKIMP_ECONOMY_REJECTED_INVALID,
    SUDEKIMP_ECONOMY_REJECTED_BUSY,
    SUDEKIMP_ECONOMY_REJECTED_STALE,
    SUDEKIMP_ECONOMY_REJECTED_FUNDS,
    SUDEKIMP_ECONOMY_REJECTED_OWNERSHIP,
    SUDEKIMP_ECONOMY_REJECTED_QUARANTINED,
    SUDEKIMP_ECONOMY_ENTERED_QUARANTINE
} SudekiMpEconomyResult;

typedef struct SudekiMpEconomyRequest {
    uint64_t operation_serial;
    uint32_t expected_coordinator_generation;
    uint32_t expected_wallet_generation;
    uint32_t expected_inventory_generation;
    /* Opaque immutable values from a proven source/merchant adapter. */
    uint64_t source_provenance;
    uint64_t merchant_provenance;
    SudekiMpEconomyOperationKind kind;
    SudekiMpWalletCharacterId initiator;
    SudekiMpWalletCharacterId recipient;
    uint32_t item_id;
    uint32_t quantity;
    uint32_t amount;
} SudekiMpEconomyRequest;

typedef struct SudekiMpEconomyObservation {
    SudekiMpEconomyExternalOutcome outcome;
    uint32_t observed_wallet_source_generation;
    uint32_t observed_inventory_generation;
    uint32_t native_quantity_before;
    uint32_t native_quantity_after;
} SudekiMpEconomyObservation;

typedef struct SudekiMpEconomyCoordinatorSnapshot {
    uint32_t schema_version;
    uint32_t generation;
    uint64_t highest_operation_serial;
    uint8_t native_save_identity[32];
    SudekiMpWalletPersistedSnapshot wallet;
    SudekiMpEntitlementLedgerSnapshot entitlements;
} SudekiMpEconomyCoordinatorSnapshot;

typedef struct SudekiMpEconomyCoordinator {
    SudekiMpEconomyCoordinatorState state;
    uint32_t generation;
    uint64_t highest_operation_serial;
    uint8_t native_save_identity[32];
    SudekiMpPersonalWallet wallet;
    SudekiMpSharedInventoryEntitlementLedger entitlements;
    SudekiMpEconomyRequest pending;
    int pending_uses_wallet;
    int last_serial_valid;
    uint64_t last_serial;
} SudekiMpEconomyCoordinator;

enum { SUDEKIMP_ECONOMY_COORDINATOR_SCHEMA_VERSION = 1u };

void SudekiMpEconomyCoordinatorInitialize(
    SudekiMpEconomyCoordinator *coordinator,
    const uint8_t native_save_identity[32]
);
SudekiMpEconomyResult SudekiMpEconomyCoordinatorPlan(
    SudekiMpEconomyCoordinator *coordinator,
    const SudekiMpEconomyRequest *request
);
SudekiMpEconomyResult SudekiMpEconomyCoordinatorBegin(
    SudekiMpEconomyCoordinator *coordinator,
    uint64_t operation_serial
);
SudekiMpEconomyResult SudekiMpEconomyCoordinatorResolve(
    SudekiMpEconomyCoordinator *coordinator,
    uint64_t operation_serial,
    const SudekiMpEconomyObservation *observation
);
SudekiMpEconomyResult SudekiMpEconomyCoordinatorCancel(
    SudekiMpEconomyCoordinator *coordinator,
    uint64_t operation_serial
);
void SudekiMpEconomyCoordinatorBeginLoad(
    SudekiMpEconomyCoordinator *coordinator
);
int SudekiMpEconomyCoordinatorExport(
    const SudekiMpEconomyCoordinator *coordinator,
    SudekiMpEconomyCoordinatorSnapshot *snapshot
);
int SudekiMpEconomyCoordinatorRestore(
    SudekiMpEconomyCoordinator *coordinator,
    const SudekiMpEconomyCoordinatorSnapshot *snapshot,
    const uint8_t expected_native_save_identity[32]
);

#endif
