#include "hooks/talos_coop_balance.h"

#include "cleanroom/engine.h"
#include "engine/log.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    BOSS_BAR_SINGLETON_RVA = 0x003c2fa0u,
    BOSS_BAR_VTABLE_RVA = 0x002cb49cu,
    BOSS_BAR_UPPER_LIMIT_OFFSET = 0x94u,
    BOSS_BAR_LOWER_LIMIT_OFFSET = 0x98u,
    BOSS_BAR_ENTITY_OFFSET = 0x9cu,
    STAT_DISPLAY_VTABLE_RVA = 0x002d21e4u,
    STAT_DISPLAY_HEALTH_UPDATE_RVA = 0x00129780u,
    CHARACTER_STAT_DISPLAY_OFFSET = 0xb0u,
    STAT_DISPLAY_HEALTH_BAR_OFFSET = 0xd0u,
    STAT_DISPLAY_LAST_HP_OFFSET = 0x16cu,
    TALOS_COMBAT_OFFSET = 0x0f64u,
    COMBAT_OWNER_OFFSET = 0x10u,
    COMBAT_KNOCKBACK_LIMIT_OFFSET = 0x60u,
    COMBAT_SESSION_SECONDS_OFFSET = 0x64u,
    CHARACTER_COMBAT_DATA_OFFSET = 0x4cu,
    COMBAT_DATA_HP_OFFSET = 0x2cu,
    COMBAT_DATA_MAX_HP_OFFSET = 0x30u
};

static const float vanilla_talos_max_hp = 45000.0f;
static BOOL balance_enabled;
static BOOL balance_coop_profile;
static unsigned int balance_health_scale = 2u;
static unsigned int balance_stagger_limit = 10u;
static unsigned int balance_stagger_window_seconds = 10u;
static unsigned int balance_generation = 1u;
static unsigned int applied_generation;
static void *applied_character;
static DWORD next_probe_tick;
static BOOL boss_bar_state_logged;
static void *last_logged_boss_bar;
static void *last_logged_boss_bar_entity;
static int last_logged_boss_bar_ratio_bucket = -1;
static void *last_refreshed_stat_display;
static float last_refreshed_stat_hp = -1.0f;
static float last_refreshed_stat_max_hp = -1.0f;
static void *last_logged_stat_refresh_display;
static int last_logged_stat_refresh_bucket = -1;

static const uint8_t stat_display_health_update_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x51u, 0xd9u,
    0x45u, 0x08u, 0x57u, 0xd9u, 0x96u, 0x6cu, 0x01u, 0x00u,
    0x00u
};

static BOOL memory_accessible(
    const void *pointer,
    size_t size,
    BOOL require_write
);

/* FUN_00529780 is Sudeki's CStatDisplay health callback. It receives the
 * display in ESI and current/maximum HP as two callee-cleaned stack floats.
 * Use the shipped callback so its embedded bar renderer and dependent
 * presentation state stay coherent instead of reproducing UI internals. */
