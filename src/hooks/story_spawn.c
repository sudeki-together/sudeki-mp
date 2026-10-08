#include "hooks/story_spawn.h"
#include "hooks/story_flight.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include "input/key_binding.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum { SLOTS = 3 };
static char resources[SLOTS][64];
static UINT keys[SLOTS];
static BOOL was_down_slot[SLOTS];
static char resource[64];               /* resource of the last spawn (for the probe) */
static float at[3];
static BOOL at_player;
static float offset[3];
static BOOL installed;
static DWORD last_spawn_at;
static unsigned spawned;
static DWORD probe_until, probe_last;

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a = (uintptr_t)p;
    return p && n && VirtualQuery(p, &m, sizeof(m)) && m.State == MEM_COMMIT &&
        !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && a + n >= a && a + n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
/* Research probe: where did the spawned entity go, and does it hold a model link? */
static void probe(void) {
    DWORD now = GetTickCount();
    if (now > probe_until || now - probe_last < 1500u) return;
    probe_last = now;
    uint8_t *entity = SudekiMpCleanroomEngineGenericEntity(resource);
    if (!readable(entity, 0x3e4u)) {
        SudekiMpLogFormat("story_spawn event=probe resource=%s entity=%p readable=0\r\n", resource, (void *)entity);
        return;
    }
    const uint8_t *position = *(const uint8_t **)(entity + 0x44u);
    float x = 0, y = 0, z = 0; const uint8_t *link = NULL; const uint8_t *lookup = NULL;
    if (readable(position, 0xbcu)) {
        x = *(const float *)(position + 0x18u); y = *(const float *)(position + 0x1cu); z = *(const float *)(position + 0x20u);
        link = *(const uint8_t **)(position + 0xb4u);
        if (readable(link, 0x14u)) lookup = *(const uint8_t **)(link + 8u);
    }
    SudekiMpLogFormat("story_spawn event=probe resource=%s entity=%p vtable=%p pos=%.2f,%.2f,%.2f link=%p lookup=%p flags3e0=0x%02x movement=%p collision=%p arbiter=%p\r\n",
        resource, (void *)entity, *(void **)entity, (double)x, (double)y, (double)z, (const void *)link, (const void *)lookup,
        entity[0x3e0u], *(void **)(entity + 0x80u), *(void **)(entity + 0x60u), *(void **)(entity + 0x90u));
}

static void observe(void *owner, uint8_t *movement, const float position[3], float dt) {
    (void)owner; (void)movement; (void)dt;
    probe();
    int slot = -1;
    for (int i = 0; i < SLOTS; ++i) {
        BOOL down = keys[i] && (GetAsyncKeyState((int)keys[i]) & 0x8000) != 0;
        if (down && !was_down_slot[i] && slot < 0) slot = i;
        was_down_slot[i] = down;
    }
    if (slot < 0 || GetTickCount() - last_spawn_at < 2000u) return;
    last_spawn_at = GetTickCount();
    strcpy(resource, resources[slot]);
    float where[3];
    for (unsigned k = 0; k < 3u; ++k) where[k] = (at_player ? position[k] : at[k]) + offset[k];
    BOOL ok = SudekiMpCleanroomEngineSpawnEntityNamed(resource, where);
    if (ok) { ++spawned; probe_until = GetTickCount() + 15000u; probe_last = 0; }
    SudekiMpLogFormat("story_spawn event=spawn resource=%s at=%.2f,%.2f,%.2f accepted=%u total=%u policy=native_generic_entity_spawn_no_lifetime_claim\r\n",
        resource, (double)where[0], (double)where[1], (double)where[2], ok, spawned);
}

static BOOL read_text(const wchar_t *path, const wchar_t *k, char *out, size_t capacity) {
    wchar_t text[128]; size_t n = 0;
    GetPrivateProfileStringW(L"Spawn", k, L"", text, 128, path);
    if (!text[0]) return FALSE;
    for (; text[n] && n + 1 < capacity; ++n) { if (text[n] < 32 || text[n] > 126) return FALSE; out[n] = (char)text[n]; }
    out[n] = 0; return TRUE;
}
static BOOL read_floats(const wchar_t *path, const wchar_t *k, float *out, unsigned count) {
    char text[128]; if (!read_text(path, k, text, sizeof(text))) return FALSE;
    char *cursor = text;
    for (unsigned i = 0; i < count; ++i) {
        char *end = NULL; out[i] = (float)strtod(cursor, &end);
        if (end == cursor || !isfinite(out[i])) return FALSE;
        cursor = end; while (*cursor == ',' || *cursor == ' ') ++cursor;
    }
    return *cursor == 0;
}

BOOL SudekiMpInstallStorySpawn(HMODULE game_module, const wchar_t *config_path) {
    wchar_t key_text[32];
    if (!game_module || !config_path) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    /* Slot 1 reads Key/Resource (or Key1/Resource1); slots 2-3 read Key2/Resource2, Key3/Resource3. */
    int configured = 0;
    for (int i = 0; i < SLOTS; ++i) {
        wchar_t kname[16], rname[16];
        swprintf(kname, 16, i ? L"Key%d" : L"Key", i + 1); swprintf(rname, 16, i ? L"Resource%d" : L"Resource", i + 1);
        if (i == 0 && !read_text(config_path, rname, resources[0], sizeof(resources[0]))) { swprintf(kname, 16, L"Key1"); swprintf(rname, 16, L"Resource1"); }
        if (!read_text(config_path, rname, resources[i], sizeof(resources[i]))) { keys[i] = 0; resources[i][0] = 0; continue; }
        GetPrivateProfileStringW(L"Spawn", kname, i == 0 ? L"F7" : L"", key_text, 32, config_path);
        if (!key_text[0] || !SudekiMpParseInputKey(key_text, &keys[i])) {
            SudekiMpLogFormat("story_spawn event=install status=rejected reason=key slot=%d\r\n", i + 1);
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        ++configured;
    }
    if (!configured) {
        SudekiMpLogWrite("story_spawn event=install status=rejected reason=config\r\n");
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    strcpy(resource, resources[0]);
    at_player = !read_floats(config_path, L"Position", at, 3);
    if (!read_floats(config_path, L"Offset", offset, 3)) memset(offset, 0, sizeof(offset));
    if (!SudekiMpStoryFlightAddControlledObserver(observe)) {
        SudekiMpLogWrite("story_spawn event=install status=rejected reason=observer_slots\r\n");
        SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE;
    }
    installed = TRUE;
    for (int i = 0; i < SLOTS; ++i)
        if (keys[i]) SudekiMpLogFormat("story_spawn event=install slot=%d key=0x%02x resource=%s\r\n", i + 1, keys[i], resources[i]);
    SudekiMpLogFormat("story_spawn event=install status=success slots=%d position=%s offset=%.2f,%.2f,%.2f\r\n",
        configured, at_player ? "controlled_character" : "fixed", (double)offset[0], (double)offset[1], (double)offset[2]);
    return TRUE;
}

BOOL SudekiMpUninstallStorySpawn(void) {
    if (!installed) return TRUE;
    SudekiMpStoryFlightRemoveControlledObserver(observe);
    installed = FALSE;
    SudekiMpLogFormat("story_spawn event=uninstall status=success spawned=%u note=spawned_entities_remain_native_owned\r\n", spawned);
    return TRUE;
}
