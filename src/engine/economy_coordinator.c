#include "engine/economy_coordinator.h"

#include <stddef.h>
#include <string.h>

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) {
        ++value;
    }
    return value;
}

static void clear_pending(SudekiMpEconomyCoordinator *coordinator) {
    memset(&coordinator->pending, 0, sizeof(coordinator->pending));
    coordinator->pending_uses_wallet = 0;
}

static SudekiMpEconomyResult quarantine(SudekiMpEconomyCoordinator *coordinator) {
    coordinator->state = SUDEKIMP_ECONOMY_QUARANTINED;
    return SUDEKIMP_ECONOMY_ENTERED_QUARANTINE;
}

static int character_valid(SudekiMpWalletCharacterId character) {
    return SudekiMpPersonalWalletCharacterIndex(character, NULL);
}

static int request_equal(
    const SudekiMpEconomyRequest *left,
    const SudekiMpEconomyRequest *right
) {
    return left->operation_serial == right->operation_serial &&
        left->expected_coordinator_generation ==
            right->expected_coordinator_generation &&
        left->expected_wallet_generation == right->expected_wallet_generation &&
        left->expected_inventory_generation ==
            right->expected_inventory_generation &&
        left->source_provenance == right->source_provenance &&
        left->merchant_provenance == right->merchant_provenance &&
        left->kind == right->kind && left->initiator == right->initiator &&
        left->recipient == right->recipient && left->item_id == right->item_id &&
        left->quantity == right->quantity && left->amount == right->amount;
}

static int uses_wallet(SudekiMpEconomyOperationKind kind) {
    return kind == SUDEKIMP_ECONOMY_OPERATION_LITERAL_FLORIN ||
        kind == SUDEKIMP_ECONOMY_OPERATION_QUEST_DIVIDEND ||
        kind == SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM ||
        kind == SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM ||
        kind == SUDEKIMP_ECONOMY_OPERATION_FORGE ||
        kind == SUDEKIMP_ECONOMY_OPERATION_DISTRIBUTE_RESERVE;
}

static int uses_entitlement(SudekiMpEconomyOperationKind kind) {
    return kind == SUDEKIMP_ECONOMY_OPERATION_PICKUP_ELIGIBLE_ITEM ||
        kind == SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM ||
        kind == SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM ||
        kind == SUDEKIMP_ECONOMY_OPERATION_TRANSFER_ENTITLEMENT;
}

static int request_valid(const SudekiMpEconomyRequest *request) {
    if (request == NULL || request->operation_serial == 0u ||
        request->expected_coordinator_generation == 0u ||
        request->expected_wallet_generation == 0u ||
        !character_valid(request->initiator)) {
        return 0;
    }
    switch (request->kind) {
    case SUDEKIMP_ECONOMY_OPERATION_LITERAL_FLORIN:
        return request->amount != 0u && request->source_provenance != 0u &&
            request->expected_inventory_generation != 0u;
    case SUDEKIMP_ECONOMY_OPERATION_QUEST_DIVIDEND:
        return request->amount != 0u && request->source_provenance != 0u &&
            request->expected_inventory_generation == 0u;
    case SUDEKIMP_ECONOMY_OPERATION_PICKUP_ELIGIBLE_ITEM:
        return request->quantity != 0u && request->source_provenance != 0u &&
            request->expected_inventory_generation != 0u;
    case SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM:
    case SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM:
        return request->quantity != 0u && request->amount != 0u &&
            request->merchant_provenance != 0u &&
            request->expected_inventory_generation != 0u;
    case SUDEKIMP_ECONOMY_OPERATION_FORGE:
        return request->amount != 0u && request->merchant_provenance != 0u &&
            request->expected_inventory_generation != 0u;
    case SUDEKIMP_ECONOMY_OPERATION_TRANSFER_ENTITLEMENT:
        return request->quantity != 0u && character_valid(request->recipient) &&
            request->expected_inventory_generation != 0u;
    case SUDEKIMP_ECONOMY_OPERATION_DISTRIBUTE_RESERVE:
        return request->amount != 0u && character_valid(request->recipient) &&
            request->expected_inventory_generation == 0u;
    default:
        return 0;
    }
}

