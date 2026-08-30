#include "engine/talos_companion_staging.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpTalosEncounterProvenance provenance(void) {
    SudekiMpTalosEncounterProvenance value;
    unsigned int i;

    memset(&value, 0, sizeof(value));
    value.serial = 101u;
    value.transition_serial = 201u;
    value.world_generation = 301u;
    value.source_generation = 401u;
    value.host_actor = (uintptr_t)0x1000u;
    value.host_actor_generation = 501u;
    value.host_lease_generation = 601u;
    value.talos_health_target = 180000u;
    value.combatant_count = 4u;
    value.active_human_mask = 0x03u;
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        value.hero_actor_identity[i] = (uintptr_t)(0x1000u + i * 0x100u);
        value.hero_actor_generation[i] = 501u + i;
        value.hero_by_seat[i] = SUDEKIMP_TALOS_HERO_NONE;
        value.controller_slot_by_seat[i] = SUDEKIMP_TALOS_CONTROLLER_NONE;
    }
    value.hero_by_seat[0] = SUDEKIMP_TALOS_HERO_TAL;
    value.hero_by_seat[1] = SUDEKIMP_TALOS_HERO_AILISH;
    value.controller_slot_by_seat[1] = 0u;
    value.input_identity_by_seat[0] = (uintptr_t)0x8000u;
    value.input_identity_by_seat[1] = (uintptr_t)0x8100u;
    value.input_generation_by_seat[0] = 701u;
    value.input_generation_by_seat[1] = 702u;
    return value;
}

static SudekiMpTalosStagingSnapshot preflight(
    const SudekiMpTalosEncounterProvenance *provenance_value
) {
    static const uint8_t group[4] = {
        SUDEKIMP_TALOS_HERO_TAL, SUDEKIMP_TALOS_HERO_AILISH,
        SUDEKIMP_TALOS_HERO_BUKI, SUDEKIMP_TALOS_HERO_ELCO
    };
    static const uint8_t formation[4] = {
        SUDEKIMP_TALOS_HERO_TAL, SUDEKIMP_TALOS_HERO_ELCO,
        SUDEKIMP_TALOS_HERO_AILISH, SUDEKIMP_TALOS_HERO_BUKI
    };
    SudekiMpTalosStagingSnapshot value;
    unsigned int i;

    memset(&value, 0, sizeof(value));
    value.observation_serial = 1001u;
    value.continuity_fingerprint = UINT64_C(0x123456789abcdef0);
    value.world_generation = provenance_value->world_generation;
    value.source_generation = provenance_value->source_generation;
    value.runtime_generation = 801u;
    value.native_thread_identity = (uintptr_t)0x9000u;
    value.group_identity = (uintptr_t)0xa000u;
    value.group_generation = 901u;
    value.formation_owner_identity = (uintptr_t)0xa100u;
    value.formation_owner_generation = 902u;
    value.formation_identity = (uintptr_t)0xa200u;
    value.formation_generation = 903u;
    value.tal_controller_identity = (uintptr_t)0xb000u;
    value.tal_controller_generation = 1001u;
    value.front_actor_identity = provenance_value->host_actor;
    value.front_actor_generation = provenance_value->host_actor_generation;
    value.camera_identity = (uintptr_t)0xb100u;
    value.camera_generation = 1002u;
    value.camera_target_actor_identity = provenance_value->host_actor;
    value.camera_target_actor_generation =
        provenance_value->host_actor_generation;
    value.ownership_identity = (uintptr_t)0xb200u;
    value.ownership_generation = 1003u;
    value.group_count = 4u;
    value.formation_count = 4u;
    value.exact_executable = 1u;
    value.exact_asset = 1u;
    value.ownership_frozen = 1u;
    memcpy(value.group_order, group, sizeof(group));
    memcpy(value.formation_order, formation, sizeof(formation));
    for (i = 0u; i < SUDEKIMP_TALOS_HERO_COUNT; ++i) {
        int human = i < 2u;
        if (i == SUDEKIMP_TALOS_HERO_ELCO) {
            value.hero[i].wrapper_identity = (uintptr_t)0xc300u;
            value.hero[i].wrapper_generation = 1104u;
        }
        value.hero[i].actor_identity = provenance_value->hero_actor_identity[i];
        value.hero[i].actor_generation = provenance_value->hero_actor_generation[i];
        value.hero[i].formation_backpointer_identity = value.formation_identity;
        value.hero[i].formation_backpointer_generation =
            value.formation_generation;
        value.hero[i].control_owner_identity =
            (uintptr_t)(0xe000u + i * 0x100u);
        value.hero[i].control_owner_generation = 1301u + i;
        value.hero[i].native_ai_enabled = (uint8_t)(human ? 0u : 1u);
        value.hero[i].human_control_owned = (uint8_t)(human ? 1u : 0u);
        value.hero[i].override_active = (uint8_t)(human &&
            i != SUDEKIMP_TALOS_HERO_TAL ? 1u : 0u);
        value.hero[i].control_mode = (uint8_t)(human ?
            SUDEKIMP_TALOS_STAGING_CONTROL_HUMAN :
            SUDEKIMP_TALOS_STAGING_CONTROL_NATIVE_AI);
    }
    return value;
}

