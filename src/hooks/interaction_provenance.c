#include "hooks/interaction_provenance.h"

#include "engine/player_statehood.h"
#include "hooks/call_hook.h"

#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
#include "engine/log.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Interaction provenance requires the 32-bit Windows target"
#endif

typedef void (__attribute__((stdcall)) *ActionCandidateDispatchFunction)(
    void *interaction,
    uint32_t source_is_native_front,
    void *source_actor,
    int candidate_count
);
typedef void (__attribute__((stdcall)) *EnqueueInteractionFunction)(
    void *interaction,
    void *target_owner,
    void *target,
    void *event_resource_name,
    uint32_t event_type,
    void *source_actor
);
typedef void (__attribute__((thiscall)) *UsableCollisionFunction)(
    void *usable_subobject,
    void *collision_source,
    void *source_actor,
    uint32_t event_type
);
typedef int (__attribute__((stdcall)) *NearbyCollisionQueryFunction)(
    void *collision_system,
    const float *position,
    float radius,
    int maximum_results,
    void *results,
    uint32_t category_mask
);
typedef void (__attribute__((thiscall)) *TrackedEntityCleanupFunction)(
    void *tracked_entity
);
/* Native CUsable::GetCanUse.  This is a pure predicate over the supplied
 * tracked entity: unlike SetCanUse or the P1-only arbiter path, it does not
 * select a target, mutate interaction state, or dispatch a script action. */
typedef uint8_t (__attribute__((cdecl)) *UsableGetCanUseFunction)(
    const void *tracked_entity
);
/* CTrigger::AllowsActor.  It reads the trigger's authored category mask and
 * the actor resource type; it does not add a contact, choose a candidate, or
 * send an event. */
typedef uint8_t (__attribute__((stdcall)) *TriggerActorEligibilityFunction)(
    const void *trigger,
    const void *actor,
    uint32_t native_front_actor
);

enum {
    RVA_ACTION_CANDIDATE_DISPATCH_CALL = 0x0000d75bu,
    RVA_ACTION_CANDIDATE_DISPATCH = 0x0000d7a0u,
    RVA_ENQUEUE_INTERACTION_CALL = 0x0000d951u,
    RVA_ENQUEUE_INTERACTION = 0x0000ccd0u,
    RVA_ON_ACTION_SOL_SUBMISSION_CALL = 0x0000caebu,
    RVA_SOL_SUBMISSION = 0x001c37b0u,
    RVA_USABLE_COLLISION = 0x000b5c30u,
    RVA_COLLISION_SYSTEM_GLOBAL = 0x00408dd4u,
    RVA_TRIGGER_MANAGER_GLOBAL = 0x00408d24u,
    RVA_NEARBY_COLLISION_QUERY = 0x00034c20u,
    RVA_TRACKED_ENTITY_CLEANUP = 0x000015e0u,
    RVA_USABLE_GET_CAN_USE = 0x000b59c0u,
    RVA_TRIGGER_ACTOR_ELIGIBILITY = 0x00124320u,
    COLLISION_SYSTEM_PRIMARY_ARRAY_COUNT_OFFSET = 0x70u,
    COLLISION_SYSTEM_PRIMARY_ARRAY_DATA_OFFSET = 0x78u,
    COLLISION_SYSTEM_SECONDARY_ARRAY_COUNT_OFFSET = 0x80u,
    COLLISION_SYSTEM_SECONDARY_ARRAY_DATA_OFFSET = 0x88u,
    COLLISION_SYSTEM_MAXIMUM_ENTITIES = 8192u,
    TRIGGER_MANAGER_OWNER_COUNT_OFFSET = 0x1c8u,
    TRIGGER_MANAGER_OWNER_DATA_OFFSET = 0x1d0u,
    TRIGGER_OWNER_TARGET_COUNT_OFFSET = 0x2cu,
    TRIGGER_OWNER_TARGET_DATA_OFFSET = 0x34u,
    /* CCollisionSystem returns the embedded collision proxy.  In this exact
     * build its containing CTrigger owner begins 0x35c bytes later.  Runtime
     * code never trusts this arithmetic by itself: the resulting address must
     * occur exactly once in CTriggerManager's bounded owner registry. */
    NEARBY_COLLISION_TRIGGER_OWNER_OFFSET = 0x35cu,
    TRIGGER_MANAGER_MAXIMUM_OWNERS = 1024u,
    TRIGGER_OWNER_MAXIMUM_TARGETS = 128u,
    TRIGGER_CORRELATION_MAXIMUM_TARGETS = 2048u,
    TRIGGER_OBJECT_ELIGIBILITY_FLAGS_OFFSET = 0x144u,
    ACTOR_TRANSFORM_OFFSET = 0x44u,
    TRANSFORM_POSITION_OFFSET = 0x18u,
    ACTOR_LOCAL_NEARBY_LIMIT = 16u,
    NATIVE_CANDIDATE_ARRAY_OFFSET = 0x20u,
    NATIVE_CANDIDATE_STRIDE = 0x1cu,
    NATIVE_CANDIDATE_EVENT_OFFSET = 0x00u,
    NATIVE_CANDIDATE_OWNER_LINK_OFFSET = 0x04u,
    NATIVE_CANDIDATE_TARGET_OFFSET = 0x10u,
    NATIVE_CANDIDATE_AUXILIARY_FLAG_OFFSET = 0x18u,
    NATIVE_CANDIDATE_REJECTED_FLAG_OFFSET = 0x19u,
    INTERACTION_MESSAGE_SIZE = 0x44u,
    INTERACTION_MESSAGE_TARGET_OWNER_OFFSET = 0x00u,
    INTERACTION_MESSAGE_TARGET_OFFSET = 0x0cu,
    INTERACTION_MESSAGE_EVENT_RESOURCE_FLAGS_OFFSET = 0x10u,
    INTERACTION_MESSAGE_EVENT_RESOURCE_STORAGE_OFFSET = 0x14u,
    INTERACTION_MESSAGE_EVENT_TYPE_OFFSET = 0x30u,
    INTERACTION_MESSAGE_SOURCE_ACTOR_OFFSET = 0x34u,
    INTERACTION_MESSAGE_NATIVE_FRONT_OFFSET = 0x40u,
    EVENT_TYPE_ON_ACTION = 2u
};

