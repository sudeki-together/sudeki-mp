#include "hooks/talos_post_movie_party_restore.h"

#include <string.h>

static BOOL strict_boolean(uint32_t value) {
    return value == 0u || value == 1u;
}

static void fail_machine(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    SudekiMpTalosPostMoviePartyRestoreFailure failure
) {
    machine->failure = (uint32_t)failure;
    machine->state = SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED;
}

void SudekiMpTalosPostMoviePartyRestoreMachineInitialize(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    BOOL enabled
) {
    if (machine == NULL) {
        return;
    }
    memset(machine, 0, sizeof(*machine));
    machine->state = enabled ?
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_TICKET :
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_DISABLED;
}

BOOL SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
    SudekiMpTalosPostMoviePartyRestoreMachine *machine,
    SudekiMpTalosPostMoviePartyRestoreEvent event,
    uint32_t value,
    uint32_t now_ms
) {
    uint32_t elapsed;

    if (machine == NULL || machine->reserved != 0u ||
        machine->state == SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_DISABLED ||
        machine->state == SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED ||
        machine->state == SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        return FALSE;
    }
    if (event == SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT) {
        if (value <= SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_NONE ||
            value > SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE) {
            return FALSE;
        }
        fail_machine(
            machine,
            (SudekiMpTalosPostMoviePartyRestoreFailure)value
        );
        return TRUE;
    }

    switch (event) {
        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_TICKET ||
                machine->ticket_claimed != 0u || value != 1u) {
                return FALSE;
            }
            machine->ticket_claimed = 1u;
            machine->ticket_claimed_at_ms = now_ms;
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_PREFLIGHT;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_ACCEPTED:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_PREFLIGHT ||
                value != 1u) {
                return FALSE;
            }
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_SPAWN_REQUESTS;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_REJECTED:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_PREFLIGHT ||
                value != 0u) {
                return FALSE;
            }
            fail_machine(
                machine,
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PREFLIGHT
            );
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SPAWN_RESULT:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_SPAWN_REQUESTS ||
                (value & ~SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK) !=
                    0u) {
                return FALSE;
            }
            machine->spawn_accepted_mask = (uint8_t)value;
            if (value !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK) {
                fail_machine(
                    machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_SPAWN_REJECTED
                );
                return TRUE;
            }
            machine->postspawn_started_at_ms = now_ms;
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY ||
                (value & ~SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK) !=
                    0u) {
                return FALSE;
            }
            machine->party_present_mask = (uint8_t)value;
            elapsed = now_ms - machine->postspawn_started_at_ms;
            if (value ==
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK &&
                elapsed >= 1000u) {
                machine->state =
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_INITIALIZE_ACTORS;
            } else if (elapsed >
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_POSTSPAWN_TIMEOUT_MS) {
                fail_machine(
                    machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_POSTSPAWN_TIMEOUT
                );
            }
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_INITIALIZE_ACTORS ||
                (value & ~SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK) !=
                    0u) {
                return FALSE;
            }
            machine->initialized_mask = (uint8_t)value;
            if (value !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK) {
                fail_machine(
                    machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_ACTOR_INITIALIZE
                );
                return TRUE;
            }
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REQUEST_PLAYER_TWO;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REQUEST_PLAYER_TWO ||
                !strict_boolean(value)) {
                return FALSE;
            }
            if (value == 0u) {
                fail_machine(
                    machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_REQUEST
                );
                return TRUE;
            }
            machine->player_two_requested = 1u;
            machine->player_two_started_at_ms = now_ms;
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PLAYER_TWO;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PLAYER_TWO ||
                !strict_boolean(value)) {
                return FALSE;
            }
            if (value != 0u) {
                machine->player_two_active = 1u;
                machine->state =
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REFRESH_COMBAT;
                return TRUE;
            }
            elapsed = now_ms - machine->player_two_started_at_ms;
            if (elapsed >
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_PLAYER_TWO_TIMEOUT_MS) {
                fail_machine(
                    machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_TIMEOUT
                );
            }
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_COMBAT_REFRESH_RESULT:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REFRESH_COMBAT ||
                !strict_boolean(value)) {
                return FALSE;
            }
            if (value == 0u) {
                fail_machine(
                    machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_COMBAT_REFRESH
                );
                return TRUE;
            }
            machine->combat_refreshed = 1u;
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SESSION_ENDED:
            if (machine->state !=
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE ||
                value != 1u) {
                return FALSE;
            }
            machine->state =
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED;
            return TRUE;

        case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT:
        default:
            return FALSE;
    }
}

BOOL SudekiMpTalosPostMoviePartyRestoreCameraAuthorized(
    const SudekiMpTalosPostMoviePartyRestoreStatus *status
) {
    return status != NULL &&
        status->installed != 0u &&
        status->enabled != 0u &&
        status->observer_registered != 0u &&
        status->ai_filter_installed != 0u &&
        status->cleanroom_initialized != 0u &&
        status->machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE &&
        status->machine.failure ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_NONE &&
        status->machine.player_two_active != 0u &&
        status->machine.combat_refreshed != 0u &&
        status->last_party_count == 4u &&
        status->target_policy_active != 0u &&
        status->boss_filter_identity_ready != 0u &&
        status->combat_ready != 0u &&
        status->party_topology_exact != 0u &&
        status->control_state_exact != 0u &&
        status->player_two_input_ready != 0u &&
        status->valid != 0u &&
        status->reload_required == 0u;
}

BOOL SudekiMpTalosPostMoviePartyRestoreCameraBundleAllowed(
    unsigned int bundle_mask
) {
    return bundle_mask == 0u ||
        bundle_mask == SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_EXACT;
}

BOOL SudekiMpTalosPostMoviePartyRestoreCameraNavigationProfileAllowed(
    unsigned int bundle_mask,
    BOOL camera_relative_movement
) {
    if (camera_relative_movement != FALSE &&
        camera_relative_movement != TRUE) {
        return FALSE;
    }
    return (bundle_mask == 0u && !camera_relative_movement) ||
        (bundle_mask == SUDEKIMP_TALOS_POST_MOVIE_CAMERA_BUNDLE_EXACT &&
         camera_relative_movement);
}

#if !defined(SUDEKIMP_TALOS_POST_MOVIE_PARTY_RESTORE_POLICY_ONLY)

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"

#include <stddef.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Talos post-movie party restoration requires 32-bit Windows GCC"
#endif

typedef uint8_t (__stdcall *AiCandidateFilterFunction)(
    const float *query,
    void *source,
    float range,
    void *candidate
);