static int make_wallet_request(
    const SudekiMpEconomyRequest *request,
    SudekiMpWalletRequest *wallet_request
) {
    memset(wallet_request, 0, sizeof(*wallet_request));
    wallet_request->operation_serial = request->operation_serial;
    wallet_request->expected_wallet_generation = request->expected_wallet_generation;
    wallet_request->character_id = request->initiator;
    wallet_request->amount = request->amount;
    switch (request->kind) {
    case SUDEKIMP_ECONOMY_OPERATION_LITERAL_FLORIN:
        wallet_request->kind = SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD;
        wallet_request->expected_external_generation =
            request->expected_inventory_generation;
        wallet_request->subject_known = 1;
        return 1;
    case SUDEKIMP_ECONOMY_OPERATION_QUEST_DIVIDEND:
        wallet_request->kind = SUDEKIMP_WALLET_TRANSACTION_QUEST_DIVIDEND;
        wallet_request->character_id = SUDEKIMP_WALLET_CHARACTER_INVALID;
        return 1;
    case SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM:
        wallet_request->kind = SUDEKIMP_WALLET_TRANSACTION_PURCHASE;
        wallet_request->expected_external_generation =
            request->expected_inventory_generation;
        wallet_request->subject_known = 1;
        wallet_request->subject_id = request->item_id;
        wallet_request->subject_quantity = request->quantity;
        return 1;
    case SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM:
        wallet_request->kind = SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND;
        wallet_request->expected_external_generation =
            request->expected_inventory_generation;
        wallet_request->subject_known = 1;
        wallet_request->subject_id = request->item_id;
        wallet_request->subject_quantity = request->quantity;
        return 1;
    case SUDEKIMP_ECONOMY_OPERATION_FORGE:
        wallet_request->kind = SUDEKIMP_WALLET_TRANSACTION_FORGE;
        wallet_request->expected_external_generation =
            request->expected_inventory_generation;
        wallet_request->subject_known = 1;
        return 1;
    case SUDEKIMP_ECONOMY_OPERATION_DISTRIBUTE_RESERVE:
        wallet_request->kind = SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION;
        wallet_request->character_id = request->recipient;
        return 1;
    default:
        return 0;
    }
}

static SudekiMpEconomyResult map_wallet_result(SudekiMpWalletResult result) {
    switch (result) {
    case SUDEKIMP_WALLET_PLAN_CREATED:
        return SUDEKIMP_ECONOMY_PLAN_CREATED;
    case SUDEKIMP_WALLET_PLAN_REPLAYED:
        return SUDEKIMP_ECONOMY_PLAN_REPLAYED;
    case SUDEKIMP_WALLET_APPLICATION_BEGUN:
        return SUDEKIMP_ECONOMY_APPLICATION_BEGUN;
    case SUDEKIMP_WALLET_APPLIED:
        return SUDEKIMP_ECONOMY_APPLIED;
    case SUDEKIMP_WALLET_ALREADY_APPLIED:
        return SUDEKIMP_ECONOMY_ALREADY_APPLIED;
    case SUDEKIMP_WALLET_CANCELLED:
        return SUDEKIMP_ECONOMY_CANCELLED;
    case SUDEKIMP_WALLET_REJECTED_FUNDS:
        return SUDEKIMP_ECONOMY_REJECTED_FUNDS;
    case SUDEKIMP_WALLET_REJECTED_STALE:
        return SUDEKIMP_ECONOMY_REJECTED_STALE;
    case SUDEKIMP_WALLET_REJECTED_QUARANTINED:
        return SUDEKIMP_ECONOMY_REJECTED_QUARANTINED;
    case SUDEKIMP_WALLET_ENTERED_QUARANTINE:
        return SUDEKIMP_ECONOMY_ENTERED_QUARANTINE;
    default:
        return SUDEKIMP_ECONOMY_REJECTED_INVALID;
    }
}

void SudekiMpEconomyCoordinatorInitialize(
    SudekiMpEconomyCoordinator *coordinator,
    const uint8_t native_save_identity[32]
) {
    if (coordinator == NULL || native_save_identity == NULL) {
        return;
    }
    memset(coordinator, 0, sizeof(*coordinator));
    coordinator->state = SUDEKIMP_ECONOMY_READY;
    coordinator->generation = 1u;
    memcpy(coordinator->native_save_identity, native_save_identity, 32u);
    SudekiMpPersonalWalletInitialize(&coordinator->wallet);
    SudekiMpSharedInventoryEntitlementLedgerInitialize(&coordinator->entitlements);
}