static const uint8_t action_candidate_dispatch_call_signature[] = {
    0xe8u, 0x40u, 0x00u, 0x00u, 0x00u,
    0x80u, 0xbeu, 0xd8u, 0x01u, 0x00u, 0x00u, 0x01u
};
static const uint8_t enqueue_interaction_call_signature[] = {
    0xe8u, 0x7au, 0xf3u, 0xffu, 0xffu,
    0xf7u, 0x44u, 0x24u, 0x20u, 0x00u, 0x00u, 0x00u, 0x80u
};
static const uint8_t on_action_sol_submission_call_signature[] = {
    0xe8u, 0xc0u, 0x6cu, 0x1bu, 0x00u,
    0x83u, 0x7du, 0x30u, 0x02u,
    0x8bu, 0x74u, 0x24u, 0x1cu,
    0x75u, 0x14u, 0x51u
};
static const uint8_t usable_collision_signature[] = {
    0x83u, 0xecu, 0x0cu, 0x56u, 0x8bu, 0xf1u
};
static const uint8_t nearby_collision_query_signature[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x34u, 0x53u, 0x33u, 0xd2u, 0x56u, 0x33u, 0xc0u, 0x57u
};
static const uint8_t tracked_entity_cleanup_signature[] = {
    0x8bu, 0x01u, 0x33u, 0xd2u, 0x3bu, 0xc2u, 0x74u, 0x2du,
    0x56u, 0x39u, 0x48u, 0x04u
};
static const uint8_t usable_get_can_use_signature[] = {
    0x8bu, 0x4cu, 0x24u, 0x04u, 0x83u, 0xecu, 0x0cu, 0x85u,
    0xc9u, 0x74u, 0x38u, 0x8du, 0x04u, 0x24u, 0xe8u, 0xddu
};
static const uint8_t trigger_actor_eligibility_signature[] = {
    0x8bu, 0x4cu, 0x24u, 0x08u, 0x8bu, 0x41u, 0x2cu, 0x8bu,
    0x50u, 0x10u, 0x83u, 0xc1u, 0x2cu, 0xffu, 0xd2u, 0x8du
};

typedef struct NativeEntityPointer {
    void *entity;
    void *link_previous;
    void *link_next;
} NativeEntityPointer;

typedef struct RuntimeDispatchContext {
    struct RuntimeDispatchContext *previous;
    void *interaction;
    uintptr_t source_actor;
    int observation_started;
} RuntimeDispatchContext;

static uint8_t *game_base;
static DWORD dispatch_tls_index = TLS_OUT_OF_INDEXES;
static uint32_t active_source_generation;
static LONG observation_serial;
static SudekiMpInteractionSeatObservation seat_observations[
    SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT];
static SudekiMpInteractionSeatObservation last_logged_seat_observations[
    SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT];
static BOOL last_logged_seat_observation_valid[
    SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT];
static SudekiMpSolInteractionProvenance sol_observations[
    SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT];
static SudekiMpRelativeCallHook action_candidate_dispatch_hook;
static SudekiMpRelativeCallHook enqueue_interaction_hook;
static SudekiMpRelativeCallHook on_action_sol_submission_hook;
static SudekiMpInlineHook usable_collision_hook;
static ActionCandidateDispatchFunction original_action_candidate_dispatch;
static EnqueueInteractionFunction original_enqueue_interaction;
static void *original_script_submission __attribute__((used));
static UsableCollisionFunction original_usable_collision;
static NearbyCollisionQueryFunction nearby_collision_query;
static TrackedEntityCleanupFunction tracked_entity_cleanup;
static UsableGetCanUseFunction usable_get_can_use;
static TriggerActorEligibilityFunction trigger_actor_eligibility;

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL finite_actor_position(uintptr_t actor, float position[3]) {
    uint8_t *transform;

    if (position == NULL || !readable_memory((const void *)actor,
            ACTOR_TRANSFORM_OFFSET + sizeof(transform))) {
        return FALSE;
    }
    transform = *(uint8_t **)(actor + ACTOR_TRANSFORM_OFFSET);
    if (!readable_memory(transform, TRANSFORM_POSITION_OFFSET +
            3u * sizeof(float))) {
        return FALSE;
    }
    memcpy(position, transform + TRANSFORM_POSITION_OFFSET,
        3u * sizeof(float));
    return isfinite(position[0]) && isfinite(position[1]) &&
        isfinite(position[2]);
}

static BOOL collision_system_ready(void **collision_system_result) {
    void *collision_system;
    uint32_t count;
    void *data;

    if (collision_system_result != NULL) {
        *collision_system_result = NULL;
    }
    if (game_base == NULL || nearby_collision_query == NULL ||
        tracked_entity_cleanup == NULL || usable_get_can_use == NULL ||
        !readable_memory(
            game_base + RVA_COLLISION_SYSTEM_GLOBAL,
            sizeof(collision_system))) {
        return FALSE;
    }
    collision_system = *(void **)(game_base + RVA_COLLISION_SYSTEM_GLOBAL);
    if (!readable_memory(collision_system,
            COLLISION_SYSTEM_SECONDARY_ARRAY_DATA_OFFSET + sizeof(data))) {
        return FALSE;
    }
    memcpy(&count, (uint8_t *)collision_system +
        COLLISION_SYSTEM_PRIMARY_ARRAY_COUNT_OFFSET, sizeof(count));
    memcpy(&data, (uint8_t *)collision_system +
        COLLISION_SYSTEM_PRIMARY_ARRAY_DATA_OFFSET, sizeof(data));
    if (count > COLLISION_SYSTEM_MAXIMUM_ENTITIES ||
        (count != 0u && !readable_memory(data, count * sizeof(void *)))) {
        return FALSE;
    }
    memcpy(&count, (uint8_t *)collision_system +
        COLLISION_SYSTEM_SECONDARY_ARRAY_COUNT_OFFSET, sizeof(count));
    memcpy(&data, (uint8_t *)collision_system +
        COLLISION_SYSTEM_SECONDARY_ARRAY_DATA_OFFSET, sizeof(data));
    if (count > COLLISION_SYSTEM_MAXIMUM_ENTITIES ||
        (count != 0u && !readable_memory(data, count * sizeof(void *)))) {
        return FALSE;
    }
    if (collision_system_result != NULL) {
        *collision_system_result = collision_system;
    }
    return TRUE;
}

static BOOL trigger_manager_ready(void ***owners_result,
    uint32_t *owner_count_result) {
    void *manager;
    void **owners;
    uint32_t count;

    if (owners_result != NULL) {
        *owners_result = NULL;
    }
    if (owner_count_result != NULL) {
        *owner_count_result = 0u;
    }
    if (game_base == NULL || trigger_actor_eligibility == NULL ||
        !readable_memory(game_base + RVA_TRIGGER_MANAGER_GLOBAL,
            sizeof(manager))) {
        return FALSE;
    }
    manager = *(void **)(game_base + RVA_TRIGGER_MANAGER_GLOBAL);
    if (!readable_memory(manager, TRIGGER_MANAGER_OWNER_DATA_OFFSET +
            sizeof(owners))) {
        return FALSE;
    }
    memcpy(&count, (uint8_t *)manager + TRIGGER_MANAGER_OWNER_COUNT_OFFSET,
        sizeof(count));
    memcpy(&owners, (uint8_t *)manager + TRIGGER_MANAGER_OWNER_DATA_OFFSET,
        sizeof(owners));
    if (count > TRIGGER_MANAGER_MAXIMUM_OWNERS ||
        (count != 0u && !readable_memory(owners, count * sizeof(*owners)))) {
        return FALSE;
    }
    if (owners_result != NULL) {
        *owners_result = owners;
    }
    if (owner_count_result != NULL) {
        *owner_count_result = count;
    }
    return TRUE;
}

static BOOL block_contains_pointer(const void *block, size_t size,
    const void *needle) {
    size_t offset;
    uintptr_t value;

    if (block == NULL || needle == NULL || size < sizeof(value) ||
        !readable_memory(block, size)) {
        return FALSE;
    }
    for (offset = 0u; offset + sizeof(value) <= size;
            offset += sizeof(value)) {
        memcpy(&value, (const uint8_t *)block + offset, sizeof(value));
        if (value == (uintptr_t)needle) {
            return TRUE;
        }
    }
    return FALSE;
}