enum {
    RVA_AI_CANDIDATE_FILTER = 0x001b6ec0u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    AI_UNIT_TYPE_OFFSET = 0x148u,
    AI_UNIT_TYPE_BOSS = 3u,
    CHARACTER_AI_COMPONENT_OFFSET = 0x94u,
    PARTY_SLOT_ZERO_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    PARTY_COUNT_OFFSET = 0xccu,
    PARTY_SLOT_COUNT = 4u,
    QUERY_COPY_SIZE = 0x28u,
    QUERY_FLAGS_OFFSET = 0x25u,
    QUERY_REJECT_BOSS_FLAG = 0x04u,
    COMPANION_COUNT = 3u
};

static const uint8_t ai_candidate_filter_entry[] = {
    0x8bu, 0x54u, 0x24u, 0x08u,
    0x8bu, 0x4cu, 0x24u, 0x10u
};
static const SudekiMpCleanroomActor companion_actors[COMPANION_COUNT] = {
    SUDEKIMP_CLEANROOM_AILISH,
    SUDEKIMP_CLEANROOM_BUKI,
    SUDEKIMP_CLEANROOM_ELCO
};
static const uint8_t companion_masks[COMPANION_COUNT] = {
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_AILISH_MASK,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_BUKI_MASK,
    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_ELCO_MASK
};
static const uint8_t observer_owner_identity;

static uint8_t *game_base;
static HMODULE pinned_module;
static SudekiMpInlineHook ai_candidate_filter_hook;
static AiCandidateFilterFunction original_ai_candidate_filter;
static SudekiMpControlUpdateObserverGate observer_gate;
static volatile LONG module_active;
static volatile LONG status_sequence;
static volatile LONG status_writer_lock;
static volatile LONG ai_filter_bypass_count;
static volatile LONG filter_session_active;
static SudekiMpTalosPostMoviePartyRestoreStatus public_status;
static SudekiMpTalosPostMoviePartyRestoreStatus published_status;
static void *companion_entities[COMPANION_COUNT];
static void *companion_ai[COMPANION_COUNT];
static void *real_boss_ai;
static void *filter_logged_source[COMPANION_COUNT];
static uint8_t target_policy_captured_mask;
static uint8_t original_allies_targeting_mask;
static BOOL target_policy_confirmed;
static BOOL boss_identity_seen;
static BOOL active_log_written;
static BOOL nested_native_control_window;
static BOOL failure_log_written;
static BOOL terminal_player_two_release_attempted;
static DWORD combat_ready_started_at;

static uint64_t advance_nonzero(uint64_t value) {
    ++value;
    return value == 0u ? 1u : value;
}

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

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void begin_status_write(void) {
    while (InterlockedCompareExchange(&status_writer_lock, 1, 0) != 0) {
    }
    (void)InterlockedIncrement(&status_sequence);
    MemoryBarrier();
}

static void end_status_write(void) {
    MemoryBarrier();
    (void)InterlockedIncrement(&status_sequence);
    (void)InterlockedExchange(&status_writer_lock, 0);
}

static void publish_status(void) {
    begin_status_write();
    public_status.status_serial = advance_nonzero(public_status.status_serial);
    public_status.ai_filter_bypass_count = (uint64_t)(uint32_t)
        InterlockedCompareExchange(&ai_filter_bypass_count, 0, 0);
    public_status.target_policy_active = target_policy_confirmed ? 1u : 0u;
    public_status.boss_filter_identity_ready = real_boss_ai != NULL ? 1u : 0u;
    public_status.combat_ready =
        public_status.machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE &&
        public_status.installed != 0u &&
        public_status.enabled != 0u &&
        public_status.observer_registered != 0u &&
        public_status.ai_filter_installed != 0u &&
        InterlockedCompareExchange(&module_active, 0, 0) != 0 &&
        InterlockedCompareExchange(&filter_session_active, 0, 0) != 0 &&
        target_policy_confirmed && real_boss_ai != NULL &&
        public_status.party_topology_exact != 0u &&
        public_status.control_state_exact != 0u ? 1u : 0u;
    public_status.valid =
        public_status.combat_ready != 0u &&
        public_status.machine.failure ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_NONE &&
        public_status.last_party_count == 4u &&
        public_status.machine.player_two_active != 0u &&
        public_status.player_two_input_ready != 0u &&
        public_status.machine.combat_refreshed != 0u ? 1u : 0u;
    if (public_status.machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        public_status.reload_required = 1u;
    }
    published_status = public_status;
    end_status_write();
}

static BOOL copy_status(
    SudekiMpTalosPostMoviePartyRestoreStatus *status
) {
    unsigned int attempt;

    if (status == NULL) {
        return FALSE;
    }
    for (attempt = 0u; attempt < 32u; ++attempt) {
        LONG before = InterlockedCompareExchange(&status_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) {
            continue;
        }
        MemoryBarrier();
        *status = published_status;
        MemoryBarrier();
        after = InterlockedCompareExchange(&status_sequence, 0, 0);
        if (before == after && (after & 1) == 0) {
            return TRUE;
        }
    }
    ZeroMemory(status, sizeof(*status));
    return FALSE;
}

static BOOL witness_entry_exact(
    const SudekiMpControlUpdateDispatchWitness *witness,
    void *controller,
    void *update_data
) {
    return controller != NULL && update_data != NULL && witness != NULL &&
        witness->dispatch_serial != 0u &&
        witness->native_thread_id == (uint32_t)GetCurrentThreadId() &&
        witness->outer_update_depth == 1u &&
        witness->active_dispatch_count == 1u &&
        witness->original_call_count == 1u &&
        witness->observer_snapshot_count == 1u &&
        witness->observer_registry_generation != 0u &&
        witness->hook_owned_exact == 1u &&
        witness->slot_owned_exact == 1u &&
        witness->service_only == 0u && witness->post_original == 1u &&
        witness->source ==
            SUDEKIMP_CONTROL_UPDATE_DISPATCH_SOURCE_NORMAL_POST_ORIGINAL &&
        witness->source_exact == 1u &&
        witness->service_post_original_exact == 0u &&
        witness->sole_observer == 1u &&
        witness->registry_generation_stable == 1u &&
        witness->reserved[0] == 0u && witness->reserved[1] == 0u &&
        witness->reserved[2] == 0u;
}

static BOOL witness_still_exact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    return witness != NULL &&
        witness->native_thread_id == (uint32_t)GetCurrentThreadId() &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(witness);
}

static BOOL ticket_shape_exact(
    const SudekiMpTalosNativePostMovieRestoreTicket *ticket,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    return ticket != NULL && witness != NULL &&
        (ticket->run_id_high != 0u || ticket->run_id_low != 0u) &&
        ticket->authorization_generation != 0u &&
        ticket->lifecycle_event_serial != 0u &&
        ticket->roster_observation_serial != 0u &&
        ticket->roster_revision != 0u &&
        ticket->kazel_observation_serial != 0u &&
        ticket->kazel_request_generation != 0u &&
        ticket->session_generation != 0u &&
        ticket->script_runtime_generation != 0u &&
        ticket->load_void_task_generation != 0u &&
        ticket->settle_validation_generation != 0u &&
        ticket->default_camera_generation != 0u &&
        ticket->native_thread_id == witness->native_thread_id &&
        ticket->native_thread_id == (uint32_t)GetCurrentThreadId() &&
        ticket->tal_token != 0u && ticket->kazel_token != 0u &&
        ticket->tal_token != ticket->kazel_token;
}

