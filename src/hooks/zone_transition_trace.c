#include "hooks/zone_transition_trace.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "engine/transition_vote.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/interaction_provenance.h"
#include "hooks/split_screen_render.h"
#include "input/bridge_receiver.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__cdecl *ZoneFunction)(const char *zone_name);
typedef void (__cdecl *SetPlayerPositionFunction)(
    float x,
    float y,
    float z
);
typedef void (__attribute__((fastcall)) *InternalPositionSetterFunction)(
    void *position,
    const float *coordinates
);
typedef BOOL (__attribute__((thiscall)) *SetRenderCameraFunction)(
    void *manager,
    const char *name
);
/* FUN_004352D0 ends with ret 4; this is stdcall, not cdecl. */
typedef unsigned char (__attribute__((stdcall)) *TemporaryCameraStateUpdateFunction)(
    void *camera_state
);
typedef void (__attribute__((thiscall)) *SwitchMainZoneFunction)(
    void *world,
    const char *zone_name
);
typedef void (__attribute__((thiscall)) *EnterTemporaryZoneFunction)(
    void *world,
    const char *zone_name,
    const void *resource_name
);
typedef void (__attribute__((thiscall)) *ExitTemporaryZoneFunction)(
    void *world
);
typedef unsigned char (__attribute__((thiscall)) *DoorActivateFunction)(
    void *door,
    BOOL requested,
    BOOL forced
);
typedef void (__cdecl *PopToNamedLocationFunction)(
    void *entity_pointer,
    const void *resource_name
);
typedef void (__attribute__((stdcall)) *ExitLeadMoveFunction)(
    void *mover,
    const void *gel_location,
    uint32_t flag_zero,
    uint32_t flag_one,
    float seconds
);
typedef void (__cdecl *FormationPopMembersFunction)(void);
typedef void (__attribute__((thiscall)) *SetModeFullPartyFunction)(
    void *group
);
typedef void (__attribute__((thiscall)) *SetModeLeadOnlyFunction)(
    void *group
);
typedef void (__attribute__((thiscall)) *PartyPresentationFunction)(
    void *group
);

enum {
    RVA_SET_ZONE_NOW = 0x00007910u,
    RVA_ENTER_ZONE = 0x00007970u,
    RVA_SWITCH_ZONE_NOW = 0x00007990u,
    RVA_LOAD_ZONE = 0x00007b80u,
    RVA_SWITCH_MAIN_ZONE = 0x00006380u,
    RVA_DOOR_ACTIVATE_FROM_SCRIPT = 0x000ce3a0u,
    RVA_ENTER_TEMPORARY_ZONE = 0x000064b0u,
    RVA_EXIT_TEMPORARY_ZONE = 0x00006710u,
    RVA_SET_PLAYER_POSITION = 0x00104ed0u,
    RVA_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_TEMPORARY_CAMERA_STATE_UPDATE = 0x000352d0u,
    RVA_ENTER_LEAD_POP_CALL = 0x00005c59u,
    RVA_EXIT_LEAD_MOVE_CALL = 0x000068d3u,
    RVA_POP_TO_NAMED_LOCATION = 0x000f63d0u,
    RVA_EXIT_LEAD_MOVE = 0x000f30a0u,
    RVA_FORMATION_POP_MEMBERS = 0x000f6260u,
    RVA_SET_MODE_LEAD_ONLY = 0x00024720u,
    RVA_SET_MODE_FULL_PARTY = 0x00024850u,
    RVA_SHOW_PARTY_MEMBERS = 0x00024950u,
    RVA_HIDE_PARTY_MEMBERS = 0x00024a70u,
    RVA_WORLD_GLOBAL = 0x00408d10u,
    RVA_AI_MANAGER_GLOBAL = 0x00409de4u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
};

enum {
    PARTY_ATOMIC_NONE = 0,
    PARTY_ATOMIC_ENTER_TEMPORARY = 1,
    PARTY_ATOMIC_EXIT_TEMPORARY = 2,
    PARTY_ATOMIC_TIMEOUT_MS = 15000u,
    PARTY_ATOMIC_RELEASE_TIMEOUT_MS = 2000u,
    PARTY_DOORWAY_STAGING_TIMEOUT_MS = 1500u,
    PARTY_PRESENTATION_CANDIDATE_TIMEOUT_MS = 5000u,
    TRANSITION_VOTE_TIMEOUT_MS = 5000u,
    TRANSITION_VOTE_OVERLAY_REPORT_TIMEOUT_MS = 1000u,
    PARTY_COUNT_OFFSET = 0xccu,
    PARTY_SLOT_ZERO_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    PARTY_SLOT_COUNT = 4u,
    CHARACTER_POSITION_OFFSET = 0x44u,
    CHARACTER_MODEL_INTERFACE_OFFSET = 0x58u,
    MODEL_HIDE_DEPTH_OFFSET = 0x74u,
    PRESENTATION_BODY_POSITION_OFFSET = 0x44u,
    POSITION_RENDER_WRAPPER_OFFSET = 0xb4u,
    RENDER_WRAPPER_OBJECT_OFFSET = 0x08u,
    RENDER_OBJECT_FLAGS_OFFSET = 0x34u,
    RENDER_OBJECT_HIDDEN_FLAG = 0x00000004u,
    POSITION_XYZ_OFFSET = 0x18u
};

static const float party_placement_maximum_distance = 5.0f;
static const float doorway_staging_minimum_extent = 0.50f;
static const float doorway_staging_maximum_extent = 4.50f;
static const float doorway_staging_advance_margin = 0.10f;
static const float doorway_staging_follower_tolerance_squared = 0.0625f;
static const float doorway_staging_direction_alignment = 0.80f;

static const uint8_t set_zone_now_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x51u
};
static const uint8_t enter_zone_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u
};
static const uint8_t switch_zone_now_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u
};
static const uint8_t load_zone_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x8bu, 0x0du, 0x10u, 0x8du,
    0x80u, 0x00u
};
static const uint8_t switch_main_zone_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x56u, 0x8bu, 0xf1u
};
static const uint8_t door_activate_entry[] = {
    0x80u, 0x7cu, 0x24u, 0x04u, 0x00u, 0x56u, 0x8bu, 0xf1u
};
static const uint8_t enter_temporary_zone_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu, 0x24u
};
static const uint8_t exit_temporary_zone_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf0u, 0x81u, 0xecu, 0x84u,
    0x00u, 0x00u, 0x00u
};
static const uint8_t set_player_position_entry[] = {
    /* The following absolute global is relocated at runtime; gate only the
     * stable prologue and leave the relocated operand untouched. */
    0x83u, 0xecu, 0x18u, 0x8bu, 0x0du
};
static const uint8_t internal_position_setter_entry[] = {
    0xd9u, 0x41u, 0x18u, 0xd9u, 0x02u
};
static const uint8_t set_render_camera_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t temporary_camera_state_update_entry[] = {
    /* Include the complete `and esp, -8` instruction; cutting it at
     * five bytes makes the inline trampoline jump into its final immediate. */
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t formation_pop_members_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x10u, 0x05u, 0xf4u, 0x00u, 0x00u,
    0x00u, 0x74u, 0x09u, 0x6au, 0x00u, 0x6au, 0x00u
};
static const uint8_t set_mode_full_party_entry[] = {
    0x83u, 0xecu, 0x10u, 0x53u, 0x8bu, 0xd9u, 0x83u,
    0xbbu, 0xd0u, 0x00u, 0x00u, 0x00u, 0x01u
};
static const uint8_t set_mode_lead_only_entry[] = {
    0x83u, 0xecu, 0x14u, 0x83u, 0xb9u, 0xd0u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x89u, 0x4cu, 0x24u, 0x04u
};
static const uint8_t show_party_members_entry[] = {
    0x83u, 0xecu, 0x10u, 0x53u, 0x55u, 0x56u, 0x57u,
    0x8du, 0xa9u, 0x9cu, 0x00u, 0x00u, 0x00u
};
static const uint8_t hide_party_members_entry[] = {
    0x83u, 0xecu, 0x10u, 0x53u, 0x55u, 0x56u, 0x57u,
    0x8du, 0xa9u, 0x9cu, 0x00u, 0x00u, 0x00u
};

static SudekiMpInlineHook set_zone_now_hook;
static SudekiMpInlineHook enter_zone_hook;
static SudekiMpInlineHook switch_zone_now_hook;
static SudekiMpInlineHook load_zone_hook;
static SudekiMpInlineHook switch_main_zone_hook;
static SudekiMpInlineHook door_activate_hook;
static SudekiMpInlineHook enter_temporary_zone_hook;
static SudekiMpInlineHook exit_temporary_zone_hook;
static SudekiMpInlineHook set_player_position_hook;
static SudekiMpInlineHook internal_position_setter_hook;
static SudekiMpInlineHook set_render_camera_hook;
static SudekiMpInlineHook temporary_camera_state_update_hook;
static SudekiMpInlineHook hide_party_members_hook;
static SudekiMpRelativeCallHook enter_lead_pop_hook;
static SudekiMpRelativeCallHook exit_lead_move_hook;
static ZoneFunction original_set_zone_now;
static ZoneFunction original_enter_zone;
static ZoneFunction original_switch_zone_now;
static ZoneFunction original_load_zone;
static SwitchMainZoneFunction original_switch_main_zone;
static DoorActivateFunction original_door_activate;
static EnterTemporaryZoneFunction original_enter_temporary_zone;
static ExitTemporaryZoneFunction original_exit_temporary_zone;
static SetPlayerPositionFunction original_set_player_position;
static InternalPositionSetterFunction original_internal_position_setter;
static SetRenderCameraFunction original_set_render_camera;
static TemporaryCameraStateUpdateFunction original_temporary_camera_state_update;
static PopToNamedLocationFunction original_enter_lead_pop;
static ExitLeadMoveFunction original_exit_lead_move;
static FormationPopMembersFunction formation_pop_members;
static SetModeFullPartyFunction set_mode_full_party;
static SetModeLeadOnlyFunction set_mode_lead_only;
static PartyPresentationFunction show_party_members;
static PartyPresentationFunction hide_party_members;
static HMODULE trace_module;
static void *last_world;
static char current_world_name[64];
static BOOL current_world_confirmed;
static char current_temporary_name[64];
static SudekiMpResourceName active_temporary_resource;
static BOOL active_temporary_resource_valid;
static unsigned int temporary_position_samples;
static unsigned int temporary_camera_samples;
static BOOL camera_trace_enabled;
static BOOL party_atomic_transitions_enabled;
static BOOL transition_vote_requested;
static BOOL transition_vote_enabled;
static unsigned int zone_source_generation;
static SudekiMpTransitionVote transition_vote;
static unsigned int party_atomic_transition_kind;
static unsigned int party_atomic_transition_serial;
static DWORD party_atomic_transition_deadline;
static DWORD party_atomic_release_deadline;
static BOOL party_atomic_position_scope;
static BOOL party_atomic_lead_setter_seen;
static void *party_atomic_expected_lead_position;
static BOOL party_atomic_placement_confirmed;
static BOOL party_atomic_presentation_attempted;
static BOOL party_atomic_presentation_confirmed;
static void *party_atomic_settle_descriptor;
static DWORD party_atomic_settle_since;
static unsigned int set_zone_now_depth;

typedef struct SudekiMpPartyPlacementSnapshot {
    void *group;
    int party_count;
    void *actors[PARTY_SLOT_COUNT];
    float positions[PARTY_SLOT_COUNT][3];
} SudekiMpPartyPlacementSnapshot;

typedef struct SudekiMpDoorwayStaging {
    BOOL armed;
    DWORD deadline;
    unsigned int transition_serial;
    void *world;
    void *descriptor;
    unsigned int source_generation;
    uint32_t last_player_two_sequence;
    float inward_x;
    float inward_z;
    float required_inward_advance;
    char destination[64];
    SudekiMpPartyPlacementSnapshot placement;
} SudekiMpDoorwayStaging;

static SudekiMpDoorwayStaging party_doorway_staging;
static BOOL transition_vote_raw_input_neutral(
    const SudekiMpInputBridgeState *state
);
static BOOL readable_zone_bytes(const char *source, size_t size);

typedef struct SudekiMpPartyPresentationFollower {
    void *character;
    void *model_interface;
    void *body_render_object;
    int16_t baseline_hide_depth;
} SudekiMpPartyPresentationFollower;

typedef struct SudekiMpPartyPresentationLease {
    BOOL snapshot_valid;
    BOOL override_active;
    BOOL exit_balance_pending;
    void *group;
    void *lead_character;
    int party_count;
    DWORD snapshot_tick;
    DWORD capture_thread_id;
    SudekiMpPartyPresentationFollower followers[PARTY_SLOT_COUNT - 1u];
} SudekiMpPartyPresentationLease;

static SudekiMpPartyPresentationLease party_presentation_lease;
static SudekiMpPartyPresentationLease party_presentation_hide_candidate;
static LONG party_presentation_hide_depth;
static unsigned int party_presentation_hide_sequence;

typedef struct SudekiMpPendingTransitionVote {
    BOOL valid;
    BOOL resource_valid;
    BOOL overlay_acknowledged;
    BOOL player_two_consent_armed;
    void *world;
    void *source_descriptor;
    unsigned int source_generation;
    DWORD overlay_report_deadline;
    SudekiMpResourceName resource;
    SudekiMpPartyPresentationLease presentation;
    uint32_t last_player_two_sequence;
    uint32_t last_player_two_buttons;
    BOOL player_one_escape_was_down;
    char destination[64];
} SudekiMpPendingTransitionVote;

static SudekiMpPendingTransitionVote pending_transition_vote;
static BOOL transition_vote_visibility_quarantined;
static SudekiMpPartyPresentationLease
    transition_vote_visibility_quarantine;
static BOOL party_presentation_snapshot_matches(
    const SudekiMpPartyPresentationLease *lease,
    int depth_delta,
    BOOL require_hidden,
    BOOL require_exact_identity
);

static void quarantine_transition_vote_visibility(
    const SudekiMpPartyPresentationLease *presentation,
    const char *reason
) {
    if (!transition_vote_visibility_quarantined) {
        transition_vote_visibility_quarantined = TRUE;
        ZeroMemory(&transition_vote_visibility_quarantine,
            sizeof(transition_vote_visibility_quarantine));
        if (presentation != NULL) {
            transition_vote_visibility_quarantine = *presentation;
        }
        SudekiMpLogFormat(
            "transition_vote event=visibility_quarantine state=entered "
            "reason=%s snapshot=%s "
            "policy=block_all_later_natural_temp_entries_until_process_restart\r\n",
            reason == NULL ? "unconfirmed_visibility_accounting" : reason,
            presentation != NULL && presentation->snapshot_valid ?
                "retained" : "unavailable");
    }
}

