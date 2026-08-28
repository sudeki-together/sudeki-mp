#include "engine/personal_wallet.h"

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

static int request_equal(
    const SudekiMpWalletRequest *left,
    const SudekiMpWalletRequest *right
) {
    return left->operation_serial == right->operation_serial &&
        left->expected_wallet_generation ==
            right->expected_wallet_generation &&
        left->expected_external_generation ==
            right->expected_external_generation &&
        left->kind == right->kind &&
        left->character_id == right->character_id &&
        left->amount == right->amount &&
        left->subject_id == right->subject_id &&
        left->subject_quantity == right->subject_quantity &&
        (left->subject_known != 0) == (right->subject_known != 0);
}

static void clear_pending(SudekiMpPersonalWallet *wallet) {
    memset(&wallet->pending, 0, sizeof(wallet->pending));
}

static SudekiMpWalletResult quarantine(SudekiMpPersonalWallet *wallet) {
    wallet->state = SUDEKIMP_WALLET_QUARANTINED;
    return SUDEKIMP_WALLET_ENTERED_QUARANTINE;
}

int SudekiMpPersonalWalletCharacterIndex(
    SudekiMpWalletCharacterId character_id,
    uint32_t *index
) {
    uint32_t resolved;

    switch (character_id) {
    case SUDEKIMP_WALLET_CHARACTER_TAL:
        resolved = 0u;
        break;
    case SUDEKIMP_WALLET_CHARACTER_AILISH:
        resolved = 1u;
        break;
    case SUDEKIMP_WALLET_CHARACTER_BUKI:
        resolved = 2u;
        break;
    case SUDEKIMP_WALLET_CHARACTER_ELCO:
        resolved = 3u;
        break;
    case SUDEKIMP_WALLET_CHARACTER_INVALID:
    default:
        return 0;
    }
    if (index != NULL) {
        *index = resolved;
    }
    return 1;
}

SudekiMpWalletCharacterId SudekiMpPersonalWalletCharacterIdAt(
    uint32_t index
) {
    static const SudekiMpWalletCharacterId ids[] = {
        SUDEKIMP_WALLET_CHARACTER_TAL,
        SUDEKIMP_WALLET_CHARACTER_AILISH,
        SUDEKIMP_WALLET_CHARACTER_BUKI,
        SUDEKIMP_WALLET_CHARACTER_ELCO
    };

    if (index >= SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT) {
        return SUDEKIMP_WALLET_CHARACTER_INVALID;
    }
    return ids[index];
}

SudekiMpWalletMoneyPolicy SudekiMpPersonalWalletMoneyPolicyForKind(
    SudekiMpWalletTransactionKind kind
) {
    switch (kind) {
    case SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD:
        return SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_CHARACTER;
    case SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD:
        return SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_RESERVE;
    case SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_QUEST_DIVIDEND:
        return SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_ALL_CHARACTERS;
    case SUDEKIMP_WALLET_TRANSACTION_PURCHASE:
    case SUDEKIMP_WALLET_TRANSACTION_FORGE:
        return SUDEKIMP_WALLET_MONEY_POLICY_DEBIT_CHARACTER;
    case SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND:
        return SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_ALL_CHARACTERS;
    case SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION:
        return SUDEKIMP_WALLET_MONEY_POLICY_TRANSFER_RESERVE_TO_CHARACTER;
    case SUDEKIMP_WALLET_TRANSACTION_NONE:
    default:
        return SUDEKIMP_WALLET_MONEY_POLICY_INVALID;
    }
}

SudekiMpWalletExternalEffect SudekiMpPersonalWalletExternalEffectForKind(
    SudekiMpWalletTransactionKind kind
) {
    switch (kind) {
    case SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD:
        return SUDEKIMP_WALLET_EXTERNAL_CONSUME_REWARD_SOURCE;
    case SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_QUEST_DIVIDEND:
    case SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION:
        return SUDEKIMP_WALLET_EXTERNAL_NONE;
    case SUDEKIMP_WALLET_TRANSACTION_PURCHASE:
        return SUDEKIMP_WALLET_EXTERNAL_ADD_SHARED_ITEM;
    case SUDEKIMP_WALLET_TRANSACTION_FORGE:
        return SUDEKIMP_WALLET_EXTERNAL_MUTATE_SHARED_ITEM;
    case SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND:
        return SUDEKIMP_WALLET_EXTERNAL_REMOVE_SHARED_ITEM_QUANTITY;
    case SUDEKIMP_WALLET_TRANSACTION_NONE:
    default:
        return SUDEKIMP_WALLET_EXTERNAL_NONE;
    }
}

