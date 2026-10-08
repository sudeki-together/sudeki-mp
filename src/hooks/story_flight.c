#include "hooks/story_flight.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Player flight.
 *
 * Seam: the character movement-controller update (RVA 0xc3200, `this` in EAX,
 * one stack argument: the frame update block whose +0xc is dt) has exactly six
 * exact-build callers, one per entity class whose update method embeds a
 * controller (RVA 0x145e4f, 0x15325c, 0x1442ec, 0x14df9c, 0x14d0af, 0x1524ef).
 * Each call is routed through one observer that may touch only the controller
 * it was handed, on the thread that is about to run the native update.
 *
 * Controller layout used here (CONFIRMED_STATIC from 0xc3200 / 0xc3de0 /
 * 0xc3b70): +0x10 owner entity; +0x34/+0x38/+0x3c velocity; +0x4c gravity
 * acceleration; +0xbf bit 0 "Should pull to ground" (serialised name), bit 1
 * the live gravity gate: `if (bf & 2) vy -= gravity * dt`. The vector at +0x34 is
 * applied per frame without a further dt factor (CONFIRMED_LIVE 2026-10-06). Elco's native
 * jetpack hover clears the same bit and zeroes +0x38. */
enum { SITE_COUNT = 6 };
static const unsigned SITE_RVA[SITE_COUNT] = {
    0x145e4fu, 0x15325cu, 0x1442ecu, 0x14df9cu, 0x14d0afu, 0x1524efu
};
enum {
    RVA_MOVEMENT_UPDATE = 0xc3200u,
    RVA_MOVEMENT_VTABLE = 0x2c8644u,
    RVA_MOVEMENT_SERIALISE = 0xc3de0u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x408da4u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    MOVEMENT_OWNER = 0x10u,
    MOVEMENT_VELOCITY_Y = 0x38u,
    MOVEMENT_FLAGS_HI = 0xbfu,
    MOVEMENT_SIZE = 0xc0u,
    ENTITY_MOVEMENT = 0x80u,
    GRAVITY_BIT = 0x2u
};
/* PUSH EBP; MOV EBP,ESP; AND ESP,-8; SUB ESP,0x14; PUSH EBX; PUSH ESI; MOV ESI,EAX; PUSH EDI */
static const uint8_t UPDATE_ENTRY[14] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu, 0x14u, 0x53u, 0x56u, 0x8bu, 0xf0u, 0x57u
};

static uint8_t *base;
static SudekiMpStoryFlightConfig cfg;
static SudekiMpRelativeCallHook sites[SITE_COUNT];
static void *native_movement_update __attribute__((used));
static DWORD game_thread;
static volatile LONG depth;

static BOOL flying;
static void *flying_actor;          /* compared only, never dereferenced */
static BOOL had_gravity;
static void *restore_actor;         /* gravity bit to put back when next seen */
static BOOL toggle_was_down;
enum { OBSERVER_SLOTS = 4 };
static SudekiMpStoryFlightControlledObserver controlled_observers[OBSERVER_SLOTS];
static DWORD identity_checked_at, last_log;
static unsigned frames;

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a = (uintptr_t)p;
    return p && n && VirtualQuery(p, &m, sizeof(m)) && m.State == MEM_COMMIT &&
        !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && a + n >= a &&
        a + n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static BOOL call_to(unsigned rva, unsigned target) {
    return readable(base + rva, 5) && base[rva] == 0xe8u &&
        (uintptr_t)(base + rva + 5 + *(int32_t *)(base + rva + 1)) == (uintptr_t)(base + target);
}
static BOOL key_down(UINT key) {
    return key && (GetAsyncKeyState((int)key) & 0x8000) != 0;
}
static uint8_t *movement_exact(uint8_t *m) {
    if (!readable(m, MOVEMENT_SIZE) || *(void **)m != base + RVA_MOVEMENT_VTABLE) return NULL;
    uint8_t *owner = *(uint8_t **)(m + MOVEMENT_OWNER);
    if (!readable(owner, ENTITY_MOVEMENT + 4u) || *(uint8_t **)(owner + ENTITY_MOVEMENT) != m) return NULL;
    return owner;
}
static void *controlled_character(void) {
    if (!readable(base + RVA_CHARACTER_CONTROLLER_GLOBAL, 4u)) return NULL;
    uint8_t *controller = *(uint8_t **)(base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    if (!readable(controller, CONTROLLER_TARGET_OFFSET + 4u)) return NULL;
    return *(void **)(controller + CONTROLLER_TARGET_OFFSET);
}
static BOOL is_ailish(void *owner) {
    if (!cfg.ailish_only) return TRUE;
    void *ailish = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_AILISH);
    return ailish && ailish == owner;
}
static void stop_flight(uint8_t *m, const char *reason) {
    if (m && had_gravity) m[MOVEMENT_FLAGS_HI] |= GRAVITY_BIT;
    else if (!m && had_gravity) restore_actor = flying_actor;
    SudekiMpLogFormat("story_flight event=off actor=%p reason=%s frames=%u gravity_restored=%u\r\n",
        flying_actor, reason, frames, m && had_gravity);
    flying = FALSE; flying_actor = NULL; frames = 0;
}

