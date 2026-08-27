#include "engine/personal_wallet.h"

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

static SudekiMpWalletRequest request_for(
    const SudekiMpPersonalWallet *wallet,
    uint64_t serial,
    SudekiMpWalletTransactionKind kind,
    SudekiMpWalletCharacterId character_id,
    uint32_t amount,
    uint32_t external_generation,
    uint32_t subject_id,
    uint32_t subject_quantity,
    int subject_known
) {
    SudekiMpWalletRequest request;

    memset(&request, 0, sizeof(request));
    request.operation_serial = serial;
    request.expected_wallet_generation = wallet->generation;
    request.expected_external_generation = external_generation;
    request.kind = kind;
    request.character_id = character_id;
    request.amount = amount;
    request.subject_id = subject_id;
    request.subject_quantity = subject_quantity;
    request.subject_known = subject_known;
    return request;
}

static SudekiMpWalletPersistedSnapshot snapshot_with_balances(
    uint32_t tal,
    uint32_t ailish,
    uint32_t buki,
    uint32_t elco,
    uint32_t reserve,
    uint32_t generation,
    uint64_t highest_serial
) {
    SudekiMpWalletPersistedSnapshot snapshot;
    const uint32_t balances[] = {tal, ailish, buki, elco};
    uint32_t index;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.schema_version = SUDEKIMP_PERSONAL_WALLET_SCHEMA_VERSION;
    snapshot.generation = generation;
    snapshot.party_reserve = reserve;
    snapshot.highest_operation_serial = highest_serial;
    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        snapshot.characters[index].character_id =
            SudekiMpPersonalWalletCharacterIdAt(index);
        snapshot.characters[index].balance = balances[index];
    }
    return snapshot;
}

static uint32_t balance_of(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletCharacterId character_id
) {
    uint32_t balance = UINT32_MAX;

    CHECK(SudekiMpPersonalWalletGetBalance(
        wallet, character_id, &balance));
    return balance;
}

static void test_stable_character_ids_and_policy(void) {
    uint32_t index = UINT32_MAX;

    CHECK(SUDEKIMP_WALLET_CHARACTER_TAL == 0x23);
    CHECK(SUDEKIMP_WALLET_CHARACTER_AILISH == 0x01);
    CHECK(SUDEKIMP_WALLET_CHARACTER_BUKI == 0x05);
    CHECK(SUDEKIMP_WALLET_CHARACTER_ELCO == 0x0e);
    CHECK(SudekiMpPersonalWalletCharacterIndex(
        SUDEKIMP_WALLET_CHARACTER_BUKI, &index));
    CHECK(index == 2u);
    CHECK(SudekiMpPersonalWalletCharacterIdAt(3u) ==
        SUDEKIMP_WALLET_CHARACTER_ELCO);
    CHECK(SudekiMpPersonalWalletCharacterIdAt(4u) ==
        SUDEKIMP_WALLET_CHARACTER_INVALID);
    CHECK(SudekiMpPersonalWalletMoneyPolicyForKind(
        SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND) ==
        SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_ALL_CHARACTERS);
    CHECK(SudekiMpPersonalWalletExternalEffectForKind(
        SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND) ==
        SUDEKIMP_WALLET_EXTERNAL_REMOVE_SHARED_ITEM_QUANTITY);
}

