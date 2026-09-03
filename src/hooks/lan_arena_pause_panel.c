#include "hooks/lan_arena_pause_panel.h"

#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_runtime.h"
#include "network/lan_arena_session.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void (*QuitScreenRenderFunction)(void);
typedef void (*QuitScreenActionFunction)(void);
typedef void (__cdecl *QuitScreenShowFunction)(BOOL visible);

typedef struct SudekiMpD3DSurfaceDesc {
    int format;
    int resource_type;
    DWORD usage;
    int pool;
    int multisample_type;
    DWORD multisample_quality;
    UINT width;
    UINT height;
} SudekiMpD3DSurfaceDesc;

typedef struct SudekiMpD3DLockedRect {
    int pitch;
    void *bits;
} SudekiMpD3DLockedRect;

typedef struct SudekiMpMenuVertex {
    float x;
    float y;
    float z;
    float reciprocal_w;
    float u;
    float v;
} SudekiMpMenuVertex;

typedef HRESULT (__stdcall *D3DCreateTextureFunction)(
    void *, UINT, UINT, UINT, DWORD, int, int, void **, HANDLE *);
typedef HRESULT (__stdcall *D3DTextureLockRectFunction)(
    void *, UINT, SudekiMpD3DLockedRect *, const RECT *, DWORD);
typedef HRESULT (__stdcall *D3DTextureUnlockRectFunction)(void *, UINT);
typedef HRESULT (__stdcall *D3DGetRenderTargetFunction)(void *, DWORD, void **);
typedef HRESULT (__stdcall *D3DSurfaceGetDescFunction)(
    void *, SudekiMpD3DSurfaceDesc *);
typedef HRESULT (__stdcall *D3DCreateStateBlockFunction)(void *, int, void **);
typedef HRESULT (__stdcall *D3DSetRenderStateFunction)(void *, int, DWORD);
typedef HRESULT (__stdcall *D3DSetTextureFunction)(void *, DWORD, void *);
typedef HRESULT (__stdcall *D3DSetTextureStageStateFunction)(
    void *, DWORD, int, DWORD);
typedef HRESULT (__stdcall *D3DSetSamplerStateFunction)(
    void *, DWORD, int, DWORD);
typedef HRESULT (__stdcall *D3DSetShaderFunction)(void *, void *);
typedef HRESULT (__stdcall *D3DSetFvfFunction)(void *, DWORD);
typedef HRESULT (__stdcall *D3DDrawPrimitiveUpFunction)(
    void *, int, UINT, const void *, UINT);
typedef HRESULT (__stdcall *D3DStateBlockApplyFunction)(void *);
typedef ULONG (__stdcall *ComReleaseFunction)(void *);

enum {
    RVA_PC_QUIT_SCREEN_GLOBAL = 0x00408d68u,
    RVA_PC_QUIT_SCREEN_SHOW = 0x0001dbe0u,
    RVA_PC_QUIT_SCREEN_SHOW_INTERNAL = 0x0001d700u,
    RVA_PC_QUIT_SCREEN_RENDER = 0x0001d690u,
    RVA_PC_QUIT_SCREEN_RENDER_CALL = 0x0028d572u,
    RVA_PC_QUIT_SCREEN_SELECT = 0x0001d780u,
    RVA_PC_QUIT_SCREEN_SELECT_CALL = 0x0001db71u,
    RVA_PC_QUIT_SCREEN_BACK = 0x0001d860u,
    RVA_PC_QUIT_SCREEN_BACK_CALL = 0x0001db64u,
    RVA_PC_QUIT_SCREEN_NAVIGATE = 0x0001d9f0u,
    RVA_PC_QUIT_SCREEN_ANALOG_NAVIGATE_CALL = 0x0001d9dfu,
    RVA_PC_QUIT_SCREEN_NAVIGATE_CALL = 0x0001dba4u,
    RVA_D3D_DEVICE_GLOBAL = 0x003c31dcu,
    PC_QUIT_SCREEN_STATE_OFFSET = 0x10u,
    PC_QUIT_SCREEN_VISIBLE_OFFSET = 0x1c2u,
    PC_QUIT_SCREEN_CURSOR_OFFSET = 0x1c4u,
    PC_QUIT_SCREEN_STATE_MAIN = 1u,
    MULTIPLAYER_PAGE_PRIMARY = 0u,
    MULTIPLAYER_PAGE_COMBAT = 1u,
    MULTIPLAYER_PAGE_BACK = 2u,
    MULTIPLAYER_PAGE_ROW_COUNT = 3u,
    OVERLAY_WIDTH = 640u,
    OVERLAY_HEIGHT = 480u,
    D3D_DEVICE_CREATE_TEXTURE_INDEX = 23u,
    D3D_DEVICE_GET_RENDER_TARGET_INDEX = 38u,
    D3D_DEVICE_SET_RENDER_STATE_INDEX = 57u,
    D3D_DEVICE_CREATE_STATE_BLOCK_INDEX = 59u,
    D3D_DEVICE_SET_TEXTURE_INDEX = 65u,
    D3D_DEVICE_SET_TEXTURE_STAGE_STATE_INDEX = 67u,
    D3D_DEVICE_SET_SAMPLER_STATE_INDEX = 69u,
    D3D_DEVICE_DRAW_PRIMITIVE_UP_INDEX = 83u,
    D3D_DEVICE_SET_FVF_INDEX = 89u,
    D3D_DEVICE_SET_VERTEX_SHADER_INDEX = 92u,
    D3D_DEVICE_SET_PIXEL_SHADER_INDEX = 107u,
    D3D_SURFACE_GET_DESC_INDEX = 12u,
    D3D_TEXTURE_LOCK_RECT_INDEX = 19u,
    D3D_TEXTURE_UNLOCK_RECT_INDEX = 20u,
    D3D_STATE_BLOCK_APPLY_INDEX = 5u,
    D3D_USAGE_DYNAMIC = 0x00000200u,
    D3D_POOL_DEFAULT = 0,
    D3D_FORMAT_A8R8G8B8 = 21,
    D3D_STATE_BLOCK_ALL = 1,
    D3D_PRIMITIVE_TRIANGLE_STRIP = 5,
    D3D_FVF_XYZRHW_TEX1 = 0x00000104u,
    D3D_RENDER_STATE_Z_ENABLE = 7,
    D3D_RENDER_STATE_Z_WRITE_ENABLE = 14,
    D3D_RENDER_STATE_ALPHA_TEST_ENABLE = 15,
    D3D_RENDER_STATE_SRC_BLEND = 19,
    D3D_RENDER_STATE_DEST_BLEND = 20,
    D3D_RENDER_STATE_CULL_MODE = 22,
    D3D_RENDER_STATE_ALPHA_BLEND_ENABLE = 27,
    D3D_RENDER_STATE_FOG_ENABLE = 28,
    D3D_RENDER_STATE_LIGHTING = 137,
    D3D_RENDER_STATE_SCISSOR_TEST_ENABLE = 174,
    D3D_BLEND_SRC_ALPHA = 5,
    D3D_BLEND_INVERSE_SRC_ALPHA = 6,
    D3D_CULL_NONE = 1,
    D3D_TEXTURE_STAGE_COLOR_OPERATION = 1,
    D3D_TEXTURE_STAGE_COLOR_ARGUMENT_ONE = 2,
    D3D_TEXTURE_STAGE_ALPHA_OPERATION = 4,
    D3D_TEXTURE_STAGE_ALPHA_ARGUMENT_ONE = 5,
    D3D_TEXTURE_OPERATION_DISABLE = 1,
    D3D_TEXTURE_OPERATION_SELECT_ARGUMENT_ONE = 2,
    D3D_TEXTURE_ARGUMENT_TEXTURE = 2,
    D3D_SAMPLER_MAG_FILTER = 5,
    D3D_SAMPLER_MIN_FILTER = 6,
    D3D_TEXTURE_FILTER_POINT = 1
};