static void log_trigger_correlations(uint32_t player_index,
    const SudekiMpPlayerLease *lease, const NativeEntityPointer *candidates,
    int candidate_count) {
    void **owners = NULL;
    uint32_t owner_count = 0u;
    uint32_t owner_index;
    uint32_t scanned_targets = 0u;
    uint32_t linked_targets = 0u;
    uint32_t direct_owner_matches = 0u;
    const char *status = "complete_no_nearby_trigger_link";
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();

#if defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
    (void)player_index;
#endif

    if (lease == NULL || candidates == NULL || candidate_count <= 0) {
        status = "invalid_probe_input";
        goto report;
    }
    if (!trigger_manager_ready(&owners, &owner_count)) {
        status = "trigger_manager_unavailable";
        goto report;
    }
    for (int candidate_index = 0; candidate_index < candidate_count;
            ++candidate_index) {
        uintptr_t entity = (uintptr_t)candidates[candidate_index].entity;
        void *matched_owner = NULL;
        uint32_t match_count = 0u;
        uint32_t target_count;
        void **targets;
        BOOL live_party_actor = FALSE;

        if (statehood != NULL) {
            for (uint32_t seat_index = 0u; seat_index <
                    SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT;
                    ++seat_index) {
                if (statehood->players[seat_index].actor == entity &&
                    entity != 0u) {
                    live_party_actor = TRUE;
                    break;
                }
            }
        }
        if (entity == 0u || live_party_actor ||
            entity > (uintptr_t)(UINT32_MAX -
                NEARBY_COLLISION_TRIGGER_OWNER_OFFSET)) {
            continue;
        }
        for (owner_index = 0u; owner_index < owner_count; ++owner_index) {
            if (owners[owner_index] == (void *)(entity +
                    NEARBY_COLLISION_TRIGGER_OWNER_OFFSET)) {
                matched_owner = owners[owner_index];
                ++match_count;
            }
        }
        if (match_count == 0u) {
            continue;
        }
        if (match_count != 1u || !readable_memory(matched_owner,
                TRIGGER_OWNER_TARGET_DATA_OFFSET + sizeof(targets))) {
            status = "ambiguous_or_unreadable_embedded_trigger_owner";
            goto report;
        }
        memcpy(&target_count, (uint8_t *)matched_owner +
            TRIGGER_OWNER_TARGET_COUNT_OFFSET, sizeof(target_count));
        memcpy(&targets, (uint8_t *)matched_owner +
            TRIGGER_OWNER_TARGET_DATA_OFFSET, sizeof(targets));
        if (target_count == 0u || target_count > TRIGGER_OWNER_MAXIMUM_TARGETS ||
            !readable_memory(targets, target_count * sizeof(*targets))) {
            status = "invalid_embedded_trigger_target_array";
            goto report;
        }
        ++direct_owner_matches;
        for (uint32_t target_index = 0u; target_index < target_count;
                ++target_index) {
            void *trigger = targets[target_index];
            BOOL eligible;
            BOOL native_front_eligible = FALSE;

            if (!readable_memory(trigger,
                    TRIGGER_OBJECT_ELIGIBILITY_FLAGS_OFFSET + sizeof(uint8_t))) {
                status = "unreadable_embedded_trigger_target";
                goto report;
            }
            eligible = trigger_actor_eligibility(trigger,
                (const void *)lease->actor, 0u) != 0u;
            if (statehood != NULL && statehood->players[0].human_present &&
                statehood->players[0].actor != 0u &&
                statehood->players[0].actor_generation != 0u) {
                native_front_eligible = trigger_actor_eligibility(trigger,
                    (const void *)statehood->players[0].actor, 1u) != 0u;
            }
#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
            SudekiMpLogFormat(
                "interaction_provenance event=actor_local_embedded_trigger "
                "seat=%lu nearby_index=%d entity=0x%08lx owner=0x%08lx "
                "target_index=%lu target=0x%08lx eligible_for_actor=%s "
                "eligible_for_native_front=%s "
                "policy=read_only_registry_validated_no_candidate_or_action\\r\\n",
                (unsigned long)player_index + 1ul, candidate_index,
                (unsigned long)entity, (unsigned long)(uintptr_t)matched_owner,
                (unsigned long)target_index, (unsigned long)(uintptr_t)trigger,
                eligible ? "true" : "false",
                native_front_eligible ? "true" : "false");
#else
            (void)eligible;
            (void)native_front_eligible;
#endif
        }
    }
    for (owner_index = 0u; owner_index < owner_count &&
            scanned_targets < TRIGGER_CORRELATION_MAXIMUM_TARGETS;
            ++owner_index) {
        void *owner = owners[owner_index];
        void **targets;
        uint32_t target_count;
        uint32_t target_index;

        if (!readable_memory(owner, TRIGGER_OWNER_TARGET_DATA_OFFSET +
                sizeof(targets))) {
            status = "unreadable_trigger_owner";
            goto report;
        }
        memcpy(&target_count, (uint8_t *)owner +
            TRIGGER_OWNER_TARGET_COUNT_OFFSET, sizeof(target_count));
        memcpy(&targets, (uint8_t *)owner + TRIGGER_OWNER_TARGET_DATA_OFFSET,
            sizeof(targets));
        if (target_count > TRIGGER_OWNER_MAXIMUM_TARGETS ||
            (target_count != 0u && !readable_memory(targets,
                target_count * sizeof(*targets)))) {
            status = "invalid_trigger_target_array";
            goto report;
        }
        for (target_index = 0u; target_index < target_count &&
                scanned_targets < TRIGGER_CORRELATION_MAXIMUM_TARGETS;
                ++target_index, ++scanned_targets) {
            void *trigger = targets[target_index];
            int candidate_index;

            if (!readable_memory(trigger,
                    TRIGGER_OBJECT_ELIGIBILITY_FLAGS_OFFSET + sizeof(uint8_t))) {
                status = "unreadable_trigger";
                goto report;
            }
            for (candidate_index = 0; candidate_index < candidate_count;
                    ++candidate_index) {
                BOOL linked;
                BOOL eligible;

                if (candidates[candidate_index].entity == NULL ||
                    candidates[candidate_index].entity == (void *)lease->actor) {
                    continue;
                }
                linked = owner == candidates[candidate_index].entity ||
                    trigger == candidates[candidate_index].entity ||
                    block_contains_pointer(trigger,
                        TRIGGER_OBJECT_ELIGIBILITY_FLAGS_OFFSET +
                            sizeof(uint8_t), candidates[candidate_index].entity);
                if (!linked) {
                    continue;
                }
                ++linked_targets;
                eligible = trigger_actor_eligibility(trigger,
                    (const void *)lease->actor, 0u) != 0u;
#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
                SudekiMpLogFormat(
                    "interaction_provenance event=actor_local_trigger_link "
                    "seat=%lu nearby_index=%d entity=0x%08lx owner=0x%08lx "
                    "trigger=0x%08lx eligible_for_actor=%s "
                    "policy=read_only_link_and_eligibility_no_candidate_or_action\\r\\n",
                    (unsigned long)player_index + 1ul, candidate_index,
                    (unsigned long)(uintptr_t)candidates[candidate_index].entity,
                    (unsigned long)(uintptr_t)owner,
                    (unsigned long)(uintptr_t)trigger,
                    eligible ? "true" : "false");
#else
                (void)eligible;
#endif
            }
        }
    }
report:
    if (status[0] == 'c' && direct_owner_matches != 0u) {
        status = "complete_with_registry_validated_embedded_trigger";
    }
#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
    SudekiMpLogFormat(
        "interaction_provenance event=actor_local_trigger_scan "
        "seat=%lu owner_count=%lu scanned_targets=%lu linked_targets=%lu "
        "embedded_owner_matches=%lu "
        "status=%s policy=read_only_no_candidate_or_action\\r\\n",
        (unsigned long)player_index + 1ul, (unsigned long)owner_count,
        (unsigned long)scanned_targets, (unsigned long)linked_targets,
        (unsigned long)direct_owner_matches,
        status);
#else
    (void)linked_targets;
    (void)direct_owner_matches;
    (void)status;
#endif
}