static void test_sale_is_one_removal_and_four_full_dividends(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletRequest request;
    SudekiMpWalletPlan plan;
    SudekiMpWalletPlan replayed;
    SudekiMpWalletReceipt receipt;
    uint32_t index;
    uint32_t generation_after;

    SudekiMpPersonalWalletInitialize(&wallet);
    request = request_for(
        &wallet,
        1u,
        SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
        SUDEKIMP_WALLET_CHARACTER_AILISH,
        125u,
        9u,
        77u,
        1u,
        1
    );
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, &plan) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(wallet.state == SUDEKIMP_WALLET_PLANNED);
    CHECK(plan.external_effect ==
        SUDEKIMP_WALLET_EXTERNAL_REMOVE_SHARED_ITEM_QUANTITY);
    CHECK(plan.shared_item_quantity_delta == -1);
    CHECK(plan.reserve_credit == 0u);
    CHECK(plan.nominal_character_credit == 500u);
    CHECK(plan.applied_character_credit == 500u);
    CHECK(plan.discarded_character_overflow == 0u);
    for (index = 0u;
         index < SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT;
         ++index) {
        CHECK(plan.character_credit[index] == 125u);
        CHECK(plan.character_debit[index] == 0u);
    }
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, &replayed) == SUDEKIMP_WALLET_PLAN_REPLAYED);
    CHECK(replayed.request.operation_serial == 1u);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 1u, request.expected_wallet_generation + 1u) ==
        SUDEKIMP_WALLET_REJECTED_STALE);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 1u, request.expected_wallet_generation) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 1u, SUDEKIMP_WALLET_EXTERNAL_VERIFIED, 10u,
        &receipt) == SUDEKIMP_WALLET_APPLIED);
    CHECK(receipt.nominal_character_credit == 500u);
    CHECK(receipt.applied_character_credit == 500u);
    CHECK(receipt.character_credit[0] == 125u);
    CHECK(receipt.character_credit[1] == 125u);
    CHECK(receipt.character_credit[2] == 125u);
    CHECK(receipt.character_credit[3] == 125u);
    CHECK(receipt.character_debit[0] == 0u);
    CHECK(receipt.character_overflow[0] == 0u);
    CHECK(receipt.party_reserve == 0u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 125u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_AILISH) == 125u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_BUKI) == 125u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_ELCO) == 125u);

    generation_after = wallet.generation;
    memset(&receipt, 0, sizeof(receipt));
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 1u, SUDEKIMP_WALLET_EXTERNAL_VERIFIED, 10u,
        &receipt) == SUDEKIMP_WALLET_ALREADY_APPLIED);
    CHECK(wallet.generation == generation_after);
    CHECK(receipt.applied_character_credit == 500u);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_ALREADY_APPLIED);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 125u);
}

static void test_sale_caps_each_wallet_and_discards_overflow(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletPersistedSnapshot snapshot = snapshot_with_balances(
        99990u, 99999u, 99900u, 0u, 17u, 5u, 10u);
    SudekiMpWalletRequest request;
    SudekiMpWalletPlan plan;
    SudekiMpWalletReceipt receipt;

    SudekiMpPersonalWalletInitialize(&wallet);
    CHECK(SudekiMpPersonalWalletRestore(&wallet, &snapshot));
    request = request_for(
        &wallet,
        11u,
        SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
        SUDEKIMP_WALLET_CHARACTER_TAL,
        200u,
        20u,
        44u,
        1u,
        1
    );
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, &plan) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(plan.character_credit[0] == 9u);
    CHECK(plan.character_credit[1] == 0u);
    CHECK(plan.character_credit[2] == 99u);
    CHECK(plan.character_credit[3] == 200u);
    CHECK(plan.character_overflow[0] == 191u);
    CHECK(plan.character_overflow[1] == 200u);
    CHECK(plan.character_overflow[2] == 101u);
    CHECK(plan.character_overflow[3] == 0u);
    CHECK(plan.nominal_character_credit == 800u);
    CHECK(plan.applied_character_credit == 308u);
    CHECK(plan.discarded_character_overflow == 492u);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 11u, request.expected_wallet_generation) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 11u, SUDEKIMP_WALLET_EXTERNAL_VERIFIED, 21u,
        &receipt) == SUDEKIMP_WALLET_APPLIED);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 99999u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_AILISH) == 99999u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_BUKI) == 99999u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_ELCO) == 200u);
    CHECK(wallet.party_reserve == 17u);
    CHECK(receipt.discarded_character_overflow == 492u);
    CHECK(receipt.character_credit[0] == 9u);
    CHECK(receipt.character_overflow[0] == 191u);
}

