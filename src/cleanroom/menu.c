#include "cleanroom/menu.h"

#include "cleanroom/audio.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/split_screen_render.h"

#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Cleanroom menu requires the 32-bit Windows target"
#endif

typedef void (__attribute__((thiscall)) *ControllerUpdateFunction)(
    void *controller,
    void *update_data
);
typedef void (*FrameEndFunction)(void);

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
    void *device,
    UINT width,
    UINT height,
    UINT levels,
    DWORD usage,
    int format,
    int pool,
    void **texture,
    HANDLE *shared_handle
);
typedef HRESULT (__stdcall *D3DGetRenderTargetFunction)(
    void *device,
    DWORD index,
    void **surface
);
typedef HRESULT (__stdcall *D3DSurfaceGetDescFunction)(
    void *surface,
    SudekiMpD3DSurfaceDesc *description
);
typedef HRESULT (__stdcall *D3DTextureLockRectFunction)(
    void *texture,
    UINT level,
    SudekiMpD3DLockedRect *locked,
    const RECT *rectangle,
    DWORD flags
);
typedef HRESULT (__stdcall *D3DTextureUnlockRectFunction)(
    void *texture,
    UINT level
);
typedef HRESULT (__stdcall *D3DCreateStateBlockFunction)(
    void *device,
    int type,
    void **state_block
);
typedef HRESULT (__stdcall *D3DSetRenderStateFunction)(
    void *device,
    int state,
    DWORD value
);
typedef HRESULT (__stdcall *D3DSetTextureFunction)(
    void *device,
    DWORD stage,
    void *texture
);
typedef HRESULT (__stdcall *D3DSetTextureStageStateFunction)(
    void *device,
    DWORD stage,
    int type,
    DWORD value
);
typedef HRESULT (__stdcall *D3DSetSamplerStateFunction)(
    void *device,
    DWORD sampler,
    int type,
    DWORD value
);
typedef HRESULT (__stdcall *D3DSetShaderFunction)(void *device, void *shader);
typedef HRESULT (__stdcall *D3DSetFvfFunction)(void *device, DWORD fvf);
typedef HRESULT (__stdcall *D3DDrawPrimitiveUpFunction)(
    void *device,
    int primitive_type,
    UINT primitive_count,
    const void *vertex_data,
    UINT vertex_stride
);
typedef HRESULT (__stdcall *D3DStateBlockApplyFunction)(void *state_block);
typedef ULONG (__stdcall *ComReleaseFunction)(void *object);

typedef enum SudekiMpPendingAction {
    SUDEKIMP_PENDING_NONE = 0,
    SUDEKIMP_PENDING_SPAWN = 1,
    SUDEKIMP_PENDING_REMOVE = 2
} SudekiMpPendingAction;

enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu,
    RVA_D3D_DEVICE_GLOBAL = 0x003c31dcu,
    MENU_ACTOR_COUNT = 4u,
    MENU_DUMMY_INDEX = 4u,
    MENU_COMBAT_INDEX = 5u,
    MENU_CAMERA_INDEX = 6u,
    MENU_MULTIPLAYER_INDEX = 7u,
    MENU_INFINITE_SP_INDEX = 8u,
    MENU_INFINITE_SPIRIT_INDEX = 9u,
    MENU_CLOSE_INDEX = 10u,
    MENU_ITEM_COUNT = 11u,
    MENU_TEXTURE_WIDTH = 640u,
    MENU_TEXTURE_HEIGHT = 480u,
    PLAYER_TWO_BADGE_WIDTH = 168u,
    PLAYER_TWO_BADGE_HEIGHT = 42u,
    MENU_TIMEOUT_MS = 6000u,
    MENU_STATUS_INTERVAL_MS = 150u,
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

static SudekiMpPointerHook controller_update_hook;
static SudekiMpRelativeCallHook frame_end_hook;
static ControllerUpdateFunction original_controller_update;
static FrameEndFunction original_frame_end;
static uint8_t *game_base;
static void **d3d_device_global;
static void *menu_texture;
static void *menu_texture_device;
static void *player_two_badge_texture;
static void *player_two_badge_device;
static BOOL player_two_badge_dirty;
static UINT menu_toggle_key;
static BOOL menu_open;
static BOOL menu_texture_dirty;
static unsigned int selected_item;
static BOOL key_was_down[6];
static BOOL item_present[MENU_DUMMY_INDEX + 1u];
static SudekiMpPendingAction pending_actions[MENU_DUMMY_INDEX + 1u];
static DWORD pending_started[MENU_DUMMY_INDEX + 1u];
static DWORD failed_until[MENU_DUMMY_INDEX + 1u];
static float cleanroom_anchor[3];
static BOOL cleanroom_anchor_valid;
static BOOL overlay_failure_logged;
static BOOL player_two_badge_failure_logged;
static BOOL cleanroom_world_ready;
static BOOL combat_mode;
static BOOL combat_mode_valid;
static BOOL first_person_mode;
static BOOL first_person_mode_valid;
static BOOL infinite_sp;
static BOOL infinite_sp_valid;
static BOOL infinite_spirit;
static BOOL infinite_spirit_valid;
static BOOL integrated_multiplayer_mode;
static BOOL multiplayer_requested;
static BOOL multiplayer_active;
static BOOL multiplayer_input_ready;
static DWORD last_status_update;

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static BOOL command_line_is_cleanroom(void) {
    const char *command_line = GetCommandLineA();

    return command_line != NULL &&
        strstr(command_line, "-Level testroom") != NULL &&
        strstr(command_line, "-DT 1") != NULL &&
        strstr(command_line, "-Ailish 1") != NULL;
}

static BOOL owns_foreground(void) {
    HWND foreground = GetForegroundWindow();
    DWORD process_id = 0u;

    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &process_id);
    }
    return process_id == GetCurrentProcessId();
}

static void release_com_object(void **object) {
    void **vtable;
    ComReleaseFunction release;

    if (object == NULL || *object == NULL) {
        return;
    }
    vtable = *(void ***)*object;
    release = vtable == NULL ? NULL : (ComReleaseFunction)vtable[2];
    if (release != NULL) {
        release(*object);
    }
    *object = NULL;
}