static uint8_t companion_presence_mask(void) {
    uint8_t mask = 0u;
    unsigned int index;

    for (index = 0u; index < COMPANION_COUNT; ++index) {
        if (SudekiMpCleanroomEngineActorPresent(companion_actors[index])) {
            mask = (uint8_t)(mask | companion_masks[index]);
        }
    }
    return mask;
}

static uint32_t party_presence_count(uint8_t companion_mask) {
    uint32_t count = SudekiMpCleanroomEngineActorPresent(
        SUDEKIMP_CLEANROOM_TAL) ? 1u : 0u;
    unsigned int index;

    for (index = 0u; index < COMPANION_COUNT; ++index) {
        if ((companion_mask & companion_masks[index]) != 0u) {
            ++count;
        }
    }
    return count;
}

static BOOL cache_companion_ai_components(void) {
    void *entities[COMPANION_COUNT];
    void *components[COMPANION_COUNT];
    unsigned int index;

    if (!SudekiMpCleanroomEngineExactRetailPartyReady()) {
        return FALSE;
    }
    for (index = 0u; index < COMPANION_COUNT; ++index) {
        uint8_t *entity = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            companion_actors[index]);
        void *ai;

        if (!readable_memory(entity, 0x98u)) {
            return FALSE;
        }
        ai = *(void **)(entity + 0x94u);
        if (!readable_memory(ai, 0x14cu)) {
            return FALSE;
        }
        entities[index] = entity;
        components[index] = ai;
    }
    memcpy(companion_entities, entities, sizeof(companion_entities));
    memcpy(companion_ai, components, sizeof(companion_ai));
    return TRUE;
}

static BOOL cached_companion_identities_exact(void) {
    unsigned int index;

    if (!SudekiMpCleanroomEngineExactRetailPartyReady()) {
        return FALSE;
    }
    for (index = 0u; index < COMPANION_COUNT; ++index) {
        uint8_t *entity = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            companion_actors[index]);

        if (entity == NULL || entity != companion_entities[index] ||
            !readable_memory(entity,
                CHARACTER_AI_COMPONENT_OFFSET + sizeof(void *)) ||
            *(void **)(entity + CHARACTER_AI_COMPONENT_OFFSET) !=
                companion_ai[index]) {
            return FALSE;
        }
    }
    return TRUE;
}

static void resolve_real_boss_ai(void) {
    uint8_t *entity = (uint8_t *)SudekiMpCleanroomEngineGenericEntity(
        "BOSS_Talos");
    void *candidate_ai;

    if (!readable_memory(entity, 0x98u)) {
        real_boss_ai = NULL;
        return;
    }
    candidate_ai = *(void **)(entity + 0x94u);
    if (!readable_memory(candidate_ai,
            AI_UNIT_TYPE_OFFSET + sizeof(uint32_t)) ||
        *(uint32_t *)((uint8_t *)candidate_ai + AI_UNIT_TYPE_OFFSET) !=
            AI_UNIT_TYPE_BOSS) {
        real_boss_ai = NULL;
        return;
    }
    real_boss_ai = candidate_ai;
}

static BOOL set_companion_target_policy(
    BOOL target_allies,
    const SudekiMpControlUpdateDispatchWitness *witness,
    BOOL require_exact_witness
) {
    unsigned int index;
    BOOL accepted = TRUE;

    for (index = 0u; index < COMPANION_COUNT; ++index) {
        BOOL current;
        BOOL desired;
        uint8_t mask = companion_masks[index];

        if (require_exact_witness && !witness_still_exact(witness)) {
            return FALSE;
        }
        if (!SudekiMpCleanroomEngineActorTargetsAllies(
                companion_actors[index], &current)) {
            if (!target_allies &&
                !SudekiMpCleanroomEngineActorPresent(
                    companion_actors[index])) {
                target_policy_captured_mask = (uint8_t)(
                    target_policy_captured_mask & ~mask);
                original_allies_targeting_mask = (uint8_t)(
                    original_allies_targeting_mask & ~mask);
                continue;
            }
            if (target_allies || (target_policy_captured_mask & mask) != 0u) {
                accepted = FALSE;
            }
            continue;
        }
        if (target_allies && (target_policy_captured_mask & mask) == 0u) {
            target_policy_captured_mask = (uint8_t)(
                target_policy_captured_mask | mask);
            if (current) {
                original_allies_targeting_mask = (uint8_t)(
                    original_allies_targeting_mask | mask);
            }
        }
        desired = target_allies ? TRUE :
            ((original_allies_targeting_mask & mask) != 0u ? TRUE : FALSE);
        if (current != desired) {
            if (require_exact_witness && !witness_still_exact(witness)) {
                return FALSE;
            }
            if (!SudekiMpCleanroomEngineSetActorTargetsAllies(
                    companion_actors[index], desired)) {
                accepted = FALSE;
                continue;
            }
        }
        if (!target_allies) {
            target_policy_captured_mask = (uint8_t)(
                target_policy_captured_mask & ~mask);
            original_allies_targeting_mask = (uint8_t)(
                original_allies_targeting_mask & ~mask);
        }
    }
    accepted = accepted && (!target_allies ||
        target_policy_captured_mask ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK);
    if (target_allies) {
        target_policy_confirmed = accepted;
    } else if (target_policy_captured_mask == 0u) {
        target_policy_confirmed = FALSE;
    }
    return accepted;
}

static BOOL cached_companion_is_current_group_member(
    unsigned int companion_index,
    void *source
) {
    uint8_t *group;
    void *entity;
    int count_before;
    int count_after;
    unsigned int slot;
    unsigned int matches = 0u;

    if (game_base == NULL || companion_index >= COMPANION_COUNT ||
        source == NULL || source != companion_ai[companion_index] ||
        !readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return FALSE;
    }
    entity = companion_entities[companion_index];
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (entity == NULL || !readable_memory(entity,
            CHARACTER_AI_COMPONENT_OFFSET + sizeof(void *)) ||
        *(void **)((uint8_t *)entity + CHARACTER_AI_COMPONENT_OFFSET) !=
            source ||
        !readable_memory(group,
            PARTY_COUNT_OFFSET + sizeof(count_before))) {
        return FALSE;
    }
    count_before = *(int *)(group + PARTY_COUNT_OFFSET);
    if (count_before != (int)PARTY_SLOT_COUNT ||
        *(void **)(group + PARTY_SLOT_ZERO_OFFSET) == entity) {
        return FALSE;
    }
    for (slot = 1u; slot < PARTY_SLOT_COUNT; ++slot) {
        if (*(void **)(group + PARTY_SLOT_ZERO_OFFSET +
                slot * PARTY_SLOT_STRIDE) == entity) {
            ++matches;
        }
    }
    count_after = *(int *)(group + PARTY_COUNT_OFFSET);
    return count_after == count_before && matches == 1u;
}

