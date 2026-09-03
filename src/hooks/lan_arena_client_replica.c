#include "hooks/lan_arena_client_replica.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "network/lan_arena_replica.h"
#include "network/lan_arena_session.h"
#include "network/lan_arena_shared_simulation.h"
#include "network/lan_arena_tal_combo_graph.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__attribute__((fastcall)) *PositionSetterFunction)(
    void *position,
    const float *coordinates
);
typedef const float *(__attribute__((thiscall)) *PositionWorldMatrixFunction)(
    void *position
);
typedef unsigned int (__attribute__((thiscall)) *AnimationCountFunction)(void *renderer);
typedef int (__attribute__((thiscall)) *AnimationLookupFunction)(
    void *renderer, int animation_handle);
typedef int (__attribute__((thiscall)) *AnimationSelectorGetFunction)(
    void *renderer, int channel, unsigned int submodel);
typedef void (__attribute__((thiscall)) *AnimationSelectorSetFunction)(
    void *renderer, int channel, unsigned int submodel, int selector);
typedef float (__attribute__((thiscall)) *AnimationValueGetFunction)(
    void *renderer, int channel, unsigned int submodel);
typedef void (__attribute__((thiscall)) *AnimationValueSetFunction)(
    void *renderer, int channel, unsigned int submodel, float value);
typedef void (__attribute__((thiscall)) *AnimationTimeSetFunction)(
    void *renderer, int channel, unsigned int submodel, float value, int force);
typedef unsigned char (__attribute__((thiscall)) *AnimationStateGetFunction)(
    void *renderer, int channel, unsigned int submodel);
typedef void (__attribute__((thiscall)) *AnimationStateSetFunction)(
    void *renderer, int channel, unsigned int submodel, int state);
typedef float (__attribute__((thiscall)) *AnimationBlendGetFunction)(
    void *renderer, int channel);
typedef void (__attribute__((thiscall)) *AnimationBlendSetFunction)(
    void *renderer, int channel, float blend);

typedef struct LanArenaAnimationMethods {
    AnimationCountFunction count;
    AnimationSelectorSetFunction set_selector;
    AnimationSelectorGetFunction get_selector;
    AnimationValueSetFunction set_rate;
    AnimationValueGetFunction get_rate;
    AnimationTimeSetFunction set_time;
    AnimationValueGetFunction get_time;
    AnimationStateSetFunction set_state;
    AnimationStateGetFunction get_state;
    AnimationBlendSetFunction set_blend;
    AnimationBlendGetFunction get_blend;
} LanArenaAnimationMethods;

typedef struct LanArenaPresentationLease {
    void *character;
    void *renderer;
    uint8_t animation_state;
    uint8_t combat_state;
    uint8_t action_variant;
    uint16_t action_sequence;
    BOOL combat_mode;
    BOOL valid;
} LanArenaPresentationLease;

typedef struct LanArenaFirstPersonLease {
    void *character;
    void *component;
    void *renderer;
    uint16_t action_sequence;
    BOOL weak_attack;
    BOOL valid;
} LanArenaFirstPersonLease;

enum {
    RVA_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_POSITION_UPDATE = 0x00110d40u,
    RVA_POSITION_WORLD_MATRIX = 0x00111cc0u,
    RVA_POSITION_WORLD_MATRIX_UPDATE_CALL = 0x00111cdau,
    RVA_POSITION_SET_FORWARD = 0x001114d0u,
    RVA_ANIMATION_RENDERER_VTABLE = 0x002df8ecu,
    RVA_ANIMATION_RENDERER_LOOKUP = 0x0021bac0u,
    RVA_ANIMATION_RENDERER_COUNT = 0x0021bb10u,
    RVA_ANIMATION_RENDERER_SELECTOR_SET = 0x00223000u,
    RVA_ANIMATION_RENDERER_SELECTOR_GET = 0x002230b0u,
    RVA_ANIMATION_RENDERER_RATE_SET = 0x002230d0u,
    RVA_ANIMATION_RENDERER_RATE_GET = 0x00223160u,
    RVA_ANIMATION_RENDERER_TIME_SET = 0x00223180u,
    RVA_ANIMATION_RENDERER_TIME_GET = 0x00223220u,
    RVA_ANIMATION_RENDERER_STATE_SET = 0x00223240u,
    RVA_ANIMATION_RENDERER_STATE_GET = 0x00223290u,
    RVA_ANIMATION_RENDERER_BLEND_SET = 0x002234c0u,
    RVA_ANIMATION_RENDERER_BLEND_GET = 0x002234e0u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    CHARACTER_POSITION_OFFSET = 0x44u,
    AILISH_RANGED_COMPONENT_OFFSET = 0x134u,
    AILISH_ANIMATION_TABLE_OFFSET = 0xdcu,
    AILISH_ANIMATION_BANK_OFFSET = 0x133u,
    AILISH_FIRST_PERSON_WRAPPER_OFFSET = 0x160u,
    AILISH_WORLD_WRAPPER_OFFSET = 0x164u,
    AILISH_FIRST_PERSON_ARBITER_FLAG = 0x00400000u,
    POSITION_ATTACHED_WRAPPER_OFFSET = 0xb4u,
    /* Exact cleanroom-world presentation captured from the supported retail
     * image.  These are renderer selectors, not protocol values: the LAN
     * snapshot remains semantic (idle/moving/action) across the wire. */
    TAL_WORLD_IDLE_SELECTOR = 4,
    TAL_WORLD_IDLE_VARIANT_ONE_SELECTOR = 10,
    TAL_WORLD_IDLE_VARIANT_TWO_SELECTOR = 11,
    TAL_WORLD_MOVE_PRIMARY_SELECTOR = 8,
    TAL_WORLD_MOVE_SECONDARY_SELECTOR = 9,
    AILISH_WORLD_IDLE_SELECTOR = 1,
    AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR = 4,
    AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR = 5,
    AILISH_WORLD_MOVE_PRIMARY_SELECTOR = 7,
    AILISH_WORLD_MOVE_SECONDARY_SELECTOR = 8,
    /* Exact host captures from the supported cleanroom combat transition.
     * These remain process-local renderer IDs; the wire protocol carries
     * only semantic idle/moving/action state plus the verified combat bit. */
    TAL_COMBAT_ENTRY_SELECTOR = 3,
    TAL_COMBAT_IDLE_SELECTOR = 17,
    TAL_COMBAT_MOVE_PRIMARY_SELECTOR = 36,
    TAL_COMBAT_MOVE_SECONDARY_SELECTOR = 32,
    AILISH_COMBAT_ENTRY_SELECTOR = 12,
    AILISH_COMBAT_IDLE_SELECTOR = 20,
    AILISH_COMBAT_MOVE_PRIMARY_SELECTOR = 22,
    AILISH_COMBAT_MOVE_SECONDARY_SELECTOR = 23,
    AILISH_COMBAT_WEAK_SELECTOR = 59,
    AILISH_FIRST_PERSON_IDLE_SELECTOR = 1,
    AILISH_FIRST_PERSON_WEAK_SELECTOR = 2,
    /* Exploration-only native Ailish action captured before LAN combat. */
    AILISH_WORLD_WEAK_SELECTOR = 55,
    ACTION_PHASE_TIME_TOLERANCE_MILLI = 10
};

static const float TAL_WORLD_MOVE_PRIMARY_RATE = 37.17093f;
static const float TAL_WORLD_MOVE_SECONDARY_RATE = 30.97577f;
static const float AILISH_WORLD_MOVE_PRIMARY_RATE = 41.22882f;
static const float AILISH_WORLD_MOVE_SECONDARY_RATE = 30.92161f;

static const uint8_t expected_position_setter_prefix[] = {
    0xd9u, 0x41u, 0x18u, 0xd9u, 0x02u, 0xdau, 0xe9u,
    0xdfu, 0xe0u, 0xf6u, 0xc4u, 0x44u
};
static const uint8_t expected_position_set_forward_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf0u, 0x83u, 0xecu,
    0x60u, 0xd9u, 0xeeu, 0xd9u, 0x54u, 0x24u, 0x14u
};
static const uint8_t expected_position_world_matrix_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x51u, 0x56u,
    0x8bu, 0xf1u, 0x8bu, 0x86u, 0x94u, 0x00u, 0x00u, 0x00u,
    0x85u, 0xc0u, 0x74u, 0x05u, 0x83u, 0xc0u, 0xfcu, 0x75u, 0x11u
};
static const uint8_t expected_position_update_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf0u, 0x81u, 0xecu,
    0xe4u, 0x00u, 0x00u, 0x00u, 0x53u, 0x8bu, 0x5du, 0x08u
};

static PositionSetterFunction set_position;
static PositionWorldMatrixFunction position_world_matrix;
static void *set_forward;
static uint8_t *game_base;
static SudekiMpLanArenaReplica replica;
static SudekiMpLanArenaReplicaRenderClock replica_render_clock;
static SudekiMpLanArenaSharedSimulation replica_simulation;
static LanArenaPresentationLease presentation_leases[2];
static LanArenaFirstPersonLease ailish_first_person_lease;
static const char *ailish_first_person_failure;
static SudekiMpLanArenaReplicaDiagnostics replica_diagnostics;
static SudekiMpLanArenaSnapshot last_applied_snapshot;
static void *last_applied_characters[2];
static void *last_applied_positions[2];
static BOOL client_combat_mode_lease_valid;
static BOOL client_original_combat_mode;
static int client_combat_mode_trace_state = -1;
static BOOL client_combat_transition_pending;
static BOOL client_combat_transition_target;
static DWORD client_combat_transition_started_at;
static int client_combat_transition_trace_state = -1;
static const char *client_ailish_combat_graph_failure;

static BOOL client_ailish_combat_graph_ready(void);

BOOL SudekiMpLanArenaClientTalActionPresentation(
    uint8_t action_variant,
    int *selector,
    int *state
) {
    return SudekiMpLanArenaTalActionToNativePresentation(
        action_variant, selector, state);
}

BOOL SudekiMpLanArenaClientIdleVariantSelector(
    uint8_t actor_type,
    uint8_t animation_state,
    int *selector
) {
    if (selector == NULL ||
        (actor_type != SUDEKIMP_LAN_ARENA_TAL_TYPE &&
         actor_type != SUDEKIMP_LAN_ARENA_AILISH_TYPE) ||
        (animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE &&
         animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO)) {
        return FALSE;
    }
    if (actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE) {
        *selector = animation_state ==
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE ?
            TAL_WORLD_IDLE_VARIANT_ONE_SELECTOR :
            TAL_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    } else {
        *selector = animation_state ==
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE ?
            AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR :
            AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    }
    return TRUE;
}

BOOL SudekiMpLanArenaClientAnimationShouldResetTime(
    unsigned int actor_index,
    uint8_t previous_animation_state,
    uint8_t next_animation_state,
    BOOL renderer_already_matches_target
) {
    if (actor_index >= 2u || renderer_already_matches_target ||
        previous_animation_state == next_animation_state) {
        return FALSE;
    }
    if (next_animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE) {
        return actor_index == 0u &&
            previous_animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    }
    return TRUE;
}

