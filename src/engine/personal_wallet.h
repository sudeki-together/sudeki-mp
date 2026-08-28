#ifndef SUDEKIMP_PERSONAL_WALLET_H
#define SUDEKIMP_PERSONAL_WALLET_H

#include <stdint.h>

enum {
    SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT = 4u,
    SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP = 99999u,
    SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP = 99999u,
    SUDEKIMP_PERSONAL_WALLET_SCHEMA_VERSION = 1u
};

/* These IDs belong to characters, never controller seats or party ordinals.
 * Their numeric values are persisted policy and must not be renumbered. */
typedef enum SudekiMpWalletCharacterId {
    SUDEKIMP_WALLET_CHARACTER_INVALID = 0,
    SUDEKIMP_WALLET_CHARACTER_TAL = 0x23,
    SUDEKIMP_WALLET_CHARACTER_AILISH = 0x01,
    SUDEKIMP_WALLET_CHARACTER_BUKI = 0x05,
    SUDEKIMP_WALLET_CHARACTER_ELCO = 0x0e
} SudekiMpWalletCharacterId;

typedef enum SudekiMpWalletState {
    SUDEKIMP_WALLET_READY = 0,
    SUDEKIMP_WALLET_PLANNED,
    SUDEKIMP_WALLET_APPLYING,
    SUDEKIMP_WALLET_QUARANTINED
} SudekiMpWalletState;

typedef enum SudekiMpWalletTransactionKind {
    SUDEKIMP_WALLET_TRANSACTION_NONE = 0,
    /* Literal florins or another actor-owned reward source. */
    SUDEKIMP_WALLET_TRANSACTION_PERSONAL_REWARD,
    /* Legacy compatibility policy: anonymous rewards credit the reserve. */
    SUDEKIMP_WALLET_TRANSACTION_PARTY_REWARD,
    /* Shared inventory receives an item; its buyer pays personally. */
    SUDEKIMP_WALLET_TRANSACTION_PURCHASE,
    /* Shared equipment changes; its builder pays personally. */
    SUDEKIMP_WALLET_TRANSACTION_FORGE,
    /* One shared item leaves; every character receives the full price. */
    SUDEKIMP_WALLET_TRANSACTION_SALE_DIVIDEND,
    /* A proven literal florin/drop source gives the full amount to all four. */
    SUDEKIMP_WALLET_TRANSACTION_DIVIDEND_REWARD,
    /* A quest/script reward has no character source but still dividends. */
    SUDEKIMP_WALLET_TRANSACTION_QUEST_DIVIDEND,
    /* Host-only party reserve distribution to one character wallet. */
    SUDEKIMP_WALLET_TRANSACTION_RESERVE_DISTRIBUTION
} SudekiMpWalletTransactionKind;

typedef enum SudekiMpWalletMoneyPolicy {
    SUDEKIMP_WALLET_MONEY_POLICY_INVALID = 0,
    SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_CHARACTER,
    SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_RESERVE,
    SUDEKIMP_WALLET_MONEY_POLICY_DEBIT_CHARACTER,
    SUDEKIMP_WALLET_MONEY_POLICY_CREDIT_ALL_CHARACTERS,
    SUDEKIMP_WALLET_MONEY_POLICY_TRANSFER_RESERVE_TO_CHARACTER
} SudekiMpWalletMoneyPolicy;

typedef enum SudekiMpWalletExternalEffect {
    SUDEKIMP_WALLET_EXTERNAL_NONE = 0,
    SUDEKIMP_WALLET_EXTERNAL_CONSUME_REWARD_SOURCE,
    SUDEKIMP_WALLET_EXTERNAL_ADD_SHARED_ITEM,
    SUDEKIMP_WALLET_EXTERNAL_MUTATE_SHARED_ITEM,
    SUDEKIMP_WALLET_EXTERNAL_REMOVE_SHARED_ITEM_QUANTITY
} SudekiMpWalletExternalEffect;

typedef enum SudekiMpWalletExternalOutcome {
    /* The adapter proved it made no external mutation. */
    SUDEKIMP_WALLET_EXTERNAL_NOT_APPLIED = 0,
    /* The adapter proved the exact requested effect and generation change. */
    SUDEKIMP_WALLET_EXTERNAL_VERIFIED,
    /* The adapter cannot prove whether all or part of the effect happened. */
    SUDEKIMP_WALLET_EXTERNAL_AMBIGUOUS
} SudekiMpWalletExternalOutcome;

typedef enum SudekiMpWalletResult {
    SUDEKIMP_WALLET_NO_CHANGE = 0,
    SUDEKIMP_WALLET_PLAN_CREATED,
    SUDEKIMP_WALLET_PLAN_REPLAYED,
    SUDEKIMP_WALLET_APPLICATION_BEGUN,
    SUDEKIMP_WALLET_APPLIED,
    SUDEKIMP_WALLET_ALREADY_APPLIED,
    SUDEKIMP_WALLET_CANCELLED,
    SUDEKIMP_WALLET_REJECTED_INVALID,
    SUDEKIMP_WALLET_REJECTED_BUSY,
    SUDEKIMP_WALLET_REJECTED_STALE,
    SUDEKIMP_WALLET_REJECTED_FUNDS,
    SUDEKIMP_WALLET_REJECTED_QUARANTINED,
    SUDEKIMP_WALLET_ENTERED_QUARANTINE
} SudekiMpWalletResult;

