#include "hooks/lan_arena_hit_feedback.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

enum { DAMAGE_RVA = 0xd21d0u, REACTION_RVA = 0xd20f0u,
    POPUP_RVA = 0x129800u, POPUP_CALL_RVA = 0x12969eu };
static const uint8_t damage_prefix[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8};
static const uint8_t reaction_prefix[] = {0x8b,0x51,0x5c,0x56,0x8b,0xf2,
    0xc1,0xe6,0x0e,0xc1,0xfe,0x17,0x3b,0xc6,0x74,0x62};
static const uint8_t popup_prefix[] = {0x55,0x8b,0x6c,0x24,0x08,0x80,0x7d,0x24,
    0x00,0x0f,0x84,0x8a,0x00,0x00,0x00};
typedef void (__attribute__((thiscall)) *DamageFn)(void *, void *);
typedef void (__stdcall *PopupFn)(void *, float, float, uint32_t);
static SudekiMpInlineHook damage_hook;
static SudekiMpRelativeCallHook popup_hook;
static DamageFn original_damage;
static PopupFn original_popup;
static SudekiMpLanHitWitness host_witness;
static DWORD game_thread;
static SudekiMpLanHitTarget host_target;
static uint32_t generation, hit_sequence;
static uint8_t history_count;
static SudekiMpLanArenaHitFeedback history[SUDEKIMP_LAN_ARENA_HIT_HISTORY_CAPACITY];
typedef struct Capture {
    struct Capture *parent;
    SudekiMpLanHitTarget target;
    SudekiMpLanArenaHitFeedback hit;
} Capture;
static Capture *current_capture;

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = (uintptr_t)p;
    return p && n && a + n >= a && VirtualQuery(p, &m, sizeof(m)) &&
        m.State == MEM_COMMIT && !(m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
        a + n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}

BOOL SudekiMpLanHitTargetValid(const SudekiMpLanHitTarget *t) {
    const uint8_t *e = t ? t->entity : NULL;
    return t && t->session && readable(e, 0xb4u) &&
        readable(t->combat, 0x74u) && readable(t->arbiter, 0x64u) &&
        readable(t->ui, 0xd0u) &&
        *(void **)(e + 0xa4u) == t->combat &&
        *(void **)(e + 0x90u) == t->arbiter &&
        *(void **)(e + 0xb0u) == t->ui &&
        *(void **)((uint8_t *)t->combat + 0x10u) == e &&
        *(void **)((uint8_t *)t->ui + 0x10u) == e;
}

BOOL SudekiMpLanHitResolveTarget(void *entity, uint64_t session, SudekiMpLanHitTarget *t) {
    uint8_t *e = entity;
    if (!t || !readable(e, 0xb4u)) return FALSE;
    *t = (SudekiMpLanHitTarget){entity,*(void **)(e + 0xa4u),
        *(void **)(e + 0xb0u),*(void **)(e + 0x90u),session};
    return SudekiMpLanHitTargetValid(t);
}

static BOOL same_target(const SudekiMpLanHitTarget *a, const SudekiMpLanHitTarget *b) {
    return a->session == b->session && a->entity == b->entity &&
        a->combat == b->combat && a->arbiter == b->arbiter && a->ui == b->ui;
}

static void bind_target(const SudekiMpLanHitTarget *target) {
    if (same_target(&host_target, target)) return;
    host_target = *target;
    if (++generation == 0u) ++generation;
    history_count = 0u;
    hit_sequence = 0u;
    memset(history, 0, sizeof(history));
}

static void __stdcall observe_popup(void *ui, float before, float after, uint32_t color) {
    Capture *capture = GetCurrentThreadId() == game_thread ? current_capture : NULL;
    if (capture && ui == capture->target.ui && SudekiMpLanHitTargetValid(&capture->target) &&
        *((uint8_t *)ui + 0x24u) &&
        (color == 0xffffffffu || color == 0xa02020ffu || color == 0xffff00ffu)) {
        capture->hit.amount = *(int32_t *)((uint8_t *)ui + 0x74u);
        capture->hit.value_before = before;
        capture->hit.value_after = after;
        capture->hit.color = color == 0xffffffffu ? 0u : color == 0xa02020ffu ? 1u : 2u;
        capture->hit.flags |= SUDEKIMP_LAN_HIT_POPUP;
    }
    original_popup(ui, before, after, color);
}