int SudekiMpLanArenaClientAnimationTransitionState(
    unsigned int actor_index,
    uint8_t previous_animation_state,
    uint8_t next_animation_state,
    int requested_state
) {
    if (actor_index == 0u &&
        previous_animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        next_animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE) {
        return 0;
    }
    return requested_state;
}

BOOL SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
    unsigned int actor_index,
    BOOL logical_transition,
    BOOL moving,
    BOOL base_target_already_matches
) {
    if (!logical_transition) return FALSE;
    if (actor_index == 0u && !moving) return TRUE;
    return moving && !base_target_already_matches;
}

BOOL SudekiMpLanArenaClientPresentationOverrideAllowed(BOOL combat_mode) {
    (void)combat_mode;
    return TRUE;
}

BOOL SudekiMpLanArenaClientActorPresentationAllowed(
    unsigned int actor_index,
    BOOL transition_pending,
    BOOL tal_ready,
    BOOL ailish_ready
) {
    if (actor_index >= 2u) return FALSE;
    if (!transition_pending) return TRUE;
    return actor_index == 0u ? tal_ready : ailish_ready;
}

BOOL SudekiMpLanArenaClientShouldApplyHostFacing(
    unsigned int actor_index,
    BOOL local_first_person_active
) {
    return actor_index < 2u &&
        !(actor_index == 1u && local_first_person_active);
}

BOOL SudekiMpLanArenaClientActionPhaseTime(
    const SudekiMpLanArenaActorSnapshot *snapshot,
    float *phase_time
) {
    if (snapshot == NULL || phase_time == NULL ||
        snapshot->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION ||
        !snapshot->action_phase_valid) return FALSE;
    *phase_time = (float)snapshot->action_phase_q8 /
        SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE;
    return isfinite(*phase_time);
}