static SudekiMpTalosStagingSnapshot later_full(
    const SudekiMpTalosStagingSnapshot *original,
    uint64_t serial
) {
    SudekiMpTalosStagingSnapshot value = *original;
    value.observation_serial = serial;
    return value;
}

static SudekiMpTalosStagingSnapshot detached(
    const SudekiMpTalosStagingSnapshot *original,
    uint64_t serial
) {
    SudekiMpTalosStagingSnapshot value = *original;

    value.observation_serial = serial;
    value.group_count = 3u;
    value.group_order[0] = SUDEKIMP_TALOS_HERO_TAL;
    value.group_order[1] = SUDEKIMP_TALOS_HERO_AILISH;
    value.group_order[2] = SUDEKIMP_TALOS_HERO_BUKI;
    value.group_order[3] = SUDEKIMP_TALOS_STAGING_MEMBER_NONE;
    value.formation_count = 3u;
    value.formation_order[0] = SUDEKIMP_TALOS_HERO_BUKI;
    value.formation_order[1] = SUDEKIMP_TALOS_HERO_TAL;
    value.formation_order[2] = SUDEKIMP_TALOS_HERO_AILISH;
    value.formation_order[3] = SUDEKIMP_TALOS_STAGING_MEMBER_NONE;
    value.hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_identity = 0u;
    value.hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_generation = 0u;
    return value;
}

static void initialize_enabled(SudekiMpTalosCompanionStaging *staging) {
    SudekiMpTalosCompanionStagingInitialize(staging);
    CHECK(staging->state == SUDEKIMP_TALOS_STAGING_DISABLED);
    SudekiMpTalosCompanionStagingConfigure(staging, 1);
    CHECK(staging->state == SUDEKIMP_TALOS_STAGING_IDLE);
}