BOOL SudekiMpInteractionProvenanceProbeActorLocalNearby(
    uint32_t player_index
) {
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    const SudekiMpPlayerLease *lease;
    NativeEntityPointer candidates[ACTOR_LOCAL_NEARBY_LIMIT];
    float position[3];
    void *collision_system;
    int count;
    int index;

    if (statehood == NULL || player_index >=
            SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT ||
        active_source_generation == 0u) {
        return FALSE;
    }
    lease = &statehood->players[player_index];
    if (!lease->human_present || lease->actor == 0u ||
        lease->actor_generation == 0u ||
        !finite_actor_position(lease->actor, position) ||
        !collision_system_ready(&collision_system)) {
        return FALSE;
    }
    ZeroMemory(candidates, sizeof(candidates));
    count = nearby_collision_query(collision_system, position, 2.0f,
        (int)ACTOR_LOCAL_NEARBY_LIMIT, candidates, UINT32_MAX);
    if (count < 0 || count > (int)ACTOR_LOCAL_NEARBY_LIMIT) {
        for (index = 0; index < (int)ACTOR_LOCAL_NEARBY_LIMIT; ++index) {
            tracked_entity_cleanup(&candidates[index]);
        }
        return FALSE;
    }
#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
    SudekiMpLogFormat(
        "interaction_provenance event=actor_local_nearby_entities "
        "seat=%lu actor=0x%08lx actor_generation=%lu "
        "source_generation=%lu radius=2.00 count=%d "
        "policy=observation_only_no_target_selection_or_native_action\\r\\n",
        (unsigned long)player_index + 1ul,
        (unsigned long)lease->actor,
        (unsigned long)lease->actor_generation,
        (unsigned long)active_source_generation,
        count);
    for (index = 0; index < count; ++index) {
        BOOL usable = candidates[index].entity != NULL &&
            readable_memory(candidates[index].entity, sizeof(void *)) &&
            usable_get_can_use(&candidates[index]) != 0u;

        SudekiMpLogFormat(
            "interaction_provenance event=actor_local_nearby_entity "
            "seat=%lu index=%d entity=0x%08lx usable_can_use=%s "
            "policy=read_only_classification_no_target_selection_or_native_action\\r\\n",
            (unsigned long)player_index + 1ul,
            index,
            (unsigned long)(uintptr_t)candidates[index].entity,
            usable ? "true" : "false");
    }
#endif
    log_trigger_correlations(player_index, lease, candidates, count);
    for (index = 0; index < (int)ACTOR_LOCAL_NEARBY_LIMIT; ++index) {
        tracked_entity_cleanup(&candidates[index]);
    }
    return TRUE;
}

static uint32_t next_observation_serial(void) {
    LONG serial = InterlockedIncrement(&observation_serial);

    if (serial == 0) {
        serial = InterlockedIncrement(&observation_serial);
    }
    return (uint32_t)serial;
}

static BOOL resolve_actor_lease(
    uintptr_t actor,
    uint32_t *player_index,
    uint32_t *actor_generation
) {
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    uint32_t index;
    uint32_t match_count = 0u;
    uint32_t matched_index = 0u;
    uint32_t matched_generation = 0u;

    if (actor == 0u || statehood == NULL) {
        return FALSE;
    }
    for (index = 0u;
         index < SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT;
         ++index) {
        const SudekiMpPlayerLease *lease = &statehood->players[index];

        if (lease->human_present && lease->actor == actor &&
            lease->actor_generation != 0u) {
            ++match_count;
            matched_index = index;
            matched_generation = lease->actor_generation;
        }
    }
    if (match_count != 1u) {
        return FALSE;
    }
    if (player_index != NULL) {
        *player_index = matched_index;
    }
    if (actor_generation != NULL) {
        *actor_generation = matched_generation;
    }
    return TRUE;
}

static BOOL observation_lease_is_current(
    uint32_t player_index,
    uintptr_t actor,
    uint32_t actor_generation,
    uint32_t source_generation
) {
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    const SudekiMpPlayerLease *lease;

    if (statehood == NULL ||
        player_index >= SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT ||
        actor == 0u || actor_generation == 0u ||
        source_generation == 0u ||
        source_generation != active_source_generation) {
        return FALSE;
    }
    lease = &statehood->players[player_index];
    return lease->human_present && lease->actor == actor &&
        lease->actor_generation == actor_generation;
}

static void observe_usable_collision_contact(
    void *usable_subobject,
    void *collision_source,
    void *source_actor,
    uint32_t event_type
) {
    uint32_t player_index;
    uint32_t actor_generation;
    const char *actor_argument;

    if (usable_subobject == NULL || active_source_generation == 0u) {
        return;
    }
    /*
     * The legacy callback's two object arguments are both opaque from this
     * interface.  Do not assume their source/target ordering: observe either
     * one only when it is an exact live player lease.  This remains purely
     * diagnostic and never promotes or dispatches a native interaction.
     */
    if (source_actor != NULL &&
        resolve_actor_lease((uintptr_t)source_actor, &player_index,
            &actor_generation)) {
        actor_argument = "source_actor";
    } else if (collision_source != NULL &&
        resolve_actor_lease((uintptr_t)collision_source, &player_index,
            &actor_generation)) {
        actor_argument = "collision_source";
    } else {
        return;
    }
#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
    SudekiMpLogFormat(
        "interaction_provenance event=usable_contact seat=%lu "
        "actor_argument=%s source_actor=0x%08lx usable=0x%08lx "
        "collision_source=0x%08lx "
        "event_type=%lu actor_generation=%lu source_generation=%lu "
        "policy=observation_only_no_native_action\r\n",
        (unsigned long)player_index + 1ul,
        actor_argument,
        (unsigned long)(uintptr_t)source_actor,
        (unsigned long)((uintptr_t)usable_subobject - 0x3cu),
        (unsigned long)(uintptr_t)collision_source,
        (unsigned long)event_type,
        (unsigned long)actor_generation,
        (unsigned long)active_source_generation);
#else
    (void)collision_source;
    (void)event_type;
    (void)actor_argument;
#endif
}