static BOOL readable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    if (pointer == NULL || length == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u ||
        address + length < address ||
        address + length > (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    return TRUE;
}

static BOOL writable_memory(void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    DWORD protection;
    if (!readable_memory(pointer, length) ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        address + length < address ||
        address + length >
            (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL client_ailish_first_person_camera_owns_facing(
    uint8_t *character
) {
    uint8_t *arbiter;
    if (!readable_memory(character, 0x94u)) return FALSE;
    arbiter = *(uint8_t **)(character + 0x90u);
    return readable_memory(arbiter, 0x54u) &&
        *(void **)(arbiter + 0x10u) == character &&
        (*(uint32_t *)(arbiter + 0x50u) &
         AILISH_FIRST_PERSON_ARBITER_FLAG) != 0u;
}

static BOOL relative_call_targets(
    const uint8_t *instruction,
    const uint8_t *expected_target
) {
    int32_t displacement;
    if (!readable_memory(instruction, 5u) || instruction[0] != 0xe8u) {
        return FALSE;
    }
    memcpy(&displacement, instruction + 1u, sizeof(displacement));
    return instruction + 5u + displacement == expected_target;
}

static BOOL client_session_status(
    SudekiMpLanArenaSessionStatus *status
) {
    SudekiMpLanArenaSessionStatus local_status;
    SudekiMpLanArenaSessionStatus *result =
        status == NULL ? &local_status : status;
    return SudekiMpLanArenaSessionGetStatus(result) &&
        result->peer_connected &&
        result->local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        result->session_token != 0u;
}

static BOOL client_session_authenticated(void) {
    SudekiMpLanArenaSessionStatus status;
    return client_session_status(&status);
}

static void trace_client_combat_mode(
    BOOL active,
    BOOL combat_enabled,
    DWORD error
) {
    int next_state = active ? (combat_enabled ? 2 : 1) : 0;
    if (client_combat_mode_trace_state == next_state) return;
    client_combat_mode_trace_state = next_state;
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_combat_mode state=%s enabled=%s "
        "win32_error=%lu policy=mirror_host_mode_for_client_presentation_only\r\n",
        active ? "confirmed" : "rejected",
        combat_enabled ? "true" : "false",
        (unsigned long)error);
}

static BOOL synchronize_client_combat_mode(uint8_t combat_enabled) {
    BOOL current;
    BOOL desired;
    BOOL changed;
    BOOL verified;
    DWORD error;
    if (combat_enabled > 1u ||
        !SudekiMpCleanroomEngineCombatMode(&current)) {
        error = combat_enabled > 1u ? ERROR_INVALID_DATA : ERROR_INVALID_STATE;
        trace_client_combat_mode(FALSE, combat_enabled != 0u, error);
        SetLastError(error);
        return FALSE;
    }
    if (!client_combat_mode_lease_valid) {
        client_original_combat_mode = current;
        client_combat_mode_lease_valid = TRUE;
    }
    desired = combat_enabled != 0u;
    changed = current != desired;
    if (current != desired && !SudekiMpCleanroomEngineSetCombatMode(desired)) {
        error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_INVALID_STATE;
        trace_client_combat_mode(FALSE, desired, error);
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineCombatMode(&verified) || verified != desired) {
        trace_client_combat_mode(FALSE, desired, ERROR_INVALID_STATE);
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (changed) {
        client_combat_transition_pending = TRUE;
        client_combat_transition_target = desired;
        client_combat_transition_started_at = GetTickCount();
        client_combat_transition_trace_state = 0;
        memset(presentation_leases, 0, sizeof(presentation_leases));
        memset(&ailish_first_person_lease, 0,
            sizeof(ailish_first_person_lease));
        ailish_first_person_failure = NULL;
        client_ailish_combat_graph_failure = NULL;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_combat_presentation "
            "state=native_transition target=%s "
            "policy=allow_weapon_attachment_before_replica_animation_override\r\n",
            desired ? "armed" : "sheathed");
    }
    trace_client_combat_mode(TRUE, desired, ERROR_SUCCESS);
    return TRUE;
}

static unsigned int client_combat_presentation_ready_mask(void) {
    SudekiMpCleanroomActorPresentation tal;
    SudekiMpCleanroomActorPresentation ailish;
    int expected_tal;
    int expected_ailish;
    BOOL tal_ready;
    BOOL ailish_ready;
    unsigned int ready_mask;
    DWORD elapsed;
    if (!client_combat_transition_pending) return 0x03u;
    expected_tal = client_combat_transition_target ?
        TAL_COMBAT_IDLE_SELECTOR : TAL_WORLD_IDLE_SELECTOR;
    expected_ailish = client_combat_transition_target ?
        AILISH_COMBAT_IDLE_SELECTOR : AILISH_WORLD_IDLE_SELECTOR;
    ZeroMemory(&tal, sizeof(tal));
    ZeroMemory(&ailish, sizeof(ailish));
    tal_ready = SudekiMpCleanroomEngineActorPresentation(
            SUDEKIMP_CLEANROOM_TAL, &tal) &&
        tal.selector[0] == expected_tal;
    /* In first person, ActorPresentation observes Ailish's two-submodel arms
     * renderer, whose native idle selector is 1. Requiring world selector 20
     * from that surface can never settle and used to leave all client combat
     * replay disabled. Validate both of her independently resolved renderer
     * banks instead; apply_actor_presentation still verifies every write. */
    ailish_ready = client_combat_transition_target ?
        client_ailish_combat_graph_ready() :
        (SudekiMpCleanroomEngineActorPresentation(
             SUDEKIMP_CLEANROOM_AILISH, &ailish) &&
         ailish.selector[0] == expected_ailish);
    ready_mask =
        (SudekiMpLanArenaClientActorPresentationAllowed(
             0u, TRUE, tal_ready, ailish_ready) ? 0x01u : 0u) |
        (SudekiMpLanArenaClientActorPresentationAllowed(
             1u, TRUE, tal_ready, ailish_ready) ? 0x02u : 0u);
    if (tal_ready && ailish_ready) {
        if (!SudekiMpLanArenaClientPresentationOverrideAllowed(
                client_combat_transition_target)) {
            /* The native transition has safely attached both weapons and put
             * the actors in their armed idles. Do not resume the experimental
             * per-selector replica writer here: the supported image's combat
             * renderer uses a different layer/addressing contract, and the
             * old writer can feed selector 20 into an invalid animation table
             * entry (retail VA 0x0061BF17). Keep transforms/resources live and
             * let the native armed presentation own the client until the
             * sheathe transition completes. */
            if (client_combat_transition_trace_state != 3) {
                client_combat_transition_trace_state = 3;
                SudekiMpLogFormat(
                    "lan_arena_client_replica event=client_combat_presentation "
                    "state=native_owned target=armed tal_selector=%ld "
                    "ailish_selector=%ld "
                    "policy=fail_closed_no_unsafe_combat_selector_override\r\n",
                    (long)tal.selector[0], (long)ailish.selector[0]);
            }
            return 0u;
        }
        client_combat_transition_pending = FALSE;
        client_combat_transition_trace_state = 1;
        memset(presentation_leases, 0, sizeof(presentation_leases));
        memset(&ailish_first_person_lease, 0,
            sizeof(ailish_first_person_lease));
        ailish_first_person_failure = NULL;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_combat_presentation "
            "state=ready target=%s tal_selector=%ld ailish_selector=%ld "
            "ailish_surface=%s "
            "policy=native_weapon_transition_completed_before_replica_override\r\n",
            client_combat_transition_target ? "armed" : "sheathed",
            (long)tal.selector[0], (long)ailish.selector[0],
            client_combat_transition_target ?
                "verified_world_and_first_person_graphs" : "attached_world");
        return 0x03u;
    }
    elapsed = GetTickCount() - client_combat_transition_started_at;
    if (elapsed >= 1500u && client_combat_transition_trace_state == 0) {
        client_combat_transition_trace_state = 2;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_combat_presentation "
            "state=waiting target=%s elapsed_ms=%lu ready_mask=0x%02x "
            "tal_ready=%s ailish_ready=%s "
            "policy=actor_local_override_only_after_native_weapon_ready\r\n",
            client_combat_transition_target ? "armed" : "sheathed",
            (unsigned long)elapsed, ready_mask,
            tal_ready ? "true" : "false",
            ailish_ready ? "true" : "false");
    }
    return ready_mask;
}

static BOOL restore_client_combat_mode(void) {
    BOOL current;
    BOOL verified;
    DWORD error;
    if (!client_combat_mode_lease_valid) return TRUE;
    if (!SudekiMpCleanroomEngineCombatMode(&current) ||
        (current != client_original_combat_mode &&
         !SudekiMpCleanroomEngineSetCombatMode(client_original_combat_mode)) ||
        !SudekiMpCleanroomEngineCombatMode(&verified) ||
        verified != client_original_combat_mode) {
        error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_INVALID_STATE;
        trace_client_combat_mode(FALSE, client_original_combat_mode, error);
        SetLastError(error);
        return FALSE;
    }
    client_combat_mode_lease_valid = FALSE;
    client_original_combat_mode = FALSE;
    client_combat_mode_trace_state = -1;
    client_combat_transition_pending = FALSE;
    client_combat_transition_target = FALSE;
    client_combat_transition_started_at = 0u;
    client_combat_transition_trace_state = -1;
    SudekiMpLogWrite(
        verified ?
            "lan_arena_client_replica event=client_combat_mode state=restored enabled=true "
            "policy=disconnect_returns_native_mode_to_pre_session_value\r\n" :
            "lan_arena_client_replica event=client_combat_mode state=restored enabled=false "
            "policy=disconnect_returns_native_mode_to_pre_session_value\r\n");
    return TRUE;
}

static void clear_last_applied_frame(void) {
    memset(&last_applied_snapshot, 0, sizeof(last_applied_snapshot));
    memset(last_applied_characters, 0, sizeof(last_applied_characters));
    memset(last_applied_positions, 0, sizeof(last_applied_positions));
    replica_diagnostics.valid = 0u;
}

static BOOL finite_position(const SudekiMpLanArenaActorSnapshot *actor) {
    return actor != NULL && isfinite(actor->x) && isfinite(actor->y) &&
        isfinite(actor->z) && fabsf(actor->x) < 1000000.0f &&
        fabsf(actor->y) < 1000000.0f && fabsf(actor->z) < 1000000.0f;
}

static BOOL finite_facing(const SudekiMpLanArenaActorSnapshot *actor) {
    float length;
    if (actor == NULL || !isfinite(actor->facing_x) ||
        !isfinite(actor->facing_z)) return FALSE;
    length = sqrtf(actor->facing_x * actor->facing_x +
        actor->facing_z * actor->facing_z);
    return isfinite(length) && length >= 0.5f && length <= 1.5f;
}

static BOOL animation_renderer_signatures_match(uint8_t *base) {
    uint8_t *vtable;
    if (base == NULL) return FALSE;
    vtable = base + RVA_ANIMATION_RENDERER_VTABLE;
    return *(void **)(vtable + 0x40u) == base + RVA_ANIMATION_RENDERER_LOOKUP &&
        *(void **)(vtable + 0xf8u) == base + RVA_ANIMATION_RENDERER_COUNT &&
        *(void **)(vtable + 0xfcu) == base + RVA_ANIMATION_RENDERER_SELECTOR_SET &&
        *(void **)(vtable + 0x100u) == base + RVA_ANIMATION_RENDERER_SELECTOR_GET &&
        *(void **)(vtable + 0x104u) == base + RVA_ANIMATION_RENDERER_RATE_SET &&
        *(void **)(vtable + 0x108u) == base + RVA_ANIMATION_RENDERER_RATE_GET &&
        *(void **)(vtable + 0x10cu) == base + RVA_ANIMATION_RENDERER_TIME_SET &&
        *(void **)(vtable + 0x110u) == base + RVA_ANIMATION_RENDERER_TIME_GET &&
        *(void **)(vtable + 0x114u) == base + RVA_ANIMATION_RENDERER_STATE_SET &&
        *(void **)(vtable + 0x118u) == base + RVA_ANIMATION_RENDERER_STATE_GET &&
        *(void **)(vtable + 0x144u) == base + RVA_ANIMATION_RENDERER_BLEND_SET &&
        *(void **)(vtable + 0x148u) == base + RVA_ANIMATION_RENDERER_BLEND_GET;
}

static BOOL animation_methods(
    void *renderer,
    LanArenaAnimationMethods *methods
) {
    void **vtable;
    if (methods == NULL || !readable_memory(renderer, sizeof(void *)) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        return FALSE;
    }
    vtable = *(void ***)renderer;
    methods->count = (AnimationCountFunction)vtable[0xf8u / sizeof(void *)];
    methods->set_selector = (AnimationSelectorSetFunction)vtable[0xfcu / sizeof(void *)];
    methods->get_selector = (AnimationSelectorGetFunction)vtable[0x100u / sizeof(void *)];
    methods->set_rate = (AnimationValueSetFunction)vtable[0x104u / sizeof(void *)];
    methods->get_rate = (AnimationValueGetFunction)vtable[0x108u / sizeof(void *)];
    methods->set_time = (AnimationTimeSetFunction)vtable[0x10cu / sizeof(void *)];
    methods->get_time = (AnimationValueGetFunction)vtable[0x110u / sizeof(void *)];
    methods->set_state = (AnimationStateSetFunction)vtable[0x114u / sizeof(void *)];
    methods->get_state = (AnimationStateGetFunction)vtable[0x118u / sizeof(void *)];
    methods->set_blend = (AnimationBlendSetFunction)vtable[0x144u / sizeof(void *)];
    methods->get_blend = (AnimationBlendGetFunction)vtable[0x148u / sizeof(void *)];
    return (void *)methods->count == game_base + RVA_ANIMATION_RENDERER_COUNT &&
        (void *)methods->set_selector == game_base + RVA_ANIMATION_RENDERER_SELECTOR_SET &&
        (void *)methods->get_selector == game_base + RVA_ANIMATION_RENDERER_SELECTOR_GET &&
        (void *)methods->set_rate == game_base + RVA_ANIMATION_RENDERER_RATE_SET &&
        (void *)methods->get_rate == game_base + RVA_ANIMATION_RENDERER_RATE_GET &&
        (void *)methods->set_time == game_base + RVA_ANIMATION_RENDERER_TIME_SET &&
        (void *)methods->get_time == game_base + RVA_ANIMATION_RENDERER_TIME_GET &&
        (void *)methods->set_state == game_base + RVA_ANIMATION_RENDERER_STATE_SET &&
        (void *)methods->get_state == game_base + RVA_ANIMATION_RENDERER_STATE_GET &&
        (void *)methods->set_blend == game_base + RVA_ANIMATION_RENDERER_BLEND_SET &&
        (void *)methods->get_blend == game_base + RVA_ANIMATION_RENDERER_BLEND_GET;
}

static BOOL resolve_ailish_world_selector(
    uint8_t *component,
    void *renderer,
    unsigned int animation_id,
    int expected_selector
) {
    uint8_t *animation_table;
    uint8_t *details;
    void **vtable;
    AnimationLookupFunction lookup;
    uint32_t handle;
    uint32_t alternate_handle;
    BOOL first_person_bank_active;

    if (game_base == NULL || !readable_memory(component, 0x168u) ||
        !readable_memory(renderer, sizeof(void *)) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        return FALSE;
    }
    vtable = *(void ***)renderer;
    if (!readable_memory(vtable, 0x44u) ||
        vtable[0x40u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_LOOKUP) {
        return FALSE;
    }
    animation_table = *(uint8_t **)(
        component + AILISH_ANIMATION_TABLE_OFFSET);
    if (!readable_memory(
            animation_table,
            0x14u + (animation_id + 1u) * sizeof(void *))) {
        return FALSE;
    }
    details = *(uint8_t **)(
        animation_table + 0x14u + animation_id * sizeof(void *));
    if (!readable_memory(details, 0x28u)) return FALSE;
    first_person_bank_active =
        (component[AILISH_ANIMATION_BANK_OFFSET] & 2u) != 0u;
    /* The native first-person transition can change the bank byte before it
     * finishes swapping both renderer wrappers. Resolve the preferred bank
     * first, then accept the alternate handle only if the destination world
     * renderer itself maps it to the exact expected selector. This preserves
     * the crash guard without mistaking a transient bank bit for ownership. */
    memcpy(&handle, details + (first_person_bank_active ? 0x20u : 0x14u),
        sizeof(handle));
    lookup = (AnimationLookupFunction)vtable[0x40u / sizeof(void *)];
    if (handle != 0u && handle != 0x0007ffffu &&
        lookup(renderer, (int)(int32_t)handle) == expected_selector) {
        return TRUE;
    }
    memcpy(&alternate_handle,
        details + (first_person_bank_active ? 0x14u : 0x20u),
        sizeof(alternate_handle));
    return alternate_handle != 0u && alternate_handle != 0x0007ffffu &&
        alternate_handle != handle &&
        lookup(renderer, (int)(int32_t)alternate_handle) == expected_selector;
}

static BOOL resolve_ailish_first_person_selector(
    uint8_t *component,
    void *renderer,
    unsigned int animation_id,
    int expected_selector,
    uint32_t *resolved_handle,
    int *resolved_selector
) {
    uint8_t *animation_table;
    uint8_t *details;
    void **vtable;
    AnimationLookupFunction lookup;
    uint32_t handle;
    uint32_t alternate_handle;
    int selector;
    BOOL first_person_bank_active;

    if (resolved_handle != NULL) *resolved_handle = 0u;
    if (resolved_selector != NULL) *resolved_selector = -1;
    if (game_base == NULL || !readable_memory(component, 0x168u) ||
        !readable_memory(renderer, sizeof(void *)) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        return FALSE;
    }
    vtable = *(void ***)renderer;
    if (!readable_memory(vtable, 0x44u) ||
        vtable[0x40u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_LOOKUP) {
        return FALSE;
    }
    animation_table = *(uint8_t **)(
        component + AILISH_ANIMATION_TABLE_OFFSET);
    if (!readable_memory(
            animation_table,
            0x14u + (animation_id + 1u) * sizeof(void *))) {
        return FALSE;
    }
    details = *(uint8_t **)(
        animation_table + 0x14u + animation_id * sizeof(void *));
    if (!readable_memory(details, 0x28u)) return FALSE;
    first_person_bank_active =
        (component[AILISH_ANIMATION_BANK_OFFSET] & 2u) != 0u;
    /* As above, use the bank bit only as ordering preference. The selected
     * handle remains valid only when this exact first-person renderer maps it
     * to the expected selector. */
    memcpy(&handle, details + (first_person_bank_active ? 0x14u : 0x20u),
        sizeof(handle));
    lookup = (AnimationLookupFunction)vtable[0x40u / sizeof(void *)];
    selector = (handle == 0u || handle == 0x0007ffffu) ? -1 :
        lookup(renderer, (int)(int32_t)handle);
    if (selector == expected_selector) {
        if (resolved_handle != NULL) *resolved_handle = handle;
        if (resolved_selector != NULL) *resolved_selector = selector;
        return TRUE;
    }
    memcpy(&alternate_handle,
        details + (first_person_bank_active ? 0x20u : 0x14u),
        sizeof(alternate_handle));
    if (alternate_handle != 0u && alternate_handle != 0x0007ffffu &&
        alternate_handle != handle) {
        int alternate_selector = lookup(
            renderer, (int)(int32_t)alternate_handle);
        if (alternate_selector == expected_selector) {
            if (resolved_handle != NULL) {
                *resolved_handle = alternate_handle;
            }
            if (resolved_selector != NULL) {
                *resolved_selector = alternate_selector;
            }
            return TRUE;
        }
    }
    if (resolved_handle != NULL) *resolved_handle = handle;
    if (resolved_selector != NULL) *resolved_selector = selector;
    return FALSE;
}

static BOOL ailish_first_person_reject(
    const char *reason,
    uint8_t *component,
    void *renderer,
    uint32_t idle_handle,
    int idle_selector,
    uint32_t fire_handle,
    int fire_selector
) {
    if (ailish_first_person_failure != reason) {
        ailish_first_person_failure = reason;
        SudekiMpLogFormat(
            "lan_arena_client_replica "
            "event=client_first_person_fire_presentation state=rejected "
            "reason=%s component=0x%08lx renderer=0x%08lx bank=%u "
            "idle_handle=0x%08lx idle_selector=%d "
            "fire_handle=0x%08lx fire_selector=%d\r\n",
            reason,
            (unsigned long)(uintptr_t)component,
            (unsigned long)(uintptr_t)renderer,
            component != NULL && readable_memory(
                component + AILISH_ANIMATION_BANK_OFFSET, 1u) ?
                (unsigned int)component[AILISH_ANIMATION_BANK_OFFSET] : 0u,
            (unsigned long)idle_handle, idle_selector,
            (unsigned long)fire_handle, fire_selector);
    }
    return FALSE;
}

static BOOL actor_presentation_renderer(
    uint8_t *character,
    unsigned int actor_index,
    void **renderer_result,
    uint8_t **ailish_component_result
) {
    uint8_t *position;
    uint8_t *wrapper;
    uint8_t *component = NULL;

    if (renderer_result == NULL || ailish_component_result == NULL ||
        !readable_memory(character,
            CHARACTER_POSITION_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    *renderer_result = NULL;
    *ailish_component_result = NULL;
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    if (actor_index == 1u) {
        if (!readable_memory(character,
                AILISH_RANGED_COMPONENT_OFFSET + sizeof(void *))) {
            return FALSE;
        }
        component = *(uint8_t **)(
            character + AILISH_RANGED_COMPONENT_OFFSET);
        if (!readable_memory(component, 0x168u)) return FALSE;
        wrapper = *(uint8_t **)(
            component + AILISH_WORLD_WRAPPER_OFFSET);
        *ailish_component_result = component;
    } else {
        wrapper = readable_memory(position,
                POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *)) ?
            *(uint8_t **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) : NULL;
    }
    if (!readable_memory(wrapper, 0x14u)) return FALSE;
    *renderer_result = *(void **)(wrapper + 0x10u);
    return *renderer_result != NULL;
}

static BOOL client_ailish_combat_graph_ready(void) {
    uint8_t *character = (uint8_t *)
        SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    uint8_t *component = NULL;
    uint8_t *first_person_wrapper = NULL;
    void *world_renderer = NULL;
    void *first_person_renderer = NULL;
    LanArenaAnimationMethods methods;
    uint32_t handle;
    int selector;
    const char *failure = NULL;

    if (!actor_presentation_renderer(
            character, 1u, &world_renderer, &component) ||
        !animation_methods(world_renderer, &methods) ||
        methods.count(world_renderer) == 0u) {
        failure = "world_renderer";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x02u,
            AILISH_COMBAT_IDLE_SELECTOR)) {
        failure = "world_idle";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x06u,
            AILISH_COMBAT_MOVE_PRIMARY_SELECTOR)) {
        failure = "world_move_primary";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x07u,
            AILISH_COMBAT_MOVE_SECONDARY_SELECTOR)) {
        failure = "world_move_secondary";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x85u,
            AILISH_COMBAT_WEAK_SELECTOR)) {
        failure = "world_fire";
    }
    if (failure != NULL) {
        goto rejected;
    }
    first_person_wrapper = *(uint8_t **)(
        component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
    first_person_renderer = readable_memory(first_person_wrapper, 0x14u) ?
        *(void **)(first_person_wrapper + 0x10u) : NULL;
    if (!animation_methods(first_person_renderer, &methods) ||
        methods.count(first_person_renderer) == 0u) {
        failure = "first_person_renderer";
    } else if (!resolve_ailish_first_person_selector(
            component, first_person_renderer, 0x05u,
            AILISH_FIRST_PERSON_IDLE_SELECTOR, &handle, &selector)) {
        failure = "first_person_idle";
    } else if (!resolve_ailish_first_person_selector(
            component, first_person_renderer, 0x8cu,
            AILISH_FIRST_PERSON_WEAK_SELECTOR, &handle, &selector)) {
        failure = "first_person_fire";
    }
    if (failure != NULL) {
rejected:
        if (client_ailish_combat_graph_failure != failure) {
            client_ailish_combat_graph_failure = failure;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_combat_graph "
                "state=waiting reason=%s character=0x%08lx "
                "component=0x%08lx bank=%u world_renderer=0x%08lx "
                "first_person_renderer=0x%08lx\r\n",
                failure,
                (unsigned long)(uintptr_t)character,
                (unsigned long)(uintptr_t)component,
                readable_memory(component, 0x168u) ?
                    (unsigned int)component[AILISH_ANIMATION_BANK_OFFSET] : 0u,
                (unsigned long)(uintptr_t)world_renderer,
                (unsigned long)(uintptr_t)first_person_renderer);
        }
        return FALSE;
    }
    client_ailish_combat_graph_failure = NULL;
    return TRUE;
}