static const uint8_t font_letters[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17},
    {31,4,4,4,4,4,31}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};
static const uint8_t font_digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static SudekiMpRelativeCallHook quit_render_hook;
static SudekiMpRelativeCallHook quit_select_hook;
static SudekiMpRelativeCallHook quit_back_hook;
static SudekiMpRelativeCallHook quit_navigate_hook;
static SudekiMpRelativeCallHook quit_analog_navigate_hook;
static QuitScreenRenderFunction original_quit_render;
static QuitScreenActionFunction original_quit_select;
static QuitScreenActionFunction original_quit_back;
static QuitScreenActionFunction original_quit_navigate;
static QuitScreenShowFunction native_quit_show;
static uint8_t *game_base;
static void **d3d_device_global;
static void *overlay_texture;
static void *overlay_texture_device;
static BOOL multiplayer_page_active __attribute__((used));
static BOOL parent_close_requested __attribute__((used));
static unsigned int multiplayer_page_cursor;
static BOOL action_handled __attribute__((used));
static BOOL backspace_was_down;
static BOOL edit_key_was_down[12];
static BOOL page_up_was_down;
static BOOL page_down_was_down;
static BOOL page_select_was_down;
static BOOL page_back_was_down;
static char client_address_edit[32];
static char last_endpoint_address[16];
static unsigned int last_endpoint_port;
static DWORD last_action_error;
static BOOL render_failure_logged;
static DWORD primary_action_blocked_until_ms;

static const uint8_t expected_quit_render_entry[] = {
    0x57u, 0x8bu, 0xf8u, 0x80u, 0xbfu,
    0xc2u, 0x01u, 0x00u, 0x00u, 0x00u
};

static BOOL relative_call_targets(
    const uint8_t *instruction,
    const uint8_t *expected_target
) {
    int32_t displacement;
    if (instruction == NULL || expected_target == NULL ||
        instruction[0] != 0xe8u) return FALSE;
    memcpy(&displacement, instruction + 1u, sizeof(displacement));
    return instruction + 5u + displacement == expected_target;
}

static BOOL quit_show_signature_matches(const uint8_t *base) {
    static const uint8_t prefix[] = {0x56u,0x8bu,0x35u};
    static const uint8_t body[] = {
        0x85u,0xf6u,0x74u,0x0au,0x8bu,0x44u,0x24u,0x08u,0x50u
    };
    static const uint8_t suffix[] = {0x5eu,0xc3u};
    const uint8_t *entry;
    uint32_t global_address;
    if (base == NULL) return FALSE;
    entry = base + RVA_PC_QUIT_SCREEN_SHOW;
    memcpy(&global_address, entry + sizeof(prefix), sizeof(global_address));
    return memcmp(entry, prefix, sizeof(prefix)) == 0 &&
        global_address == (uint32_t)(uintptr_t)(
            base + RVA_PC_QUIT_SCREEN_GLOBAL) &&
        memcmp(entry + 7u, body, sizeof(body)) == 0 &&
        relative_call_targets(
            entry + 16u, base + RVA_PC_QUIT_SCREEN_SHOW_INTERNAL) &&
        memcmp(entry + 21u, suffix, sizeof(suffix)) == 0;
}

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION region;
    uintptr_t start = (uintptr_t)pointer;
    uintptr_t finish = start + size;
    if (pointer == NULL || size == 0u || finish < start ||
        VirtualQuery(pointer, &region, sizeof(region)) != sizeof(region) ||
        region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0u ||
        region.Protect == PAGE_NOACCESS) return FALSE;
    return finish <= (uintptr_t)region.BaseAddress + region.RegionSize;
}

static void release_com_object(void **object) {
    void **vtable;
    ComReleaseFunction release;
    if (object == NULL || *object == NULL) return;
    vtable = *(void ***)*object;
    release = vtable == NULL ? NULL : (ComReleaseFunction)vtable[2];
    if (release != NULL) release(*object);
    *object = NULL;
}

static uint8_t *quit_screen(void) {
    uint8_t *screen;
    if (game_base == NULL || !readable_memory(
            game_base + RVA_PC_QUIT_SCREEN_GLOBAL, sizeof(screen))) return NULL;
    screen = *(uint8_t **)(game_base + RVA_PC_QUIT_SCREEN_GLOBAL);
    return readable_memory(screen, PC_QUIT_SCREEN_CURSOR_OFFSET + sizeof(DWORD)) ?
        screen : NULL;
}

static BOOL quit_screen_visible(void) {
    uint8_t *screen = quit_screen();
    return screen != NULL && screen[PC_QUIT_SCREEN_VISIBLE_OFFSET] != 0u;
}