void SudekiMpPersonalWalletInitialize(SudekiMpPersonalWallet *wallet) {
    if (wallet == NULL) {
        return;
    }
    memset(wallet, 0, sizeof(*wallet));
    wallet->state = SUDEKIMP_WALLET_READY;
    wallet->generation = 1u;
}

int SudekiMpPersonalWalletMigrateLegacy(
    SudekiMpPersonalWallet *wallet,
    uint64_t legacy_party_money,
    SudekiMpWalletMigrationReport *report
) {
    uint64_t migrated;
    uint32_t index;

    if (wallet == NULL) {
        return 0;
    }
    if (wallet->state != SUDEKIMP_WALLET_READY ||
        wallet->generation != 1u || wallet->party_reserve != 0u ||
        wallet->highest_operation_serial != 0u ||
        wallet->pending.request.operation_serial != 0u ||
        wallet->last_receipt_valid) {
        return 0;
    }
    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        if (wallet->character_balance[index] != 0u) {
            return 0;
        }
    }
    migrated = legacy_party_money;
    if (migrated > SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP) {
        migrated = SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP;
    }
    wallet->party_reserve = (uint32_t)migrated;
    wallet->generation = advance_nonzero(wallet->generation);
    if (report != NULL) {
        memset(report, 0, sizeof(*report));
        report->legacy_party_money = legacy_party_money;
        report->migrated_to_reserve = (uint32_t)migrated;
        report->discarded_overflow = legacy_party_money - migrated;
    }
    return 1;
}

static int decode_snapshot(
    const SudekiMpWalletPersistedSnapshot *snapshot,
    uint32_t balances[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT]
) {
    uint8_t seen = 0u;
    uint32_t entry_index;

    if (snapshot == NULL ||
        snapshot->schema_version !=
            SUDEKIMP_PERSONAL_WALLET_SCHEMA_VERSION ||
        snapshot->generation == 0u ||
        snapshot->party_reserve > SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP) {
        return 0;
    }
    memset(
        balances,
        0,
        sizeof(uint32_t) * SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT
    );
    for (entry_index = 0u;
         entry_index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++entry_index) {
        const SudekiMpWalletPersistedEntry *entry =
            &snapshot->characters[entry_index];
        uint32_t character_index;
        uint8_t bit;

        if (!SudekiMpPersonalWalletCharacterIndex(
                entry->character_id, &character_index) ||
            entry->balance > SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP) {
            return 0;
        }
        bit = (uint8_t)(1u << character_index);
        if ((seen & bit) != 0u) {
            return 0;
        }
        seen |= bit;
        balances[character_index] = entry->balance;
    }
    return seen == 0x0fu;
}

static int restore_snapshot(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletPersistedSnapshot *snapshot
) {
    uint32_t balances[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];

    if (wallet == NULL) {
        return 0;
    }
    if (!decode_snapshot(snapshot, balances)) {
        wallet->state = SUDEKIMP_WALLET_QUARANTINED;
        return 0;
    }
    SudekiMpPersonalWalletInitialize(wallet);
    memcpy(wallet->character_balance, balances, sizeof(balances));
    wallet->party_reserve = snapshot->party_reserve;
    wallet->highest_operation_serial =
        snapshot->highest_operation_serial;
    /* Loading a snapshot invalidates every runtime-only planned generation. */
    wallet->generation = advance_nonzero(snapshot->generation);
    return 1;
}

int SudekiMpPersonalWalletRestore(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletPersistedSnapshot *snapshot
) {
    if (wallet == NULL || wallet->state != SUDEKIMP_WALLET_READY) {
        return 0;
    }
    return restore_snapshot(wallet, snapshot);
}

