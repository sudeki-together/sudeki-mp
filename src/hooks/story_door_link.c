#include "hooks/story_door_link.h"
#include "hooks/story_flight.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include "input/key_binding.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Door links.
 *
 * Native facts (CONFIRMED_STATIC, Ghidra on the supported image):
 *  - CWorld global RVA 0x408d10. CWorld+0x10 = main zone record, +0xc = active
 *    temporary zone record (or NULL). Zone records are 0x54 bytes: +0x24 name
 *    (char *), +0x34 kind (4 = active temporary interior).
 *  - CWorld::EnterTemporaryZone RVA 0x64b0, thiscall(CWorld *, const char *zone,
 *    ResourceName *start_location). Forces the ResourceName type to 53
 *    (Locations), saves the lead's position into CWorld+0x28..+0x30, the
 *    negated CPosition+0x50..+0x58 vector into +0x34..+0x3c and the lead's
 *    sector id (u16) into +0x40, then loads the interior.
 *  - CWorld::ExitTemporaryZone RVA 0x6710, thiscall(CWorld *). Restores the
 *    lead from CWorld+0x28..+0x40 through the exit lead move (0xf30a0).
 *
 * A link therefore needs no script: inside the interior box we overwrite the
 * saved return block with the configured exterior target and call the native
 * exit; inside the exterior box we call the native enter with the configured
 * start Location. Both are edge-triggered (the box must be left before it can
 * fire again) and rate-limited. INFERENCE: the stored facing is the negated
 * direction, mirroring what EnterTemporaryZone stores; verified live or
 * corrected by the first test. */
enum {
    RVA_WORLD_GLOBAL = 0x408d10u,
    RVA_ENTER_TEMPORARY_ZONE = 0x64b0u,
    RVA_EXIT_TEMPORARY_ZONE = 0x6710u,
    WORLD_TEMPORARY_RECORD = 0xcu,
    WORLD_MAIN_RECORD = 0x10u,
    WORLD_RETURN_POSITION = 0x28u,
    WORLD_RETURN_FACING = 0x34u,
    WORLD_RETURN_SECTOR = 0x40u,
    RECORD_NAME = 0x24u,
    RECORD_KIND = 0x34u,
    RECORD_SIZE = 0x54u,
    KIND_TEMPORARY_ACTIVE = 4,
    MAX_LINKS = 8,
    COOLDOWN_MS = 4000
};
static const uint8_t ENTER_ENTRY[16] = {0x55u,0x8bu,0xecu,0x83u,0xe4u,0xf8u,0x83u,0xecu,0x24u,0x53u,0x56u,0x8bu,0x75u,0x0cu,0x8bu,0x06u};
static const uint8_t EXIT_ENTRY[14] = {0x55u,0x8bu,0xecu,0x83u,0xe4u,0xf0u,0x81u,0xecu,0x84u,0x00u,0x00u,0x00u,0x53u,0x56u};

typedef struct Link {
    char world[64], interior[64], location[64];
    float interior_box[6], exterior_box[6];   /* centre xyz, half extents xyz */
    float exit_target[5];                      /* x y z, facing dx dz */
    unsigned exit_sector;
    BOOL walk_in;                              /* fire on entering the box instead of on the action key */
    BOOL inside_interior, inside_exterior;     /* edge state */
} Link;

typedef void (__attribute__((thiscall)) *EnterFunction)(void *world, const char *zone, SudekiMpResourceName *location);
typedef void (__attribute__((thiscall)) *ExitFunction)(void *world);