static unsigned int multiplayer_cursor_for_screen(const uint8_t *screen) {
    /* +0x1c0 is the shipped optional Quit-to-title row.  Native row count is
     * two or three, so the appended Multiplayer index is exactly that count. */
    return screen != NULL && screen[0x1c0u] != 0u ? 3u : 2u;
}

static void fill_rectangle(
    uint32_t *pixels, int pitch, int left, int top, int right, int bottom,
    uint32_t color
) {
    int y;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (int)OVERLAY_WIDTH) right = OVERLAY_WIDTH;
    if (bottom > (int)OVERLAY_HEIGHT) bottom = OVERLAY_HEIGHT;
    for (y = top; y < bottom; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
        int x;
        for (x = left; x < right; ++x) row[x] = color;
    }
}

static void fill_rounded_rectangle(
    uint32_t *pixels, int pitch, int left, int top, int right, int bottom,
    int radius, uint32_t color
) {
    int y;
    int center_left;
    int center_right;
    int radius_squared;
    if (pixels == NULL || left >= right || top >= bottom || radius <= 0) return;
    if (radius > (bottom-top)/2) radius = (bottom-top)/2;
    center_left = left+radius;
    center_right = right-radius-1;
    radius_squared = radius*radius;
    for (y = top; y < bottom; ++y) {
        uint32_t *row;
        int center_y = y < top+radius ? top+radius : bottom-radius-1;
        int dy = y-center_y;
        int x;
        if (y < 0 || y >= (int)OVERLAY_HEIGHT) continue;
        row = (uint32_t *)((uint8_t *)pixels+y*pitch);
        for (x = left; x < right; ++x) {
            int center_x = x < center_left ? center_left :
                (x > center_right ? center_right : x);
            int dx = x-center_x;
            if (x >= 0 && x < (int)OVERLAY_WIDTH &&
                dx*dx+dy*dy <= radius_squared) row[x] = color;
        }
    }
}

static const uint8_t *glyph_rows(char character) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t colon[7] = {0,4,4,0,4,4,0};
    static const uint8_t dot[7] = {0,0,0,0,0,12,12};
    static const uint8_t slash[7] = {1,2,2,4,8,8,16};
    if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
    if (character >= 'A' && character <= 'Z') return font_letters[character-'A'];
    if (character >= '0' && character <= '9') return font_digits[character-'0'];
    if (character == '-') return dash;
    if (character == ':') return colon;
    if (character == '.') return dot;
    if (character == '/') return slash;
    return blank;
}

static void draw_text(
    uint32_t *pixels, int pitch, int x, int y, const char *text,
    uint32_t color, int scale
) {
    const char *cursor;
    for (cursor = text; cursor != NULL && *cursor != '\0'; ++cursor) {
        const uint8_t *rows = glyph_rows(*cursor);
        int row;
        for (row = 0; row < 7; ++row) {
            int column;
            for (column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4-column))) != 0u) {
                    fill_rectangle(pixels, pitch,
                        x + column*scale, y + row*scale,
                        x + (column+1)*scale, y + (row+1)*scale, color);
                }
            }
        }
        x += 6*scale;
    }
}

static void draw_centered_text(
    uint32_t *pixels, int pitch, int center_x, int y, const char *text,
    uint32_t color, int scale
) {
    int width = text == NULL ? 0 : (int)strlen(text) * 6 * scale;
    draw_text(pixels, pitch, center_x - width/2, y, text, color, scale);
}

static const char *phase_label(SudekiMpLanArenaConnectionPhase phase) {
    switch (phase) {
        case SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING: return "WAITING FOR CLIENT";
        case SUDEKIMP_LAN_ARENA_CONNECTION_JOINING: return "CONNECTING";
        case SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED: return "CONNECTED";
        case SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED: return "CONNECTION REJECTED";
        case SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT: return "CONNECTION TIMED OUT";
        case SUDEKIMP_LAN_ARENA_CONNECTION_ENDED: return "SESSION ENDED";
        default: return "OFFLINE";
    }
}

static const char *failure_label(SudekiMpLanArenaRejectReason failure) {
    switch (failure) {
        case SUDEKIMP_LAN_ARENA_REJECT_VERSION: return "ERROR PROTOCOL VERSION";
        case SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH: return "ERROR GAME BUILD HASH";
        case SUDEKIMP_LAN_ARENA_REJECT_BUILD: return "ERROR MOD BUILD";
        case SUDEKIMP_LAN_ARENA_REJECT_MAP: return "ERROR ARENA MAP";
        case SUDEKIMP_LAN_ARENA_REJECT_ROLE: return "ERROR PLAYER ROLES";
        case SUDEKIMP_LAN_ARENA_REJECT_TOKEN: return "ERROR SESSION TOKEN";
        case SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE: return "ERROR PACKET SEQUENCE";
        case SUDEKIMP_LAN_ARENA_REJECT_MALFORMED: return "ERROR MALFORMED PACKET";
        case SUDEKIMP_LAN_ARENA_REJECT_BUSY: return "ERROR HOST BUSY";
        case SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT: return "ERROR PEER TIMEOUT";
        case SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY: return "ERROR PACKET AUTHORITY";
        default: return NULL;
    }
}

static void reset_edit_edges(void) {
    backspace_was_down = FALSE;
    memset(edit_key_was_down, 0, sizeof(edit_key_was_down));
}