static void __attribute__((thiscall)) observe_damage(void *combat, void *packet) {
    Capture capture;
    SudekiMpLanHitTarget after_target;
    uint32_t before, after;
    memset(&capture, 0, sizeof(capture));
    if (GetCurrentThreadId() != game_thread || !host_witness ||
        !host_witness(&capture.target) || !SudekiMpLanHitTargetValid(&capture.target) ||
        capture.target.combat != combat) {
        original_damage(combat, packet);
        return;
    }
    bind_target(&capture.target);
    before = (*(uint32_t *)((uint8_t *)combat + 0x5cu) >> 9u) & 0x1ffu;
    capture.parent = current_capture;
    current_capture = &capture;
    original_damage(combat, packet); /* The sole gameplay consequence, host only. */
    current_capture = capture.parent;
    if (!host_witness(&after_target) || !same_target(&capture.target, &after_target) ||
        !SudekiMpLanHitTargetValid(&capture.target) ||
        !same_target(&capture.target, &host_target)) return;
    after = (*(uint32_t *)((uint8_t *)combat + 0x5cu) >> 9u) & 0x1ffu;
    if (before != after && after >= 0x2au && after <= 0x36u) {
        capture.hit.reaction = (uint8_t)after;
        capture.hit.flags |= SUDEKIMP_LAN_HIT_REACTION;
    }
    if (!capture.hit.flags) return;
    if (++hit_sequence == 0u) ++hit_sequence;
    capture.hit.sequence = hit_sequence;
    capture.hit.host_tick = GetTickCount();
    if (!SudekiMpLanArenaHitFeedbackValid(&capture.hit)) return;
    if (history_count == SUDEKIMP_LAN_ARENA_HIT_HISTORY_CAPACITY) {
        memmove(history, history + 1, (history_count - 1u) * sizeof(history[0]));
        --history_count;
    }
    history[history_count++] = capture.hit;
    SudekiMpLogFormat("lan_hit event=confirmed seq=%lu generation=%lu flags=%u reaction=%u amount=%ld\r\n",
        (unsigned long)hit_sequence, (unsigned long)generation, capture.hit.flags,
        capture.hit.reaction, (long)capture.hit.amount);
}

BOOL SudekiMpLanHitImageMatches(HMODULE image) {
    uint8_t *b = (uint8_t *)image;
    int32_t displacement;
    if (!b || memcmp(b + DAMAGE_RVA, damage_prefix, sizeof(damage_prefix)) ||
        memcmp(b + REACTION_RVA, reaction_prefix, sizeof(reaction_prefix)) ||
        b[REACTION_RVA + 0x73u] != 0xc3u ||
        memcmp(b + POPUP_RVA, popup_prefix, sizeof(popup_prefix)) ||
        memcmp(b + POPUP_RVA + 0x99u, (uint8_t[]){0x5d,0xc2,0x10,0x00}, 4u) ||
        b[POPUP_CALL_RVA] != 0xe8u) return FALSE;
    memcpy(&displacement, b + POPUP_CALL_RVA + 1u, 4u);
    return b + POPUP_CALL_RVA + 5u + displacement == b + POPUP_RVA;
}

