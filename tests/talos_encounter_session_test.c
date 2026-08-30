#include "engine/talos_encounter_session.h"

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

static SudekiMpTalosEncounterRequest request_for_mask(uint8_t mask) {
    SudekiMpTalosEncounterRequest request;
    unsigned int seat;

    memset(&request, 0, sizeof(request));
    request.transition_serial = 7u;
    request.world_generation = 10u;
    request.source_generation = 20u;
    request.host_actor = (uintptr_t)0x12340000u;
    request.host_actor_generation = 30u;
    request.host_lease_generation = 31u;
    for (seat = 0u; seat < SUDEKIMP_TALOS_HERO_COUNT; ++seat) {
        request.hero_actor_identity[seat] =
            (uintptr_t)(0x12340000u + seat * 0x100u);
        request.hero_actor_generation[seat] = 30u + seat;
    }
    request.active_human_mask = mask;
    for (seat = 0u; seat < SUDEKIMP_TALOS_SEAT_COUNT; ++seat) {
        request.controller_slot_by_seat[seat] =
            SUDEKIMP_TALOS_CONTROLLER_NONE;
        if ((mask & (uint8_t)(1u << seat)) != 0u) {
            request.input_identity_by_seat[seat] =
                (uintptr_t)(0x5000u + seat * 0x100u);
            request.input_generation_by_seat[seat] = 100u + seat;
            if (seat != 0u) {
                request.controller_slot_by_seat[seat] =
                    (uint8_t)(seat - 1u);
            }
        }
    }
    request.hero_by_seat[0] = SUDEKIMP_TALOS_HERO_TAL;
    request.hero_by_seat[1] = (mask & 0x02u) != 0u ?
        SUDEKIMP_TALOS_HERO_AILISH : SUDEKIMP_TALOS_HERO_NONE;
    request.hero_by_seat[2] = (mask & 0x04u) != 0u ?
        SUDEKIMP_TALOS_HERO_BUKI : SUDEKIMP_TALOS_HERO_NONE;
    request.hero_by_seat[3] = (mask & 0x08u) != 0u ?
        SUDEKIMP_TALOS_HERO_ELCO : SUDEKIMP_TALOS_HERO_NONE;
    request.combatant_count = SUDEKIMP_TALOS_EXPANDED_COMBATANTS;
    return request;
}

static void test_health_policy(void) {
    CHECK(SudekiMpTalosEncounterHealthTarget(0u) == 0u);
    CHECK(SudekiMpTalosEncounterHealthTarget(1u) == 45000u);
    CHECK(SudekiMpTalosEncounterHealthTarget(2u) == 90000u);
    CHECK(SudekiMpTalosEncounterHealthTarget(3u) == 135000u);
    CHECK(SudekiMpTalosEncounterHealthTarget(4u) == 180000u);
    CHECK(SudekiMpTalosEncounterHealthTarget(5u) == 0u);
    CHECK(SudekiMpTalosEncounterHumanCount(0x01u) == 1u);
    CHECK(SudekiMpTalosEncounterHumanCount(0x09u) == 2u);
    CHECK(SudekiMpTalosEncounterHumanCount(0x0fu) == 4u);
}