static void capture_page_key_edges(void) {
    page_up_was_down = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    page_down_was_down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    page_select_was_down =
        (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    page_back_was_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
}

static void close_multiplayer_page(void) {
    BOOL was_active = multiplayer_page_active;
    if (multiplayer_page_active) {
        SudekiMpLogWrite(
            "lan_arena_pause_panel event=page state=closed return=quit_menu\r\n");
    }
    multiplayer_page_active = FALSE;
    multiplayer_page_cursor = MULTIPLAYER_PAGE_PRIMARY;
    reset_edit_edges();
    capture_page_key_edges();
    if (was_active && native_quit_show != NULL) {
        native_quit_show(TRUE);
    }
}

static void poll_client_address_edit(void) {
    static const int virtual_keys[12] = {
        '0','1','2','3','4','5','6','7','8','9',VK_OEM_PERIOD,VK_OEM_1
    };
    static const char characters[12] = {
        '0','1','2','3','4','5','6','7','8','9','.',':'
    };
    size_t length = strlen(client_address_edit);
    unsigned int index;
    for (index = 0u; index < 12u; ++index) {
        BOOL down = (GetAsyncKeyState(virtual_keys[index]) & 0x8000) != 0;
        if (down && !edit_key_was_down[index] &&
            (index != 11u || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) &&
            length + 1u < sizeof(client_address_edit)) {
            client_address_edit[length++] = characters[index];
            client_address_edit[length] = '\0';
        }
        edit_key_was_down[index] = down;
    }
    {
        BOOL down = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;
        if (down && !backspace_was_down && length > 0u) {
            client_address_edit[length-1u] = '\0';
        }
        backspace_was_down = down;
    }
}

static const char *primary_action_label(
    const SudekiMpLanArenaSessionStatus *status
) {
    if (status->local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        return status->peer_connected ||
            status->phase == SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING ?
            "END SESSION" : "HOST ARENA";
    }
    return status->peer_connected ||
        status->phase == SUDEKIMP_LAN_ARENA_CONNECTION_JOINING ?
        "LEAVE SESSION" : "JOIN ARENA";
}

static const char *combat_action_label(
    const SudekiMpLanArenaSessionStatus *status
) {
    BOOL enabled = FALSE;
    if (status->local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        return "COMBAT FOLLOWS WORLD";
    }
    if (!status->peer_connected) return "TEST COMBAT NEEDS CLIENT";
    if (!SudekiMpCleanroomEngineCombatMode(&enabled)) {
        return "TEST COMBAT NOT READY";
    }
    return enabled ? "TEST: END COMBAT" : "TEST: START COMBAT";
}

static BOOL run_primary_action(void) {
    SudekiMpLanArenaSessionStatus status;
    BOOL result;
    if (!SudekiMpLanArenaRuntimeGetStatus(&status)) {
        last_action_error = GetLastError();
        return FALSE;
    }
    if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        result = status.peer_connected ||
            status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING ?
            SudekiMpLanArenaRuntimeEndSession() :
            SudekiMpLanArenaRuntimeHostArena();
    } else if (status.peer_connected ||
            status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_JOINING) {
        result = SudekiMpLanArenaRuntimeEndSession();
    } else if (client_address_edit[0] != '\0') {
        result = SudekiMpLanArenaRuntimeJoinEndpoint(client_address_edit);
    } else {
        SetLastError(ERROR_INVALID_ADDRESS);
        result = FALSE;
    }
    last_action_error = result ? ERROR_SUCCESS : GetLastError();
    SudekiMpLogFormat(
        "lan_arena_pause_panel event=action label=%s result=%s error=%lu\r\n",
        primary_action_label(&status), result ? "success" : "rejected",
        (unsigned long)last_action_error);
    return result;
}

static BOOL run_combat_action(void) {
    SudekiMpLanArenaSessionStatus status;
    BOOL enabled = FALSE;
    BOOL result = FALSE;
    if (!SudekiMpLanArenaRuntimeGetStatus(&status)) {
        last_action_error = GetLastError();
    } else if (status.local_role != SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL) {
        last_action_error = ERROR_ACCESS_DENIED;
    } else if (!status.peer_connected) {
        last_action_error = ERROR_NOT_CONNECTED;
    } else if (!SudekiMpCleanroomEngineCombatMode(&enabled)) {
        last_action_error = GetLastError();
        if (last_action_error == ERROR_SUCCESS) {
            last_action_error = ERROR_NOT_READY;
        }
    } else {
        result = SudekiMpCleanroomEngineSetCombatMode(!enabled);
        last_action_error = result ? ERROR_SUCCESS : GetLastError();
    }
    SudekiMpLogFormat(
        "lan_arena_pause_panel event=combat_action result=%s error=%lu "
        "policy=cleanroom_test_only_native_world_flag_replicated\r\n",
        result ? "success" : "rejected",
        (unsigned long)last_action_error);
    return result;
}

static void activate_multiplayer_page_row(void) {
    DWORD now;
    if (!multiplayer_page_active) return;
    if (multiplayer_page_cursor == MULTIPLAYER_PAGE_BACK) {
        close_multiplayer_page();
        return;
    }
    now = GetTickCount();
    if ((int32_t)(now-primary_action_blocked_until_ms) < 0) {
        SudekiMpLogWrite(
            "lan_arena_pause_panel event=action result=suppressed "
            "reason=select_edge_fence\r\n");
        return;
    }
    primary_action_blocked_until_ms = now+500u;
    if (multiplayer_page_cursor == MULTIPLAYER_PAGE_PRIMARY) {
        (void)run_primary_action();
    } else {
        (void)run_combat_action();
    }
}

static void service_sibling_page_keyboard(void) {
    DWORD foreground_process_id = 0u;
    HWND foreground;
    BOOL up;
    BOOL down;
    BOOL select;
    BOOL back;
    BOOL up_rising;
    BOOL down_rising;
    BOOL select_rising;
    BOOL back_rising;
    if (!multiplayer_page_active || quit_screen_visible()) return;
    foreground = GetForegroundWindow();
    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &foreground_process_id);
    }
    up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    select = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    back = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    up_rising = up && !page_up_was_down;
    down_rising = down && !page_down_was_down;
    select_rising = select && !page_select_was_down;
    back_rising = back && !page_back_was_down;
    page_up_was_down = up;
    page_down_was_down = down;
    page_select_was_down = select;
    page_back_was_down = back;
    if (foreground_process_id != GetCurrentProcessId()) return;
    if (back_rising) {
        close_multiplayer_page();
    } else if (up_rising) {
        multiplayer_page_cursor = multiplayer_page_cursor == 0u ?
            MULTIPLAYER_PAGE_ROW_COUNT-1u : multiplayer_page_cursor-1u;
    } else if (down_rising) {
        multiplayer_page_cursor =
            (multiplayer_page_cursor+1u)%MULTIPLAYER_PAGE_ROW_COUNT;
    } else if (select_rising) {
        activate_multiplayer_page_row();
    }
}