__attribute__((naked, noinline, used))
static void native_stat_display_health_update_bridge(
    void *stat_display,
    float current_hp,
    float maximum_hp,
    void *target
) {
    (void)stat_display;
    (void)current_hp;
    (void)maximum_hp;
    (void)target;
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %esi\n\t"
        "pushl 16(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "movl 8(%ebp), %esi\n\t"
        "call *20(%ebp)\n\t"
        "popl %esi\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

static BOOL refresh_talos_stat_display(
    uint8_t *game_base,
    uint8_t *stat_display,
    float current_hp,
    float maximum_hp
) {
    uint8_t *target;
    int ratio_bucket;

    target = game_base + STAT_DISPLAY_HEALTH_UPDATE_RVA;
    if (!memory_accessible(
            stat_display,
            STAT_DISPLAY_LAST_HP_OFFSET + sizeof(float),
            FALSE) ||
        *(void **)stat_display != game_base + STAT_DISPLAY_VTABLE_RVA ||
        !memory_accessible(
            target,
            sizeof(stat_display_health_update_entry),
            FALSE) ||
        memcmp(
            target,
            stat_display_health_update_entry,
            sizeof(stat_display_health_update_entry)) != 0) {
        return FALSE;
    }
    if (last_refreshed_stat_display == stat_display &&
        fabsf(last_refreshed_stat_hp - current_hp) <= 0.5f &&
        fabsf(last_refreshed_stat_max_hp - maximum_hp) <= 0.5f) {
        return TRUE;
    }
    native_stat_display_health_update_bridge(
        stat_display,
        current_hp,
        maximum_hp,
        target);
    last_refreshed_stat_display = stat_display;
    last_refreshed_stat_hp = current_hp;
    last_refreshed_stat_max_hp = maximum_hp;
    ratio_bucket = maximum_hp > 0.0f ?
        (int)floorf((current_hp / maximum_hp) * 20.0f) : -1;
    if (last_logged_stat_refresh_display != stat_display ||
        last_logged_stat_refresh_bucket != ratio_bucket) {
        SudekiMpLogFormat(
            "talos_coop_balance event=stat_display_refresh status=complete "
            "stat_display=%p hp=%.3f max_hp=%.3f "
            "callback_rva=0x%08lx "
            "policy=native_health_callback_on_change\r\n",
            stat_display,
            (double)current_hp,
            (double)maximum_hp,
            (unsigned long)STAT_DISPLAY_HEALTH_UPDATE_RVA);
        last_logged_stat_refresh_display = stat_display;
        last_logged_stat_refresh_bucket = ratio_bucket;
    }
    return TRUE;
}

static void synchronize_talos_boss_bar(uint8_t *character) {
    uint8_t *game_base;
    uint8_t *boss_bar;
    uint8_t *boss_bar_renderer;
    uint8_t *combat_data;
    uint8_t *stat_display;
    uint8_t *stat_health_bar;
    float *fill_cache;
    float *stat_fill_cache;
    float current_hp;
    float maximum_hp;
    float raw_ratio;
    float mapped_ratio;
    float fill_zero;
    float fill_one;
    float stat_fill_zero;
    float stat_fill_one;
    float stat_last_hp;
    float upper_limit;
    float lower_limit;
    int ratio_bucket;
    int fill_count;
    int stat_fill_count;
    void *bound_entity;

    game_base = (uint8_t *)GetModuleHandleW(NULL);
    if (game_base == NULL ||
        !memory_accessible(
            game_base + BOSS_BAR_SINGLETON_RVA,
            sizeof(boss_bar),
            FALSE)) {
        return;
    }
    boss_bar = *(uint8_t **)(game_base + BOSS_BAR_SINGLETON_RVA);
    if (!memory_accessible(
            boss_bar,
            BOSS_BAR_ENTITY_OFFSET + sizeof(void *),
            FALSE)) {
        return;
    }
    bound_entity = *(void **)(boss_bar + BOSS_BAR_ENTITY_OFFSET);
    if (*(void **)boss_bar != game_base + BOSS_BAR_VTABLE_RVA) {
        if (!boss_bar_state_logged || last_logged_boss_bar != boss_bar) {
            SudekiMpLogFormat(
                "talos_coop_balance event=boss_bar_state "
                "status=rejected reason=vtable_mismatch boss_bar=%p "
                "vtable=%p expected=%p bound_entity=%p talos=%p\r\n",
                boss_bar,
                *(void **)boss_bar,
                game_base + BOSS_BAR_VTABLE_RVA,
                bound_entity,
                character);
            boss_bar_state_logged = TRUE;
            last_logged_boss_bar = boss_bar;
            last_logged_boss_bar_entity = bound_entity;
        }
        return;
    }
    upper_limit = *(float *)(boss_bar + BOSS_BAR_UPPER_LIMIT_OFFSET);
    lower_limit = *(float *)(boss_bar + BOSS_BAR_LOWER_LIMIT_OFFSET);
    if (!isfinite(upper_limit) || !isfinite(lower_limit)) {
        return;
    }
    if (bound_entity != character) {
        if (!boss_bar_state_logged || last_logged_boss_bar != boss_bar ||
            last_logged_boss_bar_entity != bound_entity) {
            SudekiMpLogFormat(
                "talos_coop_balance event=boss_bar_state "
                "status=rejected reason=entity_mismatch boss_bar=%p "
                "bound_entity=%p talos=%p upper=%.6f lower=%.6f\r\n",
                boss_bar,
                bound_entity,
                character,
                (double)upper_limit,
                (double)lower_limit);
            boss_bar_state_logged = TRUE;
            last_logged_boss_bar = boss_bar;
            last_logged_boss_bar_entity = bound_entity;
        }
        return;
    }
    combat_data = *(uint8_t **)(character + CHARACTER_COMBAT_DATA_OFFSET);
    if (!memory_accessible(combat_data, 0x34u, FALSE)) {
        return;
    }
    current_hp = *(float *)(combat_data + COMBAT_DATA_HP_OFFSET);
    maximum_hp = *(float *)(combat_data + COMBAT_DATA_MAX_HP_OFFSET);
    if (!isfinite(current_hp) || !isfinite(maximum_hp) || maximum_hp <= 0.0f) {
        return;
    }
    raw_ratio = current_hp / maximum_hp;
    mapped_ratio = upper_limit > lower_limit ?
        (raw_ratio - lower_limit) / (upper_limit - lower_limit) : -1.0f;
    ratio_bucket = (int)floorf(raw_ratio * 20.0f);
    fill_count = -1;
    fill_zero = -1.0f;
    fill_one = -1.0f;
    boss_bar_renderer = *(uint8_t **)(boss_bar + 0x4cu);
    if (memory_accessible(boss_bar_renderer, 0x5cu, FALSE)) {
        fill_count = *(int *)(boss_bar_renderer + 0x50u);
        fill_cache = *(float **)(boss_bar_renderer + 0x58u);
        if (fill_count >= 2 && fill_count <= 32 &&
            memory_accessible(fill_cache, 2u * sizeof(*fill_cache), FALSE)) {
            fill_zero = fill_cache[0];
            fill_one = fill_cache[1];
        }
    }
    stat_display = *(uint8_t **)(character + CHARACTER_STAT_DISPLAY_OFFSET);
    stat_health_bar = NULL;
    stat_fill_count = -1;
    stat_fill_zero = -1.0f;
    stat_fill_one = -1.0f;
    stat_last_hp = -1.0f;
    if (memory_accessible(
            stat_display,
            STAT_DISPLAY_LAST_HP_OFFSET + sizeof(float),
            FALSE) &&
        *(void **)stat_display == game_base + STAT_DISPLAY_VTABLE_RVA) {
        (void)refresh_talos_stat_display(
            game_base,
            stat_display,
            current_hp,
            maximum_hp);
        stat_last_hp = *(float *)(stat_display + STAT_DISPLAY_LAST_HP_OFFSET);
        stat_health_bar = stat_display + STAT_DISPLAY_HEALTH_BAR_OFFSET;
        if (memory_accessible(stat_health_bar, 0x5cu, FALSE)) {
            stat_fill_count = *(int *)(stat_health_bar + 0x50u);
            stat_fill_cache = *(float **)(stat_health_bar + 0x58u);
            if (stat_fill_count >= 2 && stat_fill_count <= 32 &&
                memory_accessible(
                    stat_fill_cache,
                    2u * sizeof(*stat_fill_cache),
                    FALSE)) {
                stat_fill_zero = stat_fill_cache[0];
                stat_fill_one = stat_fill_cache[1];
            }
        }
    }
    if (!boss_bar_state_logged || last_logged_boss_bar != boss_bar ||
        last_logged_boss_bar_entity != bound_entity ||
        last_logged_boss_bar_ratio_bucket != ratio_bucket) {
        SudekiMpLogFormat(
            "talos_coop_balance event=boss_bar_state status=bound "
            "boss_bar=%p bound_entity=%p talos=%p hp=%.3f max_hp=%.3f "
            "raw_ratio=%.6f upper=%.6f lower=%.6f mapped_ratio=%.6f "
            "renderer=%p fill_count=%d fill_cache_0=%.6f "
            "fill_cache_1=%.6f stat_display=%p stat_vtable=%p "
            "stat_last_hp=%.3f stat_renderer=%p stat_fill_count=%d "
            "stat_fill_cache_0=%.6f stat_fill_cache_1=%.6f\r\n",
            boss_bar,
            bound_entity,
            character,
            (double)current_hp,
            (double)maximum_hp,
            (double)raw_ratio,
            (double)upper_limit,
            (double)lower_limit,
            (double)mapped_ratio,
            boss_bar_renderer,
            fill_count,
            (double)fill_zero,
            (double)fill_one,
            stat_display,
            memory_accessible(stat_display, sizeof(void *), FALSE) ?
                *(void **)stat_display : NULL,
            (double)stat_last_hp,
            stat_health_bar,
            stat_fill_count,
            (double)stat_fill_zero,
            (double)stat_fill_one);
        boss_bar_state_logged = TRUE;
        last_logged_boss_bar = boss_bar;
        last_logged_boss_bar_entity = bound_entity;
        last_logged_boss_bar_ratio_bucket = ratio_bucket;
    }
}

static BOOL memory_accessible(
    const void *pointer,
    size_t size,
    BOOL require_write
) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    if (end < start || end > region_end) {
        return FALSE;
    }
    if (!require_write) {
        return TRUE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

BOOL SudekiMpTalosCoopBalanceConfigure(
    BOOL enabled,
    BOOL coop_profile,
    unsigned int health_scale,
    unsigned int stagger_limit,
    unsigned int stagger_window_seconds
) {
    if (health_scale < 1u || health_scale > 4u ||
        stagger_limit < 1u || stagger_limit > 100u ||
        stagger_window_seconds < 1u || stagger_window_seconds > 60u) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    balance_enabled = enabled;
    balance_coop_profile = coop_profile;
    balance_health_scale = health_scale;
    balance_stagger_limit = stagger_limit;
    balance_stagger_window_seconds = stagger_window_seconds;
    ++balance_generation;
    if (balance_generation == 0u) {
        balance_generation = 1u;
    }
    applied_character = NULL;
    applied_generation = 0u;
    next_probe_tick = 0u;
    boss_bar_state_logged = FALSE;
    last_logged_boss_bar = NULL;
    last_logged_boss_bar_entity = NULL;
    last_logged_boss_bar_ratio_bucket = -1;
    last_refreshed_stat_display = NULL;
    last_refreshed_stat_hp = -1.0f;
    last_refreshed_stat_max_hp = -1.0f;
    last_logged_stat_refresh_display = NULL;
    last_logged_stat_refresh_bucket = -1;
    SudekiMpLogFormat(
        "talos_coop_balance event=configure enabled=%s coop_profile=%s "
        "health_scale=%ux stagger_limit=%u stagger_window=%us "
        "policy=vanilla_defaults_unless_explicitly_enabled\r\n",
        enabled ? "true" : "false",
        coop_profile ? "true" : "false",
        health_scale,
        stagger_limit,
        stagger_window_seconds);
    return TRUE;
}

void SudekiMpTalosCoopBalanceService(void) {
    DWORD now;
    uint8_t *character;
    uint8_t *combat;
    uint8_t *combat_data;
    float current_hp;
    float maximum_hp;
    float target_maximum_hp;
    float target_current_hp;

    if (!balance_enabled || !balance_coop_profile) {
        applied_character = NULL;
        applied_generation = 0u;
        return;
    }
    now = GetTickCount();
    if (next_probe_tick != 0u && (LONG)(now - next_probe_tick) < 0) {
        return;
    }
    next_probe_tick = now + 500u;
    character = (uint8_t *)SudekiMpCleanroomEngineGenericEntity("BOSS_Talos");
    if (character == NULL) {
        applied_character = NULL;
        applied_generation = 0u;
        return;
    }
    combat = character + TALOS_COMBAT_OFFSET;
    if (!memory_accessible(character, TALOS_COMBAT_OFFSET + 0x73u, FALSE) ||
        !memory_accessible(combat, 0x73u, TRUE) ||
        *(void **)(combat + COMBAT_OWNER_OFFSET) != character) {
        SudekiMpLogFormat(
            "talos_coop_balance event=apply status=rejected "
            "reason=combat_owner_gate_failed character=%p combat=%p\r\n",
            character, combat);
        applied_character = character;
        applied_generation = balance_generation;
        return;
    }
    combat_data = *(uint8_t **)(character + CHARACTER_COMBAT_DATA_OFFSET);
    if (!memory_accessible(combat_data, 0x34u, TRUE)) {
        SudekiMpLogFormat(
            "talos_coop_balance event=apply status=rejected "
            "reason=combat_data_unavailable character=%p\r\n",
            character);
        applied_character = character;
        applied_generation = balance_generation;
        return;
    }
    current_hp = *(float *)(combat_data + COMBAT_DATA_HP_OFFSET);
    maximum_hp = *(float *)(combat_data + COMBAT_DATA_MAX_HP_OFFSET);
    target_maximum_hp = vanilla_talos_max_hp * (float)balance_health_scale;
    if (!isfinite(current_hp) || !isfinite(maximum_hp) ||
        maximum_hp <= 0.0f || current_hp < 0.0f || current_hp > maximum_hp ||
        (fabsf(maximum_hp - vanilla_talos_max_hp) > 0.5f &&
            fabsf(maximum_hp - target_maximum_hp) > 0.5f)) {
        SudekiMpLogFormat(
            "talos_coop_balance event=apply status=rejected "
            "reason=unexpected_hp_state character=%p hp=%.3f max_hp=%.3f "
            "expected_vanilla=%.3f expected_target=%.3f\r\n",
            character, (double)current_hp, (double)maximum_hp,
            (double)vanilla_talos_max_hp, (double)target_maximum_hp);
        applied_character = character;
        applied_generation = balance_generation;
        return;
    }
    if (character == applied_character &&
        applied_generation == balance_generation &&
        fabsf(maximum_hp - target_maximum_hp) <= 0.5f &&
        *(uint16_t *)(combat + COMBAT_KNOCKBACK_LIMIT_OFFSET) ==
            (uint16_t)balance_stagger_limit &&
        fabsf(*(float *)(combat + COMBAT_SESSION_SECONDS_OFFSET) -
            (float)balance_stagger_window_seconds) <= 0.001f) {
        synchronize_talos_boss_bar(character);
        next_probe_tick = now + 1000u;
        return;
    }
    target_current_hp = maximum_hp > 0.0f ?
        current_hp * target_maximum_hp / maximum_hp : target_maximum_hp;
    *(float *)(combat_data + COMBAT_DATA_MAX_HP_OFFSET) = target_maximum_hp;
    *(float *)(combat_data + COMBAT_DATA_HP_OFFSET) = target_current_hp;
    *(uint16_t *)(combat + COMBAT_KNOCKBACK_LIMIT_OFFSET) =
        (uint16_t)balance_stagger_limit;
    *(float *)(combat + COMBAT_SESSION_SECONDS_OFFSET) =
        (float)balance_stagger_window_seconds;
    synchronize_talos_boss_bar(character);
    applied_character = character;
    applied_generation = balance_generation;
    next_probe_tick = now + 1000u;
    SudekiMpLogFormat(
        "talos_coop_balance event=apply status=complete character=%p "
        "hp_before=%.3f max_hp_before=%.3f hp_after=%.3f max_hp_after=%.3f "
        "health_scale=%ux stagger_limit=%u stagger_window=%us "
        "combat_offset=0x%lx owner_verified=true\r\n",
        character,
        (double)current_hp,
        (double)maximum_hp,
        (double)target_current_hp,
        (double)target_maximum_hp,
        balance_health_scale,
        balance_stagger_limit,
        balance_stagger_window_seconds,
        (unsigned long)TALOS_COMBAT_OFFSET);
}

void SudekiMpTalosCoopBalanceReset(void) {
    balance_enabled = FALSE;
    balance_coop_profile = FALSE;
    balance_health_scale = 2u;
    balance_stagger_limit = 10u;
    balance_stagger_window_seconds = 10u;
    ++balance_generation;
    applied_character = NULL;
    applied_generation = 0u;
    next_probe_tick = 0u;
    boss_bar_state_logged = FALSE;
    last_logged_boss_bar = NULL;
    last_logged_boss_bar_entity = NULL;
    last_logged_boss_bar_ratio_bucket = -1;
    last_refreshed_stat_display = NULL;
    last_refreshed_stat_hp = -1.0f;
    last_refreshed_stat_max_hp = -1.0f;
    last_logged_stat_refresh_display = NULL;
    last_logged_stat_refresh_bucket = -1;
}