BOOL SudekiMpZoneTransitionConfigureVote(BOOL enabled) {
    if (trace_module != NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    transition_vote_requested = enabled != FALSE;
    return TRUE;
}

BOOL SudekiMpZoneTransitionGetVoteSnapshot(
    SudekiMpZoneTransitionVoteSnapshot *snapshot
) {
    DWORD now;

    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    ZeroMemory(snapshot, sizeof(*snapshot));
    now = GetTickCount();
    snapshot->active = pending_transition_vote.valid &&
        (transition_vote.state == SUDEKIMP_TRANSITION_VOTE_WAITING ||
         transition_vote.state == SUDEKIMP_TRANSITION_VOTE_READY ||
         transition_vote.state == SUDEKIMP_TRANSITION_VOTE_COMMITTING);
    snapshot->state = (unsigned int)transition_vote.state;
    snapshot->serial = transition_vote.serial;
    snapshot->remaining_ms = SudekiMpTransitionVoteRemainingMs(
        &transition_vote, now);
    snapshot->requester_index = transition_vote.requester_index;
    snapshot->participant_mask = transition_vote.participant_mask;
    snapshot->accepted_mask = transition_vote.accepted_mask;
    snapshot->cancelled_mask = transition_vote.cancelled_mask;
    memcpy(snapshot->destination, pending_transition_vote.destination,
        sizeof(snapshot->destination));
    return TRUE;
}

#define ZONE_ARRIVAL_CONTEXT_CAPACITY 32u

/* This is deliberately a small authored registry, not a guessed list of
 * arbitrary strings.  These names are the same destinations exposed by the
 * cleanroom traversal page and confirmed in the archive/door research.  A
 * first request is allowed to enter the native pipeline; the position hook
 * then records the real actor anchors without requiring a manual visit. */
static const char *known_traversal_worlds[] = {
    "NewBrightwater",
    "Illumina_Countryside_Hub",
    "Illumina_Countryside_NE",
    "Illumina_Countryside_SE",
    "Illumina_Countryside_SW",
    "Illumina_Countryside_NW"
};

static const char *known_traversal_interiors[][2] = {
    {"NewBrightwater", "LNBr_Church"},
    {"NewBrightwater", "LNBr_Kamo_shop"},
    {"NewBrightwater", "LNBr_Kilks_house"},
    {"NewBrightwater", "LNBr_Lighthouse"},
    {"NewBrightwater", "LNBr_Salty_dog_Inn"},
    {"NewBrightwater", "LNBr_ShortTent"},
    {"NewBrightwater", "LNBr_TallTent01"},
    {"NewBrightwater", "LNBr_TallTent02"},
    {"Illumina_Countryside_SE", "LICo_Athlos_Shack"},
    {"Illumina_Countryside_SE", "LICo_Frappe_Farm"},
    {"Illumina_Countryside_SE", "LICo_Porkins"},
    {"Illumina_Countryside_SE", "LICo_SW_Trader_Cave"}
};

typedef struct SudekiMpZoneArrivalContext {
    BOOL valid;
    BOOL temporary;
    char world_name[64];
    char destination_name[64];
    float actor_positions[SUDEKIMP_CLEANROOM_ACTOR_COUNT][3];
    BOOL actor_position_valid[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
    float fallback_position[3];
    BOOL fallback_position_valid;
    unsigned short camera_index;
} SudekiMpZoneArrivalContext;

static SudekiMpZoneArrivalContext arrival_contexts[
    ZONE_ARRIVAL_CONTEXT_CAPACITY];
static SudekiMpZoneArrivalContext *arrival_capture_context;
static BOOL arrival_capture_armed;
static BOOL arrival_reapply_pending;
static DWORD arrival_reapply_after;
static DWORD arrival_reapply_deadline;
static unsigned int arrival_reapply_attempts;

static uint32_t zone_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void release_active_temporary_resource(void) {
    if (active_temporary_resource_valid) {
        SudekiMpCleanroomEngineReleaseResourceName(
            &active_temporary_resource);
        active_temporary_resource_valid = FALSE;
    }
}

static void clear_pending_transition_vote(void) {
    if (pending_transition_vote.resource_valid) {
        SudekiMpCleanroomEngineReleaseResourceName(
            &pending_transition_vote.resource);
    }
    ZeroMemory(&pending_transition_vote,
        sizeof(pending_transition_vote));
    SudekiMpTransitionVoteReset(&transition_vote);
    SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
}

static void cancel_pending_transition_vote(const char *reason) {
    if (!pending_transition_vote.valid) {
        return;
    }
    if (pending_transition_vote.presentation.snapshot_valid &&
        !party_presentation_snapshot_matches(
            &pending_transition_vote.presentation, 0, FALSE, TRUE)) {
        quarantine_transition_vote_visibility(
            &pending_transition_vote.presentation,
            "cancel_with_unconfirmed_waiting_visibility");
    }
    SudekiMpLogFormat(
        "transition_vote event=cancel serial=%lu destination=%s "
        "reason=%s participant_mask=0x%02x accepted_mask=0x%02x "
        "cancelled_mask=0x%02x policy=leave_exterior_visible\r\n",
        (unsigned long)transition_vote.serial,
        pending_transition_vote.destination,
        reason == NULL ? "unspecified" : reason,
        (unsigned int)transition_vote.participant_mask,
        (unsigned int)transition_vote.accepted_mask,
        (unsigned int)transition_vote.cancelled_mask);
    clear_pending_transition_vote();
}

BOOL SudekiMpZoneTransitionReportVoteOverlay(
    uint32_t serial,
    BOOL visible
) {
    if (!pending_transition_vote.valid || serial == 0u ||
        serial != transition_vote.serial ||
        transition_vote.state != SUDEKIMP_TRANSITION_VOTE_WAITING) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!visible) {
        cancel_pending_transition_vote("overlay_draw_failed");
        return TRUE;
    }
    if (!pending_transition_vote.overlay_acknowledged) {
        if (!SudekiMpTransitionVoteRestartDeadline(
                &transition_vote,
                serial,
                GetTickCount(),
                TRANSITION_VOTE_TIMEOUT_MS)) {
            cancel_pending_transition_vote(
                "overlay_countdown_restart_rejected");
            SetLastError(ERROR_INVALID_STATE);
            return FALSE;
        }
        pending_transition_vote.overlay_acknowledged = TRUE;
        SudekiMpLogFormat(
            "transition_vote event=overlay status=visible serial=%lu "
            "countdown_ms=%u policy=full_visible_window_before_commit\r\n",
            (unsigned long)serial,
            TRANSITION_VOTE_TIMEOUT_MS);
    }
    return TRUE;
}

static void bump_zone_source_generation(const char *reason) {
    if (pending_transition_vote.valid) {
        cancel_pending_transition_vote(reason);
    }
    ++zone_source_generation;
    if (zone_source_generation == 0u) {
        zone_source_generation = 1u;
    }
    SudekiMpInteractionProvenanceSetSourceGeneration(
        zone_source_generation);
}

BOOL SudekiMpZoneTransitionGetSourceSnapshot(
    uint32_t *source_generation,
    uintptr_t *world_identity
) {
    uint8_t *base = (uint8_t *)trace_module;
    void *world;

    if (source_generation == NULL || world_identity == NULL ||
        base == NULL || zone_source_generation == 0u ||
        !readable_zone_bytes(
            (const char *)(base + RVA_WORLD_GLOBAL), sizeof(world))) {
        return FALSE;
    }
    world = *(void **)(base + RVA_WORLD_GLOBAL);
    if (world == NULL) {
        return FALSE;
    }
    *source_generation = zone_source_generation;
    *world_identity = (uintptr_t)world;
    return TRUE;
}

static BOOL readable_zone_bytes(const char *source, size_t size) {
    MEMORY_BASIC_INFORMATION information;

    if (source == NULL || size == 0u ||
        VirtualQuery(source, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    return (uintptr_t)source + size <=
        (uintptr_t)information.BaseAddress + information.RegionSize;
}

static BOOL formation_pop_members_signature_matches(uint8_t *base) {
    uint32_t relocated_ai_manager;

    if (base == NULL ||
        base[RVA_FORMATION_POP_MEMBERS] != 0xa1u ||
        memcmp(
            base + RVA_FORMATION_POP_MEMBERS + 5u,
            formation_pop_members_tail,
            sizeof(formation_pop_members_tail)) != 0) {
        return FALSE;
    }
    memcpy(
        &relocated_ai_manager,
        base + RVA_FORMATION_POP_MEMBERS + 1u,
        sizeof(relocated_ai_manager));
    return relocated_ai_manager == (uint32_t)(uintptr_t)(
        base + RVA_AI_MANAGER_GLOBAL);
}

static BOOL set_mode_full_party_signature_matches(uint8_t *base) {
    return base != NULL && memcmp(
        base + RVA_SET_MODE_FULL_PARTY,
        set_mode_full_party_entry,
        sizeof(set_mode_full_party_entry)) == 0;
}

static BOOL set_mode_lead_only_signature_matches(uint8_t *base) {
    return base != NULL && memcmp(
        base + RVA_SET_MODE_LEAD_ONLY,
        set_mode_lead_only_entry,
        sizeof(set_mode_lead_only_entry)) == 0;
}

static BOOL party_presentation_signatures_match(uint8_t *base) {
    return base != NULL && memcmp(
            base + RVA_SHOW_PARTY_MEMBERS,
            show_party_members_entry,
            sizeof(show_party_members_entry)) == 0 &&
        memcmp(
            base + RVA_HIDE_PARTY_MEMBERS,
            hide_party_members_entry,
            sizeof(hide_party_members_entry)) == 0;
}

static void clear_party_presentation_lease(void) {
    ZeroMemory(
        &party_presentation_lease,
        sizeof(party_presentation_lease));
}

static void clear_party_presentation_hide_candidate(void) {
    ZeroMemory(
        &party_presentation_hide_candidate,
        sizeof(party_presentation_hide_candidate));
}

static void *presentation_render_object_from_wrapper(void *wrapper) {
    uint8_t *bytes = (uint8_t *)wrapper;
    void *render_object;

    if (bytes == NULL) {
        return NULL;
    }
    if (!readable_zone_bytes((const char *)bytes,
            RENDER_WRAPPER_OBJECT_OFFSET + sizeof(render_object))) {
        return NULL;
    }
    render_object = *(void **)(bytes + RENDER_WRAPPER_OBJECT_OFFSET);
    return readable_zone_bytes((const char *)render_object,
        RENDER_OBJECT_FLAGS_OFFSET + sizeof(uint32_t)) ?
        render_object : NULL;
}

static void *presentation_body_render_object(void *model_interface) {
    uint8_t *model = (uint8_t *)model_interface;
    uint8_t *owner;
    uint8_t *position;
    void *wrapper;

    if (!readable_zone_bytes((const char *)model, 0x14u)) {
        return NULL;
    }
    owner = *(uint8_t **)(model + 0x10u);
    if (!readable_zone_bytes((const char *)owner,
            PRESENTATION_BODY_POSITION_OFFSET + sizeof(position))) {
        return NULL;
    }
    position = *(uint8_t **)(owner + PRESENTATION_BODY_POSITION_OFFSET);
    if (!readable_zone_bytes((const char *)position,
            POSITION_RENDER_WRAPPER_OFFSET + sizeof(wrapper))) {
        return NULL;
    }
    wrapper = *(void **)(position + POSITION_RENDER_WRAPPER_OFFSET);
    return presentation_render_object_from_wrapper(wrapper);
}

static BOOL render_object_hidden(void *render_object, BOOL *hidden) {
    if (hidden == NULL || !readable_zone_bytes(
            (const char *)render_object,
            RENDER_OBJECT_FLAGS_OFFSET + sizeof(uint32_t))) {
        return FALSE;
    }
    *hidden = (*(uint32_t *)((uint8_t *)render_object +
        RENDER_OBJECT_FLAGS_OFFSET) & RENDER_OBJECT_HIDDEN_FLAG) != 0u;
    return TRUE;
}

static BOOL capture_party_presentation_snapshot(
    SudekiMpPartyPresentationLease *destination
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;
    int party_count;
    int index;
    SudekiMpPartyPresentationLease snapshot;

    ZeroMemory(&snapshot, sizeof(snapshot));
    if (destination == NULL || base == NULL || show_party_members == NULL ||
        hide_party_members == NULL ||
        !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group))) {
        return FALSE;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_zone_bytes((const char *)group,
            PARTY_COUNT_OFFSET + sizeof(party_count))) {
        return FALSE;
    }
    party_count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (party_count < 2 || party_count > (int)PARTY_SLOT_COUNT) {
        return FALSE;
    }
    snapshot.group = group;
    snapshot.lead_character = *(void **)(
        group + PARTY_SLOT_ZERO_OFFSET);
    if (!readable_zone_bytes(
            (const char *)snapshot.lead_character, 1u)) {
        return FALSE;
    }
    snapshot.party_count = party_count;
    for (index = 1; index < (int)PARTY_SLOT_COUNT; ++index) {
        SudekiMpPartyPresentationFollower *follower =
            &snapshot.followers[index - 1];
        uint8_t *character = *(uint8_t **)(
            group + PARTY_SLOT_ZERO_OFFSET +
                (size_t)index * PARTY_SLOT_STRIDE);
        uint8_t *model_interface;
        BOOL hidden;

        if (index >= party_count) {
            if (character != NULL) {
                return FALSE;
            }
            continue;
        }
        if (!readable_zone_bytes((const char *)character,
                CHARACTER_MODEL_INTERFACE_OFFSET +
                    sizeof(model_interface))) {
            return FALSE;
        }
        model_interface = *(uint8_t **)(
            character + CHARACTER_MODEL_INTERFACE_OFFSET);
        if (!readable_zone_bytes((const char *)model_interface,
                MODEL_HIDE_DEPTH_OFFSET + sizeof(int16_t))) {
            return FALSE;
        }
        follower->character = character;
        follower->model_interface = model_interface;
        follower->baseline_hide_depth = *(int16_t *)(
            model_interface + MODEL_HIDE_DEPTH_OFFSET);
        follower->body_render_object =
            presentation_body_render_object(model_interface);
        if (follower->baseline_hide_depth != 0 ||
            follower->body_render_object == NULL ||
            !render_object_hidden(
                follower->body_render_object, &hidden) || hidden) {
            return FALSE;
        }
    }
    snapshot.snapshot_valid = TRUE;
    snapshot.snapshot_tick = GetTickCount();
    snapshot.capture_thread_id = GetCurrentThreadId();
    *destination = snapshot;
    return TRUE;
}

static BOOL capture_party_presentation_lease(void) {
    if (party_presentation_lease.override_active ||
        party_presentation_lease.exit_balance_pending ||
        !capture_party_presentation_snapshot(
            &party_presentation_lease)) {
        return FALSE;
    }
    SudekiMpLogFormat(
        "party_transition event=presentation_snapshot status=confirmed "
        "group=%p party_count=%d followers=%d baseline_depth=0 "
        "policy=prove_native_hide_delta_before_balanced_show\r\n",
        party_presentation_lease.group,
        party_presentation_lease.party_count,
        party_presentation_lease.party_count - 1);
    return TRUE;
}

static BOOL party_presentation_snapshot_matches(
    const SudekiMpPartyPresentationLease *snapshot,
    int depth_delta,
    BOOL expect_hidden,
    BOOL require_render_flags
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;
    int index;

    if (snapshot == NULL || !snapshot->snapshot_valid || base == NULL ||
        !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group))) {
        return FALSE;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group != snapshot->group ||
        !readable_zone_bytes((const char *)group,
            PARTY_COUNT_OFFSET + sizeof(int)) ||
        *(int *)(group + PARTY_COUNT_OFFSET) !=
            snapshot->party_count ||
        *(void **)(group + PARTY_SLOT_ZERO_OFFSET) !=
            snapshot->lead_character) {
        return FALSE;
    }
    for (index = 1; index < (int)PARTY_SLOT_COUNT; ++index) {
        const SudekiMpPartyPresentationFollower *follower =
            &snapshot->followers[index - 1];
        uint8_t *character = *(uint8_t **)(
            group + PARTY_SLOT_ZERO_OFFSET +
                (size_t)index * PARTY_SLOT_STRIDE);
        uint8_t *model_interface;
        void *body_render_object;
        BOOL hidden;

        if (index >= snapshot->party_count) {
            if (character != NULL || follower->character != NULL) {
                return FALSE;
            }
            continue;
        }
        if (character != follower->character ||
            !readable_zone_bytes((const char *)character,
                CHARACTER_MODEL_INTERFACE_OFFSET +
                    sizeof(model_interface))) {
            return FALSE;
        }
        model_interface = *(uint8_t **)(
            character + CHARACTER_MODEL_INTERFACE_OFFSET);
        if (model_interface != follower->model_interface ||
            !readable_zone_bytes((const char *)model_interface,
                MODEL_HIDE_DEPTH_OFFSET + sizeof(int16_t)) ||
            *(int16_t *)(model_interface + MODEL_HIDE_DEPTH_OFFSET) !=
                follower->baseline_hide_depth + depth_delta) {
            return FALSE;
        }
        if (!require_render_flags) {
            continue;
        }
        body_render_object =
            presentation_body_render_object(model_interface);
        if (body_render_object != follower->body_render_object ||
            !render_object_hidden(body_render_object, &hidden) ||
            hidden != expect_hidden) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL party_presentation_matches(
    int depth_delta,
    BOOL expect_hidden,
    BOOL require_render_flags
) {
    return party_presentation_snapshot_matches(
        &party_presentation_lease,
        depth_delta,
        expect_hidden,
        require_render_flags);
}

static BOOL restore_party_presentation_after_enter(void) {
    if (party_atomic_presentation_attempted) {
        return party_atomic_presentation_confirmed;
    }
    party_atomic_presentation_attempted = TRUE;
    if (!party_presentation_matches(1, TRUE, TRUE)) {
        SudekiMpLogWrite(
            "party_transition event=presentation_restore status=rejected "
            "reason=native_hide_delta_or_identity_not_exact "
            "policy=never_decrement_unowned_visibility_counter\r\n");
        return FALSE;
    }
    show_party_members(party_presentation_lease.group);
    if (!party_presentation_matches(0, FALSE, TRUE)) {
        BOOL rollback_safe = party_presentation_matches(0, FALSE, FALSE);
        BOOL rolled_back = FALSE;

        if (rollback_safe) {
            hide_party_members(party_presentation_lease.group);
            rolled_back = party_presentation_matches(1, TRUE, TRUE);
        }
        SudekiMpLogFormat(
            "party_transition event=presentation_restore status=failed "
            "rollback=%s policy=remain_quarantined\r\n",
            rolled_back ? "confirmed" : "unconfirmed");
        return FALSE;
    }
    party_presentation_lease.override_active = TRUE;
    party_atomic_presentation_confirmed = TRUE;
    SudekiMpLogWrite(
        "party_transition event=presentation_restore status=confirmed "
        "hide_depth=1_to_0 body=visible equipment=native_type_gated "
        "policy=owned_temporary_room_visibility_override\r\n");
    return TRUE;
}

static BOOL prepare_party_presentation_for_exit(void) {
    if (!party_presentation_lease.override_active) {
        return TRUE;
    }
    if (party_presentation_lease.exit_balance_pending) {
        return party_presentation_matches(1, TRUE, TRUE);
    }
    if (!party_presentation_matches(0, FALSE, TRUE)) {
        SudekiMpLogWrite(
            "party_transition event=presentation_exit_balance "
            "status=rejected reason=owned_visibility_identity_changed "
            "policy=do_not_increment_unknown_counter\r\n");
        return FALSE;
    }
    hide_party_members(party_presentation_lease.group);
    if (!party_presentation_matches(1, TRUE, TRUE)) {
        SudekiMpLogWrite(
            "party_transition event=presentation_exit_balance "
            "status=failed policy=remain_quarantined\r\n");
        return FALSE;
    }
    party_presentation_lease.exit_balance_pending = TRUE;
    SudekiMpLogWrite(
        "party_transition event=presentation_exit_balance "
        "status=confirmed hide_depth=0_to_1 "
        "policy=native_exit_or_settled_service_consumes_exact_owned_lease\r\n");
    return TRUE;
}

unsigned int SudekiMpZoneTransitionExitPresentationAction(
    BOOL override_active,
    BOOL exit_balance_pending,
    BOOL baseline_visible,
    BOOL owned_hide_present,
    BOOL show_already_attempted
) {
    if (!override_active ||
        (exit_balance_pending && baseline_visible)) {
        return SUDEKIMP_EXIT_PRESENTATION_READY;
    }
    if (exit_balance_pending && owned_hide_present &&
        !show_already_attempted) {
        return SUDEKIMP_EXIT_PRESENTATION_SHOW_OWNED;
    }
    return SUDEKIMP_EXIT_PRESENTATION_WAIT;
}

unsigned int SudekiMpZoneTransitionDoorwayStagingAction(
    BOOL identity_matches,
    BOOL destination_matches,
    BOOL player_two_unowned,
    BOOL player_two_input_neutral,
    BOOL follower_still_staged,
    BOOL timed_out,
    float inward_advance,
    float required_inward_advance
) {
    if (!identity_matches || !destination_matches ||
        !player_two_unowned || !player_two_input_neutral ||
        !follower_still_staged || timed_out ||
        !isfinite(inward_advance) ||
        !isfinite(required_inward_advance) ||
        required_inward_advance <= 0.0f) {
        return SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST;
    }
    return inward_advance >= required_inward_advance ?
        SUDEKIMP_DOORWAY_STAGING_REPOP_NOW :
        SUDEKIMP_DOORWAY_STAGING_WAIT;
}

static BOOL restore_party_presentation_after_exit(void) {
    unsigned int action = SudekiMpZoneTransitionExitPresentationAction(
        party_presentation_lease.override_active,
        party_presentation_lease.exit_balance_pending,
        party_presentation_matches(0, FALSE, TRUE),
        party_presentation_matches(1, TRUE, TRUE),
        party_atomic_presentation_attempted);

    if (action == SUDEKIMP_EXIT_PRESENTATION_READY) {
        if (party_presentation_lease.override_active) {
            SudekiMpLogWrite(
                "party_transition event=presentation_exit_balance "
                "status=consumed_by_native_exit hide_depth=1_to_0\r\n");
            clear_party_presentation_lease();
        }
        party_atomic_presentation_confirmed = TRUE;
        return TRUE;
    }
    if (action != SUDEKIMP_EXIT_PRESENTATION_SHOW_OWNED) {
        return FALSE;
    }

    party_atomic_presentation_attempted = TRUE;
    show_party_members(party_presentation_lease.group);
    if (!party_presentation_matches(0, FALSE, TRUE)) {
        BOOL rollback_safe = party_presentation_matches(0, FALSE, FALSE);
        BOOL rolled_back = FALSE;

        if (rollback_safe) {
            hide_party_members(party_presentation_lease.group);
            rolled_back = party_presentation_matches(1, TRUE, TRUE);
        }
        SudekiMpLogFormat(
            "party_transition event=presentation_exit_balance "
            "status=failed source=owned_hide rollback=%s "
            "policy=never_retry_show_remain_quarantined\r\n",
            rolled_back ? "confirmed" : "unconfirmed");
        return FALSE;
    }
    clear_party_presentation_lease();
    party_atomic_presentation_confirmed = TRUE;
    SudekiMpLogWrite(
        "party_transition event=presentation_exit_balance "
        "status=consumed_by_sudekimp hide_depth=1_to_0 "
        "policy=exact_owned_hide_show_once_after_exterior_settle\r\n");
    return TRUE;
}

static BOOL main_world_ready_for_presentation_hide(void) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *world;
    uint8_t *descriptor;

    if (base == NULL || !readable_zone_bytes(
            (const char *)(base + RVA_WORLD_GLOBAL), sizeof(world))) {
        return FALSE;
    }
    world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
    if (!readable_zone_bytes((const char *)world, 0x39bu)) {
        return FALSE;
    }
    descriptor = *(uint8_t **)(world + 0x0cu);
    return descriptor != NULL &&
        readable_zone_bytes((const char *)descriptor, 0x38u) &&
        *(uint32_t *)(descriptor + 0x34u) == 3u &&
        *(void **)(world + 0x14u) == NULL &&
        *(uint8_t *)(world + 0x399u) != 0u &&
        *(uint8_t *)(world + 0x39au) != 0u;
}

