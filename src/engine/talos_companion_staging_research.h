#ifndef SUDEKIMP_TALOS_COMPANION_STAGING_RESEARCH_H
#define SUDEKIMP_TALOS_COMPANION_STAGING_RESEARCH_H

#include <stdint.h>

/* This research-only protocol deliberately does not include or expose the
 * production Talos encounter/staging types. All identities are opaque scalar
 * observations. They confer no lifetime, carry, or mutation authority. */
enum {
    SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT = 4u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_TAL = 0u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_AILISH = 1u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_BUKI = 2u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_ELCO = 3u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_MEMBER_NONE = 0xffu,
    SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_NATIVE_AI = 1u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_CONTROL_HUMAN = 2u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_HUD_LABEL_MAX_LENGTH = 64u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_ELCO_ARBITER_ARMED_MASK = 0x02u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_PLAYER_RVA = 0x00023390u,
    SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_PLAYER_RVA = 0x00023230u
};

typedef enum SudekiMpTalosCompanionStagingResearchState {
    SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED = 0,
    SUDEKIMP_TALOS_STAGING_RESEARCH_IDLE,
    SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT,
    SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_ISSUED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_DETACHED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_ISSUED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS,
    SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_CONSUMED
} SudekiMpTalosCompanionStagingResearchState;

typedef enum SudekiMpTalosCompanionStagingResearchResult {
    SUDEKIMP_TALOS_STAGING_RESEARCH_DISABLED_RESULT = 0,
    SUDEKIMP_TALOS_STAGING_RESEARCH_NO_CHANGE,
    SUDEKIMP_TALOS_STAGING_RESEARCH_PREFLIGHT_ACCEPTED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_REMOVE_AUTHORIZED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_DETACH_ACCEPTED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_ADD_AUTHORIZED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_RESTORE_ACCEPTED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_SUCCESS_RESULT,
    SUDEKIMP_TALOS_STAGING_RESEARCH_SAFE_CONSUMED,
    SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_INVALID,
    SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_STATE,
    SUDEKIMP_TALOS_STAGING_RESEARCH_REJECTED_REPLAY,
    SUDEKIMP_TALOS_STAGING_RESEARCH_QUARANTINE
} SudekiMpTalosCompanionStagingResearchResult;

typedef struct SudekiMpTalosStagingResearchHeroEvidence {
    uint64_t wrapper_token;
    uint64_t actor_token;
    uint64_t control_component_token;
    uint64_t control_owner_actor_token;
    uint64_t formation_backpointer_token;
    uint64_t gizmo_token;
    uint64_t stat_display_token;
    uint64_t gizmo_label_hash;
    uint32_t gizmo_state;
    uint32_t gizmo_flags_masked;
    uint32_t gizmo_label_length;
    uint32_t current_hp_bits;
    uint32_t fill_cache_primary_bits;
    uint32_t fill_cache_secondary_bits;
    uint8_t native_ai_enabled;
    uint8_t human_control_owned;
    uint8_t override_active;
    uint8_t control_mode;
} SudekiMpTalosStagingResearchHeroEvidence;

/* This is observational evidence only. The three authority fields must be
 * explicitly false in every sample. The exact-closure booleans grant only a
 * one-callback, exclusive, no-yield research proof; they never grant actor
 * lifetime, carry, or production authority. modal_active and
 * transition_active are retained diagnostics and do not decide admission.
 * in_combat and group_armed are two interpretations of the same sampled
 * group+0xd4 byte: they must be equal, and the ordinary-world value is zero.
 * reload_required must be false before the remove and true in every sample
 * after the first reported native remove. group_order and formation_order are
 * independent byte orders. */