SudekiMpEconomyResult SudekiMpEconomyCoordinatorPlan(
    SudekiMpEconomyCoordinator *coordinator,
    const SudekiMpEconomyRequest *request
) {
    SudekiMpWalletRequest wallet_request;
    SudekiMpWalletResult wallet_result;

    if (coordinator == NULL || !request_valid(request)) {
        return SUDEKIMP_ECONOMY_REJECTED_INVALID;
    }
    if (coordinator->state == SUDEKIMP_ECONOMY_QUARANTINED) {
        return SUDEKIMP_ECONOMY_REJECTED_QUARANTINED;
    }
    if (coordinator->state == SUDEKIMP_ECONOMY_PLANNED ||
        coordinator->state == SUDEKIMP_ECONOMY_APPLYING) {
        if (coordinator->pending.operation_serial == request->operation_serial &&
            request_equal(&coordinator->pending, request)) {
            return SUDEKIMP_ECONOMY_PLAN_REPLAYED;
        }
        return coordinator->pending.operation_serial == request->operation_serial ?
            quarantine(coordinator) : SUDEKIMP_ECONOMY_REJECTED_BUSY;
    }
    if (request->expected_coordinator_generation != coordinator->generation ||
        request->expected_wallet_generation != coordinator->wallet.generation ||
        request->operation_serial <= coordinator->highest_operation_serial ||
        (uses_entitlement(request->kind) &&
         request->expected_inventory_generation != coordinator->entitlements.generation)) {
        return SUDEKIMP_ECONOMY_REJECTED_STALE;
    }
    if (uses_wallet(request->kind)) {
        if (!make_wallet_request(request, &wallet_request)) {
            return SUDEKIMP_ECONOMY_REJECTED_INVALID;
        }
        wallet_result = SudekiMpPersonalWalletPlanTransaction(
            &coordinator->wallet, &wallet_request, NULL);
        if (wallet_result != SUDEKIMP_WALLET_PLAN_CREATED) {
            return map_wallet_result(wallet_result);
        }
    }
    coordinator->pending = *request;
    coordinator->pending_uses_wallet = uses_wallet(request->kind);
    coordinator->highest_operation_serial = request->operation_serial;
    coordinator->state = SUDEKIMP_ECONOMY_PLANNED;
    return SUDEKIMP_ECONOMY_PLAN_CREATED;
}

SudekiMpEconomyResult SudekiMpEconomyCoordinatorBegin(
    SudekiMpEconomyCoordinator *coordinator,
    uint64_t operation_serial
) {
    SudekiMpWalletResult wallet_result;

    if (coordinator == NULL || operation_serial == 0u) {
        return SUDEKIMP_ECONOMY_REJECTED_INVALID;
    }
    if (coordinator->state == SUDEKIMP_ECONOMY_QUARANTINED) {
        return SUDEKIMP_ECONOMY_REJECTED_QUARANTINED;
    }
    if (coordinator->state != SUDEKIMP_ECONOMY_PLANNED ||
        coordinator->pending.operation_serial != operation_serial) {
        return SUDEKIMP_ECONOMY_REJECTED_BUSY;
    }
    if (coordinator->pending_uses_wallet) {
        wallet_result = SudekiMpPersonalWalletBeginApplication(
            &coordinator->wallet, operation_serial,
            coordinator->pending.expected_wallet_generation);
        if (wallet_result != SUDEKIMP_WALLET_APPLICATION_BEGUN) {
            return map_wallet_result(wallet_result);
        }
    }
    coordinator->state = SUDEKIMP_ECONOMY_APPLYING;
    return SUDEKIMP_ECONOMY_APPLICATION_BEGUN;
}

