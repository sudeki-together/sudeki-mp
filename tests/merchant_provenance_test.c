#include "engine/merchant_provenance.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpMerchantInteractionEvidence trusted_evidence(void) {
    SudekiMpMerchantInteractionEvidence evidence;

    memset(&evidence, 0, sizeof(evidence));
    evidence.sol_thread = 0x1000u;
    evidence.source_actor = 0x2000u;
    evidence.merchant_owner = 0x3000u;
    evidence.merchant_target = 0x4000u;
    evidence.seat = 1u;
    evidence.actor_generation = 7u;
    evidence.source_generation = 11u;
    evidence.observed_at_ms = 99u;
    evidence.interaction_authority_proven = 1;
    return evidence;
}

static void test_exact_and_trusted_start(void) {
    SudekiMpMerchantProvenanceTracker tracker;
    SudekiMpMerchantInteractionEvidence evidence = trusted_evidence();
    SudekiMpMerchantProvenance provenance;

    SudekiMpMerchantProvenanceInitialize(&tracker);
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &evidence,
        0u) == SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_INVALID);
    evidence.interaction_authority_proven = 0;
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &evidence,
        SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) ==
        SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_UNTRUSTED);
    evidence.interaction_authority_proven = 1;
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &evidence,
        SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) ==
        SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED);
    CHECK(SudekiMpMerchantProvenanceGet(&tracker, 11u, &provenance));
    CHECK(provenance.token == (((uint64_t)11u << 32u) | 1u));
    CHECK(provenance.seat == 1u && provenance.merchant_target == 0x4000u);
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &evidence,
        SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) ==
        SUDEKIMP_MERCHANT_PROVENANCE_NO_CHANGE);
}

static void test_generation_and_competing_session_fail_closed(void) {
    SudekiMpMerchantProvenanceTracker tracker;
    SudekiMpMerchantInteractionEvidence evidence = trusted_evidence();
    SudekiMpMerchantInteractionEvidence other;
    SudekiMpMerchantProvenance provenance;

    SudekiMpMerchantProvenanceInitialize(&tracker);
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &evidence,
        SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) ==
        SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED);
    other = evidence;
    other.sol_thread = 0x1001u;
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &other,
        SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) ==
        SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_STALE);
    CHECK(!SudekiMpMerchantProvenanceGet(&tracker, 12u, &provenance));
    evidence.source_generation = 12u;
    CHECK(SudekiMpMerchantProvenanceObserveSolCall(&tracker, &evidence,
        SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) ==
        SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED);
    CHECK(SudekiMpMerchantProvenanceGet(&tracker, 12u, &provenance));
    CHECK(provenance.serial == 2u);
    SudekiMpMerchantProvenanceInvalidate(&tracker);
    CHECK(!SudekiMpMerchantProvenanceGet(&tracker, 12u, &provenance));
}

int main(void) {
    test_exact_and_trusted_start();
    test_generation_and_competing_session_fail_closed();
    if (failures != 0) {
        fprintf(stderr, "merchant_provenance_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("merchant_provenance_test: PASS");
    return 0;
}
