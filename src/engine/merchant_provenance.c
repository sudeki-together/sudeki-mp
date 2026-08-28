#include "engine/merchant_provenance.h"

#include <string.h>

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) {
        ++value;
    }
    return value;
}

static int evidence_valid(const SudekiMpMerchantInteractionEvidence *evidence) {
    return evidence != NULL && evidence->sol_thread != 0u &&
        evidence->source_actor != 0u && evidence->merchant_owner != 0u &&
        evidence->merchant_target != 0u &&
        evidence->seat < SUDEKIMP_MERCHANT_PROVENANCE_MAX_SEATS &&
        evidence->actor_generation != 0u && evidence->source_generation != 0u &&
        evidence->interaction_authority_proven;
}

void SudekiMpMerchantProvenanceInitialize(
    SudekiMpMerchantProvenanceTracker *tracker
) {
    if (tracker == NULL) {
        return;
    }
    memset(tracker, 0, sizeof(*tracker));
}

void SudekiMpMerchantProvenanceInvalidate(
    SudekiMpMerchantProvenanceTracker *tracker
) {
    if (tracker == NULL) {
        return;
    }
    memset(&tracker->active, 0, sizeof(tracker->active));
    tracker->active_valid = 0;
}

SudekiMpMerchantProvenanceResult SudekiMpMerchantProvenanceObserveSolCall(
    SudekiMpMerchantProvenanceTracker *tracker,
    const SudekiMpMerchantInteractionEvidence *evidence,
    uint32_t call_hash
) {
    SudekiMpMerchantProvenance *active;

    if (tracker == NULL || evidence == NULL ||
        call_hash != SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) {
        return SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_INVALID;
    }
    if (!evidence_valid(evidence)) {
        return SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_UNTRUSTED;
    }
    active = &tracker->active;
    if (tracker->active_valid) {
        if (active->source_generation != evidence->source_generation) {
            SudekiMpMerchantProvenanceInvalidate(tracker);
        } else if (active->sol_thread == evidence->sol_thread &&
                   active->source_actor == evidence->source_actor &&
                   active->merchant_owner == evidence->merchant_owner &&
                   active->merchant_target == evidence->merchant_target &&
                   active->actor_generation == evidence->actor_generation) {
            return SUDEKIMP_MERCHANT_PROVENANCE_NO_CHANGE;
        } else {
            return SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_STALE;
        }
    }
    tracker->next_serial = advance_nonzero(tracker->next_serial);
    memset(active, 0, sizeof(*active));
    active->serial = tracker->next_serial;
    active->token = ((uint64_t)evidence->source_generation << 32u) |
        (uint64_t)active->serial;
    active->seat = evidence->seat;
    active->actor_generation = evidence->actor_generation;
    active->source_generation = evidence->source_generation;
    active->opened_at_ms = evidence->observed_at_ms;
    active->sol_thread = evidence->sol_thread;
    active->source_actor = evidence->source_actor;
    active->merchant_owner = evidence->merchant_owner;
    active->merchant_target = evidence->merchant_target;
    tracker->active_valid = 1;
    return SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED;
}

int SudekiMpMerchantProvenanceGet(
    const SudekiMpMerchantProvenanceTracker *tracker,
    uint32_t current_source_generation,
    SudekiMpMerchantProvenance *provenance
) {
    if (tracker == NULL || provenance == NULL || !tracker->active_valid ||
        current_source_generation == 0u ||
        tracker->active.source_generation != current_source_generation ||
        tracker->active.token == 0u) {
        return 0;
    }
    *provenance = tracker->active;
    return 1;
}