static SudekiMpEconomyResult apply_entitlement(
    SudekiMpEconomyCoordinator *coordinator,
    const SudekiMpEconomyObservation *observation
) {
    const SudekiMpEconomyRequest *request = &coordinator->pending;
    SudekiMpEntitlementResult result;

    switch (request->kind) {
    case SUDEKIMP_ECONOMY_OPERATION_PICKUP_ELIGIBLE_ITEM:
    case SUDEKIMP_ECONOMY_OPERATION_PURCHASE_ELIGIBLE_ITEM:
        result = SudekiMpEntitlementLedgerApplyNativeAdd(
            &coordinator->entitlements, request->expected_inventory_generation,
            request->item_id, request->initiator, request->quantity,
            observation->native_quantity_before, observation->native_quantity_after);
        break;
    case SUDEKIMP_ECONOMY_OPERATION_SALE_ELIGIBLE_ITEM:
        result = SudekiMpEntitlementLedgerApplyNativeRemove(
            &coordinator->entitlements, request->expected_inventory_generation,
            request->item_id, request->initiator, request->quantity,
            observation->native_quantity_before, observation->native_quantity_after);
        break;
    case SUDEKIMP_ECONOMY_OPERATION_TRANSFER_ENTITLEMENT:
        result = SudekiMpEntitlementLedgerTransfer(
            &coordinator->entitlements, request->expected_inventory_generation,
            request->item_id, request->initiator, request->recipient,
            request->quantity);
        break;
    default:
        return SUDEKIMP_ECONOMY_NO_CHANGE;
    }
    return result == SUDEKIMP_ENTITLEMENT_APPLIED ||
        result == SUDEKIMP_ENTITLEMENT_NO_CHANGE ? SUDEKIMP_ECONOMY_APPLIED :
        (result == SUDEKIMP_ENTITLEMENT_REJECTED_OWNERSHIP ?
            SUDEKIMP_ECONOMY_REJECTED_OWNERSHIP :
            SUDEKIMP_ECONOMY_ENTERED_QUARANTINE);
}

SudekiMpEconomyResult SudekiMpEconomyCoordinatorResolve(
    SudekiMpEconomyCoordinator *coordinator,
    uint64_t operation_serial,
    const SudekiMpEconomyObservation *observation
) {
    SudekiMpWalletResult wallet_result;
    SudekiMpEconomyResult entitlement_result;

    if (coordinator == NULL || observation == NULL || operation_serial == 0u) {
        return SUDEKIMP_ECONOMY_REJECTED_INVALID;
    }
    if (coordinator->last_serial_valid && coordinator->last_serial == operation_serial) {
        return SUDEKIMP_ECONOMY_ALREADY_APPLIED;
    }
    if (coordinator->state == SUDEKIMP_ECONOMY_QUARANTINED) {
        return SUDEKIMP_ECONOMY_REJECTED_QUARANTINED;
    }
    if (coordinator->state != SUDEKIMP_ECONOMY_APPLYING ||
        coordinator->pending.operation_serial != operation_serial) {
        return SUDEKIMP_ECONOMY_REJECTED_BUSY;
    }
    if (observation->outcome == SUDEKIMP_ECONOMY_EXTERNAL_AMBIGUOUS) {
        return quarantine(coordinator);
    }
    if (observation->outcome == SUDEKIMP_ECONOMY_EXTERNAL_NOT_APPLIED) {
        if (coordinator->pending_uses_wallet) {
            wallet_result = SudekiMpPersonalWalletResolveApplication(
                &coordinator->wallet, operation_serial,
                SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED,
                coordinator->pending.expected_inventory_generation, NULL);
            if (wallet_result != SUDEKIMP_WALLET_CANCELLED) {
                return quarantine(coordinator);
            }
        }
        clear_pending(coordinator);
        coordinator->state = SUDEKIMP_ECONOMY_READY;
        return SUDEKIMP_ECONOMY_CANCELLED;
    }
    if (uses_entitlement(coordinator->pending.kind)) {
        if (coordinator->pending.kind != SUDEKIMP_ECONOMY_OPERATION_TRANSFER_ENTITLEMENT &&
            (observation->observed_inventory_generation == 0u ||
             observation->observed_inventory_generation ==
                 coordinator->pending.expected_inventory_generation)) {
            return quarantine(coordinator);
        }
        entitlement_result = apply_entitlement(coordinator, observation);
        if (entitlement_result != SUDEKIMP_ECONOMY_APPLIED &&
            entitlement_result != SUDEKIMP_ECONOMY_NO_CHANGE) {
            return quarantine(coordinator);
        }
    }
    if (coordinator->pending_uses_wallet) {
        uint32_t observed_generation = coordinator->pending.kind ==
            SUDEKIMP_ECONOMY_OPERATION_LITERAL_FLORIN ?
            observation->observed_wallet_source_generation :
            observation->observed_inventory_generation;
        if (coordinator->pending.kind == SUDEKIMP_ECONOMY_OPERATION_QUEST_DIVIDEND ||
            coordinator->pending.kind == SUDEKIMP_ECONOMY_OPERATION_DISTRIBUTE_RESERVE) {
            observed_generation = 0u;
        }
        wallet_result = SudekiMpPersonalWalletResolveApplication(
            &coordinator->wallet, operation_serial,
            SUDEKIMP_WALLET_EXTERNAL_VERIFIED, observed_generation, NULL);
        if (wallet_result != SUDEKIMP_WALLET_APPLIED) {
            return quarantine(coordinator);
        }
    }
    coordinator->generation = advance_nonzero(coordinator->generation);
    coordinator->last_serial_valid = 1;
    coordinator->last_serial = operation_serial;
    clear_pending(coordinator);
    coordinator->state = SUDEKIMP_ECONOMY_READY;
    return SUDEKIMP_ECONOMY_APPLIED;
}