int SudekiMpPersonalWalletRecover(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletPersistedSnapshot *trusted_snapshot
) {
    if (wallet == NULL || wallet->state != SUDEKIMP_WALLET_QUARANTINED) {
        return 0;
    }
    return restore_snapshot(wallet, trusted_snapshot);
}

int SudekiMpPersonalWalletExport(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletPersistedSnapshot *snapshot
) {
    uint32_t index;

    if (wallet == NULL || snapshot == NULL ||
        wallet->state != SUDEKIMP_WALLET_READY ||
        wallet->generation == 0u) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->schema_version = SUDEKIMP_PERSONAL_WALLET_SCHEMA_VERSION;
    snapshot->generation = wallet->generation;
    snapshot->party_reserve = wallet->party_reserve;
    snapshot->highest_operation_serial =
        wallet->highest_operation_serial;
    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        snapshot->characters[index].character_id =
            SudekiMpPersonalWalletCharacterIdAt(index);
        snapshot->characters[index].balance =
            wallet->character_balance[index];
    }
    return 1;
}

static int valid_request_shape(const SudekiMpWalletRequest *request) {
    SudekiMpWalletMoneyPolicy policy;
    SudekiMpWalletExternalEffect effect;
    int needs_character;

    if (request == NULL || request->operation_serial == 0u ||
        request->expected_wallet_generation == 0u ||
        request->amount == 0u) {
        return 0;
    }
    policy = SudekiMpPersonalWalletMoneyPolicyForKind(request->kind);
    effect = SudekiMpPersonalWalletExternalEffectForKind(request->kind);
    if (policy == SUDEKIMP_WALLET_MONEY_POLICY_INVALID) {
        return 0;
    }
    switch (request->kind) {
    case SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_PURCHASE:
    case SUDEKIMP_WALLET_TRANSACTION_FORGE:
    case SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND:
    case SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION:
        needs_character = 1;
        break;
    case SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_QUEST_DIVIDEND:
        needs_character = 0;
        break;
    case SUDEKIMP_WALLET_TRANSACTION_NONE:
    default:
        return 0;
    }
    if (needs_character !=
        SudekiMpPersonalWalletCharacterIndex(request->character_id, NULL)) {
        return 0;
    }
    if (effect == SUDEKIMP_WALLET_EXTERNAL_NONE) {
        return request->expected_external_generation == 0u &&
            !request->subject_known && request->subject_quantity == 0u;
    }
    if (request->expected_external_generation == 0u ||
        !request->subject_known) {
        return 0;
    }
    if (request->kind == SUDEKIMP_WALLET_TRANSACTION_PURCHASE ||
        request->kind == SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND) {
        return request->subject_quantity != 0u &&
            request->subject_quantity <= INT32_MAX;
    }
    return request->subject_quantity == 0u;
}

static void plan_credit(
    SudekiMpWalletPlan *plan,
    uint32_t index,
    uint32_t balance,
    uint32_t amount
) {
    uint32_t available =
        SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP - balance;
    uint32_t applied = amount < available ? amount : available;
    uint32_t overflow = amount - applied;

    plan->character_credit[index] = applied;
    plan->character_overflow[index] = overflow;
    plan->nominal_character_credit += amount;
    plan->applied_character_credit += applied;
    plan->discarded_character_overflow += overflow;
}