static int companion_index_for_ai(void *source) {
    unsigned int index;

    if (source == NULL || InterlockedCompareExchange(
            &module_active, 0, 0) == 0 ||
        InterlockedCompareExchange(&filter_session_active, 0, 0) == 0) {
        return -1;
    }
    for (index = 0u; index < COMPANION_COUNT; ++index) {
        if (companion_ai[index] == source &&
            cached_companion_is_current_group_member(index, source)) {
            return (int)index;
        }
    }
    return -1;
}

static uint8_t __stdcall filter_talos_boss_for_companions(
    const float *query,
    void *source,
    float range,
    void *candidate
) {
    DWORD incoming_error = GetLastError();
    DWORD original_error;
    uint8_t query_copy[QUERY_COPY_SIZE];
    uint8_t result;
    uint8_t flags;
    int companion_index;

    if (original_ai_candidate_filter == NULL) {
        SetLastError(incoming_error);
        return 0u;
    }
    companion_index = companion_index_for_ai(source);
    if (companion_index < 0 || candidate == NULL ||
        candidate != real_boss_ai ||
        !readable_memory(candidate,
            AI_UNIT_TYPE_OFFSET + sizeof(uint32_t)) ||
        *(uint32_t *)((uint8_t *)candidate + AI_UNIT_TYPE_OFFSET) !=
            AI_UNIT_TYPE_BOSS ||
        !readable_memory(query, sizeof(query_copy))) {
        SetLastError(incoming_error);
        result = original_ai_candidate_filter(query, source, range, candidate);
        original_error = GetLastError();
        SetLastError(original_error);
        return result;
    }
    flags = *((const uint8_t *)query + QUERY_FLAGS_OFFSET);
    if ((flags & QUERY_REJECT_BOSS_FLAG) == 0u) {
        SetLastError(incoming_error);
        result = original_ai_candidate_filter(query, source, range, candidate);
        original_error = GetLastError();
        SetLastError(original_error);
        return result;
    }
    memcpy(query_copy, query, sizeof(query_copy));
    query_copy[QUERY_FLAGS_OFFSET] = (uint8_t)(
        query_copy[QUERY_FLAGS_OFFSET] & ~QUERY_REJECT_BOSS_FLAG);
    SetLastError(incoming_error);
    result = original_ai_candidate_filter(
        (const float *)query_copy, source, range, candidate);
    original_error = GetLastError();
    (void)InterlockedIncrement(&ai_filter_bypass_count);
    if (filter_logged_source[companion_index] != source) {
        filter_logged_source[companion_index] = source;
        SudekiMpLogFormat(
            "talos_post_movie_party_restore event=boss_filter "
            "status=scoped_bypass actor=%s flags_before=0x%02x "
            "flags_after=0x%02x result=%u "
            "policy=temporary_query_copy_only\r\n",
            SudekiMpCleanroomActorLabel(
                companion_actors[companion_index]),
            (unsigned int)flags,
            (unsigned int)query_copy[QUERY_FLAGS_OFFSET],
            (unsigned int)result);
    }
    SetLastError(original_error);
    return result;
}

static const void *ai_candidate_filter_pointer(
    AiCandidateFilterFunction function
) {
    const void *pointer = NULL;

    if (sizeof(function) == sizeof(pointer)) {
        memcpy(&pointer, &function, sizeof(pointer));
    }
    return pointer;
}

static AiCandidateFilterFunction ai_candidate_filter_function(
    void *pointer
) {
    AiCandidateFilterFunction function = NULL;

    if (sizeof(function) == sizeof(pointer)) {
        memcpy(&function, &pointer, sizeof(function));
    }
    return function;
}

static BOOL ai_filter_inline_hook_targets_replacement(void) {
    AiCandidateFilterFunction replacement =
        filter_talos_boss_for_companions;
    const void *replacement_pointer = ai_candidate_filter_pointer(
        replacement);
    int32_t displacement;
    size_t index;

    if (replacement_pointer == NULL) {
        return FALSE;
    }
    if (!ai_candidate_filter_hook.installed ||
        ai_candidate_filter_hook.target == NULL ||
        !readable_memory(ai_candidate_filter_hook.target, 5u) ||
        ai_candidate_filter_hook.target[0] != 0xe9u) {
        return FALSE;
    }
    memcpy(&displacement, ai_candidate_filter_hook.target + 1u,
        sizeof(displacement));
    if ((uintptr_t)(ai_candidate_filter_hook.target + 5u + displacement) !=
            (uintptr_t)replacement_pointer) {
        return FALSE;
    }
    for (index = 5u; index < ai_candidate_filter_hook.length; ++index) {
        if (!readable_memory(ai_candidate_filter_hook.target + index, 1u) ||
            ai_candidate_filter_hook.target[index] != 0x90u) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL restore_ai_filter_if_owned(void) {
    if (!ai_candidate_filter_hook.installed ||
        ai_candidate_filter_hook.target == NULL) {
        return TRUE;
    }
    if (readable_memory(ai_candidate_filter_hook.target,
            ai_candidate_filter_hook.length) &&
        memcmp(ai_candidate_filter_hook.target,
            ai_candidate_filter_hook.original,
            ai_candidate_filter_hook.length) == 0) {
        if (ai_candidate_filter_hook.trampoline != NULL) {
            VirtualFree(
                ai_candidate_filter_hook.trampoline, 0u, MEM_RELEASE);
        }
        ZeroMemory(&ai_candidate_filter_hook,
            sizeof(ai_candidate_filter_hook));
        return TRUE;
    }
    if (!ai_filter_inline_hook_targets_replacement()) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return SudekiMpRestoreInlineHook(&ai_candidate_filter_hook);
}

static void abort_restore(
    SudekiMpTalosPostMoviePartyRestoreFailure failure
) {
    (void)InterlockedExchange(&filter_session_active, 0);
    if (public_status.machine.state !=
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
            &public_status.machine,
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_ABORT,
            (uint32_t)failure,
            GetTickCount());
    }
    public_status.reload_required = 1u;
    if (!failure_log_written) {
        failure_log_written = TRUE;
        SudekiMpLogFormat(
            "talos_post_movie_party_restore valid=false state=failed "
            "failure=%lu reload_required=true spawn_mask=0x%02x "
            "party_mask=0x%02x initialized_mask=0x%02x\r\n",
            (unsigned long)public_status.machine.failure,
            (unsigned int)public_status.machine.spawn_accepted_mask,
            (unsigned int)public_status.machine.party_present_mask,
            (unsigned int)public_status.machine.initialized_mask);
    }
}

static BOOL mutation_window_exact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    if (witness_still_exact(witness)) {
        return TRUE;
    }
    abort_restore(
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_WITNESS_LOST);
    return FALSE;
}