static void test_purchase_and_sale_quantities_are_explicit(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletPersistedSnapshot snapshot = snapshot_with_balances(
        1000u, 0u, 0u, 0u, 0u, 3u, 7u);
    SudekiMpWalletRequest request;
    SudekiMpWalletPlan plan;

    SudekiMpPersonalWalletInitialize(&wallet);
    CHECK(SudekiMpPersonalWalletRestore(&wallet, &snapshot));
    request = request_for(
        &wallet, 8u, SUDEKIMP_WALLET_TRANSACTION_PURCHASE,
        SUDEKIMP_WALLET_CHARACTER_TAL, 300u, 20u, 12u, 3u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, &plan) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(plan.shared_item_quantity_delta == 3);
    CHECK(plan.character_debit[0] == 300u);
    CHECK(SudekiMpPersonalWalletCancelPlan(&wallet, 8u) ==
        SUDEKIMP_WALLET_CANCELLED);

    request = request_for(
        &wallet, 9u, SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
        SUDEKIMP_WALLET_CHARACTER_TAL, 300u, 30u, 12u, 3u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, &plan) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(plan.shared_item_quantity_delta == -3);
    /* amount is total proceeds, still duplicated once to each wallet. */
    CHECK(plan.character_credit[0] == 300u);
    CHECK(plan.character_credit[1] == 300u);
    CHECK(plan.nominal_character_credit == 1200u);
    CHECK(SudekiMpPersonalWalletCancelPlan(&wallet, 9u) ==
        SUDEKIMP_WALLET_CANCELLED);

    request = request_for(
        &wallet, 10u, SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
        SUDEKIMP_WALLET_CHARACTER_TAL, 1u, 40u, 12u,
        (uint32_t)INT32_MAX + 1u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_REJECTED_INVALID);
    request.subject_quantity = 0u;
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_REJECTED_INVALID);
}

static void apply_verified(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletRequest *request,
    uint32_t observed_external_generation
) {
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        wallet, request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        wallet,
        request->operation_serial,
        request->expected_wallet_generation
    ) == SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(SudekiMpPersonalWalletResolveApplication(
        wallet,
        request->operation_serial,
        SUDEKIMP_WALLET_EXTERNAL_VERIFIED,
        observed_external_generation,
        NULL
    ) == SUDEKIMP_WALLET_APPLIED);
}

static void test_reward_purchase_forge_and_reserve_policies(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletRequest request;
    uint32_t stale_generation;

    SudekiMpPersonalWalletInitialize(&wallet);
    stale_generation = wallet.generation;
    request = request_for(
        &wallet, 1u, SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD,
        SUDEKIMP_WALLET_CHARACTER_TAL, 1000u, 40u, 901u, 0u, 1);
    apply_verified(&wallet, &request, 41u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 1000u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_AILISH) == 0u);

    request = request_for(
        &wallet, 2u, SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD,
        SUDEKIMP_WALLET_CHARACTER_INVALID, 300u, 0u, 0u, 0u, 0);
    apply_verified(&wallet, &request, 0u);
    CHECK(wallet.party_reserve == 300u);

    request = request_for(
        &wallet, 3u, SUDEKIMP_WALLET_TRANSACTION_PURCHASE,
        SUDEKIMP_WALLET_CHARACTER_TAL, 250u, 50u, 12u, 2u, 1);
    apply_verified(&wallet, &request, 51u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 750u);

    request = request_for(
        &wallet, 4u, SUDEKIMP_WALLET_TRANSACTION_FORGE,
        SUDEKIMP_WALLET_CHARACTER_TAL, 125u, 60u, 0u, 0u, 1);
    apply_verified(&wallet, &request, 61u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 625u);
    CHECK(wallet.party_reserve == 300u);

    request = request_for(
        &wallet, 5u, SUDEKIMP_WALLET_TRANSACTION_PURCHASE,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 1u, 70u, 7u, 1u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_REJECTED_FUNDS);

    request = request_for(
        &wallet, 6u, SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 1u, 80u, 2u, 0u, 1);
    request.expected_wallet_generation = stale_generation;
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_REJECTED_STALE);
}

static void test_party_reward_needs_verified_outcome_and_reserve_caps(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletPersistedSnapshot snapshot = snapshot_with_balances(
        0u, 0u, 0u, 0u, 99990u, 4u, 20u);
    SudekiMpWalletRequest request;
    SudekiMpWalletPlan plan;
    uint32_t generation_before;

    SudekiMpPersonalWalletInitialize(&wallet);
    CHECK(SudekiMpPersonalWalletRestore(&wallet, &snapshot));
    generation_before = wallet.generation;
    request = request_for(
        &wallet, 21u, SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD,
        SUDEKIMP_WALLET_CHARACTER_INVALID, 20u, 0u, 0u, 0u, 0);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, &plan) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(plan.reserve_credit == 9u);
    CHECK(plan.reserve_overflow == 11u);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 21u, generation_before) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 21u, SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED, 0u,
        NULL) == SUDEKIMP_WALLET_CANCELLED);
    CHECK(wallet.party_reserve == 99990u);
    CHECK(wallet.generation == generation_before);

    request = request_for(
        &wallet, 22u, SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD,
        SUDEKIMP_WALLET_CHARACTER_INVALID, 20u, 0u, 0u, 0u, 0);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 22u, request.expected_wallet_generation) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    {
        SudekiMpWalletReceipt receipt;
        CHECK(SudekiMpPersonalWalletResolveApplication(
            &wallet, 22u, SUDEKIMP_WALLET_EXTERNAL_VERIFIED, 0u,
            &receipt) == SUDEKIMP_WALLET_APPLIED);
        CHECK(receipt.reserve_credit == 9u);
        CHECK(receipt.reserve_overflow == 11u);
    }
    CHECK(wallet.party_reserve == 99999u);
}