static SudekiMpWalletResult build_plan(
    const SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletRequest *request,
    SudekiMpWalletPlan *plan
) {
    uint32_t character_index = 0u;
    uint32_t index;

    memset(plan, 0, sizeof(*plan));
    plan->request = *request;
    plan->money_policy =
        SudekiMpPersonalWalletMoneyPolicyForKind(request->kind);
    plan->external_effect =
        SudekiMpPersonalWalletExternalEffectForKind(request->kind);

    if ((request->kind == SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD ||
         request->kind == SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD ||
         request->kind == SUDEKIMP_WALLET_TRANSACTION_PURCHASE ||
         request->kind == SUDEKIMP_WALLET_TRANSACTION_FORGE ||
         request->kind == SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND ||
         request->kind == SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION) &&
        !SudekiMpPersonalWalletCharacterIndex(
            request->character_id, &character_index)) {
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }
    switch (request->kind) {
    case SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD:
        plan_credit(
            plan,
            character_index,
            wallet->character_balance[character_index],
            request->amount
        );
        break;
    case SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD: {
        uint64_t available = SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP -
            (uint64_t)wallet->party_reserve;
        uint64_t applied = request->amount < available ?
            request->amount : available;
        plan->reserve_credit = (uint32_t)applied;
        plan->reserve_overflow = request->amount - applied;
        break;
    }
    case SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD:
    case SUDEKIMP_WALLET_TRANSACTION_QUEST_DIVIDEND:
    case SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND:
        for (index = 0u;
             index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
             ++index) {
            plan_credit(
                plan,
                index,
                wallet->character_balance[index],
                request->amount
            );
        }
        break;
    case SUDEKIMP_WALLET_TRANSACTION_PURCHASE:
    case SUDEKIMP_WALLET_TRANSACTION_FORGE:
        if (wallet->character_balance[character_index] < request->amount) {
            return SUDEKIMP_WALLET_REJECTED_FUNDS;
        }
        plan->character_debit[character_index] = request->amount;
        break;
    case SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION:
        if (wallet->party_reserve < request->amount) {
            return SUDEKIMP_WALLET_REJECTED_FUNDS;
        }
        plan->reserve_debit = request->amount;
        plan_credit(
            plan,
            character_index,
            wallet->character_balance[character_index],
            request->amount
        );
        break;
    case SUDEKIMP_WALLET_TRANSACTION_NONE:
    default:
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }

    if (request->kind == SUDEKIMP_WALLET_TRANSACTION_PURCHASE) {
        plan->shared_item_quantity_delta =
            (int32_t)request->subject_quantity;
    } else if (request->kind ==
        SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND) {
        plan->shared_item_quantity_delta =
            -(int32_t)request->subject_quantity;
    }
    return SUDEKIMP_WALLET_PLAN_CREATED;
}

SudekiMpWalletResult SudekiMpPersonalWalletPlanTransaction(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletRequest *request,
    SudekiMpWalletPlan *plan
) {
    SudekiMpWalletPlan candidate;
    SudekiMpWalletResult result;

    if (wallet == NULL || !valid_request_shape(request)) {
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }
    if (wallet->state == SUDEKIMP_WALLET_QUARANTINED) {
        return SUDEKIMP_WALLET_REJECTED_QUARANTINED;
    }
    if (wallet->last_receipt_valid &&
        wallet->last_receipt.request.operation_serial ==
            request->operation_serial) {
        if (!request_equal(&wallet->last_receipt.request, request)) {
            return quarantine(wallet);
        }
        return SUDEKIMP_WALLET_ALREADY_APPLIED;
    }
    if (wallet->state == SUDEKIMP_WALLET_PLANNED ||
        wallet->state == SUDEKIMP_WALLET_APPLYING) {
        if (wallet->pending.request.operation_serial ==
                request->operation_serial &&
            request_equal(&wallet->pending.request, request)) {
            if (plan != NULL) {
                *plan = wallet->pending;
            }
            return SUDEKIMP_WALLET_PLAN_REPLAYED;
        }
        if (wallet->pending.request.operation_serial ==
            request->operation_serial) {
            return quarantine(wallet);
        }
        return SUDEKIMP_WALLET_REJECTED_BUSY;
    }
    if (wallet->state != SUDEKIMP_WALLET_READY) {
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }
    if (request->expected_wallet_generation != wallet->generation ||
        request->operation_serial <= wallet->highest_operation_serial) {
        return SUDEKIMP_WALLET_REJECTED_STALE;
    }
    result = build_plan(wallet, request, &candidate);
    if (result != SUDEKIMP_WALLET_PLAN_CREATED) {
        return result;
    }
    wallet->pending = candidate;
    wallet->highest_operation_serial = request->operation_serial;
    wallet->state = SUDEKIMP_WALLET_PLANNED;
    if (plan != NULL) {
        *plan = candidate;
    }
    return SUDEKIMP_WALLET_PLAN_CREATED;
}