static BOOL update_overlay_texture(void *device) {
    void **device_vtable;
    D3DCreateTextureFunction create_texture;
    void **texture_vtable;
    D3DTextureLockRectFunction lock_rect;
    D3DTextureUnlockRectFunction unlock_rect;
    SudekiMpD3DLockedRect locked;
    SudekiMpLanArenaSessionStatus status;
    uint8_t *screen;
    HRESULT result;
    char endpoint[40];
    char error_line[40];
    const char *network_error;

    if (device == NULL || !readable_memory(device, sizeof(void *))) return FALSE;
    if (overlay_texture_device != device) {
        release_com_object(&overlay_texture);
        overlay_texture_device = device;
    }
    device_vtable = *(void ***)device;
    if (!readable_memory(device_vtable,
            (D3D_DEVICE_SET_PIXEL_SHADER_INDEX+1u)*sizeof(void *))) return FALSE;
    create_texture = (D3DCreateTextureFunction)
        device_vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    if (overlay_texture == NULL) {
        if (create_texture == NULL || FAILED(create_texture(device,
                OVERLAY_WIDTH, OVERLAY_HEIGHT, 1u, D3D_USAGE_DYNAMIC,
                D3D_FORMAT_A8R8G8B8, D3D_POOL_DEFAULT,
                &overlay_texture, NULL)) || overlay_texture == NULL) return FALSE;
    }
    texture_vtable = *(void ***)overlay_texture;
    if (!readable_memory(texture_vtable,
            (D3D_TEXTURE_UNLOCK_RECT_INDEX+1u)*sizeof(void *))) return FALSE;
    lock_rect = (D3DTextureLockRectFunction)
        texture_vtable[D3D_TEXTURE_LOCK_RECT_INDEX];
    unlock_rect = (D3DTextureUnlockRectFunction)
        texture_vtable[D3D_TEXTURE_UNLOCK_RECT_INDEX];
    if (lock_rect == NULL || unlock_rect == NULL ||
        FAILED(lock_rect(overlay_texture, 0u, &locked, NULL, 0u)) ||
        locked.bits == NULL || locked.pitch <
            (int)(OVERLAY_WIDTH*sizeof(uint32_t))) return FALSE;
    memset(locked.bits, 0, (size_t)locked.pitch*OVERLAY_HEIGHT);
    if (!multiplayer_page_active) {
        screen = quit_screen();
        if (screen != NULL && *(uint32_t *)(screen+PC_QUIT_SCREEN_STATE_OFFSET) ==
                PC_QUIT_SCREEN_STATE_MAIN) {
            unsigned int multiplayer_cursor =
                multiplayer_cursor_for_screen(screen);
            BOOL selected = *(uint32_t *)(screen+PC_QUIT_SCREEN_CURSOR_OFFSET) ==
                multiplayer_cursor;
            int top = 242 + ((int)multiplayer_cursor - 2) * 40;
            /* Extend the stock dialog with one native-sized fourth row.  The
             * translucent surround prevents the old detached black-label
             * prototype while leaving all three shipped rows untouched. */
            fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
                104, top - 5, 536, top + 31, 16,
                UINT32_C(0xd52a2b31));
            fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
                116, top, 524, top + 26, 13,
                selected ? UINT32_C(0xff31dfe4) : UINT32_C(0xff17191e));
            fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
                119, top + 3, 521, top + 23, 10,
                selected ? UINT32_C(0xff205d68) : UINT32_C(0xff25252a));
            draw_centered_text((uint32_t *)locked.bits, locked.pitch,
                320, top + 6, "MULTIPLAYER", selected ? UINT32_C(0xffffffff) :
                UINT32_C(0xffffd778), 2);
        }
    } else if (SudekiMpLanArenaRuntimeGetStatus(&status)) {
        char address[32] = {0};
        unsigned int port = 0u;
        if (SudekiMpLanArenaSessionGetDisplayEndpoint(
                address, sizeof(address), &port)) {
            if (strlen(address) < sizeof(last_endpoint_address)) {
                strcpy(last_endpoint_address, address);
                last_endpoint_port = port;
            }
        } else if (last_endpoint_address[0] != '\0') {
            strcpy(address, last_endpoint_address);
            port = last_endpoint_port;
        }
        if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
            !status.peer_connected &&
            status.phase != SUDEKIMP_LAN_ARENA_CONNECTION_JOINING) {
            if (client_address_edit[0] == '\0' && address[0] != '\0') {
                strncpy(client_address_edit, address,
                    sizeof(client_address_edit)-1u);
                client_address_edit[sizeof(client_address_edit)-1u] = '\0';
            }
            poll_client_address_edit();
        } else {
            reset_edit_edges();
        }
        snprintf(endpoint, sizeof(endpoint), "%s:%u",
            address[0] == '\0' ? "0.0.0.0" : address, port);
        fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
            92, 60, 548, 432, 22, UINT32_C(0xf210141d));
        fill_rectangle((uint32_t *)locked.bits, locked.pitch,
            112, 60, 528, 65, UINT32_C(0xff38e8ed));
        fill_rectangle((uint32_t *)locked.bits, locked.pitch,
            112, 427, 528, 432, UINT32_C(0xff735a2d));
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 78, "MULTIPLAYER", UINT32_C(0xffffdc72), 3);
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 116, status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
            "HOST ARENA - TAL" : "JOIN ARENA - AILISH",
            UINT32_C(0xffcfe8ff), 2);
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 146, endpoint, UINT32_C(0xffffffff), 2);
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 172, phase_label(status.phase),
            status.peer_connected ? UINT32_C(0xff80ff80) :
            UINT32_C(0xffffff80), 2);
        if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
            !status.peer_connected) {
            snprintf(error_line, sizeof(error_line), "ADDRESS %s",
                client_address_edit[0] == '\0' ? "TYPE IP" :
                client_address_edit);
            draw_centered_text((uint32_t *)locked.bits, locked.pitch,
                320, 202, error_line, UINT32_C(0xffcfe8ff), 2);
        }
        network_error = failure_label(status.failure);
        if (network_error != NULL) {
            draw_centered_text((uint32_t *)locked.bits, locked.pitch,
                320, 230, network_error, UINT32_C(0xffff8080), 1);
        } else if (last_action_error != ERROR_SUCCESS) {
            snprintf(error_line, sizeof(error_line), "LOCAL ERROR %lu",
                (unsigned long)last_action_error);
            draw_centered_text((uint32_t *)locked.bits, locked.pitch,
                320, 230, error_line, UINT32_C(0xffff8080), 1);
        }
        fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
            145, 260, 495, 294, 16, multiplayer_page_cursor ==
            MULTIPLAYER_PAGE_PRIMARY ? UINT32_C(0xff236875) :
            UINT32_C(0xff262a32));
        fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
            145, 304, 495, 338, 16, multiplayer_page_cursor ==
            MULTIPLAYER_PAGE_COMBAT ? UINT32_C(0xff236875) :
            UINT32_C(0xff262a32));
        fill_rounded_rectangle((uint32_t *)locked.bits, locked.pitch,
            145, 348, 495, 382, 16, multiplayer_page_cursor ==
            MULTIPLAYER_PAGE_BACK ? UINT32_C(0xff236875) :
            UINT32_C(0xff262a32));
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 268, primary_action_label(&status), UINT32_C(0xffffffff), 2);
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 312, combat_action_label(&status), UINT32_C(0xffffffff), 2);
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 356, "BACK", UINT32_C(0xffffffff), 2);
        draw_centered_text((uint32_t *)locked.bits, locked.pitch,
            320, 404,
            status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
                "ENTER SELECT  F8 TEST COMBAT  ESC BACK" :
                "ENTER SELECT  ESC BACK",
            UINT32_C(0xffb9c0cc), 1);
    }
    result = unlock_rect(overlay_texture, 0u);
    return SUCCEEDED(result);
}