static void __attribute__((thiscall)) trace_hide_party_members(void *group) {
    SudekiMpPartyPresentationLease candidate;
    LONG depth;
    BOOL captured = FALSE;
    unsigned int sequence = ++party_presentation_hide_sequence;
    void *caller = __builtin_return_address(0);

    ZeroMemory(&candidate, sizeof(candidate));
    depth = InterlockedIncrement(&party_presentation_hide_depth);
    if (depth == 1) {
        clear_party_presentation_hide_candidate();
    }
    if (depth == 1 && party_atomic_transitions_enabled &&
        party_atomic_transition_kind == PARTY_ATOMIC_NONE &&
        !party_presentation_lease.snapshot_valid &&
        !party_presentation_lease.override_active &&
        !party_presentation_lease.exit_balance_pending &&
        SudekiMpSplitScreenRolesLocked() &&
        SudekiMpSplitScreenRosterParticipationRequested() &&
        main_world_ready_for_presentation_hide()) {
        captured = capture_party_presentation_snapshot(&candidate) &&
            candidate.group == group;
    }
    hide_party_members(group);
    if (captured) {
        if (party_presentation_snapshot_matches(
                &candidate, 1, TRUE, TRUE)) {
            candidate.snapshot_tick = GetTickCount();
            candidate.capture_thread_id = GetCurrentThreadId();
            party_presentation_hide_candidate = candidate;
            SudekiMpLogFormat(
                "party_transition event=presentation_pre_hide "
                "status=confirmed sequence=%u group=%p caller=%p "
                "thread=%lu candidate_timeout_ms=%u "
                "policy=consume_only_if_followed_by_matching_temp_entry\r\n",
                sequence,
                group,
                caller,
                (unsigned long)candidate.capture_thread_id,
                PARTY_PRESENTATION_CANDIDATE_TIMEOUT_MS);
        } else {
            SudekiMpLogWrite(
                "party_transition event=presentation_pre_hide "
                "status=rejected reason=native_hide_postcondition\r\n");
        }
    }
    InterlockedDecrement(&party_presentation_hide_depth);
}

static void finish_party_presentation_exit_balance(void) {
    if (!party_presentation_lease.override_active ||
        !party_presentation_lease.exit_balance_pending) {
        return;
    }
    if (!party_presentation_matches(0, FALSE, TRUE)) {
        SudekiMpLogWrite(
            "party_transition event=presentation_exit_balance "
            "status=unconfirmed_after_native_exit "
            "policy=retain_ownership_and_quarantine\r\n");
        return;
    }
    SudekiMpLogWrite(
        "party_transition event=presentation_exit_balance "
        "status=consumed_by_native_exit hide_depth=1_to_0\r\n");
    clear_party_presentation_lease();
}

static void *current_party_lead_position(void) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;
    uint8_t *lead;

    if (base == NULL || !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group))) {
        return NULL;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_zone_bytes((const char *)group,
            PARTY_SLOT_ZERO_OFFSET + sizeof(lead))) {
        return NULL;
    }
    lead = *(uint8_t **)(group + PARTY_SLOT_ZERO_OFFSET);
    if (!readable_zone_bytes((const char *)lead,
            CHARACTER_POSITION_OFFSET + sizeof(void *))) {
        return NULL;
    }
    return *(void **)(lead + CHARACTER_POSITION_OFFSET);
}

static BOOL formation_matches_current_party(void) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;
    uint8_t *ai_manager;
    uint8_t *formation;
    int party_count;
    int formation_count;
    int index;

    if (base == NULL || !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group)) || !readable_zone_bytes(
            (const char *)(base + RVA_AI_MANAGER_GLOBAL),
            sizeof(ai_manager))) {
        return FALSE;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    ai_manager = *(uint8_t **)(base + RVA_AI_MANAGER_GLOBAL);
    if (!readable_zone_bytes((const char *)group,
            0xd0u + sizeof(uint32_t)) ||
        !readable_zone_bytes((const char *)ai_manager, 0x128u)) {
        return FALSE;
    }
    party_count = *(int *)(group + PARTY_COUNT_OFFSET);
    formation_count = *(int *)(ai_manager + 0x124u);
    formation = ai_manager + 0xf4u;
    /* CGroupPlayers+0xD0 is the native SetModeLeadOnly nesting count.
     * Temporary-zone placement deliberately runs while that count is
     * nonzero: Sudeki places the lead first, pops the formation members,
     * and only then restores full-party mode. It is observable transition
     * state, not a reason to reject the native formation pop. */
    if (party_count < 1 || party_count > (int)PARTY_SLOT_COUNT ||
        formation_count != party_count ||
        !readable_zone_bytes((const char *)group,
            PARTY_SLOT_ZERO_OFFSET +
                (size_t)party_count * PARTY_SLOT_STRIDE) ||
        !readable_zone_bytes((const char *)formation,
            (size_t)formation_count * PARTY_SLOT_STRIDE) ||
        *(void **)formation != *(void **)(group + PARTY_SLOT_ZERO_OFFSET)) {
        return FALSE;
    }
    for (index = 0; index < formation_count; ++index) {
        uint8_t *actor = *(uint8_t **)(
            formation + (size_t)index * PARTY_SLOT_STRIDE);
        uint8_t *position;
        uint8_t *location;
        uint8_t *component;
        BOOL found = FALSE;
        int other;

        if (!readable_zone_bytes((const char *)actor, 0x9cu)) {
            return FALSE;
        }
        position = *(uint8_t **)(actor + 0x44u);
        location = *(uint8_t **)(actor + 0x74u);
        component = *(uint8_t **)(actor + 0x94u);
        if (!readable_zone_bytes((const char *)position, 0x24u) ||
            !readable_zone_bytes((const char *)location, 0x44u) ||
            !readable_zone_bytes((const char *)component, 0x44u) ||
            *(void **)(actor + 0x98u) == NULL) {
            return FALSE;
        }
        for (other = 0; other < index; ++other) {
            if (*(void **)(formation +
                    (size_t)other * PARTY_SLOT_STRIDE) == actor) {
                return FALSE;
            }
        }
        for (other = 0; other < party_count; ++other) {
            if (*(void **)(group + PARTY_SLOT_ZERO_OFFSET +
                    (size_t)other * PARTY_SLOT_STRIDE) == actor) {
                found = TRUE;
                break;
            }
        }
        if (!found) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL declared_followers_have_disable_ref(
    uint8_t *group,
    uint8_t expected
) {
    int count;
    int index;

    if (!readable_zone_bytes((const char *)group,
            PARTY_COUNT_OFFSET + sizeof(count))) {
        return FALSE;
    }
    count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (count < 1 || count > (int)PARTY_SLOT_COUNT) {
        return FALSE;
    }
    for (index = 1; index < (int)PARTY_SLOT_COUNT; ++index) {
        uint8_t *actor = *(uint8_t **)(
            group + PARTY_SLOT_ZERO_OFFSET +
                (size_t)index * PARTY_SLOT_STRIDE);

        if (index >= count) {
            if (actor != NULL) {
                SudekiMpLogFormat(
                    "party_transition event=follower_disable_check "
                    "status=rejected slot=%d actor=%p reason=slot_beyond_count\r\n",
                    index, actor);
                return FALSE;
            }
            continue;
        }
        if (!readable_zone_bytes((const char *)actor, 0x2cu)) {
            SudekiMpLogFormat(
                "party_transition event=follower_disable_check "
                "status=rejected slot=%d actor=%p reason=unreadable\r\n",
                index, actor);
            return FALSE;
        }
        if (*(uint8_t *)(actor + 0x2bu) != expected) {
            SudekiMpLogFormat(
                "party_transition event=follower_disable_check "
                "status=deferred slot=%d actor=%p expected=%u actual=%u\r\n",
                index,
                actor,
                (unsigned int)expected,
                (unsigned int)*(uint8_t *)(actor + 0x2bu));
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL release_native_lead_only_mode(
    unsigned int *depth_before,
    BOOL *released
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;
    uint32_t depth;

    if (depth_before != NULL) {
        *depth_before = 0xffffffffu;
    }
    if (released != NULL) {
        *released = FALSE;
    }
    if (base == NULL || set_mode_full_party == NULL ||
        !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group))) {
        return FALSE;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_zone_bytes((const char *)group,
            0xd0u + sizeof(depth))) {
        return FALSE;
    }
    depth = *(uint32_t *)(group + 0xd0u);
    if (depth_before != NULL) {
        *depth_before = depth;
    }
    if (depth == 0u) {
        return TRUE;
    }
    if (depth != 1u) {
        SudekiMpLogFormat(
            "party_transition event=lead_only_release status=rejected "
            "depth=%lu reason=nested_native_owner\r\n",
            (unsigned long)depth);
        return FALSE;
    }
    if (!declared_followers_have_disable_ref(group, 1u)) {
        SudekiMpLogWrite(
            "party_transition event=lead_only_release status=rejected "
            "depth=1 reason=follower_disable_ref_not_exactly_one\r\n");
        return FALSE;
    }
    set_mode_full_party(group);
    if (*(uint32_t *)(group + 0xd0u) != 0u ||
        !declared_followers_have_disable_ref(group, 0u)) {
        SudekiMpLogFormat(
            "party_transition event=lead_only_release status=failed "
            "depth_before=1 depth_after=%lu reason=postcondition\r\n",
            (unsigned long)*(uint32_t *)(group + 0xd0u));
        return FALSE;
    }
    if (released != NULL) {
        *released = TRUE;
    }
    SudekiMpLogWrite(
        "party_transition event=lead_only_release status=confirmed "
        "depth_before=1 depth_after=0 policy=native_full_party_once\r\n");
    return TRUE;
}

static BOOL restore_native_lead_only_mode(void) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;

    if (base == NULL || set_mode_lead_only == NULL ||
        !formation_matches_current_party() ||
        !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group))) {
        return FALSE;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_zone_bytes((const char *)group,
            0xd0u + sizeof(uint32_t)) ||
        *(uint32_t *)(group + 0xd0u) != 0u ||
        !declared_followers_have_disable_ref(group, 0u)) {
        return FALSE;
    }
    set_mode_lead_only(group);
    if (*(uint32_t *)(group + 0xd0u) != 1u ||
        !declared_followers_have_disable_ref(group, 1u) ||
        !formation_matches_current_party()) {
        SudekiMpLogFormat(
            "party_transition event=lead_only_rollback status=failed "
            "depth_after=%lu\r\n",
            (unsigned long)*(uint32_t *)(group + 0xd0u));
        return FALSE;
    }
    SudekiMpLogWrite(
        "party_transition event=lead_only_rollback status=confirmed "
        "depth_before=0 depth_after=1 policy=restore_vanilla_quarantine\r\n");
    return TRUE;
}

static BOOL capture_party_placement_snapshot(
    SudekiMpPartyPlacementSnapshot *destination
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *group;
    SudekiMpPartyPlacementSnapshot snapshot;
    int count;
    int index;

    if (destination == NULL) {
        return FALSE;
    }
    ZeroMemory(&snapshot, sizeof(snapshot));
    if (base == NULL || !readable_zone_bytes(
            (const char *)(base + RVA_ACTIVE_GROUP_GLOBAL),
            sizeof(group))) {
        return FALSE;
    }
    group = *(uint8_t **)(base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_zone_bytes((const char *)group,
            PARTY_COUNT_OFFSET + sizeof(count))) {
        return FALSE;
    }
    count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (count < 1 || count > (int)PARTY_SLOT_COUNT ||
        !readable_zone_bytes((const char *)group,
            PARTY_SLOT_ZERO_OFFSET +
                (size_t)count * PARTY_SLOT_STRIDE)) {
        return FALSE;
    }
    snapshot.group = group;
    snapshot.party_count = count;
    for (index = 0; index < count; ++index) {
        uint8_t *character = *(uint8_t **)(
            group + PARTY_SLOT_ZERO_OFFSET +
                (size_t)index * PARTY_SLOT_STRIDE);
        uint8_t *position;

        if (!readable_zone_bytes((const char *)character,
                CHARACTER_POSITION_OFFSET + sizeof(position))) {
            return FALSE;
        }
        position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
        if (!readable_zone_bytes((const char *)position,
                POSITION_XYZ_OFFSET + sizeof(float) * 3u)) {
            return FALSE;
        }
        snapshot.actors[index] = character;
        memcpy(snapshot.positions[index],
            position + POSITION_XYZ_OFFSET,
            sizeof(snapshot.positions[index]));
        if (!isfinite(snapshot.positions[index][0]) ||
            !isfinite(snapshot.positions[index][1]) ||
            !isfinite(snapshot.positions[index][2])) {
            return FALSE;
        }
    }
    *destination = snapshot;
    return TRUE;
}