static void test_ambiguous_external_effect_quarantines(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletPersistedSnapshot trusted = snapshot_with_balances(
        10u, 20u, 30u, 40u, 50u, 9u, 100u);
    SudekiMpWalletPersistedSnapshot ignored;
    SudekiMpWalletRequest request;
    uint32_t generation_before;

    SudekiMpPersonalWalletInitialize(&wallet);
    CHECK(SudekiMpPersonalWalletRestore(&wallet, &trusted));
    generation_before = wallet.generation;
    request = request_for(
        &wallet, 101u, SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
        SUDEKIMP_WALLET_CHARACTER_ELCO, 25u, 90u, 8u, 1u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 101u, generation_before) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 101u, SUDEKIMP_WALLET_EXTERNAL_AMBIGUOUS, 91u,
        NULL) == SUDEKIMP_WALLET_ENTERED_QUARANTINE);
    CHECK(wallet.state == SUDEKIMP_WALLET_QUARANTINED);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 10u);
    CHECK(!SudekiMpPersonalWalletExport(&wallet, &ignored));
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) ==
        SUDEKIMP_WALLET_REJECTED_QUARANTINED);

    CHECK(SudekiMpPersonalWalletRecover(&wallet, &trusted));
    CHECK(wallet.state == SUDEKIMP_WALLET_READY);
    CHECK(wallet.generation == 10u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_ELCO) == 40u);
}

static void test_external_proof_mismatch_quarantines(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletRequest request;

    SudekiMpPersonalWalletInitialize(&wallet);
    request = request_for(
        &wallet, 1u, SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD,
        SUDEKIMP_WALLET_CHARACTER_BUKI, 50u, 70u, 88u, 0u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 1u, wallet.generation) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    /* "Not applied" is contradictory when the external generation moved. */
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 1u, SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED, 71u,
        NULL) == SUDEKIMP_WALLET_ENTERED_QUARANTINE);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_BUKI) == 0u);
}

static void test_cancel_and_not_applied_do_not_move_money(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletRequest request;

    SudekiMpPersonalWalletInitialize(&wallet);
    request = request_for(
        &wallet, 1u, SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 50u, 70u, 88u, 0u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(SudekiMpPersonalWalletCancelPlan(&wallet, 1u) ==
        SUDEKIMP_WALLET_CANCELLED);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_AILISH) == 0u);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_REJECTED_STALE);

    request = request_for(
        &wallet, 2u, SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 50u, 70u, 88u, 0u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);
    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 2u, wallet.generation) ==
        SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 2u, SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED, 70u,
        NULL) == SUDEKIMP_WALLET_CANCELLED);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_AILISH) == 0u);
}

