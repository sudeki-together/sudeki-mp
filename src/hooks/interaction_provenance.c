#include "hooks/interaction_provenance.h"

#include "engine/player_statehood.h"
#include "hooks/call_hook.h"

#if !defined(SUDEKIMP_INTERACTION_PROVENANCE_TESTING)
#include "engine/log.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

enum {
    RVA_ACTION_CANDIDATE_DISPATCH_CALL = 0x0000d75bu,
    RVA_ACTION_CANDIDATE_DISPATCH = 0x0000d7a0u,
    RVA_ENQUEUE_INTERACTION_CALL = 0x0000d951u,
    RVA_ENQUEUE_INTERACTION = 0x0000ccd0u,
    RVA_ON_ACTION_SOL_SUBMISSION_CALL = 0x0000caebu,
    RVA_SOL_SUBMISSION = 0x001c37b0u,
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
static ActionCandidateDispatchFunction original_action_candidate_dispatch;
static EnqueueInteractionFunction original_enqueue_interaction;
static void *original_script_submission __attribute__((used));

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
        memcmp(base + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            action_candidate_dispatch_call_signature,
            sizeof(action_candidate_dispatch_call_signature)) == 0 &&
        memcmp(base + RVA_ENQUEUE_INTERACTION_CALL,
            enqueue_interaction_call_signature,
            sizeof(enqueue_interaction_call_signature)) == 0 &&
        memcmp(base + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            on_action_sol_submission_call_signature,
            sizeof(on_action_sol_submission_call_signature)) == 0;
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
    return TRUE;
}

void SudekiMpUninstallInteractionProvenance(void) {
    SudekiMpRestoreRelativeCallHook(&on_action_sol_submission_hook);
    SudekiMpRestoreRelativeCallHook(&enqueue_interaction_hook);
    SudekiMpRestoreRelativeCallHook(&action_candidate_dispatch_hook);
    if (dispatch_tls_index != TLS_OUT_OF_INDEXES) {
        TlsFree(dispatch_tls_index);
        dispatch_tls_index = TLS_OUT_OF_INDEXES;
    }
    original_script_submission = NULL;
    original_enqueue_interaction = NULL;
    original_action_candidate_dispatch = NULL;
    game_base = NULL;
    SudekiMpInteractionProvenanceInvalidate();
}