static void set_animation_channel(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    int state,
    float rate,
    BOOL reset_time
) {
    unsigned int submodel;
    for (submodel = 0u; submodel < submodels; ++submodel) {
        int current_selector = methods->get_selector(
            renderer, channel, submodel);
        int current_state = methods->get_state(
            renderer, channel, submodel);
        float current_rate = methods->get_rate(
            renderer, channel, submodel);
        BOOL selector_changed = current_selector != selector;
        BOOL state_changed = current_state != state;
        if (selector_changed) {
            methods->set_selector(renderer, channel, submodel, selector);
        }
        if (state_changed) {
            methods->set_state(renderer, channel, submodel, state);
        }
        if (reset_time) {
            methods->set_time(renderer, channel, submodel, 0.0f, 0);
        }
        if (!isfinite(current_rate) || fabsf(current_rate - rate) > 0.001f) {
            methods->set_rate(renderer, channel, submodel, rate);
        }
    }
}

static BOOL synchronize_action_phase(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    const SudekiMpLanArenaActorSnapshot *snapshot
) {
    unsigned int submodel;
    float phase_time;
    const float tolerance =
        (float)ACTION_PHASE_TIME_TOLERANCE_MILLI / 1000.0f;
    if (renderer == NULL || methods == NULL || snapshot == NULL ||
        submodels == 0u || submodels > 32u) return FALSE;
    if (!snapshot->action_phase_valid) return TRUE;
    if (!SudekiMpLanArenaClientActionPhaseTime(snapshot, &phase_time)) {
        return FALSE;
    }
    for (submodel = 0u; submodel < submodels; ++submodel) {
        float actual;
        methods->set_time(renderer, channel, submodel,
            phase_time, 0);
        actual = methods->get_time(renderer, channel, submodel);
        if (!isfinite(actual) ||
            fabsf(actual - phase_time) > tolerance) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL animation_channel_matches(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    float rate
);

static BOOL apply_ailish_first_person_presentation(
    uint8_t *character,
    uint8_t *component,
    const SudekiMpLanArenaActorSnapshot *snapshot
) {
    uint8_t *wrapper;
    void *renderer;
    LanArenaAnimationMethods methods;
    unsigned int submodels;
    BOOL weak_attack;
    BOOL transition;
    int selector;
    int state;
    uint32_t idle_handle = 0u;
    uint32_t fire_handle = 0u;
    int idle_selector = -1;
    int fire_selector = -1;

    if (!readable_memory(character,
            AILISH_RANGED_COMPONENT_OFFSET + sizeof(void *)) ||
        !readable_memory(component, 0x168u) ||
        *(uint8_t **)(character + AILISH_RANGED_COMPONENT_OFFSET) != component) {
        return ailish_first_person_reject(
            "component_lease", component, NULL,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    wrapper = *(uint8_t **)(component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
    if (!readable_memory(wrapper, 0x14u)) {
        return ailish_first_person_reject(
            "wrapper", component, NULL,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    renderer = *(void **)(wrapper + 0x10u);
    if (!animation_methods(renderer, &methods)) {
        return ailish_first_person_reject(
            "renderer_methods", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    submodels = methods.count(renderer);
    if (submodels == 0u || submodels > 32u) {
        return ailish_first_person_reject(
            "submodel_count", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    if (!resolve_ailish_first_person_selector(
            component, renderer, 0x05u,
            AILISH_FIRST_PERSON_IDLE_SELECTOR,
            &idle_handle, &idle_selector)) {
        return ailish_first_person_reject(
            "idle_lookup", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    if (!resolve_ailish_first_person_selector(
            component, renderer, 0x8cu,
            AILISH_FIRST_PERSON_WEAK_SELECTOR,
            &fire_handle, &fire_selector)) {
        return ailish_first_person_reject(
            "fire_lookup", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    weak_attack = snapshot->combat_state ==
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    transition = !ailish_first_person_lease.valid ||
        ailish_first_person_lease.character != character ||
        ailish_first_person_lease.component != component ||
        ailish_first_person_lease.renderer != renderer ||
        ailish_first_person_lease.weak_attack != weak_attack ||
        ailish_first_person_lease.action_sequence != snapshot->action_sequence;
    if (!transition) {
        return !weak_attack || synchronize_action_phase(
            renderer, &methods, submodels, 0, snapshot);
    }
    selector = weak_attack ? AILISH_FIRST_PERSON_WEAK_SELECTOR :
        AILISH_FIRST_PERSON_IDLE_SELECTOR;
    state = weak_attack ? 1 : 128;
    set_animation_channel(
        renderer, &methods, submodels, 0, selector, state, 24.0f,
        weak_attack);
    if (weak_attack && !synchronize_action_phase(
            renderer, &methods, submodels, 0, snapshot)) {
        return ailish_first_person_reject(
            "host_action_phase", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    if (!animation_channel_matches(
            renderer, &methods, submodels, 0, selector, 24.0f)) {
        return ailish_first_person_reject(
            "channel_verify", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    ailish_first_person_failure = NULL;
    ailish_first_person_lease.character = character;
    ailish_first_person_lease.component = component;
    ailish_first_person_lease.renderer = renderer;
    ailish_first_person_lease.action_sequence = snapshot->action_sequence;
    ailish_first_person_lease.weak_attack = weak_attack;
    ailish_first_person_lease.valid = TRUE;
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_first_person_fire_presentation "
        "state=%s action_sequence=%u selector=%d "
        "policy=verified_first_person_renderer_only\r\n",
        weak_attack ? "fire" : "idle",
        (unsigned int)snapshot->action_sequence,
        selector);
    return TRUE;
}

static BOOL animation_channel_matches(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    float rate
) {
    unsigned int submodel;
    for (submodel = 0u; submodel < submodels; ++submodel) {
        if (methods->get_selector(renderer, channel, submodel) != selector ||
            fabsf(methods->get_rate(renderer, channel, submodel) - rate) >
                0.001f) return FALSE;
    }
    return TRUE;
}

static BOOL actor_presentation_matches(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    unsigned int actor_index,
    uint8_t animation_state,
    BOOL weak_attack,
    uint8_t action_variant,
    BOOL combat_mode
) {
    BOOL moving = animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    int selector_zero;
    int selector_one;
    float rate_zero = actor_index == 0u ?
        (moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f) :
        (moving ? AILISH_WORLD_MOVE_PRIMARY_RATE : 12.0f);
    float rate_one;
    float expected_blend_zero = moving ? 0.99f : 0.0f;
    float blend_zero = methods->get_blend(renderer, 0);
    if (combat_mode) {
        if (actor_index == 0u) {
            int action_state;
            if (weak_attack && !SudekiMpLanArenaClientTalActionPresentation(
                    action_variant, &selector_zero, &action_state)) {
                return FALSE;
            }
            (void)action_state;
            if (!weak_attack) {
                selector_zero = moving ? TAL_COMBAT_MOVE_PRIMARY_SELECTOR :
                    TAL_COMBAT_IDLE_SELECTOR;
            }
            selector_one = moving ? TAL_COMBAT_MOVE_SECONDARY_SELECTOR : 0;
            rate_zero = weak_attack ? 24.0f :
                (moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f);
            rate_one = moving ? TAL_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        } else {
            selector_zero = moving ? AILISH_COMBAT_MOVE_PRIMARY_SELECTOR :
                AILISH_COMBAT_IDLE_SELECTOR;
            selector_one = moving ? AILISH_COMBAT_MOVE_SECONDARY_SELECTOR : 0;
            rate_one = moving ? AILISH_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        }
    } else {
        selector_zero = actor_index == 0u ?
            (moving ? TAL_WORLD_MOVE_PRIMARY_SELECTOR : TAL_WORLD_IDLE_SELECTOR) :
            (moving ? AILISH_WORLD_MOVE_PRIMARY_SELECTOR : AILISH_WORLD_IDLE_SELECTOR);
        selector_one = moving ?
            (actor_index == 0u ? TAL_WORLD_MOVE_SECONDARY_SELECTOR :
                AILISH_WORLD_MOVE_SECONDARY_SELECTOR) : 0;
        rate_one = moving ?
            (actor_index == 0u ? TAL_WORLD_MOVE_SECONDARY_RATE :
                AILISH_WORLD_MOVE_SECONDARY_RATE) : 0.0f;
    }
    if (!combat_mode && SudekiMpLanArenaClientIdleVariantSelector(
            actor_index == 0u ? SUDEKIMP_LAN_ARENA_TAL_TYPE :
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,
            animation_state, &selector_zero)) {
        rate_zero = 24.0f;
    }
    if (!animation_channel_matches(renderer, methods, submodels, 0,
            selector_zero, rate_zero) ||
        !animation_channel_matches(renderer, methods, submodels, 1,
            selector_one, rate_one) ||
        !isfinite(blend_zero) ||
        fabsf(blend_zero - expected_blend_zero) > 0.001f) {
        return FALSE;
    }
    /* Tal's spawned world renderer does not expose a safe zero selector for
     * every auxiliary channel.  His base channels are proven, and hiding the
     * stale native action layer only requires its blend to be zero. */
    if (actor_index == 0u) {
        float blend_three = methods->get_blend(renderer, 3);
        return isfinite(blend_three) && fabsf(blend_three) <= 0.001f;
    }
    return animation_channel_matches(
            renderer, methods, submodels, 2, 0, 0.0f) &&
        animation_channel_matches(
            renderer, methods, submodels, 3, 0, 0.0f) &&
        animation_channel_matches(
            renderer, methods, submodels, 4,
            actor_index == 1u && weak_attack ?
                (combat_mode ? AILISH_COMBAT_WEAK_SELECTOR :
                    AILISH_WORLD_WEAK_SELECTOR) : 0,
            actor_index == 1u && weak_attack ? 24.0f : 0.0f) &&
        isfinite(methods->get_blend(renderer, 1)) &&
        isfinite(methods->get_blend(renderer, 2)) &&
        isfinite(methods->get_blend(renderer, 3)) &&
        fabsf(methods->get_blend(renderer, 1)) <= 0.001f &&
        fabsf(methods->get_blend(renderer, 2)) <= 0.001f &&
        fabsf(methods->get_blend(renderer, 3) -
            (actor_index == 1u && weak_attack ? 1.0f : 0.0f)) <= 0.001f;
}

static BOOL ailish_locomotion_base_matches(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    BOOL combat_mode
) {
    float blend_zero = methods->get_blend(renderer, 0);
    return animation_channel_matches(
            renderer, methods, submodels, 0,
            combat_mode ? AILISH_COMBAT_MOVE_PRIMARY_SELECTOR :
                AILISH_WORLD_MOVE_PRIMARY_SELECTOR,
            AILISH_WORLD_MOVE_PRIMARY_RATE) &&
        animation_channel_matches(
            renderer, methods, submodels, 1,
            combat_mode ? AILISH_COMBAT_MOVE_SECONDARY_SELECTOR :
                AILISH_WORLD_MOVE_SECONDARY_SELECTOR,
            AILISH_WORLD_MOVE_SECONDARY_RATE) &&
        isfinite(blend_zero) && fabsf(blend_zero - 0.99f) <= 0.001f;
}

static BOOL ailish_idle_variant_base_matches(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    uint8_t animation_state
) {
    int selector;
    float blend_zero;
    if (animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE) {
        selector = AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR;
    } else if (animation_state ==
               SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO) {
        selector = AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    } else {
        return FALSE;
    }
    blend_zero = methods->get_blend(renderer, 0);
    return animation_channel_matches(
            renderer, methods, submodels, 0, selector, 24.0f) &&
        animation_channel_matches(
            renderer, methods, submodels, 1, 0, 0.0f) &&
        isfinite(blend_zero) && fabsf(blend_zero) <= 0.001f;
}

static BOOL apply_actor_presentation(
    uint8_t *character,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    unsigned int actor_index,
    BOOL combat_mode
) {
    uint8_t *ailish_component;
    void *renderer;
    LanArenaAnimationMethods methods;
    LanArenaPresentationLease *lease;
    unsigned int submodels;
    BOOL moving;
    int selector_zero;
    int selector_one;
    int state_zero;
    int state_one;
    float rate_zero;
    float rate_one;
    float expected_blend_zero;
    float expected_blend_three;
    int action_selector;
    int action_state;
    float action_rate;
    BOOL weak_attack;
    BOOL new_action_sequence;
    BOOL logical_transition;
    BOOL base_target_already_matches;
    BOOL reset_base_time;
    BOOL ailish_first_person_applied = TRUE;
    BOOL preserve_ailish_auxiliary = FALSE;
    if (character == NULL || snapshot == NULL || actor_index >= 2u ||
        snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED) {
        return FALSE;
    }
    /* Combat swaps Ailish's Position attachment to her two-submodel
     * first-person renderer. World selectors 20/22/23/59 are not valid in
     * that table. Always resolve her presentation through the distinct world
     * wrapper at ranged-component+0x164; Tal keeps the ordinary attached
     * world renderer. */
    if (!actor_presentation_renderer(
            character, actor_index, &renderer, &ailish_component)) {
        return FALSE;
    }
    if (!animation_methods(renderer, &methods)) return FALSE;
    submodels = methods.count(renderer);
    if (submodels == 0u || submodels > 32u) return FALSE;
    /* The first-person arms renderer is the owner's visible surface and is
     * independent from Ailish's retained world renderer. Validate/apply it
     * before any conservative world-bank lookup can fail. */
    if (actor_index == 1u && combat_mode) {
        ailish_first_person_applied =
            apply_ailish_first_person_presentation(
                character, ailish_component, snapshot);
    }
    if (actor_index == 1u && combat_mode &&
        (!resolve_ailish_world_selector(
             ailish_component, renderer, 0x02u,
             AILISH_COMBAT_IDLE_SELECTOR) ||
         !resolve_ailish_world_selector(
             ailish_component, renderer, 0x06u,
             AILISH_COMBAT_MOVE_PRIMARY_SELECTOR) ||
         !resolve_ailish_world_selector(
             ailish_component, renderer, 0x07u,
             AILISH_COMBAT_MOVE_SECONDARY_SELECTOR) ||
         !resolve_ailish_world_selector(
             ailish_component, renderer, 0x85u,
             AILISH_COMBAT_WEAK_SELECTOR))) {
        return FALSE;
    }
    lease = &presentation_leases[actor_index];
    moving = snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    /* This flag names the pre-LA10 action path historically.  It now means
     * any validated semantic action; Ailish remains protocol-limited to weak
     * fire while Tal may carry weak, strong, sweep, or block. */
    weak_attack =
        snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    new_action_sequence = weak_attack &&
        (!lease->valid ||
         lease->action_sequence != snapshot->action_sequence);
    logical_transition = !lease->valid || lease->character != character ||
        lease->renderer != renderer ||
        lease->animation_state != snapshot->animation_state ||
        lease->combat_state != snapshot->combat_state ||
        lease->action_variant != snapshot->action_variant ||
        lease->action_sequence != snapshot->action_sequence ||
        lease->combat_mode != combat_mode;
    if (!logical_transition) {
        /* Ailish's native renderer advances locomotion through a double-
         * buffered channel pair. Reasserting our settled pair whenever that
         * native blend progressed caused the visible limp/stumble. Own only
         * semantic edges for her and let Sudeki advance the animation clock
         * and blend between those edges. Tal's restricted base-channel path
         * remains verified continuously because his auxiliary selectors are
         * not safe to touch. */
        if (actor_index == 1u) {
            if (weak_attack) {
                return synchronize_action_phase(
                    renderer, &methods, submodels, 4, snapshot);
            }
            if (!combat_mode && !moving && ailish_idle_variant_base_matches(
                    renderer, &methods, submodels,
                    snapshot->animation_state)) return TRUE;
            if (!moving && actor_presentation_matches(
                    renderer, &methods, submodels, actor_index,
                    snapshot->animation_state, weak_attack,
                    snapshot->action_variant,
                    combat_mode)) return TRUE;
            if (moving && ailish_locomotion_base_matches(
                    renderer, &methods, submodels,
                    combat_mode)) return TRUE;
            preserve_ailish_auxiliary = moving;
        } else if (actor_presentation_matches(
                       renderer, &methods, submodels, actor_index,
                       snapshot->animation_state, weak_attack,
                       snapshot->action_variant,
                       combat_mode)) {
            return !weak_attack || synchronize_action_phase(
                renderer, &methods, submodels, 0, snapshot);
        }
    }
    /* A completed native idle variant retires into state 128 on both retail
     * actors. Sending state 0 here briefly starts the base idle from its
     * entry pose before Sudeki advances it, which is visible as an end snap.
     * Movement and variant branches below retain their proven start states. */
    state_zero = moving ? 0 : 128;
    if (actor_index == 0u) {
        if (combat_mode) {
            if (weak_attack && !SudekiMpLanArenaClientTalActionPresentation(
                    snapshot->action_variant, &selector_zero, &state_zero)) {
                return FALSE;
            }
            if (!weak_attack) {
                selector_zero = moving ? TAL_COMBAT_MOVE_PRIMARY_SELECTOR :
                    TAL_COMBAT_IDLE_SELECTOR;
            }
            selector_one = moving ? TAL_COMBAT_MOVE_SECONDARY_SELECTOR : 0;
            if (!weak_attack) state_zero = moving ? 0 : 128;
            state_one = moving ? 0 : 192;
            rate_zero = weak_attack ? 24.0f :
                (moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f);
            rate_one = moving ? TAL_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        } else {
            selector_zero = moving ?
                TAL_WORLD_MOVE_PRIMARY_SELECTOR : TAL_WORLD_IDLE_SELECTOR;
            selector_one = moving ? TAL_WORLD_MOVE_SECONDARY_SELECTOR : 0;
            state_one = moving ? 0 : 192;
            rate_zero = moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f;
            rate_one = moving ? TAL_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        }
        if (!combat_mode && snapshot->animation_state ==
                SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE) {
            selector_zero = TAL_WORLD_IDLE_VARIANT_ONE_SELECTOR;
            rate_zero = 24.0f;
            state_zero = 1;
        } else if (!combat_mode && snapshot->animation_state ==
                   SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO) {
            selector_zero = TAL_WORLD_IDLE_VARIANT_TWO_SELECTOR;
            rate_zero = 24.0f;
            state_zero = 1;
        }
    } else {
        selector_zero = moving ?
            (combat_mode ? AILISH_COMBAT_MOVE_PRIMARY_SELECTOR :
                AILISH_WORLD_MOVE_PRIMARY_SELECTOR) :
            (combat_mode ? AILISH_COMBAT_IDLE_SELECTOR :
                AILISH_WORLD_IDLE_SELECTOR);
        selector_one = moving ?
            (combat_mode ? AILISH_COMBAT_MOVE_SECONDARY_SELECTOR :
                AILISH_WORLD_MOVE_SECONDARY_SELECTOR) : 0;
        state_one = moving ? 0 : 192;
        rate_zero = moving ? AILISH_WORLD_MOVE_PRIMARY_RATE : 12.0f;
        rate_one = moving ? AILISH_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        if (!combat_mode && SudekiMpLanArenaClientIdleVariantSelector(
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,
                snapshot->animation_state, &selector_zero)) {
            rate_zero = 24.0f;
            state_zero = 1;
        }
    }
    base_target_already_matches = animation_channel_matches(
        renderer, &methods, submodels, 0, selector_zero, rate_zero);
    reset_base_time = new_action_sequence ||
        (logical_transition &&
         SudekiMpLanArenaClientAnimationShouldResetTime(
             actor_index,
             lease->valid ? lease->animation_state :
                 SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
             snapshot->animation_state,
             base_target_already_matches));
    state_zero = SudekiMpLanArenaClientAnimationTransitionState(
        actor_index,
        lease->valid ? lease->animation_state :
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        snapshot->animation_state,
        state_zero);
    set_animation_channel(renderer, &methods, submodels, 0,
        selector_zero, state_zero, rate_zero, reset_base_time);
    set_animation_channel(renderer, &methods, submodels, 1,
        selector_one, state_one, rate_one,
        SudekiMpLanArenaClientSecondaryAnimationShouldResetTime(
            actor_index, logical_transition, moving,
            base_target_already_matches));
    if (actor_index == 0u && weak_attack &&
        !synchronize_action_phase(
            renderer, &methods, submodels, 0, snapshot)) return FALSE;
    expected_blend_zero = moving ? 0.99f : 0.0f;
    expected_blend_three = 0.0f;
    action_selector = 0;
    action_state = 192;
    action_rate = 0.0f;
    methods.set_blend(renderer, 0, expected_blend_zero);
    if (actor_index == 0u) {
        methods.set_blend(renderer, 3, 0.0f);
    } else if (!preserve_ailish_auxiliary) {
        set_animation_channel(renderer, &methods, submodels, 2,
            0, 192, 0.0f, logical_transition);
        set_animation_channel(renderer, &methods, submodels, 3,
            0, 192, 0.0f, logical_transition);
        methods.set_blend(renderer, 1, 0.0f);
        methods.set_blend(renderer, 2, 0.0f);
        if (weak_attack) {
            action_selector = combat_mode ? AILISH_COMBAT_WEAK_SELECTOR :
                AILISH_WORLD_WEAK_SELECTOR;
            action_state = 1;
            action_rate = 24.0f;
            expected_blend_three = 1.0f;
        }
        set_animation_channel(renderer, &methods, submodels, 4,
            action_selector, action_state, action_rate,
            logical_transition && (!weak_attack || new_action_sequence));
        if (weak_attack && !synchronize_action_phase(
                renderer, &methods, submodels, 4, snapshot)) return FALSE;
        methods.set_blend(renderer, 3, expected_blend_three);
    }
    if (preserve_ailish_auxiliary) {
        if (!ailish_locomotion_base_matches(
                renderer, &methods, submodels,
                combat_mode)) return FALSE;
    } else {
        /* Ailish owns a distinct two-submodel first-person renderer in her
         * client process. Apply it independently of the saved three-submodel
         * world renderer: a conservative world-body verification failure
         * must not suppress the owner's already-validated arms/fire clip. */
        if (!actor_presentation_matches(
                renderer, &methods, submodels, actor_index,
                snapshot->animation_state, weak_attack,
                snapshot->action_variant,
                combat_mode)) {
            return FALSE;
        }
    }
    if (actor_index == 1u) {
        if (combat_mode) {
            if (!ailish_first_person_applied) {
                return FALSE;
            }
        } else {
            ZeroMemory(&ailish_first_person_lease,
                sizeof(ailish_first_person_lease));
            ailish_first_person_failure = NULL;
        }
    }
    lease->character = character;
    lease->renderer = renderer;
    lease->animation_state = snapshot->animation_state;
    lease->combat_state = snapshot->combat_state;
    lease->action_variant = snapshot->action_variant;
    lease->action_sequence = snapshot->action_sequence;
    lease->combat_mode = combat_mode;
    lease->valid = TRUE;
    if (actor_index == 0u && weak_attack && new_action_sequence) {
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_tal_action_presentation "
            "state=verified action_sequence=%u variant=%u selector=%d "
            "host_phase_q8=%u phase_valid=%u "
            "policy=actor_local_selector_host_timeline_after_semantic_journal_replay\r\n",
            (unsigned int)snapshot->action_sequence,
            (unsigned int)snapshot->action_variant,
            selector_zero,
            (unsigned int)snapshot->action_phase_q8,
            (unsigned int)snapshot->action_phase_valid);
    }
    return TRUE;
}

__attribute__((naked, noinline, used))
static void call_position_set_forward(
    void *position __attribute__((unused)),
    const float direction[3] __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "movl 12(%esp), %ecx\n\t"
        "call *_set_forward\n\t"
        "popl %esi\n\t"
        "ret\n\t"
    );
}

static BOOL apply_actor(
    const SudekiMpLanArenaActorSnapshot *snapshot,
    SudekiMpCleanroomActor actor,
    uint8_t expected_type,
    BOOL combat_mode,
    BOOL presentation_allowed,
    void **applied_character,
    void **applied_position
) {
    uint8_t *character;
    void *position;
    float coordinates[3];
    float facing[3];
    float current_facing_x;
    float current_facing_z;
    float current_facing_length;
    float facing_dot;
    BOOL local_first_person_facing;
    float dx;
    float dy;
    float dz;
    BOOL resources_applied;
    if (applied_character == NULL || applied_position == NULL) return FALSE;
    *applied_character = NULL;
    *applied_position = NULL;
    if (snapshot == NULL || snapshot->actor_type != expected_type ||
        snapshot->native_entity_id != expected_type ||
        !finite_position(snapshot) || !finite_facing(snapshot)) return FALSE;
    character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(actor);
    if (!readable_memory(character, CHARACTER_POSITION_OFFSET + sizeof(position))) {
        return FALSE;
    }
    position = *(void **)(character + CHARACTER_POSITION_OFFSET);
    if (!readable_memory(position, 0x5cu)) return FALSE;
    coordinates[0] = snapshot->x;
    coordinates[1] = snapshot->y;
    coordinates[2] = snapshot->z;
    facing[0] = snapshot->facing_x;
    facing[1] = 0.0f;
    facing[2] = snapshot->facing_z;
    dx = coordinates[0] - *(float *)((uint8_t *)position + 0x18u);
    dy = coordinates[1] - *(float *)((uint8_t *)position + 0x1cu);
    dz = coordinates[2] - *(float *)((uint8_t *)position + 0x20u);
    if (dx * dx + dy * dy + dz * dz > 0.00000001f) {
        set_position(position, coordinates);
    }
    current_facing_x = *(float *)((uint8_t *)position + 0x50u);
    current_facing_z = *(float *)((uint8_t *)position + 0x58u);
    current_facing_length = sqrtf(
        current_facing_x * current_facing_x +
        current_facing_z * current_facing_z);
    facing_dot = current_facing_length > 0.0001f ?
        (current_facing_x * facing[0] + current_facing_z * facing[2]) /
            current_facing_length : -1.0f;
    local_first_person_facing = expected_type ==
            SUDEKIMP_LAN_ARENA_AILISH_TYPE && combat_mode &&
        client_ailish_first_person_camera_owns_facing(character);
    /* SetForward rebuilds and dirties the complete CPosition basis. Apply a
     * mod-owned 0.5-degree hysteresis so interpolation noise cannot force a
     * rebuild on every rendered frame. The local first-person camera is the
     * sole exception: replaying a delayed host-facing sample here undoes its
     * mouse turn before the newly transmitted aim can make the round trip. */
    if (SudekiMpLanArenaClientShouldApplyHostFacing(
            expected_type == SUDEKIMP_LAN_ARENA_TAL_TYPE ? 0u : 1u,
            local_first_person_facing) &&
        (!isfinite(facing_dot) || facing_dot <= 0.99996f)) {
        call_position_set_forward(position, facing);
    }
    resources_applied = SudekiMpCleanroomEngineSetActorResources(
        actor, (float)snapshot->hp, (float)snapshot->sp);
    if (resources_applied && presentation_allowed) {
        (void)apply_actor_presentation(
            character, snapshot,
            expected_type == SUDEKIMP_LAN_ARENA_TAL_TYPE ? 0u : 1u,
            combat_mode);
    }
    if (resources_applied) {
        *applied_character = character;
        *applied_position = position;
    }
    return resources_applied;
}

static BOOL apply_training_dummy(
    const SudekiMpLanArenaEnemySnapshot *snapshot
) {
    uint8_t *entity;
    void *position;
    float coordinates[3];
    if (snapshot == NULL || snapshot->native_entity_id !=
            SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID ||
        !isfinite(snapshot->x) || !isfinite(snapshot->y) ||
        !isfinite(snapshot->z) || fabsf(snapshot->x) >= 1000000.0f ||
        fabsf(snapshot->y) >= 1000000.0f ||
        fabsf(snapshot->z) >= 1000000.0f) return FALSE;
    entity = (uint8_t *)SudekiMpCleanroomEngineGenericEntity(
        "MON_TrainingDummy");
    if (!readable_memory(entity, CHARACTER_POSITION_OFFSET + sizeof(position))) {
        return FALSE;
    }
    position = *(void **)(entity + CHARACTER_POSITION_OFFSET);
    if (!readable_memory(position, 0x24u)) return FALSE;
    coordinates[0] = snapshot->x;
    coordinates[1] = snapshot->y;
    coordinates[2] = snapshot->z;
    set_position(position, coordinates);
    return SudekiMpCleanroomEngineSetDummyHitPoints((float)snapshot->hp);
}

static void capture_actor_diagnostics(
    unsigned int actor_index,
    SudekiMpCleanroomActor actor,
    const SudekiMpLanArenaActorSnapshot *snapshot
) {
    SudekiMpLanArenaReplicaActorDiagnostics *diagnostics;
    uint8_t *character;
    uint8_t *position;
    uint8_t *wrapper;
    uint8_t *render_object;
    uint8_t *movement_controller;
    uint8_t *movement_component;
    const float *matrix;
    if (actor_index >= 2u || snapshot == NULL) return;
    diagnostics = &replica_diagnostics.actor[actor_index];
    diagnostics->position_valid = 0u;
    diagnostics->render_valid = 0u;
    diagnostics->movement_valid = 0u;
    diagnostics->sampled_position[0] = snapshot->x;
    diagnostics->sampled_position[1] = snapshot->y;
    diagnostics->sampled_position[2] = snapshot->z;
    diagnostics->sampled_facing[0] = snapshot->facing_x;
    diagnostics->sampled_facing[1] = snapshot->facing_z;
    character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(actor);
    if (!readable_memory(character, CHARACTER_POSITION_OFFSET + sizeof(void *))) {
        return;
    }
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    if (!readable_memory(position, 0xbcu)) return;
    diagnostics->native_position[0] = *(float *)(position + 0x18u);
    diagnostics->native_position[1] = *(float *)(position + 0x1cu);
    diagnostics->native_position[2] = *(float *)(position + 0x20u);
    diagnostics->native_facing[0] = *(float *)(position + 0x50u);
    diagnostics->native_facing[1] = *(float *)(position + 0x54u);
    diagnostics->native_facing[2] = *(float *)(position + 0x58u);
    diagnostics->local_yaw = *(float *)(position + 0x88u);
    diagnostics->native_dirty = *(uint8_t *)(position + 0xb8u);
    diagnostics->native_generation = *(uint16_t *)(position + 0xbau);
    diagnostics->position_valid = 1u;
    if (readable_memory(character, 0xb0u)) {
        movement_controller = *(uint8_t **)(character + 0x80u);
        movement_component = *(uint8_t **)(character + 0xacu);
        if (readable_memory(movement_controller, 0x6cu) &&
            readable_memory(movement_component, 0x54u)) {
            diagnostics->movement_target_speed =
                *(float *)(movement_controller + 0x24u);
            diagnostics->movement_smoothed_speed =
                *(float *)(movement_controller + 0x28u);
            diagnostics->movement_current_speed =
                *(float *)(movement_controller + 0x5cu);
            diagnostics->movement_run_blend =
                *(float *)(movement_controller + 0x60u);
            diagnostics->movement_mode =
                *(uint32_t *)(movement_controller + 0x68u);
            diagnostics->accepted_direction[0] =
                *(float *)(movement_component + 0x48u);
            diagnostics->accepted_direction[1] =
                *(float *)(movement_component + 0x4cu);
            diagnostics->accepted_direction[2] =
                *(float *)(movement_component + 0x50u);
            diagnostics->movement_valid = 1u;
        }
    }
    wrapper = *(uint8_t **)(position + POSITION_ATTACHED_WRAPPER_OFFSET);
    if (!readable_memory(wrapper, 0x0cu)) return;
    render_object = *(uint8_t **)(wrapper + 0x08u);
    if (!readable_memory(render_object, 0x90u + 16u * sizeof(float))) return;
    matrix = (const float *)(render_object + 0x90u);
    diagnostics->render_facing[0] = matrix[8];
    diagnostics->render_facing[1] = matrix[9];
    diagnostics->render_facing[2] = matrix[10];
    diagnostics->render_position[0] = matrix[12];
    diagnostics->render_position[1] = matrix[13];
    diagnostics->render_position[2] = matrix[14];
    diagnostics->render_valid = 1u;
}

static void capture_camera_diagnostics(void) {
    uint8_t *mode;
    uint8_t *camera_member;
    uint8_t *camera;
    uint8_t *render_state;
    const float *matrix;
    unsigned int index;

    replica_diagnostics.camera_valid = 0u;
    if (game_base == NULL || !readable_memory(
            game_base + RVA_GAME_CAMERA_MODE_GLOBAL, sizeof(mode))) return;
    mode = *(uint8_t **)(game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    if (!readable_memory(mode, 0x10u)) return;
    camera_member = *(uint8_t **)(mode + 0x0cu);
    if ((uintptr_t)camera_member < 0x2cu) return;
    camera = camera_member - 0x2cu;
    if (!readable_memory(camera, 0x38u)) return;
    render_state = *(uint8_t **)(camera + 0x34u);
    if (!readable_memory(render_state, 0xd0u)) return;
    matrix = (const float *)(render_state + 0x90u);
    for (index = 0u; index < 3u; ++index) {
        replica_diagnostics.camera_facing[index] = matrix[8u + index];
        replica_diagnostics.camera_position[index] = matrix[12u + index];
        if (!isfinite(replica_diagnostics.camera_facing[index]) ||
            !isfinite(replica_diagnostics.camera_position[index])) {
            return;
        }
    }
    replica_diagnostics.camera_valid = 1u;
}

BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    if ((client_combat_mode_lease_valid && !restore_client_combat_mode()) ||
        base == NULL || set_position != NULL ||
        position_world_matrix != NULL || set_forward != NULL ||
        memcmp(base + RVA_INTERNAL_POSITION_SETTER,
            expected_position_setter_prefix,
            sizeof(expected_position_setter_prefix)) != 0 ||
        memcmp(base + RVA_POSITION_WORLD_MATRIX,
            expected_position_world_matrix_entry,
            sizeof(expected_position_world_matrix_entry)) != 0 ||
        !relative_call_targets(
            base + RVA_POSITION_WORLD_MATRIX_UPDATE_CALL,
            base + RVA_POSITION_UPDATE) ||
        memcmp(base + RVA_POSITION_UPDATE,
            expected_position_update_entry,
            sizeof(expected_position_update_entry)) != 0 ||
        memcmp(base + RVA_POSITION_SET_FORWARD,
            expected_position_set_forward_entry,
            sizeof(expected_position_set_forward_entry)) != 0 ||
        !animation_renderer_signatures_match(base)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    set_position = (PositionSetterFunction)(base + RVA_INTERNAL_POSITION_SETTER);
    position_world_matrix = (PositionWorldMatrixFunction)(
        base + RVA_POSITION_WORLD_MATRIX);
    set_forward = base + RVA_POSITION_SET_FORWARD;
    game_base = base;
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaSharedSimulationReset(&replica_simulation);
    SudekiMpLanArenaReplicaRenderClockReset(&replica_render_clock);
    memset(presentation_leases, 0, sizeof(presentation_leases));
    memset(&ailish_first_person_lease, 0,
        sizeof(ailish_first_person_lease));
    ailish_first_person_failure = NULL;
    client_ailish_combat_graph_failure = NULL;
    memset(&replica_diagnostics, 0, sizeof(replica_diagnostics));
    clear_last_applied_frame();
    return TRUE;
}

void SudekiMpResetLanArenaClientReplica(void) {
    (void)restore_client_combat_mode();
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaSharedSimulationReset(&replica_simulation);
    set_position = NULL;
    position_world_matrix = NULL;
    set_forward = NULL;
    game_base = NULL;
    SudekiMpLanArenaReplicaRenderClockReset(&replica_render_clock);
    memset(presentation_leases, 0, sizeof(presentation_leases));
    memset(&ailish_first_person_lease, 0,
        sizeof(ailish_first_person_lease));
    ailish_first_person_failure = NULL;
    client_ailish_combat_graph_failure = NULL;
    memset(&replica_diagnostics, 0, sizeof(replica_diagnostics));
    clear_last_applied_frame();
}

void SudekiMpLanArenaClientReplicaDiscardSnapshots(void) {
    (void)restore_client_combat_mode();
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaSharedSimulationReset(&replica_simulation);
    SudekiMpLanArenaReplicaRenderClockReset(&replica_render_clock);
    memset(presentation_leases, 0, sizeof(presentation_leases));
    memset(&ailish_first_person_lease, 0,
        sizeof(ailish_first_person_lease));
    ailish_first_person_failure = NULL;
    client_ailish_combat_graph_failure = NULL;
    memset(&replica_diagnostics, 0, sizeof(replica_diagnostics));
    clear_last_applied_frame();
}

BOOL SudekiMpLanArenaClientReplicaApplyLatest(void) {
    SudekiMpLanArenaSnapshot received;
    SudekiMpLanArenaSnapshot accepted;
    SudekiMpLanArenaSnapshot snapshot;
    SudekiMpLanArenaSessionStatus status;
    DWORD now = GetTickCount();
    uint32_t render_host_tick;
    unsigned int presentation_ready_mask;
    if (set_position == NULL || set_forward == NULL ||
        !client_session_status(&status)) {
        SudekiMpLanArenaClientReplicaDiscardSnapshots();
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            &replica_simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
            status.session_token) &&
        !SudekiMpLanArenaSharedSimulationBegin(
            &replica_simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
            status.session_token)) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    while (SudekiMpLanArenaSessionTakeRemoteSnapshot(&received)) {
        if (!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
                &replica_simulation, status.session_token, &received) ||
            !SudekiMpLanArenaSharedSimulationReadFrame(
                &replica_simulation, &accepted, NULL) ||
            !SudekiMpLanArenaReplicaPush(&replica, &accepted)) return FALSE;
    }
    if (!SudekiMpLanArenaReplicaRenderClockAdvance(
            &replica, &replica_render_clock, now, &render_host_tick)) {
        return FALSE;
    }
    if (!SudekiMpLanArenaReplicaSample(
            &replica, render_host_tick, &snapshot) ||
        snapshot.match_state != 1u ||
        !synchronize_client_combat_mode(snapshot.combat_enabled)) {
        return FALSE;
    }
    presentation_ready_mask = client_combat_presentation_ready_mask();
    replica_diagnostics.valid = 0u;
    memset(replica_diagnostics.actor, 0,
        sizeof(replica_diagnostics.actor));
    capture_actor_diagnostics(
        0u, SUDEKIMP_CLEANROOM_TAL, &snapshot.tal);
    capture_actor_diagnostics(
        1u, SUDEKIMP_CLEANROOM_AILISH, &snapshot.ailish);
    {
        unsigned int actor_index;
        for (actor_index = 0u; actor_index < 2u; ++actor_index) {
            SudekiMpLanArenaReplicaActorDiagnostics *diagnostics =
                &replica_diagnostics.actor[actor_index];
            memcpy(diagnostics->pre_apply_position,
                diagnostics->native_position,
                sizeof(diagnostics->pre_apply_position));
            memcpy(diagnostics->pre_apply_facing,
                diagnostics->native_facing,
                sizeof(diagnostics->pre_apply_facing));
            diagnostics->pre_apply_target_speed =
                diagnostics->movement_target_speed;
            diagnostics->pre_apply_smoothed_speed =
                diagnostics->movement_smoothed_speed;
            diagnostics->pre_apply_current_speed =
                diagnostics->movement_current_speed;
        }
    }
    /* Each actor is independently validated and applied before the combined
     * frame is admitted. This avoids short-circuiting Tal diagnostics merely
     * because Ailish failed, while still refusing a partially valid frame. */
    {
        BOOL ailish_applied = apply_actor(
            &snapshot.ailish, SUDEKIMP_CLEANROOM_AILISH,
            SUDEKIMP_LAN_ARENA_AILISH_TYPE,
            snapshot.combat_enabled != 0u,
            (presentation_ready_mask & 0x02u) != 0u,
            &last_applied_characters[1],
            &last_applied_positions[1]);
        BOOL tal_applied = apply_actor(
            &snapshot.tal, SUDEKIMP_CLEANROOM_TAL,
            SUDEKIMP_LAN_ARENA_TAL_TYPE,
            snapshot.combat_enabled != 0u,
            (presentation_ready_mask & 0x01u) != 0u,
            &last_applied_characters[0],
            &last_applied_positions[0]);
        if (!ailish_applied || !tal_applied) {
            clear_last_applied_frame();
            return FALSE;
        }
    }
    if (last_applied_characters[0] == NULL ||
        last_applied_characters[1] == NULL ||
        last_applied_positions[0] == NULL ||
        last_applied_positions[1] == NULL) {
        clear_last_applied_frame();
        return FALSE;
    }
    last_applied_snapshot = snapshot;
    replica_diagnostics.sequence = snapshot.sequence;
    replica_diagnostics.upper_snapshot_host_tick = snapshot.host_tick;
    replica_diagnostics.render_host_tick = render_host_tick;
    replica_diagnostics.sampled_at_ms = now;
    capture_camera_diagnostics();
    capture_actor_diagnostics(
        0u, SUDEKIMP_CLEANROOM_TAL, &snapshot.tal);
    capture_actor_diagnostics(
        1u, SUDEKIMP_CLEANROOM_AILISH, &snapshot.ailish);
    {
        unsigned int actor_index;
        for (actor_index = 0u; actor_index < 2u; ++actor_index) {
            SudekiMpLanArenaReplicaActorDiagnostics *diagnostics =
                &replica_diagnostics.actor[actor_index];
            memcpy(diagnostics->post_apply_position,
                diagnostics->native_position,
                sizeof(diagnostics->post_apply_position));
            memcpy(diagnostics->post_apply_facing,
                diagnostics->native_facing,
                sizeof(diagnostics->post_apply_facing));
        }
    }
    replica_diagnostics.valid = 1u;
    if (snapshot.enemy_count == 0u) return TRUE;
    return snapshot.enemy_count == 1u &&
        apply_training_dummy(&snapshot.enemies[0]);
}

BOOL SudekiMpLanArenaClientReplicaReassertPresentation(void) {
    static const SudekiMpCleanroomActor actors[2] = {
        SUDEKIMP_CLEANROOM_TAL,
        SUDEKIMP_CLEANROOM_AILISH
    };
    static const uint8_t expected_types[2] = {
        SUDEKIMP_LAN_ARENA_TAL_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE
    };
    const SudekiMpLanArenaActorSnapshot *snapshots[2];
    uint8_t *characters[2];
    unsigned int actor_index;
    unsigned int presentation_ready_mask;
    BOOL applied[2] = { FALSE, FALSE };
    if (!client_session_authenticated() || !replica_diagnostics.valid ||
        last_applied_snapshot.match_state != 1u) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    presentation_ready_mask = client_combat_presentation_ready_mask();
    if (presentation_ready_mask == 0u) return TRUE;
    snapshots[0] = &last_applied_snapshot.tal;
    snapshots[1] = &last_applied_snapshot.ailish;
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        characters[actor_index] = (uint8_t *)
            SudekiMpCleanroomEngineActorEntity(actors[actor_index]);
        if (snapshots[actor_index]->actor_type != expected_types[actor_index] ||
            snapshots[actor_index]->native_entity_id !=
                expected_types[actor_index] ||
            characters[actor_index] != last_applied_characters[actor_index] ||
            !readable_memory(characters[actor_index],
                CHARACTER_POSITION_OFFSET + sizeof(void *)) ||
            *(void **)(characters[actor_index] + CHARACTER_POSITION_OFFSET) !=
                last_applied_positions[actor_index]) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
    }
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        if ((presentation_ready_mask & (1u << actor_index)) == 0u) {
            applied[actor_index] = TRUE;
            continue;
        }
        applied[actor_index] = apply_actor_presentation(
            characters[actor_index], snapshots[actor_index], actor_index,
            last_applied_snapshot.combat_enabled != 0u);
    }
    if (!applied[0] || !applied[1]) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    return TRUE;
}

static BOOL actor_visible_transform_matches_position(
    uint8_t *position,
    uint8_t *render_object
) {
    const float *matrix;
    float position_x;
    float position_y;
    float position_z;
    float render_x;
    float render_y;
    float render_z;
    float position_facing_x;
    float position_facing_z;
    float render_facing_x;
    float render_facing_z;
    float position_facing_length;
    float render_facing_length;
    float facing_dot;
    if (!readable_memory(position, 0x104u) ||
        !readable_memory(render_object, 0x90u + 16u * sizeof(float))) {
        return FALSE;
    }
    matrix = (const float *)(render_object + 0x90u);
    position_x = *(float *)(position + 0x18u);
    position_y = *(float *)(position + 0x1cu);
    position_z = *(float *)(position + 0x20u);
    render_x = matrix[12];
    render_y = matrix[13];
    render_z = matrix[14];
    position_facing_x = *(float *)(position + 0x50u);
    position_facing_z = *(float *)(position + 0x58u);
    render_facing_x = matrix[8];
    render_facing_z = matrix[10];
    position_facing_length = sqrtf(
        position_facing_x * position_facing_x +
        position_facing_z * position_facing_z);
    render_facing_length = sqrtf(
        render_facing_x * render_facing_x +
        render_facing_z * render_facing_z);
    if (!isfinite(position_x) || !isfinite(position_y) ||
        !isfinite(position_z) || !isfinite(render_x) ||
        !isfinite(render_y) || !isfinite(render_z) ||
        !isfinite(position_facing_length) ||
        !isfinite(render_facing_length) ||
        position_facing_length < 0.5f || render_facing_length < 0.5f) {
        return FALSE;
    }
    facing_dot =
        (position_facing_x * render_facing_x +
         position_facing_z * render_facing_z) /
        (position_facing_length * render_facing_length);
    return fabsf(position_x - render_x) <= 0.01f &&
        fabsf(position_y - render_y) <= 0.01f &&
        fabsf(position_z - render_z) <= 0.01f &&
        isfinite(facing_dot) && facing_dot >= 0.9995f;
}

BOOL SudekiMpLanArenaClientReplicaPublishVisibleTransforms(void) {
    static const SudekiMpCleanroomActor actors[2] = {
        SUDEKIMP_CLEANROOM_TAL,
        SUDEKIMP_CLEANROOM_AILISH
    };
    static const uint8_t expected_types[2] = {
        SUDEKIMP_LAN_ARENA_TAL_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE
    };
    const SudekiMpLanArenaActorSnapshot *snapshots[2];
    uint8_t *positions[2];
    uint8_t *render_objects[2];
    unsigned int actor_index;
    if (position_world_matrix == NULL || !client_session_authenticated() ||
        !replica_diagnostics.valid || last_applied_snapshot.match_state != 1u) {
        clear_last_applied_frame();
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    snapshots[0] = &last_applied_snapshot.tal;
    snapshots[1] = &last_applied_snapshot.ailish;
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        uint8_t *character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            actors[actor_index]);
        uint8_t *wrapper;
        if (snapshots[actor_index]->actor_type != expected_types[actor_index] ||
            snapshots[actor_index]->native_entity_id !=
                expected_types[actor_index] ||
            last_applied_characters[actor_index] != character ||
            !readable_memory(character,
                CHARACTER_POSITION_OFFSET + sizeof(void *))) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        positions[actor_index] = *(uint8_t **)(
            character + CHARACTER_POSITION_OFFSET);
        if (positions[actor_index] != last_applied_positions[actor_index] ||
            !readable_memory(positions[actor_index], 0x104u) ||
            !writable_memory(positions[actor_index] + 0xb8u, 1u)) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        /* The current verifier compares the local CPosition transform with
         * the attached world render object. Cleanroom Tal and Ailish are
         * unparented roots (NULL or retail sentinel 4). Reject a newly
         * parented/locator-bound actor instead of repeatedly dirtying a valid
         * composed world matrix with a local-space comparison. */
        {
            uintptr_t parent_link = *(uintptr_t *)(positions[actor_index] + 0x94u);
            if (parent_link != 0u && parent_link != 4u) {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
        }
        wrapper = *(uint8_t **)(
            positions[actor_index] + POSITION_ATTACHED_WRAPPER_OFFSET);
        if (!readable_memory(wrapper, 0x0cu)) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        render_objects[actor_index] = *(uint8_t **)(wrapper + 0x08u);
        if (!readable_memory(render_objects[actor_index],
                0x90u + 16u * sizeof(float)) ||
            !writable_memory(render_objects[actor_index] + 0x2cu,
                sizeof(uint32_t)) ||
            !writable_memory(render_objects[actor_index] + 0x90u,
                16u * sizeof(float))) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
    }
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        const float *world_matrix = position_world_matrix(
            positions[actor_index]);
        if (!readable_memory(
                world_matrix, 16u * sizeof(float))) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        if (!actor_visible_transform_matches_position(
                positions[actor_index], render_objects[actor_index])) {
            /* Sudeki may consume CPosition's dirty flag and then regenerate a
             * stale attached model basis from its accepted-direction state.
             * Re-arm only this proven native flag and let the parent-aware
             * world-matrix getter rebuild, compose, publish, and update all
             * native generations. The verification deliberately owns only
             * translation and horizontal forward: animation/presentation may
             * legitimately change other world-matrix components. */
            *(uint8_t *)(positions[actor_index] + 0xb8u) = 1u;
            world_matrix = position_world_matrix(positions[actor_index]);
            if (!readable_memory(
                    world_matrix, 16u * sizeof(float)) ||
                !actor_visible_transform_matches_position(
                    positions[actor_index], render_objects[actor_index])) {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
        }
    }
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        if (!actor_visible_transform_matches_position(
                positions[actor_index], render_objects[actor_index])) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
    }
    SudekiMpLanArenaClientReplicaRefreshDiagnostics();
    return TRUE;
}

void SudekiMpLanArenaClientReplicaRefreshDiagnostics(void) {
    if (!replica_diagnostics.valid) return;
    capture_actor_diagnostics(
        0u, SUDEKIMP_CLEANROOM_TAL, &last_applied_snapshot.tal);
    capture_actor_diagnostics(
        1u, SUDEKIMP_CLEANROOM_AILISH, &last_applied_snapshot.ailish);
    capture_camera_diagnostics();
}

BOOL SudekiMpLanArenaClientReplicaGetDiagnostics(
    SudekiMpLanArenaReplicaDiagnostics *diagnostics
) {
    if (diagnostics == NULL || !replica_diagnostics.valid) return FALSE;
    *diagnostics = replica_diagnostics;
    return TRUE;
}
