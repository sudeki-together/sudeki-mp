#include "hooks/lan_arena_client_replica.h"

#include "cleanroom/engine.h"
#include "network/lan_arena_replica.h"
#include "network/lan_arena_session.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef void (__attribute__((fastcall)) *PositionSetterFunction)(
    void *position,
    const float *coordinates
);
typedef unsigned int (__attribute__((thiscall)) *AnimationCountFunction)(void *renderer);
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
    BOOL valid;
} LanArenaPresentationLease;

enum {
    RVA_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_POSITION_SET_FORWARD = 0x001114d0u,
    RVA_ANIMATION_RENDERER_VTABLE = 0x002df8ecu,
    RVA_ANIMATION_RENDERER_COUNT = 0x0021bb10u,
    RVA_ANIMATION_RENDERER_SELECTOR_SET = 0x00223000u,
    RVA_ANIMATION_RENDERER_SELECTOR_GET = 0x002230b0u,
    RVA_ANIMATION_RENDERER_RATE_SET = 0x002230d0u,
    RVA_ANIMATION_RENDERER_RATE_GET = 0x00223160u,
    RVA_ANIMATION_RENDERER_TIME_SET = 0x00223180u,
    RVA_ANIMATION_RENDERER_STATE_SET = 0x00223240u,
    RVA_ANIMATION_RENDERER_STATE_GET = 0x00223290u,
    RVA_ANIMATION_RENDERER_BLEND_SET = 0x002234c0u,
    RVA_ANIMATION_RENDERER_BLEND_GET = 0x002234e0u,
    CHARACTER_POSITION_OFFSET = 0x44u,
    CHARACTER_PRESENTATION_COMPONENT_OFFSET = 0x134u,
    AILISH_WORLD_WRAPPER_OFFSET = 0x164u,
    POSITION_ATTACHED_WRAPPER_OFFSET = 0xb4u,
    /* Exact cleanroom-world presentation captured from the supported retail
     * image.  These are renderer selectors, not protocol values: the LAN
     * snapshot remains semantic (idle/moving/action) across the wire. */
    TAL_WORLD_IDLE_SELECTOR = 4,
    TAL_WORLD_MOVE_PRIMARY_SELECTOR = 8,
    TAL_WORLD_MOVE_SECONDARY_SELECTOR = 9,
    AILISH_WORLD_IDLE_SELECTOR = 1,
    AILISH_WORLD_MOVE_PRIMARY_SELECTOR = 7,
    AILISH_WORLD_MOVE_SECONDARY_SELECTOR = 8,
    /* Native Tal-P1/Ailish-P2 capture: ANIMID_MISSILE_COMBO3 (0x87). */
    AILISH_WORLD_WEAK_SELECTOR = 55
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

static PositionSetterFunction set_position;
static void *set_forward;
static uint8_t *game_base;
static SudekiMpLanArenaReplica replica;
static DWORD latest_snapshot_received_at;
static LanArenaPresentationLease presentation_leases[2];

enum { REPLICA_INTERPOLATION_DELAY_MS = 50u };

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
    return *(void **)(vtable + 0xf8u) == base + RVA_ANIMATION_RENDERER_COUNT &&
        *(void **)(vtable + 0xfcu) == base + RVA_ANIMATION_RENDERER_SELECTOR_SET &&
        *(void **)(vtable + 0x100u) == base + RVA_ANIMATION_RENDERER_SELECTOR_GET &&
        *(void **)(vtable + 0x104u) == base + RVA_ANIMATION_RENDERER_RATE_SET &&
        *(void **)(vtable + 0x108u) == base + RVA_ANIMATION_RENDERER_RATE_GET &&
        *(void **)(vtable + 0x10cu) == base + RVA_ANIMATION_RENDERER_TIME_SET &&
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
        (void *)methods->set_state == game_base + RVA_ANIMATION_RENDERER_STATE_SET &&
        (void *)methods->get_state == game_base + RVA_ANIMATION_RENDERER_STATE_GET &&
        (void *)methods->set_blend == game_base + RVA_ANIMATION_RENDERER_BLEND_SET &&
        (void *)methods->get_blend == game_base + RVA_ANIMATION_RENDERER_BLEND_GET;
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
        methods->set_selector(renderer, channel, submodel, selector);
        methods->set_state(renderer, channel, submodel, state);
        if (reset_time) {
            methods->set_time(renderer, channel, submodel, 0.0f, 0);
        }
        methods->set_rate(renderer, channel, submodel, rate);
    }
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
    BOOL moving,
    BOOL weak_attack
) {
    int selector_zero = actor_index == 0u ?
        (moving ? TAL_WORLD_MOVE_PRIMARY_SELECTOR : TAL_WORLD_IDLE_SELECTOR) :
        (moving ? AILISH_WORLD_MOVE_PRIMARY_SELECTOR : AILISH_WORLD_IDLE_SELECTOR);
    int selector_one = moving ?
        (actor_index == 0u ? TAL_WORLD_MOVE_SECONDARY_SELECTOR :
            AILISH_WORLD_MOVE_SECONDARY_SELECTOR) : 0;
    float rate_zero = actor_index == 0u ?
        (moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f) :
        (moving ? AILISH_WORLD_MOVE_PRIMARY_RATE : 12.0f);
    float rate_one = moving ?
        (actor_index == 0u ? TAL_WORLD_MOVE_SECONDARY_RATE :
            AILISH_WORLD_MOVE_SECONDARY_RATE) : 0.0f;
    float blend_zero = methods->get_blend(renderer, 0);
    if (!animation_channel_matches(renderer, methods, submodels, 0,
            selector_zero, rate_zero) ||
        !animation_channel_matches(renderer, methods, submodels, 1,
            selector_one, rate_one) ||
        !isfinite(blend_zero) ||
        fabsf(blend_zero - (moving ? 0.99f : 0.0f)) > 0.001f) {
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
            actor_index == 1u && weak_attack ? AILISH_WORLD_WEAK_SELECTOR : 0,
            actor_index == 1u && weak_attack ? 24.0f : 0.0f) &&
        isfinite(methods->get_blend(renderer, 1)) &&
        isfinite(methods->get_blend(renderer, 2)) &&
        isfinite(methods->get_blend(renderer, 3)) &&
        fabsf(methods->get_blend(renderer, 1)) <= 0.001f &&
        fabsf(methods->get_blend(renderer, 2)) <= 0.001f &&
        fabsf(methods->get_blend(renderer, 3) -
            (actor_index == 1u && weak_attack ? 1.0f : 0.0f)) <= 0.001f;
}