static BOOL party_placement_identity_matches(
    const SudekiMpPartyPlacementSnapshot *expected,
    const SudekiMpPartyPlacementSnapshot *actual
) {
    int index;

    if (expected == NULL || actual == NULL ||
        expected->group != actual->group ||
        expected->party_count != actual->party_count ||
        expected->party_count < 1 ||
        expected->party_count > (int)PARTY_SLOT_COUNT) {
        return FALSE;
    }
    for (index = 0; index < expected->party_count; ++index) {
        if (expected->actors[index] != actual->actors[index]) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL party_placement_within_lead_radius(
    const SudekiMpPartyPlacementSnapshot *snapshot,
    float maximum_distance
) {
    float maximum_squared;
    int index;

    if (snapshot == NULL || snapshot->party_count < 1 ||
        snapshot->party_count > (int)PARTY_SLOT_COUNT ||
        !isfinite(maximum_distance) || maximum_distance <= 0.0f) {
        return FALSE;
    }
    maximum_squared = maximum_distance * maximum_distance;
    for (index = 1; index < snapshot->party_count; ++index) {
        float dx = snapshot->positions[index][0] -
            snapshot->positions[0][0];
        float dy = snapshot->positions[index][1] -
            snapshot->positions[0][1];
        float dz = snapshot->positions[index][2] -
            snapshot->positions[0][2];

        if (!isfinite(dx) || !isfinite(dy) || !isfinite(dz) ||
            dx * dx + dy * dy + dz * dz > maximum_squared) {
            return FALSE;
        }
    }
    return TRUE;
}

static void log_party_placement_snapshot(
    const char *phase,
    const SudekiMpPartyPlacementSnapshot *snapshot
) {
    int index;

    if (snapshot == NULL) {
        return;
    }
    for (index = 0; index < snapshot->party_count; ++index) {
        float dx = snapshot->positions[index][0] -
            snapshot->positions[0][0];
        float dy = snapshot->positions[index][1] -
            snapshot->positions[0][1];
        float dz = snapshot->positions[index][2] -
            snapshot->positions[0][2];
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);

        SudekiMpLogFormat(
            "party_transition event=placement_snapshot phase=%s "
            "slot=%d actor=%p position_bits=%08lx,%08lx,%08lx "
            "lead_distance_bits=%08lx\r\n",
            phase == NULL ? "unspecified" : phase,
            index,
            snapshot->actors[index],
            (unsigned long)zone_float_bits(snapshot->positions[index][0]),
            (unsigned long)zone_float_bits(snapshot->positions[index][1]),
            (unsigned long)zone_float_bits(snapshot->positions[index][2]),
            (unsigned long)zone_float_bits(distance));
    }
}

static BOOL current_party_within_lead_radius(float maximum_distance) {
    SudekiMpPartyPlacementSnapshot snapshot;

    return capture_party_placement_snapshot(&snapshot) &&
        party_placement_within_lead_radius(&snapshot, maximum_distance);
}

static BOOL party_atomic_destination_ready(
    void *expected_world,
    void *expected_descriptor,
    unsigned int expected_generation,
    const char *expected_destination
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *world;
    uint8_t *descriptor;

    if (base == NULL || expected_world == NULL ||
        expected_descriptor == NULL || expected_destination == NULL ||
        expected_destination[0] == '\0' ||
        party_atomic_transition_kind != PARTY_ATOMIC_ENTER_TEMPORARY ||
        zone_source_generation != expected_generation ||
        strcmp(current_temporary_name, expected_destination) != 0 ||
        !readable_zone_bytes(
            (const char *)(base + RVA_WORLD_GLOBAL), sizeof(world))) {
        return FALSE;
    }
    world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
    if (world != expected_world ||
        !readable_zone_bytes((const char *)world, 0x39bu)) {
        return FALSE;
    }
    descriptor = *(uint8_t **)(world + 0x0cu);
    return descriptor == expected_descriptor &&
        readable_zone_bytes((const char *)descriptor, 0x38u) &&
        *(uint32_t *)(descriptor + 0x34u) == 4u &&
        *(void **)(world + 0x14u) == NULL &&
        *(uint8_t *)(world + 0x399u) != 0u &&
        *(uint8_t *)(world + 0x39au) != 0u;
}

static BOOL capture_party_atomic_destination(
    void **world_destination,
    void **descriptor_destination
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *world;
    uint8_t *descriptor;

    if (world_destination == NULL || descriptor_destination == NULL ||
        base == NULL || current_temporary_name[0] == '\0' ||
        !readable_zone_bytes(
            (const char *)(base + RVA_WORLD_GLOBAL), sizeof(world))) {
        return FALSE;
    }
    world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
    if (!readable_zone_bytes((const char *)world, 0x39bu)) {
        return FALSE;
    }
    descriptor = *(uint8_t **)(world + 0x0cu);
    if (descriptor == NULL ||
        !readable_zone_bytes((const char *)descriptor, 0x38u) ||
        *(uint32_t *)(descriptor + 0x34u) != 4u ||
        *(void **)(world + 0x14u) != NULL ||
        *(uint8_t *)(world + 0x399u) == 0u ||
        *(uint8_t *)(world + 0x39au) == 0u) {
        return FALSE;
    }
    *world_destination = world;
    *descriptor_destination = descriptor;
    return TRUE;
}

static BOOL player_two_fully_unowned(void) {
    return !SudekiMpControlSeparationPlayerTwoRequested() &&
        !SudekiMpControlSeparationPlayerTwoActive() &&
        SudekiMpControlSeparationPlayerTwoCharacter() == NULL &&
        !SudekiMpSplitScreenRuntimeEnabled();
}

static BOOL formation_direction_matches_doorway_staging(
    const SudekiMpDoorwayStaging *staging
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *ai_manager;
    uint8_t *formation;
    float direction_x;
    float direction_z;
    float magnitude;
    float alignment;

    if (staging == NULL || base == NULL ||
        !readable_zone_bytes(
            (const char *)(base + RVA_AI_MANAGER_GLOBAL),
            sizeof(ai_manager))) {
        return FALSE;
    }
    ai_manager = *(uint8_t **)(base + RVA_AI_MANAGER_GLOBAL);
    if (!readable_zone_bytes((const char *)ai_manager, 0x138u)) {
        return FALSE;
    }
    formation = ai_manager + 0xf4u;
    direction_x = *(float *)(formation + 0x3cu);
    direction_z = *(float *)(formation + 0x40u);
    magnitude = sqrtf(direction_x * direction_x +
        direction_z * direction_z);
    if (!isfinite(magnitude) || magnitude <= 0.0001f) {
        return FALSE;
    }
    alignment = direction_x / magnitude * staging->inward_x +
        direction_z / magnitude * staging->inward_z;
    return isfinite(alignment) &&
        alignment >= doorway_staging_direction_alignment;
}

static BOOL doorway_staging_follower_unchanged(
    const SudekiMpDoorwayStaging *staging,
    const SudekiMpPartyPlacementSnapshot *current
) {
    float dx;
    float dy;
    float dz;

    if (staging == NULL || current == NULL ||
        current->party_count != 2) {
        return FALSE;
    }
    dx = current->positions[1][0] - staging->placement.positions[1][0];
    dy = current->positions[1][1] - staging->placement.positions[1][1];
    dz = current->positions[1][2] - staging->placement.positions[1][2];
    return isfinite(dx) && isfinite(dy) && isfinite(dz) &&
        dx * dx + dy * dy + dz * dz <=
            doorway_staging_follower_tolerance_squared;
}

static void arm_party_doorway_staging(
    const SudekiMpPartyPlacementSnapshot *placement
) {
    SudekiMpInputBridgeState raw_input;
    void *world;
    void *descriptor;
    float dx;
    float dz;
    float extent;
    DWORD now;
    const char *skip_reason = NULL;

    ZeroMemory(&party_doorway_staging, sizeof(party_doorway_staging));
    ZeroMemory(&raw_input, sizeof(raw_input));
    if (placement == NULL || placement->party_count != 2) {
        skip_reason = "not_exactly_two_party_members";
    } else if (!player_two_fully_unowned()) {
        skip_reason = "player_two_still_owned";
    } else if (!SudekiMpInputBridgePollRaw(&raw_input)) {
        skip_reason = "player_two_input_unavailable";
    } else if (!transition_vote_raw_input_neutral(&raw_input)) {
        skip_reason = "player_two_input_observed";
    } else if (!capture_party_atomic_destination(&world, &descriptor)) {
        skip_reason = "destination_not_exactly_ready";
    }
    if (skip_reason != NULL) {
        SudekiMpLogFormat(
            "party_transition event=doorway_staging status=skipped "
            "reason=%s policy=accept_first_native_formation_placement\r\n",
            skip_reason);
        return;
    }
    dx = placement->positions[0][0] - placement->positions[1][0];
    dz = placement->positions[0][2] - placement->positions[1][2];
    extent = sqrtf(dx * dx + dz * dz);
    if (!isfinite(extent) || extent < doorway_staging_minimum_extent ||
        extent > doorway_staging_maximum_extent) {
        SudekiMpLogFormat(
            "party_transition event=doorway_staging status=skipped "
            "reason=unproved_native_formation_extent extent_bits=%08lx "
            "policy=accept_first_native_formation_placement\r\n",
            (unsigned long)zone_float_bits(extent));
        return;
    }
    now = GetTickCount();
    party_doorway_staging.armed = TRUE;
    party_doorway_staging.deadline = now +
        PARTY_DOORWAY_STAGING_TIMEOUT_MS;
    party_doorway_staging.transition_serial =
        party_atomic_transition_serial;
    party_doorway_staging.world = world;
    party_doorway_staging.descriptor = descriptor;
    party_doorway_staging.source_generation = zone_source_generation;
    party_doorway_staging.last_player_two_sequence = raw_input.sequence;
    party_doorway_staging.inward_x = dx / extent;
    party_doorway_staging.inward_z = dz / extent;
    party_doorway_staging.required_inward_advance = extent +
        doorway_staging_advance_margin;
    memcpy(party_doorway_staging.destination, current_temporary_name,
        sizeof(party_doorway_staging.destination));
    party_doorway_staging.placement = *placement;
    SudekiMpLogFormat(
        "party_transition event=doorway_staging status=armed serial=%u "
        "group=%p lead=%p follower=%p destination=%s descriptor=%p "
        "extent_bits=%08lx required_advance_bits=%08lx "
        "input_sequence=%lu timeout_ms=%u "
        "policy=one_delayed_native_formation_pop_after_lead_moves_inward\r\n",
        party_atomic_transition_serial,
        placement->group,
        placement->actors[0],
        placement->actors[1],
        party_doorway_staging.destination,
        descriptor,
        (unsigned long)zone_float_bits(extent),
        (unsigned long)zone_float_bits(
            party_doorway_staging.required_inward_advance),
        (unsigned long)raw_input.sequence,
        PARTY_DOORWAY_STAGING_TIMEOUT_MS);
}

static void clear_party_atomic_transition(void) {
    party_atomic_transition_kind = PARTY_ATOMIC_NONE;
    party_atomic_transition_deadline = 0u;
    party_atomic_release_deadline = 0u;
    party_atomic_position_scope = FALSE;
    party_atomic_lead_setter_seen = FALSE;
    party_atomic_expected_lead_position = NULL;
    party_atomic_placement_confirmed = FALSE;
    party_atomic_presentation_attempted = FALSE;
    party_atomic_presentation_confirmed = FALSE;
    party_atomic_settle_descriptor = NULL;
    party_atomic_settle_since = 0u;
    ZeroMemory(&party_doorway_staging, sizeof(party_doorway_staging));
}

static BOOL begin_party_atomic_transition(
    unsigned int kind,
    const char *zone_name
) {
    SudekiMpPartyPresentationLease pre_hide_candidate;
    BOOL had_pre_hide_candidate = FALSE;

    ZeroMemory(&pre_hide_candidate, sizeof(pre_hide_candidate));
    if (!party_atomic_transitions_enabled ||
        !SudekiMpSplitScreenRosterParticipationAvailable()) {
        return FALSE;
    }
    if (party_atomic_transition_kind != PARTY_ATOMIC_NONE) {
        SudekiMpLogFormat(
            "party_transition event=begin status=coalesced serial=%u "
            "active_kind=%u requested_kind=%u zone=%s\r\n",
            party_atomic_transition_serial,
            party_atomic_transition_kind,
            kind,
            zone_name == NULL ? "(exit)" : zone_name);
        return FALSE;
    }
    if (!SudekiMpSplitScreenRolesLocked()) {
        SudekiMpLogFormat(
            "party_transition event=begin status=ignored kind=%u zone=%s "
            "reason=roster_runtime_inactive "
            "policy=preserve_pending_participation\r\n",
            kind,
            zone_name == NULL ? "(exit)" : zone_name);
        return FALSE;
    }
    if (!SudekiMpSplitScreenBeginPartyTransition()) {
        SudekiMpLogFormat(
            "party_transition event=begin status=rejected kind=%u zone=%s "
            "error=%lu policy=native_transition_unchanged\r\n",
            kind,
            zone_name == NULL ? "(exit)" : zone_name,
            (unsigned long)GetLastError());
        return FALSE;
    }
    ZeroMemory(&party_doorway_staging, sizeof(party_doorway_staging));
    if (kind == PARTY_ATOMIC_ENTER_TEMPORARY) {
        DWORD now = GetTickCount();
        pre_hide_candidate = party_presentation_hide_candidate;
        had_pre_hide_candidate = pre_hide_candidate.snapshot_valid;
        clear_party_presentation_hide_candidate();
        BOOL fresh_pre_hide_candidate =
            had_pre_hide_candidate &&
            pre_hide_candidate.snapshot_tick != 0u &&
            pre_hide_candidate.capture_thread_id == GetCurrentThreadId() &&
            (DWORD)(now - pre_hide_candidate.snapshot_tick) <=
                PARTY_PRESENTATION_CANDIDATE_TIMEOUT_MS &&
            party_presentation_snapshot_matches(
                &pre_hide_candidate, 1, TRUE, TRUE);

        if (party_presentation_lease.override_active ||
            party_presentation_lease.exit_balance_pending) {
            SudekiMpLogWrite(
                "party_transition event=presentation_snapshot "
                "status=rejected reason=prior_visibility_lease_active "
                "policy=preserve_prior_ownership_and_quarantine\r\n");
        } else if (fresh_pre_hide_candidate) {
            party_presentation_lease = pre_hide_candidate;
            SudekiMpLogFormat(
                "party_transition event=presentation_snapshot "
                "status=consumed source=pre_hide age_ms=%lu thread=%lu "
                "policy=exact_native_hide_lease\r\n",
                (unsigned long)(now -
                    pre_hide_candidate.snapshot_tick),
                (unsigned long)pre_hide_candidate.capture_thread_id);
        } else {
            clear_party_presentation_lease();
            if (!capture_party_presentation_lease()) {
                SudekiMpLogWrite(
                    "party_transition event=presentation_snapshot "
                    "status=rejected reason=no_fresh_pre_hide_candidate "
                    "policy=transition_remains_quarantined\r\n");
            } else {
                SudekiMpLogWrite(
                    "party_transition event=presentation_snapshot "
                    "status=confirmed source=enter_fallback "
                    "policy=valid_only_if_native_hide_follows\r\n");
            }
        }
    }
    ++party_atomic_transition_serial;
    party_atomic_transition_kind = kind;
    party_atomic_transition_deadline = GetTickCount() +
        PARTY_ATOMIC_TIMEOUT_MS;
    party_atomic_release_deadline = 0u;
    party_atomic_position_scope = FALSE;
    party_atomic_lead_setter_seen = FALSE;
    party_atomic_expected_lead_position = NULL;
    party_atomic_placement_confirmed = FALSE;
    party_atomic_presentation_attempted = FALSE;
    party_atomic_presentation_confirmed = FALSE;
    party_atomic_settle_descriptor = NULL;
    party_atomic_settle_since = 0u;
    SudekiMpLogFormat(
        "party_transition event=begin status=armed serial=%u kind=%u "
        "zone=%s policy=native_lead_then_formation_pop\r\n",
        party_atomic_transition_serial,
        kind,
        zone_name == NULL ? "(exit)" : zone_name);
    return TRUE;
}

static void finish_party_atomic_transition(
    unsigned int expected_kind,
    BOOL lead_placement_confirmed
) {
    SudekiMpPartyPlacementSnapshot placement;
    BOOL native_formation_ok;
    BOOL placed;
    BOOL formation_ready;
    BOOL placement_snapshot_valid;
    BOOL presentation_ready;
    BOOL lead_only_released = FALSE;
    unsigned int lead_only_depth = 0xffffffffu;

    if (!party_atomic_transitions_enabled ||
        party_atomic_transition_kind != expected_kind) {
        return;
    }
    ZeroMemory(&placement, sizeof(placement));
    if (expected_kind == PARTY_ATOMIC_ENTER_TEMPORARY &&
        party_atomic_presentation_attempted &&
        !party_atomic_presentation_confirmed) {
        return;
    }
    formation_ready = lead_placement_confirmed &&
        formation_matches_current_party();
    if (formation_ready && expected_kind == PARTY_ATOMIC_ENTER_TEMPORARY) {
        formation_ready = release_native_lead_only_mode(
                &lead_only_depth, &lead_only_released) &&
            formation_matches_current_party();
    }
    if (formation_ready && formation_pop_members != NULL) {
        formation_pop_members();
    }
    placement_snapshot_valid = formation_ready &&
        formation_matches_current_party() &&
        capture_party_placement_snapshot(&placement);
    native_formation_ok = placement_snapshot_valid &&
        party_placement_within_lead_radius(
            &placement, party_placement_maximum_distance);
    if (placement_snapshot_valid) {
        log_party_placement_snapshot("first_native_pop", &placement);
    }
    presentation_ready = expected_kind != PARTY_ATOMIC_ENTER_TEMPORARY ||
        (native_formation_ok && restore_party_presentation_after_enter());
    if ((!native_formation_ok || !presentation_ready) &&
        lead_only_released &&
        !restore_native_lead_only_mode()) {
        SudekiMpLogWrite(
            "party_transition event=lead_only_rollback status=unconfirmed "
            "policy=remain_quarantined_until_restart_or_safe_exit\r\n");
    }
    placed = native_formation_ok && presentation_ready;
    SudekiMpLogFormat(
        "party_transition event=placement serial=%u kind=%u status=%s "
        "native_formation=%s presentation=%s lead_only_depth_before=%u "
        "maximum_lead_distance_bits=%08lx "
        "transform_only_fallback=disabled "
        "policy=all_declared_party_slots_follow_native_lead\r\n",
        party_atomic_transition_serial,
        expected_kind,
        placed ? "confirmed" : "failed",
        native_formation_ok ? "confirmed" : "unconfirmed",
        presentation_ready ? "confirmed" : "unconfirmed",
        lead_only_depth,
        (unsigned long)zone_float_bits(
            party_placement_maximum_distance));
    party_atomic_placement_confirmed = placed;
    party_atomic_settle_descriptor = NULL;
    party_atomic_settle_since = 0u;
    if (placed && expected_kind == PARTY_ATOMIC_ENTER_TEMPORARY) {
        arm_party_doorway_staging(&placement);
    }
}

static void copy_zone_name(
    const char *source,
    char destination[64]
) {
    size_t index;

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }
    for (index = 0u; index + 1u < 64u; ++index) {
        if (!readable_zone_bytes(source + index, 1u)) {
            break;
        }
        destination[index] = source[index];
        if (destination[index] == '\0') {
            return;
        }
    }
    destination[index < 63u ? index : 63u] = '\0';
}

static BOOL copy_exact_zone_name(
    const char *source,
    char destination[64]
) {
    size_t index;

    destination[0] = '\0';
    if (source == NULL) {
        return FALSE;
    }
    for (index = 0u; index < 64u; ++index) {
        if (!readable_zone_bytes(source + index, 1u)) {
            destination[0] = '\0';
            return FALSE;
        }
        destination[index] = source[index];
        if (destination[index] == '\0') {
            return index != 0u;
        }
    }
    destination[0] = '\0';
    return FALSE;
}

static BOOL finite_zone_position(const float *position) {
    return position != NULL &&
        isfinite(position[0]) && isfinite(position[1]) &&
        isfinite(position[2]) && fabsf(position[0]) < 1000000.0f &&
        fabsf(position[1]) < 1000000.0f &&
        fabsf(position[2]) < 1000000.0f;
}

static BOOL nonzero_zone_position(const float *position) {
    return finite_zone_position(position) &&
        (fabsf(position[0]) > 0.0001f ||
         fabsf(position[1]) > 0.0001f ||
         fabsf(position[2]) > 0.0001f);
}

static SudekiMpZoneArrivalContext *find_arrival_context(
    BOOL temporary,
    const char *world_name,
    const char *destination_name,
    BOOL create
) {
    unsigned int index;
    SudekiMpZoneArrivalContext *free_context = NULL;

    if (world_name == NULL || destination_name == NULL ||
        world_name[0] == '\0' || destination_name[0] == '\0') {
        return NULL;
    }
    for (index = 0u; index < ZONE_ARRIVAL_CONTEXT_CAPACITY; ++index) {
        SudekiMpZoneArrivalContext *context = &arrival_contexts[index];
        if (!context->valid) {
            if (free_context == NULL) {
                free_context = context;
            }
            continue;
        }
        if (context->temporary == temporary &&
            _stricmp(context->world_name, world_name) == 0 &&
            _stricmp(context->destination_name, destination_name) == 0) {
            return context;
        }
    }
    if (!create && free_context == NULL) {
        return NULL;
    }
    if (free_context == NULL) {
        /* Replace the oldest/last slot only when the cache is full.  The
         * context is a research cache, never a gameplay save format. */
        free_context = &arrival_contexts[ZONE_ARRIVAL_CONTEXT_CAPACITY - 1u];
    }
    ZeroMemory(free_context, sizeof(*free_context));
    free_context->temporary = temporary;
    lstrcpynA(free_context->world_name, world_name,
        sizeof(free_context->world_name));
    lstrcpynA(free_context->destination_name, destination_name,
        sizeof(free_context->destination_name));
    return free_context;
}

static void arm_arrival_capture(
    BOOL temporary,
    const char *world_name,
    const char *destination_name
) {
    arrival_capture_context = find_arrival_context(
        temporary, world_name, destination_name, TRUE);
    arrival_capture_armed = arrival_capture_context != NULL;
    if (arrival_capture_armed) {
        SudekiMpLogFormat(
            "zone_transition event=arrival_context_capture armed=true "
            "temporary=%d world=%s destination=%s policy=cache_native_anchor\r\n",
            temporary ? 1 : 0,
            world_name,
            destination_name);
    }
}

static BOOL context_has_actor_anchor(
    const SudekiMpZoneArrivalContext *context
) {
    unsigned int index;

    if (context == NULL || !context->valid) {
        return FALSE;
    }
    for (index = 0u; index < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++index) {
        if (context->actor_position_valid[index]) {
            return TRUE;
        }
    }
    return FALSE;
}

