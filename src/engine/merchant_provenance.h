#ifndef SUDEKIMP_MERCHANT_PROVENANCE_H
#define SUDEKIMP_MERCHANT_PROVENANCE_H

#include <stdint.h>

/* Hash stored directly before "ShopStart|B" in WINSOLM.gex for the supported
 * GOG executable. The runtime hook will obtain this from opcode 0x27 only;
 * callers must never infer a shop from the global CShopInventory singleton. */
enum {
    SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH = 0x7ff08fb5u,
    SUDEKIMP_MERCHANT_PROVENANCE_MAX_SEATS = 4u
};

typedef enum SudekiMpMerchantProvenanceResult {
    SUDEKIMP_MERCHANT_PROVENANCE_NO_CHANGE = 0,
    SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED,
    SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_INVALID,
    SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_UNTRUSTED,
    SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_STALE
} SudekiMpMerchantProvenanceResult;

/* Pointer values are short-lived runtime identities only. They are retained
 * exclusively for immediate revalidation by the native adapter; save data and
 * checkout requests use token/source_generation, never these addresses. */
typedef struct SudekiMpMerchantInteractionEvidence {
    uintptr_t sol_thread;
    uintptr_t source_actor;
    uintptr_t merchant_owner;
    uintptr_t merchant_target;
    uint32_t seat;
    uint32_t actor_generation;
    uint32_t source_generation;
    uint32_t observed_at_ms;
    int interaction_authority_proven;
} SudekiMpMerchantInteractionEvidence;

typedef struct SudekiMpMerchantProvenance {
    uint64_t token;
    uint32_t serial;
    uint32_t seat;
    uint32_t actor_generation;
    uint32_t source_generation;
    uint32_t opened_at_ms;
    uintptr_t sol_thread;
    uintptr_t source_actor;
    uintptr_t merchant_owner;
    uintptr_t merchant_target;
} SudekiMpMerchantProvenance;

typedef struct SudekiMpMerchantProvenanceTracker {
    SudekiMpMerchantProvenance active;
    uint32_t next_serial;
    int active_valid;
} SudekiMpMerchantProvenanceTracker;

void SudekiMpMerchantProvenanceInitialize(
    SudekiMpMerchantProvenanceTracker *tracker
);
void SudekiMpMerchantProvenanceInvalidate(
    SudekiMpMerchantProvenanceTracker *tracker
);

/* The caller supplies the hash dispatched by the exact SOL opcode hook. An
 * accepted record is one global merchant session; duplicate same-thread
 * ShopStart calls retain the original token, while a different source fails
 * closed until the previous session is explicitly invalidated. */
SudekiMpMerchantProvenanceResult SudekiMpMerchantProvenanceObserveSolCall(
    SudekiMpMerchantProvenanceTracker *tracker,
    const SudekiMpMerchantInteractionEvidence *evidence,
    uint32_t call_hash
);

int SudekiMpMerchantProvenanceGet(
    const SudekiMpMerchantProvenanceTracker *tracker,
    uint32_t current_source_generation,
    SudekiMpMerchantProvenance *provenance
);

#endif