static void run_spawn_requests(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const float lead_position[3],
    DWORD now
) {
    uint8_t accepted_mask = 0u;
    unsigned int index;

    for (index = 0u; index < COMPANION_COUNT; ++index) {
        float spawn_position[3];
        BOOL accepted = FALSE;

        if (!mutation_window_exact(witness)) {
            break;
        }
        ++public_status.spawn_call_count;
        spawn_position[0] = lead_position[0] + 1.5f * (float)(index + 1u);
        spawn_position[1] = lead_position[1];
        spawn_position[2] = lead_position[2] - 1.5f;
        if (!SudekiMpCleanroomEngineActorPresent(companion_actors[index])) {
            accepted = SudekiMpCleanroomEngineSpawnActor(
                companion_actors[index], spawn_position);
        }
        if (accepted) {
            accepted_mask = (uint8_t)(
                accepted_mask | companion_masks[index]);
        }
        SudekiMpLogFormat(
            "talos_post_movie_party_restore event=spawn actor=%s "
            "status=%s x_bits=%08lx y_bits=%08lx z_bits=%08lx "
            "attempt=one_shot\r\n",
            SudekiMpCleanroomActorLabel(companion_actors[index]),
            accepted ? "requested" : "rejected",
            (unsigned long)float_bits(spawn_position[0]),
            (unsigned long)float_bits(spawn_position[1]),
            (unsigned long)float_bits(spawn_position[2]));
    }
    if (public_status.machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        return;
    }
    (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
        &public_status.machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SPAWN_RESULT,
        accepted_mask,
        now);
    if (public_status.machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_SPAWN_REJECTED);
    }
}

static void initialize_restored_actors(
    const SudekiMpControlUpdateDispatchWitness *witness,
    DWORD now
) {
    uint8_t initialized_mask = 0u;
    unsigned int index;

    if (!SudekiMpCleanroomEngineExactRetailPartyReady()) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
        return;
    }
    for (index = 0u; index < COMPANION_COUNT; ++index) {
        BOOL initialized;

        if (!mutation_window_exact(witness)) {
            break;
        }
        ++public_status.actor_initialize_call_count;
        initialized = SudekiMpCleanroomEngineInitializePartyActor(
            companion_actors[index]);
        if (initialized) {
            initialized_mask = (uint8_t)(
                initialized_mask | companion_masks[index]);
        }
        SudekiMpLogFormat(
            "talos_post_movie_party_restore event=actor_initialize "
            "actor=%s status=%s policy=narrow_party_actor_only\r\n",
            SudekiMpCleanroomActorLabel(companion_actors[index]),
            initialized ? "confirmed" : "rejected");
    }
    if (public_status.machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        return;
    }
    if (initialized_mask ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK) {
        if (!SudekiMpCleanroomEngineExactRetailPartyReady()) {
            abort_restore(
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
            return;
        }
        if (!cache_companion_ai_components()) {
            initialized_mask = 0u;
        }
    }
    (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
        &public_status.machine,
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_INITIALIZE_RESULT,
        initialized_mask,
        now);
    if (public_status.machine.state ==
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_ACTOR_INITIALIZE);
    }
}

static BOOL player_two_lease_exact(void) {
    void *ailish = SudekiMpCleanroomEngineActorEntity(
        SUDEKIMP_CLEANROOM_AILISH);

    return ailish != NULL && ailish == companion_entities[0] &&
        SudekiMpControlSeparationPlayerTwoRequested() &&
        SudekiMpControlSeparationPlayerTwoActive() &&
        SudekiMpControlSeparationPlayerTwoCharacter() == ailish &&
        SudekiMpControlSeparationInputReady();
}

static void service_active_session(
    const SudekiMpControlUpdateDispatchWitness *witness,
    DWORD now
) {
    uint8_t present = companion_presence_mask();
    BOOL exact_action_state;

    public_status.last_party_count = party_presence_count(present);
    public_status.party_topology_exact = 0u;
    public_status.control_state_exact = 0u;
    public_status.player_two_input_ready = 0u;
    if (present != SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK ||
        !SudekiMpCleanroomEngineActorPresent(SUDEKIMP_CLEANROOM_TAL)) {
        if (!set_companion_target_policy(FALSE, witness, TRUE)) {
            abort_restore(
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_WITNESS_LOST);
            return;
        }
        (void)InterlockedExchange(&filter_session_active, 0);
        ZeroMemory(companion_entities, sizeof(companion_entities));
        ZeroMemory(companion_ai, sizeof(companion_ai));
        ZeroMemory(filter_logged_source, sizeof(filter_logged_source));
        real_boss_ai = NULL;
        (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
            &public_status.machine,
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SESSION_ENDED,
            1u,
            now);
        SudekiMpLogWrite(
            "talos_post_movie_party_restore event=session status=finished "
            "reason=retail_party_lifecycle_ended\r\n");
        return;
    }
    if (!cached_companion_identities_exact()) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
        return;
    }
    public_status.last_party_count = 4u;
    public_status.party_topology_exact = 1u;
    if (!SudekiMpCleanroomEnginePostRestoreControlsActive() ||
        !player_two_lease_exact()) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE);
        return;
    }
    exact_action_state =
        SudekiMpCleanroomEngineExactPostRestoreControlsReady();
    if (!exact_action_state) {
        if (!nested_native_control_window) {
            nested_native_control_window = TRUE;
            SudekiMpLogWrite(
                "talos_post_movie_party_restore event=control_lease_window "
                "state=native_nested session=active "
                "policy=preserve_owned_lease_native_input_gates_remain_exact\r\n"
            );
        }
    } else if (nested_native_control_window) {
        nested_native_control_window = FALSE;
        SudekiMpLogWrite(
            "talos_post_movie_party_restore event=control_lease_window "
            "state=exact_owned session=active "
            "policy=native_nested_lease_released\r\n"
        );
    }
    /* The session remains healthy while Sudeki owns a balanced nested lease,
     * but action consumers deliberately require our exact-one lease. Do not
     * publish input-ready/valid during that native serialization window. */
    public_status.control_state_exact = exact_action_state ? 1u : 0u;
    public_status.player_two_input_ready = exact_action_state ? 1u : 0u;
    resolve_real_boss_ai();
    if (real_boss_ai != NULL) {
        boss_identity_seen = TRUE;
    } else if (boss_identity_seen) {
        if (!set_companion_target_policy(FALSE, witness, TRUE)) {
            abort_restore(
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_WITNESS_LOST);
            return;
        }
        (void)InterlockedExchange(&filter_session_active, 0);
        ZeroMemory(companion_entities, sizeof(companion_entities));
        ZeroMemory(companion_ai, sizeof(companion_ai));
        ZeroMemory(filter_logged_source, sizeof(filter_logged_source));
        (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
            &public_status.machine,
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_SESSION_ENDED,
            1u,
            now);
        SudekiMpLogWrite(
            "talos_post_movie_party_restore event=session status=finished "
            "reason=exact_type3_boss_identity_released\r\n");
        return;
    }
    if (!set_companion_target_policy(TRUE, witness, TRUE)) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_TARGET_POLICY);
        return;
    }
    if (real_boss_ai != NULL && exact_action_state && !active_log_written) {
        active_log_written = TRUE;
        SudekiMpLogWrite(
            "talos_post_movie_party_restore valid=true "
            "state=active party_count=4 p2=Ailish "
            "p2_active=true p2_input_ready=true target_policy_active=true "
            "boss_filter_identity_ready=true "
            "party_topology_exact=true control_state_exact=true\r\n");
    } else if (real_boss_ai == NULL &&
            (DWORD)(now - combat_ready_started_at) > 5000u) {
        abort_restore(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_BOSS_FILTER_IDENTITY);
    }
}

