#include "engine/shared_inventory_entitlement_ledger.h"

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

static SudekiMpEntitlementResult quarantine(
    SudekiMpSharedInventoryEntitlementLedger *ledger
) {
    ledger->state = SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED;
    return SUDEKIMP_ENTITLEMENT_ENTERED_QUARANTINE;
}

static int entry_total(
    const SudekiMpEntitlementEntry *entry,
    uint32_t *total
) {
    uint64_t sum = entry->unallocated_quantity;
    uint32_t index;

    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        sum += entry->owner_quantity[index];
    }
    if (sum > UINT32_MAX) {
        return 0;
    }
    if (total != NULL) {
        *total = (uint32_t)sum;
    }
    return 1;
}

static SudekiMpEntitlementEntry *find_entry(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id
) {
    uint32_t index;

    for (index = 0u; index < ledger->entry_count; ++index) {
        if (ledger->entries[index].active &&
            ledger->entries[index].item_id == item_id) {
            return &ledger->entries[index];
        }
    }
    return NULL;
}

static const SudekiMpEntitlementEntry *find_entry_const(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id
) {
    return find_entry((SudekiMpSharedInventoryEntitlementLedger *)ledger,
        item_id);
}

static int ready_expected(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation
) {
    return ledger != NULL &&
        ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_READY &&
        expected_generation != 0u && expected_generation == ledger->generation;
}

static int owner_index(SudekiMpWalletCharacterId owner, uint32_t *index) {
    return SudekiMpPersonalWalletCharacterIndex(owner, index);
}

void SudekiMpSharedInventoryEntitlementLedgerInitialize(
    SudekiMpSharedInventoryEntitlementLedger *ledger
) {
    if (ledger == NULL) {
        return;
    }
    memset(ledger, 0, sizeof(*ledger));
    ledger->state = SUDEKIMP_ENTITLEMENT_LEDGER_READY;
    ledger->generation = 1u;
}

SudekiMpEntitlementResult SudekiMpEntitlementLedgerRegisterEligible(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    uint32_t native_quantity
) {
    SudekiMpEntitlementEntry *entry;

    if (ledger == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INVALID;
    }
    if (ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED;
    }
    entry = find_entry(ledger, item_id);
    if (entry != NULL) {
        uint32_t current_total;
        if (!entry_total(entry, &current_total) || current_total != native_quantity) {
            return quarantine(ledger);
        }
        return SUDEKIMP_ENTITLEMENT_NO_CHANGE;
    }
    if (ledger->entry_count >= SUDEKIMP_ENTITLEMENT_LEDGER_MAX_ITEMS) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_CAPACITY;
    }
    entry = &ledger->entries[ledger->entry_count++];
    memset(entry, 0, sizeof(*entry));
    entry->active = 1;
    entry->item_id = item_id;
    entry->unallocated_quantity = native_quantity;
    ledger->generation = advance_nonzero(ledger->generation);
    return SUDEKIMP_ENTITLEMENT_REGISTERED;
}

SudekiMpEntitlementResult SudekiMpEntitlementLedgerVerifyNativeQuantity(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    uint32_t native_quantity
) {
    SudekiMpEntitlementEntry *entry;
    uint32_t total;

    if (ledger == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INVALID;
    }
    if (ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED;
    }
    entry = find_entry(ledger, item_id);
    if (entry == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INELIGIBLE;
    }
    if (!entry_total(entry, &total) || total != native_quantity) {
        return quarantine(ledger);
    }
    return SUDEKIMP_ENTITLEMENT_NO_CHANGE;
}