static void clear_observations(void) {
    ZeroMemory(seat_observations, sizeof(seat_observations));
    ZeroMemory(last_logged_seat_observations,
        sizeof(last_logged_seat_observations));
    ZeroMemory(last_logged_seat_observation_valid,
        sizeof(last_logged_seat_observation_valid));
    ZeroMemory(sol_observations, sizeof(sol_observations));
}

#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
static const char *candidate_status_name(
    SudekiMpInteractionCandidateStatus status
) {
    switch (status) {
    case SUDEKIMP_INTERACTION_CANDIDATE_SEEN:
        return "seen_not_accepted";
    case SUDEKIMP_INTERACTION_CANDIDATE_ACCEPTED_UNVALIDATED:
        return "accepted_unvalidated";
    case SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED:
        return "native_validated";
    case SUDEKIMP_INTERACTION_CANDIDATE_UNOBSERVED:
    default:
        return "unobserved";
    }
}
#endif

static void log_dispatch_change(
    const SudekiMpInteractionSeatObservation *observation
) {
#if defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
    (void)observation;
#else
    SudekiMpInteractionSeatObservation comparable;
    SudekiMpInteractionSeatObservation previous;
    uint32_t index;

    if (observation == NULL || observation->player_index >=
            SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT) {
        return;
    }
    comparable = *observation;
    comparable.serial = 0u;
    previous = last_logged_seat_observations[
        observation->player_index];
    previous.serial = 0u;
    if (last_logged_seat_observation_valid[observation->player_index] &&
        memcmp(&comparable, &previous, sizeof(comparable)) == 0) {
        return;
    }
    last_logged_seat_observations[observation->player_index] =
        *observation;
    last_logged_seat_observation_valid[observation->player_index] = TRUE;
    SudekiMpLogFormat(
        "interaction_provenance event=dispatch_end seat=%lu "
        "source_actor=0x%08lx actor_generation=%lu "
        "source_generation=%lu source_is_native_front=%s "
        "native_count=%lu captured_count=%lu overflow=%s "
        "identity_ambiguous=%s completed=%s "
        "authority_generation=%s policy=observation_only_no_native_mutation\r\n",
        (unsigned long)observation->player_index + 1ul,
        (unsigned long)observation->source_actor,
        (unsigned long)observation->actor_generation,
        (unsigned long)observation->source_generation,
        observation->source_is_native_front ? "true" : "false",
        (unsigned long)observation->native_candidate_count,
        (unsigned long)observation->candidate_count,
        observation->overflowed ? "true" : "false",
        observation->identity_ambiguous ? "true" : "false",
        observation->completed ? "true" : "false",
        observation->source_generation != 0u ? "current" : "invalid_zero");
    for (index = 0u; index < observation->candidate_count; ++index) {
        const SudekiMpInteractionCandidateObservation *candidate =
            &observation->candidates[index];

        if (candidate->status !=
                SUDEKIMP_INTERACTION_CANDIDATE_ACCEPTED_UNVALIDATED &&
            candidate->status !=
                SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED) {
            continue;
        }
        SudekiMpLogFormat(
            "interaction_provenance event=accepted_candidate seat=%lu "
            "candidate_index=%lu status=%s target_owner=0x%08lx "
            "target=0x%08lx event_type=%lu activation_authority=%s "
            "policy=p2_unvalidated_fails_closed\r\n",
            (unsigned long)observation->player_index + 1ul,
            (unsigned long)index,
            candidate_status_name(candidate->status),
            (unsigned long)candidate->target_owner,
            (unsigned long)candidate->target,
            (unsigned long)candidate->event_type,
            candidate->status ==
                    SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED &&
                observation->source_generation != 0u &&
                !observation->overflowed &&
                !observation->identity_ambiguous ?
                    "proven" : "denied");
    }
#endif
}

void SudekiMpInteractionProvenanceSetSourceGeneration(uint32_t generation) {
    if (generation == active_source_generation) {
        return;
    }
    clear_observations();
    active_source_generation = generation;
}

void SudekiMpInteractionProvenanceInvalidate(void) {
    clear_observations();
    active_source_generation = 0u;
}

void SudekiMpInteractionProvenanceInvalidateSolThread(uintptr_t sol_thread) {
    uint32_t index;

    if (sol_thread == 0u) {
        return;
    }
    for (index = 0u;
         index < SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT;
         ++index) {
        if (sol_observations[index].sol_thread == sol_thread) {
            ZeroMemory(&sol_observations[index],
                sizeof(sol_observations[index]));
        }
    }
}

BOOL SudekiMpInteractionProvenanceObserveDispatchBegin(
    uintptr_t source_actor,
    int source_is_native_front,
    const SudekiMpInteractionCandidateObservation *candidates,
    uint32_t native_candidate_count
) {
    SudekiMpInteractionSeatObservation *observation;
    uint32_t player_index;
    uint32_t actor_generation;
    uint32_t copy_count;
    uint32_t index;

    if (!resolve_actor_lease(source_actor, &player_index,
            &actor_generation)) {
        return FALSE;
    }
    copy_count = native_candidate_count;
    if (copy_count > SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT) {
        copy_count = SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT;
    }
    if (copy_count != 0u && candidates == NULL) {
        return FALSE;
    }
    observation = &seat_observations[player_index];
    ZeroMemory(observation, sizeof(*observation));
    observation->serial = next_observation_serial();
    observation->player_index = player_index;
    observation->source_actor = source_actor;
    observation->actor_generation = actor_generation;
    observation->source_generation = active_source_generation;
    observation->native_candidate_count = native_candidate_count;
    observation->candidate_count = copy_count;
    observation->source_is_native_front =
        source_is_native_front != 0;
    observation->overflowed = native_candidate_count >
        SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT;
    for (index = 0u; index < copy_count; ++index) {
        observation->candidates[index].target_owner =
            candidates[index].target_owner;
        observation->candidates[index].target = candidates[index].target;
        observation->candidates[index].event_type =
            candidates[index].event_type;
        observation->candidates[index].initial_auxiliary_flag =
            candidates[index].initial_auxiliary_flag;
        observation->candidates[index].initial_rejected_flag =
            candidates[index].initial_rejected_flag;
        observation->candidates[index].status =
            SUDEKIMP_INTERACTION_CANDIDATE_SEEN;
    }
    return TRUE;
}

void SudekiMpInteractionProvenanceObserveAcceptedCandidate(
    uintptr_t source_actor,
    uintptr_t target_owner,
    uintptr_t target,
    uint32_t event_type
) {
    SudekiMpInteractionSeatObservation *observation;
    uint32_t player_index;
    uint32_t actor_generation;
    uint32_t matched_index = 0u;
    uint32_t match_count = 0u;
    uint32_t index;

    if (!resolve_actor_lease(source_actor, &player_index,
            &actor_generation)) {
        return;
    }
    observation = &seat_observations[player_index];
    if (observation->serial == 0u || observation->completed ||
        observation->source_actor != source_actor ||
        observation->actor_generation != actor_generation) {
        return;
    }
    for (index = 0u; index < observation->candidate_count; ++index) {
        const SudekiMpInteractionCandidateObservation *candidate =
            &observation->candidates[index];

        if (candidate->target_owner == target_owner &&
            candidate->target == target &&
            candidate->event_type == event_type) {
            matched_index = index;
            ++match_count;
        }
    }
    if (match_count != 1u) {
        observation->identity_ambiguous = 1;
        return;
    }
    observation->candidates[matched_index].status =
        observation->source_is_native_front ?
            SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED :
            SUDEKIMP_INTERACTION_CANDIDATE_ACCEPTED_UNVALIDATED;
}