static BOOL current_item_present(unsigned int index) {
    if (index < MENU_ACTOR_COUNT) {
        return SudekiMpCleanroomEngineActorPresent(
            (SudekiMpCleanroomActor)index
        );
    }
    if (index == MENU_DUMMY_INDEX) {
        return SudekiMpCleanroomEngineDummyPresent();
    }
    return FALSE;
}

static void update_action_status(void) {
    DWORD now = GetTickCount();
    unsigned int index;
    BOOL world_ready = SudekiMpCleanroomEngineWorldReady();
    BOOL mode;

    if (world_ready != cleanroom_world_ready) {
        cleanroom_world_ready = world_ready;
        SudekiMpLogFormat(
            "cleanroom_menu event=world state=%s\r\n",
            world_ready ? "ready" : "unavailable"
        );
        menu_texture_dirty = TRUE;
    }
    if (!world_ready) {
        if (menu_open) {
            SudekiMpLogWrite(
                "cleanroom_menu event=visibility state=closed reason=world_unavailable\r\n"
            );
        }
        menu_open = FALSE;
        combat_mode_valid = FALSE;
        first_person_mode_valid = FALSE;
        infinite_sp_valid = FALSE;
        infinite_spirit_valid = FALSE;
        multiplayer_active = FALSE;
        multiplayer_input_ready = FALSE;
        last_status_update = 0u;
        return;
    }
    if (last_status_update != 0u &&
        (DWORD)(now - last_status_update) < MENU_STATUS_INTERVAL_MS) {
        return;
    }
    last_status_update = now;
    SudekiMpCleanroomEngineMaintainResources();

    if (SudekiMpCleanroomEngineCombatMode(&mode)) {
        if (!combat_mode_valid || combat_mode != mode) {
            combat_mode = mode;
            combat_mode_valid = TRUE;
            menu_texture_dirty = TRUE;
        }
    } else if (combat_mode_valid) {
        combat_mode_valid = FALSE;
        menu_texture_dirty = TRUE;
    }
    if (SudekiMpCleanroomEngineFirstPersonMode(&mode)) {
        if (!first_person_mode_valid || first_person_mode != mode) {
            first_person_mode = mode;
            first_person_mode_valid = TRUE;
            menu_texture_dirty = TRUE;
        }
    } else if (first_person_mode_valid) {
        first_person_mode_valid = FALSE;
        menu_texture_dirty = TRUE;
    }
    if (SudekiMpCleanroomEngineInfiniteSp(&mode)) {
        if (!infinite_sp_valid || infinite_sp != mode) {
            infinite_sp = mode;
            infinite_sp_valid = TRUE;
            menu_texture_dirty = TRUE;
        }
    } else if (infinite_sp_valid) {
        infinite_sp_valid = FALSE;
        menu_texture_dirty = TRUE;
    }
    if (SudekiMpCleanroomEngineInfiniteSpirit(&mode)) {
        if (!infinite_spirit_valid || infinite_spirit != mode) {
            infinite_spirit = mode;
            infinite_spirit_valid = TRUE;
            menu_texture_dirty = TRUE;
        }
    } else if (infinite_spirit_valid) {
        infinite_spirit_valid = FALSE;
        menu_texture_dirty = TRUE;
    }
    if (integrated_multiplayer_mode) {
        BOOL requested = SudekiMpControlSeparationPlayerTwoRequested();
        BOOL active = SudekiMpControlSeparationPlayerTwoActive();
        BOOL input_ready = SudekiMpControlSeparationInputReady();

        if (multiplayer_requested != requested ||
            multiplayer_active != active ||
            multiplayer_input_ready != input_ready) {
            multiplayer_requested = requested;
            multiplayer_active = active;
            multiplayer_input_ready = input_ready;
            menu_texture_dirty = TRUE;
            player_two_badge_dirty = TRUE;
        }
    }

    if (!cleanroom_anchor_valid &&
        SudekiMpCleanroomEngineActorPosition(
            SUDEKIMP_CLEANROOM_AILISH,
            cleanroom_anchor)) {
        cleanroom_anchor_valid = TRUE;
        SudekiMpLogFormat(
            "cleanroom_menu event=anchor status=captured "
            "position_bits=%08lx,%08lx,%08lx policy=ailish_initial_position\r\n",
            (unsigned long)float_bits(cleanroom_anchor[0]),
            (unsigned long)float_bits(cleanroom_anchor[1]),
            (unsigned long)float_bits(cleanroom_anchor[2])
        );
    }

    for (index = 0u; index <= MENU_DUMMY_INDEX; ++index) {
        BOOL present = current_item_present(index);
        if (present != item_present[index]) {
            item_present[index] = present;
            menu_texture_dirty = TRUE;
        }
        if (failed_until[index] != 0u &&
            (LONG)(now - failed_until[index]) >= 0) {
            failed_until[index] = 0u;
            menu_texture_dirty = TRUE;
        }
        if (pending_actions[index] == SUDEKIMP_PENDING_NONE) {
            continue;
        }
        if ((pending_actions[index] == SUDEKIMP_PENDING_SPAWN && present) ||
            (pending_actions[index] == SUDEKIMP_PENDING_REMOVE && !present)) {
            BOOL removed = pending_actions[index] == SUDEKIMP_PENDING_REMOVE;
            SudekiMpLogFormat(
                "cleanroom_menu event=%s item=%s status=confirmed\r\n",
                removed ? "despawn" : "spawn",
                index < MENU_ACTOR_COUNT ?
                    SudekiMpCleanroomActorLabel(
                        (SudekiMpCleanroomActor)index) :
                    "TrainingDummy"
            );
            pending_actions[index] = SUDEKIMP_PENDING_NONE;
            failed_until[index] = 0u;
            menu_texture_dirty = TRUE;
            if (removed) {
                SudekiMpCleanroomPlayDespawnCue();
            }
        } else if ((DWORD)(now - pending_started[index]) >
            MENU_TIMEOUT_MS) {
            SudekiMpLogFormat(
                "cleanroom_menu event=%s item=%s status=timeout\r\n",
                pending_actions[index] == SUDEKIMP_PENDING_REMOVE ?
                    "despawn" : "spawn",
                index < MENU_ACTOR_COUNT ?
                    SudekiMpCleanroomActorLabel(
                        (SudekiMpCleanroomActor)index) :
                    "TrainingDummy"
            );
            pending_actions[index] = SUDEKIMP_PENDING_NONE;
            failed_until[index] = now + 2500u;
            menu_texture_dirty = TRUE;
        }
    }
}