BOOL SudekiMpLanHitHostInstall(HMODULE image, SudekiMpLanHitWitness witness) {
    uint8_t *b = (uint8_t *)image;
    if (!witness || damage_hook.installed || popup_hook.installed ||
        !SudekiMpLanHitImageMatches(image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    host_witness = witness;
    original_popup = (PopupFn)(b + POPUP_RVA);
    if (!SudekiMpInstallInlineHook(&damage_hook, b + DAMAGE_RVA,
        damage_prefix, sizeof(damage_prefix), observe_damage)) return FALSE;
    original_damage = (DamageFn)damage_hook.trampoline;
    if (!SudekiMpInstallRelativeCallHook(&popup_hook, b + POPUP_CALL_RVA,
        original_popup, observe_popup)) {
        DWORD error = GetLastError();
        (void)SudekiMpLanHitHostUninstall();
        SetLastError(error); return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpLanHitHostUninstall(void) {
    DWORD error = ERROR_SUCCESS;
    BOOL popup_ok, damage_ok;
    if (current_capture) { SetLastError(ERROR_BUSY); return FALSE; }
    popup_ok = SudekiMpRestoreRelativeCallHook(&popup_hook);
    if (!popup_ok) error = GetLastError();
    damage_ok = SudekiMpRestoreInlineHook(&damage_hook);
    if (!damage_ok && error == ERROR_SUCCESS) error = GetLastError();
    if (!popup_ok || !damage_ok) { SetLastError(error); return FALSE; }
    original_popup = NULL; original_damage = NULL; host_witness = NULL;
    game_thread = 0; current_capture = NULL;
    memset(&host_target, 0, sizeof(host_target)); history_count = 0;
    return TRUE;
}

void SudekiMpLanHitHostSnapshot(SudekiMpLanArenaEnemySnapshot *enemy) {
    SudekiMpLanHitTarget target = {0};
    if (!enemy || !damage_hook.installed || !host_witness) return;
    game_thread = GetCurrentThreadId();
    if (!host_witness(&target) || !SudekiMpLanHitTargetValid(&target)) {
        bind_target(&(SudekiMpLanHitTarget){0});
        return;
    }
    bind_target(&target);
    enemy->feedback_generation = generation;
    enemy->hit_count = history_count;
    memcpy(enemy->hits, history, sizeof(history));
}

BOOL SudekiMpLanHitConsume(SudekiMpLanHitCursor *cursor, uint64_t session,
    uintptr_t target, uint32_t host_tick, const SudekiMpLanArenaEnemySnapshot *enemy,
    SudekiMpLanHitReplay replay, void *context) {
    if (!cursor || !session || !target || !enemy || !replay ||
        enemy->hit_count > SUDEKIMP_LAN_ARENA_HIT_HISTORY_CAPACITY) return FALSE;
    /* Validate the entire journal before the first side effect. */
    for (unsigned int i = 0; i < enemy->hit_count; ++i)
        if (!enemy->feedback_generation || !SudekiMpLanArenaHitFeedbackValid(&enemy->hits[i]) ||
            (int32_t)(host_tick - enemy->hits[i].host_tick) < 0 ||
            (i && (!SudekiMpLanArenaSequenceNewer(enemy->hits[i].sequence,
                enemy->hits[i - 1].sequence) ||
                (int32_t)(enemy->hits[i].host_tick - enemy->hits[i - 1].host_tick) < 0)))
            return FALSE;
    if (!cursor->initialized || cursor->session != session || cursor->target != target ||
        cursor->generation != enemy->feedback_generation) {
        if (cursor->initialized && cursor->session == session && cursor->target == target &&
            cursor->generation && !SudekiMpLanArenaSequenceNewer(
                enemy->feedback_generation, cursor->generation)) return FALSE;
        *cursor = (SudekiMpLanHitCursor){session,target,enemy->feedback_generation,
            enemy->hit_count ? enemy->hits[enemy->hit_count - 1].sequence : 0u,1u};
        return TRUE;
    }
    for (unsigned int i = 0; i < enemy->hit_count; ++i) {
        const SudekiMpLanArenaHitFeedback *hit = &enemy->hits[i];
        if (cursor->sequence && !SudekiMpLanArenaSequenceNewer(hit->sequence, cursor->sequence)) continue;
        if (host_tick - hit->host_tick <= 1500u && !replay(context, hit)) return FALSE;
        cursor->sequence = hit->sequence;
    }
    return TRUE;
}

BOOL SudekiMpLanHitReplayNative(HMODULE image, const SudekiMpLanHitTarget *target,
    const SudekiMpLanArenaHitFeedback *hit) {
    uint8_t *b = (uint8_t *)image;
    if (!b || !SudekiMpLanHitTargetValid(target) ||
        !SudekiMpLanArenaHitFeedbackValid(hit)) return FALSE;
    /* Installation validates the exact image. No ApplyDamage, status, script,
     * audio or camera entry is reachable from these narrow presentation calls. */
    if (hit->flags & SUDEKIMP_LAN_HIT_REACTION) {
        uintptr_t combat = (uintptr_t)target->combat;
        uint32_t reaction = hit->reaction;
        void *entry = b + REACTION_RVA;
        __asm__ volatile("call *%2" : "+c"(combat), "+a"(reaction)
            : "r"(entry) : "edx", "memory", "cc");
    }
    if (hit->flags & SUDEKIMP_LAN_HIT_POPUP) {
        static const uint32_t colors[] = {0xffffffffu,0xa02020ffu,0xffff00ffu};
        *(int32_t *)((uint8_t *)target->ui + 0x74u) = hit->amount;
        ((PopupFn)(b + POPUP_RVA))(target->ui, hit->value_before,
            hit->value_after, colors[hit->color]);
    }
    SudekiMpLogFormat("lan_hit event=replayed seq=%lu flags=%u reaction=%u amount=%ld ui_enabled=%u pending_reaction=%lu\r\n",
        (unsigned long)hit->sequence, hit->flags, hit->reaction, (long)hit->amount,
        *((uint8_t *)target->ui + 0x24u),
        (unsigned long)((*(uint32_t *)((uint8_t *)target->combat + 0x5cu) >> 9u) & 0x1ffu));
    return TRUE;
}
