#include "hooks/talos_defense_trace.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#else
#error "Talos defense tracing requires 32-bit GCC thiscall support"
#endif

typedef void (SUDEKIMP_THISCALL *ApplyDamageFunction)(
    void *combat,
    void *damage_structure
);
typedef void (__stdcall *CollisionDamageFunction)(
    void *collision_damage,
    void *argument_2,
    void *argument_3,
    void *argument_4,
    void *target,
    void *argument_6,
    void *argument_7
);

enum {
    RVA_APPLY_DAMAGE = 0x000d21d0u,
    RVA_COLLISION_DAMAGE = 0x00138870u
};

static const uint8_t apply_damage_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const uint8_t collision_damage_entry[] = {
    0x83u, 0xecu, 0x78u, 0x53u, 0x55u
};

typedef struct TalosDefenseSnapshot {
    void *character;
    void *combat;
    void *combat_data;
    void *arbiter;
    float hp;
    float max_hp;
    uint32_t arbiter_flags_50;
    int8_t invulnerability_ref_54;
    uint32_t arbiter_state_58;
    uint32_t arbiter_flags_60;
    uint32_t presentation_ids_5c;
    uint16_t configured_knockbacks_60;
    float configured_session_seconds_64;
    float session_timer_68;
    uint16_t session_count_70;
    uint8_t combat_flags_72;
    BOOL valid;
} TalosDefenseSnapshot;

static SudekiMpInlineHook apply_damage_hook;
static SudekiMpInlineHook collision_damage_hook;
static ApplyDamageFunction original_apply_damage;
static CollisionDamageFunction original_collision_damage;
static volatile LONG accepted_damage_sequence;
static volatile LONG collision_sequence;

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