static void begin_and_claim_remove(
    SudekiMpTalosCompanionStaging *staging,
    SudekiMpTalosEncounterProvenance *provenance_value,
    SudekiMpTalosStagingSnapshot *original,
    SudekiMpTalosStagingTicket *ticket
) {
    SudekiMpTalosStagingSnapshot immediate;

    *provenance_value = provenance();
    *original = preflight(provenance_value);
    initialize_enabled(staging);
    CHECK(SudekiMpTalosCompanionStagingBegin(staging, 1u,
        provenance_value, original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    immediate = later_full(original, original->observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(staging,
        provenance_value, &immediate, ticket) ==
        SUDEKIMP_TALOS_STAGING_REMOVE_AUTHORIZED);
    CHECK(ticket->native_function_rva ==
        SUDEKIMP_TALOS_STAGING_REMOVE_PLAYER_RVA);
    CHECK(ticket->hero == SUDEKIMP_TALOS_HERO_ELCO);
}

static void reach_detached(
    SudekiMpTalosCompanionStaging *staging,
    SudekiMpTalosEncounterProvenance *provenance_value,
    SudekiMpTalosStagingSnapshot *original,
    SudekiMpTalosStagingSnapshot *detached_value
) {
    SudekiMpTalosStagingTicket ticket;

    begin_and_claim_remove(staging, provenance_value, original, &ticket);
    *detached_value = detached(original, ticket.authorized_observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(staging, &ticket,
        provenance_value, detached_value, 1u) ==
        SUDEKIMP_TALOS_STAGING_DETACH_PROVEN);
}

static void test_default_off_and_happy_path(void) {
    SudekiMpTalosCompanionStaging staging;
    SudekiMpTalosEncounterProvenance p = provenance();
    SudekiMpTalosStagingSnapshot original = preflight(&p);
    SudekiMpTalosStagingSnapshot immediate;
    SudekiMpTalosStagingSnapshot detached_value;
    SudekiMpTalosStagingSnapshot restored;
    SudekiMpTalosStagingSnapshot stable;
    SudekiMpTalosStagingTicket remove_ticket;
    SudekiMpTalosStagingTicket restore_ticket;

    SudekiMpTalosCompanionStagingInitialize(&staging);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_DISABLED_RESULT);
    SudekiMpTalosCompanionStagingConfigure(&staging, 1);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, 1002u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &p,
        &immediate, &remove_ticket) ==
        SUDEKIMP_TALOS_STAGING_REMOVE_AUTHORIZED);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &p,
        &immediate, &remove_ticket) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
    detached_value = detached(&original, 1003u);
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &remove_ticket,
        &p, &detached_value, 1u) == SUDEKIMP_TALOS_STAGING_DETACH_PROVEN);
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &remove_ticket,
        &p, &detached_value, 1u) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
    immediate = detached(&original, 1004u);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &restore_ticket) ==
        SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED);
    CHECK(restore_ticket.native_function_rva ==
        SUDEKIMP_TALOS_STAGING_ADD_PLAYER_RVA);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &restore_ticket) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
    restored = later_full(&original, 1005u);
    CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &restore_ticket,
        &p, &restored, 1u) == SUDEKIMP_TALOS_STAGING_RESTORE_PROVEN);
    CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &restore_ticket,
        &p, &restored, 1u) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
    CHECK(SudekiMpTalosCompanionStagingRelease(&staging) ==
        SUDEKIMP_TALOS_STAGING_REJECTED_STATE);
    stable = later_full(&original, 1006u);
    CHECK(SudekiMpTalosCompanionStagingObserveStability(&staging, &p,
        &stable) == SUDEKIMP_TALOS_STAGING_STABILITY_ACCEPTED);
    CHECK(SudekiMpTalosCompanionStagingRelease(&staging) ==
        SUDEKIMP_TALOS_STAGING_RELEASE_ACCEPTED);
    CHECK(SudekiMpTalosCompanionStagingRelease(&staging) ==
        SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
}

static void test_preflight_rejections_are_safe(void) {
    SudekiMpTalosCompanionStaging staging;
    SudekiMpTalosEncounterProvenance p = provenance();
    SudekiMpTalosStagingSnapshot good = preflight(&p);
    SudekiMpTalosStagingSnapshot bad;

#define SAFE_BAD(statement) do { \
    bad = good; \
    statement; \
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &bad) == \
        SUDEKIMP_TALOS_STAGING_SAFE_ABORT); \
    CHECK(staging.state == SUDEKIMP_TALOS_STAGING_IDLE); \
} while (0)

    initialize_enabled(&staging);
    SAFE_BAD(bad.world_generation++);
    SAFE_BAD(bad.source_generation++);
    SAFE_BAD(bad.runtime_generation = 0u);
    SAFE_BAD(bad.native_thread_identity = 0u);
    SAFE_BAD(bad.group_order[2] = SUDEKIMP_TALOS_HERO_ELCO);
    SAFE_BAD(bad.group_order[3] = SUDEKIMP_TALOS_HERO_BUKI);
    SAFE_BAD(bad.formation_order[0] = SUDEKIMP_TALOS_HERO_ELCO);
    SAFE_BAD(bad.hero[3].wrapper_identity = 0u);
    SAFE_BAD(bad.hero[3].actor_identity++);
    SAFE_BAD(bad.hero[3].actor_generation++);
    SAFE_BAD(bad.hero[3].formation_backpointer_identity++);
    SAFE_BAD(bad.hero[3].control_owner_identity = 0u);
    SAFE_BAD(bad.hero[3].control_owner_identity =
        bad.hero[2].control_owner_identity);
    SAFE_BAD(bad.hero[3].native_ai_enabled = 0u);
    SAFE_BAD(bad.hero[3].override_active = 1u);
    SAFE_BAD(bad.hero[1].override_active = 0u);
    SAFE_BAD(bad.tal_controller_identity = 0u);
    SAFE_BAD(bad.front_actor_identity++);
    SAFE_BAD(bad.camera_identity = 0u);
    SAFE_BAD(bad.camera_target_actor_identity++);
    SAFE_BAD(bad.in_combat = 1u);
    SAFE_BAD(bad.tsa_active = 1u);
    SAFE_BAD(bad.transition_active = 1u);
    SAFE_BAD(bad.modal_active = 1u);
    SAFE_BAD(bad.ownership_frozen = 0u);
    SAFE_BAD(bad.continuity_fingerprint = 0u);