typedef struct SudekiMpTalosStagingResearchSnapshot {
    uint64_t observation_serial;
    uint64_t process_token;
    uint64_t native_thread_token;
    uint64_t source_token;
    uint64_t world_token;
    uint64_t group_token;
    uint64_t formation_owner_token;
    uint64_t formation_token;
    uint64_t controller_token;
    uint64_t controller_callback_token;
    uint64_t transaction_token;
    uint64_t listener_storage_token;
    uint64_t listener_token;
    uint64_t ui_controller_token;
    uint64_t hud_owner_token;
    uint64_t ui_scene_token;
    uint64_t elco_arbiter_token;
    uint64_t front_actor_token;
    uint64_t camera_token;
    uint64_t current_render_camera_token;
    uint64_t render_state_token;
    uint64_t scene_manager_token;
    uint64_t scene_renderer_token;
    SudekiMpTalosStagingResearchHeroEvidence
        hero[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    uint8_t group_order[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    uint8_t formation_order[SUDEKIMP_TALOS_STAGING_RESEARCH_HERO_COUNT];
    uint8_t group_count;
    uint8_t formation_count;
    uint8_t exact_executable_hash;
    uint8_t exact_sol_hash;
    uint8_t foreground;
    uint8_t all_pending_loaded;
    uint8_t camera_scene_consistent;
    uint8_t controller_callback_exact;
    uint8_t game_thread_exact;
    uint8_t transaction_exclusive;
    uint8_t no_yield_window_exact;
    uint8_t listener_callback_closure_exact;
    uint8_t ui_hud_closure_exact;
    uint8_t hero_hud_state_converged;
    uint8_t elco_arbiter_safe;
    uint8_t in_combat;
    uint8_t async_active;
    uint8_t tsa_active;
    uint8_t paused;
    uint8_t transition_active;
    uint8_t modal_active;
    uint8_t group_armed;
    uint8_t production_authority;
    uint8_t carry_authority;
    uint8_t actor_lifetime_authority;
    uint8_t reload_required;
    uint32_t listener_count;
    uint32_t elco_arbiter_state_58;
    uint32_t elco_arbiter_flags_60_masked;
    uint32_t controller_current_mode;
    uint32_t controller_requested_mode;
} SudekiMpTalosStagingResearchSnapshot;

/* A ticket names one symbolic public-member operation. It contains no native
 * address, callable pointer, object pointer, or adapter callback. */
typedef struct SudekiMpTalosStagingResearchTicket {
    uint64_t attempt_serial;
    uint64_t authorization_serial;
    uint64_t authorized_observation_serial;
    uint64_t process_token;
    uint64_t native_thread_token;
    uint64_t source_token;
    uint64_t world_token;
    uint64_t group_token;
    uint64_t wrapper_token;
    uint64_t actor_token;
    uint32_t native_function_rva;
    uint8_t hero;
    uint8_t reserved[3];
} SudekiMpTalosStagingResearchTicket;

typedef struct SudekiMpTalosCompanionStagingResearch {
    SudekiMpTalosCompanionStagingResearchState state;
    SudekiMpTalosStagingResearchSnapshot original;
    SudekiMpTalosStagingResearchSnapshot detached;
    SudekiMpTalosStagingResearchSnapshot restored;
    SudekiMpTalosStagingResearchSnapshot stable;
    SudekiMpTalosStagingResearchTicket remove_ticket;
    SudekiMpTalosStagingResearchTicket add_ticket;
    uint64_t attempt_serial;
    uint64_t next_authorization_serial;
    uint64_t consumed_remove_authorization_serial;
    uint64_t consumed_add_authorization_serial;
    uint8_t enabled;
    uint8_t one_attempt_consumed;
    uint8_t production_authority;
    uint8_t carry_authority;
    uint8_t actor_lifetime_authority;
    uint8_t reload_required;
    uint8_t reserved[2];
} SudekiMpTalosCompanionStagingResearch;

/* Observation serials reserve zero and use the unsigned half-range ordering
 * rule, so MAX -> 1 is forward. The one attempt token is nonzero and exact;
 * authorization serials never wrap. */
/* Initialize one process-lifetime coordinator exactly once. There is no reset
 * API: CONSUMED, QUARANTINED, and SUCCESS are terminal for that process. */
void SudekiMpTalosCompanionStagingResearchInitialize(
    SudekiMpTalosCompanionStagingResearch *research
);

void SudekiMpTalosCompanionStagingResearchConfigure(
    SudekiMpTalosCompanionStagingResearch *research,
    int enabled
);

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchBegin(
    SudekiMpTalosCompanionStagingResearch *research,
    uint64_t attempt_serial,
    const SudekiMpTalosStagingResearchSnapshot *preflight
);

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchClaimRemove(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *immediate,
    SudekiMpTalosStagingResearchTicket *ticket
);

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchFinishRemove(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchTicket *ticket,
    const SudekiMpTalosStagingResearchSnapshot *completion,
    unsigned int native_call_count
);

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchClaimAdd(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *immediate,
    SudekiMpTalosStagingResearchTicket *ticket
);

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchFinishAdd(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchTicket *ticket,
    const SudekiMpTalosStagingResearchSnapshot *completion,
    unsigned int native_call_count
);

SudekiMpTalosCompanionStagingResearchResult
SudekiMpTalosCompanionStagingResearchObserveStability(
    SudekiMpTalosCompanionStagingResearch *research,
    const SudekiMpTalosStagingResearchSnapshot *observation
);

#endif