static void test_migration_and_id_keyed_snapshot(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpPersonalWallet restored;
    SudekiMpWalletMigrationReport report;
    SudekiMpWalletPersistedSnapshot snapshot;
    SudekiMpWalletPersistedEntry swap;

    SudekiMpPersonalWalletInitialize(&wallet);
    CHECK(SudekiMpPersonalWalletMigrateLegacy(
        &wallet, 100050u, &report));
    CHECK(wallet.party_reserve == 99999u);
    CHECK(report.migrated_to_reserve == 99999u);
    CHECK(report.discarded_overflow == 51u);
    CHECK(!SudekiMpPersonalWalletMigrateLegacy(&wallet, 1u, NULL));
    CHECK(wallet.party_reserve == 99999u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 0u);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_ELCO) == 0u);
    CHECK(SudekiMpPersonalWalletExport(&wallet, &snapshot));

    /* Restore is keyed by stable character ID, not array position. */
    snapshot.characters[0].balance = 11u;
    snapshot.characters[1].balance = 22u;
    swap = snapshot.characters[0];
    snapshot.characters[0] = snapshot.characters[1];
    snapshot.characters[1] = swap;
    SudekiMpPersonalWalletInitialize(&restored);
    CHECK(SudekiMpPersonalWalletRestore(&restored, &snapshot));
    CHECK(balance_of(&restored, SUDEKIMP_WALLET_CHARACTER_TAL) == 11u);
    CHECK(balance_of(&restored, SUDEKIMP_WALLET_CHARACTER_AILISH) == 22u);

    snapshot.characters[1].character_id =
        snapshot.characters[0].character_id;
    CHECK(!SudekiMpPersonalWalletRestore(&restored, &snapshot));
    CHECK(restored.state == SUDEKIMP_WALLET_QUARANTINED);
    CHECK(balance_of(&restored, SUDEKIMP_WALLET_CHARACTER_TAL) == 11u);
    CHECK(balance_of(&restored, SUDEKIMP_WALLET_CHARACTER_AILISH) == 22u);
    CHECK(restored.party_reserve == 99999u);
}

static void test_restore_cannot_erase_planned_or_applying_effect(void) {
    SudekiMpPersonalWallet wallet;
    SudekiMpWalletPersistedSnapshot initial = snapshot_with_balances(
        10u, 20u, 30u, 40u, 50u, 7u, 100u);
    SudekiMpWalletPersistedSnapshot replacement = snapshot_with_balances(
        900u, 800u, 700u, 600u, 500u, 20u, 200u);
    SudekiMpWalletRequest request;
    SudekiMpWalletPlan pending;
    uint32_t generation;

    SudekiMpPersonalWalletInitialize(&wallet);
    CHECK(SudekiMpPersonalWalletRestore(&wallet, &initial));
    generation = wallet.generation;
    request = request_for(
        &wallet, 101u, SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
        SUDEKIMP_WALLET_CHARACTER_AILISH, 25u, 60u, 5u, 1u, 1);
    CHECK(SudekiMpPersonalWalletPlanTransaction(
        &wallet, &request, NULL) == SUDEKIMP_WALLET_PLAN_CREATED);

    CHECK(!SudekiMpPersonalWalletRestore(&wallet, &replacement));
    CHECK(wallet.state == SUDEKIMP_WALLET_PLANNED);
    CHECK(wallet.generation == generation);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_TAL) == 10u);
    CHECK(wallet.party_reserve == 50u);
    CHECK(SudekiMpPersonalWalletGetPendingPlan(&wallet, &pending));
    CHECK(pending.request.operation_serial == 101u);

    CHECK(SudekiMpPersonalWalletBeginApplication(
        &wallet, 101u, generation) == SUDEKIMP_WALLET_APPLICATION_BEGUN);
    CHECK(!SudekiMpPersonalWalletRestore(&wallet, &replacement));
    CHECK(wallet.state == SUDEKIMP_WALLET_APPLYING);
    CHECK(wallet.generation == generation);
    CHECK(balance_of(&wallet, SUDEKIMP_WALLET_CHARACTER_AILISH) == 20u);
    CHECK(wallet.party_reserve == 50u);
    CHECK(SudekiMpPersonalWalletGetPendingPlan(&wallet, &pending));
    CHECK(pending.request.operation_serial == 101u);

    CHECK(SudekiMpPersonalWalletResolveApplication(
        &wallet, 101u, SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED, 60u,
        NULL) == SUDEKIMP_WALLET_CANCELLED);
}

int main(void) {
    test_stable_character_ids_and_policy();
    test_sale_is_one_removal_and_four_full_dividends();
    test_sale_caps_each_wallet_and_discards_overflow();
    test_purchase_and_sale_quantities_are_explicit();
    test_reward_purchase_forge_and_reserve_policies();
    test_party_reward_needs_verified_outcome_and_reserve_caps();
    test_ambiguous_external_effect_quarantines();
    test_external_proof_mismatch_quarantines();
    test_cancel_and_not_applied_do_not_move_money();
    test_migration_and_id_keyed_snapshot();
    test_restore_cannot_erase_planned_or_applying_effect();

    if (failures != 0) {
        fprintf(stderr, "%d personal-wallet checks failed\n", failures);
        return 1;
    }
    puts("personal-wallet checks passed");
    return 0;
}
