#include "engine/player_statehood.h"

#include <stdio.h>

static int failures;

static void require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void) {
    SudekiMpPlayerStatehood statehood;
    SudekiMpPlayerStatehoodSnapshot snapshot;
    uint32_t serial = 0u;

    SudekiMpPlayerStatehoodInitialize(&statehood);
    require_true(
        SudekiMpInteractionAuthorityForKind(SUDEKIMP_INTERACTION_SHOP) ==
            SUDEKIMP_INTERACTION_AUTHORITY_SERIALIZED_SHARED &&
        SudekiMpInteractionAuthorityForKind(SUDEKIMP_INTERACTION_TRAVEL) ==
            SUDEKIMP_INTERACTION_AUTHORITY_HOST_ONLY &&
        SudekiMpInteractionPresentationForKind(
            SUDEKIMP_INTERACTION_BLACKSMITH, 0) ==
            SUDEKIMP_INTERACTION_PRESENTATION_SHARED_FULL_WIDTH &&
        SudekiMpInteractionPresentationForKind(
            SUDEKIMP_INTERACTION_BLACKSMITH, 1) ==
            SUDEKIMP_INTERACTION_PRESENTATION_OWNER_VIEWPORT,
        "authority/presentation policy mismatch");

    require_true(
        SudekiMpPlayerStatehoodPublishPlayer(
            &statehood, 0u, 0x1000u, 7u, 1) &&
        SudekiMpPlayerStatehoodPublishPlayer(
            &statehood, 1u, 0x2000u, 7u, 1) &&
        SudekiMpPlayerStatehoodPublishPlayer(
            &statehood, 2u, 0x3000u, 7u, 1) &&
        SudekiMpPlayerStatehoodPublishPlayer(
            &statehood, 3u, 0x4000u, 7u, 1),
        "four-player leases were not accepted");

    require_true(
        SudekiMpPlayerStatehoodRequest(
            &statehood, 1u, 0x2000u, 7u,
            SUDEKIMP_INTERACTION_GENERIC_REQUEST,
            0u, 0, 11u, 100u, &serial),
        "P2 generic interaction request rejected");
    require_true(serial != 0u &&
        SudekiMpPlayerStatehoodGetSnapshot(&statehood, 1100u, &snapshot) &&
        snapshot.state == SUDEKIMP_INTERACTION_SESSION_REQUESTED &&
        snapshot.provenance.player_index == 1u &&
        snapshot.remaining_ms == 4000u,
        "P2 request provenance/countdown mismatch");
    require_true(!SudekiMpPlayerStatehoodCommitKnownRequest(
        &statehood, serial, 0x2000u, 7u, 0x9000u, 11u),
        "targetless attention request became an authoritative action");
    SudekiMpPlayerStatehoodService(&statehood, 5100u);
    require_true(!SudekiMpPlayerStatehoodGetSnapshot(
        &statehood, 5100u, &snapshot),
        "expired request remained active");

    require_true(SudekiMpPlayerStatehoodRequest(
        &statehood, 1u, 0x2000u, 7u,
        SUDEKIMP_INTERACTION_GENERIC_REQUEST,
        0u, 0, 0u, 5200u, &serial),
        "second P2 targetless request was rejected");
    require_true(!SudekiMpPlayerStatehoodCancelRequest(&statehood, 0u),
        "host cancelled a P2 acknowledgement request");
    require_true(SudekiMpPlayerStatehoodCancelRequest(&statehood, 1u) &&
        !SudekiMpPlayerStatehoodGetSnapshot(
            &statehood, 5201u, &snapshot),
        "P2 acknowledgement request did not cancel cleanly");

    require_true(!SudekiMpPlayerStatehoodRequest(
        &statehood, 1u, 0x2000u, 7u,
        SUDEKIMP_INTERACTION_TRAVEL,
        0x9000u, 1, 11u, 6000u, &serial),
        "non-host player acquired host-only travel authority");
    require_true(SudekiMpPlayerStatehoodRequest(
        &statehood, 1u, 0x2000u, 7u,
        SUDEKIMP_INTERACTION_SHOP,
        0x9000u, 1, 11u, 6000u, &serial),
        "known P2 serialized shop request rejected");
    require_true(!SudekiMpPlayerStatehoodCommitKnownRequest(
        &statehood, serial, 0x2000u, 7u, 0x9000u, 12u),
        "stale world generation committed a shop request");
    require_true(SudekiMpPlayerStatehoodCommitKnownRequest(
        &statehood, serial, 0x2000u, 7u, 0x9000u, 11u),
        "exact serialized shop request did not commit");
    require_true(!SudekiMpPlayerStatehoodRequest(
        &statehood, 2u, 0x3000u, 7u,
        SUDEKIMP_INTERACTION_SHOP,
        0x9100u, 1, 11u, 6100u, NULL),
        "second player entered an occupied global shop session");

    require_true(SudekiMpPlayerStatehoodPublishPlayer(
        &statehood, 1u, 0u, 8u, 0),
        "P2 dropout publication failed");
    require_true(SudekiMpPlayerStatehoodGetSnapshot(
        &statehood, 6200u, &snapshot) &&
        snapshot.state == SUDEKIMP_INTERACTION_SESSION_QUARANTINED,
        "active owner dropout forgot a live global UI session");
    SudekiMpPlayerStatehoodObserveNativeModal(
        &statehood, SUDEKIMP_INTERACTION_NONE, 0u, 6300u);
    require_true(!SudekiMpPlayerStatehoodGetSnapshot(
        &statehood, 6300u, &snapshot),
        "native modal close did not clear quarantine");

    SudekiMpPlayerStatehoodObserveNativeModal(
        &statehood, SUDEKIMP_INTERACTION_BLACKSMITH, 0u, 7000u);
    require_true(SudekiMpPlayerStatehoodGetSnapshot(
        &statehood, 7000u, &snapshot) &&
        snapshot.state == SUDEKIMP_INTERACTION_SESSION_ACTIVE &&
        snapshot.provenance.kind == SUDEKIMP_INTERACTION_BLACKSMITH &&
        snapshot.provenance.player_index == 0u,
        "observed native blacksmith session lacked host provenance");
    SudekiMpPlayerStatehoodObserveNativeModal(
        &statehood, SUDEKIMP_INTERACTION_NONE, 0u, 7100u);

    if (failures != 0) {
        fprintf(stderr, "player_statehood checks failed: %d\n", failures);
        return 1;
    }
    puts("player_statehood checks passed");
    return 0;
}