SudekiMpEntitlementResult SudekiMpEntitlementLedgerClaimUnallocated(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t quantity
) {
    SudekiMpEntitlementEntry *entry;
    uint32_t owner_slot;

    if (ledger == NULL || quantity == 0u || !owner_index(owner, &owner_slot)) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INVALID;
    }
    if (!ready_expected(ledger, expected_generation)) {
        return ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED ?
            SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED :
            SUDEKIMP_ENTITLEMENT_REJECTED_STALE;
    }
    entry = find_entry(ledger, item_id);
    if (entry == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INELIGIBLE;
    }
    if (entry->unallocated_quantity < quantity) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_OWNERSHIP;
    }
    if (UINT32_MAX - entry->owner_quantity[owner_slot] < quantity) {
        return quarantine(ledger);
    }
    entry->unallocated_quantity -= quantity;
    entry->owner_quantity[owner_slot] += quantity;
    ledger->generation = advance_nonzero(ledger->generation);
    return SUDEKIMP_ENTITLEMENT_APPLIED;
}

SudekiMpEntitlementResult SudekiMpEntitlementLedgerApplyNativeAdd(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t quantity,
    uint32_t native_before,
    uint32_t native_after
) {
    SudekiMpEntitlementEntry *entry;
    uint32_t owner_slot;
    uint32_t total;

    if (ledger == NULL || quantity == 0u ||
        native_before > UINT32_MAX - quantity ||
        native_after != native_before + quantity ||
        !owner_index(owner, &owner_slot)) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INVALID;
    }
    if (!ready_expected(ledger, expected_generation)) {
        return ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED ?
            SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED :
            SUDEKIMP_ENTITLEMENT_REJECTED_STALE;
    }
    entry = find_entry(ledger, item_id);
    if (entry == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INELIGIBLE;
    }
    if (!entry_total(entry, &total) || total != native_before ||
        UINT32_MAX - entry->owner_quantity[owner_slot] < quantity) {
        return quarantine(ledger);
    }
    entry->owner_quantity[owner_slot] += quantity;
    ledger->generation = advance_nonzero(ledger->generation);
    return SUDEKIMP_ENTITLEMENT_APPLIED;
}

SudekiMpEntitlementResult SudekiMpEntitlementLedgerApplyNativeRemove(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t quantity,
    uint32_t native_before,
    uint32_t native_after
) {
    SudekiMpEntitlementEntry *entry;
    uint32_t owner_slot;
    uint32_t total;

    if (ledger == NULL || quantity == 0u || native_before < quantity ||
        native_after != native_before - quantity ||
        !owner_index(owner, &owner_slot)) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INVALID;
    }
    if (!ready_expected(ledger, expected_generation)) {
        return ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED ?
            SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED :
            SUDEKIMP_ENTITLEMENT_REJECTED_STALE;
    }
    entry = find_entry(ledger, item_id);
    if (entry == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INELIGIBLE;
    }
    if (!entry_total(entry, &total) || total != native_before ||
        entry->owner_quantity[owner_slot] < quantity) {
        return quarantine(ledger);
    }
    entry->owner_quantity[owner_slot] -= quantity;
    ledger->generation = advance_nonzero(ledger->generation);
    return SUDEKIMP_ENTITLEMENT_APPLIED;
}

SudekiMpEntitlementResult SudekiMpEntitlementLedgerTransfer(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t expected_generation,
    uint32_t item_id,
    SudekiMpWalletCharacterId from,
    SudekiMpWalletCharacterId to,
    uint32_t quantity
) {
    SudekiMpEntitlementEntry *entry;
    uint32_t from_slot;
    uint32_t to_slot;

    if (ledger == NULL || quantity == 0u || !owner_index(from, &from_slot) ||
        !owner_index(to, &to_slot)) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INVALID;
    }
    if (!ready_expected(ledger, expected_generation)) {
        return ledger->state == SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED ?
            SUDEKIMP_ENTITLEMENT_REJECTED_QUARANTINED :
            SUDEKIMP_ENTITLEMENT_REJECTED_STALE;
    }
    entry = find_entry(ledger, item_id);
    if (entry == NULL) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_INELIGIBLE;
    }
    if (from_slot == to_slot) {
        return SUDEKIMP_ENTITLEMENT_NO_CHANGE;
    }
    if (entry->owner_quantity[from_slot] < quantity) {
        return SUDEKIMP_ENTITLEMENT_REJECTED_OWNERSHIP;
    }
    if (UINT32_MAX - entry->owner_quantity[to_slot] < quantity) {
        return quarantine(ledger);
    }
    entry->owner_quantity[from_slot] -= quantity;
    entry->owner_quantity[to_slot] += quantity;
    ledger->generation = advance_nonzero(ledger->generation);
    return SUDEKIMP_ENTITLEMENT_APPLIED;
}