static BOOL draw_overlay(void) {
    void *device;
    void **vtable;
    D3DGetRenderTargetFunction get_render_target;
    D3DCreateStateBlockFunction create_state_block;
    D3DSetRenderStateFunction set_render_state;
    D3DSetTextureFunction set_texture;
    D3DSetTextureStageStateFunction set_texture_stage_state;
    D3DSetSamplerStateFunction set_sampler_state;
    D3DSetShaderFunction set_vertex_shader;
    D3DSetShaderFunction set_pixel_shader;
    D3DSetFvfFunction set_fvf;
    D3DDrawPrimitiveUpFunction draw_primitive_up;
    void *target = NULL;
    void *state_block = NULL;
    void **surface_vtable;
    void **state_vtable;
    D3DSurfaceGetDescFunction get_description;
    D3DStateBlockApplyFunction apply_state;
    SudekiMpD3DSurfaceDesc description;
    SudekiMpMenuVertex vertices[4];
    HRESULT result = E_FAIL;
    HRESULT restore_result;
    if (d3d_device_global == NULL ||
        !readable_memory(d3d_device_global, sizeof(*d3d_device_global)) ||
        (device = *d3d_device_global) == NULL ||
        !update_overlay_texture(device)) return FALSE;
    vtable = *(void ***)device;
    get_render_target = (D3DGetRenderTargetFunction)
        vtable[D3D_DEVICE_GET_RENDER_TARGET_INDEX];
    create_state_block = (D3DCreateStateBlockFunction)
        vtable[D3D_DEVICE_CREATE_STATE_BLOCK_INDEX];
    set_render_state = (D3DSetRenderStateFunction)
        vtable[D3D_DEVICE_SET_RENDER_STATE_INDEX];
    set_texture = (D3DSetTextureFunction)vtable[D3D_DEVICE_SET_TEXTURE_INDEX];
    set_texture_stage_state = (D3DSetTextureStageStateFunction)
        vtable[D3D_DEVICE_SET_TEXTURE_STAGE_STATE_INDEX];
    set_sampler_state = (D3DSetSamplerStateFunction)
        vtable[D3D_DEVICE_SET_SAMPLER_STATE_INDEX];
    draw_primitive_up = (D3DDrawPrimitiveUpFunction)
        vtable[D3D_DEVICE_DRAW_PRIMITIVE_UP_INDEX];
    set_fvf = (D3DSetFvfFunction)vtable[D3D_DEVICE_SET_FVF_INDEX];
    set_vertex_shader = (D3DSetShaderFunction)
        vtable[D3D_DEVICE_SET_VERTEX_SHADER_INDEX];
    set_pixel_shader = (D3DSetShaderFunction)
        vtable[D3D_DEVICE_SET_PIXEL_SHADER_INDEX];
    if (get_render_target == NULL || create_state_block == NULL ||
        set_render_state == NULL || set_texture == NULL ||
        set_texture_stage_state == NULL || set_sampler_state == NULL ||
        draw_primitive_up == NULL || set_fvf == NULL ||
        set_vertex_shader == NULL || set_pixel_shader == NULL ||
        FAILED(get_render_target(device, 0u, &target)) || target == NULL) {
        return FALSE;
    }
    surface_vtable = *(void ***)target;
    get_description = surface_vtable == NULL ? NULL :
        (D3DSurfaceGetDescFunction)surface_vtable[D3D_SURFACE_GET_DESC_INDEX];
    if (get_description == NULL ||
        FAILED(get_description(target, &description))) {
        release_com_object(&target);
        return FALSE;
    }
    release_com_object(&target);
    if (FAILED(create_state_block(device, D3D_STATE_BLOCK_ALL, &state_block)) ||
        state_block == NULL) return FALSE;
    state_vtable = *(void ***)state_block;
    apply_state = state_vtable == NULL ? NULL :
        (D3DStateBlockApplyFunction)state_vtable[D3D_STATE_BLOCK_APPLY_INDEX];
    if (apply_state == NULL) {
        release_com_object(&state_block);
        return FALSE;
    }
    vertices[0] = (SudekiMpMenuVertex){-0.5f,-0.5f,0,1,0,0};
    vertices[1] = (SudekiMpMenuVertex){(float)description.width-0.5f,
        -0.5f,0,1,1,0};
    vertices[2] = (SudekiMpMenuVertex){-0.5f,
        (float)description.height-0.5f,0,1,0,1};
    vertices[3] = (SudekiMpMenuVertex){(float)description.width-0.5f,
        (float)description.height-0.5f,0,1,1,1};
#define APPLY(call) do { result = (call); if (FAILED(result)) goto restore; } while (0)
    APPLY(set_vertex_shader(device, NULL));
    APPLY(set_pixel_shader(device, NULL));
    APPLY(set_fvf(device, D3D_FVF_XYZRHW_TEX1));
    APPLY(set_render_state(device, D3D_RENDER_STATE_Z_ENABLE, FALSE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_Z_WRITE_ENABLE, FALSE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_ALPHA_TEST_ENABLE, FALSE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_CULL_MODE, D3D_CULL_NONE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_ALPHA_BLEND_ENABLE, TRUE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_SRC_BLEND, D3D_BLEND_SRC_ALPHA));
    APPLY(set_render_state(device, D3D_RENDER_STATE_DEST_BLEND,
        D3D_BLEND_INVERSE_SRC_ALPHA));
    APPLY(set_render_state(device, D3D_RENDER_STATE_FOG_ENABLE, FALSE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_LIGHTING, FALSE));
    APPLY(set_render_state(device, D3D_RENDER_STATE_SCISSOR_TEST_ENABLE, FALSE));
    APPLY(set_texture_stage_state(device, 0u,
        D3D_TEXTURE_STAGE_COLOR_OPERATION, D3D_TEXTURE_OPERATION_SELECT_ARGUMENT_ONE));
    APPLY(set_texture_stage_state(device, 0u,
        D3D_TEXTURE_STAGE_COLOR_ARGUMENT_ONE, D3D_TEXTURE_ARGUMENT_TEXTURE));
    APPLY(set_texture_stage_state(device, 0u,
        D3D_TEXTURE_STAGE_ALPHA_OPERATION, D3D_TEXTURE_OPERATION_SELECT_ARGUMENT_ONE));
    APPLY(set_texture_stage_state(device, 0u,
        D3D_TEXTURE_STAGE_ALPHA_ARGUMENT_ONE, D3D_TEXTURE_ARGUMENT_TEXTURE));
    APPLY(set_texture_stage_state(device, 1u,
        D3D_TEXTURE_STAGE_COLOR_OPERATION, D3D_TEXTURE_OPERATION_DISABLE));
    APPLY(set_sampler_state(device, 0u, D3D_SAMPLER_MAG_FILTER,
        D3D_TEXTURE_FILTER_POINT));
    APPLY(set_sampler_state(device, 0u, D3D_SAMPLER_MIN_FILTER,
        D3D_TEXTURE_FILTER_POINT));
    APPLY(set_texture(device, 0u, overlay_texture));
    result = draw_primitive_up(device, D3D_PRIMITIVE_TRIANGLE_STRIP,
        2u, vertices, sizeof(vertices[0]));
restore:
    restore_result = apply_state(state_block);
    release_com_object(&state_block);
#undef APPLY
    return SUCCEEDED(result) && SUCCEEDED(restore_result);
}