SudekiMpWalletResult SudekiMpPersonalWalletBeginApplication(
    SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial,
    uint32_t expected_wallet_generation
) {
    if (wallet == NULL || operation_serial == 0u ||
        expected_wallet_generation == 0u) {
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }
    if (wallet->state == SUDEKIMP_WALLET_QUARANTINED) {
        return SUDEKIMP_WALLET_REJECTED_QUARANTINED;
    }
    if (wallet->last_receipt_valid &&
        wallet->last_receipt.request.operation_serial == operation_serial) {
        return SUDEKIMP_WALLET_ALREADY_APPLIED;
    }
    if (wallet->state != SUDEKIMP_WALLET_PLANNED) {
        return SUDEKIMP_WALLET_REJECTED_BUSY;
    }
    if (wallet->pending.request.operation_serial != operation_serial ||
        wallet->pending.request.expected_wallet_generation !=
            expected_wallet_generation ||
        wallet->generation != expected_wallet_generation) {
        return SUDEKIMP_WALLET_REJECTED_STALE;
    }
    wallet->state = SUDEKIMP_WALLET_APPLYING;
    return SUDEKIMP_WALLET_APPLICATION_BEGUN;
}

static int external_proof_consistent(
    const SudekiMpWalletPlan *plan,
    SudekiMpWalletExternalOutcome outcome,
    uint32_t observed_external_generation
) {
    uint32_t expected = plan->request.expected_external_generation;

    if (plan->external_effect == SUDEKIMP_WALLET_EXTERNAL_NONE) {
        return observed_external_generation == 0u;
    }
    if (outcome == SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED) {
        return observed_external_generation == expected;
    }
    if (outcome == SUDEKIMP_WALLET_EXTERNAL_VERIFIED) {
        return observed_external_generation != 0u &&
            observed_external_generation != expected;
    }
    return 0;
}

static int plan_still_applies(const SudekiMpPersonalWallet *wallet) {
    uint32_t index;

    if (wallet->generation !=
        wallet->pending.request.expected_wallet_generation) {
        return 0;
    }
    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        uint64_t after_credit =
            (uint64_t)wallet->character_balance[index] +
            wallet->pending.character_credit[index];
        if (wallet->character_balance[index] <
                wallet->pending.character_debit[index] ||
            after_credit > SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP) {
            return 0;
        }
    }
    if (wallet->party_reserve < wallet->pending.reserve_debit) {
        return 0;
    }
    return (uint64_t)(wallet->party_reserve -
        wallet->pending.reserve_debit) +
        wallet->pending.reserve_credit <=
            SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP;
}

static void apply_plan(SudekiMpPersonalWallet *wallet) {
    SudekiMpWalletReceipt receipt;
    uint32_t index;

    memset(&receipt, 0, sizeof(receipt));
    receipt.request = wallet->pending.request;
    receipt.generation_before = wallet->generation;
    receipt.nominal_character_credit =
        wallet->pending.nominal_character_credit;
    receipt.applied_character_credit =
        wallet->pending.applied_character_credit;
    receipt.discarded_character_overflow =
        wallet->pending.discarded_character_overflow;
    receipt.reserve_overflow = wallet->pending.reserve_overflow;

    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        wallet->character_balance[index] -=
            wallet->pending.character_debit[index];
        wallet->character_balance[index] +=
            wallet->pending.character_credit[index];
        receipt.character_balance[index] =
            wallet->character_balance[index];
        receipt.character_credit[index] =
            wallet->pending.character_credit[index];
        receipt.character_debit[index] =
            wallet->pending.character_debit[index];
        receipt.character_overflow[index] =
            wallet->pending.character_overflow[index];
    }
    wallet->party_reserve -= wallet->pending.reserve_debit;
    wallet->party_reserve += wallet->pending.reserve_credit;
    receipt.reserve_credit = wallet->pending.reserve_credit;
    receipt.reserve_debit = wallet->pending.reserve_debit;
    wallet->generation = advance_nonzero(wallet->generation);
    receipt.generation_after = wallet->generation;
    receipt.party_reserve = wallet->party_reserve;
    wallet->last_receipt = receipt;
    wallet->last_receipt_valid = 1;
    clear_pending(wallet);
    wallet->state = SUDEKIMP_WALLET_READY;
}