SudekiMpEconomyResult SudekiMpEconomyCoordinatorCancel(
    SudekiMpEconomyCoordinator *coordinator,
    uint64_t operation_serial
) {
    SudekiMpWalletResult wallet_result;

    if (coordinator == NULL || operation_serial == 0u) {
        return SUDEKIMP_ECONOMY_REJECTED_INVALID;
    }
    if (coordinator->state == SUDEKIMP_ECONOMY_QUARANTINED) {
        return SUDEKIMP_ECONOMY_REJECTED_QUARANTINED;
    }
    if (coordinator->state != SUDEKIMP_ECONOMY_PLANNED ||
        coordinator->pending.operation_serial != operation_serial) {
        return SUDEKIMP_ECONOMY_REJECTED_BUSY;
    }
    if (coordinator->pending_uses_wallet) {
        wallet_result = SudekiMpPersonalWalletCancelPlan(&coordinator->wallet,
            operation_serial);
        if (wallet_result != SUDEKIMP_WALLET_CANCELLED) {
            return quarantine(coordinator);
        }
    }
    clear_pending(coordinator);
    coordinator->state = SUDEKIMP_ECONOMY_READY;
    return SUDEKIMP_ECONOMY_CANCELLED;
}

void SudekiMpEconomyCoordinatorBeginLoad(
    SudekiMpEconomyCoordinator *coordinator
) {
    if (coordinator == NULL) {
        return;
    }
    if (coordinator->state == SUDEKIMP_ECONOMY_PLANNED ||
        coordinator->state == SUDEKIMP_ECONOMY_APPLYING) {
        (void)quarantine(coordinator);
    }
}

int SudekiMpEconomyCoordinatorExport(
    const SudekiMpEconomyCoordinator *coordinator,
    SudekiMpEconomyCoordinatorSnapshot *snapshot
) {
    if (coordinator == NULL || snapshot == NULL ||
        coordinator->state != SUDEKIMP_ECONOMY_READY ||
        coordinator->generation == 0u) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->schema_version = SUDEKIMP_ECONOMY_COORDINATOR_SCHEMA_VERSION;
    snapshot->generation = coordinator->generation;
    snapshot->highest_operation_serial = coordinator->highest_operation_serial;
    memcpy(snapshot->native_save_identity, coordinator->native_save_identity, 32u);
    /* Export after the clear so the embedded snapshots are not erased. */
    return SudekiMpPersonalWalletExport(&coordinator->wallet, &snapshot->wallet) &&
        SudekiMpEntitlementLedgerExport(&coordinator->entitlements,
            &snapshot->entitlements);
}

int SudekiMpEconomyCoordinatorRestore(
    SudekiMpEconomyCoordinator *coordinator,
    const SudekiMpEconomyCoordinatorSnapshot *snapshot,
    const uint8_t expected_native_save_identity[32]
) {
    if (coordinator == NULL || snapshot == NULL ||
        expected_native_save_identity == NULL ||
        snapshot->schema_version != SUDEKIMP_ECONOMY_COORDINATOR_SCHEMA_VERSION ||
        snapshot->generation == 0u ||
        memcmp(snapshot->native_save_identity, expected_native_save_identity, 32u) != 0 ||
        coordinator->state != SUDEKIMP_ECONOMY_READY) {
        if (coordinator != NULL) {
            (void)quarantine(coordinator);
        }
        return 0;
    }
    if (!SudekiMpPersonalWalletRestore(&coordinator->wallet, &snapshot->wallet) ||
        !SudekiMpEntitlementLedgerRestore(&coordinator->entitlements,
            &snapshot->entitlements)) {
        (void)quarantine(coordinator);
        return 0;
    }
    coordinator->generation = advance_nonzero(snapshot->generation);
    coordinator->highest_operation_serial = snapshot->highest_operation_serial;
    memcpy(coordinator->native_save_identity, expected_native_save_identity, 32u);
    return 1;
}