static void capture_arrival_position(
    void *position,
    const float coordinates[3]
) {
    unsigned int actor_index;
    BOOL matched = FALSE;

    if (!arrival_capture_armed || arrival_capture_context == NULL ||
        coordinates == NULL || !nonzero_zone_position(coordinates)) {
        return;
    }
    for (actor_index = 0u;
         actor_index < SUDEKIMP_CLEANROOM_ACTOR_COUNT;
         ++actor_index) {
        uint8_t *actor = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            (SudekiMpCleanroomActor)actor_index);
        void *actor_position;

        if (!readable_zone_bytes((const char *)actor, 0x48u)) {
            continue;
        }
        actor_position = *(void **)(actor + 0x44u);
        if (actor_position != position ||
            arrival_capture_context->actor_position_valid[actor_index]) {
            continue;
        }
        memcpy(
            arrival_capture_context->actor_positions[actor_index],
            coordinates,
            sizeof(float) * 3u);
        arrival_capture_context->actor_position_valid[actor_index] = TRUE;
        matched = TRUE;
        SudekiMpLogFormat(
            "zone_transition event=arrival_context_actor actor=%s "
            "x_bits=%08lx y_bits=%08lx z_bits=%08lx\r\n",
            SudekiMpCleanroomActorLabel(
                (SudekiMpCleanroomActor)actor_index),
            (unsigned long)zone_float_bits(coordinates[0]),
            (unsigned long)zone_float_bits(coordinates[1]),
            (unsigned long)zone_float_bits(coordinates[2]));
    }
    if (!matched && !arrival_capture_context->fallback_position_valid) {
        memcpy(
            arrival_capture_context->fallback_position,
            coordinates,
            sizeof(float) * 3u);
        arrival_capture_context->fallback_position_valid = TRUE;
    }
    arrival_capture_context->valid = TRUE;
}

static void log_zone_phase(
    const char *event,
    const char *phase,
    const char *zone_name,
    const void *world
) {
    char safe_name[64];

    copy_zone_name(zone_name, safe_name);
    SudekiMpLogFormat(
        "zone_transition event=%s phase=%s zone=%s zone_ptr=%p world=%p "
        "policy=observation_only\r\n",
        event,
        phase,
        safe_name[0] == '\0' ? "<empty-or-unreadable>" : safe_name,
        zone_name,
        world
    );
}

static void log_transition_callsite(const char *event) {
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    uintptr_t base = (uintptr_t)trace_module;
    unsigned long rva = 0ul;

    if (base != 0u && caller >= base && caller - base < 0x1000000u) {
        rva = (unsigned long)(caller - base);
    }
    SudekiMpLogFormat(
        "zone_transition event=%s callsite=%p callsite_rva=0x%08lx "
        "policy=observation_only\r\n",
        event,
        (void *)caller,
        rva
    );
}

static void __cdecl trace_set_zone_now(const char *zone_name) {
    bump_zone_source_generation("set_zone_now");
    if (party_presentation_hide_candidate.snapshot_valid) {
        clear_party_presentation_hide_candidate();
        SudekiMpLogWrite(
            "party_transition event=presentation_pre_hide "
            "status=discarded reason=main_world_transition\r\n");
    }
    log_transition_callsite("set_zone_now");
    log_zone_phase("set_zone_now", "before", zone_name, NULL);
    current_world_confirmed = FALSE;
    copy_zone_name(zone_name, current_world_name);
    arm_arrival_capture(FALSE, current_world_name, current_world_name);
    ++set_zone_now_depth;
    original_set_zone_now(zone_name);
    --set_zone_now_depth;
    release_active_temporary_resource();
    log_zone_phase("set_zone_now", "after", zone_name, NULL);
}

static void __cdecl trace_enter_zone(const char *zone_name) {
    bump_zone_source_generation("enter_zone");
    log_transition_callsite("enter_zone");
    log_zone_phase("enter_zone", "before", zone_name, NULL);
    copy_zone_name(zone_name, current_world_name);
    if (!arrival_capture_armed) {
        arm_arrival_capture(FALSE, current_world_name, current_world_name);
    }
    original_enter_zone(zone_name);
    current_world_confirmed = TRUE;
    log_zone_phase("enter_zone", "after", zone_name, NULL);
}

static void __cdecl trace_switch_zone_now(const char *zone_name) {
    bump_zone_source_generation("switch_zone_now");
    log_transition_callsite("switch_zone_now");
    log_zone_phase("switch_zone_now", "before", zone_name, NULL);
    current_world_confirmed = FALSE;
    original_switch_zone_now(zone_name);
    copy_zone_name(zone_name, current_world_name);
    log_zone_phase("switch_zone_now", "after", zone_name, NULL);
}

static void __cdecl trace_load_zone(const char *zone_name) {
    bump_zone_source_generation("load_zone");
    log_zone_phase("load_zone", "before", zone_name, NULL);
    original_load_zone(zone_name);
    log_zone_phase("load_zone", "after", zone_name, NULL);
}

static void __attribute__((thiscall)) trace_switch_main_zone(
    void *world,
    const char *zone_name
) {
    bump_zone_source_generation("switch_main_zone");
    log_transition_callsite("switch_main_zone");
    log_zone_phase("switch_main_zone", "before", zone_name, world);
    copy_zone_name(zone_name, current_world_name);
    if (!arrival_capture_armed) {
        arm_arrival_capture(FALSE, current_world_name, current_world_name);
    }
    original_switch_main_zone(world, zone_name);
    last_world = world;
    current_world_confirmed = TRUE;
    log_zone_phase("switch_main_zone", "after", zone_name, world);
}

static unsigned char __attribute__((thiscall)) trace_door_activate(
    void *door,
    BOOL requested,
    BOOL forced
) {
    SudekiMpLogFormat(
        "zone_transition event=door_activate phase=before door=%p "
        "requested=%d forced=%d policy=observation_only\r\n",
        door,
        requested ? 1 : 0,
        forced ? 1 : 0
    );
    {
        unsigned char result = original_door_activate(door, requested, forced);
        SudekiMpLogFormat(
            "zone_transition event=door_activate phase=after door=%p "
            "requested=%d forced=%d result=%u policy=observation_only\r\n",
            door,
            requested ? 1 : 0,
            forced ? 1 : 0,
            (unsigned int)result
        );
        return result;
    }
}

static void __cdecl party_atomic_enter_lead_pop(
    void *entity_pointer,
    const void *resource_name
) {
    party_atomic_position_scope =
        party_atomic_transition_kind == PARTY_ATOMIC_ENTER_TEMPORARY;
    party_atomic_lead_setter_seen = FALSE;
    party_atomic_expected_lead_position = current_party_lead_position();
    original_enter_lead_pop(entity_pointer, resource_name);
    party_atomic_position_scope = FALSE;
    party_atomic_expected_lead_position = NULL;
    if (party_atomic_transition_kind == PARTY_ATOMIC_ENTER_TEMPORARY &&
        party_atomic_lead_setter_seen) {
        party_atomic_settle_descriptor = NULL;
        party_atomic_settle_since = 0u;
        SudekiMpLogFormat(
            "party_transition event=lead_placement status=confirmed "
            "serial=%u kind=%u policy=defer_formation_until_destination_settled\r\n",
            party_atomic_transition_serial,
            PARTY_ATOMIC_ENTER_TEMPORARY);
    } else {
        finish_party_atomic_transition(
            PARTY_ATOMIC_ENTER_TEMPORARY,
            FALSE);
    }
}

static void __attribute__((stdcall)) party_atomic_exit_lead_move(
    void *mover,
    const void *gel_location,
    uint32_t flag_zero,
    uint32_t flag_one,
    float seconds
) {
    party_atomic_position_scope =
        party_atomic_transition_kind == PARTY_ATOMIC_EXIT_TEMPORARY;
    party_atomic_lead_setter_seen = FALSE;
    party_atomic_expected_lead_position = current_party_lead_position();
    original_exit_lead_move(
        mover, gel_location, flag_zero, flag_one, seconds);
    party_atomic_position_scope = FALSE;
    party_atomic_expected_lead_position = NULL;
    if (SudekiMpZoneTransitionShouldDeferExitLeadPlacement(
            party_atomic_transition_kind == PARTY_ATOMIC_EXIT_TEMPORARY,
            party_atomic_lead_setter_seen)) {
        party_atomic_settle_descriptor = NULL;
        party_atomic_settle_since = 0u;
        SudekiMpLogFormat(
            "party_transition event=lead_placement status=confirmed "
            "serial=%u kind=%u "
            "policy=always_defer_formation_until_exterior_settled\r\n",
            party_atomic_transition_serial,
            PARTY_ATOMIC_EXIT_TEMPORARY);
    } else {
        finish_party_atomic_transition(
            PARTY_ATOMIC_EXIT_TEMPORARY, FALSE);
    }
}

enum {
    TRANSITION_VOTE_ENTRY_FALLTHROUGH = 0,
    TRANSITION_VOTE_ENTRY_DEFERRED = 1,
    TRANSITION_VOTE_ENTRY_BLOCKED = 2,
    TRANSITION_VOTE_PRESENTATION_UNCERTAIN = -1,
    TRANSITION_VOTE_PRESENTATION_HIDDEN = 0,
    TRANSITION_VOTE_PRESENTATION_VISIBLE = 1
};

static BOOL transition_vote_process_owns_foreground(void) {
    HWND foreground = GetForegroundWindow();
    DWORD process_id = 0u;

    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &process_id);
    }
    return process_id == GetCurrentProcessId();
}

static uint8_t transition_vote_active_human_mask(void) {
    uint8_t mask = 0x01u;

    if (SudekiMpSplitScreenRolesLocked() &&
        SudekiMpSplitScreenRosterParticipationRequested() &&
        SudekiMpControlSeparationPlayerTwoActive() &&
        SudekiMpControlSeparationInputReady()) {
        mask |= 0x02u;
    }
    return mask;
}

static BOOL transition_vote_raw_input_neutral(
    const SudekiMpInputBridgeState *state
) {
    return state != NULL && state->left_x == 0 && state->left_y == 0 &&
        state->right_x == 0 && state->right_y == 0 &&
        state->left_trigger == 0u && state->right_trigger == 0u &&
        state->buttons == 0u;
}

static BOOL transition_vote_exterior_source_ready(
    void *expected_world,
    void *expected_descriptor
) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *world;
    uint8_t *descriptor;

    if (base == NULL || expected_world == NULL ||
        !current_world_confirmed || last_world != expected_world ||
        current_temporary_name[0] != '\0' ||
        !readable_zone_bytes(
            (const char *)(base + RVA_WORLD_GLOBAL), sizeof(world))) {
        return FALSE;
    }
    world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
    if (world != expected_world ||
        !readable_zone_bytes((const char *)world, 0x39bu)) {
        return FALSE;
    }
    descriptor = *(uint8_t **)(world + 0x0cu);
    return descriptor != NULL &&
        (expected_descriptor == NULL || descriptor == expected_descriptor) &&
        readable_zone_bytes((const char *)descriptor, 0x38u) &&
        *(uint32_t *)(descriptor + 0x34u) == 3u &&
        *(void **)(world + 0x14u) == NULL &&
        *(uint8_t *)(world + 0x399u) != 0u &&
        *(uint8_t *)(world + 0x39au) != 0u;
}

static void *transition_vote_current_source_descriptor(void *world) {
    if (!transition_vote_exterior_source_ready(world, NULL)) {
        return NULL;
    }
    return *(void **)((uint8_t *)world + 0x0cu);
}

static BOOL transition_vote_take_fresh_hide_candidate(
    SudekiMpPartyPresentationLease *candidate
) {
    DWORD now = GetTickCount();
    SudekiMpPartyPresentationLease saved =
        party_presentation_hide_candidate;

    if (candidate == NULL) {
        return FALSE;
    }
    *candidate = saved;
    if (!saved.snapshot_valid ||
        saved.snapshot_tick == 0u ||
        saved.capture_thread_id != GetCurrentThreadId() ||
        (DWORD)(now - saved.snapshot_tick) >
            PARTY_PRESENTATION_CANDIDATE_TIMEOUT_MS ||
        !party_presentation_snapshot_matches(
            &saved, 1, TRUE, TRUE)) {
        clear_party_presentation_hide_candidate();
        return FALSE;
    }
    clear_party_presentation_hide_candidate();
    return TRUE;
}

static void transition_vote_rearm_hidden_candidate(
    const SudekiMpPartyPresentationLease *candidate
) {
    if (candidate == NULL || !party_presentation_snapshot_matches(
            candidate, 1, TRUE, TRUE)) {
        return;
    }
    party_presentation_hide_candidate = *candidate;
    party_presentation_hide_candidate.snapshot_tick = GetTickCount();
    party_presentation_hide_candidate.capture_thread_id =
        GetCurrentThreadId();
}

static int transition_vote_restore_waiting_visibility(
    const SudekiMpPartyPresentationLease *candidate
) {
    if (candidate == NULL || show_party_members == NULL ||
        hide_party_members == NULL ||
        !party_presentation_snapshot_matches(
            candidate, 1, TRUE, TRUE)) {
        return TRANSITION_VOTE_PRESENTATION_UNCERTAIN;
    }
    show_party_members(candidate->group);
    if (party_presentation_snapshot_matches(
            candidate, 0, FALSE, TRUE)) {
        return TRANSITION_VOTE_PRESENTATION_VISIBLE;
    }
    if (party_presentation_snapshot_matches(
            candidate, 1, TRUE, TRUE)) {
        return TRANSITION_VOTE_PRESENTATION_HIDDEN;
    }
    if (party_presentation_snapshot_matches(
            candidate, 0, FALSE, FALSE)) {
        hide_party_members(candidate->group);
        if (party_presentation_snapshot_matches(
                candidate, 1, TRUE, TRUE)) {
            return TRANSITION_VOTE_PRESENTATION_HIDDEN;
        }
    }
    return TRANSITION_VOTE_PRESENTATION_UNCERTAIN;
}

static BOOL transition_vote_hide_visible_presentation(
    const SudekiMpPartyPresentationLease *candidate
) {
    if (candidate == NULL || hide_party_members == NULL ||
        !party_presentation_snapshot_matches(
            candidate, 0, FALSE, TRUE)) {
        return FALSE;
    }
    hide_party_members(candidate->group);
    return party_presentation_snapshot_matches(
        candidate, 1, TRUE, TRUE);
}

static BOOL transition_vote_retain_exact_resource(
    const char *zone_name,
    const void *resource_name,
    char destination[64],
    SudekiMpResourceName *retained
) {
    ZeroMemory(retained, sizeof(*retained));
    return copy_exact_zone_name(zone_name, destination) &&
        SudekiMpCleanroomEngineRetainResourceNameExact(
            retained,
            (const SudekiMpResourceName *)resource_name);
}

static int transition_vote_handle_repeated_entry(void) {
    SudekiMpPartyPresentationLease candidate;
    int presentation;

    ZeroMemory(&candidate, sizeof(candidate));
    if (!transition_vote_take_fresh_hide_candidate(&candidate)) {
        quarantine_transition_vote_visibility(
            &candidate, "repeated_entry_without_exact_hide_snapshot");
        cancel_pending_transition_vote("repeated_entry_without_owned_hide");
        return TRANSITION_VOTE_ENTRY_BLOCKED;
    }
    presentation = transition_vote_restore_waiting_visibility(&candidate);
    if (presentation != TRANSITION_VOTE_PRESENTATION_VISIBLE) {
        quarantine_transition_vote_visibility(
            &candidate, "repeated_entry_show_unconfirmed");
    }
    cancel_pending_transition_vote(
        presentation == TRANSITION_VOTE_PRESENTATION_VISIBLE ?
            "repeated_entry_restored" :
            "repeated_entry_presentation_unconfirmed");
    return TRANSITION_VOTE_ENTRY_BLOCKED;
}

static int transition_vote_try_begin(
    void *world,
    const char *zone_name,
    const void *resource_name
) {
    SudekiMpResourceName retained;
    SudekiMpPartyPresentationLease candidate;
    SudekiMpInputBridgeState raw_input;
    SudekiMpTransitionVoteResult result;
    void *descriptor;
    char destination[64];
    DWORD now;
    int presentation;

    if (!transition_vote_enabled) {
        return TRANSITION_VOTE_ENTRY_FALLTHROUGH;
    }
    if (transition_vote_visibility_quarantined) {
        return TRANSITION_VOTE_ENTRY_BLOCKED;
    }
    if (pending_transition_vote.valid) {
        return transition_vote_handle_repeated_entry();
    }
    if (transition_vote_active_human_mask() != 0x03u) {
        return TRANSITION_VOTE_ENTRY_FALLTHROUGH;
    }
    ZeroMemory(&candidate, sizeof(candidate));
    if (!transition_vote_take_fresh_hide_candidate(&candidate)) {
        quarantine_transition_vote_visibility(
            &candidate, "no_fresh_exact_native_hide");
        SudekiMpLogWrite(
            "transition_vote event=open status=blocked "
            "reason=no_fresh_exact_native_hide "
            "policy=never_enter_without_ready_consent\r\n");
        return TRANSITION_VOTE_ENTRY_BLOCKED;
    }
    presentation = transition_vote_restore_waiting_visibility(&candidate);
    if (presentation != TRANSITION_VOTE_PRESENTATION_VISIBLE) {
        quarantine_transition_vote_visibility(
            &candidate, "owned_show_not_confirmed");
        SudekiMpLogWrite(
            "transition_vote event=open status=blocked "
            "reason=owned_show_not_confirmed "
            "policy=never_enter_or_retry_with_uncertain_visibility_accounting\r\n");
        return TRANSITION_VOTE_ENTRY_BLOCKED;
    }

    descriptor = transition_vote_current_source_descriptor(world);
    if (party_atomic_transition_kind != PARTY_ATOMIC_NONE ||
        active_temporary_resource_valid || descriptor == NULL ||
        !SudekiMpInputBridgePollRaw(&raw_input) ||
        !transition_vote_retain_exact_resource(
            zone_name, resource_name, destination, &retained)) {
        SudekiMpLogWrite(
            "transition_vote event=open status=blocked "
            "reason=runtime_resource_or_source_preflight_failed "
            "presentation=visible policy=never_enter_without_ready_consent\r\n");
        return TRANSITION_VOTE_ENTRY_BLOCKED;
    }

    now = GetTickCount();
    result = SudekiMpTransitionVoteRequest(
        &transition_vote, 0u, 0x03u, now,
        TRANSITION_VOTE_TIMEOUT_MS);
    if (result != SUDEKIMP_TRANSITION_VOTE_OPENED) {
        SudekiMpCleanroomEngineReleaseResourceName(&retained);
        SudekiMpTransitionVoteReset(&transition_vote);
        SudekiMpLogWrite(
            "transition_vote event=open status=blocked "
            "reason=request_state_rejected presentation=visible "
            "policy=never_enter_without_ready_consent\r\n");
        return TRANSITION_VOTE_ENTRY_BLOCKED;
    }

    ZeroMemory(&pending_transition_vote,
        sizeof(pending_transition_vote));
    pending_transition_vote.valid = TRUE;
    pending_transition_vote.resource_valid = TRUE;
    pending_transition_vote.world = world;
    pending_transition_vote.source_descriptor = descriptor;
    pending_transition_vote.source_generation = zone_source_generation;
    pending_transition_vote.overlay_report_deadline = now +
        TRANSITION_VOTE_OVERLAY_REPORT_TIMEOUT_MS;
    pending_transition_vote.resource = retained;
    pending_transition_vote.presentation = candidate;
    pending_transition_vote.last_player_two_sequence = raw_input.sequence;
    pending_transition_vote.last_player_two_buttons = raw_input.buttons;
    pending_transition_vote.player_one_escape_was_down =
        (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    memcpy(pending_transition_vote.destination, destination,
        sizeof(pending_transition_vote.destination));
    SudekiMpInputBridgeSetGameplaySuppressed(TRUE);
    SudekiMpLogFormat(
        "transition_vote event=open status=waiting serial=%lu "
        "requester=1 destination=%s timeout_ms=%u "
        "participant_mask=0x03 presentation=visible overlay=awaiting "
        "resource_kind=0x%08lx resource_id=0x%08lx "
        "policy=visible_5s_p1_implicit_p2_post_open_neutral_then_a_accept_b_or_p1_escape_veto_silence_commit\r\n",
        (unsigned long)transition_vote.serial,
        pending_transition_vote.destination,
        TRANSITION_VOTE_TIMEOUT_MS,
        (unsigned long)pending_transition_vote.resource.encoded_kind,
        (unsigned long)pending_transition_vote.resource.identifier);
    return TRANSITION_VOTE_ENTRY_DEFERRED;
}

static void execute_enter_temporary_zone(
    void *world,
    const char *zone_name,
    const void *resource_name
) {
    last_world = world;
    copy_zone_name(zone_name, current_temporary_name);
    temporary_position_samples = 0u;
    temporary_camera_samples = 0u;
    log_zone_phase("enter_temporary_zone", "before", zone_name, world);
    arm_arrival_capture(TRUE, current_world_name, current_temporary_name);
    SudekiMpLogFormat(
        "zone_transition event=enter_temporary_zone_resource resource=%p "
        "policy=observation_only\r\n",
        resource_name
    );
    original_enter_temporary_zone(world, zone_name, resource_name);
    copy_zone_name(zone_name, current_temporary_name);
    if (readable_zone_bytes((const char *)world, 0x42u)) {
        if (arrival_capture_context != NULL) {
            arrival_capture_context->camera_index =
                *(uint16_t *)((uint8_t *)world + 0x40u);
        }
        SudekiMpLogFormat(
            "zone_transition event=temporary_world_state phase=after "
            "world=%p start_x_bits=%08lx start_y_bits=%08lx "
            "start_z_bits=%08lx orient_x_bits=%08lx orient_y_bits=%08lx "
            "orient_z_bits=%08lx camera_index=%u policy=observation_only\r\n",
            world,
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x28u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x2cu)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x30u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x34u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x38u)),
            (unsigned long)zone_float_bits(*(float *)((uint8_t *)world + 0x3cu)),
            (unsigned int)*(uint16_t *)((uint8_t *)world + 0x40u));
    }
    log_zone_phase("enter_temporary_zone", "after", zone_name, world);
}