static void test_prompt_copies_immutable_provenance(void) {
    SudekiMpTalosEncounterSession session;
    SudekiMpTalosEncounterRequest request = request_for_mask(0x03u);
    SudekiMpTalosEncounterProvenance provenance;

    SudekiMpTalosEncounterInitialize(&session);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
    request.world_generation = 99u;
    request.hero_by_seat[1] = SUDEKIMP_TALOS_HERO_ELCO;
    CHECK(SudekiMpTalosEncounterGetProvenance(&session, &provenance));
    CHECK(provenance.serial != 0u);
    CHECK(provenance.transition_serial == 7u);
    CHECK(provenance.world_generation == 10u);
    CHECK(provenance.source_generation == 20u);
    CHECK(provenance.host_actor == (uintptr_t)0x12340000u);
    CHECK(provenance.host_actor_generation == 30u);
    CHECK(provenance.host_lease_generation == 31u);
    CHECK(provenance.hero_actor_identity[SUDEKIMP_TALOS_HERO_TAL] ==
        (uintptr_t)0x12340000u);
    CHECK(provenance.hero_actor_identity[SUDEKIMP_TALOS_HERO_ELCO] ==
        (uintptr_t)0x12340300u);
    CHECK(provenance.hero_actor_generation[SUDEKIMP_TALOS_HERO_BUKI] == 32u);
    CHECK(provenance.active_human_mask == 0x03u);
    CHECK(provenance.hero_by_seat[0] == SUDEKIMP_TALOS_HERO_TAL);
    CHECK(provenance.hero_by_seat[1] == SUDEKIMP_TALOS_HERO_AILISH);
    CHECK(provenance.controller_slot_by_seat[1] == 0u);
    CHECK(provenance.input_identity_by_seat[1] == (uintptr_t)0x5100u);
    CHECK(provenance.input_generation_by_seat[1] == 101u);
    CHECK(provenance.combatant_count == 4u);
    CHECK(provenance.talos_health_target == 180000u);
    CHECK(SudekiMpTalosEncounterSeatForHero(
        &provenance, SUDEKIMP_TALOS_HERO_TAL) == 0u);
    CHECK(SudekiMpTalosEncounterSeatForHero(
        &provenance, SUDEKIMP_TALOS_HERO_AILISH) == 1u);
    CHECK(SudekiMpTalosEncounterSeatForHero(
        &provenance, SUDEKIMP_TALOS_HERO_BUKI) ==
        SUDEKIMP_TALOS_SEAT_AI);
}

static void test_assignment_validation(void) {
    SudekiMpTalosEncounterSession session;
    SudekiMpTalosEncounterRequest request;

    SudekiMpTalosEncounterInitialize(&session);

    request = request_for_mask(0x03u);
    request.hero_by_seat[0] = SUDEKIMP_TALOS_HERO_AILISH;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x03u);
    request.hero_by_seat[1] = SUDEKIMP_TALOS_HERO_TAL;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x07u);
    request.hero_by_seat[2] = SUDEKIMP_TALOS_HERO_AILISH;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x01u);
    request.hero_by_seat[2] = SUDEKIMP_TALOS_HERO_BUKI;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x01u);
    request.combatant_count = 3u;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x01u);
    request.world_generation = 0u;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x07u);
    request.controller_slot_by_seat[2] =
        request.controller_slot_by_seat[1];
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x03u);
    request.input_generation_by_seat[1] = 0u;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x03u);
    request.input_identity_by_seat[1] = request.input_identity_by_seat[0];
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x03u);
    request.hero_actor_identity[SUDEKIMP_TALOS_HERO_ELCO] =
        request.hero_actor_identity[SUDEKIMP_TALOS_HERO_BUKI];
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x03u);
    request.hero_actor_generation[SUDEKIMP_TALOS_HERO_AILISH] = 0u;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);

    request = request_for_mask(0x10u);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_INVALID);
    CHECK(session.state == SUDEKIMP_TALOS_ENCOUNTER_IDLE);

    request = request_for_mask(0x05u);
    request.hero_by_seat[2] = SUDEKIMP_TALOS_HERO_ELCO;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
}