static float decode_damage_half(uint16_t encoded) {
    uint32_t exponent = (uint32_t)((encoded >> 10u) & 0x1fu);
    uint32_t bits = (((uint32_t)encoded & 0x8000u) << 16u) |
        (((exponent == 0u ? 0u : exponent + 0x70u) & 0xffu) << 23u) |
        (((uint32_t)encoded & 0x03ffu) << 13u);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void *real_talos_entity(void) {
    return SudekiMpCleanroomEngineGenericEntity("BOSS_Talos");
}

static BOOL is_real_talos(void *character) {
    void *talos;

    if (character == NULL) {
        return FALSE;
    }
    talos = real_talos_entity();
    return talos != NULL && talos == character;
}

static BOOL capture_snapshot(
    void *combat_pointer,
    void *character_pointer,
    TalosDefenseSnapshot *snapshot
) {
    uint8_t *combat = (uint8_t *)combat_pointer;
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *combat_data;
    uint8_t *arbiter;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->combat = combat;
    snapshot->character = character;
    if (!readable_memory(character, 0x98u) ||
        !readable_memory(combat, 0x73u) ||
        *(void **)(combat + 0x10u) != character) {
        return FALSE;
    }
    combat_data = *(uint8_t **)(character + 0x4cu);
    arbiter = *(uint8_t **)(character + 0x90u);
    if (!readable_memory(combat_data, 0x34u) ||
        !readable_memory(arbiter, 0x64u)) {
        return FALSE;
    }
    snapshot->combat_data = combat_data;
    snapshot->arbiter = arbiter;
    snapshot->hp = *(float *)(combat_data + 0x2cu);
    snapshot->max_hp = *(float *)(combat_data + 0x30u);
    snapshot->arbiter_flags_50 = *(uint32_t *)(arbiter + 0x50u);
    snapshot->invulnerability_ref_54 = *(int8_t *)(arbiter + 0x54u);
    snapshot->arbiter_state_58 = *(uint32_t *)(arbiter + 0x58u);
    snapshot->arbiter_flags_60 = *(uint32_t *)(arbiter + 0x60u);
    snapshot->presentation_ids_5c = *(uint32_t *)(combat + 0x5cu);
    snapshot->configured_knockbacks_60 = *(uint16_t *)(combat + 0x60u);
    snapshot->configured_session_seconds_64 = *(float *)(combat + 0x64u);
    snapshot->session_timer_68 = *(float *)(combat + 0x68u);
    snapshot->session_count_70 = *(uint16_t *)(combat + 0x70u);
    snapshot->combat_flags_72 = *(uint8_t *)(combat + 0x72u);
    snapshot->valid = TRUE;
    return TRUE;
}

static unsigned int presentation_id(uint32_t packed, unsigned int index) {
    return (unsigned int)((packed >> (index * 9u)) & 0x1ffu);
}

static void log_snapshot(
    const char *event,
    const char *phase,
    LONG sequence,
    const TalosDefenseSnapshot *snapshot
) {
    if (snapshot == NULL || !snapshot->valid) {
        SudekiMpLogFormat(
            "talos_defense event=%s phase=%s sequence=%ld snapshot=unavailable\r\n",
            event, phase, (long)sequence);
        return;
    }
    SudekiMpLogFormat(
        "talos_defense event=%s phase=%s sequence=%ld character=0x%08lx combat=0x%08lx hp=%.6f max_hp=%.6f invuln_ref=%d invuln_flag=%u arbiter_flags_50=0x%08lx arbiter_state_58=0x%08lx arbiter_flags_60=0x%08lx reaction_raw=0x%08lx reaction_id0=0x%03x reaction_id1=0x%03x reaction_id2=0x%03x knockback_limit=%u session_limit=%.6f session_timer=%.6f session_count=%u combat_flags=0x%02x qualifying_active=%u threshold_tripped=%u\r\n",
        event,
        phase,
        (long)sequence,
        (unsigned long)(uintptr_t)snapshot->character,
        (unsigned long)(uintptr_t)snapshot->combat,
        (double)snapshot->hp,
        (double)snapshot->max_hp,
        (int)snapshot->invulnerability_ref_54,
        (snapshot->arbiter_flags_50 & 0x00000800u) != 0u ? 1u : 0u,
        (unsigned long)snapshot->arbiter_flags_50,
        (unsigned long)snapshot->arbiter_state_58,
        (unsigned long)snapshot->arbiter_flags_60,
        (unsigned long)snapshot->presentation_ids_5c,
        presentation_id(snapshot->presentation_ids_5c, 0u),
        presentation_id(snapshot->presentation_ids_5c, 1u),
        presentation_id(snapshot->presentation_ids_5c, 2u),
        (unsigned int)snapshot->configured_knockbacks_60,
        (double)snapshot->configured_session_seconds_64,
        (double)snapshot->session_timer_68,
        (unsigned int)snapshot->session_count_70,
        (unsigned int)snapshot->combat_flags_72,
        (snapshot->combat_flags_72 & 0x08u) != 0u ? 1u : 0u,
        (snapshot->combat_flags_72 & 0x01u) != 0u ? 1u : 0u
    );
}

static void SUDEKIMP_THISCALL trace_apply_damage(
    void *combat,
    void *damage_structure
) {
    uint8_t *combat_bytes = (uint8_t *)combat;
    uint8_t *packet = (uint8_t *)damage_structure;
    void *character = NULL;
    TalosDefenseSnapshot before;
    TalosDefenseSnapshot after;
    LONG sequence;
    uint16_t encoded_damage = 0u;
    float raw_damage = 0.0f;
    void *source = NULL;

    if (readable_memory(combat_bytes, 0x14u)) {
        character = *(void **)(combat_bytes + 0x10u);
    }
    if (!is_real_talos(character)) {
        original_apply_damage(combat, damage_structure);
        return;
    }
    sequence = InterlockedIncrement(&accepted_damage_sequence);
    capture_snapshot(combat, character, &before);
    if (readable_memory(packet, 0x68u)) {
        encoded_damage = *(uint16_t *)(packet + 0x14u);
        raw_damage = decode_damage_half(encoded_damage);
        source = *(void **)(packet + 0x30u);
    }
    SudekiMpLogFormat(
        "talos_defense event=accepted_damage phase=packet sequence=%ld packet=0x%08lx source=0x%08lx raw_damage_half=0x%04x raw_damage=%.6f field_60=0x%02x field_61=0x%02x field_62=0x%02x field_63=0x%02x field_64=0x%02x field_67=0x%02x\r\n",
        (long)sequence,
        (unsigned long)(uintptr_t)damage_structure,
        (unsigned long)(uintptr_t)source,
        (unsigned int)encoded_damage,
        (double)raw_damage,
        readable_memory(packet, 0x68u) ? packet[0x60u] : 0u,
        readable_memory(packet, 0x68u) ? packet[0x61u] : 0u,
        readable_memory(packet, 0x68u) ? packet[0x62u] : 0u,
        readable_memory(packet, 0x68u) ? packet[0x63u] : 0u,
        readable_memory(packet, 0x68u) ? packet[0x64u] : 0u,
        readable_memory(packet, 0x68u) ? packet[0x67u] : 0u
    );
    log_snapshot("accepted_damage", "before", sequence, &before);
    original_apply_damage(combat, damage_structure);
    capture_snapshot(combat, character, &after);
    log_snapshot("accepted_damage", "after", sequence, &after);
    SudekiMpLogFormat(
        "talos_defense event=accepted_damage phase=classification sequence=%ld result=%s hp_delta=%.6f session_count_delta=%d threshold_edge=%u\r\n",
        (long)sequence,
        before.valid && after.valid && after.hp < before.hp ?
            "hp_reduced" : "no_hp_reduction",
        before.valid && after.valid ? (double)(after.hp - before.hp) : 0.0,
        before.valid && after.valid ?
            (int)after.session_count_70 - (int)before.session_count_70 : 0,
        before.valid && after.valid &&
            (before.combat_flags_72 & 0x01u) == 0u &&
            (after.combat_flags_72 & 0x01u) != 0u ? 1u : 0u
    );
}

static void __stdcall trace_collision_damage(
    void *collision_damage,
    void *argument_2,
    void *argument_3,
    void *argument_4,
    void *target,
    void *argument_6,
    void *argument_7
) {
    uint8_t *collision = (uint8_t *)collision_damage;
    LONG sequence;
    LONG accepted_before;
    LONG accepted_after;
    int32_t configured_damage = 0;
    float multiple_hit_delay = 0.0f;
    float multiple_hit_timer = 0.0f;

    if (!is_real_talos(target)) {
        original_collision_damage(collision_damage, argument_2, argument_3,
            argument_4, target, argument_6, argument_7);
        return;
    }
    sequence = InterlockedIncrement(&collision_sequence);
    accepted_before = InterlockedCompareExchange(
        &accepted_damage_sequence, 0, 0);
    if (readable_memory(collision, 0x58u)) {
        configured_damage = *(int32_t *)(collision + 0x48u);
        multiple_hit_delay = *(float *)(collision + 0x50u);
        multiple_hit_timer = *(float *)(collision + 0x54u);
    }
    SudekiMpLogFormat(
        "talos_defense event=collision_attempt phase=before sequence=%ld target=0x%08lx collision=0x%08lx configured_damage=%ld multiple_hit_delay=%.6f multiple_hit_timer=%.6f accepted_sequence=%ld\r\n",
        (long)sequence,
        (unsigned long)(uintptr_t)target,
        (unsigned long)(uintptr_t)collision_damage,
        (long)configured_damage,
        (double)multiple_hit_delay,
        (double)multiple_hit_timer,
        (long)accepted_before
    );
    original_collision_damage(collision_damage, argument_2, argument_3,
        argument_4, target, argument_6, argument_7);
    accepted_after = InterlockedCompareExchange(
        &accepted_damage_sequence, 0, 0);
    SudekiMpLogFormat(
        "talos_defense event=collision_attempt phase=after sequence=%ld result=%s accepted_sequence_before=%ld accepted_sequence_after=%ld rejection_hint=%s\r\n",
        (long)sequence,
        accepted_after != accepted_before ? "delivered_to_combat" : "not_delivered",
        (long)accepted_before,
        (long)accepted_after,
        accepted_after != accepted_before ? "none" :
            (multiple_hit_timer != 0.0f ? "multiple_hit_timer_active" :
                "dispatcher_or_target_filter")
    );
}

BOOL SudekiMpInstallTalosDefenseTrace(HMODULE game_module) {
    uint8_t *base;

    if (game_module == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    accepted_damage_sequence = 0;
    collision_sequence = 0;
    if (!SudekiMpInstallInlineHook(
            &apply_damage_hook,
            base + RVA_APPLY_DAMAGE,
            apply_damage_entry,
            sizeof(apply_damage_entry),
            trace_apply_damage)) {
        return FALSE;
    }
    original_apply_damage = (ApplyDamageFunction)apply_damage_hook.trampoline;
    if (!SudekiMpInstallInlineHook(
            &collision_damage_hook,
            base + RVA_COLLISION_DAMAGE,
            collision_damage_entry,
            sizeof(collision_damage_entry),
            trace_collision_damage)) {
        SudekiMpRestoreInlineHook(&apply_damage_hook);
        original_apply_damage = NULL;
        return FALSE;
    }
    original_collision_damage =
        (CollisionDamageFunction)collision_damage_hook.trampoline;
    SudekiMpLogWrite(
        "talos_defense_trace_install=success policy=observation_only exact_real_boss_resource\r\n");
    return TRUE;
}

void SudekiMpUninstallTalosDefenseTrace(void) {
    SudekiMpRestoreInlineHook(&collision_damage_hook);
    SudekiMpRestoreInlineHook(&apply_damage_hook);
    original_collision_damage = NULL;
    original_apply_damage = NULL;
    accepted_damage_sequence = 0;
    collision_sequence = 0;
}