#undef SAFE_BAD
}

static void test_pre_remove_abort_and_zero_call(void) {
    SudekiMpTalosCompanionStaging staging;
    SudekiMpTalosEncounterProvenance p;
    SudekiMpTalosEncounterProvenance stale;
    SudekiMpTalosStagingSnapshot original;
    SudekiMpTalosStagingSnapshot immediate;
    SudekiMpTalosStagingTicket ticket;

    p = provenance();
    original = preflight(&p);
    initialize_enabled(&staging);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    stale = p;
    stale.source_generation++;
    immediate = later_full(&original, 1002u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &stale,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_SAFE_ABORT);
    CHECK(staging.state == SUDEKIMP_TALOS_STAGING_RELEASED);

    begin_and_claim_remove(&staging, &p, &original, &ticket);
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &ticket,
        NULL, NULL, 0u) == SUDEKIMP_TALOS_STAGING_SAFE_ABORT);
    CHECK(staging.state == SUDEKIMP_TALOS_STAGING_RELEASED);
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &ticket,
        &p, NULL, 0u) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
}

typedef void (*SnapshotMutation)(SudekiMpTalosStagingSnapshot *value);

static void stale_world(SudekiMpTalosStagingSnapshot *v) { ++v->world_generation; }
static void stale_source(SudekiMpTalosStagingSnapshot *v) { ++v->source_generation; }
static void stale_runtime(SudekiMpTalosStagingSnapshot *v) { ++v->runtime_generation; }
static void partial_3_4(SudekiMpTalosStagingSnapshot *v) { v->formation_count = 4u; }
static void partial_4_3(SudekiMpTalosStagingSnapshot *v) { v->group_count = 4u; }
static void wrong_group_order(SudekiMpTalosStagingSnapshot *v) {
    v->group_order[1] = SUDEKIMP_TALOS_HERO_BUKI;
    v->group_order[2] = SUDEKIMP_TALOS_HERO_AILISH;
}
static void wrong_formation_set(SudekiMpTalosStagingSnapshot *v) {
    v->formation_order[2] = SUDEKIMP_TALOS_HERO_ELCO;
}
static void wrapper_change(SudekiMpTalosStagingSnapshot *v) {
    ++v->hero[SUDEKIMP_TALOS_HERO_ELCO].wrapper_identity;
}
static void actor_change(SudekiMpTalosStagingSnapshot *v) {
    ++v->hero[SUDEKIMP_TALOS_HERO_ELCO].actor_identity;
}
static void control_change(SudekiMpTalosStagingSnapshot *v) {
    ++v->hero[SUDEKIMP_TALOS_HERO_ELCO].control_owner_generation;
}
static void camera_change(SudekiMpTalosStagingSnapshot *v) {
    ++v->camera_generation;
}
static void formation_backpointer_not_cleared(
    SudekiMpTalosStagingSnapshot *v
) {
    v->hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_identity =
        (uintptr_t)0xd300u;
    v->hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_generation = 1204u;
}