static void __attribute__((thiscall)) trace_enter_temporary_zone(
    void *world,
    const char *zone_name,
    const void *resource_name
) {
    int vote_action = transition_vote_try_begin(
        world, zone_name, resource_name);

    if (vote_action != TRANSITION_VOTE_ENTRY_FALLTHROUGH) {
        return;
    }
    (void)begin_party_atomic_transition(
        PARTY_ATOMIC_ENTER_TEMPORARY, zone_name);
    bump_zone_source_generation("native_temporary_entry_started");
    execute_enter_temporary_zone(world, zone_name, resource_name);
}

static void __attribute__((thiscall)) trace_exit_temporary_zone(void *world) {
    uint8_t *active_descriptor = NULL;
    unsigned int active_state = 0u;
    BOOL genuine_temporary_exit;

    bump_zone_source_generation("exit_temporary_zone");
    if (readable_zone_bytes((const char *)world, 0x10u)) {
        active_descriptor = *(uint8_t **)((uint8_t *)world + 0x0cu);
    }
    if (readable_zone_bytes((const char *)active_descriptor, 0x38u)) {
        active_state = *(uint32_t *)(active_descriptor + 0x34u);
    }
    genuine_temporary_exit = set_zone_now_depth == 0u &&
        active_state == 4u;
    if (SudekiMpZoneTransitionShouldArmTemporaryExit(
            SudekiMpSplitScreenRolesLocked(),
            set_zone_now_depth,
            active_state)) {
        begin_party_atomic_transition(PARTY_ATOMIC_EXIT_TEMPORARY, NULL);
    } else if (party_atomic_transitions_enabled &&
            SudekiMpSplitScreenRosterParticipationAvailable()) {
        SudekiMpLogFormat(
            "party_transition event=begin status=ignored kind=%u "
            "reason=%s active_state=%u policy=preserve_roster_participation\r\n",
            PARTY_ATOMIC_EXIT_TEMPORARY,
            set_zone_now_depth != 0u ?
                "set_zone_now_cleanup" : "no_active_temporary_zone",
            active_state);
    }
    if (genuine_temporary_exit &&
        party_presentation_lease.override_active &&
        !prepare_party_presentation_for_exit()) {
        SudekiMpLogWrite(
            "party_transition event=presentation_exit_balance "
            "status=quarantine_required reason=pre_exit_balance_failed\r\n");
    }
    last_world = world;
    log_zone_phase("exit_temporary_zone", "before", NULL, world);
    original_exit_temporary_zone(world);
    finish_party_presentation_exit_balance();
    current_temporary_name[0] = '\0';
    if (genuine_temporary_exit) {
        release_active_temporary_resource();
    }
    log_zone_phase("exit_temporary_zone", "after", NULL, world);
}

BOOL SudekiMpZoneTransitionShouldArmTemporaryExit(
    BOOL roles_locked,
    unsigned int set_zone_now_nesting,
    unsigned int active_descriptor_state
) {
    return roles_locked != FALSE &&
        set_zone_now_nesting == 0u &&
        active_descriptor_state == 4u;
}

BOOL SudekiMpZoneTransitionShouldDeferExitLeadPlacement(
    BOOL exit_transition_active,
    BOOL lead_setter_seen
) {
    return exit_transition_active != FALSE && lead_setter_seen != FALSE;
}

static void __cdecl trace_set_player_position(float x, float y, float z) {
    SudekiMpLogFormat(
        "zone_transition event=set_player_position phase=before "
        "x_bits=%08lx y_bits=%08lx z_bits=%08lx policy=observation_only\r\n",
        (unsigned long)zone_float_bits(x),
        (unsigned long)zone_float_bits(y),
        (unsigned long)zone_float_bits(z));
    original_set_player_position(x, y, z);
    SudekiMpLogFormat(
        "zone_transition event=set_player_position phase=after "
        "x_bits=%08lx y_bits=%08lx z_bits=%08lx policy=observation_only\r\n",
        (unsigned long)zone_float_bits(x),
        (unsigned long)zone_float_bits(y),
        (unsigned long)zone_float_bits(z));
}

static void __attribute__((fastcall)) trace_internal_position_setter(
    void *position,
    const float *coordinates
) {
    if (party_atomic_position_scope && position != NULL &&
        position == party_atomic_expected_lead_position &&
        coordinates != NULL &&
        readable_zone_bytes(
            (const char *)coordinates, sizeof(float) * 3u) &&
        isfinite(coordinates[0]) && isfinite(coordinates[1]) &&
        isfinite(coordinates[2])) {
        party_atomic_lead_setter_seen = TRUE;
    }
    capture_arrival_position(position, coordinates);
    if (current_temporary_name[0] != '\0' &&
        coordinates != NULL && temporary_position_samples < 16u &&
        readable_zone_bytes((const char *)coordinates, sizeof(float) * 3u)) {
        SudekiMpLogFormat(
            "zone_transition event=internal_position_setter "
            "temporary=%s position=%p sample=%u "
            "x_bits=%08lx y_bits=%08lx z_bits=%08lx "
            "policy=observation_only\r\n",
            current_temporary_name,
            position,
            temporary_position_samples,
            (unsigned long)zone_float_bits(coordinates[0]),
            (unsigned long)zone_float_bits(coordinates[1]),
            (unsigned long)zone_float_bits(coordinates[2]));
        ++temporary_position_samples;
    }
    original_internal_position_setter(position, coordinates);
}

static BOOL __attribute__((thiscall)) trace_set_render_camera(
    void *manager,
    const char *name
) {
    char safe_name[64];
    BOOL result;

    copy_zone_name(name, safe_name);
    SudekiMpLogFormat(
        "zone_transition event=set_render_camera phase=before "
        "temporary=%s manager=%p name=%s policy=observation_only\r\n",
        current_temporary_name[0] == '\0' ? "<none>" : current_temporary_name,
        manager,
        safe_name[0] == '\0' ? "<empty-or-unreadable>" : safe_name);
    result = original_set_render_camera(manager, name);
    SudekiMpLogFormat(
        "zone_transition event=set_render_camera phase=after "
        "temporary=%s manager=%p name=%s result=%d policy=observation_only\r\n",
        current_temporary_name[0] == '\0' ? "<none>" : current_temporary_name,
        manager,
        safe_name[0] == '\0' ? "<empty-or-unreadable>" : safe_name,
        result ? 1 : 0);
    return result;
}

static unsigned char __attribute__((stdcall)) trace_temporary_camera_state_update(
    void *camera_state
) {
    unsigned short before = 0xffffu;
    unsigned char result;

    if (current_temporary_name[0] != '\0' && camera_state != NULL &&
        temporary_camera_samples < 8u &&
        readable_zone_bytes((const char *)camera_state, 0x30u)) {
        before = *(unsigned short *)((uint8_t *)camera_state + 0x2eu);
        SudekiMpLogFormat(
            "zone_transition event=temporary_camera_state_update "
            "phase=before temporary=%s state=%p camera_index=%u "
            "policy=observation_only\r\n",
            current_temporary_name,
            camera_state,
            (unsigned int)before);
    }
    result = original_temporary_camera_state_update(camera_state);
    if (current_temporary_name[0] != '\0' && camera_state != NULL &&
        temporary_camera_samples < 8u &&
        readable_zone_bytes((const char *)camera_state, 0x30u)) {
        SudekiMpLogFormat(
            "zone_transition event=temporary_camera_state_update "
            "phase=after temporary=%s state=%p camera_index_before=%u "
            "camera_index_after=%u result=%u policy=observation_only\r\n",
            current_temporary_name,
            camera_state,
            (unsigned int)before,
            (unsigned int)*(unsigned short *)((uint8_t *)camera_state + 0x2eu),
            (unsigned int)result);
        ++temporary_camera_samples;
    }
    return result;
}

static BOOL install_zone_hook(
    SudekiMpInlineHook *hook,
    uint8_t *target,
    const uint8_t *entry,
    size_t entry_size,
    const void *replacement
) {
    return SudekiMpInstallInlineHook(
        hook,
        target,
        entry,
        entry_size,
        replacement
    );
}

static BOOL install_named_zone_hook(
    const char *name,
    SudekiMpInlineHook *hook,
    uint8_t *target,
    const uint8_t *entry,
    size_t entry_size,
    const void *replacement
) {
    if (install_zone_hook(hook, target, entry, entry_size, replacement)) {
        return TRUE;
    }
    SudekiMpLogFormat(
        "zone_transition_trace_install status=rejected hook=%s "
        "target=%p expected_size=%lu win32_error=%lu\r\n",
        name,
        target,
        (unsigned long)entry_size,
        (unsigned long)GetLastError()
    );
    return FALSE;
}

BOOL SudekiMpInstallZoneTransitionTrace(
    HMODULE game_module,
    BOOL enable_party_atomic_transitions,
    BOOL enable_camera_trace
) {
    uint8_t *base;

    if (game_module == NULL || trace_module != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (transition_vote_requested && !enable_party_atomic_transitions) {
        SetLastError(ERROR_INVALID_PARAMETER);
        SudekiMpLogWrite(
            "zone_transition_trace transition_vote_install=rejected "
            "reason=party_atomic_transitions_required\r\n");
        return FALSE;
    }
    base = (uint8_t *)game_module;
    party_atomic_transitions_enabled =
        enable_party_atomic_transitions != FALSE;
    transition_vote_enabled = transition_vote_requested &&
        party_atomic_transitions_enabled;
    camera_trace_enabled = enable_camera_trace != FALSE;
    if (!install_named_zone_hook(
            "set_zone_now",
            &set_zone_now_hook,
            base + RVA_SET_ZONE_NOW,
            set_zone_now_entry,
            sizeof(set_zone_now_entry),
            trace_set_zone_now) ||
        !install_named_zone_hook(
            "enter_zone",
            &enter_zone_hook,
            base + RVA_ENTER_ZONE,
            enter_zone_entry,
            sizeof(enter_zone_entry),
            trace_enter_zone) ||
        !install_named_zone_hook(
            "switch_zone_now",
            &switch_zone_now_hook,
            base + RVA_SWITCH_ZONE_NOW,
            switch_zone_now_entry,
            sizeof(switch_zone_now_entry),
            trace_switch_zone_now) ||
        !install_named_zone_hook(
            "switch_main_zone",
            &switch_main_zone_hook,
            base + RVA_SWITCH_MAIN_ZONE,
            switch_main_zone_entry,
            sizeof(switch_main_zone_entry),
            trace_switch_main_zone)) {
        SudekiMpUninstallZoneTransitionTrace();
        return FALSE;
    }
    if (!install_named_zone_hook(
            "load_zone",
            &load_zone_hook,
            base + RVA_LOAD_ZONE,
            load_zone_entry,
            sizeof(load_zone_entry),
            trace_load_zone)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=load_zone status=unavailable "
            "policy=continue_without_loadzone_hook\r\n"
        );
    }
    if (!install_named_zone_hook(
            "door_activate_from_script",
            &door_activate_hook,
            base + RVA_DOOR_ACTIVATE_FROM_SCRIPT,
            door_activate_entry,
            sizeof(door_activate_entry),
            trace_door_activate)) {
        SudekiMpUninstallZoneTransitionTrace();
        return FALSE;
    }
    if (!install_named_zone_hook(
            "enter_temporary_zone",
            &enter_temporary_zone_hook,
            base + RVA_ENTER_TEMPORARY_ZONE,
            enter_temporary_zone_entry,
            sizeof(enter_temporary_zone_entry),
            trace_enter_temporary_zone) ||
        !install_named_zone_hook(
            "exit_temporary_zone",
            &exit_temporary_zone_hook,
            base + RVA_EXIT_TEMPORARY_ZONE,
            exit_temporary_zone_entry,
            sizeof(exit_temporary_zone_entry),
            trace_exit_temporary_zone)) {
        SudekiMpUninstallZoneTransitionTrace();
        return FALSE;
    }
    if (!install_named_zone_hook(
            "set_player_position",
            &set_player_position_hook,
            base + RVA_SET_PLAYER_POSITION,
            set_player_position_entry,
            sizeof(set_player_position_entry),
            trace_set_player_position)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=set_player_position "
            "status=unavailable policy=continue_without_position_hook\r\n");
    }
    if (!install_named_zone_hook(
            "internal_position_setter",
            &internal_position_setter_hook,
            base + RVA_INTERNAL_POSITION_SETTER,
            internal_position_setter_entry,
            sizeof(internal_position_setter_entry),
            trace_internal_position_setter)) {
        if (party_atomic_transitions_enabled) {
            SudekiMpLogWrite(
                "zone_transition_trace hook=internal_position_setter "
                "status=required_for_party_atomic_transitions\r\n");
            SudekiMpUninstallZoneTransitionTrace();
            return FALSE;
        }
        SudekiMpLogWrite(
            "zone_transition_trace hook=internal_position_setter "
            "status=unavailable policy=continue_without_internal_position_hook\r\n");
    }
    if (party_atomic_transitions_enabled) {
        if (!formation_pop_members_signature_matches(base) ||
            !set_mode_lead_only_signature_matches(base) ||
            !set_mode_full_party_signature_matches(base) ||
            !party_presentation_signatures_match(base)) {
            SetLastError(ERROR_INVALID_DATA);
            SudekiMpLogWrite(
                "zone_transition_trace party_atomic_install=rejected "
                "reason=formation_party_mode_or_presentation_signature_mismatch\r\n");
            SudekiMpUninstallZoneTransitionTrace();
            return FALSE;
        }
        original_enter_lead_pop = (PopToNamedLocationFunction)(
            base + RVA_POP_TO_NAMED_LOCATION);
        original_exit_lead_move = (ExitLeadMoveFunction)(
            base + RVA_EXIT_LEAD_MOVE);
        formation_pop_members = (FormationPopMembersFunction)(
            base + RVA_FORMATION_POP_MEMBERS);
        set_mode_full_party = (SetModeFullPartyFunction)(
            base + RVA_SET_MODE_FULL_PARTY);
        set_mode_lead_only = (SetModeLeadOnlyFunction)(
            base + RVA_SET_MODE_LEAD_ONLY);
        show_party_members = (PartyPresentationFunction)(
            base + RVA_SHOW_PARTY_MEMBERS);
        if (!install_named_zone_hook(
                "hide_party_members",
                &hide_party_members_hook,
                base + RVA_HIDE_PARTY_MEMBERS,
                hide_party_members_entry,
                sizeof(hide_party_members_entry),
                trace_hide_party_members)) {
            SudekiMpLogWrite(
                "zone_transition_trace party_atomic_install=rejected "
                "reason=hide_party_members_hook_failed\r\n");
            SudekiMpUninstallZoneTransitionTrace();
            return FALSE;
        }
        hide_party_members = (PartyPresentationFunction)
            hide_party_members_hook.trampoline;
        if (!SudekiMpInstallRelativeCallHook(
                &enter_lead_pop_hook,
                base + RVA_ENTER_LEAD_POP_CALL,
                original_enter_lead_pop,
                party_atomic_enter_lead_pop) ||
            !SudekiMpInstallRelativeCallHook(
                &exit_lead_move_hook,
                base + RVA_EXIT_LEAD_MOVE_CALL,
                original_exit_lead_move,
                party_atomic_exit_lead_move)) {
            SudekiMpLogFormat(
                "zone_transition_trace party_atomic_install=rejected "
                "reason=placement_callsite_mismatch error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpUninstallZoneTransitionTrace();
            return FALSE;
        }
    }
    if (camera_trace_enabled && !install_named_zone_hook(
            "set_render_camera",
            &set_render_camera_hook,
            base + RVA_SET_RENDER_CAMERA,
            set_render_camera_entry,
            sizeof(set_render_camera_entry),
            trace_set_render_camera)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=set_render_camera "
            "status=unavailable policy=continue_without_camera_hook\r\n");
    }
    if (camera_trace_enabled && !install_named_zone_hook(
            "temporary_camera_state_update",
            &temporary_camera_state_update_hook,
            base + RVA_TEMPORARY_CAMERA_STATE_UPDATE,
            temporary_camera_state_update_entry,
            sizeof(temporary_camera_state_update_entry),
            trace_temporary_camera_state_update)) {
        SudekiMpLogWrite(
            "zone_transition_trace hook=temporary_camera_state_update "
            "status=unavailable policy=continue_without_camera_state_hook\r\n");
    }
    original_set_zone_now = (ZoneFunction)set_zone_now_hook.trampoline;
    original_enter_zone = (ZoneFunction)enter_zone_hook.trampoline;
    original_switch_zone_now = (ZoneFunction)switch_zone_now_hook.trampoline;
    original_load_zone = (ZoneFunction)load_zone_hook.trampoline;
    original_switch_main_zone =
        (SwitchMainZoneFunction)switch_main_zone_hook.trampoline;
    original_door_activate =
        (DoorActivateFunction)door_activate_hook.trampoline;
    original_enter_temporary_zone =
        (EnterTemporaryZoneFunction)enter_temporary_zone_hook.trampoline;
    original_exit_temporary_zone =
        (ExitTemporaryZoneFunction)exit_temporary_zone_hook.trampoline;
    original_set_player_position =
        (SetPlayerPositionFunction)set_player_position_hook.trampoline;
    original_internal_position_setter =
        (InternalPositionSetterFunction)internal_position_setter_hook.trampoline;
    original_set_render_camera = camera_trace_enabled ?
        (SetRenderCameraFunction)set_render_camera_hook.trampoline : NULL;
    original_temporary_camera_state_update = camera_trace_enabled ?
        (TemporaryCameraStateUpdateFunction)
            temporary_camera_state_update_hook.trampoline : NULL;
    trace_module = game_module;
    last_world = NULL;
    current_world_confirmed = FALSE;
    current_world_name[0] = '\0';
    current_temporary_name[0] = '\0';
    ZeroMemory(&active_temporary_resource,
        sizeof(active_temporary_resource));
    active_temporary_resource_valid = FALSE;
    ZeroMemory(arrival_contexts, sizeof(arrival_contexts));
    arrival_capture_context = NULL;
    arrival_capture_armed = FALSE;
    arrival_reapply_pending = FALSE;
    arrival_reapply_after = 0u;
    arrival_reapply_deadline = 0u;
    arrival_reapply_attempts = 0u;
    clear_party_presentation_lease();
    clear_party_presentation_hide_candidate();
    party_presentation_hide_depth = 0;
    party_presentation_hide_sequence = 0u;
    clear_party_atomic_transition();
    SudekiMpTransitionVoteInitialize(&transition_vote);
    ZeroMemory(&pending_transition_vote,
        sizeof(pending_transition_vote));
    transition_vote_visibility_quarantined = FALSE;
    ZeroMemory(&transition_vote_visibility_quarantine,
        sizeof(transition_vote_visibility_quarantine));
    zone_source_generation = 1u;
    SudekiMpInteractionProvenanceSetSourceGeneration(
        zone_source_generation);
    SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
    SudekiMpLogWrite(
        "zone_transition_trace_install=success "
        "enter_rva=0x00007970 switch_now_rva=0x00007990 "
        "load_rva=0x00007b80 switch_main_rva=0x00006380 "
        "door_activate_rva=0x000ce3a0 enter_temp_rva=0x000064b0 "
        "exit_temp_rva=0x00006710 set_player_position_rva=0x00104ed0 "
        "internal_position_setter_rva=0x00003050 set_render_camera_rva=0x00036fb0 "
        "temporary_camera_state_update_rva=0x000352d0\r\n"
    );
    SudekiMpLogFormat(
        "party_atomic_transitions_applied=%s transition_vote_applied=%s "
        "camera_trace_applied=%s "
        "enter_lead_pop_call_rva=0x00005c59 "
        "exit_lead_move_call_rva=0x000068d3 formation_pop_rva=0x000f6260 "
        "set_mode_lead_only_rva=0x00024720 "
        "set_mode_full_party_rva=0x00024850 "
        "show_party_members_rva=0x00024950 "
        "hide_party_members_rva=0x00024a70\r\n",
        party_atomic_transitions_enabled ? "true" : "false",
        transition_vote_enabled ? "true" : "false",
        camera_trace_enabled ? "true" : "false");
    return TRUE;
}