static uint8_t *base;
static Link links[MAX_LINKS];
static unsigned link_count;
static DWORD last_transition_at;
static UINT action_key;
static BOOL action_was_down;
static SudekiMpResourceName retained_location;
static BOOL retained_valid;
static char last_logged_state[160];

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a = (uintptr_t)p;
    return p && n && VirtualQuery(p, &m, sizeof(m)) && m.State == MEM_COMMIT &&
        !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && a + n >= a &&
        a + n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static BOOL writable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    return readable(p, n) && VirtualQuery(p, &m, sizeof(m)) &&
        (m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
static const char *record_name(const uint8_t *record, int want_kind) {
    if (!readable(record, RECORD_SIZE)) return NULL;
    if (want_kind >= 0 && *(const int32_t *)(record + RECORD_KIND) != want_kind) return NULL;
    const char *name = *(const char *const *)(record + RECORD_NAME);
    if (!readable(name, 1u)) return NULL;
    for (unsigned i = 0; i < 64u; ++i) {
        if (!readable(name + i, 1u)) return NULL;
        if (!name[i]) return i ? name : NULL;
    }
    return NULL;
}
static uint8_t *world_exact(void) {
    if (!readable(base + RVA_WORLD_GLOBAL, 4u)) return NULL;
    uint8_t *world = *(uint8_t **)(base + RVA_WORLD_GLOBAL);
    return readable(world, 0x60u) ? world : NULL;
}
static BOOL in_box(const float p[3], const float box[6]) {
    for (unsigned k = 0; k < 3u; ++k)
        if (fabsf(p[k] - box[k]) > box[3u + k]) return FALSE;
    return TRUE;
}
static void release_retained(void) {
    if (retained_valid) {
        SudekiMpCleanroomEngineReleaseResourceName(&retained_location);
        retained_valid = FALSE;
    }
}

static void observe(void *owner, uint8_t *movement, const float position[3], float dt) {
    (void)owner; (void)movement; (void)dt;
    if (!base || !link_count) return;
    uint8_t *world = world_exact();
    if (!world) return;
    const char *interior = record_name(*(uint8_t **)(world + WORLD_TEMPORARY_RECORD), KIND_TEMPORARY_ACTIVE);
    const char *main_name = record_name(*(uint8_t **)(world + WORLD_MAIN_RECORD), -1);
    if (SudekiMpLogResearchEnabled()) {
        char state[160];
        snprintf(state, sizeof(state), "world=%s interior=%s", main_name ? main_name : "?", interior ? interior : "-");
        if (strcmp(state, last_logged_state)) {
            strcpy(last_logged_state, state);
            SudekiMpLogFormat("story_door_link event=state %s pos=%.2f,%.2f,%.2f\r\n", state, (double)position[0], (double)position[1], (double)position[2]);
        }
    }
    if (!interior && retained_valid) release_retained();   /* the interior is gone; the load is over */
    DWORD now = GetTickCount();
    BOOL cooling = now - last_transition_at < (DWORD)COOLDOWN_MS;
    /* The action key is sampled once per frame; a link fires on its press edge
     * while the character stands in the box (the native trigger placed there
     * shows the interact prompt). Walk-in links fire on entering the box. */
    BOOL action_down = action_key && (GetAsyncKeyState((int)action_key) & 0x8000) != 0;
    BOOL action_pressed = action_down && !action_was_down;
    action_was_down = action_down;
    for (unsigned i = 0; i < link_count; ++i) {
        Link *l = &links[i];
        BOOL inside_interior = interior && !_stricmp(interior, l->interior) && in_box(position, l->interior_box);
        BOOL inside_exterior = !interior && main_name && !_stricmp(main_name, l->world) && in_box(position, l->exterior_box);
        BOOL fire_interior = inside_interior && !cooling && (l->walk_in ? !l->inside_interior : action_pressed);
        BOOL fire_exterior = inside_exterior && !cooling && (l->walk_in ? !l->inside_exterior : action_pressed);
        if (fire_interior) {
            if (!writable(world + WORLD_RETURN_POSITION, 0x1au)) { l->inside_interior = TRUE; continue; }
            float *ret = (float *)(world + WORLD_RETURN_POSITION);
            ret[0] = l->exit_target[0]; ret[1] = l->exit_target[1]; ret[2] = l->exit_target[2];
            float *facing = (float *)(world + WORLD_RETURN_FACING);
            facing[0] = -l->exit_target[3]; facing[1] = 0.0f; facing[2] = -l->exit_target[4];
            *(uint16_t *)(world + WORLD_RETURN_SECTOR) = (uint16_t)l->exit_sector;
            SudekiMpLogFormat("story_door_link event=exit link=%u interior=%s target=%.2f,%.2f,%.2f facing=%.2f,%.2f sector=0x%04x policy=native_exit_with_overridden_return_block\r\n",
                i, l->interior, (double)ret[0], (double)ret[1], (double)ret[2], (double)l->exit_target[3], (double)l->exit_target[4], l->exit_sector);
            last_transition_at = now;
            l->inside_interior = TRUE; l->inside_exterior = TRUE;   /* arriving inside the exterior box must not re-enter */
            ((ExitFunction)(base + RVA_EXIT_TEMPORARY_ZONE))(world);
            return;                                                 /* the world changed under us */
        }
        if (fire_exterior) {
            release_retained();
            if (!SudekiMpCleanroomEngineResourceNameFromText(&retained_location, l->location)) {
                SudekiMpLogFormat("story_door_link event=enter link=%u status=rejected reason=resource_name\r\n", i);
                l->inside_exterior = TRUE; continue;
            }
            retained_valid = TRUE;
            SudekiMpLogFormat("story_door_link event=enter link=%u world=%s interior=%s location=%s policy=native_enter_with_named_location\r\n",
                i, l->world, l->interior, l->location);
            last_transition_at = now;
            l->inside_exterior = TRUE; l->inside_interior = TRUE;   /* arriving inside the interior box must not re-exit */
            ((EnterFunction)(base + RVA_ENTER_TEMPORARY_ZONE))(world, l->interior, &retained_location);
            return;
        }
        l->inside_interior = inside_interior;
        l->inside_exterior = inside_exterior;
    }
}

static BOOL read_text(const wchar_t *path, const wchar_t *key, char *out, size_t capacity) {
    wchar_t text[128];
    GetPrivateProfileStringW(L"DoorLink", key, L"", text, 128, path);
    if (!text[0]) return FALSE;
    size_t n = 0;
    for (; text[n] && n + 1 < capacity; ++n) {
        if (text[n] > 126 || text[n] < 32) return FALSE;
        out[n] = (char)text[n];
    }
    out[n] = 0;
    return TRUE;
}
static BOOL read_floats(const wchar_t *path, const wchar_t *key, float *out, unsigned count) {
    char text[128];
    if (!read_text(path, key, text, sizeof(text))) return FALSE;
    char *cursor = text;
    for (unsigned i = 0; i < count; ++i) {
        char *end = NULL;
        out[i] = (float)strtod(cursor, &end);
        if (end == cursor || !isfinite(out[i])) return FALSE;
        cursor = end;
        while (*cursor == ',' || *cursor == ' ') ++cursor;
    }
    return *cursor == 0;
}

BOOL SudekiMpInstallStoryDoorLink(HMODULE game_module, const wchar_t *config_path) {
    if (!game_module || !config_path) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    base = (uint8_t *)game_module;
    if (!readable(base + RVA_ENTER_TEMPORARY_ZONE, sizeof(ENTER_ENTRY)) ||
        memcmp(base + RVA_ENTER_TEMPORARY_ZONE, ENTER_ENTRY, sizeof(ENTER_ENTRY)) != 0 ||
        !readable(base + RVA_EXIT_TEMPORARY_ZONE, sizeof(EXIT_ENTRY)) ||
        memcmp(base + RVA_EXIT_TEMPORARY_ZONE, EXIT_ENTRY, sizeof(EXIT_ENTRY)) != 0 ||
        !readable(base + RVA_WORLD_GLOBAL, 4u)) {
        SudekiMpLogWrite("story_door_link event=install status=rejected reason=native_identity\r\n");
        base = NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    link_count = 0;
    {
        char key_text[32]; wchar_t key_w[32]; UINT key = VK_RETURN;
        GetPrivateProfileStringW(L"DoorLink", L"ActionKey", L"Enter", key_w, 32, config_path);
        if (!SudekiMpParseInputKey(key_w, &key)) {
            SudekiMpLogWrite("story_door_link event=install status=rejected reason=action_key\r\n");
            base = NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        (void)key_text;
        action_key = key;
    }
    int count = GetPrivateProfileIntW(L"DoorLink", L"Links", 0, config_path);
    if (count < 1 || count > MAX_LINKS) {
        SudekiMpLogWrite("story_door_link event=install status=rejected reason=link_count\r\n");
        base = NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for (int i = 0; i < count; ++i) {
        Link *l = &links[i]; wchar_t key[48]; float sector = 0;
        memset(l, 0, sizeof(*l));
        float target[6];
#define KEY(name) (swprintf(key, 48, L"Link%d.%ls", i + 1, name), key)
        if (!read_text(config_path, KEY(L"World"), l->world, sizeof(l->world)) ||
            !read_text(config_path, KEY(L"Interior"), l->interior, sizeof(l->interior)) ||
            !read_text(config_path, KEY(L"Location"), l->location, sizeof(l->location)) ||
            !read_floats(config_path, KEY(L"InteriorBox"), l->interior_box, 6) ||
            !read_floats(config_path, KEY(L"ExteriorBox"), l->exterior_box, 6) ||
            !read_floats(config_path, KEY(L"ExitTarget"), target, 6) ||
            l->interior_box[3] <= 0 || l->interior_box[4] <= 0 || l->interior_box[5] <= 0 ||
            l->exterior_box[3] <= 0 || l->exterior_box[4] <= 0 || l->exterior_box[5] <= 0) {
            SudekiMpLogFormat("story_door_link event=install status=rejected reason=link_config link=%d\r\n", i + 1);
            base = NULL; link_count = 0; SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
#undef KEY
        memcpy(l->exit_target, target, sizeof(l->exit_target));
        sector = target[5];
        float norm = sqrtf(target[3] * target[3] + target[4] * target[4]);
        if (norm < 1e-3f || sector < 0 || sector > 65535.0f) {
            SudekiMpLogFormat("story_door_link event=install status=rejected reason=exit_target link=%d\r\n", i + 1);
            base = NULL; link_count = 0; SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        l->exit_target[3] /= norm; l->exit_target[4] /= norm;
        l->exit_sector = (unsigned)sector;
        {
            char mode[16] = "action";
            (void)read_text(config_path, (swprintf(key, 48, L"Link%d.Trigger", i + 1), key), mode, sizeof(mode));
            l->walk_in = !_stricmp(mode, "walk");
        }
        l->inside_interior = l->inside_exterior = TRUE;   /* arm only after the boxes are seen empty once */
        ++link_count;
    }
    if (!SudekiMpStoryFlightAddControlledObserver(observe)) {
        SudekiMpLogWrite("story_door_link event=install status=rejected reason=observer_slots\r\n");
        base = NULL; link_count = 0; SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE;
    }
    for (unsigned i = 0; i < link_count; ++i)
        SudekiMpLogFormat("story_door_link event=install link=%u world=%s interior=%s location=%s interior_box=%.2f,%.2f,%.2f/%.2f,%.2f,%.2f exterior_box=%.2f,%.2f,%.2f/%.2f,%.2f,%.2f exit=%.2f,%.2f,%.2f facing=%.2f,%.2f sector=0x%04x\r\n",
            i, links[i].world, links[i].interior, links[i].location,
            (double)links[i].interior_box[0], (double)links[i].interior_box[1], (double)links[i].interior_box[2], (double)links[i].interior_box[3], (double)links[i].interior_box[4], (double)links[i].interior_box[5],
            (double)links[i].exterior_box[0], (double)links[i].exterior_box[1], (double)links[i].exterior_box[2], (double)links[i].exterior_box[3], (double)links[i].exterior_box[4], (double)links[i].exterior_box[5],
            (double)links[i].exit_target[0], (double)links[i].exit_target[1], (double)links[i].exit_target[2], (double)links[i].exit_target[3], (double)links[i].exit_target[4], links[i].exit_sector);
    SudekiMpLogFormat("story_door_link event=install status=success links=%u action_key=0x%02x seam=story_flight_controlled_observer\r\n", link_count, action_key);
    return TRUE;
}

BOOL SudekiMpUninstallStoryDoorLink(void) {
    if (!base) return TRUE;
    SudekiMpStoryFlightRemoveControlledObserver(observe);
    /* A ResourceName the loader may still read is retained rather than freed. */
    if (retained_valid) SudekiMpLogWrite("story_door_link event=uninstall note=start_location_resource_retained\r\n");
    link_count = 0; base = NULL;
    SudekiMpLogWrite("story_door_link event=uninstall status=success\r\n");
    return TRUE;
}