static void test_remove_completion_fail_closed(void) {
    static const SnapshotMutation mutations[] = {
        stale_world, stale_source, stale_runtime, partial_3_4, partial_4_3,
        wrong_group_order, wrong_formation_set, wrapper_change, actor_change,
        control_change, camera_change, formation_backpointer_not_cleared
    };
    unsigned int i;

    for (i = 0u; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        SudekiMpTalosCompanionStaging staging;
        SudekiMpTalosEncounterProvenance p;
        SudekiMpTalosStagingSnapshot original;
        SudekiMpTalosStagingSnapshot completion;
        SudekiMpTalosStagingTicket ticket;

        begin_and_claim_remove(&staging, &p, &original, &ticket);
        completion = detached(&original,
            ticket.authorized_observation_serial + 1u);
        mutations[i](&completion);
        CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &ticket,
            &p, &completion, 1u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);
        CHECK(staging.state == SUDEKIMP_TALOS_STAGING_QUARANTINED);
    }
    {
        SudekiMpTalosCompanionStaging staging;
        SudekiMpTalosEncounterProvenance p;
        SudekiMpTalosStagingSnapshot original;
        SudekiMpTalosStagingSnapshot completion;
        SudekiMpTalosStagingTicket ticket;

        begin_and_claim_remove(&staging, &p, &original, &ticket);
        completion = detached(&original,
            ticket.authorized_observation_serial + 1u);
        CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &ticket,
            &p, &completion, 2u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);
    }
}

static void test_restore_and_stability_fail_closed(void) {
    SudekiMpTalosCompanionStaging staging;
    SudekiMpTalosEncounterProvenance p;
    SudekiMpTalosStagingSnapshot original;
    SudekiMpTalosStagingSnapshot detached_value;
    SudekiMpTalosStagingSnapshot immediate;
    SudekiMpTalosStagingSnapshot completion;
    SudekiMpTalosStagingTicket ticket;

    reach_detached(&staging, &p, &original, &detached_value);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED);
    completion = later_full(&original, immediate.observation_serial + 1u);
    completion.formation_order[1] = SUDEKIMP_TALOS_HERO_AILISH;
    completion.formation_order[2] = SUDEKIMP_TALOS_HERO_ELCO;
    CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &ticket,
        &p, &completion, 1u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);

    reach_detached(&staging, &p, &original, &detached_value);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED);
    completion = later_full(&original, immediate.observation_serial + 1u);
    completion.hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_identity =
        0u;
    completion.hero[SUDEKIMP_TALOS_HERO_ELCO].formation_backpointer_generation =
        0u;
    CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &ticket,
        &p, &completion, 1u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);

    reach_detached(&staging, &p, &original, &detached_value);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED);
    completion = later_full(&original, immediate.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &ticket,
        &p, &completion, 0u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);

    reach_detached(&staging, &p, &original, &detached_value);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED);
    completion = later_full(&original, immediate.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &ticket,
        &p, &completion, 1u) == SUDEKIMP_TALOS_STAGING_RESTORE_PROVEN);
    completion.observation_serial++;
    completion.camera_generation++;
    CHECK(SudekiMpTalosCompanionStagingObserveStability(&staging, &p,
        &completion) == SUDEKIMP_TALOS_STAGING_QUARANTINE);
}

static void test_ticket_tamper_and_quarantine_persistence(void) {
    SudekiMpTalosCompanionStaging staging;
    SudekiMpTalosEncounterProvenance p;
    SudekiMpTalosStagingSnapshot original;
    SudekiMpTalosStagingSnapshot completion;
    SudekiMpTalosStagingTicket ticket;

    begin_and_claim_remove(&staging, &p, &original, &ticket);
    completion = detached(&original,
        ticket.authorized_observation_serial + 1u);
    ++ticket.actor_identity;
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &ticket,
        &p, &completion, 1u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);
    SudekiMpTalosCompanionStagingConfigure(&staging, 0);
    CHECK(staging.state == SUDEKIMP_TALOS_STAGING_QUARANTINED);
    CHECK(staging.enabled == 1u);
    SudekiMpTalosCompanionStagingConfigure(&staging, 1);
    CHECK(staging.state == SUDEKIMP_TALOS_STAGING_QUARANTINED);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 2u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_QUARANTINE);

    {
        SudekiMpTalosStagingSnapshot detached_value;
        SudekiMpTalosStagingSnapshot immediate;

        reach_detached(&staging, &p, &original, &detached_value);
        immediate = detached(&original,
            detached_value.observation_serial + 1u);
        CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
            &immediate, &ticket) ==
            SUDEKIMP_TALOS_STAGING_RESTORE_AUTHORIZED);
        completion = later_full(&original,
            immediate.observation_serial + 1u);
        ticket.native_function_rva =
            SUDEKIMP_TALOS_STAGING_REMOVE_PLAYER_RVA;
        CHECK(SudekiMpTalosCompanionStagingFinishRestore(&staging, &ticket,
            &p, &completion, 1u) == SUDEKIMP_TALOS_STAGING_QUARANTINE);
    }
}