__attribute__((used, noinline)) static void story_flight_observe(uint8_t *m, const uint8_t *update) {
    if (!base) return;
    if (!game_thread) game_thread = GetCurrentThreadId();
    if (GetCurrentThreadId() != game_thread) return;
    uint8_t *owner = movement_exact(m);
    if (!owner) return;
    if (restore_actor && owner == restore_actor) {
        m[MOVEMENT_FLAGS_HI] |= GRAVITY_BIT; restore_actor = NULL;
        SudekiMpLogFormat("story_flight event=gravity_restored actor=%p\r\n", (void *)owner);
    }
    void *target = controlled_character();
    if (owner == target) {
        float dt_obs = readable(update, 0x10u) ? *(const float *)(update + 0xcu) : 0.0f;
        if (!isfinite(dt_obs) || dt_obs < 0.0f || dt_obs > 0.25f) dt_obs = 0.0f;
        const uint8_t *position = readable(owner + 0x44u, 4u) ? *(const uint8_t **)(owner + 0x44u) : NULL;
        if (readable(position, 0x24u)) {
            float at[3] = {*(const float *)(position + 0x18u), *(const float *)(position + 0x1cu), *(const float *)(position + 0x20u)};
            for (unsigned i = 0; i < OBSERVER_SLOTS; ++i) {
                SudekiMpStoryFlightControlledObserver observer = controlled_observers[i];
                if (!observer) continue;
                observer(owner, m, at, dt_obs);
                if (!movement_exact(m) || controlled_character() != owner) return;   /* the observer may have changed the world */
            }
        }
        /* One key sample per frame, taken on the controlled character's own update. */
        BOOL toggle = key_down(cfg.toggle_key);
        if (toggle && !toggle_was_down) {
            if (flying) {
                stop_flight(flying_actor == owner ? m : NULL, "toggle");
            } else if (!SudekiMpCleanroomEngineWorldReady()) {
                SudekiMpLogWrite("story_flight event=rejected reason=world_not_ready\r\n");
            } else if (!is_ailish(owner)) {
                SudekiMpLogFormat("story_flight event=rejected reason=not_ailish actor=%p\r\n", (void *)owner);
            } else {
                flying = TRUE; flying_actor = owner; frames = 0;
                had_gravity = (m[MOVEMENT_FLAGS_HI] & GRAVITY_BIT) != 0;
                identity_checked_at = GetTickCount();
                SudekiMpLogFormat("story_flight event=on actor=%p had_gravity=%u speed=%.2f\r\n",
                    (void *)owner, had_gravity, (double)cfg.speed);
            }
        }
        toggle_was_down = toggle;
    }
    if (!flying || owner != flying_actor) return;
    if (owner != target) { stop_flight(m, "control_changed"); return; }
    DWORD now = GetTickCount();
    if (now - identity_checked_at >= 1000u) {
        identity_checked_at = now;
        if (!SudekiMpCleanroomEngineWorldReady() || !is_ailish(owner)) { stop_flight(m, "identity_lost"); return; }
    }
    /* +0x38 is the displacement applied this frame (CONFIRMED_LIVE: 6.0 moved
     * about 6 units per frame), so scale the configured units/second by dt. */
    float dt = readable(update, 0x10u) ? *(const float *)(update + 0xcu) : 0.0f;
    if (!isfinite(dt) || dt < 0.0f || dt > 0.25f) dt = 0.0f;
    float vy = 0.0f;
    BOOL up = key_down(cfg.ascend_key), down = key_down(cfg.descend_key);
    if (up && !down) vy = cfg.speed * dt;
    else if (down && !up) vy = -cfg.speed * dt;
    m[MOVEMENT_FLAGS_HI] &= (uint8_t)~GRAVITY_BIT;
    *(float *)(m + MOVEMENT_VELOCITY_Y) = vy;
    ++frames;
    if (now - last_log >= 2000u) {
        last_log = now;
        /* CPosition (entity+0x44) world X/Y/Z at +0x18/+0x1c/+0x20 (docs/structures.md). */
        const uint8_t *position = readable(owner + 0x44u, 4u) ? *(const uint8_t **)(owner + 0x44u) : NULL;
        float x = 0, y = 0, z = 0;
        if (readable(position, 0x24u)) { x = *(const float *)(position + 0x18u); y = *(const float *)(position + 0x1cu); z = *(const float *)(position + 0x20u); }
        SudekiMpLogFormat("story_flight event=hold actor=%p vy=%.3f frames=%u pos=%.2f,%.2f,%.2f\r\n",
            (void *)owner, (double)vy, frames, (double)x, (double)y, (double)z);
    }
}