static void begin_pending(unsigned int index, SudekiMpPendingAction action) {
    pending_actions[index] = action;
    pending_started[index] = GetTickCount();
    failed_until[index] = 0u;
    menu_texture_dirty = TRUE;
}

static void activate_selected_item(void) {
    float position[3];
    BOOL accepted = FALSE;
    BOOL mode;

    if (selected_item == MENU_CLOSE_INDEX) {
        menu_open = FALSE;
        return;
    }
    if (selected_item == MENU_COMBAT_INDEX) {
        if (SudekiMpCleanroomEngineCombatMode(&mode)) {
            accepted = SudekiMpCleanroomEngineSetCombatMode(!mode);
        }
        combat_mode_valid = SudekiMpCleanroomEngineCombatMode(&combat_mode);
        menu_texture_dirty = TRUE;
        if (!accepted) {
            SudekiMpLogWrite(
                "cleanroom_menu event=combat_mode status=rejected\r\n"
            );
        }
        return;
    }
    if (selected_item == MENU_CAMERA_INDEX) {
        if (SudekiMpCleanroomEngineFirstPersonMode(&mode)) {
            accepted = SudekiMpCleanroomEngineSetFirstPersonMode(!mode);
        }
        first_person_mode_valid =
            SudekiMpCleanroomEngineFirstPersonMode(&first_person_mode);
        menu_texture_dirty = TRUE;
        if (!accepted) {
            SudekiMpLogWrite(
                "cleanroom_menu event=camera_mode status=rejected\r\n"
            );
        }
        return;
    }
    if (selected_item == MENU_INFINITE_SP_INDEX) {
        if (SudekiMpCleanroomEngineInfiniteSp(&mode)) {
            accepted = SudekiMpCleanroomEngineSetInfiniteSp(!mode);
        }
        infinite_sp_valid =
            SudekiMpCleanroomEngineInfiniteSp(&infinite_sp);
        menu_texture_dirty = TRUE;
        if (!accepted) {
            SudekiMpLogWrite(
                "cleanroom_menu event=infinite_sp status=rejected\r\n"
            );
        }
        return;
    }
    if (selected_item == MENU_MULTIPLAYER_INDEX) {
        BOOL desired = !multiplayer_requested;

        accepted = integrated_multiplayer_mode &&
            SudekiMpControlSeparationRequestPlayerTwo(desired) &&
            SudekiMpSplitScreenSetRuntimeEnabled(desired);
        if (accepted) {
            multiplayer_requested = desired;
            SudekiMpLogFormat(
                "cleanroom_menu event=multiplayer_toggle state=%s "
                "input=razer_bridge\r\n",
                desired ? "enabled" : "disabled"
            );
        } else {
            SudekiMpLogWrite(
                "cleanroom_menu event=multiplayer_toggle status=rejected\r\n"
            );
        }
        menu_texture_dirty = TRUE;
        player_two_badge_dirty = TRUE;
        return;
    }
    if (selected_item == MENU_INFINITE_SPIRIT_INDEX) {
        if (SudekiMpCleanroomEngineInfiniteSpirit(&mode)) {
            accepted = SudekiMpCleanroomEngineSetInfiniteSpirit(!mode);
        }
        infinite_spirit_valid =
            SudekiMpCleanroomEngineInfiniteSpirit(&infinite_spirit);
        menu_texture_dirty = TRUE;
        if (!accepted) {
            SudekiMpLogWrite(
                "cleanroom_menu event=infinite_spirit status=rejected\r\n"
            );
        }
        return;
    }
    if (selected_item == SUDEKIMP_CLEANROOM_AILISH ||
        selected_item > MENU_DUMMY_INDEX ||
        pending_actions[selected_item] != SUDEKIMP_PENDING_NONE) {
        return;
    }
    if (!SudekiMpCleanroomEngineActorPosition(
            SUDEKIMP_CLEANROOM_AILISH,
            position)) {
        failed_until[selected_item] = GetTickCount() + 2500u;
        menu_texture_dirty = TRUE;
        return;
    }

    if (selected_item < MENU_ACTOR_COUNT) {
        if (item_present[selected_item]) {
            if (multiplayer_requested) {
                failed_until[selected_item] = GetTickCount() + 2500u;
                menu_texture_dirty = TRUE;
                SudekiMpLogFormat(
                    "cleanroom_menu event=despawn item=%s status=rejected "
                    "reason=disable_multiplayer_first\r\n",
                    SudekiMpCleanroomActorLabel(
                        (SudekiMpCleanroomActor)selected_item
                    )
                );
                return;
            }
            accepted = SudekiMpCleanroomEngineRemoveActor(
                (SudekiMpCleanroomActor)selected_item
            );
            if (accepted) {
                begin_pending(selected_item, SUDEKIMP_PENDING_REMOVE);
            }
        } else {
            static const float x_offsets[MENU_ACTOR_COUNT] = {
                -1.5f, 1.5f, -3.0f, 0.0f
            };
            position[0] += x_offsets[selected_item];
            position[2] -= 1.0f;
            accepted = SudekiMpCleanroomEngineSpawnActor(
                (SudekiMpCleanroomActor)selected_item,
                position
            );
            if (accepted) {
                begin_pending(selected_item, SUDEKIMP_PENDING_SPAWN);
            }
        }
    } else if (item_present[MENU_DUMMY_INDEX]) {
        accepted = SudekiMpCleanroomEngineRemoveDummy();
        if (accepted) {
            begin_pending(MENU_DUMMY_INDEX, SUDEKIMP_PENDING_REMOVE);
        }
    } else {
        if (cleanroom_anchor_valid) {
            position[0] = cleanroom_anchor[0];
            position[1] = cleanroom_anchor[1];
            position[2] = cleanroom_anchor[2];
        }
        accepted = SudekiMpCleanroomEngineSpawnDummy(position);
        if (accepted) {
            begin_pending(MENU_DUMMY_INDEX, SUDEKIMP_PENDING_SPAWN);
        }
    }
    if (!accepted) {
        failed_until[selected_item] = GetTickCount() + 2500u;
        menu_texture_dirty = TRUE;
        SudekiMpLogFormat(
            "cleanroom_menu event=toggle item=%u status=rejected\r\n",
            selected_item
        );
    }
}