SudekiMpWalletResult SudekiMpPersonalWalletResolveApplication(
    SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial,
    SudekiMpWalletExternalOutcome outcome,
    uint32_t observed_external_generation,
    SudekiMpWalletReceipt *receipt
) {
    if (wallet == NULL || operation_serial == 0u ||
        outcome < SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED ||
        outcome > SUDEKIMP_WALLET_EXTERNAL_AMBIGUOUS) {
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }
    if (wallet->last_receipt_valid &&
        wallet->last_receipt.request.operation_serial == operation_serial) {
        if (receipt != NULL) {
            *receipt = wallet->last_receipt;
        }
        return SUDEKIMP_WALLET_ALREADY_APPLIED;
    }
    if (wallet->state == SUDEKIMP_WALLET_QUARANTINED) {
        return SUDEKIMP_WALLET_REJECTED_QUARANTINED;
    }
    if (wallet->state != SUDEKIMP_WALLET_APPLYING) {
        return SUDEKIMP_WALLET_REJECTED_BUSY;
    }
    if (wallet->pending.request.operation_serial != operation_serial) {
        return SUDEKIMP_WALLET_REJECTED_STALE;
    }
    if (outcome == SUDEKIMP_WALLET_EXTERNAL_AMBIGUOUS ||
        !external_proof_consistent(
            &wallet->pending, outcome, observed_external_generation)) {
        return quarantine(wallet);
    }
    if (outcome == SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED) {
        clear_pending(wallet);
        wallet->state = SUDEKIMP_WALLET_READY;
        return SUDEKIMP_WALLET_CANCELLED;
    }
    if (!plan_still_applies(wallet)) {
        return quarantine(wallet);
    }
    apply_plan(wallet);
    if (receipt != NULL) {
        *receipt = wallet->last_receipt;
    }
    return SUDEKIMP_WALLET_APPLIED;
}

SudekiMpWalletResult SudekiMpPersonalWalletCancelPlan(
    SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial
) {
    if (wallet == NULL || operation_serial == 0u) {
        return SUDEKIMP_WALLET_REJECTED_INVALID;
    }
    if (wallet->state == SUDEKIMP_WALLET_QUARANTINED) {
        return SUDEKIMP_WALLET_REJECTED_QUARANTINED;
    }
    if (wallet->state != SUDEKIMP_WALLET_PLANNED) {
        return SUDEKIMP_WALLET_REJECTED_BUSY;
    }
    if (wallet->pending.request.operation_serial != operation_serial) {
        return SUDEKIMP_WALLET_REJECTED_STALE;
    }
    clear_pending(wallet);
    wallet->state = SUDEKIMP_WALLET_READY;
    return SUDEKIMP_WALLET_CANCELLED;
}

int SudekiMpPersonalWalletGetBalance(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletCharacterId character_id,
    uint32_t *balance
) {
    uint32_t index;

    if (wallet == NULL || balance == NULL ||
        !SudekiMpPersonalWalletCharacterIndex(character_id, &index)) {
        return 0;
    }
    *balance = wallet->character_balance[index];
    return 1;
}

int SudekiMpPersonalWalletGetPendingPlan(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletPlan *plan
) {
    if (wallet == NULL || plan == NULL ||
        (wallet->state != SUDEKIMP_WALLET_PLANNED &&
         wallet->state != SUDEKIMP_WALLET_APPLYING &&
         wallet->state != SUDEKIMP_WALLET_QUARANTINED) ||
        wallet->pending.request.operation_serial == 0u) {
        return 0;
    }
    *plan = wallet->pending;
    return 1;
}

int SudekiMpPersonalWalletGetLastReceipt(
    const SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial,
    SudekiMpWalletReceipt *receipt
) {
    if (wallet == NULL || receipt == NULL || operation_serial == 0u ||
        !wallet->last_receipt_valid ||
        wallet->last_receipt.request.operation_serial != operation_serial) {
        return 0;
    }
    *receipt = wallet->last_receipt;
    return 1;
}
