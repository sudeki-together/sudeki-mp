#include "hooks/lan_arena_client_replica.h"
#include "hooks/lan_arena_hit_feedback.h"

#include "cleanroom/engine.h"
#include "engine/arbiter_combat_input.h"
#include "engine/buki_replica_native.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/lan_arena_owner_view.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "hooks/lan_arena_runtime.h"
#include "hooks/lan_arena_client_skill_handoff.h"
#include "hooks/lan_arena_spirit_audio.h"
#include "hooks/lan_arena_spirit_vfx.h"
#include "network/lan_arena_replica.h"
#include "network/lan_arena_session.h"
#include "network/lan_arena_shared_simulation.h"
#include "network/lan_arena_tal_combo_graph.h"
#include "network/lan_party_motion.h"
#include "hooks/lan_party_snapshot.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static uint8_t seat_host_type(void) {
    uint8_t host_type = 0u, client_type = 0u;
    return SudekiMpLanArenaSeatActorTypes(&host_type, &client_type)
        ? host_type : SUDEKIMP_LAN_ARENA_TAL_TYPE;
}
static uint8_t seat_client_type(void) {
    uint8_t host_type = 0u, client_type = 0u;
    return SudekiMpLanArenaSeatActorTypes(&host_type, &client_type)
        ? client_type : SUDEKIMP_LAN_ARENA_AILISH_TYPE;
}
static SudekiMpCleanroomActor seat_host_actor(void) {
    SudekiMpCleanroomActor actor;
    return SudekiMpCleanroomActorFromType(seat_host_type(), &actor)
        ? actor : SUDEKIMP_CLEANROOM_TAL;
}
static SudekiMpCleanroomActor seat_client_actor(void) {
    SudekiMpCleanroomActor actor;
    return SudekiMpCleanroomActorFromType(seat_client_type(), &actor)
        ? actor : SUDEKIMP_CLEANROOM_AILISH;
}

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
typedef void (__attribute__((thiscall)) *ApplyDamageFunction)(
    void *combat, void *damage_structure);
typedef BOOL (__attribute__((thiscall)) *CameraManagerSetRenderCameraFunction)(
    void *manager, const char *name);
typedef void (__attribute__((thiscall)) *GameSpeedSetModeFunction)(
    void *game_speed, int mode);
typedef unsigned char (__attribute__((regparm(1)))
    *AilishRangedPresentationRefreshFunction)(void *component);
typedef void (__attribute__((thiscall)) *WeaponSetVisibleFunction)(
    void *weapon, int visible);

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
    void *weapon_attempt_actor;
    uint8_t animation_state;
    uint8_t combat_state;
    uint8_t action_variant;
    uint8_t weapon_attempt_slot;
    uint16_t action_sequence;
    DWORD last_early_apply_at;
    SudekiMpLanArenaLocomotion locomotion;
    BOOL action_restart_pending;
    BOOL combat_mode;
    BOOL valid;
} LanArenaPresentationLease;

typedef struct LanArenaFirstPersonLease {
    void *character;
    void *component;
    void *renderer;
    uint16_t action_sequence;
    BOOL weak_attack;
    uint8_t weapon_slot;
    BOOL weapon_swap;
    BOOL valid;
} LanArenaFirstPersonLease;

/* The fixed-four Ailish owner gets a separate renderer-only first-person
 * lease. It is deliberately not the legacy two-seat native ranged lease:
 * this record is keyed to the party generation and never owns CArbiter. */
typedef struct PartyAilishFirstPersonLease {
    void *character;
    void *component;
    void *wrapper;
    void *renderer;
    uint64_t token;
    uint32_t generation;
    uint16_t action_sequence;
    uint16_t reload_sequence;
    uint8_t action_variant;
    uint8_t weapon_slot;
    uint8_t seat;
    BOOL weapon_swap;
    BOOL reload_active;
    BOOL reload_complete;
    BOOL valid;
} PartyAilishFirstPersonLease;

typedef struct LanArenaNativeRangedLease {
    uint8_t *character;
    uint8_t *component;
    uint8_t *combat;
    uint8_t *arbiter;
    void *renderer;
    uint16_t sequence;
    BOOL active;
    BOOL weapon_shot;
    BOOL weapon_fired;
    uint8_t weapon_item;
} LanArenaNativeRangedLease;

typedef struct LanArenaTalNativePresentationLease {
    void *character;
    void *arbiter;
    void *renderer;
    uint16_t submitted_sequence;
    uint8_t submitted_variant;
    int expected_selector;
    DWORD submitted_at_ms;
    BOOL expected_selector_seen;
    BOOL timeout_logged;
    BOOL block_interrupt_attempted;
    BOOL active;
} LanArenaTalNativePresentationLease;

typedef struct LanArenaNativeSkillPresentationLease {
    void *character;
    void *skill;
    uint16_t seen_sequence;
    uint8_t slot;
    BOOL native_started;
    BOOL active_seen;
    BOOL completion_logged;
    BOOL ranged_prime_requested;
    SudekiMpLanArenaClientSkillRetryGate retry_gate;
    BOOL retry_exhaustion_logged;
    BOOL host_presentation_logged;
    int32_t host_presentation_selector;
    uint8_t host_presentation_state;
    BOOL drain_pending;
    BOOL drain_logged;
    uint16_t pending_sequence;
    uint8_t pending_kind;
} LanArenaNativeSkillPresentationLease;

typedef struct LanArenaClientViewLease {
    SudekiMpLanArenaOwnerViewLease owner_view;
    uint16_t skill_sequence;
} LanArenaClientViewLease;


typedef struct LanArenaCombatTransitionActorLease {
    void *character;
    void *position;
    void *attached_wrapper;
    void *attached_renderer;
    void *renderer;
    void *component;
    void *first_person_wrapper;
    void *first_person_renderer;
    uint64_t session_token;
    uint32_t transition_generation;
    uint32_t actor_generation;
    BOOL ready;
} LanArenaCombatTransitionActorLease;

typedef struct LanArenaAilishModelWitness {
    uint8_t *position;
    uint8_t *attached_wrapper;
    uint8_t *first_person_wrapper;
    uint8_t *saved_world_wrapper;
    void *attached_renderer;
    void *first_person_renderer;
    void *saved_world_renderer;
    BOOL first_person;
    BOOL fallback_world;
} LanArenaAilishModelWitness;

typedef struct LanArenaAilishWeaponReattachWitness {
    uint8_t *attached_wrapper;
    void *attached_renderer;
    uint8_t *model_interface;
    uint8_t *model_interface_vtable;
    void *matrix_method;
    void *locator_method;
    uint8_t *primary_wrapper;
    uint8_t *primary_render_object;
} LanArenaAilishWeaponReattachWitness;

typedef struct LanArenaAilishWeaponVisibilityWitness {
    uint8_t *primary_wrapper;
    uint8_t *primary_render_object;
    uint32_t primary_render_flags;
    uint8_t *primary_callback;
    uint8_t *primary_callback_vtable;
    void *primary_callback_method;
    uint8_t *secondary_wrapper;
    uint8_t *secondary_render_object;
    uint32_t secondary_render_flags;
    uint8_t *secondary_callback;
    uint8_t *secondary_callback_vtable;
    void *secondary_callback_method;
} LanArenaAilishWeaponVisibilityWitness;

typedef enum LanArenaAilishModelAttachmentState {
    LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN = 0,
    LAN_ARENA_AILISH_MODEL_ATTACHMENT_DESIRED = 1,
    LAN_ARENA_AILISH_MODEL_ATTACHMENT_OPPOSITE = 2
} LanArenaAilishModelAttachmentState;

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
    /* Host-actor animation identity by SEMANTIC animation id (ANIMID_*).
     * The renderer setters above take per-character clip selectors; these
     * three operate on the model's animation table (model+0xDC) and its
     * per-channel state array (model+0xF8, byte +2 = current ANIMID) and are
     * character-independent. See docs / skill reference for the decompiles. */
    RVA_MODEL_GET_CURRENT_ANIM = 0x0003af10u,
    RVA_MODEL_RESOLVE_ANIM = 0x000e42f0u,
    RVA_MODEL_PLAY_ANIM = 0x000e4f50u,
    RVA_ARBITER_COMBAT_INPUT = 0x000db0e0u,
    RVA_APPLY_DAMAGE = 0x000d21d0u,
    RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_GAME_SPEED_SET_MODE = 0x00207560u,
    RVA_FIXED_ALTERNATE_SPEED = 0x002c4018u,
    RVA_AILISH_RANGED_PRESENTATION_REFRESH = 0x001888f0u,
    RVA_AILISH_RANGED_WEAPON_REATTACH_CALL = 0x00188a76u,
    RVA_RANGED_WEAPON_REATTACH = 0x000d8280u,
    RVA_WEAPON_SET_VISIBLE = 0x000d7e30u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_SCENE_MANAGER_GLOBAL = 0x00408d58u,
    CHARACTER_POSITION_OFFSET = 0x44u,
    CHARACTER_ARBITER_OFFSET = 0x90u,
    CHARACTER_WEAPON_OFFSET = 0xc0u,
    ARBITER_CHARACTER_OFFSET = 0x10u,
    AILISH_RANGED_COMPONENT_OFFSET = 0x134u,
    /* +0x130 is the uniform character->animation-model link (melee AND ranged);
     * 0x134 is the ranged-only world/presentation component. */
    CHARACTER_ANIMATION_MODEL_OFFSET = 0x130u,
    AILISH_ANIMATION_TABLE_OFFSET = 0xdcu,
    AILISH_ANIMATION_BANK_OFFSET = 0x133u,
    AILISH_FIRST_PERSON_WRAPPER_OFFSET = 0x160u,
    AILISH_WORLD_WRAPPER_OFFSET = 0x164u,
    AILISH_FIRST_PERSON_ARBITER_FLAG = 0x00400000u,
    WEAPON_OWNER_OFFSET = 0x10u,
    WEAPON_PRIMARY_POSITION_OFFSET = 0x40u,
    WEAPON_PRIMARY_RENDER_OBJECT_OFFSET = 0xccu,
    WEAPON_PRIMARY_PARENT_OFFSET = 0xd4u,
    WEAPON_PRIMARY_LOCATOR_OFFSET = 0xecu,
    WEAPON_PRIMARY_WRAPPER_OFFSET = 0xf4u,
    WEAPON_SECONDARY_WRAPPER_OFFSET = 0x204u,
    WEAPON_CURRENT_ITEM_OFFSET = 0x268u,
    WEAPON_ACTIVE_MODEL_OFFSET = 0x3acu,
    WEAPON_FLAGS_OFFSET = 0x3b8u,
    WEAPON_VISIBLE_FLAG = 0x02u,
    RENDER_OBJECT_HIDDEN_FLAG = 0x04u,
    RENDER_OBJECT_VISIBILITY_CALLBACK_FLAG = 0x04000000u,
    WRAPPER_RENDER_OBJECT_OFFSET = 0x08u,
    WRAPPER_MODEL_INTERFACE_OFFSET = 0x0cu,
    WRAPPER_RENDERER_OFFSET = 0x10u,
    RENDER_OBJECT_CALLBACK_OFFSET = 0x14u,
    RENDER_OBJECT_FLAGS_OFFSET = 0x34u,
    VISIBILITY_CALLBACK_METHOD_OFFSET = 0x14u,
    POSITION_ATTACHED_WRAPPER_OFFSET = 0xb4u,
    /* CPosition stores its intrusive attachment link as position + 4, not
     * as the containing CPosition address. FUN_00511960 installs this exact
     * biased pointer in a child position's +0x94 parent slot. */
    POSITION_PARENT_LINK_BIAS = 0x04u,
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
    /* Elco (ranged) resolves her own renderer ids, distinct from Ailish:
     * world idle 1, world move 5/6 (rate 34.33/28.61), idle variants 2/3,
     * combat idle 22, combat move 24/25 (rate 34.33/28.61). Captured live via
     * move_selector_diag (semantic-moving correlation). */
    ELCO_WORLD_MOVE_PRIMARY_SELECTOR = 5,
    ELCO_WORLD_MOVE_SECONDARY_SELECTOR = 6,
    /* Buki (melee) shares Tal's move RATES but resolves her own ids:
     * world idle 1 (variants 4/3/2), world move 6/7, combat idle 20,
     * combat move 23/24 (rates match Tal 37.17/30.98). */
    BUKI_WORLD_IDLE_SELECTOR = 1,
    BUKI_WORLD_IDLE_VARIANT_ONE_SELECTOR = 4,
    BUKI_WORLD_IDLE_VARIANT_TWO_SELECTOR = 3,
    BUKI_WORLD_MOVE_PRIMARY_SELECTOR = 6,
    BUKI_WORLD_MOVE_SECONDARY_SELECTOR = 7,
    /* Exact host captures from the supported cleanroom combat transition.
     * These remain process-local renderer IDs; the wire protocol carries
     * only semantic idle/moving/action state plus the verified combat bit. */
    TAL_COMBAT_ENTRY_SELECTOR = 3,
    TAL_COMBAT_IDLE_SELECTOR = 17,
    TAL_COMBAT_MOVE_PRIMARY_SELECTOR = 36,
    TAL_COMBAT_MOVE_SECONDARY_SELECTOR = 32,
    BUKI_COMBAT_IDLE_SELECTOR = 20,
    BUKI_COMBAT_MOVE_PRIMARY_SELECTOR = 23,
    BUKI_COMBAT_MOVE_SECONDARY_SELECTOR = 24,
    AILISH_COMBAT_ENTRY_SELECTOR = 12,
    AILISH_COMBAT_IDLE_SELECTOR = 20,
    AILISH_COMBAT_MOVE_PRIMARY_SELECTOR = 22,
    AILISH_COMBAT_MOVE_SECONDARY_SELECTOR = 23,
    AILISH_COMBAT_WEAK_SELECTOR = 59,
    ELCO_COMBAT_IDLE_SELECTOR = 22,
    ELCO_COMBAT_MOVE_PRIMARY_SELECTOR = 24,
    ELCO_COMBAT_MOVE_SECONDARY_SELECTOR = 25,
    /* Exploration-only native Ailish action captured before LAN combat. */
    AILISH_WORLD_WEAK_SELECTOR = 55,
    ACTION_PHASE_TIME_TOLERANCE_MILLI = 10
};

static int client_world_move_selector(void) {
    return seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        ELCO_WORLD_MOVE_PRIMARY_SELECTOR : AILISH_WORLD_MOVE_PRIMARY_SELECTOR;
}

static const float TAL_WORLD_MOVE_PRIMARY_RATE = 37.17093f;
static const float TAL_WORLD_MOVE_SECONDARY_RATE = 30.97577f;
static const float AILISH_WORLD_MOVE_PRIMARY_RATE = 41.22882f;
static const float AILISH_WORLD_MOVE_SECONDARY_RATE = 30.92161f;
/* Elco's world/combat locomotion rate (single canonical rate for her two-channel
 * move clips), captured live via move_selector_diag. Buki shares Tal's rates. */
static const float ELCO_MOVE_PRIMARY_RATE = 34.33f;
static const float ELCO_MOVE_SECONDARY_RATE = 28.608f;

/* Per-seat selector/rate resolvers. Buki (melee) shares Tal's move RATES but
 * resolves different selector ids; Elco (ranged) resolves her own ids AND rates
 * (her selector space is shifted from Ailish's — Elco combat idle 22 collides
 * with Ailish combat move 22). Everything in the presentation writer keys off
 * these helpers so the four-actor pair no longer inherits Tal/Ailish ids. */
static int host_world_idle_selector(void) {
    return seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        BUKI_WORLD_IDLE_SELECTOR : TAL_WORLD_IDLE_SELECTOR;
}
static int host_world_move_primary_selector(void) {
    return seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        BUKI_WORLD_MOVE_PRIMARY_SELECTOR : TAL_WORLD_MOVE_PRIMARY_SELECTOR;
}
static int host_world_move_secondary_selector(void) {
    return seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        BUKI_WORLD_MOVE_SECONDARY_SELECTOR : TAL_WORLD_MOVE_SECONDARY_SELECTOR;
}
static int host_combat_idle_selector(void) {
    return seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        BUKI_COMBAT_IDLE_SELECTOR : TAL_COMBAT_IDLE_SELECTOR;
}
static int host_combat_move_primary_selector(void) {
    return seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        BUKI_COMBAT_MOVE_PRIMARY_SELECTOR : TAL_COMBAT_MOVE_PRIMARY_SELECTOR;
}
static int host_combat_move_secondary_selector(void) {
    return seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        BUKI_COMBAT_MOVE_SECONDARY_SELECTOR : TAL_COMBAT_MOVE_SECONDARY_SELECTOR;
}
static int client_world_move_secondary_selector(void) {
    return seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        ELCO_WORLD_MOVE_SECONDARY_SELECTOR : AILISH_WORLD_MOVE_SECONDARY_SELECTOR;
}
static float client_world_move_rate(void) {
    return seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        ELCO_MOVE_PRIMARY_RATE : AILISH_WORLD_MOVE_PRIMARY_RATE;
}
static float client_world_move_secondary_rate(void) {
    return seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        ELCO_MOVE_SECONDARY_RATE : AILISH_WORLD_MOVE_SECONDARY_RATE;
}
int SudekiMpLanArenaClientRangedCombatSelector(
    uint8_t actor_type, uint8_t animation_id
) {
    return SudekiMpLanArenaRangedCombatSelector(actor_type, animation_id);
}

static int client_combat_idle_selector(void) {
    return SudekiMpLanArenaClientRangedCombatSelector(seat_client_type(), 0x02u);
}
static int client_combat_move_primary_selector(void) {
    return SudekiMpLanArenaClientRangedCombatSelector(seat_client_type(), 0x06u);
}
static int client_combat_move_secondary_selector(void) {
    return SudekiMpLanArenaClientRangedCombatSelector(seat_client_type(), 0x07u);
}
static int client_combat_fire_selector(void) {
    return SudekiMpLanArenaRangedCombatSelector(seat_client_type(), 0x85u);
}
static float client_combat_move_rate(void) {
    return seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        ELCO_MOVE_PRIMARY_RATE : AILISH_WORLD_MOVE_PRIMARY_RATE;
}
static float client_combat_move_secondary_rate(void) {
    return seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE ?
        ELCO_MOVE_SECONDARY_RATE : AILISH_WORLD_MOVE_SECONDARY_RATE;
}

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
static const uint8_t expected_arbiter_combat_input_entry[] = {
    0x55u, 0x8bu, 0x6cu, 0x24u, 0x08u, 0x56u, 0x57u,
    0x8bu, 0xf8u, 0x8bu, 0xf1u
};
static const uint8_t expected_apply_damage_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t expected_camera_manager_set_render_camera_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t expected_game_speed_set_mode_entry[] = {
    0x8bu, 0x44u, 0x24u, 0x04u, 0x89u, 0x41u, 0x24u
};
static const uint8_t expected_fixed_alternate_speed[] = {
    0x29u, 0x5cu, 0x8fu, 0x3du
};
static const uint8_t expected_ailish_ranged_presentation_refresh_entry[] = {
    0x83u, 0xecu, 0x08u, 0x55u, 0x56u, 0x8bu, 0xf0u, 0x8bu,
    0x46u, 0x10u, 0x8bu, 0xa8u, 0x90u, 0x00u, 0x00u, 0x00u
};
static const uint8_t expected_ranged_weapon_reattach_entry[] = {
    0x53u, 0x55u, 0x8bu, 0x6cu, 0x24u, 0x0cu, 0x56u, 0x57u,
    0x85u, 0xc0u, 0x74u, 0x6au
};
static const uint8_t expected_weapon_set_visible_entry[] = {
    0x8au, 0x44u, 0x24u, 0x04u, 0x8bu, 0x91u, 0x04u, 0x02u,
    0x00u, 0x00u, 0x02u, 0xc0u, 0x32u, 0x81u, 0xb8u, 0x03u,
    0x00u, 0x00u
};

static PositionSetterFunction set_position;
static PositionWorldMatrixFunction position_world_matrix;
static AilishRangedPresentationRefreshFunction
    ailish_ranged_presentation_refresh;
static WeaponSetVisibleFunction weapon_set_visible;
static void *ranged_weapon_reattach;
static void *set_forward;
/* Character-independent ANIMID path: resolve/play on the model's animation
 * table. Resolved at install; called through naked wrappers because both use
 * a custom "this in a register" convention (resolve: EAX; play: EDI). Written
 * only in C and read only by the naked asm, so mark them used to keep the
 * linker from dead-stripping them. */
static void *model_resolve_anim __attribute__((used));
static void *model_play_anim __attribute__((used));
static uint8_t *game_base;
static SudekiMpLanArenaReplica replica;
static SudekiMpLanArenaReplicaRenderClock replica_render_clock;
static SudekiMpLanArenaSharedSimulation replica_simulation;
static SudekiMpLanArenaSpiritAudioCursor spirit_audio_cursor;
static BOOL spirit_audio_replay_failure_logged;
typedef struct SpiritViewFrameLease {
    SudekiMpLanArenaOwnerViewLease view;
    uint8_t *camera_manager;
    uint8_t *borrowed_camera;
    uint8_t *borrowed_state;
    BOOL swapped;
    BOOL in_place; /* Private-cast mode never borrows another native camera. */
    BOOL preparation_only; /* Body participates in native animation/culling before draw. */
    uint8_t *weapon;
    uint8_t *weapon_wrappers[2];
    uint8_t *weapon_render[2];
    uint32_t weapon_hidden[2];
    void *character;
    uint8_t *component;
    uint8_t *position;
    uint8_t *world_wrapper;
    uint8_t *arms_wrapper;
    uint8_t *body;
    uint8_t *arms;
    float saved_camera[19];
    uint32_t body_hidden;
    uint32_t arms_hidden;
} SpiritViewFrameLease;
static SpiritViewFrameLease spirit_view_frame;
/* An unverified SetWeapon return is not permission to repeat a native model
 * mutation each render frame. Retry only for a different requested slot or
 * native owner; confirmed equipment still follows ordinary readiness gates. */
static void *client_weapon_attempt_actor;
static void *client_weapon_attempt_component;
static uint8_t client_weapon_attempt_slot;
static BOOL spirit_vfx_replay_failure_logged;
static BOOL spirit_vfx_cache_release_failure_logged;
static uint64_t spirit_vfx_session_token;
static void *spirit_vfx_tal_actor;
static uint32_t spirit_vfx_tal_generation;
static BOOL spirit_vfx_generation_fenced;
static uint16_t spirit_vfx_generation_skill_floor;
static void *status_vfx_ailish_actor;
static uint32_t status_vfx_instance_floor;
static uint32_t status_vfx_newest_instance;
static LanArenaPresentationLease presentation_leases[2];
static LanArenaPresentationLease party_presentation_leases[4];
static SudekiMpLanPartySession *party_replica_session;
static SudekiMpLanPartyLease party_presentation_generation;
static SudekiMpLanPartyFrame party_present_frame;
static SudekiMpLanPartyLease party_present_lease;
static DWORD party_present_at;
static BOOL party_present_valid;
static BOOL party_combat_mode_lease_valid,party_combat_mode_original;
static uint64_t party_combat_mode_token;
static uint32_t party_combat_mode_generation;
static PartyAilishFirstPersonLease party_ailish_first_person_lease;
static BOOL party_retire_ailish_first_person(
    const SudekiMpControlUpdateDispatchWitness *w);
static LanArenaFirstPersonLease ailish_first_person_lease;
static LanArenaNativeRangedLease ailish_native_ranged_lease;
static void *elco_weapon_cursor_owner;
static uint64_t elco_weapon_cursor_session;
static uint16_t elco_weapon_cursor;
static DWORD weapon_snapshot_received_at;
static LanArenaTalNativePresentationLease tal_native_presentation_lease;
static LanArenaNativeSkillPresentationLease native_skill_leases[2];
static LanArenaClientViewLease remote_tal_skill_view_lease;
static SudekiMpInlineHook client_apply_damage_hook;
static SudekiMpInlineHook client_skill_camera_hook;
static SudekiMpInlineHook client_skill_speed_hook;
static ApplyDamageFunction original_apply_damage;
static volatile LONG client_skill_activation_depth;
static volatile LONG client_spirit_vfx_call_depth;
static int client_skill_activation_actor_index = -1;
static struct {
    void *actor;
    uint64_t session;
    uint16_t sequence;
    uint8_t slot;
    DWORD thread;
} client_skill_replay_admission;
static BOOL client_remote_tal_skill_input_isolation_active;
static uint32_t client_skill_original_alternate_speed_bits;
static BOOL client_skill_speed_override_active;
static BOOL client_skill_camera_suppression_logged;
static int client_skill_speed_trace_state = -1;
static int remote_tal_skill_view_trace_state = -1;
static BOOL client_damage_block_logged;
static BOOL client_replica_reset_pending;
static BOOL client_replica_containment_pinned;
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
static uint64_t client_combat_transition_session_token;
static uint32_t client_combat_transition_generation;
static DWORD client_combat_transition_started_at;
static BOOL client_combat_transition_refresh_attempted;
static unsigned int client_ailish_ranged_refresh_attempt_count;
static DWORD client_ailish_ranged_refresh_last_attempt_at;
static BOOL client_ailish_ranged_refresh_exhaustion_logged;
static LanArenaCombatTransitionActorLease
    client_combat_transition_actor_leases[2];
static void *client_remote_tal_lease_actor;
static uint32_t client_remote_tal_lease_generation;
static int client_combat_transition_trace_state = -1;
static const char *client_ailish_combat_graph_failure;

static BOOL client_ailish_combat_graph_ready(uint8_t *expected_character);
static BOOL client_ailish_visible_combat_ready(
    uint8_t *expected_character);
static BOOL client_session_authenticated(void);
static BOOL drain_tal_native_action_lease(void);
static BOOL actor_presentation_renderer(
    uint8_t *character,
    unsigned int actor_index,
    void **renderer_result,
    uint8_t **ailish_component_result
);

static int replay_client_spirit_audio(
    void *context,
    SudekiMpLanArenaSpiritAudioCue cue
) {
    (void)context;
    return SudekiMpLanArenaSpiritAudioReplayLocalCue(
        (HMODULE)game_base, cue) ? 1 : 0;
}

static void reset_client_spirit_vfx_replay(BOOL reset_binding) {
    /* Native clone/cache entry can synchronously invalidate a frame. Keep
     * its actor/session binding until the outer call returns and retires the
     * exact native leases. Reset never clears the backend's dependencies. */
    if (InterlockedCompareExchange(
            &client_spirit_vfx_call_depth, 0, 0) > 0) return;
    if (reset_binding && !spirit_vfx_cache_release_failure_logged) {
        spirit_vfx_session_token = 0u;
        spirit_vfx_tal_actor = NULL;
        spirit_vfx_tal_generation = 0u;
        spirit_vfx_generation_fenced = FALSE;
        spirit_vfx_generation_skill_floor = 0u;
        status_vfx_ailish_actor = NULL;
        status_vfx_instance_floor = 0u;
        status_vfx_newest_instance = 0u;
    }
    spirit_vfx_replay_failure_logged = FALSE;
}

static BOOL release_client_spirit_vfx_cache(const char *reason) {
    HMODULE module;
    BOOL released;
    DWORD error;

    if (game_base == NULL) {
        spirit_vfx_cache_release_failure_logged = FALSE;
        return TRUE;
    }
    if (InterlockedCompareExchange(
            &client_spirit_vfx_call_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    module = (HMODULE)game_base;
    InterlockedIncrement(&client_spirit_vfx_call_depth);
    released = SudekiMpLanArenaSpiritVfxResetVisuals(module);
    error = released ? ERROR_SUCCESS : GetLastError();
    /* The old opening-only adapter may still own a prewarm lease during an
     * installation rollback. Never uncache anything while clones remain. */
    if (released) {
        released = SudekiMpLanArenaSpiritVfxReleaseTalInitiateCache(module);
        error = released ? ERROR_SUCCESS : GetLastError();
    }
    InterlockedDecrement(&client_spirit_vfx_call_depth);
    if (released) {
        spirit_vfx_cache_release_failure_logged = FALSE;
        return TRUE;
    }
    if (!spirit_vfx_cache_release_failure_logged) {
        spirit_vfx_cache_release_failure_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=spirit_vfx_cache state=release_rejected "
            "reason=%s win32_error=%lu "
            "policy=retain_same_generation_lease_and_retry_without_double_uncache\r\n",
            reason != NULL ? reason : "unspecified",
            (unsigned long)error);
    }
    SetLastError(error == ERROR_SUCCESS ? ERROR_INVALID_STATE : error);
    return FALSE;
}


BOOL SudekiMpLanArenaClientTalActionPresentation(
    uint8_t action_variant,
    int *selector,
    int *state
) {
    return SudekiMpLanArenaTalActionToNativePresentation(
        action_variant, selector, state);
}

BOOL SudekiMpLanArenaClientBukiActionPresentation(
    uint8_t action_variant,
    int *selector,
    int *state
) {
    return SudekiMpLanArenaBukiActionToNativePresentation(
        action_variant, selector, state);
}

BOOL SudekiMpLanArenaClientTalNativeCombatInput(
    uint8_t action_variant,
    int *weak,
    int *strong,
    int *sweep,
    int *block
) {
    uint8_t combat_state;
    if (weak == NULL || strong == NULL || sweep == NULL || block == NULL ||
        !SudekiMpLanArenaTalActionCombatState(
            action_variant, &combat_state)) return FALSE;
    *weak = combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK ? 1 : 0;
    *strong = combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK ? 1 : 0;
    *sweep = combat_state == SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK ? 1 : 0;
    *block = combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK ? 1 : 0;
    return *weak != 0 || *strong != 0 || *sweep != 0 || *block != 0;
}

BOOL SudekiMpLanArenaClientIdleVariantSelector(
    uint8_t actor_type,
    uint8_t animation_state,
    int *selector
) {
    BOOL ranged;
    if (selector == NULL ||
        (animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE &&
         animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO)) {
        return FALSE;
    }
    ranged = actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
        actor_type == SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    if (actor_type != SUDEKIMP_LAN_ARENA_TAL_TYPE &&
        actor_type != SUDEKIMP_LAN_ARENA_BUKI_TYPE && !ranged) {
        return FALSE;
    }
    if (!ranged) {
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

BOOL SudekiMpLanArenaClientActorTransitionReadinessRetained(
    BOOL previously_ready,
    BOOL identity_exact,
    BOOL currently_observed_ready
) {
    return identity_exact != FALSE &&
        (currently_observed_ready != FALSE || previously_ready != FALSE);
}

BOOL SudekiMpLanArenaClientTalLifecycleLeaseExact(
    void *current_actor,
    uint32_t current_generation,
    void *leased_actor,
    uint32_t leased_generation
) {
    return current_actor != NULL && current_generation != 0u &&
        current_actor == leased_actor &&
        current_generation == leased_generation;
}

void SudekiMpLanArenaClientReplicaSetRemoteTalLease(
    void *actor,
    uint32_t actor_generation
) {
    BOOL valid = actor != NULL && actor_generation != 0u;
    void *next_actor = valid ? actor : NULL;
    uint32_t next_generation = valid ? actor_generation : 0u;
    LanArenaCombatTransitionActorLease *transition_lease =
        &client_combat_transition_actor_leases[0];

    if (client_remote_tal_lease_actor == next_actor &&
        client_remote_tal_lease_generation == next_generation) {
        return;
    }
    if (transition_lease->ready) {
        SudekiMpLogWrite(
            "lan_arena_client_replica event=client_combat_presentation "
            "state=actor_readiness_revoked actor=Tal "
            "reason=runtime_actor_generation_changed "
            "policy=runtime_lifecycle_generation_plus_renderer_identity\r\n");
    }
    client_remote_tal_lease_actor = next_actor;
    client_remote_tal_lease_generation = next_generation;
    /* Preserve the visual binding: retained instances cannot migrate to a
     * replacement that happens to reuse the same native actor address. */
    reset_client_spirit_vfx_replay(FALSE);
    ZeroMemory(transition_lease, sizeof(*transition_lease));
    ZeroMemory(&presentation_leases[0], sizeof(presentation_leases[0]));
    /* A replacement may reuse every native address. Force the next
     * authenticated combat snapshot through the native handoff again. */
    client_combat_transition_session_token = 0u;
    client_combat_transition_pending = FALSE;
    client_combat_transition_trace_state = -1;
}

BOOL SudekiMpLanArenaClientTalTransitionSelectorReady(
    BOOL combat_target,
    int selector
) {
    /* The host seat defaults to Tal, whose armed/idle and sheathed/world
     * selectors are the proven Tal handoff points.  When the host seat is
     * Buki (melee, like Tal, but her own renderer ids), accept her captured
     * settled selectors instead: sheathed world idle 1 (fidget variants 4/6)
     * and armed combat idle 20.  Her walk/run and combat-move ids are not yet
     * mapped, so stay fail-closed to the proven idles rather than guessing. */
    if (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
        return combat_target ?
            (selector == BUKI_COMBAT_IDLE_SELECTOR ||
             selector == BUKI_COMBAT_MOVE_PRIMARY_SELECTOR) :
            (selector == BUKI_WORLD_IDLE_SELECTOR ||
             selector == BUKI_WORLD_IDLE_VARIANT_ONE_SELECTOR ||
             selector == BUKI_WORLD_IDLE_VARIANT_TWO_SELECTOR ||
             selector == BUKI_WORLD_MOVE_PRIMARY_SELECTOR);
    }
    return combat_target ?
        (selector == TAL_COMBAT_IDLE_SELECTOR ||
         selector == TAL_COMBAT_MOVE_PRIMARY_SELECTOR) :
        (selector == TAL_WORLD_IDLE_SELECTOR ||
         selector == TAL_WORLD_MOVE_PRIMARY_SELECTOR);
}

BOOL SudekiMpLanArenaClientCombatTransitionRefreshDue(
    BOOL combat_target,
    BOOL refresh_attempted,
    uint32_t elapsed_ms
) {
    return combat_target != FALSE && refresh_attempted == FALSE &&
        elapsed_ms >= 100u;
}

BOOL SudekiMpLanArenaClientAilishRangedRefreshDue(
    unsigned int attempt_count,
    uint32_t elapsed_since_attempt_ms
) {
    return attempt_count <
            SUDEKIMP_LAN_ARENA_CLIENT_AILISH_REFRESH_MAX_ATTEMPTS &&
        elapsed_since_attempt_ms >=
            SUDEKIMP_LAN_ARENA_CLIENT_AILISH_REFRESH_BACKOFF_MS;
}

BOOL SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
    float actual_phase,
    float authoritative_phase
) {
    const float tolerance =
        (float)ACTION_PHASE_TIME_TOLERANCE_MILLI / 1000.0f;
    return !isfinite(actual_phase) || !isfinite(authoritative_phase) ||
        authoritative_phase < 0.0f ||
        fabsf(actual_phase - authoritative_phase) > tolerance;
}

BOOL SudekiMpLanArenaClientShouldApplyHostFacing(
    unsigned int actor_index,
    BOOL local_first_person_active
) {
    return actor_index < 2u &&
        !(actor_index == 1u && local_first_person_active);
}

BOOL SudekiMpLanArenaClientLocomotionPhase(
    float host_phase, float host_rate, uint32_t frame_ms,
    BOOL final_boundary, float *phase
) {
    if (phase == NULL) return FALSE;
    *phase = 0.0f;
    if (!isfinite(host_phase) || host_phase < 0.0f || host_phase > 4095.9375f ||
        !isfinite(host_rate) || host_rate < 0.0f || host_rate > 255.99609375f)
        return FALSE;
    if (frame_ms > 50u) frame_ms = 50u;
    if (frame_ms == 0u) frame_ms = 1u;
    *phase = final_boundary ? host_phase :
        fmaxf(0.0f, host_phase - host_rate * (float)frame_ms / 1000.0f);
    return TRUE;
}

BOOL SudekiMpLanArenaClientBukiLocomotionClock(
    float actual_phase, float target_phase, float host_rate, BOOL restart,
    float *playback_rate, BOOL *seek
) {
    float error;
    if (playback_rate == NULL || seek == NULL ||
        !isfinite(actual_phase) || actual_phase < 0.0f ||
        !isfinite(target_phase) || target_phase < 0.0f ||
        target_phase > 4095.9375f || !isfinite(host_rate) ||
        host_rate < 0.0f || host_rate > 255.99609375f) return FALSE;
    error = target_phase - actual_phase;
    *seek = restart || fabsf(error) > fmaxf(1.0f, host_rate * 0.25f);
    *playback_rate = host_rate;
    if (!*seek && host_rate > 0.0f &&
        fabsf(error) > fmaxf(0.125f, host_rate * 0.02f)) {
        *playback_rate += fmaxf(-host_rate * 0.1f,
            fminf(host_rate * 0.1f, error * 5.0f));
    }
    return TRUE;
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

BOOL SudekiMpLanArenaClientRetirementIdlePhaseTime(
    const SudekiMpLanArenaActorSnapshot *snapshot,
    float *phase_time
) {
    if (snapshot == NULL || phase_time == NULL ||
        snapshot->animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_IDLE ||
        snapshot->action_sequence == 0u ||
        !snapshot->action_retirement_valid) return FALSE;
    *phase_time = (float)snapshot->idle_entry_phase_q8 /
        SUDEKIMP_LAN_ARENA_ACTION_PHASE_SCALE;
    return isfinite(*phase_time);
}

BOOL SudekiMpLanArenaClientRetirementPreUpdatePhase(
    float host_idle_entry_phase,
    uint32_t local_frame_elapsed_ms,
    BOOL final_presentation_boundary,
    float *phase_time
) {
    float expected_advance;
    if (phase_time == NULL || !isfinite(host_idle_entry_phase) ||
        host_idle_entry_phase < 0.0f || local_frame_elapsed_ms > 50u) {
        return FALSE;
    }
    if (final_presentation_boundary) {
        *phase_time = host_idle_entry_phase;
        return TRUE;
    }
    if (local_frame_elapsed_ms == 0u) local_frame_elapsed_ms = 1u;
    expected_advance =
        12.0f * (float)local_frame_elapsed_ms / 1000.0f;
    *phase_time = host_idle_entry_phase > expected_advance ?
        host_idle_entry_phase - expected_advance : 0.0f;
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

static BOOL executable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    DWORD protection;
    if (pointer == NULL || length == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u ||
        address + length < address ||
        address + length >
            (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static void clear_remote_tal_skill_view_lease(void) {
    SudekiMpLanArenaOwnerViewClear(
        &remote_tal_skill_view_lease.owner_view);
    memset(&remote_tal_skill_view_lease, 0,
        sizeof(remote_tal_skill_view_lease));
}

static BOOL current_client_owner_view(
    void **camera_mode,
    void **scene_manager
) {
    if (camera_mode == NULL || scene_manager == NULL || game_base == NULL ||
        !readable_memory(game_base + RVA_GAME_CAMERA_MODE_GLOBAL,
            sizeof(*camera_mode)) ||
        !readable_memory(game_base + RVA_SCENE_MANAGER_GLOBAL,
            sizeof(*scene_manager))) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    *camera_mode = *(void **)(game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    *scene_manager = *(void **)(game_base + RVA_SCENE_MANAGER_GLOBAL);
    if (*camera_mode == NULL || *scene_manager == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    return TRUE;
}

static BOOL service_remote_tal_skill_view_storage(
    SudekiMpLanArenaOwnerViewBoundary boundary
) {
    void *camera_mode;
    void *scene_manager;
    if (!remote_tal_skill_view_lease.owner_view.valid) return TRUE;
    if (!current_client_owner_view(&camera_mode, &scene_manager)) {
        return FALSE;
    }
    return SudekiMpLanArenaOwnerViewService(
        &remote_tal_skill_view_lease.owner_view,
        camera_mode, scene_manager, boundary);
}

static BOOL capture_remote_tal_skill_view_lease(uint16_t skill_sequence) {
    void *camera_mode;
    void *scene_manager;

    if (skill_sequence == 0u ||
        remote_tal_skill_view_lease.owner_view.valid ||
        !current_client_owner_view(
            &camera_mode, &scene_manager)) {
        return FALSE;
    }
    if (!SudekiMpLanArenaOwnerViewCapture(
            &remote_tal_skill_view_lease.owner_view,
            camera_mode, scene_manager)) {
        return FALSE;
    }
    remote_tal_skill_view_lease.skill_sequence = skill_sequence;
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_remote_skill_view "
        "state=captured owner=Ailish remote_caster=Tal sequence=%u "
        "camera=0x%08lx render_state=0x%08lx revision=%lu "
        "policy=exact_render_only_owner_view_lease_dynamic_basis\r\n",
        (unsigned int)skill_sequence,
        (unsigned long)(uintptr_t)
            remote_tal_skill_view_lease.owner_view.camera,
        (unsigned long)(uintptr_t)
            remote_tal_skill_view_lease.owner_view.render_state,
        (unsigned long)
            remote_tal_skill_view_lease.owner_view.refresh_revision);
    return TRUE;
}

static BOOL restore_remote_tal_skill_view_storage(void) {
    return service_remote_tal_skill_view_storage(
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION);
}

static BOOL verify_remote_tal_skill_view(void) {
    return service_remote_tal_skill_view_storage(
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER);
}

static BOOL refresh_remote_tal_skill_view(void) {
    if (!remote_tal_skill_view_lease.owner_view.valid) return TRUE;
    if (!native_skill_leases[0].native_started ||
        native_skill_leases[0].seen_sequence !=
            remote_tal_skill_view_lease.skill_sequence) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return service_remote_tal_skill_view_storage(
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REFRESH_AFTER_OWNER_RENDER);
}

static BOOL reassert_remote_tal_skill_view(void) {
    if (!remote_tal_skill_view_lease.owner_view.valid) return TRUE;
    if (!native_skill_leases[0].native_started ||
        native_skill_leases[0].seen_sequence !=
            remote_tal_skill_view_lease.skill_sequence) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return restore_remote_tal_skill_view_storage();
}

static BOOL retire_remote_tal_skill_view_lease(const char *reason) {
    BOOL restored;
    uint16_t sequence;
    void *camera_mode;
    void *scene_manager;
    if (!remote_tal_skill_view_lease.owner_view.valid) return TRUE;
    sequence = remote_tal_skill_view_lease.skill_sequence;
    restored = current_client_owner_view(&camera_mode, &scene_manager) &&
        SudekiMpLanArenaOwnerViewService(
            &remote_tal_skill_view_lease.owner_view,
            camera_mode, scene_manager,
            SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE);
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_remote_skill_view "
        "state=%s owner=Ailish remote_caster=Tal sequence=%u reason=%s "
        "policy=restore_exact_owner_view_before_releasing_lease\r\n",
        restored ? "restored" : "restore_rejected",
        (unsigned int)sequence,
        reason != NULL ? reason : "unspecified");
    if (!restored) {
        SetLastError(ERROR_RETRY);
        return FALSE;
    }
    clear_remote_tal_skill_view_lease();
    remote_tal_skill_view_trace_state = -1;
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaRefreshOwnerViewAfterRender(void) {
    return refresh_remote_tal_skill_view();
}

BOOL SudekiMpLanArenaClientReplicaReassertOwnerViewAfterRemoteMutation(void) {
    return reassert_remote_tal_skill_view();
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

__attribute__((naked, noinline, used))
static unsigned char call_ranged_weapon_reattach(
    void *weapon __attribute__((unused)),
    const char *locator_name __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %edx\n\t"
        "pushl 4(%esp)\n\t"
        "call *%edx\n\t"
        "ret\n\t"
    );
}

static const char *ranged_weapon_primary_locator_name(uint8_t *weapon) {
    const char *locator_name;
    unsigned int length;
    if (!readable_memory(weapon, 0x278u)) return NULL;
    locator_name = (*(uint32_t *)(weapon + 0x270u) & 0x80000000u) != 0u ?
        (const char *)(weapon + 0x274u) :
        *(const char **)(weapon + 0x274u);
    if (locator_name == NULL) return NULL;
    for (length = 0u; length < 64u; ++length) {
        if (!readable_memory(locator_name + length, 1u)) return NULL;
        if (locator_name[length] == '\0') {
            return length == 0u ? NULL : locator_name;
        }
    }
    return NULL;
}

/* Observe the actual model attachment independently of the arbiter's desired
 * view. FUN_00588a90 is destructive before it discovers a no-op, so callers
 * may invoke it only for a fully proven opposite topology; an observation
 * failure remains unknown and must fail closed. In the LAN cleanroom's world
 * view, Ailish can legitimately have no retained +0x164 pointer:
 * CPosition+0xB4 is then the world wrapper and +0x160 is the distinct
 * first-person wrapper. */
static LanArenaAilishModelAttachmentState ailish_model_attachment_state(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    LanArenaAilishModelWitness *result
) {
    LanArenaAilishModelWitness witness;
    uint32_t arbiter_flags;
    BOOL expected_first_person;

    if (result != NULL) ZeroMemory(result, sizeof(*result));
    ZeroMemory(&witness, sizeof(witness));
    if (!readable_memory(character, 0x138u) ||
        !readable_memory(component, 0x168u) ||
        !readable_memory(arbiter, 0x54u) ||
        !readable_memory(
            position, POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *)) ||
        *(void **)(character + AILISH_RANGED_COMPONENT_OFFSET) != component ||
        *(void **)(character + CHARACTER_ARBITER_OFFSET) != arbiter ||
        *(void **)(character + CHARACTER_POSITION_OFFSET) != position ||
        *(void **)(component + 0x10u) != character ||
        *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) != character) {
        return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
    }
    arbiter_flags = *(uint32_t *)(arbiter + 0x50u);
    expected_first_person =
        (arbiter_flags & AILISH_FIRST_PERSON_ARBITER_FLAG) != 0u;
    witness.position = position;
    witness.attached_wrapper = *(uint8_t **)(
        position + POSITION_ATTACHED_WRAPPER_OFFSET);
    witness.first_person_wrapper = *(uint8_t **)(
        component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
    witness.saved_world_wrapper = *(uint8_t **)(
        component + AILISH_WORLD_WRAPPER_OFFSET);
    if (!readable_memory(witness.attached_wrapper, 0x14u) ||
        !readable_memory(witness.first_person_wrapper, 0x14u)) {
        return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
    }
    witness.attached_renderer = *(void **)(
        witness.attached_wrapper + 0x10u);
    witness.first_person_renderer = *(void **)(
        witness.first_person_wrapper + 0x10u);
    if (witness.attached_renderer == NULL ||
        witness.first_person_renderer == NULL) {
        return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
    }
    if (witness.attached_wrapper == witness.first_person_wrapper) {
        witness.first_person = TRUE;
        /* A native world-to-first-person switch always saves a distinct world
         * wrapper at +0x164 before attaching +0x160. Requiring that retained
         * lease also keeps the world combat graph available for replication. */
        if (!readable_memory(witness.saved_world_wrapper, 0x14u) ||
            witness.saved_world_wrapper == witness.first_person_wrapper ||
            *(void **)(witness.saved_world_wrapper + 0x10u) == NULL) {
            return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
        }
        witness.saved_world_renderer = *(void **)(
            witness.saved_world_wrapper + 0x10u);
    } else if (witness.saved_world_wrapper != NULL) {
        witness.first_person = FALSE;
        if (!readable_memory(witness.saved_world_wrapper, 0x14u) ||
            witness.attached_wrapper != witness.saved_world_wrapper ||
            witness.saved_world_wrapper == witness.first_person_wrapper) {
            return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
        }
        witness.saved_world_renderer = *(void **)(
            witness.saved_world_wrapper + 0x10u);
        if (witness.saved_world_renderer == NULL ||
            witness.saved_world_renderer != witness.attached_renderer) {
            return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
        }
    } else {
        witness.first_person = FALSE;
        witness.fallback_world = TRUE;
    }
    if (*(void **)(character + AILISH_RANGED_COMPONENT_OFFSET) != component ||
        *(void **)(character + CHARACTER_ARBITER_OFFSET) != arbiter ||
        *(void **)(character + CHARACTER_POSITION_OFFSET) != position ||
        *(void **)(component + 0x10u) != character ||
        *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) != character ||
        *(uint32_t *)(arbiter + 0x50u) != arbiter_flags ||
        *(void **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) !=
            witness.attached_wrapper ||
        *(void **)(component + AILISH_FIRST_PERSON_WRAPPER_OFFSET) !=
            witness.first_person_wrapper ||
        *(void **)(component + AILISH_WORLD_WRAPPER_OFFSET) !=
            witness.saved_world_wrapper ||
        *(void **)(witness.attached_wrapper + 0x10u) !=
            witness.attached_renderer ||
        *(void **)(witness.first_person_wrapper + 0x10u) !=
            witness.first_person_renderer ||
        (witness.saved_world_wrapper != NULL &&
            *(void **)(witness.saved_world_wrapper + 0x10u) !=
                witness.saved_world_renderer)) {
        return LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
    }
    if (result != NULL) *result = witness;
    return witness.first_person == expected_first_person ?
        LAN_ARENA_AILISH_MODEL_ATTACHMENT_DESIRED :
        LAN_ARENA_AILISH_MODEL_ATTACHMENT_OPPOSITE;
}

static BOOL ailish_desired_model_attached(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    LanArenaAilishModelWitness *result
) {
    return ailish_model_attachment_state(
        character, component, arbiter, position, result) ==
        LAN_ARENA_AILISH_MODEL_ATTACHMENT_DESIRED;
}

static BOOL weapon_primary_parent_matches_position(
    uint8_t *weapon,
    uint8_t *position
) {
    return readable_memory(position, POSITION_PARENT_LINK_BIAS) &&
        readable_memory(
            weapon, WEAPON_PRIMARY_PARENT_OFFSET + sizeof(void *)) &&
        *(void **)(weapon + WEAPON_PRIMARY_PARENT_OFFSET) ==
            (void *)(position + POSITION_PARENT_LINK_BIAS);
}

static BOOL ailish_ranged_pointer_graph_exact(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner
) {
    if (!readable_memory(character, 0x138u) ||
        !readable_memory(component, 0x170u) ||
        !readable_memory(arbiter, 0x54u) ||
        !readable_memory(
            position, POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *)) ||
        !readable_memory(weapon, WEAPON_FLAGS_OFFSET + sizeof(uint8_t)) ||
        !readable_memory(active_model, 0x10u) ||
        !readable_memory(active_owner, 0x48u)) {
        return FALSE;
    }
    return *(void **)(character + AILISH_RANGED_COMPONENT_OFFSET) == component &&
        *(void **)(character + CHARACTER_ARBITER_OFFSET) == arbiter &&
        *(void **)(character + CHARACTER_POSITION_OFFSET) == position &&
        *(void **)(character + CHARACTER_WEAPON_OFFSET) == weapon &&
        *(void **)(component + 0x10u) == character &&
        *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) == character &&
        *(void **)(weapon + WEAPON_ACTIVE_MODEL_OFFSET) == active_model &&
        *(void **)(active_model + 0x0cu) == active_owner &&
        active_owner == character &&
        *(void **)(active_owner + CHARACTER_POSITION_OFFSET) == position &&
        readable_memory(
            *(void **)(weapon + WEAPON_CURRENT_ITEM_OFFSET), sizeof(void *));
}

static BOOL ailish_ranged_pointer_lease_exact(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner
) {
    return SudekiMpCleanroomEngineActorEntity(seat_client_actor()) ==
            character &&
        ailish_ranged_pointer_graph_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner);
}

static BOOL ailish_model_switch_mutation_writable(
    uint8_t *component,
    uint8_t *position,
    const LanArenaAilishModelWitness *witness
) {
    uint8_t *wrappers[3];
    unsigned int index;

    if (witness == NULL ||
        !writable_memory(component, 0x170u) ||
        !writable_memory(position, 0x104u)) {
        return FALSE;
    }
    wrappers[0] = witness->attached_wrapper;
    wrappers[1] = witness->first_person_wrapper;
    wrappers[2] = witness->saved_world_wrapper;
    for (index = 0u; index < 3u; ++index) {
        uint8_t *wrapper = wrappers[index];
        uint8_t *render_object;
        unsigned int prior;
        if (wrapper == NULL) continue;
        for (prior = 0u; prior < index; ++prior) {
            if (wrappers[prior] == wrapper) break;
        }
        if (prior != index) continue;
        if (!writable_memory(wrapper, 0x19u)) return FALSE;
        render_object = *(uint8_t **)(wrapper + 0x08u);
        if (!writable_memory(render_object, 0x38u) ||
            !readable_memory(*(void **)(wrapper + 0x10u), sizeof(void *))) {
            return FALSE;
        }
    }
    return TRUE;
}

/* WeaponFollow (FUN_004d8280) is not a passive lookup. On success its
 * FUN_004d8630 installer rewrites the weapon's embedded primary CPosition
 * and render-object attachment graph. Prove every exact pointer that the
 * supported body traverses before allowing the call; readable-but-foreign
 * topology is never mutation authority. */
static BOOL ailish_weapon_reattach_graph_writable(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner,
    const LanArenaAilishModelWitness *model,
    LanArenaAilishWeaponReattachWitness *result
) {
    LanArenaAilishWeaponReattachWitness witness;

    ZeroMemory(&witness, sizeof(witness));
    if (result != NULL) ZeroMemory(result, sizeof(*result));
    if (model == NULL ||
        !ailish_ranged_pointer_graph_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        !writable_memory(position, 0x104u) ||
        !writable_memory(weapon, WEAPON_FLAGS_OFFSET + sizeof(uint8_t)) ||
        *(void **)(weapon + WEAPON_OWNER_OFFSET) != character ||
        *(void **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) !=
            model->attached_wrapper) {
        return FALSE;
    }

    witness.attached_wrapper = model->attached_wrapper;
    witness.attached_renderer = model->attached_renderer;
    if (!readable_memory(witness.attached_wrapper, 0x14u) ||
        *(void **)(witness.attached_wrapper + WRAPPER_RENDERER_OFFSET) !=
            witness.attached_renderer ||
        witness.attached_renderer == NULL) {
        return FALSE;
    }
    witness.model_interface = *(uint8_t **)(
        witness.attached_wrapper + WRAPPER_MODEL_INTERFACE_OFFSET);
    if (!readable_memory(witness.model_interface, sizeof(void *))) {
        return FALSE;
    }
    witness.model_interface_vtable = *(uint8_t **)witness.model_interface;
    if (!readable_memory(witness.model_interface_vtable, 0x2cu)) {
        return FALSE;
    }
    witness.matrix_method = *(void **)(witness.model_interface_vtable + 0x24u);
    witness.locator_method = *(void **)(witness.model_interface_vtable + 0x28u);
    if (!executable_memory(witness.matrix_method, 1u) ||
        !executable_memory(witness.locator_method, 1u)) {
        return FALSE;
    }

    witness.primary_wrapper = *(uint8_t **)(
        weapon + WEAPON_PRIMARY_WRAPPER_OFFSET);
    if (!readable_memory(witness.primary_wrapper, 0x14u)) return FALSE;
    witness.primary_render_object = *(uint8_t **)(
        witness.primary_wrapper + WRAPPER_RENDER_OBJECT_OFFSET);
    if (!readable_memory(witness.primary_render_object, 0x110u) ||
        !writable_memory(witness.primary_render_object + 0x18u, 0x34u) ||
        *(void **)(weapon + WEAPON_PRIMARY_RENDER_OBJECT_OFFSET) !=
            witness.primary_render_object) {
        return FALSE;
    }

    if (!ailish_ranged_pointer_graph_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        *(void **)(weapon + WEAPON_OWNER_OFFSET) != character ||
        *(void **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) !=
            witness.attached_wrapper ||
        *(void **)(witness.attached_wrapper + WRAPPER_MODEL_INTERFACE_OFFSET) !=
            witness.model_interface ||
        *(void **)(witness.attached_wrapper + WRAPPER_RENDERER_OFFSET) !=
            witness.attached_renderer ||
        *(void **)witness.model_interface != witness.model_interface_vtable ||
        *(void **)(witness.model_interface_vtable + 0x24u) !=
            witness.matrix_method ||
        *(void **)(witness.model_interface_vtable + 0x28u) !=
            witness.locator_method ||
        *(void **)(weapon + WEAPON_PRIMARY_WRAPPER_OFFSET) !=
            witness.primary_wrapper ||
        *(void **)(witness.primary_wrapper + WRAPPER_RENDER_OBJECT_OFFSET) !=
            witness.primary_render_object ||
        *(void **)(weapon + WEAPON_PRIMARY_RENDER_OBJECT_OFFSET) !=
            witness.primary_render_object) {
        return FALSE;
    }
    if (result != NULL) *result = witness;
    return TRUE;
}

static BOOL ailish_weapon_reattach_mutation_writable(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner,
    const LanArenaAilishModelWitness *model,
    LanArenaAilishWeaponReattachWitness *result
) {
    if (SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            character ||
        !ailish_weapon_reattach_graph_writable(
            character, component, arbiter, position, weapon,
            active_model, active_owner, model, result) ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            character) {
        if (result != NULL) ZeroMemory(result, sizeof(*result));
        return FALSE;
    }
    return TRUE;
}

static BOOL capture_visibility_render_object(
    uint8_t *wrapper,
    uint8_t **render_object_result,
    uint32_t *render_flags_result,
    uint8_t **callback_result,
    uint8_t **callback_vtable_result,
    void **callback_method_result
) {
    uint8_t *render_object;
    uint32_t render_flags;
    uint8_t *callback = NULL;
    uint8_t *callback_vtable = NULL;
    void *callback_method = NULL;

    if (render_object_result == NULL || render_flags_result == NULL ||
        callback_result == NULL || callback_vtable_result == NULL ||
        callback_method_result == NULL ||
        !readable_memory(wrapper, 0x0cu)) {
        return FALSE;
    }
    render_object = *(uint8_t **)(
        wrapper + WRAPPER_RENDER_OBJECT_OFFSET);
    if (!writable_memory(render_object, 0x38u)) return FALSE;
    render_flags = *(uint32_t *)(render_object + RENDER_OBJECT_FLAGS_OFFSET);
    if ((render_flags & (RENDER_OBJECT_VISIBILITY_CALLBACK_FLAG |
            RENDER_OBJECT_HIDDEN_FLAG)) ==
            (RENDER_OBJECT_VISIBILITY_CALLBACK_FLAG |
             RENDER_OBJECT_HIDDEN_FLAG)) {
        callback = *(uint8_t **)(
            render_object + RENDER_OBJECT_CALLBACK_OFFSET);
        if (!readable_memory(callback, sizeof(void *))) return FALSE;
        callback_vtable = *(uint8_t **)callback;
        if (!readable_memory(
                callback_vtable,
                VISIBILITY_CALLBACK_METHOD_OFFSET + sizeof(void *))) {
            return FALSE;
        }
        callback_method = *(void **)(
            callback_vtable + VISIBILITY_CALLBACK_METHOD_OFFSET);
        if (!executable_memory(callback_method, 1u)) return FALSE;
    }
    *render_object_result = render_object;
    *render_flags_result = render_flags;
    *callback_result = callback;
    *callback_vtable_result = callback_vtable;
    *callback_method_result = callback_method;
    return TRUE;
}

/* SetWeaponVisible(TRUE) mutates both weapon state and every installed
 * weapon render object, including the optional secondary model. Capture a
 * fresh exact graph only after WeaponFollow has returned, and validate the
 * conditional visibility callback before native code can dispatch it. */
static BOOL ailish_weapon_visibility_graph_writable(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner,
    LanArenaAilishWeaponVisibilityWitness *result
) {
    LanArenaAilishWeaponVisibilityWitness witness;

    ZeroMemory(&witness, sizeof(witness));
    if (result != NULL) ZeroMemory(result, sizeof(*result));
    if (!ailish_ranged_pointer_graph_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        !writable_memory(weapon + WEAPON_FLAGS_OFFSET, sizeof(uint8_t)) ||
        *(void **)(weapon + WEAPON_OWNER_OFFSET) != character ||
        *(void **)(character + CHARACTER_ARBITER_OFFSET) != arbiter ||
        *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) != character) {
        return FALSE;
    }

    witness.primary_wrapper = *(uint8_t **)(
        weapon + WEAPON_PRIMARY_WRAPPER_OFFSET);
    if (!capture_visibility_render_object(
            witness.primary_wrapper,
            &witness.primary_render_object,
            &witness.primary_render_flags,
            &witness.primary_callback,
            &witness.primary_callback_vtable,
            &witness.primary_callback_method)) {
        return FALSE;
    }
    witness.secondary_wrapper = *(uint8_t **)(
        weapon + WEAPON_SECONDARY_WRAPPER_OFFSET);
    if (witness.secondary_wrapper != NULL &&
        witness.secondary_wrapper != witness.primary_wrapper) {
        if (!capture_visibility_render_object(
                witness.secondary_wrapper,
                &witness.secondary_render_object,
                &witness.secondary_render_flags,
                &witness.secondary_callback,
                &witness.secondary_callback_vtable,
                &witness.secondary_callback_method)) {
            return FALSE;
        }
    } else if (witness.secondary_wrapper == witness.primary_wrapper) {
        witness.secondary_render_object = witness.primary_render_object;
        witness.secondary_render_flags = witness.primary_render_flags;
        witness.secondary_callback = witness.primary_callback;
        witness.secondary_callback_vtable = witness.primary_callback_vtable;
        witness.secondary_callback_method = witness.primary_callback_method;
    }

    if (!ailish_ranged_pointer_graph_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        *(void **)(weapon + WEAPON_OWNER_OFFSET) != character ||
        *(void **)(weapon + WEAPON_PRIMARY_WRAPPER_OFFSET) !=
            witness.primary_wrapper ||
        *(void **)(weapon + WEAPON_SECONDARY_WRAPPER_OFFSET) !=
            witness.secondary_wrapper ||
        *(void **)(witness.primary_wrapper + WRAPPER_RENDER_OBJECT_OFFSET) !=
            witness.primary_render_object ||
        *(uint32_t *)(witness.primary_render_object +
            RENDER_OBJECT_FLAGS_OFFSET) != witness.primary_render_flags) {
        return FALSE;
    }
    if (witness.secondary_wrapper != NULL &&
        (*(void **)(witness.secondary_wrapper +
                WRAPPER_RENDER_OBJECT_OFFSET) !=
                witness.secondary_render_object ||
         *(uint32_t *)(witness.secondary_render_object +
                RENDER_OBJECT_FLAGS_OFFSET) !=
                witness.secondary_render_flags)) {
        return FALSE;
    }
    if ((witness.primary_callback != NULL &&
            (*(void **)(witness.primary_render_object +
                    RENDER_OBJECT_CALLBACK_OFFSET) !=
                    witness.primary_callback ||
             *(void **)witness.primary_callback !=
                    witness.primary_callback_vtable ||
             *(void **)(witness.primary_callback_vtable +
                    VISIBILITY_CALLBACK_METHOD_OFFSET) !=
                    witness.primary_callback_method)) ||
        (witness.secondary_callback != NULL &&
            (*(void **)(witness.secondary_render_object +
                    RENDER_OBJECT_CALLBACK_OFFSET) !=
                    witness.secondary_callback ||
             *(void **)witness.secondary_callback !=
                    witness.secondary_callback_vtable ||
             *(void **)(witness.secondary_callback_vtable +
                    VISIBILITY_CALLBACK_METHOD_OFFSET) !=
                    witness.secondary_callback_method))) {
        return FALSE;
    }
    if (result != NULL) *result = witness;
    return TRUE;
}

static BOOL ailish_weapon_visibility_mutation_writable(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner,
    LanArenaAilishWeaponVisibilityWitness *result
) {
    if (SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            character ||
        !ailish_weapon_visibility_graph_writable(
            character, component, arbiter, position, weapon,
            active_model, active_owner, result) ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            character) {
        if (result != NULL) ZeroMemory(result, sizeof(*result));
        return FALSE;
    }
    return TRUE;
}

static BOOL ailish_weapon_visibility_postcondition(
    uint8_t *character,
    uint8_t *component,
    uint8_t *arbiter,
    uint8_t *position,
    uint8_t *weapon,
    uint8_t *active_model,
    uint8_t *active_owner,
    const LanArenaAilishWeaponVisibilityWitness *witness
) {
    if (witness == NULL ||
        !ailish_ranged_pointer_lease_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        *(void **)(weapon + WEAPON_OWNER_OFFSET) != character ||
        *(void **)(weapon + WEAPON_PRIMARY_WRAPPER_OFFSET) !=
            witness->primary_wrapper ||
        *(void **)(weapon + WEAPON_SECONDARY_WRAPPER_OFFSET) !=
            witness->secondary_wrapper ||
        !readable_memory(witness->primary_wrapper, 0x0cu) ||
        *(void **)(witness->primary_wrapper + WRAPPER_RENDER_OBJECT_OFFSET) !=
            witness->primary_render_object ||
        !readable_memory(witness->primary_render_object, 0x38u) ||
        (*(uint8_t *)(weapon + WEAPON_FLAGS_OFFSET) &
            WEAPON_VISIBLE_FLAG) == 0u ||
        (*(uint32_t *)(witness->primary_render_object +
            RENDER_OBJECT_FLAGS_OFFSET) & RENDER_OBJECT_HIDDEN_FLAG) != 0u) {
        return FALSE;
    }
    if (witness->secondary_wrapper != NULL &&
        (!readable_memory(witness->secondary_wrapper, 0x0cu) ||
         *(void **)(witness->secondary_wrapper +
                WRAPPER_RENDER_OBJECT_OFFSET) !=
                witness->secondary_render_object ||
         !readable_memory(witness->secondary_render_object, 0x38u) ||
         (*(uint32_t *)(witness->secondary_render_object +
                RENDER_OBJECT_FLAGS_OFFSET) &
                RENDER_OBJECT_HIDDEN_FLAG) != 0u)) {
        return FALSE;
    }
    return TRUE;
}

static BOOL ailish_weapon_attachment_witness(
    uint8_t *character,
    uint8_t *component,
    uint8_t **weapon_result,
    uint8_t **render_object_result,
    const char **failure_result
) {
    uint8_t *position;
    uint8_t *weapon;
    uint8_t *active_model;
    uint8_t *active_owner;
    uint8_t *wrapper;
    uint8_t *render_object;
    const char *failure = NULL;
    if (weapon_result != NULL) *weapon_result = NULL;
    if (render_object_result != NULL) *render_object_result = NULL;
    if (failure_result != NULL) *failure_result = NULL;
    if (!readable_memory(character, 0x138u) ||
        !readable_memory(component, 0x170u) ||
        *(void **)(component + 0x10u) != character) {
        failure = "weapon_character_component_lease";
        goto rejected;
    }
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    weapon = *(uint8_t **)(character + CHARACTER_WEAPON_OFFSET);
    if (!readable_memory(position,
            POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *))) {
        failure = "weapon_position_lease";
        goto rejected;
    }
    if (!readable_memory(weapon, WEAPON_FLAGS_OFFSET + sizeof(uint8_t)) ||
        !readable_memory(
            *(void **)(weapon + WEAPON_CURRENT_ITEM_OFFSET), sizeof(void *))) {
        failure = "weapon_object_or_item_lease";
        goto rejected;
    }
    if (*(void **)(weapon + WEAPON_OWNER_OFFSET) != character) {
        failure = "weapon_owner_backpointer";
        goto rejected;
    }
    if (!weapon_primary_parent_matches_position(weapon, position)) {
        failure = "weapon_primary_parent";
        goto rejected;
    }
    if (*(int *)(weapon + WEAPON_PRIMARY_LOCATOR_OFFSET) < 0) {
        failure = "weapon_primary_locator";
        goto rejected;
    }
    if ((*(uint8_t *)(weapon + WEAPON_FLAGS_OFFSET) &
            WEAPON_VISIBLE_FLAG) == 0u) {
        failure = "weapon_visible_flag";
        goto rejected;
    }
    active_model = *(uint8_t **)(weapon + WEAPON_ACTIVE_MODEL_OFFSET);
    if (!readable_memory(active_model, 0x10u)) {
        failure = "weapon_active_model_lease";
        goto rejected;
    }
    active_owner = *(uint8_t **)(active_model + 0x0cu);
    if (active_owner != character || !readable_memory(active_owner, 0x48u) ||
        *(void **)(active_owner + CHARACTER_POSITION_OFFSET) != position) {
        failure = "weapon_active_owner_lease";
        goto rejected;
    }
    wrapper = *(uint8_t **)(weapon + WEAPON_PRIMARY_WRAPPER_OFFSET);
    if (!readable_memory(wrapper, 0x0cu)) {
        failure = "weapon_primary_wrapper";
        goto rejected;
    }
    render_object = *(uint8_t **)(wrapper + 0x08u);
    if (!readable_memory(render_object, 0x38u)) {
        failure = "weapon_render_object";
        goto rejected;
    }
    if ((*(uint32_t *)(render_object + 0x34u) &
            RENDER_OBJECT_HIDDEN_FLAG) != 0u) {
        failure = "weapon_render_object_hidden";
        goto rejected;
    }
    if (*(void **)(character + CHARACTER_POSITION_OFFSET) != position ||
        *(void **)(character + CHARACTER_WEAPON_OFFSET) != weapon ||
        *(void **)(component + 0x10u) != character ||
        *(void **)(weapon + WEAPON_OWNER_OFFSET) != character ||
        *(void **)(weapon + WEAPON_ACTIVE_MODEL_OFFSET) != active_model ||
        *(void **)(active_model + 0x0cu) != active_owner ||
        *(void **)(active_owner + CHARACTER_POSITION_OFFSET) != position ||
        !weapon_primary_parent_matches_position(weapon, position) ||
        *(void **)(weapon + WEAPON_PRIMARY_WRAPPER_OFFSET) != wrapper ||
        *(void **)(wrapper + 0x08u) != render_object) {
        failure = "weapon_post_witness_lease_changed";
        goto rejected;
    }
    if (weapon_result != NULL) *weapon_result = weapon;
    if (render_object_result != NULL) *render_object_result = render_object;
    return TRUE;

rejected:
    if (failure_result != NULL) *failure_result = failure;
    return FALSE;
}

static BOOL refresh_ailish_ranged_presentation(void) {
    uint8_t *character = (uint8_t *)
        SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    uint8_t *component = NULL;
    uint8_t *arbiter = NULL;
    uint8_t *position = NULL;
    uint8_t *weapon = NULL;
    uint8_t *active_model = NULL;
    uint8_t *active_owner = NULL;
    uint8_t *witness_weapon = NULL;
    uint8_t *render_object = NULL;
    LanArenaAilishModelWitness before_model;
    LanArenaAilishModelWitness after_model;
    LanArenaAilishWeaponReattachWitness reattach_witness;
    LanArenaAilishWeaponVisibilityWitness visibility_witness;
    LanArenaAilishModelAttachmentState before_model_state;
    const char *locator_name;
    unsigned char refreshed = 0u;
    unsigned char reattached;
    BOOL expected_first_person;
    const char *model_switch_state;
    const char *attachment_failure = NULL;
    const char *failure = NULL;
    if (!client_session_authenticated() || game_base == NULL ||
        ailish_ranged_presentation_refresh == NULL ||
        weapon_set_visible == NULL ||
        ranged_weapon_reattach == NULL ||
        !readable_memory(character, 0x138u)) {
        failure = "client_actor_unavailable";
        goto rejected;
    }
    component = *(uint8_t **)(character + AILISH_RANGED_COMPONENT_OFFSET);
    arbiter = *(uint8_t **)(character + CHARACTER_ARBITER_OFFSET);
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    weapon = *(uint8_t **)(character + CHARACTER_WEAPON_OFFSET);
    if (!readable_memory(component, 0x170u) ||
        *(void **)(component + 0x10u) != character) {
        failure = "ranged_component_lease";
        goto rejected;
    }
    if (!readable_memory(arbiter, 0x54u) ||
        *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) != character) {
        failure = "arbiter_lease";
        goto rejected;
    }
    if (!readable_memory(
            position, POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *))) {
        failure = "position_lease";
        goto rejected;
    }
    if (!readable_memory(weapon, WEAPON_FLAGS_OFFSET + sizeof(uint8_t)) ||
        !readable_memory(
            *(void **)(weapon + WEAPON_CURRENT_ITEM_OFFSET), sizeof(void *))) {
        failure = "equipped_weapon_lease";
        goto rejected;
    }
    active_model = *(uint8_t **)(weapon + WEAPON_ACTIVE_MODEL_OFFSET);
    if (!readable_memory(active_model, 0x10u)) {
        failure = "active_weapon_model_lease";
        goto rejected;
    }
    active_owner = *(uint8_t **)(active_model + 0x0cu);
    if (active_owner != character || !readable_memory(active_owner, 0x48u) ||
        *(void **)(active_owner + CHARACTER_POSITION_OFFSET) != position) {
        failure = "active_weapon_owner_lease";
        goto rejected;
    }
    before_model_state = ailish_model_attachment_state(
        character, component, arbiter, position, &before_model);
    expected_first_person =
        (*(uint32_t *)(arbiter + 0x50u) &
         AILISH_FIRST_PERSON_ARBITER_FLAG) != 0u;
    if (before_model_state == LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN) {
        failure = "native_model_topology_unknown";
        goto rejected;
    }
    /* The native model-switch helper returns one only when it replaces an
     * exact opposite model. Its wrapper resets animation channels before
     * learning that an already-attached desired model is a no-op, so unknown
     * and desired observations can never authorize that mutation. */
    if (before_model_state == LAN_ARENA_AILISH_MODEL_ATTACHMENT_OPPOSITE) {
        if (!ailish_model_switch_mutation_writable(
                component, position, &before_model)) {
            failure = "native_model_switch_memory_not_writable";
            goto rejected;
        }
        refreshed = ailish_ranged_presentation_refresh(component);
        if (refreshed == 0u) {
            failure = "native_model_switch_rejected";
            goto rejected;
        }
    }
    if (!ailish_ranged_pointer_lease_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        !ailish_desired_model_attached(
            character, component, arbiter, position, &after_model) ||
        after_model.first_person != expected_first_person) {
        failure = "post_refresh_model_lease_changed";
        goto rejected;
    }
    if (!ailish_weapon_reattach_mutation_writable(
            character, component, arbiter, position, weapon,
            active_model, active_owner, &after_model,
            &reattach_witness)) {
        failure = "native_weapon_reattach_memory_not_writable";
        goto rejected;
    }
    locator_name = ranged_weapon_primary_locator_name(weapon);
    if (locator_name == NULL) {
        failure = "weapon_locator_name";
        goto rejected;
    }
    reattached = call_ranged_weapon_reattach(
        weapon, locator_name, ranged_weapon_reattach);
    if (reattached == 0u) {
        failure = "native_weapon_reattach_rejected";
        goto rejected;
    }
    if (!ailish_ranged_pointer_lease_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner) ||
        !ailish_desired_model_attached(
            character, component, arbiter, position, &after_model) ||
        after_model.first_person != expected_first_person ||
        after_model.attached_wrapper != reattach_witness.attached_wrapper ||
        after_model.attached_renderer != reattach_witness.attached_renderer) {
        failure = "post_weapon_reattach_lease_changed";
        goto rejected;
    }
    if (!ailish_weapon_visibility_mutation_writable(
            character, component, arbiter, position, weapon,
            active_model, active_owner, &visibility_witness)) {
        failure = "native_weapon_visibility_memory_not_writable";
        goto rejected;
    }
    /* The exact fallback skips only the destructive model-switch wrapper.
     * Native WeaponFollow reattachment and visibility still own the weapon;
     * never write wrapper pointers, locator indices, or render flags. */
    weapon_set_visible(weapon, 1);
    if (!ailish_weapon_visibility_postcondition(
            character, component, arbiter, position, weapon,
            active_model, active_owner, &visibility_witness) ||
        !ailish_weapon_attachment_witness(
            character, component, &witness_weapon, &render_object,
            &attachment_failure) ||
        witness_weapon != weapon ||
        !ailish_desired_model_attached(
            character, component, arbiter, position, &after_model) ||
        after_model.first_person != expected_first_person) {
        failure = attachment_failure != NULL ? attachment_failure :
            "weapon_attachment_not_visible";
        goto rejected;
    }
    /* SetVisible may traverse the current weapon item. Resolve the borrowed
     * locator string only after every native mutation and final graph witness,
     * immediately before the diagnostic consumes it. */
    locator_name = ranged_weapon_primary_locator_name(weapon);
    if (locator_name == NULL) {
        failure = "post_visibility_locator_name";
        goto rejected;
    }
    model_switch_state = refreshed != 0u ? "changed" :
        (after_model.fallback_world ?
            "already_active_world_fallback" : "already_active");
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_ailish_weapon_attachment "
        "state=ready character=0x%08lx component=0x%08lx weapon=0x%08lx "
        "parent=0x%08lx locator=%ld wrapper=0x%08lx render_object=0x%08lx "
        "character_wrapper=0x%08lx character_renderer=0x%08lx "
        "weapon_flags=0x%02x model_switch=%s locator_name=%s "
        "policy=exact_native_model_noop_or_switch_then_weapon_visibility\r\n",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)component,
        (unsigned long)(uintptr_t)weapon,
        (unsigned long)(uintptr_t)*(void **)(
            weapon + WEAPON_PRIMARY_PARENT_OFFSET),
        (long)*(int *)(weapon + WEAPON_PRIMARY_LOCATOR_OFFSET),
        (unsigned long)(uintptr_t)*(void **)(
            weapon + WEAPON_PRIMARY_WRAPPER_OFFSET),
        (unsigned long)(uintptr_t)render_object,
        (unsigned long)(uintptr_t)after_model.attached_wrapper,
        (unsigned long)(uintptr_t)after_model.attached_renderer,
        (unsigned int)*(uint8_t *)(weapon + WEAPON_FLAGS_OFFSET),
        model_switch_state,
        locator_name);
    return TRUE;

rejected:
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_ailish_weapon_attachment "
        "state=waiting reason=%s character=0x%08lx position=0x%08lx "
        "weapon=0x%08lx parent_link=0x%08lx expected_parent_link=0x%08lx "
        "policy=fail_closed_until_native_attachment_is_visible\r\n",
        failure != NULL ? failure : "unknown",
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)position,
        (unsigned long)(uintptr_t)weapon,
        (unsigned long)(uintptr_t)(readable_memory(
                weapon, WEAPON_PRIMARY_PARENT_OFFSET + sizeof(void *)) ?
            *(void **)(weapon + WEAPON_PRIMARY_PARENT_OFFSET) : NULL),
        (unsigned long)(uintptr_t)(readable_memory(
                position, POSITION_PARENT_LINK_BIAS) ?
            position + POSITION_PARENT_LINK_BIAS : NULL));
    return FALSE;
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
        result->local_simulation_node_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA &&
        result->peer_simulation_node_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD &&
        result->session_token != 0u;
}

static BOOL client_session_authenticated(void) {
    SudekiMpLanArenaSessionStatus status;
    return client_session_status(&status);
}

BOOL SudekiMpLanArenaClientSkillReplayAdmission(void *actor,int slot,uint64_t session) {
    SudekiMpLanArenaSessionStatus status;
    return actor && session && slot>=0 && slot<6 &&
        !client_replica_reset_pending && client_skill_replay_admission.sequence &&
        client_skill_replay_admission.actor==actor && client_skill_replay_admission.slot==slot &&
        client_skill_replay_admission.session==session &&
        client_skill_replay_admission.thread==GetCurrentThreadId() &&
        InterlockedCompareExchange(&client_skill_activation_depth,0,0)==1 &&
        (client_skill_activation_actor_index==0 || client_skill_activation_actor_index==1) &&
        actor==SudekiMpCleanroomEngineActorEntity(client_skill_activation_actor_index==0 ?
            seat_host_actor():seat_client_actor()) &&
        client_session_status(&status) && status.session_token==session;
}

static BOOL set_client_remote_tal_skill_input_isolation(
    BOOL enabled,
    const char *reason
) {
    BOOL next_enabled = enabled != FALSE;
    if (client_remote_tal_skill_input_isolation_active == next_enabled) {
        return TRUE;
    }
    if (next_enabled && !SudekiMpControlSeparationBindLanArenaMovementActors(
            seat_client_type(), seat_host_type())) return FALSE;
    if (!SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(
            next_enabled)) {
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_noncaster_input "
            "state=rejected owner=Ailish remote_caster=Tal enabled=%u "
            "reason=%s win32_error=%lu "
            "policy=never_cross_native_tal_skill_mode_without_exact_seat0_isolation\r\n",
            next_enabled ? 1u : 0u,
            reason != NULL ? reason : "unspecified",
            (unsigned long)GetLastError());
        return FALSE;
    }
    client_remote_tal_skill_input_isolation_active = next_enabled;
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_noncaster_input "
        "state=%s owner=Ailish remote_caster=Tal reason=%s "
        "policy=local_seat0_remains_live_during_remote_native_cskill_replay\r\n",
        next_enabled ? "isolated" : "released",
        reason != NULL ? reason : "unspecified");
    return TRUE;
}

static BOOL client_skill_replay_active(void) {
    return InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0 ||
        native_skill_leases[0].native_started ||
        native_skill_leases[1].native_started;
}

static int client_skill_replay_caster_index(void) {
    BOOL tal_active;
    BOOL ailish_active;
    if (InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0) {
        return client_skill_activation_actor_index;
    }
    tal_active = native_skill_leases[0].native_started;
    ailish_active = native_skill_leases[1].native_started;
    if (tal_active == ailish_active) return -1;
    return tal_active ? 0 : 1;
}

BOOL SudekiMpLanArenaClientReplicaLocalSkillCameraActive(void) {
    return (SudekiMpLanArenaClientPrivateCastCamerasOwned() ?
        (native_skill_leases[1].native_started ||
         (InterlockedCompareExchange(&client_skill_activation_depth,0,0)>0 &&
          client_skill_activation_actor_index==1)):
        (client_skill_replay_active() && client_skill_replay_caster_index()==1)) ||
        (client_session_authenticated() &&
         last_applied_snapshot.seat[1].actor_type == SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
         last_applied_snapshot.seat[1].skill_active &&
         last_applied_snapshot.seat[1].skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT);
}

BOOL SudekiMpLanArenaClientReplicaAnySkillReplayActive(void) {
    return client_skill_replay_active();
}

static BOOL set_client_skill_realtime_scale(BOOL enabled) {
    uint32_t *scale;
    uint32_t desired;
    DWORD old_protection;
    DWORD ignored_protection;
    BOOL restored;

    if (game_base == NULL) return FALSE;
    scale = (uint32_t *)(game_base + RVA_FIXED_ALTERNATE_SPEED);
    desired = enabled ? 0x3f800000u :
        client_skill_original_alternate_speed_bits;
    if (!readable_memory(scale, sizeof(*scale))) return FALSE;
    if (*scale == desired) {
        client_skill_speed_override_active = enabled;
        return TRUE;
    }
    if (!VirtualProtect(scale, sizeof(*scale), PAGE_EXECUTE_READWRITE,
            &old_protection)) return FALSE;
    *scale = desired;
    FlushInstructionCache(GetCurrentProcess(), scale, sizeof(*scale));
    restored = VirtualProtect(scale, sizeof(*scale), old_protection,
        &ignored_protection);
    if (restored && *scale == desired) {
        client_skill_speed_override_active = enabled;
        return TRUE;
    }
    return FALSE;
}

static BOOL client_native_transaction_retained(void) {
    return InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0 ||
        native_skill_leases[0].native_started ||
        native_skill_leases[1].native_started ||
        tal_native_presentation_lease.active ||
        ailish_native_ranged_lease.active ||
        SudekiMpCleanroomEngineRangedCombatPrimePending();
}

static BOOL client_realtime_containment_active(void) {
    return SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
        client_session_authenticated(),
        InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0,
        client_native_transaction_retained(),
        client_replica_reset_pending) != 0;
}

static BOOL __attribute__((thiscall)) preserve_client_skill_camera(
    void *manager,
    const char *name
) {
    CameraManagerSetRenderCameraFunction original =
        (CameraManagerSetRenderCameraFunction)client_skill_camera_hook.trampoline;
    int routed=SudekiMpLanArenaRouteCastCamera(manager,name);
    if(routed==1) return TRUE;
    if(routed<0) return FALSE;
    if(routed==2) return original(manager,name);
    int caster_index = client_skill_replay_caster_index();
    if (!client_skill_replay_active()) {
        client_skill_camera_suppression_logged = FALSE;
        return original(manager, name);
    }
    if (caster_index == 1) {
        if (!client_skill_camera_suppression_logged) {
            client_skill_camera_suppression_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_skill_camera "
                "state=routed owner=Ailish local_caster=Ailish requested=%s "
                "policy=local_players_native_skill_camera_only\r\n",
                name == NULL ? "(null)" : name);
        }
        return original(manager, name);
    }
    if (!client_skill_camera_suppression_logged) {
        client_skill_camera_suppression_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill_camera "
            "state=preserved owner=Ailish remote_caster=%s requested=%s "
            "policy=only_the_local_players_skill_may_replace_this_view\r\n",
            caster_index == 0 ? "Tal" : "ambiguous",
            name == NULL ? "(null)" : name);
    }
    return TRUE;
}

static void __attribute__((thiscall)) preserve_client_skill_realtime(
    void *game_speed,
    int requested_mode
) {
    GameSpeedSetModeFunction original =
        (GameSpeedSetModeFunction)client_skill_speed_hook.trampoline;
    BOOL active = client_realtime_containment_active();
    BOOL success = TRUE;
    int trace_state;

    if (active) {
        success = set_client_skill_realtime_scale(TRUE);
        trace_state = success ? 1 : 0;
        if (trace_state != client_skill_speed_trace_state ||
            requested_mode != 0) {
            client_skill_speed_trace_state = trace_state;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_skill_speed "
                "state=%s requested_mode=%d applied_scale=1.0 "
                "policy=authenticated_or_retained_native_transaction_remains_realtime\r\n",
                success ? "realtime" : "rejected", requested_mode);
        }
    } else if (client_skill_speed_override_active) {
        success = set_client_skill_realtime_scale(FALSE);
        client_skill_speed_trace_state = success ? 2 : 0;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill_speed "
            "state=%s requested_mode=%d "
            "policy=restore_exact_preinstall_alternate_scale\r\n",
            success ? "restored" : "restore_rejected", requested_mode);
    }
    original(game_speed, requested_mode);
}

static void retire_client_skill_isolation_if_idle(void) {
    if (client_skill_replay_active()) return;
    if (client_remote_tal_skill_input_isolation_active &&
        !set_client_remote_tal_skill_input_isolation(
            FALSE, "no_native_skill_replay")) {
        return;
    }
    client_skill_camera_suppression_logged = FALSE;
    if (client_skill_speed_override_active &&
        !client_realtime_containment_active()) {
        if (!set_client_skill_realtime_scale(FALSE)) {
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_skill_speed "
                "state=restore_rejected win32_error=%lu "
                "policy=retain_exact_client_replica_hooks\r\n",
                (unsigned long)GetLastError());
        }
    }
    client_skill_speed_trace_state = -1;
}

static BOOL observe_native_skill_lease(
    unsigned int actor_index,
    BOOL *native_active
) {
    LanArenaNativeSkillPresentationLease *lease;
    SudekiMpCharacterSkillState state;

    if (actor_index >= 2u || native_active == NULL) return FALSE;
    *native_active = FALSE;
    lease = &native_skill_leases[actor_index];
    if (!lease->native_started) return TRUE;
    if (lease->character == NULL ||
        !SudekiMpObserveCharacterSkill(lease->character, &state)) {
        return FALSE;
    }
    if (state.skill != lease->skill ||
        (state.active != 0u && state.slot != (int)lease->slot)) {
        return FALSE;
    }
    *native_active = state.active != 0u;
    if (*native_active) lease->active_seen = TRUE;
    return TRUE;
}

static void clear_native_skill_activation_retry(
    LanArenaNativeSkillPresentationLease *lease
) {
    if (lease == NULL) return;
    lease->ranged_prime_requested = FALSE;
    SudekiMpLanArenaClientSkillRetryReset(&lease->retry_gate);
    lease->retry_exhaustion_logged = FALSE;
}

static void clear_native_skill_drain_marker(
    LanArenaNativeSkillPresentationLease *lease
) {
    if (lease == NULL) return;
    lease->drain_pending = FALSE;
    lease->drain_logged = FALSE;
    lease->pending_sequence = 0u;
    lease->pending_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE;
}

static void mark_native_skill_drain(
    unsigned int actor_index,
    uint16_t pending_sequence,
    uint8_t pending_kind,
    const char *reason
) {
    LanArenaNativeSkillPresentationLease *lease;
    if (actor_index >= 2u) return;
    lease = &native_skill_leases[actor_index];
    lease->drain_pending = TRUE;
    if (!lease->drain_logged ||
        lease->pending_sequence != pending_sequence ||
        lease->pending_kind != pending_kind) {
        lease->drain_logged = TRUE;
        lease->pending_sequence = pending_sequence;
        lease->pending_kind = pending_kind;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill_handoff "
            "state=draining actor=%s current_sequence=%u "
            "incoming_sequence=%u incoming_kind=%u reason=%s "
            "policy=retain_native_task_and_damage_guard_until_positive_inactive_observation\r\n",
            actor_index == 0u ? "Tal" : "Ailish",
            (unsigned int)lease->seen_sequence,
            (unsigned int)pending_sequence,
            (unsigned int)pending_kind,
            reason != NULL ? reason : "unspecified");
    }
}

static BOOL retire_native_skill_lease(
    unsigned int actor_index,
    const char *reason
) {
    LanArenaNativeSkillPresentationLease *lease;
    uint16_t sequence;
    uint8_t slot;
    if (actor_index >= 2u) return FALSE;
    lease = &native_skill_leases[actor_index];
    if (!lease->native_started) {
        clear_native_skill_drain_marker(lease);
        return TRUE;
    }
    sequence = lease->seen_sequence;
    slot = lease->slot;
    if (actor_index == 0u &&
        !set_client_remote_tal_skill_input_isolation(
            FALSE, reason != NULL ? reason : "native_task_drained")) {
        return FALSE;
    }
    if (actor_index == 0u &&
        remote_tal_skill_view_lease.owner_view.valid) {
        if (!retire_remote_tal_skill_view_lease(
                reason != NULL ? reason : "native_task_drained")) {
            return FALSE;
        }
    }
    lease->native_started = FALSE;
    lease->active_seen = FALSE;
    lease->skill = NULL;
    lease->completion_logged = TRUE;
    clear_native_skill_activation_retry(lease);
    lease->host_presentation_logged = FALSE;
    clear_native_skill_drain_marker(lease);
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_skill_handoff "
        "state=retired actor=%s sequence=%u slot=%u reason=%s "
        "policy=positive_native_inactive_observation_before_lease_release\r\n",
        actor_index == 0u ? "Tal" : "Ailish",
        (unsigned int)sequence,
        (unsigned int)slot,
        reason != NULL ? reason : "unspecified");
    retire_client_skill_isolation_if_idle();
    return TRUE;
}

static BOOL drain_native_skill_leases(const char *reason) {
    BOOL all_drained = TRUE;
    unsigned int actor_index;
    for (actor_index = 0u; actor_index < 2u; ++actor_index) {
        LanArenaNativeSkillPresentationLease *lease =
            &native_skill_leases[actor_index];
        BOOL observed_active = FALSE;
        BOOL observed;
        if (!lease->native_started) {
            clear_native_skill_drain_marker(lease);
            continue;
        }
        observed = observe_native_skill_lease(actor_index, &observed_active);
        if (SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
                lease->native_started, lease->active_seen,
                observed, observed_active)) {
            if (retire_native_skill_lease(actor_index, reason)) continue;
            mark_native_skill_drain(
                actor_index, lease->seen_sequence,
                SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
                "owner_view_restore_unconfirmed");
            all_drained = FALSE;
            continue;
        }
        mark_native_skill_drain(
            actor_index, lease->seen_sequence,
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER,
            !observed ? "native_state_unconfirmed" :
            observed_active ? "native_task_active" :
                "native_task_startup_gap");
        all_drained = FALSE;
    }
    return all_drained;
}

BOOL SudekiMpLanArenaClientReplicaDrainSkillReplay(void) {
    if (InterlockedCompareExchange(&client_skill_activation_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return drain_native_skill_leases("private_cast_namespace_retirement");
}

static BOOL client_damage_containment_active(void) {
    if (party_replica_session) return TRUE;
    return SudekiMpLanArenaClientSkillDamageContainmentRequired(
        client_session_authenticated(),
        InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0,
        client_native_transaction_retained(),
        client_replica_reset_pending) != 0;
}

static void __attribute__((thiscall)) block_client_replica_damage(
    void *combat,
    void *damage_structure
) {
    if (client_damage_containment_active()) {
        if (!client_damage_block_logged) {
            client_damage_block_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_client_replica event=client_native_damage "
                "state=blocked "
                "policy=host_authority_or_retained_native_skill_drain_owns_damage_guard\r\n");
        }
        return;
    }
    if (original_apply_damage != NULL) {
        original_apply_damage(combat, damage_structure);
    }
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

static void reset_client_ailish_ranged_refresh(DWORD now) {
    client_ailish_ranged_refresh_attempt_count = 0u;
    client_ailish_ranged_refresh_last_attempt_at = now;
    client_ailish_ranged_refresh_exhaustion_logged = FALSE;
}

static void reset_client_combat_transition_actor_leases(DWORD now) {
    ++client_combat_transition_generation;
    if (client_combat_transition_generation == 0u) {
        ++client_combat_transition_generation;
    }
    ZeroMemory(client_combat_transition_actor_leases,
        sizeof(client_combat_transition_actor_leases));
    reset_client_ailish_ranged_refresh(now);
}

static BOOL bind_client_combat_transition_actor(
    unsigned int actor_index,
    uint64_t session_token,
    uint8_t **character_result
) {
    const SudekiMpCleanroomActor actors[2] = {
        seat_host_actor(),
        seat_client_actor()
    };
    LanArenaCombatTransitionActorLease *lease;
    uint8_t *character;
    uint8_t *position = NULL;
    uint8_t *attached_wrapper = NULL;
    void *attached_renderer = NULL;
    uint8_t *component = NULL;
    uint8_t *first_person_wrapper = NULL;
    void *first_person_renderer = NULL;
    void *renderer = NULL;
    void *tal_lifecycle_actor = NULL;
    uint32_t actor_generation = 0u;
    BOOL available;
    BOOL identity_exact;

    if (actor_index >= 2u || character_result == NULL) return FALSE;
    *character_result = NULL;
    lease = &client_combat_transition_actor_leases[actor_index];
    character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(
        actors[actor_index]);
    available = session_token != 0u &&
        session_token == client_combat_transition_session_token &&
        client_combat_transition_generation != 0u &&
        readable_memory(character, 0x138u);
    if (available && actor_index == 0u) {
        tal_lifecycle_actor = client_remote_tal_lease_actor;
        actor_generation = client_remote_tal_lease_generation;
        available = SudekiMpLanArenaClientTalLifecycleLeaseExact(
            character, actor_generation,
            tal_lifecycle_actor, actor_generation);
    }
    if (available) {
        position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
        available = readable_memory(position,
            POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *));
    }
    if (available) {
        attached_wrapper = *(uint8_t **)(
            position + POSITION_ATTACHED_WRAPPER_OFFSET);
        available = readable_memory(attached_wrapper, 0x14u);
    }
    if (available) {
        attached_renderer = *(void **)(attached_wrapper + 0x10u);
        available = attached_renderer != NULL && actor_presentation_renderer(
            character, actor_index, &renderer, &component);
    }
    if (available && actor_index == 1u) {
        first_person_wrapper = *(uint8_t **)(
            component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
        available = readable_memory(first_person_wrapper, 0x14u);
        if (available) {
            first_person_renderer = *(void **)(
                first_person_wrapper + 0x10u);
            available = first_person_renderer != NULL;
        }
    }
    available = available &&
        SudekiMpCleanroomEngineActorEntity(actors[actor_index]) == character &&
        *(void **)(character + CHARACTER_POSITION_OFFSET) == position &&
        *(void **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) ==
            attached_wrapper &&
        *(void **)(attached_wrapper + 0x10u) == attached_renderer &&
        (actor_index != 0u ||
         (client_remote_tal_lease_actor == tal_lifecycle_actor &&
          client_remote_tal_lease_generation == actor_generation &&
          SudekiMpLanArenaClientTalLifecycleLeaseExact(
              character, actor_generation,
              tal_lifecycle_actor, actor_generation))) &&
        (actor_index != 1u ||
         (*(void **)(character + AILISH_RANGED_COMPONENT_OFFSET) == component &&
          *(void **)(component + 0x10u) == character &&
          *(void **)(component + AILISH_FIRST_PERSON_WRAPPER_OFFSET) ==
              first_person_wrapper &&
          *(void **)(first_person_wrapper + 0x10u) ==
              first_person_renderer));
    if (!available) {
        character = NULL;
        position = NULL;
        attached_wrapper = NULL;
        attached_renderer = NULL;
        renderer = NULL;
        component = NULL;
        first_person_wrapper = NULL;
        first_person_renderer = NULL;
    }
    identity_exact = available && lease->character == character &&
        lease->position == position &&
        lease->attached_wrapper == attached_wrapper &&
        lease->attached_renderer == attached_renderer &&
        lease->renderer == renderer && lease->component == component &&
        lease->first_person_wrapper == first_person_wrapper &&
        lease->first_person_renderer == first_person_renderer &&
        lease->session_token == session_token &&
        lease->transition_generation == client_combat_transition_generation &&
        lease->actor_generation == actor_generation;
    if (!identity_exact) {
        if (lease->ready) {
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_combat_presentation "
                "state=actor_readiness_revoked actor=%s "
                "policy=session_transition_and_full_renderer_identity_changed_or_unknown\r\n",
                actor_index == 0u ? "Tal" : "Ailish");
        }
        lease->character = character;
        lease->position = position;
        lease->attached_wrapper = attached_wrapper;
        lease->attached_renderer = attached_renderer;
        lease->renderer = renderer;
        lease->component = component;
        lease->first_person_wrapper = first_person_wrapper;
        lease->first_person_renderer = first_person_renderer;
        lease->session_token = available ? session_token : 0u;
        lease->transition_generation = available ?
            client_combat_transition_generation : 0u;
        lease->actor_generation = available ? actor_generation : 0u;
        lease->ready = FALSE;
    }
    if (!available) return FALSE;
    *character_result = character;
    return TRUE;
}

static BOOL synchronize_client_combat_mode(
    uint8_t combat_enabled,
    uint64_t session_token
) {
    BOOL current;
    BOOL desired;
    BOOL changed;
    BOOL lease_started;
    BOOL session_changed;
    BOOL transition_required;
    BOOL verified;
    DWORD error;
    if (combat_enabled > 1u || session_token == 0u ||
        !SudekiMpCleanroomEngineCombatMode(&current)) {
        error = combat_enabled > 1u || session_token == 0u ?
            ERROR_INVALID_DATA : ERROR_INVALID_STATE;
        trace_client_combat_mode(FALSE, combat_enabled != 0u, error);
        SetLastError(error);
        return FALSE;
    }
    lease_started = !client_combat_mode_lease_valid;
    if (lease_started) {
        client_original_combat_mode = current;
        client_combat_mode_lease_valid = TRUE;
    }
    desired = combat_enabled != 0u;
    changed = current != desired;
    session_changed =
        client_combat_transition_session_token != session_token;
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
    transition_required = changed || (lease_started && desired) ||
        (session_changed && desired);
    if (transition_required) {
        DWORD transition_started_at = GetTickCount();
        client_combat_transition_session_token = session_token;
        client_combat_transition_pending = TRUE;
        client_combat_transition_target = desired;
        client_combat_transition_started_at = transition_started_at;
        client_combat_transition_refresh_attempted = !desired;
        reset_client_combat_transition_actor_leases(transition_started_at);
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
    } else if (session_changed) {
        /* A new authenticated stream may reuse process-local addresses. Drop
         * every readiness lease even when both streams begin sheathed. If the
         * new stream is armed, transition_required above also re-proves the
         * native handoff before admitting presentation. */
        client_combat_transition_session_token = session_token;
        client_combat_transition_pending = FALSE;
        reset_client_combat_transition_actor_leases(GetTickCount());
    }
    trace_client_combat_mode(TRUE, desired, ERROR_SUCCESS);
    return TRUE;
}

static unsigned int client_combat_presentation_ready_mask(
    uint64_t session_token
) {
    SudekiMpCleanroomActorPresentation tal;
    SudekiMpCleanroomActorPresentation ailish;
    uint8_t *tal_character = NULL;
    uint8_t *ailish_character = NULL;
    int expected_ailish;
    BOOL tal_identity_available;
    BOOL ailish_identity_available;
    BOOL ailish_graph_ready;
    BOOL tal_observed_ready;
    BOOL ailish_observed_ready;
    BOOL tal_ready;
    BOOL ailish_ready;
    int observed_ailish_selector = -1;
    unsigned int ready_mask;
    DWORD now;
    DWORD elapsed;
    if (session_token == 0u ||
        session_token != client_combat_transition_session_token) {
        return 0u;
    }
    if (!client_combat_transition_pending) return 0x03u;
    now = GetTickCount();
    elapsed = now - client_combat_transition_started_at;
    /* SetCombatMode starts Sudeki's asynchronous native ranged/UI arm pass.
     * A replica-owned actor can miss the first party event while that pass is
     * rebuilding its weapon graph.  Re-run the already-proven native group
     * transition once after the 75 ms arm window; never synthesize selectors
     * against an unconfirmed graph and never refresh every frame. */
    if (SudekiMpLanArenaClientCombatTransitionRefreshDue(
            client_combat_transition_target,
            client_combat_transition_refresh_attempted,
            elapsed)) {
        client_combat_transition_refresh_attempted = TRUE;
        if (SudekiMpCleanroomEngineRefreshCombatMode()) {
            now = GetTickCount();
            client_combat_transition_started_at = now;
            elapsed = 0u;
            SudekiMpLogWrite(
                "lan_arena_client_replica event=client_combat_presentation "
                "state=native_refresh target=armed status=confirmed "
                "policy=one_shot_post_arm_group_transition\r\n");
        } else {
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_combat_presentation "
                "state=native_refresh target=armed status=rejected "
                "win32_error=%lu "
                "policy=fail_closed_no_selector_override\r\n",
                (unsigned long)GetLastError());
        }
        /* The group refresh itself starts another asynchronous native pass.
         * Do not call Ailish's actor-local model switch in this same frame. */
        client_ailish_ranged_refresh_last_attempt_at = GetTickCount();
    }
    expected_ailish = client_combat_transition_target ?
        client_combat_idle_selector() : AILISH_WORLD_IDLE_SELECTOR;
    ZeroMemory(&tal, sizeof(tal));
    ZeroMemory(&ailish, sizeof(ailish));
    now = GetTickCount();
    tal_identity_available = bind_client_combat_transition_actor(
        0u, session_token, &tal_character);
    ailish_identity_available = bind_client_combat_transition_actor(
        1u, session_token, &ailish_character);
    {
        /* TEMPORARY diagnostic: why tal_ready never settles on sheathed. */
        BOOL tal_pres = SudekiMpCleanroomEngineActorPresentation(
            seat_host_actor(), &tal);
        BOOL tal_entity = SudekiMpCleanroomEngineActorEntity(
            seat_host_actor()) == tal_character;
        tal_observed_ready = tal_identity_available && tal_pres &&
            tal_entity &&
            SudekiMpLanArenaClientTalTransitionSelectorReady(
                client_combat_transition_target, tal.selector[0]);
        {
            static DWORD last_tal_ready_trace;
            DWORD tr = GetTickCount();
            if (last_tal_ready_trace == 0u ||
                (DWORD)(tr - last_tal_ready_trace) >= 500u) {
                last_tal_ready_trace = tr;
                SudekiMpLogFormat(
                    "lan_arena_client_replica event=tal_ready_diag "
                    "target=%s identity=%u pres=%u entity=%u sel=%d ready=%u\r\n",
                    client_combat_transition_target ? "armed" : "sheathed",
                    tal_identity_available ? 1u : 0u,
                    tal_pres ? 1u : 0u,
                    tal_entity ? 1u : 0u,
                    (int)tal.selector[0],
                    tal_observed_ready ? 1u : 0u);
            }
        }
    }
    /* In first person, ActorPresentation observes Ailish's two-submodel arms
     * renderer, whose native idle selector is 1. Requiring world selector 20
     * from that surface can never settle and used to leave all client combat
     * replay disabled. Validate both of her independently resolved renderer
     * banks instead; apply_actor_presentation still verifies every write. */
    ailish_graph_ready = ailish_identity_available &&
        client_combat_transition_target &&
        client_ailish_combat_graph_ready(ailish_character);
    ailish_observed_ready = ailish_identity_available &&
        (client_combat_transition_target ?
            client_ailish_visible_combat_ready(ailish_character) :
            (SudekiMpCleanroomEngineActorPresentation(
                 seat_client_actor(), &ailish) &&
             ailish.selector[0] == expected_ailish));
    if (client_combat_transition_target && ailish_identity_available &&
        ailish_graph_ready && !ailish_observed_ready &&
        client_combat_transition_refresh_attempted &&
        SudekiMpLanArenaClientAilishRangedRefreshDue(
            client_ailish_ranged_refresh_attempt_count,
            now - client_ailish_ranged_refresh_last_attempt_at)) {
        BOOL refreshed;
        ++client_ailish_ranged_refresh_attempt_count;
        client_ailish_ranged_refresh_last_attempt_at = now;
        refreshed = refresh_ailish_ranged_presentation();
        if (refreshed) {
            /* The model switch may replace a renderer/component lease. Bind
             * and re-prove the complete visible topology before admission. */
            ailish_identity_available = bind_client_combat_transition_actor(
                1u, session_token, &ailish_character);
            ailish_observed_ready = ailish_identity_available &&
                client_ailish_visible_combat_ready(ailish_character);
        }
        if (!ailish_observed_ready &&
            client_ailish_ranged_refresh_attempt_count >=
                SUDEKIMP_LAN_ARENA_CLIENT_AILISH_REFRESH_MAX_ATTEMPTS &&
            !client_ailish_ranged_refresh_exhaustion_logged) {
            client_ailish_ranged_refresh_exhaustion_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_ailish_weapon_attachment "
                "state=retry_exhausted attempts=%u "
                "policy=bounded_native_model_refresh_fail_closed\r\n",
                client_ailish_ranged_refresh_attempt_count);
        }
    }
    if (client_combat_transition_target && ailish_observed_ready) {
        uint8_t *arbiter = *(uint8_t **)(
            ailish_character + CHARACTER_ARBITER_OFFSET);
        /* Do not admit replica selector writes while the native actor is
         * still drawing its weapon. Those writes can replace the animation
         * whose completion must publish the actual combat-ready state. Keep
         * this at initial transition only: an admitted actor's later native
         * skill/action owns its own temporary arbiter state. */
        ailish_observed_ready = readable_memory(arbiter, 0x61u) &&
            *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) == ailish_character &&
            SudekiMpLanArenaClientNativeArmingComplete(
                *(uint32_t *)(arbiter + 0x50u),
                *(uint32_t *)(arbiter + 0x58u), arbiter[0x60u]);
    }
    client_combat_transition_actor_leases[0].ready =
        SudekiMpLanArenaClientActorTransitionReadinessRetained(
            client_combat_transition_actor_leases[0].ready,
            tal_identity_available, tal_observed_ready);
    /* Ailish's admission is never sticky: her active character wrapper,
     * first-person renderer, combat graphs, and visible weapon witness are
     * all re-proved in the current frame. */
    client_combat_transition_actor_leases[1].ready =
        ailish_identity_available && ailish_observed_ready;
    tal_ready = client_combat_transition_actor_leases[0].ready;
    ailish_ready = client_combat_transition_actor_leases[1].ready;
    ready_mask =
        (SudekiMpLanArenaClientActorPresentationAllowed(
             0u, TRUE, tal_ready, ailish_ready) ? 0x01u : 0u) |
        (SudekiMpLanArenaClientActorPresentationAllowed(
             1u, TRUE, tal_ready, ailish_ready) ? 0x02u : 0u);
    {
        static DWORD last_ready_mask_trace;
        DWORD rm = GetTickCount();
        if (last_ready_mask_trace == 0u ||
            (DWORD)(rm - last_ready_mask_trace) >= 500u) {
            last_ready_mask_trace = rm;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=ready_mask_diag target=%s "
                "tal_obs=%u ailish_obs=%u tal_ready=%u ailish_ready=%u "
                "mask=0x%02x\r\n",
                client_combat_transition_target ? "armed" : "sheathed",
                tal_observed_ready ? 1u : 0u,
                ailish_observed_ready ? 1u : 0u,
                tal_ready ? 1u : 0u, ailish_ready ? 1u : 0u,
                ready_mask);
        }
    }
    if (tal_ready && ailish_ready) {
        if (client_combat_transition_target) {
            if (SudekiMpCleanroomEngineActorPresentation(
                    seat_client_actor(), &ailish) &&
                SudekiMpCleanroomEngineActorEntity(
                    seat_client_actor()) == ailish_character) {
                observed_ailish_selector = ailish.selector[0];
            }
        } else {
            observed_ailish_selector = ailish.selector[0];
        }
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
                    (long)tal.selector[0], (long)observed_ailish_selector);
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
            (long)tal.selector[0], (long)observed_ailish_selector,
            client_combat_transition_target ?
                "verified_graphs_and_native_weapon_attachment" :
                "attached_world");
        return 0x03u;
    }
    elapsed = GetTickCount() - client_combat_transition_started_at;
    if (elapsed >= 1500u && client_combat_transition_trace_state == 0) {
        client_combat_transition_trace_state = 2;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_combat_presentation "
            "state=waiting target=%s elapsed_ms=%lu ready_mask=0x%02x "
            "tal_ready=%s ailish_ready=%s ailish_refresh_attempts=%u "
            "policy=actor_local_override_only_after_native_weapon_ready\r\n",
            client_combat_transition_target ? "armed" : "sheathed",
            (unsigned long)elapsed, ready_mask,
            tal_ready ? "true" : "false",
            ailish_ready ? "true" : "false",
            client_ailish_ranged_refresh_attempt_count);
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
    client_combat_transition_session_token = 0u;
    client_combat_transition_started_at = 0u;
    client_combat_transition_refresh_attempted = FALSE;
    reset_client_combat_transition_actor_leases(0u);
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

BOOL SudekiMpLanArenaClientReplicaHostCombatState(BOOL *enabled) {
    if (enabled == NULL || !client_session_authenticated() ||
        !replica_diagnostics.valid ||
        last_applied_snapshot.match_state != 1u ||
        last_applied_snapshot.combat_enabled > 1u) {
        if (enabled != NULL) *enabled = FALSE;
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    *enabled = last_applied_snapshot.combat_enabled != 0u;
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaRangedAim(void *actor, float direction[3]) {
    if (!actor || !direction || !client_session_authenticated() ||
        !replica_diagnostics.valid || !last_applied_snapshot.combat_enabled ||
        (DWORD)(GetTickCount()-replica_diagnostics.sampled_at_ms)>250u) return FALSE;
    for (unsigned seat=0;seat<2;++seat) {
        const SudekiMpLanArenaActorSnapshot *sample=&last_applied_snapshot.seat[seat];
        if (actor==last_applied_characters[seat] &&
            actor==SudekiMpCleanroomEngineActorEntity(seat ? seat_client_actor():seat_host_actor()) &&
            sample->ranged_aim_valid && !sample->skill_active && sample->hp) {
            for (unsigned axis=0;axis<3;++axis) direction[axis]=sample->ranged_aim[axis]/32767.0f;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL SudekiMpLanArenaClientReplicaRangedTarget(void *actor,float target[3]) {
    float direction[3];
    if(!target || !SudekiMpLanArenaClientReplicaRangedAim(actor,direction)) return FALSE;
    for(unsigned seat=0;seat<2;++seat) {
        const SudekiMpLanArenaActorSnapshot *s=&last_applied_snapshot.seat[seat];
        if(actor==last_applied_characters[seat] && s->ranged_target_valid) {
            memcpy(target,s->ranged_target,12); return TRUE;
        }
    }
    return FALSE;
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

/* TEMPORARY diagnostic: dump the client actor's world animation table
 * (index -> handle -> selector) so the per-character locomotion_ids /
 * locomotion_selectors tables can be extended for Elco. REMOVE after capture. */
static void dump_client_world_animation_table(
    uint8_t *component,
    void *renderer
) {
    static DWORD last_dump_at = 0u;
    static unsigned int dump_count = 0u;
    uint8_t *animation_table;
    void **vtable;
    AnimationLookupFunction lookup;
    unsigned int animation_id;
    DWORD now = GetTickCount();
    if (dump_count >= 6u) return;
    if (last_dump_at != 0u && (DWORD)(now - last_dump_at) < 2000u) return;
    if (game_base == NULL || !readable_memory(component, 0x168u) ||
        !readable_memory(renderer, sizeof(void *)) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        return;
    }
    vtable = *(void ***)renderer;
    if (!readable_memory(vtable, 0x44u) ||
        vtable[0x40u / sizeof(void *)] !=
            game_base + RVA_ANIMATION_RENDERER_LOOKUP) {
        return;
    }
    animation_table = *(uint8_t **)(
        component + AILISH_ANIMATION_TABLE_OFFSET);
    if (animation_table == NULL ||
        !readable_memory(animation_table, 0x14u + 0x8du * sizeof(void *))) {
        return;
    }
    last_dump_at = now;
    ++dump_count;
    lookup = (AnimationLookupFunction)vtable[0x40u / sizeof(void *)];
    SudekiMpLogFormat(
        "lan_arena_client_replica event=world_anim_table_dump pass=%u "
        "policy=temporary_capture\r\n", dump_count);
    for (animation_id = 0u; animation_id < 0x8du; ++animation_id) {
        if (animation_id >= 25u && animation_id != 0x85u &&
            animation_id != 0x8cu) continue;
        uint8_t *details = *(uint8_t **)(
            animation_table + 0x14u + animation_id * sizeof(void *));
        uint32_t handle_a;
        uint32_t handle_b;
        int sel_a;
        int sel_b;
        if (details == NULL || !readable_memory(details, 0x28u)) continue;
        memcpy(&handle_a, details + 0x14u, sizeof(handle_a));
        memcpy(&handle_b, details + 0x20u, sizeof(handle_b));
        sel_a = (handle_a != 0u && handle_a != 0x0007ffffu) ?
            lookup(renderer, (int)(int32_t)handle_a) : -1;
        sel_b = (handle_b != 0u && handle_b != 0x0007ffffu) ?
            lookup(renderer, (int)(int32_t)handle_b) : -1;
        if (sel_a > 0 || sel_b > 0) {
            SudekiMpLogFormat(
                "lan_arena_client_replica event=world_anim_table_dump "
                "index=%u sel_0x14=%d sel_0x20=%d\r\n",
                animation_id, sel_a, sel_b);
        }
    }
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
        uint8_t *attached_wrapper;
        uint8_t *first_person_wrapper;
        uint8_t *saved_world_wrapper;

        if (!readable_memory(character,
                AILISH_RANGED_COMPONENT_OFFSET + sizeof(void *))) {
            return FALSE;
        }
        component = *(uint8_t **)(
            character + AILISH_RANGED_COMPONENT_OFFSET);
        if (!readable_memory(component, 0x168u) ||
            *(void **)(component + 0x10u) != character) return FALSE;
        saved_world_wrapper = *(uint8_t **)(
            component + AILISH_WORLD_WRAPPER_OFFSET);
        if (saved_world_wrapper != NULL) {
            /* Retail Ailish retains the detached world presentation here.
             * Do not let the currently attached first-person/world choice
             * displace that exact native lease when it exists. */
            wrapper = saved_world_wrapper;
        } else {
            /* The LAN cleanroom's native Player-2 construction can omit the
             * saved +0x164 lease while leaving its world renderer attached at
             * CPosition+0xB4. Admit only that exact out-of-combat topology:
             * the attached and +0x160 wrappers must both exist, be readable,
             * and be distinct. Re-read every owning slot and the component
             * backpointer before exposing the fallback so an asynchronous
             * native presentation switch fails closed. */
            if (!readable_memory(position,
                    POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *))) {
                return FALSE;
            }
            attached_wrapper = *(uint8_t **)(
                position + POSITION_ATTACHED_WRAPPER_OFFSET);
            first_person_wrapper = *(uint8_t **)(
                component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
            if (!readable_memory(attached_wrapper, 0x14u) ||
                !readable_memory(first_person_wrapper, 0x14u) ||
                attached_wrapper == first_person_wrapper ||
                *(void **)(character + CHARACTER_POSITION_OFFSET) !=
                    position ||
                *(void **)(character + AILISH_RANGED_COMPONENT_OFFSET) !=
                    component ||
                *(void **)(component + 0x10u) != character ||
                *(void **)(component + AILISH_FIRST_PERSON_WRAPPER_OFFSET) !=
                    first_person_wrapper ||
                *(void **)(component + AILISH_WORLD_WRAPPER_OFFSET) != NULL ||
                *(void **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) !=
                    attached_wrapper) {
                return FALSE;
            }
            wrapper = attached_wrapper;
        }
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

#ifdef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_TESTING
BOOL SudekiMpLanArenaClientReplicaTestActorPresentationRenderer(
    void *character,
    unsigned int actor_index,
    void **renderer_result,
    void **ailish_component_result
) {
    uint8_t *component = NULL;
    BOOL result;
    if (ailish_component_result == NULL) return FALSE;
    *ailish_component_result = NULL;
    result = actor_presentation_renderer(
        (uint8_t *)character, actor_index, renderer_result, &component);
    *ailish_component_result = component;
    return result;
}

BOOL SudekiMpLanArenaClientReplicaTestAilishDesiredModelAttached(
    void *character_value,
    void **attached_wrapper_result,
    void **attached_renderer_result,
    BOOL *first_person_result,
    BOOL *fallback_world_result
) {
    uint8_t *character = (uint8_t *)character_value;
    uint8_t *component;
    uint8_t *arbiter;
    uint8_t *position;
    LanArenaAilishModelWitness witness;

    if (attached_wrapper_result == NULL || attached_renderer_result == NULL ||
        first_person_result == NULL || fallback_world_result == NULL) {
        return FALSE;
    }
    *attached_wrapper_result = NULL;
    *attached_renderer_result = NULL;
    *first_person_result = FALSE;
    *fallback_world_result = FALSE;
    if (!readable_memory(character, 0x138u)) return FALSE;
    component = *(uint8_t **)(character + AILISH_RANGED_COMPONENT_OFFSET);
    arbiter = *(uint8_t **)(character + CHARACTER_ARBITER_OFFSET);
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    if (!ailish_desired_model_attached(
            character, component, arbiter, position, &witness)) {
        return FALSE;
    }
    *attached_wrapper_result = witness.attached_wrapper;
    *attached_renderer_result = witness.attached_renderer;
    *first_person_result = witness.first_person;
    *fallback_world_result = witness.fallback_world;
    return TRUE;
}

int SudekiMpLanArenaClientReplicaTestAilishModelAttachmentState(
    void *character_value
) {
    uint8_t *character = (uint8_t *)character_value;
    uint8_t *component;
    uint8_t *arbiter;
    uint8_t *position;

    if (!readable_memory(character, 0x138u)) {
        return (int)LAN_ARENA_AILISH_MODEL_ATTACHMENT_UNKNOWN;
    }
    component = *(uint8_t **)(character + AILISH_RANGED_COMPONENT_OFFSET);
    arbiter = *(uint8_t **)(character + CHARACTER_ARBITER_OFFSET);
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    return (int)ailish_model_attachment_state(
        character, component, arbiter, position, NULL);
}

BOOL SudekiMpLanArenaClientReplicaTestAilishModelRefreshAllowed(
    void *character
) {
    return SudekiMpLanArenaClientReplicaTestAilishModelAttachmentState(
        character) == (int)LAN_ARENA_AILISH_MODEL_ATTACHMENT_OPPOSITE;
}

BOOL SudekiMpLanArenaClientReplicaTestWeaponParentMatchesPosition(
    void *weapon,
    void *position
) {
    return weapon_primary_parent_matches_position(
        (uint8_t *)weapon, (uint8_t *)position);
}

static BOOL test_ailish_weapon_mutation_anchors(
    void *character_value,
    uint8_t **component_result,
    uint8_t **arbiter_result,
    uint8_t **position_result,
    uint8_t **weapon_result,
    uint8_t **active_model_result,
    uint8_t **active_owner_result,
    LanArenaAilishModelWitness *model_result
) {
    uint8_t *character = (uint8_t *)character_value;
    uint8_t *component;
    uint8_t *arbiter;
    uint8_t *position;
    uint8_t *weapon;
    uint8_t *active_model;
    uint8_t *active_owner;

    if (component_result == NULL || arbiter_result == NULL ||
        position_result == NULL || weapon_result == NULL ||
        active_model_result == NULL || active_owner_result == NULL ||
        model_result == NULL || !readable_memory(character, 0x138u)) {
        return FALSE;
    }
    component = *(uint8_t **)(
        character + AILISH_RANGED_COMPONENT_OFFSET);
    arbiter = *(uint8_t **)(character + CHARACTER_ARBITER_OFFSET);
    position = *(uint8_t **)(character + CHARACTER_POSITION_OFFSET);
    weapon = *(uint8_t **)(character + CHARACTER_WEAPON_OFFSET);
    if (!readable_memory(weapon, WEAPON_FLAGS_OFFSET + sizeof(uint8_t))) {
        return FALSE;
    }
    active_model = *(uint8_t **)(weapon + WEAPON_ACTIVE_MODEL_OFFSET);
    if (!readable_memory(active_model, 0x10u)) return FALSE;
    active_owner = *(uint8_t **)(active_model + 0x0cu);
    if (!ailish_desired_model_attached(
            character, component, arbiter, position, model_result) ||
        !ailish_ranged_pointer_graph_exact(
            character, component, arbiter, position, weapon,
            active_model, active_owner)) {
        return FALSE;
    }
    *component_result = component;
    *arbiter_result = arbiter;
    *position_result = position;
    *weapon_result = weapon;
    *active_model_result = active_model;
    *active_owner_result = active_owner;
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaTestAilishWeaponReattachMutationAllowed(
    void *character_value
) {
    uint8_t *component;
    uint8_t *arbiter;
    uint8_t *position;
    uint8_t *weapon;
    uint8_t *active_model;
    uint8_t *active_owner;
    LanArenaAilishModelWitness model;
    if (!test_ailish_weapon_mutation_anchors(
            character_value, &component, &arbiter, &position, &weapon,
            &active_model, &active_owner, &model)) {
        return FALSE;
    }
    return ailish_weapon_reattach_graph_writable(
        (uint8_t *)character_value, component, arbiter, position, weapon,
        active_model, active_owner, &model, NULL);
}

BOOL SudekiMpLanArenaClientReplicaTestAilishWeaponVisibilityMutationAllowed(
    void *character_value
) {
    uint8_t *component;
    uint8_t *arbiter;
    uint8_t *position;
    uint8_t *weapon;
    uint8_t *active_model;
    uint8_t *active_owner;
    LanArenaAilishModelWitness model;
    if (!test_ailish_weapon_mutation_anchors(
            character_value, &component, &arbiter, &position, &weapon,
            &active_model, &active_owner, &model)) {
        return FALSE;
    }
    return ailish_weapon_visibility_graph_writable(
        (uint8_t *)character_value, component, arbiter, position, weapon,
        active_model, active_owner, NULL);
}

BOOL SudekiMpLanArenaClientReplicaTestRemoteTalReleaseActivationEntryBlocked(
    void
) {
    LONG prior_depth;
    int prior_actor_index;
    BOOL blocked;

    if (native_skill_leases[0].native_started ||
        tal_native_presentation_lease.active) {
        return FALSE;
    }
    prior_actor_index = client_skill_activation_actor_index;
    client_skill_activation_actor_index = 0;
    prior_depth = InterlockedExchange(&client_skill_activation_depth, 1);
    SetLastError(ERROR_SUCCESS);
    blocked = !SudekiMpLanArenaClientReplicaRemoteTalReleaseReady() &&
        GetLastError() == ERROR_BUSY;
    InterlockedExchange(&client_skill_activation_depth, prior_depth);
    client_skill_activation_actor_index = prior_actor_index;
    return blocked;
}
#endif

BOOL SudekiMpLanArenaClientNativeArmingComplete(
    uint32_t arbiter_flags, uint32_t arbiter_state, uint8_t combat_request
) {
    uint32_t mode = arbiter_state & 0x0fu;
    /* Same combat and pending-arm predicates used by retail skill readiness
     * at RVA db9b0. Renderer availability alone does not complete arming. */
    return (arbiter_flags & 2u) != 0u && (combat_request & 2u) != 0u &&
        mode != 1u && mode != 3u;
}

static BOOL client_ailish_combat_graph_ready(
    uint8_t *expected_character
) {
    uint8_t *character = expected_character;
    uint8_t *component = NULL;
    uint8_t *first_person_wrapper = NULL;
    void *world_renderer = NULL;
    void *first_person_renderer = NULL;
    LanArenaAnimationMethods methods;
    uint32_t handle;
    int selector;
    const char *failure = NULL;

    if (character == NULL ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            character ||
        !actor_presentation_renderer(
            character, 1u, &world_renderer, &component) ||
        !animation_methods(world_renderer, &methods) ||
        methods.count(world_renderer) == 0u) {
        failure = "world_renderer";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x02u,
            client_combat_idle_selector())) {
        failure = "world_idle";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x06u,
            client_combat_move_primary_selector())) {
        failure = "world_move_primary";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x07u,
            client_combat_move_secondary_selector())) {
        failure = "world_move_secondary";
    } else if (!resolve_ailish_world_selector(
            component, world_renderer, 0x85u,
            client_combat_fire_selector())) {
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
            SudekiMpLanArenaRangedCombatSelector(seat_client_type(), 0x05u),
            &handle, &selector)) {
        failure = "first_person_idle";
    } else if (!resolve_ailish_first_person_selector(
            component, first_person_renderer, 0x8cu,
            SudekiMpLanArenaRangedCombatSelector(seat_client_type(), 0x8cu),
            &handle, &selector)) {
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
    if (SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            character ||
        *(void **)(character + AILISH_RANGED_COMPONENT_OFFSET) != component ||
        *(void **)(component + 0x10u) != character ||
        *(void **)(component + AILISH_FIRST_PERSON_WRAPPER_OFFSET) !=
            first_person_wrapper ||
        *(void **)(first_person_wrapper + 0x10u) != first_person_renderer) {
        failure = "post_graph_identity";
        goto rejected;
    }
    client_ailish_combat_graph_failure = NULL;
    return TRUE;
}

static BOOL client_ailish_visible_combat_ready(
    uint8_t *expected_character
) {
    uint8_t *component;
    uint8_t *arbiter;
    uint8_t *position;
    uint8_t *weapon = NULL;
    uint8_t *render_object = NULL;
    LanArenaAilishModelWitness before_model;
    LanArenaAilishModelWitness after_model;

    if (expected_character == NULL ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            expected_character ||
        !readable_memory(expected_character, 0x138u)) {
        return FALSE;
    }
    component = *(uint8_t **)(
        expected_character + AILISH_RANGED_COMPONENT_OFFSET);
    arbiter = *(uint8_t **)(expected_character + CHARACTER_ARBITER_OFFSET);
    position = *(uint8_t **)(expected_character + CHARACTER_POSITION_OFFSET);
    if (!ailish_desired_model_attached(
            expected_character, component, arbiter, position, &before_model) ||
        !client_ailish_combat_graph_ready(expected_character) ||
        !ailish_weapon_attachment_witness(
            expected_character, component, &weapon, &render_object, NULL) ||
        !ailish_desired_model_attached(
            expected_character, component, arbiter, position, &after_model) ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) !=
            expected_character ||
        before_model.position != after_model.position ||
        before_model.attached_wrapper != after_model.attached_wrapper ||
        before_model.first_person_wrapper !=
            after_model.first_person_wrapper ||
        before_model.saved_world_wrapper != after_model.saved_world_wrapper ||
        before_model.attached_renderer != after_model.attached_renderer ||
        before_model.first_person_renderer !=
            after_model.first_person_renderer ||
        before_model.saved_world_renderer !=
            after_model.saved_world_renderer ||
        before_model.first_person != after_model.first_person ||
        before_model.fallback_world != after_model.fallback_world ||
        *(void **)(expected_character + CHARACTER_WEAPON_OFFSET) != weapon ||
        render_object == NULL) {
        return FALSE;
    }
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

static BOOL synchronize_channel_phase(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    float phase_time
) {
    unsigned int submodel;
    const float tolerance =
        (float)ACTION_PHASE_TIME_TOLERANCE_MILLI / 1000.0f;
    if (renderer == NULL || methods == NULL ||
        submodels == 0u || submodels > 32u ||
        !isfinite(phase_time) || phase_time < 0.0f) return FALSE;
    for (submodel = 0u; submodel < submodels; ++submodel) {
        float actual = methods->get_time(
            renderer, channel, submodel);
        if (SudekiMpLanArenaClientAnimationPhaseCorrectionRequired(
                actual, phase_time)) {
            methods->set_time(renderer, channel, submodel,
                phase_time, 0);
            actual = methods->get_time(renderer, channel, submodel);
        }
        if (!isfinite(actual) ||
            fabsf(actual - phase_time) > tolerance) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL synchronize_action_phase(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    const SudekiMpLanArenaActorSnapshot *snapshot
) {
    float phase_time;
    if (snapshot == NULL) return FALSE;
    if (!snapshot->action_phase_valid) return TRUE;
    if (!SudekiMpLanArenaClientActionPhaseTime(snapshot, &phase_time)) {
        return FALSE;
    }
    return synchronize_channel_phase(
        renderer, methods, submodels, channel, phase_time);
}

static BOOL animation_channel_matches(
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    float rate
);

BOOL SudekiMpLanArenaClientShouldStartWeaponSwap(
    BOOL same_owner, BOOL first_person, BOOL attacking,
    unsigned int previous_slot, unsigned int current_slot
) {
    return same_owner && first_person && !attacking &&
        previous_slot >= 1u && previous_slot <= 12u &&
        current_slot >= 1u && current_slot <= 12u &&
        previous_slot != current_slot;
}

BOOL SudekiMpLanArenaClientWeaponSwapComplete(float native_time) {
    /* The supported renderer clamps a 14-frame one-shot to 13.9999f.
     * A sub-frame tolerance admits that terminal without waiting forever
     * for the unreachable exact resource length. */
    return isfinite(native_time) && native_time >= 14.0f - 0.001f;
}

static BOOL ailish_swap_resource_matches(void *renderer) {
    uint8_t *bank;
    uint8_t *entries;
    uint8_t *resource;
    /* Same read-only length layout as the supported renderer's native
     * GetAnimationLength. Selector lookup must already have validated C1. */
    if (!readable_memory(renderer, 0x0cu)) return FALSE;
    bank = *(uint8_t **)((uint8_t *)renderer + 8u);
    if (!readable_memory(bank, 0x24u)) return FALSE;
    entries = *(uint8_t **)(bank + 0x20u);
    if (!readable_memory(entries, 9u * 28u)) return FALSE;
    resource = *(uint8_t **)(entries + 8u * 28u);
    return readable_memory(resource, 8u) &&
        *(float *)(resource + 4u) == 14.0f;
}

BOOL SudekiMpLanArenaClientNativeRangedIdle(
    unsigned int stage, BOOL active_record, unsigned int animation_id,
    float cooldown
) {
    return stage == 0u && !active_record &&
        (animation_id == 5u || animation_id == 2u) &&
        isfinite(cooldown) && cooldown <= 0.0f;
}

static BOOL observe_native_ranged(
    const LanArenaNativeRangedLease *lease, BOOL *idle
) {
    uint8_t *record;
    uint8_t *animation;
    float cooldown = 0.0f;
    if (idle == NULL || !readable_memory(lease->character, 0x138u) ||
        *(uint8_t **)(lease->character + 0xbcu) != lease->combat ||
        *(uint8_t **)(lease->character + 0x90u) != lease->arbiter ||
        *(uint8_t **)(lease->character + 0x134u) != lease->component ||
        !readable_memory(lease->combat, 0xe4u) ||
        *(void **)lease->combat != game_base + 0x002d4c8cu ||
        *(uint8_t **)(lease->combat + 0x10u) != lease->character ||
        !readable_memory(lease->arbiter, 0x64u) ||
        *(uint8_t **)(lease->arbiter + 0x10u) != lease->character ||
        !readable_memory(lease->component, 0x168u) ||
        *(uint8_t **)(lease->component + 0x10u) != lease->character)
        return FALSE;
    record = *(uint8_t **)(lease->combat + 0x60u);
    if (record != NULL) {
        uint8_t **rows = *(uint8_t ***)(lease->combat + 0x4cu);
        unsigned int count = *(unsigned int *)(lease->combat + 0x44u);
        unsigned int i;
        if (count == 0u || count > 64u ||
            !readable_memory(rows, count * sizeof(void *))) return FALSE;
        for (i = 0u; i < count && rows[i] != record; ++i) {}
        if (i == count || !readable_memory(record, 0xc4u)) return FALSE;
        cooldown = *(float *)(record + 0xc0u);
        if (!isfinite(cooldown)) return FALSE;
    }
    animation = *(uint8_t **)(lease->component + 0xf8u);
    if (!readable_memory(animation, 4u)) return FALSE;
    *idle = SudekiMpLanArenaClientNativeRangedIdle(
        lease->combat[0xe0u],
        *(void **)(lease->combat + 0x5cu) != NULL,
        animation[2], cooldown);
    return TRUE;
}

static BOOL drain_ailish_native_ranged(void) {
    BOOL idle;
    if (!ailish_native_ranged_lease.active) return TRUE;
    if (!observe_native_ranged(&ailish_native_ranged_lease, &idle) || !idle) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_native_ranged phase=complete sequence=%u\r\n",
        (unsigned int)ailish_native_ranged_lease.sequence);
    ailish_native_ranged_lease.active = FALSE;
    ZeroMemory(&ailish_first_person_lease, sizeof(ailish_first_person_lease));
    return TRUE;
}

static void *elco_native_weapon_start_scope;
static DWORD elco_native_weapon_start_thread;

BOOL SudekiMpLanArenaClientReplicaWeaponStartAuthorized(void *actor) {
    const LanArenaNativeRangedLease *lease = &ailish_native_ranged_lease;
    return actor && actor == elco_native_weapon_start_scope &&
        GetCurrentThreadId() == elco_native_weapon_start_thread &&
        lease->active && lease->weapon_shot && !lease->weapon_fired &&
        lease->character == actor && client_apply_damage_hook.installed;
}

void SudekiMpLanArenaClientReplicaNativeWeaponFired(void *actor) {
    LanArenaNativeRangedLease *lease = &ailish_native_ranged_lease;
    SudekiMpElcoWeaponObservation observed;
    if (seat_client_type() != SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        actor != SudekiMpCleanroomEngineActorEntity(seat_client_actor()) ||
        !client_apply_damage_hook.installed ||
        !SudekiMpObserveElcoWeapon(actor, &observed)) return;
    if (lease->active && lease->weapon_shot && !lease->weapon_fired &&
        lease->character == actor && observed.item == lease->weapon_item) {
        lease->weapon_fired = TRUE;
        SudekiMpLogFormat("lan_weapon event=client_shot item=%u sequence=%u\r\n",
            observed.item, lease->sequence);
    } else {
        /* Count actual native emissions outside an admitted playback event,
         * not charge drops (resource synchronization changes charge too). */
        SudekiMpLogFormat("lan_weapon event=client_unconfirmed_shot item=%u lease_active=%u sequence=%u\r\n",
            observed.item, (unsigned)lease->active, lease->sequence);
    }
}

static BOOL service_elco_native_weapon(uint8_t *character, uint8_t *component,
    void *renderer, BOOL final_boundary, BOOL *owns) {
    LanArenaNativeRangedLease *lease = &ailish_native_ranged_lease;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpElcoWeaponObservation native;
    const SudekiMpLanWeaponState *w = &replica.latest.seat[1].weapon;
    BOOL idle, fresh;
    *owns = lease->active;
    if (!SudekiMpLanArenaSessionGetStatus(&status) || !status.peer_connected ||
        !status.session_token || status.local_role != SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH ||
        status.local_simulation_node_role != SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA ||
        !client_apply_damage_hook.installed || !replica.latest_valid) return FALSE;
    fresh = weapon_snapshot_received_at &&
        (DWORD)(GetTickCount()-weapon_snapshot_received_at) <= 500u &&
        replica.latest.seat[1].actor_type == SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
        w->valid && SudekiMpLanWeaponStateValid(w, SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    if (elco_weapon_cursor_owner != character ||
        elco_weapon_cursor_session != status.session_token) {
        if (lease->active) return FALSE;
        elco_weapon_cursor_owner = character;
        elco_weapon_cursor_session = status.session_token;
        /* Joining a running session must not fire its historical shots. */
        elco_weapon_cursor = fresh && w->shot_count ? w->shots[w->shot_count-1].sequence : 0;
    }
    if (lease->active) {
        if (lease->character != character || lease->component != component ||
            lease->renderer != renderer) return FALSE;
        if (!final_boundary && lease->weapon_fired && fresh && w->item == lease->weapon_item) {
            uint16_t reload_ms;
            if (!SudekiMpLanWeaponPlaybackReloadMs(w, lease->weapon_item,
                    lease->sequence, replica.latest.host_tick, &reload_ms) ||
                !SudekiMpSyncElcoPresentationResources(character,
                    w->item, w->charge_q8, reload_ms)) return FALSE;
        }
        if (!observe_native_ranged(lease, &idle)) return FALSE;
        if (!idle) return TRUE;
        if (!drain_ailish_native_ranged()) return FALSE;
        *owns = FALSE;
    }
    if (final_boundary || !fresh || !client_ailish_first_person_camera_owns_facing(character) ||
        replica.latest.seat[1].skill_active ||
        !SudekiMpObserveElcoWeapon(character, &native) || native.item != w->item)
        return TRUE;
    {
        LanArenaNativeRangedLease observed = {0};
        const SudekiMpLanWeaponShot *shot = NULL;
        uint8_t *animation;
        observed.character = character; observed.component = component;
        observed.renderer = renderer;
        observed.combat = *(uint8_t **)(character+0xbcu);
        observed.arbiter = *(uint8_t **)(character+0x90u);
        if (!observe_native_ranged(&observed, &idle)) return FALSE;
        shot = SudekiMpLanWeaponNextShot(w, native.item, replica.latest.host_tick,
            &elco_weapon_cursor);
        animation = *(uint8_t **)(component+0xf8u); /* validated by observer */
        /* A confirmed shot may begin after the previous native clip retires.
         * The local resource display is not a second authority gate: the
         * event supplies its exact pre-shot charge, even if the host is now
         * reloading. Never interrupt an active native animation/task. */
        if (shot && native.stage == 0u && !*(void **)(observed.combat+0x5cu) &&
            (animation[2] == 5u || animation[2] == 2u)) {
            uint32_t handle; int selector;
            uint8_t *record = *(uint8_t **)(observed.combat+0x60u);
            unsigned id = *(uint32_t *)(record+0x9cu);
            if (elco_native_weapon_start_scope || id < 140u || id > 142u || shot->pre_charge_q8/256.0f < native.required_charge ||
                !resolve_ailish_first_person_selector(component, renderer, id,
                    SudekiMpLanArenaRangedCombatSelector(seat_client_type(), (uint8_t)id),
                    &handle, &selector) ||
                !SudekiMpSetElcoPresentationResources(character, shot->item, shot->pre_charge_q8, 0u))
                return FALSE;
            observed.sequence = shot->sequence;
            observed.weapon_item = shot->item;
            observed.weapon_shot = TRUE;
            observed.active = TRUE;
            *lease = observed; *owns = TRUE; /* retain damage/teardown barrier before entry */
            elco_weapon_cursor = shot->sequence;
            /* Permission exists only for this synchronous native start, not
             * throughout its asynchronous animation/reload lifetime. */
            elco_native_weapon_start_scope = character;
            elco_native_weapon_start_thread = GetCurrentThreadId();
            SudekiMpSubmitArbiterCombatInput(game_base+RVA_ARBITER_COMBAT_INPUT,
                lease->arbiter, 1, 0, 0, 0, 0, 0);
            elco_native_weapon_start_scope = NULL;
            elco_native_weapon_start_thread = 0u;
            SudekiMpLogFormat("lan_weapon event=client_submit item=%u sequence=%u precharge=%u\r\n",
                shot->item, shot->sequence, shot->pre_charge_q8);
            return TRUE;
        }
        if (!shot && !SudekiMpSyncElcoPresentationResources(character,
                w->item, w->charge_q8, w->reload_ms)) return FALSE;
        if (!idle && (native.stage != 0u || *(void **)(observed.combat+0x5cu) ||
                (*(uint32_t *)(observed.combat+0xdcu) >= 0xc2u &&
                 *(uint32_t *)(observed.combat+0xdcu) <= 0xc3u))) {
            observed.sequence = lease->sequence;
            observed.active = TRUE;
            *lease = observed; *owns = TRUE;
        }
    }
    return TRUE;
}

static BOOL service_ailish_native_ranged(
    uint8_t *character, uint8_t *component, void *renderer,
    const SudekiMpLanArenaActorSnapshot *snapshot, BOOL final_boundary,
    BOOL *owns
) {
    if (seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE)
        return service_elco_native_weapon(character, component, renderer, final_boundary, owns);
    LanArenaNativeRangedLease *lease = &ailish_native_ranged_lease;
    BOOL idle;
    BOOL shot = snapshot->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    *owns = lease->active;
    if (lease->active) {
        if (lease->character != character || lease->component != component ||
            lease->renderer != renderer) return FALSE;
        if (!observe_native_ranged(lease, &idle)) return FALSE;
        if (!idle) {
            if (shot && snapshot->action_sequence != 0u)
                lease->sequence = snapshot->action_sequence;
            return TRUE;
        }
        if (!drain_ailish_native_ranged()) return FALSE;
        *owns = FALSE;
    }
    if (!client_ailish_first_person_camera_owns_facing(character)) return TRUE;
    if (!shot) {
        LanArenaNativeRangedLease observed = {0};
        observed.character = character;
        observed.component = component;
        observed.renderer = renderer;
        observed.combat = *(uint8_t **)(character + 0xbcu);
        observed.arbiter = *(uint8_t **)(character + 0x90u);
        if (!observe_native_ranged(&observed, &idle)) return FALSE;
        /* Native recovery can start during an equipment transition or just
         * before the host action snapshot arrives. Do not overwrite an
         * already-active CCombat sequence in either window. */
        if (!idle && (observed.combat[0xe0u] != 0u ||
                *(void **)(observed.combat + 0x5cu) != NULL ||
                (*(uint32_t *)(observed.combat + 0xdcu) >= 0xc2u &&
                 *(uint32_t *)(observed.combat + 0xdcu) <= 0xc3u))) {
            observed.sequence = lease->character == character ? lease->sequence : 0u;
            observed.active = TRUE;
            *lease = observed;
            *owns = TRUE;
            return TRUE;
        }
    }
    if (!shot || snapshot->action_sequence == 0u) return TRUE;
    *owns = TRUE;
    if (final_boundary || (lease->character == character &&
            lease->sequence == snapshot->action_sequence)) return TRUE;
    if (!readable_memory(character, 0x138u)) return FALSE;
    lease->character = character;
    lease->component = component;
    lease->renderer = renderer;
    lease->combat = *(uint8_t **)(character + 0xbcu);
    lease->arbiter = *(uint8_t **)(character + 0x90u);
    if (!observe_native_ranged(lease, &idle)) return FALSE;
    /* A native shot/recharge may already be in flight. Yield to it rather
     * than replacing its clip or manufacturing a second input edge. */
    if (!idle) {
        lease->sequence = snapshot->action_sequence;
        lease->active = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_native_ranged phase=adopt sequence=%u\r\n",
            (unsigned int)lease->sequence);
        return TRUE;
    }
    /* Validate the native weapon-selected first-person animation. CCombat
     * selects its row by the equipped item's ID, not the inventory slot. */
    {
        uint8_t *weapon = *(uint8_t **)(character + 0xc0u);
        uint8_t *item;
        uint8_t *record = *(uint8_t **)(lease->combat + 0x60u);
        unsigned int id;
        uint32_t handle;
        int selector;
        if (!readable_memory(weapon, 0x26cu) ||
            *(uint8_t **)(weapon + 0x10u) != character) return FALSE;
        item = *(uint8_t **)(weapon + 0x268u);
        if (!readable_memory(item, 0x18u) || !readable_memory(record, 0xc4u) ||
            *(uint32_t *)(record + 8u) != *(uint32_t *)(item + 0x14u)) return FALSE;
        id = *(uint32_t *)(record + 0x9cu);
        if (id < 0x8cu || id > 0x8eu ||
            !resolve_ailish_first_person_selector(component, renderer,
                id, SudekiMpLanArenaRangedCombatSelector(
                    seat_client_type(), (uint8_t)id),
                &handle, &selector)) return FALSE;
    }
    lease->sequence = snapshot->action_sequence;
    lease->active = TRUE; /* Publish the teardown/damage barrier before entry. */
    SudekiMpSubmitArbiterCombatInput(game_base + RVA_ARBITER_COMBAT_INPUT,
        lease->arbiter, 1, 0, 0, 0, 0, 0);
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_native_ranged phase=submit sequence=%u "
        "policy=host_confirmed_native_weapon_sequence_client_damage_blocked\r\n",
        (unsigned int)lease->sequence);
    return TRUE;
}

static BOOL apply_ailish_first_person_presentation(
    uint8_t *character,
    uint8_t *component,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    BOOL final_boundary
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
    BOOL same_owner;
    BOOL swap_transition;
    BOOL swap_finished = FALSE;
    BOOL native_ranged_owns;

    if (!readable_memory(character,
            AILISH_RANGED_COMPONENT_OFFSET + sizeof(void *)) ||
        !readable_memory(component, 0x168u) ||
        *(uint8_t **)(character + AILISH_RANGED_COMPONENT_OFFSET) != component ||
        *(void **)(component + 0x10u) != character) {
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
            SudekiMpLanArenaRangedCombatSelector(seat_client_type(), 0x05u),
            &idle_handle, &idle_selector)) {
        return ailish_first_person_reject(
            "idle_lookup", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    if (!resolve_ailish_first_person_selector(
            component, renderer, 0x8cu,
            SudekiMpLanArenaRangedCombatSelector(seat_client_type(), 0x8cu),
            &fire_handle, &fire_selector)) {
        return ailish_first_person_reject(
            "fire_lookup", component, renderer,
            idle_handle, idle_selector, fire_handle, fire_selector);
    }
    weak_attack = seat_client_type() != SUDEKIMP_LAN_ARENA_ELCO_TYPE && snapshot->combat_state ==
        SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    if (!service_ailish_native_ranged(character, component, renderer,
            snapshot, final_boundary, &native_ranged_owns)) return FALSE;
    if (native_ranged_owns) return TRUE;
    same_owner = ailish_first_person_lease.valid &&
        ailish_first_person_lease.character == character &&
        ailish_first_person_lease.component == component &&
        ailish_first_person_lease.renderer == renderer;
    swap_transition = SudekiMpLanArenaClientShouldStartWeaponSwap(
        same_owner, client_ailish_first_person_camera_owns_facing(character),
        weak_attack, ailish_first_person_lease.weapon_slot,
        snapshot->weapon_slot_plus_one);
    if (swap_transition || (same_owner &&
            ailish_first_person_lease.weapon_swap && !weak_attack)) {
        uint32_t swap_handle;
        int swap_selector;
        unsigned int submodel;
        BOOL complete = !swap_transition;
        /* C1 resolves only in the first-person bank. This is renderer-only
         * playback after host equipment confirmation: no CArbiter state,
         * animation event dispatch, inventory cycle, or native task starts.
         * The supported C1 resource has 14 frames at its authored 24 fps. */
        if (!resolve_ailish_first_person_selector(component, renderer,
                0xc1u, 8, &swap_handle, &swap_selector) ||
            !ailish_swap_resource_matches(renderer)) return FALSE;
        if (swap_transition) {
            set_animation_channel(renderer, &methods, submodels,
                0, swap_selector, 1, 24.0f, TRUE);
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_weapon_swap "
                "phase=start slot=%u selector=%d policy=host_confirmed_fp_visual_only\r\n",
                (unsigned int)snapshot->weapon_slot_plus_one, swap_selector);
        } else {
            for (submodel = 0u; submodel < submodels; ++submodel) {
                float time = methods.get_time(renderer, 0, submodel);
                if (!isfinite(time) || time < 0.0f) return FALSE;
                if (methods.get_selector(renderer, 0, submodel) != swap_selector) {
                    /* Another presentation owner replaced this visual clip.
                     * Do not reset its clock or restart it every frame. */
                    complete = TRUE;
                    break;
                }
                if (!SudekiMpLanArenaClientWeaponSwapComplete(time))
                    complete = FALSE;
            }
        }
        if (!complete) {
            ailish_first_person_lease.character = character;
            ailish_first_person_lease.component = component;
            ailish_first_person_lease.renderer = renderer;
            ailish_first_person_lease.weapon_slot = snapshot->weapon_slot_plus_one;
            ailish_first_person_lease.weapon_swap = TRUE;
            ailish_first_person_lease.valid = TRUE;
            return TRUE;
        }
        swap_finished = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_weapon_swap phase=end slot=%u\r\n",
            (unsigned int)snapshot->weapon_slot_plus_one);
    }
    transition = !ailish_first_person_lease.valid ||
        ailish_first_person_lease.character != character ||
        ailish_first_person_lease.component != component ||
        ailish_first_person_lease.renderer != renderer ||
        ailish_first_person_lease.weak_attack != weak_attack ||
        ailish_first_person_lease.action_sequence != snapshot->action_sequence ||
        swap_finished || ailish_first_person_lease.weapon_swap;
    if (!transition) {
        ailish_first_person_lease.weapon_slot = snapshot->weapon_slot_plus_one;
        return !weak_attack || synchronize_action_phase(
            renderer, &methods, submodels, 0, snapshot);
    }
    selector = weak_attack ? fire_selector : idle_selector;
    state = weak_attack ? 1 : 128;
    set_animation_channel(
        renderer, &methods, submodels, 0, selector, state, 24.0f,
        weak_attack || swap_finished);
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
    ailish_first_person_lease.weapon_slot = snapshot->weapon_slot_plus_one;
    ailish_first_person_lease.weapon_swap = FALSE;
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

static BOOL service_tal_native_action_presentation(
    uint8_t *character,
    void *renderer,
    const LanArenaAnimationMethods *methods,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    BOOL combat_mode,
    BOOL final_presentation_boundary,
    BOOL *native_owns_presentation
) {
    LanArenaTalNativePresentationLease *lease =
        &tal_native_presentation_lease;
    uint8_t *arbiter;
    int weak, strong, sweep, block;
    int expected_selector;
    int expected_state;
    int current_selector;
    int current_state;
    DWORD now_ms;
    BOOL host_body_phase;

    if (native_owns_presentation == NULL || character == NULL ||
        renderer == NULL || methods == NULL || snapshot == NULL) return FALSE;
    *native_owns_presentation = FALSE;
    if (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
        /* LA40 starts no native combo task for Buki. Her four host-observed
         * channels own the outcome and its transition back to locomotion.
         * A retained legacy task is NOT permission to cancel it: refuse the
         * handoff rather than clearing a live lease on a timeout. Matching
         * LA40 processes start with no such lease. Tal's path is unchanged. */
        *native_owns_presentation = lease->active;
        if (lease->active) SetLastError(ERROR_BUSY);
        return !lease->active;
    }
    host_body_phase = seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE &&
        SudekiMpLanArenaBukiBodyAction(snapshot->action_variant);
    if (!combat_mode && !lease->active) return TRUE;
    if (lease->active &&
        (lease->character != character || lease->renderer != renderer)) {
        /* A changed actor or renderer is not evidence that the asynchronous
         * native action completed. Retain its exact lease so teardown cannot
         * remove the actor or damage hook underneath a still-running task. */
        *native_owns_presentation = TRUE;
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (!final_presentation_boundary && !host_body_phase &&
        snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        snapshot->action_sequence != 0u &&
        snapshot->action_sequence != lease->submitted_sequence) {
        if (!SudekiMpLanArenaClientTalNativeCombatInput(
                snapshot->action_variant, &weak, &strong, &sweep, &block) ||
            !(seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
                SudekiMpLanArenaClientBukiActionPresentation(
                    snapshot->action_variant,
                    &expected_selector, &expected_state) :
                SudekiMpLanArenaClientTalActionPresentation(
                    snapshot->action_variant,
                    &expected_selector, &expected_state)) ||
            !readable_memory(character,
                CHARACTER_ARBITER_OFFSET + sizeof(void *))) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        (void)expected_state;
        arbiter = *(uint8_t **)(character + CHARACTER_ARBITER_OFFSET);
        if (!readable_memory(arbiter, 0x64u) ||
            *(void **)(arbiter + ARBITER_CHARACTER_OFFSET) != character) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        /* The native combat-input call may synchronously re-enter lifecycle
         * observation. Publish the exact drain barrier before dispatch so a
         * Tal loss cannot release this actor during the call-entry window. */
        lease->character = character;
        lease->arbiter = arbiter;
        lease->renderer = renderer;
        lease->submitted_sequence = snapshot->action_sequence;
        lease->submitted_variant = snapshot->action_variant;
        lease->expected_selector = expected_selector;
        lease->submitted_at_ms = GetTickCount();
        lease->expected_selector_seen = FALSE;
        lease->timeout_logged = FALSE;
        lease->block_interrupt_attempted = FALSE;
        lease->active = TRUE;
        /* Each journal edge is one host-accepted swing, including Buki's
         * opening swings. Replaying W/WW/WWW here duplicates earlier inputs
         * and changes both timing and combo history. Preserve native pacing. */
        SudekiMpSubmitArbiterCombatInput(game_base + RVA_ARBITER_COMBAT_INPUT,
            arbiter, weak, strong, sweep, block, 0, 0);
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_tal_native_action "
            "state=submitted sequence=%u variant=%u expected_selector=%d "
            "input=%s "
            "policy=authenticated_presentation_edge_client_damage_blocked\r\n",
            (unsigned int)lease->submitted_sequence,
            (unsigned int)lease->submitted_variant,
            lease->expected_selector,
            weak ? "weak" : strong ? "strong" : sweep ? "sweep" : "block");
    }
    if (!lease->active) return TRUE;
    if (!final_presentation_boundary && host_body_phase &&
        snapshot->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK &&
        snapshot->locomotion.valid && !snapshot->skill_active &&
        !lease->block_interrupt_attempted) {
        SudekiMpCharacterSkillState skill;
        if (SudekiMpCleanroomEngineActorEntity(seat_host_actor()) != character ||
            !SudekiMpObserveCharacterSkill(character, &skill) || skill.active) {
            *native_owns_presentation = TRUE;
            return FALSE;
        }
        /* Mark before the synchronous native call: lifecycle observation may
         * re-enter. Unknown cleanup must retain the barrier, never retry an
         * unverified cancellation on every presentation tick. */
        lease->block_interrupt_attempted = TRUE;
        if (!SudekiMpBukiReplicaInterruptAttack((HMODULE)game_base,
                character, lease->arbiter)) {
            *native_owns_presentation = TRUE;
            SetLastError(ERROR_BUSY);
            return FALSE;
        }
        SudekiMpLogFormat(
            "lan_arena_client_replica event=buki_attack_block_handoff "
            "sequence=%u block_sequence=%u state=native_cleanup_complete\r\n",
            (unsigned)lease->submitted_sequence,
            (unsigned)snapshot->action_sequence);
        ZeroMemory(lease, sizeof(*lease));
        return TRUE;
    }
    if (!readable_memory(lease->arbiter, 0x64u) ||
        *(void **)((uint8_t *)lease->arbiter + ARBITER_CHARACTER_OFFSET) !=
            character) {
        /* Unknown/foreign ownership is fail-closed, never a retirement edge. */
        *native_owns_presentation = TRUE;
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    current_selector = methods->get_selector(renderer, 0, 0u);
    current_state = methods->get_state(renderer, 0, 0u);
    /* TEMPORARY diagnostic: trace the replica selector through the action
     * lease so we can see whether the native arbiter actually advances the
     * combo (53 -> 54 -> 65) or stays idle (20). REMOVE after capture. */
    {
        static DWORD last_replica_action_selector_diag;
        DWORD diag_now = GetTickCount();
        if (last_replica_action_selector_diag == 0u ||
            (DWORD)(diag_now - last_replica_action_selector_diag) >= 120u) {
            last_replica_action_selector_diag = diag_now;
            SudekiMpLogFormat(
                "lan_arena_client_replica "
                "event=client_replica_action_selector_diag sequence=%u "
                "selector=%d state=%d\r\n",
                (unsigned int)lease->submitted_sequence,
                current_selector, current_state);
        }
    }
    if (current_selector == lease->expected_selector) {
        lease->expected_selector_seen = TRUE;
    }
    *native_owns_presentation = TRUE;
    if (snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        !host_body_phase) {
        return TRUE;
    }
    if (lease->expected_selector_seen &&
        current_selector == host_combat_idle_selector() &&
        current_state != 192) {
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_tal_native_action "
            "state=retired sequence=%u selector=%d state_value=%d "
            "policy=native_client_animation_state_machine_handoff\r\n",
            (unsigned int)lease->submitted_sequence,
            current_selector, current_state);
        ZeroMemory(lease, sizeof(*lease));
        /* A positively drained combo can hand its renderer to the observed
         * block/run phase now. Never cancel a live native action to do so. */
        if (host_body_phase) *native_owns_presentation = FALSE;
        return TRUE;
    }
    now_ms = GetTickCount();
    if ((DWORD)(now_ms - lease->submitted_at_ms) >= 3000u &&
        !lease->timeout_logged) {
        lease->timeout_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_tal_native_action "
            "state=draining sequence=%u expected_selector=%d "
            "current_selector=%d current_state=%d observed=%u "
            "reason=native_retirement_timeout "
            "policy=retain_actor_damage_guard_and_hooks_until_positive_idle\r\n",
            (unsigned int)lease->submitted_sequence,
            lease->expected_selector, current_selector, current_state,
            lease->expected_selector_seen ? 1u : 0u);
    }
    return TRUE;
}

#ifdef SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_TESTING
BOOL SudekiMpLanArenaClientReplicaTestBukiNoNativeCombo(
    const SudekiMpLanArenaActorSnapshot *snapshot, BOOL retained,
    BOOL final_boundary
) {
    uint8_t actor[0x180] = {0};
    LanArenaAnimationMethods methods = {0};
    LanArenaTalNativePresentationLease saved = tal_native_presentation_lease;
    BOOL owns = FALSE, result, untouched;
    if (seat_host_type() != SUDEKIMP_LAN_ARENA_BUKI_TYPE) return FALSE;
    ZeroMemory(&tal_native_presentation_lease, sizeof(tal_native_presentation_lease));
    tal_native_presentation_lease.active = retained;
    result = service_tal_native_action_presentation(actor, actor, &methods,
        snapshot, TRUE, final_boundary, &owns);
    untouched = tal_native_presentation_lease.active == retained &&
        tal_native_presentation_lease.character == NULL &&
        tal_native_presentation_lease.submitted_sequence == 0u;
    tal_native_presentation_lease = saved;
    return result == !retained && owns == retained && untouched;
}
#endif

static BOOL drain_tal_native_action_lease(void) {
    LanArenaAnimationMethods methods;
    int current_selector;
    int current_state;
    if (!tal_native_presentation_lease.active) return TRUE;
    if (tal_native_presentation_lease.character == NULL ||
        tal_native_presentation_lease.renderer == NULL ||
        tal_native_presentation_lease.arbiter == NULL ||
        !readable_memory(tal_native_presentation_lease.character,
            CHARACTER_ARBITER_OFFSET + sizeof(void *)) ||
        *(void **)((uint8_t *)tal_native_presentation_lease.character +
            CHARACTER_ARBITER_OFFSET) !=
            tal_native_presentation_lease.arbiter ||
        !readable_memory(tal_native_presentation_lease.arbiter, 0x64u) ||
        *(void **)((uint8_t *)tal_native_presentation_lease.arbiter +
            ARBITER_CHARACTER_OFFSET) !=
            tal_native_presentation_lease.character ||
        !animation_methods(
            tal_native_presentation_lease.renderer, &methods)) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    current_selector = methods.get_selector(
        tal_native_presentation_lease.renderer, 0, 0u);
    current_state = methods.get_state(
        tal_native_presentation_lease.renderer, 0, 0u);
    if (current_selector ==
            tal_native_presentation_lease.expected_selector) {
        tal_native_presentation_lease.expected_selector_seen = TRUE;
    }
    if (!tal_native_presentation_lease.expected_selector_seen ||
        current_selector != host_combat_idle_selector() ||
        current_state == 192) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_tal_native_action "
        "state=retired sequence=%u selector=%d state_value=%d "
        "reason=authority_teardown_positive_idle\r\n",
        (unsigned int)tal_native_presentation_lease.submitted_sequence,
        current_selector, current_state);
    ZeroMemory(&tal_native_presentation_lease,
        sizeof(tal_native_presentation_lease));
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaRemoteTalReleaseReady(void) {
    /* The native replay entry owns its character before STARTED can publish
     * the asynchronous CSkill lease below. A reentrant lifecycle observer in
     * that window must not mistake the empty lease for permission to remove
     * Tal out from under the call. */
    if (InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0 ||
        InterlockedCompareExchange(
            &client_spirit_vfx_call_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (native_skill_leases[0].native_started) {
        BOOL observed_active = FALSE;
        BOOL observed = observe_native_skill_lease(0u, &observed_active);
        if (!SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
                native_skill_leases[0].native_started,
                native_skill_leases[0].active_seen,
                observed, observed_active) ||
            !retire_native_skill_lease(0u, "remote_tal_release")) {
            SetLastError(ERROR_BUSY);
            return FALSE;
        }
    }
    if (!release_client_spirit_vfx_cache("remote_tal_release")) return FALSE;
    return drain_tal_native_action_lease();
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
        (moving ? client_world_move_rate() : 12.0f);
    float rate_one;
    float expected_blend_zero = moving ? 0.99f : 0.0f;
    float blend_zero = methods->get_blend(renderer, 0);
    if (combat_mode) {
        if (actor_index == 0u) {
            int action_state;
            if (weak_attack &&
                !(seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
                    SudekiMpLanArenaClientBukiActionPresentation(
                        action_variant, &selector_zero, &action_state) :
                    SudekiMpLanArenaClientTalActionPresentation(
                        action_variant, &selector_zero, &action_state))) {
                return FALSE;
            }
            (void)action_state;
            if (!weak_attack) {
                selector_zero = moving ? host_combat_move_primary_selector() :
                    host_combat_idle_selector();
            }
            selector_one = moving ? host_combat_move_secondary_selector() : 0;
            rate_zero = weak_attack ? 24.0f :
                (moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f);
            rate_one = moving ? TAL_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        } else {
            selector_zero = moving ? client_combat_move_primary_selector() :
                client_combat_idle_selector();
            selector_one = moving ? client_combat_move_secondary_selector() : 0;
            rate_one = moving ? client_combat_move_secondary_rate() : 0.0f;
        }
    } else {
        selector_zero = actor_index == 0u ?
            (moving ? host_world_move_primary_selector() :
                host_world_idle_selector()) :
            (moving ? client_world_move_selector() : AILISH_WORLD_IDLE_SELECTOR);
        selector_one = moving ?
            (actor_index == 0u ? host_world_move_secondary_selector() :
                client_world_move_secondary_selector()) : 0;
        rate_one = moving ?
            (actor_index == 0u ? TAL_WORLD_MOVE_SECONDARY_RATE :
                client_world_move_secondary_rate()) : 0.0f;
    }
    if (!combat_mode && SudekiMpLanArenaClientIdleVariantSelector(
            actor_index == 0u ? seat_host_type() :
                seat_client_type(),
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
        if (seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) return TRUE;
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
                (combat_mode ? client_combat_fire_selector() :
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
            combat_mode ? client_combat_move_primary_selector() :
                client_world_move_selector(),
            combat_mode ? client_combat_move_rate() :
                client_world_move_rate()) &&
        animation_channel_matches(
            renderer, methods, submodels, 1,
            combat_mode ? client_combat_move_secondary_selector() :
                client_world_move_secondary_selector(),
            combat_mode ? client_combat_move_secondary_rate() :
                client_world_move_secondary_rate()) &&
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

static BOOL apply_host_skill_presentation(
    uint8_t *character,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    unsigned int actor_index,
    BOOL synchronize_phase
) {
    LanArenaNativeSkillPresentationLease *lease;
    LanArenaAnimationMethods methods;
    uint8_t *ailish_component;
    void *renderer;
    unsigned int submodels;
    unsigned int channel;
    unsigned int expected_channels;
    SudekiMpCharacterSkillState local_skill;
    if (character == NULL || snapshot == NULL || actor_index >= 2u) {
        return FALSE;
    }
    if (snapshot->skill_presentation_valid == 0u) return TRUE;
    expected_channels = snapshot->actor_type == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
        4u : snapshot->actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE ? 2u :
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS;
    if (snapshot->skill_sequence == 0u || snapshot->skill_active == 0u ||
        snapshot->skill_presentation_channel_count != expected_channels ||
        !actor_presentation_renderer(
            character, actor_index, &renderer, &ailish_component) ||
        !animation_methods(renderer, &methods)) {
        return FALSE;
    }
    lease = &native_skill_leases[actor_index];
    submodels = methods.count(renderer);
    if (submodels == 0u || submodels > 32u) return FALSE;
    if (actor_index == 0u && seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE &&
        !SudekiMpCleanroomBukiAnimationStorageValid(renderer, submodels, TRUE))
        return FALSE;
    /* Character skills already have a real local CSkill task. Never let wire
     * values select, transition, or phase-steer its native animation bank.
     * Treat the host tuple only as a witness when the exact actor/CSkill/
     * sequence/slot and every current local selector/state/rate already match.
     * A delayed replacement therefore cannot overwrite the task being drained. */
    if (snapshot->skill_kind ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER) {
        if (!lease->native_started || lease->character != character ||
            lease->skill == NULL ||
            lease->seen_sequence != snapshot->skill_sequence ||
            lease->slot != snapshot->skill_slot ||
            !SudekiMpObserveCharacterSkill(character, &local_skill) ||
            local_skill.skill != lease->skill || local_skill.active == 0u ||
            local_skill.slot != (int)lease->slot) {
            return TRUE;
        }
        for (channel = 0u; channel < expected_channels; ++channel) {
            unsigned int submodel;
            for (submodel = 0u; submodel < submodels; ++submodel) {
                float local_rate = methods.get_rate(
                    renderer, (int)channel, submodel);
                if (methods.get_selector(
                        renderer, (int)channel, submodel) !=
                        snapshot->skill_presentation_selector[channel] ||
                    methods.get_state(
                        renderer, (int)channel, submodel) !=
                        snapshot->skill_presentation_state[channel] ||
                    !isfinite(local_rate) || fabsf(local_rate -
                        snapshot->skill_presentation_rate[channel]) > 0.001f) {
                    return TRUE;
                }
            }
        }
        (void)synchronize_phase;
        return TRUE;
    } else if (snapshot->skill_kind ==
                   SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
        if (lease->native_started ||
            (snapshot->actor_type != SUDEKIMP_LAN_ARENA_TAL_TYPE &&
             snapshot->actor_type != SUDEKIMP_LAN_ARENA_BUKI_TYPE &&
             snapshot->actor_type != SUDEKIMP_LAN_ARENA_ELCO_TYPE) ||
            !SudekiMpLanArenaSkillPresentationValid(
                snapshot, snapshot->actor_type)) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
    } else {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    for (channel = 0u; channel < expected_channels; ++channel) {
        unsigned int submodel;
        set_animation_channel(
            renderer, &methods, submodels, (int)channel,
            snapshot->skill_presentation_selector[channel],
            snapshot->skill_presentation_state[channel],
            snapshot->skill_presentation_rate[channel], FALSE);
        if (synchronize_phase && !synchronize_channel_phase(
                renderer, &methods, submodels, (int)channel,
                snapshot->skill_presentation_time[channel])) {
            return FALSE;
        }
        for (submodel = 0u; submodel < submodels; ++submodel) {
            float actual_rate = methods.get_rate(
                renderer, (int)channel, submodel);
            if (methods.get_selector(
                    renderer, (int)channel, submodel) !=
                    snapshot->skill_presentation_selector[channel] ||
                methods.get_state(
                    renderer, (int)channel, submodel) !=
                    snapshot->skill_presentation_state[channel] ||
                !isfinite(actual_rate) || fabsf(actual_rate -
                    snapshot->skill_presentation_rate[channel]) > 0.001f) {
                return FALSE;
            }
        }
    }
    if (snapshot->actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE) {
        const int blend_channels[2] = { 0, 3 };
        for (channel = 0u; channel < 2u; ++channel) {
            int blend_channel = blend_channels[channel];
            float expected =
                snapshot->skill_presentation_blend[blend_channel];
            float actual;
            methods.set_blend(renderer, blend_channel, expected);
            actual = methods.get_blend(renderer, blend_channel);
            if (!isfinite(actual) || fabsf(actual - expected) > 0.001f) {
                return FALSE;
            }
        }
    } else {
        for (channel = 0u;
             channel < (snapshot->actor_type == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
                 3u : SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_BLENDS);
             ++channel) {
            float expected = snapshot->skill_presentation_blend[channel];
            float actual;
            methods.set_blend(renderer, (int)channel, expected);
            actual = methods.get_blend(renderer, (int)channel);
            if (!isfinite(actual) || fabsf(actual - expected) > 0.001f) {
                return FALSE;
            }
        }
    }
    if (!lease->host_presentation_logged ||
        lease->host_presentation_selector !=
            snapshot->skill_presentation_selector[0] ||
        lease->host_presentation_state !=
            snapshot->skill_presentation_state[0]) {
        lease->host_presentation_logged = TRUE;
        lease->host_presentation_selector =
            snapshot->skill_presentation_selector[0];
        lease->host_presentation_state =
            snapshot->skill_presentation_state[0];
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill_presentation "
            "state=channel_transition actor=%s sequence=%u kind=%u slot=%u channels=%u "
            "selector0=%ld state0=%u rate0=%.5f "
            "policy=host_observed_exact_build_render_channels_no_gameplay_authority\r\n",
            actor_index == 0u ? "Tal" : "Ailish",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_kind,
            (unsigned int)snapshot->skill_slot,
            expected_channels,
            (long)snapshot->skill_presentation_selector[0],
            (unsigned int)snapshot->skill_presentation_state[0],
            snapshot->skill_presentation_rate[0]);
    }
    return TRUE;
}

static BOOL service_native_skill_presentation(
    uint8_t *character,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    unsigned int actor_index,
    BOOL host_combat_authorized,
    BOOL *native_owns_presentation
) {
    LanArenaNativeSkillPresentationLease *lease;
    SudekiMpCharacterSkillState state;
    SudekiMpSkillActivationResult result;
    SudekiMpCleanroomActor actor;
    SudekiMpLanArenaClientSkillHandoffDecision handoff;
    SudekiMpLanArenaClientSkillRetryDecision retry_decision;
    uint32_t topped_sp;
    BOOL local_state_observed = TRUE;
    BOOL local_native_active = FALSE;
    BOOL host_resources_restored;
    SudekiMpLanArenaSessionStatus replay_status;

    if (native_owns_presentation == NULL || character == NULL ||
        snapshot == NULL || actor_index >= 2u) return FALSE;
    *native_owns_presentation = FALSE;
    if (actor_index == 1u && !drain_ailish_native_ranged()) {
        /* Keep the native shot/recharge alive before considering a skill.
         * Ordinary ranged presentation below still needs to service it. */
        if (snapshot->skill_kind != SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_NONE)
            return FALSE;
    }
    lease = &native_skill_leases[actor_index];
    if (snapshot->skill_sequence != 0u &&
        snapshot->skill_kind !=
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
        snapshot->skill_kind !=
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
        return FALSE;
    }
    if (lease->native_started) {
        local_state_observed = observe_native_skill_lease(
            actor_index, &local_native_active);
    }
    handoff = SudekiMpLanArenaClientSkillHandoffDecide(
        lease->native_started,
        lease->active_seen,
        lease->seen_sequence,
        snapshot->skill_sequence,
        snapshot->skill_kind,
        local_state_observed,
        local_native_active);
    if (handoff == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN) {
        mark_native_skill_drain(
            actor_index, snapshot->skill_sequence, snapshot->skill_kind,
            local_state_observed ? "prior_native_task_active" :
                "prior_native_state_unconfirmed");
        /* Do not run another native task over the retained CSkill. Host
         * channels may still present the incoming transaction while the old
         * task drains under the damage guard. */
        *native_owns_presentation = TRUE;
        return TRUE;
    }
    if (handoff ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETIRE_THEN_ACCEPT) {
        if (!retire_native_skill_lease(
                actor_index, "implicit_host_handoff")) {
            mark_native_skill_drain(
                actor_index, snapshot->skill_sequence,
                snapshot->skill_kind,
                "owner_view_restore_unconfirmed");
            *native_owns_presentation = TRUE;
            return TRUE;
        }
        local_state_observed = TRUE;
        local_native_active = FALSE;
    }

    /* Spirit Strike owns one retail-global transaction on the canonical
     * world. The replica must never call CSkill::Use for it: consume only the
     * host-observed actor-local renderer channels without acquiring native
     * global camera, resource, or gameplay authority on this client. */
    if (snapshot->skill_kind ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
        BOOL new_sequence = snapshot->skill_sequence != 0u &&
            snapshot->skill_sequence != lease->seen_sequence;
        if (new_sequence) {
            lease->seen_sequence = snapshot->skill_sequence;
            lease->character = character;
            lease->slot = 0u;
            lease->completion_logged = FALSE;
            lease->ranged_prime_requested = FALSE;
            lease->host_presentation_logged = FALSE;
            lease->native_started = FALSE;
            lease->active_seen = FALSE;
            lease->skill = NULL;
            clear_native_skill_activation_retry(lease);
            clear_native_skill_drain_marker(lease);
            memset(&presentation_leases[actor_index], 0,
                sizeof(presentation_leases[actor_index]));
            if (actor_index == 0u) {
                memset(&tal_native_presentation_lease, 0,
                    sizeof(tal_native_presentation_lease));
            }
            if (snapshot->skill_active != 0u) {
                SudekiMpLogFormat(
                    "lan_arena_client_replica event=client_spirit "
                    "phase=started actor_type=%u sequence=%u "
                    "policy=host_global_transaction_actor_local_presentation_only\r\n",
                    (unsigned int)snapshot->actor_type,
                    (unsigned int)snapshot->skill_sequence);
            }
        }
        if (snapshot->skill_sequence != 0u &&
            snapshot->skill_sequence == lease->seen_sequence &&
            snapshot->skill_active == 0u && !lease->completion_logged) {
            lease->completion_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_spirit "
                "phase=completed actor_type=%u sequence=%u "
                "policy=host_global_transaction_retired_without_local_task\r\n",
                (unsigned int)snapshot->actor_type,
                (unsigned int)lease->seen_sequence);
        }
        *native_owns_presentation = snapshot->skill_sequence != 0u &&
            snapshot->skill_active != 0u;
        return TRUE;
    }
    /* The authenticated client may run a presentation-only native task for
     * either configured actor in an admitted pair. Camera selection, game speed,
     * damage, and resources are independently contained below; this is what
     * creates native skill effects that animation-channel replication alone
     * cannot display. Unknown actors remain snapshot-only and fail closed. */
    if (!SudekiMpLanArenaClientNativeSkillTaskAllowed(
            snapshot->actor_type, seat_client_type())) {
        BOOL new_sequence = snapshot->skill_sequence != 0u &&
            snapshot->skill_sequence != lease->seen_sequence;
        if (new_sequence) {
            lease->seen_sequence = snapshot->skill_sequence;
            lease->character = character;
            lease->slot = snapshot->skill_slot;
            lease->completion_logged = FALSE;
            lease->ranged_prime_requested = FALSE;
            lease->host_presentation_logged = FALSE;
            lease->native_started = FALSE;
            lease->active_seen = FALSE;
            lease->skill = NULL;
            clear_native_skill_activation_retry(lease);
            clear_native_skill_drain_marker(lease);
            if (snapshot->skill_active != 0u) {
                SudekiMpLogFormat(
                    "lan_arena_client_replica event=client_skill phase=started "
                    "actor=Tal sequence=%u slot=%u cost=%lu "
                    "policy=remote_snapshot_channels_no_process_global_native_task\r\n",
                    (unsigned int)snapshot->skill_sequence,
                    (unsigned int)snapshot->skill_slot,
                    (unsigned long)snapshot->skill_cost);
            }
        }
        if (snapshot->skill_sequence != 0u &&
            snapshot->skill_sequence == lease->seen_sequence &&
            snapshot->skill_active == 0u && !lease->completion_logged) {
            lease->completion_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_skill phase=completed "
                "actor=Tal sequence=%u slot=%u "
                "policy=host_snapshot_channels_retired_without_global_task\r\n",
                (unsigned int)lease->seen_sequence,
                (unsigned int)lease->slot);
        }
        *native_owns_presentation = snapshot->skill_sequence != 0u &&
            snapshot->skill_active != 0u;
        return TRUE;
    }
    if(snapshot->skill_sequence && snapshot->skill_kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER) {
        SudekiMpLanArenaActorSnapshot timing=*snapshot;
        (void)SudekiMpLanArenaReplicaLatestSkillTiming(&replica,actor_index,snapshot,
            &timing.skill_target_phase,&timing.skill_target_remaining_ms);
        if(!SudekiMpLanArenaApplyClientSkillTiming(actor_index,&timing)) return FALSE;
    }
    if (lease->native_started &&
        lease->seen_sequence == snapshot->skill_sequence) {
        if (local_native_active || snapshot->skill_active != 0u) {
            *native_owns_presentation = TRUE;
            return TRUE;
        }
        if (!SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
                lease->native_started, lease->active_seen,
                local_state_observed, local_native_active)) {
            mark_native_skill_drain(
                actor_index, snapshot->skill_sequence,
                snapshot->skill_kind,
                !local_state_observed ?
                    "same_sequence_retirement_unconfirmed" :
                    "same_sequence_native_startup_gap");
            *native_owns_presentation = TRUE;
            return TRUE;
        }
        if (!lease->completion_logged) {
            lease->completion_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_skill phase=completed "
                "actor=%s sequence=%u slot=%u "
                "policy=native_task_and_host_observation_retired\r\n",
                actor_index == 0u ? "Tal" : "Ailish",
                (unsigned int)lease->seen_sequence,
                (unsigned int)lease->slot);
        }
        if (!retire_native_skill_lease(
                actor_index, "skill_completed")) {
            mark_native_skill_drain(
                actor_index, snapshot->skill_sequence,
                snapshot->skill_kind,
                "owner_view_restore_unconfirmed");
            *native_owns_presentation = TRUE;
            return TRUE;
        }
    }

    if (snapshot->skill_sequence == 0u) return TRUE;
    if (snapshot->skill_sequence == lease->seen_sequence) {
        if (snapshot->skill_active == 0u) {
            clear_native_skill_activation_retry(lease);
        }
        return TRUE;
    }

    /* An inactive first sighting is a baseline from before this replica's
     * authenticated stream. It is safe to commit because no client-native
     * action is manufactured. Active sequences remain uncommitted until the
     * exact local CSkill::Use call positively returns STARTED. */
    if (snapshot->skill_active == 0u) {
        if (lease->ranged_prime_requested &&
            SudekiMpCleanroomEngineRangedCombatPrimePending()) {
            *native_owns_presentation = TRUE;
            return TRUE;
        }
        clear_native_skill_activation_retry(lease);
        lease->seen_sequence =
            SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
                lease->seen_sequence, snapshot->skill_sequence, 0, 0);
        lease->character = character;
        lease->slot = snapshot->skill_slot;
        lease->native_started = FALSE;
        lease->active_seen = FALSE;
        lease->skill = NULL;
        lease->completion_logged = FALSE;
        lease->host_presentation_logged = FALSE;
        clear_native_skill_drain_marker(lease);
        return TRUE;
    }

    /* The retry gate is intentionally distinct from the committed sequence.
     * It binds a delayed ranged-prime cycle and a finite activation budget to
     * one authenticated transaction. Never abandon an in-flight 75 ms UI
     * prime merely because a newer host transaction reached this renderer. */
    if (lease->retry_gate.bound &&
        (lease->retry_gate.sequence != snapshot->skill_sequence ||
         lease->retry_gate.slot != snapshot->skill_slot)) {
        if (lease->ranged_prime_requested &&
            SudekiMpCleanroomEngineRangedCombatPrimePending()) {
            *native_owns_presentation = TRUE;
            return TRUE;
        }
        clear_native_skill_activation_retry(lease);
    }

    if (snapshot->skill_active != 0u &&
        !SudekiMpLanArenaOrdinarySkillOverlapOwned()) {
        unsigned int other_actor_index = actor_index ^ 1u;
        LanArenaNativeSkillPresentationLease *other_lease =
            &native_skill_leases[other_actor_index];
        BOOL other_state_observed = TRUE;
        BOOL other_native_active = FALSE;
        SudekiMpLanArenaClientOtherSkillDecision other_decision;
        if (other_lease->native_started) {
            other_state_observed = observe_native_skill_lease(
                other_actor_index, &other_native_active);
        }
        other_decision = SudekiMpLanArenaClientOtherSkillDecide(
            other_lease->native_started,
            other_lease->active_seen,
            other_state_observed,
            other_native_active);
        if (other_decision ==
                SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_DRAIN) {
            mark_native_skill_drain(
                other_actor_index, snapshot->skill_sequence,
                snapshot->skill_kind,
                other_state_observed ? "cross_actor_native_task_active" :
                    "cross_actor_native_state_unconfirmed");
            *native_owns_presentation = TRUE;
            return TRUE;
        }
        if (other_decision ==
                SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_RETIRE &&
            !retire_native_skill_lease(
                other_actor_index, "cross_actor_handoff")) {
            mark_native_skill_drain(
                other_actor_index, snapshot->skill_sequence,
                snapshot->skill_kind,
                "cross_actor_retirement_unconfirmed");
            *native_owns_presentation = TRUE;
            return TRUE;
        }
    }

    actor = actor_index == 0u ?
        seat_host_actor() : seat_client_actor();
    if (lease->ranged_prime_requested &&
        SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        *native_owns_presentation = TRUE;
        return TRUE;
    }
    retry_decision = SudekiMpLanArenaClientSkillRetryDecide(
        &lease->retry_gate,
        snapshot->skill_sequence,
        snapshot->skill_slot,
        GetTickCount(),
        lease->native_started);
    if (retry_decision == SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_WAIT) {
        *native_owns_presentation = TRUE;
        return TRUE;
    }
    if (retry_decision ==
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_EXHAUSTED) {
        *native_owns_presentation = TRUE;
        if (!lease->retry_exhaustion_logged) {
            lease->retry_exhaustion_logged = TRUE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_skill phase=failed_closed "
                "actor=%s sequence=%u slot=%u attempts=%u "
                "reason=activation_retry_exhausted "
                "policy=retain_uncommitted_sequence_and_never_spin_or_bypass_native_validation\r\n",
                actor_index == 0u ? "Tal" : "Ailish",
                (unsigned int)snapshot->skill_sequence,
                (unsigned int)snapshot->skill_slot,
                (unsigned int)lease->retry_gate.attempt_count);
        }
        return TRUE;
    }
    if (retry_decision !=
            SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT) {
        *native_owns_presentation = TRUE;
        return TRUE;
    }
    if(!client_session_status(&replay_status) ||
        InterlockedCompareExchange(&client_skill_activation_depth,0,0)>0) return FALSE;
    if (actor_index == 0u && !SudekiMpLanArenaClientPrivateCastCamerasOwned() &&
        !remote_tal_skill_view_lease.owner_view.valid &&
        !capture_remote_tal_skill_view_lease(
            snapshot->skill_sequence)) {
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill phase=deferred "
            "actor=Tal sequence=%u slot=%u reason=owner_view_lease "
            "policy=leave_sequence_uncommitted_and_retry_while_host_active\r\n",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_slot);
        return TRUE;
    }
    if (actor_index == 0u &&
        !set_client_remote_tal_skill_input_isolation(
            TRUE, "before_remote_tal_native_skill")) {
        if (!retire_remote_tal_skill_view_lease(
                "noncaster_input_isolation_rejected")) {
            return FALSE;
        }
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill phase=deferred "
            "actor=Tal sequence=%u slot=%u reason=input_isolation "
            "policy=leave_sequence_uncommitted_and_retry_while_host_active\r\n",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_slot);
        return TRUE;
    }
    topped_sp = snapshot->sp;
    if (snapshot->skill_cost <=
            SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE - topped_sp) {
        topped_sp += snapshot->skill_cost;
    } else {
        topped_sp = SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE;
    }
    /* The host snapshot already contains post-cost SP. Temporarily restore
     * exactly one authored cost so the local native Use path can validate and
     * build its presentation task, then immediately restore host resources.
     * Client ApplyDamage remains blocked by the authenticated replica guard. */
    if (!SudekiMpCleanroomEngineSetActorResources(
            actor, (float)snapshot->hp, (float)topped_sp)) {
        if (actor_index == 0u) {
            BOOL isolation_released =
                set_client_remote_tal_skill_input_isolation(
                    FALSE, "resource_lease_rejected");
            BOOL view_retired = retire_remote_tal_skill_view_lease(
                "resource_lease_rejected");
            if (!isolation_released || !view_retired) return FALSE;
        }
        retire_client_skill_isolation_if_idle();
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill phase=deferred "
            "actor=%s sequence=%u slot=%u reason=resource_lease "
            "policy=leave_sequence_uncommitted_and_retry_while_host_active\r\n",
            actor_index == 0u ? "Tal" : "Ailish",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_slot);
        return TRUE;
    }
    client_skill_camera_suppression_logged = FALSE;
    client_skill_activation_actor_index = (int)actor_index;
    client_skill_replay_admission.actor=character;
    client_skill_replay_admission.session=replay_status.session_token;
    client_skill_replay_admission.sequence=snapshot->skill_sequence;
    client_skill_replay_admission.slot=snapshot->skill_slot;
    client_skill_replay_admission.thread=GetCurrentThreadId();
    InterlockedIncrement(&client_skill_activation_depth);
    result = SudekiMpReplayHostApprovedCharacterSkillSlot(
        character, (int)snapshot->skill_slot);
    InterlockedDecrement(&client_skill_activation_depth);
    client_skill_activation_actor_index = -1;
    ZeroMemory(&client_skill_replay_admission,sizeof(client_skill_replay_admission));
    host_resources_restored = SudekiMpCleanroomEngineSetActorResources(
        actor, (float)snapshot->hp, (float)snapshot->sp);

    /* STARTED is the first point at which this active host sequence is
     * consumed. Establish the conservative native lease before observing the
     * asynchronous task so an unreadable or initially inactive state cannot
     * release its view/input/damage ownership. */
    if (result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED) {
        lease->seen_sequence =
            SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
                lease->seen_sequence, snapshot->skill_sequence, 1, 1);
        lease->character = character;
        lease->slot = snapshot->skill_slot;
        lease->skill = result.skill;
        lease->native_started = TRUE;
        lease->active_seen = FALSE;
        lease->completion_logged = FALSE;
        lease->host_presentation_logged = FALSE;
        clear_native_skill_activation_retry(lease);
        memset(&presentation_leases[actor_index], 0,
            sizeof(presentation_leases[actor_index]));
        if (actor_index == 0u) {
            memset(&tal_native_presentation_lease, 0,
                sizeof(tal_native_presentation_lease));
        }
    }
    if (!host_resources_restored) {
        if (result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED) {
            mark_native_skill_drain(
                actor_index, snapshot->skill_sequence,
                snapshot->skill_kind,
                "post_start_resource_restore_unconfirmed");
            *native_owns_presentation = TRUE;
        } else if (actor_index == 0u) {
            (void)set_client_remote_tal_skill_input_isolation(
                FALSE, "resource_restore_unconfirmed");
            (void)retire_remote_tal_skill_view_lease(
                "resource_restore_unconfirmed");
        }
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    /* CSkill::Use may synchronously mutate the active render camera's basis
     * without going through the camera-manager hook. Restore Ailish's exact
     * owner view before any later presentation work; STARTED already owns a
     * conservative lease, so failure retains containment and fails closed. */
    if (result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED &&
        actor_index == 0u && !reassert_remote_tal_skill_view()) {
        mark_native_skill_drain(
            actor_index, snapshot->skill_sequence,
            snapshot->skill_kind,
            "post_start_owner_view_reassert_unconfirmed");
        *native_owns_presentation = TRUE;
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    if (result.status != SUDEKIMP_SKILL_ACTIVATION_STARTED &&
        actor_index == 1u &&
        SudekiMpLanArenaClientSkillValidationNeedsRangedPrime(
            result.validation_result, host_combat_authorized) &&
        !lease->ranged_prime_requested &&
        (result.validation_result != 3 ||
            SudekiMpCleanroomEngineRefreshCombatMode()) &&
        SudekiMpCleanroomEnginePrimeRangedCombat()) {
        lease->ranged_prime_requested = TRUE;
        *native_owns_presentation = TRUE;
        retire_client_skill_isolation_if_idle();
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill phase=arming "
            "actor=Ailish sequence=%u slot=%u validation=%d delay_ms=75 "
            "policy=native_combat_refresh_and_ui_cycle_then_retry_without_validator_bypass\r\n",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_slot,
            result.validation_result);
        return TRUE;
    }
    if (result.status != SUDEKIMP_SKILL_ACTIVATION_STARTED) {
        if (actor_index == 0u) {
            if (!set_client_remote_tal_skill_input_isolation(
                    FALSE, "native_task_rejected")) {
                return FALSE;
            }
            if (!retire_remote_tal_skill_view_lease(
                    "native_task_rejected")) {
                return FALSE;
            }
        }
        retire_client_skill_isolation_if_idle();
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill phase=deferred "
            "actor=%s sequence=%u slot=%u status=%s validation=%d use=%u "
            "policy=leave_sequence_uncommitted_and_retry_while_host_active\r\n",
            actor_index == 0u ? "Tal" : "Ailish",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_slot,
            SudekiMpSkillActivationStatusName(result.status),
            result.validation_result, (unsigned int)result.use_result);
        return TRUE;
    }

    if (!SudekiMpObserveCharacterSkill(character, &state) ||
        state.skill != result.skill || state.active == 0u ||
        state.slot != (int)snapshot->skill_slot) {
        /* Use reported STARTED, so absence of an immediate exact observation
         * cannot be treated as cancellation. Retain the returned CSkill
         * identity, camera, damage guard, and actor until a later positive
         * inactive observation proves the asynchronous task drained. */
        mark_native_skill_drain(
            actor_index, snapshot->skill_sequence,
            snapshot->skill_kind,
            "post_start_native_state_unconfirmed");
        *native_owns_presentation = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_skill phase=draining "
            "actor=%s sequence=%u slot=%u reason=post_start_observation "
            "policy=started_result_retains_native_task_until_positive_inactive_observation\r\n",
            actor_index == 0u ? "Tal" : "Ailish",
            (unsigned int)snapshot->skill_sequence,
            (unsigned int)snapshot->skill_slot);
        return TRUE;
    }
    lease->skill = state.skill;
    lease->active_seen = TRUE;
    clear_native_skill_drain_marker(lease);
    *native_owns_presentation = TRUE;
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_skill phase=started "
        "actor=%s sequence=%u slot=%u cost=%lu "
        "policy=host_approved_native_cskill_presentation_local_unlock_restored_host_resources_restored_camera_preserved_realtime\r\n",
        actor_index == 0u ? "Tal" : "Ailish",
        (unsigned int)snapshot->skill_sequence,
        (unsigned int)snapshot->skill_slot,
        (unsigned long)snapshot->skill_cost);
    return TRUE;
}

static BOOL apply_ailish_host_locomotion(
    uint8_t *character,
    uint8_t *component,
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    BOOL final_boundary
) {
    const SudekiMpLanArenaLocomotion *motion = &snapshot->locomotion;
    LanArenaPresentationLease *lease = &presentation_leases[1];
    unsigned int channel;
    BOOL new_owner = !lease->valid || lease->character != character ||
        lease->renderer != renderer || !lease->combat_mode;
    BOOL new_epoch = new_owner || !lease->locomotion.valid ||
        lease->locomotion.sequence != motion->sequence;
    BOOL firing = snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    BOOL new_fire = new_owner || lease->action_variant != snapshot->action_variant ||
        lease->action_sequence != snapshot->action_sequence;
    DWORD now = GetTickCount();
    DWORD elapsed = !new_owner && lease->last_early_apply_at ?
        now - lease->last_early_apply_at : 17u;
    if (!motion->valid || !SudekiMpLanArenaLocomotionValid(motion) ||
        snapshot->skill_active) return FALSE;
    /* All nonempty identities must resolve in this exact actor's world bank
     * before the first write. Never pass packet selectors to first-person
     * arms or treat a readable renderer as the right animation resource. */
    for (channel = 0u; channel < 4u; ++channel) {
        unsigned int clip = motion->clip[channel];
        if (clip && !resolve_ailish_world_selector(component, renderer,
                SudekiMpLanArenaLocomotionAnimationId(clip),
                SudekiMpLanArenaLocomotionSelector(clip,
                    seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE))) return FALSE;
    }
    if (firing && !resolve_ailish_world_selector(component, renderer,
            0x85u, client_combat_fire_selector())) return FALSE;
    for (channel = 0u; channel < 4u; ++channel) {
        unsigned int submodel;
        int selector = SudekiMpLanArenaLocomotionSelector(motion->clip[channel],
            seat_client_type() == SUDEKIMP_LAN_ARENA_ELCO_TYPE);
        BOOL fresh_phase = new_epoch ||
            motion->time[channel] != lease->locomotion.time[channel];
        for (submodel = 0u; submodel < submodels; ++submodel) {
            BOOL changed = methods->get_selector(renderer, (int)channel,
                submodel) != selector;
            float phase;
            if (!SudekiMpLanArenaClientLocomotionPhase(motion->time[channel],
                    motion->rate[channel], elapsed, final_boundary, &phase))
                return FALSE;
            if (changed) methods->set_selector(renderer, (int)channel,
                submodel, selector);
            if (methods->get_state(renderer, (int)channel, submodel) !=
                    motion->state[channel])
                methods->set_state(renderer, (int)channel, submodel,
                    motion->state[channel]);
            if (methods->get_rate(renderer, (int)channel, submodel) != motion->rate[channel])
                methods->set_rate(renderer, (int)channel, submodel, motion->rate[channel]);
            /* These are post-host-update clocks. Install before the local
             * update with one measured frame subtracted; never seek again at
             * the later render boundary, or replay a held packet each frame. */
            if (!final_boundary && (fresh_phase || changed)) {
                methods->set_time(renderer, (int)channel, submodel, phase, 0);
            } else if (final_boundary && changed) {
                methods->set_time(renderer, (int)channel, submodel, phase, 0);
            }
            if (methods->get_selector(renderer, (int)channel, submodel) != selector ||
                methods->get_state(renderer, (int)channel, submodel) != motion->state[channel] ||
                methods->get_rate(renderer, (int)channel, submodel) != motion->rate[channel])
                return FALSE;
        }
    }
    for (channel = 0u; channel < 3u; ++channel)
        methods->set_blend(renderer, (int)channel, motion->blend[channel]);
    /* Firing is an independent upper-body layer, including while a native
     * backward/strafe pair is active. The wire never grants attack authority. */
    set_animation_channel(renderer, methods, submodels, 4,
        firing ? client_combat_fire_selector() : 0, firing ? 1 : 192,
        firing ? 24.0f : 0.0f, new_fire);
    if (firing && !synchronize_action_phase(renderer, methods, submodels, 4,
            snapshot)) return FALSE;
    methods->set_blend(renderer, 3, firing ? 1.0f : 0.0f);
    if (!final_boundary) {
        if (new_owner || !lease->locomotion.valid) {
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_locomotion actor=Ailish "
                "state=admitted epoch=%u clips=%u,%u,%u,%u "
                "policy=exact_world_bank_host_timing_independent_fire_layer\r\n",
                motion->sequence, motion->clip[0], motion->clip[1],
                motion->clip[2], motion->clip[3]);
        }
        lease->last_early_apply_at = now;
        lease->locomotion = *motion;
    }
    lease->character = character;
    lease->renderer = renderer;
    lease->animation_state = snapshot->animation_state;
    lease->combat_state = snapshot->combat_state;
    lease->action_variant = snapshot->action_variant;
    lease->action_sequence = snapshot->action_sequence;
    lease->combat_mode = TRUE;
    lease->valid = TRUE;
    return TRUE;
}

static BOOL apply_buki_host_locomotion(
    uint8_t *character,
    void *renderer,
    const LanArenaAnimationMethods *methods,
    unsigned int submodels,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    BOOL final_boundary
) {
    const SudekiMpLanArenaLocomotion *motion = &snapshot->locomotion;
    LanArenaPresentationLease *lease = &presentation_leases[0];
    unsigned int channel;
    BOOL new_owner = !lease->valid || lease->character != character ||
        lease->renderer != renderer || !lease->combat_mode;
    DWORD now = GetTickCount();
    DWORD elapsed = !new_owner && lease->last_early_apply_at ?
        now - lease->last_early_apply_at : 17u;
    if (!motion->valid || !SudekiMpLanArenaLocomotionValid(motion) ||
        snapshot->skill_active) return FALSE;
    /* This path currently admits only Buki. Preflight every packet clip before
     * mutating any channel: her bank has no ranged strafe/fire resources. */
    if (seat_host_type() != SUDEKIMP_LAN_ARENA_BUKI_TYPE ||
        !SudekiMpCleanroomBukiAnimationStorageValid(renderer, submodels, TRUE) ||
        !readable_memory(character, CHARACTER_ARBITER_OFFSET + sizeof(void *)) ||
        !SudekiMpBukiReplicaBodyAvailable((HMODULE)game_base, character,
            *(void **)(character + CHARACTER_ARBITER_OFFSET)))
        return FALSE;
    for (channel = 0u; channel < 4u; ++channel) {
        if (SudekiMpLanArenaLocomotionSelector(motion->clip[channel], 2u) < 0)
            return FALSE;
    }
    for (channel = 0u; channel < 4u; ++channel) {
        unsigned int submodel;
        int selector = SudekiMpLanArenaLocomotionSelector(
            motion->clip[channel], 2u);
        BOOL fresh_phase = new_owner || !lease->locomotion.valid ||
            motion->time[channel] != lease->locomotion.time[channel] ||
            motion->rate[channel] != lease->locomotion.rate[channel];
        BOOL restart = new_owner || !lease->locomotion.valid ||
            motion->clip[channel] != lease->locomotion.clip[channel] ||
            motion->time[channel] < lease->locomotion.time[channel];
        for (submodel = 0u; submodel < submodels; ++submodel) {
            BOOL changed = methods->get_selector(renderer, (int)channel,
                submodel) != selector;
            float phase;
            float playback_rate = motion->rate[channel];
            float actual_rate;
            BOOL seek = FALSE;
            if (!SudekiMpLanArenaClientLocomotionPhase(motion->time[channel],
                    motion->rate[channel], elapsed, final_boundary, &phase))
                return FALSE;
            if (changed) methods->set_selector(renderer, (int)channel,
                submodel, selector);
            if (methods->get_state(renderer, (int)channel, submodel) !=
                    motion->state[channel])
                methods->set_state(renderer, (int)channel, submodel,
                    motion->state[channel]);
            if (!final_boundary && (fresh_phase || changed)) {
                if (!SudekiMpLanArenaClientBukiLocomotionClock(
                        methods->get_time(renderer, (int)channel, submodel),
                        phase, motion->rate[channel], restart || changed,
                        &playback_rate, &seek)) return FALSE;
                if (seek) methods->set_time(renderer, (int)channel,
                    submodel, phase, 0);
                methods->set_rate(renderer, (int)channel, submodel, playback_rate);
            } else if (changed) {
                methods->set_time(renderer, (int)channel, submodel, phase, 0);
                methods->set_rate(renderer, (int)channel, submodel, playback_rate);
            }
            actual_rate = methods->get_rate(renderer, (int)channel, submodel);
            if (methods->get_selector(renderer, (int)channel, submodel) !=
                    selector ||
                methods->get_state(renderer, (int)channel, submodel) !=
                    motion->state[channel] || !isfinite(actual_rate) ||
                fabsf(actual_rate - motion->rate[channel]) >
                    motion->rate[channel] * 0.1001f + 0.001f ||
                (((!final_boundary && fresh_phase) || changed) &&
                 fabsf(actual_rate - playback_rate) > 0.001f))
                return FALSE;
        }
    }
    for (channel = 0u; channel < 3u; ++channel)
        methods->set_blend(renderer, (int)channel, motion->blend[channel]);
    if (!final_boundary) {
        if (new_owner || !lease->locomotion.valid) {
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_buki_locomotion actor=Buki "
                "state=admitted epoch=%u clips=%u,%u,%u,%u "
                "policy=host_observed_base_channels\r\n",
                motion->sequence, motion->clip[0], motion->clip[1],
                motion->clip[2], motion->clip[3]);
        }
        lease->last_early_apply_at = now;
        lease->locomotion = *motion;
    }
    lease->character = character;
    lease->renderer = renderer;
    lease->animation_state = snapshot->animation_state;
    lease->combat_state = snapshot->combat_state;
    lease->action_variant = snapshot->action_variant;
    lease->action_sequence = snapshot->action_sequence;
    lease->combat_mode = TRUE;
    lease->valid = TRUE;
    return TRUE;
}

/* Naked native-call trampolines (defined below, after call_position_set_forward);
 * forward-declared so apply_actor_presentation can reach them. */
static void *call_model_resolve_anim(void *model, unsigned int anim_id);
static unsigned int call_model_play_anim(
    void *model, unsigned int channel, unsigned int flag_a,
    void *definition, unsigned int flag_b, float time_value);

/* Resolve the actor's animation model (CNewGameModelAnimation) from the
 * character entity, verified by the +0x10 backpointer. Fail closed to NULL.
 * The +0x134 offset is the shared character->model link; it is historically
 * named "AILISH_RANGED" but the model class is character-independent. */
static uint8_t *client_resolve_actor_model(uint8_t *character) {
    uint8_t *model;
    if (character == NULL ||
        !readable_memory(character,
            AILISH_RANGED_COMPONENT_OFFSET + sizeof(void *))) {
        return NULL;
    }
    model = *(uint8_t **)(character + CHARACTER_ANIMATION_MODEL_OFFSET);
    if (model == NULL || !readable_memory(model, 0x138u) ||
        *(void **)(model + 0x10u) != character) return NULL;
    return model;
}

static BOOL apply_actor_presentation(
    uint8_t *character,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    unsigned int actor_index,
    BOOL combat_mode,
    BOOL final_presentation_boundary
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
    BOOL tal_native_owns_presentation = FALSE;
    BOOL tal_action_retirement;
    BOOL native_skill_owns_presentation = FALSE;
    float retirement_idle_phase = 0.0f;
    float retirement_pre_update_phase = 0.0f;
    DWORD previous_early_apply_at;
    DWORD early_apply_at = 0u;
    DWORD early_elapsed_ms = 17u;
    if (character == NULL || snapshot == NULL || actor_index >= 2u) {
        return FALSE;
    }
    if (!service_native_skill_presentation(
            character, snapshot, actor_index, combat_mode,
            &native_skill_owns_presentation)) return FALSE;
    if (native_skill_owns_presentation) {
        presentation_leases[actor_index].locomotion.valid = 0u;
        return apply_host_skill_presentation(
            character, snapshot, actor_index,
            !final_presentation_boundary);
    }
    /* Death/incapacitation is a presentation terminal, not proof that an
     * asynchronous local CSkill is gone. Service and positively retire the
     * retained native lease first; only then suppress ordinary channels. */
    if (snapshot->animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED) {
        return FALSE;
    }
    /* ANIMID path: when the host reports a semantic animation id, resolve and
     * play it on the actor's model directly -- character-independent, no
     * per-character selector table. Fail closed: any failure (missing model,
     * unresolvable definition) falls through to the legacy selector path.
     * TEMPORARILY DISABLED: FUN_004e4f50 hard-crashes the client even with the
     * whole renderer chain verified, so the play branch is gated off until a
     * safe renderer-driving entry point is found. */
    if (0 && snapshot->anim_id != 0u) {
        uint8_t *model = client_resolve_actor_model(character);
        void *definition = NULL;
        if (model != NULL) {
            definition = call_model_resolve_anim(model, snapshot->anim_id);
        }
        if (definition != NULL) {
            uint8_t *state_array = readable_memory(model, 0xFCu) ?
                *(uint8_t **)(model + 0xF8u) : NULL;
            BOOL state_writable = state_array != NULL &&
                writable_memory(state_array, 0x20u);
            BOOL definition_writable = writable_memory(definition, 0x28u);
            /* Renderer chain the play entry walks (model+0x10 -> character+0x44
             * -> position+0xb4 -> wrapper+0x10 -> renderer -> vtable[0x40]). */
            uint8_t *chain_character = readable_memory(model, 0x14u) ?
                *(uint8_t **)(model + 0x10u) : NULL;
            uint8_t *chain_position = chain_character != NULL &&
                readable_memory(chain_character, 0x48u) ?
                *(uint8_t **)(chain_character + 0x44u) : NULL;
            uint8_t *chain_wrapper = chain_position != NULL &&
                readable_memory(chain_position, 0xB8u) ?
                *(uint8_t **)(chain_position + 0xB4u) : NULL;
            uint8_t *chain_renderer = chain_wrapper != NULL &&
                readable_memory(chain_wrapper, 0x14u) ?
                *(uint8_t **)(chain_wrapper + 0x10u) : NULL;
            uint8_t *chain_vtable = chain_renderer != NULL &&
                readable_memory(chain_renderer, sizeof(void *)) ?
                *(uint8_t **)chain_renderer : NULL;
            void *chain_slot40 = chain_vtable != NULL &&
                readable_memory(chain_vtable, 0x44u) ?
                *(void **)(chain_vtable + 0x40u) : NULL;
            BOOL play_ok = state_writable && definition_writable &&
                chain_slot40 != NULL;
            /* TEMPORARY diagnostic: name the failing hop (model / state array /
             * definition / renderer chain) before a play call that can crash.
             * REMOVE after Phase 5. */
            {
                static DWORD last_client_anim_id_diag[2];
                DWORD anim_diag_now = GetTickCount();
                if (last_client_anim_id_diag[actor_index] == 0u ||
                    (DWORD)(anim_diag_now - last_client_anim_id_diag[actor_index]) >= 300u) {
                    last_client_anim_id_diag[actor_index] = anim_diag_now;
                    SudekiMpLogFormat(
                        "lan_arena_client_replica event=client_anim_id_apply "
                        "actor=%u anim_id=%u policy=%s model=%p state=%p "
                        "state_w=%u definition=%p definition_w=%u "
                        "chain_c=%p chain_pos=%p chain_wrp=%p chain_rnd=%p "
                        "chain_vtbl=%p chain_slot40=%p\r\n",
                        actor_index, (unsigned)snapshot->anim_id,
                        play_ok ? "animid" : "legacy_fallback",
                        (void *)model, (void *)state_array,
                        (unsigned)state_writable, definition,
                        (unsigned)definition_writable,
                        (void *)chain_character, (void *)chain_position,
                        (void *)chain_wrapper, (void *)chain_renderer,
                        (void *)chain_vtable, chain_slot40);
                }
            }
            if (!play_ok) {
                /* Fail closed: the play entry WRITES [[model+0xF8]+channel*4],
                 * dereferences the definition refcount, and calls renderer
                 * vtable[0x40] — any unwritable/unresolvable hop would crash the
                 * client. Fall through to the legacy selector path instead. */
                return FALSE;
            }
            /* channel/f1/f2/time are data values observed at the native
             * callsites. The host reads CHANNEL 0 ([[model+0xF8]+0*4+2]), so the
             * client must play on channel 0 too — channel 2 was an observed
             * default of the model's own secondary play methods, not the
             * primary channel the read targets. time 0 restarts the clip from
             * its head; Phase 5 measures whether a phase-preserving time is
             * required. */
            presentation_leases[actor_index].locomotion.valid = 0u;
            (void)call_model_play_anim(model, 0u, 0u, definition, 1u, 0.0f);
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_anim_id_played "
                "actor=%u anim_id=%u\r\n",
                actor_index, (unsigned)snapshot->anim_id);
            return TRUE;
        }
        /* TEMPORARY diagnostic: log the fallback so a silent model/resolve
         * failure is visible. REMOVE after Phase 5 capture. */
        {
            static DWORD last_client_anim_fallback_diag[2];
            DWORD anim_fb_now = GetTickCount();
            if (last_client_anim_fallback_diag[actor_index] == 0u ||
                (DWORD)(anim_fb_now - last_client_anim_fallback_diag[actor_index]) >= 300u) {
                last_client_anim_fallback_diag[actor_index] = anim_fb_now;
                SudekiMpLogFormat(
                    "lan_arena_client_replica event=client_anim_id_apply "
                    "actor=%u anim_id=%u policy=legacy_fallback\r\n",
                    actor_index, (unsigned)snapshot->anim_id);
            }
        }
    }
    /* Combat swaps Ailish's Position attachment to her two-submodel
     * first-person renderer. World selectors 20/22/23/59 are not valid in
     * that table. Prefer the distinct saved-world wrapper at component+0x164;
     * the resolver admits Position+0xB4 only for the exact cleanroom NULL-
     * saved-world/out-of-combat topology. Tal keeps the ordinary attached
     * world renderer. */
    if (!actor_presentation_renderer(
            character, actor_index, &renderer, &ailish_component)) {
        return FALSE;
    }
    if (!animation_methods(renderer, &methods)) return FALSE;
    submodels = methods.count(renderer);
    if (submodels == 0u || submodels > 32u) return FALSE;
    if (actor_index == 1u) {
        dump_client_world_animation_table(ailish_component, renderer);
    }
    if (actor_index == 0u && seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE &&
        !SudekiMpCleanroomBukiAnimationStorageValid(renderer, submodels, TRUE))
        return FALSE;
    /* The first-person arms renderer is the owner's visible surface and is
     * independent from Ailish's retained world renderer. Validate/apply it
     * before any conservative world-bank lookup can fail. */
    if (actor_index == 1u && combat_mode) {
        ailish_first_person_applied =
            apply_ailish_first_person_presentation(
                character, ailish_component, snapshot, final_presentation_boundary);
    }
    /* An active native melee lease owns every frame through its positive
     * idle observation. A fresh host locomotion packet must not bypass its
     * drain or overwrite the tail of an attack on the client. */
    lease = &presentation_leases[actor_index];
    if (actor_index == 0u &&
        !service_tal_native_action_presentation(
            character, renderer, &methods, snapshot, combat_mode,
            final_presentation_boundary, &tal_native_owns_presentation)) {
        return FALSE;
    }
    if (tal_native_owns_presentation) {
        lease->character = character;
        lease->renderer = renderer;
        lease->animation_state = snapshot->animation_state;
        lease->combat_state = snapshot->combat_state;
        lease->action_variant = snapshot->action_variant;
        lease->action_sequence = snapshot->action_sequence;
        lease->combat_mode = combat_mode;
        lease->valid = TRUE;
        return TRUE;
    }
    if (actor_index == 0u && combat_mode &&
        seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
        /* Missing/unknown body evidence must not fall back to synthetic
         * combo input or a guessed selector. Keep the last pose until the
         * next admitted host frame. This also covers fail->retry and
         * running-attack->combo without a separate local combo history. */
        return apply_buki_host_locomotion(character, renderer, &methods,
            submodels, snapshot, final_presentation_boundary);
    }
    if (actor_index == 0u && seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE &&
        SudekiMpLanArenaBukiBodyAction(snapshot->action_variant)) {
        /* Missing blend/clock evidence is unknown, not permission to start a
         * block in native gameplay or fall into the ranged fifth-channel
         * fallback. Keep the last pose until a valid four-channel frame. */
        return FALSE;
    }
    if (actor_index == 1u && combat_mode && snapshot->locomotion.valid) {
        return apply_ailish_host_locomotion(character, ailish_component,
            renderer, &methods, submodels, snapshot,
            final_presentation_boundary) && ailish_first_person_applied;
    }
    presentation_leases[actor_index].locomotion.valid = 0u;
    if (actor_index == 1u && combat_mode &&
        (!resolve_ailish_world_selector(
             ailish_component, renderer, 0x02u,
             client_combat_idle_selector()) ||
         !resolve_ailish_world_selector(
             ailish_component, renderer, 0x06u,
             client_combat_move_primary_selector()) ||
         !resolve_ailish_world_selector(
             ailish_component, renderer, 0x07u,
             client_combat_move_secondary_selector()) ||
         !resolve_ailish_world_selector(
             ailish_component, renderer, 0x85u,
             client_combat_fire_selector()))) {
        return FALSE;
    }
    previous_early_apply_at = lease->last_early_apply_at;
    if (!final_presentation_boundary) {
        early_apply_at = GetTickCount();
        if (previous_early_apply_at != 0u) {
            early_elapsed_ms = early_apply_at - previous_early_apply_at;
            if (early_elapsed_ms == 0u) early_elapsed_ms = 1u;
            if (early_elapsed_ms > 50u) early_elapsed_ms = 50u;
        }
        lease->last_early_apply_at = early_apply_at;
    }
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
    tal_action_retirement = actor_index == 0u && lease->valid &&
        lease->character == character && lease->renderer == renderer &&
        lease->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
        snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
        lease->action_sequence == snapshot->action_sequence &&
        SudekiMpLanArenaClientRetirementIdlePhaseTime(
            snapshot, &retirement_idle_phase);
    if (tal_action_retirement) {
        /* idle_entry_phase is observed after the host's native animation
         * update. The client installs this selector before its corresponding
         * update so the skeleton, root motion, and renderer all consume the
         * edge together. Back the clock up by the measured local frame step;
         * the following native update advances it to the transmitted host
         * witness instead of advancing that witness a second time. */
        if (!SudekiMpLanArenaClientRetirementPreUpdatePhase(
                retirement_idle_phase, early_elapsed_ms,
                final_presentation_boundary,
                &retirement_pre_update_phase)) return FALSE;
    }
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
            if (weak_attack && !(seat_host_type() == SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
                SudekiMpLanArenaClientBukiActionPresentation(
                    snapshot->action_variant,
                    &selector_zero, &state_zero) :
                SudekiMpLanArenaClientTalActionPresentation(
                    snapshot->action_variant,
                    &selector_zero, &state_zero))) {
                return FALSE;
            }
            if (!weak_attack) {
                selector_zero = moving ? host_combat_move_primary_selector() :
                    host_combat_idle_selector();
            }
            selector_one = moving ? host_combat_move_secondary_selector() : 0;
            if (!weak_attack) state_zero = moving ? 0 : 128;
            state_one = moving ? 0 : 192;
            rate_zero = weak_attack ? 24.0f :
                (moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f);
            rate_one = moving ? TAL_WORLD_MOVE_SECONDARY_RATE : 0.0f;
        } else {
            selector_zero = moving ?
                host_world_move_primary_selector() : host_world_idle_selector();
            selector_one = moving ? host_world_move_secondary_selector() : 0;
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
            (combat_mode ? client_combat_move_primary_selector() :
                client_world_move_selector()) :
            (combat_mode ? client_combat_idle_selector() :
                AILISH_WORLD_IDLE_SELECTOR);
        selector_one = moving ?
            (combat_mode ? client_combat_move_secondary_selector() :
                client_world_move_secondary_selector()) : 0;
        state_one = moving ? 0 : 192;
        rate_zero = moving ? client_world_move_rate() : 12.0f;
        rate_one = moving ? client_world_move_secondary_rate() : 0.0f;
        if (!combat_mode && SudekiMpLanArenaClientIdleVariantSelector(
                seat_client_type(),
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
    if (tal_action_retirement && !synchronize_channel_phase(
            renderer, &methods, submodels, 0,
            retirement_pre_update_phase)) return FALSE;
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
        if (seat_host_type() != SUDEKIMP_LAN_ARENA_BUKI_TYPE)
            methods.set_blend(renderer, 3, 0.0f);
    } else if (!preserve_ailish_auxiliary) {
        set_animation_channel(renderer, &methods, submodels, 2,
            0, 192, 0.0f, logical_transition);
        set_animation_channel(renderer, &methods, submodels, 3,
            0, 192, 0.0f, logical_transition);
        methods.set_blend(renderer, 1, 0.0f);
        methods.set_blend(renderer, 2, 0.0f);
        if (weak_attack) {
            action_selector = combat_mode ? client_combat_fire_selector() :
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
    if (tal_action_retirement) {
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_tal_action_retirement "
            "state=verified action_sequence=%u terminal_phase_q8=%u "
            "idle_entry_phase_q8=%u installed_phase=%.5f "
            "boundary=%s frame_step_ms=%lu "
            "policy=pre_update_host_idle_clock_handoff\r\n",
            (unsigned int)snapshot->action_sequence,
            (unsigned int)snapshot->action_terminal_phase_q8,
            (unsigned int)snapshot->idle_entry_phase_q8,
            retirement_pre_update_phase,
            final_presentation_boundary ? "pre_world" : "pre_animation",
            (unsigned long)(final_presentation_boundary ? 0u :
                early_elapsed_ms));
    }
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

/* FUN_004e42f0 (ANIMID resolver): model in EAX, ANIMID in ECX, definition out
 * in EAX. Fails closed to NULL on a null/empty animation table. */
__attribute__((naked, noinline, used))
static void *call_model_resolve_anim(
    void *model __attribute__((unused)),
    unsigned int anim_id __attribute__((unused))
) {
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %ecx\n\t"
        "call *_model_resolve_anim\n\t"
        "ret\n\t"
    );
}

/* FUN_004e4f50 (ANIMID play entry): model in EDI, then five __cdecl stack args
 * (channel, f1, definition, f2, time). Caller cleans the stack. Returns 0 on
 * failure (null definition). */
__attribute__((naked, noinline, used))
static unsigned int call_model_play_anim(
    void *model __attribute__((unused)),
    unsigned int channel __attribute__((unused)),
    unsigned int flag_a __attribute__((unused)),
    void *definition __attribute__((unused)),
    unsigned int flag_b __attribute__((unused)),
    float time_value __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %edi\n\t"
        "movl 8(%esp), %edi\n\t"
        "movl 28(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "movl 28(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "movl 28(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "movl 28(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "movl 28(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "call *_model_play_anim\n\t"
        "addl $20, %esp\n\t"
        "popl %edi\n\t"
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
    if ((actor == seat_client_actor() || actor == seat_host_actor()) &&
        snapshot->weapon_slot_plus_one != 0u &&
        snapshot->weapon_slot_plus_one <= 12u) {
        SudekiMpWeaponQuickList weapons;
        SudekiMpCharacterSkillState skill;
        void *weapon = *(void **)(character + 0xc0u);
        unsigned int slot = snapshot->weapon_slot_plus_one - 1u;
        if (!SudekiMpDescribeCharacterWeapons(character, &weapons) ||
            slot >= weapons.row_count) return FALSE;
        if (!weapons.rows[slot].equipped) {
            if (!SudekiMpObserveCharacterSkill(character, &skill) ||
                skill.active || client_skill_replay_active()) return FALSE;
            if (actor == seat_client_actor() &&
                !drain_ailish_native_ranged()) return FALSE;
            if (client_weapon_attempt_actor == character &&
                client_weapon_attempt_component == weapon &&
                client_weapon_attempt_slot == snapshot->weapon_slot_plus_one)
                return FALSE;
            client_weapon_attempt_actor = character;
            client_weapon_attempt_component = weapon;
            client_weapon_attempt_slot = snapshot->weapon_slot_plus_one;
            SudekiMpWeaponActivationResult result =
                SudekiMpActivateCharacterWeapon(character, slot);
            SudekiMpLogFormat("lan_arena_client_replica event=client_weapon slot=%u status=%s policy=host_observed_training_equipment\r\n",
                slot, SudekiMpWeaponActivationStatusName(result.status));
            if (result.status != SUDEKIMP_WEAPON_ACTIVATION_STARTED) return FALSE;
            /* Keep the previous confirmed slot for a renderer-only swap.
             * The presentation path revalidates actor/component/renderer;
             * initial seeding and replaced owners never trigger a clip. */
        } else {
            client_weapon_attempt_actor = NULL;
            client_weapon_attempt_component = NULL;
            client_weapon_attempt_slot = 0u;
        }
    }
    coordinates[0] = snapshot->x;
    coordinates[1] = snapshot->y;
    coordinates[2] = snapshot->z;
    facing[0] = snapshot->facing_x;
    facing[1] = 0.0f;
    facing[2] = snapshot->facing_z;
    dx = coordinates[0] - *(float *)((uint8_t *)position + 0x18u);
    dy = coordinates[1] - *(float *)((uint8_t *)position + 0x1cu);
    dz = coordinates[2] - *(float *)((uint8_t *)position + 0x20u);
    if (expected_type == seat_host_type()) {
        static DWORD last_tal_pos_diag;
        DWORD pn = GetTickCount();
        if (last_tal_pos_diag == 0u ||
            (DWORD)(pn - last_tal_pos_diag) >= 300u) {
            last_tal_pos_diag = pn;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_tal_pos_diag "
                "snap=%.3f,%.3f cur_before=%.3f,%.3f delta=%.4f anim=%u\r\n",
                (double)coordinates[0], (double)coordinates[2],
                (double)*(float *)((uint8_t *)position + 0x18u),
                (double)*(float *)((uint8_t *)position + 0x20u),
                (double)sqrtf(dx * dx + dy * dy + dz * dz),
                (unsigned)snapshot->animation_state);
        }
    }
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
    local_first_person_facing =
        (expected_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
         expected_type == SUDEKIMP_LAN_ARENA_ELCO_TYPE) && combat_mode &&
        client_ailish_first_person_camera_owns_facing(character);
    if (actor == seat_host_actor() &&
        expected_type == SUDEKIMP_LAN_ARENA_BUKI_TYPE &&
        !snapshot->skill_active) {
        SudekiMpCharacterSkillState skill;
        if (!SudekiMpObserveCharacterSkill(character, &skill)) return FALSE;
        if (!skill.active && !SudekiMpBukiReplicaSyncFacing((HMODULE)game_base,
                character, *(void **)(character + CHARACTER_ARBITER_OFFSET),
                facing)) return FALSE;
    }
    /* SetForward rebuilds and dirties the complete CPosition basis. Apply a
     * mod-owned 0.5-degree hysteresis so interpolation noise cannot force a
     * rebuild on every rendered frame. The local first-person camera is the
     * sole exception: replaying a delayed host-facing sample here undoes its
     * mouse turn before the newly transmitted aim can make the round trip. */
    if (SudekiMpLanArenaClientShouldApplyHostFacing(
            expected_type == seat_host_type() ? 0u : 1u,
            local_first_person_facing) &&
        (!isfinite(facing_dot) || facing_dot <= 0.99996f)) {
        call_position_set_forward(position, facing);
    }
    resources_applied = SudekiMpCleanroomEngineSetActorResources(
        actor, (float)snapshot->hp, (float)snapshot->sp);
    if (resources_applied) {
        unsigned int actor_index =
            expected_type == seat_host_type() ? 0u : 1u;
        if (presentation_allowed) {
            if (!apply_actor_presentation(
                    character, snapshot, actor_index,
                    combat_mode, FALSE)) {
                /* A frame with an unverified presentation boundary is not a
                 * valid replica frame. Keep the applied pointers null so
                 * ApplyLatest cannot publish diagnostics or admit this
                 * partially applied sample as its new render source. */
                resources_applied = FALSE;
            }
        } else {
            BOOL native_skill_owns_presentation = FALSE;
            /* Renderer readiness may disappear during a combat transition,
             * actor swap, or native modal. It is not permission to stop
             * observing an already-started CSkill: keep its exact lease,
             * input isolation, owner view, and damage containment serviced
             * even while ordinary presentation writes are fail-closed. */
            if (!service_native_skill_presentation(
                    character, snapshot, actor_index, combat_mode,
                    &native_skill_owns_presentation)) {
                resources_applied = FALSE;
            }
        }
    }
    if (resources_applied) {
        *applied_character = character;
        *applied_position = position;
    }
    return resources_applied;
}

static SudekiMpLanHitCursor dummy_hit_cursor;
static BOOL replay_dummy_hit(void *context, const SudekiMpLanArenaHitFeedback *hit) {
    SudekiMpLanHitTarget *target = context;
    if (SudekiMpCleanroomEngineGenericEntity("MON_TrainingDummy") != target->entity) return FALSE;
    return SudekiMpLanHitReplayNative((HMODULE)game_base, target, hit);
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

static BOOL apply_training_dummy_feedback(
    const SudekiMpLanArenaSnapshot *confirmed, uint64_t session
) {
    SudekiMpLanHitTarget target;
    void *entity;
    if (confirmed->enemy_count == 0u) {
        memset(&dummy_hit_cursor, 0, sizeof(dummy_hit_cursor));
        return TRUE;
    }
    if (confirmed->enemy_count != 1u ||
        confirmed->enemies[0].native_entity_id !=
            SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID) return FALSE;
    entity = SudekiMpCleanroomEngineGenericEntity("MON_TrainingDummy");
    if (!SudekiMpLanHitResolveTarget(entity, session, &target)) return FALSE;
    return SudekiMpLanHitConsume(&dummy_hit_cursor, session, (uintptr_t)entity,
        confirmed->host_tick, &confirmed->enemies[0], replay_dummy_hit, &target);
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

typedef struct FirstPersonFrameSample {
    uint32_t tick;
    uint32_t actor;
    uint32_t position;
    uint32_t renderer;
    uint32_t camera;
    uint32_t valid;
    float actor_position[3];
    float model_matrix[16];
    float camera_matrix[16];
} FirstPersonFrameSample;

typedef struct FirstPersonFrameTrace {
    uint32_t frame;
    FirstPersonFrameSample phase[6];
} FirstPersonFrameTrace;

/* Published only after all six game-thread boundaries, with an even/odd
 * revision for external read-only sampling. No native pointer is retained
 * for later dereference and no per-frame logging or allocation is needed. */
static volatile LONG first_person_frame_trace_revision;
static volatile FirstPersonFrameTrace first_person_frame_trace;

void SudekiMpLanArenaClientObserveFirstPersonFrame(unsigned int phase) {
    static int enabled = -1;
    static FirstPersonFrameTrace pending;
    FirstPersonFrameSample sample;
    uint8_t *character, *position, *component, *wrapper, *renderer, *animation;
    uint8_t *mode, *camera_member, *camera, *render_state;
    DWORD saved_error = GetLastError();
    unsigned int i;
    if (phase >= 6u) return;
    if (enabled < 0) {
        char setting[8];
        enabled = GetEnvironmentVariableA("SUDEKIMP_FP_FRAME_TRACE",
            setting, sizeof(setting)) == 1u && setting[0] == '1';
    }
    if (!enabled) {
        SetLastError(saved_error);
        return;
    }
    if (phase == 0u) {
        ++pending.frame;
        ZeroMemory(pending.phase, sizeof(pending.phase));
    }
    ZeroMemory(&sample, sizeof(sample));
    sample.tick = GetTickCount();
    if (game_base == NULL || !client_session_authenticated()) goto publish;
    character = (uint8_t *)SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    if (!client_ailish_first_person_camera_owns_facing(character) ||
        !readable_memory(character, 0x138u)) goto publish;
    position = *(uint8_t **)(character + 0x44u);
    component = *(uint8_t **)(character + 0x134u);
    if (!readable_memory(position, 0xb8u) ||
        !readable_memory(component, 0x168u) ||
        *(void **)(position + 0x10u) != character ||
        *(void **)(component + 0x10u) != character) goto publish;
    wrapper = *(uint8_t **)(component + AILISH_FIRST_PERSON_WRAPPER_OFFSET);
    if (*(void **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) != wrapper ||
        !readable_memory(wrapper, 0x14u)) goto publish;
    renderer = *(uint8_t **)(wrapper + 8u);
    animation = *(uint8_t **)(wrapper + 0x10u);
    if (!readable_memory(renderer, 0xd0u) ||
        !readable_memory(animation, sizeof(void *)) ||
        *(void **)animation != game_base + RVA_ANIMATION_RENDERER_VTABLE ||
        !readable_memory(game_base + RVA_GAME_CAMERA_MODE_GLOBAL, sizeof(void *)))
        goto publish;
    mode = *(uint8_t **)(game_base + RVA_GAME_CAMERA_MODE_GLOBAL);
    if (!readable_memory(mode, 0x10u)) goto publish;
    camera_member = *(uint8_t **)(mode + 0x0cu);
    if ((uintptr_t)camera_member < 0x2cu) goto publish;
    camera = camera_member - 0x2cu;
    if (!readable_memory(camera, 0x38u)) goto publish;
    render_state = *(uint8_t **)(camera + 0x34u);
    if (!readable_memory(render_state, 0xd0u)) goto publish;
    memcpy(sample.actor_position, position + 0x18u, sizeof(sample.actor_position));
    memcpy(sample.model_matrix, renderer + 0x90u, sizeof(sample.model_matrix));
    memcpy(sample.camera_matrix, render_state + 0x90u, sizeof(sample.camera_matrix));
    for (i = 0u; i < 16u; ++i) {
        if (!isfinite(sample.model_matrix[i]) || !isfinite(sample.camera_matrix[i]))
            goto publish;
    }
    for (i = 0u; i < 3u; ++i) if (!isfinite(sample.actor_position[i])) goto publish;
    sample.actor = (uint32_t)(uintptr_t)character;
    sample.position = (uint32_t)(uintptr_t)position;
    sample.renderer = (uint32_t)(uintptr_t)renderer;
    sample.camera = (uint32_t)(uintptr_t)camera;
    sample.valid = 1u;
publish:
    pending.phase[phase] = sample;
    if (phase == 5u) {
        InterlockedIncrement(&first_person_frame_trace_revision);
        first_person_frame_trace = pending;
        InterlockedIncrement(&first_person_frame_trace_revision);
    }
    SetLastError(saved_error);
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

static void discard_client_replica_frame_state(void) {
    memset(&dummy_hit_cursor, 0, sizeof(dummy_hit_cursor));
    /* Retire exact visual clones before releasing their resource caches.
     * Synchronous native entry defers cleanup to the outer service call;
     * failed cleanup retains backend leases for retry, never forgetting
     * observers that native destruction may still touch. */
    if (InterlockedCompareExchange(
            &client_spirit_vfx_call_depth, 0, 0) == 0) {
        (void)release_client_spirit_vfx_cache("snapshot_discard");
    }
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaSharedSimulationReset(&replica_simulation);
    SudekiMpLanArenaSpiritAudioCursorReset(&spirit_audio_cursor);
    spirit_audio_replay_failure_logged = FALSE;
    reset_client_spirit_vfx_replay(TRUE);
    SudekiMpLanArenaReplicaRenderClockReset(&replica_render_clock);
    memset(presentation_leases, 0, sizeof(presentation_leases));
    memset(&ailish_first_person_lease, 0,
        sizeof(ailish_first_person_lease));
    ailish_first_person_failure = NULL;
    client_ailish_combat_graph_failure = NULL;
    client_combat_transition_pending = FALSE;
    client_combat_transition_session_token = 0u;
    reset_client_combat_transition_actor_leases(GetTickCount());
    memset(&replica_diagnostics, 0, sizeof(replica_diagnostics));
    clear_last_applied_frame();
}

static void retain_client_replica_callbacks(
    const char *reason,
    DWORD error
) {
    HMODULE pinned_module = NULL;
    BOOL pinned = client_replica_containment_pinned;
    if (!pinned) {
        pinned = GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)&SudekiMpResetLanArenaClientReplica,
            &pinned_module);
        if (pinned) client_replica_containment_pinned = TRUE;
    }
    SudekiMpLogFormat(
        "lan_arena_client_replica event=client_replica_containment "
        "state=retained reason=%s win32_error=%lu module_pinned=%u "
        "policy=keep_live_hooks_dependencies_and_damage_guard_until_confirmed_drain\r\n",
        reason != NULL ? reason : "unspecified",
        (unsigned long)error,
        pinned ? 1u : 0u);
    SetLastError(error);
}

BOOL SudekiMpInitializeLanPartyClientReplica(HMODULE game_module,
    SudekiMpLanPartySession *session) {
    unsigned int seat=SudekiMpLanPartyLocalSeat(session);
    if (!session || !seat || seat>=4u || party_replica_session || game_base || set_position)
        return FALSE;
    /* Publish containment before installing a reachable native damage hook. */
    party_replica_session=session;
    party_present_valid=FALSE;
    memset(party_presentation_leases,0,sizeof(party_presentation_leases));
    memset(&party_presentation_generation,0,sizeof(party_presentation_generation));
    ZeroMemory(&party_ailish_first_person_lease,
        sizeof(party_ailish_first_person_lease));
    party_combat_mode_lease_valid=FALSE; party_combat_mode_token=0;
    party_combat_mode_generation=0;
    if(SudekiMpInitializeLanArenaClientReplica(game_module)) return TRUE;
    if(!client_apply_damage_hook.installed && !client_skill_camera_hook.installed &&
        !client_skill_speed_hook.installed) party_replica_session=NULL;
    return FALSE;
}

BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    uint32_t initial_alternate_speed_bits;
    if (client_replica_reset_pending &&
        !SudekiMpResetLanArenaClientReplica()) {
        return FALSE;
    }
    /* Keep the audio and VFX replay dependencies separately observable. No
     * other LAN hook owns these native entries, so either mismatch must fail
     * closed before any replica hook is added. */
    if (base != NULL &&
        !SudekiMpLanArenaSpiritAudioReplayImageMatches(game_module)) {
        SudekiMpLogWrite(
            "lan_arena_client_replica event=exact_preflight state=rejected "
            "reason=spirit_audio_replay_signature_mismatch "
            "get_sound_rva=0x000170b0 play_cue_rva=0x00017090\r\n");
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (base != NULL &&
        !SudekiMpLanArenaSpiritVfxVisualImageMatches(game_module)) {
        SudekiMpLogWrite(
            "lan_arena_client_replica event=exact_preflight state=rejected "
            "reason=spirit_vfx_replay_signature_mismatch "
            "policy=no_native_sfx_entry_on_unknown_image\r\n");
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if ((client_combat_mode_lease_valid && !restore_client_combat_mode()) ||
        base == NULL || set_position != NULL ||
        !SudekiMpLanHitImageMatches(game_module) ||
        !SudekiMpBukiReplicaNativeImageMatches(game_module) ||
        position_world_matrix != NULL ||
        ailish_ranged_presentation_refresh != NULL ||
        weapon_set_visible != NULL || ranged_weapon_reattach != NULL ||
        set_forward != NULL ||
        client_apply_damage_hook.installed ||
        client_skill_camera_hook.installed ||
        client_skill_speed_hook.installed || original_apply_damage != NULL ||
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
        memcmp(base + RVA_ARBITER_COMBAT_INPUT,
            expected_arbiter_combat_input_entry,
            sizeof(expected_arbiter_combat_input_entry)) != 0 ||
        memcmp(base + RVA_APPLY_DAMAGE,
            expected_apply_damage_entry,
            sizeof(expected_apply_damage_entry)) != 0 ||
        memcmp(base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            expected_camera_manager_set_render_camera_entry,
            sizeof(expected_camera_manager_set_render_camera_entry)) != 0 ||
        memcmp(base + RVA_GAME_SPEED_SET_MODE,
            expected_game_speed_set_mode_entry,
            sizeof(expected_game_speed_set_mode_entry)) != 0 ||
        memcmp(base + RVA_FIXED_ALTERNATE_SPEED,
            expected_fixed_alternate_speed,
            sizeof(expected_fixed_alternate_speed)) != 0 ||
        /* Character-independent ANIMID path. Prologues read from the exact
         * image at these RVAs; each encodes the offset the decompile relies
         * on (0x131 guard / 0xDC animation table / the play entry sequence). */
        memcmp(base + RVA_MODEL_GET_CURRENT_ANIM,
            (const uint8_t[]){
                0x80u, 0xb9u, 0x31u, 0x01u, 0x00u, 0x00u, 0x03u, 0x75u,
                0x1eu, 0x8bu, 0x81u, 0xf8u},
            12u) != 0 ||
        memcmp(base + RVA_MODEL_RESOLVE_ANIM,
            (const uint8_t[]){
                0x83u, 0xecu, 0x14u, 0x8bu, 0x80u, 0xdcu, 0x00u, 0x00u,
                0x00u, 0x53u, 0x33u, 0xdbu},
            12u) != 0 ||
        memcmp(base + RVA_MODEL_PLAY_ANIM,
            (const uint8_t[]){
                0x53u, 0x8au, 0x5cu, 0x24u, 0x0cu, 0x55u, 0x8bu, 0x6cu,
                0x24u, 0x14u, 0x56u, 0x85u},
            12u) != 0 ||
        memcmp(base + RVA_AILISH_RANGED_PRESENTATION_REFRESH,
            expected_ailish_ranged_presentation_refresh_entry,
            sizeof(expected_ailish_ranged_presentation_refresh_entry)) != 0 ||
        !relative_call_targets(
            base + RVA_AILISH_RANGED_WEAPON_REATTACH_CALL,
            base + RVA_RANGED_WEAPON_REATTACH) ||
        memcmp(base + RVA_RANGED_WEAPON_REATTACH,
            expected_ranged_weapon_reattach_entry,
            sizeof(expected_ranged_weapon_reattach_entry)) != 0 ||
        memcmp(base + RVA_WEAPON_SET_VISIBLE,
            expected_weapon_set_visible_entry,
            sizeof(expected_weapon_set_visible_entry)) != 0 ||
        !animation_renderer_signatures_match(base)) {
        SudekiMpLogWrite(
            "lan_arena_client_replica event=exact_preflight state=rejected "
            "reason=non_audio_signature_or_existing_ownership_mismatch "
            "spirit_audio_replay=exact spirit_vfx_replay=exact\r\n");
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    memcpy(&initial_alternate_speed_bits,
        base + RVA_FIXED_ALTERNATE_SPEED,
        sizeof(initial_alternate_speed_bits));
    game_base = base;
    client_skill_original_alternate_speed_bits =
        initial_alternate_speed_bits;
    client_skill_speed_override_active = FALSE;
    client_skill_camera_suppression_logged = FALSE;
    client_skill_speed_trace_state = -1;
    remote_tal_skill_view_trace_state = -1;
    clear_remote_tal_skill_view_lease();
    InterlockedExchange(&client_skill_activation_depth, 0);
    InterlockedExchange(&client_spirit_vfx_call_depth, 0);
    client_skill_activation_actor_index = -1;
    client_remote_tal_skill_input_isolation_active = FALSE;
    if (!SudekiMpInstallInlineHook(
            &client_apply_damage_hook,
            base + RVA_APPLY_DAMAGE,
            expected_apply_damage_entry,
            sizeof(expected_apply_damage_entry),
            block_client_replica_damage)) {
        game_base = NULL;
        return FALSE;
    }
    original_apply_damage =
        (ApplyDamageFunction)client_apply_damage_hook.trampoline;
    if (!SudekiMpInstallInlineHook(
            &client_skill_camera_hook,
            base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            expected_camera_manager_set_render_camera_entry,
            sizeof(expected_camera_manager_set_render_camera_entry),
            preserve_client_skill_camera) ||
        !SudekiMpInstallInlineHook(
            &client_skill_speed_hook,
            base + RVA_GAME_SPEED_SET_MODE,
            expected_game_speed_set_mode_entry,
            sizeof(expected_game_speed_set_mode_entry),
            preserve_client_skill_realtime)) {
        DWORD error = GetLastError();
        DWORD restore_error = ERROR_SUCCESS;
        BOOL speed_restored = SudekiMpRestoreInlineHook(
            &client_skill_speed_hook);
        BOOL camera_restored;
        BOOL damage_restored;
        if (!speed_restored) restore_error = GetLastError();
        camera_restored = SudekiMpRestoreInlineHook(
            &client_skill_camera_hook);
        if (!camera_restored && restore_error == ERROR_SUCCESS) {
            restore_error = GetLastError();
        }
        damage_restored = SudekiMpRestoreInlineHook(
            &client_apply_damage_hook);
        if (!damage_restored && restore_error == ERROR_SUCCESS) {
            restore_error = GetLastError();
        }
        if (!speed_restored || !camera_restored || !damage_restored) {
            client_replica_reset_pending = TRUE;
            retain_client_replica_callbacks(
                "install_rollback_unconfirmed",
                restore_error == ERROR_SUCCESS ?
                    ERROR_WRITE_FAULT : restore_error);
            return FALSE;
        }
        original_apply_damage = NULL;
        game_base = NULL;
        SetLastError(error);
        return FALSE;
    }
    set_position = (PositionSetterFunction)(base + RVA_INTERNAL_POSITION_SETTER);
    position_world_matrix = (PositionWorldMatrixFunction)(
        base + RVA_POSITION_WORLD_MATRIX);
    ailish_ranged_presentation_refresh =
        (AilishRangedPresentationRefreshFunction)(
            base + RVA_AILISH_RANGED_PRESENTATION_REFRESH);
    weapon_set_visible = (WeaponSetVisibleFunction)(
        base + RVA_WEAPON_SET_VISIBLE);
    ranged_weapon_reattach = base + RVA_RANGED_WEAPON_REATTACH;
    set_forward = base + RVA_POSITION_SET_FORWARD;
    model_resolve_anim = base + RVA_MODEL_RESOLVE_ANIM;
    model_play_anim = base + RVA_MODEL_PLAY_ANIM;
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaSharedSimulationReset(&replica_simulation);
    SudekiMpLanArenaSpiritAudioCursorReset(&spirit_audio_cursor);
    spirit_audio_replay_failure_logged = FALSE;
    reset_client_spirit_vfx_replay(TRUE);
    SudekiMpLanArenaReplicaRenderClockReset(&replica_render_clock);
    memset(presentation_leases, 0, sizeof(presentation_leases));
    memset(&ailish_first_person_lease, 0,
        sizeof(ailish_first_person_lease));
    memset(&tal_native_presentation_lease, 0,
        sizeof(tal_native_presentation_lease));
    memset(native_skill_leases, 0, sizeof(native_skill_leases));
    client_replica_reset_pending = FALSE;
    client_damage_block_logged = FALSE;
    ailish_first_person_failure = NULL;
    client_ailish_combat_graph_failure = NULL;
    client_combat_transition_pending = FALSE;
    client_combat_transition_target = FALSE;
    client_combat_transition_session_token = 0u;
    client_remote_tal_lease_actor = NULL;
    client_remote_tal_lease_generation = 0u;
    reset_client_combat_transition_actor_leases(GetTickCount());
    memset(&replica_diagnostics, 0, sizeof(replica_diagnostics));
    clear_last_applied_frame();
    return TRUE;
}

BOOL SudekiMpResetLanArenaClientReplica(void) {
    DWORD restore_error = ERROR_SUCCESS;

    if(party_combat_mode_lease_valid && !SudekiMpLanPartyClientRestoreCombatMode()) {
        restore_error=GetLastError();
        if(restore_error==ERROR_SUCCESS) restore_error=ERROR_BUSY;
        client_replica_reset_pending=TRUE;
        retain_client_replica_callbacks("party_combat_mode_restore_unconfirmed",restore_error);
        return FALSE;
    }
    if(!party_retire_ailish_first_person(NULL)) {
        client_replica_reset_pending=TRUE;
        retain_client_replica_callbacks(
            "party_ailish_first_person_retirement_unconfirmed",ERROR_BUSY);
        return FALSE;
    }

    if (!SudekiMpLanArenaClientSpiritViewEndFrame()) {
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks("spirit_view_restore_unconfirmed", ERROR_BUSY);
        return FALSE;
    }

    if (InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) > 0 ||
        InterlockedCompareExchange(
            &client_spirit_vfx_call_depth, 0, 0) > 0) {
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "native_presentation_entry_in_flight", ERROR_BUSY);
        return FALSE;
    }
    if (!release_client_spirit_vfx_cache("replica_reset")) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "spirit_vfx_cache_release_unconfirmed", restore_error);
        return FALSE;
    }
    client_skill_activation_actor_index = -1;
    if (!drain_ailish_native_ranged()) {
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks("native_ranged_drain_pending", ERROR_BUSY);
        return FALSE;
    }
    ZeroMemory(&ailish_native_ranged_lease, sizeof(ailish_native_ranged_lease));
    elco_weapon_cursor_owner = NULL;
    elco_weapon_cursor_session = 0;
    elco_weapon_cursor = 0;
    weapon_snapshot_received_at = 0;
    if (!drain_tal_native_action_lease()) {
        client_replica_reset_pending = TRUE;
        discard_client_replica_frame_state();
        retain_client_replica_callbacks(
            "native_tal_action_drain_pending", ERROR_BUSY);
        return FALSE;
    }
    if (!drain_native_skill_leases("replica_reset")) {
        client_replica_reset_pending = TRUE;
        discard_client_replica_frame_state();
        retain_client_replica_callbacks(
            "native_skill_drain_pending", ERROR_BUSY);
        return FALSE;
    }
    if (SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        client_replica_reset_pending = TRUE;
        discard_client_replica_frame_state();
        retain_client_replica_callbacks(
            "ranged_combat_prime_timer_pending", ERROR_BUSY);
        return FALSE;
    }
    if (!set_client_remote_tal_skill_input_isolation(
            FALSE, "replica_reset")) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "noncaster_input_isolation_restore_unconfirmed",
            restore_error);
        return FALSE;
    }
    if (!retire_remote_tal_skill_view_lease("replica_reset")) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "owner_view_restore_unconfirmed", restore_error);
        return FALSE;
    }
    if (client_combat_mode_lease_valid && !restore_client_combat_mode()) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "combat_mode_restore_unconfirmed", restore_error);
        return FALSE;
    }
    /* Restoring an originally enabled combat state uses Sudeki's ordinary
     * transition and may arm the ranged UI prime. Recheck after restoration;
     * never remove hooks or admit a new session while that game-thread work
     * is still pending. */
    if (SudekiMpCleanroomEngineRangedCombatPrimePending()) {
        client_replica_reset_pending = TRUE;
        discard_client_replica_frame_state();
        retain_client_replica_callbacks(
            "post_combat_restore_ranged_prime_pending", ERROR_BUSY);
        return FALSE;
    }
    if (client_skill_speed_override_active &&
        !set_client_skill_realtime_scale(FALSE)) {
        restore_error = GetLastError();
        SudekiMpLogFormat(
            "lan_arena_client_replica event=client_replica_hook_restore "
            "state=failed speed_scale=0 camera=0 damage=0 win32_error=%lu "
            "policy=retain_live_callbacks_and_reject_reinstall\r\n",
            (unsigned long)restore_error);
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "speed_scale_restore_unconfirmed", restore_error);
        return FALSE;
    }
    if (!SudekiMpRestoreInlineHook(&client_skill_speed_hook)) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "speed_hook_restore_unconfirmed", restore_error);
        return FALSE;
    }
    if (!SudekiMpRestoreInlineHook(&client_skill_camera_hook)) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "camera_hook_restore_unconfirmed", restore_error);
        return FALSE;
    }
    /* Damage is restored last. Any earlier failure therefore leaves the
     * authority guard installed while its callback dependencies are retained. */
    if (!SudekiMpRestoreInlineHook(&client_apply_damage_hook)) {
        restore_error = GetLastError();
        client_replica_reset_pending = TRUE;
        retain_client_replica_callbacks(
            "damage_hook_restore_unconfirmed", restore_error);
        return FALSE;
    }
    original_apply_damage = NULL;
    SudekiMpLanArenaReplicaReset(&replica);
    SudekiMpLanArenaSharedSimulationReset(&replica_simulation);
    SudekiMpLanArenaSpiritAudioCursorReset(&spirit_audio_cursor);
    spirit_audio_replay_failure_logged = FALSE;
    reset_client_spirit_vfx_replay(TRUE);
    set_position = NULL;
    party_replica_session = NULL;
    party_present_valid=FALSE;
    party_combat_mode_lease_valid=FALSE; party_combat_mode_token=0;
    party_combat_mode_generation=0;
    memset(party_presentation_leases,0,sizeof(party_presentation_leases));
    memset(&party_presentation_generation,0,sizeof(party_presentation_generation));
    ZeroMemory(&party_ailish_first_person_lease,
        sizeof(party_ailish_first_person_lease));
    position_world_matrix = NULL;
    ailish_ranged_presentation_refresh = NULL;
    weapon_set_visible = NULL;
    ranged_weapon_reattach = NULL;
    set_forward = NULL;
    model_resolve_anim = NULL;
    model_play_anim = NULL;
    game_base = NULL;
    SudekiMpLanArenaReplicaRenderClockReset(&replica_render_clock);
    memset(presentation_leases, 0, sizeof(presentation_leases));
    memset(&ailish_first_person_lease, 0,
        sizeof(ailish_first_person_lease));
    memset(&tal_native_presentation_lease, 0,
        sizeof(tal_native_presentation_lease));
    memset(native_skill_leases, 0, sizeof(native_skill_leases));
    client_replica_reset_pending = FALSE;
    client_remote_tal_skill_input_isolation_active = FALSE;
    client_skill_original_alternate_speed_bits = 0u;
    client_skill_speed_override_active = FALSE;
    client_skill_camera_suppression_logged = FALSE;
    client_skill_speed_trace_state = -1;
    remote_tal_skill_view_trace_state = -1;
    client_damage_block_logged = FALSE;
    ailish_first_person_failure = NULL;
    client_ailish_combat_graph_failure = NULL;
    client_combat_transition_pending = FALSE;
    client_combat_transition_target = FALSE;
    client_combat_transition_session_token = 0u;
    client_remote_tal_lease_actor = NULL;
    client_remote_tal_lease_generation = 0u;
    reset_client_combat_transition_actor_leases(0u);
    memset(&replica_diagnostics, 0, sizeof(replica_diagnostics));
    clear_last_applied_frame();
    InterlockedExchange(&client_spirit_vfx_call_depth, 0);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

void SudekiMpLanArenaClientReplicaDiscardSnapshots(void) {
    BOOL all_native_skills_drained;
    if (InterlockedCompareExchange(
            &client_skill_activation_depth, 0, 0) == 0) {
        client_skill_activation_actor_index = -1;
    }
    all_native_skills_drained = drain_native_skill_leases(
        "snapshot_discard");
    if (all_native_skills_drained &&
        !retire_remote_tal_skill_view_lease("snapshot_discard")) {
        all_native_skills_drained = FALSE;
    }
    retire_client_skill_isolation_if_idle();
    discard_client_replica_frame_state();
    if (all_native_skills_drained) client_damage_block_logged = FALSE;
    if (client_replica_reset_pending && all_native_skills_drained) {
        (void)SudekiMpResetLanArenaClientReplica();
    }
}

BOOL SudekiMpLanArenaClientReplicaApplyLatest(void) {
    SudekiMpLanArenaSnapshot received;
    SudekiMpLanArenaSnapshot accepted;
    SudekiMpLanArenaSnapshot snapshot;
    SudekiMpLanArenaSessionStatus status;
    DWORD now = GetTickCount();
    uint32_t render_host_tick;
    uint32_t previous_render_host_tick = replica_render_clock.host_tick;
    uint32_t previous_render_local_tick = replica_render_clock.local_tick;
    uint32_t previous_render_generation = replica_render_clock.stream_generation;
    BOOL previous_render_clock_initialized =
        replica_render_clock.initialized != 0u;
    BOOL action_clock_protected;
    BOOL dummy_feedback_applied = TRUE;
    BOOL dummy_state_applied;
    unsigned int presentation_ready_mask;
    if (client_replica_reset_pending ||
        set_position == NULL || set_forward == NULL ||
        !client_session_status(&status)) {
        SudekiMpLanArenaClientReplicaDiscardSnapshots();
        SetLastError(client_replica_reset_pending ?
            ERROR_BUSY : ERROR_INVALID_STATE);
        return FALSE;
    }
    /* Never write the preceding frame's basis here: Ailish's local mouse may
     * already have rotated her camera. Validate only. The RenderStart wrapper
     * refreshes the lease from the newly published owner basis, then the
     * presentation boundary reasserts it after remote-skill mutations. */
    if (remote_tal_skill_view_lease.owner_view.valid &&
        !verify_remote_tal_skill_view()) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            &replica_simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
            status.session_token)) {
        if (!release_client_spirit_vfx_cache("session_generation_changed")) {
            return FALSE;
        }
        SudekiMpLanArenaSpiritAudioCursorReset(&spirit_audio_cursor);
        reset_client_spirit_vfx_replay(TRUE);
        if (!SudekiMpLanArenaSharedSimulationBegin(
                &replica_simulation,
                SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
                status.session_token)) {
            SetLastError(ERROR_INVALID_STATE);
            return FALSE;
        }
    }
    while (SudekiMpLanArenaSessionTakeRemoteSnapshot(&received)) {
        unsigned int audio_replayed = 0u;
        if (!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
                &replica_simulation, status.session_token, &received) ||
            !SudekiMpLanArenaSharedSimulationReadFrame(
                &replica_simulation, &accepted, NULL) ||
            !SudekiMpLanArenaReplicaPush(&replica, &accepted)) return FALSE;
        weapon_snapshot_received_at = GetTickCount();
        if (!SudekiMpLanArenaSpiritAudioConsumeSnapshot(
                &spirit_audio_cursor, &accepted,
                replay_client_spirit_audio, NULL, &audio_replayed)) {
            if (!spirit_audio_replay_failure_logged) {
                spirit_audio_replay_failure_logged = TRUE;
                SudekiMpLogFormat(
                    "lan_arena_client_replica event=spirit_audio "
                    "state=rejected snapshot_sequence=%lu skill_sequence=%u "
                    "win32_error=%lu "
                    "policy=presentation_only_retry_without_frame_rejection\r\n",
                    (unsigned long)accepted.sequence,
                    (unsigned int)accepted.seat[0].skill_sequence,
                    (unsigned long)GetLastError());
            }
        } else if (audio_replayed != 0u) {
            spirit_audio_replay_failure_logged = FALSE;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=spirit_audio state=replayed "
                "snapshot_sequence=%lu replayed=%u cue=start "
                "policy=local_csound_only_exact_sequence_once\r\n",
                (unsigned long)accepted.sequence,
                audio_replayed);
        }
    }
    /* A host-confirmed hit is an independently leased target event, not an
     * actor animation. Service the latest admitted journal on the game thread
     * before interpolation/readiness gates: a ranged-fire presentation gap
     * must not delay (or expire) damage numbers already received from the host.
     * Retry the same retained journal on later frames; its cursor deduplicates
     * successful events. Never sample it back through the actor render clock. */
    if (replica.latest_valid &&
        replica.latest.match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE) {
        dummy_feedback_applied = apply_training_dummy_feedback(
            &replica.latest, status.session_token);
    }
    action_clock_protected =
        SudekiMpLanArenaReplicaActionTimelineBuffered(&replica);
    if (!SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(
            &replica, &replica_render_clock, now,
            !action_clock_protected, &render_host_tick)) {
        return FALSE;
    }
    if (!SudekiMpLanArenaReplicaSample(
            &replica, render_host_tick, &snapshot) ||
        snapshot.match_state != 1u ||
        !synchronize_client_combat_mode(
            snapshot.combat_enabled, status.session_token)) {
        return FALSE;
    }
    /* Position/HP still use the sampled world frame, but do not wait for
     * either player's independent animation writer to succeed. */
    dummy_state_applied = snapshot.enemy_count == 0u ||
        (snapshot.enemy_count == 1u && apply_training_dummy(&snapshot.enemies[0]));
    presentation_ready_mask = client_combat_presentation_ready_mask(
        status.session_token);
    replica_diagnostics.valid = 0u;
    memset(replica_diagnostics.actor, 0,
        sizeof(replica_diagnostics.actor));
    capture_actor_diagnostics(
        0u, seat_host_actor(), &snapshot.seat[0]);
    capture_actor_diagnostics(
        1u, seat_client_actor(), &snapshot.seat[1]);
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
            &snapshot.seat[1], seat_client_actor(),
            seat_client_type(),
            snapshot.combat_enabled != 0u,
            (presentation_ready_mask & 0x02u) != 0u,
            &last_applied_characters[1],
            &last_applied_positions[1]);
        BOOL tal_applied = apply_actor(
            &snapshot.seat[0], seat_host_actor(),
            seat_host_type(),
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
    replica_diagnostics.render_clock_local_elapsed_ms =
        previous_render_clock_initialized &&
        previous_render_generation == replica_render_clock.stream_generation ?
            now - previous_render_local_tick : 0u;
    replica_diagnostics.render_clock_advance_ms =
        previous_render_clock_initialized &&
        previous_render_generation == replica_render_clock.stream_generation ?
            render_host_tick - previous_render_host_tick : 0u;
    replica_diagnostics.tal_action_sequence = snapshot.seat[0].action_sequence;
    replica_diagnostics.tal_action_phase_q8 = snapshot.seat[0].action_phase_q8;
    replica_diagnostics.tal_action_terminal_phase_q8 =
        snapshot.seat[0].action_terminal_phase_q8;
    replica_diagnostics.tal_idle_entry_phase_q8 =
        snapshot.seat[0].idle_entry_phase_q8;
    replica_diagnostics.tal_animation_state = snapshot.seat[0].animation_state;
    replica_diagnostics.tal_action_variant = snapshot.seat[0].action_variant;
    replica_diagnostics.tal_action_phase_valid =
        snapshot.seat[0].action_phase_valid;
    replica_diagnostics.tal_action_retirement_valid =
        snapshot.seat[0].action_retirement_valid;
    replica_diagnostics.action_clock_protected =
        action_clock_protected ? 1u : 0u;
    capture_camera_diagnostics();
    capture_actor_diagnostics(
        0u, seat_host_actor(), &snapshot.seat[0]);
    capture_actor_diagnostics(
        1u, seat_client_actor(), &snapshot.seat[1]);
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
    return dummy_feedback_applied && dummy_state_applied;
}

BOOL SudekiMpLanArenaClientReplicaReassertPresentation(void) {
    const SudekiMpCleanroomActor actors[2] = {
        seat_host_actor(),
        seat_client_actor()
    };
    const uint8_t expected_types[2] = {
        seat_host_type(),
        seat_client_type()
    };
    const SudekiMpLanArenaActorSnapshot *snapshots[2];
    uint8_t *characters[2];
    SudekiMpLanArenaSessionStatus status;
    unsigned int actor_index;
    unsigned int presentation_ready_mask;
    BOOL applied[2] = { FALSE, FALSE };
    if (!client_session_status(&status) || !replica_diagnostics.valid ||
        last_applied_snapshot.match_state != 1u) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    presentation_ready_mask = client_combat_presentation_ready_mask(
        status.session_token);
    if (presentation_ready_mask == 0u) {
        return reassert_remote_tal_skill_view();
    }
    snapshots[0] = &last_applied_snapshot.seat[0];
    snapshots[1] = &last_applied_snapshot.seat[1];
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
            if (remote_tal_skill_view_lease.owner_view.valid) {
                (void)reassert_remote_tal_skill_view();
            }
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
            last_applied_snapshot.combat_enabled != 0u, TRUE);
    }
    if (!applied[0] || !applied[1]) {
        if (remote_tal_skill_view_lease.owner_view.valid) {
            (void)reassert_remote_tal_skill_view();
        }
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (remote_tal_skill_view_lease.owner_view.valid) {
        BOOL view_reasserted = reassert_remote_tal_skill_view();
        int trace_state = view_reasserted ? 1 : 0;
        if (trace_state != remote_tal_skill_view_trace_state) {
            remote_tal_skill_view_trace_state = trace_state;
            SudekiMpLogFormat(
                "lan_arena_client_replica event=client_remote_skill_view "
                "state=%s owner=Ailish remote_caster=Tal sequence=%u "
                "revision=%lu boundary=post_native_render_pre_world "
                "policy=refresh_live_owner_basis_then_reassert_after_remote_mutation\r\n",
                view_reasserted ? "active" : "rejected",
                (unsigned int)remote_tal_skill_view_lease.skill_sequence,
                (unsigned long)remote_tal_skill_view_lease.owner_view.
                    refresh_revision);
        }
        if (!view_reasserted) {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
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
    const SudekiMpCleanroomActor actors[2] = {
        seat_host_actor(),
        seat_client_actor()
    };
    const uint8_t expected_types[2] = {
        seat_host_type(),
        seat_client_type()
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
    snapshots[0] = &last_applied_snapshot.seat[0];
    snapshots[1] = &last_applied_snapshot.seat[1];
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

BOOL SudekiMpLanArenaClientSpiritVisualFilterGeneration(
    SudekiMpLanArenaSnapshot *snapshot, uint16_t skill_floor
) {
    unsigned int index, kept = 0u;
    if (snapshot == NULL || snapshot->spirit_vfx_observed > 1u ||
        snapshot->spirit_vfx_count > SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY ||
        (!snapshot->spirit_vfx_observed && snapshot->spirit_vfx_count != 0u))
        return FALSE;
    if (!snapshot->spirit_vfx_observed) return TRUE;
    for (index = 0u; index < snapshot->spirit_vfx_count; ++index) {
        uint16_t sequence = snapshot->spirit_vfx[index].skill_sequence;
        if (snapshot->spirit_vfx[index].owner_actor_type != 0u ||
            (sequence != 0u && (skill_floor == 0u ||
                (int16_t)(sequence - skill_floor) > 0))) {
            snapshot->spirit_vfx[kept++] = snapshot->spirit_vfx[index];
        }
    }
    snapshot->spirit_vfx_count = (uint8_t)kept;
    ZeroMemory(&snapshot->spirit_vfx[kept],
        (SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY - kept) *
            sizeof(snapshot->spirit_vfx[0]));
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaServiceSpiritVfx(void) {
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaSnapshot visual_frame;
    const SudekiMpLanArenaActorSnapshot *tal = &last_applied_snapshot.seat[0];
    void *expected_tal;
    void *expected_ailish;
    uint32_t expected_generation;
    uint64_t expected_session;
    BOOL serviced;
    DWORD error;

    if (InterlockedCompareExchange(
            &client_spirit_vfx_call_depth, 0, 0) > 0) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (client_replica_reset_pending || game_base == NULL ||
        !client_session_status(&status)) {
        if (game_base != NULL)
            (void)release_client_spirit_vfx_cache("authority_lost");
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    /* A failed frame/unknown observation is not an authoritative removal.
     * The next successfully published frame will service retained clones. */
    if (!replica_diagnostics.valid) return TRUE;
    expected_tal = client_remote_tal_lease_actor;
    expected_ailish = SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    expected_generation = client_remote_tal_lease_generation;
    expected_session = status.session_token;
    if (!SudekiMpLanArenaClientTalLifecycleLeaseExact(
            SudekiMpCleanroomEngineActorEntity(seat_host_actor()),
            client_remote_tal_lease_generation,
            expected_tal, expected_generation) ||
        last_applied_characters[0] != expected_tal ||
        expected_ailish == NULL || last_applied_characters[1] != expected_ailish ||
        tal->actor_type != seat_host_type() ||
        tal->native_entity_id != seat_host_type()) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (last_applied_snapshot.spirit_vfx_observed) {
        unsigned int index;
        for (index = 0u; index < last_applied_snapshot.spirit_vfx_count; ++index) {
            uint32_t instance = last_applied_snapshot.spirit_vfx[index].instance_sequence;
            if (status_vfx_newest_instance == 0u ||
                (int32_t)(instance - status_vfx_newest_instance) > 0)
                status_vfx_newest_instance = instance;
        }
    }
    if (spirit_vfx_session_token != 0u &&
        (spirit_vfx_session_token != expected_session ||
         spirit_vfx_tal_actor != expected_tal ||
         spirit_vfx_tal_generation != expected_generation ||
         status_vfx_ailish_actor != expected_ailish)) {
        if (!release_client_spirit_vfx_cache("visual_actor_generation_changed"))
            return FALSE;
        status_vfx_instance_floor = status_vfx_newest_instance;
        /* Same-session replacement retires old clones, then fences every
         * effect belonging to the preceding cast. A later authenticated cast
         * can render normally without transferring old instances to Tal. */
        spirit_vfx_generation_fenced = TRUE;
        spirit_vfx_generation_skill_floor = tal->skill_sequence;
    }
    /* These clones have no parent/actor renderer dependency. In particular,
     * Tal's combat readiness must not block an authoritative loop removal. */
    spirit_vfx_session_token = expected_session;
    spirit_vfx_tal_actor = expected_tal;
    spirit_vfx_tal_generation = expected_generation;
    status_vfx_ailish_actor = expected_ailish;
    visual_frame = last_applied_snapshot;
    if (spirit_vfx_generation_fenced &&
        !SudekiMpLanArenaClientSpiritVisualFilterGeneration(
            &visual_frame, spirit_vfx_generation_skill_floor)) return FALSE;
    if (status_vfx_instance_floor != 0u && visual_frame.spirit_vfx_observed) {
        unsigned int index, kept = 0u;
        for (index = 0u; index < visual_frame.spirit_vfx_count; ++index) {
            const SudekiMpLanArenaSpiritVfxSnapshot *value = &visual_frame.spirit_vfx[index];
            if (value->owner_actor_type == 0u ||
                (int32_t)(value->instance_sequence - status_vfx_instance_floor) > 0)
                visual_frame.spirit_vfx[kept++] = *value;
        }
        visual_frame.spirit_vfx_count = (uint8_t)kept;
        ZeroMemory(&visual_frame.spirit_vfx[kept],
            (SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY - kept) * sizeof(visual_frame.spirit_vfx[0]));
    }
    InterlockedIncrement(&client_spirit_vfx_call_depth);
    serviced = SudekiMpLanArenaSpiritVfxServiceVisuals(
        (HMODULE)game_base, &visual_frame, expected_session);
    error = serviced ? ERROR_SUCCESS : GetLastError();
    InterlockedDecrement(&client_spirit_vfx_call_depth);
    if (client_replica_reset_pending || game_base == NULL ||
        !client_session_status(&status) || !replica_diagnostics.valid ||
        status.session_token != expected_session ||
        client_remote_tal_lease_actor != expected_tal ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) != expected_ailish ||
        client_remote_tal_lease_generation != expected_generation) {
        /* A reentrant invalidation cannot retire an in-flight native clone.
         * Complete that obligation now, before admitting another roster. */
        (void)release_client_spirit_vfx_cache("authority_lost_during_visual_service");
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (serviced || error == ERROR_IO_PENDING || error == ERROR_NOT_READY) {
        if (serviced) spirit_vfx_replay_failure_logged = FALSE;
        return TRUE;
    }
    if (!spirit_vfx_replay_failure_logged) {
        spirit_vfx_replay_failure_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_client_replica event=spirit_vfx state=roster_rejected "
            "snapshot_sequence=%lu visual_count=%u win32_error=%lu "
            "policy=retain_exact_native_leases_without_gameplay_rejection\r\n",
            (unsigned long)last_applied_snapshot.sequence,
            (unsigned int)last_applied_snapshot.spirit_vfx_count,
            (unsigned long)error);
    }
    SetLastError(error == ERROR_SUCCESS ? ERROR_INVALID_STATE : error);
    return FALSE;
}

void SudekiMpLanArenaClientReplicaRefreshDiagnostics(void) {
    if (!replica_diagnostics.valid) return;
    capture_actor_diagnostics(
        0u, seat_host_actor(), &last_applied_snapshot.seat[0]);
    capture_actor_diagnostics(
        1u, seat_client_actor(), &last_applied_snapshot.seat[1]);
    capture_camera_diagnostics();
}

static BOOL spirit_view_frame_exact(void) {
    SpiritViewFrameLease *lease = &spirit_view_frame;
    void *mode, *scene;
    unsigned int i;
    if (!lease->view.valid || game_base == NULL ||
        SudekiMpCleanroomEngineActorEntity(seat_client_actor()) != lease->character ||
        !readable_memory(lease->character, 0x138u) ||
        *(void **)((uint8_t *)lease->character + 0x44u) != lease->position ||
        *(void **)((uint8_t *)lease->character + 0x134u) != lease->component ||
        !readable_memory(lease->component, 0x168u) ||
        *(void **)(lease->component + 0x10u) != lease->character ||
        !readable_memory(lease->position, 0xb8u) ||
        *(void **)(lease->position + 0x10u) != lease->character ||
        *(void **)(lease->component + 0x160u) != lease->arms_wrapper ||
        (*(void **)(lease->component + 0x164u) != lease->world_wrapper &&
         *(void **)(lease->position + 0xb4u) != lease->world_wrapper) ||
        !readable_memory(lease->world_wrapper, 0x14u) ||
        !readable_memory(lease->arms_wrapper, 0x14u) ||
        *(void **)(lease->world_wrapper + 8u) != lease->body ||
        *(void **)(lease->arms_wrapper + 8u) != lease->arms ||
        !writable_memory(lease->body + RENDER_OBJECT_FLAGS_OFFSET, 4u) ||
        !writable_memory(lease->arms + RENDER_OBJECT_FLAGS_OFFSET, 4u)) return FALSE;
    if (!lease->preparation_only && (
        *(uint8_t **)(game_base + 0x409d7cu) != lease->camera_manager ||
        !readable_memory(lease->camera_manager, 0x4cu) ||
        *(void **)lease->camera_manager != game_base + 0x2c7b80u ||
        (lease->in_place ?
            (!SudekiMpLanArenaClientPrivateCastCamerasOwned() ||
             lease->borrowed_camera!=lease->view.camera || lease->borrowed_state!=lease->view.render_state ||
             *(void **)(lease->camera_manager+0x20u)!=lease->borrowed_camera):
            (*(uint8_t **)(lease->camera_manager+0x2cu)!=lease->borrowed_camera)) ||
        !readable_memory(lease->borrowed_camera, 0x108u) ||
        *(void **)lease->borrowed_camera != game_base + 0x2cce5cu ||
        (!lease->in_place && memcmp(lease->borrowed_camera + 0x4cu, "SpiritCam", 10u) != 0) ||
        *(uint8_t **)(lease->borrowed_camera + 0x34u) != lease->borrowed_state ||
        !writable_memory(lease->borrowed_state + 0x90u, 76u) ||
        !writable_memory(lease->borrowed_state + 0x2cu, 2u) ||
        !current_client_owner_view(&mode, &scene))) return FALSE;
    if (*(uint8_t **)((uint8_t *)lease->character + CHARACTER_WEAPON_OFFSET) != lease->weapon ||
        !readable_memory(lease->weapon, WEAPON_SECONDARY_WRAPPER_OFFSET + sizeof(void *)) ||
        *(void **)(lease->weapon + WEAPON_OWNER_OFFSET) != lease->character) return FALSE;
    for (i = 0; i < 2u; ++i) {
        unsigned int offset = i ? WEAPON_SECONDARY_WRAPPER_OFFSET : WEAPON_PRIMARY_WRAPPER_OFFSET;
        if (*(uint8_t **)(lease->weapon + offset) != lease->weapon_wrappers[i]) return FALSE;
        if (lease->weapon_wrappers[i] &&
            (!readable_memory(lease->weapon_wrappers[i], 0x0cu) ||
             *(uint8_t **)(lease->weapon_wrappers[i] + WRAPPER_RENDER_OBJECT_OFFSET) != lease->weapon_render[i] ||
             !writable_memory(lease->weapon_render[i] + RENDER_OBJECT_FLAGS_OFFSET, 4u))) return FALSE;
    }
    return lease->preparation_only || SudekiMpLanArenaOwnerViewSwapRenderState(&lease->view, mode, scene,
        lease->swapped ? lease->borrowed_state : lease->view.render_state,
        lease->swapped ? lease->borrowed_state : lease->view.render_state);
}

BOOL SudekiMpLanArenaClientSpiritViewEndFrame(void) {
    SpiritViewFrameLease *lease = &spirit_view_frame;
    uint8_t *state;
    void *mode, *scene;
    unsigned int i;
    if (!lease->view.valid) return TRUE;
    if (!spirit_view_frame_exact()) return FALSE; /* Keep the teardown barrier. */
    if (!lease->preparation_only) {
        if (!current_client_owner_view(&mode, &scene) ||
            !SudekiMpLanArenaOwnerViewSwapRenderState(&lease->view, mode, scene,
                lease->swapped ? lease->borrowed_state : lease->view.render_state,
                lease->view.render_state)) return FALSE;
        lease->swapped = FALSE;
        state = lease->borrowed_state;
        memcpy(state + 0x90u, lease->saved_camera, sizeof(lease->saved_camera));
        ++*(uint16_t *)(state + 0x2cu);
    }
    *(uint32_t *)(lease->body + RENDER_OBJECT_FLAGS_OFFSET) =
        (*(uint32_t *)(lease->body + RENDER_OBJECT_FLAGS_OFFSET) & ~4u) | lease->body_hidden;
    *(uint32_t *)(lease->arms + RENDER_OBJECT_FLAGS_OFFSET) =
        (*(uint32_t *)(lease->arms + RENDER_OBJECT_FLAGS_OFFSET) & ~4u) | lease->arms_hidden;
    for (i = 0; i < 2u; ++i) if (lease->weapon_render[i]) {
        uint32_t *flags = (uint32_t *)(lease->weapon_render[i] + RENDER_OBJECT_FLAGS_OFFSET);
        *flags = (*flags & ~RENDER_OBJECT_HIDDEN_FLAG) | lease->weapon_hidden[i];
    }
    ZeroMemory(lease, sizeof(*lease));
    return TRUE;
}

static BOOL spirit_view_trace(const char *reason, BOOL result) {
    static DWORD last_tick;
    static const char *last_reason;
    DWORD now = GetTickCount();
    if (reason != last_reason || (DWORD)(now - last_tick) >= 1000u) {
        last_reason = reason; last_tick = now;
        SudekiMpLogFormat("lan_arena_client_replica event=spirit_view reason=%s kind=%u sequence=%u hidden=%u\r\n",
            reason, last_applied_snapshot.cast[1].spirit_view.kind,
            last_applied_snapshot.cast[1].spirit_view.skill_sequence,
            last_applied_snapshot.cast[1].spirit_view.body_hidden);
    }
    return result;
}

static BOOL begin_spirit_view_frame(BOOL preparation_only) {
    const SudekiMpLanArenaSpiritView *view = &last_applied_snapshot.cast[1].spirit_view;
    SpiritViewFrameLease pending = {0};
    void *mode, *scene;
    uint8_t *state;
    unsigned int i;
    if (!SudekiMpLanArenaClientSpiritViewEndFrame()) return spirit_view_trace("restore_lease", FALSE);
    if (!client_session_authenticated() || !replica_diagnostics.valid ||
        view->kind == 0u) return TRUE;
    if (!SudekiMpLanArenaSpiritViewValid(view) ||
        seat_client_type() != SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        !last_applied_snapshot.seat[1].skill_active ||
        last_applied_snapshot.seat[1].skill_kind != SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT ||
        view->skill_sequence != last_applied_snapshot.seat[1].skill_sequence ||
        native_skill_leases[1].native_started ||
        (native_skill_leases[0].native_started && !SudekiMpLanArenaClientPrivateCastCamerasOwned()))
        return spirit_view_trace("cast_identity", FALSE);
    pending.character = SudekiMpCleanroomEngineActorEntity(seat_client_actor());
    if (pending.character != last_applied_characters[1] ||
        !readable_memory(pending.character, 0x138u)) return spirit_view_trace("actor", FALSE);
    pending.component = *(uint8_t **)((uint8_t *)pending.character + 0x134u);
    pending.position = *(uint8_t **)((uint8_t *)pending.character + 0x44u);
    if (!readable_memory(pending.component, 0x168u) ||
        !readable_memory(pending.position, 0xb8u)) return spirit_view_trace("component", FALSE);
    pending.world_wrapper = *(uint8_t **)(pending.component + 0x164u);
    if (!pending.world_wrapper) pending.world_wrapper = *(uint8_t **)(pending.position + 0xb4u);
    pending.arms_wrapper = *(uint8_t **)(pending.component + 0x160u);
    if (pending.world_wrapper == pending.arms_wrapper ||
        !readable_memory(pending.world_wrapper, 0x14u) ||
        !readable_memory(pending.arms_wrapper, 0x14u)) return spirit_view_trace("model_wrappers", FALSE);
    pending.body = *(uint8_t **)(pending.world_wrapper + 8u);
    pending.arms = *(uint8_t **)(pending.arms_wrapper + 8u);
    if (!writable_memory(pending.body + RENDER_OBJECT_FLAGS_OFFSET, 4u) ||
        !writable_memory(pending.arms + RENDER_OBJECT_FLAGS_OFFSET, 4u) ||
        !current_client_owner_view(&mode, &scene) ||
        !SudekiMpLanArenaOwnerViewCapture(&pending.view, mode, scene)) return spirit_view_trace("owner_view", FALSE);
    pending.camera_manager = *(uint8_t **)(game_base + 0x409d7cu);
    if (!readable_memory(pending.camera_manager, 0x4cu)) return spirit_view_trace("camera_manager", FALSE);
    /* Keep the native selected-camera/scene relationship intact so scoped
     * peer callbacks can still validate their logical selection. Overlay only
     * the local owner's current-frame matrices and restore before updates.
     * The remote cast's private cameras are never borrowed or overwritten. */
    pending.in_place=SudekiMpLanArenaClientPrivateCastCamerasOwned();
    pending.preparation_only=preparation_only;
    pending.borrowed_camera = pending.in_place ? pending.view.camera:
        *(uint8_t **)(pending.camera_manager + 0x2cu);
    if (!readable_memory(pending.borrowed_camera, 0x108u)) return spirit_view_trace("borrowed_camera", FALSE);
    pending.borrowed_state = *(uint8_t **)(pending.borrowed_camera + 0x34u);
    if (!pending.in_place && pending.borrowed_state == pending.view.render_state)
        return spirit_view_trace("borrowed_alias", FALSE);
    state = pending.borrowed_state;
    if (!writable_memory(state + 0x90u, 76u) || !writable_memory(state + 0x2cu, 2u)) return spirit_view_trace("camera_storage", FALSE);
    memcpy(pending.saved_camera, state + 0x90u, sizeof(pending.saved_camera));
    pending.body_hidden = *(uint32_t *)(pending.body + RENDER_OBJECT_FLAGS_OFFSET) & 4u;
    pending.arms_hidden = *(uint32_t *)(pending.arms + RENDER_OBJECT_FLAGS_OFFSET) & 4u;
    pending.weapon = *(uint8_t **)((uint8_t *)pending.character + CHARACTER_WEAPON_OFFSET);
    if (!readable_memory(pending.weapon, WEAPON_SECONDARY_WRAPPER_OFFSET + sizeof(void *)))
        return spirit_view_trace("weapon", FALSE);
    for (i = 0; i < 2u; ++i) {
        unsigned int offset = i ? WEAPON_SECONDARY_WRAPPER_OFFSET : WEAPON_PRIMARY_WRAPPER_OFFSET;
        pending.weapon_wrappers[i] = *(uint8_t **)(pending.weapon + offset);
        if (!pending.weapon_wrappers[i]) continue;
        if (!readable_memory(pending.weapon_wrappers[i], 0x0cu)) return spirit_view_trace("weapon_wrapper", FALSE);
        pending.weapon_render[i] = *(uint8_t **)(pending.weapon_wrappers[i] + WRAPPER_RENDER_OBJECT_OFFSET);
        if (!writable_memory(pending.weapon_render[i] + RENDER_OBJECT_FLAGS_OFFSET, 4u))
            return spirit_view_trace("weapon_render", FALSE);
        pending.weapon_hidden[i] = *(uint32_t *)(pending.weapon_render[i] + RENDER_OBJECT_FLAGS_OFFSET) & RENDER_OBJECT_HIDDEN_FLAG;
    }
    spirit_view_frame = pending;
    if (!spirit_view_frame_exact()) { ZeroMemory(&spirit_view_frame, sizeof(spirit_view_frame)); return spirit_view_trace("exact_lease", FALSE); }
    if (!preparation_only) {
        memcpy(state + 0x90u, view->matrix, sizeof(view->matrix));
        memcpy(state + 0xd0u, view->projection, sizeof(view->projection));
        ++*(uint16_t *)(state + 0x2cu);
        if (!SudekiMpLanArenaOwnerViewSwapRenderState(&spirit_view_frame.view,
                mode, scene, pending.view.render_state, pending.borrowed_state)) {
            (void)SudekiMpLanArenaClientSpiritViewEndFrame();
            return spirit_view_trace("scene_swap", FALSE);
        }
        spirit_view_frame.swapped = TRUE;
    }
    *(uint32_t *)(pending.body + RENDER_OBJECT_FLAGS_OFFSET) =
        (*(uint32_t *)(pending.body + RENDER_OBJECT_FLAGS_OFFSET) & ~4u) | (view->body_hidden ? 4u : 0u);
    *(uint32_t *)(pending.arms + RENDER_OBJECT_FLAGS_OFFSET) |= 4u;
    for (i = 0; i < 2u; ++i) if (pending.weapon_render[i])
        *(uint32_t *)(pending.weapon_render[i] + RENDER_OBJECT_FLAGS_OFFSET) |= RENDER_OBJECT_HIDDEN_FLAG;
    return preparation_only ? TRUE : spirit_view_trace("applied", TRUE);
}

BOOL SudekiMpLanArenaClientSpiritViewPrepareBody(void) {
    /* A hidden first-person world model is not evaluated by the ordinary
     * native preparation path. Making it visible only at Draw displays a
     * frozen pose even though its replicated animation clock advances.
     * Keep camera matrices untouched here; the primary draw owns those. */
    return begin_spirit_view_frame(TRUE);
}

BOOL SudekiMpLanArenaClientSpiritViewBeginFrame(void) {
    return begin_spirit_view_frame(FALSE);
}

BOOL SudekiMpLanArenaClientReplicaGetSkillFade(SudekiMpLanArenaSkillFade *fade) {
    unsigned int i;
    /* Prefer the local owner's record even when the peer is also casting.
     * A peer-only record asks the renderer to suppress its local native fade;
     * it never supplies RGB for this player's cast. */
    unsigned int owner=last_applied_snapshot.cast[1].skill_fade.kind ? 1u:0u;
    if(!fade || client_replica_reset_pending || !client_session_authenticated() ||
        !replica_diagnostics.valid || !last_applied_snapshot.cast[owner].skill_fade.kind ||
        !SudekiMpLanArenaSnapshotValid(&last_applied_snapshot)) return FALSE;
    for(i=0u;i<2u;++i) {
        uint8_t *actor=SudekiMpCleanroomEngineActorEntity(i==0u ? seat_host_actor() : seat_client_actor());
        if(!actor || actor!=last_applied_characters[i] || !readable_memory(actor,0x48u) ||
            *(void **)(actor+0x44u)!=last_applied_positions[i]) return FALSE;
    }
    *fade=last_applied_snapshot.cast[owner].skill_fade;
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaGetDiagnostics(
    SudekiMpLanArenaReplicaDiagnostics *diagnostics
) {
    if (diagnostics == NULL || !replica_diagnostics.valid) return FALSE;
    *diagnostics = replica_diagnostics;
    return TRUE;
}

/* The fixed-four movement path shares this adapter's exact renderer/position
 * entry owner, not a second set of detours. It deliberately does not invoke
 * the old two-seat skill, weapon, camera or combat ownership paths. */
typedef struct PartyMovementTarget {
    SudekiMpCleanroomActor actor;
    uint8_t *character,*position,*model;
    void *renderer;
    unsigned int submodels;
    SudekiMpLanPartyMotion motion;
    LanArenaAnimationMethods methods;
} PartyMovementTarget;
static BOOL party_combat_action(uint8_t type,uint8_t variant,int *selector,
    int *state,unsigned *channel);
static BOOL party_loaded_selector(PartyMovementTarget *target,int selector) {
    if(!selector) return TRUE;
    /* Same exact native lookup as the accepted ranged path, but keyed by the
     * uniform +130 model owner. This is a transition-time lookup only: no
     * unverified cross-frame cache and no 4x full bank walk on settled poses. */
    for(unsigned id=0;id<0xc4;++id)
        if(resolve_ailish_world_selector(target->model,target->renderer,id,selector)) return TRUE;
    return FALSE;
}
static BOOL party_movement_target(unsigned seat,const SudekiMpLanArenaActorSnapshot *snapshot,
    PartyMovementTarget *t) {
    uint8_t *ranged_component;
    if(!snapshot || snapshot->actor_type!=SudekiMpLanPartyActorType(seat) ||
        !SudekiMpCleanroomActorFromType(snapshot->actor_type,&t->actor) ||
        !SudekiMpLanPartyMotionDescribe(snapshot->actor_type,snapshot->animation_state,&t->motion))
        return FALSE;
    t->character=SudekiMpCleanroomEngineActorEntity(t->actor);
    if(!readable_memory(t->character,0x138u)) return FALSE;
    t->position=*(uint8_t **)(t->character+CHARACTER_POSITION_OFFSET);
    t->model=*(uint8_t **)(t->character+0x130u);
    if(!readable_memory(t->position,0x104) ||
        !readable_memory(t->model,0x168) || *(void **)(t->model+0x10)!=t->character ||
        !actor_presentation_renderer(t->character,t->motion.ranged?1u:0u,&t->renderer,&ranged_component) ||
        !animation_methods(t->renderer,&t->methods)) return FALSE;
    t->submodels=t->methods.count(t->renderer);
    return SudekiMpCleanroomWorldMotionStorageValid(t->actor,t->renderer,t->submodels,TRUE);
}
static BOOL party_combat_target(unsigned seat,const SudekiMpLanArenaActorSnapshot *snapshot,
    PartyMovementTarget *t) {
    uint8_t *component;
    if(!snapshot || snapshot->actor_type!=SudekiMpLanPartyActorType(seat) ||
        !SudekiMpCleanroomActorFromType(snapshot->actor_type,&t->actor) ||
        !SudekiMpLanPartyMotionDescribe(snapshot->actor_type,
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,&t->motion)) return FALSE;
    t->character=SudekiMpCleanroomEngineActorEntity(t->actor);
    if(!readable_memory(t->character,0x138u)) return FALSE;
    t->position=*(uint8_t **)(t->character+CHARACTER_POSITION_OFFSET);
    t->model=*(uint8_t **)(t->character+0x130u);
    if(!readable_memory(t->position,0x104u) ||
        !readable_memory(t->model,0x168u) || *(void **)(t->model+0x10u)!=t->character ||
        !actor_presentation_renderer(t->character,t->motion.ranged?1u:0u,
            &t->renderer,&component) || !animation_methods(t->renderer,&t->methods)) return FALSE;
    t->submodels=t->methods.count(t->renderer);
    return SudekiMpCleanroomWorldMotionStorageValid(t->actor,t->renderer,t->submodels,TRUE);
}
static BOOL party_movement_identity(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyRosterObservation *roster,
    PartyMovementTarget targets[4],BOOL combat) {
    SudekiMpLanPartyRosterObservation current;
    if(!party_replica_session || !SudekiMpLanPartyLeaseActive(party_replica_session,lease) ||
        !SudekiMpLanPartyControlObserveRoster(w,&current) || current.combat!=combat ||
        current.present_mask!=15 || current.group!=roster->group ||
        current.controller!=roster->controller) return FALSE;
    for(unsigned i=0;i<4;++i) {
        SudekiMpLanPartyLease key=*lease; key.seat=(uint8_t)i;
        PartyMovementTarget *t=&targets[i]; uint8_t *component; void *renderer;
        if(current.actors[i]!=roster->actors[i] || current.actors[i]!=t->character ||
            *(void **)(t->character+CHARACTER_POSITION_OFFSET)!=t->position ||
            *(void **)(t->character+0x130)!=t->model ||
            !actor_presentation_renderer(t->character,t->motion.ranged?1u:0u,&renderer,&component) ||
            renderer!=t->renderer ||
            (i!=lease->seat && !SudekiMpLanPartyControlExact(w,&key,t->character))) return FALSE;
    }
    return TRUE;
}
static BOOL party_apply_movement(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame, BOOL final) {
    PartyMovementTarget targets[4]; SudekiMpLanPartyRosterObservation roster;
    BOOL transition[4]; DWORD now=GetTickCount();
    if(!party_replica_session || !game_base || !set_position || !set_forward ||
        !lease || lease->seat!=SudekiMpLanPartyLocalSeat(party_replica_session) ||
        !lease->seat || lease->seat>=4 || !SudekiMpLanPartyLeaseActive(party_replica_session,lease) ||
        !SudekiMpLanPartyMovementFrameValid(frame) ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) || roster.combat || roster.present_mask!=15)
        return FALSE;
    if(lease->token!=party_presentation_generation.token ||
        lease->generation!=party_presentation_generation.generation ||
        lease->seat!=party_presentation_generation.seat) {
        if(!party_retire_ailish_first_person(w)) return FALSE;
        memset(party_presentation_leases,0,sizeof(party_presentation_leases));
        party_presentation_generation=*lease;
    }
    memset(targets,0,sizeof(targets));
    for(unsigned i=0;i<4;++i) {
        const SudekiMpLanArenaActorSnapshot *s=&frame->chunk[i/2].seat[i%2];
        PartyMovementTarget *t=&targets[i]; LanArenaPresentationLease *prior=&party_presentation_leases[i];
        SudekiMpLanPartyLease key=*lease; key.seat=(uint8_t)i;
        if(!party_movement_target(i,s,t) || t->character!=roster.actors[i] ||
            !SudekiMpLanPartyMovementDrained(&key,t->character,w)) return FALSE;
        transition[i]=!prior->valid || prior->character!=t->character ||
            prior->renderer!=t->renderer;
        /* Resolve only on a new owner/clip, but prove this frame's actor and
         * renderer before every write. Native phase changes do not require
         * repeatedly walking the entire animation bank. */
        for(unsigned c=0;c<SudekiMpLanPartyMotionChannels(s->actor_type);++c) {
            int selector=SudekiMpLanPartyMotionSelector(s->actor_type,s->locomotion.clip[c]);
            if((transition[i] || !prior->locomotion.valid ||
                prior->locomotion.clip[c]!=s->locomotion.clip[c]) &&
                !party_loaded_selector(t,selector)) return FALSE;
        }
        /* Ranged actions occupy channel 4, outside the four world-motion
         * channels captured by a movement frame. A combat-to-movement edge
         * must explicitly retire the last locally applied shot, but only
         * while this exact actor/renderer still owns the action channel. */
        if(prior->valid && prior->character==t->character &&
            prior->renderer==t->renderer && prior->action_variant!=
                SUDEKIMP_LAN_ARENA_ACTION_NONE && t->motion.ranged) {
            int action_selector,action_state;
            unsigned action_channel;
            if(!party_combat_action(s->actor_type,prior->action_variant,
                    &action_selector,&action_state,&action_channel) ||
                action_channel!=4u) return FALSE;
            for(unsigned sub=0;sub<t->submodels;++sub) {
                int selector=t->methods.get_selector(t->renderer,4,sub);
                int state=t->methods.get_state(t->renderer,4,sub);
                BOOL owned_action=selector==action_selector &&
                    (state==action_state || state==192);
                BOOL already_idle=selector==0 && state==192;
                if(!owned_action && !already_idle) return FALSE;
            }
        }
    }
    if(!party_movement_identity(w,lease,&roster,targets,FALSE) ||
        (lease->seat==3u && !party_retire_ailish_first_person(w))) return FALSE;
    for(unsigned i=0;i<4;++i) {
        const SudekiMpLanArenaActorSnapshot *s=&frame->chunk[i/2].seat[i%2];
        PartyMovementTarget *t=&targets[i]; LanArenaPresentationLease *prior=&party_presentation_leases[i];
        const SudekiMpLanArenaLocomotion *m=&s->locomotion;
        unsigned count=SudekiMpLanPartyMotionChannels(s->actor_type);
        DWORD elapsed=prior->last_early_apply_at?now-prior->last_early_apply_at:17;
        float coordinates[3]={s->x,s->y,s->z},facing[3]={s->facing_x,0,s->facing_z};
        float x=*(float *)(t->position+0x50),z=*(float *)(t->position+0x58);
        float length=sqrtf(x*x+z*z),dot=length>0.0001f?(x*facing[0]+z*facing[2])/length:-1;
        if(!party_movement_identity(w,lease,&roster,targets,FALSE)) return FALSE;
        if(prior->valid && prior->character==t->character &&
            prior->renderer==t->renderer && prior->action_variant!=
                SUDEKIMP_LAN_ARENA_ACTION_NONE && t->motion.ranged) {
            set_animation_channel(t->renderer,&t->methods,t->submodels,4,
                0,192,0.0f,TRUE);
            t->methods.set_blend(t->renderer,3,0.0f);
        }
        if(SudekiMpLanPartyMotionElcoIdleEnded(s->actor_type,m) &&
            !SudekiMpRestoreElcoInterruptedIdleWeapon(t->character)) return FALSE;
        set_position(t->position,coordinates);
        if(!final && !SudekiMpLanPartyControlPresentationFacing(w,i,t->character,facing))
            return FALSE;
        if(!isfinite(dot) || dot<=0.99996f) call_position_set_forward(t->position,facing);
        if(!final && !SudekiMpCleanroomEngineSetActorResources(t->actor,(float)s->hp,(float)s->sp)) return FALSE;
        for(unsigned c=0;c<count;++c) {
            int selector=SudekiMpLanPartyMotionSelector(s->actor_type,m->clip[c]);
            BOOL restart=transition[i] || !prior->locomotion.valid ||
                prior->locomotion.clip[c]!=m->clip[c] || m->time[c]<prior->locomotion.time[c];
            BOOL fresh=restart || m->time[c]!=prior->locomotion.time[c] ||
                m->rate[c]!=prior->locomotion.rate[c];
            float phase;
            if(!SudekiMpLanArenaClientLocomotionPhase(m->time[c],m->rate[c],elapsed,final,&phase))
                return FALSE;
            for(unsigned sub=0;sub<t->submodels;++sub) {
                BOOL changed=t->methods.get_selector(t->renderer,c,sub)!=selector;
                float rate=m->rate[c]; BOOL seek=FALSE;
                if(changed) t->methods.set_selector(t->renderer,c,sub,selector);
                if(t->methods.get_state(t->renderer,c,sub)!=m->state[c])
                    t->methods.set_state(t->renderer,c,sub,m->state[c]);
                if((!final && fresh) || changed) {
                    if(!SudekiMpLanArenaClientBukiLocomotionClock(
                            t->methods.get_time(t->renderer,c,sub),phase,m->rate[c],
                            restart || changed,&rate,&seek)) return FALSE;
                    if(seek) t->methods.set_time(t->renderer,c,sub,phase,0);
                    if(fabsf(t->methods.get_rate(t->renderer,c,sub)-rate)>0.001f)
                        t->methods.set_rate(t->renderer,c,sub,rate);
                }
            }
        }
        for(unsigned c=0;c<(count==4?3u:1u);++c)
            if(fabsf(t->methods.get_blend(t->renderer,c)-m->blend[c])>0.0001f)
                t->methods.set_blend(t->renderer,c,m->blend[c]);
        prior->character=t->character; prior->renderer=t->renderer;
        prior->animation_state=s->animation_state; prior->valid=TRUE;
        /* A movement frame is host confirmation that basic combat has ended.
         * The actor-specific action channel has now either been replaced by
         * world locomotion (melee) or retired above (ranged). Do not carry a
         * stale action lease into a later disconnect/rejoin drain. */
        prior->action_variant=SUDEKIMP_LAN_ARENA_ACTION_NONE;
        prior->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
        prior->action_sequence=0;
        if(!final) { prior->locomotion=*m; prior->last_early_apply_at=now; }
        /* Same native publication as the accepted two-seat renderer path.
         * Do not leave local CPosition and the visible/root-motion basis one
         * update apart. Only unparented world actors are admitted. */
        uintptr_t parent=*(uintptr_t *)(t->position+0x94);
        uint8_t *wrapper=*(uint8_t **)(t->position+0xb4);
        uint8_t *object=readable_memory(wrapper,0x14)?*(uint8_t **)(wrapper+8):NULL;
        if((parent && parent!=4) || !position_world_matrix ||
            !readable_memory(object,0xd0) || !writable_memory(t->position+0xb8,1) ||
            !writable_memory(object+0x2c,4) || !writable_memory(object+0x90,64) ||
            !party_movement_identity(w,lease,&roster,targets,FALSE)) return FALSE;
        if(!readable_memory(position_world_matrix(t->position),64)) return FALSE;
        if(!actor_visible_transform_matches_position(t->position,object)) {
            t->position[0xb8]=1;
            if(!readable_memory(position_world_matrix(t->position),64) ||
                !actor_visible_transform_matches_position(t->position,object)) return FALSE;
        }
    }
    return party_movement_identity(w,lease,&roster,targets,FALSE);
}

static BOOL party_combat_mode_identity(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease) {
    SudekiMpLanPartyRosterObservation roster;
    if(!party_replica_session || !lease ||
        !SudekiMpLanPartyLeaseActive(party_replica_session,lease) ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) || roster.present_mask!=15)
        return FALSE;
    for(unsigned i=0;i<4;++i) if(i!=lease->seat) {
        SudekiMpLanPartyLease key=*lease; key.seat=(uint8_t)i;
        if(!SudekiMpLanPartyControlExact(w,&key,roster.actors[i])) return FALSE;
    }
    return TRUE;
}
static BOOL party_sync_combat_mode(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,BOOL desired) {
    BOOL current,verified;
    if(!party_combat_mode_identity(w,lease) ||
        !SudekiMpCleanroomEngineCombatMode(&current)) return FALSE;
    if(party_combat_mode_lease_valid &&
        (party_combat_mode_token!=lease->token ||
         party_combat_mode_generation!=lease->generation ||
         lease->seat!=party_presentation_generation.seat) &&
        (!SudekiMpLanPartyClientRestoreCombatMode() ||
         !SudekiMpCleanroomEngineCombatMode(&current))) return FALSE;
    if(!party_combat_mode_lease_valid) {
        party_combat_mode_original=current;
        party_combat_mode_token=lease->token;
        party_combat_mode_generation=lease->generation;
        party_combat_mode_lease_valid=TRUE;
    }
    if(current!=desired && !SudekiMpCleanroomEngineSetCombatMode(desired)) return FALSE;
    if(!party_combat_mode_identity(w,lease) ||
        !SudekiMpCleanroomEngineCombatMode(&verified) || verified!=desired) return FALSE;
    /* Native weapon/UI arming is asynchronous. Preserve the complete host
     * frame queue and begin selector writes only after Sudeki has drained it. */
    return !desired || !SudekiMpCleanroomEngineRangedCombatPrimePending();
}
static BOOL party_combat_action(uint8_t type,uint8_t variant,int *selector,int *state,
    unsigned *channel) {
    if(!selector || !state || !channel) return FALSE;
    if(type==SUDEKIMP_LAN_ARENA_BUKI_TYPE) {
        *channel=0;
        return SudekiMpLanArenaBukiActionToNativePresentation(variant,selector,state);
    }
    if(type==SUDEKIMP_LAN_ARENA_TAL_TYPE) {
        *channel=0;
        return SudekiMpLanArenaTalActionToNativePresentation(variant,selector,state);
    }
    if((type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) &&
        variant==SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE) {
        *channel=4; *selector=SudekiMpLanArenaRangedCombatSelector(type,0x85u);
        *state=1;
        return *selector>=0;
    }
    return FALSE;
}
static BOOL party_combat_identity(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyRosterObservation *roster,
    PartyMovementTarget targets[4]) {
    return party_movement_identity(w,lease,roster,targets,TRUE);
}
typedef struct PartyAilishFirstPersonTarget {
    uint8_t *character;
    uint8_t *component;
    uint8_t *wrapper;
    void *renderer;
    unsigned int submodels;
    int idle_selector;
    int fire_selector;
    int reload_selector;
    int reload_complete_selector;
    LanArenaAnimationMethods methods;
} PartyAilishFirstPersonTarget;
static BOOL party_ailish_first_person_target(uint8_t *character,
    PartyAilishFirstPersonTarget *target) {
    uint8_t *component,*wrapper;
    void *renderer;
    uint32_t handle;
    int selector;
    unsigned int submodels;
    if(!target || !readable_memory(character,
            AILISH_RANGED_COMPONENT_OFFSET+sizeof(void *))) return FALSE;
    component=*(uint8_t **)(character+AILISH_RANGED_COMPONENT_OFFSET);
    if(!readable_memory(component,0x168u) || *(void **)(component+0x10u)!=character)
        return FALSE;
    wrapper=*(uint8_t **)(component+AILISH_FIRST_PERSON_WRAPPER_OFFSET);
    if(!readable_memory(wrapper,0x14u)) return FALSE;
    renderer=*(void **)(wrapper+0x10u);
    if(!animation_methods(renderer,&target->methods)) return FALSE;
    submodels=target->methods.count(renderer);
    if(!submodels || submodels>32u ||
        !resolve_ailish_first_person_selector(component,renderer,0x05u,
            SudekiMpLanArenaRangedCombatSelector(
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,0x05u),&handle,&selector) ||
        !resolve_ailish_first_person_selector(component,renderer,0x8cu,
            SudekiMpLanArenaRangedCombatSelector(
                SUDEKIMP_LAN_ARENA_AILISH_TYPE,0x8cu),&handle,&selector) ||
        !readable_memory(character,
            AILISH_RANGED_COMPONENT_OFFSET+sizeof(void *)) ||
        !readable_memory(component,0x168u) || !readable_memory(wrapper,0x14u) ||
        *(uint8_t **)(character+AILISH_RANGED_COMPONENT_OFFSET)!=component ||
        *(void **)(component+0x10u)!=character ||
        *(uint8_t **)(component+AILISH_FIRST_PERSON_WRAPPER_OFFSET)!=wrapper ||
        *(void **)(wrapper+0x10u)!=renderer)
        return FALSE;
    target->character=character;
    target->component=component;
    target->wrapper=wrapper;
    target->renderer=renderer;
    target->submodels=submodels;
    target->idle_selector=SudekiMpLanArenaRangedCombatSelector(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,0x05u);
    target->fire_selector=SudekiMpLanArenaRangedCombatSelector(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,0x8cu);
    /* C2/C3 are first-person-only electric reload phases. Their world-bank
     * counterparts are intentionally not used by this renderer lease. */
    target->reload_selector=9;
    target->reload_complete_selector=10;
    return target->idle_selector>=0 && target->fire_selector>=0;
}
static BOOL party_ailish_first_person_same_target(
    const PartyAilishFirstPersonTarget *expected) {
    PartyAilishFirstPersonTarget current;
    return expected && party_ailish_first_person_target(
        expected->character,&current) && current.component==expected->component &&
        current.wrapper==expected->wrapper && current.renderer==expected->renderer &&
        current.submodels==expected->submodels;
}
static BOOL party_ailish_first_person_reload_selectors(
    const PartyAilishFirstPersonTarget *target) {
    uint32_t handle;
    int selector;
    return target &&
        resolve_ailish_first_person_selector(target->component,target->renderer,
            0xc2u,9,&handle,&selector) &&
        resolve_ailish_first_person_selector(target->component,target->renderer,
            0xc3u,10,&handle,&selector);
}
static BOOL party_apply_ailish_first_person(
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,
    const SudekiMpLanPartyRosterObservation *roster,
    PartyMovementTarget targets[4],
    const SudekiMpLanArenaActorSnapshot *snapshot,
    const SudekiMpLanPartyAilishWeaponState *weapon,
    BOOL final) {
    PartyAilishFirstPersonTarget target;
    PartyAilishFirstPersonLease *prior=&party_ailish_first_person_lease;
    BOOL same_owner,weak_attack,swap_transition,swap_finished=FALSE;
    BOOL reload_active=FALSE,reload_finished=FALSE,reload_started=FALSE;
    BOOL reload_complete=FALSE;
    BOOL transition;
    int selector,state;
    if(!lease || lease->seat!=3u || !snapshot || snapshot->actor_type!=
            SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
        (snapshot->action_variant!=SUDEKIMP_LAN_ARENA_ACTION_NONE &&
         snapshot->action_variant!=SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE) ||
        targets[3].character!=roster->actors[3] ||
        !party_ailish_first_person_target(targets[3].character,&target) ||
        !party_combat_identity(w,lease,roster,targets)) return FALSE;
    same_owner=prior->valid && prior->character==target.character &&
        prior->component==target.component && prior->wrapper==target.wrapper &&
        prior->renderer==target.renderer && prior->token==lease->token &&
        prior->generation==lease->generation && prior->seat==lease->seat;
    /* Publish the cosmetic renderer lease before a setter can re-enter a
     * lifecycle callback. No arbiter input or native ranged task is retained. */
    prior->character=target.character;
    prior->component=target.component;
    prior->wrapper=target.wrapper;
    prior->renderer=target.renderer;
    prior->token=lease->token;
    prior->generation=lease->generation;
    prior->seat=lease->seat;
    prior->valid=TRUE;
    weak_attack=snapshot->action_variant==SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    if(!same_owner) {
        prior->reload_sequence=0;
        prior->reload_active=FALSE;
        prior->reload_complete=FALSE;
    }
    if(weapon && weapon->valid) {
        reload_active=weapon->reload_ms>0u;
        reload_started=reload_active && (!prior->reload_active ||
            prior->reload_sequence!=weapon->reload_sequence);
        reload_finished=prior->reload_active && !reload_active &&
            prior->reload_sequence==weapon->reload_sequence;
        if(prior->reload_sequence!=weapon->reload_sequence) {
            prior->reload_complete=FALSE;
            prior->reload_active=FALSE;
        }
        if(reload_active) prior->reload_active=TRUE;
        if(reload_finished) {
            prior->reload_active=FALSE;
            prior->reload_complete=TRUE;
        }
        prior->reload_sequence=weapon->reload_sequence;
    } else if(same_owner) {
        /* A transient missing sidecar must not cut an already observed reload. */
        reload_active=prior->reload_active;
        reload_complete=prior->reload_complete;
    }
    reload_complete=reload_complete || reload_finished || prior->reload_complete;
    if((reload_active || reload_finished) &&
        !party_ailish_first_person_reload_selectors(&target)) return FALSE;
    swap_transition=SudekiMpLanArenaClientShouldStartWeaponSwap(
        same_owner,client_ailish_first_person_camera_owns_facing(target.character),
        weak_attack,prior->weapon_slot,snapshot->weapon_slot_plus_one);
    if(swap_transition || (same_owner && prior->weapon_swap && !weak_attack)) {
        uint32_t swap_handle;
        int swap_selector;
        BOOL complete=!swap_transition;
        if(!resolve_ailish_first_person_selector(target.component,target.renderer,
                0xc1u,8,&swap_handle,&swap_selector) ||
            !ailish_swap_resource_matches(target.renderer)) return FALSE;
        if(swap_transition) {
            if(!party_combat_identity(w,lease,roster,targets) ||
                !party_ailish_first_person_same_target(&target)) return FALSE;
            set_animation_channel(target.renderer,&target.methods,target.submodels,
                0,swap_selector,1,24.0f,TRUE);
            prior->weapon_slot=snapshot->weapon_slot_plus_one;
            prior->weapon_swap=TRUE;
            return party_combat_identity(w,lease,roster,targets) &&
                party_ailish_first_person_same_target(&target);
        }
        for(unsigned sub=0;sub<target.submodels;++sub) {
            float time=target.methods.get_time(target.renderer,0,sub);
            if(!isfinite(time) || time<0.0f) return FALSE;
            if(target.methods.get_selector(target.renderer,0,sub)!=swap_selector) {
                complete=TRUE;
                break;
            }
            if(!SudekiMpLanArenaClientWeaponSwapComplete(time)) complete=FALSE;
        }
        if(!complete) {
            prior->weapon_slot=snapshot->weapon_slot_plus_one;
            return party_combat_identity(w,lease,roster,targets) &&
                party_ailish_first_person_same_target(&target);
        }
        swap_finished=TRUE;
    }
    transition=!same_owner || prior->action_variant!=snapshot->action_variant ||
        prior->action_sequence!=snapshot->action_sequence || swap_finished ||
        prior->weapon_swap;
    if(!weak_attack && !swap_transition && !prior->weapon_swap &&
        (reload_active || reload_complete || (weapon && weapon->valid))) {
        int current=target.methods.get_selector(target.renderer,0,0);
        BOOL keep_completed=current==target.reload_complete_selector &&
            prior->reload_complete;
        int wanted=reload_active?target.reload_selector:
            ((reload_finished || keep_completed)?target.reload_complete_selector:
                target.idle_selector);
        transition=current!=wanted || reload_started;
    }
    if(!transition) {
        prior->weapon_slot=snapshot->weapon_slot_plus_one;
        if(!weak_attack) return TRUE;
        return party_combat_identity(w,lease,roster,targets) &&
            party_ailish_first_person_same_target(&target) &&
            synchronize_action_phase(
                target.renderer,&target.methods,target.submodels,0,snapshot) &&
            party_combat_identity(w,lease,roster,targets) &&
            party_ailish_first_person_same_target(&target);
    }
    selector=weak_attack?target.fire_selector:
        (reload_active?target.reload_selector:
        ((reload_finished || (reload_complete &&
            target.methods.get_selector(target.renderer,0,0)==
                target.reload_complete_selector)) ?
            target.reload_complete_selector:target.idle_selector));
    state=weak_attack || reload_active || reload_finished?1:128;
    if(!party_combat_identity(w,lease,roster,targets) ||
        !party_ailish_first_person_same_target(&target)) return FALSE;
    set_animation_channel(target.renderer,&target.methods,target.submodels,
        0,selector,state,24.0f,weak_attack || swap_finished ||
            reload_started || reload_finished);
    if(weak_attack && !synchronize_action_phase(
            target.renderer,&target.methods,target.submodels,0,snapshot)) return FALSE;
    if(!animation_channel_matches(target.renderer,&target.methods,
            target.submodels,0,selector,24.0f) ||
        !party_combat_identity(w,lease,roster,targets) ||
        !party_ailish_first_person_same_target(&target)) return FALSE;
    prior->action_sequence=snapshot->action_sequence;
    prior->action_variant=snapshot->action_variant;
    prior->weapon_slot=snapshot->weapon_slot_plus_one;
    prior->weapon_swap=FALSE;
    (void)final;
    return TRUE;
}
static BOOL party_retire_ailish_first_person(
    const SudekiMpControlUpdateDispatchWitness *w) {
    PartyAilishFirstPersonLease *prior=&party_ailish_first_person_lease;
    SudekiMpCleanroomActor actor;
    PartyAilishFirstPersonTarget target;
    BOOL owned=TRUE,already_idle=TRUE;
    if(!prior->valid) return TRUE;
    if(w && !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w))
        return FALSE;
    if(!SudekiMpCleanroomActorFromType(
            SUDEKIMP_LAN_ARENA_AILISH_TYPE,&actor)) return FALSE;
    if(SudekiMpCleanroomEngineActorEntity(actor)!=prior->character) {
        ZeroMemory(prior,sizeof(*prior));
        return TRUE;
    }
    if(!party_ailish_first_person_target(prior->character,&target) ||
        target.component!=prior->component || target.wrapper!=prior->wrapper ||
        target.renderer!=prior->renderer) return FALSE;
    for(unsigned sub=0;sub<target.submodels;++sub) {
        int current=target.methods.get_selector(target.renderer,0,sub);
        int current_state=target.methods.get_state(target.renderer,0,sub);
        BOOL known=(current==target.idle_selector && current_state==128) ||
            (current==target.fire_selector && current_state==1) ||
            (current==8 && current_state==1);
        if(!known) owned=FALSE;
        if(current!=target.idle_selector || current_state!=128) already_idle=FALSE;
    }
    /* A later native owner may have replaced the cosmetic channel. Preserve
     * its state and release only our bookkeeping in that case. */
    if(!owned || already_idle) {
        ZeroMemory(prior,sizeof(*prior));
        return TRUE;
    }
    if((w && !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) ||
        !party_ailish_first_person_same_target(&target)) return FALSE;
    set_animation_channel(target.renderer,&target.methods,target.submodels,
        0,target.idle_selector,128,24.0f,TRUE);
    if(!animation_channel_matches(target.renderer,&target.methods,
            target.submodels,0,target.idle_selector,24.0f) ||
        !party_ailish_first_person_same_target(&target)) return FALSE;
    ZeroMemory(prior,sizeof(*prior));
    return TRUE;
}
static BOOL party_apply_combat_weapon(PartyMovementTarget *target,
    const SudekiMpLanArenaActorSnapshot *snapshot,LanArenaPresentationLease *prior) {
    SudekiMpWeaponQuickList weapons;
    SudekiMpCharacterSkillState skill;
    unsigned slot;
    if(!target || !snapshot || !prior || !snapshot->weapon_slot_plus_one) return FALSE;
    slot=(unsigned)snapshot->weapon_slot_plus_one-1u;
    if(slot>=12u || !SudekiMpDescribeCharacterWeapons(target->character,&weapons) ||
        slot>=weapons.row_count) return FALSE;
    if(weapons.rows[slot].equipped) {
        prior->weapon_attempt_actor=NULL; prior->weapon_attempt_slot=0;
        return TRUE;
    }
    if(prior->weapon_attempt_actor==target->character &&
        prior->weapon_attempt_slot==snapshot->weapon_slot_plus_one) return TRUE;
    if(SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpObserveCharacterSkill(target->character,&skill) || skill.active)
        return FALSE;
    {
        SudekiMpWeaponActivationResult result=
            SudekiMpActivateCharacterWeapon(target->character,slot);
        if(result.status!=SUDEKIMP_WEAPON_ACTIVATION_STARTED &&
            result.status!=SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED) return FALSE;
        prior->weapon_attempt_actor=target->character;
        prior->weapon_attempt_slot=snapshot->weapon_slot_plus_one;
    }
    return TRUE;
}
static BOOL party_apply_combat(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame,BOOL final) {
    PartyMovementTarget targets[4]; SudekiMpLanPartyRosterObservation roster;
    int action_selector[4]={0},action_state[4]={0}; unsigned action_channel[4]={0};
    BOOL transition[4],action_restart[4]; DWORD now=GetTickCount();
    if(!party_replica_session || !game_base || !set_position || !set_forward ||
        !lease || lease->seat!=SudekiMpLanPartyLocalSeat(party_replica_session) ||
        !lease->seat || lease->seat>=4 || !SudekiMpLanPartyLeaseActive(party_replica_session,lease) ||
        !SudekiMpLanPartyBasicCombatFrameValid(frame) ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) || !roster.combat ||
        roster.present_mask!=15) return FALSE;
    if(lease->token!=party_presentation_generation.token ||
        lease->generation!=party_presentation_generation.generation ||
        lease->seat!=party_presentation_generation.seat) {
        if(!party_retire_ailish_first_person(w)) return FALSE;
        memset(party_presentation_leases,0,sizeof(party_presentation_leases));
        party_presentation_generation=*lease;
    }
    memset(targets,0,sizeof(targets));
    for(unsigned i=0;i<4;++i) {
        const SudekiMpLanArenaActorSnapshot *s=&frame->chunk[i/2].seat[i%2];
        PartyMovementTarget *t=&targets[i]; LanArenaPresentationLease *prior=&party_presentation_leases[i];
        if(!party_combat_target(i,s,t) || t->character!=roster.actors[i] ||
            !SudekiMpLanPartyMovementDrained(&(SudekiMpLanPartyLease){lease->token,
                lease->generation,(uint8_t)i},t->character,w)) return FALSE;
        transition[i]=!prior->valid || prior->character!=t->character || prior->renderer!=t->renderer;
        action_restart[i]=transition[i] || prior->action_restart_pending ||
            prior->action_sequence!=s->action_sequence ||
            prior->action_variant!=s->action_variant;
        if(s->action_variant && !party_combat_action(s->actor_type,s->action_variant,
                &action_selector[i],&action_state[i],&action_channel[i])) return FALSE;
        for(unsigned c=0;c<4;++c) {
            int selector=SudekiMpLanPartyCombatMotionSelector(s->actor_type,s->locomotion.clip[c]);
            if(selector<0 || ((transition[i] || !prior->locomotion.valid ||
                prior->locomotion.clip[c]!=s->locomotion.clip[c]) &&
                !party_loaded_selector(t,selector))) return FALSE;
        }
        if(s->action_variant && (transition[i] ||
            prior->action_sequence!=s->action_sequence ||
            prior->action_variant!=s->action_variant) &&
            !party_loaded_selector(t,action_selector[i])) return FALSE;
    }
    if(!party_combat_identity(w,lease,&roster,targets)) return FALSE;
    for(unsigned i=0;i<4;++i) {
        const SudekiMpLanArenaActorSnapshot *s=&frame->chunk[i/2].seat[i%2];
        PartyMovementTarget *t=&targets[i]; LanArenaPresentationLease *prior=&party_presentation_leases[i];
        const SudekiMpLanArenaLocomotion *m=&s->locomotion;
        unsigned count=4; DWORD elapsed=prior->last_early_apply_at?now-prior->last_early_apply_at:17;
        float coordinates[3]={s->x,s->y,s->z},facing[3]={s->facing_x,0,s->facing_z};
        for(unsigned c=0;c<count;++c) {
            int selector=SudekiMpLanPartyCombatMotionSelector(s->actor_type,m->clip[c]);
            BOOL restart=transition[i] || !prior->locomotion.valid ||
                prior->locomotion.clip[c]!=m->clip[c] || m->time[c]<prior->locomotion.time[c];
            BOOL fresh=restart || m->time[c]!=prior->locomotion.time[c] ||
                m->rate[c]!=prior->locomotion.rate[c];
            float phase,rate=m->rate[c]; BOOL seek=FALSE;
            if(!party_combat_identity(w,lease,&roster,targets)) return FALSE;
            if(c==0 && (s->actor_type==SUDEKIMP_LAN_ARENA_BUKI_TYPE ||
                s->actor_type==SUDEKIMP_LAN_ARENA_TAL_TYPE) && s->action_variant) continue;
            if(!SudekiMpLanArenaClientLocomotionPhase(m->time[c],m->rate[c],elapsed,final,&phase))
                return FALSE;
            for(unsigned sub=0;sub<t->submodels;++sub) {
                BOOL changed=t->methods.get_selector(t->renderer,(int)c,sub)!=selector;
                if(changed) t->methods.set_selector(t->renderer,(int)c,sub,selector);
                if(t->methods.get_state(t->renderer,(int)c,sub)!=m->state[c])
                    t->methods.set_state(t->renderer,(int)c,sub,m->state[c]);
                if((!final && fresh) || changed) {
                    if(!SudekiMpLanArenaClientBukiLocomotionClock(
                            t->methods.get_time(t->renderer,(int)c,sub),phase,m->rate[c],
                            restart||changed,&rate,&seek)) return FALSE;
                    if(seek) t->methods.set_time(t->renderer,(int)c,sub,phase,0);
                    if(fabsf(t->methods.get_rate(t->renderer,(int)c,sub)-rate)>0.001f)
                        t->methods.set_rate(t->renderer,(int)c,sub,rate);
                }
            }
        }
        if(!party_combat_identity(w,lease,&roster,targets)) return FALSE;
        if(!party_apply_combat_weapon(t,s,prior) ||
            !party_combat_identity(w,lease,&roster,targets)) return FALSE;
        /* Elco's first-person window owns the native local controller and
         * reload display. Reconcile only that exact local actor through the
         * existing controller-checked weapon adapter; other seats remain
         * renderer-only observers of this host state. */
        if(!final && i==1u && lease->seat==1u &&
            (!s->weapon.valid || !SudekiMpSyncElcoPresentationResources(
                t->character,s->weapon.item,s->weapon.charge_q8,
                s->weapon.reload_ms))) return FALSE;
        set_position(t->position,coordinates);
        if(!final && !SudekiMpLanPartyControlPresentationFacing(w,i,t->character,facing))
            return FALSE;
        if(!final && !SudekiMpCleanroomEngineSetActorResources(t->actor,(float)s->hp,(float)s->sp))
            return FALSE;
        /* Record the presentation lifetime before entering an action selector.
         * A disconnect/reentrant lifecycle callback must see the lease even
         * if a native renderer setter fails halfway through this frame. */
        if(s->action_variant) {
            if(!party_combat_identity(w,lease,&roster,targets)) return FALSE;
            prior->action_restart_pending=action_restart[i];
            prior->character=t->character; prior->renderer=t->renderer;
            prior->action_variant=s->action_variant;
            prior->action_sequence=s->action_sequence;
            prior->combat_state=s->combat_state; prior->valid=TRUE;
        }
        if(t->motion.ranged) {
            BOOL firing=s->action_variant!=SUDEKIMP_LAN_ARENA_ACTION_NONE;
            set_animation_channel(t->renderer,&t->methods,t->submodels,4,
                firing?action_selector[i]:0,firing?action_state[i]:192,
                firing?24.0f:0.0f,action_restart[i]);
            if(firing && !synchronize_action_phase(t->renderer,&t->methods,
                    t->submodels,4,s)) return FALSE;
            t->methods.set_blend(t->renderer,3,firing?1.0f:0.0f);
        } else if(s->action_variant) {
            float phase;
            if(!SudekiMpLanArenaClientActionPhaseTime(s,&phase)) return FALSE;
            set_animation_channel(t->renderer,&t->methods,t->submodels,
                (int)action_channel[i],action_selector[i],action_state[i],24.0f,
                action_restart[i]);
            if(!synchronize_action_phase(t->renderer,&t->methods,t->submodels,
                    (int)action_channel[i],s)) return FALSE;
        }
        if(i==3u && lease->seat==3u &&
            !party_apply_ailish_first_person(
                w,lease,&roster,targets,s,&frame->ailish_weapon,final)) return FALSE;
        prior->action_restart_pending=FALSE;
        for(unsigned c=0;c<3;++c) t->methods.set_blend(t->renderer,(int)c,m->blend[c]);
        prior->character=t->character; prior->renderer=t->renderer;
        prior->animation_state=s->animation_state; prior->combat_state=s->combat_state;
        prior->action_variant=s->action_variant; prior->action_sequence=s->action_sequence;
        prior->valid=TRUE;
        if(!final) { prior->locomotion=*m; prior->last_early_apply_at=now; }
        {
            uintptr_t parent=*(uintptr_t *)(t->position+0x94u);
            uint8_t *wrapper=*(uint8_t **)(t->position+0xb4u);
            uint8_t *object=readable_memory(wrapper,0x14u)?*(uint8_t **)(wrapper+8u):NULL;
            if((parent && parent!=4u) || !position_world_matrix ||
                !readable_memory(object,0xd0u) || !writable_memory(t->position+0xb8u,1) ||
                !writable_memory(object+0x2cu,4) || !writable_memory(object+0x90u,64) ||
                !party_combat_identity(w,lease,&roster,targets) ||
                !readable_memory(position_world_matrix(t->position),64)) return FALSE;
            if(!actor_visible_transform_matches_position(t->position,object)) {
                t->position[0xb8u]=1;
                if(!readable_memory(position_world_matrix(t->position),64) ||
                    !actor_visible_transform_matches_position(t->position,object)) return FALSE;
            }
        }
    }
    if(!party_combat_identity(w,lease,&roster,targets) ||
        !apply_training_dummy_feedback(&frame->chunk[0],lease->token) ||
        !apply_training_dummy(&frame->chunk[0].enemies[0])) return FALSE;
    return party_combat_identity(w,lease,&roster,targets);
}

BOOL SudekiMpLanPartyClientActionDrain(
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor) {
    SudekiMpLanArenaActorSnapshot snapshot;
    PartyMovementTarget target,current;
    LanArenaPresentationLease *prior;
    SudekiMpLanPartyRosterObservation roster;
    int action_selector,action_state,selector,state,idle_selector,idle_state;
    unsigned action_channel,seat;
    BOOL ranged,already_idle=TRUE;
    float idle_rate;
    if(!w || !key || !actor || !party_replica_session ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) || key->seat>=4u ||
        key->seat==SudekiMpLanPartyLocalSeat(party_replica_session) ||
        SudekiMpLanPartyControlObserveActor(w,key->seat)!=actor) return FALSE;
    seat=key->seat; prior=&party_presentation_leases[seat];
    if(prior->action_variant==SUDEKIMP_LAN_ARENA_ACTION_NONE) return TRUE;
    if(!SudekiMpLanPartyControlRetains(w,key,actor) ||
        !prior->valid || prior->character!=actor || !prior->renderer ||
        key->token!=party_presentation_generation.token ||
        key->generation!=party_presentation_generation.generation ||
        party_presentation_generation.seat!=SudekiMpLanPartyLocalSeat(party_replica_session))
        return FALSE;
    ZeroMemory(&snapshot,sizeof(snapshot));
    snapshot.actor_type=SudekiMpLanPartyActorType(seat);
    snapshot.animation_state=SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    if(!party_combat_target(seat,&snapshot,&target) || target.character!=actor ||
        target.renderer!=prior->renderer ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) ||
        roster.present_mask!=15u || roster.actors[seat]!=actor ||
        !party_combat_action(snapshot.actor_type,prior->action_variant,
            &action_selector,&action_state,&action_channel)) return FALSE;
    ranged=target.motion.ranged;
    idle_selector=ranged?0:SudekiMpLanPartyCombatMotionSelector(snapshot.actor_type,1u);
    idle_state=ranged?192:128; idle_rate=ranged?0.0f:12.0f;
    if(idle_selector<0) return FALSE;
    for(unsigned sub=0;sub<target.submodels;++sub) {
        selector=target.methods.get_selector(target.renderer,(int)action_channel,sub);
        state=target.methods.get_state(target.renderer,(int)action_channel,sub);
        if(!SudekiMpLanPartyActionTerminalObserved(snapshot.actor_type,
                prior->action_variant,selector,(uint8_t)state)) return FALSE;
        if(selector!=idle_selector || state!=idle_state) already_idle=FALSE;
    }
    if(!already_idle) {
        if(!SudekiMpLanPartyControlRetains(w,key,actor) ||
            SudekiMpLanPartyControlObserveActor(w,seat)!=actor ||
            !party_combat_target(seat,&snapshot,&current) ||
            current.character!=actor || current.renderer!=target.renderer ||
            current.submodels!=target.submodels) return FALSE;
        set_animation_channel(target.renderer,&target.methods,target.submodels,
            (int)action_channel,idle_selector,idle_state,idle_rate,TRUE);
    }
    if(!SudekiMpLanPartyControlRetains(w,key,actor) ||
        SudekiMpLanPartyControlObserveActor(w,seat)!=actor ||
        !party_combat_target(seat,&snapshot,&current) || current.character!=actor ||
        current.renderer!=target.renderer || current.submodels!=target.submodels)
        return FALSE;
    for(unsigned sub=0;sub<current.submodels;++sub)
        if(current.methods.get_selector(current.renderer,(int)action_channel,sub)!=idle_selector ||
            current.methods.get_state(current.renderer,(int)action_channel,sub)!=idle_state ||
            fabsf(current.methods.get_rate(current.renderer,(int)action_channel,sub)-idle_rate)>0.001f)
            return FALSE;
    prior->action_variant=SUDEKIMP_LAN_ARENA_ACTION_NONE;
    prior->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    return TRUE;
}

BOOL SudekiMpLanPartyClientRestoreCombatMode(void) {
    BOOL current,verified;
    if(!party_combat_mode_lease_valid) return TRUE;
    if(!SudekiMpCleanroomEngineCombatMode(&current) ||
        (current!=party_combat_mode_original &&
         !SudekiMpCleanroomEngineSetCombatMode(party_combat_mode_original)) ||
        !SudekiMpCleanroomEngineCombatMode(&verified) || verified!=party_combat_mode_original) {
        if(GetLastError()==ERROR_SUCCESS) SetLastError(ERROR_BUSY);
        return FALSE;
    }
    party_combat_mode_lease_valid=FALSE; party_combat_mode_token=0;
    party_combat_mode_generation=0;
    return TRUE;
}

BOOL SudekiMpLanPartyClientApplyMovement(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame) {
    party_present_valid=FALSE;
    if(!w || !SudekiMpLanPartyMovementFrameValid(frame) ||
        !party_sync_combat_mode(w,lease,FALSE) ||
        !party_apply_movement(w,lease,frame,FALSE)) return FALSE;
    party_present_frame=*frame; party_present_lease=*lease;
    party_present_at=GetTickCount(); party_present_valid=TRUE;
    return TRUE;
}
BOOL SudekiMpLanPartyClientApplyBasicCombat(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame) {
    party_present_valid=FALSE;
    if(!w || !SudekiMpLanPartyBasicCombatFrameValid(frame) ||
        !party_sync_combat_mode(w,lease,TRUE) ||
        !party_apply_combat(w,lease,frame,FALSE)) return FALSE;
    party_present_frame=*frame; party_present_lease=*lease;
    party_present_at=GetTickCount(); party_present_valid=TRUE;
    return TRUE;
}
BOOL SudekiMpLanPartyClientEndPresentation(
    const SudekiMpControlUpdateDispatchWitness *w) {
    if(!w || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w))
        return FALSE;
    party_present_valid=FALSE;
    return party_retire_ailish_first_person(w);
}
void SudekiMpLanPartyClientPresent(unsigned phase) {
    if(phase>2 || !party_present_valid || !SudekiMpLanPartyPresentationBoundary() ||
        (DWORD)(GetTickCount()-party_present_at)>250) return;
    SudekiMpLanAimActors(
        SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_ELCO),NULL);
    if(party_present_frame.chunk[0].combat_enabled) {
        if(!party_apply_combat(NULL,&party_present_lease,&party_present_frame,TRUE))
            party_present_valid=FALSE;
    } else if(!party_apply_movement(NULL,&party_present_lease,&party_present_frame,TRUE))
        party_present_valid=FALSE;
}

BOOL SudekiMpLanPartyClientRangedAim(void *actor,float direction[3],
    float target[3],BOOL *target_valid,BOOL *firing) {
    const SudekiMpLanArenaActorSnapshot *sample;
    if(target_valid) *target_valid=FALSE;
    if(firing) *firing=FALSE;
    if(!actor || !direction || !target || !target_valid || !firing ||
        !party_replica_session || !party_present_valid ||
        !SudekiMpLanPartyPresentationBoundary() ||
        (DWORD)(GetTickCount()-party_present_at)>250u ||
        party_present_lease.seat!=SudekiMpLanPartyLocalSeat(party_replica_session) ||
        !SudekiMpLanPartyLeaseActive(party_replica_session,&party_present_lease) ||
        !party_present_frame.chunk[0].combat_enabled ||
        actor!=SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_ELCO) ||
        SudekiMpLanPartyControlObserveActor(NULL,1u)!=actor) return FALSE;
    sample=&party_present_frame.chunk[0].seat[1];
    if(sample->actor_type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE || !sample->hp ||
        !sample->ranged_aim_valid || sample->skill_active) return FALSE;
    for(unsigned i=0;i<3;++i) {
        direction[i]=sample->ranged_aim[i]==INT16_MIN ? -1.0f :
            (float)sample->ranged_aim[i]/32767.0f;
        target[i]=sample->ranged_target[i];
        if(!isfinite(target[i])) return FALSE;
    }
    *target_valid=sample->ranged_target_valid!=0u;
    *firing=sample->action_variant!=SUDEKIMP_LAN_ARENA_ACTION_NONE;
    return TRUE;
}