static BOOL rising_key(unsigned int slot, UINT key) {
    BOOL down = (GetAsyncKeyState((int)key) & 0x8000) != 0;
    BOOL rising = down && !key_was_down[slot];
    key_was_down[slot] = down;
    return rising;
}

static void poll_menu_input(void) {
    BOOL foreground = owns_foreground();
    BOOL toggle = rising_key(0u, menu_toggle_key);
    BOOL up = rising_key(1u, VK_UP);
    BOOL down = rising_key(2u, VK_DOWN);
    BOOL activate = rising_key(3u, VK_RETURN) ||
        (menu_open && rising_key(4u, VK_SPACE));
    BOOL escape = rising_key(5u, VK_ESCAPE);

    if (!foreground) {
        return;
    }
    if (toggle) {
        menu_open = !menu_open;
        menu_texture_dirty = TRUE;
        SudekiMpLogFormat(
            "cleanroom_menu event=visibility state=%s\r\n",
            menu_open ? "open" : "closed"
        );
    }
    if (!menu_open) {
        return;
    }
    if (escape) {
        menu_open = FALSE;
        SudekiMpLogWrite(
            "cleanroom_menu event=visibility state=closed reason=escape\r\n"
        );
        return;
    }
    if (up) {
        selected_item = selected_item == 0u ?
            MENU_ITEM_COUNT - 1u : selected_item - 1u;
        menu_texture_dirty = TRUE;
    }
    if (down) {
        selected_item = (selected_item + 1u) % MENU_ITEM_COUNT;
        menu_texture_dirty = TRUE;
    }
    if (activate) {
        activate_selected_item();
    }
}

static void __attribute__((thiscall)) cleanroom_controller_update(
    void *controller,
    void *update_data
) {
    original_controller_update(controller, update_data);
    SudekiMpCleanroomMenuUpdate();
}

void SudekiMpCleanroomMenuUpdate(void) {
    if (game_base == NULL) {
        return;
    }
    update_action_status();
    poll_menu_input();
}

static const uint8_t *glyph_rows(char character) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    static const uint8_t greater[7] = {16,8,4,2,4,8,16};
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t colon[7] = {0,4,4,0,4,4,0};

    if (character >= 'A' && character <= 'Z') {
        return font_letters[character - 'A'];
    }
    if (character >= '0' && character <= '9') {
        return font_digits[character - '0'];
    }
    if (character == '>') return greater;
    if (character == '-') return dash;
    if (character == ':') return colon;
    return blank;
}

static void fill_rectangle(
    uint32_t *pixels,
    int pitch,
    int left,
    int top,
    int right,
    int bottom,
    uint32_t color
) {
    int y;
    int x;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (int)MENU_TEXTURE_WIDTH) right = MENU_TEXTURE_WIDTH;
    if (bottom > (int)MENU_TEXTURE_HEIGHT) bottom = MENU_TEXTURE_HEIGHT;
    for (y = top; y < bottom; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
        for (x = left; x < right; ++x) {
            row[x] = color;
        }
    }
}

static void draw_text(
    uint32_t *pixels,
    int pitch,
    int x,
    int y,
    const char *text,
    uint32_t color,
    int scale
) {
    const char *cursor;

    for (cursor = text; *cursor != '\0'; ++cursor) {
        const uint8_t *rows = glyph_rows(*cursor);
        int row;
        for (row = 0; row < 7; ++row) {
            int column;
            for (column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4 - column))) != 0u) {
                    fill_rectangle(
                        pixels,
                        pitch,
                        x + column * scale,
                        y + row * scale,
                        x + (column + 1) * scale,
                        y + (row + 1) * scale,
                        color
                    );
                }
            }
        }
        x += 6 * scale;
    }
}

static const char *item_status(unsigned int index) {
    DWORD now = GetTickCount();

    if (index == SUDEKIMP_CLEANROOM_AILISH) {
        return item_present[index] ? "LEAD LOCKED" : "WAITING";
    }
    if (pending_actions[index] == SUDEKIMP_PENDING_SPAWN) {
        return "SPAWNING";
    }
    if (pending_actions[index] == SUDEKIMP_PENDING_REMOVE) {
        return "REMOVING";
    }
    if (failed_until[index] != 0u &&
        (LONG)(failed_until[index] - now) > 0) {
        return "FAILED";
    }
    return item_present[index] ? "PRESENT" : "ABSENT";
}