/* Entry: EAX = controller, [ESP+4] = update block. Preserve every register and
 * the stack, observe, then continue into the native update. */
__attribute__((naked, used, noinline)) static void story_flight_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "pushl 16(%esp)\n\t"            /* update block */
        "pushl 12(%esp)\n\t"            /* saved EAX = controller */
        "call _story_flight_observe\n\t"
        "addl $8,%esp\n\t"
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\t"
        "jmp *_native_movement_update\n\t");
}

BOOL SudekiMpStoryFlightAddControlledObserver(SudekiMpStoryFlightControlledObserver observer) {
    if (!observer) return FALSE;
    for (unsigned i = 0; i < OBSERVER_SLOTS; ++i) if (controlled_observers[i] == observer) return TRUE;
    for (unsigned i = 0; i < OBSERVER_SLOTS; ++i) if (!controlled_observers[i]) { controlled_observers[i] = observer; return TRUE; }
    return FALSE;
}
void SudekiMpStoryFlightRemoveControlledObserver(SudekiMpStoryFlightControlledObserver observer) {
    for (unsigned i = 0; i < OBSERVER_SLOTS; ++i) if (controlled_observers[i] == observer) controlled_observers[i] = NULL;
}
void SudekiMpStoryFlightSetControlledObserver(SudekiMpStoryFlightControlledObserver observer) {
    memset(controlled_observers, 0, sizeof(controlled_observers));
    if (observer) controlled_observers[0] = observer;
}

BOOL SudekiMpInstallStoryFlight(HMODULE game_module, const SudekiMpStoryFlightConfig *config) {
    if (!game_module || !config || !config->toggle_key || !config->ascend_key || !config->descend_key ||
        !isfinite(config->speed) || config->speed <= 0.0f || config->speed > 100.0f) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    base = (uint8_t *)game_module; cfg = *config;
    if (!readable(base + RVA_MOVEMENT_UPDATE, sizeof(UPDATE_ENTRY)) ||
        memcmp(base + RVA_MOVEMENT_UPDATE, UPDATE_ENTRY, sizeof(UPDATE_ENTRY)) != 0 ||
        !readable(base + RVA_MOVEMENT_VTABLE, 4u) ||
        *(void **)(base + RVA_MOVEMENT_VTABLE) != base + RVA_MOVEMENT_SERIALISE) {
        SudekiMpLogWrite("story_flight event=install status=rejected reason=movement_update_identity\r\n");
        base = NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for (unsigned i = 0; i < SITE_COUNT; ++i) {
        unsigned rva = SITE_RVA[i];
        /* LEA EAX,[ESI|EDI+disp32] (8D 86|87 ..) immediately precedes every call. */
        if (!call_to(rva, RVA_MOVEMENT_UPDATE) || !readable(base + rva - 6u, 6u) ||
            base[rva - 6u] != 0x8du || (base[rva - 5u] != 0x86u && base[rva - 5u] != 0x87u)) {
            SudekiMpLogFormat("story_flight event=install status=rejected reason=call_site site=%u rva=0x%x\r\n", i, rva);
            base = NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    }
    native_movement_update = base + RVA_MOVEMENT_UPDATE;
    memset(sites, 0, sizeof(sites));
    for (unsigned i = 0; i < SITE_COUNT; ++i) {
        if (!SudekiMpInstallRelativeCallHook(&sites[i], base + SITE_RVA[i], native_movement_update, story_flight_entry)) {
            DWORD error = GetLastError();
            while (i--) (void)SudekiMpRestoreRelativeCallHook(&sites[i]);
            SudekiMpLogFormat("story_flight event=install status=failed site=%u error=%lu\r\n", i, (unsigned long)error);
            base = NULL; SetLastError(error); return FALSE;
        }
    }
    SudekiMpLogFormat("story_flight event=install status=success sites=%u toggle=0x%02x ascend=0x%02x descend=0x%02x speed=%.2f ailish_only=%u policy=observer_before_native_update_gravity_bit_and_vy_only\r\n",
        (unsigned)SITE_COUNT, cfg.toggle_key, cfg.ascend_key, cfg.descend_key, (double)cfg.speed, cfg.ailish_only);
    return TRUE;
}

BOOL SudekiMpUninstallStoryFlight(void) {
    if (!base) return TRUE;
    BOOL ok = TRUE;
    for (unsigned i = SITE_COUNT; i--;) if (!SudekiMpRestoreRelativeCallHook(&sites[i])) ok = FALSE;
    if (!ok) {
        SudekiMpLogWrite("story_flight event=uninstall status=failed retained=true\r\n");
        return FALSE;
    }
    /* A controller left without gravity cannot be touched from this thread;
     * the native default returns with the next character load. */
    SudekiMpLogFormat("story_flight event=uninstall status=success flying_left=%u\r\n", flying);
    flying = FALSE; flying_actor = NULL; restore_actor = NULL; memset(controlled_observers, 0, sizeof(controlled_observers)); base = NULL;
    return TRUE;
}