int SudekiMpEntitlementLedgerGetQuantity(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    uint32_t item_id,
    SudekiMpWalletCharacterId owner,
    uint32_t *quantity
) {
    const SudekiMpEntitlementEntry *entry;
    uint32_t owner_slot;

    if (ledger == NULL || quantity == NULL ||
        !owner_index(owner, &owner_slot)) {
        return 0;
    }
    entry = find_entry_const(ledger, item_id);
    if (entry == NULL) {
        return 0;
    }
    *quantity = entry->owner_quantity[owner_slot];
    return 1;
}

static int snapshot_valid(const SudekiMpEntitlementLedgerSnapshot *snapshot) {
    uint32_t index;
    uint32_t prior;

    if (snapshot == NULL ||
        snapshot->schema_version != SUDEKIMP_ENTITLEMENT_LEDGER_SCHEMA_VERSION ||
        snapshot->generation == 0u ||
        snapshot->entry_count > SUDEKIMP_ENTITLEMENT_LEDGER_MAX_ITEMS) {
        return 0;
    }
    for (index = 0u; index < snapshot->entry_count; ++index) {
        uint32_t total;
        if (!snapshot->entries[index].active ||
            !entry_total(&snapshot->entries[index], &total)) {
            return 0;
        }
        for (prior = 0u; prior < index; ++prior) {
            if (snapshot->entries[prior].item_id == snapshot->entries[index].item_id) {
                return 0;
            }
        }
    }
    return 1;
}

int SudekiMpEntitlementLedgerExport(
    const SudekiMpSharedInventoryEntitlementLedger *ledger,
    SudekiMpEntitlementLedgerSnapshot *snapshot
) {
    if (ledger == NULL || snapshot == NULL ||
        ledger->state != SUDEKIMP_ENTITLEMENT_LEDGER_READY ||
        ledger->generation == 0u) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->schema_version = SUDEKIMP_ENTITLEMENT_LEDGER_SCHEMA_VERSION;
    snapshot->generation = ledger->generation;
    snapshot->entry_count = ledger->entry_count;
    memcpy(snapshot->entries, ledger->entries, sizeof(snapshot->entries));
    return 1;
}

static int restore_snapshot(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    const SudekiMpEntitlementLedgerSnapshot *snapshot
) {
    if (!snapshot_valid(snapshot)) {
        ledger->state = SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED;
        return 0;
    }
    SudekiMpSharedInventoryEntitlementLedgerInitialize(ledger);
    ledger->entry_count = snapshot->entry_count;
    memcpy(ledger->entries, snapshot->entries, sizeof(ledger->entries));
    ledger->generation = advance_nonzero(snapshot->generation);
    return 1;
}

int SudekiMpEntitlementLedgerRestore(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    const SudekiMpEntitlementLedgerSnapshot *snapshot
) {
    if (ledger == NULL || ledger->state != SUDEKIMP_ENTITLEMENT_LEDGER_READY) {
        return 0;
    }
    return restore_snapshot(ledger, snapshot);
}

int SudekiMpEntitlementLedgerRecover(
    SudekiMpSharedInventoryEntitlementLedger *ledger,
    const SudekiMpEntitlementLedgerSnapshot *trusted_snapshot
) {
    if (ledger == NULL || ledger->state != SUDEKIMP_ENTITLEMENT_LEDGER_QUARANTINED) {
        return 0;
    }
    return restore_snapshot(ledger, trusted_snapshot);
}