static BOOL update_menu_texture(void *texture) {
    void **vtable = *(void ***)texture;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpD3DLockedRect locked;
    uint32_t *pixels;
    unsigned int index;
    HRESULT result;

    if (vtable == NULL) {
        return FALSE;
    }
    lock_rectangle = (D3DTextureLockRectFunction)
        vtable[D3D_TEXTURE_LOCK_RECT_INDEX];
    unlock_rectangle = (D3DTextureUnlockRectFunction)
        vtable[D3D_TEXTURE_UNLOCK_RECT_INDEX];
    if (lock_rectangle == NULL || unlock_rectangle == NULL) {
        return FALSE;
    }
    result = lock_rectangle(texture, 0u, &locked, NULL, 0u);
    if (FAILED(result) || locked.bits == NULL || locked.pitch <= 0) {
        return FALSE;
    }
    pixels = (uint32_t *)locked.bits;
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        MENU_TEXTURE_WIDTH, MENU_TEXTURE_HEIGHT, UINT32_C(0xd818202c));
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        MENU_TEXTURE_WIDTH, 4, UINT32_C(0xff35e6e0));
    fill_rectangle(
        pixels, locked.pitch, 0, MENU_TEXTURE_HEIGHT - 4,
        MENU_TEXTURE_WIDTH, MENU_TEXTURE_HEIGHT, UINT32_C(0xff35e6e0));
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        4, MENU_TEXTURE_HEIGHT, UINT32_C(0xff35e6e0));
    fill_rectangle(
        pixels, locked.pitch, MENU_TEXTURE_WIDTH - 4, 0,
        MENU_TEXTURE_WIDTH, MENU_TEXTURE_HEIGHT, UINT32_C(0xff35e6e0));
    draw_text(
        pixels, locked.pitch, 32, 24, "SUDEKIMP CLEANROOM",
        UINT32_C(0xff5ef7f0), 3);
    draw_text(
        pixels, locked.pitch, 32, 56,
        "F8 OR ESC CLOSE  ARROWS AND ENTER TOGGLE",
        UINT32_C(0xffaab8c8), 2);

    for (index = 0u; index < MENU_ITEM_COUNT; ++index) {
        int y = 94 + (int)index * 34;
        const char *label;
        const char *status;
        uint32_t status_color = UINT32_C(0xffb9c4d1);

        if (index == selected_item) {
            fill_rectangle(
                pixels, locked.pitch, 20, y - 8,
                MENU_TEXTURE_WIDTH - 20, y + 25,
                UINT32_C(0x90324962));
            draw_text(
                pixels, locked.pitch, 30, y, ">",
                UINT32_C(0xffffffff), 2);
        }
        if (index < MENU_ACTOR_COUNT) {
            label = SudekiMpCleanroomActorLabel(
                (SudekiMpCleanroomActor)index
            );
            status = item_status(index);
        } else if (index == MENU_DUMMY_INDEX) {
            label = "TRAINING DUMMY";
            status = item_status(index);
        } else if (index == MENU_COMBAT_INDEX) {
            label = "COMBAT MODE";
            status = combat_mode_valid ?
                (combat_mode ? "ENABLED" : "DISABLED") : "UNAVAILABLE";
            if (combat_mode_valid && combat_mode) {
                status_color = UINT32_C(0xff7cf29a);
            }
        } else if (index == MENU_CAMERA_INDEX) {
            label = "CAMERA MODE";
            status = first_person_mode_valid ?
                (first_person_mode ? "FIRST PERSON" : "THIRD PERSON") :
                "UNAVAILABLE";
            if (first_person_mode_valid) {
                status_color = UINT32_C(0xff7cf29a);
            }
        } else if (index == MENU_INFINITE_SP_INDEX) {
            label = "INFINITE SP";
            status = infinite_sp_valid ?
                (infinite_sp ? "ENABLED" : "DISABLED") : "UNAVAILABLE";
            if (infinite_sp_valid && infinite_sp) {
                status_color = UINT32_C(0xff7cf29a);
            }
        } else if (index == MENU_MULTIPLAYER_INDEX) {
            label = "SPLIT SCREEN P2";
            if (!integrated_multiplayer_mode) {
                status = "UNAVAILABLE";
            } else if (!multiplayer_requested) {
                status = "DISABLED";
            } else if (!multiplayer_active) {
                status = "WAITING FOR P2";
                status_color = UINT32_C(0xffffd166);
            } else if (!multiplayer_input_ready) {
                status = "RAZER WAITING";
                status_color = UINT32_C(0xffffd166);
            } else {
                status = "P2 READY";
                status_color = UINT32_C(0xff7cf29a);
            }
        } else if (index == MENU_INFINITE_SPIRIT_INDEX) {
            label = "INFINITE SPIRIT";
            status = infinite_spirit_valid ?
                (infinite_spirit ? "ENABLED" : "DISABLED") :
                "UNAVAILABLE";
            if (infinite_spirit_valid && infinite_spirit) {
                status_color = UINT32_C(0xff7cf29a);
            }
        } else {
            label = "CLOSE";
            status = "";
        }
        if (index <= MENU_DUMMY_INDEX) {
            if (pending_actions[index] != SUDEKIMP_PENDING_NONE) {
                status_color = UINT32_C(0xffffd166);
            } else if (failed_until[index] != 0u) {
                status_color = UINT32_C(0xffff6b6b);
            } else if (item_present[index]) {
                status_color = UINT32_C(0xff7cf29a);
            }
        }
        draw_text(
            pixels, locked.pitch, 58, y, label,
            UINT32_C(0xffffffff), 2);
        draw_text(
            pixels, locked.pitch, 390, y, status,
            status_color, 2);
    }
    result = unlock_rectangle(texture, 0u);
    if (FAILED(result)) {
        return FALSE;
    }
    menu_texture_dirty = FALSE;
    return TRUE;
}

static BOOL ensure_menu_texture(void *device) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    HRESULT result;

    if (menu_texture_device != device) {
        release_com_object(&menu_texture);
        menu_texture_device = device;
        menu_texture_dirty = TRUE;
    }
    if (menu_texture != NULL) {
        return !menu_texture_dirty || update_menu_texture(menu_texture);
    }
    vtable = *(void ***)device;
    if (vtable == NULL) {
        return FALSE;
    }
    create_texture = (D3DCreateTextureFunction)
        vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    if (create_texture == NULL) {
        return FALSE;
    }
    result = create_texture(
        device,
        MENU_TEXTURE_WIDTH,
        MENU_TEXTURE_HEIGHT,
        1u,
        D3D_USAGE_DYNAMIC,
        D3D_FORMAT_A8R8G8B8,
        D3D_POOL_DEFAULT,
        &menu_texture,
        NULL
    );
    if (FAILED(result) || menu_texture == NULL) {
        menu_texture = NULL;
        return FALSE;
    }
    menu_texture_dirty = TRUE;
    return update_menu_texture(menu_texture);
}