__attribute__((noinline, used))
static void render_panel(void) {
    if (!multiplayer_page_active && !quit_screen_visible()) return;
    service_sibling_page_keyboard();
    if (!draw_overlay()) {
        if (!render_failure_logged) {
            render_failure_logged = TRUE;
            SudekiMpLogWrite(
                "lan_arena_pause_panel event=render result=rejected "
                "reason=overlay_unavailable\r\n");
        }
    } else {
        render_failure_logged = FALSE;
    }
}

__attribute__((noinline, used))
static void handle_select(uint8_t *screen) {
    action_handled = FALSE;
    if (screen == NULL || screen != quit_screen()) return;
    if (multiplayer_page_active) {
        action_handled = TRUE;
        activate_multiplayer_page_row();
        return;
    }
    if (*(uint32_t *)(screen+PC_QUIT_SCREEN_STATE_OFFSET) ==
            PC_QUIT_SCREEN_STATE_MAIN &&
        *(uint32_t *)(screen+PC_QUIT_SCREEN_CURSOR_OFFSET) ==
            multiplayer_cursor_for_screen(screen)) {
        multiplayer_page_active = TRUE;
        multiplayer_page_cursor = MULTIPLAYER_PAGE_PRIMARY;
        primary_action_blocked_until_ms = GetTickCount()+250u;
        capture_page_key_edges();
        action_handled = TRUE;
        parent_close_requested = TRUE;
        SudekiMpLogWrite(
            "lan_arena_pause_panel event=page state=open source=quit_menu_multiplayer\r\n");
    }
}

__attribute__((noinline, used))
static void handle_back(uint8_t *screen) {
    action_handled = FALSE;
    if (multiplayer_page_active && screen == quit_screen()) {
        close_multiplayer_page();
        action_handled = TRUE;
    }
}

__attribute__((noinline, used))
static void handle_navigation(
    uint8_t *screen, unsigned int command, unsigned int pressed
) {
    action_handled = FALSE;
    if (screen != quit_screen()) return;
    if (!multiplayer_page_active) {
        unsigned int *cursor;
        unsigned int multiplayer_cursor;
        if (*(uint32_t *)(screen+PC_QUIT_SCREEN_STATE_OFFSET) !=
                PC_QUIT_SCREEN_STATE_MAIN ||
            (command != 0x1eu && command != 0x1fu)) return;
        action_handled = TRUE;
        if (pressed == 0u) return;
        cursor = (unsigned int *)(screen+PC_QUIT_SCREEN_CURSOR_OFFSET);
        multiplayer_cursor = multiplayer_cursor_for_screen(screen);
        if (command == 0x1eu) {
            if (*cursor > 0u) --*cursor;
        } else if (*cursor < multiplayer_cursor) {
            ++*cursor;
        }
        return;
    }
    action_handled = TRUE;
    if (pressed == 0u) return;
    if (command == 0x1eu) {
        multiplayer_page_cursor = multiplayer_page_cursor == 0u ?
            MULTIPLAYER_PAGE_ROW_COUNT-1u : multiplayer_page_cursor-1u;
    } else if (command == 0x1fu) {
        multiplayer_page_cursor =
            (multiplayer_page_cursor+1u)%MULTIPLAYER_PAGE_ROW_COUNT;
    }
}

__attribute__((naked, noinline, used))
static void quit_select_entry(void) {
    __asm__ volatile(
        "pushfl\n\tpushal\n\tpushl %esi\n\tcall _handle_select\n\taddl $4, %esp\n\t"
        "popal\n\tpopfl\n\tcmpb $0, _action_handled\n\tjne 1f\n\t"
        "jmp *_original_quit_select\n\t1:\n\t"
        "cmpb $0, _parent_close_requested\n\tje 2f\n\t"
        "movb $0, _parent_close_requested\n\t"
        "call *_original_quit_back\n\t"
        /* Keep the native event dispatcher in its main state while hidden;
         * select/navigation/back callsites therefore remain available to the
         * unpaused sibling page. */
        "movl $1, 0x10(%esi)\n\t"
        "2: ret\n\t");
}