static void test_replay_and_wrap_fences(void) {
    SudekiMpTalosCompanionStaging staging;
    SudekiMpTalosEncounterProvenance p = provenance();
    SudekiMpTalosStagingSnapshot original = preflight(&p);
    SudekiMpTalosStagingSnapshot immediate;
    SudekiMpTalosStagingSnapshot detached_value;
    SudekiMpTalosStagingTicket ticket;

    initialize_enabled(&staging);
    staging.highest_terminal_operation_serial = UINT64_MAX;
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, UINT64_MAX,
        &p, &original) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 0u,
        &p, &original) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u,
        &p, &original) == SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);

    initialize_enabled(&staging);
    staging.highest_terminal_operation_serial = 10u;
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging,
        UINT64_C(0x800000000000000a), &p, &original) ==
        SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);

    initialize_enabled(&staging);
    staging.highest_terminal_operation_serial = UINT64_MAX;
    staging.terminal_encounter_serial = UINT32_MAX;
    staging.terminal_transition_serial = UINT32_MAX;
    staging.terminal_world_generation = p.world_generation;
    staging.terminal_source_generation = p.source_generation;
    p.serial = 1u;
    p.transition_serial = 1u;
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u,
        &p, &original) == SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);

    p = provenance();
    original = preflight(&p);
    initialize_enabled(&staging);
    staging.highest_terminal_operation_serial = 10u;
    staging.terminal_encounter_serial = 10u;
    staging.terminal_transition_serial = 10u;
    staging.terminal_world_generation = p.world_generation;
    staging.terminal_source_generation = p.source_generation;
    p.serial = UINT32_C(0x8000000a);
    p.transition_serial = UINT32_C(0x8000000a);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 11u,
        &p, &original) == SUDEKIMP_TALOS_STAGING_REJECTED_REPLAY);

    p = provenance();
    original = preflight(&p);
    initialize_enabled(&staging);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    staging.next_authorization_serial = UINT64_MAX;
    immediate = later_full(&original, original.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_SAFE_ABORT);
    CHECK(staging.state == SUDEKIMP_TALOS_STAGING_RELEASED);

    initialize_enabled(&staging);
    staging.next_authorization_serial = UINT64_MAX - 1u;
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, original.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_REMOVE_AUTHORIZED);
    CHECK(ticket.authorization_serial == UINT64_MAX);
    detached_value = detached(&original, immediate.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingFinishRemove(&staging, &ticket,
        &p, &detached_value, 1u) == SUDEKIMP_TALOS_STAGING_DETACH_PROVEN);
    immediate = detached(&original, detached_value.observation_serial + 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRestore(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_QUARANTINE);

    initialize_enabled(&staging);
    original.observation_serial = UINT64_MAX;
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, 1u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_REMOVE_AUTHORIZED);

    initialize_enabled(&staging);
    original = preflight(&p);
    CHECK(SudekiMpTalosCompanionStagingBegin(&staging, 1u, &p, &original) ==
        SUDEKIMP_TALOS_STAGING_PREFLIGHT_ACCEPTED);
    immediate = later_full(&original, 0u);
    CHECK(SudekiMpTalosCompanionStagingClaimRemove(&staging, &p,
        &immediate, &ticket) == SUDEKIMP_TALOS_STAGING_SAFE_ABORT);
}

int main(void) {
    test_default_off_and_happy_path();
    test_preflight_rejections_are_safe();
    test_pre_remove_abort_and_zero_call();
    test_remove_completion_fail_closed();
    test_restore_and_stability_fail_closed();
    test_ticket_tamper_and_quarantine_persistence();
    test_replay_and_wrap_fences();
    if (failures != 0) {
        fprintf(stderr, "talos_companion_staging_test: %d failure(s)\n",
            failures);
        return 1;
    }
    puts("talos_companion_staging_test: ok");
    return 0;
}