static BOOL update_player_two_badge_texture(void *texture) {
    void **vtable = *(void ***)texture;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpD3DLockedRect locked;
    uint32_t *pixels;
    uint32_t accent;
    const char *text;
    HRESULT result;

    if (vtable == NULL) {
        return FALSE;
    }
    lock_rectangle = (D3DTextureLockRectFunction)
        vtable[D3D_TEXTURE_LOCK_RECT_INDEX];
    unlock_rectangle = (D3DTextureUnlockRectFunction)
        vtable[D3D_TEXTURE_UNLOCK_RECT_INDEX];
    if (lock_rectangle == NULL || unlock_rectangle == NULL ||
        FAILED(lock_rectangle(texture, 0u, &locked, NULL, 0u)) ||
        locked.bits == NULL || locked.pitch <= 0) {
        return FALSE;
    }
    pixels = (uint32_t *)locked.bits;
    accent = multiplayer_active && multiplayer_input_ready ?
        UINT32_C(0xff7cf29a) : UINT32_C(0xffffd166);
    text = multiplayer_active && multiplayer_input_ready ?
        "P2 READY" : (multiplayer_active ? "P2 RAZER" : "P2 JOINING");
    fill_rectangle(
        pixels,
        locked.pitch,
        0,
        0,
        PLAYER_TWO_BADGE_WIDTH,
        PLAYER_TWO_BADGE_HEIGHT,
        UINT32_C(0xc818202c)
    );
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        PLAYER_TWO_BADGE_WIDTH, 3, accent);
    fill_rectangle(
        pixels, locked.pitch, 0, PLAYER_TWO_BADGE_HEIGHT - 3,
        PLAYER_TWO_BADGE_WIDTH, PLAYER_TWO_BADGE_HEIGHT, accent);
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        3, PLAYER_TWO_BADGE_HEIGHT, accent);
    fill_rectangle(
        pixels, locked.pitch, PLAYER_TWO_BADGE_WIDTH - 3, 0,
        PLAYER_TWO_BADGE_WIDTH, PLAYER_TWO_BADGE_HEIGHT, accent);
    fill_rectangle(
        pixels, locked.pitch, 12, 13, 26, 29, accent);
    draw_text(
        pixels,
        locked.pitch,
        36,
        13,
        text,
        UINT32_C(0xffffffff),
        2
    );
    result = unlock_rectangle(texture, 0u);
    if (FAILED(result)) {
        return FALSE;
    }
    player_two_badge_dirty = FALSE;
    return TRUE;
}

static BOOL ensure_player_two_badge_texture(void *device) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    HRESULT result;

    if (player_two_badge_device != device) {
        release_com_object(&player_two_badge_texture);
        player_two_badge_device = device;
        player_two_badge_dirty = TRUE;
    }
    if (player_two_badge_texture != NULL) {
        return !player_two_badge_dirty ||
            update_player_two_badge_texture(player_two_badge_texture);
    }
    vtable = *(void ***)device;
    if (vtable == NULL) {
        return FALSE;
    }
    create_texture = (D3DCreateTextureFunction)
        vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    if (create_texture == NULL) {
        return FALSE;
    }
    result = create_texture(
        device,
        PLAYER_TWO_BADGE_WIDTH,
        PLAYER_TWO_BADGE_HEIGHT,
        1u,
        D3D_USAGE_DYNAMIC,
        D3D_FORMAT_A8R8G8B8,
        D3D_POOL_DEFAULT,
        &player_two_badge_texture,
        NULL
    );
    if (FAILED(result) || player_two_badge_texture == NULL) {
        player_two_badge_texture = NULL;
        return FALSE;
    }
    player_two_badge_dirty = TRUE;
    return update_player_two_badge_texture(player_two_badge_texture);
}