__attribute__((naked, noinline, used))
static void quit_back_entry(void) {
    __asm__ volatile(
        "pushfl\n\tpushal\n\tpushl %esi\n\tcall _handle_back\n\taddl $4, %esp\n\t"
        "popal\n\tpopfl\n\tcmpb $0, _action_handled\n\tjne 1f\n\t"
        "jmp *_original_quit_back\n\t1: ret\n\t");
}

__attribute__((naked, noinline, used))
static void quit_navigate_entry(void) {
    __asm__ volatile(
        "pushfl\n\tpushal\n\tmovzbl %dl, %ecx\n\tpushl %ecx\n\t"
        "pushl %eax\n\tpushl %edi\n\tcall _handle_navigation\n\taddl $12, %esp\n\t"
        "popal\n\tpopfl\n\tcmpb $0, _action_handled\n\tjne 1f\n\t"
        "jmp *_original_quit_navigate\n\t1: ret\n\t");
}

__attribute__((naked, noinline, used))
static void quit_render_entry(void) {
    __asm__ volatile(
        "cmpb $0, _multiplayer_page_active\n\tjne 1f\n\t"
        "call *_original_quit_render\n\t1: pushfl\n\tpushal\n\t"
        "call _render_panel\n\tpopal\n\tpopfl\n\tret\n\t");
}

BOOL SudekiMpInstallLanArenaPausePanel(HMODULE game_module) {
    DWORD error;
    if (game_module == NULL || game_base != NULL ||
        original_quit_render != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    game_base = (uint8_t *)game_module;
    if (!quit_show_signature_matches(game_base) ||
        memcmp(game_base+RVA_PC_QUIT_SCREEN_RENDER,
            expected_quit_render_entry,
            sizeof(expected_quit_render_entry)) != 0) {
        game_base = NULL;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    original_quit_render = (QuitScreenRenderFunction)(
        game_base+RVA_PC_QUIT_SCREEN_RENDER);
    original_quit_select = (QuitScreenActionFunction)(
        game_base+RVA_PC_QUIT_SCREEN_SELECT);
    original_quit_back = (QuitScreenActionFunction)(
        game_base+RVA_PC_QUIT_SCREEN_BACK);
    original_quit_navigate = (QuitScreenActionFunction)(
        game_base+RVA_PC_QUIT_SCREEN_NAVIGATE);
    if (!SudekiMpInstallRelativeCallHook(&quit_render_hook,
            game_base+RVA_PC_QUIT_SCREEN_RENDER_CALL,
            original_quit_render, quit_render_entry) ||
        !SudekiMpInstallRelativeCallHook(&quit_select_hook,
            game_base+RVA_PC_QUIT_SCREEN_SELECT_CALL,
            original_quit_select, quit_select_entry) ||
        !SudekiMpInstallRelativeCallHook(&quit_back_hook,
            game_base+RVA_PC_QUIT_SCREEN_BACK_CALL,
            original_quit_back, quit_back_entry) ||
        !SudekiMpInstallRelativeCallHook(&quit_analog_navigate_hook,
            game_base+RVA_PC_QUIT_SCREEN_ANALOG_NAVIGATE_CALL,
            original_quit_navigate, quit_navigate_entry) ||
        !SudekiMpInstallRelativeCallHook(&quit_navigate_hook,
            game_base+RVA_PC_QUIT_SCREEN_NAVIGATE_CALL,
            original_quit_navigate, quit_navigate_entry)) {
        error = GetLastError();
        SudekiMpUninstallLanArenaPausePanel();
        SetLastError(error);
        return FALSE;
    }
    d3d_device_global = (void **)(game_base+RVA_D3D_DEVICE_GLOBAL);
    native_quit_show = (QuitScreenShowFunction)(game_base+RVA_PC_QUIT_SCREEN_SHOW);
    multiplayer_page_active = FALSE;
    parent_close_requested = FALSE;
    multiplayer_page_cursor = MULTIPLAYER_PAGE_PRIMARY;
    action_handled = FALSE;
    client_address_edit[0] = '\0';
    last_endpoint_address[0] = '\0';
    last_endpoint_port = 0u;
    last_action_error = ERROR_SUCCESS;
    render_failure_logged = FALSE;
    primary_action_blocked_until_ms = 0u;
    reset_edit_edges();
    capture_page_key_edges();
    SudekiMpLogWrite(
        "lan_arena_pause_panel event=install layer=native_pc_quit_screen "
        "option=multiplayer native_rows=preserved appended_row=true "
        "sibling_page=true sibling_pause=disabled local_input=owner_gated "
        "native_quick_menu=unchanged\r\n");
    return TRUE;
}

BOOL SudekiMpLanArenaPausePanelActive(void) {
    return multiplayer_page_active;
}

void SudekiMpUninstallLanArenaPausePanel(void) {
    SudekiMpRestoreRelativeCallHook(&quit_navigate_hook);
    SudekiMpRestoreRelativeCallHook(&quit_analog_navigate_hook);
    SudekiMpRestoreRelativeCallHook(&quit_back_hook);
    SudekiMpRestoreRelativeCallHook(&quit_select_hook);
    SudekiMpRestoreRelativeCallHook(&quit_render_hook);
    release_com_object(&overlay_texture);
    overlay_texture_device = NULL;
    d3d_device_global = NULL;
    original_quit_render = NULL;
    original_quit_select = NULL;
    original_quit_back = NULL;
    original_quit_navigate = NULL;
    native_quit_show = NULL;
    game_base = NULL;
    multiplayer_page_active = FALSE;
    parent_close_requested = FALSE;
    multiplayer_page_cursor = MULTIPLAYER_PAGE_PRIMARY;
    action_handled = FALSE;
    client_address_edit[0] = '\0';
    last_endpoint_address[0] = '\0';
    last_endpoint_port = 0u;
    last_action_error = ERROR_SUCCESS;
    render_failure_logged = FALSE;
    primary_action_blocked_until_ms = 0u;
    reset_edit_edges();
    capture_page_key_edges();
}