static void service_restore_machine(
    const SudekiMpControlUpdateDispatchWitness *witness,
    DWORD now
) {
    float lead_position[3];
    unsigned int immediate_steps = 0u;

    while (immediate_steps++ < 8u) {
        switch ((SudekiMpTalosPostMoviePartyRestoreState)
                public_status.machine.state) {
            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_TICKET:
                ++public_status.ticket_claim_call_count;
                ZeroMemory(&public_status.ticket,
                    sizeof(public_status.ticket));
                if (!SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(
                        &public_status.ticket)) {
                    return;
                }
                if (!ticket_shape_exact(&public_status.ticket, witness)) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_TICKET_SHAPE);
                    return;
                }
                (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                    &public_status.machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_TICKET_CLAIMED,
                    1u,
                    now);
                SudekiMpLogFormat(
                    "talos_post_movie_party_restore event=ticket_claim "
                    "status=confirmed authorization_generation=%lu "
                    "session_generation=%lu runtime_generation=%lu "
                    "task_generation=%lu native_thread_id=%lu "
                    "policy=exact_kazel_delete_and_tsa_settle_one_shot\r\n",
                    (unsigned long)public_status.ticket.authorization_generation,
                    (unsigned long)public_status.ticket.session_generation,
                    (unsigned long)public_status.ticket.script_runtime_generation,
                    (unsigned long)public_status.ticket.load_void_task_generation,
                    (unsigned long)public_status.ticket.native_thread_id);
                break;

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_PREFLIGHT:
                if (!mutation_window_exact(witness)) {
                    return;
                }
                if (!SudekiMpCleanroomEngineWorldReady() ||
                    !SudekiMpCleanroomEngineActorPresent(
                        SUDEKIMP_CLEANROOM_TAL) ||
                    companion_presence_mask() != 0u ||
                    !SudekiMpCleanroomEngineActorPosition(
                        SUDEKIMP_CLEANROOM_TAL, lead_position)) {
                    (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                        &public_status.machine,
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_REJECTED,
                        0u,
                        now);
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PREFLIGHT);
                    return;
                }
                public_status.last_party_count = 1u;
                (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                    &public_status.machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PREFLIGHT_ACCEPTED,
                    1u,
                    now);
                break;

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_SPAWN_REQUESTS:
                if (!SudekiMpCleanroomEngineActorPosition(
                        SUDEKIMP_CLEANROOM_TAL, lead_position)) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PREFLIGHT);
                    return;
                }
                run_spawn_requests(witness, lead_position, now);
                return;

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY: {
                uint8_t present = companion_presence_mask();
                BOOL topology_exact = present ==
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_COMPANION_MASK &&
                    SudekiMpCleanroomEngineExactRetailPartyReady();

                public_status.last_party_count = party_presence_count(present);
                public_status.party_topology_exact =
                    topology_exact ? 1u : 0u;
                if (!SudekiMpCleanroomEngineActorPresent(
                        SUDEKIMP_CLEANROOM_TAL)) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PREFLIGHT);
                    return;
                }
                (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                    &public_status.machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PARTY_OBSERVED,
                    topology_exact ? present : 0u,
                    now);
                if (public_status.machine.state ==
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_POSTSPAWN_TIMEOUT);
                    return;
                }
                if (public_status.machine.state ==
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PARTY) {
                    return;
                }
                break;
            }

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_INITIALIZE_ACTORS:
                initialize_restored_actors(witness, now);
                if (public_status.machine.state ==
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
                    return;
                }
                break;

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REQUEST_PLAYER_TWO: {
                void *ailish = SudekiMpCleanroomEngineActorEntity(
                    SUDEKIMP_CLEANROOM_AILISH);
                BOOL accepted;
                BOOL party_exact = ailish != NULL &&
                    ailish == companion_entities[0] &&
                    cached_companion_identities_exact();

                if (!party_exact || !mutation_window_exact(witness)) {
                    if (!party_exact) {
                        abort_restore(
                            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
                    }
                    return;
                }
                ++public_status.player_two_request_call_count;
                accepted =
                    SudekiMpControlSeparationRequestPlayerTwoCharacter(ailish);
                (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                    &public_status.machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_REQUEST_RESULT,
                    accepted ? 1u : 0u,
                    now);
                SudekiMpLogFormat(
                    "talos_post_movie_party_restore event=player_two_handoff "
                    "actor=Ailish status=%s policy=exact_roster_request\r\n",
                    accepted ? "requested" : "rejected");
                if (!accepted) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_REQUEST);
                }
                return;
            }

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_WAITING_PLAYER_TWO: {
                void *ailish = SudekiMpCleanroomEngineActorEntity(
                    SUDEKIMP_CLEANROOM_AILISH);
                BOOL active = ailish != NULL &&
                    SudekiMpControlSeparationPlayerTwoRequested() &&
                    SudekiMpControlSeparationPlayerTwoActive() &&
                    SudekiMpControlSeparationPlayerTwoCharacter() == ailish;

                if (!cached_companion_identities_exact()) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
                    return;
                }

                (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                    &public_status.machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_PLAYER_TWO_OBSERVED,
                    active ? 1u : 0u,
                    now);
                if (public_status.machine.state ==
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_TIMEOUT);
                    return;
                }
                if (!active) {
                    return;
                }
                break;
            }

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_REFRESH_COMBAT: {
                BOOL refreshed;

                if (!mutation_window_exact(witness)) {
                    return;
                }
                if (combat_ready_started_at == 0u) {
                    combat_ready_started_at = now;
                }
                if (!cached_companion_identities_exact()) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
                    return;
                }
                public_status.last_party_count = 4u;
                public_status.party_topology_exact = 1u;
                if (!SudekiMpCleanroomEngineExactPostRestoreControlsReady() ||
                    !player_two_lease_exact()) {
                    public_status.control_state_exact = 0u;
                    public_status.player_two_input_ready = 0u;
                    if ((DWORD)(now - combat_ready_started_at) >
                            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_PLAYER_TWO_TIMEOUT_MS) {
                        abort_restore(
                            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE);
                    }
                    return;
                }
                public_status.control_state_exact = 1u;
                public_status.player_two_input_ready = 1u;
                resolve_real_boss_ai();
                if (real_boss_ai == NULL) {
                    if ((DWORD)(now - combat_ready_started_at) > 5000u) {
                        abort_restore(
                            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_BOSS_FILTER_IDENTITY);
                    }
                    return;
                }
                boss_identity_seen = TRUE;
                (void)InterlockedExchange(&filter_session_active, 1);
                if (!set_companion_target_policy(TRUE, witness, TRUE)) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_TARGET_POLICY);
                    return;
                }
                if (!mutation_window_exact(witness)) {
                    return;
                }
                ++public_status.combat_refresh_call_count;
                refreshed = SudekiMpCleanroomEngineRefreshCombatMode();
                if (!refreshed) {
                    (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                        &public_status.machine,
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_COMBAT_REFRESH_RESULT,
                        0u,
                        now);
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_COMBAT_REFRESH);
                    return;
                }
                if (!mutation_window_exact(witness)) {
                    return;
                }
                if (!cached_companion_identities_exact()) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PARTY_TOPOLOGY);
                    return;
                }
                if (!SudekiMpCleanroomEngineExactPostRestoreControlsReady() ||
                    !player_two_lease_exact()) {
                    abort_restore(
                        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CONTROL_STATE);
                    return;
                }
                public_status.last_party_count = 4u;
                public_status.party_topology_exact = 1u;
                public_status.control_state_exact = 1u;
                public_status.player_two_input_ready = 1u;
                (void)SudekiMpTalosPostMoviePartyRestoreMachineAdvance(
                    &public_status.machine,
                    SUDEKIMP_TALOS_POST_MOVIE_RESTORE_EVENT_COMBAT_REFRESH_RESULT,
                    1u,
                    now);
                return;
            }

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_ACTIVE:
                service_active_session(witness, now);
                return;

            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED:
            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED:
            case SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_DISABLED:
            default:
                return;
        }
    }
}