static void test_confirm_claim_and_replay(void) {
    SudekiMpTalosEncounterSession session;
    SudekiMpTalosEncounterRequest request = request_for_mask(0x0fu);
    SudekiMpTalosEncounterProvenance provenance;
    uint32_t serial;

    SudekiMpTalosEncounterInitialize(&session);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
    CHECK(SudekiMpTalosEncounterGetProvenance(&session, &provenance));
    serial = provenance.serial;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY);
    CHECK(SudekiMpTalosEncounterConfirm(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_CONFIRM_ACCEPTED);
    CHECK(SudekiMpTalosEncounterClaimTransition(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_CLAIM_ACCEPTED);
    CHECK(SudekiMpTalosEncounterClaimTransition(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY);
    CHECK(SudekiMpTalosEncounterFinishTransition(&session, serial, 1) ==
        SUDEKIMP_TALOS_ENCOUNTER_FINISHED);
    CHECK(session.state == SUDEKIMP_TALOS_ENCOUNTER_IDLE);
    CHECK(!SudekiMpTalosEncounterGetProvenance(&session, &provenance));

    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY);
    ++request.transition_serial;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
    CHECK(SudekiMpTalosEncounterGetProvenance(&session, &provenance));
    CHECK(provenance.serial != serial);
    provenance.serial = serial;
    CHECK(SudekiMpTalosEncounterConfirm(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE);
    CHECK(session.state == SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPEN);
}

static void test_cancel_is_host_prompt_only(void) {
    SudekiMpTalosEncounterSession session;
    SudekiMpTalosEncounterRequest request = request_for_mask(0x01u);
    SudekiMpTalosEncounterProvenance provenance;
    uint32_t serial;

    SudekiMpTalosEncounterInitialize(&session);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
    CHECK(SudekiMpTalosEncounterGetProvenance(&session, &provenance));
    serial = provenance.serial;
    CHECK(SudekiMpTalosEncounterCancel(&session, serial + 1u) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE);
    CHECK(SudekiMpTalosEncounterCancel(&session, serial) ==
        SUDEKIMP_TALOS_ENCOUNTER_CANCEL_ACCEPTED);
    CHECK(SudekiMpTalosEncounterConfirm(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_BUSY);
    CHECK(SudekiMpTalosEncounterDismissCancelled(&session, serial));
    CHECK(session.state == SUDEKIMP_TALOS_ENCOUNTER_IDLE);
    CHECK(!SudekiMpTalosEncounterDismissCancelled(&session, serial));
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_REPLAY);
    ++request.transition_serial;
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
}

static void test_mismatch_quarantines_and_requires_new_generation(void) {
    SudekiMpTalosEncounterSession session;
    SudekiMpTalosEncounterRequest request = request_for_mask(0x03u);
    SudekiMpTalosEncounterProvenance provenance;

    SudekiMpTalosEncounterInitialize(&session);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
    CHECK(SudekiMpTalosEncounterGetProvenance(&session, &provenance));
    provenance.source_generation++;
    CHECK(SudekiMpTalosEncounterConfirm(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_MISMATCH);
    CHECK(session.state == SUDEKIMP_TALOS_ENCOUNTER_QUARANTINED);
    CHECK(session.quarantine_reason ==
        SUDEKIMP_TALOS_QUARANTINE_PROVENANCE_MISMATCH);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED);
    CHECK(SudekiMpTalosEncounterRecover(&session, 10u, 20u) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_STALE);
    CHECK(SudekiMpTalosEncounterRecover(&session, 11u, 20u) ==
        SUDEKIMP_TALOS_ENCOUNTER_RECOVERED);
    CHECK(session.state == SUDEKIMP_TALOS_ENCOUNTER_IDLE);
}

static void test_failed_native_transition_quarantines(void) {
    SudekiMpTalosEncounterSession session;
    SudekiMpTalosEncounterRequest request = request_for_mask(0x01u);
    SudekiMpTalosEncounterProvenance provenance;

    SudekiMpTalosEncounterInitialize(&session);
    CHECK(SudekiMpTalosEncounterOpenPrompt(&session, &request) ==
        SUDEKIMP_TALOS_ENCOUNTER_PROMPT_OPENED);
    CHECK(SudekiMpTalosEncounterGetProvenance(&session, &provenance));
    CHECK(SudekiMpTalosEncounterConfirm(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_CONFIRM_ACCEPTED);
    CHECK(SudekiMpTalosEncounterClaimTransition(&session, &provenance) ==
        SUDEKIMP_TALOS_ENCOUNTER_CLAIM_ACCEPTED);
    CHECK(SudekiMpTalosEncounterFinishTransition(
        &session, provenance.serial, 0) ==
        SUDEKIMP_TALOS_ENCOUNTER_REJECTED_QUARANTINED);
    CHECK(session.quarantine_reason ==
        SUDEKIMP_TALOS_QUARANTINE_TRANSITION_FAILED);
}

int main(void) {
    test_health_policy();
    test_prompt_copies_immutable_provenance();
    test_assignment_validation();
    test_confirm_claim_and_replay();
    test_cancel_is_host_prompt_only();
    test_mismatch_quarantines_and_requires_new_generation();
    test_failed_native_transition_quarantines();

    if (failures != 0) {
        fprintf(stderr, "talos encounter session checks failed: %d\n",
            failures);
        return 1;
    }
    puts("talos encounter session checks passed");
    return 0;
}