static BOOL draw_texture_overlay(
    void *texture,
    UINT texture_width,
    UINT texture_height,
    BOOL player_two_badge
) {
    void *device;
    void **device_vtable;
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
    void *render_target = NULL;
    void *state_block = NULL;
    void **surface_vtable;
    void **state_block_vtable;
    D3DSurfaceGetDescFunction get_description;
    D3DStateBlockApplyFunction apply_state_block;
    SudekiMpD3DSurfaceDesc description;
    SudekiMpMenuVertex vertices[4];
    float left;
    float top;
    float right;
    float bottom;
    HRESULT result;
    HRESULT restore_result;

    if (texture == NULL || d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        (device_vtable = *(void ***)device) == NULL) {
        return FALSE;
    }
    get_render_target = (D3DGetRenderTargetFunction)
        device_vtable[D3D_DEVICE_GET_RENDER_TARGET_INDEX];
    create_state_block = (D3DCreateStateBlockFunction)
        device_vtable[D3D_DEVICE_CREATE_STATE_BLOCK_INDEX];
    set_render_state = (D3DSetRenderStateFunction)
        device_vtable[D3D_DEVICE_SET_RENDER_STATE_INDEX];
    set_texture = (D3DSetTextureFunction)
        device_vtable[D3D_DEVICE_SET_TEXTURE_INDEX];
    set_texture_stage_state = (D3DSetTextureStageStateFunction)
        device_vtable[D3D_DEVICE_SET_TEXTURE_STAGE_STATE_INDEX];
    set_sampler_state = (D3DSetSamplerStateFunction)
        device_vtable[D3D_DEVICE_SET_SAMPLER_STATE_INDEX];
    draw_primitive_up = (D3DDrawPrimitiveUpFunction)
        device_vtable[D3D_DEVICE_DRAW_PRIMITIVE_UP_INDEX];
    set_fvf = (D3DSetFvfFunction)
        device_vtable[D3D_DEVICE_SET_FVF_INDEX];
    set_vertex_shader = (D3DSetShaderFunction)
        device_vtable[D3D_DEVICE_SET_VERTEX_SHADER_INDEX];
    set_pixel_shader = (D3DSetShaderFunction)
        device_vtable[D3D_DEVICE_SET_PIXEL_SHADER_INDEX];
    if (get_render_target == NULL || create_state_block == NULL ||
        set_render_state == NULL || set_texture == NULL ||
        set_texture_stage_state == NULL || set_sampler_state == NULL ||
        draw_primitive_up == NULL || set_fvf == NULL ||
        set_vertex_shader == NULL || set_pixel_shader == NULL ||
        FAILED(get_render_target(device, 0u, &render_target)) ||
        render_target == NULL) {
        return FALSE;
    }
    surface_vtable = *(void ***)render_target;
    get_description = surface_vtable == NULL ? NULL :
        (D3DSurfaceGetDescFunction)
            surface_vtable[D3D_SURFACE_GET_DESC_INDEX];
    if (get_description == NULL ||
        FAILED(get_description(render_target, &description))) {
        release_com_object(&render_target);
        return FALSE;
    }
    release_com_object(&render_target);
    if (description.width < texture_width ||
        description.height < texture_height) {
        return FALSE;
    }
    result = create_state_block(device, D3D_STATE_BLOCK_ALL, &state_block);
    if (FAILED(result) || state_block == NULL) {
        return FALSE;
    }
    state_block_vtable = *(void ***)state_block;
    apply_state_block = state_block_vtable == NULL ? NULL :
        (D3DStateBlockApplyFunction)
            state_block_vtable[D3D_STATE_BLOCK_APPLY_INDEX];
    if (apply_state_block == NULL) {
        release_com_object(&state_block);
        return FALSE;
    }

    if (player_two_badge) {
        left = (float)description.width * 0.75f -
            (float)texture_width * 0.5f - 0.5f;
        top = 14.0f - 0.5f;
    } else {
        left = ((float)description.width - texture_width) * 0.5f - 0.5f;
        top = ((float)description.height - texture_height) * 0.5f - 0.5f;
    }
    right = left + texture_width;
    bottom = top + texture_height;
    vertices[0] = (SudekiMpMenuVertex){left, top, 0.0f, 1.0f, 0.0f, 0.0f};
    vertices[1] = (SudekiMpMenuVertex){right, top, 0.0f, 1.0f, 1.0f, 0.0f};
    vertices[2] = (SudekiMpMenuVertex){left, bottom, 0.0f, 1.0f, 0.0f, 1.0f};
    vertices[3] = (SudekiMpMenuVertex){right, bottom, 0.0f, 1.0f, 1.0f, 1.0f};

    result = set_vertex_shader(device, NULL);
    if (FAILED(result)) goto restore_state;
    result = set_pixel_shader(device, NULL);
    if (FAILED(result)) goto restore_state;
    result = set_fvf(device, D3D_FVF_XYZRHW_TEX1);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_Z_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_Z_WRITE_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_ALPHA_TEST_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_CULL_MODE, D3D_CULL_NONE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_ALPHA_BLEND_ENABLE, TRUE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_SRC_BLEND, D3D_BLEND_SRC_ALPHA);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_DEST_BLEND, D3D_BLEND_INVERSE_SRC_ALPHA);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_FOG_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_LIGHTING, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_SCISSOR_TEST_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_texture_stage_state(
        device, 0u, D3D_TEXTURE_STAGE_COLOR_OPERATION,
        D3D_TEXTURE_OPERATION_SELECT_ARGUMENT_ONE);
    if (FAILED(result)) goto restore_state;
    result = set_texture_stage_state(
        device, 0u, D3D_TEXTURE_STAGE_COLOR_ARGUMENT_ONE,
        D3D_TEXTURE_ARGUMENT_TEXTURE);
    if (FAILED(result)) goto restore_state;
    result = set_texture_stage_state(
        device, 0u, D3D_TEXTURE_STAGE_ALPHA_OPERATION,
        D3D_TEXTURE_OPERATION_SELECT_ARGUMENT_ONE);
    if (FAILED(result)) goto restore_state;
    result = set_texture_stage_state(
        device, 0u, D3D_TEXTURE_STAGE_ALPHA_ARGUMENT_ONE,
        D3D_TEXTURE_ARGUMENT_TEXTURE);
    if (FAILED(result)) goto restore_state;
    result = set_texture_stage_state(
        device, 1u, D3D_TEXTURE_STAGE_COLOR_OPERATION,
        D3D_TEXTURE_OPERATION_DISABLE);
    if (FAILED(result)) goto restore_state;
    result = set_sampler_state(
        device, 0u, D3D_SAMPLER_MAG_FILTER, D3D_TEXTURE_FILTER_POINT);
    if (FAILED(result)) goto restore_state;
    result = set_sampler_state(
        device, 0u, D3D_SAMPLER_MIN_FILTER, D3D_TEXTURE_FILTER_POINT);
    if (FAILED(result)) goto restore_state;
    result = set_texture(device, 0u, texture);
    if (FAILED(result)) goto restore_state;
    result = draw_primitive_up(
        device,
        D3D_PRIMITIVE_TRIANGLE_STRIP,
        2u,
        vertices,
        sizeof(vertices[0])
    );

restore_state:
    restore_result = apply_state_block(state_block);
    release_com_object(&state_block);
    return SUCCEEDED(result) && SUCCEEDED(restore_result);
}

static BOOL draw_menu_overlay(void) {
    void *device;

    if (!menu_open || d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_menu_texture(device)) {
        return FALSE;
    }
    return draw_texture_overlay(
        menu_texture,
        MENU_TEXTURE_WIDTH,
        MENU_TEXTURE_HEIGHT,
        FALSE
    );
}

static BOOL draw_player_two_badge(void) {
    void *device;

    if (!integrated_multiplayer_mode || !multiplayer_requested ||
        d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_player_two_badge_texture(device)) {
        return FALSE;
    }
    return draw_texture_overlay(
        player_two_badge_texture,
        PLAYER_TWO_BADGE_WIDTH,
        PLAYER_TWO_BADGE_HEIGHT,
        TRUE
    );
}

void SudekiMpCleanroomFrameEndDispatch(void) {
    SudekiMpCleanroomMenuRender();
    original_frame_end();
}