BOOL SudekiMpZoneTraversalSwitchWorld(const char *zone_name) {
    if (zone_name == NULL || zone_name[0] == '\0' ||
        original_set_zone_now == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!SudekiMpZoneTraversalArrivalContextReady(zone_name, NULL)) {
        SetLastError(ERROR_NOT_FOUND);
        SudekiMpLogFormat(
            "zone_traversal action=set_world status=rejected "
            "reason=no_cached_native_arrival_context zone=%s\r\n",
            zone_name);
        return FALSE;
    }
    if (!context_has_actor_anchor(find_arrival_context(
            FALSE, zone_name, zone_name, FALSE))) {
        SudekiMpLogFormat(
            "zone_traversal action=set_world discovery=automatic "
            "world=%s policy=native_transition_capture\r\n",
            zone_name);
    }
    SudekiMpLogFormat(
        "zone_traversal action=set_world zone=%s policy=authored_full_transition\r\n",
        zone_name);
    /* SwitchZoneNOW only marks a zone for switching. Authored transitions
     * use SetZoneNOW, which performs the complete teardown/load pipeline. */
    trace_set_zone_now(zone_name);
    current_temporary_name[0] = '\0';
    arrival_reapply_pending = TRUE;
    arrival_reapply_after = GetTickCount() + 750u;
    arrival_reapply_deadline = GetTickCount() + 15000u;
    arrival_reapply_attempts = 0u;
    return TRUE;
}