static BOOL apply_actor_presentation(
    uint8_t *character,
    const SudekiMpLanArenaActorSnapshot *snapshot,
    unsigned int actor_index
) {
    uint8_t *position;
    uint8_t *component;
    uint8_t *wrapper;
    void *renderer;
    LanArenaAnimationMethods methods;
    LanArenaPresentationLease *lease;
    unsigned int submodels;
    BOOL moving;
    int selector_zero;
    int selector_one;
    int state_one;
    float rate_zero;
    float rate_one;
    float expected_blend_zero;
    float expected_blend_three;
    int action_selector;
    int action_state;
    float action_rate;
    BOOL weak_attack;
    BOOL logical_transition;
    if (character == NULL || snapshot == NULL || actor_index >= 2u ||
        snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_INCAPACITATED) {
        return FALSE;
    }
    position = readable_memory(character, CHARACTER_POSITION_OFFSET + sizeof(void *)) ?
        *(uint8_t **)(character + CHARACTER_POSITION_OFFSET) : NULL;
    component = readable_memory(
        character, CHARACTER_PRESENTATION_COMPONENT_OFFSET + sizeof(void *)) ?
        *(uint8_t **)(character + CHARACTER_PRESENTATION_COMPONENT_OFFSET) : NULL;
    if (actor_index == 1u && readable_memory(
            component, AILISH_WORLD_WRAPPER_OFFSET + sizeof(void *))) {
        wrapper = *(uint8_t **)(component + AILISH_WORLD_WRAPPER_OFFSET);
    } else {
        wrapper = readable_memory(position,
            POSITION_ATTACHED_WRAPPER_OFFSET + sizeof(void *)) ?
            *(uint8_t **)(position + POSITION_ATTACHED_WRAPPER_OFFSET) : NULL;
    }
    renderer = readable_memory(wrapper, 0x14u) ? *(void **)(wrapper + 0x10u) : NULL;
    if (!animation_methods(renderer, &methods)) return FALSE;
    submodels = methods.count(renderer);
    if (submodels == 0u || submodels > 32u) return FALSE;
    lease = &presentation_leases[actor_index];
    moving = snapshot->animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING;
    weak_attack = actor_index == 1u &&
        snapshot->combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    logical_transition = !lease->valid || lease->character != character ||
        lease->renderer != renderer ||
        lease->animation_state != snapshot->animation_state ||
        lease->combat_state != snapshot->combat_state;
    if (!logical_transition && actor_presentation_matches(
            renderer, &methods, submodels, actor_index, moving, weak_attack)) {
        return TRUE;
    }
    if (actor_index == 0u) {
        selector_zero = moving ?
            TAL_WORLD_MOVE_PRIMARY_SELECTOR : TAL_WORLD_IDLE_SELECTOR;
        selector_one = moving ? TAL_WORLD_MOVE_SECONDARY_SELECTOR : 0;
        state_one = moving ? 0 : 192;
        rate_zero = moving ? TAL_WORLD_MOVE_PRIMARY_RATE : 12.0f;
        rate_one = moving ? TAL_WORLD_MOVE_SECONDARY_RATE : 0.0f;
    } else {
        selector_zero = moving ?
            AILISH_WORLD_MOVE_PRIMARY_SELECTOR : AILISH_WORLD_IDLE_SELECTOR;
        selector_one = moving ? AILISH_WORLD_MOVE_SECONDARY_SELECTOR : 0;
        state_one = moving ? 0 : 192;
        rate_zero = moving ? AILISH_WORLD_MOVE_PRIMARY_RATE : 12.0f;
        rate_one = moving ? AILISH_WORLD_MOVE_SECONDARY_RATE : 0.0f;
    }
    set_animation_channel(renderer, &methods, submodels, 0,
        selector_zero, 0, rate_zero, logical_transition);
    set_animation_channel(renderer, &methods, submodels, 1,
        selector_one, state_one, rate_one, logical_transition);
    expected_blend_zero = moving ? 0.99f : 0.0f;
    expected_blend_three = 0.0f;
    action_selector = 0;
    action_state = 192;
    action_rate = 0.0f;
    methods.set_blend(renderer, 0, expected_blend_zero);
    if (actor_index == 0u) {
        /* We do not yet have an exact Tal third-person weak-attack selector.
         * Keep the client replica on its clean base pose instead of guessing
         * or retaining a native action layer from before the AI lease. */
        methods.set_blend(renderer, 3, 0.0f);
    } else {
        set_animation_channel(renderer, &methods, submodels, 2,
            0, 192, 0.0f, logical_transition);
        set_animation_channel(renderer, &methods, submodels, 3,
            0, 192, 0.0f, logical_transition);
        methods.set_blend(renderer, 1, 0.0f);
        methods.set_blend(renderer, 2, 0.0f);
        if (weak_attack) {
            action_selector = AILISH_WORLD_WEAK_SELECTOR;
            action_state = 1;
            action_rate = 24.0f;
            expected_blend_three = 1.0f;
        }
        set_animation_channel(renderer, &methods, submodels, 4,
            action_selector, action_state, action_rate, logical_transition);
        methods.set_blend(renderer, 3, expected_blend_three);
    }
    if (!actor_presentation_matches(
            renderer, &methods, submodels, actor_index, moving, weak_attack)) {
        return FALSE;
    }
    lease->character = character;
    lease->renderer = renderer;
    lease->animation_state = snapshot->animation_state;
    lease->combat_state = snapshot->combat_state;
    lease->valid = TRUE;
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
    uint8_t expected_type
) {
    uint8_t *character;
    void *position;
    float coordinates[3];
    float facing[3];
    BOOL resources_applied;
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
    set_position(position, coordinates);
    call_position_set_forward(position, facing);
    resources_applied = SudekiMpCleanroomEngineSetActorResources(
        actor, (float)snapshot->hp, (float)snapshot->sp);
    if (resources_applied) {
        (void)apply_actor_presentation(
            character, snapshot,
            expected_type == SUDEKIMP_LAN_ARENA_TAL_TYPE ? 0u : 1u);
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

BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    if (base == NULL || set_position != NULL || set_forward != NULL ||
        memcmp(base + RVA_INTERNAL_POSITION_SETTER,
            expected_position_setter_prefix,
            sizeof(expected_position_setter_prefix)) != 0 ||
        memcmp(base + RVA_POSITION_SET_FORWARD,
            expected_position_set_forward_entry,
            sizeof(expected_position_set_forward_entry)) != 0 ||
        !animation_renderer_signatures_match(base)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    set_position = (PositionSetterFunction)(base + RVA_INTERNAL_POSITION_SETTER);
    set_forward = base + RVA_POSITION_SET_FORWARD;
    game_base = base;
    SudekiMpLanArenaReplicaReset(&replica);
    latest_snapshot_received_at = 0u;
    memset(presentation_leases, 0, sizeof(presentation_leases));
    return TRUE;
}

void SudekiMpResetLanArenaClientReplica(void) {
    SudekiMpLanArenaReplicaReset(&replica);
    set_position = NULL;
    set_forward = NULL;
    game_base = NULL;
    latest_snapshot_received_at = 0u;
    memset(presentation_leases, 0, sizeof(presentation_leases));
}

void SudekiMpLanArenaClientReplicaDiscardSnapshots(void) {
    SudekiMpLanArenaReplicaReset(&replica);
    latest_snapshot_received_at = 0u;
    memset(presentation_leases, 0, sizeof(presentation_leases));
}

BOOL SudekiMpLanArenaClientReplicaApplyLatest(void) {
    SudekiMpLanArenaSnapshot received;
    SudekiMpLanArenaSnapshot snapshot;
    DWORD now = GetTickCount();
    uint32_t render_host_tick;
    if (set_position == NULL || set_forward == NULL) {
        return FALSE;
    }
    if (SudekiMpLanArenaSessionTakeRemoteSnapshot(&received)) {
        if (!SudekiMpLanArenaReplicaPush(&replica, &received)) return FALSE;
        latest_snapshot_received_at = now;
    }
    if (!replica.latest_valid || latest_snapshot_received_at == 0u) return FALSE;
    render_host_tick = replica.latest.host_tick +
        (uint32_t)(now - latest_snapshot_received_at);
    if (render_host_tick > REPLICA_INTERPOLATION_DELAY_MS) {
        render_host_tick -= REPLICA_INTERPOLATION_DELAY_MS;
    } else {
        render_host_tick = 0u;
    }
    if (!SudekiMpLanArenaReplicaSample(
            &replica, render_host_tick, &snapshot) ||
        snapshot.match_state != 1u) {
        return FALSE;
    }
    if (!apply_actor(
            &snapshot.ailish, SUDEKIMP_CLEANROOM_AILISH,
            SUDEKIMP_LAN_ARENA_AILISH_TYPE) ||
        !apply_actor(
            &snapshot.tal, SUDEKIMP_CLEANROOM_TAL,
            SUDEKIMP_LAN_ARENA_TAL_TYPE)) return FALSE;
    if (snapshot.enemy_count == 0u) return TRUE;
    return snapshot.enemy_count == 1u &&
        apply_training_dummy(&snapshot.enemies[0]);
}