void SudekiMpCleanroomMenuRender(void) {
    if (game_base == NULL) {
        return;
    }
    if (integrated_multiplayer_mode && multiplayer_requested &&
        !draw_player_two_badge() && !player_two_badge_failure_logged) {
        player_two_badge_failure_logged = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=player_two_badge status=unavailable "
            "fallback=split_screen_unchanged\r\n"
        );
    }
    if (menu_open && !draw_menu_overlay() && !overlay_failure_logged) {
        overlay_failure_logged = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=overlay status=unavailable "
            "fallback=gameplay_unchanged\r\n"
        );
    }
}

__attribute__((naked, noinline, used))
static void cleanroom_frame_end_entry(void) {
    __asm__ volatile(
        "call _SudekiMpCleanroomFrameEndDispatch\n\t"
        "ret\n\t"
    );
}

static BOOL install_cleanroom_menu_internal(
    HMODULE game_module,
    UINT toggle_key,
    BOOL integrated
) {
    uint8_t *base;
    void **controller_slot;

    if (game_module == NULL || game_base != NULL || toggle_key == 0u ||
        toggle_key > 0xffu || !command_line_is_cleanroom()) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineInitialize(game_module) ||
        !SudekiMpCleanroomAudioInitialize()) {
        SudekiMpCleanroomEngineReset();
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineSetInfiniteSp(TRUE) ||
        !SudekiMpCleanroomEngineSetInfiniteSpirit(TRUE)) {
        SudekiMpCleanroomEngineReset();
        return FALSE;
    }
    base = (uint8_t *)game_module;
    controller_slot = (void **)(base + RVA_CONTROLLER_UPDATE_VTABLE_SLOT);
    game_base = base;
    d3d_device_global = (void **)(base + RVA_D3D_DEVICE_GLOBAL);
    original_controller_update = integrated ? NULL :
        (ControllerUpdateFunction)(base + RVA_CONTROLLER_UPDATE);
    original_frame_end = integrated ? NULL :
        (FrameEndFunction)(base + RVA_FRAME_END);
    menu_toggle_key = toggle_key;
    menu_open = FALSE;
    menu_texture_dirty = TRUE;
    player_two_badge_dirty = TRUE;
    selected_item = 0u;
    ZeroMemory(key_was_down, sizeof(key_was_down));
    ZeroMemory(item_present, sizeof(item_present));
    ZeroMemory(pending_actions, sizeof(pending_actions));
    ZeroMemory(pending_started, sizeof(pending_started));
    ZeroMemory(failed_until, sizeof(failed_until));
    cleanroom_anchor_valid = FALSE;
    overlay_failure_logged = FALSE;
    player_two_badge_failure_logged = FALSE;
    cleanroom_world_ready = FALSE;
    combat_mode = FALSE;
    combat_mode_valid = FALSE;
    first_person_mode = FALSE;
    first_person_mode_valid = FALSE;
    infinite_sp = TRUE;
    infinite_sp_valid = TRUE;
    infinite_spirit = TRUE;
    infinite_spirit_valid = TRUE;
    integrated_multiplayer_mode = integrated;
    multiplayer_requested = FALSE;
    multiplayer_active = FALSE;
    multiplayer_input_ready = FALSE;
    last_status_update = 0u;
    update_action_status();

    if (!integrated && (!SudekiMpInstallPointerHook(
            &controller_update_hook,
            controller_slot,
            original_controller_update,
            cleanroom_controller_update) ||
        !SudekiMpInstallRelativeCallHook(
            &frame_end_hook,
            base + RVA_FRAME_END_CALL,
            original_frame_end,
            cleanroom_frame_end_entry))) {
        SudekiMpUninstallCleanroomMenu();
        return FALSE;
    }
    SudekiMpLogFormat(
        "cleanroom_menu event=install status=success toggle_key=0x%02lx "
        "lead=PC_Ailish actor_policy=native_internal_spawn_and_remove "
        "dummy_resource=MON_TrainingDummy dummy_placement=cleanroom_center_anchor "
        "controls=combat_camera_infinite_sp_infinite_spirit "
        "resource_defaults=enabled multiplayer_integration=%s\r\n",
        (unsigned long)menu_toggle_key,
        integrated ? "external_control_and_render_hooks" : "standalone_hooks"
    );
    return TRUE;
}

BOOL SudekiMpInstallCleanroomMenu(HMODULE game_module, UINT toggle_key) {
    return install_cleanroom_menu_internal(game_module, toggle_key, FALSE);
}

BOOL SudekiMpInstallIntegratedCleanroomMenu(
    HMODULE game_module,
    UINT toggle_key
) {
    return install_cleanroom_menu_internal(game_module, toggle_key, TRUE);
}

void SudekiMpUninstallCleanroomMenu(void) {
    SudekiMpRestoreRelativeCallHook(&frame_end_hook);
    SudekiMpRestorePointerHook(&controller_update_hook);
    release_com_object(&menu_texture);
    release_com_object(&player_two_badge_texture);
    menu_texture_device = NULL;
    player_two_badge_device = NULL;
    player_two_badge_dirty = FALSE;
    original_controller_update = NULL;
    original_frame_end = NULL;
    game_base = NULL;
    d3d_device_global = NULL;
    menu_toggle_key = 0u;
    menu_open = FALSE;
    menu_texture_dirty = FALSE;
    selected_item = 0u;
    cleanroom_anchor_valid = FALSE;
    overlay_failure_logged = FALSE;
    player_two_badge_failure_logged = FALSE;
    cleanroom_world_ready = FALSE;
    combat_mode = FALSE;
    combat_mode_valid = FALSE;
    first_person_mode = FALSE;
    first_person_mode_valid = FALSE;
    infinite_sp = FALSE;
    infinite_sp_valid = FALSE;
    infinite_spirit = FALSE;
    infinite_spirit_valid = FALSE;
    integrated_multiplayer_mode = FALSE;
    multiplayer_requested = FALSE;
    multiplayer_active = FALSE;
    multiplayer_input_ready = FALSE;
    last_status_update = 0u;
    SudekiMpCleanroomEngineReset();
}