BOOL SudekiMpZoneTraversalEnterTemporary(const char *zone_name) {
    SudekiMpResourceName resource_name;

    if (zone_name == NULL || zone_name[0] == '\0' ||
        original_enter_temporary_zone == NULL || last_world == NULL ||
        current_world_name[0] == '\0') {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (!SudekiMpZoneTraversalArrivalContextReady(
            current_world_name, zone_name)) {
        SetLastError(ERROR_NOT_FOUND);
        SudekiMpLogFormat(
            "zone_traversal action=enter_temporary status=rejected "
            "reason=no_cached_native_arrival_context world=%s zone=%s\r\n",
            current_world_name,
            zone_name);
        return FALSE;
    }
    if (!context_has_actor_anchor(find_arrival_context(
            TRUE, current_world_name, zone_name, FALSE))) {
        SudekiMpLogFormat(
            "zone_traversal action=enter_temporary discovery=automatic "
            "world=%s zone=%s policy=native_transition_capture\r\n",
            current_world_name,
            zone_name);
    }
    if (!SudekiMpCleanroomEngineResourceNameFromText(
            &resource_name, zone_name)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (current_temporary_name[0] != '\0' &&
        original_exit_temporary_zone != NULL) {
        original_exit_temporary_zone(last_world);
        current_temporary_name[0] = '\0';
        release_active_temporary_resource();
    }
    SudekiMpLogFormat(
        "zone_traversal action=enter_temporary world=%s zone=%s "
        "policy=active_world_only\r\n",
        current_world_name, zone_name);
    /*
     * Temporary-zone loads can outlive this call.  Keep the reference-backed
     * ResourceName alive until the corresponding exit instead of releasing
     * it immediately after the native call returns.
     */
    active_temporary_resource = resource_name;
    ZeroMemory(&resource_name, sizeof(resource_name));
    active_temporary_resource_valid = TRUE;
    original_enter_temporary_zone(
        last_world, zone_name, &active_temporary_resource);
    copy_zone_name(zone_name, current_temporary_name);
    arrival_reapply_pending = TRUE;
    arrival_reapply_after = GetTickCount() + 750u;
    arrival_reapply_deadline = GetTickCount() + 15000u;
    arrival_reapply_attempts = 0u;
    return TRUE;
}

BOOL SudekiMpZoneTraversalExitTemporary(void) {
    if (original_exit_temporary_zone == NULL || last_world == NULL ||
        current_temporary_name[0] == '\0') {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    original_exit_temporary_zone(last_world);
    current_temporary_name[0] = '\0';
    temporary_position_samples = 0u;
    temporary_camera_samples = 0u;
    release_active_temporary_resource();
    return TRUE;
}

const char *SudekiMpZoneTraversalCurrentWorld(void) {
    return current_world_name[0] == '\0' ? NULL : current_world_name;
}

const char *SudekiMpZoneTraversalCurrentTemporary(void) {
    return current_temporary_name[0] == '\0' ? NULL : current_temporary_name;
}

BOOL SudekiMpZoneTraversalWorldMatches(const char *zone_name) {
    return current_world_confirmed && zone_name != NULL &&
        current_world_name[0] != '\0' &&
        _stricmp(current_world_name, zone_name) == 0;
}

BOOL SudekiMpZoneTraversalKnownDestination(
    const char *world_name,
    const char *temporary_name
) {
    unsigned int index;

    if (world_name == NULL || world_name[0] == '\0') {
        return FALSE;
    }
    if (temporary_name == NULL || temporary_name[0] == '\0') {
        for (index = 0u; index < sizeof(known_traversal_worlds) /
                sizeof(known_traversal_worlds[0]); ++index) {
            if (_stricmp(world_name, known_traversal_worlds[index]) == 0) {
                return TRUE;
            }
        }
        return FALSE;
    }
    for (index = 0u; index < sizeof(known_traversal_interiors) /
            sizeof(known_traversal_interiors[0]); ++index) {
        if (_stricmp(world_name, known_traversal_interiors[index][0]) == 0 &&
            _stricmp(temporary_name, known_traversal_interiors[index][1]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static SudekiMpZoneArrivalContext *current_arrival_context(void) {
    if (current_temporary_name[0] != '\0') {
        return find_arrival_context(
            TRUE,
            current_world_name,
            current_temporary_name,
            FALSE);
    }
    if (!current_world_confirmed || current_world_name[0] == '\0') {
        return NULL;
    }
    return find_arrival_context(
        FALSE,
        current_world_name,
        current_world_name,
        FALSE);
}

BOOL SudekiMpZoneTraversalArrivalContextReady(
    const char *world_name,
    const char *temporary_name
) {
    SudekiMpZoneArrivalContext *context;

    if (world_name == NULL || world_name[0] == '\0') {
        return FALSE;
    }
    context = find_arrival_context(
        temporary_name != NULL && temporary_name[0] != '\0',
        world_name,
        temporary_name != NULL && temporary_name[0] != '\0' ?
            temporary_name : world_name,
        FALSE);
    if (context_has_actor_anchor(context)) {
        return TRUE;
    }
    /* The first request for an authored destination is intentionally allowed
     * to proceed.  The native door/world pipeline will populate the context
     * through trace_internal_position_setter; later requests use the cached
     * actor-specific anchors above. */
    return SudekiMpZoneTraversalKnownDestination(world_name, temporary_name);
}

BOOL SudekiMpZoneTraversalApplyArrivalContext(void) {
    SudekiMpZoneArrivalContext *context = current_arrival_context();
    unsigned int actor_index;
    unsigned int applied = 0u;

    if (context == NULL || original_internal_position_setter == NULL ||
        !context_has_actor_anchor(context)) {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    for (actor_index = 0u;
         actor_index < SUDEKIMP_CLEANROOM_ACTOR_COUNT;
         ++actor_index) {
        uint8_t *actor = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
            (SudekiMpCleanroomActor)actor_index);
        void *position;

        if (!context->actor_position_valid[actor_index] ||
            !readable_zone_bytes((const char *)actor, 0x48u)) {
            continue;
        }
        position = *(void **)(actor + 0x44u);
        if (!readable_zone_bytes((const char *)position, 0x24u)) {
            continue;
        }
        original_internal_position_setter(
            position,
            context->actor_positions[actor_index]);
        ++applied;
    }
    if (applied == 0u) {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    SudekiMpLogFormat(
        "zone_traversal event=arrival_context_apply status=success "
        "temporary=%d world=%s destination=%s actors=%u camera_index=%u "
        "policy=native_cached_savepoint_anchor\r\n",
        context->temporary ? 1 : 0,
        context->world_name,
        context->destination_name,
        applied,
        (unsigned int)context->camera_index);
    return TRUE;
}

static BOOL pending_transition_vote_source_valid(void) {
    return pending_transition_vote.valid &&
        pending_transition_vote.resource_valid &&
        transition_vote.state != SUDEKIMP_TRANSITION_VOTE_IDLE &&
        pending_transition_vote.source_generation ==
            zone_source_generation &&
        party_atomic_transition_kind == PARTY_ATOMIC_NONE &&
        !active_temporary_resource_valid &&
        !party_presentation_lease.snapshot_valid &&
        !party_presentation_lease.override_active &&
        !party_presentation_lease.exit_balance_pending &&
        transition_vote_exterior_source_ready(
            pending_transition_vote.world,
            pending_transition_vote.source_descriptor) &&
        party_presentation_snapshot_matches(
            &pending_transition_vote.presentation, 0, FALSE, TRUE);
}

static BOOL transition_vote_rollback_owned_hide(
    const SudekiMpPartyPresentationLease *presentation
) {
    if (presentation == NULL || show_party_members == NULL ||
        !party_presentation_snapshot_matches(
            presentation, 1, TRUE, FALSE)) {
        return FALSE;
    }
    show_party_members(presentation->group);
    return party_presentation_snapshot_matches(
        presentation, 0, FALSE, TRUE);
}

static void commit_pending_transition_vote(void) {
    SudekiMpPartyPresentationLease presentation;
    uint32_t serial;

    if (!pending_transition_vote_source_valid()) {
        cancel_pending_transition_vote("source_generation_or_identity_changed");
        return;
    }
    if (!pending_transition_vote.overlay_acknowledged) {
        cancel_pending_transition_vote("commit_without_visible_overlay");
        return;
    }
    serial = transition_vote.serial;
    if (SudekiMpTransitionVoteBeginCommit(
            &transition_vote, serial) !=
            SUDEKIMP_TRANSITION_VOTE_COMMIT_STARTED) {
        cancel_pending_transition_vote("commit_claim_rejected");
        return;
    }
    presentation = pending_transition_vote.presentation;
    if (!transition_vote_hide_visible_presentation(&presentation)) {
        BOOL restored = party_presentation_snapshot_matches(
                &presentation, 0, FALSE, TRUE) ||
            transition_vote_rollback_owned_hide(&presentation);

        SudekiMpLogFormat(
            "transition_vote event=commit status=blocked serial=%lu "
            "reason=owned_hide_postcondition rollback=%s "
            "policy=never_enter_with_unproved_visibility_delta\r\n",
            (unsigned long)serial,
            restored ? "confirmed" : "unconfirmed");
        if (!restored) {
            quarantine_transition_vote_visibility(
                &presentation, "commit_hide_or_rollback_unconfirmed");
        }
        clear_pending_transition_vote();
        return;
    }
    transition_vote_rearm_hidden_candidate(&presentation);
    if (!begin_party_atomic_transition(
            PARTY_ATOMIC_ENTER_TEMPORARY,
            pending_transition_vote.destination)) {
        BOOL restored;

        clear_party_presentation_hide_candidate();
        restored = transition_vote_rollback_owned_hide(&presentation);
        SudekiMpLogFormat(
            "transition_vote event=commit status=blocked serial=%lu "
            "reason=party_atomic_quiesce_rejected rollback=%s\r\n",
            (unsigned long)serial,
            restored ? "confirmed" : "unconfirmed");
        if (!restored) {
            quarantine_transition_vote_visibility(
                &presentation, "party_atomic_rejection_rollback_unconfirmed");
        }
        clear_pending_transition_vote();
        return;
    }

    active_temporary_resource = pending_transition_vote.resource;
    ZeroMemory(&pending_transition_vote.resource,
        sizeof(pending_transition_vote.resource));
    pending_transition_vote.resource_valid = FALSE;
    active_temporary_resource_valid = TRUE;
    pending_transition_vote.valid = FALSE;
    bump_zone_source_generation("transition_vote_commit");
    execute_enter_temporary_zone(
        pending_transition_vote.world,
        pending_transition_vote.destination,
        &active_temporary_resource);
    SudekiMpLogFormat(
        "transition_vote event=commit status=invoked_once serial=%lu "
        "destination=%s policy=existing_party_atomic_path\r\n",
        (unsigned long)serial,
        pending_transition_vote.destination);
    clear_pending_transition_vote();
}

static void service_pending_transition_vote(void) {
    SudekiMpInputBridgeState raw_input;
    SudekiMpTransitionVoteResult response =
        SUDEKIMP_TRANSITION_VOTE_NO_CHANGE;
    uint32_t buttons;
    DWORD now;
    BOOL escape_down;
    BOOL foreground;

    if (!pending_transition_vote.valid) {
        return;
    }
    if (!pending_transition_vote_source_valid()) {
        cancel_pending_transition_vote("source_generation_or_identity_changed");
        return;
    }
    now = GetTickCount();
    foreground = transition_vote_process_owns_foreground();
    escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    if (foreground && escape_down &&
        !pending_transition_vote.player_one_escape_was_down) {
        response = SudekiMpTransitionVoteRespond(
            &transition_vote, transition_vote.serial, 0u, 0);
    }
    pending_transition_vote.player_one_escape_was_down = escape_down;

    if (SudekiMpInputBridgePollRaw(&raw_input) &&
        SudekiMpInputBridgeSequenceIsNewer(
            raw_input.sequence,
            pending_transition_vote.last_player_two_sequence)) {
        pending_transition_vote.last_player_two_sequence =
            raw_input.sequence;
        buttons = raw_input.buttons;
        if (!pending_transition_vote.player_two_consent_armed) {
            pending_transition_vote.last_player_two_buttons = buttons;
            if (transition_vote_raw_input_neutral(&raw_input)) {
                pending_transition_vote.player_two_consent_armed = TRUE;
                SudekiMpLogFormat(
                    "transition_vote event=input_armed player=2 serial=%lu "
                    "sequence=%lu policy=post_open_newer_neutral_required\r\n",
                    (unsigned long)transition_vote.serial,
                    (unsigned long)raw_input.sequence);
            }
        } else if (response != SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW &&
            pending_transition_vote.overlay_acknowledged &&
            (buttons & SUDEKIMP_BRIDGE_BUTTON_B) != 0u &&
            (pending_transition_vote.last_player_two_buttons &
                SUDEKIMP_BRIDGE_BUTTON_B) == 0u) {
            response = SudekiMpTransitionVoteRespond(
                &transition_vote, transition_vote.serial, 1u, 0);
        } else if (response != SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW &&
            pending_transition_vote.overlay_acknowledged &&
            (buttons & SUDEKIMP_BRIDGE_BUTTON_A) != 0u &&
            (pending_transition_vote.last_player_two_buttons &
                SUDEKIMP_BRIDGE_BUTTON_A) == 0u) {
            response = SudekiMpTransitionVoteRespond(
                &transition_vote, transition_vote.serial, 1u, 1);
        }
        pending_transition_vote.last_player_two_buttons = buttons;
    }
    if (response == SUDEKIMP_TRANSITION_VOTE_CANCELLED_NOW ||
        transition_vote.state == SUDEKIMP_TRANSITION_VOTE_CANCELLED) {
        cancel_pending_transition_vote(
            transition_vote.cancelled_mask == 0x01u ?
                "player_one_escape" : "player_two_b");
        return;
    }
    if (!pending_transition_vote.overlay_acknowledged) {
        if ((LONG)(now -
                pending_transition_vote.overlay_report_deadline) >= 0) {
            cancel_pending_transition_vote("overlay_not_reported");
        }
        return;
    }
    if (transition_vote.state == SUDEKIMP_TRANSITION_VOTE_WAITING) {
        (void)SudekiMpTransitionVoteUpdate(
            &transition_vote,
            transition_vote_active_human_mask(),
            now);
    }
    if (transition_vote.state == SUDEKIMP_TRANSITION_VOTE_CANCELLED) {
        cancel_pending_transition_vote("requester_dropout");
        return;
    }
    if (transition_vote.state == SUDEKIMP_TRANSITION_VOTE_READY) {
        commit_pending_transition_vote();
    }
}

/* Return TRUE while final transition commit must remain deferred. */
static BOOL service_party_doorway_staging(DWORD now) {
    SudekiMpDoorwayStaging staging;
    SudekiMpPartyPlacementSnapshot current;
    SudekiMpPartyPlacementSnapshot after;
    SudekiMpInputBridgeState raw_input;
    unsigned int action;
    BOOL identity_matches;
    BOOL destination_matches;
    BOOL player_two_unowned;
    BOOL raw_input_available;
    BOOL input_neutral;
    BOOL follower_still_staged;
    BOOL direction_matches;
    BOOL timed_out;
    float inward_advance = 0.0f;
    const char *accept_reason = "policy_guard";

    if (!party_doorway_staging.armed) {
        return FALSE;
    }
    staging = party_doorway_staging;
    ZeroMemory(&current, sizeof(current));
    identity_matches =
        party_atomic_transition_kind == PARTY_ATOMIC_ENTER_TEMPORARY &&
        party_atomic_placement_confirmed &&
        party_atomic_transition_serial == staging.transition_serial &&
        capture_party_placement_snapshot(&current) &&
        party_placement_identity_matches(&staging.placement, &current) &&
        formation_matches_current_party();
    destination_matches = party_atomic_destination_ready(
        staging.world,
        staging.descriptor,
        staging.source_generation,
        staging.destination);
    player_two_unowned = player_two_fully_unowned();
    ZeroMemory(&raw_input, sizeof(raw_input));
    raw_input_available = SudekiMpInputBridgePollRaw(&raw_input);
    input_neutral = raw_input_available &&
        transition_vote_raw_input_neutral(&raw_input);
    follower_still_staged = identity_matches &&
        doorway_staging_follower_unchanged(&staging, &current);
    direction_matches = identity_matches &&
        formation_direction_matches_doorway_staging(&staging);
    timed_out = (LONG)(now - staging.deadline) >= 0;
    if (identity_matches) {
        float lead_dx = current.positions[0][0] -
            staging.placement.positions[0][0];
        float lead_dz = current.positions[0][2] -
            staging.placement.positions[0][2];

        inward_advance = lead_dx * staging.inward_x +
            lead_dz * staging.inward_z;
    }
    action = SudekiMpZoneTransitionDoorwayStagingAction(
        identity_matches && direction_matches,
        destination_matches,
        player_two_unowned,
        input_neutral,
        follower_still_staged,
        timed_out,
        inward_advance,
        staging.required_inward_advance);
    if (action == SUDEKIMP_DOORWAY_STAGING_WAIT) {
        return TRUE;
    }
    if (action == SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST) {
        if (!identity_matches) {
            accept_reason = "party_or_transition_identity_changed";
        } else if (!destination_matches) {
            accept_reason = "destination_changed";
        } else if (!player_two_unowned) {
            accept_reason = "player_two_ownership_returned";
        } else if (!input_neutral) {
            accept_reason = raw_input_available ?
                "player_two_input_observed" : "player_two_input_unavailable";
        } else if (!follower_still_staged) {
            accept_reason = "follower_left_first_staging_point";
        } else if (!direction_matches) {
            accept_reason = "formation_direction_changed";
        } else if (timed_out) {
            accept_reason = "lead_did_not_advance_before_timeout";
        }
        ZeroMemory(&party_doorway_staging,
            sizeof(party_doorway_staging));
        SudekiMpLogFormat(
            "party_transition event=doorway_staging status=accepted_first "
            "serial=%u reason=%s inward_advance_bits=%08lx "
            "required_advance_bits=%08lx input_sequence_arm=%lu "
            "input_sequence_now=%lu "
            "policy=no_second_pop_and_no_transform_write\r\n",
            staging.transition_serial,
            accept_reason,
            (unsigned long)zone_float_bits(inward_advance),
            (unsigned long)zone_float_bits(
                staging.required_inward_advance),
            (unsigned long)staging.last_player_two_sequence,
            (unsigned long)raw_input.sequence);
        return FALSE;
    }

    /* Clear before calling the native engine: this attempt is one-shot even
     * if the postcondition cannot be observed afterward. */
    ZeroMemory(&party_doorway_staging, sizeof(party_doorway_staging));
    if (formation_pop_members == NULL ||
        !formation_matches_current_party() ||
        !party_atomic_destination_ready(
            staging.world,
            staging.descriptor,
            staging.source_generation,
            staging.destination)) {
        SudekiMpLogFormat(
            "party_transition event=doorway_staging status=accepted_first "
            "serial=%u reason=final_native_precondition_changed "
            "policy=no_second_pop_and_no_transform_write\r\n",
            staging.transition_serial);
        return FALSE;
    }
    formation_pop_members();
    ZeroMemory(&after, sizeof(after));
    {
        BOOL after_snapshot_valid =
            formation_matches_current_party() &&
            capture_party_placement_snapshot(&after);
        BOOL after_identity_matches = after_snapshot_valid &&
            party_placement_identity_matches(&staging.placement, &after);
        BOOL after_destination_matches = party_atomic_destination_ready(
            staging.world,
            staging.descriptor,
            staging.source_generation,
            staging.destination);
        BOOL after_radius_valid = after_identity_matches &&
            party_placement_within_lead_radius(
                &after, party_placement_maximum_distance);
        float follower_inward_advance = 0.0f;
        float minimum_follower_advance =
            staging.required_inward_advance -
            doorway_staging_advance_margin - 0.25f;
        BOOL follower_advanced;

        if (after_identity_matches) {
            float follower_dx = after.positions[1][0] -
                staging.placement.positions[1][0];
            float follower_dz = after.positions[1][2] -
                staging.placement.positions[1][2];

            follower_inward_advance =
                follower_dx * staging.inward_x +
                follower_dz * staging.inward_z;
        }
        if (minimum_follower_advance < doorway_staging_minimum_extent) {
            minimum_follower_advance = doorway_staging_minimum_extent;
        }
        follower_advanced = after_identity_matches &&
            isfinite(follower_inward_advance) &&
            follower_inward_advance >= minimum_follower_advance;
        if (after_snapshot_valid) {
            log_party_placement_snapshot("second_native_pop", &after);
        }
        SudekiMpLogFormat(
            "party_transition event=doorway_staging status=%s serial=%u "
            "inward_advance_bits=%08lx required_advance_bits=%08lx "
            "follower_advance_bits=%08lx minimum_follower_advance_bits=%08lx "
            "identity=%s destination=%s radius_5=%s "
            "policy=one_shot_native_formation_pop_no_transform_fallback\r\n",
            after_identity_matches && after_destination_matches &&
                after_radius_valid && follower_advanced ?
                    "repop_confirmed" : "repop_unconfirmed",
            staging.transition_serial,
            (unsigned long)zone_float_bits(inward_advance),
            (unsigned long)zone_float_bits(
                staging.required_inward_advance),
            (unsigned long)zone_float_bits(follower_inward_advance),
            (unsigned long)zone_float_bits(minimum_follower_advance),
            after_identity_matches ? "exact" : "changed",
            after_destination_matches ? "exact" : "changed",
            after_radius_valid ? "confirmed" : "unconfirmed");
    }
    party_atomic_settle_descriptor = NULL;
    party_atomic_settle_since = 0u;
    return TRUE;
}

void SudekiMpZoneTransitionService(void) {
    uint8_t *base = (uint8_t *)trace_module;
    uint8_t *world;
    uint8_t *descriptor;
    unsigned int expected_state;
    DWORD now;
    BOOL settled = FALSE;

    service_pending_transition_vote();
    if (!party_atomic_transitions_enabled ||
        party_atomic_transition_kind == PARTY_ATOMIC_NONE) {
        return;
    }
    now = GetTickCount();
    if (!party_atomic_placement_confirmed &&
        party_atomic_lead_setter_seen && base != NULL &&
        readable_zone_bytes((const char *)(base + RVA_WORLD_GLOBAL),
            sizeof(world))) {
        world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
        if (readable_zone_bytes((const char *)world, 0x39bu)) {
            descriptor = *(uint8_t **)(world + 0x0cu);
            expected_state = party_atomic_transition_kind ==
                PARTY_ATOMIC_ENTER_TEMPORARY ? 4u : 3u;
            settled = descriptor != NULL &&
                readable_zone_bytes((const char *)descriptor, 0x38u) &&
                *(uint32_t *)(descriptor + 0x34u) == expected_state &&
                *(void **)(world + 0x14u) == NULL &&
                *(uint8_t *)(world + 0x399u) != 0u &&
                *(uint8_t *)(world + 0x39au) != 0u;
            if (settled) {
                if (party_atomic_settle_descriptor != descriptor) {
                    party_atomic_settle_descriptor = descriptor;
                    party_atomic_settle_since = now;
                } else if ((DWORD)(now - party_atomic_settle_since) >=
                        250u) {
                    BOOL presentation_ready =
                        party_atomic_transition_kind !=
                            PARTY_ATOMIC_EXIT_TEMPORARY ||
                        restore_party_presentation_after_exit();

                    if (presentation_ready) {
                        finish_party_atomic_transition(
                            party_atomic_transition_kind, TRUE);
                        party_atomic_settle_descriptor = NULL;
                        party_atomic_settle_since = 0u;
                    }
                }
                if (party_atomic_transition_deadline == 0u ||
                    (LONG)(now - party_atomic_transition_deadline) < 0) {
                    return;
                }
            } else {
                party_atomic_settle_descriptor = NULL;
                party_atomic_settle_since = 0u;
            }
        }
    }
    if (party_atomic_placement_confirmed &&
        service_party_doorway_staging(now)) {
        return;
    }
    settled = FALSE;
    if (party_atomic_placement_confirmed && base != NULL &&
        readable_zone_bytes((const char *)(base + RVA_WORLD_GLOBAL),
            sizeof(world))) {
        world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
        if (readable_zone_bytes((const char *)world, 0x39bu)) {
            descriptor = *(uint8_t **)(world + 0x0cu);
            expected_state = party_atomic_transition_kind ==
                PARTY_ATOMIC_ENTER_TEMPORARY ? 4u : 3u;
            settled = descriptor != NULL &&
                readable_zone_bytes((const char *)descriptor, 0x38u) &&
                *(uint32_t *)(descriptor + 0x34u) == expected_state &&
                *(void **)(world + 0x14u) == NULL &&
                *(uint8_t *)(world + 0x399u) != 0u &&
                *(uint8_t *)(world + 0x39au) != 0u &&
                formation_matches_current_party() &&
                current_party_within_lead_radius(
                    party_placement_maximum_distance) &&
                (party_atomic_transition_kind !=
                        PARTY_ATOMIC_ENTER_TEMPORARY ||
                    (party_presentation_lease.override_active &&
                     party_presentation_matches(0, FALSE, TRUE)));
            if (settled) {
                if (party_atomic_settle_descriptor != descriptor) {
                    party_atomic_settle_descriptor = descriptor;
                    party_atomic_settle_since = now;
                } else if ((DWORD)(now - party_atomic_settle_since) >=
                        250u) {
                    unsigned int serial = party_atomic_transition_serial;
                    unsigned int kind = party_atomic_transition_kind;

                    if (!SudekiMpSplitScreenEndPartyTransition(TRUE)) {
                        if (party_atomic_release_deadline == 0u) {
                            party_atomic_release_deadline = now +
                                PARTY_ATOMIC_RELEASE_TIMEOUT_MS;
                            SudekiMpLogFormat(
                                "party_transition event=commit "
                                "status=quarantine_pending serial=%u kind=%u "
                                "timeout_ms=%u\r\n",
                                serial,
                                kind,
                                PARTY_ATOMIC_RELEASE_TIMEOUT_MS);
                        }
                        if ((LONG)(now - party_atomic_release_deadline) < 0) {
                            return;
                        }
                    } else {
                        clear_party_atomic_transition();
                        SudekiMpLogFormat(
                            "party_transition event=commit status=confirmed "
                            "serial=%u kind=%u policy=reacquire_locked_roster_after_world_settle\r\n",
                            serial, kind);
                        return;
                    }
                }
                /* Loading may legitimately take longer than the watchdog.
                 * Once the exact destination is observable, give its own
                 * 250-ms stability window priority over wall-clock timeout. */
                if (party_atomic_release_deadline == 0u ||
                    (LONG)(now - party_atomic_release_deadline) < 0) {
                    return;
                }
            } else {
                party_atomic_settle_descriptor = NULL;
                party_atomic_settle_since = 0u;
            }
        }
    }
    if ((party_atomic_transition_deadline != 0u &&
            (LONG)(now - party_atomic_transition_deadline) >= 0) ||
        (party_atomic_release_deadline != 0u &&
            (LONG)(now - party_atomic_release_deadline) >= 0)) {
        unsigned int serial = party_atomic_transition_serial;
        unsigned int kind = party_atomic_transition_kind;
        BOOL placement_confirmed = party_atomic_placement_confirmed;
        BOOL release_quarantine_expired =
            party_atomic_release_deadline != 0u;

        clear_party_atomic_transition();
        if (party_presentation_lease.snapshot_valid &&
            !party_presentation_lease.override_active &&
            !party_presentation_lease.exit_balance_pending) {
            clear_party_presentation_lease();
        }
        (void)SudekiMpSplitScreenEndPartyTransition(FALSE);
        SudekiMpLogFormat(
            "party_transition event=commit status=timeout serial=%u kind=%u "
            "placement=%s release_quarantine=%s "
            "policy=retain_roster_drop_player_two_until_manual_rejoin\r\n",
            serial,
            kind,
            placement_confirmed ? "confirmed" : "unconfirmed",
            release_quarantine_expired ? "expired" : "not_started");
    }
}

void SudekiMpZoneTraversalService(void) {
    DWORD now;

    if (!arrival_reapply_pending ||
        (LONG)(GetTickCount() - arrival_reapply_after) < 0) {
        return;
    }
    now = GetTickCount();
    if (SudekiMpZoneTraversalApplyArrivalContext()) {
        arrival_reapply_pending = FALSE;
        arrival_reapply_deadline = 0u;
        arrival_reapply_attempts = 0u;
        return;
    }
    /* Native actor creation/placement can lag the world load by several
     * seconds. Retry on the game thread while this initiated transition is
     * still settling; this makes first-use discovery automatic. */
    if (arrival_reapply_deadline != 0u &&
        (LONG)(now - arrival_reapply_deadline) < 0) {
        ++arrival_reapply_attempts;
        arrival_reapply_after = now + 250u;
        return;
    }
    arrival_reapply_pending = FALSE;
    SudekiMpLogFormat(
        "zone_traversal event=arrival_context_apply status=rejected "
        "reason=no_actor_anchor_or_destination_not_ready attempts=%u\r\n",
        arrival_reapply_attempts);
    arrival_reapply_deadline = 0u;
    arrival_reapply_attempts = 0u;
}

void SudekiMpUninstallZoneTransitionTrace(void) {
    clear_pending_transition_vote();
    SudekiMpRestoreRelativeCallHook(&exit_lead_move_hook);
    SudekiMpRestoreRelativeCallHook(&enter_lead_pop_hook);
    SudekiMpRestoreInlineHook(&exit_temporary_zone_hook);
    SudekiMpRestoreInlineHook(&set_render_camera_hook);
    SudekiMpRestoreInlineHook(&temporary_camera_state_update_hook);
    SudekiMpRestoreInlineHook(&internal_position_setter_hook);
    SudekiMpRestoreInlineHook(&set_player_position_hook);
    SudekiMpRestoreInlineHook(&enter_temporary_zone_hook);
    SudekiMpRestoreInlineHook(&door_activate_hook);
    SudekiMpRestoreInlineHook(&switch_main_zone_hook);
    SudekiMpRestoreInlineHook(&load_zone_hook);
    SudekiMpRestoreInlineHook(&switch_zone_now_hook);
    SudekiMpRestoreInlineHook(&enter_zone_hook);
    SudekiMpRestoreInlineHook(&set_zone_now_hook);
    SudekiMpRestoreInlineHook(&hide_party_members_hook);
    original_set_zone_now = NULL;
    original_enter_zone = NULL;
    original_switch_zone_now = NULL;
    original_load_zone = NULL;
    original_switch_main_zone = NULL;
    original_door_activate = NULL;
    original_enter_temporary_zone = NULL;
    original_exit_temporary_zone = NULL;
    original_set_player_position = NULL;
    original_internal_position_setter = NULL;
    original_set_render_camera = NULL;
    original_temporary_camera_state_update = NULL;
    original_enter_lead_pop = NULL;
    original_exit_lead_move = NULL;
    formation_pop_members = NULL;
    set_mode_full_party = NULL;
    set_mode_lead_only = NULL;
    show_party_members = NULL;
    hide_party_members = NULL;
    trace_module = NULL;
    last_world = NULL;
    current_world_confirmed = FALSE;
    current_world_name[0] = '\0';
    current_temporary_name[0] = '\0';
    temporary_position_samples = 0u;
    temporary_camera_samples = 0u;
    ZeroMemory(arrival_contexts, sizeof(arrival_contexts));
    arrival_capture_context = NULL;
    arrival_capture_armed = FALSE;
    arrival_reapply_pending = FALSE;
    arrival_reapply_after = 0u;
    arrival_reapply_deadline = 0u;
    arrival_reapply_attempts = 0u;
    camera_trace_enabled = FALSE;
    party_atomic_transitions_enabled = FALSE;
    transition_vote_enabled = FALSE;
    transition_vote_visibility_quarantined = FALSE;
    ZeroMemory(&transition_vote_visibility_quarantine,
        sizeof(transition_vote_visibility_quarantine));
    SudekiMpInteractionProvenanceInvalidate();
    zone_source_generation = 0u;
    set_zone_now_depth = 0u;
    clear_party_presentation_lease();
    clear_party_presentation_hide_candidate();
    party_presentation_hide_depth = 0;
    party_presentation_hide_sequence = 0u;
    clear_party_atomic_transition();
    release_active_temporary_resource();
}