void SudekiMpInteractionProvenanceObserveDispatchEnd(
    uintptr_t source_actor
) {
    uint32_t player_index;
    uint32_t actor_generation;
    SudekiMpInteractionSeatObservation *observation;

    if (!resolve_actor_lease(source_actor, &player_index,
            &actor_generation)) {
        return;
    }
    observation = &seat_observations[player_index];
    if (observation->serial != 0u &&
        observation->source_actor == source_actor &&
        observation->actor_generation == actor_generation) {
        observation->completed = 1;
        log_dispatch_change(observation);
    }
}

BOOL SudekiMpInteractionProvenanceGetSeat(
    uint32_t player_index,
    SudekiMpInteractionSeatObservation *observation
) {
    const SudekiMpInteractionSeatObservation *source;

    if (observation == NULL ||
        player_index >= SUDEKIMP_INTERACTION_PROVENANCE_PLAYER_COUNT) {
        return FALSE;
    }
    ZeroMemory(observation, sizeof(*observation));
    source = &seat_observations[player_index];
    if (source->serial == 0u || !observation_lease_is_current(
            source->player_index,
            source->source_actor,
            source->actor_generation,
            source->source_generation)) {
        return FALSE;
    }
    *observation = *source;
    return TRUE;
}

BOOL SudekiMpInteractionCandidateAuthorityProven(
    const SudekiMpInteractionSeatObservation *observation,
    uint32_t candidate_index
) {
    const SudekiMpInteractionCandidateObservation *candidate;

    if (observation == NULL || !observation->completed ||
        observation->overflowed || observation->identity_ambiguous ||
        !observation->source_is_native_front ||
        candidate_index >= observation->candidate_count ||
        !observation_lease_is_current(
            observation->player_index,
            observation->source_actor,
            observation->actor_generation,
            observation->source_generation)) {
        return FALSE;
    }
    candidate = &observation->candidates[candidate_index];
    return candidate->status ==
            SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED &&
        candidate->event_type == EVENT_TYPE_ON_ACTION &&
        candidate->target_owner != 0u && candidate->target != 0u;
}

BOOL SudekiMpInteractionProvenanceObserveSolSubmission(
    const SudekiMpSolInteractionProvenance *provenance
) {
    SudekiMpSolInteractionProvenance candidate;
    uint32_t player_index;
    uint32_t actor_generation;
    uint32_t index;
    uint32_t selected = SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT;
    uint32_t oldest_age = 0u;

    if (provenance == NULL || provenance->source_actor == 0u ||
        provenance->target_owner == 0u || provenance->target == 0u ||
        provenance->event_type != EVENT_TYPE_ON_ACTION ||
        provenance->task_handle == 0u || provenance->sol_thread == 0u ||
        !resolve_actor_lease(provenance->source_actor, &player_index,
            &actor_generation)) {
        return FALSE;
    }
    for (index = 0u;
         index < SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT;
         ++index) {
        const SudekiMpSolInteractionProvenance *entry =
            &sol_observations[index];

        if (entry->sol_thread == provenance->sol_thread) {
            selected = index;
            break;
        }
    }
    if (selected == SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT) {
        for (index = 0u;
             index < SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT;
             ++index) {
            if (sol_observations[index].serial == 0u) {
                selected = index;
                break;
            }
        }
    }
    if (selected == SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT) {
        selected = 0u;
        oldest_age = provenance->observed_at_ms -
            sol_observations[0].observed_at_ms;
        for (index = 1u;
             index < SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT;
             ++index) {
            uint32_t age = provenance->observed_at_ms -
                sol_observations[index].observed_at_ms;

            if (age > oldest_age) {
                selected = index;
                oldest_age = age;
            }
        }
    }
    if (selected >= SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT) {
        return FALSE;
    }
    candidate = *provenance;
    candidate.serial = next_observation_serial();
    candidate.player_index = player_index;
    candidate.actor_generation = actor_generation;
    candidate.source_generation = active_source_generation;
    candidate.native_thread_id = GetCurrentThreadId();
    sol_observations[selected] = candidate;
    return TRUE;
}