static void restore_target_policy_after_terminal_state(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    uint32_t state = public_status.machine.state;

    if ((state != SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED &&
         state != SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) ||
        target_policy_captured_mask == 0u) {
        return;
    }
    if (!witness_still_exact(witness) ||
        !set_companion_target_policy(FALSE, witness, TRUE)) {
        public_status.reload_required = 1u;
        return;
    }
    SudekiMpLogWrite(
        "talos_post_movie_party_restore event=target_policy_restore "
        "status=confirmed context=exact_update_terminal\r\n");
}

static void release_player_two_after_terminal_state(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    uint32_t state = public_status.machine.state;
    BOOL live_active;
    BOOL live_requested;
    BOOL released;

    if (terminal_player_two_release_attempted ||
        (state != SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FINISHED &&
         state != SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED) ||
        !witness_still_exact(witness)) {
        return;
    }
    live_active = SudekiMpControlSeparationPlayerTwoActive();
    live_requested = SudekiMpControlSeparationPlayerTwoRequested();
    if (!live_active && !live_requested &&
        public_status.machine.player_two_active == 0u &&
        public_status.machine.player_two_requested == 0u) {
        return;
    }
    if (live_active) {
        released = SudekiMpControlSeparationReleasePlayerTwoNow();
    } else if (live_requested) {
        released = SudekiMpControlSeparationRequestPlayerTwoCharacter(NULL);
    } else {
        released = TRUE;
    }
    if (released) {
        terminal_player_two_release_attempted = TRUE;
        public_status.machine.player_two_active = 0u;
        public_status.machine.player_two_requested = 0u;
        public_status.player_two_input_ready = 0u;
        SudekiMpLogWrite(
            "talos_post_movie_party_restore event=player_two_release "
            "actor=Ailish status=confirmed context=exact_update_terminal\r\n");
        return;
    }
    public_status.machine.state =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED;
    public_status.machine.failure =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_PLAYER_TWO_REQUEST;
    public_status.reload_required = 1u;
    SudekiMpLogWrite(
        "talos_post_movie_party_restore event=player_two_release "
        "actor=Ailish status=rejected context=exact_update_terminal "
        "reload_required=true\r\n");
}

static void talos_post_movie_update_observer(
    void *controller,
    void *update_data,
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    DWORD saved_error = GetLastError();
    DWORD now;

    if (!SudekiMpControlUpdateObserverGateTryEnter(&observer_gate)) {
        SetLastError(saved_error);
        return;
    }
    if (InterlockedCompareExchange(&module_active, 0, 0) == 0) {
        SudekiMpControlUpdateObserverGateLeave(&observer_gate);
        SetLastError(saved_error);
        return;
    }
    if (!witness_entry_exact(witness, controller, update_data) ||
        !witness_still_exact(witness)) {
        ++public_status.rejected_update_count;
        publish_status();
        SudekiMpControlUpdateObserverGateLeave(&observer_gate);
        SetLastError(saved_error);
        return;
    }
    ++public_status.exact_update_count;
    public_status.last_native_thread_id = witness->native_thread_id;
    public_status.last_witness_dispatch_serial = witness->dispatch_serial;
    public_status.last_witness_registry_generation =
        witness->observer_registry_generation;
    public_status.last_witness_overlap_generation =
        witness->dispatch_overlap_generation;
    now = GetTickCount();
    service_restore_machine(witness, now);
    restore_target_policy_after_terminal_state(witness);
    release_player_two_after_terminal_state(witness);
    publish_status();
    SudekiMpControlUpdateObserverGateLeave(&observer_gate);
    SetLastError(saved_error);
}

static void publish_install_failure(
    SudekiMpTalosPostMoviePartyRestoreFailure failure
) {
    public_status.machine.state =
        SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED;
    public_status.machine.failure = (uint32_t)failure;
    public_status.reload_required = 1u;
    publish_status();
    SudekiMpLogFormat(
        "talos_post_movie_party_restore valid=false state=failed "
        "failure=%lu reload_required=true phase=install\r\n",
        (unsigned long)failure);
}