typedef struct SudekiMpWalletRequest {
    /* Supplied by the interaction coordinator. Serials are nonzero and
     * strictly increasing for this save identity. */
    uint64_t operation_serial;
    uint32_t expected_wallet_generation;
    uint32_t expected_external_generation;
    SudekiMpWalletTransactionKind kind;
    SudekiMpWalletCharacterId character_id;
    /* Total reward, price, or proceeds for this whole operation. */
    uint32_t amount;
    /* Zero is a valid native item ID when subject_known is true. */
    uint32_t subject_id;
    /* Native purchase/sale quantity. Other kinds require zero. */
    uint32_t subject_quantity;
    int subject_known;
} SudekiMpWalletRequest;

typedef struct SudekiMpWalletPlan {
    SudekiMpWalletRequest request;
    SudekiMpWalletMoneyPolicy money_policy;
    SudekiMpWalletExternalEffect external_effect;
    /* Purchases add the requested shared quantity, sales remove it, and other
     * operations do not change a shared item quantity through this plan. */
    int32_t shared_item_quantity_delta;
    uint32_t character_credit[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t character_debit[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t character_overflow[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t reserve_credit;
    uint32_t reserve_debit;
    uint64_t reserve_overflow;
    uint64_t nominal_character_credit;
    uint64_t applied_character_credit;
    uint64_t discarded_character_overflow;
} SudekiMpWalletPlan;

typedef struct SudekiMpWalletReceipt {
    SudekiMpWalletRequest request;
    uint32_t generation_before;
    uint32_t generation_after;
    uint32_t character_balance[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t character_credit[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t character_debit[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t character_overflow[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t party_reserve;
    uint32_t reserve_credit;
    uint32_t reserve_debit;
    uint64_t nominal_character_credit;
    uint64_t applied_character_credit;
    uint64_t discarded_character_overflow;
    uint64_t reserve_overflow;
} SudekiMpWalletReceipt;

typedef struct SudekiMpWalletPersistedEntry {
    SudekiMpWalletCharacterId character_id;
    uint32_t balance;
} SudekiMpWalletPersistedEntry;

/* This is a semantic snapshot for a future serializer, not a packed on-disk
 * format. It contains no pointer, actor, seat, or native UI state. */
typedef struct SudekiMpWalletPersistedSnapshot {
    uint32_t schema_version;
    uint32_t generation;
    uint32_t party_reserve;
    uint64_t highest_operation_serial;
    SudekiMpWalletPersistedEntry
        characters[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
} SudekiMpWalletPersistedSnapshot;

typedef struct SudekiMpWalletMigrationReport {
    uint64_t legacy_party_money;
    uint32_t migrated_to_reserve;
    uint64_t discarded_overflow;
} SudekiMpWalletMigrationReport;

typedef struct SudekiMpPersonalWallet {
    SudekiMpWalletState state;
    uint32_t generation;
    uint32_t character_balance[SUDEKIMP_PERSONAL_WALLET_CHARACTER_COUNT];
    uint32_t party_reserve;
    uint64_t highest_operation_serial;
    SudekiMpWalletPlan pending;
    SudekiMpWalletReceipt last_receipt;
    int last_receipt_valid;
} SudekiMpPersonalWallet;

void SudekiMpPersonalWalletInitialize(SudekiMpPersonalWallet *wallet);

/* Call Initialize first. Existing single-player money migrates once into the
 * reserve. It is never duplicated into the four character wallets. Re-running
 * migration on a used ledger is rejected without resetting it. */
int SudekiMpPersonalWalletMigrateLegacy(
    SudekiMpPersonalWallet *wallet,
    uint64_t legacy_party_money,
    SudekiMpWalletMigrationReport *report
);

int SudekiMpPersonalWalletExport(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletPersistedSnapshot *snapshot
);
/* Initialize before first restore. Ordinary restore is READY-only so it can
 * never erase a planned/applying external effect. Invalid READY-state input
 * preserves current balances and serials, then quarantines the ledger. */
int SudekiMpPersonalWalletRestore(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletPersistedSnapshot *snapshot
);
int SudekiMpPersonalWalletRecover(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletPersistedSnapshot *trusted_snapshot
);

SudekiMpWalletMoneyPolicy SudekiMpPersonalWalletMoneyPolicyForKind(
    SudekiMpWalletTransactionKind kind
);
SudekiMpWalletExternalEffect SudekiMpPersonalWalletExternalEffectForKind(
    SudekiMpWalletTransactionKind kind
);
int SudekiMpPersonalWalletCharacterIndex(
    SudekiMpWalletCharacterId character_id,
    uint32_t *index
);
SudekiMpWalletCharacterId SudekiMpPersonalWalletCharacterIdAt(
    uint32_t index
);

/* Planning is native-inert. A repeated identical in-flight request returns
 * PLAN_REPLAYED. A completed serial can never debit or credit twice. */
SudekiMpWalletResult SudekiMpPersonalWalletPlanTransaction(
    SudekiMpPersonalWallet *wallet,
    const SudekiMpWalletRequest *request,
    SudekiMpWalletPlan *plan
);
SudekiMpWalletResult SudekiMpPersonalWalletBeginApplication(
    SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial,
    uint32_t expected_wallet_generation
);
SudekiMpWalletResult SudekiMpPersonalWalletResolveApplication(
    SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial,
    SudekiMpWalletExternalOutcome outcome,
    uint32_t observed_external_generation,
    SudekiMpWalletReceipt *receipt
);
SudekiMpWalletResult SudekiMpPersonalWalletCancelPlan(
    SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial
);

int SudekiMpPersonalWalletGetBalance(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletCharacterId character_id,
    uint32_t *balance
);
int SudekiMpPersonalWalletGetPendingPlan(
    const SudekiMpPersonalWallet *wallet,
    SudekiMpWalletPlan *plan
);
int SudekiMpPersonalWalletGetLastReceipt(
    const SudekiMpPersonalWallet *wallet,
    uint64_t operation_serial,
    SudekiMpWalletReceipt *receipt
);

#endif