BOOL SudekiMpInteractionProvenanceFindSolThread(
    uintptr_t sol_thread,
    uint32_t now_ms,
    SudekiMpSolInteractionProvenance *provenance
) {
    uint32_t index;

    if (sol_thread == 0u || provenance == NULL) {
        return FALSE;
    }
    ZeroMemory(provenance, sizeof(*provenance));
    for (index = 0u;
         index < SUDEKIMP_INTERACTION_PROVENANCE_SOL_THREAD_LIMIT;
         ++index) {
        const SudekiMpSolInteractionProvenance *entry =
            &sol_observations[index];

        if (entry->serial != 0u && entry->sol_thread == sol_thread &&
            entry->native_thread_id == GetCurrentThreadId() &&
            (uint32_t)(now_ms - entry->observed_at_ms) <=
                SUDEKIMP_INTERACTION_PROVENANCE_SOL_LIFETIME_MS &&
            observation_lease_is_current(
                entry->player_index,
                entry->source_actor,
                entry->actor_generation,
                entry->source_generation)) {
            *provenance = *entry;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL SudekiMpSolInteractionAuthorityProven(
    const SudekiMpSolInteractionProvenance *provenance,
    uint32_t now_ms
) {
    if (provenance == NULL || provenance->serial == 0u ||
        !provenance->source_is_native_front ||
        provenance->event_type != EVENT_TYPE_ON_ACTION ||
        provenance->source_actor == 0u ||
        provenance->target_owner == 0u || provenance->target == 0u ||
        provenance->task_handle == 0u || provenance->sol_thread == 0u ||
        provenance->native_thread_id != GetCurrentThreadId() ||
        (uint32_t)(now_ms - provenance->observed_at_ms) >
            SUDEKIMP_INTERACTION_PROVENANCE_SOL_LIFETIME_MS) {
        return FALSE;
    }
    return observation_lease_is_current(
        provenance->player_index,
        provenance->source_actor,
        provenance->actor_generation,
        provenance->source_generation);
}

static BOOL capture_native_candidates(
    void *interaction,
    int candidate_count,
    SudekiMpInteractionCandidateObservation
        candidates[SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT],
    uint32_t *native_candidate_count
) {
    uint8_t *base = (uint8_t *)interaction;
    uint32_t count;
    uint32_t copy_count;
    uint32_t index;

    if (candidates == NULL || native_candidate_count == NULL ||
        interaction == NULL || candidate_count < 0) {
        return FALSE;
    }
    count = (uint32_t)candidate_count;
    copy_count = count;
    if (copy_count > SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT) {
        copy_count = SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT;
    }
    if (copy_count != 0u && !readable_memory(
            base + NATIVE_CANDIDATE_ARRAY_OFFSET,
            copy_count * NATIVE_CANDIDATE_STRIDE)) {
        return FALSE;
    }
    ZeroMemory(candidates,
        sizeof(*candidates) *
            SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT);
    for (index = 0u; index < copy_count; ++index) {
        const uint8_t *native_candidate = base +
            NATIVE_CANDIDATE_ARRAY_OFFSET +
            index * NATIVE_CANDIDATE_STRIDE;
        uintptr_t owner_link;

        memcpy(&candidates[index].event_type,
            native_candidate + NATIVE_CANDIDATE_EVENT_OFFSET,
            sizeof(candidates[index].event_type));
        memcpy(&owner_link,
            native_candidate + NATIVE_CANDIDATE_OWNER_LINK_OFFSET,
            sizeof(owner_link));
        candidates[index].target_owner = owner_link >= sizeof(uint32_t) ?
            owner_link - sizeof(uint32_t) : 0u;
        memcpy(&candidates[index].target,
            native_candidate + NATIVE_CANDIDATE_TARGET_OFFSET,
            sizeof(candidates[index].target));
        candidates[index].initial_auxiliary_flag = *(
            native_candidate + NATIVE_CANDIDATE_AUXILIARY_FLAG_OFFSET);
        candidates[index].initial_rejected_flag = *(
            native_candidate + NATIVE_CANDIDATE_REJECTED_FLAG_OFFSET);
        candidates[index].status =
            SUDEKIMP_INTERACTION_CANDIDATE_SEEN;
    }
    *native_candidate_count = count;
    return TRUE;
}

static void __attribute__((stdcall)) observe_action_candidate_dispatch(
    void *interaction,
    uint32_t source_is_native_front,
    void *source_actor,
    int candidate_count
) {
    SudekiMpInteractionCandidateObservation candidates[
        SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT];
    RuntimeDispatchContext context;
    uint32_t native_candidate_count = 0u;
    DWORD entry_error = GetLastError();
    DWORD result_error;

    ZeroMemory(&context, sizeof(context));
    context.interaction = interaction;
    context.source_actor = (uintptr_t)source_actor;
    if (dispatch_tls_index != TLS_OUT_OF_INDEXES) {
        context.previous = (RuntimeDispatchContext *)TlsGetValue(
            dispatch_tls_index);
    }
    if (capture_native_candidates(
            interaction, candidate_count, candidates,
            &native_candidate_count)) {
        context.observation_started =
            SudekiMpInteractionProvenanceObserveDispatchBegin(
                (uintptr_t)source_actor,
                source_is_native_front != 0u,
                candidates,
                native_candidate_count);
    }
    if (dispatch_tls_index != TLS_OUT_OF_INDEXES) {
        (void)TlsSetValue(dispatch_tls_index, &context);
    }
    SetLastError(entry_error);
    original_action_candidate_dispatch(
        interaction,
        source_is_native_front,
        source_actor,
        candidate_count);
    result_error = GetLastError();
    if (context.observation_started) {
        SudekiMpInteractionProvenanceObserveDispatchEnd(
            (uintptr_t)source_actor);
    }
    if (dispatch_tls_index != TLS_OUT_OF_INDEXES) {
        (void)TlsSetValue(dispatch_tls_index, context.previous);
    }
    SetLastError(result_error);
}

static void __attribute__((stdcall)) observe_enqueue_interaction(
    void *interaction,
    void *target_owner,
    void *target,
    void *event_resource_name,
    uint32_t event_type,
    void *source_actor
) {
    RuntimeDispatchContext *context;
    DWORD result_error;

    original_enqueue_interaction(
        interaction,
        target_owner,
        target,
        event_resource_name,
        event_type,
        source_actor);
    result_error = GetLastError();
    context = dispatch_tls_index == TLS_OUT_OF_INDEXES ? NULL :
        (RuntimeDispatchContext *)TlsGetValue(dispatch_tls_index);
    if (context != NULL && context->observation_started &&
        context->interaction == interaction &&
        context->source_actor == (uintptr_t)source_actor) {
        SudekiMpInteractionProvenanceObserveAcceptedCandidate(
            (uintptr_t)source_actor,
            (uintptr_t)target_owner,
            (uintptr_t)target,
            event_type);
    }
    SetLastError(result_error);
}

static void __attribute__((noinline, used))
observe_script_submission_result(
    const void *interaction_message,
    void *const *output_task_handle
) {
    const uint8_t *message =
        (const uint8_t *)interaction_message;
    SudekiMpSolInteractionProvenance provenance;
    void *task_handle;
    void *sol_thread;
    uintptr_t target_owner_link;
    DWORD result_error = GetLastError();

    if (!readable_memory(message, INTERACTION_MESSAGE_SIZE) ||
        !readable_memory(output_task_handle,
            sizeof(*output_task_handle))) {
        SetLastError(result_error);
        return;
    }
    task_handle = *output_task_handle;
    if (!readable_memory(task_handle, sizeof(sol_thread))) {
        SetLastError(result_error);
        return;
    }
    sol_thread = *(void **)task_handle;
    ZeroMemory(&provenance, sizeof(provenance));
    memcpy(&target_owner_link,
        message + INTERACTION_MESSAGE_TARGET_OWNER_OFFSET,
        sizeof(target_owner_link));
    provenance.target_owner = target_owner_link >= sizeof(uint32_t) ?
        target_owner_link - sizeof(uint32_t) : 0u;
    memcpy(&provenance.target,
        message + INTERACTION_MESSAGE_TARGET_OFFSET,
        sizeof(provenance.target));
    memcpy(&provenance.event_resource_flags,
        message + INTERACTION_MESSAGE_EVENT_RESOURCE_FLAGS_OFFSET,
        sizeof(provenance.event_resource_flags));
    memcpy(&provenance.event_resource_storage,
        message + INTERACTION_MESSAGE_EVENT_RESOURCE_STORAGE_OFFSET,
        sizeof(provenance.event_resource_storage));
    memcpy(&provenance.event_type,
        message + INTERACTION_MESSAGE_EVENT_TYPE_OFFSET,
        sizeof(provenance.event_type));
    memcpy(&provenance.source_actor,
        message + INTERACTION_MESSAGE_SOURCE_ACTOR_OFFSET,
        sizeof(provenance.source_actor));
    provenance.source_is_native_front =
        *(message + INTERACTION_MESSAGE_NATIVE_FRONT_OFFSET) != 0u;
    provenance.task_handle = (uintptr_t)task_handle;
    provenance.sol_thread = (uintptr_t)sol_thread;
    provenance.observed_at_ms = GetTickCount();
    (void)SudekiMpInteractionProvenanceObserveSolSubmission(&provenance);
    SetLastError(result_error);
}

/* RVA 0x0000CAEB calls a hybrid ABI: the script resource is in EAX and seven
 * arguments are callee-cleaned from the stack. EBP still points at the 0x44
 * interaction message in its caller. The native continuation consumes both
 * EAX and the helper's post-call ECX (the latter at RVA 0x0000CAFA). This
 * bridge copies the original stack, invokes the untouched helper, and observes
 * the returned task handle only after native submission has completed while
 * preserving both return registers exactly. */
__attribute__((naked, noinline, used))
static void *observe_script_submission(void) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "subl $8, %esp\n\t"
        "movl %eax, -16(%ebp)\n\t"
        "pushl 32(%ebp)\n\t"
        "pushl 28(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "pushl 16(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "movl -16(%ebp), %eax\n\t"
        "call *_original_script_submission\n\t"
        "movl %eax, -20(%ebp)\n\t"
        "pushl %ecx\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl 0(%ebp)\n\t"
        "call _observe_script_submission_result\n\t"
        "addl $8, %esp\n\t"
        "popl %ecx\n\t"
        "movl -20(%ebp), %eax\n\t"
        "addl $8, %esp\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret $28\n\t"
    );
}

static void __attribute__((thiscall)) observe_usable_collision(
    void *usable_subobject,
    void *collision_source,
    void *source_actor,
    uint32_t event_type
) {
    DWORD incoming_error = GetLastError();

    observe_usable_collision_contact(usable_subobject, collision_source,
        source_actor, event_type);
    SetLastError(incoming_error);
    original_usable_collision(usable_subobject, collision_source,
        source_actor, event_type);
}

static BOOL signatures_match(uint8_t *base) {
    return readable_memory(
            base + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            sizeof(action_candidate_dispatch_call_signature)) &&
        readable_memory(
            base + RVA_ENQUEUE_INTERACTION_CALL,
            sizeof(enqueue_interaction_call_signature)) &&
        readable_memory(
            base + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            sizeof(on_action_sol_submission_call_signature)) &&
        readable_memory(base + RVA_USABLE_COLLISION,
            sizeof(usable_collision_signature)) &&
        readable_memory(base + RVA_NEARBY_COLLISION_QUERY,
            sizeof(nearby_collision_query_signature)) &&
        readable_memory(base + RVA_TRACKED_ENTITY_CLEANUP,
            sizeof(tracked_entity_cleanup_signature)) &&
        readable_memory(base + RVA_USABLE_GET_CAN_USE,
            sizeof(usable_get_can_use_signature)) &&
        readable_memory(base + RVA_TRIGGER_ACTOR_ELIGIBILITY,
            sizeof(trigger_actor_eligibility_signature)) &&
        memcmp(base + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            action_candidate_dispatch_call_signature,
            sizeof(action_candidate_dispatch_call_signature)) == 0 &&
        memcmp(base + RVA_ENQUEUE_INTERACTION_CALL,
            enqueue_interaction_call_signature,
            sizeof(enqueue_interaction_call_signature)) == 0 &&
        memcmp(base + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            on_action_sol_submission_call_signature,
            sizeof(on_action_sol_submission_call_signature)) == 0 &&
        memcmp(base + RVA_USABLE_COLLISION, usable_collision_signature,
            sizeof(usable_collision_signature)) == 0 &&
        memcmp(base + RVA_NEARBY_COLLISION_QUERY,
            nearby_collision_query_signature,
            sizeof(nearby_collision_query_signature)) == 0 &&
        memcmp(base + RVA_TRACKED_ENTITY_CLEANUP,
            tracked_entity_cleanup_signature,
            sizeof(tracked_entity_cleanup_signature)) == 0 &&
        memcmp(base + RVA_USABLE_GET_CAN_USE, usable_get_can_use_signature,
            sizeof(usable_get_can_use_signature)) == 0 &&
        memcmp(base + RVA_TRIGGER_ACTOR_ELIGIBILITY,
            trigger_actor_eligibility_signature,
            sizeof(trigger_actor_eligibility_signature)) == 0;
}

BOOL SudekiMpInstallInteractionProvenance(
    HMODULE game_module,
    BOOL enabled
) {
    uint8_t *base = (uint8_t *)game_module;

    if (!enabled) {
        return TRUE;
    }
    if (base == NULL || game_base != NULL ||
        (uintptr_t)base >
            (uintptr_t)(UINT32_MAX - RVA_SOL_SUBMISSION) ||
        !signatures_match(base)) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    dispatch_tls_index = TlsAlloc();
    if (dispatch_tls_index == TLS_OUT_OF_INDEXES) {
        return FALSE;
    }
    game_base = base;
    original_action_candidate_dispatch =
        (ActionCandidateDispatchFunction)(
            base + RVA_ACTION_CANDIDATE_DISPATCH);
    original_enqueue_interaction =
        (EnqueueInteractionFunction)(base + RVA_ENQUEUE_INTERACTION);
    original_script_submission = base + RVA_SOL_SUBMISSION;
    nearby_collision_query = (NearbyCollisionQueryFunction)(
        base + RVA_NEARBY_COLLISION_QUERY);
    tracked_entity_cleanup = (TrackedEntityCleanupFunction)(
        base + RVA_TRACKED_ENTITY_CLEANUP);
    usable_get_can_use = (UsableGetCanUseFunction)(
        base + RVA_USABLE_GET_CAN_USE);
    trigger_actor_eligibility = (TriggerActorEligibilityFunction)(
        base + RVA_TRIGGER_ACTOR_ELIGIBILITY);
    if (!SudekiMpInstallRelativeCallHook(
            &action_candidate_dispatch_hook,
            base + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            base + RVA_ACTION_CANDIDATE_DISPATCH,
            observe_action_candidate_dispatch) ||
        !SudekiMpInstallRelativeCallHook(
            &enqueue_interaction_hook,
            base + RVA_ENQUEUE_INTERACTION_CALL,
            base + RVA_ENQUEUE_INTERACTION,
            observe_enqueue_interaction) ||
        !SudekiMpInstallRelativeCallHook(
            &on_action_sol_submission_hook,
            base + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            base + RVA_SOL_SUBMISSION,
            observe_script_submission)) {
        SudekiMpUninstallInteractionProvenance();
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(&usable_collision_hook,
            base + RVA_USABLE_COLLISION, usable_collision_signature,
            sizeof(usable_collision_signature), observe_usable_collision)) {
        SudekiMpUninstallInteractionProvenance();
        return FALSE;
    }
    original_usable_collision = (UsableCollisionFunction)
        usable_collision_hook.trampoline;
    return TRUE;
}

void SudekiMpUninstallInteractionProvenance(void) {
    SudekiMpRestoreInlineHook(&usable_collision_hook);
    SudekiMpRestoreRelativeCallHook(&on_action_sol_submission_hook);
    SudekiMpRestoreRelativeCallHook(&enqueue_interaction_hook);
    SudekiMpRestoreRelativeCallHook(&action_candidate_dispatch_hook);
    if (dispatch_tls_index != TLS_OUT_OF_INDEXES) {
        TlsFree(dispatch_tls_index);
        dispatch_tls_index = TLS_OUT_OF_INDEXES;
    }
    original_script_submission = NULL;
    tracked_entity_cleanup = NULL;
    nearby_collision_query = NULL;
    usable_get_can_use = NULL;
    trigger_actor_eligibility = NULL;
    original_usable_collision = NULL;
    original_enqueue_interaction = NULL;
    original_action_candidate_dispatch = NULL;
    game_base = NULL;
    SudekiMpInteractionProvenanceInvalidate();
}