BOOL SudekiMpInstallTalosPostMoviePartyRestore(
    HMODULE game_module,
    BOOL enabled
) {
    uint8_t *base;
    const void *replacement = ai_candidate_filter_pointer(
        filter_talos_boss_for_companions);

    if (InterlockedCompareExchange(&module_active, 0, 0) != 0 ||
        public_status.installed != 0u) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    ZeroMemory(&public_status, sizeof(public_status));
    ZeroMemory(companion_entities, sizeof(companion_entities));
    ZeroMemory(companion_ai, sizeof(companion_ai));
    ZeroMemory(filter_logged_source, sizeof(filter_logged_source));
    target_policy_captured_mask = 0u;
    original_allies_targeting_mask = 0u;
    target_policy_confirmed = FALSE;
    real_boss_ai = NULL;
    boss_identity_seen = FALSE;
    active_log_written = FALSE;
    nested_native_control_window = FALSE;
    failure_log_written = FALSE;
    terminal_player_two_release_attempted = FALSE;
    combat_ready_started_at = 0u;
    (void)InterlockedExchange(&ai_filter_bypass_count, 0);
    (void)InterlockedExchange(&filter_session_active, 0);
    SudekiMpTalosPostMoviePartyRestoreMachineInitialize(
        &public_status.machine, enabled);
    public_status.enabled = enabled ? 1u : 0u;
    if (!enabled) {
        publish_status();
        return TRUE;
    }
    if (game_module == NULL || replacement == NULL) {
        publish_install_failure(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_INSTALL_ARGUMENT);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pinned_module == NULL && !GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(const void *)&observer_owner_identity,
            &pinned_module)) {
        publish_install_failure(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_DLL_PIN);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (!SudekiMpCleanroomEngineInitialize(game_module)) {
        publish_install_failure(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_CLEANROOM_INITIALIZE);
        return FALSE;
    }
    public_status.cleanroom_initialized = 1u;
    game_base = base;
    if (!SudekiMpInstallInlineHook(
            &ai_candidate_filter_hook,
            base + RVA_AI_CANDIDATE_FILTER,
            ai_candidate_filter_entry,
            sizeof(ai_candidate_filter_entry),
            replacement)) {
        game_base = NULL;
        SudekiMpCleanroomEngineReset();
        public_status.cleanroom_initialized = 0u;
        publish_install_failure(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_AI_FILTER_HOOK);
        return FALSE;
    }
    public_status.ai_filter_installed = 1u;
    original_ai_candidate_filter = ai_candidate_filter_function(
        ai_candidate_filter_hook.trampoline);
    if (original_ai_candidate_filter == NULL) {
        DWORD rollback_error = ERROR_INVALID_ADDRESS;

        if (!restore_ai_filter_if_owned()) {
            rollback_error = GetLastError();
            public_status.installed = 1u;
            publish_install_failure(
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_AI_FILTER_HOOK);
            SetLastError(rollback_error);
            return FALSE;
        }
        public_status.ai_filter_installed = 0u;
        game_base = NULL;
        SudekiMpCleanroomEngineReset();
        public_status.cleanroom_initialized = 0u;
        publish_install_failure(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_AI_FILTER_HOOK);
        SetLastError(rollback_error);
        return FALSE;
    }
    if (!SudekiMpControlUpdateObserverGateEnable(&observer_gate) ||
        !SudekiMpControlSeparationRegisterUpdateObserver(
            &observer_owner_identity,
            talos_post_movie_update_observer)) {
        DWORD rollback_error;

        SudekiMpControlUpdateObserverGateDisable(&observer_gate);
        SudekiMpControlUpdateObserverGateDrain(&observer_gate);
        if (!restore_ai_filter_if_owned()) {
            rollback_error = GetLastError();
            public_status.installed = 1u;
            publish_install_failure(
                SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_OBSERVER_REGISTER);
            SetLastError(rollback_error);
            return FALSE;
        }
        original_ai_candidate_filter = NULL;
        public_status.ai_filter_installed = 0u;
        game_base = NULL;
        SudekiMpCleanroomEngineReset();
        public_status.cleanroom_initialized = 0u;
        publish_install_failure(
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_OBSERVER_REGISTER);
        return FALSE;
    }
    public_status.observer_registered = 1u;
    public_status.installed = 1u;
    (void)InterlockedExchange(&module_active, 1);
    publish_status();
    SudekiMpLogWrite(
        "talos_post_movie_party_restore_install=success enabled=true "
        "observer=normal_post_original ai_filter_rva=0x001b6ec0 "
        "policy=exact_post_kazel_ticket_one_shot\r\n");
    return TRUE;
}

void SudekiMpUninstallTalosPostMoviePartyRestore(void) {
    DWORD saved_error = GetLastError();
    DWORD teardown_error;

    (void)InterlockedExchange(&module_active, 0);
    (void)InterlockedExchange(&filter_session_active, 0);
    SudekiMpControlUpdateObserverGateDisable(&observer_gate);
    if (public_status.observer_registered != 0u) {
        (void)SudekiMpControlSeparationUnregisterUpdateObserver(
            &observer_owner_identity);
    }
    SudekiMpControlUpdateObserverGateDrain(&observer_gate);
    public_status.observer_registered = 0u;
    /* Native P2/target-policy rollback belongs to the exact game-thread
     * terminal callback. Uninstall is startup/rollback/process-teardown only
     * and must not issue control or targeter calls from an arbitrary thread. */
    if (!restore_ai_filter_if_owned()) {
        teardown_error = GetLastError();
        public_status.installed = 1u;
        public_status.ai_filter_installed = 1u;
        public_status.machine.state =
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_STATE_FAILED;
        public_status.machine.failure =
            SUDEKIMP_TALOS_POST_MOVIE_RESTORE_FAILURE_AI_FILTER_HOOK;
        public_status.party_topology_exact = 0u;
        public_status.control_state_exact = 0u;
        public_status.player_two_input_ready = 0u;
        public_status.reload_required = 1u;
        publish_status();
        SetLastError(teardown_error);
        return;
    }
    original_ai_candidate_filter = NULL;
    public_status.ai_filter_installed = 0u;
    if (public_status.cleanroom_initialized != 0u) {
        SudekiMpCleanroomEngineReset();
    }
    public_status.cleanroom_initialized = 0u;
    public_status.installed = 0u;
    public_status.enabled = 0u;
    game_base = NULL;
    real_boss_ai = NULL;
    ZeroMemory(companion_entities, sizeof(companion_entities));
    ZeroMemory(companion_ai, sizeof(companion_ai));
    ZeroMemory(filter_logged_source, sizeof(filter_logged_source));
    publish_status();
    SetLastError(saved_error);
}

BOOL SudekiMpTalosPostMoviePartyRestoreGetStatus(
    SudekiMpTalosPostMoviePartyRestoreStatus *status
) {
    DWORD saved_error = GetLastError();
    BOOL copied = copy_status(status);
    SetLastError(saved_error);
    return copied;
}

#endif
